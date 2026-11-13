# create-skyultra-mod

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

Create a Vite project for a SkyUltra web mod. The generated project includes a
schema-v1 manifest, a responsive HTML/CSS/JavaScript page, relative production
URLs, and the SkyUltra Vite plugin.

## Create a project

After publishing the package, use:

```powershell
npm create skyultra-mod MyMod
```

For development inside this SkyUltra checkout, run the CLI source directly:

```powershell
node D:\SkyUltra\skyrim\sdk\create-skyultra-mod\bin\create-skyultra-mod.js MyMod
```

The source checkout automatically points the generated project at its local
`sdk/vite-plugin` package. You can also set a custom destination or manifest
ID:

```powershell
node D:\SkyUltra\skyrim\sdk\create-skyultra-mod\bin\create-skyultra-mod.js "My Mod" --dir D:\Mods\MyMod --id my-mod
```

Names are kept for display. IDs are lowercase and use only letters, digits,
dots, underscores, and hyphens. The generated README explains how to preview
the page and install the root manifest plus `Files/` content under
`Data/SkyUltra/ModName/`.
New projects pin to Ultralight by default. Authors can set the manifest's
`browserEngine` to `"cef"`; the host can run mods pinned to either backend
side by side when the host includes both backends. The host also exposes menu
and per-mod engine choices; check actual runtime metadata when testing a port.
