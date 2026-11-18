import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
import { createSkyUltraGame, type SkyUltraEquipmentSlot, type SkyUltraInventoryOptions, type SkyUltraInventoryPage, type SkyUltraItemAction, type SkyWebViewAPI } from '@skyultra/sdk';
import { ItemStage } from './ItemStage.js';
import { ItemThumbnail, ItemImage } from './ItemThumbnail.js';
import { itemThumbnailQueue } from './itemThumbnails.js';
import { itemDragPosition } from './itemDrag.js';
import { EquipmentPanel, EquipmentSelection, ItemCombatBadges, fitsEquipmentSlot, formatItemCombatMeta } from './EquipmentPanel.js';
import { BookReader, parseBookActionResult, type BookReaderDesignChoice, type ReadingBookState } from './BookReader.js';
import { SkyChoice, SkyDataList, SkySegmentedControl } from '../core.js';
import { Icon, itemIcon } from './icons.js';
import { Empty } from './pieces.js';
import { formId, type Item, type Palette, type Player } from './model.js';

const pageSize = 64;
const categoryOrder = ['weapon','armor','potion','ingredient','ammo','book','scroll','soul gem','key','miscellaneous'];
const categoryLabels: Record<string, string> = {all:'All items',weapon:'Weapons',armor:'Armor',potion:'Potions',ingredient:'Ingredients',ammo:'Ammo',book:'Books',scroll:'Scrolls','soul gem':'Soul gems',key:'Keys',miscellaneous:'Misc.'};
type Drag = { item: Item; pointer: number; x: number; y: number; active: boolean };
type SmartChip = 'all' | 'enchanted' | 'tempered' | 'books' | 'unread' | 'stolen' | 'poisoned';
type OutfitLoadout = { name: string; slots: Array<{ slot: SkyUltraEquipmentSlot; id: string; itemName: string }> };

const LOADOUT_STORAGE_KEY = 'skyultra.loadouts.v1';

function readLocalInventoryPrefs(): {
  defaultTheme?: string;
  bookReaderDesign?: BookReaderDesignChoice;
  bookThemeSync?: boolean;
  showValuePerWeight?: boolean;
  stolenProtection?: boolean;
} {
  try {
    const raw = window.localStorage?.getItem('skyultra.uiPreferences.v2');
    if (raw) {
      return JSON.parse(raw) as {
        defaultTheme?: string;
        bookReaderDesign?: BookReaderDesignChoice;
        bookThemeSync?: boolean;
        showValuePerWeight?: boolean;
        stolenProtection?: boolean;
      };
    }
  } catch {
    // Ignore storage access errors
  }
  return {};
}

function readOutfitLoadouts(): Array<OutfitLoadout | null> {
  try {
    const raw = window.localStorage?.getItem(LOADOUT_STORAGE_KEY);
    if (!raw) return [null, null, null, null];
    const parsed = JSON.parse(raw) as unknown;
    if (Array.isArray(parsed)) {
      return [0, 1, 2, 3].map((i) => (parsed[i] && typeof parsed[i] === 'object' ? (parsed[i] as OutfitLoadout) : null));
    }
  } catch {
    // Ignore storage errors
  }
  return [null, null, null, null];
}

function writeOutfitLoadouts(next: Array<OutfitLoadout | null>): void {
  try {
    window.localStorage?.setItem(LOADOUT_STORAGE_KEY, JSON.stringify(next));
  } catch {
    // Ignore storage errors
  }
}

