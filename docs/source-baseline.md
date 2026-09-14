# Source Baseline

This document records the immutable source baseline selected for the first-generation Kindle Paperwhite port. Full commit SHAs are used so builds, investigations, upstream synchronization, and regressions can be reproduced without relying on moving branch names.

## Repository genealogy

```text
crosspoint-reader/crosspoint-reader
└── uxjulia/CrossInk
    └── thiagodluz/CrossInk-Kindle
```

## Pinned baseline

| Source | Repository | Role | Recorded ref | Immutable commit |
| --- | --- | --- | --- | --- |
| CrossInk | [uxjulia/CrossInk](https://github.com/uxjulia/CrossInk) | Application baseline inherited by this fork | `main` | [`4f2da6b7777b9a38fdf2606796d6da34caa7d2cb`](https://github.com/uxjulia/CrossInk/commit/4f2da6b7777b9a38fdf2606796d6da34caa7d2cb) |
| FreeInk SDK | [Free-Ink/freeink-sdk](https://github.com/Free-Ink/freeink-sdk) | Git submodule used by the application baseline | `main` | [`9f4d3f9ca675e64cc9d616081f39a33cdefce57e`](https://github.com/Free-Ink/freeink-sdk/commit/9f4d3f9ca675e64cc9d616081f39a33cdefce57e) |
| CrossInk Simulator | [uxjulia/crossink-simulator](https://github.com/uxjulia/crossink-simulator) | External native-platform reference for initial port work | `main` | [`95be4c2e546d6e88625fb33c31478893d4f06f1b`](https://github.com/uxjulia/crossink-simulator/commit/95be4c2e546d6e88625fb33c31478893d4f06f1b) |
| CrossPoint Reader | [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) | Fork-network origin and CrossInk divergence point | `develop` history | [`6e8dbd7f239eb562daa5de81362c0025ce833dd8`](https://github.com/crosspoint-reader/crosspoint-reader/commit/6e8dbd7f239eb562daa5de81362c0025ce833dd8) |

The machine-readable form of this table is in [`dependencies/baseline.json`](../dependencies/baseline.json).

## Meaning of each record

### CrossInk

Commit `4f2da6b7777b9a38fdf2606796d6da34caa7d2cb` is the actual CrossInk commit inherited before Kindle-specific documentation was added. It is the application baseline against which the port's changes and future upstream synchronization are measured.

Recorded upstream metadata:

- authored on 2026-09-11;
- commit message: `Update release manifests for v1.5.1`;
- tree: `4679b952e6c8662269c5cdaf48b6560db48d21af`.

### FreeInk SDK

The CrossInk baseline records `freeink-sdk` as a Git gitlink at commit `9f4d3f9ca675e64cc9d616081f39a33cdefce57e`. It is therefore a pinned source dependency, not merely the current head of the SDK's `main` branch.

Recorded upstream metadata:

- authored on 2026-09-10;
- commit message: `Merge pull request #71 from oreglio/feat/list-item-section-heading`;
- tree: `d226e528b056cbe6b3e090425e0e5eb5b49830a7`.

### CrossInk Simulator

The simulator is not currently a submodule or build dependency of this repository. Commit `95be4c2e546d6e88625fb33c31478893d4f06f1b` records the exact external implementation that will be examined while designing the Linux/Kindle platform layer.

Recorded upstream metadata:

- authored on 2026-09-03;
- commit message: `fix: add missing shutdown shims`.

### CrossPoint Reader

Commit `6e8dbd7f239eb562daa5de81362c0025ce833dd8` is the common ancestor between the current CrossInk line and CrossPoint Reader's `develop` history at the time this baseline was researched. It records where the code lines diverged; it is not a dependency checkout for the Kindle build.

Recorded upstream metadata:

- authored on 2026-06-24;
- commit message: `chore: Update version to 1.4.0 (#2283)`.

CrossPoint Reader's `develop` branch is moving and must not be treated as an immutable version. Its observed head on 2026-09-14 was `f64ba736a7c44f9b08bc04037ab10296013a4914`, while the comparison with CrossInk was diverged. This observation is intentionally excluded from the machine-readable baseline.

## Submodule note

At the CrossInk baseline, the root tree contains one gitlink: `freeink-sdk`. The inherited `.gitmodules` file also names `assets/tabler-icons`, but that path is not recorded as a gitlink in the baseline tree. It is therefore not part of the pinned gitlink set and must be audited separately during the license and asset review.

## Verification

After checking out this repository with submodules, the recorded state can be inspected with:

```sh
git show --no-patch --format=fuller 4f2da6b7777b9a38fdf2606796d6da34caa7d2cb
git ls-tree 4f2da6b7777b9a38fdf2606796d6da34caa7d2cb freeink-sdk
git submodule status --recursive
```

Expected `git ls-tree` output for the SDK path:

```text
160000 commit 9f4d3f9ca675e64cc9d616081f39a33cdefce57e	freeink-sdk
```

## Update policy

Do not build or investigate regressions from an unrecorded moving branch head. For every upstream synchronization:

1. use a branch named `chore/sync-crossink-YYYY-MM-DD`;
2. record the previous and new CrossInk full SHAs;
3. record any changed FreeInk SDK gitlink;
4. document conflicts and decisions;
5. run the applicable host, firmware, and hardware tests;
6. update this document and `dependencies/baseline.json` in the same pull request.

Kindle-specific toolchains and libraries are not yet part of this lock. They will be added only after their selection, compatibility, and license strategy have been documented.
