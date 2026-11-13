# Mod author guide

Choose the integration that fits your mod:

| Integration | Start here |
| --- | --- |
| HTML/JS, TypeScript or React, without another native DLL | [SDK walkthrough](SDK_GUIDE.md) |
| An SKSE C/C++ plugin with custom game logic | [Native API guide](../api/README.md) and [example](../api/examples/MinimalSksePlugin.cpp) |
| Quest/script-driven views | [Papyrus SDK](../sdk/papyrus/README.md) |
| Existing PrismaUI frontend/native consumer | [PrismaUI migration](PRISMAUI_MIGRATION.md) |
| Existing Magelight UI manifest, frontend or native consumer | [Magelight UI migration](MAGELIGHT_UI_MIGRATION.md) |
| Adapter/shim installation and common porting steps | [Migration overview](MIGRATION_GUIDE.md) |

SkyUltra uses schema-v1 manifests with `schemaVersion`, `entry` and explicit `permissions`. Its page bridge is `window.SkyWebView`; the SDK helper is `getSkyWebView()`. The native exports are `SkyUltra_GetAPI_V1` through `SkyUltra_GetAPI_V4`; use the matching structures in the supplied header. A native mod registers an existing manifest rather than passing an in-memory manifest to a different ABI.

Browser selection is per view/mod and depends on the host's available backends and settings. A manifest may pin `browserEngine` to `cef` or `ultralight`. Test the CSS/JavaScript features your pages use in each engine you support.

Renderer and gameplay endpoints are listed in the [TypeScript contracts](../sdk/typescript/README.md). World-anchor projection uses `ui.projectWorldAnchors` (SDK `projectSkyUltraWorldAnchors`); spatial placement uses `ui.setSpatialPlacement` (SDK `configureSkyUltraSpatialPlacement`). There is no `world.projectPositions` or `world.setPlacement` endpoint in this host.

The legacy shims are supported frontend helpers, not replacement DLLs for another framework's native ABI. Their installation, custom-message loopback, native request migration and limits are documented in the migration guides. Library acknowledgments and upstream notices are in [Credits](THIRD_PARTY_LIBRARIES.md).
