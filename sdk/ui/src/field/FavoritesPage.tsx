import * as React from 'react';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  createSkyUltraGame,
  readSkyUltraUiPrefs,
  type SkyUltraFavorites,
  type SkyUltraItemAction,
  type SkyUltraSpell,
  type SkyUltraSpellHand,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { SkySegmentedControl } from '../core.js';
import { BookReader, parseBookActionResult, type ReadingBookState } from './BookReader.js';
import { Icon, itemIcon } from './icons.js';
import { ItemThumbnail } from './ItemThumbnail.js';
import { Empty } from './pieces.js';
import type { Item, Palette, Player } from './model.js';
import { HeartIcon, SpellCategoryGlyph, SpellSigil, spellColorStyle, spellCostLabel } from './spellArt.js';
import {
  SpellEquipActions,
  SpellHandBadges,
  applySpellEquippedState,
  computeOptimisticSpellEquipped,
  containerModeLabels,
  nextContainerMode,
  resolveSpellClickHand,
  type ContainerPageProps,
} from './MagicPage.js';

export function applyFavoriteItemEquipment<T extends Pick<Item, 'id' | 'equipped'>>(
  items: T[],
  equipment: Player['equipment'] | Record<string, { id?: string } | null | undefined> | undefined,
): T[] {
  const equippedIds = new Set<string>();
  for (const entry of Object.values(equipment ?? {})) {
    if (entry && typeof entry.id === 'string' && entry.id) {
      equippedIds.add(entry.id);
    }
  }
  let changed = false;
  const next = items.map(item => {
    const equipped = equippedIds.has(item.id);
    if (item.equipped === equipped) return item;
    changed = true;
    return { ...item, equipped };
  });
  return changed ? next : items;
}

type FavoriteEntry =
  | { kind: 'item'; key: string; name: string; item: Item }
  | { kind: 'spell'; key: string; name: string; spell: SkyUltraSpell };

export function resolveFavoriteItemClickAction(
  item: Pick<Item, 'equipSlots' | 'equipped' | 'useAction'>,
  button: 'left' | 'right',
): { kind: 'equip'; slot: 'auto' | 'mainHand' | 'offHand' } | { kind: 'use' } | null {
  const slots = item.equipSlots ?? [];
  const dualWield = slots.includes('mainHand') && slots.includes('offHand');
  if (dualWield) {
    return { kind: 'equip', slot: button === 'right' ? 'mainHand' : 'offHand' };
  }
  if (button === 'right') {
    return slots.includes('mainHand') ? { kind: 'equip', slot: 'mainHand' } : null;
  }
  if (slots.length > 0) return { kind: 'equip', slot: 'auto' };
  if (item.useAction) return { kind: 'use' };
  return null;
}

const useActionLabels: Record<SkyUltraItemAction, string> = {
  drink: 'Drink',
  eat: 'Eat',
  read: 'Read',
  learn: 'Learn',
  activate: 'Use',
  charge: 'Charge',
  drop: 'Drop',
};

function itemSubtitle(item: Item): string {
  const parts = [item.category];
  if (item.count > 1) parts.push(`×${item.count.toLocaleString()}`);
  if (item.enchantmentName) parts.push(item.enchantmentName);
  return parts.join(' · ');
}

