# CrossInk for Kindle Compatibility Matrix

This document tracks the intended compatibility of inherited CrossInk features on the first-generation Kindle Paperwhite (PW1, 2012).

The status describes the **Kindle port**, not upstream CrossInk. Until a feature is implemented and tested on a real PW1, it is not considered supported.

## Status and target definitions

| Status | Meaning |
| --- | --- |
| Supported | Implemented and verified on a real PW1. |
| Planned | Intended for the port but not yet verified on a real PW1. |
| Unsupported | Excluded from the current port scope or blocked by the selected approach. |
| Not applicable | Tied to hardware, firmware installation, or behavior that does not exist on the PW1. |

| Target | Meaning |
| --- | --- |
| Prototype | Required to prove that native code, display, and input work. |
| Alpha | Required for useful local EPUB reading. |
| Beta | Required for practical daily use or the planned network feature set. |
| Later | Retained as a possible post-beta feature. |
| — | No implementation target. |

No runtime feature is marked Supported yet because no CrossInk Kindle executable has been tested on the target device.

## Platform and hardware

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| Native application startup | Planned | Prototype | Build an ARM Linux executable for the PW1 userspace. |
| KUAL or equivalent launcher | Planned | Prototype | Launch beside the Amazon reader from an isolated application directory. |
| Safe exit to Amazon interface | Planned | Prototype | Restore display ownership and return to the normal Kindle UI. |
| Application removal and recovery | Planned | Prototype | Provide scripts that remove only port-owned files. |
| Versioned diagnostic logs | Planned | Prototype | Replace ESP32 serial logging with redacted files on internal storage. |
| Full e-ink refresh | Planned | Prototype | Add a Kindle display backend and verify waveform behavior on hardware. |
| Partial e-ink refresh | Planned | Prototype | Required for usable menus and page turns. |
| Grayscale rendering | Planned | Alpha | Adapt CrossInk image and antialiasing output to the Kindle framebuffer. |
| Portrait orientation | Planned | Prototype | Initial fixed orientation. |
| Landscape and inverted orientations | Planned | Beta | Add after coordinate and refresh mapping are stable. |
| Capacitive touchscreen | Planned | Prototype | Read Linux input events and map them to CrossInk logical actions. |
| Multitouch gestures | Planned | Later | Depends on the PW1 input driver's reported contacts. |
| Physical front buttons | Not applicable | — | The PW1 has no CrossInk-style front navigation buttons. |
| Physical side/volume buttons | Not applicable | — | The PW1 has no CrossInk-style side navigation buttons. |
| Power-button integration | Planned | Beta | Coordinate short press, suspension, wake, and application state. |
| Front-light brightness | Planned | Beta | Use the PW1 system interface and restore the previous value on exit. |
| Warm/cool front-light control | Not applicable | — | The first-generation Paperwhite has no adjustable color-temperature light. |
| Battery percentage | Planned | Beta | Read the Kindle power-supply interface. |
| Charging status | Planned | Beta | Read and display the system charging state. |
| Suspend and resume | Planned | Beta | Save state, release resources, and restore display and input after wake. |
| Clock and date display | Planned | Beta | Use the Kindle system clock. |
| Accelerometer/tilt page turn | Not applicable | — | The PW1 has no CrossInk tilt-control hardware profile. |
| Application screenshots | Planned | Later | Save the application framebuffer without using ESP32 shortcuts. |
| SD-card storage | Not applicable | — | The PW1 uses internal storage rather than a removable SD card. |
| Internal book and data storage | Planned | Prototype | Use an isolated folder on the user-visible Kindle storage. |
| CrossInk USB Drive mode | Not applicable | — | Use the Kindle's native USB storage behavior. |
| ESP32 watchdog/crash flow | Not applicable | — | Replace it with Linux process exit and diagnostic logging. |
| ESP32 board detection | Not applicable | — | Use an explicit PW1 platform profile. |
| ESP32 firmware OTA | Not applicable | — | The port is an application, not Kindle firmware. |
| Application updater | Planned | Later | A Kindle-specific package update path will be designed after releases exist. |

