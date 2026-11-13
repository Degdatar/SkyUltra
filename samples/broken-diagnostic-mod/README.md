# Broken Diagnostic Sample Mod (`samples.broken_mod`)

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

This sample intentionally demonstrates three common SkyUltra mod authoring mistakes so you can diagnose and hot-reload them live from inside Skyrim using **Developer Studio → Broken Mod Doctor**:

1. **`PERMISSION_DENIED_RUNTIME`** — `src/broken.js` invokes `game.getInventory`, which is omitted from `manifest.json` `permissions.pageRequests`. SkyUltra's native host blocks the request and logs the exact denied method in the **Bridge & Errors** ledger.
2. **`ASSET_MISSING`** — `index.html` references `./assets/missing-emblem.svg`, which is absent from the package folder. **Mod Doctor** scans HTML local asset references and flags missing package files.
3. **`MISSING_PAGE_READY_PERMISSION`** — Neither `manifest.json` nor `src/broken.js` declares/invokes `ui.pageReady` after first paint.

## Diagnosing & Repairing Inside Skyrim
1. Open SkyUltra in-game and select **Module 08 — Developer Studio**.
2. Switch to the **Broken Mod Doctor** tab and click **Deploy Broken Mod** (or copy this folder into `Data/SkyUltra/samples.broken_mod/`).
3. Inspect the live permission denial in **Bridge & Errors** and the three structured findings + copyable fix snippets in **Mod Doctor**.
4. Click **Repair & Hot Reload** (or edit the files in `Data/SkyUltra/samples.broken_mod/` with Hot Reload Watcher enabled) to watch the mod recover to `HEALTHY` inside Skyrim without restarting the game.
