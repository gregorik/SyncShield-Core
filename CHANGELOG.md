# Changelog

## 0.1 — 2026-09-12

First tagged OSS release, with 12 of the 24 commercial capabilities documented in [the feature matrix](docs/FEATURES.md).

- Add guarded Blueprint-only and current-level saves, including World Partition external actor/object packages.
- Port the separated Git/Plastic probes, process runner, status presentation, and asynchronous execution from the current commercial source.
- Refresh provider status before guarded saves and pass packages to checkout requests; support providers without checkout.
- Surface executable launch failures, command errors, and incomplete status instead of reporting a healthy repository.
- Block pull/push for unmerged files even when ordinary staged/unstaged counts are zero.
- Preserve process output and improve timeout handling and asynchronous widget lifetime management.
- Add Git LFS/local-lock diagnostics, basic Unreal `.gitattributes` entry checks, and configurable CLI executable paths.
- Reduce repeated status notifications and correctly scope toolbar registration to module lifetime.
- Retain basic Git fetch/pull/push, Plastic update, teammate-lock alerts, and native submit integration.
- Remove demo commands from the OSS package. Commercial history, validation, branch switching, Conflict Sentinel, and advanced Git actions are outside this release.
- Add a reproducible clean-package automation script, installation guide, source ZIP, and checksums.

The descriptor changes from the untagged public `1.2` to `0.1` to establish independent OSS release numbering.
