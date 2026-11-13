# SkyUltra TypeScript SDK

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

The root entry also exports `createSkyUltraRenderer`, `createSkyUltraGame`,
`skyUltraRendererMethods`, game-data contracts, `parseSkyUltraHudLayout`,
`readSkyUltraSettings`, `updateSkyUltraSettings`, `closeSkyUltraPage` and
`setSkyUltraCursor`. These APIs have no frontend-framework dependency.
Only the built-in menu can write framework settings. Request methods require
manifest permission and a per-view grant. See [the complete SDK guide](../README.md).

`@skyultra/sdk` provides typed events and native requests, runtime and
capability detection, and a browser mock for SkyUltra web pages. It does not
load the game-side bridge script or expose the native C ABI.

## Setup

From this directory, build the package with Node.js and npm:

```text
npm install
npm run build
```

In your page project's `package.json`, add this directory as a local file
dependency (for example, `"@skyultra/sdk": "file:../sdk/typescript"`), then
bundle the TypeScript page with your normal front-end build tool.

## Local preview with live reload

Run the development server against a web mod's source folder:

```text
npm run dev -- --root "D:/path/to/your/mod-page"
```

Open the printed `127.0.0.1` preview address and enter the page path and target
viewport size. Changes to files reload the page. The preview can emit
`skyultra:message` events to the page and supplies a simulated `SkyWebView`
bridge whose requests resolve with `{ preview: true, name, params }`. The
server binds only to localhost and serves files from the selected root.

This previews standard browser HTML/CSS/JavaScript behavior; it does not run
inside Skyrim, reproduce D3D11 composition, or exercise native API callbacks.

The bridge source is `resources/SkyUltraBridge.js`. For a hand-packaged mod,
copy it beside the page as `Files/SkyUltraBridge.js`; the Vite plugin serves
that local file in development and packages it automatically for release.
Load it before calling `getSkyWebView()`:

```html
<script src="./SkyUltraBridge.js"></script>
<script type="module" src="./app.js"></script>
```

Import the SDK in a TypeScript page and declare the event/request shapes used
by that mod:

```ts
import { getSkyUltraClient, getSkyUltraRuntimeInfo } from "@skyultra/sdk";

interface Events {
  inventoryUpdated: { count: number };
}

interface Requests {
  "sample.getStatus": {
    params: { verbose?: boolean };
    response: { ready: boolean };
  };
}

const sky = getSkyUltraClient<Events, Requests>();
const runtime = getSkyUltraRuntimeInfo();
console.info(runtime.host, runtime.pluginVersion, runtime.capabilities);

const stopListening = sky.on("inventoryUpdated", (data) => {
  document.querySelector("output")!.textContent = String(data.count);
});

const status = await sky.request("sample.getStatus", { verbose: true });
console.log(status.ready);

window.addEventListener("pagehide", stopListening, { once: true });
```

`getSkyUltraRuntimeInfo()` reports the detected host (`skyrim`, `preview`,
`browser`, or `unknown` when a bridge provides no host metadata), available
bridge capabilities, and version metadata when provided by the host.
For local file pages hosted by SkyUltra, `browserEngine` reports the actual
per-view backend (`cef` or `ultralight`). It remains `unknown` for standalone
browser pages, data URLs, and other hosts that do not provide engine metadata.
`getSkyWebView()` remains available for pages that use the original untyped
accessor.

For a manifest-backed in-game page, SkyUltra also implements native actor
preview requests. Declare each method in `permissions.pageRequests` and add it
to the view's allowlist. Papyrus pages use `SkyUltraNative.AddPageRequestMethod`;
the host handles these requests directly after both permission checks.

