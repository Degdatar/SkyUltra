# @skyultra/vite-plugin

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

Vite integration for SkyUltra web mods. The plugin validates `manifest.json`
with the same schema and path rules as SkyUltra's native loader, uses relative
asset URLs for installed pages, and packages HTML, scripts, styles, fonts, and
images into the mod's `Files/` directory. It also places `SkyUltraBridge.js`
beside the page so the bridge stays inside the mod's permitted asset root when
MO2 resolves the manifest to a physical mod folder. The manifest stays at the
mod root.
It is plain ESM JavaScript and has no separate compile or `npm run build` step.

The npm package name is for use after publication. While developing this
checkout, `samples/react-mod-starter/` imports the plugin source directly.

## Install

```powershell
npm install -D @skyultra/vite-plugin
```

## Configure

Keep the mod manifest at `public/manifest.json` and add the plugin to Vite:

```ts
import { defineConfig } from "vite";
import { skyultra } from "@skyultra/vite-plugin";

export default defineConfig({
  plugins: [skyultra()],
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
```

The plugin supplies `base: "./"` when no base is set and rejects a different
base during production builds. Relative URLs are required because the built
page is installed under `Data/SkyUltra/ModName/Files`, while its manifest stays
at `Data/SkyUltra/ModName/manifest.json`. The plugin updates the built manifest
entry to point into `Files/`, serves and injects the local bridge in Vite's
development preview, and adds it to the packaged page during production builds.
If you include it manually, load it before your app script:

```html
<script src="./SkyUltraBridge.js"></script>
```

For local `file:///` pages, the plugin also strips Vite's `crossorigin`
attributes, disables the module-preload polyfill (which fetches local files),
and defaults the JavaScript target to ES2022. These defaults avoid silent
script and stylesheet load failures in Ultralight; a custom `build.target` is
preserved.

The manifest must follow schema version 1. Its `entry` must be a safe relative
path that exists under the project root. Network permissions accept exact DNS
hostnames only; wildcards, ports, paths, duplicate hosts, and unknown manifest
fields are rejected. CEF and Ultralight builds with network callback support
block requests with embedded URL credentials.
Local images, styles, scripts, and fonts imported or
referenced by the page are bundled or copied by Vite into `dist/Files`.
Set the optional top-level `browserEngine` field to `"ultralight"` or `"cef"`
to pin every view registered for this mod to that backend. If omitted, SkyUltra
uses the host's configured default (the current CMake default is CEF). Builds
can include both backends, allowing different views/mods to use different engines
in the same session. The host settings expose menu/per-mod engine choices;
manifest pins and backend availability also affect selection. Check runtime
metadata for the engine actually used and test the page on each supported engine.
For Papyrus-owned pages, optionally set `papyrusPlugin` to the ESP/ESM/ESL
filename containing the quest. `RegisterModForQuest(Self)` uses that field to
find this manifest without hard-coding the web-mod folder ID in Papyrus.

To keep the native manifest at the project root instead of `public/`, pass its
relative path and the plugin emits it at the output root:

```ts
plugins: [skyultra({ manifest: "manifest.json" })]
```

For a standalone Vite browser preview that installs the SDK mock, use
`skyultra({ devBridge: false })`. The default dev bridge identifies the page
as a native SkyUltra page; disabling that injection leaves `window.SkyWebView`
available for `installSkyUltraMock`. Packaged build bridge injection is unchanged.

Build the page, then copy the contents of `dist` to
`Data/SkyUltra/ModName/`. Keep the mod folder aligned with the `id` field
registered with SkyUltra; the HTML page and its assets live inside its `Files/`
subdirectory.

## Readable generated filenames

The plugin defaults to semantic filenames without hash suffixes:

```text
Files/assets/main.js
Files/assets/hud.js
Files/assets/ActorsPage.js
Files/assets/main.css
Files/assets/styles.css
```

JavaScript imports, lazy routes, HTML links and CSS references are generated
against these names by the bundler. Files are not renamed after bundling.
Name collisions receive a readable numeric suffix from Rollup. Authors can set
`build.rollupOptions.output.entryFileNames`, `chunkFileNames` and `assetFileNames`
explicitly; the plugin preserves those choices and a custom `build.assetsDir`.

When updating an installed mod, replace its generated assets folder as a unit
so retired files do not accumulate. SkyUltra's menu build helper clears only
its own staged `Files/assets` directory after a successful build, then copies
the fresh output. It never modifies installed Skyrim or mod-manager folders.
