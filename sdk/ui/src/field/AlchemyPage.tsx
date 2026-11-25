import * as React from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  createSkyUltraGame,
  type SkyUltraAlchemyIngredient,
  type SkyUltraAlchemyIngredientEffect,
  type SkyUltraAlchemyKnownEffect,
  type SkyUltraAlchemySnapshot,
  type SkyUltraContainerDisplayMode,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import { SkyChoice, SkySegmentedControl } from "../core.js";
import { ItemThumbnail } from "./ItemThumbnail.js";
import { Icon } from "./icons.js";
import { Empty } from "./pieces.js";
import type { Item, Player } from "./model.js";

export interface AlchemyPageProps {
  player: Player | null;
  bridge: SkyWebViewAPI | null;
  obscured?: boolean;
  displayMode?: SkyUltraContainerDisplayMode;
  pauseWorld?: boolean;
  onChangeDisplayMode?: (mode: SkyUltraContainerDisplayMode) => void;
  onTogglePauseWorld?: () => void;
  onClose?: () => void;
}

export interface AlchemyPowerContext {
  alchemySkill?: number;
  fortifyAlchemyPct?: number;
  initMult?: number;
  skillFactor?: number;
  goldMult?: number;
  alchemistRank?: number;
  hasPhysician?: boolean;
  hasBenefactor?: boolean;
  hasPoisoner?: boolean;
  hasPurity?: boolean;
}

export interface AlchemyPreviewEffect {
  effectId: string;
  name: string;
  school: string;
  isHostile: boolean;
  isDetrimental: boolean;
  isPoisonEffect: boolean;
  magnitude: number;
  duration: number;
  cost: number;
  text: string;
  sourceCount: number;
}

export interface AlchemyCraftPreview {
  valid: boolean;
  isPoison: boolean;
  isExperimentalMix: boolean;
  hasUnknownTraits: boolean;
  selectedCount: number;
  dominantName: string;
  defaultName: string;
  goldValue: number;
  xpPerCraft: number;
  maxCraftable: number;
  purifiedCount: number;
  purifiedNames: string[];
  effects: AlchemyPreviewEffect[];
}

export interface AlchemyBrewableRecipe {
  id: string;
  title: string;
  ingredientIds: string[];
  ingredientNames: string[];
  isPoison: boolean;
  effects: AlchemyPreviewEffect[];
  goldValue: number;
  xpPerCraft: number;
  maxCraftable: number;
  purifiedCount: number;
}

type AlchemyTab = "ingredients" | "effects" | "recipes";
type IngredientFilter = "all" | "compatible" | "unknown" | "potion" | "poison";
type EffectPolarityFilter = "all" | "potion" | "poison";
type AlchemySortBy = "name" | "count" | "value" | "known";

const DISPLAY_MODE_ORDER: SkyUltraContainerDisplayMode[] = ["half", "full", "windowed"];
const DISPLAY_MODE_LABELS: Record<SkyUltraContainerDisplayMode, string> = {
  half: "Half Container",
  full: "Full Screen",
  windowed: "Windowed",
};

const LIST_ITEMS_PER_PAGE = 18;
const GRID_ITEMS_PER_PAGE = 20;

function computeAlchemyEffectCost(baseCost: number, magnitude: number, duration: number): number {
  const safeBase = Number.isFinite(baseCost) && baseCost > 0 ? baseCost : 1;
  const magFactor = magnitude <= 1 ? 1 : Math.pow(magnitude, 1.1);
  const durFactor = duration <= 0 ? 1 : Math.pow(duration / 10, 1.1);
  return Math.max(0, safeBase * magFactor * durFactor);
}

/**
 * Formats an alchemical MagicEffect description (`<mag>`, `<dur>`) at brewed potency.
 */
export function formatAlchemyEffectText(
  effect: Pick<
    SkyUltraAlchemyIngredientEffect,
    | "name"
    | "descriptionTemplate"
    | "baseMagnitude"
    | "baseDuration"
    | "powerAffectsMagnitude"
    | "powerAffectsDuration"
    | "noMagnitude"
    | "noDuration"
  >,
  magnitude: number,
  duration: number,
): string {
  const cleanName = (effect.name ?? "Alchemical Effect").trim() || "Alchemical Effect";
  const roundedMag = effect.noMagnitude ? 0 : Math.max(0, Math.round(magnitude));
  const roundedDur = effect.noDuration ? 0 : Math.max(0, Math.round(duration));
  const tpl = (effect.descriptionTemplate ?? "").trim();
  if (tpl.length > 0) {
    return tpl
      .replace(/<\s*mag\s*>/gi, String(roundedMag))
      .replace(/<\s*dur\s*>/gi, String(roundedDur))
      .trim();
  }
  if (roundedMag > 0 && roundedDur > 0) {
    return `${cleanName} for ${roundedMag} pts over ${roundedDur}s`;
  }
  if (roundedMag > 0) {
    return `${cleanName} for ${roundedMag} pts`;
  }
  if (roundedDur > 0) {
    return `${cleanName} for ${roundedDur}s`;
  }
  return cleanName;
}

/**
 * Computes live potion/poison polarity, perk scaling, Purity filtering, gold value, and XP
 * mirroring the native Skyrim Alchemy calculation in `Papyrus.cpp`.
 */
export function computeAlchemyPreview(
  ingredients: ReadonlyArray<SkyUltraAlchemyIngredient>,
  selectedIds: ReadonlyArray<string>,
  context: AlchemyPowerContext,
): AlchemyCraftPreview {
  const byId = new Map(ingredients.map((item) => [item.id, item]));
  const selected: SkyUltraAlchemyIngredient[] = [];
  const seenIds = new Set<string>();
  for (const id of selectedIds) {
    if (seenIds.has(id)) continue;
    const ing = byId.get(id);
    if (!ing) continue;
    seenIds.add(id);
    selected.push(ing);
    if (selected.length >= 3) break;
  }

  const hasUnknownTraits = selected.some((ing) => ing.effects.some((e) => !e.known));
  if (selected.length < 2) {
    return {
      valid: false,
      isPoison: false,
      isExperimentalMix: false,
      hasUnknownTraits,
      selectedCount: selected.length,
      dominantName: "",
      defaultName: "",
      goldValue: 0,
      xpPerCraft: 0,
      maxCraftable: 0,
      purifiedCount: 0,
      purifiedNames: [],
      effects: [],
    };
  }

  const initMult = Number.isFinite(context.initMult) && (context.initMult ?? 0) > 0 ? Number(context.initMult) : 4.0;
  const skillFactor = Number.isFinite(context.skillFactor) && (context.skillFactor ?? 0) > 0 ? Number(context.skillFactor) : 1.5;
  const goldMult = Number.isFinite(context.goldMult) && (context.goldMult ?? 0) > 0 ? Number(context.goldMult) : 0.35;
  const skill = Math.max(0, Number(context.alchemySkill ?? 15));
  const fortifyPct = Number(context.fortifyAlchemyPct ?? 0);
  const alchemistRank = Math.max(0, Math.min(5, Math.floor(Number(context.alchemistRank ?? 0))));

  const skillMult = initMult * (1 + (skillFactor - 1) * (skill / 100));
  const fortifyMult = Math.max(0.1, 1 + fortifyPct / 100);
  const alchemistMult = 1 + 0.2 * alchemistRank;
  const basePower = skillMult * fortifyMult * alchemistMult;

  interface CandidateEffect {
    effectId: string;
    sample: SkyUltraAlchemyIngredientEffect;
    sourceCount: number;
    baselineCost: number;
  }

  const candidateMap = new Map<string, CandidateEffect>();
  for (const ing of selected) {
    const perIngSeen = new Set<string>();
    for (const eff of ing.effects) {
      if (!eff.known || !eff.effectId) continue;
      if (perIngSeen.has(eff.effectId)) continue;
      perIngSeen.add(eff.effectId);

      const baseMag = Math.max(0, eff.baseMagnitude ?? 0);
      const baseDur = Math.max(0, eff.baseDuration ?? 0);
      const scaleMag = !eff.noMagnitude && (eff.powerAffectsMagnitude || !eff.powerAffectsDuration);
      const scaleDur = !eff.noDuration && Boolean(eff.powerAffectsDuration) && !eff.powerAffectsMagnitude;
      const trialMag = eff.noMagnitude || baseMag <= 0 ? 0 : Math.max(1, Math.round(baseMag * (scaleMag ? basePower : 1)));
      const trialDur = eff.noDuration || baseDur <= 0 ? 0 : Math.max(1, Math.round(baseDur * (scaleDur ? basePower : 1)));
      const trialCost = computeAlchemyEffectCost(eff.baseCost ?? 1, trialMag, trialDur);

      const existing = candidateMap.get(eff.effectId);
      if (!existing) {
        candidateMap.set(eff.effectId, {
          effectId: eff.effectId,
          sample: eff,
          sourceCount: 1,
          baselineCost: trialCost,
        });
      } else {
        existing.sourceCount += 1;
        if (trialCost > existing.baselineCost) {
          existing.sample = eff;
          existing.baselineCost = trialCost;
        }
      }
    }
  }

  const matched = Array.from(candidateMap.values()).filter((c) => c.sourceCount >= 2);
  if (matched.length === 0) {
    return {
      valid: false,
      isPoison: false,
      isExperimentalMix: hasUnknownTraits,
      hasUnknownTraits,
      selectedCount: selected.length,
      dominantName: "",
      defaultName: "",
      goldValue: 0,
      xpPerCraft: 0,
      maxCraftable: 1,
      purifiedCount: 0,
      purifiedNames: [],
      effects: [],
    };
  }

  matched.sort((a, b) => b.baselineCost - a.baselineCost);
  const dominantRaw = matched[0]!.sample;
  const isPoison = Boolean(dominantRaw.isHostile || dominantRaw.isDetrimental);

  const purifiedNames: string[] = [];
  const surviving = matched.filter((cand) => {
    const effNeg = Boolean(cand.sample.isHostile || cand.sample.isDetrimental);
    if (context.hasPurity && effNeg !== isPoison) {
      purifiedNames.push((cand.sample.name ?? "Effect").trim() || "Effect");
      return false;
    }
    return true;
  });

  const resolvedEffects: AlchemyPreviewEffect[] = surviving.map((cand) => {
    const eff = cand.sample;
    const effNeg = Boolean(eff.isHostile || eff.isDetrimental);
    let perkMult = 1.0;
    if (context.hasPhysician && eff.isRestoreVital) {
      perkMult *= 1.25;
    }
    if (!isPoison && context.hasBenefactor && !effNeg) {
      perkMult *= 1.25;
    }
    if (isPoison && context.hasPoisoner && effNeg) {
      perkMult *= 1.25;
    }
    const finalPower = basePower * perkMult;
    const baseMag = Math.max(0, eff.baseMagnitude ?? 0);
    const baseDur = Math.max(0, eff.baseDuration ?? 0);
    const scaleMag = !eff.noMagnitude && (eff.powerAffectsMagnitude || !eff.powerAffectsDuration);
    const scaleDur = !eff.noDuration && Boolean(eff.powerAffectsDuration) && !eff.powerAffectsMagnitude;

    const magnitude =
      eff.noMagnitude || baseMag <= 0 ? 0 : Math.max(1, Math.round(baseMag * (scaleMag ? finalPower : 1)));
    const duration =
      eff.noDuration || baseDur <= 0 ? 0 : Math.max(1, Math.round(baseDur * (scaleDur ? finalPower : 1)));
    const cost = computeAlchemyEffectCost(eff.baseCost ?? 1, magnitude, duration);
    const name = (eff.name ?? "Alchemical Effect").trim() || "Alchemical Effect";

    return {
      effectId: cand.effectId,
      name,
      school: eff.school || "Alchemy",
      isHostile: Boolean(eff.isHostile),
      isDetrimental: Boolean(eff.isDetrimental),
      isPoisonEffect: effNeg,
      magnitude,
      duration,
      cost,
      text: formatAlchemyEffectText(eff, magnitude, duration),
      sourceCount: cand.sourceCount,
    };
  });

  resolvedEffects.sort((a, b) => b.cost - a.cost);
  const dominant = resolvedEffects[0]!;
  const sumCost = resolvedEffects.reduce((sum, e) => sum + e.cost, 0);
  const goldValue = Math.max(1, Math.floor(sumCost));
  const xpPerCraft = Math.round(Math.max(1, goldValue * goldMult) * 10) / 10;
  const maxCraftable = Math.max(0, Math.min(...selected.map((s) => Math.max(0, s.count))));

  return {
    valid: true,
    isPoison,
    isExperimentalMix: false,
    hasUnknownTraits,
    selectedCount: selected.length,
    dominantName: dominant.name,
    defaultName: `${isPoison ? "Poison" : "Potion"} of ${dominant.name}`,
    goldValue,
    xpPerCraft,
    maxCraftable,
    purifiedCount: purifiedNames.length,
    purifiedNames,
    effects: resolvedEffects,
  };
}

