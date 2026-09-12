# Core 0.1 feature boundary

Core includes **12 of 24 capabilities, or 50%**, inventoried from the local commercial SyncShield 0.7.0 source at release preparation. Each row counts once. This is a product-scope count, not a claim about equal complexity, value, code size, or development effort. Supporting settings, reliability fixes, tests, and developer demo commands are not separate product capabilities.

| # | Capability | Core 0.1 | Commercial |
|---|---|---|---|
| 1 | Toolbar status, dirty-asset counts, details, and status notifications | Yes | Yes |
| 2 | Guarded Save All with provider lock checks and checkout | Yes | Yes |
| 3 | Teammate-lock alert on asset opening | Yes | Yes |
| 4 | Native source-control Submit Content dialog | Yes | Yes |
| 5 | Git repository/branch, changes, ahead/behind, and conflict status | Yes | Yes |
| 6 | Git Fetch and configurable Auto Fetch | Yes | Yes |
| 7 | Git Pull (Rebase) | Yes | Yes |
| 8 | Git Push | Yes | Yes |
| 9 | Plastic workspace status and Update Workspace | Yes | Yes |
| 10 | Save Blueprints | Yes | Yes |
| 11 | Save Current Level, including World Partition external packages | Yes | Yes |
| 12 | LFS/local-lock diagnostics and Unreal `.gitattributes` entry checks | Yes | Yes |
| 13 | Save Recently Touched | No | Yes |
| 14 | Pre-save UObject and Blueprint validation, with error blocking | No | Yes |
| 15 | Asset naming and texture-dimension validation guidance | No | Yes |
| 16 | Local snapshot capture and retention | No | Yes |
| 17 | Restore latest local snapshot | No | Yes |
| 18 | Diff active asset against latest local snapshot | No | Yes |
| 19 | Conflict Sentinel: remote-file changes versus unsaved local assets | No | Yes |
| 20 | Safe branch switching with asset-editor/package handling | No | Yes |
| 21 | Quick Commit with staging and commit-message UI | No | Yes |
| 22 | Stash Changes and Pop Stash | No | Yes |
| 23 | Merge/rebase abort menu | No | Yes |
| 24 | Manual and automatic Git LFS lock release | No | Yes |

The omitted services, commands, settings, and supporting feature probes are absent from the OSS source. They are not disabled behind a license flag. Core still reports existing Git conflicts; predicting collisions with remote changes is the separate commercial Conflict Sentinel capability.

The old public descriptor was `1.2`, with no published GitHub releases. **0.1 starts the independently versioned OSS release series** requested for this update; it does not follow the commercial version number. The established module name `SyncShield` remains unchanged for installation compatibility.
