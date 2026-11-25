import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  createSkyUltraGame,
  type SkyUltraDisenchantableItem,
  type SkyUltraEnchantingItem,
  type SkyUltraEnchantingSnapshot,
  type SkyUltraKnownEnchantment,
  type SkyUltraKnownEnchantmentEffect,
  type SkyUltraSoulGemEntry,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import { SkyChoice, SkySegmentedControl } from "../core.js";
import { ItemThumbnail } from "./ItemThumbnail.js";
import { Icon } from "./icons.js";
import { Empty } from "./pieces.js";
import { formId, type Item, type Player } from "./model.js";

export type EnchantingDisplayMode = "half" | "full" | "windowed";

type EnchantingTab = "enchant" | "disenchant";
type EnchantSlotStep = "item" | "enchantment" | "soulgem";
type ItemFilter = "all" | "weapon" | "armor" | "jewelry";
type EnchFilter = "compatible" | "all" | "weapon" | "armor";
type GemFilter = "all" | "grand" | "reusable";
type DisenchantFilter = "all" | "learnable" | "known" | "blocked";
type SortBy = "name" | "value" | "stat" | "tier" | "count";

export interface EnchantingPageProps {
  player?: Player | null;
  bridge?: SkyWebViewAPI | null;
  obscured?: boolean;
  displayMode?: EnchantingDisplayMode;
  pauseWorld?: boolean;
  onChangeDisplayMode?: (mode: EnchantingDisplayMode) => void;
  onTogglePauseWorld?: () => void;
  onClose?: () => void;
}

export interface EnchantmentCompatibilityResult {
  compatible: boolean;
  reason?: string;
}

export interface WeaponChargePreview {
  uses: number;
  maxCharge: number;
  costPerUse: number;
  magnitudeScale: number;
}

const DISPLAY_MODE_ORDER: EnchantingDisplayMode[] = ["half", "full", "windowed"];
const DISPLAY_MODE_LABELS: Record<EnchantingDisplayMode, string> = {
  half: "Half Container",
  full: "Full Screen",
  windowed: "Windowed",
};

type SigilTheme =
  | "fire"
  | "frost"
  | "shock"
  | "chaos"
  | "soul"
  | "poison"
  | "sun"
  | "shout"
  | "absorb-hp"
  | "absorb-mp"
  | "absorb-sp"
  | "health"
  | "magicka"
  | "stamina"
  | "combat"
  | "stealth"
  | "craft"
  | "resist"
  | "control"
  | "restoration"
  | "destruction"
  | "conjuration"
  | "illusion"
  | "alteration"
  | "moon";

