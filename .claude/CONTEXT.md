# CrossPoint Reader — Durable Context

Keep this file focused on repo-specific gotchas that are worth reusing in future sessions.

## FreeInk SDK

Refer to https://freeink.org/llms.txt for guidance.

## Simulator

- Simulator patches belong in the adjacent `crossink-simulator` repo.
- The valid local simulator env in this repo is `simulator`, and `pio run -e simulator` currently builds cleanly.
- The simulator `PNGdec` stub in `crossink-simulator/src/PNGdec.h` needs to mirror the real API shape used by app code, including `hasAlpha()` and `getTransparentColor()`, even though decode still fails intentionally.
- Known simulator limits:
  - No image rendering: `platformio.ini` ignores `hal`, `PNGdec`, and `JPEGDEC`, so image decoders are intentionally absent.
  - JPEGDEC stub always fails; `JPEGDEC fallback: open failed (err=-1)` is expected in simulator.
  - `esp_deep_sleep_start()` is a no-op in simulator.
  - `HalStorage` uses POSIX file access under `./fs_` and allows multiple readers, unlike real hardware.

## Real Hardware / Storage

- SdFat on hardware allows only one open reader per file path at a time. If a fallback needs to reopen the same file, close the first handle before reopening.

## Rendering / Reader Pipeline

- `lib/Epub/Epub/Page.cpp`: images must render only in `GfxRenderer::BW`; grayscale passes are text anti-aliasing passes only.
- Kindle EPUBs may contain paired high-res and old-Kindle fallback images. `ChapterHtmlSlimParser` should skip `<img>` nodes with `data-AmznRemoved-M8` to avoid duplicate stacked images.
- After image/layout pipeline changes that affect cached EPUB output, clear the affected `.crosspoint/epub_<hash>/` cache if behavior looks stale.

## UI Consistency

- Use FreeInkUI SDK components and input routing for list-style screens where possible. Row rendering, touch targets,
  hit testing, and pagination should share the same FreeInkUI list configuration instead of custom touch scaling.

## Heap Baselines (X4 hardware, SD card font)

- A normal resume-into-partial reading session runs at ~85-90KB free / ~49KB maxAlloc by
  the first watermark crossing (Epub metadata + x-locations + resident glyph caches).
  Do not read mid-range heap numbers as session degradation without checking the scenario.
- SD-font section builds cost ~38-50KB at cold start; the 4-style advance-table prewarm
  (~30KB incl. 16KB contiguous scratch) dominates and is skipped below 80KB free.

## Kindle PW1 branch

- `platform/kindle/` provides the validated diagnostic and a 0.2.0 native TXT preview (device validation pending). The preview reuses GfxRenderer/FreeInkUI/fonts/Txt; ActivityManager, UITheme and EPUB are not yet connected. Package it with `scripts/kindle/package-ui.py`; diagnostic-only packaging remains `build-package.py`.
- Two device runs on 2026-10-14 confirmed `mxc_epdc_fb` at 758x1024, 8bpp, stride 768, offsets 0/0, rotate field 3, and the Pearl send/wait ioctls. Portrait output is upright without changing the framebuffer mode.
- `cyttsp` on event0 reports ABS_MT_POSITION_X/Y (53/54), X 0..758, Y 0..1024, and BTN_TOUCH. Uncorrected normalized coordinates match portrait targets; do not apply a second rotation based solely on framebuffer `rotate=3`.
- Native rendering uses a canonical 1024x758 1-bit buffer; portrait drawing and inverse touch conversion produce the physical 758x1024 pixels. Storage is anchored below `/mnt/us/crossink/data`; the render worker is joinable and owns UI/file state. Static ARM softfp builds avoid newer runtime dependencies.
- Normal Exit restores pixels and resumes `cvm`/`awesome`. Automatic timeout and forced-crash recovery still need device validation. Use `/proc/PID/status` rather than `/proc/PID/comm` on the old kernel.

## Misc Repo Gotchas

- POSIX TZ signs are inverted from ISO 8601 in `TimeStore::applyTimezone()`: `"UTC-1"` means UTC+1.
- `LyraTheme::drawHeader()` does not call `BaseTheme::drawHeader()`, so header changes in the base theme must be duplicated in Lyra if needed.