/**
 * Computes discoverable 2-ingredient and 3-ingredient recipes from known traits in the player's pouch.
 */
export function computeBrewableRecipes(
  ingredients: ReadonlyArray<SkyUltraAlchemyIngredient>,
  context: AlchemyPowerContext,
): AlchemyBrewableRecipe[] {
  const carried = ingredients.filter((ing) => ing.count > 0);
  if (carried.length < 2) return [];

  const effectBuckets = new Map<string, SkyUltraAlchemyIngredient[]>();
  for (const ing of carried) {
    const seen = new Set<string>();
    for (const eff of ing.effects) {
      if (!eff.known || !eff.effectId || seen.has(eff.effectId)) continue;
      seen.add(eff.effectId);
      const list = effectBuckets.get(eff.effectId);
      if (list) list.push(ing);
      else effectBuckets.set(eff.effectId, [ing]);
    }
  }

  const pairKeys = new Set<string>();
  const pairs: Array<[SkyUltraAlchemyIngredient, SkyUltraAlchemyIngredient]> = [];
  for (const bucket of effectBuckets.values()) {
    if (bucket.length < 2) continue;
    for (let i = 0; i < bucket.length; i += 1) {
      for (let j = i + 1; j < bucket.length; j += 1) {
        const a = bucket[i]!;
        const b = bucket[j]!;
        const key = a.id < b.id ? `${a.id}|${b.id}` : `${b.id}|${a.id}`;
        if (pairKeys.has(key)) continue;
        pairKeys.add(key);
        pairs.push([a, b]);
      }
    }
  }

  const recipes: AlchemyBrewableRecipe[] = [];
  const recipeKeys = new Set<string>();

  for (const [a, b] of pairs) {
    const preview = computeAlchemyPreview(carried, [a.id, b.id], context);
    if (!preview.valid || preview.effects.length === 0) continue;
    const sortedPair = a.id < b.id ? [a, b] : [b, a];
    const ids = sortedPair.map((x) => x.id);
    const key = ids.join("|");
    if (!recipeKeys.has(key)) {
      recipeKeys.add(key);
      recipes.push({
        id: key,
        title: preview.defaultName,
        ingredientIds: ids,
        ingredientNames: sortedPair.map((x) => x.name),
        isPoison: preview.isPoison,
        effects: preview.effects,
        goldValue: preview.goldValue,
        xpPerCraft: preview.xpPerCraft,
        maxCraftable: preview.maxCraftable,
        purifiedCount: preview.purifiedCount,
      });
    }
  }

  // Upgrade top pairs into 3-ingredient multi-effect combos when a 3rd carried reagent adds a new effect.
  recipes.sort((a, b) => b.goldValue - a.goldValue || b.effects.length - a.effects.length);
  const topPairs = recipes.slice(0, 36);
  const threeRecipes: AlchemyBrewableRecipe[] = [];

  for (const base of topPairs) {
    const baseEffectIds = new Set(base.effects.map((e) => e.effectId));
    const pairSet = new Set(base.ingredientIds);
    let bestThird: AlchemyBrewableRecipe | null = null;
    for (const cand of carried) {
      if (pairSet.has(cand.id)) continue;
      const candKnown = cand.effects.some((e) => e.known && e.effectId && !baseEffectIds.has(e.effectId));
      if (!candKnown) continue;
      const triIds = [...base.ingredientIds, cand.id];
      const preview = computeAlchemyPreview(carried, triIds, context);
      if (!preview.valid || preview.effects.length <= base.effects.length) continue;
      const canonical = [...triIds].sort();
      const key = canonical.join("|");
      if (recipeKeys.has(key)) continue;
      if (!bestThird || preview.goldValue > bestThird.goldValue) {
        bestThird = {
          id: key,
          title: preview.defaultName,
          ingredientIds: canonical,
          ingredientNames: canonical.map((id) => carried.find((c) => c.id === id)?.name || id),
          isPoison: preview.isPoison,
          effects: preview.effects,
          goldValue: preview.goldValue,
          xpPerCraft: preview.xpPerCraft,
          maxCraftable: preview.maxCraftable,
          purifiedCount: preview.purifiedCount,
        };
      }
    }
    if (bestThird && !recipeKeys.has(bestThird.id)) {
      recipeKeys.add(bestThird.id);
      threeRecipes.push(bestThird);
    }
  }

  const combined = [...threeRecipes, ...recipes];
  combined.sort(
    (a, b) =>
      b.goldValue - a.goldValue ||
      b.effects.length - a.effects.length ||
      b.maxCraftable - a.maxCraftable ||
      a.title.localeCompare(b.title),
  );
  return combined.slice(0, 120);
}

type AlchemySigilKind =
  | "health"
  | "magicka"
  | "stamina"
  | "fire"
  | "frost"
  | "shock"
  | "poison"
  | "shadow"
  | "fortify"
  | "cure";

function classifyAlchemySigil(name: string, isPoison?: boolean): { kind: AlchemySigilKind; tone: string } {
  const n = name.toLowerCase();
  if (n.includes("fire")) return { kind: "fire", tone: "fire" };
  if (n.includes("frost")) return { kind: "frost", tone: "frost" };
  if (n.includes("shock")) return { kind: "shock", tone: "shock" };
  if (n.includes("invisib") || n.includes("paral") || n.includes("frenzy") || n.includes("fear") || n.includes("slow")) {
    return { kind: "shadow", tone: isPoison ? "poison" : "shadow" };
  }
  if (isPoison || n.includes("damage") || n.includes("ravage") || n.includes("linger") || n.includes("weakness")) {
    return { kind: "poison", tone: "poison" };
  }
  if (n.includes("cure") || n.includes("waterbreath") || n.includes("resist poison") || n.includes("resist disease")) {
    return { kind: "cure", tone: "cure" };
  }
  if (n.includes("magicka")) return { kind: "magicka", tone: "magicka" };
  if (n.includes("stamina") || n.includes("carry")) return { kind: "stamina", tone: "stamina" };
  if (n.includes("health")) return { kind: "health", tone: "health" };
  return { kind: "fortify", tone: isPoison ? "poison" : "fortify" };
}

function AlchemyEffectSigil({
  name,
  isPoison = false,
  size = "md",
}: {
  name: string;
  isPoison?: boolean;
  size?: "sm" | "md" | "lg";
}) {
  const { kind, tone } = classifyAlchemySigil(name, isPoison);
  const px = size === "sm" ? 13 : size === "lg" ? 22 : 17;

  let artwork = (
    <>
      <path d="M12 3.5c-2.5 3.6-5.2 6.8-5.2 10.7A5.2 5.2 0 0 0 12 19.5a5.2 5.2 0 0 0 5.2-5.3C17.2 10.3 14.5 7.1 12 3.5z" fill="currentColor" fillOpacity=".22" />
      <path d="M9.8 14.5h4.4M12 12.3v4.4" />
    </>
  );

  if (kind === "health") {
    artwork = (
      <>
        <path d="M6 4.5h12v7.2c0 4.5-6 8.3-6 8.3s-6-3.8-6-8.3V4.5z" fill="currentColor" fillOpacity=".22" />
        <path d="M12 8.2v5.8M9.1 11.1h5.8" />
      </>
    );
  } else if (kind === "magicka") {
    artwork = (
      <>
        <path d="M14.8 4.2a7.6 7.6 0 1 0 3.8 13.8A6.3 6.3 0 0 1 14.8 4.2z" fill="currentColor" fillOpacity=".24" />
        <path d="M14.5 9.2c1.4-1.2 3.2-1.2 4.6 0m-4 3.4c1.2-.9 2.7-.9 3.9 0" />
      </>
    );
  } else if (kind === "stamina") {
    artwork = (
      <>
        <path d="M4.5 14.2c4.2 0 7.4-2.1 10.8-6.4l3.6-2.4-2.1 4.6c-2.1 4.5-6.2 7.8-12.3 7.8z" fill="currentColor" fillOpacity=".22" />
        <path d="M7 18.2h11.5M9.5 10.5l3.2 2" />
      </>
    );
  } else if (kind === "fire") {
    artwork = (
      <>
        <path d="M12.6 3.2c1 3-1.4 4.8-.6 7.1.7 1.7 2.8 1.4 3.1-.6 2 1.8 2.6 4.2 1.6 6.8-.9 2.3-2.9 3.5-4.8 3.5-3 0-5.4-2.2-5.4-5.4 0-3.2 2.4-5.2 4-7.2.9-1.1 1.6-2.6 2.1-4.2z" fill="currentColor" fillOpacity=".22" />
        <path d="M11.6 13.8c-1 .9-1.1 2.3-.2 3.2.8.8 2.1.7 2.6-.3" />
      </>
    );
  } else if (kind === "frost") {
    artwork = (
      <>
        <path d="M12 3v18M4.2 7.5l15.6 9M19.8 7.5l-15.6 9" />
        <path d="M9.6 5.2L12 7.4l2.4-2.2M9.6 18.8L12 16.6l2.4 2.2M5.4 10.2l2.2-.8-.8-2.2M17 16.8l-.8-2.2 2.2-.8" />
      </>
    );
  } else if (kind === "shock") {
    artwork = (
      <>
        <path d="M13.6 3L5.6 13h5l-1.4 8 9.2-10.8h-5.2L13.6 3z" fill="currentColor" fillOpacity=".24" />
        <path d="M17.5 15.8l2.5 2M4.5 7l2 1.8" strokeOpacity=".7" />
      </>
    );
  } else if (kind === "poison") {
    artwork = (
      <>
        <path d="M12 3.2c-2.6 3.6-5.6 6.8-5.6 10.9A5.6 5.6 0 0 0 12 19.8a5.6 5.6 0 0 0 5.6-5.7C17.6 10 14.6 6.8 12 3.2z" fill="currentColor" fillOpacity=".24" />
        <path d="M9.4 11.8l1.6 4.4 1-1.9 1 1.9 1.6-4.4" />
      </>
    );
  } else if (kind === "shadow") {
    artwork = (
      <>
        <path d="M2.8 12c2.5-3.9 5.6-5.8 9.2-5.8s6.7 1.9 9.2 5.8c-2.5 3.9-5.6 5.8-9.2 5.8S5.3 15.9 2.8 12z" fill="currentColor" fillOpacity=".2" />
        <polygon points="12 9.2 14.6 10.6 14.6 13.4 12 14.8 9.4 13.4 9.4 10.6" fill="currentColor" fillOpacity=".4" />
      </>
    );
  } else if (kind === "fortify") {
    artwork = (
      <>
        <polygon points="12 2.8 19.6 7.2 19.6 16.8 12 21.2 4.4 16.8 4.4 7.2" fill="currentColor" fillOpacity=".18" />
        <path d="M9 12.6l3-3.4 3 3.4M12 9.4v6.2" />
      </>
    );
  }

  return (
    <span className={`alchemy-sigil alchemy-sigil--${tone} alchemy-sigil--${size}`} aria-hidden="true">
      <svg width={px} height={px} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round">
        {artwork}
      </svg>
    </span>
  );
}

