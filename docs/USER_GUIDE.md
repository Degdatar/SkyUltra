# SkyUltra — Player & User Guide

Welcome to **SkyUltra**, the hardware-accelerated WebView UI framework and interactive command deck for **Skyrim Special Edition (SE)**, **Anniversary Edition (AE)**, and **Skyrim VR**.

---

## 1. Requirements to Play This Mod & Installation

### Requirements to Play This Mod
- **Skyrim Base Game**: A Windows SE, AE or VR executable supported by your SkyUltra release; match the exact runtime with SKSE and Address Library.
- **SKSE64 / SKSEVR**: [**SKSE64**](https://skse.silverlock.org/) (SE/AE) or [**SKSEVR**](https://skse.silverlock.org/) (VR) matching your exact game version
- **Address Library**: [**Address Library for SKSE Plugins**](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (All-in-One SE/AE) or [**VR Address Library for SKSEVR**](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
- **Windows Runtime & DirectX 11 GPU**: **Windows 10 / 11 (64-bit)**, **Microsoft Visual C++ Redistributable (2015–2022 x64)**, and a **DirectX 11 hardware GPU**
- **Complete release package**: Install the plugin, bundled browser runtimes, web assets and scripts together. Players do not need separate CEF/Ultralight SDK downloads.

SkyUI, ENB, Community Shaders, SSE Engine Fixes, TrueHUD and Wheeler are **not requirements**. Test the particular UI/graphics-mod combination you choose. MO2 or Vortex can manage the install; MO2 normally redirects generated `Data/SkyUltra/Cache/` files to Overwrite or a configured output mod.

### Installing with Mod Organizer 2 (MO2) or Vortex

1. Install the **SkyUltra** archive through MO2 or Vortex and enable it in your mod list.
2. Ensure no other mod overwrites `Data/SKSE/Plugins/SkyUltra.dll`, `Data/SKSE/Plugins/SkyUltra.ini`, or the `Data/Interface/skyultra_*.swf` files (`skyultra_inventory.swf`, `skyultra_barter.swf`, `skyultra_container.swf`, `skyultra_loot.swf`, `skyultra_pickpocket.swf`, `skyultra_mannequin.swf`, `skyultra_magic.swf`, `skyultra_favorites.swf`).
3. Launch Skyrim via `skse64_loader.exe` (or `sksevr_loader.exe` in VR).

---

## 2. Default Hotkeys & Controls

All hotkeys support **1 to 3 input chords** and can be rebound in **Settings (`Module 05`) $\rightarrow$ Hotkeys & VR**:

| Action | Default Keyboard | Default Gamepad | Default VR Controller |
| :--- | :--- | :--- | :--- |
| **Open / Close SkyUltra Menu** | `Shift + F6` | `LB + RB` | `Left Grip + Right Grip` (or custom) |
| **Toggle Virtual Keyboard** | `Shift + K` | Footer `KEYBOARD` pill | `Right Thumbstick Click` / Footer pill |
| **Search / Command Palette** | `Space` (page search first) | Top bar `Command` button | Laser click on `Command` |
| **Quick Help / Shortcuts** | `Ctrl + F3` | Top bar `?` button | Laser click on `?` |
| **Switch Modules (`01`–`09`)** | `1`–`9` (when not typing) | `LB` / `RB` or D-Pad | Laser pointer selection |
| **Close Menu / Skip Splash** | `Escape` | `B` / `Back` | `Secondary Button` |

Press **Space** outside a text field to focus the visible page's search. If the main menu page has no search, Space opens the command palette. **Enter** activates the focused button, slot, or tile. Text fields keep normal spaces, and popup/hotkey recording scopes keep their own input. These controls also apply to the web menus opened by the inventory, barter, magic, favorites, container, loot, pickpocket, and mannequin SWF launchers.

Performance Lab keeps startup wall times (which include hidden prewarming, the title screen, and waiting for a loaded character) separate from the most recent visible opening. Use **Most recent opening → live-data callback** to measure opening latency; warm opens can use already cached player data. Page callbacks measure browser scheduling, not the native frame's presentation time.

### Footer Controls (Always Accessible)
At the bottom-right of the SkyUltra Command Deck footer, you can always toggle:
- **`V-CURSOR` (`ON` / `OFF`)**: Enables or disables the analog Virtual Cursor for Gamepad and VR users. When enabled, move the cursor smoothly with the **Left Stick** and click with **`A` / `Enter`**.
- **`KEYBOARD` (`Shift + K`)**: Opens the on-screen **Virtual Keyboard** (complete with `ESC`, `F1`–`F12`, `Tab`, `Caps`, `Shift`, `Ctrl`, `Alt`, and full alphanumeric keys) for Desktop, Gamepad, and VR players.

---

## 3. Simultaneous Browser Engines (`Ultralight` + `CEF`)

SkyUltra includes **two** hardware-accelerated browser engines that run **simultaneously**:
1. **Ultralight GPU (Default)**: Ultra-low memory footprint (`~8 MB` VRAM per surface), records directly onto a D3D11 deferred context at up to `120 FPS+`.
2. **Chromium CEF (Double-Buffered GPU Shared Texture)**: Full Chromium compatibility for heavy web apps, complex CSS filters, and WebGL.

### Switching Engines Without Affecting Other Mods
- Open **Settings (`05`) $\rightarrow$ Cursor & Telemetry**.
- Under **Simultaneous Multi-Engine Browser Runtime**, click **Ultralight GPU** or **Chromium CEF**.
- SkyUltra transitions smoothly in place without flashing back to the game world, and **other mods keep running on their own configured engine at the same time**.
- You can also override the engine of any individual installed mod using the **Per-Mod Browser Engine Override** controls.

---

## 4. Skyrim VR Features (`atGaze` Head Tracking & Laser Input)

SkyUltra treats **Skyrim VR** as a first-class platform:
- **VR Head Tracking (`atGaze` HMD Follow)**:
  - Enabled by default in **Settings (`05`) $\rightarrow$ Hotkeys & VR / Cursor & Telemetry**.
  - Smoothly keeps the open SkyUltra menu in front of your headset's gaze direction (`atGaze`) so you can read and interact with the UI comfortably regardless of which way you turn your head in real life.
  - Prefer a stationary floating wall screen? Toggle **VR Head Tracking** off to lock the menu in world space (`worldLocked`).
- **Dual-Hand VR Laser Pointer & Virtual Keyboard**:
  - Point with either your left or right VR controller and pull the trigger to click, drag equipment slots, or type on the built-in Virtual Keyboard (`Shift + K` or footer button).

---

## 5. Module Walkthrough

### Module 01 // Overview
- Instant summary of your character's **Health, Magicka, and Stamina** rings, armor rating, active magic effects, encumbrance, and 11 equipped gear slots.

### Module 02 // Actor Observatory
- **Nearby Radar (`1.5K` to `12K` units)**: Inspect every loaded NPC, creature, or follower around you with auto-generated 3D bust portraits (close-up of head, shoulders, and upper body) cached persistently in `Data/SkyUltra/Cache/Portraits/` (`Overwrite/SkyUltra/Cache/Portraits/` in MO2).
- **Find NPC**: Search across all NPC records in your load order by name.
- **3D Character Studio & Live Dress-Up**: Orbit, zoom, pan, switch between **Full model** and **Portrait** (head, shoulders & upper body close-up), change lighting presets, click **Update portrait** (Camera button) to overwrite the cached portrait in-game, and **equip or unequip** armor, weapons, and ammo directly on the selected NPC with instant visual and slot updates.

### Module 03 // Inventory, Vendor Barter, Storage Containers, Corpse Looting, NPC Pickpocketing, Mannequins & 3D Item Studio
- Filter your inventory by category (Weapons, Armor, Potions, Ingredients, Ammo, Books, Keys, Misc), equipment state, or search query (`Space` shortcut).
- **Persistent Overwrite Icon & Portrait Cache**: Item icons (`Data/SkyUltra/Cache/Items/`) and player/actor portraits (`Data/SkyUltra/Cache/Portraits/`) are saved to your MO2 `Overwrite` folder so icons and portraits load immediately on future game launches without waiting to re-render. Click the **Camera** button under the Player Inventory 3D stage at any time to capture a fresh head-and-shoulders bust portrait of your character and update the cached portrait across the HUD, Overview, and Character Sheet.
- **Expanded Combat Stats Under Item Icons**: View **Heavy Armor**, **Light Armor**, **Clothing**, and **Shield** badges, **AR** armor ratings, weapon class labels, and **DMG** damage badges directly beneath each item icon across Player Inventory, Vendor Barter, Storage Containers, Corpse Looting, NPC Pickpocketing, and Mannequins.
- **Drag-and-Drop Equipping**: Drag any weapon, armor piece, or ammo card directly onto the 11 character equipment slots (or click **Equip** / **Unequip**).
- **Storage Containers (`skyultra_container.swf`)**: Opens automatically for chests, safes, urns, barrels, sacks, and world storage containers. Displays **Gold (`0x0000000F`)** in both inventories, transfers items on click (with a quantity slider when stack count exceeds `5`), supports **Take All** and **Store All**, and provides two cycling header buttons: **Half Container** (default) $\rightarrow$ **Windowed** $\rightarrow$ **Full Screen** and **Live World** (default) $\leftrightarrow$ **Pause World**.
- **Corpse & Monster Looting (`skyultra_loot.swf`)**: Opens automatically when looting dead NPCs, creatures, and monsters (`actor->IsDead()`). Supports **Take** and **Store** single-click transfers, **Take All** and **Store All** bulk actions, **Gold (`0x0000000F`)** in both inventories so gold can be looted or stored, and the two cycling header buttons (**Half Container** $\rightarrow$ **Windowed** $\rightarrow$ **Full Screen** and **Live World** $\leftrightarrow$ **Pause World**).
- **NPC Pickpocketing (`skyultra_pickpocket.swf`)**: Opens automatically when pickpocketing a live NPC. Transfers from the targeted NPC to your inventory use **Steal** and **Steal All** (with native `kSteal` ownership/crime handling), transfers from your inventory to the targeted NPC use **Plant** and **Plant All** to place items on the NPC, worn NPC gear displays an **EQUIPPED** badge on the item card, **Gold (`0x0000000F`)** is visible in both inventories so gold can be stolen or planted, and the header includes the **Half Container** $\rightarrow$ **Windowed** $\rightarrow$ **Full Screen** and **Live World** $\leftrightarrow$ **Pause World** cycling buttons.
- **Mannequin Armor Stand (`skyultra_mannequin.swf`)**: Opens a Player Inventory-style split view (`1520×880` in Windowed mode) with a motionless 3D Mannequin Stage and its 11 equipment slots on the right. Drag items directly onto the mannequin or its slots, or click **Equip** / **Unequip** / **Store** / **Take**.
- **Item Studio**: Inspect any inventory item in 3D and export transparent PNG renders up to **4096×4096 (`4K`)**.

### Module 04 // Character Sheet, Spellbook (`skyultra_magic.swf`) & Favorites (`skyultra_favorites.swf`)
- **Spellbook Rename & Delete**: Select any spell, shout, or power to **Rename** it in place or **Delete** it with confirmation, or delete/dispel individual **Active Effects** directly from the Active Effects list.
- **Favorites List & Grid View**: Switch between **List** and **Grid** views in the Favorites menu, filter by spaced category tabs, bind `1–8` quick slots, and press `Space` to search.

### Module 06 // HUD Studio & Live Code Editor
- **Visual HUD Builder**: Position and style 12 live HUD widgets (Vital Bars, Compass, Enemy Health, Active Effects, Kill Feed, Quest Tracker, Stealth Meter, etc.) across 6 screen zones (`Top-Left`, `Top-Center`, `Top-Right`, `Bottom-Left`, `Bottom-Center`, `Bottom-Right`).
- **Live HUD Code Editor**: Write or edit live **HTML, CSS, and JavaScript** with instant preview on a 16:9 stage, load built-in templates (*Sovereign Glass*, *Cyber Tactical*, *Arcane Radial Rings*, *Minimalist Soulslike*), click **Pin Live Code HUD to Skyrim** to wear your custom HUD in-game immediately, or click **Export as Mod** to generate a standalone SkyUltra mod package!

---

## 6. Opening Video Splash Screen

In **Settings (`05`) $\rightarrow$ Opening Splash**, you can customize the hardware-accelerated startup splash video:
- **Splash Screen (`ON` / `OFF`)**: Enable or disable the opening splash video.
- **Playback Length**: Play the **Full splash screen** or dismiss automatically **Until page ready**.
- **Replay Schedule**: Play **Once per game session** (default), **Once per mod**, or **Every menu open**.
- **Original Soundtrack & Volume**: Toggle stereo audio and adjust volume (`0%`–`100%`), or press `Escape` at any time to skip playback immediately.
