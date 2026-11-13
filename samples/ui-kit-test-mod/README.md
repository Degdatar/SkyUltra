# Legacy UI-kit sample

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

The UI is now the built-in **SkyUltra menu**. Use [the current build/player guide](../../apps/skyultra-menu/README.md) and [SDK guide](../../sdk/README.md). The source files here are compatibility consumers of SDK components; new work belongs to the SDK and apps/skyultra-menu.

# SkyUltra Field Atlas — player and mod-author guide

A reusable Skyrim interface kit and test mod for **CEF and Ultralight**. The
Atlas includes a framed navigation shell, permanent header/footer, eleven
palettes, a logo loading screen, live character data, actor previews, inventory,
a HUD builder, component demonstrations, and performance measurements.

## Start here

1. Open the installed Degdatar view as usual. `Shift+F6` reopens a closed
   panel. The SkyUltra logo stays visible until the page signals readiness.
2. Use **Overview** to see the current character and equipment. Select a gear
   row or category to inspect it in **The collection**.
3. Visit **HUDs & widgets** to choose a layout. You can keep it entirely inside
   the preview, or explicitly **Pin over gameplay**.
4. Press **F1** for help with the current section, or **Ctrl+K** to find a tool.
   The **field guide** is also available directly in the navigation rail.

Non-editable text cannot be selected or dragged. Inputs and text areas retain
normal editing and selection. Palettes use in-page controls on both engines.

## Controls

| Input | Action |
| --- | --- |
| `1`–`8` | Overview, actors, inventory, character, workshop, widgets, performance, guide |
| `Ctrl+K` | Search the command palette |
| `F1` | Contextual help |
| `/` | Focus the current page's search field |
| `Tab`, `Shift+Tab` | Move keyboard focus one item at a time (1-by-1 typematic guard) |
| `Enter`, `Space` | Activate a focused control |
| `Esc` | Dismiss an open popup/menu first; otherwise close the main panel |
| `Shift+F6` (or custom 1–3 input chord) | Reopen the panel in Skyrim (supports Keyboard, Gamepad Controller, and VR Headset) |
| Drag the actor | Orbit horizontally and vertically |
| Mouse wheel over actor | Zoom; the page does not scroll underneath |
| Double-click actor, or `Home` | Reset the camera |
| Arrows, `+`, `−` while actor focused | Keyboard orbit and zoom |

Section shortcuts are ignored while editing, choosing a menu item, or using a
modal popup. Motion can be reduced in the workshop or through the OS setting.

## The eight sections

| Section | Features |
| --- | --- |
| Overview | Health/magicka/stamina rings; gold, armor, carry weight; live equipment; inventory totals and category links |
| Actor observatory | Nearby actors, followers, debounced NPC name search; full model, frozen pose, portrait; direct mouse camera control; recapture |
| The collection | Search, category/worn filters, sort, grid/list views, item inspection and item metadata |
| Character sheet | Six defenses, radar plot, 18 skills, timed/constant/harmful effect filters |
| Interface workshop | Buttons, inputs, sliders, toggles, tabs, badges, notices, toasts, tooltips, dialogs, context menus, inventory/equipment surfaces |
| HUDs & widgets | Seven live widgets, four presets, pin/unpin, position/size/opacity; action wheel, quantities, confirmations, transaction and searchable picker patterns |
| Performance lab | Startup milestones, callback cadence chart, 24/48/128/240-item fixture, 30-second captures and session comparisons |
| SkyUltra Guide | Interactive 5-step walkthrough, Keyboard/Gamepad/VR shortcuts, feature instructions, and mod-author hotkey & UI integration notes |

### HUDs and widgets

The live gallery contains a **vital ribbon**, **character plate**, **readied
hands**, **field supplies**, **active influences**, **wayfarer compass**, and
**Tamriel clock**. Values come from Skyrim; the clock uses the game calendar.
The compass reports player heading and location, not Skyrim's map markers.

