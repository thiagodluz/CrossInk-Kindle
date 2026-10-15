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

### Current PW1 evidence and validation package

The diagnostic report collected on 2026-10-13 confirms Cortex-A8/ARMv7 with
VFPv3/NEON, a 2.6.31 Lab126 kernel, approximately 256 MB RAM, glibc 2.12.1,
an 8-bit `mxc_epdc_fb` framebuffer with 768-byte stride, and `cyttsp` on
`/dev/input/event0`. An uploaded copy of `/bin/sh` confirms ELF32 little-endian
ARM EABI5 and loader `/lib/ld-linux.so.3`, without a VFP-register argument ABI.
The user confirms KUAL is installed and uses it to launch KOReader.

`platform/kindle/` now contains standalone display and input backends and their
diagnostic client. `scripts/kindle/build-package.py` builds with KOReader's
`arm-kindle5-linux-gnueabi` toolchain and audits ARM EABI, static linkage and the
minimum kernel ABI.
`kual/crossink/` supplies the KUAL entry. Installation and hardware validation
instructions are in [the package guide](../platform/kindle/README.md).

The display geometry and offsets are queried at runtime; no framebuffer mode is
changed. Display update ioctls follow the PW1 Pearl layout documented by KOReader.
The user ran package 0.1.0 twice on the PW1 on 2026-10-14 and supplied both logs
plus a photo. The logs confirm successful complete and partial GC16 updates,
touch down/move/up, framebuffer restoration, exit status 0, and resumption of
both paused UI processes (`cvm` and `awesome`). The photo shows the intended
portrait orientation, grayscale ramp, checkerboard and center-touch marker.
The validation tool deliberately logs uncorrected normalized coordinates; no
axis swap or inversion is needed for the tested portrait orientation.
The full CrossInk activities, FreeRTOS compatibility, sandboxed book storage,
EPUB reading, suspend/resume and networking are not yet connected to this target.

### Hardware validation results — 2026-10-14

Both logs agree on the live interfaces:

| Property | Observed value |
| --- | --- |
| Framebuffer | `/dev/fb0`, `mxc_epdc_fb`, 758 x 1024, 8 bits per pixel |
| Memory layout | stride 768, x/y offsets 0/0, rotation field 3, mapped memory 4,718,592 bytes |
| Touch | `cyttsp`, `/dev/input/event0`, axes 53/54 (`ABS_MT_POSITION_X/Y`) |
| Axis ranges | X 0..758, Y 0..1024, `BTN_TOUCH` available |
| Earlier run | 53 down/move/up events; 15 successful display updates; normal exit 0 |
| Latest run | 25 down/move/up events; 9 successful display updates; normal exit 0 |

The final release coordinate for each requested target in the latest run was:

| Target | Raw X | Raw Y |
| --- | ---: | ---: |
| 1: top left | 41 | 95 |
| 2: top right | 686 | 68 |
| 3: bottom left | 57 | 959 |
| 4: bottom right | 679 | 963 |
| Center | 388 | 508 |
| Exit | 370 | 942 |

All six releases fall within the intended target regions. Each contact has one
down and one up; intermediate move events account for normal finger movement.
Neither run reports an update failure, dropped input events or supervisor timeout.
Both ended through the Exit button (`interrupted=0`), followed by a successful
full refresh restoring the snapshot and UI-process resumption. These runs do
not establish the 90-second automatic-exit or forced-crash watchdog behavior on
hardware, other orientations, long reading sessions or suspend/resume.

The next implementation milestone is the reader-facing HAL and compatibility
layer: sandboxed POSIX storage, joinable tasks/render synchronization, and a
minimal CrossInk activity using the validated display and input backends.


### Native TXT preview 0.2.0 — 2026-10-14

This is an intermediate integration, awaiting device validation. The target now
compiles the original GfxRenderer, EpdFont/font cache/decompressor, built-in Bitter
and Lexend fonts, FreeInkUI from the pinned SDK, I18n and Txt. A small PortApp
provides a bounded folder browser and TXT preview. It does not yet use
ActivityManager, UITheme, TxtReaderActivity, settings stores or the EPUB engine.
No complete FreeRTOS task/notification API is claimed: the preview has its own
joinable pthread render owner and a narrow mutex/time compatibility surface.

