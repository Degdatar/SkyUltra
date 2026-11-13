# __MOD_NAME__

This Vite project is a starter web mod for SkyUltra. Its manifest ID is
`__MOD_ID__`; the visible name can contain spaces and uppercase letters.

## Preview and build

```powershell
npm install
npm run dev
```

Open the localhost URL printed by Vite. The page reports whether it can see the
SkyUltra runtime. Build the installable files with:

```powershell
npm run build
```

The Vite plugin validates `public/manifest.json`, keeps asset paths relative,
leaves the manifest at the root of `dist`, and puts the page and its assets in
`dist/Files`. Copy the contents of `dist` to `Data/SkyUltra/ModName/`, then
register `__MOD_ID__` through SkyUltra's native API. The `ModName` directory is
the web mod's own folder; it is separate from SkyUltra's SKSE plugin resources.
The generated manifest selects Ultralight by default. Change its top-level
`browserEngine` value to `"cef"` to pin this mod to CEF. Mods can use different
backends at the same time because SkyUltra starts both backends included in its
build. Host settings can also select menu/per-mod engines; test the actual backend used.

Edit `src/main.js` and `src/style.css` to build the page. Add local images and
fonts under `public/` or import them from `src/`; avoid remote assets so the
page can load without network access.
