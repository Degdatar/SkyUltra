# Moving a Magelight UI mod to SkyUltra

**Preserve the page and the gameplay. Recreate the manifest, host messages and view ownership around them.**

This guide covers Magelight manifest mods, `send`/`on` frontends, native consumers and spatial interfaces. [Magelight UI](https://github.com/Severause/MagelightUI) uses Ultralight; porting it is not inherently a conversion from raw CEF. SkyUltra can host pages through CEF or Ultralight, depending on configuration and the backends included in the installed host.

**Companions:** [Shared migration guide](MIGRATION_GUIDE.md) · [SDK](SDK_GUIDE.md) · [Native API](../api/README.md) · [Complete native round trip](PRISMAUI_MIGRATION.md#complete-a-native-round-trip)

## Contents

1. [Choose the first milestone](#choose-the-first-milestone)
2. [Translate the manifest](#translate-the-manifest)
3. [Keep the Magelight shim](#keep-the-magelight-shim)
4. [Route real gameplay requests](#route-real-gameplay-requests)
5. [Port the native or Papyrus owner](#port-the-native-or-papyrus-owner)
6. [Rebuild the lifecycle](#rebuild-the-lifecycle)
7. [Audio, spatial UI and images](#audio-spatial-ui-and-images)
8. [Move data and package the port](#move-data-and-package-the-port)
9. [Troubleshoot and verify](#troubleshoot-and-verify)

## Choose the first milestone

| Your mod | First milestone | Read next |
| --- | --- | --- |
| Manifest plus static page/HUD | Render one page with the correct bounds/input behavior | Manifest conversion below and [SDK layout](SDK_GUIDE.md#manifest-and-installed-layout) |
| Page already using `magelight.send` / `on` | Deliver one compatibility echo, then one real native action | Shim and request sections below |
| A native DLL owns views and game logic | Create the view, allowlist one custom method, return JSON | [Native API](../api/README.md) and [wire test](PRISMAUI_MIGRATION.md#complete-a-native-round-trip) |
| A Papyrus controller owns the interface | Create one view and handle a page request | [Papyrus SDK](../sdk/papyrus/README.md) |
| VR/spatial UI or native images | Establish the page lifecycle, then port placement/image ownership separately | Audio, spatial UI and images below |

Inventory every page, native listener, pushed event, opening rule and stored setting before editing. Keep the original package available during the port, with distinct test hotkeys and identities. Changing the web host does not require adopting SkyUltra's own menu appearance.

## Translate the manifest

Magelight's [manifest reference](https://github.com/Severause/MagelightUI/blob/main/docs/MANIFEST.md) describes its per-mod `views` map. SkyUltra uses a schema-v1 manifest with a **single entry page**, plus view configuration supplied by its owner/integration.

A small Magelight package might declare:

```json
{
  "modId": "MyStatus",
  "name": "My Status",
  "version": "1.0.0",
  "views": {
    "hud": {
      "path": "views/hud/index.html",
      "layer": "hud",
      "clickThrough": true,
      "startVisible": true
    }
  }
}
```

The corresponding SkyUltra page package:

```json
{
  "schemaVersion": 1,
  "id": "author.my_status",
  "name": "My Status",
  "version": "1.0.0",
  "entry": "Files/index.html",
  "permissions": {
    "storage": false,
    "assetAccess": true,
    "networkHosts": [],
    "pageRequests": ["ui.pageReady", "ui.interopCall"]
  }
}
```

```text
Data/SkyUltra/author.my_status/
  manifest.json
  Files/
    index.html
    SkyUltraBridge.js
    assets/...
```

This grants readiness and the shim echo only. Your view owner still needs to create a **HUD-layer, transparent, click-through** surface with the intended size, bounds and visibility. Copying `layer` or `startVisible` into SkyUltra's manifest does not configure it.

| Old declaration | SkyUltra decision |
| --- | --- |
| `modId` / folder identity | Use a matching lowercase SkyUltra `id` and folder |
| `views.<name>.path` | Put the page in the manifest's single `entry` |
| Multiple views/pages | Use one app router, or separate manifests for independently declared entry pages |
| Layer, anchor, bounds and click-through | Configure the view through the owner; raw native `SkyUltraViewConfigV1` exposes these fields |
| `minHost` | No same-name SkyUltra manifest field; declare requirements in your distribution and check the ABI/capabilities you consume |
| Session/storage declarations | Make persistence/import decisions explicitly; grant SkyUltra storage only when needed |
| Network declarations | Use explicit `permissions.networkHosts`; do not copy the other host's policy enum/string |
| Host-specific cursor, preload or development fields | Consult SkyUltra's actual APIs/settings instead of preserving unknown schema keys |

Keep `schemaVersion`, `entry` and the supported permission fields. The [SkyUltra manifest validator](../sdk/typescript/src/compatibility.ts) rejects unknown fields. A manifest conversion is not complete until the chosen owner opens the page with the intended input/layer behavior.

For a build pipeline, follow the [SDK walkthrough](SDK_GUIDE.md#choose-a-starting-point) and [Vite plugin](../sdk/vite-plugin/README.md). The plugin supplies relative packaging and the bridge. A plain page can copy [SkyUltraBridge.js](../resources/SkyUltraBridge.js) beside its HTML and load it before application scripts. SDK package availability on a public registry is not assumed by these instructions.

## Keep the Magelight shim

The Magelight-shaped frontend helpers are supported and opt in per page:

```ts
import { getSkyWebView, installSkyUltraCompetitorShims } from '@skyultra/sdk';

const bridge = getSkyWebView();
const shims = installSkyUltraCompetitorShims(window, bridge);
const stop = shims.magelight.on('port.preview', payload => {
  console.log('Compatibility preview:', payload);
});
await shims.magelight.send('port.preview', { enabled: true });
// On page/app teardown:
// stop(); shims.uninstall();
```

This example's custom type travels through `ui.interopCall`, then a same-view `skyultra.legacy.interop` event. Both the manifest and applicable view allowlist must grant `ui.interopCall`. Listener payloads remain JSON values; unlike the Prisma-shaped callback, this callback is not converted into a JSON string. Direct native events with the requested name are also delivered. Prisma shim envelopes do not leak into the Magelight shim's matching listener.

| Helper | SkyUltra behavior | Required request permission |
| --- | --- | --- |
| `send('custom.type', payload)` | Same-view compatibility echo | `ui.interopCall` |
| `send('game.…', payload)` / `send('ui.…', payload)` | Calls that exact SkyUltra endpoint | The exact method invoked |
| `on(type, callback)` | Direct event + matching Magelight compatibility envelope | No new request permission for subscribing |
| `close()` | Requests host close; returns no payload | `ui.close` |
| `playSound('ok')` | Uses SkyUltra's game-audio helper | `ui.playSound` |
| `vr.atGaze(distance)` / `vr.setPlacement(options)` | Uses SkyUltra's spatial placement helper | `ui.setSpatialPlacement` |
| `gamepad` | Page-side controller helper | Depends on any additional host APIs your page calls |

The shim is a defined subset. For example, use the function returned by `on` to unsubscribe; do not assume an `off` alias exists. Use `playSound`, not an assumed `sound` alias. A shim version string does not imply a particular Magelight host version. Native ABI compatibility, session APIs, arbitrary native texture registration and automatic event replay are not supplied by this JavaScript object.

In Developer Studio, **Verify Active Shims** checks API presence and the actual host echo and displays inline results. It does not test another DLL's gameplay code.

## Route real gameplay requests

A custom shim send and a custom gameplay request are distinct routes:

```ts
import { getSkyWebView, installSkyUltraCompetitorShims } from '@skyultra/sdk';
const bridge = getSkyWebView();
const shims = installSkyUltraCompetitorShims(window, bridge);

await shims.magelight.send('port.preview', { enabled: true }); // same-view echo
await bridge.request('port.saveSettings', { enabled: true }); // your ported owner
```

The second line needs a manifest grant, a view grant and a `port.saveSettings` implementation. Granting `ui.interopCall` does not grant that custom method. A matching event name alone does not register a native handler.

To preserve old names while changing transport:

```ts
import { createSkyUltraLegacyBridgeAdapter, getSkyWebView } from '@skyultra/sdk';
const legacy = createSkyUltraLegacyBridgeAdapter(getSkyWebView(), {
  methodMap: { saveSettings: 'port.saveSettings' },
  eventMap: { status: 'port.state' },
});
const stop = legacy.listen('status', state => console.log('Native state:', state));
await legacy.ready();
const result = await legacy.call('saveSettings', { enabled: true });
console.log('Native reply:', result);
// On teardown: stop();
```

Validate the payload in your native/Papyrus owner, apply the action and return a result or error. Push `port.state` for independent updates. Asynchronous requests need rejection handling and, for reads that become stale, an `AbortController`. Cancellation cannot roll back an action the game already completed.

The [complete wire test](PRISMAUI_MIGRATION.md#complete-a-native-round-trip) provides a matched manifest, page, native owner link and exact expected output. Use it to establish the native path before adding the rest of your mod's handlers.

## Port the native or Papyrus owner

Magelight's [native reference](https://github.com/Severause/MagelightUI/blob/main/docs/CPP.md) is useful for inventorying the APIs your existing consumer uses. Target SkyUltra's own header for the replacement:

| Integration concern | SkyUltra destination |
| --- | --- |
| Host discovery and versioned table | `SkyUltra::Client::Acquire()` for V4, or the matching raw C ABI export |
| Mod identity and page entry | Register an installed manifest with `RegisterMod`; create its view |
| View visibility and geometry | `SetVisible`, `SetBounds` and native creation configuration |
| Reload / release | `ReloadView` / `DestroyView` |
| Native-to-page data | `SendEventJson` and page `SkyWebView.on` |
| Page-to-native actions | `SkyWebView.request`, two permission lists and the custom request callback |
| Independent UI-mode/focus ownership | Redesign using SkyUltra's visibility/layer/input model; the exported table has no independent focus setter |
| Script evaluation | Replace native script injection with a named JSON request/event contract |
| Papyrus calls | Recreate them with `SkyUltraNative.psc`; old native handles are not transferable |
| Cross-host native image/texture handles | Use SkyUltra's own renderer/asset lifecycle; do not reuse opaque handles from another host |

[NativeAPI.h](../api/include/SkyUltra/NativeAPI.h) is the ABI; [SkyUltraAPI.hpp](../api/include/SkyUltra/SkyUltraAPI.hpp) wraps that same table. Request IDs are strings, callback spans are borrowed, and game object access belongs on the game thread. Copy callback inputs that must outlive the callback and handle cancellation.

**Callback ownership matters:** the current SkyUltra custom-request callback slot is global. A later registration replaces it. Coordinate a dispatcher for multiple native consumers rather than assuming one callback per view or mod. The [native guide](../api/README.md#requests-replies-and-cancellation) documents the boundary; [Papyrus](../sdk/papyrus/README.md) has its own request-event route.

## Rebuild the lifecycle

Subscribe before announcing readiness. `ui.pageReady` releases SkyUltra's loading gate; it does not tell your custom native callback that a React application is ready. Define an allowlisted `port.getInitialState` request when your owner needs a separate handshake. Pull the current state after mounting rather than relying on a one-time push before subscriptions exist.

Use `bridge.getVisibility?.()` and `skyultra.visibility` alongside document visibility for expensive visible work. Do not make a hidden page's `requestAnimationFrame`, animation completion or delayed React effect responsible for releasing native input. Reload creates a new page lifetime: attach listeners again, request state again and release the previous lifetime's timers, subscriptions and preview controllers.

For shared player data, use `subscribeSkyUltraGame` with only the topics/scope your panel needs; the [SDK guide](SDK_GUIDE.md#typed-requests-and-shared-telemetry) explains merged snapshots and cleanup. A simple standalone raw-bridge implementation is in the [port sample](../samples/legacy-webview-port-sample/README.md).

Test focus, cursor and Escape while the map, message boxes and other native menus open and close. Distinct hotkeys help identify which host owns an interaction. Source-only bridge checks do not establish coexistence with a real load order.

## Audio, spatial UI and images

**Audio.** Use `playSkyUltraSound('ok', bridge)` or the shim's `playSound('ok')`. For declarative sound, install `connectSkyUltraDeclarativeSounds` and use `data-sky-sound`; attributes from a different host are not automatically interpreted. Grant `ui.playSound` and use the [defined sound presets](../sdk/typescript/src/enhancements.ts).

**Spatial UI.** The shim forwards `vr.atGaze(distance)` and `vr.setPlacement(options)` to `ui.setSpatialPlacement`. SkyUltra options use `distance`, `scale`, `bone` and `targetRefId`; world-anchor projection is a separate `ui.projectWorldAnchors` request. Review coordinates, ownership and input on your target runtime instead of transferring host-specific placement objects unchanged.

**Images and studios.** Actor/item rendering uses SkyUltra's renderer SDK and an owning view. Update bounds after relevant layout changes, hide a stage before navigation/obscuring UI, close it on teardown and cancel stale updates. Native actor/item preview compositing currently reports unsupported in VR; do not infer its availability from spatial-interface support.

**Engine testing.** Omit `browserEngine` to follow host configuration, or explicitly pin `cef` / `ultralight`. Log `getSkyUltraRuntimeInfo()` to confirm the actual backend. Check your page's fonts, CSS, input and browser features on every engine you advertise. Use the same scene and visible layout for performance comparisons; a host compositor GPU number does not include CEF subprocess rendering.

## Move data and package the port

An old browser session/cache is not a portable settings format. Export the values you own, validate them and import them through your new settings/native save layer. Make that import versioned and safe to repeat. Grant storage if you need persistence, preserve a backup and retain unrelated player data.

Ship the new root manifest and complete `Files/` subtree, plus a ported native/Papyrus owner when your mod needs one. Do not ship source dependencies or assume players have Node. SkyUI, ENB and Community Shaders are not required to run this migration. Change the dependency for this edition without telling players to uninstall Magelight from unrelated mods.

Keep old/new package identities and hotkeys distinct while testing. Review the [library and distribution notices](THIRD_PARTY_LIBRARIES.md) before bundling host/runtime binaries. Retain the notices for assets and libraries your own mod ships.

## Troubleshoot and verify

| Symptom | Likely boundary | Next step |
| --- | --- | --- |
| Manifest rejected | Old `views`, `minHost` or other foreign schema fields | Convert to supported SkyUltra keys; configure views through their owner |
| `magelight` undefined | No shim or application imported too early | Install shims before adapted legacy code |
| Send succeeds but gameplay does not change | Compatibility echo | Call a custom request with a ported native/Papyrus implementation |
| `sound`, `off` or another method missing | Assuming the entire old API is emulated | Use `playSound`, returned cleanup functions and the documented subset |
| No event after first open/reload | Subscription/readiness race | Subscribe first and request a fresh initial state |
| Custom requests fail only with another native mod | Global callback replaced | Coordinate a shared dispatcher; inspect request names in bridge logs |
| UI responds while hidden or a preview floats across pages | Lifecycle mismatch | Gate expensive work on visibility and release outgoing controllers immediately |
| VR placement works but actor preview does not | Different feature support | Keep spatial testing separate from the currently unsupported VR preview compositor |
| Settings vanish | New storage/session identity | Validate the explicit import and persistence path |

Use Developer Studio's view records, bridge errors, diagnostics and inline shim verification. Then check `SkyUltra.log` and, for CEF, `SkyUltra_CEF.log`.

```powershell
# From the SkyUltra project root; no build or game launch.
node tools/Check-DocumentationSource.cjs
node tools/Check-LegacyShims.cjs
node tools/Check-MigrationExamples.cjs
```

Before release, verify a real custom action, repeated open/close, reload, save/load, imported settings, text/controller input, native menu transitions and each advertised engine/VR mode. A passing source check is useful evidence; the complete mod still needs in-game validation.

Thanks to Severause and the Magelight UI contributors for their framework, examples and public APIs, and to the [libraries behind SkyUltra](THIRD_PARTY_LIBRARIES.md). SkyUltra-specific behavior in this guide follows the [shim source](../sdk/typescript/src/enhancements.ts), [adapter](../sdk/typescript/src/compatibility.ts) and [public C ABI](../api/include/SkyUltra/NativeAPI.h).
