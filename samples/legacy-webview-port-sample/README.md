# Plain bridge migration sample

[PrismaUI walkthrough](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI walkthrough](../../docs/MAGELIGHT_UI_MIGRATION.md) · [SDK](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md)

This small page demonstrates the destination of a basic web-UI port: real player vitals through `window.SkyWebView`, with no custom gameplay DLL or frontend SDK bundle needed. It uses SkyUltra's built-in endpoints. It does not claim to replace another host's native plugin.

## Install and open

```text
Data/SkyUltra/samples.legacy_webview_port/
  manifest.json             # copy this sample's manifest
  index.html                # this manifest's entry is index.html
  SkyUltraBridge.js         # copy from resources/SkyUltraBridge.js
```

This is a hand-written page, so its root `entry` is intentionally `index.html`; the Vite plugin's packaged `Files/` layout is not required here. The view owner/SkyUltra integration must create and open the installed package. Use the [Papyrus view walkthrough](../../sdk/papyrus/README.md#open-and-control-a-view) with this sample's ID, or the native API with the sample manifest's method allowlist.

The page should show your loaded character's health, magicka and stamina. With no loaded character it displays an unavailable state. In an ordinary browser without the bridge, it identifies preview mode and does not invent live game data.

## What the example exercises

- Listener registration before readiness and the initial subscription request.
- `game.subscribe` with `scope: 'hud'`, canonical `vitals` topic and a 250 ms requested interval.
- The modern `skyultra.game.update` delta envelope, including the initial `subscription.update`.
- Merging changed fields and rejecting an older subscription/snapshot response after a newer event.
- A manual snapshot refresh, host close/Escape and visible request errors.
- Listener removal, request cancellation and `game.unsubscribe` on page teardown.

Hiding a view does not trigger `pagehide`; the host controls feed delivery while hidden. Page teardown occurs on navigation/reload/destruction. Larger applications should use [subscribeSkyUltraGame](../../docs/SDK_GUIDE.md#typed-requests-and-shared-telemetry) to share and merge data among their consumers instead of duplicating this tiny page's merge logic.

## Bring your legacy callbacks

This sample uses the plain bridge directly. For a larger app, keep old names through [createSkyUltraLegacyBridgeAdapter](../../docs/PRISMAUI_MIGRATION.md#map-existing-callbacks-to-gameplay), or [install the supported shims](../../docs/MIGRATION_GUIDE.md#install-the-shims) before adapted legacy scripts execute. Grant `ui.interopCall` if adding shim echoes; custom gameplay needs its own granted request and a ported owner.

For a custom native request rather than built-in vitals, use the [complete native wire test](../../docs/PRISMAUI_MIGRATION.md#complete-a-native-round-trip). It has a matching manifest, page, C++ owner and expected response/event.

## Source checks

From the SkyUltra project root:

```powershell
node tools/Check-MigrationExamples.cjs
node tools/Check-LegacyShims.cjs
node tools/Check-DocumentationSource.cjs
```

These checks run in memory and do not build, install or launch Skyrim. Validate the installed page in every engine and game/input configuration you advertise.

Thanks to the PrismaUI and Magelight UI authors and the [libraries behind SkyUltra](../../docs/THIRD_PARTY_LIBRARIES.md).
