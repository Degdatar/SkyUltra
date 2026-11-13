import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
import {
  createSkyUltraGame,
  type SkyUltraBarterDisplayMode,
  type SkyUltraBarterSnapshot,
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

const displayModeLabels: Record<SkyUltraBarterDisplayMode, string> = {
  half: 'Half Container',
  windowed: 'Windowed',
  full: 'Full Screen',
};

export default function BarterPage({
  player,
  bridge,
  obscured = false,
  displayMode = 'half',
  pauseWorld = false,
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
  const [side, setSide] = useState<'vendor' | 'player'>('vendor');
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('all');
  const [sort, setSort] = useState<'name' | 'value' | 'weight' | 'count'>('name');
  const [layout, setLayout] = useState<'grid' | 'list'>('grid');
  const [selectedId, setSelectedId] = useState('');
  const [tradeQuantity, setTradeQuantity] = useState(1);
  const [offset, setOffset] = useState(0);
  const [snapshot, setSnapshot] = useState<SkyUltraBarterSnapshot | null>(null);
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [statusMessage, setStatusMessage] = useState('');
  const [pendingConfirmSell, setPendingConfirmSell] = useState<{
    item: Item;
    count: number;
    availableGold: number;
    totalGold: number;
  } | null>(null);
  const [revision, setRevision] = useState(0);
  const locked = useRef(false);
  const activeController = useRef<AbortController | null>(null);

  useEffect(() => () => {
    activeController.current?.abort();
  }, [bridge]);

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
      setLoading(true);
      try {
        const res = await game.barterSnapshot(undefined, { signal: controller.signal });
        if (active) {
          setSnapshot(res);
          if (!res.available && res.message) {
            setStatusMessage(res.message);
          }
        }
      } catch (error) {
        if (active) {
          setStatusMessage(error instanceof Error ? error.message : 'Could not read vendor inventory.');
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
    const unsubscribeBarter = bridge.on('skyultra.barterMode', onVis);
    document.addEventListener('visibilitychange', onVis);
    return () => {
      active = false;
      controller.abort();
      unsubscribeVis();
      unsubscribeBarter();
      document.removeEventListener('visibilitychange', onVis);
    };
  }, [game, bridge, obscured, revision]);

  const rawItems: Item[] = useMemo(() => {
    const source =
      side === 'vendor'
        ? snapshot?.vendorItems ?? []
        : snapshot?.playerItems && snapshot.playerItems.length > 0
          ? snapshot.playerItems
          : player?.inventory ?? [];
    return source.filter(item => {
      const trimmed = item.name?.trim() ?? '';
      if (!trimmed) return false;
      const lower = trimmed.toLowerCase();
      return !lower.startsWith('<missing name') && lower !== 'missing name';
    });
  }, [side, snapshot, player]);

  const categories = useMemo(() => {
    const rawCats =
      side === 'vendor'
        ? snapshot?.vendorCategories ?? [...new Set(rawItems.map(i => i.category))]
        : snapshot?.playerCategories ?? [...new Set(rawItems.map(i => i.category))];
    return ['all', ...[...new Set(rawCats)].sort((a, b) => {
      const ai = categoryOrder.indexOf(a);
      const bi = categoryOrder.indexOf(b);
      return (ai < 0 ? 99 : ai) - (bi < 0 ? 99 : bi) || a.localeCompare(b);
    })];
  }, [side, snapshot, rawItems]);

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    return rawItems
      .filter(item => category === 'all' || item.category === category)
      .filter(item => !q || `${item.name} ${item.category} ${item.id} ${formId(item.id)}`.toLowerCase().includes(q))
      .sort((a, b) =>
        sort === 'value'
          ? b.value - a.value
          : sort === 'weight'
            ? b.weight - a.weight
            : sort === 'count'
              ? b.count - a.count
              : a.name.localeCompare(b.name),
      );
  }, [rawItems, category, query, sort]);

  const pagedItems = useMemo(() => filtered.slice(offset, offset + pageSize), [filtered, offset]);
  const selected = useMemo(
    () => pagedItems.find(i => i.id === selectedId) ?? filtered.find(i => i.id === selectedId) ?? pagedItems[0] ?? null,
    [pagedItems, filtered, selectedId],
  );

  useEffect(() => {
    setTradeQuantity(1);
    setPendingConfirmSell((prev) => (prev && prev.item.id === selected?.id ? prev : null));
  }, [selected?.id, side]);

  const maxQuantity = Math.max(1, selected?.count ?? 1);
  const clampedQuantity = Math.max(1, Math.min(maxQuantity, tradeQuantity));
  const showQuantitySlider = Boolean(selected && selected.count > 5);
  const unitPrice = selected?.value ?? 0;
  const totalTransactionGold = unitPrice * clampedQuantity;

  useEffect(() => {
    setPendingConfirmSell(null);
  }, [clampedQuantity]);

  const vendorName = snapshot?.vendorName || 'Skyrim Merchant';
  const vendorGold = snapshot?.vendorGold ?? 0;
  const playerName = snapshot?.playerName || player?.playerName || 'Dragonborn';
  const playerGold = snapshot?.playerGold ?? player?.gold ?? 0;
  const carryCurrent = Math.round(snapshot?.playerCarryWeight?.current ?? player?.carryWeight?.current ?? 0);
  const carryMax = Math.max(1, Math.round(snapshot?.playerCarryWeight?.max ?? player?.carryWeight?.max ?? 300));

  const cycleDisplayMode = (): void => {
    if (!onChangeDisplayMode) return;
    const order: SkyUltraBarterDisplayMode[] = ['half', 'windowed', 'full'];
    const next = order[(order.indexOf(displayMode) + 1) % order.length];
    onChangeDisplayMode(next);
  };

  async function executeTrade(item: Item, count: number, allowPartialGold = false): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true;
    setBusy(true);
    const actionType = side === 'vendor' ? 'buy' : 'sell';
    const safeCount = Math.max(1, Math.min(item.count, count));
    setStatusMessage(`${actionType === 'buy' ? 'Purchasing' : 'Selling'} ${item.name}${safeCount > 1 ? ` ×${safeCount}` : ''}…`);
    const controller = new AbortController();
    activeController.current = controller;
    try {
      const res = await game.barterTransaction(
        {
          action: actionType,
          formId: item.id,
          count: safeCount,
          vendorReferenceId: snapshot?.vendorReferenceId,
          allowPartialGold,
        },
        { signal: controller.signal },
      );
      if (!res.accepted) {
        if (
          actionType === 'sell' &&
          !allowPartialGold &&
          (res.requiresConfirmation || (res.message ?? '').toLowerCase().includes('only has'))
        ) {
          const availGold = res.availableVendorGold ?? res.vendorGold ?? vendorGold;
          const reqGold = res.totalGold ?? item.value * safeCount;
          setPendingConfirmSell({
            item,
            count: safeCount,
            availableGold: availGold,
            totalGold: reqGold,
          });
          setStatusMessage(
            res.message ??
              `The merchant only has ${availGold.toLocaleString()} gold available (worth ${reqGold.toLocaleString()} gold). Confirm to sell anyway.`,
          );
          return;
        }
        setPendingConfirmSell(null);
        setStatusMessage(res.message ?? 'Transaction was declined.');
        return;
      }
      setPendingConfirmSell(null);
      if (res.barter) {
        setSnapshot(res.barter);
      } else {
        setRevision(v => v + 1);
      }
      setStatusMessage(res.message ?? `${actionType === 'buy' ? 'Purchased' : 'Sold'} ${item.name}.`);
      setTradeQuantity(1);
    } catch (error) {
      if (!controller.signal.aborted) {
        setStatusMessage(error instanceof Error ? error.message : 'Barter transaction failed.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  const sliderPercent = maxQuantity > 1 ? Math.round(((clampedQuantity - 1) / (maxQuantity - 1)) * 100) : 100;
  const selectedEnchanted = Boolean(selected && (selected.enchanted || selected.enchantments?.length));
  const selectedEnchantCount = selected?.enchantments?.length ?? 0;

  return (
    <div
      className={`page inventory-page inventory-page--wardrobe inventory-page--armory inventory-page--standalone barter-page barter-page--${displayMode}`}
      data-barter-mode={displayMode}
    >
      <header className="armory-masthead barter-masthead">
        <div className="armory-heading">
          <span className="eyebrow">
            {side === 'vendor'
              ? `MERCHANT STOCK · BUYING FROM ${vendorName.toUpperCase()}`
              : `PLAYER BELONGINGS · SELLING TO ${vendorName.toUpperCase()}`}
          </span>
          <h1>{side === 'vendor' ? vendorName : `${playerName}'s Pack`}</h1>
        </div>

        <div className="armory-masthead__end barter-masthead__end">
          <div className="armory-mode-pills" role="group" aria-label="Barter container layout and world state">
            {onChangeDisplayMode && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (displayMode !== 'half' ? ' is-active' : '')}
                onClick={cycleDisplayMode}
                title={`Cycle Barter Container Mode: Half Container / Full Container / Windowed Container (Current: ${displayModeLabels[displayMode]})`}
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
                title={`Toggle pausing the Skyrim world while trading (Current: ${pauseWorld ? 'Pause World' : 'Live World'})`}
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
            <button type="button" className="control-button armory-close-btn" onClick={onClose} aria-label="Close barter menu">
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          )}
        </div>
      </header>

      <div className="inventory-outfit-workbench barter-workbench">
        <section className="surface inventory-surface barter-surface" aria-label="Merchant Barter Container" aria-busy={loading || busy}>
          <div className="inventory-toolbar">
            <label className="page-search">
              <Icon name="search" size={16} />
              <input
                aria-label="Search barter items"
                value={query}
                onChange={e => setQuery(e.currentTarget.value)}
                placeholder={side === 'vendor' ? `Search ${vendorName}'s wares…` : 'Search your items to sell…'}
              />
              {query ? (
                <button type="button" aria-label="Clear barter search" onClick={() => setQuery('')}>
                  <Icon name="close" size={12} />
                </button>
              ) : (
                <kbd>Space</kbd>
              )}
            </label>
            <SkySegmentedControl
              label="Barter layout"
              value={layout}
              onValueChange={setLayout}
              options={[
                { value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' },
                { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' },
              ]}
            />
          </div>

          <div className="pack-categories" role="group" aria-label="Barter item categories">
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
              label="Sort barter items"
              value={sort}
              onValueChange={v => setSort(v as 'name' | 'value' | 'weight' | 'count')}
              options={[
                { value: 'name', label: 'Name A–Z' },
                { value: 'value', label: 'Highest price' },
                { value: 'weight', label: 'Highest weight' },
                { value: 'count', label: 'Largest stack' },
              ]}
            />
            <span className="pack-filter-state">
              {categoryLabels[category] ?? category}
              <i />
              {side === 'vendor' ? 'Merchant Stock · Click item to Buy' : 'Your Pack · Click item to Sell'}
            </span>
          </div>

          <div className={'inventory-results inventory-results--' + layout}>
            {pagedItems.length ? (
              pagedItems.map(item => {
                const isEnchanted = Boolean(item.enchanted || item.enchantments?.length);
                const isStolen = side === 'player' && Boolean(item.stolen) && item.id !== '15';
                const enchantSummary = item.enchantments?.length
                  ? item.enchantments.map(e => (e.description ? `${e.name}: ${e.description}` : e.name)).join(' · ')
                  : item.enchantmentName || 'Enchanted';
                const tradeVerb = side === 'vendor' ? 'Buy' : 'Sell';
                return (
                  <button
                    type="button"
                    key={item.id}
                    aria-pressed={item.id === selected?.id}
                    title={
                      (isEnchanted ? `${item.name} (${enchantSummary})` : item.name) +
                      (isStolen ? ' [STOLEN]' : '') +
                      ` — Click to ${tradeVerb}${item.count > 1 ? ' (double-click for full stack)' : ''}`
                    }
                    data-category={item.category}
                    data-enchanted={isEnchanted ? 'true' : undefined}
                    data-stolen={isStolen ? 'true' : undefined}
                    className={
                      'item-tile' +
                      (item.id === selected?.id ? ' is-active' : '') +
                      (item.equipped ? ' is-equipped' : '') +
                      (isEnchanted ? ' is-enchanted' : '') +
                      (isStolen ? ' is-stolen' : '')
                    }
                    onMouseEnter={() => {
                      if (!busy && !pendingConfirmSell && selectedId !== item.id) {
                        setSelectedId(item.id);
                      }
                    }}
                    onClick={() => {
                      if (busy || obscured) return;
                      if (pendingConfirmSell && pendingConfirmSell.item.id === item.id) {
                        const pending = pendingConfirmSell;
                        setPendingConfirmSell(null);
                        void executeTrade(pending.item, pending.count, true);
                        return;
                      }
                      setPendingConfirmSell(null);
                      setSelectedId(item.id);
                      const qty = selected?.id === item.id && showQuantitySlider ? clampedQuantity : 1;
                      void executeTrade(item, qty);
                    }}
                    onDoubleClick={() => {
                      if (!busy && !obscured) void executeTrade(item, item.count);
                    }}
                  >
                    <span className="item-tile__top">
                      <small>
                        {item.equipped ? (
                          <>
                            <Icon name="check" size={10} />
                            WORN
                          </>
                        ) : isStolen ? (
                          <span className="item-tile__stolen-tag">
                            <Icon name="stolen" size={10} />
                            STOLEN
                          </span>
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
                title={loading ? 'Reading merchant inventory' : side === 'vendor' ? 'No merchant wares matched' : 'No salable items matched'}
                detail={
                  side === 'vendor'
                    ? 'Try another category or switch to your player inventory below to sell items.'
                    : 'Try another category or switch back to the vendor tab to buy items.'
                }
                icon="inventory"
              />
            )}
          </div>

          <div className="roster-footer inventory-pagination">
            <span>
              {loading
                ? 'Synchronizing ledger…'
                : filtered.length
                  ? `${offset + 1}–${offset + pagedItems.length} of ${filtered.length.toLocaleString()} (${side === 'vendor' ? 'Merchant Wares' : 'Your Salable Pack'})`
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
            aria-label="Selected barter item"
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
                    ? side === 'vendor'
                      ? 'MERCHANT OFFERING · FOR SALE'
                      : 'YOUR BELONGING · READY TO SELL'
                    : side === 'vendor'
                      ? 'MERCHANT WARES'
                      : 'PLAYER BELONGINGS'}
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
                      <strong>{unitPrice.toLocaleString()}</strong>
                      <small>{side === 'vendor' ? 'BUY PRICE' : 'SELL VALUE'}</small>
                    </span>
                    {showQuantitySlider && (
                      <span>
                        <Icon name="gold" size={11} />
                        <strong>{totalTransactionGold.toLocaleString()}</strong>
                        <small>TOTAL ({clampedQuantity}×)</small>
                      </span>
                    )}
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
                  {selected?.name ?? (side === 'vendor' ? 'Select an item to purchase' : 'Select an item from your pack to sell')}
                </h3>
                <span className="selection-category">
                  {selected
                    ? `${selected.armorTypeLabel || selected.weaponType || selected.category}${selected.speed ? ` · Speed ${selected.speed}` : ''}${selected.enchantmentName ? ` · ${selected.enchantmentName}` : ''}${selected.teachesSpell ? ` · Teaches ${selected.teachesSpell}` : ''}`
                    : 'Use the Vendor and Player tabs below to switch between buying and selling'}
                </span>
              </div>

              {selected && selectedEnchanted && (
                <div className="selection-enchantments" aria-label="Item enchantments" tabIndex={0}>
                  {selected.enchantments?.length ? (
                    selected.enchantments.map((eff, idx) => (
                      <div key={`${eff.name}-${idx}`} className="selection-enchantment">
                        <Icon name="flame" size={11} />
                        <strong>{eff.name}</strong>
                        {eff.description && <span>{eff.description}</span>}
                      </div>
                    ))
                  ) : (
                    <div className="selection-enchantment">
                      <Icon name="flame" size={11} />
                      <strong>{selected.enchantmentName || 'Enchanted Item'}</strong>
                      <span>Arcane enchantment active</span>
                    </div>
                  )}
                </div>
              )}

              {showQuantitySlider && (
                <div className="barter-quantity-slider" role="group" aria-label="Transaction quantity selector">
                  <div className="barter-quantity-slider__head">
                    <span className="barter-quantity-slider__label">
                      {side === 'vendor' ? 'QUANTITY TO BUY' : 'QUANTITY TO SELL'}
                    </span>
                    <div className="barter-quantity-slider__presets">
                      <button type="button" className={clampedQuantity === 1 ? 'is-active' : ''} onClick={() => setTradeQuantity(1)}>
                        1
                      </button>
                      <button
                        type="button"
                        className={clampedQuantity === Math.max(1, Math.floor(maxQuantity / 2)) ? 'is-active' : ''}
                        onClick={() => setTradeQuantity(Math.max(1, Math.floor(maxQuantity / 2)))}
                      >
                        Half ({Math.max(1, Math.floor(maxQuantity / 2))})
                      </button>
                      <button
                        type="button"
                        className={clampedQuantity === maxQuantity ? 'is-active' : ''}
                        onClick={() => setTradeQuantity(maxQuantity)}
                      >
                        All ({maxQuantity})
                      </button>
                    </div>
                    <strong className="barter-quantity-slider__readout">
                      ×{clampedQuantity.toLocaleString()} · <Icon name="gold" size={11} /> {totalTransactionGold.toLocaleString()}
                    </strong>
                  </div>
                  <div className="barter-quantity-slider__track-row">
                    <button
                      type="button"
                      className="barter-quantity-slider__step"
                      disabled={clampedQuantity <= 1}
                      onClick={() => setTradeQuantity(q => Math.max(1, q - 1))}
                      aria-label="Decrease quantity"
                    >
                      −
                    </button>
                    <input
                      type="range"
                      min={1}
                      max={maxQuantity}
                      value={clampedQuantity}
                      aria-label="Select item quantity"
                      style={{ '--barter-slider-pct': `${sliderPercent}%` } as React.CSSProperties}
                      onChange={e => setTradeQuantity(Math.max(1, Math.min(maxQuantity, Number(e.currentTarget.value) || 1)))}
                    />
                    <button
                      type="button"
                      className="barter-quantity-slider__step"
                      disabled={clampedQuantity >= maxQuantity}
                      onClick={() => setTradeQuantity(q => Math.min(maxQuantity, q + 1))}
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
                {pendingConfirmSell && pendingConfirmSell.item.id === selected.id && side === 'player' ? (
                  <>
                    <button
                      type="button"
                      className="control-button is-active barter-trade-btn barter-trade-btn--confirm"
                      disabled={!bridge || busy || obscured}
                      onClick={() => void executeTrade(pendingConfirmSell.item, pendingConfirmSell.count, true)}
                      title={`Sell anyway for ${pendingConfirmSell.availableGold.toLocaleString()} gold`}
                    >
                      <Icon name="check" size={14} />
                      <span>Confirm to Sell ({pendingConfirmSell.availableGold.toLocaleString()}g)</span>
                    </button>
                    <button
                      type="button"
                      className="control-button barter-trade-btn--cancel"
                      disabled={busy}
                      onClick={() => {
                        setPendingConfirmSell(null);
                        setStatusMessage('Sale cancelled.');
                      }}
                    >
                      <span>Cancel</span>
                    </button>
                  </>
                ) : (
                  <button
                    type="button"
                    className="control-button is-active barter-trade-btn"
                    disabled={!bridge || busy || obscured}
                    onClick={() => void executeTrade(selected, showQuantitySlider ? clampedQuantity : 1)}
                  >
                    <Icon name="gold" size={14} />
                    <span>
                      {side === 'vendor' ? 'Buy' : 'Sell'}
                      {showQuantitySlider && clampedQuantity > 1 ? ` ×${clampedQuantity.toLocaleString()}` : ''}
                    </span>
                  </button>
                )}
              </div>
            )}
          </div>

          <div className={'inventory-notice barter-notice' + (pendingConfirmSell ? ' is-confirm-prompt' : '')} role="status">
            <span>
              {statusMessage ||
                (side === 'vendor'
                  ? `Viewing ${vendorName}'s merchant stock. Click ${playerName} below to sell items from your inventory.`
                  : `Viewing ${playerName}'s inventory. Click ${vendorName} below to buy from the merchant.`)}
            </span>
            {pendingConfirmSell && side === 'player' && (
              <div className="barter-notice__actions">
                <button
                  type="button"
                  className="control-button is-active barter-notice__confirm-btn"
                  disabled={!bridge || busy || obscured}
                  onClick={() => void executeTrade(pendingConfirmSell.item, pendingConfirmSell.count, true)}
                >
                  <Icon name="check" size={12} />
                  <span>Confirm to Sell ({pendingConfirmSell.availableGold.toLocaleString()}g)</span>
                </button>
                <button
                  type="button"
                  className="control-button barter-notice__cancel-btn"
                  disabled={busy}
                  onClick={() => {
                    setPendingConfirmSell(null);
                    setStatusMessage('Sale cancelled.');
                  }}
                >
                  <span>Cancel</span>
                </button>
              </div>
            )}
          </div>

          <footer className="barter-trader-switcher" role="tablist" aria-label="Switch between Vendor and Player inventories">
            <button
              type="button"
              role="tab"
              aria-selected={side === 'vendor'}
              className={'barter-trader-tab' + (side === 'vendor' ? ' is-active' : '')}
              onClick={() => setSide('vendor')}
            >
              <div className="barter-trader-tab__identity">
                <span className="barter-trader-tab__role">VENDOR INVENTORY · BUY WARES</span>
                <strong className="barter-trader-tab__name">{vendorName}</strong>
              </div>
              <div className="barter-trader-tab__meta">
                <span className="barter-trader-tab__count">{(snapshot?.vendorItems?.length ?? 0).toLocaleString()} items</span>
                <span className="barter-trader-tab__gold">
                  <Icon name="gold" size={14} />
                  <b>{vendorGold.toLocaleString()}</b>
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
                <span className="barter-trader-tab__role">PLAYER INVENTORY · SELL BELONGINGS</span>
                <strong className="barter-trader-tab__name">{playerName}</strong>
              </div>
              <div className="barter-trader-tab__meta">
                <span className="barter-trader-tab__count">
                  {(snapshot?.playerItems?.length ?? player?.inventoryTypeCount ?? 0).toLocaleString()} items
                </span>
                <span className="barter-trader-tab__gold">
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
