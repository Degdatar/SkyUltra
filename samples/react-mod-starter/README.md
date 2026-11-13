# ModName React Starter

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

A React/Vite sample for `@skyultra/sdk`, `@skyultra/react`, `@skyultra/ui`,
and `@skyultra/vite-plugin`. The Vite plugin checks the native manifest rules,
keeps production URLs relative for local-file installation, and ensures the
manifest ships beside the built page. This in-repository starter imports the
plugin directly from `sdk/vite-plugin`; published mod projects can install the
package and import `@skyultra/vite-plugin` by name.
It includes four interactive sections (Armory, Party, Chronicle, Wayfinder),
seven vivid color palettes, an inventory grid, item details, quick kit, search
shortcuts, rarity, sorting, favourites, item comparison, event-driven companion
vitals, and an interactive sample route map. The Armory includes three starter
loadouts plus up to five custom presets; save, apply, and delete actions update
the local Quick Kit and add save/apply entries to Chronicle. Quick Kit slots
open a picker that swaps an item into that slot and records the change in
Chronicle. The sample demonstrates `inventoryUpdated` and `companionUpdated`
events through the SkyUltra bridge. The Overview banner summarizes pack,
companion, and sample contract data, with shortcuts to the companion dossier
and map.
The Chronicle combines illustrative sample history with a live session feed;
those bridge events appear at the top as they arrive. Select a record to focus
its story card, or filter the thread by gear, journey, party, or quests; the
chapter index shows counts for each kind.
The Party page presents the current companion profile and event-driven vitals,
with shortcuts to that companion's pack and last reported map location.
Bridge updates also show a short-lived toast with a shortcut to the related
section. The Wayfinder includes two sample contracts with selectable tracked
quests and objective progress using the accessible `SkyProgress` component;
map marker changes and objective updates are added to Chronicle. Page
transitions, route beacons, quest progress, and stat
bars use lightweight CSS motion; reduced-motion preferences are respected. The
Wayfinder's self-contained illustrated Skyrim atlas marks all nine holds and
their capitals. Its destination selection is independent of companion data;
the map art is original vector artwork rather than a screenshot from the game.
Toggle relief, hold borders, and roads, or zoom from 100% to 160% to inspect
the map. The collapsible Hold Directory searches either hold or capital name
and jumps to the selected destination.
The Wayfinder's destination, selected contract, and objective progress use a
Zustand store in `src/wayfinderStore.ts`. This state is local to the preview
session and does not write to a Skyrim save.
Press `Ctrl+K` to open Quick Open and search archive sections, sample items, or
color palettes from anywhere in the page. Use the arrow keys and `Enter` to
choose a result without the mouse.

The companion portrait currently visible is **sample artwork** from
`public/companion-portrait.svg`. It is not taken from Skyrim and does not update
automatically from an NPC. Mods can ship their own portrait under
`Data\SkyUltra\ModName\Files` and send its path relative to the page in a
`companionUpdated` event. For example:

```json
{
  "name": "Lydia",
  "title": "Housecarl",
  "race": "Nord",
  "role": "Warrior",
  "level": 32,
  "location": "Riften",
  "portraitUrl": "./portraits/lydia.webp",
  "health": 760,
  "healthMax": 900,
  "stamina": 280,
  "staminaMax": 350,
  "magicka": 40,
  "magickaMax": 120
}
```

SkyUltra does not currently capture a live 3D NPC portrait from the game. That
would require a native actor-capture/rendering feature. Until then, each mod
provides its own portrait art or the page displays initials when no portrait is
provided.

## Preview and interaction

Start the local sample preview from this folder:

```powershell
npm run dev
```

Open the Vite URL (normally `http://127.0.0.1:5173/`). When running in Vite
development mode, the local mock is available in the browser console. To try
the portrait path below, first put a portrait image at
`public/portraits/lydia.webp`; without a portrait asset, the page intentionally
shows the companion's initials:

```js
window.__SKYULTRA_MOCK__.emit("companionUpdated", {
  name: "Lydia",
  title: "Housecarl",
  race: "Nord",
  role: "Warrior",
  level: 32,
  portraitUrl: "./portraits/lydia.webp",
  health: 760,
  healthMax: 900,
  stamina: 280,
  staminaMax: 350,
  magicka: 40,
  magickaMax: 120
});
```

The mock also accepts `inventoryUpdated`, for example:

```js
window.__SKYULTRA_MOCK__.emit("inventoryUpdated", { count: 27 });
```

Try the four top-level sections, palette selector, category filters, `/` search
shortcut, sorting button, favourites filter, item cards, and equip preview. In
the Armory, select an item and choose **Compare** to view its rating, weight,
and value against the equipped category item (or a clearly marked collection
reference). Choose **Stage this item** to swap it into that category in the
sample Quick Kit. Toggle gear with **EQUIP IN SAMPLE**, enter a name in
the Loadout Shelf, and press **Save**. Click a Quick Kit row to choose a
replacement for that slot. Apply a different kit to see the Quick Kit update,
then switch to Chronicle to see the gear and loadout entries. Custom kits
can be deleted with the small **×** button. These presets last for the current
page session and do not equip items in Skyrim. In Chronicle, send either sample
event and the live entry appears at the top. In
Wayfinder, switch between the two sample contracts and toggle objectives to
animate quest progress; completing a contract raises a toast. Selecting any of
the nine hold capitals changes the map destination and adds a Chronicle entry,
without changing companion data. Use the map layer toggles and zoom buttons to
adjust the atlas, or expand Hold Directory to search and jump to a capital.
Press `Ctrl+K`, then search for a section,
item, or palette; use the
arrow keys and `Enter` to choose a result. Search
opens the inventory with `/`; Escape clears it. Quest, equip, and map changes
are local sample state only and do not alter the game.

To test with the SkyUltra preview bridge and its event panel, build the packages
and sample, then run the SDK preview server:

```powershell
Set-Location ..\..\sdk\typescript
npm run dev -- --root "D:\SkyUltra\skyrim\samples\react-mod-starter\dist\Files"
```

Send `companionUpdated` or `inventoryUpdated` from the preview panel. The preview
bridge simulates page events; it is not a Skyrim runtime test.

## Build the packages and sample

Start in `D:\SkyUltra\skyrim\samples\react-mod-starter`. Build the three local
packages first, then install and build this sample:

```powershell
Set-Location ..\..\sdk\typescript
npm install
npm run build
Set-Location ..\react
npm install
npm run build
Set-Location ..\ui
npm install
npm run build
Set-Location ..\samples\react-mod-starter
npm install
npm run build
```

The SkyUltra Vite plugin validates the manifest, keeps it at the root of `dist`,
and places the built page and assets in `dist\Files`. For in-game installation,
copy the contents of `dist` into `Data\SkyUltra\ModName` and register the
`modname` manifest ID through the native API. Verify the built page in Skyrim
with the chosen backend before distributing it.

Do not open the source `index.html` with `file://`. Vite must compile its TSX
entry and serve the module assets over HTTP.
