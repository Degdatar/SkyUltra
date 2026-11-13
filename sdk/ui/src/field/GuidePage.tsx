import * as React from "react";
import { useState } from "react";
import { SkyFrame, SkyKeybind, SkyStepGuide } from "../widgets.js";
import { Icon } from "./icons.js";
import type { Route } from "./model.js";

export const routeHelp: Record<Route, { title: string; body: string; tips: string[] }> = {
  overview: {
    title: "Your world, at a glance",
    body: "Inspect your live character command dossier, 3D player face portrait in Active Session, circular radial vital signs, 11-slot equipped armory with 3D item icons, and carried inventory.",
    tips: [
      "Active Session renders a live 3D FaceGen portrait of your character (including worn headgear) alongside armor mitigation, Septim treasury, carry capacity, and active magical effects.",
      "Equipped for the road displays all 11 equipment slots with live 3D item icons — click any card to jump directly to that item in Inventory.",
      "Click the ? button in the top-right header next to ESC (or press Ctrl+F3) at any time to open contextual help for the current module.",
    ],
  },
  actors: {
    title: "Actor Observatory, Bust Portraits & Persistent Cache",
    body: "Browse nearby actors and creatures with head-and-shoulders bust portraits cached in your Overwrite folder, dress up NPCs from your wardrobe, and frame custom portraits.",
    tips: [
      "Click Update portrait in Character Studio to save the current angle, lighting, and pose to Data/SkyUltra/Cache/Portraits/ (redirected by your mod manager when active).",
      "Portrait frames the head and shoulders when the skeleton supports them; creature framing adapts to the rig. Full Model frames the entire body.",
      "Drag any armor, weapon, cape, lantern, jewelry, or ammo from the Wardrobe & Armory onto the actor's 11 equipment slots to dress them up.",
      "Animal and creature portraits use their canonical upright .nif bind pose so grazing goats or pecking chickens always face the camera cleanly.",
    ],
  },
  inventory: {
    title: "Inventory, Looting, Pickpocketing, Containers, Mannequins & Barter",
    body: "Manage all 11 equipment slots, read books/maps with BSA artwork, loot dead NPCs & monsters (skyultra_loot.swf), pickpocket NPCs with Steal & Plant (skyultra_pickpocket.swf), use Storage Containers (skyultra_container.swf), dress Armor Mannequins in 3D (skyultra_mannequin.swf), and trade with merchants (skyultra_barter.swf).",
    tips: [
      "Corpse & Monster Looting (skyultra_loot.swf) opens in Half Container mode by default with single-click Take & Store, Take All & Store All bulk actions, and Gold displayed in both inventories.",
      "NPC Pickpocketing (skyultra_pickpocket.swf) opens in Half Container mode by default: items on the targeted NPC say Steal (and show an EQUIPPED badge if worn by the NPC), while items in your inventory say Plant to reverse-pickpocket items or Gold onto the NPC.",
      "Every container header includes two 1-click cycling buttons: one cycles Half Container → Windowed → Full Screen, and the other toggles Live World ↔ Pause World.",
      "3D item icons are cached in Data/SkyUltra/Cache/Items/. MO2 normally redirects generated files to Overwrite or a configured output mod.",
    ],
  },
  character: {
    title: "Character Sheet, Constellations & Loadouts",
    body: "Inspect your live 3D player portrait, derived combat archetype, physical & elemental mitigation caps, 18 unique skill constellations, stacked active enchantments, and 11-slot outfit loadouts.",
    tips: [
      "Overview & Build features your live 3D FaceGen character portrait, physical damage reduction toward Skyrim's 80% (667 armor) cap, and Effective Health Pool (EHP).",
      "Skill Constellations render custom crests for all 18 disciplines across Combat, Magic, and Stealth; Active Magic & Buffs stacks duplicate enchantments with school-specific icons.",
      "Loadouts & Staging lets you snapshot, rename, diff, apply, undo, and JSON-sync 11-slot outfits with 3D item thumbnails persisted per character.",
    ],
  },
  components: {
    title: "Interface Workshop",
    body: "Explore reusable controls with single-step focus traversal, input scopes, and adaptive motion.",
    tips: [
      "Tab and Shift+Tab step one control at a time; Enter activates the focused control.",
      "Escape dismisses an open dropdown, modal, or recording scope before closing the menu.",
      "All components adapt automatically across all 16 built-in SkyUltra Classic, Celestial, and Neon themes (Dragonstone default).",
    ],
  },
  widgets: {
    title: "Visual HUD Builder, Live Code Editor & Mod Exporter",
    body: "Drag, snap, resize, write or paste custom HTML/CSS/JS with real-time HUD streaming, pin containerless HUD overlays over gameplay, or export a standalone zero-DLL SkyUltra mod.",
    tips: [
      "Drag any HUD widget directly on the 6-zone stage canvas with 0/8/16/24px grid snapping, corner resize handles, custom labels, live data-binding paths, and conditional visibility rules.",
      "Switch to Live Code Editor to write or paste custom HTML, CSS, and JavaScript (or a complete single-file HTML document) using SkyUltraHud.onUpdate(), data-sky-bind, and data-sky-bar with real-time Skyrim HUD updates.",
      "Open Export Working Mod to write a ready-to-run mod folder (manifest.json + index.html + hud.html + README.md) straight into Data/SkyUltra/<id>/ (e.g. Data/SkyUltra/test/) and inspect it directly in Developer Studio.",
    ],
  },
  performance: {
    title: "Performance Lab",
    body: "Compare startup milestones, native D3D11 compositor GPU timings, and fixed 30-second frame cadence captures.",
    tips: [
      "Page RAF measures UI callbacks; Compositor GPU measures native composition after browser and 3D preview rendering.",
      "Use identical resolution, theme, location, and pinned views when comparing Ultralight and CEF.",
      "Export structured JSON reports with full sample counts and percentile breakdowns.",
    ],
  },
  devtools: {
    title: "Developer Studio, Mod Doctor & Migration Tools",
    body: "Inspect live Ultralight/CEF surfaces, filter bridge traffic & permission denials, hot-reload mod packages, repair broken manifests, and audit legacy PrismaUI / MageLight ports.",
    tips: [
      "Views & Layers and Bridge & Errors stream active HUD/panel surfaces, VRAM footprints, input scope depths, latencies, and copyable manifest.json permission fixes.",
      "Broken Mod Doctor deploys a live diagnostic sample inside Skyrim, surfaces manifest/asset faults, and repairs + hot-reloads it without restarting the game.",
      "Runtime Coexistence & Legacy Migration Scanner verifies D3D11 layer/cursor rules alongside PrismaUI, MageLight, TrueHUD, Wheeler, and Scaleform menus.",
    ],
  },
  settings: {
    title: "Profiles, Multi-Engine Runtime, All 8 In-Game Menu Replacements & VR",
    body: "Switch SkyUltra's browser engine between CEF (Default) and Ultralight without affecting any other mod, configure all 8 Skyrim menu replacements (Inventory, Barter, Storage Containers, Corpse Looting, NPC Pickpocketing, Mannequins, Spellbook, Favorites), and customize 16 themes.",
    tips: [
      "Simultaneous Multi-Engine Runtime defaults to Chromium CEF and lets you switch SkyUltra between CEF and Ultralight (and set per-mod engine overrides) concurrently at peak GPU speed.",
      "Configure independent toggles, layout modes (Half Container / Windowed / Full Screen), and world state (Live World / Pause World) for Inventory, Vendor Barter, Storage Containers, Corpse & Monster Looting, NPC Pickpocketing, Armor Mannequins, Spellbook, and Favorites.",
      "The Virtual Keyboard Hotkey (Shift + K default on Keyboard, LT + LeftThumb on Gamepad, LeftGrip + LeftStickClick in VR) toggles the full ESC / F1–F12 / Shift / Ctrl / Alt / Tab / Caps keyboard whether the SkyUltra menu is open or closed.",
    ],
  },
  guide: {
    title: "SkyUltra Guide",
    body: "Complete starter guide and technical reference for SkyUltra's native D3D11 compositor, 3D studios, visual HUD builder, persistent character profiles, Developer Studio, and mod migration.",
    tips: [
      "Press Tab / Shift+Tab to step through interactive items one by one without skipping.",
      "Use 1–9 to jump between all 9 command modules, Space to search the current page or open the command palette, and the ? button in the header (or Ctrl+F3) for contextual help.",
      "See the Mod Authoring, Persistent Profiles & Coexistence blueprints below for schemaVersion 1 manifests and legacy web-mod porting.",
    ],
  },
};

