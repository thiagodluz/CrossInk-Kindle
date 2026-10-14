# CrossInk for Kindle

> [!WARNING]
> Experimental PW1 port: the 0.1.0 hardware diagnostic passed device testing. A 0.2.0 TXT preview is ready for testing; the full CrossInk activities and EPUB reader are still pending.

CrossInk for Kindle is an experimental port of [CrossInk](https://github.com/uxjulia/CrossInk) to the Linux-based **first-generation Kindle Paperwhite (PW1, 2012)**.

The goal is to run the CrossInk reading experience as an application on a jailbroken Kindle while preserving the option to return to Amazon's original reader. The current CrossInk firmware targets ESP32-based e-readers and **cannot be installed on a Kindle**. This repository will contain the Linux/Kindle adaptation, build tools, documentation, test packages, and compatibility notes developed for the port.

## Target device

| Item | Initial target |
| --- | --- |
| Device | Kindle Paperwhite, first generation |
| Platform | Kindle Linux |
| Form | Application launched alongside the original Kindle software |
| Initial reading goal | EPUB; current preview supports TXT |
| Input | Touchscreen |
| Display | Kindle e-ink framebuffer |
| Project status | Native TXT prototype; device validation pending |

Support for other Kindle models may be considered after the PW1 port becomes stable. No compatibility with another model should be assumed unless it is explicitly documented.

## Current status

| Area | Status |
| --- | --- |
| Kindle environment diagnostics | CPU, ABI, framebuffer and touch recorded from PW1 |
| Reproducible ARM build | Static kindle5 executable; ELF audited |
| E-ink display backend | Diagnostic full/partial updates validated on PW1 |
| Touch input | Diagnostic corner/center/exit contacts validated |
| CrossInk interface | Native renderer/FreeInkUI TXT preview; full activities pending |
| EPUB reading | Planned |
| Reading progress persistence | TXT preview tested locally and under ARM emulation |
| Front light, battery, suspend and resume | Planned |
| Wi-Fi, OPDS and progress sync | Planned |
| Installable test package | KUAL diagnostic 0.1.0 and TXT preview 0.2.0 |

## Development plan

The first milestones are deliberately small and hardware-focused:

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

The port will build on CrossInk's separation between the reader and its hardware abstraction layer, together with the native simulator work that already allows parts of the application to run outside the ESP32 environment.

The Kindle implementation is expected to provide dedicated backends for:

- e-ink drawing and refresh control;
- touchscreen input;
- local storage and application data;
- timers, tasks, and synchronization on Linux;
- battery, charging, front light, suspend, and resume;
- Kindle launch, exit, logging, installation, and removal.

Existing Kindle projects such as [FBInk](https://github.com/NiLuJe/FBInk), [KOReader](https://github.com/koreader/koreader), and [koxtoolchain](https://github.com/koreader/koxtoolchain) are useful technical references. Any code or library incorporated from them will be reviewed for license compatibility before distribution.

## Installation

Use the [0.2.0 TXT preview instructions](./platform/kindle/UI-README.md) to copy the package to a jailbroken PW1 and launch it from KUAL. The package includes a sample TXT and keeps the [hardware diagnostic](./platform/kindle/README.md). Build it with `scripts/kindle/package-ui.py` after initializing the pinned SDK.

The preview requires a real-device test of the new renderer, page controls and saved progress. It is an application package, not firmware; do not flash ESP32 firmware to a Kindle.

## Contributing

Development is being tracked publicly so research, device findings, code, and test results remain reproducible. Bug reports and contributions will become useful once the first diagnostic and build instructions are available.

When reporting results from a Kindle, remove serial numbers, account information, Wi-Fi credentials, and other personal data from logs.

## Upstream projects and credits

This repository is a fork of [CrossInk](https://github.com/uxjulia/CrossInk), which is itself based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader). Credit for the existing reader, firmware, hardware abstractions, and related work belongs to their respective contributors.

The Kindle port is an independent experimental project and is not an official CrossInk, CrossPoint Reader, Amazon, or Kindle release.

## License

The inherited CrossInk and CrossPoint Reader code is distributed under the [MIT License](./LICENSE). Original copyright and license notices are preserved.

The dependency choices, source-distribution policy, and reference-only boundaries are recorded in the [licensing strategy](./docs/licensing.md). Package notices are tracked in [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) and must be reviewed again before every binary release.