NativeDisplay preserves the renderer's canonical landscape buffer (1024x758,
1bpp) and converts its portrait output to physical 758x1024, 8bpp pixels. Evdev
coordinates pass through the exact inverse transform before SDK hit testing.
Grayscale capability is disabled; full and partial updates remain synchronous.

NativeStorage implements the existing HalStorage/HalFile interface under a root
directory descriptor, walks components with openat/O_NOFOLLOW, rejects parent
traversal, and writes progress by fsync plus rename. Renderer and file state have
one worker owner; main only enqueues fixed-size touch jobs and joins before
framebuffer restoration. Finite waits use CLOCK_MONOTONIC.

The UI has bounded TXT/entry/page sizes, five-minute sessions and a six-minute
supervisor watchdog. UI mode pauses volumd when present to prevent concurrent
USB mounting. Suspend, light control, battery information and networking are
unavailable; the normal Exit path returns to Amazon UI. Actual hardware checks
for the new renderer/touch path, volumd handling and progress remain required.

The next milestone is integrating the original activity lifecycle and required
FreeRTOS notification semantics, followed by EPUB parsing/layout/cache. Shared
changes in Memory, GfxRenderer and Txt are gated by CROSSINK_PLATFORM_KINDLE;
the SDK gitlink and ESP32 implementations remain unchanged.


### TXT preview 0.2.1 hardware validation — 2026-10-14

The user's photo shows upright Portuguese text on page 4/5 and aligned page
controls. UI logs from 20:51 and 20:52 confirm opening, page advance, saving,
reopening page 2, and reopening page 3 after a new launch. The last normal run
saved page 4. Both runs exited with code 0, joined the worker, restored the
framebuffer and resumed volumd, cvm and awesome.

The 20:59:34 run recognized a five-second top-right hold, joined and restored
normally, then resumed all three services. This tests the gesture path, not a
blocked render worker. The user separately confirmed Previous and the return to
the Amazon UI after five minutes. Those two confirmations are user reports,
without new complete logs. Forced-crash recovery and power behavior remain
unproven on the device.

### Power-event handoff 0.3.x — device validation pending

`KindlePower` owns a read-only `lipc-wait-event -m -s 0 com.lab126.powerd <explicit-event-list>`
subscriber. The native input loop consumes bounded lines from a nonblocking terminal
and recognizes sleep/suspend/wake events independently of renderer ownership.
A recognized event asks the launcher for a two-second shutdown deadline, stops
and joins the render worker (which saves progress), restores the display, and
reaps the subscriber before process exit lets the launcher resume the stock UI.
Subscriber failure also exits; there is no unmonitored fallback session.

The helper receives Linux PR_SET_PDEATHSIG before exec and verifies its parent,
so it is killed if the reader is forced to exit. The preflight exercises helper
startup without display writes or exclusive input capture. No powerd settings,
sysfs power states or front-light values are written. The five-minute preview
and six-minute watchdog remain. This phase yields control to Amazon for sleep;
after waking, launch KUAL again and reopen the TXT to resume. It does not keep a
live reading session suspended in memory.

References checked for this phase:
- KOReader's Kindle power lifecycle and event names:
  https://github.com/koreader/koreader/blob/master/frontend/device/kindle/device.lua
- KOReader's powerd state handling:
  https://github.com/koreader/koreader/blob/master/frontend/device/kindle/powerd.lua
- An open implementation using the stock command and its event-line format:
  https://github.com/aleung27/kindle-eink-photo-frame

Physical test: save a known TXT page, briefly press power, wake, reopen through
KUAL and verify the page, controls and stock UI. Supply ui.log and ui.previous.log.
A new firmware missing the helper should fail preflight before pausing stock UI.

### Power button did not react on 0.3.0 — 2026-10-14