function classifyEnchantmentSigil(
  name: string,
  effectName?: string,
  school?: string,
): { variant: SigilTheme; glyph: string } {
  const hay = `${name} ${effectName ?? ""}`.toLowerCase();
  const sch = (school ?? "").toLowerCase();
  const has = (re: RegExp) => re.test(hay);

  if (has(/\b(chaos|prismatic)\b/)) return { variant: "chaos", glyph: "chaos" };
  if (has(/soul trap|fiery soul/)) return { variant: "soul", glyph: "soul" };
  if (has(/\bsun\b|auriel|dawnbreaker|turn undead|stendarr/)) return { variant: "sun", glyph: "sun" };
  if (has(/\b(poison|venom|viper)\b/)) return { variant: "poison", glyph: "poison" };
  if (has(/\b(shout|shouts|voice|thu'um|talos)\b/)) return { variant: "shout", glyph: "shout" };
  if (has(/(absorb|drain|steal)\b[^.\n]*\bhealth|\bvampir/)) return { variant: "absorb-hp", glyph: "vampiric" };
  if (has(/(absorb|drain)\b[^.\n]*\bmagicka/)) return { variant: "absorb-mp", glyph: "siphon-mp" };
  if (has(/(absorb|drain)\b[^.\n]*\bstamina/)) return { variant: "absorb-sp", glyph: "siphon-sp" };
  if (has(/\bresist\b|spell absor/)) return { variant: "resist", glyph: "ward" };
  if (has(/\b(lunar|moons|silent moons)\b/)) return { variant: "moon", glyph: "moon" };
  if (has(/\b(nightingale|shadow|sneak|muffle)\b/)) return { variant: "stealth", glyph: "nightingale" };
  if (has(/\b(fire|flame|flames|burn|burns|fiery|scorch|inferno)\b/)) return { variant: "fire", glyph: "fire" };
  if (has(/\b(frost|ice|cold|chill|freeze|freezing|glacial|stalhrim)\b/)) return { variant: "frost", glyph: "frost" };
  if (has(/\b(shock|lightning|storm|spark|sparks|thunder)\b/)) return { variant: "shock", glyph: "shock" };
  if (has(/\b(paralyze|paralysis|slow|slows|petrify)\b/)) return { variant: "control", glyph: "paralyze" };
  if (has(/\b(banish|fear|frenzy|calm|demoralize|daedra)\b/)) return { variant: "control", glyph: "banish" };
  if (has(/carry weight/)) return { variant: "craft", glyph: "pack" };
  if (has(/\balchemy\b/)) return { variant: "alteration", glyph: "flask" };
  if (has(/\bsmithing\b/)) return { variant: "craft", glyph: "anvil" };
  if (has(/\b(barter|speech|persuasion|persuade)\b/)) return { variant: "craft", glyph: "scales" };
  if (has(/\b(lockpicking|pickpocket|lockpick)\b/)) return { variant: "stealth", glyph: "key" };
  if (has(/\b(archery|marksman|bow)\b/)) return { variant: "combat", glyph: "bow" };
  if (has(/\b(unarmed|pugilist|brawl)\b/)) return { variant: "combat", glyph: "gauntlet" };
  if (has(/\bblock\b|heavy armor|light armor/)) return { variant: "combat", glyph: "shield" };
  if (has(/one-handed|two-handed|huntsman|\bweapons?\b/)) return { variant: "combat", glyph: "blade" };
  if (has(/\b(health|healing|regen)\b/)) return { variant: "health", glyph: "heart" };
  if (has(/waterbreath|waterwalk/)) return { variant: "alteration", glyph: "wave" };
  if (has(/\bconjuration\b/)) return { variant: "conjuration", glyph: "conjuration" };
  if (has(/\bdestruction\b/)) return { variant: "destruction", glyph: "destruction" };
  if (has(/\brestoration\b/)) return { variant: "restoration", glyph: "restoration" };
  if (has(/\billusion\b/)) return { variant: "illusion", glyph: "illusion" };
  if (has(/\balteration\b/)) return { variant: "alteration", glyph: "alteration" };
  if (has(/\bmagicka\b/)) return { variant: "magicka", glyph: "magicka" };
  if (has(/\bstamina\b/)) return { variant: "stamina", glyph: "vigor" };

  if (sch.includes("destruction")) return { variant: "destruction", glyph: "destruction" };
  if (sch.includes("conjuration")) return { variant: "conjuration", glyph: "conjuration" };
  if (sch.includes("restoration")) return { variant: "restoration", glyph: "restoration" };
  if (sch.includes("illusion")) return { variant: "illusion", glyph: "illusion" };
  if (sch.includes("alteration")) return { variant: "alteration", glyph: "alteration" };
  return { variant: "magicka", glyph: "scroll" };
}

function EnchantmentSigil({
  name,
  effectName,
  school,
  size = "md",
}: {
  name: string;
  effectName?: string;
  school?: string;
  size?: "lg" | "md" | "sm";
}) {
  const { variant, glyph } = classifyEnchantmentSigil(name, effectName, school);
  const px = size === "sm" ? 14 : size === "lg" ? 26 : 20;

  let paths = (
    <>
      <path d="M7 4.2h11a2 2 0 0 1 2 2v1.6h-2.5v10a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-1.6h2.5v-10a2 2 0 0 1 .5-2z" fill="currentColor" fillOpacity=".18" />
      <path d="M9.5 8.5h5M9.5 12h5M9.5 15.5h3.2" />
    </>
  );

  if (glyph === "sun") {
    paths = (
      <>
        <polygon points="12 7 15.5 8.5 17 12 15.5 15.5 12 17 8.5 15.5 7 12 8.5 8.5" fill="currentColor" fillOpacity=".24" />
        <path d="M12 2.5v2.7M12 18.8v2.7M2.5 12h2.7M18.8 12h2.7M5.2 5.2l2 2M16.8 16.8l2 2M18.8 5.2l-2 2M7.2 16.8l-2 2" />
      </>
    );
  } else if (glyph === "poison") {
    paths = (
      <>
        <path d="M12 3c-2.6 3.7-5.8 7-5.8 11.1A5.8 5.8 0 0 0 12 20a5.8 5.8 0 0 0 5.8-5.9C17.8 10 14.6 6.7 12 3z" fill="currentColor" fillOpacity=".22" />
        <path d="M9.6 12.2l1.2 1.6m2.4-1.6l1.2 1.6M9.8 16.4c1.3-.8 3.1-.8 4.4 0" />
      </>
    );
  } else if (glyph === "shout") {
    paths = (
      <>
        <path d="M4.2 10.2h3.2L11.5 6v12l-4.1-4.2H4.2z" fill="currentColor" fillOpacity=".22" />
        <path d="M14.5 9.2c1.4 1.6 1.4 4 0 5.6M17.3 6.8c2.6 3 2.6 7.4 0 10.4M20 4.8c1.4 2.1 2 4.6 2 7.2s-.6 5.1-2 7.2" />
      </>
    );
  } else if (glyph === "chaos") {
    paths = (
      <>
        <path d="M12 12c0-3.2 2.4-6.2 5.8-7.4M12 12c2.8 1.6 4.1 5.1 3.4 8.6M12 12c-2.8 1.6-6.5.9-8.8-1.9" />
        <path d="M15.6 3.5l2.6 1-1 2.6M17.5 19.2l-2.1 1.9-1.4-2.4M3.5 12.6L3 9.8l2.7.5" />
        <polygon points="12 9.8 14 11 14 13 12 14.2 10 13 10 11" fill="currentColor" fillOpacity=".35" />
      </>
    );
  } else if (glyph === "fire") {
    paths = (
      <>
        <path d="M12.5 3.2c1.2 3.1-1.3 4.8-.6 7.1.7 1.8 2.8 1.5 3.1-.5 2 1.8 2.7 4.2 1.7 6.8-.9 2.3-2.9 3.6-4.9 3.6-3 0-5.4-2.2-5.4-5.4 0-3.2 2.4-5.2 4-7.2.9-1.2 1.6-2.7 2.1-4.4z" fill="currentColor" fillOpacity=".22" />
        <path d="M11.6 13.6c-1 1-1.2 2.4-.2 3.4.9.9 2.3.8 2.8-.2.4-.8.1-1.8-.5-2.4" />
      </>
    );
  } else if (glyph === "frost") {
    paths = (
      <>
        <path d="M12 3v18M4.2 7.5l15.6 9M19.8 7.5l-15.6 9" />
        <path d="M9.8 5.2L12 7.4l2.2-2.2M9.8 18.8L12 16.6l2.2 2.2M5.4 10.2l2.2-.8-.8-2.2M17 16.8l-.8-2.2 2.2-.8M18.6 10.2l-2.2-.8.8-2.2M7 16.8l.8-2.2-2.2-.8" />
      </>
    );
  } else if (glyph === "shock") {
    paths = (
      <>
        <path d="M13.6 2.8L5.5 13h5.2l-1.5 8.2 9.3-11h-5.3l.4-7.4z" fill="currentColor" fillOpacity=".24" />
        <path d="M17.5 15.5l2.5 2M4.5 7l2 1.8" strokeOpacity=".7" />
      </>
    );
  } else if (glyph === "soul") {
    paths = (
      <>
        <polygon points="12 2.8 17.5 6.8 17.5 17.2 12 21.2 6.5 17.2 6.5 6.8" fill="currentColor" fillOpacity=".2" />
        <path d="M6.5 8.2L12 10.8l5.5-2.6M12 10.8v10.4" strokeOpacity=".7" />
        <path d="M12 5.4c1.3 1.4 1.3 2.8 0 4.2-1.3-1.4-1.3-2.8 0-4.2z" fill="currentColor" fillOpacity=".45" />
      </>
    );
  } else if (glyph === "vampiric") {
    paths = (
      <>
        <path d="M6.5 5.5l2 5 2-3.2M17.5 5.5l-2 5-2-3.2" />
        <path d="M12 8.8c-2.3 3-4.6 5.5-4.6 8.3a4.6 4.6 0 0 0 9.2 0c0-2.8-2.3-5.3-4.6-8.3z" fill="currentColor" fillOpacity=".26" />
      </>
    );
  } else if (glyph === "siphon-mp") {
    paths = (
      <>
        <path d="M5 7c0-2 3.1-3.2 7-3.2s7 1.2 7 3.2c0 2-3.1 3.2-7 3.2-3.1 0-5.2.8-5.2 2s2.2 2 5.2 2" />
        <path d="M12 10v10.5M9 17.5l3 3 3-3" />
      </>
    );
  } else if (glyph === "siphon-sp") {
    paths = (
      <>
        <path d="M4.5 8.5c3-3.5 7.5-3.5 10.5 0M19.5 15.5c-3 3.5-7.5 3.5-10.5 0" />
        <path d="M12.5 5.5l3 3-3.5 1.5M11.5 18.5l-3-3 3.5-1.5" />
        <path d="M12 9.5v5" />
      </>
    );
  } else if (glyph === "paralyze") {
    paths = (
      <>
        <path d="M6.5 3.8h11M6.5 20.2h11" />
        <path d="M7.8 3.8v3.6c0 2.1 2.1 3.6 4.2 4.6 2.1-1 4.2-2.5 4.2-4.6V3.8M7.8 20.2v-3.6c0-2.1 2.1-3.6 4.2-4.6 2.1 1 4.2 2.5 4.2 4.6v3.6" fill="currentColor" fillOpacity=".2" />
      </>
    );
  } else if (glyph === "banish") {
    paths = (
      <>
        <path d="M5.5 20V11a6.5 6.5 0 0 1 13 0v9" fill="currentColor" fillOpacity=".16" />
        <path d="M9 20v-7.5a3 3 0 0 1 6 0V20M4.5 18.5l15-13" />
      </>
    );
  } else if (glyph === "ward") {
    paths = (
      <>
        <path d="M12 3.2L5 6.4v5.3c0 4.6 3 8.1 7 9.5 4-1.4 7-4.9 7-9.5V6.4L12 3.2z" fill="currentColor" fillOpacity=".18" />
        <path d="M8.5 10.5l3.5-2.6 3.5 2.6M9 14.2l3 2.5 3-2.5" />
      </>
    );
  } else if (glyph === "moon") {
    paths = (
      <>
        <path d="M15.8 4.2a6.4 6.4 0 1 0 4 9.5 5.1 5.1 0 0 1-4-9.5z" fill="currentColor" fillOpacity=".24" />
        <path d="M3.5 19.5l5.2-6.2 3.5 4.1 2.8-3 5.5 5.1z" fill="currentColor" fillOpacity=".14" />
      </>
    );
  } else if (glyph === "nightingale") {
    paths = (
      <>
        <path d="M12 3.5L5.5 12.2 8.2 20h7.6l2.7-7.8L12 3.5z" fill="currentColor" fillOpacity=".2" />
        <path d="M9.5 10.5c1.6 1.2 3.4 1.2 5 0M12 13v4.5" />
      </>
    );
  } else if (glyph === "blade") {
    paths = (
      <>
        <path d="M19 4L7 16M6 12l5 5M4.5 18.5L7 16" />
        <path d="M5 4l12 12M18 12l-5 5M19.5 18.5L17 16" />
      </>
    );
  } else if (glyph === "bow") {
    paths = (
      <>
        <path d="M8 4.2c5.4 2.5 5.4 13.1 0 15.6" />
        <path d="M8 4.2v15.6" strokeOpacity=".5" />
        <path d="M4.2 12h15.6M16.5 8.8l3.3 3.2-3.3 3.2" />
      </>
    );
  } else if (glyph === "gauntlet") {
    paths = (
      <>
        <path d="M6.5 9.5a2 2 0 0 1 2-2h7a2 2 0 0 1 2 2v4.2l-2 2.3H8.5l-2-2.3V9.5z" fill="currentColor" fillOpacity=".2" />
        <path d="M8.8 7.5V5.2M12 7.5V4.6M15.2 7.5V5.2M8.5 16l-.7 4h8.4l-.7-4" />
      </>
    );
  } else if (glyph === "shield") {
    paths = (
      <>
        <path d="M12 3.2L5 6.4v5.4c0 4.6 3 8.1 7 9.4 4-1.3 7-4.8 7-9.4V6.4L12 3.2z" fill="currentColor" fillOpacity=".2" />
        <path d="M12 6.8v11M8 11h8" />
      </>
    );
  } else if (glyph === "anvil") {
    paths = (
      <>
        <path d="M4 9.5h15.5l-2.6 4H7.2L4 9.5z" fill="currentColor" fillOpacity=".22" />
        <path d="M8.5 13.5v4h7v-4M6 19.5h12M13.5 5.2l3.8-1.4.9 2.4-3.8 1.4z" />
      </>
    );
  } else if (glyph === "flask") {
    paths = (
      <>
        <path d="M9.5 4h5M10.5 4v4.4L5.8 16a2.8 2.8 0 0 0 2.4 4.2h7.6A2.8 2.8 0 0 0 18.2 16L13.5 8.4V4" fill="currentColor" fillOpacity=".2" />
        <path d="M7.8 15c1.6-.9 2.8.9 4.2 0s2.6-.9 4.2 0" />
      </>
    );
  } else if (glyph === "scales") {
    paths = (
      <>
        <path d="M12 4v15M8 19h8M5 7.5h14" />
        <path d="M5 7.5L3.2 13h3.6zM19 7.5L17.2 13h3.6z" fill="currentColor" fillOpacity=".22" />
      </>
    );
  } else if (glyph === "key") {
    paths = (
      <>
        <polygon points="4.2 12 6.2 8.6 10.2 8.6 12.2 12 10.2 15.4 6.2 15.4" fill="currentColor" fillOpacity=".2" />
        <path d="M12.2 12H20M17.2 12v3.2M19.8 12v2.4" />
      </>
    );
  } else if (glyph === "pack") {
    paths = (
      <>
        <rect x="6" y="7" width="12" height="12.5" rx="2.5" fill="currentColor" fillOpacity=".2" />
        <path d="M9.2 7V5.2a2.8 2.8 0 0 1 5.6 0V7M6 12h12M10.5 12v2.5h3V12" />
      </>
    );
  } else if (glyph === "heart") {
    paths = (
      <>
        <path d="M12 19.8s-7.2-4.4-7.2-9.8a4.1 4.1 0 0 1 7.2-2.7A4.1 4.1 0 0 1 19.2 10c0 5.4-7.2 9.8-7.2 9.8z" fill="currentColor" fillOpacity=".22" />
        <path d="M12 9.2v4.2M9.9 11.3h4.2" />
      </>
    );
  } else if (glyph === "magicka") {
    paths = (
      <>
        <path d="M14.5 4.2a7.6 7.6 0 1 0 4.8 13.5A6.2 6.2 0 0 1 14.5 4.2z" fill="currentColor" fillOpacity=".22" />
        <path d="M16.5 6.5v3.2M14.9 8.1h3.2M18.2 11.8l1.6 1.6" />
      </>
    );
  } else if (glyph === "vigor") {
    paths = (
      <>
        <path d="M4.8 14.5L12 7.2l7.2 7.3" />
        <path d="M7.2 18.5L12 13.5l4.8 5" />
        <path d="M12 4.2v3" />
      </>
    );
  } else if (glyph === "wave") {
    paths = (
      <>
        <path d="M3.8 10c2.2 0 2.2-2.5 4.5-2.5s2.3 2.5 4.5 2.5 2.3-2.5 4.5-2.5 2.9 2.5 2.9 2.5" />
        <path d="M3.8 15.5c2.2 0 2.2-2.5 4.5-2.5s2.3 2.5 4.5 2.5 2.3-2.5 4.5-2.5 2.9 2.5 2.9 2.5" />
      </>
    );
  } else if (glyph === "destruction") {
    paths = (
      <>
        <path d="M12 19.8V10.5M9.5 14l-3.5 3.5M14.5 14l3.5 3.5" />
        <path d="M12 3.2l4.2 6.8H7.8L12 3.2z" fill="currentColor" fillOpacity=".24" />
      </>
    );
  } else if (glyph === "conjuration") {
    paths = (
      <>
        <path d="M5.5 7.5C6.2 4.8 8.8 3.5 12 3.5s5.8 1.3 6.5 4M6.5 20V11a5.5 5.5 0 0 1 11 0v9" fill="currentColor" fillOpacity=".18" />
        <polygon points="12 8.2 15.6 15.2 8.4 15.2" />
      </>
    );
  } else if (glyph === "restoration") {
    paths = (
      <>
        <path d="M7 6.5h10l-1.5 6a3.6 3.6 0 0 1-7 0L7 6.5z" fill="currentColor" fillOpacity=".22" />
        <path d="M12 15.2v4.6M8.5 19.8h7M12 3v1.6M7.2 3.8l1.1 1.1M16.8 3.8l-1.1 1.1" />
      </>
    );
  } else if (glyph === "illusion") {
    paths = (
      <>
        <path d="M3.2 12s3.8-5.5 8.8-5.5 8.8 5.5 8.8 5.5-3.8 5.5-8.8 5.5S3.2 12 3.2 12z" fill="currentColor" fillOpacity=".18" />
        <path d="M12 9.2c-1.4 1.6-1.4 4 0 5.6 1.4-1.6 1.4-4 0-5.6z" fill="currentColor" fillOpacity=".35" />
      </>
    );
  } else if (glyph === "alteration") {
    paths = (
      <>
        <polygon points="12 3 19.8 7.5 19.8 16.5 12 21 4.2 16.5 4.2 7.5" fill="currentColor" fillOpacity=".18" />
        <polygon points="12 6.8 16.5 14.8 7.5 14.8" />
        <path d="M12 3v3.8M4.2 16.5l3.3-1.7M19.8 16.5l-3.3-1.7" />
      </>
    );
  }

  return (
    <span
      className={`enchant-sigil enchant-sigil--${variant}${size === "sm" ? " enchant-sigil--sm" : size === "lg" ? " enchant-sigil--lg" : ""}`}
      aria-hidden="true"
    >
      <svg width={px} height={px} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
        {paths}
      </svg>
    </span>
  );
}

function AltarCrestEmblem({ variant = "bind" }: { variant?: "bind" | "extract" }): React.JSX.Element {
  return (
    <svg
      width="20"
      height="20"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.65"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polygon
        points="12 2.4 18.8 5.2 21.6 12 18.8 18.8 12 21.6 5.2 18.8 2.4 12 5.2 5.2"
        fill="currentColor"
        fillOpacity=".14"
      />
      {variant === "extract" ? (
        <>
          <path d="M12 6.4v11.2M8.2 10.2L12 6.4l3.8 3.8" />
          <path d="M6.8 15.8h10.4" strokeOpacity=".65" />
        </>
      ) : (
        <>
          <polygon points="12 6.4 16.8 9.2 16.8 14.8 12 17.6 7.2 14.8 7.2 9.2" fill="currentColor" fillOpacity=".2" />
          <path d="M12 2.4v4M12 17.6v4M2.4 12h4.8M16.8 12h4.8" strokeOpacity=".75" />
        </>
      )}
    </svg>
  );
}

function AltarPlinthBackdrop(): React.JSX.Element {
  return (
    <svg
      className="enchanting-altar__plinth-rune"
      viewBox="0 0 64 64"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.2"
      aria-hidden="true"
    >
      <polygon points="32 4 51.8 12.2 60 32 51.8 51.8 32 60 12.2 51.8 4 32 12.2 12.2" fill="currentColor" fillOpacity=".05" />
      <polygon points="32 12 49.3 22 49.3 42 32 52 14.7 42 14.7 22" strokeOpacity=".45" />
      <path d="M32 4v8M32 52v8M4 32h8M52 32h8" strokeOpacity=".55" />
    </svg>
  );
}

function getCompactDisenchantStatus(
  entry: Pick<SkyUltraDisenchantableItem, "alreadyKnown" | "isQuestItem" | "isUniqueLocked">,
): string {
  if (entry.alreadyKnown) return "Known";
  if (entry.isQuestItem) return "Quest Item";
  if (entry.isUniqueLocked) return "Locked · Super 100";
  return "Protected";
}

export function reconcileExactGemAllocations(
  soulGems: ReadonlyArray<Pick<SkyUltraSoulGemEntry, "id" | "count" | "soulLevel" | "isReusable">>,
  current: Record<string, number>,
  requiredCount: number,
): Record<string, number> {
  const target = Math.max(1, Math.floor(requiredCount));
  const capMap = new Map<string, number>();
  const levelMap = new Map<string, number>();
  for (const g of soulGems) {
    const cap = g.isReusable ? Math.min(1, g.count) : Math.max(0, g.count);
    if (cap > 0) {
      capMap.set(g.id, cap);
      levelMap.set(g.id, g.soulLevel);
    }
  }
  const next: Record<string, number> = {};
  let total = 0;
  for (const [id, qty] of Object.entries(current)) {
    const cap = capMap.get(id) ?? 0;
    if (cap <= 0 || qty <= 0) continue;
    const clamped = Math.min(cap, Math.floor(qty));
    if (clamped > 0) {
      next[id] = clamped;
      total += clamped;
    }
  }
  if (total > target) {
    const allocatedIds = Object.keys(next).sort(
      (a, b) => (levelMap.get(a) ?? 0) - (levelMap.get(b) ?? 0) || a.localeCompare(b),
    );
    for (const id of allocatedIds) {
      if (total <= target) break;
      const drop = Math.min(next[id], total - target);
      next[id] -= drop;
      total -= drop;
      if (next[id] <= 0) delete next[id];
    }
  }
  if (total < target) {
    const fillOrder = [...soulGems].sort(
      (a, b) => b.soulLevel - a.soulLevel || b.count - a.count || a.id.localeCompare(b.id),
    );
    for (const g of fillOrder) {
      if (total >= target) break;
      const cap = capMap.get(g.id) ?? 0;
      const cur = next[g.id] ?? 0;
      const spare = Math.max(0, cap - cur);
      if (spare <= 0) continue;
      const take = Math.min(spare, target - total);
      next[g.id] = cur + take;
      total += take;
    }
  }
  return next;
}

export function rebalanceGemAllocation(
  soulGems: ReadonlyArray<Pick<SkyUltraSoulGemEntry, "id" | "count" | "soulLevel" | "isReusable">>,
  current: Record<string, number>,
  targetGemId: string,
  delta: number,
  requiredCount: number,
): Record<string, number> {
  const targetTotal = Math.max(1, Math.floor(requiredCount));
  const base = reconcileExactGemAllocations(soulGems, current, targetTotal);
  const capOf = (g: Pick<SkyUltraSoulGemEntry, "count" | "isReusable">) =>
    g.isReusable ? Math.min(1, g.count) : Math.max(0, g.count);
  const gem = soulGems.find((g) => g.id === targetGemId);
  if (!gem) return base;
  const targetCap = capOf(gem);
  if (targetCap <= 0) return base;

  if (targetTotal <= 1) {
    return delta > 0 ? { [targetGemId]: 1 } : base;
  }

  const next = { ...base };
  const curQty = next[targetGemId] ?? 0;
  const currentSum = Object.values(next).reduce((a, b) => a + b, 0);

  if (delta > 0) {
    const step = Math.min(delta, targetCap - curQty);
    if (step <= 0) return base;
    if (currentSum + step > targetTotal) {
      const donors = soulGems
        .filter((g) => g.id !== targetGemId && (next[g.id] ?? 0) > 0)
        .sort((a, b) => a.soulLevel - b.soulLevel || a.id.localeCompare(b.id));
      const maxDonor = donors.reduce((s, d) => s + (next[d.id] ?? 0), 0);
      const allowedStep = Math.min(step, (targetTotal - currentSum) + maxDonor);
      if (allowedStep <= 0) return base;
      let trim = Math.max(0, currentSum + allowedStep - targetTotal);
      next[targetGemId] = curQty + allowedStep;
      for (const donor of donors) {
        if (trim <= 0) break;
        const take = Math.min(next[donor.id] ?? 0, trim);
        next[donor.id] = (next[donor.id] ?? 0) - take;
        if (next[donor.id] <= 0) delete next[donor.id];
        trim -= take;
      }
      return reconcileExactGemAllocations(soulGems, next, targetTotal);
    }
    next[targetGemId] = curQty + step;
    return reconcileExactGemAllocations(soulGems, next, targetTotal);
  }

  if (delta < 0) {
    if (curQty <= 0) return base;
    const replacements = soulGems
      .filter((g) => g.id !== targetGemId && capOf(g) - (next[g.id] ?? 0) > 0)
      .sort((a, b) => b.soulLevel - a.soulLevel || b.count - a.count || a.id.localeCompare(b.id));
    const maxSpare = replacements.reduce((s, r) => s + Math.max(0, capOf(r) - (next[r.id] ?? 0)), 0);
    if (maxSpare <= 0) {
      return base;
    }
    const drop = Math.min(curQty, -delta, maxSpare);
    next[targetGemId] = curQty - drop;
    if (next[targetGemId] <= 0) delete next[targetGemId];
    let rem = drop;
    for (const r of replacements) {
      if (rem <= 0) break;
      const spare = Math.max(0, capOf(r) - (next[r.id] ?? 0));
      const add = Math.min(spare, rem);
      if (add > 0) {
        next[r.id] = (next[r.id] ?? 0) + add;
        rem -= add;
      }
    }
    return reconcileExactGemAllocations(soulGems, next, targetTotal);
  }

  return base;
}

function normalizeFormIdKey(id?: string): string {
  const raw = (id ?? "").trim();
  if (!raw) return "";
  let parsed = NaN;
  if (/^0x/i.test(raw)) {
    parsed = Number.parseInt(raw.slice(2), 16);
  } else if (/[a-f]/i.test(raw) || (raw.length === 8 && raw.startsWith("0"))) {
    parsed = Number.parseInt(raw, 16);
  } else if (/^\d+$/.test(raw)) {
    parsed = Number.parseInt(raw, 10);
  }
  if (!Number.isFinite(parsed) || parsed <= 0) {
    return raw.toUpperCase();
  }
  return Math.trunc(parsed).toString(16).toUpperCase().padStart(8, "0");
}

function isJewelryItem(entry: Pick<SkyUltraEnchantingItem, "category" | "armorTypeLabel" | "name">): boolean {
  const cat = (entry.category ?? "").toLowerCase();
  if (cat === "jewelry") return true;
  if (/jewelry|ring|necklace|amulet|circlet/i.test(entry.armorTypeLabel ?? "")) return true;
  return /\b(ring|necklace|amulet|circlet)\b/i.test(entry.name);
}

/**
 * Checks whether a known enchantment can bind onto the given item.
 * When `unrestrictedSlots` is true (Super Enchanting at skill 100),
 * apparel slot restrictions are bypassed while still keeping weapon vs apparel separation.
 */
export function isEnchantmentCompatibleWithItem(
  enchantment: Pick<SkyUltraKnownEnchantment, "targetKind" | "wornRestrictionIds" | "wornRestrictionLabels">,
  item: Pick<SkyUltraEnchantingItem, "itemKind" | "keywordIds"> | null | undefined,
  unrestrictedSlots = false,
): EnchantmentCompatibilityResult {
  if (!item) {
    return { compatible: true };
  }
  if (enchantment.targetKind !== item.itemKind) {
    return {
      compatible: false,
      reason:
        enchantment.targetKind === "weapon"
          ? "For weapons only"
          : "For armor, clothing & jewelry only",
    };
  }
  if (unrestrictedSlots) {
    return { compatible: true };
  }
  const restrictions = enchantment.wornRestrictionIds ?? [];
  if (item.itemKind === "armor" && restrictions.length > 0) {
    const itemKeywords = new Set((item.keywordIds ?? []).map(normalizeFormIdKey).filter(Boolean));
    const matchesAny = restrictions.some((kw) => itemKeywords.has(normalizeFormIdKey(kw)));
    if (!matchesAny) {
      const labels = (enchantment.wornRestrictionLabels ?? []).filter(Boolean);
      return {
        compatible: false,
        reason:
          labels.length > 0
            ? `Requires: ${labels.slice(0, 3).join(", ")}${labels.length > 3 ? "…" : ""}`
            : "Incompatible equipment slot",
      };
    }
  }
  return { compatible: true };
}

/**
 * Computes weapon charge capacity, cost per strike, and remaining uses for a
 * chosen power slider position (`0.15`..`1.0`).
 */
export function computePreviewWeaponUses(
  soulPoints: number,
  baseCost: number,
  skillMultiplier: number,
  chargeSlider: number,
): WeaponChargePreview {
  const safeSoul = Number.isFinite(soulPoints) && soulPoints > 0 ? soulPoints : 0;
  const safeCost = Number.isFinite(baseCost) && baseCost > 0 ? baseCost : 1;
  const safeSkill = Number.isFinite(skillMultiplier) && skillMultiplier > 0 ? skillMultiplier : 1;
  const clampedPower = Math.max(0.15, Math.min(1.0, Number.isFinite(chargeSlider) ? chargeSlider : 0.8));

  const magnitudeScale = safeSkill * clampedPower;
  const costPerUse = Math.max(1, Math.round(safeCost * Math.pow(clampedPower, 1.1)));
  const maxCharge = Math.round(safeSoul);
  const uses = maxCharge > 0 ? Math.max(1, Math.floor(maxCharge / costPerUse)) : 0;

  return {
    uses,
    maxCharge,
    costPerUse,
    magnitudeScale,
  };
}

/**
 * Formats an enchantment effect's Skyrim description template (`<mag>`, `<dur>`)
 * using the computed enchanting magnitude and duration multipliers.
 */
export function formatScaledEffectText(
  effect: Pick<
    SkyUltraKnownEnchantmentEffect,
    | "name"
    | "descriptionTemplate"
    | "baseMagnitude"
    | "baseDuration"
    | "powerAffectsMagnitude"
    | "powerAffectsDuration"
    | "noMagnitude"
    | "perkBonusMult"
  >,
  magnitudeScale: number,
  durationScale?: number,
): string {
  const perkMult = effect.perkBonusMult && effect.perkBonusMult > 0 ? effect.perkBonusMult : 1;
  const effectiveMagMult =
    effect.powerAffectsMagnitude || !effect.powerAffectsDuration ? magnitudeScale * perkMult : perkMult;
  const effectiveDurMult =
    durationScale !== undefined
      ? durationScale * perkMult
      : effect.powerAffectsDuration && !effect.powerAffectsMagnitude
        ? magnitudeScale * perkMult
        : 1;

  const rawMag = effect.noMagnitude ? 0 : effect.baseMagnitude * effectiveMagMult;
  const scaledMag =
    effect.noMagnitude || effect.baseMagnitude <= 0 ? 0 : Math.max(1, Math.round(rawMag));
  const scaledDur =
    effect.baseDuration <= 0 ? 0 : Math.max(1, Math.round(effect.baseDuration * effectiveDurMult));

  const tpl = (effect.descriptionTemplate ?? "").trim();
  if (tpl.length > 0) {
    return tpl
      .replace(/<\s*mag\s*>/gi, String(scaledMag))
      .replace(/<\s*dur\s*>/gi, String(scaledDur))
      .trim();
  }

  if (scaledMag > 0 && scaledDur > 0) {
    return `${effect.name}: ${scaledMag} pts for ${scaledDur}s`;
  }
  if (scaledMag > 0) {
    return `${effect.name}: ${scaledMag}`;
  }
  if (scaledDur > 0) {
    return `${effect.name} for ${scaledDur}s`;
  }
  return effect.name;
}

function toThumbnailItem(
  entry: Pick<
    SkyUltraEnchantingItem,
    "itemId" | "name" | "category" | "count" | "value" | "weight" | "equipped" | "damage" | "armorRating"
  > & { enchanted?: boolean },
): Item {
  return {
    id: entry.itemId,
    name: entry.name,
    category: entry.category || "misc",
    count: entry.count,
    value: entry.value,
    weight: entry.weight,
    equipped: entry.equipped,
    enchanted: Boolean(entry.enchanted),
    damage: entry.damage,
    armorRating: entry.armorRating,
  };
}

function toSoulGemThumbnailItem(gem: SkyUltraSoulGemEntry): Item {
  return {
    id: gem.gemId,
    name: gem.name,
    category: "soulGem",
    count: gem.count,
    value: gem.value,
    weight: gem.weight,
    equipped: false,
    enchanted: true,
  };
}

const ITEMS_PER_PAGE = 18;
const ENCH_GRID_ITEMS_PER_PAGE = 20;

const PREVIEW_SNAPSHOT: SkyUltraEnchantingSnapshot = {
  available: true,
  stationName: "Arcane Enchanter",
  enchantingSkill: 68,
  enchantingBaseSkill: 65,
  skillMultiplier: 1.54,
  maxEnchantments: 2,
  hasExtraEffect: true,
  superEnchantingUnlocked: false,
  superEnchantingEnabled: false,
  superEnchantingActive: false,
  unlimitedEnchantments: false,
  playerName: "Dragonborn",
  playerGold: 6750,
  playerCarryWeight: { current: 214, max: 350 },
  enchantableItems: [
    {
      id: "80313:0",
      itemId: "80313",
      stackIndex: 0,
      name: "Daedric Sword",
      itemKind: "weapon",
      category: "weapon",
      count: 1,
      value: 1250,
      weight: 16,
      equipped: true,
      damage: 42,
      weaponType: "One-Handed Sword",
      keywordIds: ["124689"],
    },
    {
      id: "80225:0",
      itemId: "80225",
      stackIndex: 0,
      name: "Ebony Armor",
      itemKind: "armor",
      category: "armor",
      count: 1,
      value: 1500,
      weight: 38,
      equipped: false,
      armorRating: 96,
      armorTypeLabel: "Heavy Armor · Cuirass",
      keywordIds: ["442604"],
    },
    {
      id: "118571:0",
      itemId: "118571",
      stackIndex: 0,
      name: "Gold Diamond Ring",
      itemKind: "armor",
      category: "armor",
      count: 1,
      value: 900,
      weight: 0.5,
      equipped: false,
      armorRating: 0,
      armorTypeLabel: "Jewelry · Ring",
      keywordIds: ["441321"],
    },
  ],
  disenchantableItems: [
    {
      id: "709696:0",
      itemId: "709696",
      stackIndex: 0,
      name: "Elven Bow of Burning",
      itemKind: "weapon",
      category: "weapon",
      count: 1,
      value: 820,
      weight: 12,
      equipped: false,
      damage: 24,
      weaponType: "Bow",
      enchantmentId: "286810",
      baseEnchantmentId: "286810",
      enchantmentName: "Fire Damage",
      effects: [
        {
          name: "Fire Damage",
          description: "Burns the target for 15 points. Targets on fire take extra damage.",
          magnitude: 15,
          duration: 1,
        },
      ],
      alreadyKnown: true,
      isDisallowed: false,
      isQuestItem: false,
      isPlayerCreated: false,
      canDisenchant: false,
      blockReason: "Enchantment already known.",
      estimatedXp: 0,
    },
    {
      id: "741619:0",
      itemId: "741619",
      stackIndex: 0,
      name: "Ring of Peerless Smithing",
      itemKind: "armor",
      category: "armor",
      count: 1,
      value: 1480,
      weight: 0.3,
      equipped: false,
      armorTypeLabel: "Jewelry · Ring",
      enchantmentId: "709759",
      baseEnchantmentId: "709759",
      enchantmentName: "Fortify Smithing",
      effects: [
        {
          name: "Fortify Smithing",
          description: "Weapons and armor can be improved 25% better.",
          magnitude: 25,
          duration: 0,
        },
      ],
      alreadyKnown: false,
      isDisallowed: false,
      isQuestItem: false,
      isPlayerCreated: false,
      canDisenchant: true,
      estimatedXp: 2.2,
    },
  ],
  knownEnchantments: [
    {
      id: "286810",
      name: "Fire Damage",
      targetKind: "weapon",
      school: "Destruction",
      baseCost: 22,
      wornRestrictionIds: [],
      wornRestrictionLabels: [],
      effects: [
        {
          name: "Fire Damage",
          descriptionTemplate: "Burns the target for <mag> points. Targets on fire take extra damage.",
          baseMagnitude: 10,
          baseDuration: 1,
          area: 0,
          powerAffectsMagnitude: true,
          powerAffectsDuration: false,
          noMagnitude: false,
          perkBonusMult: 1.25,
        },
      ],
    },
    {
      id: "286811",
      name: "Soul Trap",
      targetKind: "weapon",
      school: "Conjuration",
      baseCost: 14,
      wornRestrictionIds: [],
      wornRestrictionLabels: [],
      effects: [
        {
          name: "Soul Trap",
          descriptionTemplate: "If target dies within <dur> seconds, fills a soul gem.",
          baseMagnitude: 1,
          baseDuration: 5,
          area: 0,
          powerAffectsMagnitude: false,
          powerAffectsDuration: true,
          noMagnitude: true,
          perkBonusMult: 1,
        },
      ],
    },
    {
      id: "499970",
      name: "Fortify Health",
      targetKind: "armor",
      school: "Restoration",
      baseCost: 24,
      wornRestrictionIds: ["442604", "441321"],
      wornRestrictionLabels: ["Chest / Cuirass", "Ring"],
      effects: [
        {
          name: "Fortify Health",
          descriptionTemplate: "Increases your Health by <mag> points.",
          baseMagnitude: 20,
          baseDuration: 0,
          area: 0,
          powerAffectsMagnitude: true,
          powerAffectsDuration: false,
          noMagnitude: false,
          perkBonusMult: 1,
        },
      ],
    },
  ],
  soulGems: [
    {
      id: "189695:0",
      gemId: "189695",
      stackIndex: 0,
      name: "Grand Soul Gem (Grand)",
      soulLevel: 5,
      soulLabel: "Grand",
      soulPoints: 3000,
      armorScale: 1.0,
      count: 3,
      value: 500,
      weight: 0.5,
      isReusable: false,
    },
    {
      id: "408361:0",
      gemId: "408361",
      stackIndex: 0,
      name: "The Black Star (Grand)",
      soulLevel: 5,
      soulLabel: "Grand",
      soulPoints: 3000,
      armorScale: 1.0,
      count: 1,
      value: 1000,
      weight: 1.0,
      isReusable: true,
    },
  ],
  message: "Studio preview data",
};

export default function EnchantingPage({
  player,
  bridge,
  obscured = false,
  displayMode = "half",
  pauseWorld = true,
  onChangeDisplayMode,
  onTogglePauseWorld,
  onClose,
}: EnchantingPageProps) {
  const gameApi = useMemo(() => (bridge ? createSkyUltraGame(bridge) : null), [bridge]);

  const [snapshot, setSnapshot] = useState<SkyUltraEnchantingSnapshot>(() =>
    bridge
      ? {
          ...PREVIEW_SNAPSHOT,
          available: false,
          enchantableItems: [],
          disenchantableItems: [],
          knownEnchantments: [],
          soulGems: [],
        }
      : PREVIEW_SNAPSHOT,
  );
  const [loading, setLoading] = useState<boolean>(Boolean(bridge));
  const [busy, setBusy] = useState<boolean>(false);

  const [activeTab, setActiveTab] = useState<EnchantingTab>("enchant");
  const [slotStep, setSlotStep] = useState<EnchantSlotStep>("item");

  const [selectedItemIds, setSelectedItemIds] = useState<string[]>([]);
  const [itemQuantities, setItemQuantities] = useState<Record<string, number>>({});
  const [itemCustomNames, setItemCustomNames] = useState<Record<string, string>>({});
  const [selectedEnchIds, setSelectedEnchIds] = useState<string[]>([]);
  const [gemAllocations, setGemAllocations] = useState<Record<string, number>>({});
  const [selectedDisenchantId, setSelectedDisenchantId] = useState<string>("");

  const [powerSlider, setPowerSlider] = useState<number>(0.8);

  const [renamingEnchId, setRenamingEnchId] = useState<string | null>(null);
  const [enchRenameInput, setEnchRenameInput] = useState<string>("");
  const [renamingBusy, setRenamingBusy] = useState<boolean>(false);

  const [viewLayout, setViewLayout] = useState<"list" | "grid">("list");
  const [search, setSearch] = useState<string>("");
  const [itemFilter, setItemFilter] = useState<ItemFilter>("all");
  const [enchFilter, setEnchFilter] = useState<EnchFilter>("compatible");
  const [gemFilter, setGemFilter] = useState<GemFilter>("all");
  const [disenchantFilter, setDisenchantFilter] = useState<DisenchantFilter>("learnable");
  const [sortBy, setSortBy] = useState<SortBy>("name");
  const [pageIndex, setPageIndex] = useState<number>(0);

  const [confirmDisenchantArmed, setConfirmDisenchantArmed] = useState<boolean>(false);
  const [statusMessage, setStatusMessage] = useState<{ kind: "info" | "success" | "error"; text: string } | null>(null);

  const searchInputRef = useRef<HTMLInputElement | null>(null);
  const selectedItemIdsRef = useRef<string[]>(selectedItemIds);
  selectedItemIdsRef.current = selectedItemIds;
  const itemQuantitiesRef = useRef<Record<string, number>>(itemQuantities);
  itemQuantitiesRef.current = itemQuantities;

  const syncSelections = useCallback((next: SkyUltraEnchantingSnapshot) => {
    setSnapshot(next);
    if (!next.available) return;

    const itemMap = new Map(next.enchantableItems.map((it) => [it.id, it]));
    const isSuper = Boolean(next.superEnchantingActive);
    const prevIds = selectedItemIdsRef.current;
    const prevQtyMap = itemQuantitiesRef.current;
    const nextItemIds = (() => {
      const kept = prevIds.filter((id) => itemMap.has(id));
      const trimmed = isSuper ? kept : kept.slice(0, 1);
      if (trimmed.length > 0) return trimmed;
      return next.enchantableItems[0] ? [next.enchantableItems[0].id] : [];
    })();
    setSelectedItemIds(nextItemIds);

    const nextQuantities: Record<string, number> = {};
    for (const id of nextItemIds) {
      const item = itemMap.get(id);
      if (!item) continue;
      nextQuantities[id] = isSuper ? Math.max(1, Math.min(item.count, prevQtyMap[id] ?? 1)) : 1;
    }
    setItemQuantities(nextQuantities);
    setItemCustomNames((prev) => {
      const kept: Record<string, string> = {};
      for (const id of nextItemIds) {
        if (prev[id]) kept[id] = prev[id];
      }
      return kept;
    });

    const pieceCount = nextItemIds.reduce((sum, id) => sum + (nextQuantities[id] ?? 1), 0);
    setGemAllocations((prev) => reconcileExactGemAllocations(next.soulGems, prev, Math.max(1, pieceCount)));

    setSelectedDisenchantId((prev) =>
      prev && next.disenchantableItems.some((it) => it.id === prev)
        ? prev
        : next.disenchantableItems.find((it) => it.canDisenchant)?.id ?? next.disenchantableItems[0]?.id ?? "",
    );
  }, []);

  const loadSnapshot = useCallback(async () => {
    if (!gameApi) {
      syncSelections(PREVIEW_SNAPSHOT);
      setLoading(false);
      return;
    }
    try {
      setLoading(true);
      const next = await gameApi.enchantingSnapshot();
      syncSelections(next);
    } catch (err) {
      setStatusMessage({
        kind: "error",
        text: err instanceof Error ? err.message : "Failed to load Arcane Enchanter data.",
      });
    } finally {
      setLoading(false);
    }
  }, [gameApi, syncSelections]);

  useEffect(() => {
    void loadSnapshot();
  }, [loadSnapshot]);

  useEffect(() => {
    if (!bridge) return;
    const offMode = bridge.on<{ active?: boolean }>("skyultra.enchantingMode", (payload) => {
      if (payload?.active) {
        setConfirmDisenchantArmed(false);
        setStatusMessage(null);
        setRenamingEnchId(null);
        void loadSnapshot();
      }
    });
    const offVis = bridge.on<{ visible?: boolean }>("skyultra.visibility", (payload) => {
      if (payload?.visible) {
        void loadSnapshot();
      }
    });
    return () => {
      offMode();
      offVis();
    };
  }, [bridge, loadSnapshot]);

  const superActive = Boolean(snapshot.superEnchantingActive);
  const unlimitedSlots = Boolean(snapshot.unlimitedEnchantments || superActive);
  const maxEnchantments = unlimitedSlots ? 64 : Math.max(1, snapshot.maxEnchantments ?? 1);
  const enchantingSkill = snapshot.enchantingSkill ?? 15;
  const enchantingBaseSkill = snapshot.enchantingBaseSkill ?? enchantingSkill;
  const skillMultiplier = snapshot.skillMultiplier ?? 1;

  const selectedItems = useMemo(() => {
    const map = new Map(snapshot.enchantableItems.map((it) => [it.id, it]));
    return selectedItemIds.map((id) => map.get(id)).filter((it): it is SkyUltraEnchantingItem => Boolean(it));
  }, [snapshot.enchantableItems, selectedItemIds]);

  const selectedItem = selectedItems[0] ?? null;
  const batchKind = selectedItem?.itemKind ?? null;
  const getItemQty = useCallback(
    (item: Pick<SkyUltraEnchantingItem, "id" | "count">): number =>
      superActive ? Math.max(1, Math.min(Math.max(1, item.count), itemQuantities[item.id] ?? 1)) : 1,
    [superActive, itemQuantities],
  );
  const totalPieceCount = useMemo(
    () => selectedItems.reduce((sum, it) => sum + getItemQty(it), 0),
    [selectedItems, getItemQty],
  );
  const requiredGemCount = Math.max(1, totalPieceCount);

  const selectedEnchantments = useMemo(
    () =>
      selectedEnchIds
        .map((id) => snapshot.knownEnchantments.find((e) => e.id === id))
        .filter((e): e is SkyUltraKnownEnchantment => Boolean(e)),
    [snapshot.knownEnchantments, selectedEnchIds],
  );

  const allocatedGemList = useMemo(() => {
    const out: SkyUltraSoulGemEntry[] = [];
    for (const gem of snapshot.soulGems) {
      const cap = gem.isReusable ? Math.min(1, gem.count) : gem.count;
      const qty = Math.min(cap, Math.max(0, gemAllocations[gem.id] ?? 0));
      for (let i = 0; i < qty; i += 1) {
        out.push(gem);
      }
    }
    return out;
  }, [snapshot.soulGems, gemAllocations]);

  const selectedGem = allocatedGemList[0] ?? null;

  const totalAvailableSoulGems = useMemo(
    () =>
      snapshot.soulGems.reduce(
        (sum, gem) => sum + (gem.isReusable ? Math.min(1, gem.count) : gem.count),
        0,
      ),
    [snapshot.soulGems],
  );

  const selectedDisenchantItem = useMemo(
    () => snapshot.disenchantableItems.find((it) => it.id === selectedDisenchantId) ?? null,
    [snapshot.disenchantableItems, selectedDisenchantId],
  );

  // Drop incompatible enchantments only when primary vessel item is present and incompatible.
  useEffect(() => {
    if (!selectedItem) return;
    setSelectedEnchIds((prev) =>
      prev.filter((id) => {
        const ench = snapshot.knownEnchantments.find((k) => k.id === id);
        return ench ? isEnchantmentCompatibleWithItem(ench, selectedItem, superActive).compatible : false;
      }),
    );
  }, [selectedItem, snapshot.knownEnchantments, superActive]);

  // Always keep allocated Soul Gems strictly auto-filled & equal to totalPieceCount.
  useEffect(() => {
    setGemAllocations((prev) => reconcileExactGemAllocations(snapshot.soulGems, prev, requiredGemCount));
  }, [snapshot.soulGems, requiredGemCount]);

  useEffect(() => {
    setPageIndex(0);
  }, [activeTab, slotStep, search, itemFilter, enchFilter, gemFilter, disenchantFilter, sortBy]);

  useEffect(() => {
    setConfirmDisenchantArmed(false);
  }, [selectedDisenchantId, activeTab]);

  const queryLower = search.trim().toLowerCase();

  const filteredItems = useMemo(() => {
    const list = snapshot.enchantableItems.filter((it) => {
      const jewelry = isJewelryItem(it);
      if (itemFilter === "weapon" && it.itemKind !== "weapon") return false;
      if (itemFilter === "armor" && (it.itemKind !== "armor" || jewelry)) return false;
      if (itemFilter === "jewelry" && !jewelry) return false;
      if (
        queryLower &&
        !it.name.toLowerCase().includes(queryLower) &&
        !it.category.toLowerCase().includes(queryLower) &&
        !(it.armorTypeLabel ?? "").toLowerCase().includes(queryLower) &&
        !(it.weaponType ?? "").toLowerCase().includes(queryLower)
      ) {
        return false;
      }
      return true;
    });
    return [...list].sort((a, b) => {
      if (sortBy === "value") return b.value - a.value || a.name.localeCompare(b.name);
      if (sortBy === "stat") {
        const sa = a.damage ?? a.armorRating ?? 0;
        const sb = b.damage ?? b.armorRating ?? 0;
        return sb - sa || a.name.localeCompare(b.name);
      }
      return a.name.localeCompare(b.name);
    });
  }, [snapshot.enchantableItems, itemFilter, queryLower, sortBy]);

  const filteredEnchantments = useMemo(() => {
    return snapshot.knownEnchantments
      .filter((ench) => {
        if (enchFilter === "weapon" && ench.targetKind !== "weapon") return false;
        if (enchFilter === "armor" && ench.targetKind !== "armor") return false;
        if (enchFilter === "compatible" && selectedItem) {
          if (!isEnchantmentCompatibleWithItem(ench, selectedItem, superActive).compatible) return false;
        }
        if (queryLower) {
          const hitName = ench.name.toLowerCase().includes(queryLower);
          const hitOrig = (ench.originalName ?? "").toLowerCase().includes(queryLower);
          const hitSchool = ench.school.toLowerCase().includes(queryLower);
          const hitEff = ench.effects.some((ef) => ef.name.toLowerCase().includes(queryLower));
          if (!hitName && !hitOrig && !hitSchool && !hitEff) return false;
        }
        return true;
      })
      .sort((a, b) => {
        const ca = isEnchantmentCompatibleWithItem(a, selectedItem, superActive).compatible ? 0 : 1;
        const cb = isEnchantmentCompatibleWithItem(b, selectedItem, superActive).compatible ? 0 : 1;
        if (ca !== cb) return ca - cb;
        return a.name.localeCompare(b.name);
      });
  }, [snapshot.knownEnchantments, enchFilter, selectedItem, superActive, queryLower]);

  const filteredGems = useMemo(() => {
    const list = snapshot.soulGems.filter((gem) => {
      if (gemFilter === "grand" && gem.soulLevel < 5) return false;
      if (gemFilter === "reusable" && !gem.isReusable) return false;
      if (queryLower && !gem.name.toLowerCase().includes(queryLower) && !gem.soulLabel.toLowerCase().includes(queryLower)) {
        return false;
      }
      return true;
    });
    return [...list].sort((a, b) => {
      if (sortBy === "count") return b.count - a.count || b.soulLevel - a.soulLevel;
      if (sortBy === "name") return a.name.localeCompare(b.name);
      return b.soulLevel - a.soulLevel || (b.isReusable ? 1 : 0) - (a.isReusable ? 1 : 0) || a.name.localeCompare(b.name);
    });
  }, [snapshot.soulGems, gemFilter, queryLower, sortBy]);

  const filteredDisenchantItems = useMemo(() => {
    const list = snapshot.disenchantableItems.filter((it) => {
      if (disenchantFilter === "learnable" && !it.canDisenchant) return false;
      if (disenchantFilter === "known" && !it.alreadyKnown) return false;
      if (disenchantFilter === "blocked" && (it.canDisenchant || it.alreadyKnown)) return false;
      if (
        queryLower &&
        !it.name.toLowerCase().includes(queryLower) &&
        !it.enchantmentName.toLowerCase().includes(queryLower)
      ) {
        return false;
      }
      return true;
    });
    return [...list].sort((a, b) => {
      if (a.canDisenchant !== b.canDisenchant) return a.canDisenchant ? -1 : 1;
      if (sortBy === "value") return b.value - a.value;
      return a.name.localeCompare(b.name);
    });
  }, [snapshot.disenchantableItems, disenchantFilter, queryLower, sortBy]);

  const totalActiveCount =
    activeTab === "disenchant"
      ? filteredDisenchantItems.length
      : slotStep === "item"
        ? filteredItems.length
        : slotStep === "enchantment"
          ? filteredEnchantments.length
          : filteredGems.length;

  const pageSize =
    viewLayout === "grid"
      ? ENCH_GRID_ITEMS_PER_PAGE
      : ITEMS_PER_PAGE;
  const totalPages = Math.max(1, Math.ceil(totalActiveCount / pageSize));
  const safePage = Math.min(pageIndex, totalPages - 1);
  const pageStart = safePage * pageSize;
  const pageEnd = pageStart + pageSize;

  const renamingGridEnch =
    activeTab === "enchant" &&
    slotStep === "enchantment" &&
    viewLayout === "grid" &&
    renamingEnchId
      ? filteredEnchantments.find((e) => e.id === renamingEnchId) ??
        snapshot.knownEnchantments.find((e) => e.id === renamingEnchId) ??
        null
      : null;

  const isWeaponCraft = selectedItem?.itemKind === "weapon";
  const combinedBaseCost = useMemo(
    () => selectedEnchantments.reduce((acc, ench) => acc + Math.max(1, ench.baseCost), 0),
    [selectedEnchantments],
  );

  const weaponPreview = useMemo(
    () =>
      computePreviewWeaponUses(
        selectedGem?.soulPoints ?? 0,
        combinedBaseCost || 20,
        skillMultiplier,
        powerSlider,
      ),
    [selectedGem, combinedBaseCost, skillMultiplier, powerSlider],
  );

  const effectiveMagnitudeScale = useMemo(() => {
    if (isWeaponCraft) {
      return weaponPreview.magnitudeScale;
    }
    const gemScale = selectedGem?.armorScale && selectedGem.armorScale > 0 ? selectedGem.armorScale : 1;
    return skillMultiplier * gemScale;
  }, [isWeaponCraft, weaponPreview.magnitudeScale, skillMultiplier, selectedGem]);

  const canCraftEnchant = useMemo(() => {
    if (selectedItems.length === 0 || totalPieceCount <= 0 || selectedEnchantments.length === 0) return false;
    if (allocatedGemList.length !== totalPieceCount) return false;
    if (!unlimitedSlots && selectedEnchantments.length > maxEnchantments) return false;
    if (selectedItems.some((it) => it.itemKind !== selectedItems[0].itemKind)) return false;
    return selectedItems.every((it) =>
      selectedEnchantments.every((ench) => isEnchantmentCompatibleWithItem(ench, it, superActive).compatible),
    );
  }, [selectedItems, totalPieceCount, allocatedGemList.length, selectedEnchantments, unlimitedSlots, maxEnchantments, superActive]);

  const setBatchItemQuantity = useCallback((item: SkyUltraEnchantingItem, nextQty: number) => {
    const clamped = Math.max(1, Math.min(Math.max(1, item.count), Math.floor(nextQty)));
    setItemQuantities((prev) => ({ ...prev, [item.id]: clamped }));
  }, []);

  const setBatchItemName = useCallback((itemId: string, nextName: string) => {
    setItemCustomNames((prev) => {
      if (!nextName) {
        const next = { ...prev };
        delete next[itemId];
        return next;
      }
      return { ...prev, [itemId]: nextName };
    });
  }, []);

  const toggleTargetItem = useCallback(
    (item: SkyUltraEnchantingItem) => {
      if (!superActive) {
        setSelectedItemIds([item.id]);
        setItemQuantities({ [item.id]: 1 });
        setStatusMessage(null);
        if (selectedEnchantments.length === 0) {
          setSlotStep("enchantment");
        }
        return;
      }

      setSelectedItemIds((prev) => {
        if (prev.includes(item.id)) {
          setItemQuantities((q) => {
            const next = { ...q };
            delete next[item.id];
            return next;
          });
          setItemCustomNames((n) => {
            const next = { ...n };
            delete next[item.id];
            return next;
          });
          return prev.filter((id) => id !== item.id);
        }
        const firstExisting = snapshot.enchantableItems.find((cand) => cand.id === prev[0]);
        if (firstExisting && firstExisting.itemKind !== item.itemKind) {
          setStatusMessage({
            kind: "error",
            text: `Cannot mix weapons and apparel in one batch (${item.itemKind === "weapon" ? "weapons" : "apparel"} use separate enchantments). Clear batch to switch type.`,
          });
          return prev;
        }
        setItemQuantities((q) => ({ ...q, [item.id]: 1 }));
        setStatusMessage(null);
        return [...prev, item.id];
      });
    },
    [superActive, selectedEnchantments.length, snapshot.enchantableItems],
  );

  const toggleEnchantment = useCallback(
    (ench: SkyUltraKnownEnchantment) => {
      if (selectedItem && !isEnchantmentCompatibleWithItem(ench, selectedItem, superActive).compatible) {
        return;
      }
      setSelectedEnchIds((prev) => {
        if (prev.includes(ench.id)) {
          return prev.filter((id) => id !== ench.id);
        }
        if (unlimitedSlots) {
          return [...prev, ench.id];
        }
        if (maxEnchantments === 1) {
          return [ench.id];
        }
        if (prev.length >= maxEnchantments) {
          return [...prev.slice(1), ench.id];
        }
        return [...prev, ench.id];
      });
    },
    [selectedItem, superActive, unlimitedSlots, maxEnchantments],
  );

  const adjustGemAllocation = useCallback(
    (gem: SkyUltraSoulGemEntry, delta: number) => {
      setGemAllocations((prev) =>
        rebalanceGemAllocation(snapshot.soulGems, prev, gem.id, delta, requiredGemCount),
      );
    },
    [snapshot.soulGems, requiredGemCount],
  );

  const handlePerformEnchant = useCallback(async () => {
    if (!canCraftEnchant || busy) return;
    const singleCustomName = selectedItem ? (itemCustomNames[selectedItem.id] ?? "").trim() : "";
    if (!gameApi) {
      setStatusMessage({
        kind: "success",
        text:
          totalPieceCount > 1
            ? `Studio preview: Super-enchanted ${totalPieceCount} items.`
            : `Studio preview: Enchanted ${singleCustomName || selectedItem?.name}.`,
      });
      return;
    }
    if (selectedItems.length === 0 || allocatedGemList.length !== totalPieceCount) return;
    setBusy(true);
    setStatusMessage(null);
    try {
      const gemsPayload = allocatedGemList.slice(0, totalPieceCount).map((g) => ({
        soulGemId: g.gemId,
        soulGemStackIndex: g.stackIndex,
      }));
      const res =
        totalPieceCount > 1 || selectedItems.length > 1
          ? await gameApi.enchantItem({
              items: selectedItems.map((it) => ({
                itemId: it.itemId,
                stackIndex: it.stackIndex,
                count: getItemQty(it),
                customName: (itemCustomNames[it.id] ?? "").trim() || undefined,
              })),
              enchantmentIds: selectedEnchantments.map((e) => e.id),
              soulGems: gemsPayload,
              chargeSlider: isWeaponCraft ? powerSlider : 1.0,
            })
          : await gameApi.enchantItem({
              itemId: selectedItems[0].itemId,
              stackIndex: selectedItems[0].stackIndex,
              enchantmentIds: selectedEnchantments.map((e) => e.id),
              soulGemId: gemsPayload[0].soulGemId,
              soulGemStackIndex: gemsPayload[0].soulGemStackIndex,
              chargeSlider: isWeaponCraft ? powerSlider : 1.0,
              customName: singleCustomName || undefined,
            });
      if (res.accepted) {
        setStatusMessage({
          kind: "success",
          text:
            res.message ||
            (totalPieceCount > 1
              ? `Super-enchanted ${totalPieceCount} items.`
              : `Created ${res.itemName || singleCustomName || selectedItem?.name}.`),
        });
        setItemCustomNames({});
        setItemQuantities({});
        setSelectedEnchIds([]);
        if (res.snapshot) {
          syncSelections(res.snapshot);
        } else {
          await loadSnapshot();
        }
      } else {
        setStatusMessage({
          kind: "error",
          text: res.message || "Enchanting failed.",
        });
      }
    } catch (err) {
      setStatusMessage({
        kind: "error",
        text: err instanceof Error ? err.message : "Enchanting request failed.",
      });
    } finally {
      setBusy(false);
    }
  }, [
    canCraftEnchant,
    busy,
    gameApi,
    selectedItems,
    totalPieceCount,
    allocatedGemList,
    itemCustomNames,
    getItemQty,
    selectedItem,
    selectedEnchantments,
    isWeaponCraft,
    powerSlider,
    syncSelections,
    loadSnapshot,
  ]);

  const handlePerformDisenchant = useCallback(async () => {
    if (!selectedDisenchantItem || !selectedDisenchantItem.canDisenchant || busy) return;
    if (!confirmDisenchantArmed) {
      setConfirmDisenchantArmed(true);
      return;
    }
    if (!gameApi) {
      setStatusMessage({
        kind: "success",
        text: `Studio preview: Learned ${selectedDisenchantItem.enchantmentName}.`,
      });
      setConfirmDisenchantArmed(false);
      return;
    }
    setBusy(true);
    setStatusMessage(null);
    try {
      const res = await gameApi.disenchantItem({
        itemId: selectedDisenchantItem.itemId,
        stackIndex: selectedDisenchantItem.stackIndex,
      });
      if (res.accepted) {
        setStatusMessage({
          kind: "success",
          text:
            res.message ||
            `Destroyed ${selectedDisenchantItem.name} and learned ${res.learnedEnchantmentName || selectedDisenchantItem.enchantmentName}.`,
        });
        setConfirmDisenchantArmed(false);
        if (res.snapshot) {
          syncSelections(res.snapshot);
        } else {
          await loadSnapshot();
        }
      } else {
        setStatusMessage({
          kind: "error",
          text: res.message || "Disenchanting failed.",
        });
      }
    } catch (err) {
      setStatusMessage({
        kind: "error",
        text: err instanceof Error ? err.message : "Disenchanting request failed.",
      });
    } finally {
      setBusy(false);
    }
  }, [selectedDisenchantItem, busy, confirmDisenchantArmed, gameApi, syncSelections, loadSnapshot]);

  const submitEnchantmentRename = useCallback(
    async (ench: SkyUltraKnownEnchantment, reset = false) => {
      if (renamingBusy) return;
      const trimmed = enchRenameInput.trim().slice(0, 80);
      if (!reset && !trimmed) {
        setStatusMessage({ kind: "error", text: "Enter a non-empty enchantment name." });
        return;
      }
      if (!gameApi) {
        setSnapshot((prev) => ({
          ...prev,
          knownEnchantments: prev.knownEnchantments.map((k) =>
            k.id === ench.id
              ? {
                  ...k,
                  originalName: k.originalName || k.name,
                  name: reset ? k.originalName || k.name : trimmed,
                  isCustomNamed: !reset,
                }
              : k,
          ),
        }));
        setRenamingEnchId(null);
        setStatusMessage({
          kind: "success",
          text: reset ? `Restored name to ${ench.originalName || ench.name}.` : `Renamed enchantment to ${trimmed}.`,
        });
        return;
      }
      setRenamingBusy(true);
      try {
        const res = await gameApi.renameEnchantment(
          reset ? { enchantmentId: ench.id, reset: true } : { enchantmentId: ench.id, name: trimmed },
        );
        if (res.accepted) {
          setRenamingEnchId(null);
          setStatusMessage({ kind: "success", text: res.message || "Enchantment renamed." });
          if (res.snapshot) {
            syncSelections(res.snapshot);
          } else {
            await loadSnapshot();
          }
        } else {
          setStatusMessage({ kind: "error", text: res.message || "Could not rename enchantment." });
        }
      } catch (err) {
        setStatusMessage({
          kind: "error",
          text: err instanceof Error ? err.message : "Rename request failed.",
        });
      } finally {
        setRenamingBusy(false);
      }
    },
    [renamingBusy, enchRenameInput, gameApi, syncSelections, loadSnapshot],
  );

  const cycleDisplayMode = useCallback(() => {
    if (!onChangeDisplayMode) return;
    const idx = DISPLAY_MODE_ORDER.indexOf(displayMode);
    const next = DISPLAY_MODE_ORDER[(idx + 1) % DISPLAY_MODE_ORDER.length] ?? "half";
    onChangeDisplayMode(next);
  }, [displayMode, onChangeDisplayMode]);

  useEffect(() => {
    if (obscured) return;
    const onKeyDown = (ev: KeyboardEvent) => {
      const target = ev.target as HTMLElement | null;
      const inInput = target && (target.tagName === "INPUT" || target.tagName === "TEXTAREA");
      if (ev.key === "Escape") {
        ev.preventDefault();
        if (renamingEnchId) {
          setRenamingEnchId(null);
        } else if (confirmDisenchantArmed) {
          setConfirmDisenchantArmed(false);
        } else {
          onClose?.();
        }
        return;
      }
      if (inInput) return;
      if (ev.key === " ") {
        ev.preventDefault();
        searchInputRef.current?.focus();
      } else if (ev.key === "Enter") {
        ev.preventDefault();
        if (activeTab === "enchant" && canCraftEnchant) {
          void handlePerformEnchant();
        } else if (activeTab === "disenchant" && selectedDisenchantItem?.canDisenchant) {
          void handlePerformDisenchant();
        }
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [
    obscured,
    onClose,
    renamingEnchId,
    confirmDisenchantArmed,
    activeTab,
    canCraftEnchant,
    selectedDisenchantItem,
    handlePerformEnchant,
    handlePerformDisenchant,
  ]);

  const learnableCount = useMemo(
    () => snapshot.disenchantableItems.filter((it) => it.canDisenchant).length,
    [snapshot.disenchantableItems],
  );

  const playerGold = player?.gold ?? snapshot.playerGold ?? 0;
  const carryWeight = player?.carryWeight?.current ?? snapshot.playerCarryWeight?.current ?? 0;

  return (
    <div className="enchanting-page" data-testid="enchanting-page">
      <header className="enchanting-header">
        <div className="enchanting-header__identity">
          <div className="enchanting-header__sigil">
            <Icon name="enchant" size={20} />
          </div>
          <div>
            <div className="enchanting-header__eyebrow">
              ARCANE ENCHANTER · {activeTab === "enchant" ? "SOUL BINDING" : "ARCANE EXTRACTION"}
            </div>
            <h1 className="enchanting-header__title">{snapshot.stationName || "Arcane Enchanter"}</h1>
          </div>
        </div>

        <div className="enchanting-header__stats">
          <div className="enchanting-stat-pill">
            <Icon name="rune" size={14} />
            <span>ENCHANTING</span>
            <strong>{Math.round(enchantingSkill)}</strong>
            {enchantingSkill > enchantingBaseSkill ? (
              <em className="enchanting-stat-pill__boost">
                +{Math.round(enchantingSkill - enchantingBaseSkill)}
              </em>
            ) : null}
          </div>
          {superActive ? (
            <div className="enchanting-super-badge" title="Enchanting 100 Super Mode: Disenchant any non-quest item, unlimited slots, unrestricted armor/weapon binding, and batch enchanting">
              <Icon name="crown" size={13} />
              <span>SUPER ENCHANTING · UNLIMITED</span>
            </div>
          ) : snapshot.hasExtraEffect ? (
            <div className="enchanting-stat-pill enchanting-stat-pill--accent">
              <Icon name="enchant" size={14} />
              <span>EXTRA EFFECT</span>
              <strong>×{maxEnchantments}</strong>
            </div>
          ) : null}
          <div className="enchanting-stat-pill">
            <Icon name="gold" size={14} />
            <span>{playerGold.toLocaleString()}</span>
            <small>GOLD</small>
          </div>
          <div className="enchanting-stat-pill">
            <Icon name="weight" size={14} />
            <span>{Math.round(carryWeight)}</span>
            <small>WT</small>
          </div>
        </div>

        <div className="enchanting-header__controls">
          {onChangeDisplayMode ? (
            <button
              type="button"
              className={`enchanting-mode-btn${displayMode !== "half" ? " is-active" : ""}`}
              onClick={cycleDisplayMode}
              title={`Cycle Layout Mode: Half Container / Full Screen / Windowed (Current: ${DISPLAY_MODE_LABELS[displayMode]})`}
            >
              <Icon name="widgets" size={13} />
              <span>{DISPLAY_MODE_LABELS[displayMode]}</span>
            </button>
          ) : null}
          {onTogglePauseWorld ? (
            <button
              type="button"
              className={`enchanting-btn enchanting-btn--chip${pauseWorld ? " is-active" : ""}`}
              onClick={onTogglePauseWorld}
              title="Toggle whether world simulation pauses while enchanting"
            >
              <Icon name={pauseWorld ? "pause" : "play"} size={14} />
              <span>{pauseWorld ? "Pause World" : "Live World"}</span>
            </button>
          ) : null}
          <button
            type="button"
            className="enchanting-btn enchanting-btn--icon"
            onClick={() => void loadSnapshot()}
            title="Refresh Arcane Enchanter inventory"
          >
            <Icon name="refresh" size={15} />
          </button>
          {onClose ? (
            <button type="button" className="enchanting-btn enchanting-btn--close" onClick={onClose}>
              <Icon name="close" size={14} />
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          ) : null}
        </div>
      </header>

      <div className="enchanting-body">
        <section className="enchanting-browser">
          <div className="enchanting-mode-bar">
            <SkySegmentedControl
              label="Arcane Enchanter discipline"
              value={activeTab}
              onValueChange={(v) => setActiveTab(v)}
              options={[
                {
                  value: "enchant",
                  label: "Enchant Item",
                  count: snapshot.enchantableItems.length,
                },
                {
                  value: "disenchant",
                  label: "Disenchant",
                  count: learnableCount,
                },
              ]}
            />
          </div>

          {activeTab === "enchant" ? (
            <>
              <div className="enchanting-ritual-slots">
                <button
                  type="button"
                  className={`enchanting-slot-tab${slotStep === "item" ? " is-current" : ""}${selectedItems.length > 0 ? " is-filled" : ""}`}
                  onClick={() => setSlotStep("item")}
                >
                  <span className="enchanting-slot-tab__step">
                    <span>I. {superActive ? `Vessels (${totalPieceCount})` : "Vessel"}</span>
                    {selectedItems.length > 0 ? <Icon name="check" size={11} /> : null}
                  </span>
                  <strong className="enchanting-slot-tab__val">
                    {totalPieceCount > 1
                      ? `${totalPieceCount} ${batchKind === "weapon" ? "Weapons" : "Apparel"} Queued`
                      : selectedItem
                        ? (itemCustomNames[selectedItem.id] ?? "").trim() || selectedItem.name
                        : "Choose Item"}
                  </strong>
                  <small>
                    {selectedItem
                      ? selectedItems.length > 1
                        ? `${selectedItems.map((i) => `${i.name}${getItemQty(i) > 1 ? ` ×${getItemQty(i)}` : ""}`).slice(0, 2).join(", ")}${selectedItems.length > 2 ? "…" : ""}`
                        : totalPieceCount > 1
                          ? `${selectedItem.name} ×${totalPieceCount}/${selectedItem.count}`
                          : selectedItem.weaponType || selectedItem.armorTypeLabel || selectedItem.category
                      : `${snapshot.enchantableItems.length} available`}
                  </small>
                </button>

                <button
                  type="button"
                  className={`enchanting-slot-tab${slotStep === "enchantment" ? " is-current" : ""}${selectedEnchantments.length > 0 ? " is-filled" : ""}`}
                  onClick={() => setSlotStep("enchantment")}
                >
                  <span className="enchanting-slot-tab__step">
                    <span>II. Sigils ({selectedEnchantments.length}/{unlimitedSlots ? "∞" : maxEnchantments})</span>
                    {selectedEnchantments.length > 0 ? <Icon name="check" size={11} /> : null}
                  </span>
                  <strong className="enchanting-slot-tab__val">
                    {selectedEnchantments.length > 0
                      ? selectedEnchantments.map((e) => e.name).join(" + ")
                      : "Choose Effect"}
                  </strong>
                  <small>{snapshot.knownEnchantments.length} learned</small>
                </button>

                <button
                  type="button"
                  className={`enchanting-slot-tab${slotStep === "soulgem" ? " is-current" : ""}${allocatedGemList.length === requiredGemCount ? " is-filled" : ""}`}
                  onClick={() => setSlotStep("soulgem")}
                >
                  <span className="enchanting-slot-tab__step">
                    <span>III. Soul {requiredGemCount > 1 ? `(${allocatedGemList.length}/${requiredGemCount})` : "Gem"}</span>
                    {allocatedGemList.length >= requiredGemCount ? <Icon name="check" size={11} /> : null}
                  </span>
                  <strong className="enchanting-slot-tab__val">
                    {requiredGemCount > 1
                      ? `${allocatedGemList.length} / ${requiredGemCount} Auto-Filled`
                      : selectedGem
                        ? selectedGem.name
                        : "Choose Soul Gem"}
                  </strong>
                  <small>
                    {selectedGem
                      ? requiredGemCount > 1
                        ? `Exact 1:1 gem match (${totalAvailableSoulGems} available)`
                        : `${selectedGem.soulLabel} · ${selectedGem.isReusable ? "Reusable" : `${selectedGem.count} left`}`
                      : `${snapshot.soulGems.length} filled`}
                  </small>
                </button>
              </div>

              {superActive ? (
                <div className="enchanting-batch-toolbar">
                  <span>
                    <strong className="enchanting-batch-toolbar__kind">SUPER BATCH MODE:</strong>{" "}
                    {slotStep === "item"
                      ? `Multi-select ${batchKind === "weapon" ? "weapons" : batchKind === "armor" ? "apparel (helmets, armor, gauntlets, boots, shields, rings, amulets)" : "matching weapons or apparel"} & adjust stack quantities in the Altar`
                      : slotStep === "enchantment"
                        ? "Unlimited slots enabled · All slot restrictions removed"
                        : `Auto-filled ${allocatedGemList.length}/${requiredGemCount} filled Soul Gems (locked to exact 1 gem per item)`}
                  </span>
                  {(selectedItems.length > 1 || totalPieceCount > 1) && slotStep === "item" ? (
                    <button
                      type="button"
                      className="enchanting-link"
                      onClick={() => {
                        if (selectedItem) {
                          setSelectedItemIds([selectedItem.id]);
                          setItemQuantities({ [selectedItem.id]: 1 });
                        }
                      }}
                    >
                      Keep Only 1 Item
                    </button>
                  ) : null}
                </div>
              ) : null}
            </>
          ) : null}

          <div className="enchanting-toolbar">
            <label className="enchanting-search">
              <Icon name="search" size={14} />
              <input
                ref={searchInputRef}
                type="text"
                value={search}
                onChange={(e) => setSearch(e.target.value)}
                placeholder={
                  activeTab === "disenchant"
                    ? "Search enchanted gear or effect..."
                    : slotStep === "item"
                      ? "Search weapons, armor, jewelry..."
                      : slotStep === "enchantment"
                        ? "Search learned enchantments..."
                        : "Search filled soul gems..."
                }
              />
              {search ? (
                <button
                  type="button"
                  className="enchanting-search__clear"
                  onClick={() => setSearch("")}
                  aria-label="Clear search"
                >
                  <Icon name="close" size={12} />
                </button>
              ) : (
                <kbd>Space</kbd>
              )}
            </label>

            <div className="enchanting-filters">
              {activeTab === "enchant" && slotStep === "item" ? (
                <SkyChoice
                  label="Filter items"
                  value={itemFilter}
                  onValueChange={(v) => setItemFilter(v)}
                  options={[
                    { value: "all", label: "All Gear" },
                    { value: "weapon", label: "Weapons" },
                    { value: "armor", label: "Armor & Clothing" },
                    { value: "jewelry", label: "Jewelry" },
                  ]}
                />
              ) : null}

              {activeTab === "enchant" && slotStep === "enchantment" ? (
                <SkyChoice
                  label="Filter enchantments"
                  value={enchFilter}
                  onValueChange={(v) => setEnchFilter(v)}
                  options={[
                    { value: "compatible", label: selectedItem ? "Compatible" : "All" },
                    { value: "all", label: "All Known" },
                    { value: "weapon", label: "Weapon" },
                    { value: "armor", label: "Apparel" },
                  ]}
                />
              ) : null}

              {activeTab === "enchant" && slotStep === "soulgem" ? (
                <SkyChoice
                  label="Filter soul gems"
                  value={gemFilter}
                  onValueChange={(v) => setGemFilter(v)}
                  options={[
                    { value: "all", label: "All Filled" },
                    { value: "grand", label: "Grand / Black" },
                    { value: "reusable", label: "Reusable Stars" },
                  ]}
                />
              ) : null}

              {activeTab === "disenchant" ? (
                <SkyChoice
                  label="Filter disenchantable items"
                  value={disenchantFilter}
                  onValueChange={(v) => setDisenchantFilter(v)}
                  options={[
                    { value: "learnable", label: `Can Learn (${learnableCount})` },
                    { value: "all", label: "All" },
                    { value: "known", label: "Known" },
                    { value: "blocked", label: "Protected" },
                  ]}
                />
              ) : null}

              <SkyChoice
                label="Sort entries"
                value={sortBy}
                onValueChange={(v) => setSortBy(v)}
                options={[
                  { value: "name", label: "Sort: Name" },
                  { value: "value", label: "Sort: Value" },
                  { value: "stat", label: "Sort: Damage / AR" },
                  { value: "tier", label: "Sort: Soul Tier" },
                  { value: "count", label: "Sort: Quantity" },
                ]}
              />

              <SkySegmentedControl
                label="View layout"
                value={viewLayout}
                onValueChange={setViewLayout}
                options={[
                  { value: "list", label: <Icon name="list" size={14} />, ariaLabel: "List view" },
                  { value: "grid", label: <Icon name="grid" size={14} />, ariaLabel: "Grid view" },
                ]}
              />
            </div>
          </div>

          {renamingGridEnch ? (
            <div className="enchanting-inline-rename enchanting-inline-rename--top">
              <span className="enchanting-inline-rename__target">
                <Icon name="pen" size={12} />
                <span>{renamingGridEnch.originalName || renamingGridEnch.name}</span>
              </span>
              <input
                type="text"
                maxLength={80}
                value={enchRenameInput}
                onChange={(e) => setEnchRenameInput(e.target.value)}
                placeholder={renamingGridEnch.originalName || renamingGridEnch.name}
                onKeyDown={(e) => {
                  if (e.key === "Enter") {
                    e.preventDefault();
                    void submitEnchantmentRename(renamingGridEnch, false);
                  } else if (e.key === "Escape") {
                    e.preventDefault();
                    setRenamingEnchId(null);
                  }
                }}
                autoFocus
              />
              <button
                type="button"
                className="enchanting-btn enchanting-btn--chip is-active"
                disabled={renamingBusy}
                onClick={() => void submitEnchantmentRename(renamingGridEnch, false)}
              >
                Save Name
              </button>
              {renamingGridEnch.isCustomNamed ? (
                <button
                  type="button"
                  className="enchanting-btn enchanting-btn--chip"
                  disabled={renamingBusy}
                  onClick={() => void submitEnchantmentRename(renamingGridEnch, true)}
                >
                  Reset
                </button>
              ) : null}
              <button
                type="button"
                className="enchanting-btn enchanting-btn--chip"
                onClick={() => setRenamingEnchId(null)}
              >
                Cancel
              </button>
            </div>
          ) : null}

          <div
            className={`enchanting-list${
              viewLayout === "grid" ? " enchanting-list--grid enchanting-list--grid-4" : ""
            }`}
          >
            {loading ? (
              <Empty
                title="Attuning Arcane Conduit..."
                detail="Reading inventory, known enchantments, and soul gems."
                icon="enchant"
              />
            ) : activeTab === "disenchant" ? (
              filteredDisenchantItems.length === 0 ? (
                <Empty
                  title="No matching enchanted gear"
                  detail="Switch the filter to All or carry enchanted items to extract their effects."
                  icon="enchant"
                />
              ) : (
                filteredDisenchantItems.slice(pageStart, pageEnd).map((entry) => {
                  const selected = entry.id === selectedDisenchantId;
                  const compactStatus = getCompactDisenchantStatus(entry);
                  const disenchantEffectText = entry.effects.map((e) => `${e.name} ${e.description ?? ""}`).join(" ");
                  if (viewLayout === "grid") {
                    return (
                      <button
                        key={entry.id}
                        type="button"
                        className={`enchanting-grid-card enchanting-grid-card--disenchant${selected ? " is-selected" : ""}${!entry.canDisenchant ? " is-muted" : ""}`}
                        onClick={() => setSelectedDisenchantId(entry.id)}
                        title={entry.canDisenchant ? `${entry.name} · ${entry.enchantmentName}` : `${entry.name} · ${entry.blockReason || compactStatus}`}
                      >
                        <div className="enchanting-grid-card__top">
                          {entry.equipped ? (
                            <span className="enchanting-badge">Equipped</span>
                          ) : entry.isUniqueLocked && superActive ? (
                            <span className="enchanting-badge enchanting-badge--accent">Unlocked</span>
                          ) : (
                            <span />
                          )}
                          <span className={`enchanting-pill${entry.canDisenchant ? " enchanting-pill--ready" : " enchanting-pill--blocked"}`}>
                            {!entry.canDisenchant ? <Icon name="lock" size={10} /> : null}
                            <span>
                              {entry.canDisenchant
                                ? `+${entry.estimatedXp < 10 ? entry.estimatedXp.toFixed(1) : Math.round(entry.estimatedXp)} XP`
                                : compactStatus}
                            </span>
                          </span>
                        </div>
                        <div className="enchanting-grid-card__art">
                          <ItemThumbnail item={toThumbnailItem({ ...entry, enchanted: true })} bridge={bridge ?? undefined} paused={obscured} />
                        </div>
                        <div className="enchanting-grid-card__title">{entry.name}</div>
                        <div className="enchanting-grid-card__sub enchanting-grid-card__sub--sigil">
                          <EnchantmentSigil
                            name={entry.enchantmentName}
                            effectName={disenchantEffectText}
                            size="sm"
                          />
                          <span>{entry.enchantmentName}</span>
                        </div>
                      </button>
                    );
                  }
                  return (
                    <button
                      key={entry.id}
                      type="button"
                      className={`enchanting-row${selected ? " is-selected" : ""}${!entry.canDisenchant ? " is-muted" : ""}`}
                      onClick={() => setSelectedDisenchantId(entry.id)}
                      title={entry.canDisenchant ? entry.enchantmentName : entry.blockReason || compactStatus}
                    >
                      <div className="enchanting-row__thumb">
                        <ItemThumbnail item={toThumbnailItem({ ...entry, enchanted: true })} bridge={bridge ?? undefined} paused={obscured} />
                      </div>
                      <div className="enchanting-row__main">
                        <div className="enchanting-row__title">
                          <span>{entry.name}</span>
                          {entry.equipped ? <span className="enchanting-badge">Equipped</span> : null}
                          {entry.isUniqueLocked && superActive ? (
                            <span className="enchanting-badge enchanting-badge--accent">Unlocked</span>
                          ) : null}
                        </div>
                        <div className="enchanting-row__sub">
                          <EnchantmentSigil
                            name={entry.enchantmentName}
                            effectName={disenchantEffectText}
                            size="sm"
                          />
                          <span>{entry.enchantmentName}</span>
                          <span>·</span>
                          <span>{entry.value.toLocaleString()}g</span>
                        </div>
                      </div>
                      <div className="enchanting-row__right">
                        {entry.canDisenchant ? (
                          <span className="enchanting-pill enchanting-pill--ready">
                            +{entry.estimatedXp < 10 ? entry.estimatedXp.toFixed(1) : Math.round(entry.estimatedXp)} XP · Learn
                          </span>
                        ) : (
                          <span className="enchanting-pill enchanting-pill--blocked">
                            <Icon name="lock" size={12} />
                            <span>{compactStatus}</span>
                          </span>
                        )}
                      </div>
                    </button>
                  );
                })
              )
            ) : slotStep === "item" ? (
              filteredItems.length === 0 ? (
                <Empty
                  title="No unenchanted gear available"
                  detail="Unenchanted weapons (except staves), armor, clothing, and jewelry appear here."
                  icon="sword"
                />
              ) : (
                filteredItems.slice(pageStart, pageEnd).map((item) => {
                  const selected = selectedItemIds.includes(item.id);
                  const qty = getItemQty(item);
                  const customLabel = (itemCustomNames[item.id] ?? "").trim();
                  const mixedIncompatible = Boolean(superActive && batchKind && batchKind !== item.itemKind);
                  const statBadge =
                    typeof item.damage === "number" && item.damage > 0
                      ? `DMG ${Math.round(item.damage)}`
                      : typeof item.armorRating === "number" && item.armorRating > 0
                        ? `AR ${Math.round(item.armorRating)}`
                        : item.armorTypeLabel || item.category;

                  if (viewLayout === "grid") {
                    return (
                      <button
                        key={item.id}
                        type="button"
                        className={`enchanting-grid-card${selected ? " is-selected" : ""}${mixedIncompatible ? " is-muted" : ""}`}
                        onClick={() => toggleTargetItem(item)}
                        title={customLabel ? `${customLabel} (${item.name})` : item.name}
                      >
                        <div className="enchanting-grid-card__top">
                          {item.count > 1 ? (
                            <span className="enchanting-count">×{selected && superActive ? `${qty}/${item.count}` : item.count}</span>
                          ) : item.equipped ? (
                            <span className="enchanting-badge">Equipped</span>
                          ) : (
                            <span />
                          )}
                          {selected ? (
                            <span className="enchanting-pill enchanting-pill--ready">
                              <Icon name="check" size={11} /> {superActive ? (item.count > 1 ? `×${qty}` : "Batch") : "Active"}
                            </span>
                          ) : (
                            <span className="enchanting-pill">{statBadge}</span>
                          )}
                        </div>
                        <div className="enchanting-grid-card__art">
                          <ItemThumbnail item={toThumbnailItem(item)} bridge={bridge ?? undefined} paused={obscured} />
                        </div>
                        <div className="enchanting-grid-card__title">{customLabel || item.name}</div>
                        <div className="enchanting-grid-card__sub">
                          <span>{item.weaponType || item.armorTypeLabel || item.category}</span>
                          <span>·</span>
                          <span>{item.value.toLocaleString()}g</span>
                        </div>
                      </button>
                    );
                  }

                  return (
                    <button
                      key={item.id}
                      type="button"
                      className={`enchanting-row${selected ? " is-selected" : ""}${mixedIncompatible ? " is-muted" : ""}`}
                      onClick={() => toggleTargetItem(item)}
                    >
                      <div className="enchanting-row__thumb">
                        <ItemThumbnail item={toThumbnailItem(item)} bridge={bridge ?? undefined} paused={obscured} />
                      </div>
                      <div className="enchanting-row__main">
                        <div className="enchanting-row__title">
                          <span>{customLabel ? `${customLabel} (${item.name})` : item.name}</span>
                          {item.count > 1 ? <span className="enchanting-count">×{item.count}</span> : null}
                          {item.equipped ? <span className="enchanting-badge">Equipped</span> : null}
                        </div>
                        <div className="enchanting-row__sub">
                          <span>{item.weaponType || item.armorTypeLabel || item.category}</span>
                          <span>·</span>
                          <span>{item.value.toLocaleString()}g</span>
                        </div>
                      </div>
                      <div className="enchanting-row__right">
                        {selected ? (
                          <span className="enchanting-pill enchanting-pill--ready">
                            <Icon name="check" size={12} />{" "}
                            {superActive ? (item.count > 1 ? `In Batch ×${qty}` : "In Batch") : "Selected"}
                          </span>
                        ) : (
                          <span className="enchanting-pill">{statBadge}</span>
                        )}
                      </div>
                    </button>
                  );
                })
              )
            ) : slotStep === "enchantment" ? (
              filteredEnchantments.length === 0 ? (
                <Empty
                  title="No matching enchantments"
                  detail="Disenchant magical items to learn their base enchantments, or switch the compatibility filter."
                  icon="rune"
                />
              ) : (
                filteredEnchantments.slice(pageStart, pageEnd).map((ench) => {
                  const selected = selectedEnchIds.includes(ench.id);
                  const compat = isEnchantmentCompatibleWithItem(ench, selectedItem, superActive);
                  const primaryEffect = ench.effects[0];
                  const enchEffectText = ench.effects
                    .map((e) => `${e.name} ${e.descriptionTemplate ?? ""}`)
                    .join(" ");
                  const effectDesc = primaryEffect
                    ? formatScaledEffectText(primaryEffect, effectiveMagnitudeScale)
                    : ench.school;
                  const isRenaming = renamingEnchId === ench.id;
                  const statusTip = selected
                    ? "Bound"
                    : compat.compatible
                      ? "Deactivated"
                      : compat.reason
                        ? `Deactivated · ${compat.reason}`
                        : "Deactivated";
                  const statusClass = selected ? "is-bound" : compat.compatible ? "is-inactive" : "is-blocked";

                  if (viewLayout === "grid") {
                    return (
                      <div
                        key={ench.id}
                        role="button"
                        tabIndex={compat.compatible ? 0 : -1}
                        aria-disabled={!compat.compatible}
                        className={`enchanting-ench-card${selected ? " is-selected" : ""}${!compat.compatible ? " is-disabled" : ""}`}
                        onClick={() => {
                          if (compat.compatible) toggleEnchantment(ench);
                        }}
                        onKeyDown={(ev) => {
                          if (!compat.compatible) return;
                          if (ev.key === "Enter" || ev.key === " ") {
                            ev.preventDefault();
                            toggleEnchantment(ench);
                          }
                        }}
                      >
                        <div className="enchanting-ench-card__top">
                          <button
                            type="button"
                            className={`enchanting-icon-btn enchanting-icon-tip${isRenaming ? " is-active" : ""}`}
                            data-tip={isRenaming ? "Cancel Rename" : "Rename"}
                            aria-label={isRenaming ? "Cancel Rename" : `Rename ${ench.name}`}
                            onClick={(e) => {
                              e.stopPropagation();
                              if (isRenaming) {
                                setRenamingEnchId(null);
                              } else {
                                setRenamingEnchId(ench.id);
                                setEnchRenameInput(ench.name);
                              }
                            }}
                          >
                            <Icon name="pen" size={12} />
                          </button>
                          <span
                            className={`enchanting-status-icon enchanting-icon-tip ${statusClass}`}
                            data-tip={statusTip}
                            aria-label={statusTip}
                          >
                            <Icon name={selected ? "check" : "close"} size={12} />
                          </span>
                        </div>
                        <div className="enchanting-ench-card__sigil">
                          <EnchantmentSigil
                            name={ench.originalName || ench.name}
                            effectName={enchEffectText}
                            school={ench.school}
                            size="lg"
                          />
                        </div>
                        <div className="enchanting-ench-card__title" title={ench.name}>
                          {ench.name}
                        </div>
                        <div className="enchanting-ench-card__desc" title={effectDesc}>
                          {effectDesc}
                        </div>
                        <div className="enchanting-ench-card__foot">
                          <span className="enchanting-badge">
                            {ench.targetKind === "weapon" ? "Weapon" : "Apparel"}
                          </span>
                          {ench.isCustomNamed ? (
                            <span className="enchanting-badge enchanting-badge--accent">Renamed</span>
                          ) : null}
                        </div>
                      </div>
                    );
                  }

                  return (
                    <div key={ench.id} className="enchanting-row__wrap">
                      <div
                        role="button"
                        tabIndex={compat.compatible ? 0 : -1}
                        aria-disabled={!compat.compatible}
                        className={`enchanting-row${selected ? " is-selected" : ""}${!compat.compatible ? " is-disabled" : ""}`}
                        onClick={() => {
                          if (compat.compatible) toggleEnchantment(ench);
                        }}
                        onKeyDown={(ev) => {
                          if (!compat.compatible) return;
                          if (ev.key === "Enter" || ev.key === " ") {
                            ev.preventDefault();
                            toggleEnchantment(ench);
                          }
                        }}
                      >
                        <EnchantmentSigil
                          name={ench.originalName || ench.name}
                          effectName={enchEffectText}
                          school={ench.school}
                        />
                        <div className="enchanting-row__main">
                          <div className="enchanting-row__title">
                            <span>{ench.name}</span>
                            <span className="enchanting-badge">
                              {ench.targetKind === "weapon" ? "Weapon" : "Apparel"}
                            </span>
                            {ench.isCustomNamed ? (
                              <span className="enchanting-badge enchanting-badge--accent">Renamed</span>
                            ) : null}
                          </div>
                          <div className="enchanting-row__sub">
                            <span>{effectDesc}</span>
                          </div>
                        </div>
                        <div className="enchanting-row__right enchanting-row__actions">
                          <button
                            type="button"
                            className={`enchanting-icon-btn enchanting-icon-tip${isRenaming ? " is-active" : ""}`}
                            data-tip={isRenaming ? "Cancel Rename" : "Rename"}
                            aria-label={isRenaming ? "Cancel Rename" : `Rename ${ench.name}`}
                            onClick={(e) => {
                              e.stopPropagation();
                              if (isRenaming) {
                                setRenamingEnchId(null);
                              } else {
                                setRenamingEnchId(ench.id);
                                setEnchRenameInput(ench.name);
                              }
                            }}
                          >
                            <Icon name="pen" size={12} />
                          </button>
                          <span
                            className={`enchanting-status-icon enchanting-icon-tip ${statusClass}`}
                            data-tip={statusTip}
                            aria-label={statusTip}
                          >
                            <Icon name={selected ? "check" : "close"} size={12} />
                          </span>
                        </div>
                      </div>
                      {isRenaming ? (
                        <div className="enchanting-inline-rename">
                          <input
                            type="text"
                            maxLength={80}
                            value={enchRenameInput}
                            onChange={(e) => setEnchRenameInput(e.target.value)}
                            placeholder={ench.originalName || ench.name}
                            onKeyDown={(e) => {
                              if (e.key === "Enter") {
                                e.preventDefault();
                                void submitEnchantmentRename(ench, false);
                              } else if (e.key === "Escape") {
                                e.preventDefault();
                                setRenamingEnchId(null);
                              }
                            }}
                            autoFocus
                          />
                          <button
                            type="button"
                            className="enchanting-btn enchanting-btn--chip is-active"
                            disabled={renamingBusy}
                            onClick={() => void submitEnchantmentRename(ench, false)}
                          >
                            Save Name
                          </button>
                          {ench.isCustomNamed ? (
                            <button
                              type="button"
                              className="enchanting-btn enchanting-btn--chip"
                              disabled={renamingBusy}
                              onClick={() => void submitEnchantmentRename(ench, true)}
                            >
                              Reset
                            </button>
                          ) : null}
                        </div>
                      ) : null}
                    </div>
                  );
                })
              )
            ) : filteredGems.length === 0 ? (
              <Empty
                title="No filled soul gems carried"
                detail="Fill an empty soul gem or Azura's Star using Soul Trap before binding."
                icon="soulGem"
              />
            ) : (
              filteredGems.slice(pageStart, pageEnd).map((gem) => {
                const gemQty = gemAllocations[gem.id] ?? 0;
                const gemCap = gem.isReusable ? Math.min(1, gem.count) : gem.count;
                const selected = gemQty > 0;
                const spareElsewhere = snapshot.soulGems.some((other) => {
                  if (other.id === gem.id) return false;
                  const oCap = other.isReusable ? Math.min(1, other.count) : other.count;
                  return (gemAllocations[other.id] ?? 0) < oCap;
                });
                const disableRemove = gemQty <= 0 || !spareElsewhere;
                const disableAdd = gemQty >= gemCap || gemQty >= requiredGemCount;
                if (viewLayout === "grid") {
                  return (
                    <div
                      key={gem.id}
                      role="button"
                      tabIndex={0}
                      className={`enchanting-grid-card${selected ? " is-selected" : ""}`}
                      onClick={() => adjustGemAllocation(gem, 1)}
                      onKeyDown={(ev) => {
                        if (ev.key === "Enter" || ev.key === " ") {
                          ev.preventDefault();
                          adjustGemAllocation(gem, 1);
                        }
                      }}
                    >
                      <div className="enchanting-grid-card__top">
                        {gem.count > 1 ? <span className="enchanting-count">×{gem.count}</span> : <span />}
                        <span className={`enchanting-pill${selected ? " enchanting-pill--ready" : ""}`}>
                          {superActive && requiredGemCount > 1 ? `${gemQty}/${gemCap} Used` : gem.soulLabel}
                        </span>
                      </div>
                      <div className="enchanting-grid-card__art">
                        <ItemThumbnail item={toSoulGemThumbnailItem(gem)} bridge={bridge ?? undefined} paused={obscured} />
                      </div>
                      <div className="enchanting-grid-card__title">{gem.name}</div>
                      <div className="enchanting-grid-card__sub">
                        <span>{gem.soulPoints} pts</span>
                        <span>·</span>
                        <span>{Math.round(gem.armorScale * 100)}%</span>
                      </div>
                      {superActive && requiredGemCount > 1 ? (
                        <div className="enchanting-gem-qty" onClick={(e) => e.stopPropagation()}>
                          <button
                            type="button"
                            className="enchanting-gem-qty__btn"
                            disabled={disableRemove}
                            onClick={() => adjustGemAllocation(gem, -1)}
                            aria-label={`Swap one ${gem.name} for another gem`}
                          >
                            -
                          </button>
                          <span className={`enchanting-pill${gemQty > 0 ? " enchanting-pill--ready" : ""}`}>
                            {gemQty}
                          </span>
                          <button
                            type="button"
                            className="enchanting-gem-qty__btn"
                            disabled={disableAdd}
                            onClick={() => adjustGemAllocation(gem, 1)}
                            aria-label={`Use one more ${gem.name}`}
                          >
                            +
                          </button>
                        </div>
                      ) : null}
                    </div>
                  );
                }
                return (
                  <div
                    key={gem.id}
                    role="button"
                    tabIndex={0}
                    className={`enchanting-row${selected ? " is-selected" : ""}`}
                    onClick={() => adjustGemAllocation(gem, 1)}
                    onKeyDown={(ev) => {
                      if (ev.key === "Enter" || ev.key === " ") {
                        ev.preventDefault();
                        adjustGemAllocation(gem, 1);
                      }
                    }}
                  >
                    <div className="enchanting-row__thumb">
                      <ItemThumbnail item={toSoulGemThumbnailItem(gem)} bridge={bridge ?? undefined} paused={obscured} />
                    </div>
                    <div className="enchanting-row__main">
                      <div className="enchanting-row__title">
                        <span>{gem.name}</span>
                        {gem.count > 1 ? <span className="enchanting-count">×{gem.count}</span> : null}
                        {gem.isReusable ? (
                          <span className="enchanting-badge enchanting-badge--accent">Reusable</span>
                        ) : null}
                      </div>
                      <div className="enchanting-row__sub">
                        <span>{gem.soulLabel} Soul</span>
                        <span>·</span>
                        <span>{gem.soulPoints} Charge Pts</span>
                        <span>·</span>
                        <span>{Math.round(gem.armorScale * 100)}% Armor Scale</span>
                      </div>
                    </div>
                    <div className="enchanting-row__right enchanting-row__actions">
                      {superActive && requiredGemCount > 1 ? (
                        <div className="enchanting-gem-qty" onClick={(e) => e.stopPropagation()}>
                          <button
                            type="button"
                            className="enchanting-gem-qty__btn"
                            disabled={disableRemove}
                            onClick={() => adjustGemAllocation(gem, -1)}
                            aria-label={`Swap one ${gem.name} for another gem`}
                          >
                            -
                          </button>
                          <span className={`enchanting-pill${gemQty > 0 ? " enchanting-pill--ready" : ""}`}>
                            {gemQty}/{gemCap}
                          </span>
                          <button
                            type="button"
                            className="enchanting-gem-qty__btn"
                            disabled={disableAdd}
                            onClick={() => adjustGemAllocation(gem, 1)}
                            aria-label={`Use one more ${gem.name}`}
                          >
                            +
                          </button>
                        </div>
                      ) : (
                        <span className="enchanting-pill">Tier {gem.soulLevel}/5</span>
                      )}
                    </div>
                  </div>
                );
              })
            )}
          </div>

          <footer className="enchanting-pagination">
            <span>
              {totalActiveCount === 0
                ? "0 entries"
                : `${pageStart + 1}-${Math.min(pageEnd, totalActiveCount)} of ${totalActiveCount}`}
            </span>
            <div className="enchanting-pagination__btns">
              <button
                type="button"
                className="enchanting-btn enchanting-btn--chip"
                disabled={safePage <= 0}
                onClick={() => setPageIndex((p) => Math.max(0, p - 1))}
              >
                Previous
              </button>
              <button
                type="button"
                className="enchanting-btn enchanting-btn--chip"
                disabled={safePage >= totalPages - 1}
                onClick={() => setPageIndex((p) => Math.min(totalPages - 1, p + 1))}
              >
                Next
              </button>
            </div>
          </footer>
        </section>

        <aside className="enchanting-altar">
          {statusMessage ? (
            <div className={`enchanting-banner enchanting-banner--${statusMessage.kind}`}>
              <Icon
                name={
                  statusMessage.kind === "success"
                    ? "check"
                    : statusMessage.kind === "error"
                      ? "alertTriangle"
                      : "enchant"
                }
                size={15}
              />
              <span>{statusMessage.text}</span>
            </div>
          ) : null}

          {activeTab === "enchant" ? (() => {
            const soulReady = allocatedGemList.length >= requiredGemCount && Boolean(selectedGem);
            const vesselReady = selectedItems.length > 0;
            const sigilsReady = selectedEnchantments.length > 0;
            const readinessLabel = canCraftEnchant
              ? "READY TO BIND"
              : !vesselReady
                ? "PICK VESSEL"
                : !sigilsReady
                  ? "ADD SIGIL"
                  : !soulReady
                    ? "NEEDS SOUL GEM"
                    : "ATTUNING";
            const readinessTone = canCraftEnchant
              ? "ready"
              : vesselReady && sigilsReady && !soulReady
                ? "warn"
                : "idle";
            const soulFillPct = Math.min(100, Math.round((allocatedGemList.length / Math.max(1, requiredGemCount)) * 100));
            return (
            <div className="enchanting-altar__card">
              <div className="enchanting-altar__header">
                <div className="enchanting-altar__crest">
                  <div className={`enchanting-altar__emblem${canCraftEnchant ? " is-ready" : ""}`}>
                    <AltarCrestEmblem variant="bind" />
                  </div>
                  <div className="enchanting-altar__headings">
                    <span className="enchanting-altar__eyebrow">
                      {superActive ? "SOVEREIGN CONDUIT" : "ARCANE SANCTUM"}
                    </span>
                    <strong className="enchanting-altar__title">Arcane Binding Altar</strong>
                  </div>
                </div>
                <div className="enchanting-altar__header-badges">
                  <span className={`enchanting-altar__readiness is-${readinessTone}`}>
                    {readinessLabel}
                  </span>
                  <span className="enchanting-altar__slots-badge">
                    {selectedEnchantments.length}/{unlimitedSlots ? "∞" : maxEnchantments}
                  </span>
                </div>
              </div>

              {superActive ? (
                <div className="enchanting-super-notice">
                  <Icon name="crown" size={14} />
                  <span>
                    Super Enchanting active: unlimited sigils, unrestricted slots &amp; multi-piece batching.
                  </span>
                </div>
              ) : null}

              <div className="enchanting-vessel">
                {selectedItem ? (
                  <>
                    <div className="enchanting-altar__stage">
                      <div className="enchanting-altar__pedestal">
                        <div className={`enchanting-altar__plinth${sigilsReady ? " is-charged" : ""}`}>
                          <AltarPlinthBackdrop />
                          <ItemThumbnail item={toThumbnailItem(selectedItem)} bridge={bridge ?? undefined} paused={obscured} />
                        </div>
                        <div className="enchanting-vessel__meta">
                          <div className="enchanting-vessel__kind">
                            {totalPieceCount > 1
                              ? `BATCH · ${totalPieceCount} ${batchKind === "weapon" ? "WEAPONS" : "APPAREL PIECES"}`
                              : `${selectedItem.weaponType || selectedItem.armorTypeLabel || selectedItem.category} · ${formId(selectedItem.itemId)}`}
                          </div>
                          <h2>
                            {selectedItems.length > 1
                              ? `${totalPieceCount} Items Queued`
                              : totalPieceCount > 1
                                ? `${(itemCustomNames[selectedItem.id] ?? "").trim() || selectedItem.name} ×${totalPieceCount}`
                                : (itemCustomNames[selectedItem.id] ?? "").trim() || selectedItem.name}
                          </h2>
                          <div className="enchanting-vessel__stats">
                            <span className="enchanting-vessel__stat-chip">{selectedItem.value.toLocaleString()}g</span>
                            <span className="enchanting-vessel__stat-chip">{selectedItem.weight} wt</span>
                            {selectedItem.damage ? (
                              <span className="enchanting-vessel__stat-chip is-accent">
                                {Math.round(selectedItem.damage)} DMG
                              </span>
                            ) : null}
                            {selectedItem.armorRating ? (
                              <span className="enchanting-vessel__stat-chip is-accent">
                                {Math.round(selectedItem.armorRating)} AR
                              </span>
                            ) : null}
                          </div>
                        </div>
                      </div>

                      <div className="enchanting-altar__conduit" role="navigation" aria-label="Binding ritual conduit">
                        <button
                          type="button"
                          className={`enchanting-conduit__node${vesselReady ? " is-done" : ""}${slotStep === "item" ? " is-active" : ""}`}
                          onClick={() => setSlotStep("item")}
                        >
                          <b>I</b>
                          <span>Vessel ×{totalPieceCount}</span>
                        </button>
                        <span className={`enchanting-conduit__link${vesselReady && sigilsReady ? " is-lit" : ""}`} />
                        <button
                          type="button"
                          className={`enchanting-conduit__node${sigilsReady ? " is-done" : ""}${slotStep === "enchantment" ? " is-active" : ""}`}
                          onClick={() => setSlotStep("enchantment")}
                        >
                          <b>II</b>
                          <span>Sigils {selectedEnchantments.length}</span>
                        </button>
                        <span className={`enchanting-conduit__link${sigilsReady && soulReady ? " is-lit" : ""}`} />
                        <button
                          type="button"
                          className={`enchanting-conduit__node${soulReady ? " is-done" : ""}${slotStep === "soulgem" ? " is-active" : ""}`}
                          onClick={() => setSlotStep("soulgem")}
                        >
                          <b>III</b>
                          <span>Soul {allocatedGemList.length}/{requiredGemCount}</span>
                        </button>
                      </div>
                    </div>

                    <div className="enchanting-altar__section-head">
                      <span>STAGED {selectedItems.length > 1 ? `VESSELS (${totalPieceCount})` : "VESSEL"}</span>
                      <span>RENAME &amp; QUANTITY</span>
                    </div>

                    <div className="enchanting-batch-list">
                      {selectedItems.map((it) => {
                        const qty = getItemQty(it);
                        const itemNameVal = itemCustomNames[it.id] ?? "";
                        return (
                          <div key={it.id} className="enchanting-batch-item">
                            <div className="enchanting-batch-item__head">
                              <div className="enchanting-batch-item__title">
                                <ItemThumbnail item={toThumbnailItem(it)} bridge={bridge ?? undefined} paused={obscured} />
                                <span>{it.name}</span>
                                {it.count > 1 ? (
                                  <span className="enchanting-count">×{qty}/{it.count}</span>
                                ) : null}
                              </div>
                              {selectedItems.length > 1 ? (
                                <button
                                  type="button"
                                  className="enchanting-batch-item__remove"
                                  onClick={() => toggleTargetItem(it)}
                                  title={`Remove ${it.name} from batch`}
                                >
                                  ×
                                </button>
                              ) : null}
                            </div>

                            <div className="enchanting-batch-item__rename">
                              <span className="enchanting-batch-item__rename-icon" aria-hidden="true">
                                <Icon name="pen" size={11} />
                              </span>
                              <input
                                type="text"
                                maxLength={64}
                                value={itemNameVal}
                                onChange={(e) => setBatchItemName(it.id, e.target.value)}
                                placeholder={
                                  selectedItems.length > 1
                                    ? `Rename ${it.name} (optional)...`
                                    : `Custom name for ${it.name} (optional)`
                                }
                              />
                              {itemNameVal ? (
                                <button
                                  type="button"
                                  className="enchanting-btn enchanting-btn--icon"
                                  onClick={() => setBatchItemName(it.id, "")}
                                  title={`Reset name for ${it.name}`}
                                >
                                  <Icon name="close" size={12} />
                                </button>
                              ) : null}
                            </div>

                            {superActive && it.count > 1 ? (
                              <div className="enchanting-batch-item__qty">
                                <div className="enchanting-altar__section-head">
                                  <span>ENCHANT QUANTITY</span>
                                  <div className="enchanting-qty-stepper">
                                    <button
                                      type="button"
                                      className="enchanting-qty-stepper__btn"
                                      disabled={qty <= 1}
                                      onClick={() => setBatchItemQuantity(it, qty - 1)}
                                      aria-label={`Decrease quantity for ${it.name}`}
                                    >
                                      -
                                    </button>
                                    <strong>{qty} / {it.count}</strong>
                                    <button
                                      type="button"
                                      className="enchanting-qty-stepper__btn"
                                      disabled={qty >= it.count}
                                      onClick={() => setBatchItemQuantity(it, qty + 1)}
                                      aria-label={`Increase quantity for ${it.name}`}
                                    >
                                      +
                                    </button>
                                    <button
                                      type="button"
                                      className="enchanting-qty-stepper__max"
                                      disabled={qty >= it.count}
                                      onClick={() => setBatchItemQuantity(it, it.count)}
                                    >
                                      Max
                                    </button>
                                  </div>
                                </div>
                                <input
                                  type="range"
                                  className="enchanting-range"
                                  min={1}
                                  max={it.count}
                                  step={1}
                                  value={qty}
                                  aria-label={`Quantity of ${it.name} to enchant`}
                                  onChange={(e) => setBatchItemQuantity(it, Number(e.target.value))}
                                />
                              </div>
                            ) : null}
                          </div>
                        );
                      })}
                    </div>
                  </>
                ) : (
                  <Empty
                    title="Select a Vessel"
                    detail="Pick an unenchanted weapon, armor piece, or jewelry from Step I."
                    icon="sword"
                  />
                )}
              </div>

              <div className="enchanting-effects-box">
                <div className="enchanting-altar__section-head">
                  <span>BOUND SIGILS ({selectedEnchantments.length})</span>
                  <button
                    type="button"
                    className="enchanting-link"
                    onClick={() => setSlotStep("enchantment")}
                  >
                    + Choose Effect
                  </button>
                </div>

                {selectedEnchantments.length === 0 ? (
                  <button type="button" className="enchanting-empty-slot" onClick={() => setSlotStep("enchantment")}>
                    {unlimitedSlots
                      ? "Select any number of enchantments in Step II (Super Enchanting active)."
                      : `Select up to ${maxEnchantments} compatible enchantment${maxEnchantments > 1 ? "s" : ""} in Step II.`}
                  </button>
                ) : (
                  <div className="enchanting-effects-list">
                    {selectedEnchantments.map((ench) => (
                      <div key={ench.id} className="enchanting-effect-card">
                        <div className="enchanting-effect-card__head">
                          <div className="enchanting-effect-card__title">
                            <EnchantmentSigil
                              name={ench.originalName || ench.name}
                              effectName={ench.effects.map((e) => `${e.name} ${e.descriptionTemplate ?? ""}`).join(" ")}
                              school={ench.school}
                              size="sm"
                            />
                            <strong>{ench.name}</strong>
                          </div>
                          <button
                            type="button"
                            className="enchanting-btn enchanting-btn--icon"
                            onClick={() => setSelectedEnchIds((p) => p.filter((x) => x !== ench.id))}
                            title="Remove enchantment"
                          >
                            <Icon name="close" size={12} />
                          </button>
                        </div>
                        {ench.effects.map((eff, idx) => (
                          <p key={`${ench.id}-${idx}`} className="enchanting-effect-card__desc">
                            {formatScaledEffectText(eff, effectiveMagnitudeScale)}
                          </p>
                        ))}
                      </div>
                    ))}
                  </div>
                )}
              </div>

              <div className="enchanting-gem-box">
                <div className="enchanting-altar__section-head">
                  <span>
                    SOUL CONDUIT ({allocatedGemList.length}/{requiredGemCount})
                  </span>
                  <button type="button" className="enchanting-link" onClick={() => setSlotStep("soulgem")}>
                    {requiredGemCount > 1 ? "Swap Gem Types" : "Change Gem"}
                  </button>
                </div>
                <div className="enchanting-soul-meter" aria-hidden="true">
                  <div
                    className={`enchanting-soul-meter__fill${allocatedGemList.length >= requiredGemCount ? " is-full" : allocatedGemList.length > 0 ? " is-partial" : ""}`}
                    style={{ width: `${soulFillPct}%` }}
                  />
                </div>
                {allocatedGemList.length > 0 ? (
                  requiredGemCount > 1 ? (
                    <div className="enchanting-gem-chips">
                      {Object.entries(gemAllocations).map(([gid, qty]) => {
                        const gem = snapshot.soulGems.find((g) => g.id === gid);
                        if (!gem || qty <= 0) return null;
                        return (
                          <span key={gid} className="enchanting-gem-chip">
                            <span>
                              {qty}× {gem.name}
                            </span>
                          </span>
                        );
                      })}
                      {allocatedGemList.length < requiredGemCount ? (
                        <span className="enchanting-pill enchanting-pill--blocked">
                          Need {requiredGemCount} filled Soul Gems ({totalAvailableSoulGems} carried)
                        </span>
                      ) : null}
                    </div>
                  ) : (
                    <div className="enchanting-gem-summary">
                      <ItemThumbnail item={toSoulGemThumbnailItem(selectedGem!)} bridge={bridge ?? undefined} paused={obscured} />
                      <div className="enchanting-gem-summary__meta">
                        <strong>{selectedGem!.name}</strong>
                        <span>
                          {selectedGem!.soulLabel} ({selectedGem!.soulPoints} pts) ·{" "}
                          {selectedGem!.isReusable
                            ? "Star retained (soul emptied)"
                            : "Consumed on binding"}
                        </span>
                      </div>
                    </div>
                  )
                ) : (
                  <button type="button" className="enchanting-empty-slot" onClick={() => setSlotStep("soulgem")}>
                    No filled Soul Gems available ({requiredGemCount} required).
                  </button>
                )}
              </div>

              {isWeaponCraft && selectedEnchantments.length > 0 && selectedGem ? (
                <div className="enchanting-slider-box">
                  <div className="enchanting-altar__section-head">
                    <span>POWER VS. CHARGES</span>
                    <span>
                      {weaponPreview.uses} Uses · {weaponPreview.maxCharge} Charge
                    </span>
                  </div>
                  <input
                    type="range"
                    className="enchanting-range"
                    min={0.15}
                    max={1.0}
                    step={0.05}
                    value={powerSlider}
                    onChange={(e) => setPowerSlider(parseFloat(e.target.value))}
                  />
                  <div className="enchanting-slider-box__labels">
                    <span>{Math.round(powerSlider * 100)}% Power</span>
                    <span>{weaponPreview.costPerUse} Charge / Hit</span>
                  </div>
                </div>
              ) : null}

              <div className="enchanting-altar__footer">
                <button
                  type="button"
                  className={`enchanting-cta${canCraftEnchant ? " is-ready" : ""}`}
                  disabled={!canCraftEnchant || busy}
                  onClick={() => void handlePerformEnchant()}
                >
                  <Icon name="enchant" size={16} />
                  <span>
                    {busy
                      ? "Binding Arcane Sigils..."
                      : totalPieceCount > 1
                        ? `Super-Enchant ${totalPieceCount} Items (${requiredGemCount} Gems)`
                        : `Craft ${(selectedItem ? (itemCustomNames[selectedItem.id] ?? "").trim() : "") || selectedItem?.name || "Enchanted Item"}`}
                  </span>
                  <kbd>Enter</kbd>
                </button>
                <p className="enchanting-altar__hint">
                  Fires native Skyrim enchanting events, consumes {requiredGemCount === 1 ? "the soul" : `${requiredGemCount} soul gems`}, and grants Enchanting skill XP.
                </p>
              </div>
            </div>
            );
          })() : (
            <div className="enchanting-altar__card">
              <div className="enchanting-altar__header">
                <div className="enchanting-altar__crest">
                  <div className={`enchanting-altar__emblem${selectedDisenchantItem?.canDisenchant ? " is-ready" : ""}`}>
                    <AltarCrestEmblem variant="extract" />
                  </div>
                  <div className="enchanting-altar__headings">
                    <span className="enchanting-altar__eyebrow">ARCANE EXTRACTION</span>
                    <strong className="enchanting-altar__title">Disenchanting Altar</strong>
                  </div>
                </div>
                <div className="enchanting-altar__header-badges">
                  <span
                    className={`enchanting-altar__readiness is-${
                      !selectedDisenchantItem
                        ? "idle"
                        : selectedDisenchantItem.canDisenchant
                          ? "ready"
                          : "warn"
                    }`}
                  >
                    {!selectedDisenchantItem
                      ? "PICK RELIC"
                      : selectedDisenchantItem.canDisenchant
                        ? "EXTRACTABLE"
                        : "PROTECTED"}
                  </span>
                </div>
              </div>

              {selectedDisenchantItem ? (
                <>
                  <div className="enchanting-altar__stage enchanting-altar__stage--extract">
                    <div className="enchanting-altar__pedestal">
                      <div className={`enchanting-altar__plinth${selectedDisenchantItem.canDisenchant ? " is-charged" : ""}`}>
                        <AltarPlinthBackdrop />
                        <ItemThumbnail
                          item={toThumbnailItem({ ...selectedDisenchantItem, enchanted: true })}
                          bridge={bridge ?? undefined}
                          paused={obscured}
                        />
                      </div>
                      <div className="enchanting-vessel__meta">
                        <div className="enchanting-vessel__kind">
                          {selectedDisenchantItem.weaponType ||
                            selectedDisenchantItem.armorTypeLabel ||
                            selectedDisenchantItem.category}
                        </div>
                        <h2>{selectedDisenchantItem.name}</h2>
                        <div className="enchanting-vessel__stats">
                          <span className="enchanting-vessel__stat-chip">
                            {selectedDisenchantItem.value.toLocaleString()}g
                          </span>
                          <span className="enchanting-vessel__stat-chip">
                            {selectedDisenchantItem.weight} wt
                          </span>
                          {selectedDisenchantItem.canDisenchant ? (
                            <span className="enchanting-vessel__stat-chip is-accent">
                              +{selectedDisenchantItem.estimatedXp < 10 ? selectedDisenchantItem.estimatedXp.toFixed(1) : Math.round(selectedDisenchantItem.estimatedXp)} XP
                            </span>
                          ) : null}
                        </div>
                      </div>
                    </div>

                    <div className="enchanting-altar__conduit" aria-hidden="true">
                      <span className="enchanting-conduit__node is-done">
                        <b>I</b>
                        <span>Relic</span>
                      </span>
                      <span className={`enchanting-conduit__link${selectedDisenchantItem.canDisenchant ? " is-lit" : ""}`} />
                      <span className={`enchanting-conduit__node${selectedDisenchantItem.canDisenchant ? " is-done" : ""}`}>
                        <b>II</b>
                        <span>Unravel</span>
                      </span>
                      <span className={`enchanting-conduit__link${selectedDisenchantItem.canDisenchant ? " is-lit" : ""}`} />
                      <span className={`enchanting-conduit__node${selectedDisenchantItem.canDisenchant ? " is-done" : ""}`}>
                        <b>III</b>
                        <span>Learn Sigil</span>
                      </span>
                    </div>
                  </div>

                  <div className="enchanting-effects-box">
                    <div className="enchanting-altar__section-head">
                      <span>EXTRACTS ENCHANTMENT</span>
                      <span>
                        {selectedDisenchantItem.canDisenchant
                          ? `+${selectedDisenchantItem.estimatedXp < 10 ? selectedDisenchantItem.estimatedXp.toFixed(1) : Math.round(selectedDisenchantItem.estimatedXp)} XP`
                          : "LOCKED"}
                      </span>
                    </div>
                    <div className="enchanting-extract-card">
                      <EnchantmentSigil
                        name={selectedDisenchantItem.enchantmentName}
                        effectName={selectedDisenchantItem.effects.map((e) => `${e.name} ${e.description ?? ""}`).join(" ")}
                        size="lg"
                      />
                      <div className="enchanting-extract-card__body">
                        <strong>{selectedDisenchantItem.enchantmentName}</strong>
                        {selectedDisenchantItem.effects.map((eff, idx) => (
                          <p key={`dis-${idx}`} className="enchanting-effect-card__desc">
                            {eff.description || eff.name}
                          </p>
                        ))}
                      </div>
                    </div>
                  </div>

                  {selectedDisenchantItem.canDisenchant ? (
                    <div className="enchanting-warning">
                      <Icon name="alertTriangle" size={16} />
                      <span>
                        Disenchanting permanently destroys <strong>{selectedDisenchantItem.name}</strong> to learn{" "}
                        <strong>{selectedDisenchantItem.enchantmentName}</strong>.
                      </span>
                    </div>
                  ) : (
                    <div className="enchanting-banner enchanting-banner--error">
                      <Icon name="lock" size={15} />
                      <span>{selectedDisenchantItem.blockReason || "This item cannot be disenchanted."}</span>
                    </div>
                  )}

                  <div className="enchanting-altar__footer">
                    <button
                      type="button"
                      className={`enchanting-cta${confirmDisenchantArmed ? " is-armed" : selectedDisenchantItem.canDisenchant ? " is-ready" : ""}`}
                      disabled={!selectedDisenchantItem.canDisenchant || busy}
                      onClick={() => void handlePerformDisenchant()}
                    >
                      <Icon name={confirmDisenchantArmed ? "trash" : "rune"} size={16} />
                      <span>
                        {busy
                          ? "Extracting..."
                          : confirmDisenchantArmed
                            ? `Confirm Destroy & Learn ${selectedDisenchantItem.enchantmentName}`
                            : `Disenchant ${selectedDisenchantItem.name}`}
                      </span>
                      <kbd>Enter</kbd>
                    </button>
                    {confirmDisenchantArmed ? (
                      <button
                        type="button"
                        className="enchanting-btn enchanting-btn--chip"
                        onClick={() => setConfirmDisenchantArmed(false)}
                      >
                        Cancel Destruction (Esc)
                      </button>
                    ) : null}
                  </div>
                </>
              ) : (
                <Empty
                  title="No Enchanted Item Selected"
                  detail="Pick an enchanted item on the left to inspect its base enchantment and skill XP reward."
                  icon="rune"
                />
              )}
            </div>
          )}
        </aside>
      </div>
    </div>
  );
}
