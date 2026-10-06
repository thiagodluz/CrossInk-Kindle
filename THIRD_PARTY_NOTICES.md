# Third-Party Notices

This file records notices for third-party material that is inherited by, or may be included in, CrossInk for Kindle. It is a source-tree notice file, not a declaration that every entry is present in every binary.

Each release must retain only the entries applicable to its exact package and add the full license texts where required.

## CrossInk and CrossPoint Reader

CrossInk for Kindle is derived from CrossInk and CrossPoint Reader.

Copyright (c) 2025 Dave Allie

Licensed under the MIT License. The full inherited license text is in [`LICENSE`](./LICENSE).

- CrossInk baseline: [`9914146eeae7b46b300f475a16c32426fc02ec1f`](https://github.com/uxjulia/CrossInk/commit/9914146eeae7b46b300f475a16c32426fc02ec1f)
- CrossPoint Reader divergence point: [`6e8dbd7f239eb562daa5de81362c0025ce833dd8`](https://github.com/crosspoint-reader/crosspoint-reader/commit/6e8dbd7f239eb562daa5de81362c0025ce833dd8)

## FreeInk SDK

Copyright (c) 2026 FreeInk

Licensed under the MIT License.

Pinned source: [`699370183fa3a0e33c9cb83a36f701bbb6022095`](https://github.com/Free-Ink/freeink-sdk/commit/699370183fa3a0e33c9cb83a36f701bbb6022095).

If a release includes FreeInk SDK source or compiled portions, retain the upstream [LICENSE](https://github.com/Free-Ink/freeink-sdk/blob/699370183fa3a0e33c9cb83a36f701bbb6022095/LICENSE) and [NOTICE](https://github.com/Free-Ink/freeink-sdk/blob/699370183fa3a0e33c9cb83a36f701bbb6022095/NOTICE). Its NOTICE credits the OpenX4 E-Paper Community SDK, community device ports, CidVonHighwind, and the inherited SSD1677/UC8253 initialization and waveform work.

## CrossInk Simulator

Copyright (c) 2026 Julia Nguyen

Licensed under the MIT License.

Pinned reference: [`95be4c2e546d6e88625fb33c31478893d4f06f1b`](https://github.com/uxjulia/crossink-simulator/commit/95be4c2e546d6e88625fb33c31478893d4f06f1b).

The simulator is not currently a Kindle package dependency. Preserve this notice if any simulator source is copied into the Kindle compatibility layer.

## Bundled Noto fonts

Noto Sans and Noto Sans Arabic are licensed under the SIL Open Font License, Version 1.1.

Copyright 2022 The Noto Project Authors.

Retain the corresponding `OFL.txt` file with any shipped font:

- [Noto Sans OFL](./lib/EpdFont/builtinFonts/source/NotoSans/OFL.txt)
- [Noto Sans Arabic OFL](./lib/EpdFont/builtinFonts/source/NotoSansArabic/OFL.txt)

## Additional CrossInk scalable fonts

CrossInk v1.6.1 adds scalable font assets to the source tree. Retain each font's matching license notice when distributing the font or binaries containing it:

- Bitter — [`Bitter-OFL.txt`](./lib/EpdFont/scalableFonts/licenses/Bitter-OFL.txt)
- ChareInk 7 — [`ChareInk7-OFL.txt`](./lib/EpdFont/scalableFonts/licenses/ChareInk7-OFL.txt)
- Lexend Deca — [`LexendDeca-OFL.txt`](./lib/EpdFont/scalableFonts/licenses/LexendDeca-OFL.txt)
- Noto Sans CJK SC — [`NotoSansCJKsc-OFL.txt`](./lib/EpdFont/scalableFonts/licenses/NotoSansCJKsc-OFL.txt)
- Noto Symbols — [`NotoSymbols.txt`](./lib/EpdFont/scalableFonts/licenses/NotoSymbols.txt)

These are inherited upstream assets; their presence in this source baseline does not mean they are part of a Kindle build/package.

## Tabler Icons

Copyright (c) 2020–2026 Paweł Kuna

Licensed under the MIT License. The inherited `.gitmodules` entry points to [Tabler Icons](https://github.com/tabler/tabler-icons). Preserve its license and copyright notice if generated icons or source assets are shipped.

## FreeInk SDK bundled notices

The following notices apply only when the relevant FreeInk SDK source or binary material is included:

| Component | License / notice |
| --- | --- |
| OpenX4 E-Paper Community SDK | MIT; credit required by FreeInk SDK NOTICE. |
| miniz | MIT-style license; copyright RAD Game Tools, Valve Software, Rich Geldreich, and Tenacious Software. |
| Expat | MIT; copyright Thai Open Source Software Center, Clark Cooper, and Expat maintainers. |
| pngle | MIT; copyright 2019 kikuchan. |
| hyph-en-us patterns | Copying and distribution permitted if the copyright and notice are preserved; copyright Gerard D. C. Kuiken. |

The precise files are enumerated in [the licensing strategy](./docs/licensing.md).

## Candidates not included in the first Kindle package

These components are not approved for inclusion in the initial local-reader package:

| Component | License | Current status |
| --- | --- | --- |
| FBInk | GPLv3 | Reference only; not bundled, linked, or invoked. |
| KOReader | AGPLv3 | Reference only; no code, assets, headers, or build files copied. |
| ArduinoWebSockets | LGPL-2.1 | Not selected; network is disabled initially. |
| wolfSSL | GPLv3 or commercial terms | Not selected; network is disabled initially. |
| Arduino-ESP32 / pioarduino toolchain | LGPL-2.1 and transitive licenses | ESP32-only; excluded from the Kindle dependency graph. |
| PNGdec / JPEGDEC | Apache-2.0 | Candidate only; not selected initially. |
| ArduinoJson / QRCode | MIT | Candidate only; not selected until the Kindle build proves they are necessary. |
| koxtoolchain scripts | No top-level license found during audit | May be studied as build reference; not copied or redistributed. |

See [`docs/licensing.md`](./docs/licensing.md) for the policy and evidence.
