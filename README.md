<div align="center">
  
# 🛡️ SyncShield Core
**The Source Control Safety Net for Unreal Engine 5**

[![Unreal Engine](https://img.shields.io/badge/Unreal_Engine-5.2+-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Win64-lightgray.svg)]()

</div>

---
[Watch it in action](https://www.youtube.com/watch?v=UCpiobdiJYM) <br>

Ever spent hours tweaking a Blueprint or meticulously painting a landscape, only to hit *Save All* and realize a teammate already locked the file in your source control system? 

**SyncShield Core** is a lightweight, battle-tested Unreal Engine 5 plugin designed to eliminate source control friction and protect your team from lost work and locked-file conflicts. It natively integrates with Unreal's Editor to warn you *before* you make changes, and safely catches you if you try to overwrite someone else's work.

This is a major upgrade and more ambitious refactoring of the earlier [SafeSave plugin](https://github.com/gregorik/SafeSave) which remains fully functional in its own narrower scope.

Also, in case you need this adapted or integrated into a production UE5 project: I offer paid Unreal Engine [consulting and implementation](https://gregorigin.com/contact.html).

## ✨ Core Features

### 🚫 Strict File Locking Protection
SyncShield natively intercepts your "Save All" commands. Before any data is written, it rapidly probes your source control provider (Git, Plastic SCM, Perforce) for checkout status. If any package is locked by a teammate (`IsCheckedOutOther`), SyncShield **blocks the save for those specific files** with an explicit warning, while automatically issuing checkouts and safely saving the rest.

### 🔔 Preemptive "Dropbox-Style" Alerts
Don't wait until you save to find out a file is locked. SyncShield hooks directly into the Unreal Asset Editor. The moment you open a Blueprint, Material, or Data Asset that is locked by another developer, you instantly get a prominent Toast Notification warning you not to edit it.

### 🛠️ Streamlined Editor Toolbar
Stop fumbling with external CLI windows or hidden context menus. SyncShield adds a dedicated, dynamic status widget right to the Level Editor Toolbar.
* **Live Status Updates:** See your Branch, pending changes, and unsaved asset counts at a glance.
* **One-Click Actions:** Save All, Submit Content, or Refresh Status.
* **Native Git Integration:** Auto-Fetch (configurable interval), Pull (Rebase), and Push directly from the toolbar.
* **Native Plastic SCM Integration:** One-click Workspace Update.

---

## 🚀 Installation

1. Download or clone this repository.
2. Place the `SyncShield` folder into your project's `Plugins` directory: `[YourProject]/Plugins/SyncShield/`.
3. Right-click your `.uproject` file and select **Generate Visual Studio project files**.
4. Compile your project.
5. Launch the Unreal Editor. SyncShield will automatically appear in your Level Editor Toolbar.

## ⚙️ Configuration

You can configure SyncShield's behavior via **Project Settings > Plugins > SyncShield**:
* Adjust polling intervals for dirty checks and Git/Plastic SCM status.
* Enable or disable **Auto Fetch** (for Git).
* Toggle Toast Notifications on/off.

---

## 💎 Upgrade to SyncShield Pro

**SyncShield Core** provides the essential safety net for collaborative teams. For solo developers and studio environments looking for maximum data security and advanced workflow automation, check out **[SyncShield Pro on Fab (WIP)](https://www.fab.com/sellers/GregOrigin)**!

**The Pro version includes everything in Core, plus:**

* ⚔️ **Conflict Sentinel:** Stop merge conflicts *before* they happen. SyncShield Pro quietly tracks your locally dirty assets and runs background checks against your remote Git branch. If a teammate pushes a change to a file you are currently editing, you get an instant toast warning you of the impending collision.
* 🌿 **Safe Branch Shifter:** Switch Git branches without playing Russian Roulette with the Unreal Editor. SyncShield Pro's "Safe Switch" pipeline protects your unsaved data, forcefully closes vulnerable asset editors, performs a clean checkout, and lets the Asset Registry hot-reload safely without crashing.
* ⏪ **Local Save History & Time Travel:** SyncShield Pro quietly takes lightweight, localized snapshots of your assets every time you save. Broke a Blueprint? Click "Restore Latest Snapshot" to instantly revert the active asset to its last known good state—without needing to pull from remote source control.
* 🛑 **Pre-Save Data Validation:** Automatically run custom or engine-level validation checks *before* assets are committed to disk. Prevent broken references, bad naming conventions, or uncompiled Blueprints from ever reaching your repository.
* 📂 **Advanced Save Profiles:** Stop saving everything just to be safe. Use precision save commands:
  * *Save Blueprints Only*
  * *Save Current Level Only*
  * *Save Recently Touched (Time-windowed)*

---

## 🤝 Contributing
Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](../../issues). 

## 📜 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. 

*Copyright (c) 2026 GregOrigin. All Rights Reserved.*