```ts
import { getSkyWebView } from "@skyultra/sdk";

const previews = getSkyWebView();
await previews.request("game.setActorPreview", {
  referenceId: "0x000A2C94", // or baseFormId for an already loaded instance
  bounds: { x: 0.15, y: 0.12, width: 0.3, height: 0.7 },
  width: 512, height: 640,
  visible: true, live: true, portrait: false,
  yaw: 0, zoom: 1,
});
const status = await previews.request("game.getActorPreviewStatus", {});
await previews.request("game.closeActorPreview", {});
```

Bounds are normalized to the owning page's viewport. Update them on resize or
scroll, and send `{ visible: false }` while an HTML modal covers the preview.
The native texture is composited after that page, so its own HTML cannot cover
the model. Higher browser views and the native cursor retain their normal order.
`live: false` freezes the captured pose; `portrait: true` frames the head;
`refresh: true` captures a new pose. Rotation is in radians and zoom is bounded.
Live motion follows the existing actor's in-world pose; it does not run an
independent animation or outfit-staging scene.

The renderer facade also exposes preview poses, lighting, panning and PNG export:

```ts
import { createSkyUltraRenderer, skyUltraRendererMethods } from '@skyultra/sdk';
// Grant every method in skyUltraRendererMethods in the manifest and allowlist.
const renderer = createSkyUltraRenderer();
await renderer.setActor({
  referenceId: '0x000A2C94',
  bounds: { x: .15, y: .12, width: .3, height: .7 },
  pose: 'current', portrait: false, live: false,
  lighting: 'soft',
  panX: .1, panY: -.1, zoom: 1.2,
});
// Wait for status().ready before saving a newly selected actor or pose.
const { path } = await renderer.save({ longEdge: 4096 }); // fresh 4K render
```

`current` preserves the captured in-world pose. The legacy `pose` values `idle`,
`fighting`, `guard` and `spellcasting` apply static arm poses to one captured humanoid skeleton, retaining
the captured body posture and propagating changes to attached descendants.
FaceGen and outfit bind spaces remain separate; the preview does not reconstruct
the skeleton from per-mesh inverse binds. Presets neither run HKX animations nor
alter the NPC.
Non-humanoid skeletons retain their current pose (`posesAvailable: false`).
The menu now uses independent HKX poses through `loadPoses()` instead of those
legacy arm adjustments; the legacy request values remain for existing mods.
Positive `panX`/`panY` moves the actor right/down by a fraction of the image;
the native host clamps each to -2…2. Camera-only changes reuse the snapshot.
Yaw and pitch support free rotation in either direction. Camera dragging wraps
angles into -π…π radians while preserving orientation through complete turns;
pitch is not limited to the former 0.55-radian range.

### Captured animation poses

After a model is selected, `renderer.captureAnimations()` samples compatible
active HKX clips from that actor. It returns
`{available, clips:[{id,name,duration,sampleCount}], message}`. With no supported
clips it returns `available:false, clips:[]`; show the message instead of claiming
a preset is available. Grant `game.captureActorAnimations` in both the manifest
and per-view allowlist (`skyUltraRendererMethods` includes it).

```ts
// Reuse the identity, bounds and camera passed to setActor().
const captured = await renderer.captureAnimations({signal});
const clip = captured.clips[0];
if (clip) {
  await renderer.setActor({...actorAndBounds, live:false,
    animation:{id:clip.id, frame:Math.floor((clip.sampleCount - 1) / 2)}});
  // Wait for status().ready, then export the selected frame and lighting.
  await renderer.save();
}
```

The host captures up to four clips, each with 33 discrete sampled frames
(`frame:0..32`, evenly spaced across `duration` seconds). Capture is explicit,
not part of the live polling loop. IDs belong to that view/actor and expire on
another animation capture, actor change or view close. Camera, lighting and
portrait/full-view changes reuse them. `animation` and a non-`current` arm
`pose` are mutually exclusive; omitting `animation` restores normal live/frozen
capture. The facade validates metadata/frames and forwards AbortSignal.