## Application interface and library

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| CrossInk activity/navigation stack | Planned | Alpha | Reuse after Linux task, display, input, and storage adaptation. |
| Home screen | Planned | Alpha | Adapt layout to 758 × 1024. |
| File browser | Planned | Alpha | Browse approved directories in internal storage. |
| Large-folder indexed browsing | Planned | Beta | Retain if still useful with the Kindle filesystem and memory profile. |
| Recent Books list | Planned | Alpha | Store recent entries in the port's data directory. |
| Recent Books grid | Planned | Beta | Adapt cover thumbnails and touch layout. |
| Book covers and thumbnails | Planned | Alpha | Move caches from SD-card assumptions to Kindle storage. |
| Home themes: Minimal, Dashboard, and Lyra Carousel | Planned | Later | Port after the basic home screen and reading flow are stable. |
| Settings screens | Planned | Alpha | Hide controls that do not apply to the PW1. |
| Touch navigation shortcuts on Home | Planned | Beta | Revalidate swipe directions and gesture priority. |
| Long-press file actions | Planned | Beta | Map to the Kindle touchscreen backend. |
| Move finished books to a Read folder | Planned | Later | Preserve or migrate book metadata and caches when moving. |
| Quick Resume | Planned | Alpha | Restore the last book and precise position. |
| Quick Actions menu | Planned | Later | Requires stable Kindle input and capability filtering. |
| Quick Lock | Planned | Later | Lock only CrossInk input while retaining a reliable unlock and exit path. |
| Internationalized UI | Planned | Later | Reuse translations after Kindle-specific strings are defined. |
| Boot screen replacement | Not applicable | — | The application will not replace the Kindle boot process. |
| CrossInk sleep screens | Planned | Later | Consider only after suspend and resume are reliable. |
| Custom sleep images and cover sleep screen | Planned | Later | Must coexist safely with the Kindle's native sleep handling. |

## Book formats and rendering

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| EPUB reading | Planned | Alpha | Initial supported book format. |
| TXT reading | Planned | Later | Reuse after EPUB establishes the platform and persistence layers. |
| Markdown text handling | Planned | Later | Evaluate with the inherited TXT reader. |
| XTC and XTCH reading | Planned | Later | Retain only if the pre-rendered format remains useful on 758 × 1024. |
| EPUB metadata and table of contents | Planned | Alpha | Reuse parser and move caches to Kindle storage. |
| Incremental EPUB indexing | Planned | Alpha | Validate under the PW1 memory and storage profile. |
| Full-section EPUB indexing | Planned | Beta | Retain as an alternative after performance testing. |
| CrossInk Default render mode | Planned | Alpha | Initial rendering path. |
| Balanced and Light render modes | Planned | Beta | Retain fallbacks for complex books. |
| Automatic Safe Mode fallback | Planned | Beta | Adapt restart/recovery behavior for a Linux application. |
| Embedded publisher CSS | Planned | Alpha | Validate layout and cache behavior. |
| EPUB images | Planned | Beta | Convert and display grayscale images through the Kindle backend. |
| Tables and horizontal rules | Planned | Beta | Verify layout on the PW1 dimensions. |
| Underline, strikethrough, and redaction rendering | Planned | Beta | Reuse text-run styles after basic layout works. |
| Ruby annotations | Planned | Later | Validate CJK layout and fonts. |
| Right-to-left and mixed-direction text | Planned | Later | Retain shaping and ordering support with compatible fonts. |
| Hyphenation | Planned | Beta | Reuse language tries and validate Portuguese and other supported languages. |
| EPUB links and chapter navigation | Planned | Alpha | Map link selection to touch input. |
| Footnote previews and return | Planned | Beta | Requires reliable links, overlays, and position restoration. |
| Publisher page numbers | Planned | Later | Retain when present in EPUB metadata. |
| Stable CrossInk page numbers | Planned | Later | Retain support for optimized EPUB location metadata. |
| Image-file viewer and previews | Planned | Later | Evaluate PNG, BMP, JPEG, GIF, and WebP decoders separately. |
| PDF and comic-book formats | Unsupported | — | They are outside the inherited CrossInk reader scope. |

