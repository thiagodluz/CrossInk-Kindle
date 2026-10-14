#include <Arduino.h>
#include <EpdFont.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <Txt.h>
#include <Utf8.h>
#include <builtinFonts/bitter_14_regular.h>
#include <builtinFonts/lexenddeca_10_regular.h>
#include <builtinFonts/lexenddeca_14_regular.h>
#include <builtinFonts/lexenddeca_16_bold.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "RenderWorker.h"

namespace fui = freeink::ui;
namespace {
volatile sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
constexpr int UI_FONT = 0, READING_FONT = 1, TITLE_FONT = 2, SMALL_FONT = 3;
constexpr size_t MAX_TEXT_BYTES = 256 * 1024, MAX_TEXT_PAGES = 512, MAX_FILES = 200;
constexpr fui::ActionId OPEN = 1, BACK = 2, PREV = 3, NEXT = 4, EXIT = 5;
const EpdFont uiFont(&lexenddeca_14_regular), readingFont(&bitter_14_regular), titleFont(&lexenddeca_16_bold),
    smallFont(&lexenddeca_10_regular);
uint32_t fnv(const std::string& text) {
  uint32_t value = 2166136261U;
  for (unsigned char c : text) {
    value ^= c;
    value *= 16777619U;
  }
  return value;
}
bool hasTxtExtension(const std::string& path) {
  if (path.size() < 4) return false;
  std::string extension = path.substr(path.size() - 4);
  for (auto& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return extension == ".txt";
}
struct Entry {
  std::string name;
  bool directory = false;
};
struct Page {
  std::vector<std::string> lines;
  size_t offset = 0;
};

class PortApp {
 public:
  explicit PortApp(GfxRenderer& renderer)
      : renderer_(renderer), target_(renderer, true), ui_(target_, target_.deviceContext()) {
    target_.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT);
    target_.setFont(fui::GfxRendererTarget::FONT_BODY, UI_FONT);
    target_.setFont(fui::GfxRendererTarget::FONT_TITLE, TITLE_FONT);
    auto device = target_.deviceContext();
    device.hasButtons = false;
    ui_.setDevice(device);
    auto theme = fui::themeTokensForLineHeight(renderer_.getLineHeight(UI_FONT));
    theme.bodyText.font = fui::GfxRendererTarget::FONT_BODY;
    theme.smallText.font = fui::GfxRendererTarget::FONT_SMALL;
    theme.titleText.font = fui::GfxRendererTarget::FONT_TITLE;
    theme.headerHeight = 110;
    theme.footerHeight = 96;
    theme.rowHeight = 78;
    theme.minTouchSize = 64;
    theme.headerTitleAlign = fui::TextAlign::Center;
    ui_.setTheme(theme);
    ui_.setScreen(&PortApp::screen, this);
    for (const auto action : {OPEN, BACK, PREV, NEXT, EXIT}) ui_.on(action, &PortApp::action, this);
  }
  static void work(const kindle::UiJob& job, void* context) { static_cast<PortApp*>(context)->handle(job); }
  bool exited() const { return exit_.load(); }
  bool failed() const { return failed_.load(); }
  uint64_t generation() const { return generation_.load(); }
  size_t currentPage() const { return page_; }
  size_t pageCount() const { return pages_.size(); }

  bool previewOpen(const char* path) { return openBook(path); }
  void previewNext() { changePage(1); }
  void previewRender() { paint(true); }
  void previewSave() { saveProgress(); }
  void previewLoad() {
    loadDirectory();
    paint(true);
  }
  void previewTap(int rawX, int rawY) {
    kindle::UiJob job;
    job.kind = kindle::UiJob::Kind::Touch;
    job.generation = generation_.load();
    job.touch.rawX = rawX;
    job.touch.rawY = rawY;
    job.touch.nx = rawX / 758.0f;
    job.touch.ny = rawY / 1024.0f;
    job.touch.phase = kindle::Touch::Phase::Down;
    handle(job);
    job.touch.phase = kindle::Touch::Phase::Up;
    handle(job);
  }

 private:
  void handle(const kindle::UiJob& job) {
    if (job.kind == kindle::UiJob::Kind::Start) {
      loadDirectory();
      paint(true);
      return;
    }
    if (job.kind == kindle::UiJob::Kind::Stop) {
      saveProgress();
      return;
    }
    if (job.kind == kindle::UiJob::Kind::Tick) {
      if (dirty_ && millis() - lastSave_ >= 10000) saveProgress();
      return;
    }
    if (job.generation != generation_.load()) {
      LOG_DBG("UI", "Ignoring input from an older frame");
      return;
    }
    float nx, ny;
    int x, y;
    HalGPIO::canonicalTouch(job.touch, nx, ny);
    renderer_.tapToLogical(nx, ny, x, y);
    fui::InputSnapshot input;
    input.touchX = static_cast<int16_t>(x);
    input.touchY = static_cast<int16_t>(y);
    input.touchPressed = job.touch.phase == kindle::Touch::Phase::Down;
    input.touchReleased = job.touch.phase == kindle::Touch::Phase::Up;
    if (!input.touchPressed && !input.touchReleased) return;
    LOG_DBG("UI", "touch=%s logical=%d,%d", input.touchPressed ? "down" : "up", x, y);
    const auto event = ui_.route(input);
    if (event && !exit_.load()) paint(false);
  }
  void paint(bool full) {
    renderer_.clearScreen();
    ui_.render();
    renderer_.displayBuffer(full ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
    if (!display.healthy()) {
      failed_ = true;
      exit_ = true;
    }
    ++generation_;
    LOG_INF("UI", "frame=%llu view=%s page=%zu/%zu", static_cast<unsigned long long>(generation_.load()),
            book_ ? "txt" : "files", book_ ? page_ + 1 : top_ + 1, book_ ? pages_.size() : entries_.size());
  }
  static void screen(fui::FreeInkApp<32, 8>::ScreenType& s, void* context) {
    static_cast<PortApp*>(context)->buildScreen(s);
  }
  static void action(const fui::ActionEvent& event, void* context) {
    auto* app = static_cast<PortApp*>(context);
    app->ui_.clearTapFlash();
    LOG_INF("UI", "action=%u value=%d", event.action, event.value);
    switch (event.action) {
      case EXIT:
        app->saveProgress();
        app->exit_ = true;
        break;
      case BACK:
        if (app->error_)
          app->error_ = nullptr;
        else if (app->book_) {
          app->saveProgress();
          app->book_.reset();
          app->pages_.clear();
          app->text_.clear();
        } else if (app->directory_ != "/books") {
          app->directory_ = app->directory_.substr(0, app->directory_.find_last_of('/'));
          app->top_ = 0;
          app->loadDirectory();
        }
        break;
      case PREV:
        app->changePage(-1);
        break;
      case NEXT:
        app->changePage(1);
        break;
      case OPEN:
        if (event.value < 0 || static_cast<size_t>(event.value) >= app->entries_.size()) break;
        {
          const Entry entry = app->entries_[event.value];
          const std::string path = app->directory_ + "/" + entry.name;
          if (entry.directory) {
            app->directory_ = path;
            app->top_ = 0;
            app->loadDirectory();
          } else
            app->openBook(path.c_str());
        }
        break;
    }
  }
  void buildScreen(fui::FreeInkApp<32, 8>::ScreenType& screen) {
    screen.setContentMargin({12, 20, 12, 20});
    char subtitle[64];
    const std::string title = book_ ? book_->getTitle() : std::string(tr(STR_KINDLE_BOOKS));
    if (book_)
      std::snprintf(subtitle, sizeof(subtitle), tr(STR_KINDLE_PAGE_FMT), static_cast<unsigned>(page_ + 1),
                    static_cast<unsigned>(pages_.size()));
    screen.header(title.c_str(), book_ ? subtitle : directory_.c_str());
    const auto footer = screen.takeBottom(96);
    const char* labels[] = {tr(STR_BACK), tr(STR_KINDLE_PREVIOUS), tr(STR_KINDLE_NEXT), tr(STR_EXIT)};
    const fui::ActionId actions[] = {BACK, PREV, NEXT, EXIT};
    for (int i = 0; i < 4; ++i) {
      fui::ButtonProps button;
      button.label = labels[i];
      button.action = actions[i];
      button.text = screen.theme().smallText;
      button.minTouchSize = 64;
      button.styles = fui::defaultButtonStyles();
      for (auto* style : {&button.styles.normal, &button.styles.selected, &button.styles.focused, &button.styles.active,
                          &button.styles.disabled}) {
        style->border = fui::Paint::solid(fui::Color::Black);
        style->borderWidth = 1;
      }
      const int width = footer.width / 4;
      screen.button(button, {static_cast<int16_t>(footer.x + i * width + 4), static_cast<int16_t>(footer.y + 8),
                             static_cast<int16_t>(width - 8), static_cast<int16_t>(footer.height - 16)});
    }
    const auto body = screen.body();
    if (error_) {
      screen.centeredText(error_, screen.theme().bodyText);
      return;
    }
    if (book_) {
      int y = body.y + 12;
      for (const auto& line : pages_[page_].lines) {
        renderer_.drawText(READING_FONT, body.x + 12, y, line.c_str());
        y += lineHeight_;
      }
      return;
    }
    if (entries_.empty()) {
      screen.centeredText(tr(STR_NO_FILES_FOUND), screen.theme().bodyText);
      renderer_.drawCenteredText(SMALL_FONT, body.y + body.height / 2 + 65, tr(STR_KINDLE_BOOKS_HELP));
      return;
    }
    visible_ = std::max<size_t>(1, std::min<size_t>(8, body.height / 86));
    const size_t count = std::min(visible_, entries_.size() - top_);
    for (size_t i = 0; i < count; ++i) {
      rows_[i] = {};
      rows_[i].label = entries_[top_ + i].name.c_str();
      rows_[i].value = entries_[top_ + i].directory ? "/" : nullptr;
      rows_[i].actionValue = static_cast<int16_t>(top_ + i);
    }
    fui::ListProps props;
    props.items = rows_;
    props.count = static_cast<uint16_t>(count);
    props.action = OPEN;
    props.inputMask = fui::InputTouch;
    props.rowHeight = 78;
    props.rowGap = 8;
    props.selectedIndex = -1;
    props.labelText = screen.theme().bodyText;
    props.partialTrailingRow = false;
    screen.list(props);
  }
  void loadDirectory() {
    entries_.clear();
    error_ = nullptr;
    auto folder = Storage.open(directory_.c_str());
    if (!folder.isDirectory()) {
      folder.close();
      error_ = tr(STR_KINDLE_FILE_READ_ERROR);
      return;
    }
    while (entries_.size() < MAX_FILES) {
      auto file = folder.openNextFile();
      if (!file) break;
      char name[256];
      file.getName(name, sizeof(name));
      if (name[0] != '.' && !std::strchr(name, '\n')) entries_.push_back({name, file.isDirectory()});
      file.close();
    }
    if (folder.iterationFailed()) error_ = tr(STR_KINDLE_FILE_READ_ERROR);
    folder.close();
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
      return a.directory != b.directory ? a.directory > b.directory : a.name < b.name;
    });
    if (top_ >= entries_.size()) top_ = 0;
    LOG_INF("STO", "directory=%s entries=%zu", directory_.c_str(), entries_.size());
  }
  void changePage(int delta) {
    error_ = nullptr;
    if (book_) {
      if (delta > 0 && page_ + 1 < pages_.size()) {
        ++page_;
        dirty_ = true;
      }
      if (delta < 0 && page_) {
        --page_;
        dirty_ = true;
      }
    } else {
      if (delta > 0 && top_ + visible_ < entries_.size()) top_ += visible_;
      if (delta < 0) top_ = top_ > visible_ ? top_ - visible_ : 0;
    }
  }
  bool addLine(const std::string& line, size_t offset, size_t rows) {
    if (pages_.empty() || pages_.back().lines.size() == rows) {
      if (pages_.size() >= MAX_TEXT_PAGES) return false;
      pages_.push_back(Page{{}, offset});
      pages_.back().lines.reserve(rows);
    }
    pages_.back().lines.push_back(line);
    return true;
  }
  bool paginate() {
    pages_.clear();
    lineHeight_ = renderer_.getLineHeight(READING_FONT) + 7;
    const int usableHeight = renderer_.getScreenHeight() - 110 - 96 - 70;
    const size_t rows = std::max(1, usableHeight / lineHeight_);
    const int width = renderer_.getScreenWidth() - 76;
    std::string line;
    size_t cursor = 0, lineOffset = 0;
    if (text_.size() >= 3 && text_.compare(0, 3, "\xef\xbb\xbf") == 0) cursor = 3;
    while (cursor < text_.size()) {
      if (text_[cursor] == '\r' || text_[cursor] == ' ' || text_[cursor] == '\t') {
        ++cursor;
        continue;
      }
      if (text_[cursor] == '\n') {
        if (!addLine(line, line.empty() ? cursor : lineOffset, rows)) return false;
        line.clear();
        ++cursor;
        continue;
      }
      const size_t start = cursor;
      while (cursor < text_.size() && text_[cursor] != ' ' && text_[cursor] != '\t' && text_[cursor] != '\r' &&
             text_[cursor] != '\n')
        ++cursor;
      const std::string word = text_.substr(start, cursor - start);
      const std::string candidate = line.empty() ? word : line + " " + word;
      if (!line.empty() && renderer_.getTextWidth(READING_FONT, candidate.c_str()) > width) {
        if (!addLine(line, lineOffset, rows)) return false;
        line.clear();
      }
      if (renderer_.getTextWidth(READING_FONT, word.c_str()) <= width) {
        if (line.empty())
          lineOffset = start;
        else
          line += ' ';
        line += word;
        continue;
      }
      // Split long tokens only at UTF-8 boundaries, never between continuation bytes.
      size_t begin = 0;
      while (begin < word.size()) {
        size_t end = begin, fit = begin;
        do {
          ++end;
          while (end < word.size() && (static_cast<unsigned char>(word[end]) & 0xc0) == 0x80) ++end;
          if (renderer_.getTextWidth(READING_FONT, word.substr(begin, end - begin).c_str()) > width && fit > begin)
            break;
          fit = end;
        } while (end < word.size());
        if (!addLine(word.substr(begin, fit - begin), start + begin, rows)) return false;
        begin = fit;
      }
    }
    if (!line.empty() && !addLine(line, lineOffset, rows)) return false;
    if (pages_.empty()) addLine("", 0, rows);
    return true;
  }
  bool openBook(const char* path) {
    error_ = nullptr;
    if (!hasTxtExtension(path)) {
      error_ = tr(STR_KINDLE_FILE_UNSUPPORTED);
      return false;
    }
    book_ = makeUniqueNoThrow<Txt>(std::string(path), "/.crosspoint");
    if (!book_ || !book_->load()) {
      book_.reset();
      error_ = tr(STR_KINDLE_FILE_READ_ERROR);
      return false;
    }
    const size_t size = book_->getFileSize();
    if (size > MAX_TEXT_BYTES) {
      book_.reset();
      error_ = tr(STR_KINDLE_FILE_TOO_LARGE);
      return false;
    }
    auto bytes = makeUniqueNoThrow<uint8_t[]>(size + 1);
    if (!bytes || !book_->readContent(bytes.get(), 0, size)) {
      book_.reset();
      error_ = tr(STR_KINDLE_FILE_READ_ERROR);
      return false;
    }
    if (std::memchr(bytes.get(), 0, size)) {
      book_.reset();
      error_ = tr(STR_KINDLE_FILE_UNSUPPORTED);
      return false;
    }
    text_.assign(reinterpret_cast<const char*>(bytes.get()), size);
    contentHash_ = fnv(text_);
    if (!paginate()) {
      book_.reset();
      pages_.clear();
      text_.clear();
      error_ = tr(STR_KINDLE_FILE_TOO_LARGE);
      return false;
    }
    page_ = 0;
    dirty_ = false;
    lastSave_ = millis();
    const String saved = Storage.readFile(progressPath().c_str());
    const std::string state(saved.c_str());
    const auto first = state.find('\n'), second = state.find('\n', first == std::string::npos ? 0 : first + 1);
    if (first != std::string::npos && second != std::string::npos && state.substr(0, first) == book_->getPath()) {
      const auto hash = std::strtoul(state.c_str() + first + 1, nullptr, 10);
      const auto offset = std::strtoull(state.c_str() + second + 1, nullptr, 10);
      if (hash == contentHash_ && offset <= text_.size())
        for (size_t i = 0; i < pages_.size(); ++i)
          if (pages_[i].offset <= offset) page_ = i;
    }
    LOG_INF("TXT", "opened=%s bytes=%zu pages=%zu resume_page=%zu", path, size, pages_.size(), page_ + 1);
    return true;
  }
  std::string progressPath() const { return "/progress/txt-" + std::to_string(fnv(book_->getPath())) + ".txt"; }
  void saveProgress() {
    if (!book_ || pages_.empty()) return;
    const std::string state =
        book_->getPath() + "\n" + std::to_string(contentHash_) + "\n" + std::to_string(pages_[page_].offset) + "\n";
    const bool ok = Storage.writeFile(progressPath().c_str(), String(state));
    LOG_INF("TXT", "progress_saved=%s page=%zu", ok ? "yes" : "no", page_ + 1);
    if (ok) {
      dirty_ = false;
      lastSave_ = millis();
    } else
      failed_ = true;
  }
  GfxRenderer& renderer_;
  fui::GfxRendererTarget target_;
  fui::FreeInkApp<32, 8> ui_;
  fui::ListItem rows_[8]{};  // Reused by the render owner; no row scratch on its stack.
  std::vector<Entry> entries_;
  std::string directory_ = "/books", text_;
  std::unique_ptr<Txt> book_;
  std::vector<Page> pages_;
  size_t top_ = 0, visible_ = 8, page_ = 0;
  int lineHeight_ = 40;
  uint32_t contentHash_ = 0;
  unsigned long lastSave_ = 0;
  const char* error_ = nullptr;
  bool dirty_ = false;
  std::atomic<bool> exit_{false}, failed_{false};
  std::atomic<uint64_t> generation_{0};
};
}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("CrossInk PW1 UI preview 0.2.0 (native renderer, FreeInkUI and TXT module)");
  if (argc == 2 && !std::strcmp(argv[1], "--version")) return 0;
  if (argc == 2 && !std::strcmp(argv[1], "--probe")) {
    kindle::Display probe;
    kindle::Input input;
    return probe.open(false) && input.open(false) ? 0 : 1;
  }
  bool preview = false;
  const char* root = "/mnt/us/crossink/data";
  const char* output = nullptr;
  const char* bookPath = nullptr;
  unsigned nextPages = 0;
  std::vector<std::pair<int, int>> previewTaps;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--app")) continue;
    if (!std::strcmp(argv[i], "--preview"))
      preview = true;
    else if (!std::strcmp(argv[i], "--data-root") && i + 1 < argc)
      root = argv[++i];
    else if (!std::strcmp(argv[i], "--output") && i + 1 < argc)
      output = argv[++i];
    else if (!std::strcmp(argv[i], "--book") && i + 1 < argc)
      bookPath = argv[++i];
    else if (!std::strcmp(argv[i], "--next") && i + 1 < argc)
      nextPages = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    else if (!std::strcmp(argv[i], "--tap") && i + 1 < argc) {
      int x, y;
      char tail;
      if (previewTaps.size() >= 100 || std::sscanf(argv[++i], "%d,%d%c", &x, &y, &tail) != 2 || x < 0 || x > 758 ||
          y < 0 || y > 1024)
        return 2;
      previewTaps.emplace_back(x, y);
    } else {
      std::fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      return 2;
    }
  }
  if ((preview && !output) || (!preview && (output || bookPath || nextPages || !previewTaps.empty()))) return 2;
  if (!kindle::setStorageRoot(root) || !Storage.begin() || !Storage.mkdir("/books") || !Storage.mkdir("/progress"))
    return 1;
  struct sigaction handler{};
  handler.sa_handler = interrupt;
  sigemptyset(&handler.sa_mask);
  sigaction(SIGTERM, &handler, nullptr);
  sigaction(SIGINT, &handler, nullptr);
  if (!preview && !gpio.begin()) return 1;
  if (!display.initialize(preview)) return 1;
  GfxRenderer renderer(display);
  renderer.begin();
  renderer.insertFont(UI_FONT, EpdFontFamily(&uiFont));
  renderer.insertFont(READING_FONT, EpdFontFamily(&readingFont));
  renderer.insertFont(TITLE_FONT, EpdFontFamily(&titleFont));
  renderer.insertFont(SMALL_FONT, EpdFontFamily(&smallFont));
  FontDecompressor decompressor;
  decompressor.init();
  FontCacheManager fontCache(renderer.getFontMap(), renderer.getSdCardFonts());
  fontCache.setFontDecompressor(&decompressor);
  renderer.setFontCacheManager(&fontCache);
  I18N.setLanguage(Language::PT);
  // The UI owns persistent page/row state; heap lifetime encloses the worker.
  auto app = makeUniqueNoThrow<PortApp>(renderer);
  if (!app) return 1;
  if (preview) {
    app->previewLoad();
    if (bookPath && !app->previewOpen(bookPath)) return 1;
    for (unsigned i = 0; i < nextPages && i < 512; ++i) app->previewNext();
    for (const auto& point : previewTaps) {
      app->previewTap(point.first, point.second);
      if (app->exited()) break;
    }
    app->previewRender();
    const bool saved = display.savePreview(output);
    app->previewSave();
    std::printf("preview page=%zu count=%zu\n", app->currentPage() + 1, app->pageCount());
    return saved && !app->failed() ? 0 : 1;
  }
  kindle::RenderWorker worker(&PortApp::work, app.get());
  if (!worker.start()) return 1;
  const unsigned long start = millis();
  uint64_t contactGeneration = 0;
  bool inputError = false;
  while (!interrupted && !app->exited() && millis() - start < 300000) {
    kindle::Touch touch;
    const int result = gpio.poll(touch, 100);
    if (result < 0) {
      inputError = true;
      break;
    }
    if (!result || touch.phase == kindle::Touch::Phase::Move) continue;
    if (touch.phase == kindle::Touch::Phase::Down) contactGeneration = app->generation();
    kindle::UiJob job;
    job.kind = kindle::UiJob::Kind::Touch;
    job.touch = touch;
    job.generation = contactGeneration;
    if (!worker.enqueue(job)) {
      LOG_ERR("TASK", "Input queue overflow; ending preview");
      inputError = true;
      break;
    }
  }
  worker.stop();  // Save progress and join before releasing display/storage/font state.
  const bool restored = display.restore();
  LOG_INF("UI", "exit joined=yes display_restored=%d interrupted=%d", restored, static_cast<int>(interrupted));
  return !inputError && !app->failed() && restored ? 0 : 1;
}