The game task reads the actor's animation graph under its update lock and calls
`hkaAnimation::SampleTracks` into private aligned buffers. Rig names, bone maps,
hierarchy, transforms and limits are checked. The captured pose anchors root
motion, honors locked bone translations and calibrates the rig-to-NIF units and
model alignment from matching captured bones. Only owned numeric frame data is
retained. The renderer applies sampled bones consistently to FaceGen/outfit skin
matrices; extra face bones and rigid attachments inherit their nearest sampled
ancestor. Full-view bounds are recomputed for the pose, and portraits use the
posed head. Native rendering/export keep using the existing private D3D11 path.

`captureAnimations()` samples existing active clips; it skips additive,
mirrored, retargeted/mapper clips, incomplete rigs,
clips longer than 120 seconds and rigs over 512 bones. Only clips already active
on the selected actor can be captured; their recorded frames can then be chosen
without changing the live NPC. Float tracks, animation events, procedural IK,
physics and prop visibility events are not replayed. The remaining standalone
preview-animation validation is tracked in roadmap milestone R2.3.

Capture resolves the active playback control's binding before falling back to
the generator binding. An empty `transformTrackToBoneIndices` map means identity
mapping; explicit `-1` entries are skipped. Invalid/out-of-range/duplicate bone
maps remain rejected. Generator mapper metadata alone is not treated as proof
that an active playback control needs retargeting. Capture diagnostics in
`SkyUltra.log` include traversal/binding counts and bounded per-clip reasons.

### Independent pose library

`renderer.loadPoses({signal})` loads Idle, Fighting, Guard and Spellcasting
directly from the installed game's animation resources. The NPC can be doing
another action. Grant `game.loadActorPoses` in the manifest and per-view
allowlist; it is included in `skyUltraRendererMethods`. The method returns the
same catalog as capture, with optional `preset: 'idle' | 'fighting' | 'guard' |
'spellcasting'` on each library clip (`SkyUltraActorPreset` and
`skyUltraActorPosePresets` expose these IDs). Missing/incompatible files may
produce a partial catalog or `available:false` with a message.

```ts
const library = await renderer.loadPoses({signal});
const fighting = library.clips.find(clip => clip.preset === 'fighting');
if (fighting) {
  await renderer.setActor({...actorAndBounds, pose:'current', live:false,
    animation:{id:fighting.id, frame:16}, lighting:'studio'});
}
```

Loading poses replaces the view's previous clip collection. The returned IDs
work with the existing frame slider, portrait/full view, camera, lighting and
PNG export. This source implementation targets the standard 99-bone humanoid
animation rig. Clips must cover its body bones; camera tracks may be omitted,
including by 97-track idle overrides. Animals and other rigs continue to
use active-clip capture. The four sources are male/female `mt_idle.hkx`,
`h2h_idle.hkx`, `1hm_blockidle.hkx` and `mrh_aimedconcentration.hkx` under
`meshes/actors/character/animations`. They provide an unarmed fighting stance,
one-handed guard and right-hand concentration cast; equipment is retained.

The game resource stream resolves BSA/loose files and active mod-manager
overrides. No game assets are bundled or copied by SkyUltra. A private loader
validates 64-bit Havok 2010/version-8 packfile structure, pointer/array bounds,
normal spline class, timing/block tables, endian and track mapping before
using the game's matching spline codec. Serialized root-motion/annotation
pointers are cleared, and codec input is owned for the sampling operation.
Only numeric frames remain afterward. The live actor's graph/bindings are never
swapped or activated; graph locks are released before resource I/O. Physics,
animation events, magic effects, automatic equipment changes and a separate
behavior graph are not part of this pose library.

Pose availability depends on the installed animation assets and rig compatibility.
The loader permits omitted camera tracks in idle overrides. Test each preset and
engine you support in the target game setup; private sampling does not replay the
full game animation graph or its procedural effects.

### Item world-model previews

