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

The nine modules cover Overview, Actor Observatory, Inventory, Character Sheet,
Settings, HUD workshop, Performance Lab, Developer Studio and the Guide. Actor
and item studios support lighting, camera controls and exports. Creature captures
retain missing secondary skin-bone mappings, and single-bone skinned props use
explicit weights when their vertex layout omits skin attributes. Icon/portrait
caches live under `Data/SkyUltra/Cache/`; generated dynamic actor portraits include
identity tokens to avoid reusing another creature's image.

Passive HUDs use the native UI render pass without the carrier's old
menu-blocking flag. Skyrim menu transitions suppress HUD views and close the
carrier as needed. Verify movement, map-marker dialogs and other overlays after
changing the renderer; a source check is not a visual/runtime test.

## Build and package (developer action)

From the project root, inspect and run `tools/Build-SkyUltraMenu.ps1` when you
intend to build. `-SkipNative` is for frontend-only work;
`-SkipDependencyInstall` reuses installed dependencies. Review its current
parameters before packaging or installation.

The package includes the plugin, `SkyUltra/skyultra.menu/manifest.json`,
`Files/index.html`, `Files/hud.html`, `Files/SkyUltraBridge.js`, bundled web assets,
native/Papyrus scripts, browser runtime directories and configured menu-launcher
assets. Copy the **contents** of its `Data` folder into the chosen mod output,
preserving relative paths. The plugin DLL alone is insufficient.

## Settings and author documentation

Settings persist under `Data/SKSE/Plugins/SkyUltra.settings.json` (redirected by
MO2 when active). The supplied splash video/audio is retained; its playback,
volume and replay choices are settings. Runtime browser choices also use
`Data/SKSE/Plugins/SkyUltra.ini`.

The built-in app installs `installSkyUltraCompetitorShims` during its lifecycle.
Independent mod pages explicitly install their own shims before legacy scripts.
See the [SDK walkthrough](../../docs/SDK_GUIDE.md), [native API](../../api/README.md),
[PrismaUI guide](../../docs/PRISMAUI_MIGRATION.md), [Magelight guide](../../docs/MAGELIGHT_UI_MIGRATION.md),
and [credits/dependency inventory](../../docs/THIRD_PARTY_LIBRARIES.md).
