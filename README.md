<div align="center">

<img width="1935" height="1080" alt="SyncShield promotional banner" src="https://github.com/user-attachments/assets/b7ca8455-5b53-47b9-8d99-0e6ec12b227d" />

# 🛡️ SyncShield Core 0.1

[![Unreal Engine](https://img.shields.io/badge/Unreal_Engine-5.7-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
![Platform](https://img.shields.io/badge/Platform-Win64-lightgray.svg)

</div>

Source-control status and guarded saves for the Unreal Editor. Free and open source under the [MIT license](LICENSE).

Core 0.1 brings the current SyncShield implementation to OSS with **12 of 24 user-facing capabilities (50%)**. The [feature matrix](docs/FEATURES.md) defines the counting method and commercial boundary. This is a source plugin for **Unreal Engine 5.7 on Windows (Win64)**.

![SyncShield promotional graphic](https://github.com/user-attachments/assets/ffadb37f-8cf2-4cae-b4ec-2193eb42cc37)

## Features

- A Level Editor toolbar showing unsaved assets, branch, pending changes, ahead/behind state, conflicts, and diagnostic details.
- **Save All**, **Save Blueprints**, and **Save Current Level**, including dirty World Partition external actor/object packages belonging to that level.
- Source-control status refresh before guarded saves, exclusion of packages reported locked by teammates, and checkout requests where the provider supports them.
- A warning when you open an asset whose cached provider state reports another user's lock.
- Unreal's native **Submit Content** dialog.
- Git **Fetch**, optional **Auto Fetch**, **Pull (Rebase)**, and **Push** with state-based availability checks.
- Plastic SCM / Unity Version Control workspace status and **Update Workspace**.
- Git LFS detection, local lock counts, and a basic `.gitattributes` check for Unreal asset entries.

## Install

1. Download `SyncShield-Core-0.1-UE5.7-source.zip` from the [0.1 release](https://github.com/gregorik/SyncShield-Core/releases/tag/v0.1).
2. Extract its `SyncShield` folder into `<YourProject>/Plugins/`. The descriptor should be at `<YourProject>/Plugins/SyncShield/SyncShield.uplugin`.
3. Close Unreal Editor, generate your C++ project's files, and build its Editor target with an Unreal-compatible Visual Studio toolchain.
4. Open the project and enable SyncShield Core if prompted. The toolbar appears in the Level Editor.

If cloning the repository, clone it directly into `<YourProject>/Plugins/SyncShield`. The source ZIP contains no precompiled binaries. A Blueprint-only project needs a C++ build host or an engine-specific plugin build using `RunUAT BuildPlugin` before installation.

Core and commercial SyncShield use the same plugin/module name. Install one edition per project.

## Configure and use

Open **Editor Preferences > Plugins > SyncShield** to set polling intervals, status notifications, optional auto-fetch, and explicit paths to `git` or `cm`. Leave executable paths empty to use `PATH`. Auto-fetch is off by default.

Use the SyncShield toolbar dropdown for guarded saves. Blueprint and current-level saves work without source control. An enabled Unreal source-control provider is required for checkout and teammate-lock information; Git CLI status alone cannot enforce remote locks. Perforce may supply native provider lock/checkout/submit behavior, but Core does not implement a Perforce CLI status probe or sync command.

Pull requires an upstream, incoming commits, no unresolved conflicts or tracked modifications, and no unsaved editor assets. Push also requires outgoing commits and no incoming commits. Git may still reject an operation if the repository changes or an untracked file would be overwritten. Review the reported error and resolve it with your source-control tools.

## Scope

The guard applies to **SyncShield's save actions**, not every Unreal save path, Ctrl+S, or autosave. Asset-open alerts use cached provider state and can miss a lock until the provider refreshes. Provider failures or stale lock data can limit protection; the plugin does not provide atomic distributed locking.

LFS information is diagnostic only; Core does not release locks automatically or provide an unlock command. The attributes check looks for `*.uasset` and `*.umap` entries and is not a complete audit of Git attribute precedence. Pull and workspace update modify files on disk; follow Unreal's reload prompts or reopen affected assets after external changes.

Only UE 5.7 / Win64 is targeted by this release. See [validation](docs/VALIDATION.md) for actual checks and provider coverage. No telemetry, bundled third-party executables, demo assets, or runtime game module are included. Network operations use your own Git/Plastic configuration.

## Development

With PowerShell 7 and UE 5.7 installed:

```powershell
./Scripts/Test-Release.ps1 -EngineRoot 'F:\Epic Games\UE_5.7' -WorkRoot C:\sscore-test
```

Use a new, short work directory. The script builds the plugin, installs the built artifact in a fresh Blueprint-only host, runs its automation suite, and rejects missing reports, failed tests, and unexpected test counts. Git must be available. Optional live Plastic workspace checks use `SYNCSHIELD_PLASTIC_WORKSPACE`.

Issues and contributions are welcome on [GitHub](https://github.com/gregorik/SyncShield-Core/issues). For history, validation, Conflict Sentinel, and advanced Git workflows, see [commercial SyncShield](https://www.fab.com/listings/dd5a848f-d6a5-4c5d-a766-87ea191fdd98).