The user reported that briefly pressing power had no visible effect, then that
the Kindle entered sleep immediately after leaving CrossInk. The supplied
`ui(1).log` shows both subscriber startups, no recognized power event or helper
failure, and a normal Exit followed by resumption of volumd/cvm/awesome. This
suggests a pending stock sleep transition while the UI is paused; it does not
prove whether the subscription missed an event or the stock event was delayed.
The 0.3.1 subscriber replaces `*` with the exact explicit event list used by
KOReader's `input/input-kindle.h`, and logs every complete raw event and the
received-line count. This addresses a compatibility difference and improves
observability; it is not proof that the hardware failure is fixed.

Primary reference: https://github.com/koreader/koreader-base/blob/master/input/input-kindle.h


### No event lines on 0.3.1 — terminal transport in 0.3.2

The user's `ui(2).log` identifies 0.3.1, both explicit subscriber startups and
`lines_received=0` at normal shutdown. Thus named subscriptions did not resolve
the device failure. The log cannot establish whether the stock helper buffered
its output or the paused framework delayed event publication.

A local reproduction using an unflushed stdout line receives nothing through a
pipe but delivers promptly through a terminal. The 0.3.2 backend gives the stock
helper a raw POSIX pseudoterminal so default terminal line buffering can flush
an event immediately. This is a tested transport fix, not proof of the PW1 root
cause. It retains nonblocking reads, bounded parsing and owned helper cleanup.
No controlling terminal is assigned and stderr remains in the session log.

For the next hardware test, the launcher logs read-only powerd `state` before
pausing stock processes, after pausing them and before resuming on normal exit.
The native query has a one-second monotonic deadline, bounded output and child
reaping, and is only run in separate single-threaded processes. These snapshots
can distinguish a powerd state transition without received event lines from an
unchanged state. Queries do not write properties or change the stock pause policy.
The event counters additionally distinguish zero bytes from an incomplete line.

References: KOReader `frontend/device/kindle/powerd.lua:getPowerdState`,
https://man7.org/linux/man-pages/man3/posix_openpt.3.html and
https://www.gnu.org/software/libc/manual/html_node/Buffering-Concepts.html.


### 0.3.2 preflight abort — NSS-free PTY preparation in 0.3.3

The supplied `ui(3).log` shows version 0.3.2 passing display and touch identity
checks, then aborting on `__getpagesize`'s `_rtld_global_ro._dl_pagesize != 0`
assertion. There is no `paused` line: the stock UI was never stopped by this run.

The newly introduced `grantpt` in glibc 2.12 resolves the `tty` group through
`__getgrnam_r`, bringing the NSS loader into the static binary. Static glibc
programs can load shared NSS modules and the system's shared libc/loader; the
upstream glibc discussion documents exactly this page-size assertion in that
path. The 0.3.2 binary includes grantpt/group/NSS symbols; 0.3.1 did not.

The 0.3.3 backend keeps the terminal transport, omits grantpt on this Linux
backend and verifies the devpts slave's character-device type, effective owner,
owner read/write bits and absence of group-read/other access. It refuses an
unexpected configuration without chmod/chown, consulting group names or
modifying system libraries. Unlock/name/termios operations remain. The supplied
PW1 mount report records devpts with gid=5 and mode=620.

Regression evidence: the old ARM monitor works under QEMU without target shared
libraries, but crashes with `-L <kindle5-sysroot>`. The trace opens nsswitch.conf,
libnss_files.so.2, libc.so.6 and ld-linux.so.3 before SIGSEGV. This reproduces a
legacy loading failure, not the exact on-device assertion. The corrected ARM
monitor passes in that same environment, with none of those shared libraries
opened. `check-power.py --qemu ... --sysroot ...` now exercises this case, and
UI release auditing rejects account/NSS symbols before stripping the executable.
These checks do not replace a hardware opening/power-button retest.

Primary references:
- https://github.com/bminor/glibc/blob/glibc-2.12.2/sysdeps/unix/grantpt.c
- https://sourceware.org/legacy-ml/libc-alpha/2010-08/msg00051.html
- https://sourceware.org/pipermail/glibc-cvs/2020q4/070779.html