Select individual widgets or choose Adventurer, Minimalist, Spellbinder, or
Wayfarer. Set one of four corner anchors, interface size, and opacity. Pin the
layout, then close the panel. The widgets pass clicks through to gameplay.
Reopen the panel and select **Update pinned layout** to apply changes, including
a new palette. **Unpin all** removes the extra surface. Pinning is optional and
only available in Skyrim. Layouts are session-only and clear on a new/load game.

A pinned layout uses one transparent browser surface and one compact snapshot
feed, regardless of its widget count. It requests updates every 2.5 seconds;
its values are sampled, not frame-by-frame. Dense layouts use tighter spacing
and fit down to the available surface height so widgets remain inside it.
At very small game resolutions, use fewer widgets or a smaller interface size.

The main panel suspends its browser when closed; the pinned surface stays
visible during gameplay and suspends for Skyrim modal menus. Returning from a
menu does not reopen an already closed main panel. Unpinned previews create no
extra browser surface. Eight mods may each
own one pinned layout. A mod cannot configure another mod's HUD.

### Actor rendering

Select a nearby actor/follower, or type at least three characters in **Find NPC**.
Search returns up to 40 records. Native rendering needs a matching **loaded
actor reference with third-person 3D**. Searching an unloaded NPC's record does
not spawn an actor. Portrait mode captures a head view; full-model live mode
tracks the loaded actor's existing pose at up to 30 updates/second. Freezing
stops pose capture while keeping camera control available.

The renderer independently consumes supported Skyrim triangle geometry, skin
bones, dynamic FaceGen positions, and loaded material resources. Its material
path now includes hair tint (with vertex-green tint weighting), FaceGen tint
and detail maps, RGB skin tint, vertex colors/alpha, tangent/model-space normal
maps, specular masks, UV scale/offset and clamp modes, soft/rim/back-light masks,
environment maps, and glow maps. Opaque meshes and transparent meshes use their
authored culling/depth/alpha settings; transparent packets draw back to front.
SSE shared vertex domains/global skin indices are used consistently. Actor pixels are
composited as a D3D11 texture after the owning browser surface; they are not
PNG/JPEG frames or a CPU canvas stream. Camera requests are coalesced, shader
variants initialize only when needed, and unchanged supported poses reuse the
existing output. Materials blend into a private linear half-float target, then
one GPU pass converts to display color and premultiplies for UI composition.
This avoids dark linear colors and incorrect alpha edges in the UI texture.
Both targets and material states are cached; there is no pixel readback or
image encoding. `SkyUltra.log` records the captured material coverage and tint
values on the first rendered frame, so missing hair/face resources are visible.

This is **not an exact replay of Skyrim's world render**. The preview uses its
own lighting and does not run ENB/Community Shaders, world shadows, or the game's
image-space pipeline. Some custom geometry, special shaders, and FaceGen
combinations may still differ or be omitted. VR
actor previews and assembling an unloaded NPC are not implemented. The current
face/mesh corrections require a rebuilt in-game check; source review cannot
prove compatibility with the user's complete mesh/mod list. The page reports
unsupported geometry rather than substituting a fake portrait.

Native pixels sit above their owning page, so integrations must hide their
stage during covering menus, popups, and clipping/scroll transitions. The Atlas
handles its own overlays and viewport changes.

### Popups, wheels, and fixtures

**Popups & wheels** provides an eight-direction quick wheel, bounded quantity
picker, confirmation, transaction summary, searchable real-inventory picker,
step guide, notifications, timeline, character card, and objective tracker.
Keyboard focus stays inside a modal popup and returns when it closes. Escape
returns to the panel before closing the panel itself.

Confirmation/transaction/quest examples are **UI patterns**. They do not buy,
sell, equip, recruit, issue commands, or alter quests. A mod supplies the data
and authorized action handlers. Inventory icons identify categories; actual
item thumbnails are not implemented by this sample.

## Performance and loading

