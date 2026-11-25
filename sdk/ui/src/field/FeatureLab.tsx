import * as React from "react";
import { useMemo, useState } from "react";
import {
  SkyBadge, SkyButton, SkyCard, SkyCardContent, SkyCardDescription, SkyCardHeader, SkyCardTitle,
  SkyCheckbox, SkyColorWheel, SkyContextMenuContent, SkyContextMenuItem, SkyContextMenuRoot, SkyContextMenuSeparator, SkyContextMenuTrigger,
  SkyControllerPrompt, SkyDialogClose, SkyDialogContent, SkyDialogDescription, SkyDialogRoot, SkyDialogTitle, SkyDialogTrigger,
  SkyDropdownMenuContent, SkyDropdownMenuItem, SkyDropdownMenuRoot, SkyDropdownMenuSeparator, SkyDropdownMenuTrigger,
  SkyEquipmentGlyph, SkyEquipmentPanel, SkyEquipmentSlot, SkyGameTooltip, SkyHud, SkyHudStat, SkyIcon, SkyIconButton,
  SkyInput, SkyInventoryGrid, SkyLabel, SkyNotice, SkyProgress, SkyRangeSlider, SkyReveal, SkySlider, SkyStudioScrubber, SkySwitch,
  SkyTabsContent, SkyTabsList, SkyTabsRoot, SkyTabsTrigger, SkyTacticalHealthStrip, SkyTextarea, SkyThemeProvider, SkyToast,
  SkyTooltipContent, SkyTooltipProvider, SkyTooltipRoot, SkyTooltipTrigger, skyEquipmentSlotIds, skyIconNames,
  type SkyIconName, type SkyInventoryItem,
} from "../index.js";
import { SkyChoice, SkyDataList, SkyRingMeter, SkySegmentedControl, SkySparkline, SkyStatTile } from "../core.js";
import { Icon } from "./icons.js";
import { SectionTitle } from "./pieces.js";
import { palettes, type Palette } from "./model.js";

