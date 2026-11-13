# SkyUltra SDK

[SDK walkthrough](../docs/SDK_GUIDE.md) · [Native API](../api/README.md) · [PrismaUI migration](../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../docs/THIRD_PARTY_LIBRARIES.md)

[Bridge delivery and runtime settings](../docs/BRIDGE_DELIVERY.md) explains native queue capacity/backpressure, the per-view diagnostics, browser frame pacing, the 1 GB package warning and JSON-only startup configuration.

Develop a web application with Skyrim as its runtime. C++ is optional. The core
bridge is framework-neutral; use vanilla JavaScript, TypeScript, React, Vue or
Svelte with your own HTML and CSS. SkyUltra's components, themes, menu and cursor
are opt-in. No runtime theme stylesheet is injected by the TypeScript SDK.

## Packages and entry points

| Entry | Purpose |
| --- | --- |
| `@skyultra/sdk` | Typed bridge, events, requests, cancellation, runtime detection, mocks, game data, actor renderer, performance, layout schema |
| `@skyultra/react` | Optional React bridge hooks |
| `@skyultra/ui/core` | React-only choices, meters, lists, segmented controls and stat surfaces |
| `@skyultra/ui/widgets` | Frames, HUD primitives, popups, action wheels, notifications, timelines, guides and command palette |
| `@skyultra/ui/game` | Actor stage/directory, inventory browser, character sheet, HUD editor/composer, live widgets, diagnostics and optional galleries |
| `@skyultra/ui/gameplay` | Small standalone gameplay HUD entry; one shared native-data feed |
| `@skyultra/ui/chrome` | Cursor scope, menu header/footer and settings sections |
| `@skyultra/ui/menu` | Optional complete SkyUltra menu |
| `@skyultra/ui/styles.css` | Opt-in component styles and theme variables |
| `@skyultra/ui/menu.css`, `/chrome.css`, `/hud.css` | Optional full-menu, chrome and gameplay-HUD layouts; import only the surfaces you use |

`@skyultra/ui/field/*` also exposes individual screen/hooks modules for explicit
lazy imports. The public game/chrome exports give the reusable names. The menu
uses React.lazy for heavy pages and does not load the component demonstration
gallery in its normal navigation.

## Start without C++

Create a framework-neutral Vite project from this checkout:

```powershell
node ./sdk/create-skyultra-mod/bin/create-skyultra-mod.js MyMod --id my.mod --local-sdk
Set-Location .\my.mod
npm install
npm run dev
```

