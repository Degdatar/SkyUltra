# Using the SkyUltra SDK

## Choose a starting point

Use plain HTML/JavaScript for a small page, `@skyultra/sdk` for typed bridge/game helpers, `@skyultra/react` for hooks, and `@skyultra/ui` for reusable components. Components and styles are opt-in; a mod does not need to use SkyUltra's menu design. Players need the packaged mod, not Node or a frontend toolchain.

The npm names identify the packages in this repository; availability on a public registry is not assumed. From this checkout, use the local scaffolder or an existing sample:

```powershell
# Run from the SkyUltra project root (the folder containing this docs directory).
node sdk/create-skyultra-mod/bin/create-skyultra-mod.js MyUI --dir ../MyUI --id author.my_ui
cd ../MyUI
npm install
npm run dev
# When you intend to produce an installable web package:
npm run build
```

The scaffold uses the local Vite plugin when run from the checkout. For React, start with [react-mod-starter](../samples/react-mod-starter/README.md); its `file:` dependencies point at `sdk/typescript`, `sdk/react` and `sdk/ui`. Building those SDK packages is part of frontend setup; changing a README does not rebuild them automatically. Vite projects here need Node 20.19 or newer. See each package's scripts before building.

## Manifest and installed layout

```text
Data/SkyUltra/author.my_ui/
  manifest.json
  Files/
    index.html
    SkyUltraBridge.js
    assets/...
```

```json
{
  "schemaVersion": 1,
  "id": "author.my_ui",
  "name": "My UI",
  "version": "1.0.0",
  "entry": "Files/index.html",
  "permissions": {
    "storage": false,
    "assetAccess": true,
    "networkHosts": [],
    "pageRequests": ["ui.pageReady", "ui.close", "game.getPlayerSnapshot", "game.subscribe", "game.unsubscribe"]
  }
}
```

Keep the folder identity aligned with the lowercase `id`. Paths stay relative to the mod root. `entry` is one page, not an `entries.hud/panel` object. `schemaVersion` is not `manifestVersion`. Grant only the methods your page uses. For native/Papyrus-created views, the per-view allowlist must also grant those methods.

For a hand-written page, copy `resources/SkyUltraBridge.js` next to the HTML and load it before application scripts. The [Vite plugin](../sdk/vite-plugin/README.md) handles bridge inclusion and writes a root manifest plus the `Files/` subtree. Copy the **contents** of the generated distribution to your mod folder. Bundle styles, fonts and images locally; keep Vite's relative `base: './'` for installed file URLs.

## Basic page bridge

The exact global is `window.SkyWebView`. The API has `request`, `on`, `off`, runtime information and visibility information; it is not `window.skywebview` or `window.skyQuery`.

```js
const bridge = window.SkyWebView;
if (!bridge) throw new Error('Load SkyUltraBridge.js before this script');

const stop = bridge.on('sample.status', payload => {
  document.querySelector('#status').textContent = JSON.stringify(payload);
});

async function start() {
  await bridge.request('ui.pageReady', {});
  const player = await bridge.request('game.getPlayerSnapshot', {});
  console.log(player);
}
start().catch(console.error);
window.addEventListener('pagehide', stop, { once: true });
```

Use JSON-compatible payloads and handle rejected requests. Requests have a 64 KiB serialized-message limit including envelope overhead. For cancellable work, pass `{ signal: controller.signal }` as the third request argument; cancellation does not undo a gameplay action already completed.

## Typed requests and shared telemetry

```ts
import { getSkyWebView, markSkyUltraPageReady, subscribeSkyUltraGame } from '@skyultra/sdk';

const bridge = getSkyWebView();
const subscription = subscribeSkyUltraGame(bridge, event => {
  const hp = event.snapshot.health;
  document.querySelector('#hp')!.textContent = `${hp.current} / ${hp.max}`;
}, { scope: 'hud', topics: ['vitals'], intervalMs: 250 });
await markSkyUltraPageReady(bridge);
// On component/page cleanup:
// subscription.unsubscribe();
```

