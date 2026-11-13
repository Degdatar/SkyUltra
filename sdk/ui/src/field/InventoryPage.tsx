import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
import { createSkyUltraGame, type SkyUltraEquipmentSlot, type SkyUltraInventoryOptions, type SkyUltraInventoryPage, type SkyUltraItemAction, type SkyWebViewAPI } from '@skyultra/sdk';
import { ItemStage } from './ItemStage.js';
import { ItemThumbnail, ItemImage } from './ItemThumbnail.js';
import { itemThumbnailQueue } from './itemThumbnails.js';
import { itemDragPosition } from './itemDrag.js';
import { EquipmentPanel, EquipmentSelection, ItemCombatBadges, fitsEquipmentSlot, formatItemCombatMeta } from './EquipmentPanel.js';
import { SkyChoice, SkyDataList, SkySegmentedControl } from '../core.js';
import { Icon, itemIcon } from './icons.js';
import { Empty } from './pieces.js';
import { formId, type Item, type Player } from './model.js';

const pageSize = 64;
const categoryOrder = ['weapon','armor','potion','ingredient','ammo','book','scroll','soul gem','key','miscellaneous'];
const categoryLabels: Record<string, string> = {all:'All items',weapon:'Weapons',armor:'Armor',potion:'Potions',ingredient:'Ingredients',ammo:'Ammo',book:'Books',scroll:'Scrolls','soul gem':'Soul gems',key:'Keys',miscellaneous:'Misc.'};
type Drag = { item: Item; pointer: number; x: number; y: number; active: boolean };
type ReadingBookState = {
  id: string;
  title: string;
  text: string;
  pages: string[];
  images: string[];
  style: 'book' | 'note' | 'map';
};

function renderFormattedPageContent(rawPage: string, isFirstFolio: boolean): React.JSX.Element {
  const blocks = rawPage
    .replace(/\r\n/g, '\n')
    .split(/\n{2,}/)
    .map(b => b.trim())
    .filter(Boolean);
  let appliedDropCap = false;
  return (
    <div className="inventory-book-folio__content">
      {blocks.map((block, idx) => {
        if (/^[-—―*~_=]{2,}$/.test(block)) {
          return <hr key={idx} className="inventory-book-folio__rule" />;
        }
        const lines = block.split('\n');
        const isShortHeading = idx === 0 && isFirstFolio && lines.length <= 3 && block.length < 90 && !/[.!?:]$/.test(block);
        if (isShortHeading) {
          return (
            <div key={idx} className="inventory-book-folio__title-block">
              {lines.map((line, lIdx) => <p key={lIdx}>{line}</p>)}
            </div>
          );
        }
        const canDropCap = isFirstFolio && !appliedDropCap && /^[A-Za-z]/.test(block) && block.length > 45;
        if (canDropCap) {
          appliedDropCap = true;
          const firstChar = block.charAt(0);
          const rest = block.slice(1);
          return (
            <p key={idx} className="inventory-book-folio__paragraph has-dropcap">
              <span className="inventory-book-folio__dropcap" aria-hidden="true">{firstChar}</span>
              <span>{firstChar}{rest}</span>
            </p>
          );
        }
        return (
          <p key={idx} className="inventory-book-folio__paragraph">
            {block}
          </p>
        );
      })}
    </div>
  );
}

type BookLeaf =
  | { kind: 'cover' | 'back'; src?: string; title: string }
  | { kind: 'plate'; src: string; index: number }
  | { kind: 'text'; text: string; pageNo: number }
  | { kind: 'finis' };

/**
 * Lays a book out as physical leaves: front cover, text folios (interior engravings
 * after the first folio), and a back cover. The cover only appears on the first and
 * last spreads, and is always paired with a facing page so it never sits alone.
 */
function buildBookLeaves(book: ReadingBookState, hasRealText: boolean): BookLeaf[] {
  const coverSrc = book.images[0];
  const interior = book.images.slice(1);
  const leaves: BookLeaf[] = [{ kind: 'cover', src: coverSrc, title: book.title }];
  const pages = hasRealText ? book.pages : [];
  pages.forEach((text, idx) => {
    leaves.push({ kind: 'text', text, pageNo: idx + 1 });
    if (idx === 0) interior.forEach((src, pIdx) => leaves.push({ kind: 'plate', src, index: pIdx + 1 }));
  });
  if (pages.length === 0) interior.forEach((src, pIdx) => leaves.push({ kind: 'plate', src, index: pIdx + 1 }));
  if (leaves.length % 2 === 0) leaves.push({ kind: 'finis' });
  leaves.push({ kind: 'back', src: coverSrc, title: book.title });
  return leaves;
}

