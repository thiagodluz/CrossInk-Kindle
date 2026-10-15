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

- `platform/kindle/` provides the validated diagnostic and a native TXT preview: 0.2.1 basic reading/normal exit/corner gesture validated on PW1, and 0.3.0/0.3.1 power button tests failed; 0.3.2 aborted in preflight; 0.3.3 opens but power remains active/no event bytes; 0.3.4 physical power-button save-and-exit validated; 0.3.5 power exit and native launcher signal validated, but reopening failed; 0.3.6 power/wake/KUAL reopen and normal Exit validated by logs, and saved-page retention confirmed by user. The preview reuses GfxRenderer/FreeInkUI/fonts/Txt; ActivityManager, UITheme and EPUB are not yet connected. Package it with `scripts/kindle/package-ui.py`; diagnostic-only packaging remains `build-package.py`.
- Two device runs on 2026-10-14 confirmed `mxc_epdc_fb` at 758x1024, 8bpp, stride 768, offsets 0/0, rotate field 3, and the Pearl send/wait ioctls. Portrait output is upright without changing the framebuffer mode.
- `cyttsp` on event0 reports ABS_MT_POSITION_X/Y (53/54), X 0..758, Y 0..1024, and BTN_TOUCH. Uncorrected normalized coordinates match portrait targets; do not apply a second rotation based solely on framebuffer `rotate=3`.
- Native rendering uses a canonical 1024x758 1-bit buffer; portrait drawing and inverse touch conversion produce the physical 758x1024 pixels. Storage is anchored below `/mnt/us/crossink/data`; the render worker is joinable and owns UI/file state. Static ARM softfp builds avoid newer runtime dependencies.
- 0.3.0 `ui(1).log` shows subscribers start, no recognized power event, then normal Exit and stock resumption; user saw sleep immediately after exit. This suggests a pending stock transition, not proof of a wildcard subscription failure. KOReader on upstart keeps cvm running and only pauses awesome on newer firmware; do not blindly alter UI ownership without device evidence.
- 0.3.1 `ui(2).log` records zero complete event lines despite explicit subscriptions. 0.3.2 uses a raw PTY (local unflushed-event regression) and bounded read-only state snapshots around stock pause/resume, plus byte/partial counters. Do not claim the PW1 cause or fix is established until retested.
- 0.3.2 `ui(3).log`: __getpagesize/_rtld_global_ro assertion during preflight, before any stock STOP. Legacy grantpt loads group/NSS modules into static glibc. Avoid grantpt/account lookup; verify kernel devpts owner/mode without changing permissions. ARM tests MUST include the target sysroot (-L); without it, missing NSS libraries conceal the crash. Release audit rejects NSS/account symbols.
- `KindlePower` owns a stock LIPC helper; power events trigger save-and-exit to Amazon UI, not in-app suspend. Keep the five-minute limit while lifecycle/recovery validation remains incomplete. Missing helper must fail preflight.
- 0.3.4 TXT keeps CVM/awesome alive (PW1 upstart/KOReader policy), temporarily soft-hides Pillow and pauses only volumd. KindleSystem bounds stock CLI calls; hide is recorded before mutation, failed show retains recovery lock. On a real power event, join then relinquish framebuffer restoration to preserve the stock sleep screen. Normal exit still restores pixels. The diagnostic retains its old STOP policy.
- The user confirmed the five-minute timeout. 0.3.4 ui(5).log confirms goingToScreenSaver, page 4 saved, joined worker, display release, exit 0, screenSaver, volumd resumed and bar show success. It also rejects the parent launcher for signal 12: normal exit succeeded, but the two-second forced-recovery handshake was not activated. 0.3.5 corrects the notifier in code; 0.3.6 later validates wake/reopen, while forced-crash recovery remains unvalidated on hardware. Historical evidence is in docs/validation/pw1-0.3.4-power-ui.txt. Use `/proc/PID/status` rather than `/proc/PID/comm` on the old kernel.

- 0.3.5 serializes setup/cleanup/Restore through `LauncherSession` flock on a permanent operation.lock inode, inherited on fd 9 and closed before child wait. Never unlink that inode. Cleanup retries contention, tokens reject delayed cleanup, and record writes precede mutations. UI/diagnostic both implement the guard; app and watchdog close fd 9 before exec. UI recovery yields pixels; failed/forced cleanup redraws Home only for active powerd, retaining records on query/redraw failure.
- Capture the verified launcher owner boot/start identity before reader startup; notify only that session, regardless of direct parent or PID environment. Log both libc and /proc parent identities. On power: atomically block new publication, notify launcher, drain in-flight publication, stop/join worker, then relinquish restoration. Do not move post-join non-atomic ownership state into a running renderer.

- 0.3.5 ui(6).log: actual reader parent is an intermediary sh, not launcher; owner verification/signaling succeeds (sent=1), page 2 saved, sleep exit and resource restoration succeed. Reopen failed per user; launcher.log empty, failed attempt absent. Preserve uncertainty about device root cause.
- 0.3.6 explicitly LOCK_UNs the inherited open-file description: closing fd 9 is insufficient when shells/helpers retain copies. Stock fork/exec paths sanitize ALL descriptors above stderr (preserve CLOEXEC exec-error pipe). A locally reproduced helper-descendant lock leak is regression-tested on host/ARM; it is not proof of the exact PW1 failure. Reader self-registers PID/start under guard; recovery uses it before verified wrapper termination. Startup waits boundedly for cleanup, recovers stale records, and logs KUAL/runner entry before native exec. Restart once when upgrading from 0.3.5; never unlink operation.lock to force release.

- 0.3.6 physical logs (07:57:05 and 07:57:47 on 2026-10-15) confirm native reader registration behind sh, page 3 saved on power, sent=1, sleep-aware cleanup, successful KUAL reopen to file list and normal Exit/cleanup. User separately confirms selecting Boas-vindas resumes the same page; that action has no supplied log. File-list startup is expected save-and-exit behavior. Do not claim in-memory sleep, forced-crash recovery or overlay-free bar behavior. Raw evidence/hashes are in docs/validation/pw1-0.3.6-checks.md.

## Misc Repo Gotchas

- POSIX TZ signs are inverted from ISO 8601 in `TimeStore::applyTimezone()`: `"UTC-1"` means UTC+1.
- `LyraTheme::drawHeader()` does not call `BaseTheme::drawHeader()`, so header changes in the base theme must be duplicated in Lyra if needed.