const FavoriteEntryItem = React.memo(function FavoriteEntryItem({
  entry,
  index,
  layout,
  isSelected,
  bridge,
  paused,
  actionDisabled,
  onLeftClick,
  onRightDown,
  onRightUp,
  onRemoveFavorite,
}: {
  entry: FavoriteEntry;
  index: number;
  layout: 'grid' | 'list';
  isSelected: boolean;
  bridge?: SkyWebViewAPI;
  paused: boolean;
  actionDisabled: boolean;
  onLeftClick: (entry: FavoriteEntry) => void;
  onRightDown: (key: string) => void;
  onRightUp: (entry: FavoriteEntry) => void;
  onRemoveFavorite: (entry: FavoriteEntry) => void;
}): React.JSX.Element {
  const equipped = entry.kind === 'item'
    ? entry.item.equipped
    : entry.spell.equippedLeft || entry.spell.equippedRight || entry.spell.equippedVoice;
  const spellStyle = entry.kind === 'spell' ? spellColorStyle(entry.spell) : undefined;
  const sub = entry.kind === 'spell'
    ? `${entry.spell.kind === 'shout' ? 'Shout' : entry.spell.kind === 'spell' ? entry.spell.school : 'Power'} · ${spellCostLabel(entry.spell)}`
    : itemSubtitle(entry.item);

  const handleHeartMouseDown = (event: React.MouseEvent): void => {
    event.stopPropagation();
  };
  const handleHeartMouseUp = (event: React.MouseEvent): void => {
    event.stopPropagation();
  };
  const handleHeartClick = (event: React.MouseEvent): void => {
    event.stopPropagation();
    onRemoveFavorite(entry);
  };
  const handleHeartContextMenu = (event: React.MouseEvent): void => {
    event.preventDefault();
    event.stopPropagation();
  };

  if (layout === 'grid') {
    return (
      <div
        role="option"
        aria-selected={isSelected}
        tabIndex={0}
        className={'favorites-tile favorites-tile--' + entry.kind + (isSelected ? ' is-active' : '') + (equipped ? ' is-equipped' : '')}
        style={spellStyle}
        onClick={() => onLeftClick(entry)}
        onMouseDown={e => {
          if (e.button === 2) onRightDown(entry.key);
        }}
        onMouseUp={e => {
          if (e.button === 2) onRightUp(entry);
        }}
        onContextMenu={e => e.preventDefault()}
      >
        <div className="favorites-tile__top">
          <span className="favorites-tile__index">{index < 9 ? `#${index + 1}` : entry.kind.toUpperCase()}</span>
          <div className="favorites-tile__badges">
            {entry.kind === 'spell' ? (
              <SpellHandBadges spell={entry.spell} />
            ) : entry.item.equipped ? (
              <span className="spellbook-hands"><b title="Equipped">E</b></span>
            ) : null}
            <button
              type="button"
              className="favorites-row__heart favorites-tile__heart"
              aria-label={`Remove ${entry.name} from favorites`}
              title="Remove from favorites"
              disabled={actionDisabled}
              onMouseDown={handleHeartMouseDown}
              onMouseUp={handleHeartMouseUp}
              onClick={handleHeartClick}
              onContextMenu={handleHeartContextMenu}
            >
              <HeartIcon filled size={13} />
            </button>
          </div>
        </div>
        <span className="favorites-tile__art">
          {entry.kind === 'spell' ? (
            <SpellSigil spell={entry.spell} size={64} active={isSelected} />
          ) : (
            <ItemThumbnail item={entry.item} bridge={bridge} paused={paused} />
          )}
        </span>
        <span className="favorites-tile__copy">
          <strong>{entry.name}</strong>
          <small>{sub}</small>
        </span>
      </div>
    );
  }

  return (
    <div
      role="option"
      aria-selected={isSelected}
      tabIndex={0}
      className={'favorites-row favorites-row--' + entry.kind + (isSelected ? ' is-active' : '') + (equipped ? ' is-equipped' : '')}
      style={spellStyle}
      onClick={() => onLeftClick(entry)}
      onMouseDown={e => {
        if (e.button === 2) onRightDown(entry.key);
      }}
      onMouseUp={e => {
        if (e.button === 2) onRightUp(entry);
      }}
      onContextMenu={e => e.preventDefault()}
    >
      <span className="favorites-row__index">{index < 9 ? index + 1 : ''}</span>
      <span className="favorites-row__art">
        {entry.kind === 'spell' ? (
          <SpellSigil spell={entry.spell} size={44} active={isSelected} />
        ) : (
          <ItemThumbnail item={entry.item} bridge={bridge} paused={paused} />
        )}
      </span>
      <span className="favorites-row__copy">
        <strong>{entry.name}</strong>
        <small>{sub}</small>
      </span>
      {entry.kind === 'spell' ? <SpellHandBadges spell={entry.spell} /> : entry.item.equipped ? <span className="spellbook-hands"><b title="Equipped">E</b></span> : null}
      <button
        type="button"
        className="favorites-row__heart"
        aria-label={`Remove ${entry.name} from favorites`}
        title="Remove from favorites"
        disabled={actionDisabled}
        onMouseDown={handleHeartMouseDown}
        onMouseUp={handleHeartMouseUp}
        onClick={handleHeartClick}
        onContextMenu={handleHeartContextMenu}
      >
        <HeartIcon filled size={15} />
      </button>
    </div>
  );
});