## Typography and reader layout

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| Built-in Lexend Deca and Bitter fonts | Planned | Alpha | Verify licenses, glyph coverage, rasterization, and visual weight. |
| Inter interface font | Planned | Alpha | Adapt sizes for the higher-resolution display. |
| Additional custom font families | Planned | Beta | Load from internal storage instead of an SD card. |
| Downloadable `.cpfont` packages | Planned | Later | Reuse only if the binary format remains suitable. |
| Separate dictionary font | Planned | Later | Depends on dictionary and custom-font support. |
| Font size selection | Planned | Alpha | Recalculate the point-size mapping for the PW1 display. |
| Line spacing | Planned | Alpha | Reuse layout setting. |
| Word spacing | Planned | Beta | Reuse EPUB-specific levels. |
| Vertical and horizontal margins | Planned | Alpha | Tune defaults for 758 × 1024. |
| Paragraph alignment and justification | Planned | Alpha | Validate spacing and punctuation. |
| Force Paragraph Indents | Planned | Beta | Reuse after paragraph layout is stable. |
| Focus Reading | Planned | Beta | Reuse text transformation and reindexing flow. |
| Guide Dots | Planned | Later | Reuse after normal word spacing and layout are stable. |
| Dark Reader Mode | Planned | Later | Requires acceptable full-refresh behavior and ghosting tests. |
| Status-bar customization | Planned | Beta | Adapt fields and touch-to-hide behavior. |
| Reader image-rendering options | Planned | Beta | Map to the Kindle grayscale modes. |
| Per-book reader settings | Planned | Alpha | Store separately from global defaults. |

## Reading, navigation, and persistence

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| Tap and swipe page turns | Planned | Alpha | Initial reader controls. |
| Configurable tap zones and inverted taps | Planned | Beta | Add after basic page turns are reliable. |
| Pinch to resize font | Planned | Later | Depends on confirmed PW1 multitouch reporting. |
| Two-finger swipe actions | Planned | Later | Depends on confirmed PW1 multitouch reporting. |
| Touch-to-hide status bar | Planned | Beta | Reuse after status-bar layout is adapted. |
| Reader menu and in-book options | Planned | Alpha | Initial settings access. |
| Chapter selection | Planned | Alpha | Use EPUB table of contents. |
| Auto Page Turn and per-book interval | Planned | Later | Replace FreeRTOS timer behavior with Linux timing. |
| Time-left estimates | Planned | Later | Retain reading-pace model after statistics are stable. |
| Bookmarks | Planned | Beta | Store outside disposable render caches. |
| Text clippings and highlights | Planned | Later | Adapt touch selection and internal-storage export. |
| Kindle-style `My Clippings.txt` export | Planned | Later | Use a port-owned or explicitly documented export path. |
| Dictionary lookup | Planned | Later | Port index access, touch selection, and definition UI. |
| Dictionary history and chained lookup | Planned | Later | Follows basic dictionary support. |
| Per-book dictionary selection | Planned | Later | Follows custom-font and dictionary support. |
| Reading position persistence | Planned | Alpha | Save on safe intervals, layout changes, exit, and suspend. |
| Global and per-book settings persistence | Planned | Alpha | Replace SD-specific paths without changing data ownership. |
| Recent-books persistence | Planned | Alpha | Maintain a recoverable, versioned store. |
| EPUB render caches | Planned | Alpha | Preserve cache versioning and invalidation rules. |
| Clear Reading Cache | Planned | Beta | Delete only rebuildable cache data. |
| Reading statistics | Planned | Later | Sessions, reading time, pages turned, pace, and completed books. |
| Mark book as finished | Planned | Later | Integrate with statistics and optional Read-folder move. |
| End-of-book suggestions | Planned | Later | Depends on library and finished-book behavior. |

