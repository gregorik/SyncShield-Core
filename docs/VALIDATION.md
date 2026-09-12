# Core 0.1 validation

Verified on 2026-09-12 with Unreal Engine **5.7.4**, Win64, using the release descriptor's `EngineVersion: 5.7.0`.

| Check | Result |
|---|---|
| Clean `RunUAT BuildPlugin -TargetPlatforms=Win64 -Rocket -installed` | Passed |
| Built plugin loaded in a fresh Blueprint-only host project | Passed |
| Editor automation report | 19 successful results, 0 failed, 0 incomplete |
| Blueprint save selection | Dirty Blueprint included; dirty texture and clean Blueprint excluded |
| Current-level scope | World Partition actor/object paths and unrelated-level exclusions passed |
| Git status and workflow fixtures | Disposable local remotes, dirty/clean/ahead/behind/diverged states passed |
| Conflict gating | Unmerged files block pull/push even with zero ordinary change counts |
| Process, presentation, async ownership, settings, LFS parsing | Passed |
| Source ZIP | Explicit payload list, descriptor checks, 12/24 feature boundary, byte-for-byte verification, SHA-256 checksum |

Of the 19 reported successes, 17 were warning-free and two contained expected missing-executable warnings: the unavailable Plastic CLI and the deliberately nonexistent executable used to verify launch-failure reporting.

`PlasticConfiguredWorkspace` returns without probing when `SYNCSHIELD_PLASTIC_WORKSPACE` is unset, as it was during this release run. That reported success is a conditional no-op, **not evidence of a live Plastic workspace test**. The separate Plastic missing-client fallback was exercised. Perforce and a real multi-user lock server were not tested. The remaining provider lock behavior depends on Unreal's configured source-control provider.

The checks exercised the packaged Editor module with `-nullrhi`; they do not establish interactive visual QA, cloud authentication, other Unreal versions, or Mac/Linux support. Engine headers emitted deprecation warnings during compilation. No standalone Shipping game build was performed; the descriptor declares an Editor-only module.

Reproduce the build and tests with [Test-Release.ps1](../Scripts/Test-Release.ps1), using a new short work directory. Build the source archive with:

```powershell
python Scripts/package_release.py
```

The package script validates the descriptor and feature boundary, writes `Artifacts/SyncShield-Core-0.1-UE5.7-source.zip`, reads every file back for comparison, and generates `Artifacts/SHA256SUMS.txt`. The archive contains source, plugin configuration/resources, license, documentation, and reproduction scripts, with one top-level `SyncShield` folder. Binaries, intermediate output, project assets, and user configuration are excluded.