function AlchemyCrestEmblem() {
  return (
    <svg width="24" height="24" viewBox="0 0 32 32" fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
      <polygon points="10 2.5 22 2.5 29.5 10 29.5 22 22 29.5 10 29.5 2.5 22 2.5 10" fill="currentColor" fillOpacity=".1" />
      <path d="M12.5 7.5h7M13.5 7.5v4.3l-4.4 7.6A2.1 2.1 0 0 0 11 22.5h10a2.1 2.1 0 0 0 1.9-3.1l-4.4-7.6V7.5" />
      <path d="M10.1 17.8c1.9-.9 4 .9 5.9 0s4-.9 5.9 0l.4.8a1.4 1.4 0 0 1-1.3 2.1H11a1.4 1.4 0 0 1-1.3-2.1z" fill="currentColor" fillOpacity=".35" />
      <rect x="13.5" y="13.2" width="2" height="2" rx="0.5" fill="currentColor" fillOpacity=".5" />
      <rect x="17" y="14.9" width="1.6" height="1.6" rx="0.4" fill="currentColor" fillOpacity=".4" />
    </svg>
  );
}

function AlchemyPhialShowcase({
  mode,
  craftCount,
}: {
  mode: "empty" | "experimental" | "potion" | "poison";
  craftCount: number;
}) {
  return (
    <svg
      className={`alchemy-phial-svg alchemy-phial-svg--${mode}`}
      viewBox="0 0 180 144"
      fill="none"
      aria-hidden="true"
    >
      <path d="M36 128h108l-10 10H46z" fill="currentColor" fillOpacity=".14" stroke="currentColor" strokeOpacity=".45" strokeWidth="1.4" />
      <rect x="62" y="118" width="56" height="10" rx="3" fill="currentColor" fillOpacity=".2" stroke="currentColor" strokeOpacity=".55" strokeWidth="1.3" />

      {craftCount > 1 ? (
        <g opacity=".65">
          <path d="M33 56h12v14c6 4 10 11 10 19 0 11-7 19-16 19s-16-8-16-19c0-8 4-15 10-19V56z" fill="currentColor" fillOpacity=".09" stroke="currentColor" strokeWidth="1.3" />
          <path d="M26 91c4-2 9 2 13 0s9-2 13 0c0 8-5 14-13 14s-13-6-13-14z" fill="currentColor" fillOpacity=".28" />
          <path d="M135 56h12v14c6 4 10 11 10 19 0 11-7 19-16 19s-16-8-16-19c0-8 4-15 10-19V56z" fill="currentColor" fillOpacity=".09" stroke="currentColor" strokeWidth="1.3" />
          <path d="M128 91c4-2 9 2 13 0s9-2 13 0c0 8-5 14-13 14s-13-6-13-14z" fill="currentColor" fillOpacity=".28" />
        </g>
      ) : null}

      <rect x="81" y="12" width="18" height="11" rx="2.5" fill="currentColor" fillOpacity=".25" stroke="currentColor" strokeWidth="1.4" />
      <rect x="73" y="21" width="34" height="6" rx="2" fill="currentColor" fillOpacity=".18" stroke="currentColor" strokeWidth="1.4" />

      <path
        d="M78 27h24v19c14 6 24 20 24 36 0 20-16 35-36 35s-36-15-36-35c0-16 10-30 24-36V27z"
        fill="currentColor"
        fillOpacity={mode === "empty" ? ".06" : ".12"}
        stroke="currentColor"
        strokeWidth="2"
      />

      {mode === "empty" ? (
        <g stroke="currentColor" strokeOpacity=".36" strokeWidth="1.2" strokeDasharray="3 3">
          <path d="M62 84h56M68 97h44" />
        </g>
      ) : (
        <>
          <path
            d="M57 82c10-5 21 5 33 0s23-5 33 0c0 18-14 31-33 31s-33-13-33-31z"
            fill="currentColor"
            fillOpacity={mode === "experimental" ? ".28" : ".38"}
            stroke="currentColor"
            strokeOpacity=".65"
            strokeWidth="1.2"
          />
          {mode === "potion" ? (
            <g stroke="currentColor" strokeWidth="1.8" strokeLinecap="round">
              <path d="M90 66v22M79 77h22" />
              <path d="M84 42c-4 6-4 11 0 16M96 42c4 6 4 11 0 16" strokeOpacity=".55" strokeWidth="1.3" />
            </g>
          ) : mode === "poison" ? (
            <g stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round">
              <path d="M81 66l4.5 14 4.5-6.5 4.5 6.5L99 66" />
              <path d="M75 91c9-4 21-4 30 0" strokeOpacity=".7" />
              <path d="M84 39c4 5-4 9 0 14M96 39c-4 5 4 9 0 14" strokeOpacity=".55" strokeWidth="1.3" />
            </g>
          ) : (
            <g stroke="currentColor" strokeWidth="1.6" strokeLinecap="round">
              <path d="M85 67c0-3 2.3-5 5-5s5 2 5 5c0 3.2-5 4.2-5 8v2" />
              <rect x="88.8" y="81.5" width="2.4" height="2.4" rx="0.5" fill="currentColor" />
            </g>
          )}
        </>
      )}
    </svg>
  );
}

function toThumbnailItem(ing: SkyUltraAlchemyIngredient): Item {
  return {
    id: ing.itemId,
    name: ing.name,
    category: "ingredient",
    count: ing.count,
    value: ing.value,
    weight: ing.weight,
    equipped: false,
    enchanted: false,
  };
}

