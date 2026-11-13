import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
import {
  createSkyUltraGame,
  type SkyUltraContainerDisplayMode,
  type SkyUltraContainerSnapshot,
  type SkyUltraEquipmentSlot,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { ItemThumbnail, ItemImage } from './ItemThumbnail.js';
import { itemDragPosition } from './itemDrag.js';
import { EquipmentPanel, EquipmentSelection, ItemCombatBadges, fitsEquipmentSlot } from './EquipmentPanel.js';
import { SkyChoice, SkySegmentedControl } from '../core.js';
import { Icon, itemIcon } from './icons.js';
import { Empty } from './pieces.js';
import { formId, type Item, type Npc, type Player } from './model.js';

const pageSize = 64;
const categoryOrder = ['armor', 'weapon', 'ammo', 'miscellaneous', 'potion', 'ingredient', 'book', 'scroll', 'soul gem', 'key'];
const categoryLabels: Record<string, string> = {
  all: 'All items',
  armor: 'Armor',
  weapon: 'Weapons',
  ammo: 'Ammo',
  potion: 'Potions',
  ingredient: 'Ingredients',
  book: 'Books',
  scroll: 'Scrolls',
  'soul gem': 'Soul gems',
  key: 'Keys',
  miscellaneous: 'Misc.',
};

type Drag = { item: Item; pointer: number; x: number; y: number; active: boolean };
type MannequinSourceTab = 'player' | 'mannequin';

export interface MannequinPageProps {
  player: Player | null;
  bridge?: SkyWebViewAPI;
  obscured?: boolean;
  displayMode?: SkyUltraContainerDisplayMode;
  pauseWorld?: boolean;
  onChangeDisplayMode?: (mode: SkyUltraContainerDisplayMode) => void;
  onTogglePauseWorld?: () => void;
  onClose?: () => void;
}

function isValidNamedItem(item: Item | null | undefined): item is Item {
  if (!item) return false;
  const trimmed = item.name?.trim() ?? '';
  if (!trimmed) return false;
  const lower = trimmed.toLowerCase();
  return !lower.startsWith('<missing name') && lower !== 'missing name';
}

const EMPTY_EQUIPMENT: Player['equipment'] = {
  head: null,
  body: null,
  hands: null,
  feet: null,
  neck: null,
  ring: null,
  cape: null,
  lantern: null,
  ammo: null,
  mainHand: null,
  offHand: null,
};

export default function MannequinPage({
  player,
  bridge,
  obscured = false,
  displayMode = 'full',
  pauseWorld = false,
  onChangeDisplayMode,
  onTogglePauseWorld,
  onClose,
}: MannequinPageProps): React.JSX.Element {
  const game = useMemo(() => (bridge ? createSkyUltraGame(bridge) : null), [bridge]);
  const [sourceTab, setSourceTab] = useState<MannequinSourceTab>('player');
  const [snapshot, setSnapshot] = useState<SkyUltraContainerSnapshot | null>(null);
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('all');
  const [sort, setSort] = useState<'name' | 'value' | 'weight' | 'count'>('name');
  const [layout, setLayout] = useState<'grid' | 'list'>('grid');
  const [offset, setOffset] = useState(0);
  const [selectedId, setSelectedId] = useState('');
  const [slotSelection, setSlotSelection] = useState<Item | null>(null);
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [actionMessage, setActionMessage] = useState('');
  const [dragging, setDragging] = useState<Item | null>(null);
  const [hoverSlot, setHoverSlot] = useState('');
  const [revision, setRevision] = useState(0);
  const locked = useRef(false);
  const action = useRef<AbortController | null>(null);
  const drag = useRef<Drag | null>(null);
  const suppressClick = useRef(false);
  const ghostRef = useRef<HTMLDivElement>(null);
  const ghostPoint = useRef({ x: 0, y: 0 });
  const dragImage = useRef<string | undefined>(undefined);

  useEffect(() => () => action.current?.abort(), [bridge]);
  useEffect(() => {
    setOffset(0);
  }, [sourceTab, query, category, sort]);

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
          if (!res.available && res.message) setActionMessage(res.message);
        }
      } catch (error) {
        if (active) {
          setActionMessage(error instanceof Error ? error.message : 'Could not read mannequin inventory.');
        }
      } finally {
        pending = false;
        if (active) setLoading(false);
      }
    };
    void refresh();
    const onVis = (): void => void refresh();
    const unsubscribeVis = bridge.on('skyultra.visibility', onVis);
    const unsubscribeMannequin = bridge.on('skyultra.mannequinMode', onVis);
    document.addEventListener('visibilitychange', onVis);
    return () => {
      active = false;
      controller.abort();
      unsubscribeVis();
      unsubscribeMannequin();
      document.removeEventListener('visibilitychange', onVis);
    };
  }, [game, bridge, obscured, revision]);

  const mannequinName = snapshot?.containerName || 'Armor Mannequin';
  const mannequinRefId = snapshot?.containerRefId || '';
  const mannequinBaseFormId = snapshot?.containerBaseFormId || '';

  const mannequinActor: Npc = useMemo(
    () =>
      snapshot?.actor ?? {
        referenceId: mannequinRefId || '20',
        baseFormId: mannequinBaseFormId || '7',
        name: mannequinName,
        race: 'Mannequin',
        sex: 'None',
        armorRating: snapshot?.armorRating ?? 0,
        equipment: snapshot?.equipment ?? EMPTY_EQUIPMENT,
      },
    [snapshot, mannequinRefId, mannequinBaseFormId, mannequinName],
  );

  const mannequinPlayerProxy: Player = useMemo(
    () => ({
      available: Boolean(snapshot?.available ?? player?.available),
      playerName: mannequinName,
      level: 1,
      location: 'Armor Stand · Motionless Pose',
      carryWeight: snapshot?.playerCarryWeight ?? player?.carryWeight ?? { current: 0, max: 300 },
      health: { current: 100, max: 100 },
      stamina: { current: 100, max: 100 },
      magicka: { current: 100, max: 100 },
      armorRating: snapshot?.armorRating ?? 0,
      gold: snapshot?.playerGold ?? player?.gold ?? 0,
      equipment: { ...EMPTY_EQUIPMENT, ...(snapshot?.equipment ?? {}) },
      inventory: snapshot?.containerItems ?? [],
      inventoryTypeCount: snapshot?.containerTotalTypes ?? 0,
      inventoryItemCount: (snapshot?.containerItems ?? []).reduce((sum, i) => sum + i.count, 0),
      inventoryTruncated: false,
    }),
    [snapshot, player, mannequinName],
  );

  const rawPlayerItems = useMemo(() => {
    const src = snapshot?.available ? snapshot.playerItems : player?.inventory ?? [];
    return src.filter(isValidNamedItem).filter(item => item.id !== '15');
  }, [snapshot, player?.inventory]);

  const rawMannequinItems = useMemo(
    () => (snapshot?.containerItems ?? []).filter(isValidNamedItem),
    [snapshot?.containerItems],
  );

  const activeItems = sourceTab === 'player' ? rawPlayerItems : rawMannequinItems;

  const categories = useMemo(() => {
    const set = new Set<string>();
    for (const item of activeItems) set.add(item.category);
    const sorted = [...set].sort((a, b) => {
      const ai = categoryOrder.indexOf(a);
      const bi = categoryOrder.indexOf(b);
      return (ai < 0 ? 99 : ai) - (bi < 0 ? 99 : bi) || a.localeCompare(b);
    });
    return ['all', ...sorted];
  }, [activeItems]);

  const filtered = useMemo(() => {
    const trimmed = query.trim().toLowerCase();
    return activeItems
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
  }, [activeItems, category, query, sort]);

  const items = useMemo(() => filtered.slice(offset, offset + pageSize), [filtered, offset]);
  const selected = useMemo(
    () =>
      items.find(item => item.id === selectedId) ??
      filtered.find(item => item.id === selectedId) ??
      (slotSelection?.id === selectedId ? slotSelection : null) ??
      items[0] ??
      null,
    [items, filtered, selectedId, slotSelection],
  );

  const selectItem = (item: Item): void => {
    setSelectedId(item.id);
    setSlotSelection(null);
    if (!locked.current) setActionMessage('');
  };

  async function changeMannequinEquipment(item: Item, slot: SkyUltraEquipmentSlot, unequip = false): Promise<void> {
    if (!game || obscured || locked.current || !mannequinRefId) return;
    locked.current = true;
    setBusy(true);
    setActionMessage(
      unequip
        ? `Unequipping ${item.name} from ${mannequinName}…`
        : `Equipping ${item.name} on ${mannequinName}…`,
    );
    action.current?.abort();
    const controller = new AbortController();
    action.current = controller;
    try {
      const result = unequip
        ? await game.unequipActorItem(mannequinRefId, item.id, slot, true, { signal: controller.signal })
        : await game.equipActorItem(mannequinRefId, item.id, slot, { signal: controller.signal });
      if (!result.accepted) {
        setActionMessage(result.message ?? 'Skyrim declined that mannequin equipment change.');
        return;
      }
      await new Promise(resolve => window.setTimeout(resolve, 140));
      if (!controller.signal.aborted) {
        const updated = await game.containerSnapshot({}, { signal: controller.signal });
        if (!controller.signal.aborted && updated.available) {
          setSnapshot(updated);
        }
      }
      setActionMessage(
        result.message ??
          (unequip
            ? `Unequipped ${item.name} from ${mannequinName} and returned to your pack.`
            : `Equipped ${item.name} on ${mannequinName}.`),
      );
    } catch (error) {
      if (!controller.signal.aborted) {
        setActionMessage(error instanceof Error ? error.message : 'Could not update mannequin equipment.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  async function transferItem(item: Item, mode: 'take' | 'store'): Promise<void> {
    if (!game || obscured || locked.current || !mannequinRefId) return;
    locked.current = true;
    setBusy(true);
    setActionMessage(`${mode === 'take' ? 'Taking' : 'Storing'} ${item.name}…`);
    action.current?.abort();
    const controller = new AbortController();
    action.current = controller;
    try {
      const res = await game.containerTransfer(
        {
          mode,
          formId: item.id,
          count: 1,
          containerRefId: mannequinRefId,
          equip: mode === 'store' && Boolean(item.equipSlots?.length),
        },
        { signal: controller.signal },
      );
      if (!res.accepted) {
        setActionMessage(res.message ?? 'Skyrim declined that transfer.');
        return;
      }
      if (res.container && res.container.available) {
        setSnapshot(res.container);
      }
      setActionMessage(res.message ?? `${mode === 'take' ? 'Took' : 'Stored'} ${item.name}.`);
    } catch (error) {
      if (!controller.signal.aborted) {
        setActionMessage(error instanceof Error ? error.message : 'Transfer failed.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  async function takeAllFromMannequin(): Promise<void> {
    if (!game || obscured || locked.current || !mannequinRefId) return;
    locked.current = true;
    setBusy(true);
    setActionMessage(`Taking all gear from ${mannequinName}…`);
    action.current?.abort();
    const controller = new AbortController();
    action.current = controller;
    try {
      const res = await game.containerTransfer(
        { mode: 'takeAll', containerRefId: mannequinRefId },
        { signal: controller.signal },
      );
      if (res.container && res.container.available) setSnapshot(res.container);
      setActionMessage(res.message ?? `Took all gear from ${mannequinName}.`);
    } catch (error) {
      if (!controller.signal.aborted) {
        setActionMessage(error instanceof Error ? error.message : 'Could not take all gear.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  const endDragAtPoint = (pointerId: number, clientX: number, clientY: number, cancel = false, targetEl?: HTMLButtonElement | null): void => {
    const current = drag.current;
    if (!current || current.pointer !== pointerId) return;
    drag.current = null;
    setDragging(null);
    setHoverSlot('');
    try {
      if (targetEl?.hasPointerCapture?.(pointerId)) targetEl.releasePointerCapture(pointerId);
    } catch {
      /* ignore */
    }
    if (!current.active) return;
    suppressClick.current = true;
    const target = cancel ? '' : document.elementFromPoint(clientX, clientY)?.closest<HTMLElement>('[data-equip-slot]')?.dataset.equipSlot ?? '';
    if (fitsEquipmentSlot(current.item, target)) {
      void changeMannequinEquipment(current.item, target as SkyUltraEquipmentSlot, false);
    } else {
      setActionMessage(target ? 'This item does not fit that mannequin slot.' : 'Drop the item onto the mannequin or a matching equipment slot.');
    }
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
      if (starting) {
        setDragging(current.item);
        selectItem(current.item);
      }
      ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight);
      if (ghostRef.current) {
        ghostRef.current.style.transform = `translate3d(${ghostPoint.current.x}px,${ghostPoint.current.y}px,0)`;
      }
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

  // Mark items equipped on the mannequin so EquipmentSelection shows Equip / Unequip accurately
  const selectedForMannequin = useMemo(() => {
    if (!selected) return null;
    const wornOnMannequin =
      Object.values(mannequinPlayerProxy.equipment).some(eq => eq?.id === selected.id) ||
      rawMannequinItems.some(mi => mi.id === selected.id && mi.equipped);
    return { ...selected, equipped: wornOnMannequin };
  }, [selected, mannequinPlayerProxy.equipment, rawMannequinItems]);

  return (
    <div className="page inventory-page inventory-page--wardrobe inventory-page--armory inventory-page--standalone mannequin-page">
      <header className="armory-masthead">
        <div className="armory-heading">
          <span className="eyebrow">SKYRIM ARMOR MANNEQUIN · DRAG OR CLICK TO EQUIP</span>
          <h1>{mannequinName}</h1>
        </div>

        <div className="armory-workspaces">
          <SkySegmentedControl
            label="Inventory source"
            value={sourceTab}
            onValueChange={value => {
              drag.current = null;
              setDragging(null);
              setSourceTab(value as MannequinSourceTab);
            }}
            options={[
              { value: 'player', label: `Your Gear (${rawPlayerItems.length})` },
              { value: 'mannequin', label: `Mannequin (${rawMannequinItems.length})` },
            ]}
          />
        </div>

        <div className="armory-masthead__end">
          <div className="armory-mode-pills" role="group" aria-label="Mannequin actions and world state">
            <button
              type="button"
              className="control-button armory-mode-btn"
              disabled={!bridge || busy || obscured || rawMannequinItems.length === 0}
              onClick={() => void takeAllFromMannequin()}
              title={`Take all items from ${mannequinName}`}
            >
              <Icon name="inventory" size={12} />
              <span>Take All ({rawMannequinItems.length})</span>
            </button>
            {onChangeDisplayMode && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (displayMode === 'windowed' ? ' is-active' : '')}
                onClick={() => onChangeDisplayMode(displayMode === 'windowed' ? 'full' : 'windowed')}
                aria-pressed={displayMode === 'windowed'}
                title="Toggle between Full Screen and Windowed mode"
              >
                <Icon name="widgets" size={12} />
                <span>{displayMode === 'windowed' ? 'Windowed' : 'Full Screen'}</span>
              </button>
            )}
            {onTogglePauseWorld && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (pauseWorld ? ' is-active' : '')}
                onClick={onTogglePauseWorld}
                aria-pressed={!!pauseWorld}
                title={`Toggle pausing the Skyrim world (Current: ${pauseWorld ? 'Pause World' : 'Live World'})`}
              >
                <Icon name="clock" size={12} />
                <span>{pauseWorld ? 'Pause World' : 'Live World'}</span>
              </button>
            )}
          </div>
          {onClose && (
            <button type="button" className="control-button armory-close-btn" onClick={onClose} aria-label="Close mannequin menu">
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          )}
        </div>
      </header>

      <div className="inventory-outfit-workbench">
        <section className="surface inventory-surface" aria-label="Mannequin and Player Gear" aria-busy={loading || busy}>
          <div className="inventory-toolbar">
            <label className="page-search">
              <Icon name="search" size={16} />
              <input
                aria-label="Search gear"
                value={query}
                onChange={event => setQuery(event.currentTarget.value)}
                placeholder={sourceTab === 'player' ? 'Search your gear to dress the mannequin…' : `Search items on ${mannequinName}…`}
              />
              {query ? (
                <button type="button" aria-label="Clear search" onClick={() => setQuery('')}>
                  <Icon name="close" size={12} />
                </button>
              ) : (
                <kbd>Space</kbd>
              )}
            </label>
            <SkySegmentedControl
              label="Inventory layout"
              value={layout}
              onValueChange={setLayout}
              options={[
                { value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' },
                { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' },
              ]}
            />
          </div>

          <div className="pack-categories" role="group" aria-label="Gear categories">
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
              label="Sort gear"
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
              {sourceTab === 'player' ? 'Drag to Mannequin or press Equip' : `${mannequinName} Inventory`}
            </span>
          </div>

          <div className={'inventory-results inventory-results--' + layout}>
            {items.length ? (
              items.map(item => {
                const isEnchanted = Boolean(item.enchanted || item.enchantments?.length);
                const enchantSummary = item.enchantments?.length
                  ? item.enchantments.map(e => (e.description ? `${e.name}: ${e.description}` : e.name)).join(' · ')
                  : item.enchantmentName || 'Enchanted';
                const wornOnMannequin =
                  Object.values(mannequinPlayerProxy.equipment).some(eq => eq?.id === item.id) ||
                  (sourceTab === 'mannequin' && item.equipped);
                return (
                  <button
                    type="button"
                    aria-pressed={item.id === selected?.id}
                    key={item.id}
                    title={isEnchanted ? `${item.name} (${enchantSummary})` : item.name}
                    data-category={item.category}
                    data-enchanted={isEnchanted ? 'true' : undefined}
                    className={
                      'item-tile' +
                      (item.id === selected?.id ? ' is-active' : '') +
                      (wornOnMannequin ? ' is-equipped' : '') +
                      (isEnchanted ? ' is-enchanted' : '') +
                      (item.equipSlots?.length ? ' can-equip' : '')
                    }
                    onClick={() => {
                      if (suppressClick.current) {
                        suppressClick.current = false;
                        return;
                      }
                      selectItem(item);
                    }}
                    onDoubleClick={() => {
                      if (busy || obscured) return;
                      if (item.equipSlots?.length) {
                        void changeMannequinEquipment(item, item.equipSlots[0] ?? 'auto', wornOnMannequin);
                      } else {
                        void transferItem(item, sourceTab === 'mannequin' ? 'take' : 'store');
                      }
                    }}
                    onDragStart={event => event.preventDefault()}
                    onPointerDown={event => {
                      suppressClick.current = false;
                      if (event.button !== 0 || busy || obscured || !item.equipSlots?.length) return;
                      try {
                        event.currentTarget.setPointerCapture(event.pointerId);
                      } catch {
                        /* ignore */
                      }
                      const image = event.currentTarget.querySelector?.<HTMLImageElement>('.item-thumbnail img');
                      dragImage.current = image?.getAttribute('src') || undefined;
                      ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight);
                      drag.current = { item, pointer: event.pointerId, x: event.clientX, y: event.clientY, active: false };
                    }}
                    onPointerMove={event => {
                      const current = drag.current;
                      if (!current || current.pointer !== event.pointerId) return;
                      if (!current.active && Math.hypot(event.clientX - current.x, event.clientY - current.y) < 6) return;
                      const starting = !current.active;
                      current.active = true;
                      event.preventDefault();
                      if (starting) {
                        setDragging(item);
                        selectItem(item);
                      }
                      ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight);
                      if (ghostRef.current) {
                        ghostRef.current.style.transform = `translate3d(${ghostPoint.current.x}px,${ghostPoint.current.y}px,0)`;
                      }
                      setHoverSlot(document.elementFromPoint(event.clientX, event.clientY)?.closest<HTMLElement>('[data-equip-slot]')?.dataset.equipSlot ?? '');
                    }}
                    onPointerUp={event => endDrag(event)}
                    onPointerCancel={event => endDrag(event, true)}
                  >
                    <span className="item-tile__top">
                      <small>
                        {wornOnMannequin ? (
                          <>
                            <Icon name="check" size={10} />
                            ON STAND
                          </>
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
                      {item.count > 1 && <b className="item-tile__quantity">×{item.count.toLocaleString()}</b>}
                    </span>
                    <span className="item-tile__copy">
                      <strong>
                        {isEnchanted && <Icon name="flame" size={10} />}
                        {item.name}
                      </strong>
                      <small>
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
                title={loading ? 'Reading mannequin gear' : sourceTab === 'mannequin' ? `${mannequinName} has no items` : 'No items matched'}
                detail={
                  sourceTab === 'mannequin'
                    ? 'Switch to "Your Gear" above and drag armor or weapons onto the mannequin to dress it.'
                    : 'Try another category or clear the search.'
                }
                icon="inventory"
              />
            )}
          </div>

          <div className="roster-footer inventory-pagination">
            <span>
              {loading
                ? 'Refreshing…'
                : filtered.length
                  ? `${offset + 1}–${offset + items.length} of ${filtered.length.toLocaleString()}`
                  : '0 items'}
            </span>
            <div>
              <button
                type="button"
                className="control-button"
                disabled={offset === 0 || loading}
                onClick={() => setOffset(value => Math.max(0, value - pageSize))}
              >
                Previous
              </button>
              <button
                type="button"
                className="control-button"
                disabled={offset + items.length >= filtered.length || loading}
                onClick={() => setOffset(value => value + items.length)}
              >
                Next
              </button>
            </div>
          </div>

          <EquipmentSelection
            player={mannequinPlayerProxy}
            selected={selectedForMannequin}
            bridge={bridge}
            obscured={obscured}
            enabled={!!bridge && !busy && !obscured}
            onChange={(item, slot, unequip) => {
              void changeMannequinEquipment(item, slot, unequip);
            }}
            transferLabel={sourceTab === 'mannequin' ? 'Take to Pack' : 'Store on Stand'}
            onTransfer={item => {
              void transferItem(item, sourceTab === 'mannequin' ? 'take' : 'store');
            }}
          />

          <div className="inventory-notice" role="status">
            {actionMessage || 'Drag any armor or weapon onto the mannequin or its 11 equipment slots, or press Equip / Unequip.'}
          </div>
        </section>

        <EquipmentPanel
          player={mannequinPlayerProxy}
          bridge={bridge}
          obscured={obscured}
          selected={selectedForMannequin}
          dragging={dragging}
          hoverSlot={hoverSlot}
          busy={busy}
          revision={revision}
          actorOverride={mannequinActor}
          eyebrowOverride="MANNEQUIN DISPLAY · STATIC POSE"
          subtitleOverride="Drag gear onto mannequin or slots"
          hideVitals
          hideStudioControls
          initialMotion={false}
          onSelect={item => {
            selectItem(item);
            setSlotSelection(item);
          }}
          onChange={(item, slot, unequip) => {
            void changeMannequinEquipment(item, slot, unequip);
          }}
        />
      </div>

      {dragging && (
        <div
          ref={ghostRef}
          className={'item-drag-ghost' + (hoverSlot ? (fitsEquipmentSlot(dragging, hoverSlot) ? ' is-valid' : ' is-invalid') : '')}
          style={{ transform: `translate3d(${ghostPoint.current.x}px,${ghostPoint.current.y}px,0)` }}
          aria-hidden="true"
        >
          <span className="item-drag-ghost__art">
            <ItemImage item={dragging} src={dragImage.current} />
          </span>
          <span>
            <strong>{dragging.name}</strong>
            <small>{hoverSlot ? (hoverSlot === 'auto' ? `Equip on ${mannequinName}` : `Drop on ${hoverSlot}`) : `Drag to ${mannequinName}`}</small>
          </span>
        </div>
      )}
    </div>
  );
}
