# SkyUltra UI Kit

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

The former test-menu screens now live in this package. Use `/game` for
`SkyActorStage`, `SkyItemStage`, `SkyActorDirectory`, `SkyInventoryBrowser`, `SkyCharacterSheet`,
`SkyHudEditor`, `SkyHudComposer`, and `SkyGameWidget`. `/chrome` provides
`SkyCursorScope`, `SkyMenuHeader`, `SkyMenuFooter`, and `SkySettingsSection`.
`/menu` exports the optional complete `SkyUltraMenu`. `/gameplay` exports the
small standalone `SkyGameplayHud` entry; prefer that direct import for HUD pages.
Galleries, popup patterns and the synthetic benchmark remain optional SDK examples.
All styles and themes are opt-in. See [SDK onboarding and renderer examples](../README.md).

`SkyItemStage` takes `{item,bridge,obscured?}` and shows the inventory record's
native world model with orbit/pan/zoom, lighting, Recapture and transparent PNG
Save. `SkyInventoryBrowser` accepts optional `bridge`/`obscured` props to enable
that inspector; without a bridge it retains category artwork. Grant the four
`skyUltraItemRendererMethods` in the manifest and per-view allowlist. Import
`@skyultra/ui/menu.css` for the optional stage styling. One actor or item stage
is available per view; modal, scroll and view cleanup follow `SkyActorStage`.
The source supports record texture-set overrides and generated item thumbnails.
Confirm the target item's model/materials in game; source checks do not establish
visual correctness for every modded item.

`@skyultra/ui` is the reusable React component set for SkyUltra web mods. It
ships eleven palettes (the original Frost, Dragonstone, and Arcane remain
available), buttons and icon buttons, cards and panels, HUD stats, sidebar
navigation, inventory grids, equipment slots, game-style item tooltips,
notices and dismissible toasts, form controls, controller prompts, and
accessible Radix dialogs, tabs, dropdowns, context menus, and tooltips. It also
exports Lucide React icons and an entrance-animation component. `SkyProgress`
clamps its value, exposes a labeled ARIA progress bar, supports accent, gold,
success, and danger tones, and respects reduced motion.

## Build the local packages

Build the SDK packages in dependency order from PowerShell:

```powershell
Set-Location ..\typescript
npm install
npm run build
Set-Location ..\react
npm install
npm run build
Set-Location ..\ui
npm install
npm run build
```

The UI package build produces JavaScript declarations and a compiled
`dist/styles.css`. It uses Tailwind CSS 3 with its preflight reset disabled, so
loading the SkyUltra stylesheet does not reset the rest of a mod page.

## Use in a React mod page

Add React and the three local packages to the mod project's dependencies. The
local paths below are relative to a project under `skyrim/samples/`:

```json
{
  "dependencies": {
    "@skyultra/sdk": "file:../../sdk/typescript",
    "@skyultra/react": "file:../../sdk/react",
    "@skyultra/ui": "file:../../sdk/ui",
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  }
}
```

Import the stylesheet once, then scope components with a theme root:

```tsx
import "@skyultra/ui/styles.css";
import { SkyButton, SkyPanel, SkyPanelHeader, SkyPanelTitle, SkyThemeProvider } from "@skyultra/ui";

export function ModPage() {
  return (
    <SkyThemeProvider theme="frost">
      <SkyPanel>
        <SkyPanelHeader>
          <SkyPanelTitle>Inventory</SkyPanelTitle>
        </SkyPanelHeader>
        <SkyButton onClick={() => console.info("Open inventory")}>Open</SkyButton>
      </SkyPanel>
    </SkyThemeProvider>
  );
}
```

Available themes are `frost`, `dragonstone`, `arcane`, `aurora`, `ember`,
`glacier`, `verdant`, `aether`, `dwemer`, `sunspire`, and `obsidian`. Components expose
normal React props and `className` for customization. Icon-only buttons require
an accessible `label`; Radix dialogs should include `SkyDialogTitle` and, when
useful, `SkyDialogDescription` for accessible naming and context.

## Starter sample

`skyrim/samples/react-mod-starter/` is a Vite project demonstrating the SDK,
React hooks, seven local color palettes, and four interactive sections:
Armory, Party, Chronicle, and Wayfinder. It handles `inventoryUpdated` and
`companionUpdated` events; both event types appear in the Chronicle's live
session feed. Wayfinder includes sample quests, animated objective progress,
and map marker interactions that are recorded in Chronicle as local preview
data. The sample also has a `Ctrl+K` quick-open palette for sections, items, and
color themes. Its visible companion portrait is sample artwork;
mods can provide a local portrait asset through `companionUpdated`. SkyUltra
also provides a native loaded-actor preview service, demonstrated in the
separate Field Atlas sample. Build this starter
with `npm run build`, then point the SkyUltra preview server at its `dist`
directory to try the page with the simulated bridge. The preview is not a
substitute for checking both Ultralight and CEF in Skyrim; this first component
set still needs that compatibility pass.

