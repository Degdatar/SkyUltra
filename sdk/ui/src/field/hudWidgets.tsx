import * as React from "react";
import {
  getSkyWebView,
  type SkyUltraCompassMarker,
  type SkyUltraCompassMarkerKind,
  type SkyUltraGameTopic,
  type SkyUltraHudBindingPath,
  type SkyUltraVisibilityRule,
  type SkyUltraWidgetAnchor,
  type SkyUltraWidgetFontStyle,
  type SkyUltraWidgetPlacement,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import { skyMeterFraction } from "../widgets.js";
import { Icon, itemIcon, SkillIcon, type IconName } from "./icons.js";
import { actorThumbnailQueue } from "./itemThumbnails.js";
import type { Npc, Player } from "./model.js";
import { PlayerPortrait } from "./PlayerPortrait.js";

export type WidgetKind =
  | "vitals"
  | "compass"
  | "radar"
  | "stealth"
  | "loot"
  | "wards"
  | "clock"
  | "character"
  | "telemetry"
  | "target"
  | "skills"
  | "armory"
  | "equipment"
  | "resources"
  | "effects";

export interface WidgetCatalogEntry {
  id: WidgetKind;
  title: string;
  detail: string;
  category: "Combat" | "Navigation" | "Character" | "System";
  defaultColor: string;
  defaultAnchor: SkyUltraWidgetAnchor;
  icon: IconName;
}

export const widgetCatalog: WidgetCatalogEntry[] = [
  {
    id: "vitals",
    title: "Tri-Vitality Radial Rings",
    detail: "Overview-style circular Health, Magicka & Stamina radial dials with 36-tick precision arcs",
    category: "Combat",
    defaultColor: "#ff5c6c",
    defaultAnchor: "bottom-center",
    icon: "heart",
  },
  {
    id: "compass",
    title: "Sovereign Compass & POI Bar",
    detail: "Ornate Skyrim compass HUD replacement with custom icons for Homes, Inns, Castles, Blacksmiths, Vendors & Quest Markers",
    category: "Navigation",
    defaultColor: "#f0c36d",
    defaultAnchor: "top-center",
    icon: "compass",
  },
  {
    id: "character",
    title: "Sovereign Portrait Crest",
    detail: "Live 3D player face portrait, Dragonborn level badge, hold & vitality pulse",
    category: "Character",
    defaultColor: "#e8b059",
    defaultAnchor: "top-left",
    icon: "shield",
  },
  {
    id: "radar",
    title: "Tactical Proximity Radar",
    detail: "Live sweeping proximity ring & distance beacons for nearby actors",
    category: "Navigation",
    defaultColor: "#39ff14",
    defaultAnchor: "top-right",
    icon: "actors",
  },
  {
    id: "target",
    title: "Nearest Actor & Companion Scanner",
    detail: "Live lock-on readout of the closest NPC or follower with distance, level & vitality bar",
    category: "Combat",
    defaultColor: "#ff5c8a",
    defaultAnchor: "top-center",
    icon: "target",
  },
  {
    id: "stealth",
    title: "Combat & Loadout Strip",
    detail: "Floating main/off-hand weapons, ammo count & armor cap mitigation %",
    category: "Combat",
    defaultColor: "#ffb95c",
    defaultAnchor: "bottom-left",
    icon: "sword",
  },
  {
    id: "armory",
    title: "Equipped Armory & Ward Matrix",
    detail: "11-slot gear occupancy, 80% armor cap progress, Effective Health (EHP) & ward status",
    category: "Combat",
    defaultColor: "#38bdf8",
    defaultAnchor: "bottom-left",
    icon: "shield",
  },
  {
    id: "skills",
    title: "Signature Skills & Mastery Tracker",
    detail: "Top 3 highest character skills with mastery tier badges and constellation bars",
    category: "Character",
    defaultColor: "#a78bfa",
    defaultAnchor: "top-left",
    icon: "rune",
  },
  {
    id: "loot",
    title: "Encumbrance & Treasury Ticker",
    detail: "Remaining carry capacity gauge, overburden threshold & Septim counter",
    category: "Character",
    defaultColor: "#dfa84e",
    defaultAnchor: "bottom-right",
    icon: "inventory",
  },
  {
    id: "wards",
    title: "Elemental Wards & Buff Timers",
    detail: "Floating Fire/Frost/Shock/Magic resistance badges & active spell timers",
    category: "Combat",
    defaultColor: "#be8cff",
    defaultAnchor: "bottom-right",
    icon: "rune",
  },
  {
    id: "clock",
    title: "Tamrielic Sundial & Chronicle",
    detail: "Celestial phase arc, live 24h time & 4th Era calendar stamp",
    category: "Navigation",
    defaultColor: "#52e6d1",
    defaultAnchor: "top-right",
    icon: "clock",
  },
  {
    id: "telemetry",
    title: "Engine FPS & Frame-Time HUD",
    detail: "Live floating frame rate, ms pacing bar & Ultralight bridge telemetry",
    category: "System",
    defaultColor: "#ff2e93",
    defaultAnchor: "top-right",
    icon: "performance",
  },
];

export const hudColorSwatches: Array<{ label: string; hex: string }> = [
  { label: "Dragonstone Gold", hex: "#e8b059" },
  { label: "Cyber Cyan", hex: "#00f0ff" },
  { label: "Synth Magenta", hex: "#ff2e93" },
  { label: "Venom Green", hex: "#39ff14" },
  { label: "Crimson Vitality", hex: "#ff5c6c" },
  { label: "Plasma Violet", hex: "#a78bfa" },
  { label: "Aurora Teal", hex: "#52e6d1" },
  { label: "Frost Blue", hex: "#38bdf8" },
  { label: "Sunspire Amber", hex: "#ffb95c" },
  { label: "Pure Ivory", hex: "#f4efe6" },
];

export const hudFontOptions: Array<{
  value: SkyUltraWidgetFontStyle;
  label: string;
  tag: string;
  sample: string;
  description: string;
}> = [
  {
    value: "theme",
    label: "Theme Native",
    tag: "DYNAMIC",
    sample: "Aa 128",
    description: "Inherits typography from the active HUD theme",
  },
  {
    value: "serif",
    label: "Imperial Serif",
    tag: "ENGRAVED",
    sample: "IV · 278",
    description: "Sculpted elder serif display with crisp numerals",
  },
  {
    value: "display",
    label: "Sovereign Display",
    tag: "TACTICAL",
    sample: "HDG 045°",
    description: "High-contrast condensed tactical command lettering",
  },
  {
    value: "data",
    label: "Geometric Sans",
    tag: "PRECISION",
    sample: "98,690 G",
    description: "Ultra-legible modern neo-grotesque UI telemetry",
  },
  {
    value: "mono",
    label: "Cyber Monospace",
    tag: "TELEMETRY",
    sample: "16.7 ms",
    description: "Fixed-width tabular instrumentation & frame counters",
  },
];

export const hudAnchorOptions: Array<{
  value: SkyUltraWidgetAnchor;
  label: string;
  shortLabel: string;
  sub: string;
}> = [
  { value: "top-left", label: "Top Left", shortLabel: "TOP-LEFT", sub: "NW Corner" },
  { value: "top-center", label: "Top Center", shortLabel: "TOP-CENTER", sub: "Upper Horizon" },
  { value: "top-right", label: "Top Right", shortLabel: "TOP-RIGHT", sub: "NE Corner" },
  { value: "bottom-left", label: "Bottom Left", shortLabel: "BOTTOM-LEFT", sub: "SW Corner" },
  { value: "bottom-center", label: "Bottom Center", shortLabel: "BOTTOM-CENTER", sub: "Lower Arc" },
  { value: "bottom-right", label: "Bottom Right", shortLabel: "BOTTOM-RIGHT", sub: "SE Corner" },
];

export interface ResolvedWidgetPlacement {
  anchor: SkyUltraWidgetAnchor;
  scale: number;
  opacity: number;
  glow: number;
  color: string;
  fontStyle: SkyUltraWidgetFontStyle;
  offsetX: number;
  offsetY: number;
  visibilityRule: SkyUltraVisibilityRule;
  width?: number;
  customLabel?: string;
  bindingPath?: SkyUltraHudBindingPath;
}

export const hudBindingOptions: Array<{
  path: SkyUltraHudBindingPath;
  label: string;
  topic: SkyUltraGameTopic;
  unit: string;
}> = [
  { path: "health.current", label: "Player Health (Current)", topic: "vitals", unit: "HP" },
  { path: "magicka.current", label: "Player Magicka (Current)", topic: "vitals", unit: "MP" },
  { path: "stamina.current", label: "Player Stamina (Current)", topic: "vitals", unit: "SP" },
  { path: "armorRating", label: "Total Armor Rating", topic: "equipment", unit: "AR" },
  { path: "gold", label: "Septim Treasury", topic: "inventory", unit: "GOLD" },
  { path: "carryWeight.current", label: "Carry Weight (Used)", topic: "inventory", unit: "WT" },
  { path: "level", label: "Character Level", topic: "vitals", unit: "LVL" },
  { path: "location", label: "Current Hold / Cell", topic: "vitals", unit: "CELL" },
  { path: "activeEffectCount", label: "Active Magic Count", topic: "effects", unit: "BUFFS" },
  { path: "questCount", label: "Active Quest Count", topic: "quests", unit: "QUESTS" },
  { path: "followerCount", label: "Active Followers", topic: "followers", unit: "PARTY" },
];

export const hudVisibilityOptions: Array<{
  value: SkyUltraVisibilityRule;
  label: string;
  detail: string;
}> = [
  { value: "always", label: "Always Visible", detail: "Renders whenever gameplay HUD is active" },
  { value: "combat-low-hp", label: "Health < 65%", detail: "Appears dynamically when player vitality drops" },
  { value: "encumbered", label: "Carry Weight ≥ 75%", detail: "Appears when approaching encumbrance cap" },
  { value: "has-effects", label: "Active Effects > 0", detail: "Appears when buffs or afflictions are active" },
  { value: "has-followers", label: "Has Followers", detail: "Appears when companions are in your party" },
];

export function resolveHudBindingValue(player: Player | null, path?: SkyUltraHudBindingPath): string | null {
  if (!player || !path) return null;
  switch (path) {
    case "health.current":
      return `${Math.round(player.health.current).toLocaleString()} / ${Math.round(player.health.max).toLocaleString()} HP`;
    case "magicka.current":
      return `${Math.round(player.magicka.current).toLocaleString()} / ${Math.round(player.magicka.max).toLocaleString()} MP`;
    case "stamina.current":
      return `${Math.round(player.stamina.current).toLocaleString()} / ${Math.round(player.stamina.max).toLocaleString()} SP`;
    case "armorRating":
      return `${Math.round(player.armorRating).toLocaleString()} AR`;
    case "gold":
      return `${Math.round(player.gold).toLocaleString()} G`;
    case "carryWeight.current":
      return `${Math.round(player.carryWeight?.current ?? 0).toLocaleString()} / ${Math.round(player.carryWeight?.max ?? 300).toLocaleString()} WT`;
    case "level":
      return `LVL ${player.level}`;
    case "location":
      return player.location || "Skyrim";
    case "activeEffectCount":
      return `${player.activeEffectCount ?? player.activeEffects?.length ?? 0} Effects`;
    case "questCount":
      return `${player.questCount ?? player.quests?.length ?? 0} Quests`;
    case "followerCount":
      return `${player.followerCount ?? player.followers?.length ?? 0} Followers`;
    default:
      return null;
  }
}

export function evaluateHudVisibilityRule(player: Player | null, rule?: SkyUltraVisibilityRule): boolean {
  if (!rule || rule === "always" || !player) return true;
  if (rule === "combat-low-hp") {
    const max = Math.max(1, player.health.max || 100);
    return player.health.current / max < 0.65;
  }
  if (rule === "encumbered") {
    const max = Math.max(1, player.carryWeight?.max || 300);
    return (player.carryWeight?.current ?? 0) / max >= 0.75;
  }
  if (rule === "has-effects") {
    return (player.activeEffectCount ?? player.activeEffects?.length ?? 0) > 0;
  }
  if (rule === "has-followers") {
    return (player.followerCount ?? player.followers?.length ?? 0) > 0;
  }
  return true;
}

export function defaultWidgetPlacements(): Record<WidgetKind, ResolvedWidgetPlacement> {
  const base: Partial<Record<WidgetKind, ResolvedWidgetPlacement>> = {};
  for (const entry of widgetCatalog) {
    base[entry.id] = {
      anchor: entry.defaultAnchor,
      scale: 1,
      opacity: 0.96,
      glow: 100,
      color: entry.defaultColor,
      fontStyle: "theme",
      offsetX: 0,
      offsetY: 0,
      visibilityRule: "always",
    };
  }
  base.equipment = {
    anchor: "bottom-left",
    scale: 1,
    opacity: 0.96,
    glow: 100,
    color: "#ffb95c",
    fontStyle: "theme",
    offsetX: 0,
    offsetY: 0,
    visibilityRule: "always",
  };
  base.resources = {
    anchor: "bottom-right",
    scale: 1,
    opacity: 0.96,
    glow: 100,
    color: "#dfa84e",
    fontStyle: "theme",
    offsetX: 0,
    offsetY: 0,
    visibilityRule: "always",
  };
  base.effects = {
    anchor: "bottom-right",
    scale: 1,
    opacity: 0.96,
    glow: 100,
    color: "#be8cff",
    fontStyle: "theme",
    offsetX: 0,
    offsetY: 0,
    visibilityRule: "always",
  };
  return base as Record<WidgetKind, ResolvedWidgetPlacement>;
}

export function encodePlacementsParam(
  widgets: WidgetKind[],
  placements: Partial<Record<WidgetKind, SkyUltraWidgetPlacement>>,
): string {
  const defaults = defaultWidgetPlacements();
  return widgets
    .map((kind) => {
      const def = defaults[kind] ?? defaults.vitals;
      const p = placements[kind];
      const anchor = p?.anchor ?? def.anchor;
      const scale = Math.round((p?.scale ?? def.scale) * 100);
      const opacity = Math.round((p?.opacity ?? def.opacity) * 100);
      const glow = Math.round(p?.glow ?? def.glow);
      const color = (p?.color ?? def.color).replace(/^#/, "");
      const font = p?.fontStyle ?? def.fontStyle;
      const ox = Math.max(-600, Math.min(600, Math.round(p?.offsetX ?? 0)));
      const oy = Math.max(-400, Math.min(400, Math.round(p?.offsetY ?? 0)));
      const vis = p?.visibilityRule ?? "always";
      const bind = p?.bindingPath ?? "";
      return `${kind}:${anchor}:${scale}:${opacity}:${glow}:${color}:${font}:${ox}:${oy}:${vis}:${bind}`;
    })
    .join(";");
}

export function decodePlacementsParam(
  raw: string | null | undefined,
  fallbackAnchor?: SkyUltraWidgetAnchor,
  fallbackScale?: number,
  fallbackOpacity?: number,
  fallbackGlow?: number,
  fallbackColors?: Partial<Record<WidgetKind, string>>,
): Record<WidgetKind, ResolvedWidgetPlacement> {
  const map = defaultWidgetPlacements();
  if (fallbackAnchor || fallbackScale || fallbackOpacity || fallbackGlow || fallbackColors) {
    for (const key of Object.keys(map) as WidgetKind[]) {
      if (fallbackAnchor && key !== "compass") map[key].anchor = fallbackAnchor;
      if (typeof fallbackScale === "number" && Number.isFinite(fallbackScale)) map[key].scale = fallbackScale;
      if (typeof fallbackOpacity === "number" && Number.isFinite(fallbackOpacity)) map[key].opacity = fallbackOpacity;
      if (typeof fallbackGlow === "number" && Number.isFinite(fallbackGlow)) map[key].glow = fallbackGlow;
      if (fallbackColors?.[key]) map[key].color = fallbackColors[key]!;
    }
  }
  if (!raw) return map;
  const validAnchors = new Set<string>(hudAnchorOptions.map((a) => a.value));
  const validFonts = new Set<string>(hudFontOptions.map((f) => f.value));
  const validVis = new Set<string>(hudVisibilityOptions.map((v) => v.value));
  const validBind = new Set<string>(hudBindingOptions.map((b) => b.path));
  for (const token of raw.split(/[;,]+/)) {
    if (!token) continue;
    const [kindRaw, anchorRaw, scaleRaw, opacityRaw, glowRaw, colorRaw, fontRaw, oxRaw, oyRaw, visRaw, bindRaw] =
      token.split(":");
    const kind = kindRaw as WidgetKind;
    if (!kind || !map[kind]) continue;
    if (anchorRaw && validAnchors.has(anchorRaw)) {
      map[kind].anchor = anchorRaw as SkyUltraWidgetAnchor;
    }
    const scaleNum = Number(scaleRaw);
    if (Number.isFinite(scaleNum) && scaleNum >= 50 && scaleNum <= 200) {
      map[kind].scale = Number((scaleNum / 100).toFixed(2));
    }
    const opacityNum = Number(opacityRaw);
    if (Number.isFinite(opacityNum) && opacityNum >= 20 && opacityNum <= 100) {
      map[kind].opacity = Number((opacityNum / 100).toFixed(2));
    }
    const glowNum = Number(glowRaw);
    if (Number.isFinite(glowNum) && glowNum >= 0 && glowNum <= 200) {
      map[kind].glow = Math.round(glowNum);
    }
    if (colorRaw && /^[0-9a-f]{3,6}$/i.test(colorRaw)) {
      map[kind].color = `#${colorRaw}`;
    }
    if (fontRaw && validFonts.has(fontRaw)) {
      map[kind].fontStyle = fontRaw as SkyUltraWidgetFontStyle;
    }
    const oxNum = Number(oxRaw);
    if (oxRaw !== undefined && oxRaw !== "" && Number.isFinite(oxNum)) {
      map[kind].offsetX = Math.max(-600, Math.min(600, Math.round(oxNum)));
    }
    const oyNum = Number(oyRaw);
    if (oyRaw !== undefined && oyRaw !== "" && Number.isFinite(oyNum)) {
      map[kind].offsetY = Math.max(-400, Math.min(400, Math.round(oyNum)));
    }
    if (visRaw && validVis.has(visRaw)) {
      map[kind].visibilityRule = visRaw as SkyUltraVisibilityRule;
    }
    if (bindRaw && validBind.has(bindRaw)) {
      map[kind].bindingPath = bindRaw as SkyUltraHudBindingPath;
    }
  }
  return map;
}

export interface HudWidgetProps {
  kind: WidgetKind;
  player: Player | null;
  bridge?: SkyWebViewAPI;
  nearbyActors?: Npc[];
  fps?: number;
  accentColor?: string;
  glow?: number;
  fontStyle?: SkyUltraWidgetFontStyle;
  customLabel?: string;
  bindingPath?: SkyUltraHudBindingPath;
  variant?: "aether" | "plain";
}

export function HudWidget({
  kind,
  player,
  bridge,
  nearbyActors,
  fps,
  accentColor,
  glow = 100,
  fontStyle = "theme",
  customLabel,
  bindingPath,
}: HudWidgetProps): React.JSX.Element {
  const entry = widgetCatalog.find((widget) => widget.id === kind);
  const resolvedColor = accentColor || entry?.defaultColor || "#e8b059";
  const glowFactor = Math.max(0, Math.min(1.5, glow / 100));
  const boundText = resolveHudBindingValue(player, bindingPath);
  const style = {
    "--hud-accent": resolvedColor,
    "--hud-glow-alpha": (0.42 * glowFactor).toFixed(2),
    "--hud-glow-spread": `${Math.round(14 * glowFactor)}px`,
  } as React.CSSProperties;

  return (
    <div
      className="sky-floating-hud"
      data-hud-kind={kind}
      data-hud-font={fontStyle}
      style={style}
    >
      {(customLabel || boundText) && (
        <div className="sky-floating-hud__binding-ribbon">
          {customLabel && <span className="sky-floating-hud__custom-label">{customLabel}</span>}
          {boundText && <strong className="sky-floating-hud__bound-val">{boundText}</strong>}
        </div>
      )}
      <WidgetContent
        kind={kind}
        player={player}
        bridge={bridge}
        nearbyActors={nearbyActors}
        fps={fps}
        accentColor={resolvedColor}
      />
    </div>
  );
}

function fmt(n: number | undefined): string {
  return typeof n === "number" && Number.isFinite(n) ? Math.round(n).toLocaleString() : "0";
}

function masteryTier(level: number): string {
  if (level >= 100) return "LEGENDARY";
  if (level >= 90) return "MASTER";
  if (level >= 75) return "EXPERT";
  if (level >= 50) return "ADEPT";
  if (level >= 25) return "APPRENTICE";
  return "NOVICE";
}

function shortestCompassDelta(targetDeg: number, headingDeg: number): number {
  let diff = ((targetDeg - headingDeg + 540) % 360) - 180;
  if (diff < -180) diff += 360;
  return diff;
}

function normalizeCompassDeg(deg: number): number {
  return ((deg % 360) + 360) % 360;
}

const COMPASS_HALF_FOV = 95;
const COMPASS_SPAN_PCT = 44.5;
const COMPASS_EDGE_LEFT_PCT = 3.6;
const COMPASS_EDGE_RIGHT_PCT = 96.4;
const COMPASS_LOCAL_RADIUS_M = 50;

function compassIsPinned(kind: SkyUltraCompassMarkerKind): boolean {
  return kind === "quest" || kind === "questDoor" || kind === "customMarker" || kind === "follower";
}

interface CompassTrack {
  angle: number;
  targetAngle: number;
  dist: number;
  targetDist: number;
  x: number;
  placed: boolean;
}

interface CompassElementState {
  transform: string;
  shown: boolean;
  edge: string;
  dist: string;
  opacity: string;
}

interface CompassAnimator {
  focusHeading: number;
  trackElRef: React.RefObject<HTMLDivElement | null>;
  hdgNumRef: React.RefObject<HTMLSpanElement | null>;
  hdgCardRef: React.RefObject<HTMLElement | null>;
  registerMarker: (id: string) => (el: HTMLDivElement | null) => void;
  registerTick: (index: number) => (el: HTMLSpanElement | null) => void;
}

/**
 * Drives the compass at the display refresh rate by writing GPU-friendly
 * translate3d transforms straight to the DOM. React only re-renders when new
 * game data arrives (and ~8x/sec for the focus callout), so the strip glides
 * smoothly while walking/turning and never steals CPU from other SkyUltra views.
 */
function useCompassAnimator(rawHeading: number, markers: SkyUltraCompassMarker[]): CompassAnimator {
  const targetHeading = Number.isFinite(rawHeading) ? normalizeCompassDeg(rawHeading) : 0;

  const headingRef = React.useRef<number>(targetHeading);
  const headingPacketRef = React.useRef<{ value: number; ms: number }>({ value: targetHeading, ms: performance.now() });
  const headingVelRef = React.useRef<number>(0);
  const tracksRef = React.useRef<Map<string, CompassTrack>>(new Map());
  const markersRef = React.useRef<SkyUltraCompassMarker[]>(markers);
  markersRef.current = markers;

  const markerEls = React.useRef<Map<string, HTMLDivElement>>(new Map());
  const markerCallbacks = React.useRef<Map<string, (el: HTMLDivElement | null) => void>>(new Map());
  const tickEls = React.useRef<Array<HTMLSpanElement | null>>([]);
  const tickCallbacks = React.useRef<Array<(el: HTMLSpanElement | null) => void>>([]);
  const elementState = React.useRef<WeakMap<HTMLElement, CompassElementState>>(new WeakMap());
  const trackElRef = React.useRef<HTMLDivElement>(null);
  const hdgNumRef = React.useRef<HTMLSpanElement>(null);
  const hdgCardRef = React.useRef<HTMLElement>(null);
  const widthRef = React.useRef<number>(0);
  const frameCountRef = React.useRef<number>(0);
  const hdgTextRef = React.useRef<string>("");

  const [focusHeading, setFocusHeading] = React.useState<number>(targetHeading);
  const focusRef = React.useRef<{ value: number; ms: number }>({ value: targetHeading, ms: 0 });

  const registerMarker = React.useCallback((id: string) => {
    let cb = markerCallbacks.current.get(id);
    if (!cb) {
      cb = (el: HTMLDivElement | null) => {
        if (el) markerEls.current.set(id, el);
        else {
          markerEls.current.delete(id);
          markerCallbacks.current.delete(id);
        }
      };
      markerCallbacks.current.set(id, cb);
    }
    return cb;
  }, []);

  const registerTick = React.useCallback((index: number) => {
    let cb = tickCallbacks.current[index];
    if (!cb) {
      cb = (el: HTMLSpanElement | null) => {
        tickEls.current[index] = el;
      };
      tickCallbacks.current[index] = cb;
    }
    return cb;
  }, []);

  const stateFor = (el: HTMLElement): CompassElementState => {
    let state = elementState.current.get(el);
    if (!state) {
      state = { transform: "", shown: false, edge: "", dist: "", opacity: "" };
      elementState.current.set(el, state);
    }
    return state;
  };

  const frameRef = React.useRef<((now: number, dt: number) => void) | null>(null);
  if (frameRef.current === null) {
    frameRef.current = (now: number, dt: number): void => {
      // 1. Heading: follow the latest packet extrapolated by measured turn rate.
      const packet = headingPacketRef.current;
      const sincePacket = Math.max(0, (now - packet.ms) / 1000);
      const predicted = normalizeCompassDeg(packet.value + headingVelRef.current * Math.min(sincePacket, 0.07));
      const current = headingRef.current;
      const headingDelta = shortestCompassDelta(predicted, current);
      if (dt <= 0) {
        // layout pass: keep current value
      } else if (Math.abs(headingDelta) > 100) {
        headingRef.current = predicted;
      } else {
        headingRef.current = normalizeCompassDeg(current + headingDelta * (1 - Math.exp(-dt * 20)));
      }
      const heading = headingRef.current;

      // Heading readout.
      const hdgText = String(Math.round(heading) % 360).padStart(3, "0");
      if (hdgText !== hdgTextRef.current) {
        hdgTextRef.current = hdgText;
        if (hdgNumRef.current) hdgNumRef.current.textContent = `${hdgText}°`;
        if (hdgCardRef.current) hdgCardRef.current.textContent = cardinalLabelForHeading(heading);
      }

      // Throttled React refresh for focus callout / floating names.
      const focus = focusRef.current;
      if (dt > 0 && now - focus.ms > 120 && Math.abs(shortestCompassDelta(heading, focus.value)) > 1.5) {
        focusRef.current = { value: heading, ms: now };
        setFocusHeading(heading);
      }

      // 2. Marker bearings & distances glide toward their latest values.
      const angleBlend = dt > 0 ? 1 - Math.exp(-dt * 14) : 0;
      const distBlend = dt > 0 ? 1 - Math.exp(-dt * 10) : 0;
      for (const track of tracksRef.current.values()) {
        const dA = shortestCompassDelta(track.targetAngle, track.angle);
        track.angle = Math.abs(dA) > 60 ? track.targetAngle : normalizeCompassDeg(track.angle + dA * angleBlend);
        const dD = track.targetDist - track.dist;
        track.dist = Math.abs(dD) > 20 ? track.targetDist : track.dist + dD * distBlend;
      }

      // 3. Project + continuous soft repulsion (no threshold snapping).
      const projected: Array<{ id: string; track: CompassTrack; pct: number; edge: string; shown: boolean }> = [];
      for (const marker of markersRef.current) {
        const track = tracksRef.current.get(marker.id);
        if (!track) continue;
        const pinned = compassIsPinned(marker.kind);
        const delta = shortestCompassDelta(track.angle, heading);
        const inFov = Math.abs(delta) <= COMPASS_HALF_FOV;
        const shown = (pinned || track.dist <= COMPASS_LOCAL_RADIUS_M) && (inFov || pinned);
        const edge = inFov ? "" : delta < 0 ? "left" : "right";
        const pct = inFov
          ? 50 + (delta / COMPASS_HALF_FOV) * COMPASS_SPAN_PCT
          : delta < 0
            ? COMPASS_EDGE_LEFT_PCT
            : COMPASS_EDGE_RIGHT_PCT;
        projected.push({ id: marker.id, track, pct, edge, shown });
      }
      const inView = projected.filter((p) => p.shown && p.edge === "").sort((a, b) => a.pct - b.pct);
      const softRadius = 4.2;
      for (let iter = 0; iter < 2; iter++) {
        for (let i = 1; i < inView.length; i++) {
          const left = inView[i - 1];
          const right = inView[i];
          const gap = right.pct - left.pct;
          if (gap < softRadius) {
            const t = 1 - Math.max(0, gap) / softRadius;
            const push = t * t * 1.05;
            left.pct = Math.max(6.5, left.pct - push);
            right.pct = Math.min(93.5, right.pct + push);
          }
        }
      }

      // 4. Width cache (re-read rarely to avoid layout thrash).
      frameCountRef.current = (frameCountRef.current + 1) % 30;
      if (widthRef.current <= 0 || frameCountRef.current === 0) {
        const w = trackElRef.current?.clientWidth ?? 0;
        if (w > 0) widthRef.current = w;
      }
      const width = widthRef.current;
      if (width <= 0) return;

      const xBlend = dt > 0 ? 1 - Math.exp(-dt * 26) : 0;
      for (const p of projected) {
        const track = p.track;
        if (!track.placed || Math.abs(p.pct - track.x) > 30) {
          track.x = p.pct;
          track.placed = true;
        } else {
          track.x += (p.pct - track.x) * xBlend;
        }
        const el = markerEls.current.get(p.id);
        if (!el) continue;
        const state = stateFor(el);
        if (!p.shown) {
          if (state.shown || state.opacity !== "0") {
            el.style.opacity = "0";
            el.style.visibility = "hidden";
            state.shown = false;
            state.opacity = "0";
          }
          continue;
        }
        const transform = `translate3d(${((track.x / 100) * width).toFixed(1)}px,0,0) translate(-50%,-50%)`;
        if (transform !== state.transform) {
          el.style.transform = transform;
          state.transform = transform;
        }
        if (!state.shown || state.opacity !== "1") {
          el.style.opacity = "1";
          el.style.visibility = "visible";
          state.shown = true;
          state.opacity = "1";
        }
        if (p.edge !== state.edge) {
          el.dataset.edge = p.edge;
          state.edge = p.edge;
        }
        const distLabel = `${Math.round(track.dist)}m`;
        if (distLabel !== state.dist) {
          const distEl = el.querySelector<HTMLElement>("[data-role='dist']");
          if (distEl) distEl.textContent = distLabel;
          state.dist = distLabel;
        }
      }

      // 5. Bearing ticks & cardinals.
      const ticks = tickEls.current;
      for (let i = 0; i < ticks.length; i++) {
        const el = ticks[i];
        if (!el) continue;
        const state = stateFor(el);
        const delta = shortestCompassDelta(i * 5, heading);
        if (Math.abs(delta) > COMPASS_HALF_FOV) {
          if (state.shown || state.opacity === "") {
            el.style.visibility = "hidden";
            state.shown = false;
            state.opacity = "h";
          }
          continue;
        }
        const pct = 50 + (delta / COMPASS_HALF_FOV) * COMPASS_SPAN_PCT;
        const suffix = el.dataset.kind === "cardinal" ? " translate(-50%,-50%)" : " translateX(-50%)";
        const transform = `translate3d(${((pct / 100) * width).toFixed(1)}px,0,0)${suffix}`;
        if (transform !== state.transform) {
          el.style.transform = transform;
          state.transform = transform;
        }
        const fade = Math.max(0.18, Math.min(1, (1 - Math.abs(delta) / COMPASS_HALF_FOV) * 1.45)).toFixed(2);
        if (fade !== state.opacity || !state.shown) {
          el.style.opacity = fade;
          el.style.visibility = "visible";
          state.opacity = fade;
          state.shown = true;
        }
      }
    };
  }

  // New heading packet: estimate turn rate for extrapolation.
  React.useLayoutEffect(() => {
    const now = performance.now();
    const prev = headingPacketRef.current;
    const dtSample = (now - prev.ms) / 1000;
    const delta = shortestCompassDelta(targetHeading, prev.value);
    if (dtSample > 0.004 && dtSample < 0.25 && Math.abs(delta) < 120) {
      const inst = Math.max(-720, Math.min(720, delta / dtSample));
      headingVelRef.current = headingVelRef.current * 0.45 + inst * 0.55;
    } else {
      headingVelRef.current = 0;
    }
    headingPacketRef.current = { value: targetHeading, ms: now };
  }, [targetHeading]);

  // New marker packet: update targets, add/prune tracks.
  React.useLayoutEffect(() => {
    const tracks = tracksRef.current;
    const seen = new Set<string>();
    for (const m of markers) {
      seen.add(m.id);
      const angle = Number.isFinite(m.angle) ? normalizeCompassDeg(m.angle) : 0;
      const dist = Number.isFinite(m.distance) ? m.distance : 0;
      const existing = tracks.get(m.id);
      if (existing) {
        existing.targetAngle = angle;
        existing.targetDist = dist;
      } else {
        tracks.set(m.id, { angle, targetAngle: angle, dist, targetDist: dist, x: 50, placed: false });
      }
    }
    for (const id of Array.from(tracks.keys())) {
      if (!seen.has(id)) tracks.delete(id);
    }
  }, [markers]);

  // After every commit, place freshly mounted elements immediately (no flash).
  React.useLayoutEffect(() => {
    frameRef.current?.(performance.now(), 0);
  });

  React.useEffect(() => {
    const bridge = getSkyWebView();
    let rafId = 0;
    let last = performance.now();
    let hostVisible = bridge?.getVisibility?.().visible ?? true;
    const loop = (now: number): void => {
      rafId = 0;
      if ((typeof document !== "undefined" && document.hidden) || !hostVisible) {
        return;
      }
      const dt = Math.min(0.05, Math.max(0.0005, (now - last) / 1000));
      last = now;
      frameRef.current?.(now, dt);
      rafId = requestAnimationFrame(loop);
    };
    const syncLoop = (): void => {
      const active = (typeof document === "undefined" || !document.hidden) && hostVisible;
      if (active && !rafId) {
        last = performance.now();
        rafId = requestAnimationFrame(loop);
      } else if (!active && rafId) {
        cancelAnimationFrame(rafId);
        rafId = 0;
      }
    };
    syncLoop();
    const offVis = bridge?.on<{ visible: boolean }>("skyultra.visibility", (ev) => {
      hostVisible = Boolean(ev?.visible);
      syncLoop();
    });
    document.addEventListener("visibilitychange", syncLoop);
    return () => {
      offVis?.();
      document.removeEventListener("visibilitychange", syncLoop);
      if (rafId) cancelAnimationFrame(rafId);
    };
  }, []);

  return { focusHeading, trackElRef, hdgNumRef, hdgCardRef, registerMarker, registerTick };
}

function CompassActorPortrait({
  bridge,
  referenceId,
  baseFormId,
  name,
  hostile = true,
  size = 34,
}: {
  bridge?: SkyWebViewAPI;
  referenceId?: string;
  baseFormId?: string;
  name: string;
  hostile?: boolean;
  size?: number;
}) {
  const requestKey = React.useMemo(() => {
    const ref = referenceId && referenceId !== "0" ? referenceId : "";
    const base = baseFormId && baseFormId !== "0" ? baseFormId : "";
    return ref || base ? `${ref}:${base}` : "";
  }, [referenceId, baseFormId]);

  const [dataUrl, setDataUrl] = React.useState<string | null>(() => {
    if (!bridge || !requestKey) return null;
    const hit = actorThumbnailQueue(bridge).peek(requestKey);
    return hit?.available && hit.dataUrl ? hit.dataUrl : null;
  });

  React.useEffect(() => {
    if (!bridge || !requestKey) {
      setDataUrl(null);
      return;
    }
    const queue = actorThumbnailQueue(bridge);
    const cached = queue.peek(requestKey);
    if (cached?.available && cached.dataUrl) {
      setDataUrl(cached.dataUrl);
    }
    const unsub = queue.subscribe(requestKey, (url) => {
      setDataUrl(url);
    });

    let active = true;
    const controller = new AbortController();
    let retryTimer = 0;
    let attempts = 0;

    const fetchPortrait = (): void => {
      if (!active) return;
      const hit = queue.peek(requestKey);
      if (hit?.available && hit.dataUrl) {
        setDataUrl(hit.dataUrl);
        return;
      }
      void queue.request(requestKey, controller.signal).then((res) => {
        if (!active || controller.signal.aborted) return;
        if (res.available && res.dataUrl) {
          setDataUrl(res.dataUrl);
        } else if (attempts < 4) {
          attempts += 1;
          retryTimer = window.setTimeout(fetchPortrait, 350 + attempts * 300);
        }
      });
    };

    fetchPortrait();
    return () => {
      active = false;
      unsub();
      controller.abort();
      if (retryTimer) window.clearTimeout(retryTimer);
    };
  }, [bridge, requestKey]);

  return (
    <span
      className={
        "hud-compass-actor-portrait" +
        (hostile ? " is-hostile" : " is-ally") +
        (dataUrl ? " has-photo" : "")
      }
      style={{ width: `${size}px`, height: `${size}px` }}
    >
      {dataUrl ? (
        <img
          src={dataUrl}
          alt={name}
          className="hud-compass-actor-portrait__img"
          draggable={false}
        />
      ) : (
        <svg
          viewBox="0 0 36 36"
          className="hud-compass-actor-portrait__fallback"
          aria-hidden="true"
        >
          <circle cx="18" cy="18" r="16" fill="rgba(16, 8, 14, 0.9)" />
          <path
            d="M8 32c1.8-6.4 5.5-9.6 10-9.6s8.2 3.2 10 9.6"
            fill="currentColor"
            fillOpacity="0.32"
            stroke="currentColor"
            strokeWidth="1.5"
          />
          <path
            d="M12 14c.8-4.5 11.2-4.5 12 0 .8 4.2.4 8.2-2.2 10.8-1.4 1.4-2.5 2.1-3.8 2.1s-2.4-.7-3.8-2.1C11.6 22.2 11.2 18.2 12 14z"
            fill="currentColor"
            fillOpacity="0.26"
            stroke="currentColor"
            strokeWidth="1.6"
          />
          {hostile ? (
            <>
              <circle cx="15" cy="16.5" r="1.4" fill="#ffccd5" />
              <circle cx="21" cy="16.5" r="1.4" fill="#ffccd5" />
              <path d="M13 13.5l3.2 1.2M23 13.5l-3.2 1.2" stroke="#ffccd5" strokeWidth="1.3" strokeLinecap="round" />
            </>
          ) : (
            <>
              <circle cx="15.2" cy="16.5" r="1.2" fill="#e6fff5" />
              <circle cx="20.8" cy="16.5" r="1.2" fill="#e6fff5" />
            </>
          )}
        </svg>
      )}
    </span>
  );
}

function cardinalLabelForHeading(deg: number): string {
  const dirs = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
  const idx = Math.round((((deg % 360) + 360) % 360) / 45) % 8;
  return dirs[idx] ?? "N";
}

function isNamedPlaceKind(kind: SkyUltraCompassMarkerKind): boolean {
  return (
    kind === "home" ||
    kind === "inn" ||
    kind === "blacksmith" ||
    kind === "vendor" ||
    kind === "castle" ||
    kind === "city" ||
    kind === "temple"
  );
}

export function CompassPoiIcon({
  kind,
  size = 22,
}: {
  kind: SkyUltraCompassMarkerKind;
  size?: number;
}) {
  switch (kind) {
    case "home":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Crossed Nordic Gable Dragon Horns */}
          <path
            d="M16 4.2L4.2 14.2l2.2 2.2L16 8.2l9.6 8.2 2.2-2.2L16 4.2z"
            fill="currentColor"
          />
          <path
            d="M12.5 5.6L8.6 2.2 6.8 3.8l3.5 3.2M19.5 5.6l3.9-3.4 1.8 1.6-3.5 3.2"
            stroke="currentColor"
            strokeWidth="1.8"
            strokeLinecap="round"
          />
          {/* Timbered Great-Hall Facade */}
          <path
            d="M7.5 14.6v12.6h17V14.6L16 8.4l-8.5 6.2z"
            fill="currentColor"
            fillOpacity="0.28"
            stroke="currentColor"
            strokeWidth="1.75"
            strokeLinejoin="round"
          />
          {/* Warm Hearth Archway & Keyhole */}
          <path
            d="M12.6 27.2v-6.4a3.4 3.4 0 0 1 6.8 0v6.4"
            fill="#fff6d6"
            fillOpacity="0.92"
          />
          <circle cx="16" cy="13.8" r="2.1" fill="#fff6d6" />
          <path d="M5.2 27.4h21.6" stroke="currentColor" strokeWidth="1.9" strokeLinecap="round" />
        </svg>
      );
    case "inn":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Overflowing Mead Foam Crown */}
          <path
            d="M7.8 10.6c-.4-2.2 1.3-3.9 3.4-3.7 1-1.9 3.4-2.4 5.2-1.1 1.7-1.1 4.1-.3 4.7 1.7 1.7.3 2.7 1.9 2.2 3.5H7.8z"
            fill="#fff8e4"
          />
          {/* Oak & Brass Tankard Body */}
          <path
            d="M8.6 12.2h13.2l-1.2 14.4a2.1 2.1 0 0 1-2.1 1.9h-6.6a2.1 2.1 0 0 1-2.1-1.9L8.6 12.2z"
            fill="currentColor"
            fillOpacity="0.34"
            stroke="currentColor"
            strokeWidth="1.85"
            strokeLinejoin="round"
          />
          {/* Ornate Scroll Handle */}
          <path
            d="M21.8 14.2h2.8a2.7 2.7 0 0 1 2.7 2.7v3.6a2.9 2.9 0 0 1-2.9 2.9h-3.2"
            fill="none"
            stroke="currentColor"
            strokeWidth="2.2"
            strokeLinecap="round"
          />
          <line x1="12.6" y1="15.4" x2="13.1" y2="25" stroke="currentColor" strokeWidth="1.65" strokeLinecap="round" />
          <line x1="17.8" y1="15.4" x2="17.3" y2="25" stroke="currentColor" strokeWidth="1.65" strokeLinecap="round" />
          <line x1="8.8" y1="15.2" x2="21.4" y2="15.2" stroke="#fff8e4" strokeWidth="1.2" strokeOpacity="0.7" />
        </svg>
      );
    case "castle":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Sovereign Crown Spire */}
          <path d="M11.6 6.8L16 2.6l4.4 4.2-1.4 2.2h-6L11.6 6.8z" fill="#fff6d8" />
          {/* Citadel Keep & Twin Battlement Towers */}
          <path
            d="M4.8 9.2h5.4v3.4h2.6V9.8h6.4v2.8h2.6V9.2h5.4v18.4H4.8V9.2z"
            fill="currentColor"
            fillOpacity="0.3"
            stroke="currentColor"
            strokeWidth="1.8"
            strokeLinejoin="round"
          />
          <path
            d="M4.8 9.2V6.2h2.2v3M8.2 9.2V6.2H10v3M22 9.2V6.2h1.8v3M25 9.2V6.2h2.2v3"
            stroke="currentColor"
            strokeWidth="1.75"
            strokeLinecap="round"
          />
          {/* Grand Portcullis Arch */}
          <path d="M12.8 27.6v-6.6a3.2 3.2 0 0 1 6.4 0v6.6" fill="#fff6d8" />
          <rect x="7.2" y="14.2" width="2" height="3.8" rx="0.8" fill="currentColor" />
          <rect x="22.8" y="14.2" width="2" height="3.8" rx="0.8" fill="currentColor" />
        </svg>
      );
    case "blacksmith":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Crossed Forged War-Hammer & Sparks */}
          <path d="M10.4 4.8l5.8-2 2.5 2.6-5.4 2.8-2.9-3.4z" fill="#fff3dc" />
          <path d="M14.8 6.8l6.8 7.4" stroke="#fff3dc" strokeWidth="2.3" strokeLinecap="round" />
          <circle cx="24.2" cy="7.4" r="1.3" fill="#fff3dc" />
          <circle cx="7.4" cy="11.2" r="1.1" fill="currentColor" />
          {/* Heavy Horned Nordic Anvil */}
          <path
            d="M4 16.4h18.2c3 0 5.4-1.3 6.2-3.2v5.6c-2.1.9-4 2-5.1 3.7H10.4c-.8-1.9-2.8-3.1-6.4-3.7v-2.4z"
            fill="currentColor"
            fillOpacity="0.36"
            stroke="currentColor"
            strokeWidth="1.8"
            strokeLinejoin="round"
          />
          <path d="M8.6 22.5h14l2.2 4.6H6.4l2.2-4.6z" fill="currentColor" />
        </svg>
      );
    case "vendor":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Imperial Balance Scales & Septim Crest */}
          <circle cx="16" cy="5.8" r="2.5" fill="#e8fff4" />
          <line x1="16" y1="8.2" x2="16" y2="26.4" stroke="currentColor" strokeWidth="2.1" strokeLinecap="round" />
          <path d="M5.8 10.8h20.4" stroke="currentColor" strokeWidth="2.1" strokeLinecap="round" />
          <path
            d="M7.2 10.8L4.4 18.2h6.4l-3.6-7.4zM24.8 10.8L21.2 18.2h6.4l-2.8-7.4z"
            fill="currentColor"
            fillOpacity="0.28"
            stroke="currentColor"
            strokeWidth="1.55"
            strokeLinejoin="round"
          />
          <path d="M3.8 18.2a3.8 3.8 0 0 0 7.6 0H3.8zM20.6 18.2a3.8 3.8 0 0 0 7.6 0h-7.6z" fill="#e8fff4" />
          <path d="M10.8 26.8h10.4" stroke="currentColor" strokeWidth="2.4" strokeLinecap="round" />
        </svg>
      );
    case "quest":
      return (
        <svg viewBox="0 0 32 36" width={size} height={Math.round(size * 1.12)} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Tall Sovereign Skyrim Double-Chevron Quest Obelisk */}
          <path
            d="M16 1.5l9.2 10.2L16 34.5 6.8 11.7 16 1.5z"
            fill="currentColor"
            fillOpacity="0.34"
            stroke="currentColor"
            strokeWidth="2.1"
            strokeLinejoin="round"
          />
          <path d="M16 5.8l4.8 5.8L16 27.6l-4.8-16L16 5.8z" fill="#fff8d6" />
          <path d="M7.6 5.4L16 12.2l8.4-6.8" fill="none" stroke="#fff8d6" strokeWidth="1.9" strokeLinecap="round" />
          <circle cx="16" cy="12" r="2.1" fill="#00f2fe" />
        </svg>
      );
    case "questDoor":
      return (
        <svg viewBox="0 0 32 36" width={size} height={Math.round(size * 1.12)} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Gilded Quest Portal Arch + Sovereign Quest Chevron */}
          <path
            d="M6.4 31.8V13.8a9.6 9.6 0 0 1 19.2 0v18"
            fill="currentColor"
            fillOpacity="0.24"
            stroke="currentColor"
            strokeWidth="2"
          />
          <path d="M16 4.2l5.8 6.8L16 26.8 10.2 11l5.8-6.8z" fill="#fff8d6" />
          <circle cx="16" cy="11.2" r="1.9" fill="#00f2fe" />
          <line x1="4" y1="31.8" x2="28" y2="31.8" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" />
        </svg>
      );
    case "customMarker":
      return (
        <svg viewBox="0 0 32 36" width={size} height={Math.round(size * 1.12)} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Royal Amethyst Hexagonal Map Pin & Compass Star ("Marker They Put") */}
          <path
            d="M16 2.2l9.8 5.6v9.8L16 33.8 6.2 17.6V7.8L16 2.2z"
            fill="currentColor"
            fillOpacity="0.34"
            stroke="currentColor"
            strokeWidth="2.1"
            strokeLinejoin="round"
          />
          {/* 4-Point Waypoint Star */}
          <path
            d="M16 6.2l2.4 4.9 4.9 2.4-4.9 2.4-2.4 4.9-2.4-4.9-4.9-2.4 4.9-2.4 2.4-4.9z"
            fill="#fff0ff"
          />
          <circle cx="16" cy="13.5" r="2.2" fill="#d946ef" />
        </svg>
      );
    case "follower":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          {/* Companion Shield & Banner Crest */}
          <path
            d="M16 3.2l10.2 4.2v8.2c0 6.8-4.4 11.4-10.2 13.6C10.2 27 5.8 22.4 5.8 15.6V7.4L16 3.2z"
            fill="currentColor"
            fillOpacity="0.32"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinejoin="round"
          />
          <path d="M16 8.2l2.2 4.4 4.8.7-3.5 3.4.8 4.8-4.3-2.3-4.3 2.3.8-4.8-3.5-3.4 4.8-.7L16 8.2z" fill="#ecfdf5" />
        </svg>
      );
    case "enemy":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          <polygon
            points="16,2.8 28.2,16 16,29.2 3.8,16"
            fill="currentColor"
            fillOpacity="0.38"
            stroke="currentColor"
            strokeWidth="2.1"
          />
          <polygon points="16,8.4 22.6,16 16,23.6 9.4,16" fill="#ffe8ec" />
        </svg>
      );
    case "city":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          <path
            d="M16 2.6L27 10v12l-11 7.4L5 22V10l11-7.4z"
            fill="currentColor"
            fillOpacity="0.26"
            stroke="currentColor"
            strokeWidth="1.9"
          />
          <path d="M10.5 22.5v-10L16 8.2l5.5 4.3v10h-3.8v-4.8h-3.4v4.8h-3.8z" fill="#f7f0ff" />
        </svg>
      );
    case "temple":
    case "stone":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          <path
            d="M16 2.6l5.4 8v16.4h-10.8V10.6l5.4-8z"
            fill="currentColor"
            fillOpacity="0.28"
            stroke="currentColor"
            strokeWidth="1.9"
          />
          <circle cx="16" cy="13.2" r="2.7" fill="#eefbff" />
          <path d="M7.2 27h17.6" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" />
        </svg>
      );
    case "dragon":
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          <path
            d="M16 3.4l5.2 6 7.2-2.8-3.3 9.2 3.8 5.4-8.1-1.4L16 28.8l-4.8-9-8.1 1.4 3.8-5.4-3.3-9.2 7.2 2.8 5.2-6z"
            fill="currentColor"
            fillOpacity="0.34"
            stroke="currentColor"
            strokeWidth="1.8"
            strokeLinejoin="round"
          />
        </svg>
      );
    default:
      return (
        <svg viewBox="0 0 32 32" width={size} height={size} aria-hidden="true" className="hud-compass-poi-svg">
          <path
            d="M5.8 26.4L12.6 7.6 16 14 20.2 6 26.2 26.4H5.8z"
            fill="currentColor"
            fillOpacity="0.28"
            stroke="currentColor"
            strokeWidth="1.9"
            strokeLinejoin="round"
          />
          <path d="M13.2 26.4v-6a2.8 2.8 0 0 1 5.6 0v6" fill="currentColor" />
        </svg>
      );
  }
}