The native compositor draws the original SkyUltra logo before a visible CEF
view begins its host initialization, then keeps a logo while an opted-in page
is loading. The HTML boot shell uses the same original logo. There is **no
minimum splash time, fake percentage, or intentional multi-second delay**.
`ui.pageReady` hands off to the committed shell; it does not wait for inventory
or NPC scans. After 20 seconds without that signal, the host exposes the HTML
recovery screen so a failed page is diagnosable.

The initial route imports React-only core components. Other pages and the
heavier workshop dependencies load on demand. Host data requests run on the game
task queue directly, without a Papyrus VM request/response roundtrip. Compact
HUD snapshots skip inventory enumeration/sorting and the skills/resistance
arrays. Hidden main panels stop rendering and skip snapshot polling. CEF OSR
renderers keep normal process priority; hidden views still use `WasHidden`.

The performance lab separates:

- **App module → shell RAF:** work after the app module starts executing.
- **Navigation → shell RAF:** includes document/module loading.
- **Navigation → live-data RAF:** first scheduled frame with character data.
- **Page RAF / mean interval:** callback cadence while the page is visible.

RAF is not proof of presented Skyrim FPS, GPU execution time, or a fair engine
winner. The counter now snapshots its numeric samples before resetting the
bucket, avoiding the previous deferred-update `NaN`/`Infinity` result.

For comparison: use the same save, camera, resolution, theme, motion setting,
widget state, and item density. Measure cold process startup separately from a
warm reopen. Live snapshot polling pauses during a 30-second main-panel capture;
unpin gameplay widgets for a baseline run. Read native create/load/first-paint
and GPU/compositor diagnostics in `SkyUltra.log`, plus `SkyUltra_CEF.log` for
CEF. Include Skyrim's own frame-time measurements. **No measured performance
win over Magelight or either backend is claimed by these source changes.**

## Build it yourself

From PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\SkyUltra\skyrim\tools\Build-UiKitTest.ps1
```

If dependencies are already installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\SkyUltra\skyrim\tools\Build-UiKitTest.ps1 -SkipDependencyInstall
```

The helper compiles `SkyUltraNative.psc` and `SkyUltraPapyrusSampleQuest.psc`,
uses the existing `local` / `local-release` CMake presets, and builds the
TypeScript SDK, UI kit, and multipage sample in order. It requires the existing
CMake/C++ toolchain, Node/npm, vcpkg, and Skyrim Papyrus compiler/flags. Use
`-GameDirectory "D:\path\to\Skyrim Special Edition"` for another game directory.
It does not copy anything into the game or mod manager.

### Files to copy after building