const guideSteps: Array<{ id: string; title: string; description: string; route: Route; badge: string }> = [
  {
    id: "overview",
    title: "Command Dossier, 3D Portrait & Equipped Armory",
    description:
      "Start in Overview to read your circular radial vital signs, Active Session deck with your live 3D face portrait, and all 11 equipped gear slots with 3D item icons.",
    route: "overview",
    badge: "MODULE 01 · OVERVIEW",
  },
  {
    id: "actors",
    title: "Actor Observatory & Custom Portraits",
    description:
      "Browse nearby people and animals with upright front-facing portraits, dress them up from your wardrobe, and click the studio Camera icon to set a new portrait.",
    route: "actors",
    badge: "MODULE 02 · ACTOR OBSERVATORY",
  },
  {
    id: "inventory",
    title: "11-Slot Outfit & Item Studio",
    description:
      "Equip weapons, armor, capes (slot 46), and wearable lanterns (slot 49) via drag-and-drop, or inspect any item mesh in 3D.",
    route: "inventory",
    badge: "MODULE 03 · INVENTORY",
  },
  {
    id: "character",
    title: "Character Sheet, 18 Skills & Outfit Loadouts",
    description:
      "Audit physical & elemental mitigation caps, Effective Health Pool (EHP), 18 unique skill crests, stacked enchantments, and 11-slot staged outfit presets.",
    route: "character",
    badge: "MODULE 04 · CHARACTER SHEET",
  },
  {
    id: "settings",
    title: "Per-Character Profiles, 16 Themes & Mod Hotkeys",
    description:
      "Save per-character themes & layouts with global defaults in SkyUltra.settings.json, switch between 16 themes, and bind 1–3 input chords on Keyboard, Gamepad, and VR.",
    route: "settings",
    badge: "MODULE 05 · SETTINGS",
  },
  {
    id: "widgets",
    title: "Visual HUD Builder, Live Code Editor & Mod Export",
    description:
      "Drag and resize floating HUDs on a 6-zone grid, code or paste custom HTML/CSS/JS live, export standalone zero-DLL HUD mods, and benchmark Ultralight vs CEF.",
    route: "widgets",
    badge: "MODULES 06–07 · HUDS & LAB",
  },
  {
    id: "devtools",
    title: "Developer Studio, Broken Mod Doctor & Migration",
    description:
      "Inspect live browser surfaces and bridge traffic, hot-reload edited mod packages in-game, repair broken manifests, and scan PrismaUI / MageLight sources for porting.",
    route: "devtools",
    badge: "MODULE 08 · DEVELOPER STUDIO",
  },
];