export default function InventoryPage({ player, bridge, obscured = false, initialCategory = 'all', initialId = '', standalone = false, displayMode = 'fullscreen', pauseWorld = false, onToggleDisplayMode, onTogglePauseWorld, onClose }: {
  player: Player | null; bridge?: SkyWebViewAPI; obscured?: boolean; initialCategory?: string; initialId?: string; standalone?: boolean; displayMode?: 'fullscreen' | 'windowed'; pauseWorld?: boolean; onToggleDisplayMode?: () => void; onTogglePauseWorld?: () => void; onClose?: () => void;
}): React.JSX.Element {
  const game = useMemo(() => bridge ? createSkyUltraGame(bridge) : null, [bridge]);
  const [tab, setTab] = useState<'inventory' | 'studio'>(!standalone && initialId ? 'studio' : 'inventory');
  const activeTab = standalone ? 'inventory' : tab;
  const [query, setQuery] = useState(''), [category, setCategory] = useState(initialCategory);
  const [wearFilter, setWearFilter] = useState<NonNullable<SkyUltraInventoryOptions['equipment']>>('all');
  const [sort, setSort] = useState<NonNullable<SkyUltraInventoryOptions['sort']>>('name');
  const [layout, setLayout] = useState<'grid' | 'list'>('grid');
  const [selectedId, setSelectedId] = useState(initialId), [offset, setOffset] = useState(0);
  const [slotSelection, setSlotSelection] = useState<Item | null>(null);
  const [remote, setRemote] = useState<SkyUltraInventoryPage | null>(null), [loading, setLoading] = useState(false);
  const [readMessage, setReadMessage] = useState(''), [actionMessage, setActionMessage] = useState('');
  const [readingBook, setReadingBook] = useState<ReadingBookState | null>(null);
  const [smartChip, setSmartChip] = useState<SmartChip>('all');
  const [renamingItem, setRenamingItem] = useState<Item | null>(null);
  const [renameDraft, setRenameDraft] = useState('');
  const [pendingDropItem, setPendingDropItem] = useState<Item | null>(null);
  const [loadouts, setLoadouts] = useState<Array<OutfitLoadout | null>>(() => readOutfitLoadouts());
  const [outfitPlayer, setOutfitPlayer] = useState(player), [busy, setBusy] = useState(false), [revision, setRevision] = useState(0);
  const localPrefs = useMemo(() => readLocalInventoryPrefs(), [revision, readingBook]);
  const action = useRef<AbortController | null>(null), locked = useRef(false);
  const drag = useRef<Drag | null>(null), suppressClick = useRef(false);
  const [dragging, setDragging] = useState<Item | null>(null), [hoverSlot, setHoverSlot] = useState('');
  const ghostRef = useRef<HTMLDivElement>(null);
  const ghostPoint = useRef({ x: 0, y: 0 });
  const dragImage = useRef<string | undefined>(undefined);
  const selectItem = (item: Item): void => { setSelectedId(item.id); if (!locked.current) setActionMessage(''); };
  useEffect(() => { setOutfitPlayer(player); }, [player]);
  useEffect(() => { setCategory(initialCategory); setSelectedId(initialId); setOffset(0); if (initialId && !standalone) setTab('studio'); }, [initialCategory, initialId, standalone]);
  useEffect(() => { setOffset(0); }, [query, category, wearFilter, sort]);
  useEffect(() => () => { action.current?.abort(); }, [bridge]);
  useEffect(() => { if (obscured) { drag.current = null; setDragging(null); setHoverSlot(''); } }, [obscured]);
  useEffect(() => {
    if (!game || !bridge || obscured) return;
    let active = true, pending = false;
    const controller = new AbortController();
    const refresh = async (): Promise<void> => {
      if (pending || document.hidden || !(bridge.getVisibility?.().visible ?? true)) return;
      pending = true; setLoading(true);
      try {
        const value = await game.inventory({ query: query.trim(), category, equipment: wearFilter, sort, offset, limit: pageSize }, { signal: controller.signal });
        if (active) { setRemote(value); setReadMessage(value.available ? '' : 'Inventory is unavailable.'); if (value.available && offset >= value.totalMatches && offset > 0) setOffset(Math.max(0, Math.floor((value.totalMatches - 1) / pageSize) * pageSize)); }
      } catch (error) { if (active) setReadMessage(error instanceof Error ? error.message : 'Could not read inventory.'); }
      finally { pending = false; if (active) setLoading(false); }
    };
    const timer = window.setTimeout(() => void refresh(), query ? 220 : 0);
    const interval = window.setInterval(() => void refresh(), 5000);
    const visibility = (): void => { void refresh(); };
    const unsubscribe = bridge.on('skyultra.visibility', visibility); document.addEventListener('visibilitychange', visibility);
    return () => { active = false; controller.abort(); window.clearTimeout(timer); window.clearInterval(interval); unsubscribe(); document.removeEventListener('visibilitychange', visibility); };
  }, [game, bridge, obscured, query, category, wearFilter, sort, offset, revision]);
  const isValidNamedItem = (item: Item): boolean => {
    const trimmed = item.name?.trim() ?? '';
    if (!trimmed) return false;
    const lower = trimmed.toLowerCase();
    return !lower.startsWith('<missing name') && lower !== 'missing name';
  };
  const fallback = useMemo(() => (player?.inventory ?? [])
    .filter(isValidNamedItem)
    .filter(item => category === 'all' || item.category === category)
    .filter(item => wearFilter === 'all' || item.equipped === (wearFilter === 'equipped'))
    .filter(item => !query.trim() || (item.name + ' ' + item.category + ' ' + item.id + ' ' + formId(item.id)).toLowerCase().includes(query.trim().toLowerCase()))
    .sort((a, b) => sort === 'vw' ? (b.value / Math.max(b.weight, 0.05)) - (a.value / Math.max(a.weight, 0.05)) : sort === 'value' ? b.value - a.value : sort === 'weight' ? b.weight - a.weight : sort === 'count' ? b.count - a.count : a.name.localeCompare(b.name)),
  [player, category, wearFilter, query, sort]);
  const rawItems = (remote?.available ? remote.results : fallback.slice(offset, offset + pageSize)).filter(isValidNamedItem);
  const items = rawItems.filter((item) => {
    if (smartChip === 'enchanted') return Boolean(item.enchanted || item.enchantments?.length);
    if (smartChip === 'tempered') return Boolean(item.temperTier);
    if (smartChip === 'books') return item.category.toLowerCase() === 'book';
    if (smartChip === 'unread') return item.category.toLowerCase() === 'book' && item.isRead !== true;
    if (smartChip === 'stolen') return Boolean(item.stolen);
    if (smartChip === 'poisoned') return Boolean(item.poisonName);
    return true;
  });
  const total = remote?.available ? remote.totalMatches : fallback.length;
  const categories = remote?.available ? remote.categories : [...new Set((player?.inventory ?? []).map(item => item.category))].sort();
  const categoryTabs = ['all', ...[...new Set([...categories, ...(category === 'all' ? [] : [category])])].sort((a,b) => {
    const ai=categoryOrder.indexOf(a),bi=categoryOrder.indexOf(b);return (ai<0?99:ai)-(bi<0?99:bi)||a.localeCompare(b);
  })];
  const selected = items.find(item => item.id === selectedId) ?? player?.inventory.find(item => item.id === selectedId) ?? (slotSelection?.id === selectedId ? slotSelection : null) ?? items[0] ?? null;
  async function changeEquipment(item: Item, slot: SkyUltraEquipmentSlot, unequip = false): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true; setBusy(true); setActionMessage(unequip ? 'Unequipping ' + item.name + '…' : 'Equipping ' + item.name + '…');
    const controller = new AbortController(); action.current = controller;
    const baseId = item.id.split(':')[0]!;
    const targetSlot: SkyUltraEquipmentSlot = slot === 'auto' ? (item.equipSlots?.[0] ?? 'auto') : slot;
    const applyOptimisticState = (equipping: boolean, serverEquipment?: Player['equipment']): void => {
      setOutfitPlayer(prev => {
        if (!prev) return prev;
        const nextEquip: Player['equipment'] = serverEquipment ? { ...serverEquipment } : { ...prev.equipment };
        if (!serverEquipment) {
          if (!equipping) {
            if (targetSlot !== 'auto') nextEquip[targetSlot] = null;
            for (const key of Object.keys(nextEquip) as Array<keyof Player['equipment']>) {
              if (nextEquip[key]?.id.split(':')[0] === baseId) nextEquip[key] = null;
            }
          } else {
            const equippedEntry: Item = { ...item, equipped: true };
            if (item.category === 'armor' && item.equipSlots?.length) {
              for (const s of item.equipSlots) {
                if (s !== 'auto') nextEquip[s] = equippedEntry;
              }
            } else if (targetSlot !== 'auto') {
              nextEquip[targetSlot] = equippedEntry;
            }
          }
        }
        const nextInventory = prev.inventory.map(entry =>
          entry.id.split(':')[0] === baseId ? { ...entry, equipped: equipping } : entry
        );
        return { ...prev, equipment: nextEquip, inventory: nextInventory };
      });
      setRemote(prev => {
        if (!prev) return prev;
        return {
          ...prev,
          results: prev.results.map(entry =>
            entry.id.split(':')[0] === baseId ? { ...entry, equipped: equipping } : entry
          ),
        };
      });
      setSlotSelection(prev => (prev && prev.id.split(':')[0] === baseId ? { ...prev, equipped: equipping } : prev));
    };
    applyOptimisticState(!unequip);
    try {
      const result = await (unequip ? game.unequipItem(item.id, slot, { signal: controller.signal }) : game.equipItem(item.id, slot, { signal: controller.signal }));
      if (!result.accepted) {
        applyOptimisticState(unequip);
        setActionMessage(result.message ?? 'Skyrim declined the equipment change.');
        return;
      }
      applyOptimisticState(!unequip, result.equipment);
      setRevision(value => value + 1);
      setActionMessage(item.name + (unequip ? ' unequipped.' : ' equipped.'));
      window.setTimeout(() => {
        if (controller.signal.aborted) return;
        void game.player('full', { signal: controller.signal }).then(snapshot => {
          if (!controller.signal.aborted && snapshot.available) setOutfitPlayer(snapshot);
        }).catch(() => undefined);
      }, 150);
    } catch (error) {
      if (!controller.signal.aborted) {
        applyOptimisticState(unequip);
        setActionMessage(error instanceof Error ? error.message : 'Could not change player equipment.');
      }
    } finally { locked.current = false; if (!controller.signal.aborted) setBusy(false); }
  }
  async function performItemAction(item: Item, itemAction: SkyUltraItemAction, count = 1, openNative = false): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true; setBusy(true);
    const verb = itemAction === 'drop' ? 'Dropping ' : itemAction === 'drink' ? 'Drinking ' : itemAction === 'eat' ? 'Eating ' : itemAction === 'learn' ? 'Learning ' : itemAction === 'read' ? 'Reading ' : itemAction === 'charge' ? 'Charging ' : 'Activating ';
    setActionMessage(verb + item.name + '…');
    const controller = new AbortController(); action.current = controller;
    try {
      const result = await game.itemAction({ formId: item.id, action: itemAction, count, openNative }, { signal: controller.signal });
      if (!result.accepted) { setActionMessage(result.message ?? 'Skyrim declined that item action.'); return; }
      if (result.bookText !== undefined && !openNative) {
        const parsedBook = parseBookActionResult(item, result);
        if (parsedBook) setReadingBook(parsedBook);
      }
      await new Promise(resolve => window.setTimeout(resolve, 120));
      if (!controller.signal.aborted) {
        const snapshot = await game.player('full', { signal: controller.signal });
        if (!controller.signal.aborted && snapshot.available) {
          setOutfitPlayer(snapshot);
        }
      }
      setRevision(value => value + 1);
      setActionMessage(result.message ?? (item.name + ' updated.'));
    } catch (error) {
      if (!controller.signal.aborted) setActionMessage(error instanceof Error ? error.message : 'Could not perform item action.');
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }
  async function toggleFavorite(item: Item): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true; setBusy(true);
    const next = !item.favorite;
    setActionMessage((next ? 'Adding ' : 'Removing ') + item.name + (next ? ' to favorites…' : ' from favorites…'));
    const controller = new AbortController(); action.current = controller;
    try {
      const result = await game.setFavorite(item.id, 'item', next, { signal: controller.signal });
      setRevision(value => value + 1);
      setActionMessage(result.message ?? (item.name + ' updated.'));
    } catch (error) {
      if (!controller.signal.aborted) setActionMessage(error instanceof Error ? error.message : 'Could not update favorites.');
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }
  async function submitRenameItem(item: Item, nextName: string, clear = false): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true;
    setBusy(true);
    setActionMessage(clear ? `Restoring ${item.name}…` : `Renaming ${item.name}…`);
    const controller = new AbortController();
    action.current = controller;
    try {
      const result = await game.renameItem({ id: item.id, name: nextName, clear }, { signal: controller.signal });
      if (result.updated) {
        setRenamingItem(null);
        setRenameDraft('');
        setRevision(value => value + 1);
      }
      setActionMessage(result.message ?? (result.updated ? 'Item renamed.' : 'Could not rename item.'));
    } catch (error) {
      if (!controller.signal.aborted) setActionMessage(error instanceof Error ? error.message : 'Could not rename item.');
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }
  function saveCurrentLoadout(slotIndex: number): void {
    const equip = outfitPlayer?.equipment;
    if (!equip) return;
    const entries: OutfitLoadout['slots'] = [];
    const seen = new Set<string>();
    for (const [slotKey, eqItem] of Object.entries(equip)) {
      if (!eqItem) continue;
      const key = `${slotKey}:${eqItem.id}`;
      if (seen.has(key)) continue;
      seen.add(key);
      entries.push({ slot: slotKey as SkyUltraEquipmentSlot, id: eqItem.id, itemName: eqItem.name });
    }
    const next = [...loadouts];
    next[slotIndex] = { name: `Loadout ${slotIndex + 1} (${entries.length})`, slots: entries };
    setLoadouts(next);
    writeOutfitLoadouts(next);
    setActionMessage(`Saved ${entries.length} equipped slots to Loadout ${slotIndex + 1}.`);
  }
  async function applySavedLoadout(slotIndex: number): Promise<void> {
    const preset = loadouts[slotIndex];
    if (!preset || !game || obscured || locked.current) return;
    locked.current = true;
    setBusy(true);
    setActionMessage(`Equipping Loadout ${slotIndex + 1}…`);
    const controller = new AbortController();
    action.current = controller;
    let applied = 0;
    try {
      for (const entry of preset.slots) {
        if (controller.signal.aborted) break;
        try {
          const res = await game.equipItem(entry.id, entry.slot, { signal: controller.signal });
          if (res.accepted) ++applied;
        } catch {
          // Skip missing or un-equippable pieces cleanly
        }
      }
      setRevision(v => v + 1);
      setActionMessage(`Applied Loadout ${slotIndex + 1} (${applied}/${preset.slots.length} equipped).`);
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }
  const endDragAtPoint = (pointerId: number, clientX: number, clientY: number, cancel = false, targetEl?: HTMLButtonElement | null): void => {
    const current = drag.current; if (!current || current.pointer !== pointerId) return;
    drag.current = null; setDragging(null); setHoverSlot('');
    try { if (targetEl?.hasPointerCapture?.(pointerId)) targetEl.releasePointerCapture(pointerId); } catch { /* ignore */ }
    if (!current.active) return;
    suppressClick.current = true;
    const target = cancel ? '' : document.elementFromPoint(clientX, clientY)?.closest<HTMLElement>('[data-equip-slot]')?.dataset.equipSlot ?? '';
    if (fitsEquipmentSlot(current.item, target)) void changeEquipment(current.item, target as SkyUltraEquipmentSlot);
    else setActionMessage(target ? 'This item does not fit that equipment slot.' : 'Drop the item onto a matching equipment slot.');
  };
  const endDrag = (event: React.PointerEvent<HTMLButtonElement>, cancel = false): void => {
    endDragAtPoint(event.pointerId, event.clientX, event.clientY, cancel, event.currentTarget);
  };
  useEffect(() => {
    const onWindowPointerMove = (event: PointerEvent): void => {
      const current = drag.current;
      if (!current || current.pointer !== event.pointerId) return;
      if (!current.active && Math.hypot(event.clientX - current.x, event.clientY - current.y) < 6) return;
      const starting = !current.active;
      current.active = true;
      if (starting) { setDragging(current.item); selectItem(current.item); }
      ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight);
      if (ghostRef.current) ghostRef.current.style.transform = 'translate3d(' + ghostPoint.current.x + 'px,' + ghostPoint.current.y + 'px,0)';
      setHoverSlot(document.elementFromPoint(event.clientX, event.clientY)?.closest<HTMLElement>('[data-equip-slot]')?.dataset.equipSlot ?? '');
    };
    const onWindowPointerUp = (event: PointerEvent): void => {
      if (drag.current && drag.current.pointer === event.pointerId) {
        endDragAtPoint(event.pointerId, event.clientX, event.clientY, false);
      }
    };
    const onWindowPointerCancel = (event: PointerEvent): void => {
      if (drag.current && drag.current.pointer === event.pointerId) {
        endDragAtPoint(event.pointerId, event.clientX, event.clientY, true);
      }
    };
    window.addEventListener?.('pointermove', onWindowPointerMove, { passive: true });
    window.addEventListener?.('pointerup', onWindowPointerUp);
    window.addEventListener?.('pointercancel', onWindowPointerCancel);
    return () => {
      window.removeEventListener?.('pointermove', onWindowPointerMove);
      window.removeEventListener?.('pointerup', onWindowPointerUp);
      window.removeEventListener?.('pointercancel', onWindowPointerCancel);
    };
  });
  const browser = <section className="surface inventory-surface" aria-label="Carried items" aria-busy={loading}>
    <div className="inventory-toolbar"><label className="page-search"><Icon name="search" size={16} /><input aria-label="Search inventory" value={query} onChange={event => setQuery(event.currentTarget.value)} placeholder="Search your belongings…" />{query ? <button type="button" aria-label="Clear inventory search" onClick={() => setQuery('')}><Icon name="close" size={12} /></button> : <kbd>Space</kbd>}</label><SkySegmentedControl label="Inventory layout" value={layout} onValueChange={setLayout} options={[{ value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' }, { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' }]} /></div>
    <div className="pack-categories" role="group" aria-label="Inventory categories">{categoryTabs.map(value => <button type="button" key={value} aria-pressed={category === value} onClick={() => setCategory(value)} title={categoryLabels[value] ?? value}><Icon name={value === 'all' ? 'inventory' : itemIcon(value)} size={18} /><span>{categoryLabels[value] ?? value}</span></button>)}</div>
    <div className="inventory-filters">
      <SkyChoice label="Equipment state" value={wearFilter} onValueChange={setWearFilter} options={[{ value: 'all', label: 'All equipment states' }, { value: 'equipped', label: 'Equipped' }, { value: 'carried', label: 'Carried' }]} />
      <SkyChoice label="Sort inventory" value={sort} onValueChange={setSort} options={[{ value: 'name', label: 'Name A–Z' }, { value: 'value', label: 'Highest value' }, { value: 'weight', label: 'Highest weight' }, { value: 'vw', label: 'Gold / weight (V/W)' }, { value: 'count', label: 'Largest stack' }]} />
      <span className="pack-filter-state">{categoryLabels[category] ?? category}<i />{wearFilter === 'all' ? 'All gear' : wearFilter === 'equipped' ? 'Equipped' : 'Carried'}</span>
    </div>
    <div className="inventory-subbar">
      <div className="smart-filter-chips" role="group" aria-label="Quick item traits">
        {([
          { id: 'all', label: 'All Traits' },
          { id: 'enchanted', label: 'Enchanted' },
          { id: 'tempered', label: 'Tempered' },
          { id: 'poisoned', label: 'Poisoned' },
          { id: 'books', label: 'Books' },
          { id: 'unread', label: 'Unread' },
          { id: 'stolen', label: 'Stolen' },
        ] as const).map(chip => (
          <button key={chip.id} type="button" className={'smart-chip' + (smartChip === chip.id ? ' is-active' : '')} onClick={() => setSmartChip(chip.id)}>{chip.label}</button>
        ))}
      </div>
      <div className="loadout-strip" role="group" aria-label="Quick outfit loadouts">
        <small className="eyebrow">OUTFITS</small>
        {[0, 1, 2, 3].map(idx => {
          const preset = loadouts[idx];
          return (
            <div key={idx} className="loadout-slot">
              <button type="button" className={'control-button loadout-slot__equip' + (preset ? ' is-active' : '')} disabled={!preset || busy || obscured} title={preset ? `Equip ${preset.name}` : `Loadout ${idx + 1} empty`} onClick={() => { void applySavedLoadout(idx); }}>
                <span>{preset ? `#${idx + 1}` : `${idx + 1}`}</span>
              </button>
              <button type="button" className="control-button loadout-slot__save" disabled={busy || obscured || !bridge} title={`Save current outfit to slot ${idx + 1}`} onClick={() => saveCurrentLoadout(idx)}>+</button>
            </div>
          );
        })}
      </div>
    </div>
    <div className={'inventory-results inventory-results--' + layout}>{items.length ? items.map(item => {
      const isEnchanted = Boolean(item.enchanted || item.enchantments?.length);
      const isStolen = Boolean(item.stolen) && item.id !== '15';
      const enchantSummary = item.enchantments?.length ? item.enchantments.map(e => e.description ? `${e.name}: ${e.description}` : e.name).join(' · ') : item.enchantmentName || 'Enchanted';
      const vwRatio = item.weight > 0 ? Math.round((item.value / item.weight) * 10) / 10 : null;
      const combatBadgeLabel =
        typeof item.damage === 'number' && item.damage > 0
          ? `DMG ${Math.round(item.damage)}`
          : typeof item.armorRating === 'number' && item.armorRating > 0
            ? `AR ${Math.round(item.armorRating)}`
            : null;
      return <button type="button" aria-pressed={item.id === selected?.id} key={item.id} title={(isEnchanted ? `${item.name} (${enchantSummary})` : item.name) + (isStolen ? ' [STOLEN]' : '') + (item.isRead === true ? ' [Already read]' : '')} data-category={item.category} data-enchanted={isEnchanted ? 'true' : undefined} data-stolen={isStolen ? 'true' : undefined} className={'item-tile' + (item.id === selected?.id ? ' is-active' : '') + (item.equipped ? ' is-equipped' : '') + (isEnchanted ? ' is-enchanted' : '') + (isStolen ? ' is-stolen' : '') + (activeTab === 'inventory' && item.equipSlots?.length ? ' can-equip' : '')}
        onClick={() => { if (suppressClick.current) { suppressClick.current = false; return; } selectItem(item); }}
        onDragStart={event => event.preventDefault()}
        onPointerDown={event => { suppressClick.current = false; if (activeTab !== 'inventory' || event.button !== 0 || busy || obscured || !item.equipSlots?.length) return; try { event.currentTarget.setPointerCapture(event.pointerId); } catch { /* ignore */ } const image = event.currentTarget.querySelector?.<HTMLImageElement>('.item-thumbnail img'); dragImage.current = image?.getAttribute('src') || undefined; ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight); drag.current = { item, pointer: event.pointerId, x: event.clientX, y: event.clientY, active: false }; }}
        onPointerMove={event => { const current = drag.current; if (!current || current.pointer !== event.pointerId) return; if (!current.active && Math.hypot(event.clientX - current.x, event.clientY - current.y) < 6) return; const starting = !current.active; current.active = true; event.preventDefault(); if (starting) { setDragging(item); selectItem(item); } ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight); if (ghostRef.current) ghostRef.current.style.transform = 'translate3d(' + ghostPoint.current.x + 'px,' + ghostPoint.current.y + 'px,0)'; setHoverSlot(document.elementFromPoint(event.clientX, event.clientY)?.closest<HTMLElement>('[data-equip-slot]')?.dataset.equipSlot ?? ''); }}
        onPointerUp={event => endDrag(event)} onPointerCancel={event => endDrag(event, true)}>
        <span className="item-tile__top"><small>{item.equipped ? <><Icon name="check" size={10} />WORN</> : isStolen ? <span className="item-tile__stolen-tag"><Icon name="stolen" size={10} />STOLEN</span> : (item.armorTypeLabel || item.weaponType || item.category)}</small><span>{isEnchanted && <em className="item-tile__enchant-mark" title={enchantSummary}><Icon name="flame" size={10} /></em>}{item.value.toLocaleString()}<Icon name="gold" size={10} /></span></span>
        <span className="item-tile__image"><ItemThumbnail item={item} bridge={bridge} paused={obscured} />{isEnchanted && <span className="item-tile__enchant-badge" title={enchantSummary} aria-label="Enchanted item"><Icon name="flame" size={11} /></span>}
          {item.isRead === true && <span className="item-tile__read-seal" role="img" title="Already read" aria-label="Already read"><Icon name="bookRead" size={22} /></span>}
          {isStolen && <span className="item-tile__stolen-pill" title="Stolen item"><Icon name="stolen" size={10} />STOLEN</span>}
          {combatBadgeLabel && <b className="item-tile__stat-chip">{combatBadgeLabel}</b>}
          {item.count > 1 && <b className="item-tile__quantity">×{item.count.toLocaleString()}</b>}</span>
        <span className="item-tile__copy"><strong>{isEnchanted && <Icon name="flame" size={10} />}{item.name}</strong><small>{isStolen ? 'Stolen · ' : ''}{item.armorTypeLabel || item.weaponType || item.category}<ItemCombatBadges item={item} compact /><span className="item-meta-pills">{item.temperTier && <b className="item-meta-pill">{item.temperTier}</b>}{item.poisonName && <b className="item-meta-pill is-poison">Poisoned</b>}{item.soulSize && <b className="item-meta-pill">{item.soulSize}</b>}{item.teachesSkill && <b className="item-meta-pill">+1 {item.teachesSkill}</b>}{localPrefs.showValuePerWeight !== false && vwRatio !== null && item.value > 0 && <b className="item-meta-pill">{vwRatio} V/W</b>}</span><span>{item.weight} wt</span></small></span>
      </button>;
    }) : <Empty title={loading ? 'Reading inventory' : 'No items matched'} detail="Try a different filter or search." icon="inventory" />}</div>
    <div className="roster-footer inventory-pagination"><span>{loading ? 'Refreshing…' : total ? `${offset + 1}–${offset + items.length} of ${total.toLocaleString()}` : '0 items'}{!bridge && player?.inventoryTruncated ? ' · Snapshot is partial' : ''}</span><div><button className="control-button" disabled={offset === 0 || loading} onClick={() => setOffset(value => Math.max(0, value - pageSize))}>Previous</button><button className="control-button" disabled={offset + items.length >= total || loading} onClick={() => setOffset(value => value + items.length)}>Next</button></div></div>
    {activeTab === 'inventory' && <EquipmentSelection player={outfitPlayer} selected={selected} bridge={bridge} obscured={obscured} onInspect={standalone ? undefined : () => { if (selected) setSelectedId(selected.id); setTab('studio'); }} enabled={!!bridge && !busy && !obscured} onChange={(item, slot, unequip) => { void changeEquipment(item, slot, unequip); }} onItemAction={(item, act, count) => { if (act === 'drop' && item.stolen && localPrefs.stolenProtection !== false) { setPendingDropItem(item); return; } void performItemAction(item, act, count); }} onToggleFavorite={(item) => { void toggleFavorite(item); }} onRenameItem={(item) => { setRenamingItem(item); setRenameDraft(item.name); }} />}
    {renamingItem && (
      <form className="inventory-inline-modal" onSubmit={ev => { ev.preventDefault(); void submitRenameItem(renamingItem, renameDraft); }}>
        <strong>Rename {renamingItem.name}:</strong>
        <input className="pack-search" style={{ minHeight: 30, padding: '0 10px', flex: '1 1 200px' }} value={renameDraft} maxLength={96} onChange={e => setRenameDraft(e.currentTarget.value)} aria-label="New item name" />
        <button type="submit" className="control-button is-active" disabled={busy || !renameDraft.trim()}>Apply</button>
        <button type="button" className="control-button" disabled={busy} onClick={() => { void submitRenameItem(renamingItem, '', true); }}>Reset Name</button>
        <button type="button" className="control-button" onClick={() => setRenamingItem(null)}>Cancel</button>
      </form>
    )}
    {pendingDropItem && (
      <div className="inventory-inline-modal" role="alert">
        <span><strong>Stolen Protection:</strong> Confirm dropping <em>{pendingDropItem.name}</em>?</span>
        <button type="button" className="control-button is-active" onClick={() => { const item = pendingDropItem; setPendingDropItem(null); void performItemAction(item, 'drop', 1); }}>Confirm Drop</button>
        <button type="button" className="control-button" onClick={() => setPendingDropItem(null)}>Keep Item</button>
      </div>
    )}
    <div className="inventory-notice" role="status">{actionMessage || readMessage || (activeTab === 'inventory' ? 'Select an item or drag it to a matching equipment slot.' : 'Select an item to open its studio preview.')}</div>
  </section>;
  return <div className={'page inventory-page inventory-page--wardrobe inventory-page--armory' + (standalone ? ' inventory-page--standalone' : '')}>
    <header className="armory-masthead"><div className="armory-heading"><span className="eyebrow">{standalone ? 'SKYRIM PLAYER INVENTORY · LIVE 3D LOADOUT' : 'EQUIPMENT & BELONGINGS'}</span><h1>{activeTab === 'studio' ? 'Item studio' : standalone ? 'Player Inventory' : 'Inventory'}</h1></div>
      {!standalone && <div className="armory-workspaces"><SkySegmentedControl label="Inventory workspace" value={tab} onValueChange={value => { drag.current = null; setDragging(null); setTab(value); }} options={[{ value: 'inventory', label: 'Inventory' }, { value: 'studio', label: 'Item studio' }]} /></div>}
      <div className="armory-masthead__end">
        {standalone && (onToggleDisplayMode || onTogglePauseWorld) && (
          <div className="armory-mode-pills" role="group" aria-label="Display and world state">
            {onToggleDisplayMode && (
              <button type="button" className={'control-button armory-mode-btn' + (displayMode === 'windowed' ? ' is-active' : '')} onClick={onToggleDisplayMode} aria-pressed={displayMode === 'windowed'} title={`Toggle between Full Screen and Windowed mode (Current: ${displayMode === 'windowed' ? 'Windowed' : 'Full Screen'})`}>
                <Icon name="widgets" size={12} />
                <span>{displayMode === 'windowed' ? 'Windowed' : 'Full Screen'}</span>
              </button>
            )}
            {onTogglePauseWorld && (
              <button type="button" className={'control-button armory-mode-btn' + (pauseWorld ? ' is-active' : '')} onClick={onTogglePauseWorld} aria-pressed={!!pauseWorld} title={`Toggle pausing the Skyrim world while open (Current: ${pauseWorld ? 'World Paused' : 'Live World'})`}>
                <Icon name="clock" size={12} />
                <span>{pauseWorld ? 'Pause World' : 'Live World'}</span>
              </button>
            )}
          </div>
        )}
        <div className="armory-total"><strong>{(remote?.totalTypes ?? player?.inventoryTypeCount)?.toLocaleString() ?? '—'}</strong><span>ITEM TYPES</span></div>
        {standalone && onClose && (
          <button type="button" className="control-button armory-close-btn" onClick={onClose} aria-label="Close inventory">
            <span>Close</span>
            <kbd>ESC</kbd>
          </button>
        )}
      </div>
    </header>
    <div className={activeTab === 'inventory' ? 'inventory-outfit-workbench' : 'inventory-workbench'}>

      {browser}
      {activeTab === 'inventory' && <EquipmentPanel player={outfitPlayer} bridge={bridge} obscured={obscured || Boolean(readingBook)} selected={selected} dragging={dragging} hoverSlot={hoverSlot} busy={busy} revision={revision} onSelect={item => { selectItem(item); setSlotSelection(item); }} onChange={(item, slot, unequip) => { void changeEquipment(item, slot, unequip); }} />}
      {activeTab === 'studio' && <aside className="surface item-inspector">{selected ? <>{bridge ? <ItemStage item={selected} bridge={bridge} obscured={obscured || Boolean(readingBook)} onCapturePortrait={async () => {
        const result = await createSkyUltraGame(bridge).thumbnail(selected.id, { usePreview: true });
        if (!result.available || !result.dataUrl) throw new Error(result.message ?? 'Could not capture item portrait from the current view.');
        itemThumbnailQueue(bridge).set(selected.id, result.dataUrl);
        return 'Saved as item portrait in Overwrite cache.';
      }} /> : <ItemThumbnail item={selected} />}<div className="item-inspector__copy"><span className="eyebrow">{selected.armorTypeLabel || selected.weaponType || selected.category}{(selected.enchanted || selected.enchantments?.length) ? ' · ENCHANTED' : ''}{formatItemCombatMeta(selected) ? ` · ${formatItemCombatMeta(selected).toUpperCase()}` : ''}</span><h2>{selected.name}</h2><p className="item-form">{formId(selected.id)}</p><SkyDataList label="Item properties" rows={[...(typeof selected.damage === 'number' && selected.damage > 0 ? [{ id: 'damage', label: 'Damage (DMG)', value: String(Math.round(selected.damage)) }] : []), ...(typeof selected.armorRating === 'number' && selected.armorRating > 0 ? [{ id: 'armorRating', label: selected.armorTypeLabel ? `${selected.armorTypeLabel} Rating` : 'Armor Rating', value: String(Math.round(selected.armorRating)) }] : []), ...(selected.armorTypeLabel ? [{ id: 'armorClass', label: 'Armor Class', value: selected.armorTypeLabel }] : []), ...(selected.weaponType ? [{ id: 'weaponType', label: 'Weapon Class', value: selected.weaponType }] : []), { id: 'value', label: 'Value', value: selected.value.toLocaleString() + ' gold' }, { id: 'weight', label: 'Weight', value: String(selected.weight) }, { id: 'count', label: 'Quantity', value: selected.count.toLocaleString() }, { id: 'ratio', label: 'Value / weight', value: selected.weight > 0 ? (selected.value / selected.weight).toFixed(1) : 'Weightless' }, ...(selected.enchantmentCharge !== undefined ? [{ id: 'charge', label: 'Enchantment charge', value: `${selected.enchantmentCharge}%` }] : [])]} />{(selected.enchanted || selected.enchantments?.length) && <div className="selection-enchantments">{selected.enchantments?.length ? selected.enchantments.map((eff, idx) => <div key={`${eff.name}-${idx}`} className="selection-enchantment"><Icon name="flame" size={11} /><strong>{eff.name}</strong>{eff.description && <span>{eff.description}</span>}</div>) : <div className="selection-enchantment"><Icon name="flame" size={11} /><strong>{selected.enchantmentName || 'Enchanted Item'}</strong><span>Arcane enchantment active</span></div>}</div>}<p className="inspector-note">World model and alternate textures from the item record. Preview and Save use the selected lighting.</p></div></> : <Empty title="Select an item" detail="Details appear here." icon="inventory" />}</aside>}
    </div>

    {readingBook && (
      <BookReader
        book={readingBook}
        playerName={player?.playerName}
        theme={(localPrefs.defaultTheme as Palette) || 'dragonstone'}
        design={localPrefs.bookReaderDesign ?? 'auto'}
        themeSync={localPrefs.bookThemeSync !== false}
        onOpenNative={(book) => { void performItemAction({ id: book.id, name: book.title, category: 'book', count: 1, value: 0, weight: 0, equipped: false }, 'read', 1, true); }}
        onClose={() => setReadingBook(null)}
      />
    )}
    {dragging && <div ref={ghostRef} className="equipment-drag-ghost" style={{ transform: 'translate3d(' + ghostPoint.current.x + 'px,' + ghostPoint.current.y + 'px,0)' }} aria-hidden="true"><span className="equipment-drag-ghost__art">{dragImage.current ? <ItemImage item={dragging} src={dragImage.current} /> : <ItemThumbnail item={dragging} bridge={bridge} paused={obscured} />}</span><span className="equipment-drag-ghost__copy"><strong>{dragging.name}</strong><small>{hoverSlot ? fitsEquipmentSlot(dragging, hoverSlot) ? 'Release to equip' : 'Incompatible slot' : 'Drag to an equipment slot'}</small></span></div>}
  </div>;
}