| Source (under `D:\SkyUltra\skyrim\`) | Destination |
| --- | --- |
| `build\package\Data\SKSE\Plugins\SkyUltra.dll` and optional debugging `SkyUltra.pdb` | Native SkyUltra mod: `SKSE\Plugins\` |
| `build\package\Data\SKSE\Plugins\resources\SkyUltra\Files\SkyUltraLogo.png` | Native SkyUltra mod: `SKSE\Plugins\resources\SkyUltra\Files\SkyUltraLogo.png` |
| `build\package\Data\SKSE\Plugins\resources\SkyUltra\Files\SkyUltraBridge.js` | Native SkyUltra mod: the same relative resources path |
| `build\package\Data\Scripts\SkyUltraNative.pex` | Native SkyUltra mod: `Scripts\` |
| `build\package\Data\Scripts\SkyUltraPapyrusSampleQuest.pex` | Sample mod: `Scripts\` |
| **Entire contents** of `samples\ui-kit-test-mod\dist\` | `D:\Steam\steamapps\common\Skyrim Special Edition\MO2\mods\<YourMod>\SkyUltra\Degdatar\` |

The web destination must contain `manifest.json` directly and `Files\` beside
it. `Files\` must include **index.html, hud.html, SkyUltraBridge.js,
SkyUltraLogo.png, and assets\ with every emitted chunk**. Do not copy only the
main JS file or nest another `dist` folder. Preserve existing runtime DLLs,
engine resources, ESP, and your configured INI. The new DLL, quest PEX, web
manifest, and full web folder belong to the same update.

The manifest ID is `degdatar`. The source defaults to `"browserEngine":"cef"`.
For an Ultralight comparison change that field to `"ultralight"` before building, or in the
built manifest for a one-off install. Fully restart Skyrim between engine runs
and confirm the actual selected engine in the log.

For desktop preview, after building the local SDK/UI packages run `npm run dev`
in `D:\SkyUltra\skyrim\samples\ui-kit-test-mod`. It uses fixture character data;
native actors and gameplay pinning are unavailable there.

## Mod-author integration

Use `@skyultra/ui/core` for a small initial shell and `@skyultra/ui/widgets` for
the React-only widget/popup collection. The full `@skyultra/ui` entry also
provides Radix and motion-based workshop primitives. Import the stylesheet once.

```tsx
import { useEffect } from "react";
import { getSkyWebView, connectSkyUltraInputScopes,
         markSkyUltraPageReady } from "@skyultra/sdk";
import { SkyFrame, SkyVitalsHud } from "@skyultra/ui/widgets";
import "@skyultra/ui/styles.css";

const bridge = getSkyWebView();
function Panel() {
  useEffect(() => connectSkyUltraInputScopes(bridge), []);
  useEffect(() => {
    let second = 0;
    const first = requestAnimationFrame(() => {
      second = requestAnimationFrame(() => {
        void markSkyUltraPageReady(bridge).catch(console.error);
      });
    });
    return () => { cancelAnimationFrame(first); cancelAnimationFrame(second); };
  }, []);
  return <SkyFrame title="Condition" eyebrow="YOUR MOD">
    <SkyVitalsHud vitals={[{id:"health",label:"Health",value:80,max:100,tone:"health"}]} />
  </SkyFrame>;
}
```

Enable JavaScript, asset access, and matching **manifest + per-view allowlists**:

```json
{
"pageRequests": [
  "game.getPlayerSnapshot", "game.getNearbyNpcs", "game.searchNpcs",
  "game.setActorPreview", "game.getActorPreviewStatus", "game.closeActorPreview",
  "game.saveActorPreview", "game.captureActorAnimations",
  "game.loadActorPoses",
  "game.setItemPreview", "game.getItemPreviewStatus", "game.closeItemPreview", "game.saveItemPreview",
  "ui.pageReady", "ui.setInputScope", "ui.configureWidgets", "ui.getPerformanceSnapshot"
]
}
```

Papyrus integrations use `SkyUltraNative.AddPageRequestMethod` for each needed
name; native integrations use the equivalent per-view API. Built-in game/UI
requests are serviced by the host after the permission checks. Custom request
names still need your own handler. Page types do not grant permissions.

The collection's selected-item inspector uses `SkyItemStage` and
`createSkyUltraItemRenderer()` for native world models, orbit/pan/zoom, lighting
and PNG Save. Its four item methods above need both permission gates; the sample
quest source includes them. PNGs use the full-view quality setting and write
under `Data/SkyUltra/Items`, redirected by MO2 to Overwrite/output mod. The initial
source path supports record world/armor ground models; alternate texture sets
report unavailable, and list/grid category glyphs remain until thumbnail caching
is implemented. Rebuild the sample quest as well as its page when testing this
separate sample. The built-in SkyUltra menu grants its own manifest methods.

- `ui.pageReady`, `{}`: releases the opted-in native loading screen.
- `ui.setInputScope`, `{depth: 0..8}`: tells the host a popup/menu owns Escape.
  `connectSkyUltraInputScopes` handles kit primitives automatically. Custom
  overlays can pair `enterSkyInputScope()` with its returned release function
  from `@skyultra/ui/interaction`.
- `game.getPlayerSnapshot`, `{scope:"hud"}`: compact vitals/identity/hands/
  resources/effects/calendar feed; inventory arrays/totals and skill/resistance
  arrays are deliberately not populated in this scope. Omit scope for full data.
- `ui.configureWidgets`, `{query:true}`: reads this mod's session layout.
- `ui.configureWidgets`, `{enabled:true,widgets:["vitals","equipment"],
  anchor:"top-right",scale:1,opacity:1,theme:"obsidian"}`: creates/replaces this
  mod's HUD. Other widget IDs: character, resources, effects, compass, clock.
- `ui.configureWidgets`, `{enabled:false}`: destroys this mod's pinned surface.

The host resolves a sibling **hud.html** beside the manifest entry, constrains
it to the mod's asset root, uses the owning view's actual backend, and grants
the child only its required built-in snapshot/readiness methods. Supply that
page if using the generic pin service. The Vite plugin injects the native bridge
into every emitted HTML entry and packages them under `Files/`.

## Feature coverage and boundaries

MagelightUI and SeverActions were inspected as behavior/design references.
This implementation does not call their APIs or copy their renderer code.
SeverActions' public repository distributes the mod; its native renderer source
is private ([project details](https://github.com/Severause/SeverActions#under-the-hood)).

| Reference capability | SkyUltra status |
| --- | --- |
| GPU web UI, bridge, themes, reusable control surfaces | Implemented in SkyUltra; both backends |
| Portraits and live model stage | Independent supported-mesh renderer for loaded actors; compatibility needs in-game validation |
| Inventory, equipment, stats, skills, active effects | Live sample surfaces; category icons, no item thumbnail renderer |
| Search, popups, quantities, confirmations, wheel, help | Reusable components and working UI demonstrations |
| HUDs, party-style character cards, supplies, timelines | Reusable patterns; seven native-data widgets can be pinned |
| Outfits, staged equipment, survival rules, NPC commands, travel, economy, relationship/bio systems | A mod must supply gameplay logic; not implemented as native game systems by this UI kit |
| Unloaded NPC assembly, full game-shader parity, VR actor renderer | Not implemented |

These are source changes. Type checks and static asset checks do not replace a
user build or game testing on CEF and Ultralight.

### Native timing and report export

The performance page also calls `ui.getPerformanceSnapshot` once per second.
It reports completed D3D11 timestamp samples, elapsed overlay callback CPU
measurements, and main swap-chain Present-call intervals. It creates no extra
browser view and does not start the profiler on the initial route. The added
CPU/Present/compositor measurement is leased for five seconds and stops when
requests stop; existing host GPU log instrumentation remains.

- **Compositor GPU** starts after browser/actor rendering. Use this matched scope
  to compare composition with the same visible/pinned view layout.
- **Host-device GPU** includes work submitted on Skyrim's device. It excludes
  CEF's external browser-rendering work; it is not total UI GPU execution time.
- **Callback CPU** is elapsed render-callback time, including waits/preemption.
  It is not whole-game CPU execution time or CEF subprocess CPU time.
- **Present interval** is swap-chain call cadence. It is not a GPU timer or a
  count of displayed frames.

Live mean/P95/max values use up to 512 recent completed samples. A 30-second
capture uses before/after cumulative counters to calculate weighted native
means; it does not average the once-per-second display averages. The report
states the actual native observation duration and sample counts. Completion
latency means native and browser sampling boundaries can differ slightly.
GPU queries are read without flushing or waiting; skipped/disjoint samples
are counted. Device-generation changes invalidate the native comparison.
Missing or stale data displays as unavailable/pending/stale, not zero cost.

**Export report** opens copyable JSON containing startup figures, page runs,
native summaries, workload metadata and scope descriptions. Copy it to a local
file for comparisons. The browser preview records no fake native GPU results.
The updated DLL, sample quest PEX and manifest/web package are all needed for
this request's permission. Mod authors must allow `ui.getPerformanceSnapshot`
in both their manifest and their per-view request allowlist.

The [working roadmap](../../ROADMAP.md) records which parts of the original UI
and platform plan remain, including item thumbnails, outfit staging, larger
collections, richer domain surfaces, and complete controller/VR validation.
