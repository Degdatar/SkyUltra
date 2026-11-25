import * as React from 'react';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  createSkyUltraGame,
  type SkyUltraBarterDisplayMode,
  type SkyUltraBlacksmithMaterial,
  type SkyUltraBlacksmithRecipe,
  type SkyUltraBlacksmithRequirement,
  type SkyUltraBlacksmithSnapshot,
  type SkyUltraBlacksmithStationKind,
  type SkyUltraBlacksmithTemperItem,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { ItemThumbnail } from './ItemThumbnail.js';
import { SkyChoice, SkySegmentedControl } from '../core.js';
import { Icon, smithingCategoryIcon, type IconName } from './icons.js';
import { Empty } from './pieces.js';
import { formId, type Item, type Player } from './model.js';

const PAGE_SIZE = 48;
const MAX_BATCH_CRAFT = 99;

const DISPLAY_MODE_LABELS: Record<SkyUltraBarterDisplayMode, string> = {
  half: 'Half Panel',
  windowed: 'Windowed',
  full: 'Full Screen',
};

interface StationMeta {
  icon: IconName;
  eyebrow: string;
  actionVerb: string;
  emptyTitle: string;
  emptyDetail: string;
}

export function stationMeta(kind: SkyUltraBlacksmithStationKind, isSkyforge = false): StationMeta {
  switch (kind) {
    case 'tanning':
      return {
        icon: 'smithPelt',
        eyebrow: 'TANNING RACK · LEATHERWORK',
        actionVerb: 'Tan',
        emptyTitle: 'No Tanning Recipes',
        emptyDetail: 'No leather or strip recipes matched your active search or filters.',
      };
    case 'smelter':
      return {
        icon: 'flame',
        eyebrow: 'SMELTER · ORE REFINING',
        actionVerb: 'Smelt',
        emptyTitle: 'No Smelting Recipes',
        emptyDetail: 'No ore or ingot smelting recipes matched your active search or filters.',
      };
    case 'grindstone':
      return {
        icon: 'sword',
        eyebrow: 'GRINDSTONE · WEAPON TEMPERING',
        actionVerb: 'Sharpen',
        emptyTitle: 'No Improvable Weapons',
        emptyDetail: 'No carried or equipped weapons matched your active search or filters.',
      };
    case 'workbench':
      return {
        icon: 'shield',
        eyebrow: 'ARMOR WORKBENCH · TEMPERING',
        actionVerb: 'Improve',
        emptyTitle: 'No Improvable Armor',
        emptyDetail: 'No carried or equipped armor pieces matched your active search or filters.',
      };
    case 'forge':
    default:
      return {
        icon: 'anvil',
        eyebrow: isSkyforge ? 'SKYFORGE · ANCIENT SMITHING' : 'BLACKSMITH FORGE · CRAFTING',
        actionVerb: 'Forge',
        emptyTitle: 'No Forge Recipes',
        emptyDetail: 'No forge crafting recipes matched your active search or filters.',
      };
  }
}

export function canShowBlacksmithQuantitySlider(recipe: Pick<SkyUltraBlacksmithRecipe, 'canCraft' | 'maxCraftable'> | null): boolean {
  return Boolean(recipe && recipe.canCraft && recipe.maxCraftable > 1);
}

function toThumbItem(id: string, name: string, category: string): Item {
  return {
    id,
    name,
    category: category || 'miscellaneous',
    count: 1,
    value: 0,
    weight: 0,
    equipped: false,
  };
}

function temperKey(item: Pick<SkyUltraBlacksmithTemperItem, 'itemId' | 'stackIndex' | 'recipeId'>): string {
  return `${item.itemId}:${item.stackIndex}:${item.recipeId}`;
}

function recipePrimaryStat(recipe: SkyUltraBlacksmithRecipe): { value: number; unit: 'DMG' | 'AR' } | null {
  if (typeof recipe.damage === 'number' && recipe.damage > 0) {
    return { value: Math.round(recipe.damage), unit: 'DMG' };
  }
  if (typeof recipe.armorRating === 'number' && recipe.armorRating > 0) {
    return { value: Math.round(recipe.armorRating), unit: 'AR' };
  }
  return null;
}

const PREVIEW_SNAPSHOT: SkyUltraBlacksmithSnapshot = {
  available: true,
  stationKind: 'forge',
  stationMode: 'craft',
  stationName: 'Skyforge',
  isSkyforge: true,
  smithingSkill: 58,
  smithingBaseSkill: 50,
  hasArcaneBlacksmith: true,
  playerName: 'Dragonborn',
  playerGold: 4250,
  playerCarryWeight: { current: 164, max: 350 },
  categories: [
    { id: 'Steel', label: 'Steel', count: 2 },
    { id: 'Leather', label: 'Leather', count: 1 },
    { id: 'Jewelry', label: 'Jewelry', count: 1 },
  ],
  recipes: [
    {
      recipeId: '0x00013987',
      itemId: '0x00013989',
      name: 'Skyforge Steel Sword',
      category: 'Steel',
      itemType: 'weapon',
      outputCount: 1,
      value: 140,
      weight: 10,
      damage: 18,
      weaponType: 'One-Handed Sword',
      ownedCount: 1,
      materials: [
        { id: '0x0005ACE5', name: 'Steel Ingot', required: 2, owned: 6, met: true },
        { id: '0x0005ACE4', name: 'Iron Ingot', required: 1, owned: 4, met: true },
        { id: '0x000800E4', name: 'Leather Strips', required: 1, owned: 9, met: true },
      ],
      requirements: [{ label: 'Steel Smithing', met: true, kind: 'perk' }],
      conditionsMet: true,
      materialsMet: true,
      canCraft: true,
      maxCraftable: 3,
    },
    {
      recipeId: '0x0003B9D1',
      itemId: '0x00013952',
      name: 'Steel Armor',
      category: 'Steel',
      itemType: 'armor',
      outputCount: 1,
      value: 275,
      weight: 35,
      armorRating: 48,
      armorTypeLabel: 'Heavy Armor',
      ownedCount: 0,
      materials: [
        { id: '0x0005ACE5', name: 'Steel Ingot', required: 4, owned: 6, met: true },
        { id: '0x000800E4', name: 'Leather Strips', required: 3, owned: 9, met: true },
      ],
      requirements: [{ label: 'Steel Smithing', met: true, kind: 'perk' }],
      conditionsMet: true,
      materialsMet: true,
      canCraft: true,
      maxCraftable: 1,
    },
    {
      recipeId: '0x00013920',
      itemId: '0x0001391E',
      name: 'Leather Bracers',
      category: 'Leather',
      itemType: 'armor',
      outputCount: 1,
      value: 25,
      weight: 2,
      armorRating: 11,
      armorTypeLabel: 'Light Armor',
      ownedCount: 0,
      materials: [
        { id: '0x000DB5D2', name: 'Leather', required: 1, owned: 5, met: true },
        { id: '0x000800E4', name: 'Leather Strips', required: 2, owned: 9, met: true },
      ],
      requirements: [],
      conditionsMet: true,
      materialsMet: true,
      canCraft: true,
      maxCraftable: 4,
    },
    {
      recipeId: '0x0001CF2B',
      itemId: '0x0003B97C',
      name: 'Gold Diamond Necklace',
      category: 'Jewelry',
      itemType: 'armor',
      outputCount: 1,
      value: 1200,
      weight: 0.5,
      armorTypeLabel: 'Jewelry',
      ownedCount: 0,
      materials: [
        { id: '0x0005AD9E', name: 'Gold Ingot', required: 1, owned: 2, met: true },
        { id: '0x00063B47', name: 'Flawless Diamond', required: 1, owned: 0, met: false },
      ],
      requirements: [],
      conditionsMet: true,
      materialsMet: false,
      canCraft: false,
      maxCraftable: 0,
      blockReason: 'Missing Flawless Diamond (0/1)',
    },
  ],
  temperItems: [],
};