const PREVIEW_SNAPSHOT: SkyUltraAlchemySnapshot = {
  available: true,
  stationName: "Alchemy Lab",
  alchemySkill: 58,
  alchemyBaseSkill: 50,
  fortifyAlchemyPct: 16,
  skillMultiplier: 1.44,
  initMult: 4,
  skillFactor: 1.5,
  goldMult: 0.35,
  alchemistRank: 3,
  hasPhysician: true,
  hasBenefactor: true,
  hasPoisoner: false,
  hasPurity: true,
  experimenterEffects: 2,
  playerName: "Dragonborn",
  playerGold: 6750,
  carryWeight: 214,
  maxCarryWeight: 380,
  ingredients: [
    {
      id: "491021",
      itemId: "491021",
      name: "Blue Mountain Flower",
      count: 14,
      value: 2,
      weight: 0.1,
      knownCount: 3,
      effects: [
        { slot: 0, known: true, effectId: "256580", name: "Restore Health", descriptionTemplate: "Restore <mag> points of Health.", isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 0, baseCost: 1.5, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Restoration", isRestoreVital: true },
        { slot: 1, known: true, effectId: "256590", name: "Fortify Conjuration", descriptionTemplate: "Conjuration spells last <mag>% longer for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 60, baseCost: 0.75, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Conjuration", isRestoreVital: false },
        { slot: 2, known: true, effectId: "256578", name: "Fortify Health", descriptionTemplate: "Health is increased by <mag> points for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 60, baseCost: 0.35, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Restoration", isRestoreVital: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "308880",
      itemId: "308880",
      name: "Wheat",
      count: 11,
      value: 5,
      weight: 0.1,
      isFood: true,
      knownCount: 2,
      effects: [
        { slot: 0, known: true, effectId: "256580", name: "Restore Health", descriptionTemplate: "Restore <mag> points of Health.", isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 0, baseCost: 1.5, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Restoration", isRestoreVital: true },
        { slot: 1, known: true, effectId: "256578", name: "Fortify Health", descriptionTemplate: "Health is increased by <mag> points for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 60, baseCost: 0.35, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Restoration", isRestoreVital: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "294565",
      itemId: "294565",
      name: "Giant's Toe",
      count: 5,
      value: 20,
      weight: 1.0,
      knownCount: 3,
      effects: [
        { slot: 0, known: true, effectId: "256584", name: "Damage Stamina", descriptionTemplate: "Target loses <mag> points of Stamina.", isHostile: true, isDetrimental: true, baseMagnitude: 2, baseDuration: 0, baseCost: 0.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Destruction", isRestoreVital: false },
        { slot: 1, known: true, effectId: "256578", name: "Fortify Health", descriptionTemplate: "Health is increased by <mag> points for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 300, baseCost: 5.9, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Restoration", isRestoreVital: false },
        { slot: 2, known: true, effectId: "256598", name: "Fortify Carry Weight", descriptionTemplate: "Carrying capacity increases by <mag> for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 300, baseCost: 2.0, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Alteration", isRestoreVital: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "242342",
      itemId: "242342",
      name: "Creep Cluster",
      count: 9,
      value: 1,
      weight: 0.2,
      knownCount: 2,
      effects: [
        { slot: 0, known: true, effectId: "256581", name: "Restore Magicka", descriptionTemplate: "Restore <mag> points of Magicka.", isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 0, baseCost: 1.6, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Restoration", isRestoreVital: true },
        { slot: 1, known: true, effectId: "256598", name: "Fortify Carry Weight", descriptionTemplate: "Carrying capacity increases by <mag> for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 300, baseCost: 2.0, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Alteration", isRestoreVital: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "242346",
      itemId: "242346",
      name: "Mora Tapinella",
      count: 16,
      value: 4,
      weight: 0.3,
      knownCount: 2,
      effects: [
        { slot: 0, known: true, effectId: "256581", name: "Restore Magicka", descriptionTemplate: "Restore <mag> points of Magicka.", isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 0, baseCost: 1.6, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Restoration", isRestoreVital: true },
        { slot: 1, known: true, effectId: "256585", name: "Lingering Damage Health", descriptionTemplate: "Causes <mag> points of poison damage for <dur> seconds.", isHostile: true, isDetrimental: true, baseMagnitude: 1, baseDuration: 10, baseCost: 1.9, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Destruction", isRestoreVital: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "256235",
      itemId: "256235",
      name: "Deathbell",
      count: 8,
      value: 4,
      weight: 0.1,
      knownCount: 3,
      effects: [
        { slot: 0, known: true, effectId: "256583", name: "Damage Health", descriptionTemplate: "Causes <mag> points of poison damage.", isHostile: true, isDetrimental: true, baseMagnitude: 3, baseDuration: 0, baseCost: 1.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Destruction", isRestoreVital: false },
        { slot: 1, known: true, effectId: "256601", name: "Slow", descriptionTemplate: "Target moves at 50% speed for <dur> seconds.", isHostile: true, isDetrimental: true, baseMagnitude: 50, baseDuration: 5, baseCost: 1.4, powerAffectsMagnitude: false, powerAffectsDuration: true, noMagnitude: true, noDuration: false, school: "Alteration", isRestoreVital: false },
        { slot: 2, known: true, effectId: "256584", name: "Damage Stamina", descriptionTemplate: "Target loses <mag> points of Stamina.", isHostile: true, isDetrimental: true, baseMagnitude: 2, baseDuration: 0, baseCost: 0.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Destruction", isRestoreVital: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "242343",
      itemId: "242343",
      name: "Salt Pile",
      count: 19,
      value: 2,
      weight: 0.2,
      knownCount: 2,
      effects: [
        { slot: 0, known: true, effectId: "256601", name: "Slow", descriptionTemplate: "Target moves at 50% speed for <dur> seconds.", isHostile: true, isDetrimental: true, baseMagnitude: 50, baseDuration: 5, baseCost: 1.4, powerAffectsMagnitude: false, powerAffectsDuration: true, noMagnitude: true, noDuration: false, school: "Alteration", isRestoreVital: false },
        { slot: 1, known: true, effectId: "256594", name: "Regenerate Magicka", descriptionTemplate: "Magicka regenerates <mag>% faster for <dur> seconds.", isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 300, baseCost: 2.1, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Restoration", isRestoreVital: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "256237",
      itemId: "256237",
      name: "Imp Stool",
      count: 7,
      value: 0,
      weight: 0.3,
      knownCount: 3,
      effects: [
        { slot: 0, known: true, effectId: "256583", name: "Damage Health", descriptionTemplate: "Causes <mag> points of poison damage.", isHostile: true, isDetrimental: true, baseMagnitude: 2, baseDuration: 0, baseCost: 1.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Destruction", isRestoreVital: false },
        { slot: 1, known: true, effectId: "256585", name: "Lingering Damage Health", descriptionTemplate: "Causes <mag> points of poison damage for <dur> seconds.", isHostile: true, isDetrimental: true, baseMagnitude: 1, baseDuration: 10, baseCost: 1.9, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: "Destruction", isRestoreVital: false },
        { slot: 2, known: true, effectId: "256602", name: "Paralysis", descriptionTemplate: "Target is paralyzed for <dur> seconds.", isHostile: true, isDetrimental: true, baseMagnitude: 1, baseDuration: 1, baseCost: 12.0, powerAffectsMagnitude: false, powerAffectsDuration: true, noMagnitude: true, noDuration: false, school: "Alteration", isRestoreVital: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "256233",
      itemId: "256233",
      name: "Canis Root",
      count: 6,
      value: 5,
      weight: 0.1,
      knownCount: 2,
      effects: [
        { slot: 0, known: true, effectId: "256584", name: "Damage Stamina", descriptionTemplate: "Target loses <mag> points of Stamina.", isHostile: true, isDetrimental: true, baseMagnitude: 2, baseDuration: 0, baseCost: 0.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Destruction", isRestoreVital: false },
        { slot: 1, known: true, effectId: "256602", name: "Paralysis", descriptionTemplate: "Target is paralyzed for <dur> seconds.", isHostile: true, isDetrimental: true, baseMagnitude: 1, baseDuration: 1, baseCost: 12.0, powerAffectsMagnitude: false, powerAffectsDuration: true, noMagnitude: true, noDuration: false, school: "Alteration", isRestoreVital: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: "491025",
      itemId: "491025",
      name: "Crimson Nirnroot",
      count: 4,
      value: 10,
      weight: 0.2,
      isQuest: true,
      knownCount: 1,
      effects: [
        { slot: 0, known: true, effectId: "256583", name: "Damage Health", descriptionTemplate: "Causes <mag> points of poison damage.", isHostile: true, isDetrimental: true, baseMagnitude: 6, baseDuration: 0, baseCost: 3.0, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: "Destruction", isRestoreVital: false },
        { slot: 1, known: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
  ],
  knownEffects: [
    { effectId: "256580", name: "Restore Health", isHostile: false, isDetrimental: false, school: "Restoration", ingredientCount: 2 },
    { effectId: "256578", name: "Fortify Health", isHostile: false, isDetrimental: false, school: "Restoration", ingredientCount: 3 },
    { effectId: "256581", name: "Restore Magicka", isHostile: false, isDetrimental: false, school: "Restoration", ingredientCount: 2 },
    { effectId: "256598", name: "Fortify Carry Weight", isHostile: false, isDetrimental: false, school: "Alteration", ingredientCount: 2 },
    { effectId: "256583", name: "Damage Health", isHostile: true, isDetrimental: true, school: "Destruction", ingredientCount: 3 },
    { effectId: "256585", name: "Lingering Damage Health", isHostile: true, isDetrimental: true, school: "Destruction", ingredientCount: 2 },
    { effectId: "256584", name: "Damage Stamina", isHostile: true, isDetrimental: true, school: "Destruction", ingredientCount: 3 },
    { effectId: "256601", name: "Slow", isHostile: true, isDetrimental: true, school: "Alteration", ingredientCount: 2 },
    { effectId: "256602", name: "Paralysis", isHostile: true, isDetrimental: true, school: "Alteration", ingredientCount: 2 },
  ],
  message: "Studio preview data",
};

export default function AlchemyPage({
  player,
  bridge,
  obscured = false,
  displayMode = "half",
  pauseWorld = true,
  onChangeDisplayMode,
  onTogglePauseWorld,
  onClose,
}: AlchemyPageProps) {
  const gameApi = useMemo(() => (bridge ? createSkyUltraGame(bridge) : null), [bridge]);

  const [snapshot, setSnapshot] = useState<SkyUltraAlchemySnapshot>(() =>
    bridge
      ? {
          ...PREVIEW_SNAPSHOT,
          available: false,
          ingredients: [],
          knownEffects: [],
        }
      : PREVIEW_SNAPSHOT,
  );
  const [loading, setLoading] = useState<boolean>(Boolean(bridge));
  const [busy, setBusy] = useState<boolean>(false);

  const [activeTab, setActiveTab] = useState<AlchemyTab>("ingredients");
  const [selectedIds, setSelectedIds] = useState<string[]>([]);
  const [craftCount, setCraftCount] = useState<number>(1);
  const [customName, setCustomName] = useState<string>("");
  const [selectedEffectId, setSelectedEffectId] = useState<string>("");

  const [search, setSearch] = useState<string>("");
  const [ingredientFilter, setIngredientFilter] = useState<IngredientFilter>("all");
  const [effectFilter, setEffectFilter] = useState<EffectPolarityFilter>("all");
  const [recipeFilter, setRecipeFilter] = useState<EffectPolarityFilter>("all");
  const [sortBy, setSortBy] = useState<AlchemySortBy>("name");
  const [viewLayout, setViewLayout] = useState<"list" | "grid">("grid");
  const [pageIndex, setPageIndex] = useState<number>(0);

  const [statusMessage, setStatusMessage] = useState<{ kind: "info" | "success" | "error"; text: string } | null>(null);
  const searchInputRef = useRef<HTMLInputElement | null>(null);
  const selectedIdsRef = useRef<string[]>(selectedIds);
  selectedIdsRef.current = selectedIds;

  const syncSelectionWithSnapshot = useCallback((next: SkyUltraAlchemySnapshot) => {
    setSnapshot(next);
    if (!next.available) return;
    const ingMap = new Map(next.ingredients.filter((i) => i.count > 0).map((i) => [i.id, i]));
    const kept = selectedIdsRef.current.filter((id) => ingMap.has(id)).slice(0, 3);
    setSelectedIds(kept);
    setSelectedEffectId((prev) => (prev && next.knownEffects.some((e) => e.effectId === prev) ? prev : ""));
  }, []);

  const loadSnapshot = useCallback(async () => {
    if (!gameApi) {
      setLoading(false);
      return;
    }
    setLoading(true);
    try {
      const next = await gameApi.alchemySnapshot();
      syncSelectionWithSnapshot(next);
      if (!next.available && next.message) {
        setStatusMessage({ kind: "info", text: next.message });
      }
    } catch (err) {
      setStatusMessage({
        kind: "error",
        text: err instanceof Error ? err.message : "Failed to read Alchemy Lab state.",
      });
    } finally {
      setLoading(false);
    }
  }, [gameApi, syncSelectionWithSnapshot]);

  useEffect(() => {
    if (!obscured) {
      void loadSnapshot();
    }
  }, [obscured, loadSnapshot]);

  useEffect(() => {
    if (!bridge || obscured) return;
    const offSession = bridge.on("skyultra.menuSession", (payload) => {
      if (payload && typeof payload === "object" && (payload as { alchemyMode?: boolean }).alchemyMode) {
        void loadSnapshot();
      }
    });
    const offAlchemyMode = bridge.on("skyultra.alchemyMode", (payload) => {
      if (payload && typeof payload === "object" && (payload as { active?: boolean }).active) {
        void loadSnapshot();
      }
    });
    return () => {
      offSession();
      offAlchemyMode();
    };
  }, [bridge, obscured, loadSnapshot]);

  const powerContext = useMemo<AlchemyPowerContext>(() => {
    const skillFromPlayer = (player?.skills ?? []).find(
      (s) => s.id.toLowerCase() === "alchemy" || s.label.toLowerCase() === "alchemy",
    )?.value;
    const skillNum =
      typeof snapshot.alchemySkill === "number"
        ? snapshot.alchemySkill
        : typeof skillFromPlayer === "number"
          ? skillFromPlayer
          : 15;
    return {
      alchemySkill: skillNum,
      fortifyAlchemyPct: snapshot.fortifyAlchemyPct ?? 0,
      initMult: snapshot.initMult ?? 4,
      skillFactor: snapshot.skillFactor ?? 1.5,
      goldMult: snapshot.goldMult ?? 0.35,
      alchemistRank: snapshot.alchemistRank ?? 0,
      hasPhysician: Boolean(snapshot.hasPhysician),
      hasBenefactor: Boolean(snapshot.hasBenefactor),
      hasPoisoner: Boolean(snapshot.hasPoisoner),
      hasPurity: Boolean(snapshot.hasPurity),
    };
  }, [player, snapshot]);

  const selectedIngredients = useMemo(() => {
    const map = new Map(snapshot.ingredients.map((i) => [i.id, i]));
    return selectedIds.map((id) => map.get(id)).filter((x): x is SkyUltraAlchemyIngredient => Boolean(x));
  }, [snapshot.ingredients, selectedIds]);

  const preview = useMemo(
    () => computeAlchemyPreview(snapshot.ingredients, selectedIds, powerContext),
    [snapshot.ingredients, selectedIds, powerContext],
  );

  const selectedKnownEffectSet = useMemo(() => {
    const set = new Set<string>();
    for (const ing of selectedIngredients) {
      for (const eff of ing.effects) {
        if (eff.known && eff.effectId) set.add(eff.effectId);
      }
    }
    return set;
  }, [selectedIngredients]);

  const compatibleIdSet = useMemo(() => {
    const set = new Set<string>();
    if (selectedIngredients.length === 0 || selectedKnownEffectSet.size === 0) return set;
    for (const ing of snapshot.ingredients) {
      if (selectedIds.includes(ing.id)) continue;
      if (ing.effects.some((e) => e.known && e.effectId && selectedKnownEffectSet.has(e.effectId))) {
        set.add(ing.id);
      }
    }
    return set;
  }, [snapshot.ingredients, selectedIngredients.length, selectedIds, selectedKnownEffectSet]);

  const brewableRecipes = useMemo(
    () => computeBrewableRecipes(snapshot.ingredients, powerContext),
    [snapshot.ingredients, powerContext],
  );

  const activeEffectFilterObj: SkyUltraAlchemyKnownEffect | null = useMemo(
    () => (selectedEffectId ? snapshot.knownEffects.find((e) => e.effectId === selectedEffectId) ?? null : null),
    [selectedEffectId, snapshot.knownEffects],
  );

  const effectiveMaxCraft = useMemo(() => {
    if (selectedIngredients.length < 2) return 0;
    return Math.max(1, preview.valid ? preview.maxCraftable : 1);
  }, [selectedIngredients.length, preview.valid, preview.maxCraftable]);

  const effectiveCraftCount = useMemo(
    () => Math.max(1, Math.min(effectiveMaxCraft || 1, craftCount)),
    [craftCount, effectiveMaxCraft],
  );

  useEffect(() => {
    setCraftCount((prev) => {
      if (effectiveMaxCraft <= 1) return 1;
      return Math.max(1, Math.min(effectiveMaxCraft, prev));
    });
  }, [effectiveMaxCraft]);

  const itemsPerPage = viewLayout === "grid" ? GRID_ITEMS_PER_PAGE : LIST_ITEMS_PER_PAGE;

  const filteredIngredients = useMemo(() => {
    const q = search.trim().toLowerCase();
    const list = snapshot.ingredients.filter((ing) => {
      if (ing.count <= 0) return false;
      if (selectedEffectId) {
        const hasEff = ing.effects.some((e) => e.known && e.effectId === selectedEffectId);
        if (!hasEff) return false;
      }
      if (q) {
        const matchName = ing.name.toLowerCase().includes(q);
        const matchEff = ing.effects.some((e) => e.known && (e.name ?? "").toLowerCase().includes(q));
        if (!matchName && !matchEff) return false;
      }
      if (ingredientFilter === "compatible") {
        if (selectedIds.length > 0 && !selectedIds.includes(ing.id) && !compatibleIdSet.has(ing.id)) return false;
      } else if (ingredientFilter === "unknown") {
        if (!ing.effects.some((e) => !e.known)) return false;
      } else if (ingredientFilter === "potion") {
        if (!ing.effects.some((e) => e.known && !(e.isHostile || e.isDetrimental))) return false;
      } else if (ingredientFilter === "poison") {
        if (!ing.effects.some((e) => e.known && (e.isHostile || e.isDetrimental))) return false;
      }
      return true;
    });

    list.sort((a, b) => {
      if (sortBy === "count") {
        if (b.count !== a.count) return b.count - a.count;
      } else if (sortBy === "value") {
        if (b.value !== a.value) return b.value - a.value;
      } else if (sortBy === "known") {
        if (b.knownCount !== a.knownCount) return b.knownCount - a.knownCount;
      }
      return a.name.localeCompare(b.name);
    });
    return list;
  }, [snapshot.ingredients, selectedEffectId, search, ingredientFilter, selectedIds, compatibleIdSet, sortBy]);

  const filteredKnownEffects = useMemo(() => {
    const q = search.trim().toLowerCase();
    return snapshot.knownEffects.filter((eff) => {
      const poisonEff = Boolean(eff.isHostile || eff.isDetrimental);
      if (effectFilter === "potion" && poisonEff) return false;
      if (effectFilter === "poison" && !poisonEff) return false;
      if (q && !eff.name.toLowerCase().includes(q) && !eff.school.toLowerCase().includes(q)) return false;
      return true;
    });
  }, [snapshot.knownEffects, search, effectFilter]);

  const filteredRecipes = useMemo(() => {
    const q = search.trim().toLowerCase();
    return brewableRecipes.filter((rec) => {
      if (recipeFilter === "potion" && rec.isPoison) return false;
      if (recipeFilter === "poison" && !rec.isPoison) return false;
      if (q) {
        const hitTitle = rec.title.toLowerCase().includes(q);
        const hitIng = rec.ingredientNames.some((n) => n.toLowerCase().includes(q));
        const hitEff = rec.effects.some((e) => e.name.toLowerCase().includes(q));
        if (!hitTitle && !hitIng && !hitEff) return false;
      }
      return true;
    });
  }, [brewableRecipes, recipeFilter, search]);

  const activeListCount =
    activeTab === "ingredients"
      ? filteredIngredients.length
      : activeTab === "effects"
        ? filteredKnownEffects.length
        : filteredRecipes.length;
  const totalPages = Math.max(1, Math.ceil(activeListCount / itemsPerPage));
  const clampedPage = Math.min(pageIndex, totalPages - 1);
  const pageStart = clampedPage * itemsPerPage;
  const pageEnd = pageStart + itemsPerPage;

  useEffect(() => {
    setPageIndex(0);
  }, [activeTab, search, ingredientFilter, effectFilter, recipeFilter, sortBy, viewLayout, selectedEffectId]);

  const toggleIngredientSelection = useCallback((ing: SkyUltraAlchemyIngredient) => {
    setSelectedIds((prev) => {
      if (prev.includes(ing.id)) {
        return prev.filter((id) => id !== ing.id);
      }
      if (prev.length >= 3) {
        return [prev[1]!, prev[2]!, ing.id];
      }
      return [...prev, ing.id];
    });
  }, []);

  const loadRecipeInAltar = useCallback((recipe: AlchemyBrewableRecipe) => {
    setSelectedIds(recipe.ingredientIds.slice(0, 3));
    setCraftCount(1);
    setStatusMessage({
      kind: "info",
      text: `Loaded ${recipe.ingredientNames.join(" + ")} into the Crucible.`,
    });
  }, []);

  const canTasteIngredient = useCallback(
    (ing: SkyUltraAlchemyIngredient): boolean => {
      if (ing.count <= 0) return false;
      const limit = Math.max(1, Math.min(4, snapshot.experimenterEffects ?? 1));
      return ing.effects.slice(0, limit).some((eff) => !eff.known);
    },
    [snapshot.experimenterEffects],
  );

  const handleEatIngredient = useCallback(
    async (ing: SkyUltraAlchemyIngredient, ev: React.MouseEvent) => {
      ev.stopPropagation();
      if (busy) return;
      if (!canTasteIngredient(ing)) {
        setStatusMessage({
          kind: "info",
          text: `Already sampled the first ${Math.max(1, snapshot.experimenterEffects ?? 1)} trait(s) of ${ing.name} — brew to discover deeper effects.`,
        });
        return;
      }
      if (!gameApi) {
        const limit = Math.max(1, Math.min(4, snapshot.experimenterEffects ?? 1));
        setSnapshot((prev) => ({
          ...prev,
          ingredients: prev.ingredients
            .map((item) => {
              if (item.id !== ing.id) return item;
              let learned = 0;
              const updatedEffects = item.effects.map((eff, idx) => {
                if (idx < limit && !eff.known) {
                  learned += 1;
                  return {
                    ...eff,
                    known: true,
                    effectId: eff.effectId || `preview-${item.id}-${idx}`,
                    name: eff.name || "Discovered Trait",
                  };
                }
                return eff;
              });
              return {
                ...item,
                count: Math.max(0, item.count - 1),
                knownCount: item.knownCount + learned,
                effects: updatedEffects,
              };
            })
            .filter((i) => i.count > 0),
        }));
        setStatusMessage({ kind: "success", text: `Sampled ${ing.name} and recorded alchemical trait(s).` });
        return;
      }

      setBusy(true);
      try {
        const res = await gameApi.eatIngredient({ ingredientId: ing.id });
        if (res.accepted) {
          setStatusMessage({ kind: "success", text: res.message || `Sampled ${ing.name}.` });
          if (res.snapshot) {
            syncSelectionWithSnapshot(res.snapshot);
          } else {
            await loadSnapshot();
          }
        } else {
          setStatusMessage({ kind: "error", text: res.message || `Could not sample ${ing.name}.` });
        }
      } catch (err) {
        setStatusMessage({
          kind: "error",
          text: err instanceof Error ? err.message : "Failed to sample ingredient.",
        });
      } finally {
        setBusy(false);
      }
    },
    [busy, canTasteIngredient, snapshot.experimenterEffects, gameApi, syncSelectionWithSnapshot, loadSnapshot],
  );

  const handleBrew = useCallback(async () => {
    if (busy || selectedIngredients.length < 2) return;
    if (!gameApi) {
      if (!preview.valid) {
        setSnapshot((prev) => ({
          ...prev,
          ingredients: prev.ingredients
            .map((item) => (selectedIds.includes(item.id) ? { ...item, count: Math.max(0, item.count - 1) } : item))
            .filter((i) => i.count > 0),
        }));
        setStatusMessage({
          kind: "info",
          text: "Potion creation failed — the selected ingredients share no matching effects.",
        });
        return;
      }
      const craftedTitle = customName.trim() || preview.defaultName;
      setSnapshot((prev) => ({
        ...prev,
        ingredients: prev.ingredients
          .map((item) =>
            selectedIds.includes(item.id) ? { ...item, count: Math.max(0, item.count - effectiveCraftCount) } : item,
          )
          .filter((i) => i.count > 0),
      }));
      setStatusMessage({
        kind: "success",
        text: `Brewed ${effectiveCraftCount > 1 ? `${effectiveCraftCount}x ` : ""}${craftedTitle}.`,
      });
      return;
    }

    setBusy(true);
    try {
      const res = await gameApi.alchemyCraft({
        ingredientIds: selectedIds,
        count: effectiveCraftCount,
        customName: customName.trim() || undefined,
      });
      if (res.accepted) {
        setStatusMessage({
          kind: res.failedCombination ? "info" : "success",
          text:
            res.message ||
            (res.failedCombination
              ? "The mix yielded no stable effect."
              : `Brewed ${res.craftedCount || 1}x ${res.craftedName || "Potion"}.`),
        });
        if (!res.failedCombination) {
          setCustomName("");
        }
        if (res.snapshot) {
          syncSelectionWithSnapshot(res.snapshot);
        } else {
          await loadSnapshot();
        }
      } else {
        setStatusMessage({ kind: "error", text: res.message || "Alchemy brewing failed." });
      }
    } catch (err) {
      setStatusMessage({
        kind: "error",
        text: err instanceof Error ? err.message : "Alchemy craft request failed.",
      });
    } finally {
      setBusy(false);
    }
  }, [
    busy,
    selectedIngredients.length,
    gameApi,
    preview,
    selectedIds,
    customName,
    effectiveCraftCount,
    syncSelectionWithSnapshot,
    loadSnapshot,
  ]);

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
        onClose?.();
        return;
      }
      if (inInput) return;
      if (ev.key === " ") {
        ev.preventDefault();
        searchInputRef.current?.focus();
      } else if (ev.key === "Enter" && selectedIngredients.length >= 2) {
        ev.preventDefault();
        void handleBrew();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [obscured, onClose, selectedIngredients.length, handleBrew]);

  const alchemySkill = Math.round(powerContext.alchemySkill ?? 15);
  const fortifyPct = Math.round(powerContext.fortifyAlchemyPct ?? 0);
  const playerGold = player?.gold ?? snapshot.playerGold ?? 0;
  const carryWeight = player?.carryWeight?.current ?? snapshot.carryWeight ?? 0;
  const maxCarry = player?.carryWeight?.max ?? snapshot.maxCarryWeight ?? 300;
  const vesselsFilled = selectedIngredients.length;
  const phialMode: "empty" | "experimental" | "potion" | "poison" =
    vesselsFilled < 2 ? "empty" : !preview.valid ? "experimental" : preview.isPoison ? "poison" : "potion";

  return (
    <div className="alchemy-page" data-testid="alchemy-page">
      <header className="alchemy-header">
        <div className="alchemy-header__identity">
          <div className="alchemy-header__sigil">
            <AlchemyCrestEmblem />
          </div>
          <div>
            <div className="alchemy-header__eyebrow">
              ALCHEMICAL SYNTHESIS ·{" "}
              {activeTab === "ingredients"
                ? "DISTILLATION"
                : activeTab === "effects"
                  ? "EFFECT MATRIX"
                  : "BREWBOOK"}
            </div>
            <h1 className="alchemy-header__title">{snapshot.stationName || "Alchemy Lab"}</h1>
          </div>
        </div>

        <div className="alchemy-header__stats">
          <div className="alchemy-stat-pill">
            <Icon name="flask" size={14} />
            <span>ALCHEMY</span>
            <strong>{alchemySkill}</strong>
            {fortifyPct > 0 ? <em className="alchemy-stat-pill__boost">+{fortifyPct}%</em> : null}
          </div>
          {snapshot.hasPurity ? (
            <div className="alchemy-stat-pill alchemy-stat-pill--accent" title="Purity perk cleanses negative side effects from potions and beneficial effects from poisons.">
              <Icon name="check" size={13} />
              <span>PURITY</span>
            </div>
          ) : (snapshot.alchemistRank ?? 0) > 0 ? (
            <div className="alchemy-stat-pill alchemy-stat-pill--accent">
              <Icon name="flask" size={13} />
              <span>ALCHEMIST</span>
              <strong>R{snapshot.alchemistRank}</strong>
            </div>
          ) : null}
          <div className="alchemy-stat-pill">
            <Icon name="gold" size={14} />
            <span>{playerGold.toLocaleString()}</span>
            <small>GOLD</small>
          </div>
          <div className="alchemy-stat-pill">
            <Icon name="weight" size={14} />
            <span>{Math.round(carryWeight)}</span>
            <small>/{Math.round(maxCarry)} WT</small>
          </div>
        </div>

        <div className="alchemy-header__controls">
          {onChangeDisplayMode ? (
            <button
              type="button"
              className={`alchemy-mode-btn${displayMode !== "half" ? " is-active" : ""}`}
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
              className={`alchemy-btn alchemy-btn--chip${pauseWorld ? " is-active" : ""}`}
              onClick={onTogglePauseWorld}
              title="Toggle whether world simulation pauses at the Alchemy Lab"
            >
              <Icon name={pauseWorld ? "pause" : "play"} size={14} />
              <span>{pauseWorld ? "Pause World" : "Live World"}</span>
            </button>
          ) : null}
          <button
            type="button"
            className="alchemy-btn alchemy-btn--icon"
            onClick={() => void loadSnapshot()}
            title="Refresh Alchemy Lab pouch"
          >
            <Icon name="refresh" size={15} />
          </button>
          {onClose ? (
            <button type="button" className="alchemy-btn alchemy-btn--close" onClick={onClose}>
              <Icon name="close" size={14} />
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          ) : null}
        </div>
      </header>

      <div className="alchemy-body">
        <section className="alchemy-browser">
          <div className="alchemy-mode-bar">
            <SkySegmentedControl
              label="Alchemy Lab workspace"
              value={activeTab}
              onValueChange={(v) => setActiveTab(v)}
              options={[
                { value: "ingredients", label: "Ingredients", count: snapshot.ingredients.length },
                { value: "effects", label: "By Effect", count: snapshot.knownEffects.length },
                { value: "recipes", label: "Known Recipes", count: brewableRecipes.length },
              ]}
            />
          </div>

          {activeEffectFilterObj && activeTab === "ingredients" ? (
            <div className="alchemy-effect-pin">
              <div className="alchemy-effect-pin__info">
                <AlchemyEffectSigil
                  name={activeEffectFilterObj.name}
                  isPoison={Boolean(activeEffectFilterObj.isHostile || activeEffectFilterObj.isDetrimental)}
                  size="sm"
                />
                <span>
                  Showing reagents with <strong>{activeEffectFilterObj.name}</strong>
                </span>
              </div>
              <button
                type="button"
                className="alchemy-link"
                onClick={() => setSelectedEffectId("")}
              >
                Clear Effect Filter
              </button>
            </div>
          ) : null}

          <div className="alchemy-toolbar">
            <label className="alchemy-search">
              <Icon name="search" size={14} />
              <input
                ref={searchInputRef}
                type="text"
                value={search}
                onChange={(e) => setSearch(e.target.value)}
                placeholder={
                  activeTab === "ingredients"
                    ? "Search ingredients or known effects..."
                    : activeTab === "effects"
                      ? "Search known alchemical effects..."
                      : "Search discoverable recipes or reagents..."
                }
              />
              {search ? (
                <button
                  type="button"
                  className="alchemy-search__clear"
                  onClick={() => setSearch("")}
                  aria-label="Clear search"
                >
                  <Icon name="close" size={12} />
                </button>
              ) : (
                <kbd>Space</kbd>
              )}
            </label>

            <div className="alchemy-filters">
              {activeTab === "ingredients" ? (
                <SkyChoice
                  label="Filter ingredients"
                  value={ingredientFilter}
                  onValueChange={(v) => setIngredientFilter(v)}
                  options={[
                    { value: "all", label: "All Reagents" },
                    { value: "compatible", label: selectedIds.length > 0 ? `Compatible (${compatibleIdSet.size})` : "Compatible" },
                    { value: "unknown", label: "Unknown (?)" },
                    { value: "potion", label: "Potions" },
                    { value: "poison", label: "Poisons" },
                  ]}
                />
              ) : activeTab === "effects" ? (
                <SkyChoice
                  label="Filter effects"
                  value={effectFilter}
                  onValueChange={(v) => setEffectFilter(v)}
                  options={[
                    { value: "all", label: "All Effects" },
                    { value: "potion", label: "Beneficial" },
                    { value: "poison", label: "Toxic / Poison" },
                  ]}
                />
              ) : (
                <SkyChoice
                  label="Filter recipes"
                  value={recipeFilter}
                  onValueChange={(v) => setRecipeFilter(v)}
                  options={[
                    { value: "all", label: "All Recipes" },
                    { value: "potion", label: "Potions Only" },
                    { value: "poison", label: "Poisons Only" },
                  ]}
                />
              )}

              {activeTab === "ingredients" ? (
                <SkyChoice
                  label="Sort reagents"
                  value={sortBy}
                  onValueChange={(v) => setSortBy(v)}
                  options={[
                    { value: "name", label: "Sort: Name" },
                    { value: "count", label: "Sort: Quantity" },
                    { value: "value", label: "Sort: Value" },
                    { value: "known", label: "Sort: Known Traits" },
                  ]}
                />
              ) : null}

              <SkySegmentedControl
                label="View layout"
                value={viewLayout}
                onValueChange={setViewLayout}
                options={[
                  { value: "list", label: "List", icon: <Icon name="list" size={13} /> },
                  { value: "grid", label: "Grid", icon: <Icon name="grid" size={13} /> },
                ]}
              />
            </div>
          </div>

          <div
            className={`alchemy-list${
              viewLayout === "grid"
                ? activeTab === "ingredients"
                  ? " alchemy-list--grid alchemy-list--grid-4"
                  : " alchemy-list--grid"
                : ""
            }`}
          >
            {loading ? (
              <Empty title="Preparing Alembic..." detail="Reading pouch ingredients and discovered traits." icon="flask" />
            ) : activeTab === "ingredients" ? (
              filteredIngredients.length === 0 ? (
                <Empty
                  title="No matching ingredients"
                  detail="Adjust your search or filter to view carried alchemical reagents."
                  icon="ingredient"
                />
              ) : (
                filteredIngredients.slice(pageStart, pageEnd).map((ing) => {
                  const selectedIndex = selectedIds.indexOf(ing.id);
                  const selected = selectedIndex >= 0;
                  const compatible = !selected && compatibleIdSet.has(ing.id);
                  const tasteAvailable = canTasteIngredient(ing);
                  const Roman = selectedIndex === 0 ? "I" : selectedIndex === 1 ? "II" : "III";
                  const tooltipTraits = ing.effects
                    .slice(0, 4)
                    .map((eff) => (eff.known && eff.name ? eff.name : "? Unknown"))
                    .join(" · ");

                  if (viewLayout === "grid") {
                    return (
                      <div
                        key={ing.id}
                        role="button"
                        tabIndex={0}
                        className={`alchemy-grid-card${selected ? " is-selected" : ""}${compatible ? " is-compatible" : ""}`}
                        onClick={() => toggleIngredientSelection(ing)}
                        onKeyDown={(ev) => {
                          if (ev.key === "Enter" || ev.key === " ") {
                            ev.preventDefault();
                            toggleIngredientSelection(ing);
                          }
                        }}
                        title={`${ing.name} · ${ing.knownCount}/4 known traits — ${tooltipTraits}`}
                      >
                        <div className="alchemy-grid-card__top">
                          <small className={selected ? "is-ready" : compatible ? "is-compat" : ing.isStolen ? "is-warn" : ""}>
                            {selected ? (
                              <>
                                <Icon name="check" size={10} /> Vessel {Roman}
                              </>
                            ) : compatible ? (
                              "Match"
                            ) : ing.isStolen ? (
                              "Stolen"
                            ) : (
                              "Ingredient"
                            )}
                          </small>
                          <span>
                            {ing.value.toLocaleString()}
                            <Icon name="gold" size={10} />
                          </span>
                        </div>
                        <div className="alchemy-grid-card__art">
                          <ItemThumbnail item={toThumbnailItem(ing)} bridge={bridge ?? undefined} paused={obscured} />
                          <span
                            className={`alchemy-art-known${ing.knownCount >= 4 ? " is-mastered" : ing.knownCount === 0 ? " is-untested" : ""}`}
                            title={`${ing.knownCount} of 4 traits known`}
                          >
                            <span className="alchemy-art-pips" aria-hidden="true">
                              {ing.effects.slice(0, 4).map((eff, pipIdx) => (
                                <span
                                  key={`${ing.id}-p-${pipIdx}`}
                                  className={`alchemy-art-pip${
                                    eff.known
                                      ? (eff.isHostile || eff.isDetrimental ? " is-poison" : " is-potion") +
                                        (eff.effectId && selectedKnownEffectSet.has(eff.effectId) ? " is-matched" : "")
                                      : " is-unknown"
                                  }`}
                                />
                              ))}
                            </span>
                            <span>{ing.knownCount}/4</span>
                          </span>
                          <span className="alchemy-art-count">×{ing.count.toLocaleString()}</span>
                        </div>
                        <div className="alchemy-grid-card__title">{ing.name}</div>
                        {ing.knownCount === 0 ? (
                          <div className="alchemy-traits-untested">
                            <span className="alchemy-art-pips" aria-hidden="true">
                              {ing.effects.slice(0, 4).map((_, pipIdx) => (
                                <span key={`${ing.id}-u-${pipIdx}`} className="alchemy-art-pip is-unknown" />
                              ))}
                            </span>
                            <span>Untested · 0/4</span>
                          </div>
                        ) : (
                          <div className="alchemy-trait-grid">
                            {ing.effects.slice(0, 4).map((eff, slotIdx) =>
                              eff.known ? (
                                <span
                                  key={`${ing.id}-${slotIdx}`}
                                  title={eff.name || "Effect"}
                                  className={`alchemy-trait-pill${
                                    eff.isHostile || eff.isDetrimental ? " is-poison" : " is-potion"
                                  }${eff.effectId && selectedKnownEffectSet.has(eff.effectId) ? " is-matched" : ""}`}
                                >
                                  <AlchemyEffectSigil
                                    name={eff.name || "Effect"}
                                    isPoison={Boolean(eff.isHostile || eff.isDetrimental)}
                                    size="sm"
                                  />
                                  <span>{eff.name}</span>
                                </span>
                              ) : (
                                <span key={`${ing.id}-${slotIdx}`} className="alchemy-trait-pill is-unknown" title="Unlearned effect">
                                  <em>?</em>
                                  <span>Unknown</span>
                                </span>
                              ),
                            )}
                          </div>
                        )}
                        <div className="alchemy-grid-card__foot">
                          <span>{ing.weight.toFixed(1)} wt</span>
                          {tasteAvailable ? (
                            <button
                              type="button"
                              className="alchemy-taste-btn"
                              onClick={(ev) => void handleEatIngredient(ing, ev)}
                              disabled={busy}
                              title={`Sample 1× ${ing.name} to learn up to ${snapshot.experimenterEffects ?? 1} effect(s)`}
                            >
                              Taste
                            </button>
                          ) : ing.knownCount >= 4 ? (
                            <span className="alchemy-mastered-tag">
                              <Icon name="check" size={10} /> Mastered
                            </span>
                          ) : null}
                        </div>
                      </div>
                    );
                  }

                  return (
                    <div
                      key={ing.id}
                      role="button"
                      tabIndex={0}
                      className={`alchemy-row${selected ? " is-selected" : ""}${compatible ? " is-compatible" : ""}`}
                      onClick={() => toggleIngredientSelection(ing)}
                      onKeyDown={(ev) => {
                        if (ev.key === "Enter" || ev.key === " ") {
                          ev.preventDefault();
                          toggleIngredientSelection(ing);
                        }
                      }}
                    >
                      <div className="alchemy-row__thumb">
                        <ItemThumbnail item={toThumbnailItem(ing)} bridge={bridge ?? undefined} paused={obscured} />
                      </div>
                      <div className="alchemy-row__main">
                        <div className="alchemy-row__title">
                          <span>{ing.name}</span>
                          <span className="alchemy-count">×{ing.count}</span>
                          {ing.isStolen ? <span className="alchemy-pill alchemy-pill--warn">Stolen</span> : null}
                          {compatible ? <span className="alchemy-pill alchemy-pill--compat">Match</span> : null}
                        </div>
                        <div className="alchemy-row__traits">
                          {ing.effects.slice(0, 4).map((eff, slotIdx) =>
                            eff.known ? (
                              <span
                                key={`${ing.id}-r-${slotIdx}`}
                                className={`alchemy-trait-chip${
                                  eff.isHostile || eff.isDetrimental ? " is-poison" : " is-potion"
                                }${eff.effectId && selectedKnownEffectSet.has(eff.effectId) ? " is-matched" : ""}`}
                              >
                                <AlchemyEffectSigil
                                  name={eff.name || "Effect"}
                                  isPoison={Boolean(eff.isHostile || eff.isDetrimental)}
                                  size="sm"
                                />
                                <span>{eff.name}</span>
                              </span>
                            ) : (
                              <span key={`${ing.id}-r-${slotIdx}`} className="alchemy-trait-chip is-unknown">
                                <em>?</em> Unknown
                              </span>
                            ),
                          )}
                        </div>
                      </div>
                      <div className="alchemy-row__right">
                        {tasteAvailable ? (
                          <button
                            type="button"
                            className="alchemy-taste-btn"
                            onClick={(ev) => void handleEatIngredient(ing, ev)}
                            disabled={busy}
                          >
                            Taste
                          </button>
                        ) : null}
                        {selected ? (
                          <span className="alchemy-pill alchemy-pill--ready">
                            <Icon name="check" size={11} /> Vessel {Roman}
                          </span>
                        ) : (
                          <span className="alchemy-pill">{ing.value}g</span>
                        )}
                      </div>
                    </div>
                  );
                })
              )
            ) : activeTab === "effects" ? (
              filteredKnownEffects.length === 0 ? (
                <Empty
                  title="No known effects yet"
                  detail="Sample or mix ingredients to populate your Effect Matrix."
                  icon="flask"
                />
              ) : (
                filteredKnownEffects.slice(pageStart, pageEnd).map((eff) => {
                  const isPoisonEff = Boolean(eff.isHostile || eff.isDetrimental);
                  const active = selectedEffectId === eff.effectId;
                  return (
                    <button
                      key={eff.effectId}
                      type="button"
                      className={`alchemy-effect-card${active ? " is-selected" : ""}${isPoisonEff ? " is-poison" : " is-potion"}`}
                      onClick={() => {
                        setSelectedEffectId(eff.effectId);
                        setActiveTab("ingredients");
                      }}
                    >
                      <div className="alchemy-effect-card__head">
                        <AlchemyEffectSigil name={eff.name} isPoison={isPoisonEff} size="lg" />
                        <div className="alchemy-effect-card__meta">
                          <strong>{eff.name}</strong>
                          <small>{eff.school} · {isPoisonEff ? "Toxic Effect" : "Beneficial Effect"}</small>
                        </div>
                        <span className={`alchemy-pill${eff.ingredientCount >= 2 ? " alchemy-pill--ready" : ""}`}>
                          {eff.ingredientCount} reagent{eff.ingredientCount === 1 ? "" : "s"}
                        </span>
                      </div>
                      {eff.descriptionTemplate ? (
                        <div className="alchemy-effect-card__desc">{eff.descriptionTemplate}</div>
                      ) : null}
                    </button>
                  );
                })
              )
            ) : filteredRecipes.length === 0 ? (
              <Empty
                title="No brewable known recipes"
                detail="Carry at least two reagents that share a discovered trait, or test unknown combinations."
                icon="flask"
              />
            ) : (
              filteredRecipes.slice(pageStart, pageEnd).map((rec) => {
                const isLoaded =
                  rec.ingredientIds.length === selectedIds.length &&
                  rec.ingredientIds.every((id) => selectedIds.includes(id));
                return (
                  <button
                    key={rec.id}
                    type="button"
                    className={`alchemy-recipe-card${isLoaded ? " is-selected" : ""}${rec.isPoison ? " is-poison" : " is-potion"}`}
                    onClick={() => loadRecipeInAltar(rec)}
                  >
                    <div className="alchemy-recipe-card__head">
                      <AlchemyEffectSigil name={rec.effects[0]?.name || rec.title} isPoison={rec.isPoison} size="md" />
                      <div className="alchemy-recipe-card__titles">
                        <strong>{rec.title}</strong>
                        <small>{rec.ingredientNames.join(" + ")}</small>
                      </div>
                      <span className="alchemy-pill alchemy-pill--ready">{rec.goldValue}g</span>
                    </div>
                    <div className="alchemy-recipe-card__effects">
                      {rec.effects.map((e) => (
                        <span
                          key={`${rec.id}-${e.effectId}`}
                          className={`alchemy-trait-chip${e.isPoisonEffect ? " is-poison" : " is-potion"}`}
                        >
                          <span>{e.name}</span>
                        </span>
                      ))}
                    </div>
                    <div className="alchemy-recipe-card__foot">
                      <span>Max craft: ×{rec.maxCraftable}</span>
                      <span>+{rec.xpPerCraft} XP</span>
                      {rec.purifiedCount > 0 ? <span className="alchemy-pill alchemy-pill--accent">Purified</span> : null}
                    </div>
                  </button>
                );
              })
            )}
          </div>

          {totalPages > 1 ? (
            <div className="alchemy-pagination">
              <span>
                {pageStart + 1}–{Math.min(pageEnd, activeListCount)} of {activeListCount}
              </span>
              <div className="alchemy-pagination__btns">
                <button
                  type="button"
                  className="alchemy-btn"
                  disabled={clampedPage === 0}
                  onClick={() => setPageIndex((p) => Math.max(0, p - 1))}
                >
                  Previous
                </button>
                <button
                  type="button"
                  className="alchemy-btn"
                  disabled={clampedPage >= totalPages - 1}
                  onClick={() => setPageIndex((p) => Math.min(totalPages - 1, p + 1))}
                >
                  Next
                </button>
              </div>
            </div>
          ) : null}
        </section>

        <aside className="alchemy-altar" aria-label="Distillation Altar">
          <div className="alchemy-altar__head">
            <div>
              <div className="alchemy-altar__eyebrow">DISTILLATION ALTAR</div>
              <h2>Crucible &amp; Phial</h2>
            </div>
            <span className={`alchemy-polarity-badge alchemy-polarity-badge--${phialMode}`}>
              {phialMode === "empty"
                ? "AWAITING REAGENTS"
                : phialMode === "experimental"
                  ? "EXPERIMENTAL SYNTHESIS"
                  : phialMode === "poison"
                    ? "TOXIC POISON"
                    : "VITAL ELIXIR"}
            </span>
          </div>

          <div className="alchemy-crucible-slots">
            {[0, 1, 2].map((slotIdx) => {
              const ing = selectedIngredients[slotIdx];
              const roman = slotIdx === 0 ? "I" : slotIdx === 1 ? "II" : "III";
              return (
                <button
                  key={roman}
                  type="button"
                  className={`alchemy-vessel-slot${ing ? " is-filled" : ""}`}
                  onClick={() => {
                    if (ing) toggleIngredientSelection(ing);
                  }}
                  title={ing ? `Remove ${ing.name} from Vessel ${roman}` : `Vessel ${roman} (select an ingredient)`}
                >
                  <div className="alchemy-vessel-slot__head">
                    <span>VESSEL {roman}</span>
                    {ing ? <Icon name="close" size={11} /> : null}
                  </div>
                  {ing ? (
                    <div className="alchemy-vessel-slot__body">
                      <div className="alchemy-vessel-slot__thumb">
                        <ItemThumbnail item={toThumbnailItem(ing)} bridge={bridge ?? undefined} paused={obscured} />
                      </div>
                      <div className="alchemy-vessel-slot__meta">
                        <strong>{ing.name}</strong>
                        <small>×{ing.count} carried</small>
                      </div>
                    </div>
                  ) : (
                    <div className="alchemy-vessel-slot__empty">
                      <span>{slotIdx < 2 ? "Required" : "Optional 3rd"}</span>
                    </div>
                  )}
                </button>
              );
            })}
          </div>

          <div className="alchemy-conduits" aria-hidden="true">
            <span className={`alchemy-conduit-line${selectedIngredients[0] ? " is-lit" : ""}`} />
            <span className={`alchemy-conduit-line${selectedIngredients[1] ? " is-lit" : ""}`} />
            <span className={`alchemy-conduit-line${selectedIngredients[2] ? " is-lit" : ""}`} />
          </div>

          <div className={`alchemy-plinth alchemy-plinth--${phialMode}`}>
            <AlchemyPhialShowcase mode={phialMode} craftCount={effectiveCraftCount} />
            <div className="alchemy-plinth__caption">
              <strong>
                {vesselsFilled < 2
                  ? "Select 2 or 3 Ingredients"
                  : preview.valid
                    ? customName.trim() || preview.defaultName
                    : preview.hasUnknownTraits
                      ? "Uncharted Alchemical Reaction"
                      : "Incompatible Reagents"}
              </strong>
              <span>
                {vesselsFilled < 2
                  ? "Combine reagents from your pouch to synthesize potions or poisons."
                  : preview.valid
                    ? `${preview.effects.length} active effect${preview.effects.length === 1 ? "" : "s"} · ${preview.goldValue}g value`
                    : preview.hasUnknownTraits
                      ? "Mix 1× batch to test undiscovered traits cleanly."
                      : "No matching effects discovered across these reagents."}
              </span>
            </div>
          </div>

          {preview.purifiedCount > 0 ? (
            <div className="alchemy-notice alchemy-notice--purity">
              <Icon name="check" size={14} />
              <span>
                <strong>Purity Cleansed:</strong> Removed {preview.purifiedNames.join(", ")}.
              </span>
            </div>
          ) : null}

          {selectedIngredients.length >= 2 ? (
            <div className="alchemy-altar-controls">
              <div className="alchemy-rename-box">
                <Icon name="pen" size={13} />
                <input
                  type="text"
                  maxLength={64}
                  value={customName}
                  onChange={(e) => setCustomName(e.target.value)}
                  placeholder={preview.defaultName ? `Custom label (${preview.defaultName})...` : "Custom potion/poison label..."}
                  aria-label="Custom potion or poison name"
                />
                {customName ? (
                  <button
                    type="button"
                    className="alchemy-link"
                    onClick={() => setCustomName("")}
                    title="Reset to default Skyrim name"
                  >
                    Reset
                  </button>
                ) : null}
              </div>

              {preview.effects.length > 0 ? (
                <div className="alchemy-preview-effects">
                  {preview.effects.map((eff) => (
                    <div
                      key={eff.effectId}
                      className={`alchemy-preview-effect${eff.isPoisonEffect ? " is-poison" : " is-potion"}`}
                    >
                      <AlchemyEffectSigil name={eff.name} isPoison={eff.isPoisonEffect} size="sm" />
                      <div className="alchemy-preview-effect__copy">
                        <strong>{eff.name}</strong>
                        <span>{eff.text}</span>
                      </div>
                      <small>{eff.sourceCount}× linked</small>
                    </div>
                  ))}
                </div>
              ) : null}

              <div className="alchemy-batch-box">
                <div className="alchemy-batch-box__head">
                  <span>BREW QUANTITY</span>
                  <strong>
                    {effectiveCraftCount} / {effectiveMaxCraft}
                  </strong>
                </div>
                <div className="alchemy-batch-box__row">
                  <button
                    type="button"
                    className="alchemy-stepper-btn"
                    onClick={() => setCraftCount((c) => Math.max(1, c - 1))}
                    disabled={effectiveCraftCount <= 1 || busy}
                    aria-label="Decrease brew quantity"
                  >
                    −
                  </button>
                  <input
                    type="range"
                    className="alchemy-range"
                    min={1}
                    max={Math.max(1, effectiveMaxCraft)}
                    step={1}
                    value={effectiveCraftCount}
                    onChange={(e) => setCraftCount(Number(e.target.value))}
                    disabled={effectiveMaxCraft <= 1 || busy}
                    aria-label="Brew batch quantity slider"
                  />
                  <button
                    type="button"
                    className="alchemy-stepper-btn"
                    onClick={() => setCraftCount((c) => Math.min(effectiveMaxCraft, c + 1))}
                    disabled={effectiveCraftCount >= effectiveMaxCraft || busy}
                    aria-label="Increase brew quantity"
                  >
                    +
                  </button>
                  <button
                    type="button"
                    className="alchemy-btn"
                    onClick={() => setCraftCount(Math.max(1, effectiveMaxCraft))}
                    disabled={effectiveMaxCraft <= 1 || effectiveCraftCount >= effectiveMaxCraft || busy}
                  >
                    Max
                  </button>
                </div>
                {!preview.valid ? (
                  <small className="alchemy-batch-box__hint">
                    Locked to 1× for experimental unknown combinations so stacks aren&apos;t wasted.
                  </small>
                ) : null}
              </div>

              <div className="alchemy-telemetry-row">
                <div>
                  <span>TOTAL VALUE</span>
                  <strong>{(preview.goldValue * effectiveCraftCount).toLocaleString()}g</strong>
                </div>
                <div>
                  <span>EST. SKILL XP</span>
                  <strong>+{(preview.xpPerCraft * effectiveCraftCount).toFixed(1)}</strong>
                </div>
                <div>
                  <span>WEIGHT</span>
                  <strong>{(0.5 * effectiveCraftCount).toFixed(1)} wt</strong>
                </div>
              </div>
            </div>
          ) : null}

          {statusMessage ? (
            <div className={`alchemy-status alchemy-status--${statusMessage.kind}`}>
              <Icon
                name={
                  statusMessage.kind === "success"
                    ? "check"
                    : statusMessage.kind === "error"
                      ? "alertTriangle"
                      : "flask"
                }
                size={14}
              />
              <span>{statusMessage.text}</span>
            </div>
          ) : null}

          <div className="alchemy-altar__footer">
            {selectedIds.length > 0 ? (
              <button
                type="button"
                className="alchemy-btn"
                onClick={() => {
                  setSelectedIds([]);
                  setCraftCount(1);
                }}
                disabled={busy}
              >
                Clear Crucible
              </button>
            ) : null}
            <button
              type="button"
              className={`alchemy-brew-btn alchemy-brew-btn--${phialMode}`}
              disabled={busy || selectedIngredients.length < 2}
              onClick={() => void handleBrew()}
            >
              <Icon name={preview.isPoison ? "potion" : "flask"} size={15} />
              <span>
                {busy
                  ? "Synthesizing..."
                  : selectedIngredients.length < 2
                    ? "Select 2 Ingredients to Brew"
                    : preview.valid
                      ? `Brew ${effectiveCraftCount > 1 ? `${effectiveCraftCount}× ` : ""}${customName.trim() || preview.defaultName}`
                      : "Attempt Experimental Mix (1×)"}
              </span>
              <kbd>Enter</kbd>
            </button>
          </div>
        </aside>
      </div>
    </div>
  );
}
