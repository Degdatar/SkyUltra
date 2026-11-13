# My Mod

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

This Vite project is a starter web mod for SkyUltra. Its manifest ID is
`my-mod`; the visible name can contain spaces and uppercase letters.

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
register `my-mod` through SkyUltra's native API. The `ModName` directory is
the web mod's own folder; it is separate from SkyUltra's SKSE plugin resources.
This sample pins its views to CEF in `public/manifest.json`. The React starter
and generated project use Ultralight by default, showing how different mods
can select different backends in the same session.

Edit `src/main.js` and `src/style.css` to build the page. Add local images and
fonts under `public/` or import them from `src/`; avoid remote assets so the
page can load without network access.