Use any Vite frontend template in an existing project and add the SkyUltra Vite
plugin if you prefer another framework. The plugin supplies the local bridge,
validates the manifest, and emits `dist/manifest.json` with `dist/Files/` assets.
Copy those contents to **Data/SkyUltra/my.mod/**: the folder is the manifest ID.

For typed APIs, add the local SDK dependency while developing this checkout:

```powershell
npm install <source-checkout>/skyrim/sdk/typescript
```

Register the mod and view from a small Papyrus quest; no C++ DLL is required:

```papyrus
Int modHandle = SkyUltraNative.RegisterMod("my.mod")
Int view = SkyUltraNative.CreateFullScreenModView(modHandle, "main", True)
SkyUltraNative.AddPageRequestMethod(view, "ui.pageReady")
SkyUltraNative.AddPageRequestMethod(view, "ui.close")
SkyUltraNative.AddPageRequestMethod(view, "game.getPlayerSnapshot")
SkyUltraNative.AddPageRequestMethod(view, "game.setActorPreview")
SkyUltraNative.AddPageRequestMethod(view, "game.getActorPreviewStatus")
SkyUltraNative.AddPageRequestMethod(view, "game.closeActorPreview")
SkyUltraNative.AddPageRequestMethod(view, "game.saveActorPreview")
SkyUltraNative.AddPageRequestMethod(view, "game.captureActorAnimations")
SkyUltraNative.AddPageRequestMethod(view, "game.loadActorPoses")
SkyUltraNative.AddPageRequestMethod(view, "game.getActorThumbnail")
SkyUltraNative.AddPageRequestMethod(view, "game.setItemPreview")
SkyUltraNative.AddPageRequestMethod(view, "game.getItemPreviewStatus")
SkyUltraNative.AddPageRequestMethod(view, "game.closeItemPreview")
SkyUltraNative.AddPageRequestMethod(view, "game.saveItemPreview")
```

Put the same requested method names in `permissions.pageRequests` in your
manifest. Both gates apply: the manifest is the package's upper bound, and the
owner grants methods to each view. Keep storage/network permissions disabled
unless needed. A manifest declares content and permissions; it does not itself
start a quest or auto-open a mod. Custom request methods still need a Papyrus or
native handler. Renderer/game/UI methods above are handled by the framework.

Call `markSkyUltraPageReady()` after your first committed UI frame; the host can
keep a loading cover until then. Mods without that method release the global
until-ready splash screen when they produce a drawable frame. Pinned HUDs do not replay
the splash screen. A mod never needs to bundle its own copy of SkyUltraSplash.mp4.

## Actor renderer in TypeScript, Vue, Svelte or vanilla DOM

```ts
import { createSkyUltraRenderer, createSkyUltraGame } from '@skyultra/sdk';
const game = createSkyUltraGame();
const renderer = createSkyUltraRenderer();
const actors = await game.nearby(6000);
const actor = actors.results.find(value => value.referenceId);
if (actor) {
  const rect = document.querySelector('#actor-stage')!.getBoundingClientRect();
  await renderer.setActor({
    referenceId: actor.referenceId,
    bounds: { x: rect.x / innerWidth, y: rect.y / innerHeight,
      width: rect.width / innerWidth, height: rect.height / innerHeight },
    width: 512, height: 640, live: true, portrait: false,
    yaw: 0, pitch: 0, zoom: 1, lighting: 'soft',
  });
}
// Hide when obscured; reposition after scroll/resize; close on unmount.
await renderer.hide();
await renderer.close();
```

This API has no React or UI-kit dependency. It validates finite camera values,
supports AbortSignal through request options, and returns availability/readiness
and geometry counts. One native stage belongs to each view. Bounds are normalized
to that view. The renderer composites over the page: hide it while a popup covers
it. A record found by name still needs a loaded actor to render.

The public `lighting` option supports `studio`, `soft`, `dramatic`, `warm` and
`cool`; it applies to full views, portraits and PNG saves. It uses private
camera-relative lights and reuses captured geometry when only lighting changes.
The exported `SkyUltraPreviewLighting` type and `skyUltraPreviewLightingPresets`
list are available without importing React or the UI kit.

`renderer.captureAnimations()` explicitly samples compatible active clips for the
selected actor. Choose a returned opaque clip ID and frame with
`setActor({...actorAndBounds, animation:{id,frame}, live:false})`. The actor stage
provides **Capture animations**, clip choices and a frame slider; lighting and
PNG export use that pose. Captures are private to the owning view and expire on
recapture, pose-library loading, actor change or close.
`renderer.loadPoses()` provides an independent Idle/Fighting/Guard/Spellcasting
library for the standard humanoid rig through `game.loadActorPoses`; the actor
stage exposes **Load poses**. Availability depends on the installed rig and clip
assets. Verify supported species/presets in the target game setup. It reads installed animation
assets; no per-mod native DLL or shipped copy of game animations is needed.
See the TypeScript package guide for limits and the full request contract.

React users can use `<SkyActorStage actor={npc} bridge={bridge} obscured={false}/>`
from `/game`; it owns drag/wheel/keyboard input, sizing, visibility and cleanup.
Import optional styles or supply your own. Directory, inventory and sheet
components take explicit data/bridge props and do not force the complete menu.

For items, use `createSkyUltraItemRenderer()` and `skyUltraItemRendererMethods`.
`setItem({formId,bounds,lighting,...camera})` loads the record's world model;
`status()`, `hide()`, `close()` and `save({longEdge:4096})` follow the actor
workflow. A view owns one actor or item stage. PNGs go to `Data/SkyUltra/Items`
(redirected to MO2 Overwrite/output mod when active) using the full-view export
preference. `SkyItemStage` from `/game` provides the optional React controls;
`SkyInventoryBrowser` enables it when supplied a `bridge`. Both use the shared
camera/visibility/cleanup behavior. The latest source supports named alternate
texture-set overrides and lazy model thumbnails. Inventory now has a player
outfit/equipment tab plus Item studio. Grant skyUltraInventoryMethods for the
new paged inventory, thumbnail and explicit gameplay equip/unequip APIs. Runtime
validation of these additions remains user-run.
See the [TypeScript contract](typescript/README.md#item-world-model-previews).

## Renderer in Papyrus

```papyrus
; After the manifest and per-view methods above have been granted:
Bool queued = SkyUltraNative.SetActorPreview(view, "{\"referenceId\":\"0x000A2C94\",\"bounds\":{\"x\":0.5,\"y\":0.2,\"width\":0.4,\"height\":0.6},\"live\":true}")
; queued means accepted onto the game-thread queue, not painted.
String status = SkyUltraNative.GetActorPreviewStatusJSON(view)
; Poll after a delay, or receive skyultra.renderer in the page.
SkyUltraNative.CloseActorPreview(view)
```

Mutations run on Skyrim's game thread. Status reads copy a protected DTO without
touching actor graphs. The page receives `skyultra.renderer` for queued request
results; follow with a status read for eventual readiness. Portrait and live modes
use the same engine-independent native renderer. CEF/Ultralight rendering and
native materials remain subject to the compatibility limits below.

Wait until `GetViewState(view) == 1` before sending a Papyrus renderer mutation.

## Compose interfaces

```tsx
import { SkyCursorScope, SkyMenuHeader } from '@skyultra/ui/chrome';
import { SkyGameWidget, SkyHudComposer } from '@skyultra/ui/game';
import { closeSkyUltraPage } from '@skyultra/sdk';
import '@skyultra/ui/styles.css';  // Optional theme and component styles
import '@skyultra/ui/chrome.css'; // Optional header/cursor layout

<SkyCursorScope>
  <SkyMenuHeader title="My own interface" onClose={() => void closeSkyUltraPage()} />
  <SkyGameWidget kind="vitals" player={snapshot} variant="aether" />
</SkyCursorScope>
```

Use `variant="plain"` for an undecorated game-data widget. `SkyCursorScope` is
a CSS-native cursor for browser previews, without a mousemove/RAF loop; Skyrim
draws its corresponding native cursor before/after browser startup. It does not
attempt to make OS cursor images render inside an offscreen browser. Use your
own DOM cursor with your own input handling if a mod needs a different effect.
Grant `ui.setCursor` and call `setSkyUltraCursor('hidden')`, or pass your bridge
and `nativeMode="hidden"` to `SkyCursorScope`, to avoid a second native pointer.
Cursor visibility belongs to the view under the pointer, so a hidden cursor setting
does not affect another mod's view. Restore `'native'` when disposing your custom cursor.

`SkyHudComposer` is controlled: pass a version-1 layout, `onValueChange`, and
`renderWidget`. It supports order editing and JSON import/export; it does not pin,
save, or modify the game itself. `parseSkyUltraHudLayout` validates exported data.
The built-in HUD editor adds native pinning and waits for child-page readiness.
Custom hosts can instead use their own transparent, click-through HUD entry.

## Reliability and performance

- Keep assets local; the same API works on both engines. Feature-test browser
  features and use CSS fallbacks where their support differs.
- Unmount or hide actor stages when unused. Avoid scanning game objects every RAF.
- The shared HUD feed requests compact player data every 2.5 seconds and pauses
  when hidden. It does not repeatedly sort full inventory data.
- GPU splash screen transfers use shared textures and no CPU pixel readback. H.264 hardware
  decode availability depends on Windows/driver support; a GPU texture path alone
  is not proof of hardware video decoding.
- RAF is callback cadence, not game FPS. Native compositor timing excludes browser
  work; host-device timing excludes CEF's separate producer. Match workloads and
  inspect native sample counts before comparing engines.
- New code is source-checked, not yet a proven performance win. The native actor
  lighting is independent preview lighting, not a reproduction of world lighting,
  ENB/Community Shaders or the complete Skyrim material renderer. Unloaded NPC
  assembly and VR actor previews remain unfinished.

See the [menu build/player guide](../apps/skyultra-menu/README.md),
[Papyrus SDK](papyrus/README.md), [TypeScript SDK](typescript/README.md),
[UI kit](ui/README.md), and [working roadmap](../ROADMAP.md).
