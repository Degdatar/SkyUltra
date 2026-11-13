import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
import {
  createSkyUltraGame,
  type SkyUltraContainerDisplayMode,
  type SkyUltraContainerSnapshot,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { ItemThumbnail } from './ItemThumbnail.js';
import { ItemCombatBadges } from './EquipmentPanel.js';
import { SkyChoice, SkySegmentedControl } from '../core.js';
import { Icon, itemIcon } from './icons.js';
import { Empty } from './pieces.js';
import { formId, type Item, type Player } from './model.js';

const pageSize = 48;
const categoryOrder = ['weapon', 'armor', 'potion', 'ingredient', 'ammo', 'book', 'scroll', 'soul gem', 'key', 'miscellaneous'];
const categoryLabels: Record<string, string> = {
  all: 'All items',
  weapon: 'Weapons',
  armor: 'Armor',
  potion: 'Potions',
  ingredient: 'Ingredients',
  ammo: 'Ammo',
  book: 'Books',
  scroll: 'Scrolls',
  'soul gem': 'Soul gems',
  key: 'Keys',
  miscellaneous: 'Misc.',
};

type ContainerSide = 'container' | 'player';
export type StorageContainerVariant = 'container' | 'loot' | 'pickpocket';

const displayModeLabels: Record<SkyUltraContainerDisplayMode, string> = {
  half: 'Half Container',
  windowed: 'Windowed',
  full: 'Full Screen',
};

function nextContainerDisplayMode(mode: SkyUltraContainerDisplayMode): SkyUltraContainerDisplayMode {
  return mode === 'half' ? 'windowed' : mode === 'windowed' ? 'full' : 'half';
}

export interface StorageContainerPageProps {
  player: Player | null;
  bridge?: SkyWebViewAPI;
  obscured?: boolean;
  variant?: StorageContainerVariant;
  displayMode?: SkyUltraContainerDisplayMode;
  pauseWorld?: boolean;
  onChangeDisplayMode?: (mode: SkyUltraContainerDisplayMode) => void;
  onTogglePauseWorld?: () => void;
  onPlayerChanged?: () => void;
  onClose?: () => void;
}

function isValidNamedItem(item: Item | null | undefined): item is Item {
  if (!item) return false;
  const trimmed = item.name?.trim() ?? '';
  if (!trimmed) return false;
  const lower = trimmed.toLowerCase();
  return !lower.startsWith('<missing name') && lower !== 'missing name';
}

export default function ContainerPage({
  player,
  bridge,
  obscured = false,
  variant = 'container',
  displayMode = 'half',
  pauseWorld = false,
  onChangeDisplayMode,
  onTogglePauseWorld,
  onPlayerChanged,
  onClose,
}: StorageContainerPageProps): React.JSX.Element {
  const game = useMemo(() => (bridge ? createSkyUltraGame(bridge) : null), [bridge]);
  const [side, setSide] = useState<ContainerSide>('container');
  const [snapshot, setSnapshot] = useState<SkyUltraContainerSnapshot | null>(null);
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('all');
  const [sort, setSort] = useState<'name' | 'value' | 'weight' | 'count'>('name');
  const [layout, setLayout] = useState<'grid' | 'list'>('grid');
  const [offset, setOffset] = useState(0);
  const [selectedId, setSelectedId] = useState('');
  const [transferQuantity, setTransferQuantity] = useState(1);
  const [sliderModalItem, setSliderModalItem] = useState<Item | null>(null);
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [statusMessage, setStatusMessage] = useState('');
  const [revision, setRevision] = useState(0);
  const locked = useRef(false);
  const actionController = useRef<AbortController | null>(null);

  useEffect(() => () => actionController.current?.abort(), [bridge]);
  useEffect(() => {
    setOffset(0);
  }, [side, query, category, sort]);

  useEffect(() => {
    if (!game || !bridge || obscured) return;
    let active = true;
    let pending = false;
    const controller = new AbortController();
    const refresh = async (): Promise<void> => {
      if (pending || document.hidden || !(bridge.getVisibility?.().visible ?? true)) return;
      pending = true;
      setLoading(prev => (snapshot ? prev : true));
      try {
        const res = await game.containerSnapshot({}, { signal: controller.signal });
        if (active) {
          setSnapshot(res);
          if (!res.available && res.message) {
            setStatusMessage(res.message);
          }
        }
      } catch (error) {
        if (active) {
          setStatusMessage(error instanceof Error ? error.message : 'Could not read container inventory.');
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
    const unsubscribeVis = bridge.on('skyultra.visibility', onVis);
    const unsubscribeContainer = bridge.on('skyultra.containerMode', onVis);
    const unsubscribeLoot = bridge.on('skyultra.lootMode', onVis);
    const unsubscribePickpocket = bridge.on('skyultra.pickpocketMode', onVis);
    document.addEventListener('visibilitychange', onVis);
    return () => {
      active = false;
      controller.abort();
      unsubscribeVis();
      unsubscribeContainer();
      unsubscribeLoot();
      unsubscribePickpocket();
      document.removeEventListener('visibilitychange', onVis);
    };
  }, [game, bridge, obscured, revision, variant]);

  const rawContainerItems = useMemo(
    () => (snapshot?.containerItems ?? []).filter(isValidNamedItem),
    [snapshot?.containerItems],
  );
  const rawPlayerItems = useMemo(() => {
    const source = snapshot?.available ? snapshot.playerItems : player?.inventory ?? [];
    const list = source.filter(isValidNamedItem);
    const fallbackGold = snapshot?.playerGold ?? player?.gold ?? 0;
    if (fallbackGold > 0 && !list.some(item => item.id === '15')) {
      list.unshift({
        id: '15',
        name: 'Gold',
        category: 'miscellaneous',
        count: fallbackGold,
        value: 1,
        weight: 0,
        equipped: false,
      });
    }
    return list;
  }, [snapshot, player?.inventory, player?.gold]);

  const activeSourceItems = side === 'container' ? rawContainerItems : rawPlayerItems;

  const categories = useMemo(() => {
    const set = new Set<string>();
    for (const item of activeSourceItems) set.add(item.category);
    const sorted = [...set].sort((a, b) => {
      const ai = categoryOrder.indexOf(a);
      const bi = categoryOrder.indexOf(b);
      return (ai < 0 ? 99 : ai) - (bi < 0 ? 99 : bi) || a.localeCompare(b);
    });
    return ['all', ...sorted];
  }, [activeSourceItems]);

  useEffect(() => {
    if (category !== 'all' && !categories.includes(category)) {
      setCategory('all');
    }
  }, [categories, category]);

  const filtered = useMemo(() => {
    const trimmed = query.trim().toLowerCase();
    return activeSourceItems
      .filter(item => category === 'all' || item.category === category)
      .filter(
        item =>
          !trimmed ||
          `${item.name} ${item.category} ${item.armorTypeLabel ?? ''} ${item.weaponType ?? ''} ${item.id} ${formId(item.id)}`
            .toLowerCase()
            .includes(trimmed),
      )
      .sort((a, b) =>
        sort === 'value'
          ? b.value - a.value
          : sort === 'weight'
            ? b.weight - a.weight
            : sort === 'count'
              ? b.count - a.count
              : a.name.localeCompare(b.name),
      );
  }, [activeSourceItems, category, query, sort]);

  const pagedItems = useMemo(() => filtered.slice(offset, offset + pageSize), [filtered, offset]);
  const selected = useMemo(
    () =>
      pagedItems.find(item => item.id === selectedId) ??
      filtered.find(item => item.id === selectedId) ??
      pagedItems[0] ??
      null,
    [pagedItems, filtered, selectedId],
  );

  useEffect(() => {
    setTransferQuantity(1);
    if (sliderModalItem && selected?.id !== sliderModalItem.id) {
      setSliderModalItem(null);
    }
  }, [selected?.id, side]);

  const effectiveVariant: StorageContainerVariant =
    variant === 'loot' || variant === 'pickpocket'
      ? variant
      : snapshot?.containerType === 'corpse'
        ? 'loot'
        : snapshot?.containerMode === 'pickpocket' ||
            (snapshot?.containerType === 'actor' && !snapshot?.isMannequin && !snapshot?.actor?.dead)
          ? 'pickpocket'
          : 'container';

  const isPickpocket = effectiveVariant === 'pickpocket';
  const isCorpseLoot = effectiveVariant === 'loot';
  const isStealing = isPickpocket || snapshot?.containerMode === 'steal' || snapshot?.containerMode === 'pickpocket';

  const containerName =
    snapshot?.containerName ||
    (isPickpocket ? 'Targeted NPC' : isCorpseLoot ? 'Fallen Target' : 'Storage Container');
  const playerName = snapshot?.playerName || player?.playerName || 'Dragonborn';
  const carryCurrent = Math.round(snapshot?.playerCarryWeight?.current ?? player?.carryWeight?.current ?? 0);
  const carryMax = Math.round(snapshot?.playerCarryWeight?.max ?? player?.carryWeight?.max ?? 300);
  const playerGold =
    rawPlayerItems.find(item => item.id === '15')?.count ?? snapshot?.playerGold ?? player?.gold ?? 0;
  const containerGold = rawContainerItems.find(item => item.id === '15')?.count ?? 0;

  const takeVerb = isPickpocket ? 'Steal' : 'Take';
  const storeVerb = isPickpocket ? 'Plant' : 'Store';
  const activeVerb = side === 'container' ? takeVerb : storeVerb;
  const bulkTakeLabel = isPickpocket ? 'Steal All' : 'Take All';
  const bulkStoreLabel = isPickpocket ? 'Plant All' : 'Store All';

  const maxQuantity = Math.max(1, selected?.count ?? 1);
  const showQuantitySlider = Boolean(selected && selected.count > 5);
  const clampedQuantity = Math.max(1, Math.min(maxQuantity, transferQuantity));
  const sliderPercent = maxQuantity > 1 ? Math.round(((clampedQuantity - 1) / (maxQuantity - 1)) * 100) : 100;

  async function executeTransfer(item: Item, count: number): Promise<void> {
    if (!game || obscured || locked.current) return;
    const mode = side === 'container' ? (isPickpocket ? 'steal' : 'take') : isPickpocket ? 'plant' : 'store';
    const isTaking = side === 'container';
    const qty = Math.max(1, Math.min(item.count, count));
    locked.current = true;
    setBusy(true);
    setSliderModalItem(null);
    const progressVerb = isTaking ? (isPickpocket ? 'Stealing' : 'Taking') : isPickpocket ? 'Planting' : 'Storing';
    setStatusMessage(`${progressVerb} ${qty > 1 ? `${qty.toLocaleString()}× ` : ''}${item.name}…`);
    // Optimistically update the displayed lists for non-pickpocket transfers so single-click feels instantaneous.
    if (!isPickpocket) {
      setSnapshot(prev => {
        if (!prev) return prev;
        const baseId = item.id.split(':')[0] ?? item.id;
        const adjustList = (list: Item[]): Item[] =>
          list
            .map(entry => (entry.id === item.id ? { ...entry, count: entry.count - qty } : entry))
            .filter(entry => entry.count > 0);
        const mergeIntoList = (list: Item[], markStolen: boolean): Item[] => {
          const targetId = markStolen ? `${baseId}:stolen` : baseId;
          const existingIdx = list.findIndex(entry => entry.id === targetId);
          if (existingIdx >= 0) {
            return list.map((entry, idx) =>
              idx === existingIdx
                ? { ...entry, count: entry.count + qty, stolen: markStolen }
                : entry,
            );
          }
          return [
            ...list,
            {
              ...item,
              id: targetId,
              count: qty,
              equipped: false,
              stolen: markStolen,
            },
          ];
        };
        const markStolen = isTaking && isStealing && baseId !== '15';
        return isTaking
          ? {
              ...prev,
              containerItems: adjustList(prev.containerItems ?? []),
              playerItems: mergeIntoList(prev.playerItems ?? [], markStolen),
            }
          : {
              ...prev,
              playerItems: adjustList(prev.playerItems ?? []),
              containerItems: mergeIntoList(prev.containerItems ?? [], false),
            };
      });
    }
    actionController.current?.abort();
    const controller = new AbortController();
    actionController.current = controller;
    try {
      const res = await game.containerTransfer(
        {
          mode,
          formId: item.id,
          count: qty,
          containerRefId: snapshot?.containerRefId,
        },
        { signal: controller.signal },
      );
      if (controller.signal.aborted) return;
      if (res.caught || res.closeMenu) {
        if (res.container && res.container.available) {
          setSnapshot(res.container);
        }
        onPlayerChanged?.();
        setStatusMessage(res.message ?? 'You have been caught pickpocketing!');
        onClose?.();
        return;
      }
      if (!res.accepted) {
        setStatusMessage(res.message ?? 'Skyrim declined that container transfer.');
        setRevision(v => v + 1);
        return;
      }
      if (res.container && res.container.available) {
        setSnapshot(res.container);
      }
      onPlayerChanged?.();
      const doneVerb = isTaking ? (isPickpocket ? 'Stole' : 'Took') : isPickpocket ? 'Planted' : 'Stored';
      setStatusMessage(res.message ?? `${doneVerb} ${item.name}.`);
      setTransferQuantity(1);
    } catch (error) {
      if (!controller.signal.aborted) {
        setStatusMessage(error instanceof Error ? error.message : 'Container transfer failed.');
        setRevision(v => v + 1);
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  async function executeBulkTransfer(bulkKind: 'takeAll' | 'storeAll'): Promise<void> {
    if (!game || obscured || locked.current) return;
    const bulkMode =
      bulkKind === 'takeAll'
        ? isPickpocket
          ? 'stealAll'
          : 'takeAll'
        : isPickpocket
          ? 'plantAll'
          : 'storeAll';
    locked.current = true;
    setBusy(true);
    setSliderModalItem(null);
    setStatusMessage(
      bulkKind === 'takeAll'
        ? `${isPickpocket ? 'Stealing' : 'Taking'} all items from ${containerName}…`
        : `${isPickpocket ? 'Planting' : 'Storing'} all unequipped items ${isPickpocket ? 'on' : 'into'} ${containerName}…`,
    );
    actionController.current?.abort();
    const controller = new AbortController();
    actionController.current = controller;
    try {
      const res = await game.containerTransfer(
        {
          mode: bulkMode,
          containerRefId: snapshot?.containerRefId,
          category: bulkKind === 'storeAll' && category !== 'all' ? category : 'all',
        },
        { signal: controller.signal },
      );
      if (controller.signal.aborted) return;
      if (res.caught || res.closeMenu) {
        if (res.container && res.container.available) {
          setSnapshot(res.container);
        }
        onPlayerChanged?.();
        setStatusMessage(res.message ?? 'You have been caught pickpocketing!');
        onClose?.();
        return;
      }
      if (!res.accepted) {
        setStatusMessage(res.message ?? 'Skyrim declined that bulk transfer.');
        return;
      }
      if (res.container && res.container.available) {
        setSnapshot(res.container);
      }
      onPlayerChanged?.();
      setStatusMessage(
        res.message ??
          (bulkKind === 'takeAll'
            ? `${isPickpocket ? 'Stole' : 'Took'} all items.`
            : `${isPickpocket ? 'Planted' : 'Stored'} all items.`),
      );
    } catch (error) {
      if (!controller.signal.aborted) {
        setStatusMessage(error instanceof Error ? error.message : 'Bulk transfer failed.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  const handleItemClick = (item: Item): void => {
    setSelectedId(item.id);
    if (busy || obscured) return;
    const isGold = item.id === '15';
    const qty =
      selected?.id === item.id && showQuantitySlider
        ? clampedQuantity
        : isGold
          ? item.count
          : 1;
    void executeTransfer(item, qty);
  };

  useEffect(() => {
    const onKey = (event: KeyboardEvent): void => {
      if (busy || obscured || event.ctrlKey || event.altKey || event.metaKey) return;
      if ((event.target as HTMLElement | null)?.closest('input,textarea,select,[contenteditable=true]')) return;
      const key = event.key.toLowerCase();
      if (key === 'r') {
        event.preventDefault();
        if (side === 'container') void executeBulkTransfer('takeAll');
        else void executeBulkTransfer('storeAll');
      } else if (key === 'enter' && selected) {
        event.preventDefault();
        void executeTransfer(selected, selected.count > 5 ? clampedQuantity : 1);
      } else if (key === 'tab') {
        event.preventDefault();
        setSide(s => (s === 'container' ? 'player' : 'container'));
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  const selectedEnchanted = Boolean(selected && (selected.enchanted || selected.enchantments?.length));
  const selectedEnchantCount = selected?.enchantments?.length ?? 0;
  const actorMeta = snapshot?.actor;

  return (
    <div
      className={`page inventory-page inventory-page--wardrobe inventory-page--armory inventory-page--standalone barter-page container-page container-page--${effectiveVariant} barter-page--${displayMode}`}
      data-barter-mode={displayMode}
      data-container-mode={displayMode}
      data-container-variant={effectiveVariant}
    >
      <header className="armory-masthead barter-masthead container-masthead">
        <div className="armory-heading">
          <span className="eyebrow">
            {side === 'container'
              ? isPickpocket
                ? `PICKPOCKETING TARGET · STEAL FROM ${containerName.toUpperCase()}`
                : isCorpseLoot
                  ? `CORPSE & MONSTER LOOT · ${containerName.toUpperCase()}`
                  : `${isStealing ? 'STEALING FROM' : 'STORAGE CONTAINER'} · ${containerName.toUpperCase()}`
              : isPickpocket
                ? `PLAYER BELONGINGS · PLANT ON ${containerName.toUpperCase()}`
                : `PLAYER BELONGINGS · STORING INTO ${containerName.toUpperCase()}`}
            {actorMeta && (
              <em className={`container-variant-badge container-variant-badge--${effectiveVariant}`}>
                {isPickpocket ? 'PICKPOCKET' : isCorpseLoot ? 'DECEASED' : 'TARGET'}
                {actorMeta.level ? ` · LVL ${actorMeta.level}` : ''}
                {actorMeta.race ? ` · ${actorMeta.race.toUpperCase()}` : ''}
              </em>
            )}
          </span>
          <h1>{side === 'container' ? containerName : `${playerName}'s Pack`}</h1>
        </div>

        <div className="armory-masthead__end barter-masthead__end">
          <div className="armory-mode-pills" role="group" aria-label="Container layout and world state">
            {onChangeDisplayMode && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (displayMode !== 'half' ? ' is-active' : '')}
                onClick={() => onChangeDisplayMode(nextContainerDisplayMode(displayMode))}
                title={`Cycle Container Layout: Half Container → Windowed → Full Screen (Current: ${displayModeLabels[displayMode]})`}
              >
                <Icon name="widgets" size={12} />
                <span>{displayModeLabels[displayMode]}</span>
              </button>
            )}
            {onTogglePauseWorld && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (pauseWorld ? ' is-active' : '')}
                onClick={onTogglePauseWorld}
                aria-pressed={!!pauseWorld}
                title={`Toggle Skyrim World State: Live World / Pause World (Current: ${pauseWorld ? 'Pause World' : 'Live World'})`}
              >
                <Icon name="clock" size={12} />
                <span>{pauseWorld ? 'Pause World' : 'Live World'}</span>
              </button>
            )}
          </div>

          <div className="barter-carry-pill" title="Player Carry Weight">
            <Icon name="inventory" size={12} />
            <strong>
              {carryCurrent} / {carryMax}
            </strong>
            <small>WT</small>
          </div>

          {onClose && (
            <button type="button" className="control-button armory-close-btn" onClick={onClose} aria-label="Close container menu">
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          )}
        </div>
      </header>

      <div className="inventory-outfit-workbench barter-workbench container-workbench">
        <section
          className="surface inventory-surface barter-surface container-surface"
          aria-label={isPickpocket ? 'Pickpocketing Container' : isCorpseLoot ? 'Corpse Loot Container' : 'Storage Container'}
          aria-busy={loading || busy}
        >
          <div className="inventory-toolbar">
            <label className="page-search">
              <Icon name="search" size={16} />
              <input
                aria-label="Search container items"
                value={query}
                onChange={e => setQuery(e.currentTarget.value)}
                placeholder={
                  side === 'container'
                    ? `Search ${containerName} to ${takeVerb.toLowerCase()}…`
                    : `Search your items to ${storeVerb.toLowerCase()}…`
                }
              />
              {query ? (
                <button type="button" aria-label="Clear search" onClick={() => setQuery('')}>
                  <Icon name="close" size={12} />
                </button>
              ) : (
                <kbd>Space</kbd>
              )}
            </label>

            <div className="container-bulk-actions" role="group" aria-label="Bulk container actions">
              <button
                type="button"
                className={
                  'control-button is-active container-bulk-btn' +
                  (isPickpocket ? ' container-bulk-btn--steal' : isCorpseLoot ? ' container-bulk-btn--loot' : '')
                }
                disabled={!bridge || busy || obscured || rawContainerItems.length === 0}
                onClick={() => void executeBulkTransfer('takeAll')}
                title={`${bulkTakeLabel} (${rawContainerItems.length} item types) from ${containerName} (R)`}
              >
                <Icon name={isPickpocket ? 'target' : 'inventory'} size={13} />
                <span>
                  {bulkTakeLabel} ({rawContainerItems.length})
                </span>
              </button>
              <button
                type="button"
                className={'control-button container-bulk-btn' + (isPickpocket ? ' container-bulk-btn--plant' : '')}
                disabled={!bridge || busy || obscured || rawPlayerItems.length === 0}
                onClick={() => void executeBulkTransfer('storeAll')}
                title={`${bulkStoreLabel} unequipped items${category !== 'all' ? ` in ${categoryLabels[category] ?? category}` : ''} ${isPickpocket ? 'on' : 'into'} ${containerName}`}
              >
                <Icon name="arrow" size={13} />
                <span>
                  {bulkStoreLabel}
                  {category !== 'all' ? ` (${categoryLabels[category] ?? category})` : ''}
                </span>
              </button>
            </div>

            <SkySegmentedControl
              label="Container layout"
              value={layout}
              onValueChange={setLayout}
              options={[
                { value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' },
                { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' },
              ]}
            />
          </div>

          <div className="pack-categories" role="group" aria-label="Container item categories">
            {categories.map(value => (
              <button
                type="button"
                key={value}
                aria-pressed={category === value}
                onClick={() => setCategory(value)}
                title={categoryLabels[value] ?? value}
              >
                <Icon name={value === 'all' ? 'inventory' : itemIcon(value)} size={18} />
                <span>{categoryLabels[value] ?? value}</span>
              </button>
            ))}
          </div>

          <div className="inventory-filters">
            <SkyChoice
              label="Sort container items"
              value={sort}
              onValueChange={v => setSort(v as 'name' | 'value' | 'weight' | 'count')}
              options={[
                { value: 'name', label: 'Name A–Z' },
                { value: 'value', label: 'Highest value' },
                { value: 'weight', label: 'Highest weight' },
                { value: 'count', label: 'Largest stack' },
              ]}
            />
            <span className="pack-filter-state">
              {categoryLabels[category] ?? category}
              <i />
              {side === 'container'
                ? `${containerName} · Click item to ${takeVerb}`
                : `Your Pack · Click item to ${storeVerb}`}
            </span>
          </div>

          <div className={'inventory-results inventory-results--' + layout}>
            {pagedItems.length ? (
              pagedItems.map(item => {
                const isEnchanted = Boolean(item.enchanted || item.enchantments?.length);
                const enchantSummary = item.enchantments?.length
                  ? item.enchantments.map(e => (e.description ? `${e.name}: ${e.description}` : e.name)).join(' · ')
                  : item.enchantmentName || 'Enchanted';
                const isGoldItem = item.id === '15';
                const isStolen = side === 'player' && Boolean(item.stolen) && !isGoldItem;
                return (
                  <button
                    type="button"
                    key={item.id}
                    aria-pressed={item.id === selected?.id}
                    title={
                      (isEnchanted ? `${item.name} (${enchantSummary})` : item.name) +
                      (isStolen ? ' [STOLEN]' : '') +
                      ` — Click to ${activeVerb}${item.count > 1 ? ' (double-click for full stack)' : ''}`
                    }
                    data-category={item.category}
                    data-enchanted={isEnchanted ? 'true' : undefined}
                    data-stolen={isStolen ? 'true' : undefined}
                    data-gold={isGoldItem ? 'true' : undefined}
                    className={
                      'item-tile' +
                      (item.id === selected?.id ? ' is-active' : '') +
                      (item.equipped ? ' is-equipped' : '') +
                      (isEnchanted ? ' is-enchanted' : '') +
                      (isStolen ? ' is-stolen' : '')
                    }
                    onMouseEnter={() => {
                      if (!busy && selectedId !== item.id) setSelectedId(item.id);
                    }}
                    onClick={() => handleItemClick(item)}
                    onDoubleClick={() => {
                      if (!busy && !obscured) void executeTransfer(item, item.count);
                    }}
                  >
                    <span className="item-tile__top">
                      <small>
                        {item.equipped ? (
                          <span className="item-tile__equipped-tag">
                            <Icon name="check" size={10} />
                            EQUIPPED
                          </span>
                        ) : isStolen ? (
                          <span className="item-tile__stolen-tag">
                            <Icon name="stolen" size={10} />
                            STOLEN
                          </span>
                        ) : isGoldItem ? (
                          'SEPTIMS'
                        ) : (
                          item.armorTypeLabel || item.weaponType || item.category
                        )}
                      </small>
                      <span>
                        {isEnchanted && (
                          <em className="item-tile__enchant-mark" title={enchantSummary}>
                            <Icon name="flame" size={10} />
                          </em>
                        )}
                        {item.value.toLocaleString()}
                        <Icon name="gold" size={10} />
                      </span>
                    </span>
                    <span className="item-tile__image">
                      <ItemThumbnail item={item} bridge={bridge} paused={obscured} />
                      {isEnchanted && (
                        <span className="item-tile__enchant-badge" title={enchantSummary} aria-label="Enchanted item">
                          <Icon name="flame" size={11} />
                        </span>
                      )}
                      {item.equipped && (
                        <span className="item-tile__worn-pill" title={side === 'container' ? `Equipped by ${containerName}` : 'Equipped by you'}>
                          EQUIPPED
                        </span>
                      )}
                      {isStolen && (
                        <span className="item-tile__stolen-pill" title="Stolen item">
                          <Icon name="stolen" size={10} />
                          STOLEN
                        </span>
                      )}
                      {item.count > 1 && <b className="item-tile__quantity">×{item.count.toLocaleString()}</b>}
                    </span>
                    <span className="item-tile__copy">
                      <strong>
                        {isEnchanted && <Icon name="flame" size={10} />}
                        {item.name}
                      </strong>
                      <small>
                        {item.equipped ? 'Equipped · ' : ''}
                        {isStolen ? 'Stolen · ' : ''}
                        {item.armorTypeLabel || item.weaponType || item.category}
                        <ItemCombatBadges item={item} compact />
                        <span>{item.weight} wt</span>
                      </small>
                    </span>
                  </button>
                );
              })
            ) : (
              <Empty
                title={
                  loading
                    ? 'Reading container contents'
                    : side === 'container'
                      ? `${containerName} is empty`
                      : 'No items matched'
                }
                detail={
                  side === 'container'
                    ? `Switch to your Player Inventory tab below to ${storeVerb.toLowerCase()} items ${isPickpocket ? 'on' : 'in'} ${containerName}.`
                    : 'Try another category or switch back to the target tab.'
                }
                icon="inventory"
              />
            )}
          </div>

          <div className="roster-footer inventory-pagination">
            <span>
              {loading
                ? 'Synchronizing container…'
                : filtered.length
                  ? `${offset + 1}–${offset + pagedItems.length} of ${filtered.length.toLocaleString()} (${side === 'container' ? containerName : 'Your Pack'})`
                  : '0 items'}
            </span>
            <div>
              <button
                type="button"
                className="control-button"
                disabled={offset === 0 || loading}
                onClick={() => setOffset(v => Math.max(0, v - pageSize))}
              >
                Previous
              </button>
              <button
                type="button"
                className="control-button"
                disabled={offset + pagedItems.length >= filtered.length || loading}
                onClick={() => setOffset(v => v + pagedItems.length)}
              >
                Next
              </button>
            </div>
          </div>

          <div
            className={
              'equipment-selection barter-selection' +
              (selected?.equipped ? ' is-equipped' : '') +
              (selectedEnchanted ? ' is-enchanted' : '')
            }
            aria-label="Selected container item"
          >
            <div className="selection-art" aria-hidden="true">
              {selected ? <ItemThumbnail item={selected} bridge={bridge} paused={obscured} /> : <Icon name="inventory" size={28} />}
              {selected && selectedEnchanted && (
                <span className="selection-art__enchant-badge" title="Enchanted item">
                  <Icon name="flame" size={12} />
                </span>
              )}
            </div>

            <div className="selection-copy">
              <div className="selection-copy__top">
                <span className="eyebrow">
                  {selected
                    ? side === 'container'
                      ? isPickpocket
                        ? `STEAL FROM ${containerName.toUpperCase()}${selected.equipped ? ' · EQUIPPED' : ''}`
                        : isCorpseLoot
                          ? `LOOT FROM ${containerName.toUpperCase()}${selected.equipped ? ' · EQUIPPED' : ''}`
                          : `${isStealing ? 'STEAL FROM' : 'STORED IN'} ${containerName.toUpperCase()}`
                      : isPickpocket
                        ? `CARRIED BY ${playerName.toUpperCase()} · READY TO PLANT ON ${containerName.toUpperCase()}`
                        : `CARRIED BY ${playerName.toUpperCase()} · READY TO STORE`
                    : side === 'container'
                      ? containerName.toUpperCase()
                      : 'PLAYER INVENTORY'}
                  {selected?.equipped && (
                    <em className="selection-equipped-pill">
                      <Icon name="check" size={10} />
                      EQUIPPED
                    </em>
                  )}
                  {side === 'player' && selected?.stolen && (
                    <em className="selection-stolen-pill">
                      <Icon name="stolen" size={10} />
                      STOLEN
                    </em>
                  )}
                  {selectedEnchanted && (
                    <em className="selection-enchant-pill">
                      <Icon name="flame" size={10} />
                      ENCHANTED
                      {selected?.enchantmentCharge !== undefined ? ` · ${selected.enchantmentCharge}%` : ''}
                      {selectedEnchantCount > 1 ? ` · ${selectedEnchantCount} EFFECTS` : ''}
                    </em>
                  )}
                </span>
                {selected && (
                  <div className="selection-properties">
                    {typeof selected.damage === 'number' && selected.damage > 0 && (
                      <span>
                        <Icon name="target" size={11} />
                        <strong>{Math.round(selected.damage)}</strong>
                        <small>DMG</small>
                      </span>
                    )}
                    {typeof selected.armorRating === 'number' && selected.armorRating > 0 && (
                      <span>
                        <Icon name="shield" size={11} />
                        <strong>{Math.round(selected.armorRating)}</strong>
                        <small>{selected.armorWeightClass === 'heavy' ? 'HEAVY AR' : selected.armorWeightClass === 'light' ? 'LIGHT AR' : 'ARMOR'}</small>
                      </span>
                    )}
                    <span>
                      <Icon name="gold" size={11} />
                      <strong>{selected.value.toLocaleString()}</strong>
                      <small>VALUE</small>
                    </span>
                    <span>
                      <strong>{selected.weight}</strong>
                      <small>WEIGHT</small>
                    </span>
                    <span>
                      <strong>{selected.count.toLocaleString()}</strong>
                      <small>IN STACK</small>
                    </span>
                  </div>
                )}
              </div>

              <div className="selection-copy__headline">
                <h3 title={selected?.name}>
                  {selectedEnchanted && <Icon name="flame" size={13} />}
                  {selected?.name ?? `Click any item to ${activeVerb.toLowerCase()} it`}
                </h3>
                <span className="selection-category">
                  {selected
                    ? `${selected.equipped ? 'Equipped · ' : ''}${selected.armorTypeLabel || selected.weaponType || selected.category}${selected.speed ? ` · Speed ${selected.speed}` : ''}${selected.enchantmentName ? ` · ${selected.enchantmentName}` : ''}${selected.teachesSpell ? ` · Teaches ${selected.teachesSpell}` : ''}`
                    : `Click an item to ${activeVerb.toLowerCase()} immediately (stacks above 5 open the quantity slider)`}
                </span>
              </div>

              {selected && showQuantitySlider && (
                <div className="barter-quantity-slider" role="group" aria-label="Transfer quantity selector">
                  <div className="barter-quantity-slider__head">
                    <span className="barter-quantity-slider__label">
                      QUANTITY TO {activeVerb.toUpperCase()}
                    </span>
                    <div className="barter-quantity-slider__presets">
                      <button type="button" className={clampedQuantity === 1 ? 'is-active' : ''} onClick={() => setTransferQuantity(1)}>
                        1
                      </button>
                      <button
                        type="button"
                        className={clampedQuantity === Math.max(1, Math.floor(maxQuantity / 2)) ? 'is-active' : ''}
                        onClick={() => setTransferQuantity(Math.max(1, Math.floor(maxQuantity / 2)))}
                      >
                        Half ({Math.max(1, Math.floor(maxQuantity / 2)).toLocaleString()})
                      </button>
                      <button
                        type="button"
                        className={clampedQuantity === maxQuantity ? 'is-active' : ''}
                        onClick={() => setTransferQuantity(maxQuantity)}
                      >
                        All ({maxQuantity.toLocaleString()})
                      </button>
                    </div>
                    <strong className="barter-quantity-slider__readout">
                      ×{clampedQuantity.toLocaleString()} · {(selected.weight * clampedQuantity).toFixed(1)} wt
                    </strong>
                  </div>
                  <div className="barter-quantity-slider__track-row">
                    <button
                      type="button"
                      className="barter-quantity-slider__step"
                      disabled={clampedQuantity <= 1}
                      onClick={() => setTransferQuantity(q => Math.max(1, q - 1))}
                      aria-label="Decrease quantity"
                    >
                      −
                    </button>
                    <input
                      type="range"
                      className="barter-quantity-slider__input"
                      min={1}
                      max={maxQuantity}
                      value={clampedQuantity}
                      style={{ '--slider-pct': `${sliderPercent}%` } as React.CSSProperties}
                      onChange={e => setTransferQuantity(Number(e.currentTarget.value))}
                      aria-label={`Quantity to ${activeVerb.toLowerCase()}`}
                    />
                    <button
                      type="button"
                      className="barter-quantity-slider__step"
                      disabled={clampedQuantity >= maxQuantity}
                      onClick={() => setTransferQuantity(q => Math.min(maxQuantity, q + 1))}
                      aria-label="Increase quantity"
                    >
                      +
                    </button>
                  </div>
                </div>
              )}
            </div>

            {selected && (
              <div className="equipment-selection__actions barter-selection__actions">
                <button
                  type="button"
                  className={
                    'control-button is-active barter-trade-btn' +
                    (isPickpocket && side === 'container'
                      ? ' container-action-btn--steal'
                      : isPickpocket && side === 'player'
                        ? ' container-action-btn--plant'
                        : '')
                  }
                  disabled={!bridge || busy || obscured}
                  onClick={() => void executeTransfer(selected, showQuantitySlider ? clampedQuantity : 1)}
                >
                  <Icon name={side === 'container' ? (isPickpocket ? 'target' : 'inventory') : 'arrow'} size={13} />
                  <span>
                    {activeVerb}
                    {showQuantitySlider && clampedQuantity > 1 ? ` ×${clampedQuantity.toLocaleString()}` : ''}
                  </span>
                </button>
                {selected.count > 1 && (
                  <button
                    type="button"
                    className="control-button"
                    disabled={!bridge || busy || obscured}
                    onClick={() => void executeTransfer(selected, selected.count)}
                  >
                    <span>
                      {activeVerb} Stack (×{selected.count.toLocaleString()})
                    </span>
                  </button>
                )}
              </div>
            )}
          </div>

          <div className="inventory-notice barter-notice" role="status">
            <span>
              {statusMessage ||
                (side === 'container'
                  ? `Click any item (including Gold) to ${takeVerb.toLowerCase()} it from ${containerName} · Click ${playerName} below to ${storeVerb.toLowerCase()} items.`
                  : `Click any item (including Gold) to ${storeVerb.toLowerCase()} it ${isPickpocket ? 'on' : 'in'} ${containerName} · Click ${containerName} below to ${takeVerb.toLowerCase()} items.`)}
            </span>
          </div>

          <footer className="barter-trader-switcher" role="tablist" aria-label="Switch between Target and Player inventories">
            <button
              type="button"
              role="tab"
              aria-selected={side === 'container'}
              className={'barter-trader-tab' + (side === 'container' ? ' is-active' : '')}
              onClick={() => setSide('container')}
            >
              <div className="barter-trader-tab__identity">
                <span className="barter-trader-tab__role">
                  {isPickpocket
                    ? `TARGETED NPC · CLICK ITEM TO STEAL`
                    : isCorpseLoot
                      ? `FALLEN TARGET · CLICK ITEM TO TAKE`
                      : `STORAGE CONTAINER · CLICK ITEM TO TAKE`}
                </span>
                <strong className="barter-trader-tab__name">{containerName}</strong>
              </div>
              <div className="barter-trader-tab__meta">
                <span className="barter-trader-tab__count">{rawContainerItems.length.toLocaleString()} items</span>
                <span className="barter-trader-tab__gold" title={containerGold > 0 ? `${containerGold.toLocaleString()} Gold` : undefined}>
                  <Icon name={containerGold > 0 ? 'gold' : 'inventory'} size={14} />
                  <b>
                    {containerGold > 0
                      ? containerGold.toLocaleString()
                      : isPickpocket
                        ? 'PICKPOCKET'
                        : isCorpseLoot
                          ? 'CORPSE'
                          : (snapshot?.containerType?.toUpperCase() ?? 'CHEST')}
                  </b>
                </span>
              </div>
            </button>

            <button
              type="button"
              role="tab"
              aria-selected={side === 'player'}
              className={'barter-trader-tab' + (side === 'player' ? ' is-active' : '')}
              onClick={() => setSide('player')}
            >
              <div className="barter-trader-tab__identity">
                <span className="barter-trader-tab__role">
                  {isPickpocket
                    ? `PLAYER INVENTORY · CLICK ITEM TO PLANT`
                    : `PLAYER INVENTORY · CLICK ITEM TO STORE`}
                </span>
                <strong className="barter-trader-tab__name">{playerName}</strong>
              </div>
              <div className="barter-trader-tab__meta">
                <span className="barter-trader-tab__count">{rawPlayerItems.length.toLocaleString()} items</span>
                <span className="barter-trader-tab__gold" title="Player Gold">
                  <Icon name="gold" size={14} />
                  <b>{playerGold.toLocaleString()}</b>
                </span>
              </div>
            </button>
          </footer>
        </section>
      </div>
    </div>
  );
}