export default function FeatureLab({ theme, motionEnabled }: { theme: Palette; motionEnabled: boolean }): React.JSX.Element {
  const [quality, setQuality] = useState("balanced");
  const [meter, setMeter] = useState(76);
  const [studioFrame, setStudioFrame] = useState(14);
  const [sound, setSound] = useState(true);
  const [hints, setHints] = useState(false);
  const [toast, setToast] = useState("");
  const [selected, setSelected] = useState("sample-1");
  const [tone, setTone] = useState<"accent" | "danger" | "success" | "gold">("accent");
  const [wheelColor, setWheelColor] = useState("#68b5ff");
  const activeThemeColor = palettes.find((p) => p.value === theme)?.color ?? "#68b5ff";

  const inventory = useMemo<SkyInventoryItem[]>(() => {
    const sampleNames = ["Frostfang", "Dawnward", "Ember Tonic", "Grand Soul Gem"];
    const sampleIcons: SkyIconName[] = ["sword", "shield", "potion", "soulGem"];
    return Array.from({ length: 12 }, (_, index) => ({
      id: "sample-" + (index + 1),
      name: sampleNames[index % 4],
      detail: "COMPONENT FIXTURE · NOT GAME DATA",
      icon: <SkyIcon name={sampleIcons[index % 4]} size={28} />,
      count: index % 4 === 0 ? 3 : undefined,
      badge: index === 0 ? "NEW" : undefined,
    }));
  }, []);

  const copyComponentSnippet = (snippet: string): void => {
    if (typeof navigator !== "undefined" && navigator.clipboard?.writeText) {
      void navigator.clipboard.writeText(snippet).catch(() => undefined);
    }
    setToast(`Copied ${snippet}`);
  };

  return (
    <SkyThemeProvider theme={theme} className="workshop">
      <SkyTooltipProvider delayDuration={180}>
        <div className="workshop-grid">
          <section className="surface">
            <SectionTitle eyebrow="CORE ENTRY · REACT ONLY" title="New game primitives" />
            <div className="primitive-meter">
              <SkyRingMeter label="Interactive ring meter" value={meter} max={100} tone={tone} />
              <div>
                <SkySegmentedControl
                  label="Ring tone"
                  value={tone}
                  onValueChange={setTone}
                  options={[
                    { value: "accent", label: "Accent" },
                    { value: "danger", label: "Health" },
                    { value: "success", label: "Stamina" },
                    { value: "gold", label: "Gold" },
                  ]}
                />
                <SkySlider
                  aria-label="Ring meter value"
                  min={0}
                  max={100}
                  value={meter}
                  onChange={(event) => setMeter(Number(event.currentTarget.value))}
                />
                <p className="inspector-note">
                  Ring meter, segmented controls, stat tile, data list, and sparkline are available from the lightweight core entry.
                </p>
              </div>
            </div>
            <div className="primitive-stats">
              <SkyStatTile label="Ring value" value={meter + "%"} hint="Bound to the slider" icon={<Icon name="target" />} />
              <div>
                <SkySparkline label="Sample chart" values={[7, 6, 8, 4, 7, 9, 6, 8, 5, 6, 8, 7]} />
                <small className="fixture-label">SYNTHETIC CHART VALUES</small>
              </div>
            </div>
            <SkyDataList
              label="Core component examples"
              rows={[
                { id: "interaction", label: "Interactions", value: "Keyboard & pointer", icon: <Icon name="components" size={15} /> },
                { id: "theme", label: "Active theme", value: theme, icon: <Icon name="rune" size={15} /> },
              ]}
            />
          </section>
          <SkyCard className="workshop-card">
            <SkyCardHeader>
              <SkyCardTitle>Inputs &amp; preferences</SkyCardTitle>
              <SkyCardDescription>Local component state. Changes here do not alter Skyrim settings.</SkyCardDescription>
            </SkyCardHeader>
            <SkyCardContent className="form-stack">
              <label><SkyLabel>Profile name</SkyLabel><SkyInput defaultValue="Dovahkiin" /></label>
              <label><SkyLabel>Field notes</SkyLabel><SkyTextarea defaultValue="Prepare the same scene before each capture." /></label>
              <label>
                <SkyLabel>Quality profile</SkyLabel>
                <SkyChoice
                  label="Quality profile"
                  value={quality}
                  onValueChange={setQuality}
                  options={[
                    { value: "fast", label: "Fast" },
                    { value: "balanced", label: "Balanced" },
                    { value: "detailed", label: "Detailed" },
                  ]}
                />
              </label>
              <div className="form-toggle">
                <SkySwitch checked={sound} onCheckedChange={setSound} aria-label="Demo UI sounds" />
                <span>Demo UI sounds<small>{sound ? "Enabled" : "Muted"}</small></span>
                <SkyCheckbox id="workshop-hints" checked={hints} onChange={(event) => setHints(event.currentTarget.checked)} />
                <SkyLabel htmlFor="workshop-hints">Hints</SkyLabel>
              </div>
              <SkyButton onClick={() => setToast("Preview settings saved for this page session.")}>
                <Icon name="check" size={15} /> Save preview
              </SkyButton>
            </SkyCardContent>
          </SkyCard>
        </div>

        <section className="surface workshop-actions">
          <SectionTitle eyebrow="INTERACTION PATTERNS" title="Actions, feedback & overlays" />
          <div className="button-row">
            <SkyButton onClick={() => setToast("Primary action activated.")}>Primary action</SkyButton>
            <SkyButton variant="secondary" onClick={() => setToast("Secondary action activated.")}>Secondary action</SkyButton>
            <SkyButton variant="ghost" onClick={() => setToast("Ghost action activated.")}>Ghost</SkyButton>
            <SkyButton variant="danger" onClick={() => setToast("Demo action cancelled.")}>Cancel</SkyButton>
            <SkyTooltipRoot>
              <SkyTooltipTrigger asChild>
                <SkyIconButton label="Inspect the tooltip" icon={<Icon name="target" size={17} />} variant="secondary" />
              </SkyTooltipTrigger>
              <SkyTooltipContent>Keyboard and pointer tooltip</SkyTooltipContent>
            </SkyTooltipRoot>
            <SkyDialogRoot>
              <SkyDialogTrigger asChild><SkyButton variant="secondary">Open dialog</SkyButton></SkyDialogTrigger>
              <SkyDialogContent aria-describedby="workshop-dialog-copy">
                <SkyDialogTitle>Before you leave the camp</SkyDialogTitle>
                <SkyDialogDescription id="workshop-dialog-copy">
                  This dialog demonstrates the kit's focus handling, keyboard dismissal, and themed portal.
                </SkyDialogDescription>
                <div className="dialog-action-row">
                  <SkyDialogClose asChild><SkyButton variant="secondary">Stay here</SkyButton></SkyDialogClose>
                  <SkyDialogClose asChild><SkyButton onClick={() => setToast("Dialog confirmation received.")}>Confirm</SkyButton></SkyDialogClose>
                </div>
              </SkyDialogContent>
            </SkyDialogRoot>
            <SkyDropdownMenuRoot>
              <SkyDropdownMenuTrigger asChild>
                <SkyButton variant="secondary">More actions <Icon name="chevron" size={13} /></SkyButton>
              </SkyDropdownMenuTrigger>
              <SkyDropdownMenuContent align="end">
                <SkyDropdownMenuItem onSelect={() => setToast("Entry pinned to the demo ledger.")}>Pin entry</SkyDropdownMenuItem>
                <SkyDropdownMenuItem onSelect={() => setToast("Demo record inspected.")}>Inspect record</SkyDropdownMenuItem>
                <SkyDropdownMenuSeparator />
                <SkyDropdownMenuItem onSelect={() => setToast("Demo entry dismissed.")}>Dismiss</SkyDropdownMenuItem>
              </SkyDropdownMenuContent>
            </SkyDropdownMenuRoot>
          </div>
          <div className="badge-row">
            <SkyBadge>Common</SkyBadge>
            <SkyBadge tone="accent">Active</SkyBadge>
            <SkyBadge tone="gold">Relic</SkyBadge>
            <SkyBadge tone="success">Ready</SkyBadge>
            <SkyBadge tone="danger">Blocked</SkyBadge>
          </div>
          <SkyNotice tone={hints ? "success" : "info"}>
            {hints ? "Hints are enabled in this workshop." : "All examples below are interactive component fixtures."}
          </SkyNotice>
          <div className="workshop-progress">
            <SkyProgress label={"Preview health · " + meter + "%"} value={meter} tone="success" />
            <div className="prompt-row">
              <SkyControllerPrompt button="A">Select</SkyControllerPrompt>
              <SkyControllerPrompt button="B">Back</SkyControllerPrompt>
              <SkyControllerPrompt button="L2">Compare</SkyControllerPrompt>
            </div>
          </div>
        </section>

        <section className="surface">
          <SectionTitle eyebrow="GAME SURFACES & ICON LIBRARY" title="Inventory, equipment slots, studio bars & UI icons" />
          <SkyTabsRoot defaultValue="icons">
            <SkyTabsList aria-label="Game component examples">
              <SkyTabsTrigger value="icons">UI Icons &amp; Glyphs</SkyTabsTrigger>
              <SkyTabsTrigger value="color">Color wheel</SkyTabsTrigger>
              <SkyTabsTrigger value="inventory">Inventory grid</SkyTabsTrigger>
              <SkyTabsTrigger value="equipment">Equipment slots</SkyTabsTrigger>
              <SkyTabsTrigger value="tooltip">Item details</SkyTabsTrigger>
              <SkyTabsTrigger value="hud">Sliders &amp; Studio Bars</SkyTabsTrigger>
            </SkyTabsList>

            <SkyTabsContent value="icons">
              <div className="workshop-icons-showcase">
                <div className="workshop-icons-block">
                  <span className="eyebrow">11 ARMORY EQUIPMENT SLOT GLYPHS · CLICK TO COPY JSX</span>
                  <div className="workshop-glyph-grid">
                    {skyEquipmentSlotIds.map((slotId) => (
                      <button
                        key={slotId}
                        type="button"
                        className="workshop-icon-chip workshop-icon-chip--glyph"
                        onClick={() => copyComponentSnippet(`<SkyEquipmentGlyph slot="${slotId}" size={28} />`)}
                        title={`Copy <SkyEquipmentGlyph slot="${slotId}" />`}
                      >
                        <span className="workshop-icon-chip__well">
                          <SkyEquipmentGlyph slot={slotId} size={26} />
                        </span>
                        <code>{slotId}</code>
                      </button>
                    ))}
                  </div>
                </div>
                <div className="workshop-icons-block">
                  <span className="eyebrow">@skyultra/ui SCULPTED UI ICON SET ({skyIconNames.length}) · CLICK TO COPY JSX</span>
                  <div className="workshop-icon-grid">
                    {skyIconNames.map((iconName) => (
                      <button
                        key={iconName}
                        type="button"
                        className="workshop-icon-chip"
                        onClick={() => copyComponentSnippet(`<SkyIcon name="${iconName}" size={18} />`)}
                        title={`Copy <SkyIcon name="${iconName}" />`}
                      >
                        <span className="workshop-icon-chip__well">
                          <SkyIcon name={iconName} size={18} />
                        </span>
                        <code>{iconName}</code>
                      </button>
                    ))}
                  </div>
                </div>
              </div>
            </SkyTabsContent>

            <SkyTabsContent value="color">
              <div className="workbench-color-sandbox">
                <SkyColorWheel
                  mode="inline"
                  size={162}
                  label="Inline 360° Wheel + Inner Brightness Disc"
                  value={wheelColor}
                  allowThemeMatch
                  isThemeMatched={wheelColor.toLowerCase() === activeThemeColor.toLowerCase()}
                  themeColor={activeThemeColor}
                  onResetTheme={() => setWheelColor(activeThemeColor)}
                  onChange={setWheelColor}
                />
                <div className="workbench-color-sandbox__meta">
                  <span className="eyebrow">MODDER API · INLINE &amp; COMPACT POPOVER MODES</span>
                  <p className="inspector-note">
                    Exported from <code>@skyultra/ui</code> as <code>&lt;SkyColorWheel /&gt;</code> with outer 360° Hue ring,
                    concentric Brightness &amp; Saturation canvas wheel, keyboard-scoped popover dismissal, and hex input.
                  </p>
                  <div style={{ display: "flex", flexWrap: "wrap", alignItems: "center", gap: "10px" }}>
                    <SkyColorWheel
                      mode="popover"
                      size={146}
                      label="Popover Accent"
                      value={wheelColor}
                      onChange={setWheelColor}
                    />
                    <SkyButton
                      variant="secondary"
                      size="sm"
                      onClick={() =>
                        copyComponentSnippet(
                          `<SkyColorWheel mode="inline" value="${wheelColor}" onChange={setColor} />`,
                        )
                      }
                    >
                      Copy JSX Snippet
                    </SkyButton>
                  </div>
                </div>
              </div>
            </SkyTabsContent>

            <SkyTabsContent value="inventory">
              <SkyContextMenuRoot>
                <SkyContextMenuTrigger asChild>
                  <div className="context-demo" tabIndex={0}>
                    <Icon name="target" size={15} /> Right click here or use your keyboard menu key.
                  </div>
                </SkyContextMenuTrigger>
                <SkyContextMenuContent>
                  <SkyContextMenuItem onSelect={() => setSelected("sample-1")}>Select first item</SkyContextMenuItem>
                  <SkyContextMenuItem onSelect={() => setToast("Item marked as a demo favorite.")}>Favorite entry</SkyContextMenuItem>
                  <SkyContextMenuSeparator />
                  <SkyContextMenuItem onSelect={() => setToast("Context action received.")}>Inspect</SkyContextMenuItem>
                </SkyContextMenuContent>
              </SkyContextMenuRoot>
              <SkyInventoryGrid items={inventory} selectedId={selected} onSelect={(item) => setSelected(item.id)} />
            </SkyTabsContent>

            <SkyTabsContent value="equipment">
              <div className="workshop-equipment-showcase">
                <SkyEquipmentPanel className="workshop-equipment workshop-equipment--plates">
                  <SkyEquipmentSlot variant="plate" slotKind="head" slot="Head" />
                  <SkyEquipmentSlot variant="plate" slotKind="body" slot="Body" item="Nordic Carved Cuirass" enchanted onClick={() => setToast("Selected Body slot")} />
                  <SkyEquipmentSlot variant="plate" slotKind="mainHand" slot="Right hand" item="Dawnbreaker" enchanted onClick={() => setToast("Selected Right hand slot")} />
                  <SkyEquipmentSlot variant="plate" slotKind="offHand" slot="Left hand" item="Spellbreaker" enchanted onClick={() => setToast("Selected Left hand slot")} />
                  <SkyEquipmentSlot variant="plate" slotKind="cape" slot="Cape" bipedSlot="46" item="Traveler Fur Cloak" onClick={() => setToast("Selected Cape slot (46)")} />
                  <SkyEquipmentSlot variant="plate" slotKind="lantern" slot="Lantern" bipedSlot="49" item="Wearable Iron Lantern" enchanted onClick={() => setToast("Selected Lantern slot (49)")} />
                </SkyEquipmentPanel>
                <SkyEquipmentPanel className="workshop-equipment">
                  <SkyEquipmentSlot slotKind="neck" slot="Neck" item="Amulet of Talos" enchanted subtitle="Shouts recharge 20% faster" />
                  <SkyEquipmentSlot slotKind="ammo" slot="Ammo" item="Dragonbone Arrow" subtitle="24 DMG · 120 carried" />
                </SkyEquipmentPanel>
              </div>
            </SkyTabsContent>

            <SkyTabsContent value="tooltip">
              <SkyGameTooltip
                title="Frostfang, the Pale Edge"
                subtitle="One-handed sword · 12 weight"
                rarity="Relic"
                description="An ancient blade that draws a cold line through the air."
                footer="VALUE 1,250 · WEIGHT 12 · DAMAGE 36"
              />
            </SkyTabsContent>

            <SkyTabsContent value="hud">
              <div style={{ display: "grid", gap: "14px" }}>
                <SkyHud label="Fixture HUD">
                  <SkyHudStat icon={<Icon name="heart" size={16} />} label="Health" value={`${meter * 5} / 500`} tone="danger" />
                  <SkyHudStat icon={<Icon name="rune" size={16} />} label="Magicka" value="92 / 140" />
                  <SkyHudStat icon={<Icon name="shield" size={16} />} label="Armor" value="62" tone="gold" />
                </SkyHud>
                <SkyTacticalHealthStrip current={meter * 5} max={500} label="HP" />
                <div style={{ display: "grid", gap: "10px", gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))" }}>
                  <SkyRangeSlider
                    label="HUD Instrument Scale"
                    min={70}
                    max={150}
                    step={5}
                    unit="%"
                    value={70 + Math.round((meter / 100) * 80)}
                    onValueChange={(val) => setMeter(Math.round(((val - 70) / 80) * 100))}
                  />
                  <div className="sky-range">
                    <div className="sky-range__heading">
                      <label htmlFor="lab-direct-slider">Lightweight Direct Slider</label>
                      <output htmlFor="lab-direct-slider">{meter}%</output>
                    </div>
                    <SkySlider
                      id="lab-direct-slider"
                      aria-label="Lightweight direct slider"
                      min={0}
                      max={100}
                      value={meter}
                      onChange={(e) => setMeter(Number(e.currentTarget.value))}
                    />
                  </div>
                </div>
                <SkyStudioScrubber
                  value={studioFrame}
                  min={0}
                  max={32}
                  label="Studio frame fixture"
                  meta={`mt_idle · ${(studioFrame * 0.06).toFixed(2)}s`}
                  onChange={setStudioFrame}
                />
                <div style={{ display: "flex", flexWrap: "wrap", gap: "8px" }}>
                  <SkyButton
                    variant="secondary"
                    size="sm"
                    onClick={() =>
                      copyComponentSnippet(
                        `<SkyRangeSlider label="HUD Scale" min={70} max={150} step={5} unit="%" value={scale} onValueChange={setScale} />`,
                      )
                    }
                  >
                    Copy &lt;SkyRangeSlider /&gt; JSX
                  </SkyButton>
                  <SkyButton
                    variant="secondary"
                    size="sm"
                    onClick={() =>
                      copyComponentSnippet(`<SkySlider min={0} max={100} value={value} onChange={handleChange} />`)
                    }
                  >
                    Copy &lt;SkySlider /&gt; JSX
                  </SkyButton>
                </div>
              </div>
            </SkyTabsContent>
          </SkyTabsRoot>
        </section>

        <SkyReveal initial={motionEnabled ? { opacity: 0, y: 6 } : false} transition={{ duration: .16 }}>
          <p className="fixture-label">WORKSHOP FIXTURES · EXPORTED FROM @skyultra/ui FOR ALL WEB MODS · LIVE GAME DATA IN INVENTORY &amp; OBSERVATORY.</p>
        </SkyReveal>
        {toast && (
          <div className="workshop-toast">
            <SkyToast tone="success" onDismiss={() => setToast("")}>{toast}</SkyToast>
          </div>
        )}
      </SkyTooltipProvider>
    </SkyThemeProvider>
  );
}