`createSkyUltraItemRenderer()` provides a separate typed facade over the same
native stage. Grant all four entries in `skyUltraItemRendererMethods` in both
the manifest and the per-view allowlist:
`game.setItemPreview`, `game.getItemPreviewStatus`, `game.closeItemPreview` and
`game.saveItemPreview`. No mod-specific DLL is needed.

```ts
import { createSkyUltraItemRenderer, skyUltraItemRendererMethods } from '@skyultra/sdk';
const items = createSkyUltraItemRenderer();
await items.setItem({
  formId: '0x00012EB7',
  bounds: {x:.55,y:.2,width:.35,height:.45},
  width:640, height:480, yaw:.5, pitch:.35, zoom:1,
  lighting:'soft', panX:0, panY:0,
});
// Once (await items.status()).ready is true:
await items.save({longEdge:4096});
await items.hide();  // when a popup covers the preview
await items.close(); // on unmount
```

`formId` is a positive 32-bit integer, decimal string or `0x` hexadecimal string;
it identifies an inventory base form, not a placed reference or a file path.
Bounds, camera, lighting, visibility, refresh and AbortSignal follow the actor
contract. Camera/light updates reuse captured geometry; `refresh:true` reloads
the model. `available` reports capture success, while `ready` waits for an actual
render. One actor **or** item occupies a view's stage. Calls to hide/close/save
an actor stage cannot operate on an item stage, and vice versa. View teardown
releases either kind.

The host loads the installed item's world-model asset into a private clone and
captures its geometry/materials. Armor uses its first nonempty male/female
ground model. Nothing is spawned, equipped or inserted into the game world,
and the vanilla inventory renderer is not used. Items with no model, unsupported
geometry report unavailable. Named alternate-texture overrides are applied to
captured packets while retaining their texture owners; shared game materials
are not changed. Animated/effect-only shaders still have limited coverage.
The menu's Item studio inspector and Inventory grid use real models/thumbnails;
its separate equipment tab renders the loaded player's current outfit.

Item redraws use private vertex/index buffers and texture views copied on the
render thread once per captured snapshot. Camera/lighting changes, target resize
and PNG export reuse those copies. The source model/material owners stay leased
through the snapshot lifetime and release on a game task; drawing does not
dereference those game objects. Texture copies preserve mip chains, cube faces
and sRGB/data formats and deduplicate identical source views. The optional React stage
updates lighting without restarting its observer/request controller. These
follow-up changes address a reported lighting-selection crash and still need
native build and game retesting.

A subsequent movement crash still occurred with the private mesh patch active.
The latest native source gives each item packet/target its own default-usage
constant buffer and performs full updates, avoiding the shared mapped actor
buffer for both camera and lighting changes. Source syntax and UI request tests
pass; this additional compatibility change still needs a user DLL build and
driver/game retest. Crash resolution is not claimed by source checks.

Rotation also crashed with those buffer changes active. The current flat native
compositor adds engine-renderer and D3D11 immediate-context synchronization for
the complete shared pass, including state restoration and UI/Present handoff.
This closes a source-level synchronization gap; runtime crash resolution still
requires user testing. The `SkyUltra render synchronization v3` log identifies
the updated path. Public renderer methods and frontend assets are unchanged.

The next crash identified an invalid diffuse texture view in SkyUltra's redraw
loop with synchronization v3 active. The latest native source adds the model
lease/private texture snapshots described above, removing original game views
from item redraw device checks/binds. `SkyUltra item GPU snapshot v4` identifies
this path. Separate software-D3D11 copy/lifetime tests pass; the Skyrim/NVIDIA
crash fix still requires user build and game confirmation.

PNG saves use a fresh transparent render under `Data/SkyUltra/Items`. MO2
redirects new output to Overwrite (or its configured output mod); Vortex/normal
launches write to Data. `save()` defaults to the full-view quality preference;
`longEdge` can override it from 720 to 4096. Portrait/animation controls apply
only to actors. This new item path is source-only and awaits native build and
both-engine game validation.

