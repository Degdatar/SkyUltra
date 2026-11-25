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

SkyUI, ENB, Community Shaders, SSE Engine Fixes, TrueHUD and Wheeler are **not requirements**. On flat SE/AE, SkyUltra composites inside Skyrim's native UI render pass with scoped D3D11 state save/restore (avoiding an eager DXGI swap-chain `Present` hook) and skips idle compositor work to preserve dynamic lighting and shader mods such as Community Shaders and wearable lanterns. Test the particular UI/graphics-mod combination you choose. MO2 or Vortex can manage the install; MO2 normally redirects generated `Data/SkyUltra/Cache/` files to Overwrite or a configured output mod.

### Installing with Mod Organizer 2 (MO2) or Vortex

1. Install the **SkyUltra** archive through MO2 or Vortex and enable it in your mod list.
2. Ensure no other mod overwrites `Data/SKSE/Plugins/SkyUltra.dll` or the `Data/Interface/skyultra_*.swf` files (`skyultra_inventory.swf`, `skyultra_barter.swf`, `skyultra_container.swf`, `skyultra_loot.swf`, `skyultra_pickpocket.swf`, `skyultra_mannequin.swf`, `skyultra_magic.swf`, `skyultra_favorites.swf`, `skyultra_blacksmith.swf`, `skyultra_enchanting.swf`, `skyultra_alchemy.swf`, `skyultra_book.swf`, `skyultra_compass.swf`). Settings persist in `Data/SKSE/Plugins/SkyUltra.settings.json`; SkyUltra no longer reads or ships an INI. The bundled Chromium CEF runtime stays compact at **16 files** under `SKSE/Plugins/SkyUltra/CEF/` (`locales/en-US.pak` only; debug PDBs live separately in `Diagnostics/`). Advanced startup options are documented in [runtime settings](BRIDGE_DELIVERY.md#one-settings-file).
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

SkyUltra's menu hotkey selects SkyUltra's own menu, even after another mod was open. Per-mod hotkeys select that mod's panels. Escape closes the current panels; opening SkyUltra afterward does not reopen the last mod. HUDs and pinned widgets keep their own visibility. A temporary Skyrim modal menu restores only the panels that were open before it interrupted them.

Press **Space** immediately outside a text field to focus the visible page's search right away — without clicking into the window first. This works directly on open across the standalone SWF inventory, container, corpse loot, pickpocket, barter, mannequin, magic, and favorites menus, as well as the main command modules. If the active module has no search bar, Space opens the command palette. **Enter** activates the focused button, slot, or tile. Text fields keep normal spaces, and popup/hotkey recording scopes keep their own input.

Performance Lab keeps startup wall times (which include hidden prewarming, the title screen, and waiting for a loaded character) separate from the most recent visible opening. Use **Most recent opening → live-data callback** to measure opening latency; warm opens can use already cached player data. Page callbacks measure browser scheduling, not the native frame's presentation time.

### Footer Controls (Always Accessible)
At the bottom-right of the SkyUltra Command Deck footer, you can always toggle:
- **`V-CURSOR` (`ON` / `OFF`)**: Enables or disables the analog Virtual Cursor for Gamepad and VR users. When enabled, move the cursor smoothly with the **Left Stick** and click with **`A` / `Enter`**.
- **`KEYBOARD` (`Shift + K`)**: Opens the on-screen **Virtual Keyboard** (complete with `ESC`, `F1`–`F12`, `Tab`, `Caps`, `Shift`, `Ctrl`, `Alt`, and full alphanumeric keys) for Desktop, Gamepad, and VR players.

---

## 3. Simultaneous Browser Engines (`Ultralight` + `CEF`)

SkyUltra includes **two** hardware-accelerated browser engines that run **simultaneously**:
1. **Ultralight GPU**: Ultra-low memory footprint (`~8 MB` VRAM per surface), prewarms its D3D11 deferred-context renderer at startup, uses persistent JavaScriptCore batch dispatch, and drives rendering from Skyrim's presentation cadence (`30–240` Hz) with bounded GPU command-list backpressure.
2. **Chromium CEF (Default in CMake)**: Full Chromium compatibility for heavy web apps, complex CSS filters, and WebGL, using GPU shared textures with merged dirty-rect copies, staggered hidden-view priming, and internal Viz pacing matching your active monitor refresh rate up to **120 FPS** while respecting explicit per-view caps.

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
- **Nearby Radar (`1.5K` to `12K` units) & Smart Filters**: Inspect nearby NPCs, creatures, or followers with auto-generated 3D bust portraits cached persistently in `Data/SkyUltra/Cache/Portraits/`. Filter by **All / Followers / Unique / Essential / In Combat / Pinned**, sort by **Distance / Name / Level / Health**, and **Pin** favorite targets to the top.
- **Tri-Vitals & Rich Dossier**: Inspect **Health**, **Magicka**, and **Stamina** meters alongside **Class**, **Essential**, **Unique**, **Combat State**, and **Relationship** pills, and click **Base Form** or **Reference ID** cards to copy hex FormIDs.
- **3D Character Studio, Camera Presets & Reset Portrait**: Orbit, zoom, pan, trigger **Bust / 3/4 View / Full Body / Profile** camera presets, toggle **Full model** vs **Portrait**, change lighting presets (`Studio`, `Soft`, `Dramatic`, `Warm`, `Cool`), click **Update portrait** to save to Overwrite (or **Reset portrait** to clear custom overrides), and **equip or unequip** gear directly on loaded NPCs. Preview HLSL shaders prewarm asynchronously at startup and cache compiled bytecode in `Data/SkyUltra/Cache/ActorPreviewShaders.dxbc.bin` so opening Actor Observatory does not freeze cursor movement.

### Module 03 // Inventory, Containers, Pickpocketing, Book Reader & 3D Item Studio
- **Smart Filters, `V/W` Sort & Inline Item Renaming**: Filter by category, equipment state, search (`Space`), or smart chips (**Enchanted**, **Tempered**, **Poisoned**, **Books**, **Unread**, **Stolen**); sort by **Value/Weight (`V/W`)**; save and equip **4 Quick Outfit Loadouts**; compare selected stats against worn gear; and **rename carried items** inline (`game.renameItem`).
- **Theme-Reactive 7-Design Book Reader & World Book Replacement (`skyultra_book.swf`)**: Open inventory books, letters, journals, and maps in **7 selectable styles** (**Auto**, **Codex**, **Scroll**, **Grimoire**, **Dwemer**, **Scholar**, **Datapad**), toggle **Theme Sync vs Parchment Paper**, adjust text scale (`S / M / L / XL`), search highlighted terms in-book, save page bookmarks, and pan/zoom illustrated maps. When reading books in the world via `skyultra_book.swf`, the reader stays distraction-free by inheriting your Settings design/theme, pauses the world by default, flips pages with **`←` / `→`** keyboard arrows, and displays your currently bound Skyrim **Interact** button to Take/Steal and close automatically.
- **Smart Loot, Pickpocket `%` & Combined Actions**: Storage and corpse loot containers add **Smart Loot Valuables** (collecting Gold + high `V/W` or weightless valuables while honoring **Stolen Protection**), live carry-weight forecasts, **Take & Equip / Take & Consume / Take & Read**, and native **Pickpocket `%`** probability badges in `skyultra_pickpocket.swf`.
- **Blacksmith Crafting & Tempering (`skyultra_blacksmith.swf`)**: Unified Forge/Anvil crafting, Tanning Rack, Smelter, Grindstone, and Workbench menu firing native Skyrim crafting actions and displaying exact material & perk requirements.
- **Arcane Enchanter & Super Enchanting (`skyultra_enchanting.swf`)**: Interactive Vessel -> Enchantment -> Soul Gem crafting with List & Grid views, calibrated Disenchant XP, active-quest protection (unlocking items once quests finish), and persistent enchantment renaming. Reaching **Enchanting 100** unlocks the **Super Enchanting** switch in **Settings (`05`) $\rightarrow$ Inventory & Barter**: disenchant unique non-quest artifacts, bind unlimited enchantments, bypass armor slot restrictions, batch-enchant multiple armors or multiple weapons at once with individual per-item names and stack quantity sliders, and consume strictly 1 auto-filled Soul Gem per piece.
- **Alchemy Lab (`skyultra_alchemy.swf`)**: Replaces Skyrim's Alchemy Station with 4-column ingredient Grid & List views, **Taste** trait discovery, **By Effect** matrix & **Known Recipes** builder, Purity perk cleansing, custom potion/poison naming, Distillation Altar batch controls (sliders kept exclusively inside the Altar), native `CraftingMenu` / `BGSCreatedObjectManager` & **SkyRewrite** action hooks, and a Settings switch to return to Vanilla/SkyUI at any time.
- **Zero-Overlap SWF Window Transitions**: Session-epoch transition gating discards stale cached surfaces immediately before switching or closing SWF overlays (`skyultra_inventory`, `barter`, `container`, `loot`, `pickpocket`, `mannequin`, `magic`, `favorites`, `blacksmith`, `enchanting`, `alchemy`, `book`, `compass`) so windows never overlap for stray frames.
- **Persistent Overwrite Icon, Portrait & Shader Cache**: Item icons (`Data/SkyUltra/Cache/Items/`), player/actor portraits (`Data/SkyUltra/Cache/Portraits/`), and compiled preview shader bytecode (`Data/SkyUltra/Cache/ActorPreviewShaders.dxbc.bin`) are saved to your MO2 `Overwrite` folder so icons, portraits, and 3D previews load quickly without stalling subsequent launches. Click the **Camera** button under the Player Inventory 3D stage at any time to capture a fresh head-and-shoulders bust portrait of your character and update the cached portrait across the HUD, Overview, and Character Sheet.
- **Expanded Combat Stats Under Item Icons**: View **Heavy Armor**, **Light Armor**, **Clothing**, and **Shield** badges, **AR** armor ratings, weapon class labels, and **DMG** damage badges directly beneath each item icon across Player Inventory, Vendor Barter, Storage Containers, Corpse Looting, NPC Pickpocketing, and Mannequins.
- **Already Read Seal**: Inventory book cards display a gold book-and-bookmark seal with a check mark only when Skyrim marks the item as read. This includes readable notes, letters, maps and spell tomes. The seal appears in grid and list views; hover the card for “Already read.” Unread items have no seal.
- **Drag-and-Drop Equipping**: Drag any weapon, armor piece, or ammo card directly onto the 11 character equipment slots (or click **Equip** / **Unequip**).
- **Storage Containers (`skyultra_container.swf`)**: Opens automatically for chests, safes, urns, barrels, sacks, and world storage containers. Displays **Gold (`0x0000000F`)** in both inventories, transfers items on click (with a quantity slider when stack count exceeds `5`), supports **Take All** and **Store All**, and provides two cycling header buttons: **Half Container** (default) $\rightarrow$ **Windowed** $\rightarrow$ **Full Screen** and **Live World** (default) $\leftrightarrow$ **Pause World**.
- **Corpse & Monster Looting (`skyultra_loot.swf`)**: Opens automatically when looting dead NPCs, creatures, and monsters (`actor->IsDead()`). Supports **Take** and **Store** single-click transfers, **Take All** and **Store All** bulk actions, **Gold (`0x0000000F`)** in both inventories so gold can be looted or stored, and the two cycling header buttons (**Half Container** $\rightarrow$ **Windowed** $\rightarrow$ **Full Screen** and **Live World** $\leftrightarrow$ **Pause World**).
- **NPC Pickpocketing (`skyultra_pickpocket.swf`)**: Opens automatically when pickpocketing a live NPC. Transfers from the targeted NPC to your inventory use **Steal** and **Steal All** (with native `kSteal` ownership/crime handling), transfers from your inventory to the targeted NPC use **Plant** and **Plant All** to place items on the NPC, worn NPC gear displays an **EQUIPPED** badge on the item card, **Gold (`0x0000000F`)** is visible in both inventories so gold can be stolen or planted, and the header includes the **Half Container** $\rightarrow$ **Windowed** $\rightarrow$ **Full Screen** and **Live World** $\leftrightarrow$ **Pause World** cycling buttons.
- **Mannequin Armor Stand (`skyultra_mannequin.swf`)**: Opens a Player Inventory-style split view (`1520×880` in Windowed mode) with a motionless 3D Mannequin Stage and its 11 equipment slots on the right. Drag items directly onto the mannequin or its slots, or click **Equip** / **Unequip** / **Store** / **Take**.
- **Item Studio**: Inspect any inventory item in 3D and export transparent PNG renders up to **4096×4096 (`4K`)**.

### Module 04 // Character Sheet, Spellbook (`skyultra_magic.swf`) & Favorites (`skyultra_favorites.swf`)
- **Spellbook Click-to-Equip, Rename & Delete**: **Left-click** any spell to equip it to your **Left Hand** (or click any **Power** / **Shout** to equip it directly to your Voice slot); **Right-click** any spell to equip it to your **Right Hand** (two-handed spells equip to Both Hands on either click). **Active Effects** are display-only and are not equipped by clicking. You can also **Rename** spells, shouts, or powers inline, **Delete** them with confirmation, or dispel individual **Active Effects**.
- **Favorites List & Grid View**: **Left-click** favorite spells or dual-wield weapons to equip **Left Hand** (or equip armor, Shouts/Powers, or use potions/food/books); **Right-click** spells or main-hand weapons to equip **Right Hand**. Switch between **List** and **Grid** views, filter by spaced category tabs, bind `1–9` quick slots, and press `Space` to search.

### Module 06 // HUD Studio & Live Code Editor
- **Visual HUD Builder & Sovereign Compass Radius**: Position and style 12 live HUD widgets (Vital Bars, Sovereign Compass, Enemy Health, Active Effects, Kill Feed, Quest Tracker, Stealth Meter, etc.) across 9 screen zones (`Top/Middle/Bottom` × `Left/Center/Right`) with `90%` Safe Guides. Select **Sovereign Compass & POI Bar** to configure its **Compass Marker Radius (`50m`–`1000m`, default `50m`)** via the slider, quick presets (`50m`, `150m`, `300m`, `500m`, `1000m`), or custom numeric input (quests, custom waypoints, and followers remain pinned at any range).
- **Live HUD Code Editor**: Write or edit live **HTML, CSS, and JavaScript** with instant preview on a 16:9 stage, load built-in templates (*Sovereign Glass*, *Cyber Tactical*, *Arcane Radial Rings*, *Minimalist Soulslike*), click **Pin Live Code HUD to Skyrim** to wear your custom HUD in-game immediately, or click **Export as Mod** to generate a standalone SkyUltra mod package!

---

### Module 05 // Settings & UX Customization
- Choose across **21 themes** spanning 4 families (**Eye-Comfort Velvet Slate**, **Classic**, **Celestial**, **Neon**) and customize accents with the `SkyColorWheel` 360° color picker, configure **Tab Transition Splash** (**Full Motion**, **Subtle**, or **Off** with live preview of the animated SkyUltra crest & loading bar), **Default Book Reader Design** and **Theme Sync**, **Glass Intensity**, **UI Scale** (`90%`–`115%`), **Show V/W Badges**, and **Stolen Drop Protection**.

### Module 08 // Developer Studio & Modder Workbench

- **Live View Controls**: **Show / Hide**, **Focus**, emit diagnostic **Probe** events, open the embedded DOM/CSS/REPL inspector, reload, suspend, resume, or unload registered views.
- **Bridge Ledger Controls**: Filter IPC calls and permission denials, pause/resume live streaming, **Clear Ledger** (`ui.clearBridgeLog`), or copy filtered telemetry as JSON.
- **Modder Workbench (Tab 5)**: Interactive **Bridge Playground** with round-trip latency readout and **TypeScript SDK / Vanilla JS / Papyrus** snippet generation, a **Visual `manifest.json` Builder & Validator**, and a live **`--sky-*` CSS Token Inspector**.
- **Mod runtime controls & diagnostics**: Suspend, resume, or unload mod pages and inspect per-mod surface reservations, shared queues, renderer crashes, WebGL context loss, and stalled bridge batches. SkyUltra's own menu cannot suspend itself. See [runtime resilience and optional local crash dumps](RUNTIME_RESILIENCE.md).

## 6. Opening Video Splash Screen

In **Settings (`05`) $\rightarrow$ Opening Splash**, you can customize the Media Foundation startup splash video (frame 0 prewarms off-thread at launch and renders cleanly over the scene without an opaque black loading backdrop):
- **Splash Screen (`ON` / `OFF`)**: Enable or disable the opening splash video.
- **Playback Length**: Play the **Full splash screen** or dismiss automatically **Until page ready**.
- **Replay Schedule**: Play **Once per game session** (default), **Once per mod**, or **Every menu open**.
- **Original Soundtrack & Volume**: Toggle stereo audio and adjust volume (`0%`–`100%`), or press `Escape` at any time to skip playback immediately.

---

## 7. Project Contributors & Special Thanks

- **Marty Edwar** ([`@martinedwar1` on Instagram](https://www.instagram.com/martinedwar1)) — Created the SkyUltra opening splash screen, performed its voice-over, and helped with HUD development.
- **StabbyINK** ([StabbyINK on Nexus Mods](https://www.nexusmods.com/profile/StabbyINK)) — Helped with the UI/UX design and the development of SkyUltra Menus.

See [Credits and third-party libraries](THIRD_PARTY_LIBRARIES.md) for the complete list of upstream libraries and licenses.
