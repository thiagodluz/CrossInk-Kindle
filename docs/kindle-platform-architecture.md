# Kindle Platform Architecture

This document maps the inherited CrossInk codebase and defines the initial platform boundary for the first-generation Kindle Paperwhite (PW1). It is a design baseline, not a claim that the APIs below already run on a Kindle.

The central decision is to preserve the reader application, activities, EPUB engine, and their existing HAL-facing calls. The Kindle port will replace the platform implementations behind those calls, following the same compilation pattern already used by the native simulator.

## Scope and non-goals

The first Kindle milestone is a safe local reader. It does not require Wi-Fi, USB Mass Storage, OTA firmware updates, an ESP32 reset path, or the original board profiles.

Until real-device diagnostics confirm each system interface, this document specifies contracts and test points rather than device paths or writable system locations.

## Existing architecture

```text
CrossInk application
  activities, reader, EPUB engine, settings, UI policy
          │
          │ HalDisplay / HalGPIO / HalStorage / HalPowerManager / HalClock
          ▼
FreeInk SDK and ESP32 implementation
  EInkDisplay, InputManager, SdFat, BatteryMonitor, Rtc, board configuration
          ▼
ESP32 boards and their e-ink, touch, SD, battery, and power hardware
```

The Kindle target keeps the top layer and replaces the two lower layers with Linux/Kindle implementations:

```text
CrossInk application
          │
          │ same HAL-facing API, capability-gated features
          ▼
Kindle platform compatibility layer
  framebuffer/update API, evdev touch, POSIX files, threads, logging, power bridge
          ▼
Kindle Paperwhite 1 Linux
```

## Code map