const platformPillars: Array<{ tag: string; title: string; copy: string; specs: string[] }> = [
  {
    tag: "NATIVE D3D11 COMPOSITOR",
    title: "Shared In-Process SKSE64 UI Platform",
    copy: "SkyUltra is a native SKSE64 plugin (SkyUltra.dll) that composites web interfaces and 3D GPU render surfaces directly inside Skyrim's Direct3D 11 swapchain across Skyrim SE, AE, and VR.",
    specs: ["Match SKSE and Address Library to your runtime", "Shared push game-data feed", "Native UI and HUD compositing"],
  },
  {
    tag: "SIMULTANEOUS DUAL ENGINE",
    title: "Concurrent Ultralight & Chromium (CEF) Runtimes",
    copy: "Includes CEF and Ultralight browser backends with per-view routing. Different mods can use different engines in the same session; available engines and settings determine the actual selection.",
    specs: ["Per-mod and menu engine settings", "GPU rendering on Skyrim's D3D11 device", "Compare your workload in Performance Lab"],
  },
  {
    tag: "3D GPU PREVIEW STUDIOS",
    title: "Live Player & Actor FaceGen + .NIF Rendering",
    copy: "Dedicated native D3D11 render targets load your live Dragonborn face & helmet portrait, nearby NPCs, 11-slot equipment loadouts, BSA/loose .nif meshes, DDS textures, and 33-frame animations.",
    specs: ["Live 3D player portrait in UI & HUD", "Upright creature bind-pose portraits", "Transparent PNG export up to 4096px"],
  },
  {
    tag: "STUDIO, PROFILES & DEVTOOLS",
    title: "Visual HUD Export, Profiles & Mod Doctor",
    copy: "Author zero-DLL HUD mods visually, persist layouts & loadouts per character with schemaVersion 2 migrations, and debug or hot-reload mods live in Developer Studio.",
    specs: ["Per-character profiles & global defaults", "1-click zero-DLL HUD mod exporter", "Live hot-reload & migration scanner"],
  },
];