The SDK merges deltas into a shared snapshot and shares the native feed among consumers in a view. Prefer this over one polling loop per widget. Subscribe before signaling application readiness if native code will push initial data. `ui.pageReady` releases the startup gate; it does not register a custom C++ callback or manufacture permissions.

For your own request contract:

```ts
import { getSkyUltraClient } from '@skyultra/sdk';
type Events = { 'sample.status': { connected: boolean } };
type Requests = { 'sample.ping': { params: {}; response: { message: string } } };
const client = getSkyUltraClient<Events, Requests>();
const result = await client.request('sample.ping', {});
```

`sample.ping` needs a native or Papyrus handler as well as its permission. See the [native request example](../api/README.md) or [Papyrus request handling](../sdk/papyrus/README.md).

## React

```tsx
import { useEffect } from 'react';
import { markSkyUltraPageReady } from '@skyultra/sdk';
import { useSkyUltraGameSubscription } from '@skyultra/react';

export function Health() {
  const { player } = useSkyUltraGameSubscription({
    scope: 'hud', topics: ['vitals'], intervalMs: 250,
  });
  useEffect(() => { void markSkyUltraPageReady().catch(console.error); }, []);
  return <output>{player ? `${player.health.current} / ${player.health.max}` : 'Connecting…'}</output>;
}
```

Use `useSkyUltraClient`, `useSkyUltraEvent` and `useSkyUltraRequest` for typed custom messages. Hooks remove subscriptions on unmount. Import UI components from the smallest useful entry (`@skyultra/ui/core`, `/widgets`, `/game`, `/gameplay`, `/chrome`); import the corresponding CSS explicitly. The [UI README](../sdk/ui/README.md) lists exports.

## Input, engines and previews

- A passive HUD should be a HUD layer with `clickThrough: true`. Interactive panels use panel/modal/input configuration from their owner. Popup focus/Escape scopes use `connectSkyUltraInputScopes` and the `ui.setInputScope` permission.
- `browserEngine: 'cef'` or `'ultralight'` pins a manifest's backend. Omission follows the host default. The host can include both engines and settings can route its menu/mods; query `getSkyUltraRuntimeInfo()` for actual capabilities. Test both engines you intend to support.
- Use `installSkyUltraMock` only in a browser that has no bridge. For a Vite mock preview, configure `skyultra({ devBridge: false })`; installed pages still use the real bridge. Mock success is not game validation.
- [Actor/item renderer APIs](../sdk/typescript/README.md) provide native stages and exports on flat SE/AE; the native preview compositor currently reports unsupported in VR. Hide a stage when obscured, update its view-relative bounds after resize/scroll, and close it when unmounted. Animation capture is explicit and owns private sampled frames.
- `playSkyUltraSound` calls `ui.playSound`; declarative sound attributes need `connectSkyUltraDeclarativeSounds`. `projectSkyUltraWorldAnchors` calls `ui.projectWorldAnchors`; `configureSkyUltraSpatialPlacement` calls `ui.setSpatialPlacement`. Grant the matching methods.

## Legacy helpers

`installSkyUltraCompetitorShims(window, bridge)` installs the Prisma/Magelight frontend helpers. `createSkyUltraLegacyBridgeAdapter` maps old method/event names to real SkyUltra contracts. See the [migration index](MIGRATION_GUIDE.md) for setup, working examples, cleanup and the distinction between interop loopback and native gameplay requests.

## Package and diagnose

Install the distribution with its root `manifest.json` and `Files/` content. Use Developer Studio to inspect the manifest, selected engine, bridge errors and permission denials. Check `SkyUltra.log`, and `SkyUltra_CEF.log` for CEF failures. An unrecognized field, missing entry, denied method, missing bridge or unsupported browser feature needs a different fix; enabling a shader mod does not resolve those errors.

See [library acknowledgments](THIRD_PARTY_LIBRARIES.md). Node, npm, Vite, TypeScript and SDK source dependencies are author tools; they are not extra player requirements.