const DEFAULT_DEMO_COMPASS_MARKERS: SkyUltraCompassMarker[] = [
  { id: "demo:quest", name: "Bleak Falls Barrow", subtitle: "Retrieve the Dragonstone", kind: "quest", categoryLabel: "Active Quest", angle: 12, distance: 340, discovered: true },
  { id: "demo:custom", name: "Custom Waypoint", subtitle: "Placed on World Map", kind: "customMarker", categoryLabel: "Custom Map Marker", angle: 48, distance: 520, discovered: true },
  { id: "demo:home", name: "Breezehome", kind: "home", categoryLabel: "Player Home", angle: 332, distance: 19, discovered: true },
  { id: "demo:blacksmith", name: "Warmaiden's", kind: "blacksmith", categoryLabel: "Blacksmith", angle: 310, distance: 26, discovered: true },
  { id: "demo:inn", name: "The Bannered Mare", kind: "inn", categoryLabel: "Inn & Tavern", angle: 28, distance: 38, discovered: true },
  { id: "demo:undiscovered", name: "Secunda's Kiss", kind: "camp", categoryLabel: "Undiscovered Giant Camp", angle: 348, distance: 46, discovered: false },
];

const COMPASS_CARDINAL_POINTS: Array<{ deg: number; label: string; major: boolean }> = [
  { deg: 0, label: "N", major: true },
  { deg: 45, label: "NE", major: false },
  { deg: 90, label: "E", major: true },
  { deg: 135, label: "SE", major: false },
  { deg: 180, label: "S", major: true },
  { deg: 225, label: "SW", major: false },
  { deg: 270, label: "W", major: true },
  { deg: 315, label: "NW", major: false },
];