| Area | Current ownership | Kindle treatment | Evidence |
| --- | --- | --- | --- |
| Application start and global loop | `src/main.cpp` creates the renderer, logical input manager, and activity manager; it also contains ESP32 boot, reset, sleep, NVS, and board handling. | Keep application construction and activity flow; split ESP32 boot and restart code behind the platform compatibility layer. | [`main.cpp:123–125`](../src/main.cpp#L123-L125), [`main.cpp:1181`](../src/main.cpp#L1181), [`main.cpp:1609`](../src/main.cpp#L1609) |
| Screen flow | Activities and `ActivityManager` own navigation, lifecycle, rendering requests, and the render task. | Keep unchanged where possible. Provide a Linux implementation of the FreeRTOS task, notification, and mutex API used here. | [`ActivityManager.h:71–99`](../src/activities/ActivityManager.h#L71-L99), [`ActivityManager.cpp:225–270`](../src/activities/ActivityManager.cpp#L225-L270) |
| Reader and book engine | `src/activities/reader/`, `lib/Epub/`, `lib/Txt/`, and `lib/Xtc/`. | Preserve for the initial local-EPUB target. Adapt only code that assumes ESP32 storage, display, or task APIs. | [repository guide](../AGENTS.md#architecture-and-fast-navigation) |
| Rendering and UI | `GfxRenderer`, FreeInkUI, `UITheme`, fonts, and image utilities. | Preserve logical drawing; the Kindle display backend translates its framebuffer and refresh calls to the PW1 panel. | [`HalDisplay.h:34–104`](../lib/hal/HalDisplay.h#L34-L104) |
| Input policy | `MappedInputManager` and activities consume logical buttons and normalized touch. | Keep logical actions. Feed them from Linux touchscreen events through `HalGPIO`. | [`HalGPIO.h:35–124`](../lib/hal/HalGPIO.h#L35-L124) |
| Files and application data | `HalStorage` wraps SdFat and serializes SD access. | Reimplement with a private POSIX application root while preserving the file and locking contract. | [`HalStorage.h:23–94`](../lib/hal/HalStorage.h#L23-L94) |
| Power, light, battery, clock | HAL wrappers are board-specific. | Provide read-only capability probing first; add write or suspend behavior only after PW1 validation. | [`HalPowerManager.h:21–86`](../lib/hal/HalPowerManager.h#L21-L86), [`HalClock.h`](../lib/hal/HalClock.h) |
| Network and transfers | `src/activities/network/` and `src/network/`. | Compile out or capability-gate for the first local-reader milestone. Revisit after safe display, input, storage, and suspend behavior exist. | [`platformio.ini:189–225`](../platformio.ini#L189-L225) |
| ESP32-specific platform helpers | `src/platform/`, ESP headers, NVS, SPI, board detection, USB/OTA paths. | Exclude from the Kindle target; do not emulate firmware update or raw USB-drive ownership. | [`main.cpp:20`](../src/main.cpp#L20), [`main.cpp:68–78`](../src/main.cpp#L68-L78) |

## What can be reused

The reader's activities, EPUB parser/layout/cache, settings model, UI policy, logical input mapping, and file-oriented application flows are the intended reuse candidates. They already depend primarily on `Hal*` classes rather than calling board drivers directly.

The native simulator is the proof that this is feasible as an architectural approach: its PlatformIO profile excludes the local `hal` library and supplies a host-side simulator package in its place. See [`platformio.ini:185–225`](../platformio.ini#L185-L225). The Kindle target should use the same principle, but it must not inherit simulator assumptions such as SDL display output, `./fs_` storage, permanently full battery, or fake sleep.

## Existing hardware boundary

| Existing API | Current board-backed dependency | Kindle backend responsibility |
| --- | --- | --- |
| `HalDisplay` | `EInkDisplay`, panel waveform controls, 1-bit 800 × 480 framebuffer | Own a Kindle-compatible framebuffer, convert to the PW1 pixel representation, request complete or partial updates, and restore the Amazon interface after exit. |
| `HalGPIO` | `InputManager`, GPIO buttons, touch controller, USB state | Read Linux input events, normalize touch coordinates to 0–1, emit touch lifecycle and logical input data, and provide a clean stop mechanism. |
| `HalStorage` / `HalFile` | SdFat and an SD-access mutex | Restrict all files to a dedicated application directory, expose the current file operations, serialize access when needed, and avoid Amazon/KOReader data. |
| `HalPowerManager` | battery monitor, clock frequency, deep sleep | Report only confirmed battery/charging data; initially map sleep to a safe application suspend/exit path rather than an ESP32 deep-sleep equivalent. |
| `HalFrontlight` | board PWM front light | Start disabled until the PW1 interface and safe restore behavior are diagnosed. |
| `HalClock` | RTC and SNTP | Use Linux time if available; do not make NTP a requirement for offline reading. |
| `HalSystem` | ESP32 panic/reset diagnostics | Redirect logs and crash information to the app directory; never reset the Kindle system to recover the application. |

The target uses one platform-owned implementation per responsibility. Activities must continue to consume `MappedInputManager::Button` and HAL APIs; they must not gain direct Linux, framebuffer, evdev, or Amazon-service calls.

## ESP32 and FreeRTOS replacement map

| Dependency | Where it matters | Kindle replacement |
| --- | --- | --- |
| Arduino core | pervasive types, `millis()`, `delay()`, serial logging, and application entry | a small compatibility header backed by POSIX time, sleep, and a file logger; migrate isolated code to standard C++ only when it reduces the compatibility surface. |
| ESP reset/deep sleep | boot/sleep/restart sections of `main.cpp` | application-controlled shutdown or launcher return. A normal CrossInk failure must never reboot the Kindle. |
| NVS, SPI, board detection | ESP32 startup and board selection | omit from Kindle builds; configuration lives below the dedicated application directory. |
| `xTaskCreatePinnedToCore`, task notifications | rendering loop in `ActivityManager` | one `std::thread` or pthread worker, condition-variable notification, and no meaningful core affinity. |
| FreeRTOS mutex/critical sections | `RenderLock`, storage locking, background tasks | recursive mutexes plus a narrow compatibility layer with the same ownership and shutdown semantics. |
| FreeRTOS delays/timers | activity timing and background work | monotonic Linux clock and condition-variable or timer-based waits. |
| Wi-Fi/USB/OTA firmware services | network, transfer, updater activities | capability-gate as unavailable in the initial PW1 build. Do not use ESP32 OTA/update code for application packaging. |

The render worker is the most important compatibility requirement. `ActivityManager` creates it at [`ActivityManager.cpp:225–237`](../src/activities/ActivityManager.cpp#L225-L237), waits for notifications at [`ActivityManager.cpp:246–270`](../src/activities/ActivityManager.cpp#L246-L270), and relies on the mutex ownership checks around synchronous rendering at [`ActivityManager.cpp:954–1024`](../src/activities/ActivityManager.cpp#L954-L1024). The Kindle compatibility layer must preserve those semantics before the CrossInk interface is launched.

## Simulator replacements to reuse as patterns

The simulator is a reference implementation, not the Kindle backend:

| Simulator component | Useful pattern | Must be replaced for Kindle |
| --- | --- | --- |
| `HalDisplay` | separates rendering from presentation and accepts runtime orientation | SDL output and its simulated grayscale model |
| `HalGPIO` | normalizes pointer input into tap, hold, release, and swipe events | SDL event polling and keyboard button emulation |
| `HalStorage` | maps reader file operations to an operating-system filesystem | `./fs_` paths and the simulator's multiple-reader assumptions |
| `HalPowerManager` | allows the rest of the app to run without a battery backend | fixed 100% battery and no-op power saving |
| `freertos/` headers | maps tasks, notifications, and recursive mutexes to host threads | its deliberately simplified lifecycle and detached-thread behavior |
| `BoardConfig.h` | exposes device capabilities independently of the app | Xteink/Sticky board identities and their dimensions |

The simulator's simplified power behavior is explicit: its `HalPowerManager::getBatteryPercentage()` returns 100 and `setPowerSaving()` is empty. That is acceptable for a desktop preview, but it is not a valid PW1 power implementation.

## Initial Kindle platform contracts

The first implementation will live under a Kindle-specific platform directory and provide headers and sources ahead of the ESP32 HAL in the Kindle build include order. The exact build system is a later Phase 3 decision; the API shape below is intentionally based on the current app-facing HAL surface.

### Display

`KindleDisplayBackend` will back the existing `HalDisplay` API.

Required first-pass operations:

- initialize after the launcher has safely yielded the screen;
- report physical dimensions at runtime;
- provide a canonical application framebuffer;
- present full-screen black, white, checkerboard, text, and grayscale-ramp tests;
- request full and partial e-ink refreshes, with unsupported modes reported explicitly;
- wait for an in-flight update before the buffer is changed;
- release or restore the screen on normal exit and leave a recoverable path after forced exit.

The diagnostic phase must determine the PW1 framebuffer format, stride, update ioctl/API, ownership rules, and whether a 1-bit application buffer can be converted safely to the panel's grayscale format. The port must not hard-code 800 × 480. The PW1's intended 758 × 1024 geometry becomes authoritative only after device verification.

### Input

`KindleInputBackend` will back `HalGPIO` and keep CrossInk's normalized coordinate convention:

- consume one or more Linux input devices without requiring root beyond the established launcher environment;
- normalize raw coordinates to `nx`/`ny` in the inclusive 0–1 range;
- deliver down, move, release, tap, long press, and swipe;
- apply orientation at one defined boundary, never both in the driver and activity;
- coalesce noisy or duplicate events so one physical tap produces one logical action;
- support cancellation and a wakeable shutdown path.

The PW1 is a single-touch device, so multitouch and a capacitive home-key contract will initially advertise unavailable rather than synthesize gestures.

### Storage and logging

`KindleStorageBackend` will back `HalStorage` and `HalFile`.

- all CrossInk state, caches, logs, and books selected by the application live under one dedicated CrossInk root;
- application code receives relative logical paths, while the backend rejects traversal outside that root;
- no path may point into Amazon reader data or KOReader data by default;
- writes use a temporary file plus rename where the Kindle filesystem supports it;
- a process-local mutex protects the same serialized-access contract currently used by `HalStorage`;
- logs are size-limited, versioned, and safe to copy over USB.

The final root and launcher-visible permissions are Phase 2 findings, not assumptions in this document.

### Tasks and timing

`KindleTaskCompat` will provide the narrow FreeRTOS subset currently used by the application:

- task creation and joinable shutdown;
- task notification with no lost wakeups;
- recursive render mutex ownership checks;
- critical-section protection for the small shared state used by `ActivityManager`;
- monotonic millisecond timing and bounded waits.

Threads will be joined before storage or display teardown. The simulator's detach-on-delete behavior must not be copied because it makes device shutdown and resource release unreliable.

### Power, light, suspend, and resume

`KindlePowerBackend` starts in a conservative mode:

- expose “unknown” rather than fabricated battery or charging values;
- do not alter CPU frequency;
- treat CrossInk's initial sleep request as a save-and-yield operation, not a full-device deep sleep;
- persist book position and release display/input resources before the Kindle suspends;
- restore only after the Kindle system makes the application runnable again;
- keep front-light writes disabled until their correct interface and restoration behavior have been proven on PW1 hardware.

This contract deliberately prevents the port from using ESP32 reset/deep-sleep semantics on the Kindle.

### Network

The initial Kindle capability profile disables Wi-Fi-dependent activities, USB Mass Storage, firmware flashing, and OTA. Local EPUB reading must work without them. A later profile can expose network support only when the Kindle-managed connection, certificates, clocks, and failure behavior have been tested.

## Proposed initial capability profile

| Capability | Initial PW1 value | Reason |
| --- | --- | --- |
| Touch | enabled after evdev validation | Core navigation depends on it. |
| Physical buttons | unavailable unless diagnostics find a usable event source | The target is primarily touch-driven. |
| Multitouch | disabled | PW1 hardware is single-touch. |
| Display | enabled only after test-pattern validation | Incorrect update calls can leave the screen unusable. |
| Grayscale | unknown, then capability-gated | Panel support and update path need measurement. |
| Local storage | enabled after sandbox/path validation | Required for logs, settings, and EPUBs. |
| Battery and charging | unknown initially | No simulated telemetry. |
| Front light | disabled initially | Write path needs hardware confirmation. |
| Suspend/resume | disabled until safe save-and-yield test passes | Avoid interaction with Amazon power management. |
| Wi-Fi, OPDS, sync | disabled | Not needed for the first reader milestone. |
| USB drive, OTA, firmware flashing | disabled permanently for the Kindle target | These are ESP32 firmware concepts, not Kindle application features. |

## Implementation sequence

1. Build a tiny native Kindle executable with the compatibility/logging foundation.
2. Use a standalone display diagnostic to prove the update path and safe exit.
3. Add a standalone input diagnostic and verify normalized coordinates.
4. Add the storage sandbox and persistent log.
5. Implement the task compatibility layer and launch a minimal CrossInk activity.
6. Only then connect the full CrossInk interface and local EPUB workflow.

The next repository work item is the read-only PW1 diagnostic script. Its results will replace the explicitly unknown items in this document before any display or power backend is written.