`lighting` accepts `studio` (default), `soft`, `dramatic`, `warm` or `cool`.
`skyUltraPreviewLightingPresets` and `SkyUltraPreviewLighting` expose the choices
and type. Key/fill lights follow the preview camera; the presets change the
private studio shader's light colors, ambient and highlight strengths. Switching
lighting reuses the captured geometry, including a frozen pose. A PNG save
retains the selected preset at either export resolution. This does not change
world lights, bake shadows or reproduce ENB/Community Shaders. Unsupported values
are rejected by both the facade and native JSON handler. Existing callers can
omit the option. Native/frontend builds and both-engine visual checks remain
user-run; source contract checks alone cannot verify the resulting appearance.

`game.saveActorPreview` exports the current camera, pose and lighting to a unique PNG under
`Data/SkyUltra/Actors`, resolved against the running Skyrim executable. When
Skyrim runs through MO2, its virtual filesystem redirects new files to Overwrite
(or the executable's configured output mod). Vortex and unmanaged launches write
to the physical Data directory. Merely having MO2 installed does not activate its
virtual filesystem. The encoder runs inside the game process and reports the
final file path when Windows can resolve it. It returns `{saved:true,path}`
or `{saved:false,message}`; the facade rejects failed saves. No caller-supplied
filesystem path is accepted. The image has a transparent background and straight
alpha, with the same portrait/full-model framing and aspect ratio as the stage.
Exports render a fresh target using the saved full-view or portrait resolution;
they do not upscale the screen preview. `readSkyUltraSettings()` exposes
`actorExport: {fullView, portrait}`. The built-in menu can persist each choice
with `updateSkyUltraSettings()`: 720, 1080, 1440, 2048, 2160 or 4096 pixels on
the longest edge. Old settings files default both to 4096. An explicit `longEdge`
in `renderer.save()` overrides the preference for that capture and accepts
integers from 720 to 4096. Omit it to use the saved preference. Live preview
resolution and cadence are unchanged.
Readback is requested only on Save and polled without waiting on the GPU;
PNG encoding and disk writes run off the render thread. This method requires
its own manifest permission and view allowlist entry.

Portraits resolve the head from race body-part data, then common head-node names.
Head skin bounds (including weighted horns/antlers and head descendants) set
the portrait center and size. An unknown skeleton falls back to the whole actor,
and the near plane stays in front of the camera instead of slicing the head.

The renderer reads actual loaded actor geometry, diffuse textures, and skin
transforms into an owned scene snapshot, and draws a private D3D11 target for
both CEF and Ultralight. It does not stream encoded images through JavaScript.
It uses its own studio shader; unsupported geometry is skipped and counted in
`skippedGeometryCount`. It does not reproduce Skyrim's complete material,
lighting, hair, or postprocessing pipeline. A database NPC with no loaded actor
instance returns an unavailable status; this API does not spawn actors or alter
the world. Check `available`, `ready`, and `message` instead of showing a fixture
as a rendered actor.

For a standalone browser page, install a mock before app code starts:

```ts
import { installSkyUltraMock } from "@skyultra/sdk";

const mock = installSkyUltraMock(window);
mock.setRequestHandler(async (name, params, signal) => {
  if (name === "sample.getStatus") return { ready: !signal?.aborted };
  return { preview: true, name, params };
});
mock.emit("inventoryUpdated", { count: 3 });
```

The mock supports events, Promise responses, and `AbortSignal` cancellation. It
is a development aid and does not enforce native manifests or permissions. The
SkyUltra preview server already supplies its own mock bridge; pages opened
there should use that bridge instead of installing a second mock.

The host must enable JavaScript for the view. Page requests also require a
native request callback, a per-view allowlist, and matching manifest
`permissions.pageRequests` entries. The page-side bridge reports cancellation
to native code while a request is still pending. Request names and JSON payloads
retain the byte and size limits documented in the native API.

`JsonValue` describes values that can cross the bridge. Keep page request names
in the mod's manifest and the native per-view allowlist; TypeScript types do not
grant permissions.

## Page readiness and popup input

```ts
import { getSkyWebView, connectSkyUltraInputScopes,
         markSkyUltraPageReady } from "@skyultra/sdk";
const bridge = getSkyWebView();
const dispose = connectSkyUltraInputScopes(bridge);
// After your page shell commits (not after expensive inventory/NPC collection):
requestAnimationFrame(() => requestAnimationFrame(() => {
  void markSkyUltraPageReady(bridge).catch(console.error);
}));
// On application disposal:
dispose();
```

Allow `ui.pageReady` and `ui.setInputScope` in the manifest and per-view native
allowlist. The former opts an opaque page into the native logo handoff; the
latter lets kit popups own Escape before it closes the panel. Built-in
`game.getPlayerSnapshot`, nearby/search, actor-preview, and `ui.*` services do
not need a custom Papyrus or native callback, but retain all permission checks.
Custom methods still need a request handler.

`ui.configureWidgets` creates a mod-owned, transparent, click-through child
view using the parent's selected backend and a sibling packaged `hud.html`.
The [Field Atlas README](../../samples/ui-kit-test-mod/README.md) documents
parameters, compact snapshot scope, build/copy layout, and integration limits.

## Native performance snapshots

```ts
import { getSkyWebView, readSkyUltraPerformance,
         compareSkyUltraPerformance } from "@skyultra/sdk";
const bridge = getSkyWebView();
const before = await readSkyUltraPerformance(bridge);
// Run the chosen workload. Poll at a modest cadence (the sample uses 1 Hz).
const after = await readSkyUltraPerformance(bridge);
const result = compareSkyUltraPerformance(before, after);
```

Declare `ui.getPerformanceSnapshot` in the manifest and per-view allowlist.
`SkyUltraPerformanceSnapshot` contains device generation, monotonic host capture
clock, cumulative samples/totals, recent-window statistics and sample ages.
`compareSkyUltraPerformance` rejects device resets/unavailable measurements and
uses cumulative completed-sample deltas for means. It returns null means for
zero samples rather than Infinity or zero-cost claims.

The compositor GPU scope excludes browser/actor rendering. The host-device GPU
scope includes host submissions but excludes CEF's separate producer work.
Callback CPU is elapsed callback time; Present cadence is not GPU duration.
All scopes aggregate visible host views and require matched view layouts.
Detailed monitoring has a five-second request-renewed lease. Native reads copy
completed counters; they never flush or synchronously wait for GPU queries.
See the Field Atlas guide for the report format and comparison rules.

### Inventory, item thumbnails and player equipment

Grant all entries in `skyUltraInventoryMethods` in the mod manifest and each
view's page-request allowlist: `game.getInventory`, `game.getItemThumbnail`,
`game.equipItem` and `game.unequipItem`. These are host-owned methods.

```ts
import { createSkyUltraGame, skyUltraInventoryMethods } from '@skyultra/sdk';
const game = createSkyUltraGame();
const page = await game.inventory({category:'armor', query:'', offset:0, limit:64});
const next = await game.inventory({offset:page.offset + page.results.length});
const icon = await game.thumbnail(page.results[0].id);
// icon.available ? icon.dataUrl : icon.message; transparent PNG data URL

// Only on the user's explicit equipment action:
const result = await game.equipItem('0x00012EB7', 'mainHand');
if (result.accepted) {
  // Queued by Skyrim; refresh player() and confirm equipment before reporting success.
}
await game.unequipItem('0x00012EB7', 'mainHand');
```

`inventory()` queries the full carried set, independently of the overview
snapshot's 128-type cap. Options include query, category, equipment
(all/equipped/carried), sort (name/value/weight/count), offset and limit (1–64).
Results contain results, totalTypes, totalMatches, offset and categories.
Item rows include optional equipSlots supplied from actual armor-slot/weapon
metadata. For old data without this field, the UI does not guess compatibility.

Equipment destinations are head, body, hands, feet, neck, ring, mainHand,
offHand, ammo, cape, lantern or auto. The host independently validates ownership and fit,
rejects consumables, and calls Skyrim's equipment manager on a game task without
forcing restricted unequips. Two-handed weapons use their native equip slot.
This changes the real player; camera, lighting and item preview APIs still only
change private previews. Base-form rows aggregate copies; choosing a particular
custom enchanted/tempered instance is not yet supported.
For loaded NPCs, `Npc.equipment` and `Npc.armorRating` report the actor's
11-slot worn gear (`head`, `body`, `hands`, `feet`, `neck`, `ring`, `cape`,
`lantern`, `ammo`, `mainHand`, `offHand`). Use `game.equipActorItem(referenceId, formId, slot)`
and `game.unequipActorItem(referenceId, formId, slot)` (which pass optional
`referenceId` to `game.equipItem` / `game.unequipItem`) to dress up or unequip
a loaded actor and receive the refreshed `equipment` map in the response.

`thumbnail()` captures a 128×128 transparent PNG offscreen without replacing
the view's actor/item stage and persists the encoded PNG in `Data/SkyUltra/Cache/Items/`
(redirected by MO2/USVFS to `Overwrite/SkyUltra/Cache/Items/item-<plugin>-<localFormId>.png`)
so subsequent game sessions load item icons directly from disk without waiting on NIF demand or GPU readback.
`actorThumbnail()` (`game.getActorThumbnail`) similarly captures a close-up 3D bust portrait
(head, shoulders, and upper body) for a loaded actor by `referenceId` or `baseFormId` without disturbing
the active preview stage, and caches the resulting PNG in `Data/SkyUltra/Cache/Portraits/`
(`Overwrite/SkyUltra/Cache/Portraits/portrait-bust-player-<name>.png` and `portrait-bust-<plugin>-<localFormId>.png`).
Passing `usePreview: true` to `actorThumbnail()` forces bust portrait framing (`portrait: true`) while preserving
the studio stage's rotation and lighting, overwrites the cached PNG in Overwrite, and broadcasts
`skyultra.portraitUpdated` to all active views. Both methods return `available`, `dataUrl` or
`message`; missing models have an explicit unavailable result. Keep requests
bounded and cache results. The optional Inventory UI uses visibility observation,
one request at a time, cancellation of unused queued tiles, and a 4096-entry in-memory LRU cache.
The native capture queue remains shared and bounded; busy replies can be retried.
The thumbnail payload fits the bridge's existing 64 KiB response limit. Studio
PNG export keeps its independently configured 720–4096 longest-edge resolution.

These additions are source-validated only. Build the updated native plugin,
frontend and permission manifests, then verify both engines in Skyrim.

### Isolated outfit opacity and accessory slots

For an outfit stage, pass `ignoreWorldFade:true` to `setActor()`. Skyrim can
write camera/effect fades into both the shader opacity and materialAlpha. This
option makes private captured scalar opacity 1, retaining texture/vertex alpha,
NIF blending and alpha testing. It also overrides authored scalar opacity, so it
is opt-in; the optional Inventory outfit view enables it for its preview and
Save calls. Default actor previews keep their previous opacity behavior. The
host never writes alpha or materials back to the real actor. Changing this
option invalidates the captured material snapshot.

The equipment contract now includes optional cape and lantern snapshot entries.
Cape is biped mask bit 16 (slot 46); lantern is bit 19 (slot 49).
These fields remain optional for compatibility with older snapshot fixtures.
Native ownership/equipment checks apply to these destinations as to other armor.

The optional model stage ignores scroll events from unrelated lists. Ancestor
scroll/resize updates layout without hiding a fully visible stage; real clipping,
modal input scopes and view/document visibility still hide it.

### Item combat metadata, storage containers, mannequins & spellbook actions

`Item` objects returned by `game.inventory()`, `game.barterSnapshot()`, and `game.containerSnapshot()` include expanded combat and equipment stats:
- **Weapon & Ammo stats**: `damage?: number` (`DMG`), `baseDamage?: number`, `weaponType?: string` (e.g. `'One-Handed Sword'`, `'Greatsword'`, `'Bow'`, `'Dagger'`, `'War Axe'`, `'Mace'`, `'Battleaxe'`, `'Warhammer'`, `'Staff'`, `'Crossbow'`, `'Arrow'`, `'Bolt'`), `speed?: number`, `reach?: number`, `critDamage?: number`.
- **Armor & Shield stats**: `armorRating?: number` (`AR`), `baseArmorRating?: number`, `armorWeightClass?: 'heavy' | 'light' | 'clothing' | 'shield'`, `armorTypeLabel?: string` (`'Heavy Armor'`, `'Light Armor'`, `'Clothing'`, `'Shield'`).

`createSkyUltraGame()` also exposes native storage container, mannequin, and spellbook management methods:

```ts
import { createSkyUltraGame } from '@skyultra/sdk';
const game = createSkyUltraGame();

// 1. Rename a spell, shout, or power (up to 96 characters)
await game.renameSpell('0x00012FCC', 'Arcane Flames');

// 2. Delete a spell, shout, power, or dispel/remove an active effect
await game.deleteSpell('0x00012FCC', 'spell'); // 'spell' | 'power' | 'lesserPower' | 'shout' | 'effect'

// 3. Read the active Storage Container, Corpse Loot, NPC Pickpocket, or Mannequin snapshot
const container = await game.containerSnapshot();
// container.container.containerMode: 'container' | 'loot' | 'pickpocket' | 'mannequin'
// container.container.isCorpse is true when looting a dead NPC or monster
// container.container.isPickpocket is true when pickpocketing a live NPC (worn NPC items have item.equipped === true)
// container.container.isMannequin is true for armor mannequins (with container.mannequin 11-slot actor state)
// Gold (0x0000000F, id '15') is included in both container.container.items and container.player.items

// 4. Transfer items (or Gold '0x0000000F') between the player and a container, corpse, NPC, or mannequin
await game.containerTransfer({ action: 'take', formId: '0x00012EB7', count: 10 });      // or 'steal' in pickpocket mode
await game.containerTransfer({ action: 'store', formId: '0x00012EB7', count: 1, equip: true, slot: 'mainHand' }); // or 'plant'
await game.containerTransfer({ action: 'takeAll' });                                    // or 'stealAll'
await game.containerTransfer({ action: 'storeAll', includeEquipped: false });           // or 'plantAll'
```

`SkyUltraSettings` (`readSkyUltraSettings` / `updateSkyUltraSettings`) includes native menu replacement and container configuration fields for Storage Containers (`skyultra_container.swf`), Corpse & Monster Looting (`skyultra_loot.swf`), NPC Pickpocketing (`skyultra_pickpocket.swf`), and Mannequins (`skyultra_mannequin.swf`):
- `replaceSkyrimContainer: boolean` (`containerDisplayMode: 'half' | 'full' | 'windowed'`, `containerPauseWorld: boolean`)
- `replaceSkyrimLoot: boolean` (`lootDisplayMode: 'half' | 'full' | 'windowed'`, `lootPauseWorld: boolean`)
- `replaceSkyrimPickpocket: boolean` (`pickpocketDisplayMode: 'half' | 'full' | 'windowed'`, `pickpocketPauseWorld: boolean`)
- `replaceSkyrimMannequin: boolean` (`mannequinDisplayMode: 'half' | 'full' | 'windowed'`, `mannequinPauseWorld: boolean`)
