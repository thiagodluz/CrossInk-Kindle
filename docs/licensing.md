# Licensing Strategy

This document records the license strategy for the CrossInk for Kindle port. It is an engineering record, not legal advice. Before a binary release, re-audit the exact source tree, dependency lockfile, compiler/linker options, bundled assets, and generated package.

**Audit date:** 2026-09-14  
**Application baseline:** [CrossInk commit `4f2da6b`](./source-baseline.md)  
**Target:** first-generation Kindle Paperwhite (PW1)

## Decision summary

The initial Kindle prototype will be built from the MIT-licensed CrossInk application and MIT-compatible components already selected for the port. It will not copy KOReader source code and will not include, link, or invoke FBInk.

This keeps the first distributable package outside the GPLv3/AGPLv3 design choices while display access, device diagnostics, and packaging are still unknown. It does not prohibit a later FBInk-based design; that would require a separate licensing and distribution decision before implementation.

## Source and dependency matrix

| Component | License verified | Initial Kindle decision | Distribution requirement |
| --- | --- | --- | --- |
| CrossInk / CrossPoint Reader baseline | MIT | Included and modified | Preserve the inherited MIT copyright and license notice. |
| FreeInk SDK at the pinned gitlink | MIT, with its own `NOTICE` | Reuse only if a Kindle build needs compatible portions | Preserve its license and applicable `NOTICE` attributions, including OpenX4 E-Paper Community SDK credit. |
| CrossInk Simulator | MIT | Reference and host-testing source only | Preserve notice if any simulator code is copied into the Kindle platform layer. |
| Noto Sans / Noto Sans Arabic bundled fonts | SIL Open Font License 1.1 | May be bundled | Preserve their OFL files and reserved-font-name terms. |
| Tabler Icons | MIT | Existing source/asset reference | Preserve upstream notice if assets or generated derivatives are shipped. |
| ArduinoJson | MIT | Candidate only; include only if required by the Kindle build | Preserve notice. |
| QRCode | MIT | Candidate only; not required for the first local reader | Preserve Richard Moore and Project Nayuki attribution if included. |
| PNGdec / JPEGDEC | Apache-2.0 | Candidate only; not selected for the first Kindle build | Include Apache-2.0 license and NOTICE handling if included. |
| miniz, Expat, pngle, hyphenation patterns inside FreeInk SDK | permissive notices (MIT-style; hyphenation has its own preservation notice) | Inherit only when the corresponding FreeInk SDK source is shipped | Preserve every applicable upstream notice. |
| Arduino-ESP32, ESP-IDF, pioarduino, wolfSSL, ArduinoWebSockets | LGPL/GPL-family or mixed transitive firmware dependencies | Excluded from the Kindle target | Do not treat the ESP32 PlatformIO dependency graph as a Kindle dependency graph. Re-audit any later replacement. |
| FBInk | GPLv3 | Technical reference only; not included, linked, or invoked initially | A later integration needs an explicit GPLv3 source/distribution plan. |
| KOReader | AGPLv3 | Technical behavior and protocol reference only; no copied code, headers, assets, or build files | A later code reuse requires an explicit AGPLv3 decision. |
| koxtoolchain | No top-level license file found in the repository at audit time | Build-reference material only; do not copy or redistribute its scripts | Audit the selected compiler/toolchain and each component before distribution. |

The project-wide notices are in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

## What “reference only” means

The project may inspect public behavior, APIs, file formats, protocol documentation, or independently reproduce an interface after understanding it. It must not copy source files, headers, snippets, assets, generated output, test fixtures, or build scripts from a reference-only project into this repository.

For KOReader specifically, an independently implemented reading-position sync client is permitted as a project decision, but it must be based on protocol-level behavior and original code. Interoperability does not itself make this project a derivative of KOReader; copying its implementation could.

For FBInk specifically, the initial display work will use the Kindle's documented/observed framebuffer and update interfaces directly through an original backend. Invoking FBInk as a separate executable is also deliberately deferred: distribution and process-boundary details should be reviewed only if that option becomes necessary.

## Package policy

Every released package must contain:

1. the exact source commit and dependency lock/baseline;
2. this project’s MIT license;
3. `THIRD_PARTY_NOTICES.md` updated for the exact package contents;
4. full license texts or an equivalent compliant inclusion for every bundled dependency;
5. source availability and build instructions required by every included license;
6. a list of all dynamically loaded or separately executed components;
7. a statement of any features intentionally excluded from that release.

No package may claim that a reference-only component is bundled.

## Review gates

A pull request that adds or changes a Kindle dependency must include:

- upstream repository or authoritative source;
- immutable version or commit;
- license identifier and a link to the license text;
- whether the component is compiled in, dynamically loaded, run separately, bundled as an asset, or used only to build;
- required notice/source obligations;
- whether it changes this project’s distribution model;
- a change to `THIRD_PARTY_NOTICES.md` when it can enter a released package.

A release must be blocked if any bundled component has an unknown, missing, incompatible, or unreviewed license.

## Evidence consulted

- [CrossInk license](https://github.com/uxjulia/CrossInk/blob/4f2da6b7777b9a38fdf2606796d6da34caa7d2cb/LICENSE) — MIT.
- [FreeInk SDK license](https://github.com/Free-Ink/freeink-sdk/blob/9f4d3f9ca675e64cc9d616081f39a33cdefce57e/LICENSE) and [NOTICE](https://github.com/Free-Ink/freeink-sdk/blob/9f4d3f9ca675e64cc9d616081f39a33cdefce57e/NOTICE) — MIT plus attributions.
- [CrossInk Simulator license](https://github.com/uxjulia/crossink-simulator/blob/95be4c2e546d6e88625fb33c31478893d4f06f1b/LICENSE) — MIT.
- [FBInk license](https://github.com/NiLuJe/FBInk/blob/master/LICENSE) — GPLv3.
- [KOReader copying file](https://github.com/koreader/koreader/blob/master/COPYING) — AGPLv3.
- [Noto Sans OFL](../lib/EpdFont/builtinFonts/source/NotoSans/OFL.txt) and [Noto Sans Arabic OFL](../lib/EpdFont/builtinFonts/source/NotoSansArabic/OFL.txt).
- [Tabler Icons license](https://github.com/tabler/tabler-icons/blob/main/LICENSE) — MIT.
- [ArduinoJson license](https://github.com/bblanchon/ArduinoJson/blob/v7.4.2/LICENSE.txt) — MIT.
- [QRCode license](https://github.com/ricmoo/QRCode/blob/master/LICENSE.txt) — MIT.
- [PNGdec license](https://github.com/bitbank2/PNGdec/blob/master/LICENSE) and [JPEGDEC license](https://github.com/bitbank2/JPEGDEC/blob/86282979224c8a32fd51e091ed5a35b0c699a52b/LICENSE) — Apache-2.0.
- [ArduinoWebSockets license](https://github.com/Links2004/arduinoWebSockets/blob/master/LICENSE) — LGPL-2.1.
