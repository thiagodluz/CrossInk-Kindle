# Contributing to CrossInk for Kindle

CrossInk for Kindle is an experimental port targeting the first-generation Kindle Paperwhite. Development is organized so incomplete hardware work can be tested without making the default branch appear installable or stable.

## Branches

| Branch | Purpose |
| --- | --- |
| `main` | Public project documentation, validated milestones, and release-ready states. |
| `kindle-pw1` | Long-lived integration branch for the Kindle Paperwhite 1 port. |
| Short-lived branches | One focused feature, fix, test, refactor, documentation change, or maintenance task. |

Short-lived branches should normally start from `kindle-pw1` and merge back into `kindle-pw1`. Project-wide documentation and repository maintenance may start from and target `main`.

Use these prefixes:

- `feat/` for new functionality;
- `fix/` for bug fixes;
- `docs/` for documentation;
- `test/` for test work;
- `refactor/` for behavior-preserving restructuring;
- `chore/` for maintenance.

Examples: `feat/kindle-display`, `feat/touch-input`, and `docs/pw1-diagnostics`.

## Change flow

1. Select one roadmap item or a narrowly defined issue.
2. Create a prefixed branch from the appropriate target branch.
3. Keep commits focused and use messages in the form `<type>: <short summary>`.
4. Document how the change was verified on a computer and, when relevant, on a real PW1.
5. Open a pull request into `kindle-pw1` for port implementation or into `main` for project-wide documentation and maintenance.
6. Merge `kindle-pw1` into `main` only when a documented milestone has been validated on hardware.

Prefer squash merging for short-lived branches so each pull request becomes one focused integration commit. A merge commit may be used when promoting a validated milestone from `kindle-pw1` to `main`.

## Pull requests

A pull request should explain:

- the problem or milestone it addresses;
- what changed;
- the expected behavior;
- tests performed and their results;
- the exact Kindle model and firmware when hardware was involved;
- known limitations or follow-up work.

Do not claim Kindle compatibility based only on simulator results. Hardware-related changes remain experimental until tested on a first-generation Kindle Paperwhite.

## Hardware testing and logs

Device tests should include the command or launch path used, expected behavior, observed behavior, and the commit tested.

Before publishing logs, remove Kindle serial numbers, Amazon account information, Wi-Fi names and credentials, IP addresses that should remain private, book contents, and other personal data.

## Generated files and inherited code

Follow the repository's `AGENTS.md` instructions for architecture, generated assets, formatting, and verification. Preserve upstream copyright and license notices.

Changes copied or adapted from another project must identify their source and license. New dependencies must be reviewed before their code or binaries are distributed.

## Releases

Tags and releases are created from `main`. Preview packages must identify the supported Kindle model, required jailbreak environment, installation steps, removal steps, tested commit, and known limitations.

Until the README explicitly says otherwise, the repository has no installable Kindle build.