function leafLabel(leaf: BookLeaf | undefined): string {
  if (!leaf) return '';
  switch (leaf.kind) {
    case 'cover': return 'Front Cover';
    case 'back': return 'Back Cover';
    case 'plate': return `Plate ${leaf.index}`;
    case 'finis': return 'The End';
    default: return `Folio ${leaf.pageNo}`;
  }
}

function renderBookLeaf(leaf: BookLeaf | undefined, side: 'left' | 'right', title: string, ownerName = 'Degdatar'): React.JSX.Element {
  const base = `inventory-book-folio inventory-book-folio--${side}`;
  if (!leaf) return <article className={base + ' is-blank'} />;
  if (leaf.kind === 'cover' || leaf.kind === 'back') {
    return (
      <article className={`${base} inventory-book-folio--cover is-${leaf.kind}`}>
        {leaf.src ? (
          <img className="inventory-book-cover__art" src={leaf.src} alt={`${title} ${leaf.kind === 'cover' ? 'cover' : 'back cover'}`} />
        ) : (
          <div className="inventory-book-cover__leather">
            <img className="inventory-book-cover__logo" src="./SkyUltraLogoMark.png" alt="SkyUltra" draggable={false} />
            {leaf.kind === 'cover' && <strong>{title}</strong>}
            <small>{leaf.kind === 'cover' ? 'SkyUltra Archives' : ownerName}</small>
          </div>
        )}
      </article>
    );
  }
  if (leaf.kind === 'plate') {
    return (
      <article className={`${base} inventory-book-folio--plate`}>
        <div className="inventory-book-plate__frame"><img src={leaf.src} alt={`${title} plate ${leaf.index}`} /></div>
        <small className="inventory-book-plate__caption">Illuminated Plate {leaf.index}</small>
      </article>
    );
  }
  if (leaf.kind === 'finis') {
    return (
      <article className={base}>
        <div className="inventory-book-folio__endpiece">
          <small>The End</small>
        </div>
      </article>
    );
  }
  if (leaf.kind !== 'text') return <article className={base} />;
  return (
    <article className={base}>
      <header className="inventory-book-folio__header">
        <span>{title}</span>
        <small>Folio {leaf.pageNo}</small>
      </header>
      {renderFormattedPageContent(leaf.text, leaf.pageNo === 1)}
      <footer className="inventory-book-folio__footer">
        <span>— {leaf.pageNo} —</span>
      </footer>
    </article>
  );
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
  const [bookSpreadIndex, setBookSpreadIndex] = useState(0);
  const [bookViewMode, setBookViewMode] = useState<'spread' | 'scroll'>('spread');
  const [bookZoomed, setBookZoomed] = useState(false);
  const [bookTurnDir, setBookTurnDir] = useState<'next' | 'prev' | ''>('');
  const bookSpreadMax = useRef(0);
  const [outfitPlayer, setOutfitPlayer] = useState(player), [busy, setBusy] = useState(false), [revision, setRevision] = useState(0);
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
    if (!readingBook) return;
    const onKey = (ev: KeyboardEvent): void => {
      if (ev.key === 'Escape') { ev.preventDefault(); ev.stopPropagation(); setReadingBook(null); return; }
      if (ev.key === 'ArrowRight' || ev.key === 'PageDown') { setBookTurnDir('next'); setBookSpreadIndex(i => Math.min(bookSpreadMax.current, i + 1)); }
      else if (ev.key === 'ArrowLeft' || ev.key === 'PageUp') { setBookTurnDir('prev'); setBookSpreadIndex(i => Math.max(0, i - 1)); }
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [readingBook]);
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
    .sort((a, b) => sort === 'value' ? b.value - a.value : sort === 'weight' ? b.weight - a.weight : sort === 'count' ? b.count - a.count : a.name.localeCompare(b.name)),
  [player, category, wearFilter, query, sort]);
  const items = (remote?.available ? remote.results : fallback.slice(offset, offset + pageSize)).filter(isValidNamedItem);
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
        const cleanTitle = (result.bookTitle || item.name).replace(/^\$+/, '').trim();
        const pages = (result.bookPages && result.bookPages.length > 0 ? result.bookPages : [result.bookText])
          .map(p => p.trim())
          .filter(Boolean);
        const images = result.bookImages ?? [];
        const style: 'book' | 'note' | 'map' =
          result.bookStyle ??
          (images.length > 0 && (/map/i.test(cleanTitle) || pages.length <= 1)
            ? 'map'
            : pages.length > 1
              ? 'book'
              : 'note');
        setReadingBook({
          id: item.id,
          title: cleanTitle,
          text: result.bookText,
          pages: pages.length > 0 ? pages : [result.bookText],
          images,
          style,
        });
        setBookSpreadIndex(0);
        setBookZoomed(false);
        setBookTurnDir('');
        setBookViewMode(style === 'book' && (pages.length > 1 || images.length > 0) ? 'spread' : 'scroll');
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
      <SkyChoice label="Sort inventory" value={sort} onValueChange={setSort} options={[{ value: 'name', label: 'Name A–Z' }, { value: 'value', label: 'Highest value' }, { value: 'weight', label: 'Highest weight' }, { value: 'count', label: 'Largest stack' }]} />
      <span className="pack-filter-state">{categoryLabels[category] ?? category}<i />{wearFilter === 'all' ? 'All gear' : wearFilter === 'equipped' ? 'Equipped' : 'Carried'}</span>
    </div>
    <div className={'inventory-results inventory-results--' + layout}>{items.length ? items.map(item => {
      const isEnchanted = Boolean(item.enchanted || item.enchantments?.length);
      const isStolen = Boolean(item.stolen) && item.id !== '15';
      const enchantSummary = item.enchantments?.length ? item.enchantments.map(e => e.description ? `${e.name}: ${e.description}` : e.name).join(' · ') : item.enchantmentName || 'Enchanted';
      return <button type="button" aria-pressed={item.id === selected?.id} key={item.id} title={(isEnchanted ? `${item.name} (${enchantSummary})` : item.name) + (isStolen ? ' [STOLEN]' : '')} data-category={item.category} data-enchanted={isEnchanted ? 'true' : undefined} data-stolen={isStolen ? 'true' : undefined} className={'item-tile' + (item.id === selected?.id ? ' is-active' : '') + (item.equipped ? ' is-equipped' : '') + (isEnchanted ? ' is-enchanted' : '') + (isStolen ? ' is-stolen' : '') + (activeTab === 'inventory' && item.equipSlots?.length ? ' can-equip' : '')}
        onClick={() => { if (suppressClick.current) { suppressClick.current = false; return; } selectItem(item); }}
        onDragStart={event => event.preventDefault()}
        onPointerDown={event => { suppressClick.current = false; if (activeTab !== 'inventory' || event.button !== 0 || busy || obscured || !item.equipSlots?.length) return; try { event.currentTarget.setPointerCapture(event.pointerId); } catch { /* ignore */ } const image = event.currentTarget.querySelector?.<HTMLImageElement>('.item-thumbnail img'); dragImage.current = image?.getAttribute('src') || undefined; ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight); drag.current = { item, pointer: event.pointerId, x: event.clientX, y: event.clientY, active: false }; }}
        onPointerMove={event => { const current = drag.current; if (!current || current.pointer !== event.pointerId) return; if (!current.active && Math.hypot(event.clientX - current.x, event.clientY - current.y) < 6) return; const starting = !current.active; current.active = true; event.preventDefault(); if (starting) { setDragging(item); selectItem(item); } ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight); if (ghostRef.current) ghostRef.current.style.transform = 'translate3d(' + ghostPoint.current.x + 'px,' + ghostPoint.current.y + 'px,0)'; setHoverSlot(document.elementFromPoint(event.clientX, event.clientY)?.closest<HTMLElement>('[data-equip-slot]')?.dataset.equipSlot ?? ''); }}
        onPointerUp={event => endDrag(event)} onPointerCancel={event => endDrag(event, true)}>
        <span className="item-tile__top"><small>{item.equipped ? <><Icon name="check" size={10} />WORN</> : isStolen ? <span className="item-tile__stolen-tag"><Icon name="stolen" size={10} />STOLEN</span> : (item.armorTypeLabel || item.weaponType || item.category)}</small><span>{isEnchanted && <em className="item-tile__enchant-mark" title={enchantSummary}><Icon name="flame" size={10} /></em>}{item.value.toLocaleString()}<Icon name="gold" size={10} /></span></span>
        <span className="item-tile__image"><ItemThumbnail item={item} bridge={bridge} paused={obscured} />{isEnchanted && <span className="item-tile__enchant-badge" title={enchantSummary} aria-label="Enchanted item"><Icon name="flame" size={11} /></span>}{isStolen && <span className="item-tile__stolen-pill" title="Stolen item"><Icon name="stolen" size={10} />STOLEN</span>}{item.count > 1 && <b className="item-tile__quantity">×{item.count.toLocaleString()}</b>}</span>
        <span className="item-tile__copy"><strong>{isEnchanted && <Icon name="flame" size={10} />}{item.name}</strong><small>{isStolen ? 'Stolen · ' : ''}{item.armorTypeLabel || item.weaponType || item.category}<ItemCombatBadges item={item} compact /><span>{item.weight} wt</span></small></span>
      </button>;
    }) : <Empty title={loading ? 'Reading inventory' : 'No items matched'} detail="Try a different filter or search." icon="inventory" />}</div>
    <div className="roster-footer inventory-pagination"><span>{loading ? 'Refreshing…' : total ? `${offset + 1}–${offset + items.length} of ${total.toLocaleString()}` : '0 items'}{!bridge && player?.inventoryTruncated ? ' · Snapshot is partial' : ''}</span><div><button className="control-button" disabled={offset === 0 || loading} onClick={() => setOffset(value => Math.max(0, value - pageSize))}>Previous</button><button className="control-button" disabled={offset + items.length >= total || loading} onClick={() => setOffset(value => value + items.length)}>Next</button></div></div>
    {activeTab === 'inventory' && <EquipmentSelection player={outfitPlayer} selected={selected} bridge={bridge} obscured={obscured} onInspect={standalone ? undefined : () => { if (selected) setSelectedId(selected.id); setTab('studio'); }} enabled={!!bridge && !busy && !obscured} onChange={(item, slot, unequip) => { void changeEquipment(item, slot, unequip); }} onItemAction={(item, act, count) => { void performItemAction(item, act, count); }} onToggleFavorite={(item) => { void toggleFavorite(item); }} />}
    <div className="inventory-notice" role="status">{actionMessage || readMessage || (activeTab === 'inventory' ? 'Select an item or drag it to a matching equipment slot.' : 'Select an item to open its studio preview.')}</div>
  </section>;
  return <div className={'page inventory-page inventory-page--wardrobe inventory-page--armory' + (standalone ? ' inventory-page--standalone' : '')}>
    <header className="armory-masthead"><div className="armory-heading"><span className="eyebrow">{standalone ? 'SKYRIM PLAYER INVENTORY · LIVE 3D LOADOUT' : 'EQUIPMENT & BELONGINGS'}</span><h1>{activeTab === 'studio' ? 'Item studio' : standalone ? 'Player Inventory' : 'Inventory'}</h1></div>
      {!standalone && <div className="armory-workspaces"><SkySegmentedControl label="Inventory workspace" value={tab} onValueChange={value => { drag.current = null; setDragging(null); setTab(value); }} options={[{ value: 'inventory', label: 'Inventory' }, { value: 'studio', label: 'Item studio' }]} /></div>}
      <div className="armory-masthead__end">
        {(onToggleDisplayMode || onTogglePauseWorld) && (
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

    {readingBook && (() => {
      const hasRealText = Boolean(readingBook.text.trim()) && !/This note or book has no inscription text\./i.test(readingBook.text);
      const isMap = readingBook.style === 'map';
      // Maps only ever show their front face; the back of the sheet is never rendered.
      const shownImages = isMap ? readingBook.images.slice(0, 1) : readingBook.images;
      const useCodex = readingBook.style === 'book' && bookViewMode === 'spread' && (readingBook.pages.length > 1 || readingBook.images.length > 0);
      const leaves = useCodex ? buildBookLeaves(readingBook, hasRealText) : [];
      const totalSpreads = Math.max(1, Math.ceil(leaves.length / 2));
      const clampedSpread = Math.min(bookSpreadIndex, totalSpreads - 1);
      bookSpreadMax.current = totalSpreads - 1;
      const leftLeaf = leaves[clampedSpread * 2];
      const rightLeaf = leaves[clampedSpread * 2 + 1];
      const textPageCount = hasRealText ? readingBook.pages.length : 0;
      const turn = (dir: 'next' | 'prev'): void => {
        setBookTurnDir(dir);
        setBookSpreadIndex(i => dir === 'next' ? Math.min(totalSpreads - 1, i + 1) : Math.max(0, i - 1));
      };
      const eyebrowText =
        isMap
          ? "CARTOGRAPHER'S ARCHIVE · SURVEY PLATE"
          : readingBook.style === 'note'
            ? 'IMPERIAL DISPATCH · PARCHMENT, MISSIVE & RECIPE'
            : 'SKYULTRA CHRONICLE · ILLUMINATED CODEX';
      return (
        <div className={`inventory-book-modal inventory-book-modal--${readingBook.style}`} role="dialog" aria-modal="true" aria-label={readingBook.title} onClick={() => setReadingBook(null)}>
          <div className={`inventory-book-reader inventory-book-reader--${readingBook.style}${useCodex ? ' is-codex' : ''} surface`} onClick={e => e.stopPropagation()}>
            <span className="inventory-book-reader__corner is-tl" aria-hidden="true" />
            <span className="inventory-book-reader__corner is-tr" aria-hidden="true" />
            <span className="inventory-book-reader__corner is-bl" aria-hidden="true" />
            <span className="inventory-book-reader__corner is-br" aria-hidden="true" />
            <header className="inventory-book-reader__header">
              <div className="inventory-book-reader__title-group">
                <span className="eyebrow"><Icon name="book" size={12} /> {eyebrowText}</span>
                <h2>{readingBook.title}</h2>
                <div className="inventory-book-reader__meta">
                  <span>{isMap ? 'Field Survey Plate' : readingBook.style === 'note' ? 'Parchment Manuscript' : 'Bound Tome'}</span>
                  {textPageCount > 0 && <span>{textPageCount} {textPageCount === 1 ? 'folio' : 'folios'}</span>}
                  {!isMap && shownImages.length > 0 && <span>{shownImages.length} {shownImages.length === 1 ? 'engraving' : 'engravings'}</span>}
                </div>
              </div>
              <div className="inventory-book-reader__actions">
                {isMap && shownImages.length > 0 && (
                  <button type="button" className={'book-action-btn' + (bookZoomed ? ' is-active' : '')} onClick={() => setBookZoomed(z => !z)}>
                    <Icon name="search" size={12} />
                    <span>{bookZoomed ? 'Fit Plate' : 'Magnify'}</span>
                  </button>
                )}
                {readingBook.style === 'book' && (readingBook.pages.length > 1 || readingBook.images.length > 0) && (
                  <button type="button" className="book-action-btn" onClick={() => { setBookViewMode(m => m === 'spread' ? 'scroll' : 'spread'); setBookSpreadIndex(0); setBookTurnDir(''); }}>
                    <Icon name="book" size={12} />
                    <span>{bookViewMode === 'spread' ? 'Scroll View' : 'Open Codex'}</span>
                  </button>
                )}
                <button type="button" className="book-action-btn" onClick={() => { const id = readingBook.id; const title = readingBook.title; setReadingBook(null); void performItemAction({ id, name: title, category: 'book', count: 1, value: 0, weight: 0, equipped: false }, 'read', 1, true); }}>
                  <span>Open in Skyrim</span>
                </button>
                <button type="button" className="book-action-btn is-close" onClick={() => setReadingBook(null)} aria-label="Close reader">
                  <span>Close</span><kbd>ESC</kbd>
                </button>
              </div>
            </header>

            <div className="inventory-book-reader__body" tabIndex={0}>
              {useCodex ? (
                <div className="inventory-book-reader__codex-wrap">
                  <div className="inventory-book-reader__binding">
                    <button type="button" className="inventory-book-reader__turn is-prev" disabled={clampedSpread === 0} onClick={() => turn('prev')} aria-label="Previous pages">‹</button>
                    <div key={clampedSpread} className={'inventory-book-reader__codex' + (bookTurnDir ? ` is-turn-${bookTurnDir}` : '')}>
                      {renderBookLeaf(leftLeaf, 'left', readingBook.title, player?.playerName || 'Degdatar')}
                      <div className="inventory-book-reader__spine" aria-hidden="true" />
                      {renderBookLeaf(rightLeaf, 'right', readingBook.title, player?.playerName || 'Degdatar')}
                    </div>
                    <button type="button" className="inventory-book-reader__turn is-next" disabled={clampedSpread >= totalSpreads - 1} onClick={() => turn('next')} aria-label="Next pages">›</button>
                  </div>
                  <div className="inventory-book-reader__pager">
                    <span className="inventory-book-reader__pager-status">{leafLabel(leftLeaf)}{rightLeaf ? ` · ${leafLabel(rightLeaf)}` : ''}</span>
                    <div className="inventory-book-reader__ribbon" aria-hidden="true">
                      {Array.from({ length: totalSpreads }, (_, i) => (
                        <button key={i} type="button" tabIndex={-1} className={i === clampedSpread ? 'is-active' : ''} onClick={() => { setBookTurnDir(i > clampedSpread ? 'next' : 'prev'); setBookSpreadIndex(i); }} />
                      ))}
                    </div>
                    <span className="inventory-book-reader__pager-hint">← → to turn pages</span>
                  </div>
                </div>
              ) : (
                <>
                  {shownImages.length > 0 && (
                    <div className={'inventory-book-reader__gallery' + (bookZoomed ? ' is-zoomed' : '') + (isMap ? ' is-map' : '')}>
                      {shownImages.map((src, idx) => (
                        <figure key={idx} className="inventory-book-reader__figure" onClick={() => setBookZoomed(z => !z)}>
                          <div className="inventory-book-reader__art-frame">
                            <img src={src} alt={`${readingBook.title} plate ${idx + 1}`} />
                          </div>
                          <figcaption>
                            <span>{isMap ? `${readingBook.title} · Cartographic Plate` : `Illuminated Plate ${idx + 1}`}</span>
                            <small>{bookZoomed ? 'Click plate to fit frame' : 'Click plate to magnify'}</small>
                          </figcaption>
                        </figure>
                      ))}
                    </div>
                  )}
                  {(hasRealText || shownImages.length === 0) && (
                    <article className={`inventory-book-parchment inventory-book-parchment--${readingBook.style}`}>
                      <div className="inventory-book-parchment__crest" aria-hidden="true">
                        <span />
                      </div>
                      {readingBook.pages.map((pageText, pIdx) => (
                        <div key={pIdx} className="inventory-book-parchment__section">
                          {readingBook.pages.length > 1 && (
                            <div className="inventory-book-parchment__folio-tag">Folio {pIdx + 1} of {readingBook.pages.length}</div>
                          )}
                          {renderFormattedPageContent(pageText, pIdx === 0)}
                        </div>
                      ))}
                      <div className="inventory-book-parchment__seal" aria-hidden="true">
                        <span>SKYULTRA ARCHIVES</span>
                      </div>
                    </article>
                  )}
                </>
              )}
            </div>
          </div>
        </div>
      );
    })()}
    {dragging && <div ref={ghostRef} className="equipment-drag-ghost" style={{ transform: 'translate3d(' + ghostPoint.current.x + 'px,' + ghostPoint.current.y + 'px,0)' }} aria-hidden="true"><span className="equipment-drag-ghost__art">{dragImage.current ? <ItemImage item={dragging} src={dragImage.current} /> : <ItemThumbnail item={dragging} bridge={bridge} paused={obscured} />}</span><span className="equipment-drag-ghost__copy"><strong>{dragging.name}</strong><small>{hoverSlot ? fitsEquipmentSlot(dragging, hoverSlot) ? 'Release to equip' : 'Incompatible slot' : 'Drag to an equipment slot'}</small></span></div>}
  </div>;
}
