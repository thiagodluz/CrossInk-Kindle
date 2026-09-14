# CrossInk for Kindle Roadmap

This roadmap tracks the experimental port of CrossInk to the first-generation Kindle Paperwhite (PW1, 2012). Progress is based on reproducible results and real-device validation rather than target dates.

## Status legend

| Status | Meaning |
| --- | --- |
| Complete | Merged and verified for the scope described. |
| Current | The next active area of work. |
| Planned | Defined but not started. |
| Blocked | Cannot continue until a dependency or hardware result is available. |

## Project milestones

| Milestone | Outcome | Status |
| --- | --- | --- |
| M0 — Project foundation | Repository purpose, workflow, and roadmap are public. | Complete |
| M1 — PW1 environment known | Hardware and software interfaces are documented from a real device. | Planned |
| M2 — First native executable | A reproducible ARM program starts, logs, and exits on the PW1. | Planned |
| M3 — Display and input | The application draws safely and receives correct touch events. | Planned |
| M4 — CrossInk interface | CrossInk navigation runs through Kindle-specific platform backends. | Planned |
| M5 — Local EPUB reading | A local EPUB opens, paginates, and resumes correctly. | Planned |
| M6 — Daily-use platform support | Front light, battery, suspend, and resume work reliably. | Planned |
| M7 — Network features | Wi-Fi, OPDS, and progress sync work without risking local reading state. | Planned |
| M8 — Preview release | A reversible, documented package is available for PW1 testing. | Planned |
| M9 — Stable PW1 release | The supported feature set passes prolonged hardware testing. | Planned |

## Phase 0 — Repository foundation

- [x] Replace the inherited README with the Kindle port description.
- [x] Define the roles of `main`, `kindle-pw1`, and short-lived branches.
- [x] Publish contribution, pull request, hardware-test, and release rules.
- [x] Publish this roadmap and link it from the README.

**Exit criterion:** the project's purpose, development flow, current limitations, and planned milestones are visible from `main`.

## Phase 1 — Scope, source baseline, and licensing

**Status: Current**

- [x] Create `docs/compatibility.md` with every inherited CrossInk feature classified as planned, supported, unsupported, or not applicable.
- [x] Record the exact starting commits for CrossInk, CrossPoint Reader, FreeInk SDK, and the simulator.
- [x] Map application code, HAL boundaries, ESP32-only code, FreeRTOS dependencies, and simulator replacements.
- [x] Define the initial Kindle platform interfaces for display, input, storage, tasks, power, and network.
- [x] Create `THIRD_PARTY_NOTICES.md`.
- [x] Verify the licenses of all code, libraries, fonts, icons, and build tools expected in distributed packages.
- [x] Decide whether FBInk will be linked, invoked as a separate program, or used only as a technical reference.
- [x] Record what may be learned from KOReader without copying AGPL-covered implementation.

**Exit criterion:** the code baseline and planned architecture are reproducible, and every proposed dependency has a documented license strategy.

## Phase 2 — PW1 hardware and system diagnostics

- [ ] Create a read-only diagnostic script for jailbroken PW1 devices.
- [ ] Detect the Kindle model, firmware, CPU architecture, kernel, libc, and available shared libraries.
- [ ] Inspect framebuffer resolution, pixel format, rotation, stride, and refresh interfaces.
- [ ] Identify touchscreen input devices, event formats, coordinate range, and orientation.
- [ ] Identify front-light, battery, charging, power-button, suspend, and resume interfaces.
- [ ] Inspect KUAL, KOReader, SSH, logging, storage, and launch options available on the test device.
- [ ] Redact serial numbers, account data, Wi-Fi details, and other personal information automatically.
- [ ] Run the diagnostic on a real PW1 and commit a sanitized hardware profile.
- [ ] Document a safe application directory and recovery path.

**Exit criterion:** a sanitized diagnostic report from the target PW1 identifies the interfaces needed for the first executable, display, input, and power work.

## Phase 3 — Reproducible ARM build and safe launcher

- [ ] Select and pin a PW1-compatible cross-compilation toolchain.
- [ ] Create a reproducible local build environment.
- [ ] Build a minimal ARM executable with no unnecessary runtime dependencies.
- [ ] Run it on the PW1 and confirm architecture and dynamic-linker compatibility.
- [ ] Write a versioned log and a test file, then exit cleanly.
- [ ] Create the first KUAL or equivalent launcher entry.
- [ ] Keep all application files inside a dedicated CrossInk directory.
- [ ] Add stop, cleanup, and removal scripts.
- [ ] Document installation, execution, log collection, and recovery for the prototype.

**Exit criterion:** the same source can be rebuilt and launched repeatedly on the PW1, and exiting restores normal Kindle operation.

## Phase 4 — E-ink display backend

- [ ] Choose the display implementation after the Phase 1 license decision.
- [ ] Draw full-screen black, white, and checkerboard test patterns.
- [ ] Draw rectangles, text, grayscale ramps, and a test image.
- [ ] Confirm resolution, stride, pixel format, clipping, and orientation.
- [ ] Implement full and partial refresh modes.
- [ ] Measure refresh time and observe ghosting on the real display.
- [ ] Prevent concurrent drawing by the Kindle interface while the prototype owns the screen.
- [ ] Restore and refresh the Kindle interface after normal exit.
- [ ] Test recovery after forced termination.

**Exit criterion:** test content is rendered correctly and repeatedly, and the device returns safely to the Amazon interface.

## Phase 5 — Touch input backend

- [ ] Read raw touchscreen events without blocking clean shutdown.
- [ ] Map raw coordinates to the current screen orientation.
- [ ] Implement touch down, move, release, tap, and long press as required by CrossInk.
- [ ] Filter duplicate and noisy events.
- [ ] Validate corners, edges, center, swipes, and orientation changes.
- [ ] Create an interactive screen with large Previous, Next, and Exit targets.
- [ ] Confirm that each gesture triggers exactly one expected action.