export default function GuidePage({
  onNavigate,
}: {
  onNavigate(route: Route): React.JSX.Element | void;
}): React.JSX.Element {
  const [step, setStep] = useState(0);
  const [deviceTab, setDeviceTab] = useState<"keyboard" | "gamepad" | "vr">("keyboard");
  const activeStep = guideSteps[step] ?? guideSteps[0];

  return (
    <div className="page guide-page">
      <div className="page-lead guide-hero">
        <div className="guide-hero__copy">
          <span className="eyebrow">THE OFFICIAL SKYULTRA PLATFORM HANDBOOK</span>
          <h1>
            SkyUltra Guide<span className="title-period">.</span>
          </h1>
          <p>
            Welcome to <strong>SkyUltra</strong> — the Advanced Webview Interface, 3D Character &amp; Item Studio, Visual
            HUD Builder, Persistent Character Profile Manager, and In-Game Developer Studio for{" "}
            <em>The Elder Scrolls V: Skyrim Special Edition</em>, <em>Anniversary Edition</em>, and <em>Skyrim VR</em>.
          </p>
          <div className="guide-hero__pills">
            <button type="button" className="control-button is-active" onClick={() => onNavigate("overview")}>
              <Icon name="overview" size={14} /> Command Dossier
            </button>
            <button type="button" className="control-button" onClick={() => onNavigate("actors")}>
              <Icon name="actors" size={14} /> Actor Observatory
            </button>
            <button type="button" className="control-button" onClick={() => onNavigate("character")}>
              <Icon name="shield" size={14} /> Sheet &amp; Loadouts
            </button>
            <button type="button" className="control-button" onClick={() => onNavigate("widgets")}>
              <Icon name="widgets" size={14} /> Visual HUD Builder
            </button>
            <button type="button" className="control-button" onClick={() => onNavigate("devtools")}>
              <Icon name="rune" size={14} /> Developer Studio
            </button>
            <button type="button" className="control-button" onClick={() => onNavigate("settings")}>
              <Icon name="components" size={14} /> Profiles &amp; Settings
            </button>
          </div>
        </div>
        <div className="guide-hero__emblem" aria-label="SkyUltra Emblem">
          <Icon name="book" size={56} />
          <span>SKYULTRA</span>
        </div>
      </div>

      <SkyFrame
        title="Requirements to Play This Mod"
        eyebrow="ESSENTIAL SYSTEM & MOD REQUIREMENTS · SE / AE / VR"
        className="guide-starter-overview"
        actions={
          <button type="button" className="control-button" onClick={() => onNavigate("settings")}>
            Configure Menu Replacements <Icon name="arrow" size={13} />
          </button>
        }
      >
        <div className="guide-starter-lead">
          <p>
            Use a Skyrim SE, AE or VR runtime supported by your SkyUltra release, with matching SKSE and Address
            Library files. These are the runtime dependencies; browser libraries are supplied with SkyUltra.
          </p>
        </div>

        <div className="guide-pillars-grid">
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">01 · SKSE64 / SKSEVR</span>
            <h3>Skyrim Script Extender (Required)</h3>
            <p>
              Install <strong>SKSE64</strong> for your exact supported SE/AE executable, or
              <strong>SKSEVR</strong> for VR, and always launch the game through{" "}
              <code>skse64_loader.exe</code> / <code>sksevr_loader.exe</code>.
            </p>
            <ul className="guide-pillar-card__specs">
              <li><Icon name="check" size={12} /> Match the installed game runtime</li>
              <li><Icon name="check" size={12} /> Loads Data/SKSE/Plugins/SkyUltra.dll</li>
            </ul>
          </article>

          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">02 · ADDRESS LIBRARY</span>
            <h3>Address Library for SKSE Plugins (Required)</h3>
            <p>
              Install <strong>Address Library for SKSE Plugins</strong> (All-in-One SE/AE) or{" "}
              <strong>VR Address Library for SKSEVR</strong> so <code>SkyUltra.dll</code> resolves engine offsets
              for the game version supported by that release. A game update also needs matching SKSE and address data.
            </p>
            <ul className="guide-pillar-card__specs">
              <li><Icon name="check" size={12} /> Match the database to your executable</li>
              <li><Icon name="check" size={12} /> Required for SE, AE, and VR builds</li>
            </ul>
          </article>

          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">03 · SYSTEM &amp; GPU RUNTIME</span>
            <h3>MSVC 2022 Redistributable &amp; DirectX 11</h3>
            <p>
              Requires 64-bit Windows 10/11, the <strong>Microsoft Visual C++ 2015–2022 Redistributable (x64)</strong>,
              and a DirectX 11-capable GPU with a working graphics driver.
            </p>
            <ul className="guide-pillar-card__specs">
              <li><Icon name="check" size={12} /> Bundled CEF &amp; Ultralight runtimes included</li>
              <li><Icon name="check" size={12} /> Persistent Overwrite cache in SkyUltra/Cache/</li>
            </ul>
          </article>

          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">04 · COMPLETE SKYULTRA PACKAGE</span>
            <h3>Plugin, browser runtimes &amp; web assets</h3>
            <p>
              Keep the plugin, bundled CEF/Ultralight runtimes, scripts and web assets from the same release
              together. Players do not need a browser SDK, Node.js or a frontend compiler.
            </p>
            <ul className="guide-pillar-card__specs">
              <li><Icon name="check" size={12} /> Install the complete release archive</li>
              <li><Icon name="check" size={12} /> Launch Skyrim through SKSE</li>
            </ul>
          </article>
        </div>
      </SkyFrame>

      <SkyFrame
        title="In-Game Menu Replacements (8 SWFs) & Persistent Overwrite Cache"
        eyebrow="LOOTING · PICKPOCKETING · CONTAINERS · MANNEQUINS · BARTER · SPELLBOOK · FAVORITES"
        className="guide-starter-overview"
      >
        <div className="guide-pillars-grid">
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">SKYULTRA_LOOT.SWF · DEAD NPCS &amp; MONSTERS</span>
            <h3>Corpse &amp; Monster Looting Container</h3>
            <p>
              Opening any fallen NPC, creature, or monster launches the <strong>Corpse &amp; Monster Loot Container</strong>{" "}
              in <strong>Half Container</strong> mode by default. Click any item (including <strong>Gold</strong>) to{" "}
              <strong>Take</strong> or <strong>Store</strong>, use <strong>Take All</strong> / <strong>Store All</strong>,
              and cycle <strong>Half Container → Windowed → Full Screen</strong> or <strong>Live World ↔ Pause World</strong>{" "}
              from the two header buttons.
            </p>
          </article>

          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">SKYULTRA_PICKPOCKET.SWF · STEAL &amp; PLANT</span>
            <h3>NPC Pickpocketing Container</h3>
            <p>
              Pickpocketing a live NPC launches the <strong>Pickpocket Container</strong> in <strong>Half Container</strong>{" "}
              mode by default. Items on the targeted NPC display <strong>Steal</strong> (with an{" "}
              <strong>EQUIPPED</strong> badge on worn gear), while your inventory tab displays <strong>Plant</strong> to
              reverse-pickpocket items or <strong>Gold</strong> onto the target.
            </p>
          </article>

          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">CONTAINERS · MANNEQUINS · BARTER · MAGIC</span>
            <h3>Storage, 3D Mannequins, Barter &amp; Spellbook</h3>
            <p>
              Includes dedicated replacements for <strong>Storage Containers</strong> (<code>skyultra_container.swf</code>{" "}
              with Gold transfer), <strong>Armor Mannequins</strong> (<code>skyultra_mannequin.swf</code> with a static 3D
              dress-up stage), <strong>Vendor Barter</strong> (<code>skyultra_barter.swf</code>),{" "}
              <strong>Spellbook</strong> (<code>skyultra_magic.swf</code> with Spell Rename &amp; Delete), and{" "}
              <strong>Favorites</strong> (<code>skyultra_favorites.swf</code>).
            </p>
          </article>

          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">PERSISTENT OVERWRITE CACHE</span>
            <h3>Instant Item Icons &amp; Bust Portraits</h3>
            <p>
              Rendered 3D item icons and actor/player bust portraits (head, shoulders &amp; upper chest) are saved as PNGs
              in <code>Data/SKSE/Plugins/SkyUltra/Cache/Items/</code> and <code>Portraits/</code> (MO2 Overwrite) so you
              never wait for icons to re-render on launch, and clicking <strong>Update portrait</strong> refreshes the
              cached file immediately.
            </p>
          </article>
        </div>
      </SkyFrame>

      <SkyFrame
        title="What is SkyUltra? — Starter Overview & Architecture"
        eyebrow="FOR STARTERS · FROM THE SKYULTRA ARCHITECTURE"
        className="guide-starter-overview"
        actions={
          <button type="button" className="control-button" onClick={() => onNavigate("devtools")}>
            Open Developer Studio <Icon name="arrow" size={13} />
          </button>
        }
      >
        <div className="guide-starter-lead">
          <p>
            <strong>SkyUltra</strong> is a shared native SKSE64 web UI platform and 3D GPU studio that lets Skyrim mods
            build responsive, hardware-accelerated interfaces, click-through HUD overlays, and 3D model inspectors using
            standard web technology (<code>HTML</code>, <code>CSS</code>, <code>TypeScript</code>, and <code>React</code>)
            without each mod shipping its own browser runtime or Direct3D swapchain hook.
          </p>
        </div>

        <div className="guide-pillars-grid">
          {platformPillars.map((pillar) => (
            <article key={pillar.title} className="guide-pillar-card">
              <span className="guide-pillar-card__tag">{pillar.tag}</span>
              <h3>{pillar.title}</h3>
              <p>{pillar.copy}</p>
              <ul className="guide-pillar-card__specs">
                {pillar.specs.map((s) => (
                  <li key={s}>
                    <Icon name="check" size={12} /> {s}
                  </li>
                ))}
              </ul>
            </article>
          ))}
        </div>
      </SkyFrame>

      <div className="guide-highlights-strip" aria-label="Core interface capabilities">
        <div className="guide-highlight-card">
          <span className="guide-highlight-card__icon">
            <Icon name="camera" size={16} />
          </span>
          <div>
            <strong>Live 3D Player &amp; NPC Portraits</strong>
            <small>Real-time FaceGen + headgear portraits across Overview, Character Sheet, HUDs &amp; Observatory.</small>
          </div>
        </div>
        <div className="guide-highlight-card">
          <span className="guide-highlight-card__icon">
            <Icon name="widgets" size={16} />
          </span>
          <div>
            <strong>Visual HUD Builder &amp; Exporter</strong>
            <small>Drag, snap, resize &amp; export standalone zero-DLL HUD mods into Data/SkyUltra/.</small>
          </div>
        </div>
        <div className="guide-highlight-card">
          <span className="guide-highlight-card__icon">
            <Icon name="shield" size={16} />
          </span>
          <div>
            <strong>Per-Character Profiles (v2)</strong>
            <small>Automatic per-character themes, HUD layouts &amp; loadouts with global defaults &amp; JSON sync.</small>
          </div>
        </div>
        <div className="guide-highlight-card">
          <span className="guide-highlight-card__icon">
            <Icon name="rune" size={16} />
          </span>
          <div>
            <strong>In-Game Developer Studio</strong>
            <small>Hot-reload mods, audit bridge permissions, fix broken manifests &amp; port legacy UIs.</small>
          </div>
        </div>
      </div>

      <div className="guide-grid">
        <SkyFrame
          title="Interactive mastery tour"
          eyebrow={activeStep.badge}
          actions={
            <button className="primary-button" onClick={() => onNavigate(activeStep.route)}>
              Open {activeStep.route === "overview" ? "Overview" : activeStep.route} <Icon name="arrow" size={14} />
            </button>
          }
        >
          <SkyStepGuide
            active={step}
            onStepChange={setStep}
            steps={guideSteps.map(({ id, title, description }) => ({ id, title, description }))}
          />
        </SkyFrame>

        <SkyFrame title="At your fingertips" eyebrow="MULTI-DEVICE NAVIGATION & HOTKEYS">
          <div className="guide-device-tabs" role="tablist" aria-label="Control device reference">
            {(
              [
                { id: "keyboard", label: "Keyboard & Mouse" },
                { id: "gamepad", label: "Gamepad Controller" },
                { id: "vr", label: "VR Headset & Controllers" },
              ] as const
            ).map((tab) => (
              <button
                key={tab.id}
                type="button"
                role="tab"
                aria-selected={deviceTab === tab.id}
                className={"guide-device-tab" + (deviceTab === tab.id ? " is-active" : "")}
                onClick={() => setDeviceTab(tab.id)}
              >
                {tab.label}
              </button>
            ))}
          </div>

          {deviceTab === "keyboard" && (
            <div className="guide-keybinds">
              <SkyKeybind keys={["Tab"]}>Move to next interactive item (1 by 1)</SkyKeybind>
              <SkyKeybind keys={["Shift", "Tab"]}>Move to previous item (1 by 1)</SkyKeybind>
              <SkyKeybind keys={["Enter"]}>Activate focused button, slot, or tile</SkyKeybind>
              <SkyKeybind keys={["1–9"]}>Switch across all 9 command modules</SkyKeybind>
              <SkyKeybind keys={["Shift", "K"]}>Toggle full Virtual Keyboard (ESC, F1–F12, Shift, Ctrl, Alt, Tab, Caps) on/off anytime</SkyKeybind>
              <SkyKeybind keys={["Space"]}>Search this page, or open the command palette</SkyKeybind>
              <SkyKeybind keys={["Ctrl", "F3"]}>Open contextual help (? button in header)</SkyKeybind>
              <SkyKeybind keys={["Esc"]}>Dismiss popup / scope, then close panel</SkyKeybind>
              <SkyKeybind keys={["Shift", "F6"]}>Default SkyUltra toggle (customizable up to 3 keys)</SkyKeybind>
            </div>
          )}

          {deviceTab === "gamepad" && (
            <div className="guide-keybinds">
              <SkyKeybind keys={["Back", "RightThumb"]}>Default controller toggle (customizable up to 3 buttons)</SkyKeybind>
              <SkyKeybind keys={["LT", "LeftThumb"]}>Toggle full on-screen Virtual Keyboard anytime (menu open or closed)</SkyKeybind>
              <SkyKeybind keys={["L-Stick / R-Stick"]}>Move V-CURSOR virtual pointer &amp; step through controls</SkyKeybind>
              <SkyKeybind keys={["A"]}>Activate focused control, V-CURSOR target, or equip item</SkyKeybind>
              <SkyKeybind keys={["B"]}>Dismiss open popup or close SkyUltra panel</SkyKeybind>
              <SkyKeybind keys={["LB / RB"]}>Combine with any button for 2- or 3-button mod chords</SkyKeybind>
              <SkyKeybind keys={["LT / RT"]}>Supported in custom 1-to-3 button hotkey chords</SkyKeybind>
            </div>
          )}

          {deviceTab === "vr" && (
            <div className="guide-keybinds">
              <SkyKeybind keys={["LeftGrip", "RightBY"]}>Default VR toggle (customizable up to 3 VR inputs)</SkyKeybind>
              <SkyKeybind keys={["LeftGrip", "LeftStickClick"]}>Toggle full VR Virtual Keyboard anytime (menu open or closed)</SkyKeybind>
              <SkyKeybind keys={["HMD Head Tracking"]}>Open menus follow your headset gaze (atGaze) wherever you look</SkyKeybind>
              <SkyKeybind keys={["Trigger"]}>Laser pointer click &amp; drag in OpenVR overlay</SkyKeybind>
              <SkyKeybind keys={["Thumbstick"]}>Smooth page &amp; roster scrolling in VR</SkyKeybind>
              <SkyKeybind keys={["Left / Right BY"]}>Dismiss popup or exit VR overlay</SkyKeybind>
              <SkyKeybind keys={["Grip + Trigger + BY"]}>Full 3-input VR chord support for SkyUltra &amp; mods</SkyKeybind>
            </div>
          )}
        </SkyFrame>
      </div>

      <div className="guide-chapters">
        {(
          ["overview", "actors", "inventory", "character", "settings", "widgets", "performance", "devtools"] as Route[]
        ).map((route) => (
          <SkyFrame
            key={route}
            title={routeHelp[route].title}
            eyebrow={route.toUpperCase()}
            actions={
              <button className="text-action" onClick={() => onNavigate(route)}>
                Explore <Icon name="arrow" size={13} />
              </button>
            }
          >
            <p>{routeHelp[route].body}</p>
            <ul>
              {routeHelp[route].tips.map((tip) => (
                <li key={tip}>{tip}</li>
              ))}
            </ul>
          </SkyFrame>
        ))}
      </div>

      <SkyFrame
        title="How Mod Hotkeys, Zero-DLL Packages & Manifests Work"
        eyebrow="STEP-BY-STEP ARCHITECTURE FOR MOD AUTHORS & PLAYERS"
        className="author-guide"
      >
        <p>
          SkyUltra discovers third-party mods through a <strong>3-step manifest &amp; native bridge pipeline</strong>{" "}
          inside <code>SkyUltra.dll</code> so both <strong>zero-DLL web mods</strong> (pure HTML/JS/CSS folders or
          exported HUD packages) and <strong>C++ / Papyrus SKSE mods</strong> get full 1-to-3 input hotkey chords across
          Keyboard, Gamepad, and VR Headset:
        </p>

        <div className="guide-pillars-grid" style={{ marginTop: 12, marginBottom: 14 }}>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">STEP 1 · FOLDER &amp; MANIFEST DISCOVERY</span>
            <h3>Drop Your Mod into Data/SkyUltra/&lt;id&gt;/</h3>
            <p>
              Every SkyUltra mod ships a folder at <code>Data/SkyUltra/&lt;id&gt;/manifest.json</code> (such as{" "}
              <code>test</code> or <code>author.modname</code>) alongside its HTML bundle. On game launch (and in
              Developer Studio / Settings), <code>DiscoverAvailableMods()</code> scans <code>Data/SkyUltra/</code>,
              validates <code>schemaVersion: 1</code>, and lists the mod automatically.
            </p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">STEP 2 · HOTKEY CHORDS &amp; PERMISSIONS</span>
            <h3>Bind 1–3 Input Chords &amp; Least-Privilege Bridge Access</h3>
            <p>
              Declare explicit <code>permissions.pageRequests</code> in <code>manifest.json</code> and bind 1–3 input
              chords (<code>keyboard</code>, <code>gamepad</code>, <code>vr</code>) in{" "}
              <strong>Settings → Mod Hotkeys</strong> or via <code>updateSkyUltraModHotkey()</code>. Missing permissions
              are caught live in <strong>Developer Studio → Bridge &amp; Errors</strong> with one-click fix snippets.
            </p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">STEP 3 · ZERO-DLL TOGGLE &amp; HOT RELOAD</span>
            <h3>Automatic In-Game Launch &amp; Live File Watcher</h3>
            <p>
              Pressing a mod&apos;s hotkey chord in Skyrim registers and opens its panel—with{" "}
              <strong>no C++ DLL or ESP required</strong>. Enable the file watcher in{" "}
              <strong>Developer Studio</strong> to hot-reload HTML/CSS/JS edits while Skyrim stays open.
            </p>
          </article>
        </div>

        <div className="guide-code">
          <code>// 1. Data/SkyUltra/author.my_mod/manifest.json (schemaVersion 1)</code>
          <code>&#123;</code>
          <code>  &quot;schemaVersion&quot;: 1,</code>
          <code>  &quot;id&quot;: &quot;author.my_mod&quot;,</code>
          <code>  &quot;name&quot;: &quot;My Awesome SkyUltra Mod&quot;,</code>
          <code>  &quot;version&quot;: &quot;1.0.0&quot;,</code>
          <code>  &quot;entry&quot;: &quot;Files/index.html&quot;,</code>
          <code>  &quot;permissions&quot;: &#123;</code>
          <code>    &quot;storage&quot;: false,</code>
          <code>    &quot;assetAccess&quot;: true,</code>
          <code>    &quot;networkHosts&quot;: [],</code>
          <code>    &quot;pageRequests&quot;: [&quot;ui.pageReady&quot;, &quot;ui.close&quot;, &quot;ui.setInputScope&quot;, &quot;ui.getSettings&quot;, &quot;ui.updateSettings&quot;, &quot;game.getPlayerSnapshot&quot;, &quot;game.subscribe&quot;]</code>
          <code>  &#125;</code>
          <code>&#125;</code>
          <code> </code>
          <code>// 2. Connect shared game telemetry &amp; self-service hotkeys with @skyultra/sdk</code>
          <code>import &#123; markSkyUltraPageReady, readSkyUltraModHotkey, updateSkyUltraModHotkey &#125; from &quot;@skyultra/sdk&quot;;</code>
          <code>await markSkyUltraPageReady();</code>
          <code>const binding = await readSkyUltraModHotkey(&quot;author.my_mod&quot;);</code>
          <code>await updateSkyUltraModHotkey(&quot;author.my_mod&quot;, nextBinding);</code>
        </div>
      </SkyFrame>

      <SkyFrame
        title="Persistent Character Profiles, Coexistence & Porting Legacy Web Mods"
        eyebrow="PROFILES V2 · PRISMAUI / MAGELIGHT MIGRATION & RUNTIME COEXISTENCE"
        className="author-guide"
      >
        <div className="guide-pillars-grid" style={{ marginBottom: 14 }}>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">PERSISTENT PROFILES (SCHEMAVERSION 2)</span>
            <h3>Per-Character Layouts &amp; Global Defaults</h3>
            <p>
              SkyUltra stores themes, UI density/glow, 6-zone HUD placements, and 11-slot outfit loadouts natively inside{" "}
              <code>SkyUltra.settings.json</code> (<code>profileState</code> v2). Switching Skyrim saves automatically
              activates that character&apos;s profile while new characters inherit your Global Defaults.
            </p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">PORTING PRISMAUI &amp; MAGELIGHT MODS</span>
            <h3>Incremental Adapter &amp; Source Scanner</h3>
            <p>
              Install <code>installSkyUltraCompetitorShims(window, bridge)</code> before adapted legacy scripts
              to keep Prisma-style helpers or Magelight <code>send/on</code>. Use{" "}
              <code>createSkyUltraLegacyBridgeAdapter()</code> for explicit method/event mappings and the source
              scanner in Developer Studio to locate old bridge calls.
            </p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">RUNTIME COEXISTENCE BOUNDARY</span>
            <h3>Other UI &amp; graphics mods</h3>
            <p>
              SkyUI, ENB, Community Shaders, Engine Fixes, TrueHUD and Wheeler are not SkyUltra requirements.
              Enable only the menu replacements you want, keep passive HUDs click-through and use distinct hotkeys.
              Specific mod and graphics-injector combinations still need testing.
            </p>
          </article>
        </div>

        <div className="guide-capabilities">
          <span>
            <Icon name="check" />
            Balanced 9-Module Command Suite (Keys 1–9)
          </span>
          <span>
            <Icon name="check" />
            Per-Character Profiles &amp; Global Defaults (v2)
          </span>
          <span>
            <Icon name="check" />
            1-Click Working HUD Mod Exporter
          </span>
          <span>
            <Icon name="check" />
            Developer Studio Hot-Reload &amp; Mod Doctor
          </span>
          <span>
            <Icon name="shield" />
            Legacy bridge shims &amp; migration tools
          </span>
        </div>
      </SkyFrame>

      <SkyFrame title="Use the APIs & SDK" eyebrow="MOD AUTHOR QUICK START" className="author-guide">
        <div className="guide-pillars-grid">
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">HTML / TYPESCRIPT / REACT</span>
            <h3>A page, a manifest &amp; the bridge</h3>
            <p>Place <code>manifest.json</code> at <code>Data/SkyUltra/your.mod/</code>, with{" "}
              <code>schemaVersion: 1</code>, an <code>entry</code> inside <code>Files/</code> and explicit
              {" "}<code>permissions.pageRequests</code>. Load <code>SkyUltraBridge.js</code> before your app.
              The exact global is <code>window.SkyWebView</code>.</p>
            <p>Use <code>@skyultra/sdk</code> for typed requests and shared game updates,{" "}
              <code>@skyultra/react</code> for hooks, and <code>@skyultra/ui</code> for opt-in components/styles.
              The walkthrough is <code>docs/SDK_GUIDE.md</code>.</p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">NATIVE / PAPYRUS</span>
            <h3>Register, create &amp; exchange JSON</h3>
            <p>Use the versioned C ABI or <code>SkyUltraAPI.hpp</code> to register an installed manifest and create
              a view. Creation is queued: check readiness, then use a page handshake before sending initial data.
              Custom requests need both manifest and view permissions. Native request IDs are strings.</p>
            <p>See <code>api/README.md</code> and its SKSE example, or{" "}
              <code>sdk/papyrus/README.md</code> for <code>SkyUltraNative</code> functions. The current native API
              has one global request-callback slot; native consumers must coordinate it.</p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">PRISMAUI MIGRATION</span>
            <h3>Keep the shim; port the native owner</h3>
            <p>The SDK shim supplies <code>prismaUI.RegisterJSListener</code> and <code>InteropCall</code>.
              Grant <code>ui.interopCall</code> for its same-view compatibility bus. Native PrismaUI DLL calls
              still need a SkyUltra owner/handler; a frontend shim does not replace another host&apos;s ABI.</p>
            <p>Follow <code>docs/PRISMAUI_MIGRATION.md</code> for a complete native round trip with expected output,
              preserved legacy callbacks, API mappings, cleanup and troubleshooting.</p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">MAGELIGHT UI MIGRATION</span>
            <h3>Retain send/on with explicit routing</h3>
            <p>Magelight UI uses Ultralight. SkyUltra&apos;s shim retains <code>magelight.send/on</code>, close,
              audio and spatial helpers. Custom messages use the interop bus; gameplay actions should map to
              real allowlisted requests. Translate the manifest and native/Papyrus lifecycle as well.</p>
            <p>Follow <code>docs/MAGELIGHT_UI_MIGRATION.md</code> for before/after manifests, native API mappings,
              permission tables and troubleshooting. Independent pages install their shims explicitly;
              the built-in SkyUltra menu already installs them.</p>
          </article>
        </div>
        <div className="author-code-block" aria-label="Minimal SkyUltra bridge example">
          <code>const bridge = window.SkyWebView;</code>
          <code>const stop = bridge.on(&quot;myMod.status&quot;, payload =&gt; console.log(payload));</code>
          <code>await bridge.request(&quot;ui.pageReady&quot;, &#123;&#125;); // Grant ui.pageReady in the manifest.</code>
          <code>// On teardown: stop();</code>
        </div>
      </SkyFrame>

      <SkyFrame title="Credits & acknowledgments" eyebrow="THANK YOU TO THE LIBRARY AUTHORS & CONTRIBUTORS" className="guide-starter-overview">
        <div className="guide-pillars-grid">
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">BROWSER ENGINES</span>
            <h3>CEF &amp; Ultralight</h3>
            <p>Special thanks to Chromium Embedded Framework (CEF), Chromium, Ultralight and WebKit teams and
              contributors for the browser engines behind SkyUltra&apos;s web views, and to the authors of their
              rendering, networking, image, text and compression libraries.</p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">SKYRIM &amp; NATIVE RENDERING</span>
            <h3>SKSE &amp; CommonLibSSE-NG</h3>
            <p>Thank you to the SKSE, CommonLibSSE and CommonLibSSE-NG authors and contributors, Address Library
              maintainers, Microsoft DirectXTK/DirectXMath teams, Valve OpenVR contributors and Ultralight Platform
              authors. Thanks also to HDE64 and D3D12 Memory Allocator contributors.</p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">NATIVE UTILITIES</span>
            <h3>Formatting, data &amp; runtime tools</h3>
            <p>Thank you to the authors of &#123;fmt&#125;, spdlog, nlohmann/json, rapidcsv, SimpleIni, toml11,
              Xbyak, vcpkg and CMake, and the Microsoft Windows, Direct3D, Media Foundation and WIC platform teams.</p>
          </article>
          <article className="guide-pillar-card">
            <span className="guide-pillar-card__tag">FRONTEND &amp; MODDING COMMUNITY</span>
            <h3>React, Radix, Motion &amp; more</h3>
            <p>Thanks to React/React DOM, Radix UI, Motion, Lucide, Zustand, Tailwind CSS, class-variance-authority,
              clsx, tailwind-merge, TypeScript, Vite, PostCSS, Autoprefixer, DefinitelyTyped, Node.js and npm
              contributors. Thank you to PrismaUI and Magelight UI authors for their frameworks and public APIs.</p>
          </article>
        </div>
        <p>The project documentation includes upstream links, retained notices and the complete dependency inventory:
          <code>docs/THIRD_PARTY_LIBRARIES.md</code> and <code>docs/dependencies/inventory.json</code>.</p>
      </SkyFrame>
    </div>
  );
}