`skyrim/samples/ui-kit-test-mod/` is the Field Atlas in-game fixture. It includes
six on-demand sections for live character data, inventory, skills/defenses and
active effects, loaded-actor 3D/portrait previews, interactive components, and
30-second browser cadence captures at four workload sizes. Its README includes
the user-run build command, exact copy list, actor renderer limitations, and
both-engine comparison procedure.

## Lightweight core entry

Use `@skyultra/ui/core` for pages that do not need the optional Radix, Motion,
and icon-library dependencies on startup. This entry imports React only:

```tsx
import { SkyThemeScope, SkyRingMeter, SkyChoice } from "@skyultra/ui/core";
import "@skyultra/ui/styles.css";
```

The core exports `SkyThemeScope`, `SkyRingMeter`, `SkySegmentedControl`,
`SkyStatTile`, `SkyDataList`, `SkySparkline`, `SkyRadarChart`, and `SkyChoice`.
They are also re-exported by the full package. Charts are static SVGs; sampling
and update cadence belong to the caller. `SkyChoice` renders a keyboard/pointer
listbox inside the page rather than relying on an off-screen browser's native
select popup. Pass a visible description plus an accessible `label`.

`SkyThemeScope` and `SkyThemeProvider` suppress selection on ordinary game UI
text while preserving text selection inside editable controls. Obsidian adds
a charcoal/mineral palette with ivory text and champagne accents. The sample
supports reduced motion without a continuous animation loop for the kit's
core meters and charts.

## Browser support note

The components target React 18.3 and React 19. They use modern CSS and the
current Radix, Motion, and Lucide packages, but SkyUltra has not yet validated
every component against Ultralight 2.0 and the bundled CEF runtime. Pages should
test the controls they use in both engines before distribution, especially
focus behavior, animations, fonts, and any third-party component added on top.

## Framed widgets, HUDs, and popups

The React-only `@skyultra/ui/widgets` entry exports `SkyFrame`, `SkyMeterBar`,
`SkyVitalsHud`, `SkyCompassHud`, `SkyCharacterCard`, `SkyEquipmentHud`,
`SkyResourceWidget`, `SkyClockWidget`, `SkyEffectStack`, `SkyQuestTracker`,
`SkyNotificationStack`, `SkyActionWheel`, `SkyTimeline`, `SkyStepGuide`,
`SkyKeybind`, `SkySkeleton`, `SkyPagination`, `SkyPopup`, and `SkyCommandPalette`.
The main entry re-exports these too. Import `@skyultra/ui/styles.css` once.

These components accept state and callbacks. They do not poll Skyrim, create
browser surfaces, mutate saves, or provide game-specific survival/outfit/travel
logic. An application can share one snapshot feed across its widgets. The
Field Atlas sample demonstrates preview-only and pinned gameplay layouts.

```tsx
import { SkyFrame, SkyVitalsHud, SkyPopup } from "@skyultra/ui/widgets";
import "@skyultra/ui/styles.css";

<SkyFrame title="Field condition" eyebrow="YOUR MOD" footer="Live snapshot">
  <SkyVitalsHud vitals={[
    { id:"health", label:"Health", value:80, max:100, tone:"health" },
    { id:"stamina", label:"Stamina", value:60, max:120, tone:"stamina" }
  ]} />
</SkyFrame>
```

`SkyPopup` traps focus, restores it on close, dismisses on Escape/backdrop, and
uses a native input scope when the app connects
`connectSkyUltraInputScopes(bridge)` from `@skyultra/sdk`. Declare
`ui.setInputScope` in both manifest and per-view allowlists. Without that host
integration, Skyrim's global Escape handler can close the panel before the
popup handles it. Custom overlays can import `enterSkyInputScope` from
`@skyultra/ui/interaction` and call its returned release function on cleanup.
Radix menu/dialog wrappers and `SkyChoice` also participate in input scopes.

Meters clamp their drawing and handle non-finite values. Skeletons are static;
no idle shimmer loop runs. The action wheel supports up to eight callbacks and
arrow-key movement. Popup, quantity/transaction/picker examples, all seven live
widgets, presets, and their player instructions are documented in
[the Field Atlas guide](../../samples/ui-kit-test-mod/README.md).

## SkyUltra Interface components

The built-in menu consumes the same public components available to mods. Nothing
requires its layout, fonts, palette, application shell, or a C++ plugin.
The TypeScript bridge also works with Vue, Svelte and plain JavaScript.

| Import | Components and helpers |
| --- | --- |
| `@skyultra/ui/core` | SkyRingMeter, SkyRadarChart, SkyChoice, SkySegmentedControl, SkyDataList |
| `@skyultra/ui/controls` | SkyToggleSwitch, SkyRangeSlider, SkyFilterChips, SkyStatBar, skyStatMaximum, skyStatFraction |
| `@skyultra/ui/chrome` | SkyBrand, SkyNavigationRail, SkyMenuHeader, SkyMenuFooter, SkySettingsSection, SkyHotkeyEditor, SkyCursorScope, createSkyUltraCursorImage |
| `@skyultra/ui/game` | Actor directory/stage, inventory, character sheet, HUD composer/editor, guide, profiling surfaces |

