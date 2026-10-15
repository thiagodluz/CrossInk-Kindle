# CrossInk for Kindle

> [!WARNING]
> **Experimental proof of concept — made for fun.** This independent hobby project explores giving the first-generation Kindle Paperwhite another life as a reading device, with an app for a Kindle model Amazon has long since left behind. It is an experiment, not a finished or officially supported product.

CrossInk for Kindle is an experimental port of [CrossInk](https://github.com/uxjulia/CrossInk) to the Linux-based **first-generation Kindle Paperwhite (PW1, 2012)**. It is a personal, community-built project pursued for the fun of seeing whether this old e-reader can run a different reading app.

The idea is simple: give a Kindle that Amazon has stopped prioritizing another chance to be useful for reading. We are testing whether pieces of CrossInk can run as a removable application on a jailbroken PW1, while keeping Amazon's Kindle software available. This is a proof of concept: the current preview opens local TXT files, while the complete CrossInk interface and EPUB reading remain future work.

This project is independent and is not an official CrossInk or Amazon product. The current CrossInk firmware targets ESP32-based e-readers and **cannot be installed on a Kindle**. This repository contains the experimental Linux/Kindle port, build tools, documentation, test packages, and compatibility notes.

## Target device

| Item | Initial target |
| --- | --- |
| Device | Kindle Paperwhite, first generation |
| Platform | Kindle Linux |
| Form | Application launched alongside the original Kindle software |
| Reading goal | Explore local book reading; current preview supports TXT |
| Input | Touchscreen |
| Display | Kindle e-ink framebuffer |
| Project status | Experimental proof of concept; basic TXT preview validated on PW1 |

The PW1 is the only target for this experiment. Other Kindle models are not supported unless they are explicitly tested and documented.

## Current status

| Area | Status |
| --- | --- |
| Kindle environment diagnostics | CPU, ABI, framebuffer and touch recorded from PW1 |
| Reproducible ARM build | Static kindle5 executable; ELF audited |
| E-ink display and touch diagnostic | Full/partial updates and touch targets tested on a PW1 |
| Reading app | Native-renderer TXT preview; basic reading/navigation validated on PW1 (0.2.1) |
| EPUB reading | Planned |
| Reading progress persistence | TXT save/reopen/relaunch validated on PW1 (0.2.1) |
| Power button / sleep | 0.3.6 power save/exit and wake/KUAL reopen validated; position retained per user |
| Front light, battery, in-app suspend/resume | Planned |
| Wi-Fi, OPDS and progress sync | Planned |
| Installable experiment | KUAL diagnostic 0.1.0 and TXT preview 0.3.7 (continuous-reading device retest pending) |

The hardware diagnostic and the basic TXT preview (0.2.1) have been tested on the user's PW1: page controls, progress persistence across launches, normal exit, the corner-hold exit gesture and the five-minute session limit worked. Version 0.3.0 adds a power-event handoff that saves and exits when the Kindle begins to sleep; the first 0.3.0 device test did not react to the power button. Version 0.3.1 also failed on the device, receiving zero complete event lines. Version 0.3.2 added terminal transport and state diagnostics but aborted during device preflight. Version 0.3.3 restored device startup, but received zero event bytes and still saw powerd as active before normal exit. Version 0.3.4 keeps CVM/WM running and temporarily hides/restores the status bar, following the legacy PW1 framework-cooperation approach. The 2026-10-14 device test confirmed the power event, page-4 save, clean reader exit and stock screenSaver state. The launcher recovery signal was rejected; forced recovery and wake/reopen remain unvalidated. See the [device evidence](./docs/kindle-platform-architecture.md#034-power-button-device-validation--2026-10-14). Version 0.3.5 fixes session coordination and launcher notification, gates display publication during power exit and adds bounded state-aware recovery. Its physical log confirms the signal and power exit, but the user reports reopening failed. Version 0.3.6 corrects inherited operation-lock descriptors, records the real reader behind the observed shell intermediary and logs opening before native preflight. A descriptor leak was reproduced locally; the exact failed PW1 launch is unknown. The 0.3.6 logs confirm power exit, wake/KUAL reopen and normal Exit; the user confirms selecting the book resumes the saved page, corner hold and the five-minute timeout permit reopening, and the status bar/sleep image behave correctly. See the [0.3.6 evidence](./docs/validation/pw1-0.3.6-checks.md#physical-036-result). The preview remains an experimental proof of concept with no EPUB support.

Version 0.3.7 adds continuous TXT reading with independent input/render
heartbeat supervision and real-touch updates to the stock inactivity timer.
The five/six-minute UI limits are removed; physical prolonged-reading and idle
sleep tests are pending. The diagnostic timeout remains unchanged.

## Development plan

The next steps for this proof of concept are deliberately small and hardware-focused:

1. Collect system, display, input, power, and library information from a jailbroken PW1.
2. Build and run a minimal ARM executable on the device.
3. Draw test patterns through a Kindle display backend.
4. Read touchscreen events and map them to CrossInk actions.
5. Launch the CrossInk interface with Kindle-specific hardware abstractions.
6. Open, paginate, and resume a local EPUB.
7. Implement front light, battery reporting, suspend, and resume.
8. Add network features such as OPDS and reading progress sync.
9. Package, document, and test a removable and reversible preview release.

Track detailed progress and milestone criteria in the [project roadmap](./ROADMAP.md), review the intended PW1 feature scope in the [compatibility matrix](./docs/compatibility.md), consult the exact upstream revisions in the [source baseline](./docs/source-baseline.md), see the planned backend boundaries in the [Kindle platform architecture](./docs/kindle-platform-architecture.md), and run the [PW1 diagnostics](./docs/pw1-diagnostics.md) before device-specific implementation.

## Architecture

The port builds on CrossInk's separation between the reading app and its hardware abstraction layer. It reuses selected CrossInk rendering, font, UI, and TXT components behind a small Kindle-specific compatibility layer.

The Kindle implementation is expected to provide dedicated backends for:

- e-ink drawing and refresh control;
- touchscreen input;
- local storage and application data;
- timers, tasks, and synchronization on Linux;
- battery, charging, front light, suspend, and resume;
- Kindle launch, exit, logging, installation, and removal.

Existing Kindle projects such as [FBInk](https://github.com/NiLuJe/FBInk), [KOReader](https://github.com/koreader/koreader), and [koxtoolchain](https://github.com/koreader/koxtoolchain) are useful technical references. Any code or library incorporated from them will be reviewed for license compatibility before distribution.

## Installation

Use the [0.3.7 TXT preview instructions](./platform/kindle/UI-README.md) to copy the package to a jailbroken PW1 and launch it from KUAL. The package includes a sample TXT and keeps the [hardware diagnostic](./platform/kindle/README.md). Build it with `scripts/kindle/package-ui.py` after initializing the pinned SDK.

Basic TXT reading, power save-and-exit, wake/KUAL reopen and reading-position retention have been tested on the PW1 with 0.3.6. Forced recovery of a blocked app still requires device validation. It is an application package, not firmware; do not flash ESP32 firmware to a Kindle.

## Contributing

Development is tracked publicly so device findings, code, and test results are easy to follow. Further testing of forced-crash recovery and the six-minute watchdog on a PW1 is especially useful. See the [test instructions](./platform/kindle/UI-README.md) before installing; the preview is still experimental.

When reporting results from a Kindle, remove serial numbers, account information, Wi-Fi credentials, and other personal data from logs.

## Upstream projects and credits

This project adapts [CrossInk](https://github.com/uxjulia/CrossInk), which is itself based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader), for an experimental Kindle proof of concept. Credit for the existing reader, firmware, hardware abstractions, and related work belongs to their respective contributors.

The Kindle port is an independent experimental project and is not an official CrossInk, CrossPoint Reader, Amazon, or Kindle release.

## License

The inherited CrossInk and CrossPoint Reader code is distributed under the [MIT License](./LICENSE). Original copyright and license notices are preserved.

The dependency choices, source-distribution policy, and reference-only boundaries are recorded in the [licensing strategy](./docs/licensing.md). Package notices are tracked in [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) and must be reviewed again before every binary release.
