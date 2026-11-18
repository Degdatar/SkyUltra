# SkyUltra built-in menu

Open with **Shift+F6** or the configured shortcut. Escape or the header's close
button closes the menu. Settings control layout, world pause, hotkeys, browser
selection, individual Skyrim menu replacements, themes, profiles and the splash.
The host can include both CEF and Ultralight; actual selection depends on settings,
manifest pins and available backends. Engine switching has no fixed latency guarantee.

## Player setup

Install a complete release, matching SKSE and Address Library, the Visual C++ x64
runtime, and a supported Windows/D3D11 game setup. **SkyUI, ENB, Community Shaders,
Engine Fixes, TrueHUD and Wheeler are not requirements.** See the
[player guide](../../docs/USER_GUIDE.md). The menu's in-game Guide contains the same
requirements distinction, mod-author guidance and library acknowledgments.

## Source and surfaces

The app consumes `@skyultra/ui`; reusable screens live in `sdk/ui/src/field`.
Public `/menu`, `/game`, `/gameplay`, `/chrome`, `/core` and `/widgets` entries let
other mods use individual surfaces without importing this entire menu.

The nine modules cover Overview, Actor Observatory (tri-vitals, pinning, smart filters/sort, camera presets, Reset Portrait), Inventory (smart filter chips, `V/W` sort, inline renaming, 4 quick loadouts, 7-design theme-synced Book Reader), Character Sheet, Settings (17 themes across 4 families, motion `TabSplash`, glass intensity, UI scale, Book Reader styles), HUD workshop, Performance Lab, Developer Studio (with Modder Workbench: Bridge Playground, TS/JS/Papyrus snippets, visual `manifest.json` builder & `--sky-*` token inspector) and the Guide. Actor
and item studios support lighting, camera controls and exports. Creature captures
retain missing secondary skin-bone mappings, and single-bone skinned props use
explicit weights when their vertex layout omits skin attributes. Icon/portrait
caches live under `Data/SkyUltra/Cache/`; generated dynamic actor portraits include
identity tokens to avoid reusing another creature's image.

Passive HUDs (including `skyultra_compass.swf`) use the native UI render pass without the carrier's old
menu-blocking flag. Skyrim menu transitions suppress HUD views, broadcast synchronized `skyultra.menuSession` epochs, and discard cached surfaces cleanly so windows do not overlap across frames. Verify movement, map-marker dialogs and other overlays after
changing the renderer; a source check is not a visual/runtime test.

## Build and package (developer action)

From the project root, inspect and run `tools/Build-SkyUltraMenu.ps1` when you
intend to build. `-SkipNative` is for frontend-only work;
`-SkipDependencyInstall` reuses installed dependencies. Review its current
parameters before packaging or installation.

The package includes the plugin, `SkyUltra/skyultra.menu/manifest.json`,
`Files/index.html`, `Files/hud.html`, `Files/SkyUltraBridge.js`, bundled web assets,
native/Papyrus scripts, the trimmed 16-file CEF runtime (`locales/en-US.pak` only, with `SkyUltraCefSubprocess.pdb` staged separately in `Diagnostics/`), Ultralight assets, and configured menu-launcher
assets (`9` SWF carriers). Copy the **contents** of its `Data` folder into the chosen mod output,
preserving relative paths. The plugin DLL alone is insufficient.

## Settings and author documentation

Settings persist under `Data/SKSE/Plugins/SkyUltra.settings.json` (redirected by
MO2 when active). The supplied splash video/audio is retained; its playback,
volume and replay choices are settings. Advanced startup options use the same
JSON file's `runtime` object and apply after restarting Skyrim. The INI is no
longer read or shipped. See [runtime settings and bridge delivery](../../docs/BRIDGE_DELIVERY.md).

Developer Studio now exposes runtime suspension/resumption, unloading, aggregate mod resource usage and CEF recovery information. Suspension unloads documents and Resume loads fresh ones while retaining native handles. See [runtime resilience](../../docs/RUNTIME_RESILIENCE.md), including the optional local-only crash reporter setup; no game-root configuration is installed by the menu build.

The built-in app installs `installSkyUltraCompetitorShims` during its lifecycle.
Independent mod pages explicitly install their own shims before legacy scripts.
See the [SDK walkthrough](../../docs/SDK_GUIDE.md), [native API](../../api/README.md),
[PrismaUI guide](../../docs/PRISMAUI_MIGRATION.md), [Magelight guide](../../docs/MAGELIGHT_UI_MIGRATION.md),
and [credits/dependency inventory](../../docs/THIRD_PARTY_LIBRARIES.md).
