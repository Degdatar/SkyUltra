export type Route = "overview" | "actors" | "inventory" | "character" | "components" | "performance" | "widgets" | "devtools" | "guide" | "settings";
export type Palette =
  | "darkmode"
  | "warmdusk"
  | "sagehaven"
  | "twilightplum"
  | "nordiclinen"
  | "obsidian"
  | "dragonstone"
  | "aurora"
  | "ember"
  | "glacier"
  | "verdant"
  | "aether"
  | "dwemer"
  | "sunspire"
  | "frost"
  | "arcane"
  | "cyberneon"
  | "synthwave"
  | "venomneon"
  | "plasmaneon"
  | "crimsonneon";
import type { Item, Player, Npc, ActorResponse, SearchResponse } from "@skyultra/sdk";
export type { Item, Player, Npc, ActorResponse, SearchResponse };
export interface Run { id:string; engine: string; items: number; theme: Palette; viewport:{width:number;height:number;devicePixelRatio:number}; averageFps: number; averageMs: number; p95Ms: number; slowFrames: number; frames: number; native?:import("@skyultra/sdk").SkyUltraNativeCapture; }
export function formId(id: string): string { const base = id.split(":")[0] ?? id; const number = Number(base); return Number.isFinite(number) ? "0x" + Math.trunc(number).toString(16).toUpperCase().padStart(8, "0") : id; }
export function initials(name: string): string { return name.split(/\s+/).filter(Boolean).slice(0, 2).map((word) => word[0]).join("").toUpperCase(); }
export interface PaletteInfo {
  value: Palette;
  label: string;
  color: string;
  secondary: string;
  family: "darkmode" | "classic" | "celestial" | "neon";
  styleTag: string;
}
export const palettes: PaletteInfo[] = [
  { value: "darkmode", label: "Velvet Slate", color: "#68b5ff", secondary: "#a38bf8", family: "darkmode", styleTag: "Eye-Comfort Matte · Cool Slate" },
  { value: "warmdusk", label: "Warm Dusk", color: "#e6b87d", secondary: "#d4926a", family: "darkmode", styleTag: "Eye-Comfort Matte · Low-Blue Amber" },
  { value: "sagehaven", label: "Sage Haven", color: "#7ec8a7", secondary: "#8db4e0", family: "darkmode", styleTag: "Eye-Comfort Matte · Calm Eucalyptus" },
  { value: "twilightplum", label: "Twilight Mist", color: "#a5a0e8", secondary: "#d99ec2", family: "darkmode", styleTag: "Eye-Comfort Matte · Soft Lavender" },
  { value: "nordiclinen", label: "Parchment Dusk", color: "#d4a373", secondary: "#b6c4a2", family: "darkmode", styleTag: "Eye-Comfort Matte · Reading Lamp" },
  { value: "obsidian", label: "Obsidian", color: "#d8c49c", secondary: "#70caff", family: "classic", styleTag: "Nordic Slate · Serif" },
  { value: "dragonstone", label: "Dragonstone", color: "#dfa84e", secondary: "#ff765f", family: "classic", styleTag: "Imperial Inlay · Engraved" },
  { value: "dwemer", label: "Dwemer", color: "#63e0c1", secondary: "#e4b363", family: "classic", styleTag: "Automaton Brass · Beveled" },
  { value: "sunspire", label: "Sunspire", color: "#ffb95c", secondary: "#ffe08a", family: "classic", styleTag: "Aldmeri Gold · Ornate" },
  { value: "ember", label: "Emberfall", color: "#ff765f", secondary: "#ffb36b", family: "classic", styleTag: "Volcanic Ash · Chiseled" },
  { value: "verdant", label: "Verdant", color: "#a5df67", secondary: "#4ee0a8", family: "classic", styleTag: "Wildwood Canopy · Organic" },
  { value: "aurora", label: "Aurora", color: "#52e6d1", secondary: "#a899ff", family: "celestial", styleTag: "Sovngarde Sky · Glass" },
  { value: "glacier", label: "Glacier", color: "#70caff", secondary: "#9ef0ff", family: "celestial", styleTag: "Sea of Ghosts · Crystalline" },
  { value: "frost", label: "Frost", color: "#4ed5e5", secondary: "#7ab8ff", family: "celestial", styleTag: "Winterhold Ice · Sharp" },
  { value: "aether", label: "Aether", color: "#be8cff", secondary: "#62efff", family: "celestial", styleTag: "Blackreach Star · Astral" },
  { value: "arcane", label: "Arcane", color: "#a899ff", secondary: "#ff7ae5", family: "celestial", styleTag: "Mystic Glyph · Runic" },
  { value: "cyberneon", label: "Neon Cyber", color: "#00f0ff", secondary: "#ff2a85", family: "neon", styleTag: "Chamfered HUD · Cyber Grid" },
  { value: "synthwave", label: "Neon Synthwave", color: "#ff2e93", secondary: "#00e5ff", family: "neon", styleTag: "Outrun Horizon · Scanlines" },
  { value: "venomneon", label: "Neon Venom", color: "#39ff14", secondary: "#00ffcc", family: "neon", styleTag: "Bio-Luminescent · Matrix" },
  { value: "plasmaneon", label: "Neon Plasma", color: "#bf55ec", secondary: "#00f5d4", family: "neon", styleTag: "High-Voltage · Arc Glass" },
  { value: "crimsonneon", label: "Neon Bloodmoon", color: "#ff1744", secondary: "#ff9100", family: "neon", styleTag: "Vampiric Laser · Razor" },
];