## Network, transfer, and synchronization

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| Use an existing Kindle Wi-Fi connection | Planned | Beta | Rely on the Kindle system network configuration. |
| Join and manage Wi-Fi inside CrossInk | Unsupported | — | Initial port will not store or manage Wi-Fi credentials. |
| Create an open CrossInk hotspot | Unsupported | — | Excluded from the current Kindle application scope. |
| OPDS browsing and multiple catalogs | Planned | Beta | Reuse client logic with Kindle networking and HTTPS support. |
| OPDS search and book download | Planned | Beta | Save atomically and recover from interrupted downloads. |
| KOReader-compatible progress sync | Planned | Beta | High-priority interoperability feature. |
| Filename- and hash-based document identity | Planned | Beta | Validate against EPUBs shared with KOReader and CrossInk devices. |
| Sync conflict and failure handling | Planned | Beta | Never overwrite a valid local position after network failure. |
| Built-in HTTP file manager | Planned | Later | Reassess need and security on the Kindle platform. |
| Browser upload, download, rename, move, and delete | Planned | Later | Depends on the HTTP file manager. |
| Browser settings management | Planned | Later | Depends on the HTTP server and a safe authentication decision. |
| WebDAV server | Planned | Later | Reassess after basic transfers work. |
| Calibre Wireless upload | Planned | Later | Reuse protocol logic if dependencies fit the PW1. |
| In-browser EPUB optimizer | Planned | Later | Browser-side code may remain usable independently of the Kindle backend. |
| Inky firmware flashing | Not applicable | — | ESP32 firmware images cannot be installed on a Kindle. |
| Inky font and EPUB preparation tools | Planned | Later | May remain useful as external preparation tools. |
| ESP-NOW Nearby Position Sync | Not applicable | — | The Kindle Wi-Fi hardware does not provide the ESP-NOW transport used by CrossInk. |
| Alternative local-network position transfer | Planned | Later | Could preserve the feature through a different transport. |
| ESP-NOW Nearby File Transfer | Not applicable | — | The inherited transport is hardware-specific. |
| Alternative local-network file transfer | Planned | Later | Could use ordinary Wi-Fi after network support is stable. |
| ESP-NOW Reading Stats Sync | Not applicable | — | The inherited transport and MAC-based device records are ESP32-specific. |
| Alternative reading-stats synchronization | Planned | Later | Reassess after local statistics exist. |
| Network time setup | Planned | Beta | Prefer the Kindle system clock and synchronization services. |

## Packaging and releases

| Feature | Status | Target | Kindle approach or reason |
| --- | --- | --- | --- |
| Device-model verification | Planned | Alpha | Refuse to run an install script on unrecognized models. |
| Jailbreak/environment verification | Planned | Alpha | Detect required launcher and permissions. |
| Reversible installation | Planned | Alpha | Keep backups and application-owned paths explicit. |
| Complete uninstaller | Planned | Alpha | Remove the port while preserving books and unrelated applications. |
| Automated ARM build | Planned | Alpha | Pin the toolchain and source commits. |
| GitHub Actions build artifact | Planned | Beta | Produce traceable packages after local cross-compilation works. |
| Alpha release | Planned | Alpha | Local EPUB reading, persistence, safe exit, and documented recovery. |
| Beta release | Planned | Beta | Add power stability and the selected network features. |
| Stable PW1 release | Planned | Later | Requires prolonged real-device validation. |
| Other Kindle models | Unsupported | — | Each model requires a separate profile and hardware validation before entering scope. |

## Promotion rules

A feature moves from Planned to Supported only when:

1. its implementation is merged into the appropriate integration branch;
2. the exact commit builds reproducibly;
3. the feature passes a documented test on a real first-generation Paperwhite;
4. failure and recovery behavior have been checked where relevant;
5. the result and known limitations are recorded here or in linked documentation.

Unsupported and Not applicable decisions may be revisited when new evidence or a different platform approach justifies the change.