type SortMode = 'name' | 'ready' | 'stat' | 'value' | 'weight';
type FilterMode = 'all' | 'ready' | 'equipped';

export default function BlacksmithPage({
  player,
  bridge,
  obscured = false,
  displayMode = 'half',
  pauseWorld = true,
  onChangeDisplayMode,
  onTogglePauseWorld,
  onClose,
}: {
  player: Player | null;
  bridge?: SkyWebViewAPI;
  obscured?: boolean;
  displayMode?: SkyUltraBarterDisplayMode;
  pauseWorld?: boolean;
  onChangeDisplayMode?: (mode: SkyUltraBarterDisplayMode) => void;
  onTogglePauseWorld?: () => void;
  onClose?: () => void;
}): React.JSX.Element {
  const game = useMemo(() => (bridge ? createSkyUltraGame(bridge) : null), [bridge]);
  const [snapshot, setSnapshot] = useState<SkyUltraBlacksmithSnapshot | null>(() => (bridge ? null : PREVIEW_SNAPSHOT));
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('all');
  const [filterMode, setFilterMode] = useState<FilterMode>('all');
  const [sortMode, setSortMode] = useState<SortMode>('ready');
  const [layout, setLayout] = useState<'grid' | 'list'>('list');
  const [selectedId, setSelectedId] = useState('');
  const [craftQuantity, setCraftQuantity] = useState(1);
  const [offset, setOffset] = useState(0);
  const [statusMessage, setStatusMessage] = useState('');
  const [statusTone, setStatusTone] = useState<'neutral' | 'success' | 'error'>('neutral');
  const [revision, setRevision] = useState(0);
  const lockedRef = useRef(false);
  const actionControllerRef = useRef<AbortController | null>(null);

  useEffect(() => () => {
    actionControllerRef.current?.abort();
  }, [bridge]);

  useEffect(() => {
    setOffset(0);
  }, [query, category, filterMode, sortMode, snapshot?.stationKind]);

  useEffect(() => {
    if (!game || !bridge || obscured) return;
    let active = true;
    let pending = false;
    const controller = new AbortController();
    const refresh = async (): Promise<void> => {
      if (pending || document.hidden || !(bridge.getVisibility?.().visible ?? true)) return;
      pending = true;
      setLoading(true);
      try {
        const res = await game.blacksmithSnapshot({ signal: controller.signal });
        if (active) {
          setSnapshot(res);
          if (!res.available && res.message) {
            setStatusMessage(res.message);
            setStatusTone('neutral');
          }
        }
      } catch (error) {
        if (active && !controller.signal.aborted) {
          setStatusMessage(error instanceof Error ? error.message : 'Unable to read blacksmith station data.');
          setStatusTone('error');
        }
      } finally {
        pending = false;
        if (active) setLoading(false);
      }
    };
    void refresh();
    const onVis = (): void => {
      void refresh();
    };
    const onStationEvent = (ev?: { active?: boolean }): void => {
      if (ev?.active === false) return;
      setCategory('all');
      setFilterMode('all');
      setQuery('');
      setCraftQuantity(1);
      setStatusMessage('');
      setStatusTone('neutral');
      void refresh();
    };
    const unsubscribeVis = bridge.on('skyultra.visibility', onVis);
    const unsubscribeBlacksmith = bridge.on<{ active?: boolean }>('skyultra.blacksmithMode', onStationEvent);
    document.addEventListener('visibilitychange', onVis);
    return () => {
      active = false;
      controller.abort();
      unsubscribeVis();
      unsubscribeBlacksmith();
      document.removeEventListener('visibilitychange', onVis);
    };
  }, [game, bridge, obscured, revision]);

  const effectiveSnapshot = snapshot;
  const stationKind: SkyUltraBlacksmithStationKind = effectiveSnapshot?.stationKind ?? 'forge';
  const stationMode = effectiveSnapshot?.stationMode ?? (stationKind === 'grindstone' || stationKind === 'workbench' ? 'temper' : 'craft');
  const meta = useMemo(() => stationMeta(stationKind, effectiveSnapshot?.isSkyforge), [stationKind, effectiveSnapshot?.isSkyforge]);
  const isTemperMode = stationMode === 'temper';

  useEffect(() => {
    if (!isTemperMode && filterMode === 'equipped') {
      setFilterMode('all');
    }
  }, [isTemperMode, filterMode]);

  const categories = useMemo(() => {
    const raw = (effectiveSnapshot?.categories ?? []).filter((cat) => cat.id.toLowerCase() !== 'all');
    const totalCount = isTemperMode ? effectiveSnapshot?.temperItems.length ?? 0 : effectiveSnapshot?.recipes.length ?? 0;
    return [{ id: 'all', label: 'All', count: totalCount }, ...raw];
  }, [effectiveSnapshot, isTemperMode]);

  useEffect(() => {
    if (category !== 'all' && !categories.some((c) => c.id === category)) {
      setCategory('all');
    }
  }, [category, categories]);

  const filteredRecipes = useMemo(() => {
    if (isTemperMode) return [] as SkyUltraBlacksmithRecipe[];
    const q = query.trim().toLowerCase();
    const list = (effectiveSnapshot?.recipes ?? []).filter((recipe) => {
      if (category !== 'all' && recipe.category !== category) return false;
      if (filterMode === 'ready' && !recipe.canCraft) return false;
      if (!q) return true;
      const matsText = recipe.materials.map((m) => m.name).join(' ');
      const reqsText = recipe.requirements.map((r) => r.label).join(' ');
      return `${recipe.name} ${recipe.category} ${recipe.weaponType ?? ''} ${recipe.armorTypeLabel ?? ''} ${matsText} ${reqsText} ${formId(recipe.itemId)}`
        .toLowerCase()
        .includes(q);
    });
    return [...list].sort((a, b) => {
      if (sortMode === 'ready') {
        const diff = Number(b.canCraft) - Number(a.canCraft);
        if (diff !== 0) return diff;
      } else if (sortMode === 'stat') {
        const sa = recipePrimaryStat(a)?.value ?? 0;
        const sb = recipePrimaryStat(b)?.value ?? 0;
        if (sb !== sa) return sb - sa;
      } else if (sortMode === 'value') {
        if (b.value !== a.value) return b.value - a.value;
      } else if (sortMode === 'weight') {
        if (b.weight !== a.weight) return b.weight - a.weight;
      }
      return a.name.localeCompare(b.name);
    });
  }, [isTemperMode, effectiveSnapshot?.recipes, category, filterMode, query, sortMode]);

  const filteredTemperItems = useMemo(() => {
    if (!isTemperMode) return [] as SkyUltraBlacksmithTemperItem[];
    const q = query.trim().toLowerCase();
    const list = (effectiveSnapshot?.temperItems ?? []).filter((item) => {
      if (category !== 'all' && item.category !== category) return false;
      if (filterMode === 'ready' && !item.canTemper) return false;
      if (filterMode === 'equipped' && !item.equipped) return false;
      if (!q) return true;
      const matsText = item.materials.map((m) => m.name).join(' ');
      return `${item.name} ${item.currentTier} ${item.targetTier} ${item.weaponType ?? ''} ${item.armorTypeLabel ?? ''} ${matsText} ${formId(item.itemId)}`
        .toLowerCase()
        .includes(q);
    });
    return [...list].sort((a, b) => {
      if (sortMode === 'ready') {
        const diff = Number(b.canTemper) - Number(a.canTemper);
        if (diff !== 0) return diff;
        if (b.equipped !== a.equipped) return Number(b.equipped) - Number(a.equipped);
      } else if (sortMode === 'stat') {
        if (b.currentStat !== a.currentStat) return b.currentStat - a.currentStat;
      } else if (sortMode === 'value') {
        if (b.value !== a.value) return b.value - a.value;
      } else if (sortMode === 'weight') {
        if (b.weight !== a.weight) return b.weight - a.weight;
      }
      return a.name.localeCompare(b.name);
    });
  }, [isTemperMode, effectiveSnapshot?.temperItems, category, filterMode, query, sortMode]);

  const totalFiltered = isTemperMode ? filteredTemperItems.length : filteredRecipes.length;
  const pagedRecipes = useMemo(() => filteredRecipes.slice(offset, offset + PAGE_SIZE), [filteredRecipes, offset]);
  const pagedTemper = useMemo(() => filteredTemperItems.slice(offset, offset + PAGE_SIZE), [filteredTemperItems, offset]);

  const selectedRecipe = useMemo(() => {
    if (isTemperMode) return null;
    return filteredRecipes.find((r) => r.recipeId === selectedId) ?? pagedRecipes[0] ?? filteredRecipes[0] ?? null;
  }, [isTemperMode, filteredRecipes, pagedRecipes, selectedId]);

  const selectedTemper = useMemo(() => {
    if (!isTemperMode) return null;
    return (
      filteredTemperItems.find((item) => temperKey(item) === selectedId) ??
      pagedTemper[0] ??
      filteredTemperItems[0] ??
      null
    );
  }, [isTemperMode, filteredTemperItems, pagedTemper, selectedId]);

  useEffect(() => {
    setCraftQuantity(1);
  }, [selectedRecipe?.recipeId]);

  const maxBatch = Math.max(1, Math.min(MAX_BATCH_CRAFT, selectedRecipe?.maxCraftable ?? 1));
  const clampedQty = Math.max(1, Math.min(maxBatch, craftQuantity));
  const showQtySlider = canShowBlacksmithQuantitySlider(selectedRecipe);

  const smithingSkill = Math.round(effectiveSnapshot?.smithingSkill ?? 15);
  const smithingBase = Math.round(effectiveSnapshot?.smithingBaseSkill ?? smithingSkill);
  const smithingBuff = smithingSkill - smithingBase;
  const playerGold = effectiveSnapshot?.playerGold ?? player?.gold ?? 0;
  const carryCurrent = Math.round(effectiveSnapshot?.playerCarryWeight?.current ?? player?.carryWeight?.current ?? 0);
  const carryMax = Math.max(1, Math.round(effectiveSnapshot?.playerCarryWeight?.max ?? player?.carryWeight?.max ?? 300));
  const stationTitle = effectiveSnapshot?.stationName || 'Blacksmith Station';

  const readyCount = useMemo(() => {
    if (isTemperMode) {
      return (effectiveSnapshot?.temperItems ?? []).filter((item) => item.canTemper).length;
    }
    return (effectiveSnapshot?.recipes ?? []).filter((recipe) => recipe.canCraft).length;
  }, [isTemperMode, effectiveSnapshot]);

  const cycleDisplayMode = (): void => {
    if (!onChangeDisplayMode) return;
    const order: SkyUltraBarterDisplayMode[] = ['half', 'windowed', 'full'];
    const next = order[(order.indexOf(displayMode) + 1) % order.length] ?? 'half';
    onChangeDisplayMode(next);
  };

  const executeCraft = useCallback(
    async (recipe: SkyUltraBlacksmithRecipe, count: number): Promise<void> => {
      if (!game || obscured || lockedRef.current) return;
      const safeCount = Math.max(1, Math.min(Math.max(1, recipe.maxCraftable), count));
      if (!recipe.canCraft) {
        setStatusMessage(recipe.blockReason || `Cannot ${meta.actionVerb.toLowerCase()} ${recipe.name}.`);
        setStatusTone('error');
        return;
      }
      lockedRef.current = true;
      setBusy(true);
      actionControllerRef.current?.abort();
      const controller = new AbortController();
      actionControllerRef.current = controller;
      try {
        const res = await game.blacksmithCraft({ recipeId: recipe.recipeId, count: safeCount }, { signal: controller.signal });
        if (res.snapshot) {
          setSnapshot(res.snapshot);
        } else {
          setRevision((r) => r + 1);
        }
        if (res.accepted) {
          const totalOut = res.craftedCount ?? safeCount * Math.max(1, recipe.outputCount);
          const xpNote = typeof res.xpGained === 'number' && res.xpGained > 0 ? ` (+${res.xpGained} Smithing XP)` : '';
          setStatusMessage(res.message || `${meta.actionVerb}d ${res.craftedName || recipe.name}${totalOut > 1 ? ` ×${totalOut}` : ''}${xpNote}.`);
          setStatusTone('success');
          setCraftQuantity(1);
        } else {
          setStatusMessage(res.message || `Failed to ${meta.actionVerb.toLowerCase()} ${recipe.name}.`);
          setStatusTone('error');
        }
      } catch (error) {
        if (!controller.signal.aborted) {
          setStatusMessage(error instanceof Error ? error.message : 'Crafting request failed.');
          setStatusTone('error');
        }
      } finally {
        lockedRef.current = false;
        setBusy(false);
      }
    },
    [game, obscured, meta.actionVerb],
  );

  const executeTemper = useCallback(
    async (item: SkyUltraBlacksmithTemperItem): Promise<void> => {
      if (!game || obscured || lockedRef.current) return;
      if (!item.canTemper) {
        setStatusMessage(item.blockReason || `Cannot ${meta.actionVerb.toLowerCase()} ${item.name}.`);
        setStatusTone('error');
        return;
      }
      lockedRef.current = true;
      setBusy(true);
      actionControllerRef.current?.abort();
      const controller = new AbortController();
      actionControllerRef.current = controller;
      try {
        const res = await game.blacksmithTemper(
          { recipeId: item.recipeId, itemId: item.itemId, stackIndex: item.stackIndex },
          { signal: controller.signal },
        );
        if (res.snapshot) {
          setSnapshot(res.snapshot);
        } else {
          setRevision((r) => r + 1);
        }
        if (res.accepted) {
          const xpNote = typeof res.xpGained === 'number' && res.xpGained > 0 ? ` (+${res.xpGained} Smithing XP)` : '';
          setStatusMessage(
            res.message ||
              `Improved ${res.itemName || item.name} to ${res.newTier || item.targetTier}${xpNote}.`,
          );
          setStatusTone('success');
        } else {
          setStatusMessage(res.message || `Failed to ${meta.actionVerb.toLowerCase()} ${item.name}.`);
          setStatusTone('error');
        }
      } catch (error) {
        if (!controller.signal.aborted) {
          setStatusMessage(error instanceof Error ? error.message : 'Tempering request failed.');
          setStatusTone('error');
        }
      } finally {
        lockedRef.current = false;
        setBusy(false);
      }
    },
    [game, obscured, meta.actionVerb],
  );

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent): void => {
      if (obscured || busy || document.querySelector('[role="dialog"]')) return;
      if (event.altKey || event.ctrlKey || event.metaKey) return;
      const target = event.target as HTMLElement | null;
      if (target?.closest('input, textarea, select, [contenteditable=true]')) return;

      const keys = isTemperMode ? pagedTemper.map(temperKey) : pagedRecipes.map((r) => r.recipeId);
      if (!keys.length) return;

      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        const activeKey = isTemperMode ? (selectedTemper ? temperKey(selectedTemper) : '') : selectedRecipe?.recipeId ?? '';
        const idx = keys.indexOf(activeKey);
        const nextIdx =
          event.key === 'ArrowDown'
            ? (idx < 0 ? 0 : Math.min(keys.length - 1, idx + 1))
            : (idx < 0 ? 0 : Math.max(0, idx - 1));
        setSelectedId(keys[nextIdx] ?? '');
      } else if (event.key === 'Enter' && !target?.closest('button, a, [role="button"]')) {
        if (!bridge) return;
        if (!isTemperMode && selectedRecipe?.canCraft) {
          event.preventDefault();
          void executeCraft(selectedRecipe, showQtySlider ? clampedQty : 1);
        } else if (isTemperMode && selectedTemper?.canTemper) {
          event.preventDefault();
          void executeTemper(selectedTemper);
        }
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [
    obscured,
    busy,
    bridge,
    isTemperMode,
    pagedTemper,
    pagedRecipes,
    selectedTemper,
    selectedRecipe,
    showQtySlider,
    clampedQty,
    executeCraft,
    executeTemper,
  ]);

  const stationUnavailable = Boolean(bridge && effectiveSnapshot && !effectiveSnapshot.available);

  return (
    <div className={`page blacksmith-page blacksmith-page--${stationKind}`}>
      <header className="blacksmith-masthead">
        <div className="blacksmith-masthead__title-wrap">
          <div className="blacksmith-station-badge" aria-hidden="true">
            <Icon name={meta.icon} size={20} />
          </div>
          <div className="blacksmith-heading">
            <span className="eyebrow">{meta.eyebrow}</span>
            <h1>
              {stationTitle}
              {effectiveSnapshot?.isSkyforge && <span className="blacksmith-skyforge-tag">SKYFORGE</span>}
            </h1>
          </div>
        </div>

        <div className="blacksmith-masthead__end">
          <div className="blacksmith-skill-pill" title="Current Smithing Skill">
            <Icon name={meta.icon} size={13} />
            <span>SMITHING</span>
            <strong>{smithingSkill}</strong>
            {smithingBuff > 0 && <em className="is-buff">+{smithingBuff}</em>}
            {effectiveSnapshot?.hasArcaneBlacksmith && (
              <em className="is-arcane" title="Arcane Blacksmith perk active — enchanted gear can be improved">
                ARCANE
              </em>
            )}
          </div>

          <div className="blacksmith-stat-chip" title="Player Gold">
            <Icon name="gold" size={12} />
            <strong>{playerGold.toLocaleString()}</strong>
            <small>GOLD</small>
          </div>

          <div className="blacksmith-stat-chip" title="Player Carry Weight">
            <Icon name="weight" size={12} />
            <strong>
              {carryCurrent} / {carryMax}
            </strong>
            <small>WT</small>
          </div>

          <div className="blacksmith-mode-pills" role="group" aria-label="Blacksmith layout and world controls">
            {onChangeDisplayMode && (
              <button
                type="button"
                className={'control-button blacksmith-mode-btn' + (displayMode !== 'half' ? ' is-active' : '')}
                onClick={cycleDisplayMode}
                title={`Cycle Blacksmith Layout Mode (Current: ${DISPLAY_MODE_LABELS[displayMode]})`}
              >
                <Icon name="widgets" size={12} />
                <span>{DISPLAY_MODE_LABELS[displayMode]}</span>
              </button>
            )}
            {onTogglePauseWorld && (
              <button
                type="button"
                className={'control-button blacksmith-mode-btn' + (pauseWorld ? ' is-active' : '')}
                onClick={onTogglePauseWorld}
                aria-pressed={Boolean(pauseWorld)}
                title={`Toggle pausing Skyrim world while crafting (Current: ${pauseWorld ? 'Pause World' : 'Live World'})`}
              >
                <Icon name={pauseWorld ? 'pause' : 'play'} size={12} />
                <span>{pauseWorld ? 'Pause World' : 'Live World'}</span>
              </button>
            )}
            {bridge && (
              <button
                type="button"
                className="control-button blacksmith-mode-btn is-icon-only"
                disabled={loading || busy}
                onClick={() => setRevision((r) => r + 1)}
                title="Refresh station recipes and inventory"
                aria-label="Refresh blacksmith snapshot"
              >
                <Icon name="refresh" size={12} />
              </button>
            )}
          </div>

          {onClose && (
            <button type="button" className="control-button blacksmith-close-btn" onClick={onClose} aria-label="Close blacksmith menu">
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          )}
        </div>
      </header>

      {stationUnavailable ? (
        <section className="surface blacksmith-unavailable" aria-label="Blacksmith Station Unavailable">
          <Empty
            icon={meta.icon}
            title="Blacksmith Station Closed"
            detail={effectiveSnapshot?.message || 'Activate a Forge, Tanning Rack, Smelter, Grindstone, or Armor Workbench in Skyrim.'}
          />
        </section>
      ) : (
        <div className="blacksmith-workbench" aria-busy={loading || busy}>
          <section className="surface blacksmith-catalog" aria-label={`${stationTitle} catalog`}>
            <div className="blacksmith-toolbar">
              <div className="blacksmith-toolbar__top">
                <label className="page-search">
                  <Icon name="search" size={15} />
                  <input
                    aria-label="Search recipes or materials"
                    value={query}
                    onChange={(e) => setQuery(e.currentTarget.value)}
                    placeholder={isTemperMode ? 'Search gear or tempering ingot…' : 'Search recipes, perks, or materials…'}
                  />
                  {query ? (
                    <button type="button" aria-label="Clear blacksmith search" onClick={() => setQuery('')}>
                      <Icon name="close" size={12} />
                    </button>
                  ) : (
                    <kbd>Space</kbd>
                  )}
                </label>

                <SkySegmentedControl
                  label="Blacksmith catalog view"
                  value={layout}
                  onValueChange={setLayout}
                  options={[
                    { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' },
                    { value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' },
                  ]}
                />
              </div>

              <div className="blacksmith-toolbar__filters">
                <div className="blacksmith-filter-pills" role="group" aria-label="Recipe readiness filter">
                  <button
                    type="button"
                    className={'control-button' + (filterMode === 'all' ? ' is-active' : '')}
                    onClick={() => setFilterMode('all')}
                  >
                    All ({isTemperMode ? effectiveSnapshot?.temperItems.length ?? 0 : effectiveSnapshot?.recipes.length ?? 0})
                  </button>
                  <button
                    type="button"
                    className={'control-button' + (filterMode === 'ready' ? ' is-active' : '')}
                    onClick={() => setFilterMode('ready')}
                  >
                    Ready ({readyCount})
                  </button>
                  {isTemperMode && (
                    <button
                      type="button"
                      className={'control-button' + (filterMode === 'equipped' ? ' is-active' : '')}
                      onClick={() => setFilterMode('equipped')}
                    >
                      Equipped
                    </button>
                  )}
                </div>

                <SkyChoice
                  label="Sort blacksmith items"
                  value={sortMode}
                  onValueChange={(v) => setSortMode(v as SortMode)}
                  options={[
                    { value: 'ready', label: 'Ready First' },
                    { value: 'name', label: 'Name A–Z' },
                    { value: 'stat', label: isTemperMode ? 'Highest Stat' : 'Highest DMG/AR' },
                    { value: 'value', label: 'Highest Value' },
                    { value: 'weight', label: 'Highest Weight' },
                  ]}
                />
              </div>
            </div>

            <div className="blacksmith-body">
              <div className="blacksmith-categories" role="group" aria-label="Smithing categories">
                {categories.map((cat) => (
                  <button
                    type="button"
                    key={cat.id}
                    aria-pressed={category === cat.id}
                    onClick={() => setCategory(cat.id)}
                    title={`${cat.label} (${cat.count})`}
                  >
                    <Icon name={smithingCategoryIcon(cat.id, meta.icon)} size={16} />
                    <span>{cat.label}</span>
                    <small>{cat.count}</small>
                  </button>
                ))}
              </div>

              <div className={'blacksmith-results blacksmith-results--' + layout}>
                {isTemperMode ? (
                  pagedTemper.length ? (
                    pagedTemper.map((item) => {
                      const key = temperKey(item);
                      const isSelected = selectedTemper ? temperKey(selectedTemper) === key : false;
                      const statUnit = stationKind === 'grindstone' ? 'DMG' : 'AR';
                      const delta = Math.round((item.projectedStat - item.currentStat) * 10) / 10;
                      const thumb = toThumbItem(item.itemId, item.name, item.itemType || 'weapon');
                      return layout === 'list' ? (
                        <button
                          type="button"
                          key={key}
                          aria-pressed={isSelected}
                          className={
                            'blacksmith-row' +
                            (isSelected ? ' is-active' : '') +
                            (item.canTemper ? ' is-ready' : ' is-locked') +
                            (item.equipped ? ' is-equipped' : '') +
                            (item.enchanted ? ' is-enchanted' : '')
                          }
                          onClick={() => setSelectedId(key)}
                          onDoubleClick={() => {
                            if (item.canTemper && bridge && !busy && !obscured) void executeTemper(item);
                          }}
                        >
                          <span className="blacksmith-row__thumb">
                            <ItemThumbnail item={thumb} bridge={bridge} paused={obscured} />
                          </span>
                          <span className="blacksmith-row__main">
                            <strong>
                              {item.enchanted && <Icon name="flame" size={11} />}
                              <span>{item.name}</span>
                              {item.count > 1 && <em>×{item.count}</em>}
                            </strong>
                            <small>
                              {item.equipped && <b className="blacksmith-pill is-equipped">EQUIPPED</b>}
                              <span>{item.weaponType || item.armorTypeLabel || item.category}</span>
                              {item.currentTier ? (
                                <span className="blacksmith-tier-pill">{item.currentTier}</span>
                              ) : (
                                <span className="blacksmith-tier-pill is-base">Untempered</span>
                              )}
                            </small>
                          </span>
                          <span className="blacksmith-row__stats">
                            <strong className="blacksmith-stat-delta">
                              {Math.round(item.currentStat)} → {Math.round(item.projectedStat)} <small>{statUnit}</small>
                              {delta > 0 && <em>+{Math.round(delta)}</em>}
                            </strong>
                            <span className={'blacksmith-status-chip' + (item.canTemper ? ' is-ready' : ' is-locked')}>
                              <Icon name={item.canTemper ? 'check' : 'lock'} size={10} />
                              {item.canTemper ? item.targetTier : 'Locked'}
                            </span>
                          </span>
                        </button>
                      ) : (
                        <button
                          type="button"
                          key={key}
                          aria-pressed={isSelected}
                          className={
                            'blacksmith-tile' +
                            (isSelected ? ' is-active' : '') +
                            (item.canTemper ? ' is-ready' : ' is-locked') +
                            (item.equipped ? ' is-equipped' : '') +
                            (item.enchanted ? ' is-enchanted' : '')
                          }
                          onClick={() => setSelectedId(key)}
                          onDoubleClick={() => {
                            if (item.canTemper && bridge && !busy && !obscured) void executeTemper(item);
                          }}
                        >
                          <span className="blacksmith-tile__top">
                            <small>{item.equipped ? 'EQUIPPED' : item.currentTier || 'Base'}</small>
                            <span>
                              {Math.round(item.projectedStat)} {statUnit}
                            </span>
                          </span>
                          <span className="blacksmith-tile__art">
                            <ItemThumbnail item={thumb} bridge={bridge} paused={obscured} />
                            {item.count > 1 && <b className="blacksmith-tile__qty">×{item.count}</b>}
                          </span>
                          <span className="blacksmith-tile__copy">
                            <strong>{item.name}</strong>
                            <span className={'blacksmith-status-chip' + (item.canTemper ? ' is-ready' : ' is-locked')}>
                              <Icon name={item.canTemper ? 'check' : 'lock'} size={10} />
                              {item.canTemper ? `→ ${item.targetTier}` : 'Locked'}
                            </span>
                          </span>
                        </button>
                      );
                    })
                  ) : (
                    <Empty icon={meta.icon} title={loading ? 'Scanning carried equipment…' : meta.emptyTitle} detail={meta.emptyDetail} />
                  )
                ) : pagedRecipes.length ? (
                  pagedRecipes.map((recipe) => {
                    const isSelected = selectedRecipe?.recipeId === recipe.recipeId;
                    const stat = recipePrimaryStat(recipe);
                    const thumb = toThumbItem(recipe.itemId, recipe.name, recipe.itemType || 'miscellaneous');
                    return layout === 'list' ? (
                      <button
                        type="button"
                        key={recipe.recipeId}
                        aria-pressed={isSelected}
                        className={
                          'blacksmith-row' +
                          (isSelected ? ' is-active' : '') +
                          (recipe.canCraft ? ' is-ready' : ' is-locked') +
                          (recipe.enchanted ? ' is-enchanted' : '')
                        }
                        onClick={() => setSelectedId(recipe.recipeId)}
                        onDoubleClick={() => {
                          if (recipe.canCraft && bridge && !busy && !obscured) void executeCraft(recipe, 1);
                        }}
                      >
                        <span className="blacksmith-row__thumb">
                          <ItemThumbnail item={thumb} bridge={bridge} paused={obscured} />
                        </span>
                        <span className="blacksmith-row__main">
                          <strong>
                            {recipe.enchanted && <Icon name="flame" size={11} />}
                            <span>{recipe.name}</span>
                            {recipe.outputCount > 1 && <em>×{recipe.outputCount}</em>}
                          </strong>
                          <small>
                            <span>{recipe.weaponType || recipe.armorTypeLabel || recipe.category}</span>
                            {typeof recipe.ownedCount === 'number' && recipe.ownedCount > 0 && (
                              <span className="blacksmith-owned-tag">Owned ×{recipe.ownedCount}</span>
                            )}
                            <span className="blacksmith-row__mats">
                              {recipe.materials.map((m) => `${m.name} (${m.owned}/${m.required})`).join(' · ')}
                            </span>
                          </small>
                        </span>
                        <span className="blacksmith-row__stats">
                          {stat && (
                            <span className="blacksmith-stat-pill">
                              <strong>{stat.value}</strong>
                              <small>{stat.unit}</small>
                            </span>
                          )}
                          <span className={'blacksmith-status-chip' + (recipe.canCraft ? ' is-ready' : ' is-locked')}>
                            <Icon name={recipe.canCraft ? 'check' : 'lock'} size={10} />
                            {recipe.canCraft ? (recipe.maxCraftable > 1 ? `Up to ×${recipe.maxCraftable}` : 'Ready') : 'Locked'}
                          </span>
                        </span>
                      </button>
                    ) : (
                      <button
                        type="button"
                        key={recipe.recipeId}
                        aria-pressed={isSelected}
                        className={
                          'blacksmith-tile' +
                          (isSelected ? ' is-active' : '') +
                          (recipe.canCraft ? ' is-ready' : ' is-locked') +
                          (recipe.enchanted ? ' is-enchanted' : '')
                        }
                        onClick={() => setSelectedId(recipe.recipeId)}
                        onDoubleClick={() => {
                          if (recipe.canCraft && bridge && !busy && !obscured) void executeCraft(recipe, 1);
                        }}
                      >
                        <span className="blacksmith-tile__top">
                          <small>{recipe.category}</small>
                          <span>
                            {stat ? `${stat.value} ${stat.unit}` : `${recipe.value}g`}
                          </span>
                        </span>
                        <span className="blacksmith-tile__art">
                          <ItemThumbnail item={thumb} bridge={bridge} paused={obscured} />
                          {recipe.outputCount > 1 && <b className="blacksmith-tile__qty">×{recipe.outputCount}</b>}
                        </span>
                        <span className="blacksmith-tile__copy">
                          <strong>{recipe.name}</strong>
                          <span className={'blacksmith-status-chip' + (recipe.canCraft ? ' is-ready' : ' is-locked')}>
                            <Icon name={recipe.canCraft ? 'check' : 'lock'} size={10} />
                            {recipe.canCraft ? (recipe.maxCraftable > 1 ? `×${recipe.maxCraftable} max` : 'Ready') : 'Locked'}
                          </span>
                        </span>
                      </button>
                    );
                  })
                ) : (
                  <Empty icon={meta.icon} title={loading ? 'Loading station recipes…' : meta.emptyTitle} detail={meta.emptyDetail} />
                )}
              </div>
            </div>

            <div className="blacksmith-pagination">
              <span>
                {loading
                  ? 'Reading live station state…'
                  : totalFiltered
                    ? `${offset + 1}–${offset + (isTemperMode ? pagedTemper.length : pagedRecipes.length)} of ${totalFiltered.toLocaleString()}`
                    : '0 entries'}
              </span>
              <div>
                <button
                  type="button"
                  className="control-button"
                  disabled={offset === 0 || loading}
                  onClick={() => setOffset((v) => Math.max(0, v - PAGE_SIZE))}
                >
                  Previous
                </button>
                <button
                  type="button"
                  className="control-button"
                  disabled={offset + PAGE_SIZE >= totalFiltered || loading}
                  onClick={() => setOffset((v) => v + PAGE_SIZE)}
                >
                  Next
                </button>
              </div>
            </div>
          </section>

          <aside className="surface blacksmith-inspector" aria-label="Selected item details and requirements">
            {!isTemperMode && selectedRecipe ? (
              <CraftInspector
                recipe={selectedRecipe}
                actionVerb={meta.actionVerb}
                stationIcon={meta.icon}
                bridge={bridge}
                obscured={obscured}
                busy={busy}
                qty={clampedQty}
                maxBatch={maxBatch}
                showSlider={showQtySlider}
                onChangeQty={setCraftQuantity}
                onCraft={() => void executeCraft(selectedRecipe, showQtySlider ? clampedQty : 1)}
              />
            ) : isTemperMode && selectedTemper ? (
              <TemperInspector
                item={selectedTemper}
                stationKind={stationKind}
                actionVerb={meta.actionVerb}
                stationIcon={meta.icon}
                bridge={bridge}
                obscured={obscured}
                busy={busy}
                onTemper={() => void executeTemper(selectedTemper)}
              />
            ) : (
              <Empty
                icon={meta.icon}
                title="Select an Entry"
                detail={
                  isTemperMode
                    ? 'Choose an item from the list to preview tempering improvement, tier progression, and ingot requirements.'
                    : 'Choose a recipe from the list to inspect required ingots, leather, perks, and crafting stats.'
                }
              />
            )}

            <div className={'blacksmith-notice is-' + statusTone} role="status">
              <span>
                {statusMessage ||
                  (isTemperMode
                    ? `Select gear to ${meta.actionVerb.toLowerCase()} at ${stationTitle}. Enter confirms when ready.`
                    : `Select a recipe to ${meta.actionVerb.toLowerCase()} at ${stationTitle}. Double-click or press Enter when ready.`)}
              </span>
            </div>
          </aside>
        </div>
      )}
    </div>
  );
}

function MaterialsSection({
  materials,
  multiplier = 1,
  bridge,
  obscured,
}: {
  materials: SkyUltraBlacksmithMaterial[];
  multiplier?: number;
  bridge?: SkyWebViewAPI;
  obscured: boolean;
}): React.JSX.Element {
  return (
    <div className="blacksmith-section">
      <div className="blacksmith-section__head">
        <span className="eyebrow">REQUIRED MATERIALS</span>
        <small>{materials.length ? `${materials.filter((m) => m.owned >= m.required * multiplier).length}/${materials.length} MET` : 'NONE'}</small>
      </div>
      {materials.length ? (
        <div className="blacksmith-materials">
          {materials.map((mat) => {
            const need = Math.max(1, mat.required * multiplier);
            const hasEnough = mat.owned >= need;
            return (
              <div key={`${mat.id}:${mat.name}`} className={'blacksmith-material' + (hasEnough ? ' is-met' : ' is-missing')}>
                <span className="blacksmith-material__thumb" aria-hidden="true">
                  <ItemThumbnail item={toThumbItem(mat.id, mat.name, 'miscellaneous')} bridge={bridge} paused={obscured} />
                </span>
                <span className="blacksmith-material__name" title={mat.name}>
                  {mat.name}
                </span>
                <strong className="blacksmith-material__count">
                  {mat.owned.toLocaleString()} / {need.toLocaleString()}
                </strong>
              </div>
            );
          })}
        </div>
      ) : (
        <p className="blacksmith-muted-note">No raw materials required.</p>
      )}
    </div>
  );
}

function RequirementsSection({
  requirements,
  blockReason,
}: {
  requirements: SkyUltraBlacksmithRequirement[];
  blockReason?: string;
}): React.JSX.Element | null {
  if (!requirements.length && !blockReason) return null;
  return (
    <div className="blacksmith-section">
      <div className="blacksmith-section__head">
        <span className="eyebrow">PERKS & CONDITIONS</span>
        <small>{requirements.every((r) => r.met) && !blockReason ? 'UNLOCKED' : 'CHECK'}</small>
      </div>
      {requirements.length > 0 && (
        <div className="blacksmith-requirements">
          {requirements.map((req, idx) => (
            <div key={`${req.label}:${idx}`} className={'blacksmith-requirement' + (req.met ? ' is-met' : ' is-missing')}>
              <Icon name={req.met ? 'check' : 'lock'} size={12} />
              <span>{req.label}</span>
              {req.kind && <small>{req.kind.toUpperCase()}</small>}
            </div>
          ))}
        </div>
      )}
      {blockReason && (
        <div className="blacksmith-block-alert" role="alert">
          <Icon name="alertTriangle" size={13} />
          <span>{blockReason}</span>
        </div>
      )}
    </div>
  );
}

function CraftInspector({
  recipe,
  actionVerb,
  stationIcon,
  bridge,
  obscured,
  busy,
  qty,
  maxBatch,
  showSlider,
  onChangeQty,
  onCraft,
}: {
  recipe: SkyUltraBlacksmithRecipe;
  actionVerb: string;
  stationIcon: IconName;
  bridge?: SkyWebViewAPI;
  obscured: boolean;
  busy: boolean;
  qty: number;
  maxBatch: number;
  showSlider: boolean;
  onChangeQty: (next: number) => void;
  onCraft: () => void;
}): React.JSX.Element {
  const stat = recipePrimaryStat(recipe);
  const totalProduced = qty * Math.max(1, recipe.outputCount);
  const sliderPct = maxBatch > 1 ? Math.round(((qty - 1) / (maxBatch - 1)) * 100) : 100;
  return (
    <div className="blacksmith-inspector__inner">
      <div className="blacksmith-hero">
        <div className="blacksmith-hero__art">
          <ItemThumbnail item={toThumbItem(recipe.itemId, recipe.name, recipe.itemType || 'miscellaneous')} bridge={bridge} paused={obscured} />
          {recipe.outputCount > 1 && <span className="blacksmith-hero__qty">×{recipe.outputCount}</span>}
        </div>
        <div className="blacksmith-hero__copy">
          <span className="eyebrow">
            {recipe.weaponType || recipe.armorTypeLabel || recipe.category.toUpperCase()} · {formId(recipe.itemId)}
          </span>
          <h2 title={recipe.name}>{recipe.name}</h2>
          <div className="blacksmith-hero__pills">
            {stat && (
              <span className="blacksmith-metric-pill is-accent">
                <strong>{stat.value}</strong> <small>{stat.unit}</small>
              </span>
            )}
            <span className="blacksmith-metric-pill">
              <Icon name="gold" size={11} />
              <strong>{(recipe.value * Math.max(1, recipe.outputCount)).toLocaleString()}</strong>
            </span>
            <span className="blacksmith-metric-pill">
              <Icon name="weight" size={11} />
              <strong>{recipe.weight}</strong> <small>WT</small>
            </span>
            {typeof recipe.ownedCount === 'number' && (
              <span className="blacksmith-metric-pill">
                <strong>{recipe.ownedCount}</strong> <small>OWNED</small>
              </span>
            )}
          </div>
        </div>
      </div>

      <MaterialsSection materials={recipe.materials} multiplier={qty} bridge={bridge} obscured={obscured} />
      <RequirementsSection requirements={recipe.requirements} blockReason={recipe.canCraft ? undefined : recipe.blockReason} />

      {showSlider && (
        <div className="barter-quantity-slider blacksmith-qty-box" role="group" aria-label="Craft quantity">
          <div className="barter-quantity-slider__head">
            <span className="barter-quantity-slider__label">CRAFT BATCH</span>
            <div className="barter-quantity-slider__presets">
              <button type="button" className={qty === 1 ? 'is-active' : ''} onClick={() => onChangeQty(1)}>
                1
              </button>
              {maxBatch >= 4 && (
                <button
                  type="button"
                  className={qty === Math.floor(maxBatch / 2) ? 'is-active' : ''}
                  onClick={() => onChangeQty(Math.max(1, Math.floor(maxBatch / 2)))}
                >
                  Half ({Math.floor(maxBatch / 2)})
                </button>
              )}
              <button type="button" className={qty === maxBatch ? 'is-active' : ''} onClick={() => onChangeQty(maxBatch)}>
                Max ({maxBatch})
              </button>
            </div>
            <strong className="barter-quantity-slider__readout">
              {qty}× → {totalProduced} created
            </strong>
          </div>
          <div className="barter-quantity-slider__track-row">
            <button
              type="button"
              className="barter-quantity-slider__step"
              disabled={qty <= 1}
              onClick={() => onChangeQty(Math.max(1, qty - 1))}
              aria-label="Decrease craft count"
            >
              −
            </button>
            <input
              type="range"
              min={1}
              max={maxBatch}
              value={qty}
              aria-label="Select craft batch count"
              style={{ '--barter-slider-pct': `${sliderPct}%` } as React.CSSProperties}
              onChange={(e) => onChangeQty(Math.max(1, Math.min(maxBatch, Number(e.currentTarget.value) || 1)))}
            />
            <button
              type="button"
              className="barter-quantity-slider__step"
              disabled={qty >= maxBatch}
              onClick={() => onChangeQty(Math.min(maxBatch, qty + 1))}
              aria-label="Increase craft count"
            >
              +
            </button>
          </div>
        </div>
      )}

      <div className="blacksmith-actions">
        <button
          type="button"
          className={'control-button blacksmith-cta' + (recipe.canCraft ? ' is-active' : '')}
          disabled={!bridge || busy || obscured || !recipe.canCraft}
          onClick={onCraft}
        >
          <Icon name={stationIcon} size={15} />
          <span>
            {busy ? `${actionVerb}ing…` : `${actionVerb} ${recipe.name}${totalProduced > 1 ? ` (×${totalProduced})` : ''}`}
          </span>
        </button>
      </div>
    </div>
  );
}

function TemperInspector({
  item,
  stationKind,
  actionVerb,
  stationIcon,
  bridge,
  obscured,
  busy,
  onTemper,
}: {
  item: SkyUltraBlacksmithTemperItem;
  stationKind: SkyUltraBlacksmithStationKind;
  actionVerb: string;
  stationIcon: IconName;
  bridge?: SkyWebViewAPI;
  obscured: boolean;
  busy: boolean;
  onTemper: () => void;
}): React.JSX.Element {
  const unit = stationKind === 'grindstone' ? 'DAMAGE' : 'ARMOR RATING';
  const curStat = Math.round(item.currentStat);
  const nextStat = Math.round(item.projectedStat);
  const delta = nextStat - curStat;
  return (
    <div className="blacksmith-inspector__inner">
      <div className="blacksmith-hero">
        <div className="blacksmith-hero__art">
          <ItemThumbnail item={toThumbItem(item.itemId, item.name, item.itemType || 'weapon')} bridge={bridge} paused={obscured} />
          {item.enchanted && (
            <span className="blacksmith-hero__enchant" title={item.enchantmentName || 'Enchanted'}>
              <Icon name="flame" size={12} />
            </span>
          )}
        </div>
        <div className="blacksmith-hero__copy">
          <span className="eyebrow">
            {item.weaponType || item.armorTypeLabel || item.category.toUpperCase()} · {formId(item.itemId)}
          </span>
          <h2 title={item.name}>{item.name}</h2>
          <div className="blacksmith-hero__pills">
            {item.equipped && <span className="blacksmith-metric-pill is-accent">EQUIPPED</span>}
            {item.enchanted && (
              <span className="blacksmith-metric-pill is-arcane">
                <Icon name="flame" size={10} /> ENCHANTED
              </span>
            )}
            <span className="blacksmith-metric-pill">
              <Icon name="gold" size={11} />
              <strong>{item.value.toLocaleString()}</strong>
            </span>
            <span className="blacksmith-metric-pill">
              <Icon name="weight" size={11} />
              <strong>{item.weight}</strong> <small>WT</small>
            </span>
          </div>
        </div>
      </div>

      <div className="blacksmith-temper-card">
        <div className="blacksmith-temper-card__col">
          <small>CURRENT QUALITY</small>
          <strong>{item.currentTier || 'Untempered'}</strong>
          <span>
            {curStat} {unit}
          </span>
        </div>
        <div className="blacksmith-temper-card__arrow" aria-hidden="true">
          <Icon name="arrow" size={18} />
        </div>
        <div className="blacksmith-temper-card__col is-projected">
          <small>AFTER TEMPERING</small>
          <strong>{item.targetTier}</strong>
          <span>
            {nextStat} {unit}
            {delta > 0 && <em> (+{delta})</em>}
          </span>
        </div>
      </div>

      <MaterialsSection materials={item.materials} multiplier={1} bridge={bridge} obscured={obscured} />
      <RequirementsSection requirements={item.requirements} blockReason={item.canTemper ? undefined : item.blockReason} />

      <div className="blacksmith-actions">
        <button
          type="button"
          className={'control-button blacksmith-cta' + (item.canTemper ? ' is-active' : '')}
          disabled={!bridge || busy || obscured || !item.canTemper}
          onClick={onTemper}
        >
          <Icon name={stationIcon} size={15} />
          <span>{busy ? `Improving…` : `${actionVerb} ${item.name} → ${item.targetTier}`}</span>
        </button>
      </div>
    </div>
  );
}