**Exit criterion:** display and touch operate together through a safe interactive test application.

## Phase 6 — Linux platform layer and CrossInk interface

- [ ] Add Kindle-specific implementations for display, input, storage, timing, tasks, and power.
- [ ] Replace required FreeRTOS primitives with Linux-compatible equivalents.
- [ ] Remove or capability-gate ESP32-only initialization and services.
- [ ] Route application files, settings, caches, fonts, and logs to Kindle-safe paths.
- [ ] Start the CrossInk activity loop through the Kindle platform layer.
- [ ] Render and navigate the home screen, file browser, settings, and menus.
- [ ] Adapt layout and touch targets to the PW1's 758 × 1024 display.
- [ ] Measure startup time, memory use, and idle CPU use.
- [ ] Ensure crashes cannot corrupt Amazon reader or KOReader data.

**Exit criterion:** the CrossInk interface can be navigated on the PW1 without opening a book and exits safely.

## Phase 7 — Local EPUB reading

- [ ] Open a simple text-first EPUB from local storage.
- [ ] Render text with correct page dimensions, margins, fonts, and line spacing.
- [ ] Navigate forward, backward, and between chapters.
- [ ] Save and restore the current book, chapter, and reading position.
- [ ] Persist font, size, margin, orientation, and reader settings.
- [ ] Validate bookmarks and finished-book behavior selected for the initial scope.
- [ ] Test images, tables, notes, special characters, CSS, large chapters, and large files.
- [ ] Define cache ownership, versioning, invalidation, and cleanup.
- [ ] Test repeated opening, closing, and reboot cycles without data corruption.

**Exit criterion:** a reading session can be completed, closed, and resumed at the correct location after restarting the application and device.

## Phase 8 — Power, front light, suspend, and resume

- [ ] Read battery percentage and charging state.
- [ ] Read, change, and restore front-light intensity.
- [ ] Handle power-button events without conflicting with the Kindle system.
- [ ] Save state and stop background work before suspension.
- [ ] Restore display, input, book state, and timers after resume.
- [ ] Verify correct behavior with low battery and while charging.
- [ ] Measure idle, reading, illuminated, Wi-Fi, and suspended power use.
- [ ] Run repeated suspend and resume cycles.
- [ ] Investigate and fix abnormal battery drain.

**Exit criterion:** the PW1 can suspend for an extended period, resume to the correct reading state, and retain reasonable battery behavior.

## Phase 9 — Wi-Fi, OPDS, and progress sync

- [ ] Use the Kindle-managed Wi-Fi connection without storing network credentials.
- [ ] Handle unavailable networks, disconnects, timeouts, invalid clocks, and HTTPS certificates.
- [ ] Browse and authenticate with an OPDS catalog.
- [ ] Download books safely and recover from interrupted transfers.
- [ ] Implement the selected reading-progress synchronization protocol.
- [ ] Test sync between the PW1, KOReader, and CrossInk devices where applicable.
- [ ] Validate paragraph boundaries and books with different pagination.
- [ ] Preserve the local reading position when the server or network fails.
- [ ] Document identifiers, conflict handling, and privacy behavior.

**Exit criterion:** network failures cannot damage local reading state, and successful OPDS and progress-sync operations are reproducible.

## Phase 10 — Remaining features and hardening

- [ ] Update the compatibility matrix with actual results.
- [ ] Port or explicitly defer statistics, dictionaries, themes, sleep screens, favorites, and transfer tools.
- [ ] Add versioned, size-limited logs that are easy to export.
- [ ] Add automated host builds and relevant static checks.
- [ ] Build artifacts from identified source commits.
- [ ] Test low storage, large libraries, malformed books, USB connection, forced exit, and reboot.
- [ ] Run prolonged reading and page-turn tests.
- [ ] Measure startup, page-turn, memory, storage, and battery behavior.
- [ ] Document known limitations and recovery procedures.

**Exit criterion:** the selected preview feature set survives normal use and defined failure scenarios on real hardware.

## Phase 11 — Packaging and releases

- [ ] Build an installer that verifies the device model and required jailbreak environment.
- [ ] Prevent installation on unsupported Kindle models.
- [ ] Include a complete removal and recovery procedure.
- [ ] Publish an alpha package for controlled PW1 testing.
- [ ] Collect reproducible reports tied to exact commits and device firmware.
- [ ] Fix release-blocking display, input, storage, power, and resume issues.
- [ ] Publish a beta with the intended network features.
- [ ] Define the supported stable feature set.
- [ ] Tag and publish the first stable PW1 release from `main`.

**Exit criterion:** a user can install, use, update, and remove the documented release on a supported PW1 without developer assistance.

## Phase 12 — Maintenance and additional Kindle models

- [ ] Define how upstream CrossInk changes will be reviewed and integrated.
- [ ] Track Kindle firmware compatibility and known regressions.
- [ ] Maintain dependency and license notices with every release.
- [ ] Keep the compatibility matrix and recovery instructions current.
- [ ] Evaluate reusable platform code for later Kindle generations.
- [ ] Require a separate hardware profile and real-device validation for every additional model.

Support for another Kindle model begins only after the PW1 port has a stable platform layer. No model is considered compatible until it is listed explicitly.

## Validation principles

- Simulator success is useful evidence but does not establish Kindle compatibility.
- Hardware claims must identify the device, firmware, tested commit, procedure, and result.
- Every prototype must have a safe exit and recovery path.
- Reading progress and user files must remain valid after failure.
- `main` receives implementation milestones only after their documented exit criteria pass on real hardware.