Import `@skyultra/ui/styles.css` for standard themes and controls, and
`@skyultra/ui/chrome.css` for navigation/header/cursor/settings chrome.
The optional `@skyultra/ui/controls.css` entry contains only control styles for
an author who provides their own theme tokens. No network font download occurs.
Use your own CSS variables, fonts and markup freely. Menu-only layout CSS is
opt-in via `@skyultra/ui/menu.css`.

`SkyToggleSwitch` takes `label, description?, checked, onCheckedChange,
disabled?`; it exposes an accessible switch instead of a platform checkbox.
`SkyRangeSlider` takes `label, value, min?, max?, step?, unit?, onValueChange,
disabled?` and retains native keyboard range input behavior.
`SkyFilterChips` takes `label, value, options, onValueChange`; options can
include counts. Each control inherits its theme. No idle animation loop runs.

For large game values, pass **one shared maximum** to every compared bar and
radar. For example, `skyStatMaximum([40000, 80000])` returns 80000: the first
bar occupies half the track, the second the whole track. The default minimum
scale is 100. Negative bars/points are red; radar distance represents magnitude,
while labels keep the signed value. Explicit `max` remains available on radar
charts for authors who need a fixed comparison domain. Ring meters show current
versus capacity, with percentage, tick marks and an endpoint indicator.

`SkyNavigationRail` accepts controlled `items, value, onValueChange`, optional
`brand, footer, caption, label, disabled, className`. Each item supplies
`id, title, note?, icon?, shortcut?`. Bind keyboard shortcuts in your own app;
the component never changes game state on its own.

`SkyCursorScope` accepts `bridge` and a six-digit hex `accent`. Update
that accent when your theme changes; the built-in menu does this for every
palette. The native GPU cursor and browser SVG both follow the color. Grant
`ui.setCursor` in the manifest. Native tint changes reuse the same cursor
texture; there is no mousemove React state or per-frame bridge request.

## Installed paths and opening splash screen

All paths below are relative to Skyrim's **Data** directory:

- `SKSE/Plugins/SkyUltra.dll`: framework plugin.
- `SKSE/Plugins/resources/SkyUltra/Files/SkyUltraSplash.mp4`: original splash screen
  and soundtrack, shared by CEF and Ultralight.
- `SkyUltra/skyultra.menu/`: complete built-in menu, manifest and Files tree.
- `SkyUltra/[Mod Name]/manifest.json`: a custom mod's manifest.
- `SkyUltra/[Mod Name]/index.html` and `SkyUltra/[Mod Name]/Files/`: its entry
  and local web assets (or the equivalent entry named by its manifest).

In **Settings**, use **SkyUltra interface hotkey**, **Mod hotkeys for every SkyUltra mod**, and the unified **Opening splash screen & soundtrack** section (Splash screen toggle, Full splash screen / Until page ready, replay schedule, Original soundtrack, and Splash screen volume). The default splash schedule is full duration, once per game session, with audio. Hotkey chords accept **1 to 3 simultaneous inputs** across **Keyboard**, **Gamepad Controller**, and **VR Headset & Controllers**, and every mod created with SkyUltra can either be rebound from SkyUltra's Settings page or embed `<SkyHotkeyEditor />` with `readSkyUltraModHotkey()` / `updateSkyUltraModHotkey()` from `@skyultra/sdk`. Playback failures show the stage and HRESULT. A failed initialization does not consume the session replay. The top-right X or Escape closes the menu.

The TypeScript SDK exports `readSkyUltraSplashStatus()` and
`replaySkyUltraSplash()`; grant `ui.getSplashStatus` / `ui.replaySplash`.
Only the built-in menu may force replay or write global settings. Other mods
use the shared runtime opening automatically when their panel opens; gameplay
HUDs do not trigger it.

## Reading startup numbers

The lab preserves navigation/module wall times, which can include hidden loading
and idle. Opening → first page RAF measures the host visibility event to the
next page callback. It does **not** measure presentation, GPU duration, or total
cold initialization. Compare this with native first-composite logs and the
existing GPU/CPU counters. Reports retain all scopes separately. Hidden
Ultralight warm-up waits through shell readiness instead of stopping at
DOMReady before React has mounted. Cold startup costs still require new runtime
logs after building; no performance multiplier is implied by the metric change.

## Welcome banner and branding

`SkyWelcomeBanner` from `@skyultra/ui/chrome` is the reusable overview panel.
It accepts `title`, `description`, `eyebrow`, `emblem`, `brandName`, `brandCaption`,
`facts`, `actions`, and `status`. Facts contain an id, label and renderable value.
Use an independent emblem image, or supply `emblemViewBox`, `emblemWidth` and
`emblemHeight` to display a region of the original artwork through an SVG
viewport. This preserves source resolution without generating another bitmap.
The original SkyUltra artwork supplies the menu's emblem; the title and brand
labels use live typography. The orbit, frame and tick marks are static and
inherit the theme. Import `@skyultra/ui/chrome.css` for its styling.

The sidebar's `SkyBrand` now centers its entire grid track and artwork within
the rail, rather than centering content in an intrinsic-width track.