const COMPASS_DEGREE_TICKS: number[] = Array.from({ length: 72 }, (_, idx) => idx * 5);

function SkyUltraCompassBar({
  player,
  heading: rawHeading,
  bridge,
}: {
  player: Player;
  heading: number;
  bridge?: SkyWebViewAPI;
}) {
  const markers =
    player.compassMarkers !== undefined
      ? player.compassMarkers
      : DEFAULT_DEMO_COMPASS_MARKERS;

  const anim = useCompassAnimator(rawHeading, markers);
  const focusHeading = anim.focusHeading;

  // Coarse projection (refreshed on data packets and ~8x/sec) used only for
  // focus selection & label decisions. Pixel positions are animated by the hook.
  const projectedMarkers = markers
    .map((marker) => {
      const delta = shortestCompassDelta(marker.angle, focusHeading);
      const absDelta = Math.abs(delta);
      const pinned = compassIsPinned(marker.kind);
      const inFov = absDelta <= COMPASS_HALF_FOV;
      const visible = (pinned || marker.distance <= COMPASS_LOCAL_RADIUS_M) && (inFov || pinned);
      return { marker, delta, absDelta, clampedEdge: !inFov, distance: marker.distance, visible };
    })
    .filter((entry) => entry.visible);

  const aimedCandidates = projectedMarkers.filter((m) => !m.clampedEdge && m.absDelta <= 28);
  const scoreFocusCandidate = (entry: (typeof projectedMarkers)[number]): number => {
    const m = entry.marker;
    let score = 100 - entry.absDelta * 2.4;
    if (m.kind === "home") score += 55;
    else if (m.kind === "enemy") score += 48;
    else if (m.kind === "follower") score += 44;
    else if (isNamedPlaceKind(m.kind)) score += 38;
    else if (m.kind === "quest" || m.kind === "questDoor") score += 30;
    else if (m.kind === "customMarker") score += 28;
    if (entry.distance <= 12) score += 42;
    else if (entry.distance <= 30) score += 24;
    else if (entry.distance <= 50) score += 12;
    return score;
  };

  const focusedEntry =
    aimedCandidates.slice().sort((a, b) => scoreFocusCandidate(b) - scoreFocusCandidate(a))[0] ??
    projectedMarkers.find((m) => m.marker.kind === "enemy" && !m.clampedEdge) ??
    projectedMarkers.find((m) => m.marker.kind === "home" && m.distance <= 25) ??
    projectedMarkers.find((m) => m.marker.kind === "quest" || m.marker.kind === "questDoor") ??
    projectedMarkers.find((m) => m.marker.kind === "customMarker") ??
    projectedMarkers.slice().sort((a, b) => a.absDelta - b.absDelta)[0] ??
    null;

  const secondaryPinnedEntry =
    focusedEntry &&
    focusedEntry.marker.kind !== "quest" &&
    focusedEntry.marker.kind !== "questDoor" &&
    focusedEntry.marker.kind !== "customMarker"
      ? projectedMarkers.find(
          (m) =>
            m.marker.id !== focusedEntry.marker.id &&
            (m.marker.kind === "quest" ||
              m.marker.kind === "questDoor" ||
              m.marker.kind === "customMarker"),
        ) ?? null
      : null;

  const cardinalLabel = cardinalLabelForHeading(focusHeading);
  const absDeltaById = new Map<string, number>();
  for (const entry of projectedMarkers) absDeltaById.set(entry.marker.id, entry.absDelta);

  return (
    <section
      className="hud-sovereign-compass"
      aria-label={`SkyUltra Compass Heading ${Math.round(focusHeading)} degrees ${cardinalLabel}`}
    >
      <div className="hud-sovereign-compass__shell">
        {/* Left Sovereign Imperial Astrolabe & Dragon-Wing Filigree End-Cap */}
        <svg className="hud-sovereign-compass__cap hud-sovereign-compass__cap--left" viewBox="0 0 92 64" aria-hidden="true">
          <defs>
            <linearGradient id="skyCompassGoldL" x1="0%" y1="0%" x2="100%" y2="100%">
              <stop offset="0%" stopColor="#fff8dc" />
              <stop offset="38%" stopColor="#f2c464" />
              <stop offset="75%" stopColor="#b88228" />
              <stop offset="100%" stopColor="#5e3d0c" />
            </linearGradient>
            <linearGradient id="skyCompassObsidianL" x1="0%" y1="0%" x2="0%" y2="100%">
              <stop offset="0%" stopColor="rgba(22, 26, 38, 0.96)" />
              <stop offset="50%" stopColor="rgba(10, 12, 18, 0.98)" />
              <stop offset="100%" stopColor="rgba(5, 6, 10, 0.98)" />
            </linearGradient>
          </defs>
          <path d="M92 5H42L6 32l36 27h50" fill="url(#skyCompassObsidianL)" stroke="url(#skyCompassGoldL)" strokeWidth="2.2" strokeLinejoin="round" />
          <path d="M90 11H46L18 32l28 21h44" fill="rgba(242, 196, 100, 0.08)" stroke="url(#skyCompassGoldL)" strokeWidth="1.25" strokeOpacity="0.85" />
          <path d="M34 21L21 32l13 11M68 15L48 32l20 17" fill="none" stroke="url(#skyCompassGoldL)" strokeWidth="1.3" strokeOpacity="0.65" strokeLinecap="round" />
          <polygon points="34,32 47,21 60,32 47,43" fill="url(#skyCompassGoldL)" />
          <polygon points="39,32 47,25.5 55,32 47,38.5" fill="#131824" />
          <polygon points="42.5,32 47,28.2 51.5,32 47,35.8" fill="#fff7d6" />
          <circle cx="15" cy="32" r="2.5" fill="#fff8dc" />
          <line x1="0" y1="32" x2="11" y2="32" stroke="url(#skyCompassGoldL)" strokeWidth="1.8" strokeLinecap="round" />
        </svg>

        {/* Main Sovereign Astrolabe Horizon Ribbon Track */}
        <div className="hud-sovereign-compass__track" ref={anim.trackElRef}>
          <div className="hud-sovereign-compass__filigree-top" aria-hidden="true" />
          <div className="hud-sovereign-compass__filigree-bottom" aria-hidden="true" />
          <div className="hud-sovereign-compass__inner-grid" aria-hidden="true" />
          <div className="hud-sovereign-compass__vignette" aria-hidden="true" />

          <div className="hud-sovereign-compass__center-notch" aria-hidden="true">
            <svg viewBox="0 0 44 20" className="hud-sovereign-compass__crown-svg">
              <path d="M4 17L13 5l9 7.5L31 5l9 12H4z" fill="rgba(12, 15, 22, 0.95)" stroke="#f5c96a" strokeWidth="1.6" strokeLinejoin="round" />
              <path d="M13 15l9-10 9 10H13z" fill="#f7d27d" />
              <polygon points="22,2 25.2,7.5 22,12.5 18.8,7.5" fill="#fffbeb" />
            </svg>
            <span className="hud-sovereign-compass__center-hairline" />
          </div>

          {/* Degree & Cardinal Marks — positioned per-frame by the animator */}
          <div className="hud-sovereign-compass__bearings" aria-hidden="true">
            {COMPASS_DEGREE_TICKS.map((deg, index) => {
              const cardinal = COMPASS_CARDINAL_POINTS.find((c) => c.deg === deg);
              if (cardinal) {
                return (
                  <span
                    key={`card-${deg}`}
                    ref={anim.registerTick(index)}
                    data-kind="cardinal"
                    className={
                      "hud-sovereign-compass__cardinal" +
                      (cardinal.major ? " is-major" : " is-inter") +
                      (cardinal.label === "N" ? " is-north" : "")
                    }
                  >
                    <b>{cardinal.label}</b>
                    <i />
                  </span>
                );
              }
              const isThirty = deg % 30 === 0;
              const isTen = deg % 10 === 0;
              return (
                <span
                  key={`tick-${deg}`}
                  ref={anim.registerTick(index)}
                  data-kind="tick"
                  className={
                    "hud-sovereign-compass__tick" +
                    (isThirty ? " is-thirty" : isTen ? " is-ten" : " is-five")
                  }
                />
              );
            })}
          </div>

          {/* Quest Spires, Waypoint Beacons, Portrait Medallions & Local POIs */}
          <div className="hud-sovereign-compass__markers">
            {markers.map((marker) => {
              const absDelta = absDeltaById.get(marker.id) ?? 180;
              const isFocused = focusedEntry?.marker.id === marker.id;
              const isQuest = marker.kind === "quest" || marker.kind === "questDoor";
              const isCustomWaypoint = marker.kind === "customMarker";
              const isEnemy = marker.kind === "enemy";
              const isFollower = marker.kind === "follower";
              const isUndiscovered = marker.discovered === false;

              const showFloatingName =
                isFocused ||
                (isEnemy && absDelta <= 32) ||
                (marker.kind === "home" && marker.distance <= 50 && absDelta <= 42) ||
                (isNamedPlaceKind(marker.kind) && marker.distance <= 30 && absDelta <= 26);

              const zOrder = isFocused
                ? 50
                : isQuest
                  ? 44
                  : isCustomWaypoint
                    ? 42
                    : isEnemy
                      ? 38
                      : isFollower
                        ? 36
                        : marker.kind === "home"
                          ? 34
                          : Math.max(5, 24 - Math.round(absDelta / 8));

              const hpPct =
                isEnemy && typeof marker.healthPct === "number"
                  ? Math.max(4, Math.min(100, marker.healthPct))
                  : 100;
              const ringCirc = 2 * Math.PI * 19;
              const ringDashOffset = ringCirc * (1 - hpPct / 100);

              return (
                <div
                  key={marker.id}
                  ref={anim.registerMarker(marker.id)}
                  className={
                    `hud-sovereign-compass__marker hud-sovereign-compass__marker--${marker.kind}` +
                    (isQuest ? " is-quest-banner" : "") +
                    (isCustomWaypoint ? " is-waypoint-beacon" : "") +
                    (isEnemy || isFollower ? " is-actor-portrait" : "") +
                    (isFocused ? " is-focused" : "") +
                    (isUndiscovered ? " is-undiscovered" : " is-discovered")
                  }
                  style={{ zIndex: zOrder }}
                  title={marker.name}
                >
                  {isQuest && (
                    <span className="hud-sovereign-compass__special-crown hud-sovereign-compass__special-crown--quest">
                      {marker.kind === "questDoor" ? "PASSAGE" : "QUEST"}
                    </span>
                  )}
                  {isCustomWaypoint && (
                    <span className="hud-sovereign-compass__special-crown hud-sovereign-compass__special-crown--waypoint">
                      WAYPOINT
                    </span>
                  )}
                  {isFollower && (
                    <span className="hud-sovereign-compass__special-crown hud-sovereign-compass__special-crown--follower">
                      {marker.name}
                    </span>
                  )}

                  {showFloatingName && !isQuest && !isCustomWaypoint && !isFollower && (
                    <span className="hud-sovereign-compass__marker-tag">
                      {isUndiscovered ? `? · ${marker.name}` : marker.name}
                    </span>
                  )}

                  <span className="hud-sovereign-compass__edge-arrow hud-sovereign-compass__edge-arrow--left">‹</span>

                  <div className="hud-sovereign-compass__marker-glyph">
                    <span className="hud-sovereign-compass__marker-halo" aria-hidden="true" />
                    {isEnemy || isFollower ? (
                      <>
                        <CompassActorPortrait
                          bridge={bridge}
                          referenceId={marker.referenceId}
                          baseFormId={marker.baseFormId}
                          name={marker.name}
                          hostile={isEnemy}
                          size={isFocused ? 36 : 32}
                        />
                        {isEnemy && (
                          <svg viewBox="0 0 44 44" className="hud-sovereign-compass__enemy-ring" aria-hidden="true">
                            <circle cx="22" cy="22" r="19" fill="none" stroke="rgba(255, 255, 255, 0.14)" strokeWidth="2.4" />
                            <circle
                              cx="22"
                              cy="22"
                              r="19"
                              fill="none"
                              stroke="#ff3b5c"
                              strokeWidth="2.5"
                              strokeLinecap="round"
                              strokeDasharray={ringCirc}
                              strokeDashoffset={ringDashOffset}
                              transform="rotate(-90 22 22)"
                            />
                          </svg>
                        )}
                      </>
                    ) : (
                      <CompassPoiIcon
                        kind={marker.kind}
                        size={isQuest || isCustomWaypoint ? (isFocused ? 27 : 24) : isFocused ? 23 : 20}
                      />
                    )}
                    {marker.elevation && marker.elevation !== "level" && (
                      <em className={`hud-sovereign-compass__elev hud-sovereign-compass__elev--${marker.elevation}`}>
                        {marker.elevation === "above" ? "▲" : "▼"}
                      </em>
                    )}
                  </div>

                  <span className="hud-sovereign-compass__edge-arrow hud-sovereign-compass__edge-arrow--right">›</span>

                  <small className="hud-sovereign-compass__marker-dist" data-role="dist" />
                </div>
              );
            })}
          </div>
        </div>

        {/* Right Sovereign Imperial Astrolabe & Dragon-Wing Filigree End-Cap */}
        <svg className="hud-sovereign-compass__cap hud-sovereign-compass__cap--right" viewBox="0 0 92 64" aria-hidden="true">
          <defs>
            <linearGradient id="skyCompassGoldR" x1="100%" y1="0%" x2="0%" y2="100%">
              <stop offset="0%" stopColor="#fff8dc" />
              <stop offset="38%" stopColor="#f2c464" />
              <stop offset="75%" stopColor="#b88228" />
              <stop offset="100%" stopColor="#5e3d0c" />
            </linearGradient>
            <linearGradient id="skyCompassObsidianR" x1="0%" y1="0%" x2="0%" y2="100%">
              <stop offset="0%" stopColor="rgba(22, 26, 38, 0.96)" />
              <stop offset="50%" stopColor="rgba(10, 12, 18, 0.98)" />
              <stop offset="100%" stopColor="rgba(5, 6, 10, 0.98)" />
            </linearGradient>
          </defs>
          <path d="M0 5h50l36 27-36 27H0" fill="url(#skyCompassObsidianR)" stroke="url(#skyCompassGoldR)" strokeWidth="2.2" strokeLinejoin="round" />
          <path d="M2 11h44l28 21-28 21H2" fill="rgba(242, 196, 100, 0.08)" stroke="url(#skyCompassGoldR)" strokeWidth="1.25" strokeOpacity="0.85" />
          <path d="M58 21l13 11-13 11M24 15l20 17-20 17" fill="none" stroke="url(#skyCompassGoldR)" strokeWidth="1.3" strokeOpacity="0.65" strokeLinecap="round" />
          <polygon points="58,32 45,21 32,32 45,43" fill="url(#skyCompassGoldR)" />
          <polygon points="53,32 45,25.5 37,32 45,38.5" fill="#131824" />
          <polygon points="49.5,32 45,28.2 40.5,32 45,35.8" fill="#fff7d6" />
          <circle cx="77" cy="32" r="2.5" fill="#fff8dc" />
          <line x1="81" y1="32" x2="92" y2="32" stroke="url(#skyCompassGoldR)" strokeWidth="1.8" strokeLinecap="round" />
        </svg>
      </div>

      {/* Bottom Sovereign Horizon Telemetry Plaque & Rich Target Dossier */}
      <div className="hud-sovereign-compass__subbar">
        <span className="hud-sovereign-compass__hold">
          <Icon name="compass" size={13} /> {player.location || "Skyrim"}
        </span>
        {focusedEntry ? (
          <div
            className={
              `hud-sovereign-compass__callout hud-sovereign-compass__callout--${focusedEntry.marker.kind}` +
              (focusedEntry.marker.discovered === false ? " is-undiscovered" : "")
            }
          >
            {focusedEntry.marker.kind === "enemy" || focusedEntry.marker.kind === "follower" ? (
              <CompassActorPortrait
                bridge={bridge}
                referenceId={focusedEntry.marker.referenceId}
                baseFormId={focusedEntry.marker.baseFormId}
                name={focusedEntry.marker.name}
                hostile={focusedEntry.marker.kind === "enemy"}
                size={24}
              />
            ) : (
              <CompassPoiIcon kind={focusedEntry.marker.kind} size={18} />
            )}
            <span className="hud-sovereign-compass__callout-cat">
              {focusedEntry.marker.discovered === false
                ? "UNDISCOVERED"
                : focusedEntry.marker.categoryLabel || focusedEntry.marker.kind.toUpperCase()}
            </span>
            <strong className="hud-sovereign-compass__callout-name">{focusedEntry.marker.name}</strong>
            {focusedEntry.marker.subtitle && focusedEntry.marker.subtitle !== focusedEntry.marker.name ? (
              <span className="hud-sovereign-compass__callout-sub">· {focusedEntry.marker.subtitle}</span>
            ) : null}
            <b className="hud-sovereign-compass__callout-dist">{Math.round(focusedEntry.distance)}m</b>
          </div>
        ) : null}
        {secondaryPinnedEntry ? (
          <div
            className={`hud-sovereign-compass__quest-pill hud-sovereign-compass__quest-pill--${secondaryPinnedEntry.marker.kind}`}
          >
            <CompassPoiIcon kind={secondaryPinnedEntry.marker.kind} size={15} />
            <span>
              {secondaryPinnedEntry.marker.subtitle || secondaryPinnedEntry.marker.name} (
              {Math.round(secondaryPinnedEntry.distance)}m)
            </span>
          </div>
        ) : null}
        <strong className="hud-sovereign-compass__hdg">
          <span ref={anim.hdgNumRef} /> <em ref={anim.hdgCardRef} />
        </strong>
      </div>
    </section>
  );
}

