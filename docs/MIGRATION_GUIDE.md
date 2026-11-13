# Port an existing web UI to SkyUltra

**A migration should preserve the mod's behavior, its design and the player's data.** Use a small working slice to establish the new host boundary, then move the remaining screens through the same contract.

| Starting point | Main guide | Working reference |
| --- | --- | --- |
| PrismaUI frontend or native consumer | [PrismaUI migration](PRISMAUI_MIGRATION.md) | [Matched native request/reply example](PRISMAUI_MIGRATION.md#complete-a-native-round-trip) |
| Magelight UI manifest, page or native consumer | [Magelight UI migration](MAGELIGHT_UI_MIGRATION.md) | [Manifest conversion](MAGELIGHT_UI_MIGRATION.md#translate-the-manifest) and [send/on routing](MAGELIGHT_UI_MIGRATION.md#route-real-gameplay-requests) |
| A plain page needing built-in game data | [SDK walkthrough](SDK_GUIDE.md) | [Plain bridge port sample](../samples/legacy-webview-port-sample/README.md) |
| Existing callback names with custom game logic | [Explicit adapter](#prefer-explicit-mappings-for-custom-gameplay) | [Preserved Prisma-style globals](PRISMAUI_MIGRATION.md#map-existing-callbacks-to-gameplay) |
| A Scaleform menu or an unrelated raw CEF bridge | [Scope and coexistence](#scaleform-and-coexistence) | Recreate the owner/transport; the HTML and game logic remain separate concerns |

## A practical port order

1. **Record behavior.** Inventory screens, native methods/events, hotkeys, input ownership, storage and the browser features your UI actually uses.
2. **Package one page.** Use `Data/SkyUltra/<lowercase-id>/manifest.json` and its `Files/` tree. A schema-v1 manifest has one `entry`, not a foreign host's `views` map.
3. **Create its owner.** Choose built-in integration, [C/C++](../api/README.md) or [Papyrus](../sdk/papyrus/README.md). Configure visibility, bounds, layer and input intentionally.
4. **Prove one complete interaction.** Receive a native reply and a pushed event, or display built-in game data. A compatibility echo alone does not prove custom gameplay.
5. **Adapt incrementally.** Keep reusable components and legacy names through the supported shims/adapter while replacing host calls.
6. **Migrate settings.** Import your own versioned values and preserve player data; browser cache/session identities do not automatically transfer.
7. **Exercise lifecycle.** First open, hide/show, reload, repeated close, save/load, native menus and each advertised engine/input mode.
8. **Distribute the complete port.** Ship the manifest, assets, owner and notices. Update this edition's dependency without removing hosts needed by unrelated mods.

## Install the shims

SkyUltra includes working, opt-in **frontend** helpers for PrismaUI and Magelight UI. The built-in menu installs them itself. For an independent page, install them before the adapted application executes:

```ts
import { getSkyWebView, installSkyUltraCompetitorShims } from '@skyultra/sdk';
const bridge = getSkyWebView();
const shims = installSkyUltraCompetitorShims(window, bridge);
// Load your own legacy entry AFTER installing its expected helpers.
await import('./legacy-app.js');
// On application teardown: shims.uninstall();
```

Static imports execute before the importing module's body. Putting `import './legacy-app.js'` visually below the install call does not defer it; the dynamic import above does. In a non-bundled page, make the equivalent script-loading order explicit. [SDK setup](SDK_GUIDE.md#choose-a-starting-point) explains local package use and [bridge inclusion](SDK_GUIDE.md#basic-page-bridge).

| API | Supported frontend surface | Payload/cleanup |
| --- | --- | --- |
| Prisma-shaped | `RegisterJSListener`, `InteropCall`, local `Invoke`, `SetOrder` / `GetOrder` | Listener values are strings; registration returns an unsubscribe function |
| Magelight-shaped | `send`, `on`, `close`, `playSound`, `vr.atGaze`, `vr.setPlacement`, `gamepad` | Listener values remain JSON values; `on` returns an unsubscribe function |
| Installer | `installSkyUltraCompetitorShims(window, bridge)` | `uninstall` releases registrations and restores previous globals it still owns |

The helpers do not emulate every upstream method. Prisma order is a stored compatibility value, not native reordering. `Invoke` runs in the page. An arbitrary old global such as `window.saveSettings` still needs an explicit mapping. Existing native DLLs/handles/function tables do not become SkyUltra consumers through a JavaScript shim.

## Understand the three message routes

| Your call | Where it goes | What success establishes |
| --- | --- | --- |
| Prisma shim `InteropCall('port.preview', data)` or Magelight shim `send('port.preview', data)` | `ui.interopCall` → same-view `skyultra.legacy.interop` event | The matching framework's listener echo works |
| Magelight shim `send('game.getPlayerSnapshot', {})` | That exact SkyUltra built-in endpoint | The granted built-in request ran |
| `bridge.request('port.saveSettings', data)` or an adapter mapping to it | Your ported native/Papyrus handler | The custom action replied; its implementation determines what changed |

Custom shim types do not call an old DLL's listeners. A `{ accepted: true }` echo response is useful for compatibility messaging, but does not mean an SKSE gameplay action happened. Framework envelopes stay isolated even when the method names match; direct named events can intentionally be observed by either helper.

## Prefer explicit mappings for custom gameplay

```ts
import { createSkyUltraLegacyBridgeAdapter, getSkyWebView } from '@skyultra/sdk';
const legacy = createSkyUltraLegacyBridgeAdapter(getSkyWebView(), {
  methodMap: { saveSettings: 'port.saveSettings' },
  eventMap: { oldStatus: 'port.state' },
});
const stop = legacy.listen('oldStatus', payload => console.log(payload));
await legacy.ready();
const result = await legacy.call('saveSettings', { enabled: true });
console.log(result);
// On teardown: stop();
```

Implement `port.saveSettings` and allow it in both permission lists. Push `port.state` from the owner for independent updates. The adapter maps names; it does not fabricate native logic. `ready()` requests `ui.pageReady`; `close()` requests `ui.close`. A custom application handshake requires its own declared method because SkyUltra consumes the framework readiness request itself.

## Permission worksheet

| Feature | Manifest `permissions.pageRequests` | Additional responsibility |
| --- | --- | --- |
| Release the startup gate | `ui.pageReady` | Attach app listeners before signaling |
| Close through the host | `ui.close` | Keep owner/input state consistent |
| Shim echo | `ui.interopCall` | Install and clean up the intended helpers |
| Read one player snapshot | `game.getPlayerSnapshot` | Handle an unavailable character and failed requests |
| Shared player feed | `game.subscribe`, `game.unsubscribe` | Use canonical topics such as `vitals`; unsubscribe on teardown |
| Sound | `ui.playSound` | Use defined sound IDs/presets |
| Popup input scopes | `ui.setInputScope` | Pair every acquired scope with its release |
| Spatial placement / world projection | `ui.setSpatialPlacement` / `ui.projectWorldAnchors` | Verify placement and input on the actual target runtime |
| Your native action | Exact custom name, for example `port.saveSettings` | Implement validation, action, success/error completion and cancellation |

Native/Papyrus-owned views also need the matching per-view allowlist. It cannot grant a method absent from the manifest. Named event subscriptions are not request grants. Set storage/network permissions separately when required; a method allowlist does not enable arbitrary file or network access.

## Readiness, cleanup and cost

A native view being ready, the page installing its listeners and the application receiving initial state are different milestones. Subscribe first, signal `ui.pageReady`, then pull initial state through a supported built-in or custom method. Do not assume another host's event buffering/replay behavior exists here.

Hiding a view is not destroying its page. Pause expensive visible work using host/document visibility, and release listeners/controllers when their owning component or page is actually torn down. Cancel stale reads, stop timers/RAF work, pair input scopes and hide native previews before page navigation. Reinstalling a new set of listeners on every open without removing the previous set produces duplicate actions.

Use shared telemetry instead of per-widget polling, keep messages within the 64 KiB serialized envelope limit and send changed data. Compare cold startup and visible-opening latency separately. Test the same page, viewport and scene on each supported engine before making performance claims; the host GPU scope does not measure CEF subprocess rendering.

The current native custom-request callback slot is **global**, and later registrations replace it. Native consumers need coordinated dispatch. The [native API guide](../api/README.md#requests-replies-and-cancellation) documents string request IDs, borrowed callback arguments, thread ownership and cancellation.

## Validation supplied with this checkout

With the checkout's author-tool dependencies installed, run from the SkyUltra project root:

```powershell
node tools/Check-DocumentationSource.cjs
node tools/Check-LegacyShims.cjs
node tools/Check-MigrationExamples.cjs
```

These checks validate documentation links/manifests/snippets, production shim contracts and the migration sample/wire-test page in memory. They do not build a mod, install it or launch Skyrim. Developer Studio's inline **Verify Active Shims** adds a live host echo check; complete gameplay, engine rendering, input and coexistence still need in-game testing.

## Scaleform and coexistence

A web page does not execute an existing `.swf`'s ActionScript. Recreate its interface/owner contract in HTML and move gameplay calls through the bridge. SkyUltra's own small replacement-menu SWFs are launch carriers; they do not convert arbitrary Scaleform mods.

For a passive HUD, configure click-through input on the HUD layer. For a menu, use an interactive owner and deliberate open/close rules. Keep test hotkeys distinct across hosts and check map/message-box cursor behavior. SkyUI, ENB and Community Shaders are not dependencies of the migrated page; list optional interoperability separately from requirements.

Keep the original mod edition available while the replacement is being verified, and preserve imported settings. See [PrismaUI troubleshooting](PRISMAUI_MIGRATION.md#diagnose-a-failed-port), [Magelight troubleshooting](MAGELIGHT_UI_MIGRATION.md#troubleshoot-and-verify) and [credits/notices](THIRD_PARTY_LIBRARIES.md).