export default function FavoritesPage({
  player,
  bridge,
  obscured = false,
  displayMode = 'half',
  pauseWorld = true,
  onChangeDisplayMode,
  onTogglePauseWorld,
  onClose,
}: ContainerPageProps): React.JSX.Element {
  const game = useMemo(() => (bridge ? createSkyUltraGame(bridge) : null), [bridge]);
  const [favorites, setFavorites] = useState<SkyUltraFavorites | null>(null);
  const [filter, setFilter] = useState<'all' | 'items' | 'spells'>('all');
  const [query, setQuery] = useState('');
  const [layout, setLayout] = useState<'grid' | 'list'>('list');
  const [selectedKey, setSelectedKey] = useState('');
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [statusMessage, setStatusMessage] = useState('');
  const [revision, setRevision] = useState(0);
  const [readingBook, setReadingBook] = useState<ReadingBookState | null>(null);
  const listRef = useRef<HTMLDivElement | null>(null);
  const locked = useRef(false);
  const mountedRef = useRef(true);
  const hasLoadedRef = useRef(false);
  const lastFavoritesJsonRef = useRef('');
  const rightDownKeyRef = useRef('');
  const equipChainRef = useRef<Promise<void>>(Promise.resolve());
  const obscuredRef = useRef(obscured);
  obscuredRef.current = obscured;
  const actionController = useRef<AbortController | null>(null);
  const uiPrefs = useMemo(() => readSkyUltraUiPrefs(), [readingBook]);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      actionController.current?.abort();
    };
  }, [bridge]);

  useEffect(() => {
    if (!game || !bridge || obscured) return;
    let active = true;
    let pending = false;
    const controller = new AbortController();
    const refresh = async (): Promise<void> => {
      if (pending || document.hidden || !(bridge.getVisibility?.().visible ?? true)) return;
      pending = true;
      if (!hasLoadedRef.current) setLoading(true);
      try {
        const res = await game.favorites({ signal: controller.signal });
        if (active) {
          hasLoadedRef.current = true;
          const nextJson = JSON.stringify(res);
          if (nextJson !== lastFavoritesJsonRef.current) {
            lastFavoritesJsonRef.current = nextJson;
            setFavorites(res);
          }
          if (!res.available && res.message) setStatusMessage(res.message);
        }
      } catch (error) {
        if (active && !controller.signal.aborted) {
          setStatusMessage(error instanceof Error ? error.message : 'Could not read your favorites.');
        }
      } finally {
        pending = false;
        if (active) setLoading(false);
      }
    };
    void refresh();
    const interval = window.setInterval(() => void refresh(), 15000);
    const onVis = (): void => void refresh();
    const unsubscribe = bridge.on('skyultra.visibility', onVis);
    document.addEventListener('visibilitychange', onVis);
    return () => {
      active = false;
      controller.abort();
      window.clearInterval(interval);
      unsubscribe();
      document.removeEventListener('visibilitychange', onVis);
    };
  }, [game, bridge, obscured, revision]);

  useEffect(() => {
    const el = listRef.current;
    if (!el) return;
    let timer = 0;
    const onScroll = (): void => {
      if (!el.classList.contains('is-scrolling')) el.classList.add('is-scrolling');
      if (timer) window.clearTimeout(timer);
      timer = window.setTimeout(() => {
        timer = 0;
        el.classList.remove('is-scrolling');
      }, 140);
    };
    el.addEventListener('scroll', onScroll, { passive: true });
    return () => {
      if (timer) window.clearTimeout(timer);
      el.classList.remove('is-scrolling');
      el.removeEventListener('scroll', onScroll);
    };
  }, []);

  const entries = useMemo<FavoriteEntry[]>(() => {
    const items: FavoriteEntry[] = (favorites?.items ?? []).map(item => ({ kind: 'item', key: 'item:' + item.id, name: item.name, item }));
    const spells: FavoriteEntry[] = (favorites?.spells ?? []).map(spell => ({ kind: 'spell', key: 'spell:' + spell.id, name: spell.name, spell }));
    const list = filter === 'items' ? items : filter === 'spells' ? spells : [...spells, ...items];
    const q = query.trim().toLowerCase();
    const matched = q
      ? list.filter(entry =>
          entry.name.toLowerCase().includes(q) ||
          (entry.kind === 'spell'
            ? (entry.spell.school?.toLowerCase().includes(q) || entry.spell.description?.toLowerCase().includes(q))
            : (entry.item.category.toLowerCase().includes(q) || entry.item.enchantmentName?.toLowerCase().includes(q))),
        )
      : list;
    return matched.sort((a, b) => (a.kind === b.kind ? a.name.localeCompare(b.name) : a.kind === 'spell' ? -1 : 1));
  }, [favorites, filter, query]);

  const selected = useMemo(() => entries.find(e => e.key === selectedKey) ?? entries[0] ?? null, [entries, selectedKey]);
  const itemCount = favorites?.items.length ?? 0;
  const spellCount = favorites?.spells.length ?? 0;

  async function runAction<T extends { accepted: boolean; message?: string }>(
    label: string,
    request: (signal: AbortSignal) => Promise<T>,
    onAccepted?: (result: T) => void,
  ): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true;
    setBusy(true);
    setStatusMessage(label);
    actionController.current?.abort();
    const controller = new AbortController();
    actionController.current = controller;
    try {
      const result = await request(controller.signal);
      if (!controller.signal.aborted && mountedRef.current) {
        setStatusMessage(result.message ?? (result.accepted ? 'Done.' : 'Skyrim declined that action.'));
        if (result.accepted && onAccepted) {
          onAccepted(result);
        } else {
          setRevision(v => v + 1);
        }
      }
    } catch (error) {
      if (!controller.signal.aborted && mountedRef.current) {
        setStatusMessage(error instanceof Error ? error.message : 'The favorite action failed.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted && mountedRef.current) setBusy(false);
    }
  }

  const equipSpell = useCallback((spell: SkyUltraSpell, hand: SkyUltraSpellHand): void => {
    if (!game || obscuredRef.current) return;
    setStatusMessage(`Readying ${spell.name}…`);
    lastFavoritesJsonRef.current = '';
    setFavorites(prev => {
      if (!prev) return prev;
      const optimistic = computeOptimisticSpellEquipped(prev.equipped, spell.id, hand);
      return { ...prev, equipped: optimistic, spells: applySpellEquippedState(prev.spells, optimistic) };
    });
    equipChainRef.current = equipChainRef.current.then(async () => {
      if (!mountedRef.current || obscuredRef.current) return;
      try {
        const result = await game.equipSpell(spell.id, hand);
        if (!mountedRef.current) return;
        setStatusMessage(result.message ?? (result.accepted ? 'Done.' : 'Skyrim declined that action.'));
        if (result.equipped) {
          lastFavoritesJsonRef.current = '';
          const eq = result.equipped;
          setFavorites(prev => (prev ? { ...prev, equipped: eq, spells: applySpellEquippedState(prev.spells, eq) } : prev));
        } else {
          setRevision(v => v + 1);
        }
      } catch (error) {
        if (mountedRef.current) {
          setStatusMessage(error instanceof Error ? error.message : 'The favorite action failed.');
          setRevision(v => v + 1);
        }
      }
    });
  }, [game]);
  const equipItem = useCallback((item: Item, slot: 'auto' | 'mainHand' | 'offHand'): void => {
    if (!game || obscuredRef.current) return;
    const unequip = slot === 'auto' && item.equipped;
    setStatusMessage(`${unequip ? 'Unequipping' : 'Equipping'} ${item.name}…`);
    lastFavoritesJsonRef.current = '';
    setFavorites(prev =>
      prev
        ? {
            ...prev,
            items: prev.items.map(entry => (entry.id === item.id ? { ...entry, equipped: !unequip } : entry)),
          }
        : prev,
    );
    equipChainRef.current = equipChainRef.current.then(async () => {
      if (!mountedRef.current || obscuredRef.current) return;
      try {
        const result = unequip ? await game.unequipItem(item.id, 'auto') : await game.equipItem(item.id, slot);
        if (!mountedRef.current) return;
        setStatusMessage(result.message ?? (result.accepted ? 'Done.' : 'Skyrim declined that action.'));
        if (result.equipment) {
          lastFavoritesJsonRef.current = '';
          const eq = result.equipment;
          setFavorites(prev => (prev ? { ...prev, items: applyFavoriteItemEquipment(prev.items, eq) } : prev));
        } else {
          setRevision(v => v + 1);
        }
      } catch (error) {
        if (mountedRef.current) {
          setStatusMessage(error instanceof Error ? error.message : 'The favorite action failed.');
          setRevision(v => v + 1);
        }
      }
    });
  }, [game]);
  const useItem = useCallback((item: Item, openNative = false): void => {
    if (!game || !item.useAction) return;
    const action = item.useAction;
    void (async () => {
      if (obscured || locked.current) return;
      locked.current = true;
      setBusy(true);
      setStatusMessage(`${useActionLabels[action]} ${item.name}…`);
      actionController.current?.abort();
      const controller = new AbortController();
      actionController.current = controller;
      try {
        const result = await game.itemAction({ formId: item.id, action, openNative }, { signal: controller.signal });
        if (!controller.signal.aborted && mountedRef.current) {
          const parsedBook = !openNative ? parseBookActionResult(item, result) : null;
          if (parsedBook) setReadingBook(parsedBook);
          setStatusMessage(result.message ?? (result.accepted ? 'Done.' : 'Skyrim declined that action.'));
          setRevision(v => v + 1);
        }
      } catch (error) {
        if (!controller.signal.aborted && mountedRef.current) {
          setStatusMessage(error instanceof Error ? error.message : 'The favorite action failed.');
        }
      } finally {
        locked.current = false;
        if (!controller.signal.aborted && mountedRef.current) setBusy(false);
      }
    })();
  }, [game, obscured]);
  const removeFavorite = useCallback((entry: FavoriteEntry): void => {
    if (!game) return;
    const id = entry.kind === 'item' ? entry.item.id : entry.spell.id;
    void runAction(
      `Removing ${entry.name} from favorites…`,
      signal => game.setFavorite(id, entry.kind, false, { signal }),
      () => {
        lastFavoritesJsonRef.current = '';
        setFavorites(prev =>
          prev
            ? entry.kind === 'spell'
              ? { ...prev, spells: prev.spells.filter(spell => spell.id !== id) }
              : { ...prev, items: prev.items.filter(item => item.id !== id) }
            : prev,
        );
      },
    );
  }, [game]);
  const quickUse = (entry: FavoriteEntry): void => {
    if (entry.kind === 'spell') {
      const spell = entry.spell;
      equipSpell(spell, spell.hands.includes('voice') ? 'voice' : spell.twoHanded ? 'both' : 'right');
      return;
    }
    const item = entry.item;
    const slots = item.equipSlots ?? [];
    if (slots.includes('mainHand')) equipItem(item, 'mainHand');
    else if (slots.length) equipItem(item, 'auto');
    else if (item.useAction) useItem(item);
  };

  const handleEntryClick = useCallback((entry: FavoriteEntry, button: 'left' | 'right'): void => {
    setSelectedKey(entry.key);
    if (entry.kind === 'spell') {
      const hand = resolveSpellClickHand(entry.spell, button);
      if (hand) equipSpell(entry.spell, hand);
      return;
    }
    const itemAction = resolveFavoriteItemClickAction(entry.item, button);
    if (!itemAction) return;
    if (itemAction.kind === 'equip') equipItem(entry.item, itemAction.slot);
    else useItem(entry.item);
  }, [equipSpell, equipItem, useItem]);

  const handleEntryLeftClick = useCallback((entry: FavoriteEntry): void => {
    handleEntryClick(entry, 'left');
  }, [handleEntryClick]);

  const handleEntryRightDown = useCallback((key: string): void => {
    rightDownKeyRef.current = key;
  }, []);

  const handleEntryRightUp = useCallback((entry: FavoriteEntry): void => {
    if (rightDownKeyRef.current !== entry.key) return;
    rightDownKeyRef.current = '';
    handleEntryClick(entry, 'right');
  }, [handleEntryClick]);

  const keyHandlerRef = useRef<(event: KeyboardEvent) => void>(() => {});
  keyHandlerRef.current = (event: KeyboardEvent): void => {
    if (busy || obscured || event.ctrlKey || event.altKey || event.metaKey) return;
    if ((event.target as HTMLElement | null)?.closest('input,textarea,select,[contenteditable=true]')) return;
    const index = Number(event.key) - 1;
    if (Number.isInteger(index) && index >= 0 && index < Math.min(9, entries.length)) {
      event.preventDefault();
      setSelectedKey(entries[index].key);
      quickUse(entries[index]);
    } else if (event.key === 'Enter' && selected) {
      event.preventDefault();
      quickUse(selected);
    } else if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      if (!entries.length) return;
      event.preventDefault();
      const current = Math.max(0, entries.findIndex(e => e.key === selected?.key));
      const next = (current + (event.key === 'ArrowDown' ? 1 : -1) + entries.length) % entries.length;
      setSelectedKey(entries[next].key);
    }
  };

  useEffect(() => {
    const onKey = (event: KeyboardEvent): void => keyHandlerRef.current(event);
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const renderItemActions = (item: Item): React.ReactNode => {
    const slots = item.equipSlots ?? [];
    const disabled = !bridge || busy || obscured;
    const handed = slots.includes('mainHand') || slots.includes('offHand');
    return (
      <>
        {handed && slots.includes('offHand') && (
          <button type="button" className="control-button spellbook-equip-btn spellbook-equip-btn--left" disabled={disabled} onClick={() => equipItem(item, 'offHand')}>
            <kbd>L</kbd>
            <span>Equip Left</span>
          </button>
        )}
        {handed && slots.includes('mainHand') && (
          <button type="button" className="control-button is-active spellbook-equip-btn spellbook-equip-btn--right" disabled={disabled} onClick={() => equipItem(item, 'mainHand')}>
            <span>Equip Right</span>
            <kbd>R</kbd>
          </button>
        )}
        {!handed && slots.length > 0 && (
          <button type="button" className={'control-button spellbook-equip-btn' + (item.equipped ? '' : ' is-active')} disabled={disabled} onClick={() => equipItem(item, 'auto')}>
            <Icon name={item.equipped ? 'close' : 'check'} size={13} />
            <span>{item.equipped ? 'Unequip' : 'Equip'}</span>
          </button>
        )}
        {item.useAction && !handed && (
          <button type="button" className="control-button is-active spellbook-equip-btn" disabled={disabled} onClick={() => useItem(item)}>
            <Icon name={itemIcon(item.category)} size={13} />
            <span>{useActionLabels[item.useAction]}</span>
          </button>
        )}
      </>
    );
  };

  return (
    <div
      className={`page inventory-page inventory-page--armory inventory-page--standalone favorites-page favorites-page--${displayMode}`}
      data-container-mode={displayMode}
      onContextMenu={e => {
        if (!(e.target as HTMLElement | null)?.closest('input,textarea')) e.preventDefault();
      }}
    >
      <div className="spellbook-backdrop favorites-backdrop" aria-hidden="true">
        <i /><i /><i />
      </div>
      <header className="armory-masthead spellbook-masthead favorites-masthead">
        <div className="armory-heading">
          <span className="eyebrow">FAVORITES · QUICK ACCESS</span>
          <h1>
            <HeartIcon filled size={22} /> Favorites
          </h1>
        </div>
        <div className="armory-masthead__end spellbook-masthead__end">
          <div className="armory-mode-pills" role="group" aria-label="Favorites layout and world state">
            {onChangeDisplayMode && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (displayMode !== 'half' ? ' is-active' : '')}
                onClick={() => onChangeDisplayMode(nextContainerMode(displayMode))}
                title={`Cycle layout: Half / Full / Windowed (Current: ${containerModeLabels[displayMode]})`}
              >
                <Icon name="widgets" size={12} />
                <span>{containerModeLabels[displayMode]}</span>
              </button>
            )}
            {onTogglePauseWorld && (
              <button
                type="button"
                className={'control-button armory-mode-btn' + (pauseWorld ? ' is-active' : '')}
                onClick={onTogglePauseWorld}
                aria-pressed={!!pauseWorld}
                title={`Toggle pausing the world while favorites are open (Current: ${pauseWorld ? 'Pause World' : 'Live World'})`}
              >
                <Icon name="clock" size={12} />
                <span>{pauseWorld ? 'Pause World' : 'Live World'}</span>
              </button>
            )}
          </div>
          {onClose && (
            <button type="button" className="control-button armory-close-btn" onClick={onClose} aria-label="Close favorites">
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          )}
        </div>
      </header>

      <section className="surface spellbook-surface favorites-surface" aria-busy={loading || busy} aria-label="Favorites">
        <div className="inventory-toolbar favorites-toolbar">
          <div className="favorites-categories" role="group" aria-label="Favorite types">
            <button
              type="button"
              className="favorites-category-btn"
              aria-pressed={filter === 'all'}
              onClick={() => setFilter('all')}
            >
              <HeartIcon filled={filter === 'all'} size={14} />
              <span>All</span>
              <small>{itemCount + spellCount}</small>
            </button>
            <button
              type="button"
              className="favorites-category-btn"
              aria-pressed={filter === 'spells'}
              onClick={() => setFilter('spells')}
            >
              <SpellCategoryGlyph category="all" size={14} />
              <span>Spells</span>
              <small>{spellCount}</small>
            </button>
            <button
              type="button"
              className="favorites-category-btn"
              aria-pressed={filter === 'items'}
              onClick={() => setFilter('items')}
            >
              <Icon name="inventory" size={14} />
              <span>Items</span>
              <small>{itemCount}</small>
            </button>
          </div>
          <label className="page-search favorites-search">
            <Icon name="search" size={15} />
            <input
              aria-label="Search favorites"
              value={query}
              onChange={event => setQuery(event.currentTarget.value)}
              placeholder="Search favorites…"
            />
            {query ? (
              <button type="button" aria-label="Clear favorites search" onClick={() => setQuery('')}>
                <Icon name="close" size={12} />
              </button>
            ) : (
              <kbd>Space</kbd>
            )}
          </label>
          <SkySegmentedControl
            label="Favorites layout"
            value={layout}
            onValueChange={setLayout}
            options={[
              { value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' },
              { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' },
            ]}
          />
        </div>

        <div
          ref={listRef}
          className={'favorites-list favorites-list--' + layout}
          role="listbox"
          aria-label="Favorite entries"
          onMouseUp={e => {
            if (e.button === 2) rightDownKeyRef.current = '';
          }}
        >
          {entries.length ? (
            entries.map((entry, index) => (
              <FavoriteEntryItem
                key={entry.key}
                entry={entry}
                index={index}
                layout={layout}
                isSelected={entry.key === selected?.key}
                bridge={bridge}
                paused={obscured}
                actionDisabled={!bridge || busy || obscured}
                onLeftClick={handleEntryLeftClick}
                onRightDown={handleEntryRightDown}
                onRightUp={handleEntryRightUp}
                onRemoveFavorite={removeFavorite}
              />
            ))
          ) : (
            <Empty
              title={loading ? 'Gathering your favorites' : query ? 'No favorites matched' : 'No favorites yet'}
              detail={query ? 'Try a different filter or clear the search.' : 'Mark items in the Inventory or spells in the Spellbook with the heart icon and they will appear here for quick access.'}
              icon="heart"
            />
          )}
        </div>

        {selected && (
          <div className="spellbook-selection favorites-selection" style={selected.kind === 'spell' ? spellColorStyle(selected.spell) : undefined}>
            <div className="spellbook-selection__hero">
              {selected.kind === 'spell' ? (
                <SpellSigil spell={selected.spell} size={84} active hero />
              ) : (
                <span className="favorites-selection__item-art"><ItemThumbnail item={selected.item} bridge={bridge} paused={obscured} /></span>
              )}
            </div>
            <div className="spellbook-selection__copy">
              <span className="eyebrow">{selected.kind === 'spell' ? 'FAVORITE SPELL' : `FAVORITE ${selected.item.category.toUpperCase()}`}</span>
              <h3>{selected.name}</h3>
              {selected.kind === 'spell' ? (
                selected.spell.description && <p className="spellbook-description">{selected.spell.description}</p>
              ) : (
                <p className="spellbook-description">
                  {selected.item.enchantments?.length
                    ? selected.item.enchantments.map(e => (e.description ? `${e.name}: ${e.description}` : e.name)).join(' · ')
                    : `${selected.item.weight} weight · ${selected.item.value.toLocaleString()} gold${selected.item.count > 1 ? ` · ${selected.item.count} carried` : ''}`}
                </p>
              )}
            </div>
            <div className="spellbook-selection__actions">
              {selected.kind === 'spell' ? (
                <SpellEquipActions spell={selected.spell} disabled={!bridge || busy || obscured} onEquip={equipSpell} />
              ) : (
                renderItemActions(selected.item)
              )}
              <button
                type="button"
                className="control-button spellbook-fav-btn is-favorite"
                disabled={!bridge || busy || obscured}
                aria-label="Remove from favorites"
                title="Remove from favorites"
                onClick={() => removeFavorite(selected)}
              >
                <HeartIcon filled size={16} />
              </button>
            </div>
          </div>
        )}

        <div className="inventory-notice spellbook-notice" role="status">
          <span>{statusMessage || `${entries.length} favorite${entries.length === 1 ? '' : 's'} · Left-click: Left hand / Equip / Use · Right-click: Right hand · 1–9 quick-use`}</span>
        </div>
      </section>
      {readingBook && (
        <BookReader
          book={readingBook}
          playerName={player?.playerName}
          theme={(uiPrefs.defaultTheme as Palette) || 'dragonstone'}
          design={uiPrefs.bookReaderDesign}
          themeSync={uiPrefs.bookThemeSync}
          onOpenNative={(book) => useItem({ id: book.id, name: book.title, category: 'book', count: 1, value: 0, weight: 0, equipped: false, useAction: 'read' }, true)}
          onClose={() => setReadingBook(null)}
        />
      )}
    </div>
  );
}