function WidgetContent({
  kind,
  player,
  bridge,
  nearbyActors,
  fps,
  accentColor,
}: {
  kind: WidgetKind;
  player: Player | null;
  bridge?: SkyWebViewAPI;
  nearbyActors?: Npc[];
  fps?: number;
  accentColor: string;
}): React.JSX.Element {
  const [liveFps, setLiveFps] = React.useState<number>(fps ?? 60);
  const [frameMs, setFrameMs] = React.useState<number>(16.7);

  React.useEffect(() => {
    if (kind !== "telemetry") return;
    if (typeof fps === "number" && fps > 0) {
      setLiveFps(Math.round(fps));
      setFrameMs(Number((1000 / Math.max(1, fps)).toFixed(1)));
      return;
    }
    let raf = 0;
    let frames = 0;
    let start = performance.now();
    const loop = (now: number): void => {
      frames++;
      const elapsed = now - start;
      if (elapsed >= 600) {
        const measured = Math.max(1, Math.round((frames * 1000) / elapsed));
        setLiveFps(measured);
        setFrameMs(Number((elapsed / Math.max(1, frames)).toFixed(1)));
        frames = 0;
        start = now;
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, [kind, fps]);

  if (!player) {
    return (
      <div className="hud-ghost-sync">
        <span className="hud-ghost-sync__dot" />
        <span>AWAITING SKYRIM TELEMETRY…</span>
      </div>
    );
  }

  if (kind === "vitals") {
    const channels = [
      {
        id: "health",
        name: "HEALTH",
        current: player.health.current,
        max: player.health.max,
        color: accentColor || "#ff5c6c",
      },
      {
        id: "magicka",
        name: "MAGICKA",
        current: player.magicka.current,
        max: player.magicka.max,
        color: "#eab308",
      },
      {
        id: "stamina",
        name: "STAMINA",
        current: player.stamina.current,
        max: player.stamina.max,
        color: "#34d399",
      },
    ];
    const radius = 42;
    const circumference = 2 * Math.PI * radius;
    const ticks = Array.from({ length: 36 }, (_, idx) => idx);

    return (
      <section className="hud-vitals-rings" aria-label="The vital signs">
        {channels.map((ch) => {
          const frac = skyMeterFraction(ch.current, ch.max);
          const pct = Math.round(frac * 100);
          const critical = pct <= 25;
          const dashOffset = circumference * (1 - frac);
          const tipAngle = frac * 360 - 90;
          const tipRad = (tipAngle * Math.PI) / 180;
          const tipX = 56 + radius * Math.cos(tipRad);
          const tipY = 56 + radius * Math.sin(tipRad);

          return (
            <div
              key={ch.id}
              className={"hud-vital-ring" + (critical ? " is-critical" : "")}
              style={{ "--vital-ring-color": ch.color } as React.CSSProperties}
            >
              <div className="hud-vital-ring__dial">
                <svg viewBox="0 0 112 112" aria-hidden="true">
                  <g className="hud-vital-ring__ticks">
                    {ticks.map((tick) => {
                      const angle = (tick * 10 - 90) * (Math.PI / 180);
                      const x1 = 56 + 48.5 * Math.cos(angle);
                      const y1 = 56 + 48.5 * Math.sin(angle);
                      const x2 = 56 + 51.5 * Math.cos(angle);
                      const y2 = 56 + 51.5 * Math.sin(angle);
                      return <line key={tick} x1={x1} y1={y1} x2={x2} y2={y2} />;
                    })}
                  </g>
                  <circle cx="56" cy="56" r={radius} className="hud-vital-ring__track" />
                  <circle
                    cx="56"
                    cy="56"
                    r={radius}
                    className="hud-vital-ring__arc"
                    strokeDasharray={circumference}
                    strokeDashoffset={dashOffset}
                    transform="rotate(-90 56 56)"
                  />
                  {frac > 0.02 && <circle cx={tipX} cy={tipY} r="3.2" className="hud-vital-ring__pip" />}
                </svg>
                <div className="hud-vital-ring__center">
                  <em className="hud-vital-ring__pct">{pct}%</em>
                  <strong className="hud-vital-ring__value">{fmt(ch.current)}</strong>
                  <small className="hud-vital-ring__max">/{fmt(ch.max)}</small>
                </div>
              </div>
              <span className="hud-vital-ring__label">{ch.name}</span>
            </div>
          );
        })}
      </section>
    );
  }

  if (kind === "compass") {
    const rawHeading = player.heading ?? 0;
    const angle = Number.isFinite(rawHeading) ? ((rawHeading % 360) + 360) % 360 : 0;
    return <SkyUltraCompassBar player={player} heading={angle} bridge={bridge} />;
  }

  if (kind === "radar") {
    const actors = (nearbyActors && nearbyActors.length > 0 ? nearbyActors : []).slice(0, 6);
    const headingRad = ((player.heading ?? 0) * Math.PI) / 180;
    return (
      <section className="hud-radar-floating" aria-label="Tactical Proximity Radar">
        <div className="hud-radar-floating__scope">
          <svg viewBox="0 0 100 100" aria-hidden="true">
            <circle cx="50" cy="50" r="45" className="hud-radar__ring-outer" />
            <circle cx="50" cy="50" r="28" className="hud-radar__ring-mid" />
            <line x1="50" y1="5" x2="50" y2="95" className="hud-radar__axis" />
            <line x1="5" y1="50" x2="95" y2="50" className="hud-radar__axis" />
            <path d="M50 50 L36 10 A45 45 0 0 1 64 10 Z" className="hud-radar__cone" />
            <circle cx="50" cy="50" r="3.2" className="hud-radar__self" />
            {actors.map((npc, idx) => {
              const distNorm = Math.min(40, Math.max(8, ((npc.distance ?? (idx + 1) * 450) / 3000) * 40));
              const seedAngle = idx * 1.37 + (npc.name.charCodeAt(0) || 1) * 0.45 - headingRad;
              const cx = 50 + Math.sin(seedAngle) * distNorm;
              const cy = 50 - Math.cos(seedAngle) * distNorm;
              return <circle key={npc.referenceId || npc.baseFormId || idx} cx={cx} cy={cy} r="2.8" className="hud-radar__blip" />;
            })}
          </svg>
        </div>
        <div className="hud-radar-floating__list">
          <div className="hud-radar-floating__head">
            <span>PROXIMITY SCAN</span>
            <strong>{actors.length ? `${actors.length} DETECTED` : "SECTOR CLEAR"}</strong>
          </div>
          {actors.length > 0 ? (
            actors.slice(0, 3).map((npc, idx) => (
              <div key={npc.referenceId || npc.baseFormId || idx} className="hud-radar-floating__row">
                <i />
                <span>{npc.name}</span>
                <small>{npc.distance ? `${Math.round(npc.distance / 70)}m` : `LVL ${npc.level ?? 1}`}</small>
              </div>
            ))
          ) : (
            <div className="hud-radar-floating__row">
              <i />
              <span>{player.location || "Skyrim Hold"}</span>
              <small>{Math.round(player.heading ?? 0)}° BRG</small>
            </div>
          )}
        </div>
      </section>
    );
  }

  if (kind === "target") {
    const primary = nearbyActors && nearbyActors.length > 0 ? nearbyActors[0] : null;
    const secondary = nearbyActors && nearbyActors.length > 1 ? nearbyActors[1] : null;
    const hpCur = primary?.health ?? 100;
    const hpMax = primary?.healthMax ?? 100;
    const hpPct = Math.round(skyMeterFraction(hpCur, hpMax) * 100);
    const distMeters = primary?.distance ? Math.max(1, Math.round(primary.distance / 70)) : 4;

    return (
      <section className="hud-target-floating" aria-label="Nearest Actor & Companion Scanner">
        <div className="hud-target-floating__head">
          <span className="hud-target-floating__tag">
            <Icon name="target" size={12} /> {primary ? (primary.follower ? "COMPANION LOCK" : "PROXIMITY TARGET") : "HOLD SCANNER"}
          </span>
          <em className="hud-target-floating__dist">{primary ? `${distMeters}m · LVL ${primary.level ?? 1}` : "CLEAR"}</em>
        </div>
        <div className="hud-target-floating__main">
          <strong>{primary ? primary.name : `${player.location || "Skyrim"} Overwatch`}</strong>
          <small>{primary ? `${primary.race || "Tamrielic"} · ${primary.sex || "Actor"}` : "No actors within immediate lock range"}</small>
        </div>
        <div className="hud-target-floating__bar">
          <i style={{ width: `${primary ? hpPct : 100}%` }} />
        </div>
        {secondary && (
          <div className="hud-target-floating__sub">
            <span>NEXT NEARBY: {secondary.name}</span>
            <b>{secondary.distance ? `${Math.max(1, Math.round(secondary.distance / 70))}m` : `LVL ${secondary.level}`}</b>
          </div>
        )}
      </section>
    );
  }

  if (kind === "skills") {
    const topSkills = [...(player.skills ?? [])]
      .sort((a, b) => b.value - a.value)
      .slice(0, 3);
    const fallbackSkills = [
      { id: "onehanded", label: "One-Handed", value: Math.min(100, 45 + player.level), group: "combat" },
      { id: "destruction", label: "Destruction", value: Math.min(100, 40 + player.level), group: "magic" },
      { id: "sneak", label: "Sneak", value: Math.min(100, 35 + player.level), group: "stealth" },
    ];
    const list = topSkills.length > 0 ? topSkills : fallbackSkills;

    return (
      <section className="hud-skills-floating" aria-label="Signature Skills & Mastery Tracker">
        <div className="hud-skills-floating__head">
          <span>
            <Icon name="rune" size={12} /> SIGNATURE MASTERY
          </span>
          <strong>LVL {player.level}</strong>
        </div>
        <div className="hud-skills-floating__list">
          {list.map((skill) => {
            const pct = Math.min(100, Math.max(8, Math.round(skill.value)));
            return (
              <div key={skill.id} className="hud-skills-floating__item">
                <div className="hud-skills-floating__meta">
                  <SkillIcon skill={skill.id || skill.label} size={14} />
                  <strong>{skill.label}</strong>
                  <em>{masteryTier(skill.value)}</em>
                  <b>{Math.round(skill.value)}</b>
                </div>
                <div className="hud-skills-floating__track">
                  <i style={{ width: `${pct}%` }} />
                </div>
              </div>
            );
          })}
        </div>
      </section>
    );
  }

  if (kind === "armory") {
    const eq = player.equipment;
    const equippedCount = [
      eq.head,
      eq.body,
      eq.hands,
      eq.feet,
      eq.cape,
      eq.neck,
      eq.ring,
      eq.mainHand,
      eq.offHand,
      eq.ammo,
      eq.lantern,
    ].filter(Boolean).length;
    const armorRating = Math.max(0, Math.round(player.armorRating ?? 0));
    const mitigationPct = Math.min(80, Math.round((armorRating / 667) * 80));
    const ehp = Math.round((player.health?.max ?? 100) / Math.max(0.2, 1 - mitigationPct / 100));

    return (
      <section className="hud-armory-floating" aria-label="Equipped Armory & Ward Matrix">
        <div className="hud-armory-floating__top">
          <div className="hud-armory-floating__metric">
            <small>ARMOR RATING</small>
            <strong>{armorRating.toLocaleString()}</strong>
          </div>
          <div className="hud-armory-floating__metric">
            <small>MITIGATION</small>
            <strong>{mitigationPct}% CAP</strong>
          </div>
          <div className="hud-armory-floating__metric">
            <small>EFFECTIVE HP</small>
            <strong>{fmt(ehp)}</strong>
          </div>
        </div>
        <div className="hud-armory-floating__bar">
          <i style={{ width: `${Math.round((mitigationPct / 80) * 100)}%` }} />
        </div>
        <div className="hud-armory-floating__foot">
          <span>
            <Icon name="shield" size={12} /> {equippedCount} / 11 SLOTS WORN
          </span>
          <em>{eq.mainHand?.name || "Unarmed"}</em>
        </div>
      </section>
    );
  }

  if (kind === "stealth" || kind === "equipment") {
    const main = player.equipment.mainHand?.name ?? "Unarmed";
    const off = player.equipment.offHand?.name ?? "Empty";
    const ammo = player.equipment.ammo?.name;
    const armorRating = Math.max(0, Math.round(player.armorRating ?? 0));
    const mitigationPct = Math.min(80, Math.round(armorRating * 0.12));
    return (
      <section className="hud-loadout-floating" aria-label="Combat & Loadout Telemetry">
        <div className="hud-loadout-floating__hands">
          <div className="hud-loadout-floating__slot">
            <Icon name={itemIcon(player.equipment.mainHand?.category ?? "weapon")} size={15} />
            <div>
              <small>RIGHT HAND</small>
              <strong>{main}</strong>
            </div>
          </div>
          <div className="hud-loadout-floating__slot">
            <Icon name={itemIcon(player.equipment.offHand?.category ?? "armor")} size={15} />
            <div>
              <small>LEFT HAND</small>
              <strong>{off}</strong>
            </div>
          </div>
        </div>
        <div className="hud-loadout-floating__metrics">
          <span>
            <Icon name="shield" size={12} /> {armorRating.toLocaleString()} AR <em>({mitigationPct}% CAP)</em>
          </span>
          {ammo && (
            <span>
              <Icon name="compass" size={12} /> {ammo}
            </span>
          )}
        </div>
      </section>
    );
  }

  if (kind === "loot" || kind === "resources") {
    const curW = Math.round(player.carryWeight?.current ?? 0);
    const maxW = Math.max(1, Math.round(player.carryWeight?.max ?? 300));
    const freeW = Math.max(0, maxW - curW);
    const pct = Math.min(100, Math.round((curW / maxW) * 100));
    return (
      <section className="hud-loot-floating" aria-label="Encumbrance & Treasury">
        <div className="hud-loot-floating__top">
          <span className="hud-loot-floating__gold">
            <Icon name="gold" size={14} />
            <strong>{fmt(player.gold)}</strong> <small>SEPTIMS</small>
          </span>
          <span className="hud-loot-floating__weight">
            <strong>{curW}</strong> <small>/ {maxW} WT ({freeW} FREE)</small>
          </span>
        </div>
        <div className="hud-loot-floating__bar">
          <i style={{ width: `${pct}%` }} className={pct >= 90 ? "is-heavy" : ""} />
        </div>
      </section>
    );
  }

  if (kind === "wards" || kind === "effects") {
    const effects = (player.activeEffects ?? []).slice(0, 3);
    const res = (player.resistances ?? []).filter((r) => Math.abs(r.value) > 0).slice(0, 4);
    return (
      <section className="hud-wards-floating" aria-label="Elemental Wards & Active Buffs">
        {res.length > 0 && (
          <div className="hud-wards-floating__res">
            {res.map((r) => (
              <span key={r.id} className="hud-ward-pill">
                <small>{r.label.replace(/resistance/i, "").trim().slice(0, 5).toUpperCase()}</small>
                <strong>{Math.round(r.value)}%</strong>
              </span>
            ))}
          </div>
        )}
        <div className="hud-wards-floating__buffs">
          {effects.map((eff) => (
            <div key={eff.id} className="hud-buff-line" data-hostile={eff.hostile || undefined}>
              <i />
              <span>{eff.name}</span>
              <b>{eff.duration > 0 ? `${Math.ceil(eff.remaining)}s` : "PASSIVE"}</b>
            </div>
          ))}
          {!effects.length && (
            <div className="hud-buff-line">
              <i />
              <span>No active spell effects</span>
              <b>CLEAR</b>
            </div>
          )}
        </div>
      </section>
    );
  }

  if (kind === "character") {
    const hpPct = Math.round(skyMeterFraction(player.health.current, player.health.max) * 100);
    return (
      <section className="hud-character-floating" aria-label="Character Identity">
        <PlayerPortrait
          player={player}
          bridge={bridge}
          size="sm"
          badge={`L${player.level}`}
          className="hud-character-floating__portrait"
        />
        <div className="hud-character-floating__info">
          <div className="hud-character-floating__row">
            <strong>{player.playerName}</strong>
            <span>LVL {player.level}</span>
          </div>
          <div className="hud-character-floating__sub">
            <span>{player.location}</span>
            <em>HP {hpPct}%</em>
          </div>
        </div>
      </section>
    );
  }

  if (kind === "telemetry") {
    const fpsPct = Math.min(100, Math.round((liveFps / 120) * 100));
    return (
      <section className="hud-telemetry-floating" aria-label="Engine FPS Telemetry">
        <div className="hud-telemetry-floating__main">
          <strong>{liveFps}</strong>
          <small>FPS</small>
        </div>
        <div className="hud-telemetry-floating__meta">
          <div className="hud-telemetry-floating__row">
            <span>FRAME PACE</span>
            <b>{frameMs.toFixed(1)} ms</b>
          </div>
          <div className="hud-telemetry-floating__bar">
            <i style={{ width: `${fpsPct}%` }} />
          </div>
        </div>
      </section>
    );
  }

  const calendar = player.calendar;
  const hourNum = calendar && Number.isFinite(calendar.hour) ? Math.floor(calendar.hour) % 24 : 12;
  const hour = String(hourNum).padStart(2, "0");
  const minute = calendar && Number.isFinite(calendar.minute) ? String(calendar.minute % 60).padStart(2, "0") : "00";
  const isNight = hourNum < 6 || hourNum >= 20;
  return (
    <section className="hud-clock-floating" aria-label={"Time " + hour + ":" + minute}>
      <div className="hud-clock-floating__time">
        <strong>
          {hour}
          <span>:</span>
          {minute}
        </strong>
        <em>{isNight ? "MOONLIT WATCH" : "SOLAR WATCH"}</em>
      </div>
      <div className="hud-clock-floating__date">
        <span>{calendar ? `${Math.floor(calendar.day)} ${calendar.month}, 4E ${calendar.year}` : "4E 201 · Tamriel"}</span>
        <small>{player.location}</small>
      </div>
    </section>
  );
}
