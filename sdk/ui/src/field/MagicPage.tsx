import * as React from 'react';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  createSkyUltraGame,
  type SkyUltraContainerDisplayMode,
  type SkyUltraSpell,
  type SkyUltraSpellBook,
  type SkyUltraSpellHand,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { SkyChoice, SkySegmentedControl } from '../core.js';
import { Icon } from './icons.js';
import { Empty } from './pieces.js';
import type { Player } from './model.js';
import {
  HeartIcon,
  SpellCategoryGlyph,
  SpellDeleteIcon,
  SpellRenameIcon,
  SpellSigil,
  categoryColorStyle,
  spellCastingLabels,
  spellCategoryLabels,
  spellColorStyle,
  spellCostLabel,
  spellDeliveryLabels,
  type SpellRailCategory,
} from './spellArt.js';

const EMPTY_SPELLS: SkyUltraSpell[] = [];
const EMPTY_ACTIVE_EFFECTS: NonNullable<Player['activeEffects']> = [];

/** Maps Left/Right click in Magic and Favorites to Skyrim equip slots. */
export function resolveSpellClickHand(
  spell: Pick<SkyUltraSpell, 'kind' | 'hands' | 'twoHanded'>,
  button: 'left' | 'right',
): SkyUltraSpellHand | null {
  const isVoice =
    spell.kind === 'shout' ||
    spell.kind === 'power' ||
    spell.kind === 'lesserPower' ||
    spell.hands.includes('voice');
  if (isVoice) {
    return button === 'left' ? 'voice' : null;
  }
  if (spell.twoHanded || (spell.hands.length === 1 && spell.hands[0] === 'both')) {
    return 'both';
  }
  return button === 'right' ? 'right' : 'left';
}

export function computeOptimisticSpellEquipped(
  current: SkyUltraSpellBook['equipped'] | undefined,
  spellId: string,
  hand: SkyUltraSpellHand,
): SkyUltraSpellBook['equipped'] {
  const base: SkyUltraSpellBook['equipped'] = {
    left: current?.left ?? null,
    right: current?.right ?? null,
    voice: current?.voice ?? null,
  };
  if (hand === 'voice') return { ...base, voice: spellId };
  if (hand === 'both') return { ...base, left: spellId, right: spellId };
  if (hand === 'left') return { ...base, left: spellId };
  return { ...base, right: spellId };
}

export function applySpellEquippedState(
  spells: SkyUltraSpell[],
  equipped: SkyUltraSpellBook['equipped'],
): SkyUltraSpell[] {
  let changed = false;
  const next = spells.map(spell => {
    const equippedLeft = equipped.left === spell.id;
    const equippedRight = equipped.right === spell.id;
    const equippedVoice = equipped.voice === spell.id;
    if (
      spell.equippedLeft === equippedLeft &&
      spell.equippedRight === equippedRight &&
      spell.equippedVoice === equippedVoice
    ) {
      return spell;
    }
    changed = true;
    return { ...spell, equippedLeft, equippedRight, equippedVoice };
  });
  return changed ? next : spells;
}

export const containerModeLabels: Record<SkyUltraContainerDisplayMode, string> = {
  half: 'Half Container',
  windowed: 'Windowed',
  full: 'Full Screen',
};
export function nextContainerMode(mode: SkyUltraContainerDisplayMode): SkyUltraContainerDisplayMode {
  return mode === 'half' ? 'windowed' : mode === 'windowed' ? 'full' : 'half';
}

const railOrder: SpellRailCategory[] = [
  'all',
  'favorites',
  'alteration',
  'conjuration',
  'destruction',
  'illusion',
  'restoration',
  'powers',
  'shouts',
  'other',
  'effects',
];
const schoolOrder: Record<string, number> = { alteration: 0, conjuration: 1, destruction: 2, illusion: 3, restoration: 4, powers: 5, shouts: 6, other: 7 };

export interface ContainerPageProps {
  player: Player | null;
  bridge?: SkyWebViewAPI;
  obscured?: boolean;
  displayMode?: SkyUltraContainerDisplayMode;
  pauseWorld?: boolean;
  onChangeDisplayMode?: (mode: SkyUltraContainerDisplayMode) => void;
  onTogglePauseWorld?: () => void;
  onClose?: () => void;
}

export const SpellHandBadges = React.memo(function SpellHandBadges({ spell }: { spell: SkyUltraSpell }): React.JSX.Element | null {
  if (!spell.equippedLeft && !spell.equippedRight && !spell.equippedVoice) return null;
  return (
    <span className="spellbook-hands" aria-label="Equipped slots">
      {spell.equippedLeft && <b title="Equipped in left hand">L</b>}
      {spell.equippedRight && <b title="Equipped in right hand">R</b>}
      {spell.equippedVoice && <b title="Equipped as power / shout">V</b>}
    </span>
  );
});

const SpellTile = React.memo(function SpellTile({
  spell,
  isSelected,
  sigilSize,
  onLeftClick,
  onRightDown,
  onRightUp,
}: {
  spell: SkyUltraSpell;
  isSelected: boolean;
  sigilSize: number;
  onLeftClick: (spell: SkyUltraSpell) => void;
  onRightDown: (id: string) => void;
  onRightUp: (spell: SkyUltraSpell) => void;
}): React.JSX.Element {
  const isVoice =
    spell.kind === 'shout' ||
    spell.kind === 'power' ||
    spell.kind === 'lesserPower' ||
    spell.hands.includes('voice');
  const equipHint = isVoice
    ? 'Click to equip'
    : spell.twoHanded
      ? 'Click to equip (Both hands)'
      : 'Left-click: Left hand · Right-click: Right hand';
  return (
    <button
      type="button"
      className={'spellbook-tile' + (isSelected ? ' is-active' : '') + (spell.favorite ? ' is-favorite' : '')}
      style={spellColorStyle(spell)}
      aria-pressed={isSelected}
      title={spell.description ? `${spell.description}\n(${equipHint})` : `${spell.name} (${equipHint})`}
      onClick={() => onLeftClick(spell)}
      onMouseDown={e => {
        if (e.button === 2) onRightDown(spell.id);
      }}
      onMouseUp={e => {
        if (e.button === 2) onRightUp(spell);
      }}
      onContextMenu={e => e.preventDefault()}
    >
      <SpellSigil spell={spell} size={sigilSize} active={isSelected} />
      <span className="spellbook-tile__copy">
        <strong>{spell.name}</strong>
        <small>
          {spell.level ?? spellCategoryLabels[spell.category as SpellRailCategory] ?? spell.category}
          <span>{spellCostLabel(spell)}</span>
        </small>
      </span>
      <SpellHandBadges spell={spell} />
      {spell.favorite && <span className="spellbook-tile__fav"><HeartIcon filled size={12} /></span>}
    </button>
  );
});

/** Equip buttons shared by the Magic and Favorites pages. */
export function SpellEquipActions({
  spell,
  disabled,
  onEquip,
}: {
  spell: SkyUltraSpell;
  disabled: boolean;
  onEquip: (spell: SkyUltraSpell, hand: SkyUltraSpellHand) => void;
}): React.JSX.Element {
  if (spell.hands.includes('voice')) {
    return (
      <button type="button" className="control-button is-active spellbook-equip-btn" disabled={disabled || spell.equippedVoice} onClick={() => onEquip(spell, 'voice')}>
        <SpellCategoryGlyph category={spell.kind === 'shout' ? 'shouts' : 'powers'} size={14} />
        <span>{spell.equippedVoice ? 'Equipped' : spell.kind === 'shout' ? 'Equip Shout' : 'Equip Power'}</span>
      </button>
    );
  }
  if (spell.twoHanded || (spell.hands.length === 1 && spell.hands[0] === 'both')) {
    return (
      <button type="button" className="control-button is-active spellbook-equip-btn" disabled={disabled} onClick={() => onEquip(spell, 'both')}>
        <Icon name="target" size={14} />
        <span>Equip Both Hands</span>
      </button>
    );
  }
  return (
    <>
      <button
        type="button"
        className={'control-button spellbook-equip-btn spellbook-equip-btn--left' + (spell.equippedLeft ? ' is-equipped' : '')}
        disabled={disabled}
        onClick={() => onEquip(spell, 'left')}
        title="Equip to left hand"
      >
        <kbd>L</kbd>
        <span>Equip Left</span>
      </button>
      <button
        type="button"
        className={'control-button is-active spellbook-equip-btn spellbook-equip-btn--right' + (spell.equippedRight ? ' is-equipped' : '')}
        disabled={disabled}
        onClick={() => onEquip(spell, 'right')}
        title="Equip to right hand"
      >
        <span>Equip Right</span>
        <kbd>R</kbd>
      </button>
      <button
        type="button"
        className="control-button spellbook-equip-btn spellbook-equip-btn--both"
        disabled={disabled}
        onClick={() => onEquip(spell, 'both')}
        title="Equip to both hands (dual cast)"
      >
        <span>Both</span>
      </button>
    </>
  );
}

export default function MagicPage({
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
  const [book, setBook] = useState<SkyUltraSpellBook | null>(null);
  const [category, setCategory] = useState<SpellRailCategory>('all');
  const [query, setQuery] = useState('');
  const [sort, setSort] = useState<'name' | 'cost' | 'school'>('school');
  const [layout, setLayout] = useState<'grid' | 'list'>('grid');
  const [selectedId, setSelectedId] = useState('');
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [statusMessage, setStatusMessage] = useState('');
  const [revision, setRevision] = useState(0);
  const [renamingSpellId, setRenamingSpellId] = useState<string | null>(null);
  const [renameDraft, setRenameDraft] = useState('');
  const [confirmDeleteId, setConfirmDeleteId] = useState<string | null>(null);
  const renameInputRef = useRef<HTMLInputElement | null>(null);
  const resultsRef = useRef<HTMLDivElement | null>(null);
  const locked = useRef(false);
  const mountedRef = useRef(true);
  const hasLoadedRef = useRef(false);
  const lastBookJsonRef = useRef('');
  const rightDownIdRef = useRef('');
  const equipChainRef = useRef<Promise<void>>(Promise.resolve());
  const obscuredRef = useRef(obscured);
  obscuredRef.current = obscured;
  const actionController = useRef<AbortController | null>(null);

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
        const res = await game.spells({ signal: controller.signal });
        if (active) {
          hasLoadedRef.current = true;
          const nextJson = JSON.stringify(res);
          if (nextJson !== lastBookJsonRef.current) {
            lastBookJsonRef.current = nextJson;
            setBook(res);
          }
          if (!res.available && res.message) setStatusMessage(res.message);
        }
      } catch (error) {
        if (active && !controller.signal.aborted) {
          setStatusMessage(error instanceof Error ? error.message : 'Could not read your spellbook.');
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
    const el = resultsRef.current;
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
  }, [category]);

  const spells = book?.spells ?? EMPTY_SPELLS;
  const counts = useMemo(() => {
    const result: Partial<Record<SpellRailCategory, number>> = { all: spells.length, favorites: 0, effects: player?.activeEffects?.length ?? 0 };
    for (const spell of spells) {
      const key = spell.category as SpellRailCategory;
      result[key] = (result[key] ?? 0) + 1;
      if (spell.favorite) result.favorites = (result.favorites ?? 0) + 1;
    }
    return result;
  }, [spells, player?.activeEffects]);
  const rail = useMemo(() => railOrder.filter(id => id === 'all' || id === 'favorites' || id === 'effects' || (counts[id] ?? 0) > 0 || id !== 'other'), [counts]);

  const filtered = useMemo(() => {
    const needle = query.trim().toLowerCase();
    const list = spells.filter(spell => {
      if (category === 'favorites' && !spell.favorite) return false;
      if (category !== 'all' && category !== 'favorites' && category !== 'effects' && spell.category !== category) return false;
      if (!needle) return true;
      return (
        spell.name.toLowerCase().includes(needle) ||
        (spell.description ?? '').toLowerCase().includes(needle) ||
        spell.effects.some(e => e.name.toLowerCase().includes(needle))
      );
    });
    return [...list].sort((a, b) => {
      if (sort === 'cost') return b.cost - a.cost || a.name.localeCompare(b.name);
      if (sort === 'school') return (schoolOrder[a.category] ?? 9) - (schoolOrder[b.category] ?? 9) || (a.skillLevel ?? 0) - (b.skillLevel ?? 0) || a.name.localeCompare(b.name);
      return a.name.localeCompare(b.name);
    });
  }, [spells, category, query, sort]);

  const selected = useMemo(() => filtered.find(s => s.id === selectedId) ?? filtered[0] ?? null, [filtered, selectedId]);
  const activeEffects = player?.activeEffects ?? EMPTY_ACTIVE_EFFECTS;
  const magicka = book?.magicka ?? player?.magicka ?? { current: 0, max: 1 };
  const magickaPct = Math.max(0, Math.min(100, Math.round((magicka.current / Math.max(1, magicka.max)) * 100)));
  const playerName = player?.playerName || 'Dragonborn';

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
        setStatusMessage(error instanceof Error ? error.message : 'The spell action failed.');
      }
    } finally {
      locked.current = false;
      if (!controller.signal.aborted && mountedRef.current) setBusy(false);
    }
  }

  const equip = useCallback((spell: SkyUltraSpell, hand: SkyUltraSpellHand): void => {
    if (!game || obscuredRef.current) return;
    setStatusMessage(`Readying ${spell.name}…`);
    lastBookJsonRef.current = '';
    setBook(prev => {
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
          lastBookJsonRef.current = '';
          const eq = result.equipped;
          setBook(prev => (prev ? { ...prev, equipped: eq, spells: applySpellEquippedState(prev.spells, eq) } : prev));
        } else {
          setRevision(v => v + 1);
        }
      } catch (error) {
        if (mountedRef.current) {
          setStatusMessage(error instanceof Error ? error.message : 'The spell action failed.');
          setRevision(v => v + 1);
        }
      }
    });
  }, [game]);
  const toggleFavorite = (spell: SkyUltraSpell): void => {
    if (!game) return;
    const nextFav = !spell.favorite;
    void runAction(
      spell.favorite ? `Removing ${spell.name} from favorites…` : `Adding ${spell.name} to favorites…`,
      signal => game.setFavorite(spell.id, 'spell', nextFav, { signal }),
      result => {
        const appliedFav = typeof result.favorite === 'boolean' ? result.favorite : nextFav;
        lastBookJsonRef.current = '';
        setBook(prev =>
          prev
            ? {
                ...prev,
                spells: prev.spells.map(entry => (entry.id === spell.id ? { ...entry, favorite: appliedFav } : entry)),
              }
            : prev,
        );
      },
    );
  };
  const defaultEquip = (spell: SkyUltraSpell): void => equip(spell, spell.hands.includes('voice') ? 'voice' : spell.twoHanded ? 'both' : 'right');

  const handleTileLeftClick = useCallback((spell: SkyUltraSpell): void => {
    setSelectedId(spell.id);
    const hand = resolveSpellClickHand(spell, 'left');
    if (hand) equip(spell, hand);
  }, [equip]);

  const handleTileRightDown = useCallback((id: string): void => {
    rightDownIdRef.current = id;
  }, []);

  const handleTileRightUp = useCallback((spell: SkyUltraSpell): void => {
    if (rightDownIdRef.current !== spell.id) return;
    rightDownIdRef.current = '';
    setSelectedId(spell.id);
    const hand = resolveSpellClickHand(spell, 'right');
    if (hand) {
      equip(spell, hand);
    } else {
      setStatusMessage('');
    }
  }, [equip]);

  const beginRenameSpell = (spell: SkyUltraSpell): void => {
    setConfirmDeleteId(null);
    setRenamingSpellId(spell.id);
    setRenameDraft(spell.name);
    window.setTimeout(() => {
      renameInputRef.current?.focus();
      renameInputRef.current?.select();
    }, 10);
  };

  const commitRenameSpell = (spell: SkyUltraSpell): void => {
    if (!game) return;
    const trimmed = renameDraft.trim();
    if (!trimmed || trimmed === spell.name) {
      setRenamingSpellId(null);
      return;
    }
    setRenamingSpellId(null);
    void runAction(`Renaming ${spell.name} to "${trimmed}"…`, signal =>
      game.renameSpell(spell.id, trimmed, { signal }),
    );
  };

  const deleteSpellEntry = (spell: SkyUltraSpell): void => {
    if (!game) return;
    setConfirmDeleteId(null);
    setRenamingSpellId(null);
    void runAction(`Deleting ${spell.name}…`, signal =>
      game.deleteSpell(spell.id, spell.kind, { signal }),
    );
  };

  const deleteActiveEffectEntry = (effect: { id: string; name: string }): void => {
    if (!game) return;
    setConfirmDeleteId(null);
    void runAction(`Dispelling ${effect.name}…`, signal =>
      game.deleteSpell(effect.id, 'effect', { signal }),
    );
  };

  useEffect(() => {
    setRenamingSpellId(null);
    setConfirmDeleteId(null);
  }, [selected?.id, category]);

  const keyHandlerRef = useRef<(event: KeyboardEvent) => void>(() => {});
  keyHandlerRef.current = (event: KeyboardEvent): void => {
    if (!selected || busy || obscured || event.ctrlKey || event.altKey || event.metaKey) return;
    if ((event.target as HTMLElement | null)?.closest('input,textarea,select,[contenteditable=true]')) return;
    const key = event.key.toLowerCase();
    if (key === 'f') { event.preventDefault(); toggleFavorite(selected); }
    else if (key === 'r') { event.preventDefault(); beginRenameSpell(selected); }
    else if (key === 'delete' || key === 'x') {
      event.preventDefault();
      if (confirmDeleteId === selected.id) deleteSpellEntry(selected);
      else setConfirmDeleteId(selected.id);
    }
    else if (key === 'enter') { event.preventDefault(); defaultEquip(selected); }
    else if (!selected.hands.includes('voice') && !selected.twoHanded && (key === 'q' || key === 'e')) { event.preventDefault(); equip(selected, key === 'q' ? 'left' : 'right'); }
  };

  useEffect(() => {
    const onKey = (event: KeyboardEvent): void => keyHandlerRef.current(event);
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  return (
    <div
      className={`page inventory-page inventory-page--armory inventory-page--standalone spellbook-page spellbook-page--${displayMode}`}
      data-container-mode={displayMode}
      style={categoryColorStyle(category)}
      onContextMenu={e => {
        if (!(e.target as HTMLElement | null)?.closest('input,textarea')) e.preventDefault();
      }}
    >
      <div className="spellbook-backdrop" aria-hidden="true">
        <i /><i /><i />
      </div>
      <header className="armory-masthead spellbook-masthead">
        <div className="armory-heading">
          <span className="eyebrow">SPELLBOOK · {spellCategoryLabels[category].toUpperCase()}</span>
          <h1>{playerName}'s Spellbook</h1>
        </div>
        <div className="armory-masthead__end spellbook-masthead__end">
          <div className="spellbook-magicka" title={`Magicka ${magicka.current} / ${magicka.max}`}>
            <span className="spellbook-magicka__label">MAGICKA</span>
            <span className="spellbook-magicka__track"><i style={{ width: `${magickaPct}%` }} /></span>
            <strong>{Math.round(magicka.current)} / {Math.round(magicka.max)}</strong>
          </div>
          <div className="armory-mode-pills" role="group" aria-label="Spellbook layout and world state">
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
                title={`Toggle pausing the world while the spellbook is open (Current: ${pauseWorld ? 'Pause World' : 'Live World'})`}
              >
                <Icon name="clock" size={12} />
                <span>{pauseWorld ? 'Pause World' : 'Live World'}</span>
              </button>
            )}
          </div>
          {onClose && (
            <button type="button" className="control-button armory-close-btn" onClick={onClose} aria-label="Close spellbook">
              <span>Close</span>
              <kbd>ESC</kbd>
            </button>
          )}
        </div>
      </header>

      <div className="spellbook-workbench">
        <nav className="spellbook-rail" aria-label="Spell schools">
          {rail.map(id => (
            <button
              type="button"
              key={id}
              className={'spellbook-rail__item spellbook-rail__item--' + id}
              aria-pressed={category === id}
              style={categoryColorStyle(id)}
              onClick={() => setCategory(id)}
              title={spellCategoryLabels[id]}
            >
              <span className="spellbook-rail__icon"><SpellCategoryGlyph category={id} size={20} /></span>
              <span className="spellbook-rail__label">{spellCategoryLabels[id]}</span>
              <small>{counts[id] ?? 0}</small>
            </button>
          ))}
        </nav>

        <section className="surface spellbook-surface" aria-busy={loading || busy} aria-label="Spells">
          {category === 'effects' ? (
            <div className="spellbook-effects">
              {activeEffects.length ? (
                activeEffects.map(effect => (
                  <div key={effect.id} className={'spellbook-effect' + (effect.hostile ? ' is-hostile' : '')}>
                    <span className="spellbook-effect__icon"><SpellCategoryGlyph category="effects" size={18} /></span>
                    <span className="spellbook-effect__copy">
                      <strong>{effect.name}</strong>
                      <small>
                        {effect.magnitude ? `Magnitude ${Math.round(effect.magnitude)}` : 'Active'}
                        {effect.duration > 0 ? ` · ${Math.max(0, Math.round(effect.remaining))}s of ${Math.round(effect.duration)}s` : ''}
                      </small>
                    </span>
                    {effect.duration > 0 && (
                      <span className="spellbook-effect__timer">
                        <i style={{ width: `${Math.max(0, Math.min(100, (effect.remaining / Math.max(1, effect.duration)) * 100))}%` }} />
                      </span>
                    )}
                    <button
                      type="button"
                      className={'control-button spellbook-delete-btn spellbook-effect__delete' + (confirmDeleteId === effect.id ? ' is-confirming' : '')}
                      disabled={!bridge || busy || obscured}
                      title={confirmDeleteId === effect.id ? `Click again to dispel & delete ${effect.name}` : `Dispel / delete active effect ${effect.name}`}
                      onClick={() => {
                        if (confirmDeleteId === effect.id) deleteActiveEffectEntry(effect);
                        else setConfirmDeleteId(effect.id);
                      }}
                    >
                      <SpellDeleteIcon size={14} />
                      <span>{confirmDeleteId === effect.id ? 'Confirm Delete' : 'Delete Effect'}</span>
                    </button>
                  </div>
                ))
              ) : (
                <Empty title="No active effects" detail="Potions, spells, blessings and diseases affecting you appear here." icon="performance" />
              )}
            </div>
          ) : (
            <>
              <div className="inventory-toolbar spellbook-toolbar">
                <label className="page-search">
                  <Icon name="search" size={16} />
                  <input aria-label="Search spells" value={query} onChange={e => setQuery(e.currentTarget.value)} placeholder="Search spells, effects…" />
                  {query ? (
                    <button type="button" aria-label="Clear spell search" onClick={() => setQuery('')}>
                      <Icon name="close" size={12} />
                    </button>
                  ) : (
                    <kbd>Space</kbd>
                  )}
                </label>
                <SkyChoice
                  label="Sort spells"
                  value={sort}
                  onValueChange={v => setSort(v as 'name' | 'cost' | 'school')}
                  options={[
                    { value: 'school', label: 'By school' },
                    { value: 'name', label: 'Name A–Z' },
                    { value: 'cost', label: 'Highest cost' },
                  ]}
                />
                <SkySegmentedControl
                  label="Spell layout"
                  value={layout}
                  onValueChange={setLayout}
                  options={[
                    { value: 'grid', label: <Icon name="grid" size={14} />, ariaLabel: 'Grid view' },
                    { value: 'list', label: <Icon name="list" size={14} />, ariaLabel: 'List view' },
                  ]}
                />
              </div>

              <div
                ref={resultsRef}
                className={'spellbook-results spellbook-results--' + layout}
                onMouseUp={e => {
                  if (e.button === 2) rightDownIdRef.current = '';
                }}
              >
                {filtered.length ? (
                  filtered.map(spell => (
                    <SpellTile
                      key={spell.id}
                      spell={spell}
                      isSelected={spell.id === selected?.id}
                      sigilSize={layout === 'grid' ? 64 : 40}
                      onLeftClick={handleTileLeftClick}
                      onRightDown={handleTileRightDown}
                      onRightUp={handleTileRightUp}
                    />
                  ))
                ) : (
                  <Empty
                    title={loading ? 'Reading your spellbook' : category === 'favorites' ? 'No favorite spells yet' : 'No spells matched'}
                    detail={category === 'favorites' ? 'Select a spell and press the heart (or F) to add it to your favorites.' : 'Try another school or clear the search.'}
                    icon="book"
                  />
                )}
              </div>

              <div className="spellbook-selection" style={spellColorStyle(selected)} aria-label="Selected spell">
                {selected ? (
                  <>
                    <div className="spellbook-selection__hero">
                      <SpellSigil spell={selected} size={displayMode === 'half' ? 104 : 132} active hero />
                    </div>
                    <div className="spellbook-selection__copy">
                      <span className="eyebrow">
                        {selected.kind === 'shout' ? 'DRAGON SHOUT' : selected.kind === 'power' ? 'GREATER POWER' : selected.kind === 'lesserPower' ? 'LESSER POWER' : `${selected.school.toUpperCase()}${selected.level ? ' · ' + selected.level.toUpperCase() : ''}`}
                      </span>
                      {renamingSpellId === selected.id ? (
                        <form
                          className="spellbook-rename-bar"
                          onSubmit={e => {
                            e.preventDefault();
                            commitRenameSpell(selected);
                          }}
                        >
                          <SpellRenameIcon size={16} />
                          <input
                            ref={renameInputRef}
                            type="text"
                            className="spellbook-rename-input"
                            value={renameDraft}
                            maxLength={96}
                            aria-label={`Rename ${selected.name}`}
                            placeholder="Enter new spell, shout, or power name…"
                            onChange={e => setRenameDraft(e.currentTarget.value)}
                            onKeyDown={e => {
                              if (e.key === 'Escape') {
                                e.preventDefault();
                                e.stopPropagation();
                                setRenamingSpellId(null);
                              }
                            }}
                          />
                          <button type="submit" className="control-button is-active" disabled={!renameDraft.trim()}>
                            <span>Save</span>
                          </button>
                          <button type="button" className="control-button" onClick={() => setRenamingSpellId(null)}>
                            <span>Cancel</span>
                          </button>
                        </form>
                      ) : (
                        <h3>{selected.name}</h3>
                      )}
                      <div className="spellbook-stats">
                        {selected.kind === 'spell' && <span><strong>{spellCostLabel(selected)}</strong><small>MAGICKA</small></span>}
                        {selected.kind !== 'spell' && <span><strong>{spellCostLabel(selected)}</strong><small>{selected.kind === 'shout' ? 'COOLDOWN' : 'USE'}</small></span>}
                        <span><strong>{spellCastingLabels[selected.castingType]}</strong><small>CASTING</small></span>
                        <span><strong>{spellDeliveryLabels[selected.delivery]}</strong><small>DELIVERY</small></span>
                        {selected.chargeTime ? <span><strong>{selected.chargeTime.toFixed(1)}s</strong><small>CHARGE</small></span> : null}
                      </div>
                      {selected.description && <p className="spellbook-description">{selected.description}</p>}
                      {selected.words && selected.words.length > 0 && (
                        <div className="spellbook-words" aria-label="Words of power">
                          {selected.words.map((w, i) => (
                            <span key={i} className={'spellbook-word' + (w.unlocked ? ' is-unlocked' : '')}>
                              <strong>{w.word}</strong>
                              <small>{w.translation}</small>
                            </span>
                          ))}
                        </div>
                      )}
                    </div>
                    <div className="spellbook-selection__actions">
                      <SpellEquipActions spell={selected} disabled={!bridge || busy || obscured} onEquip={equip} />
                      <button
                        type="button"
                        className={'control-button spellbook-rename-btn' + (renamingSpellId === selected.id ? ' is-active' : '')}
                        disabled={!bridge || busy || obscured}
                        title={`Rename ${selected.kind === 'shout' ? 'shout' : selected.kind === 'spell' ? 'spell' : 'power'} (R)`}
                        onClick={() => beginRenameSpell(selected)}
                      >
                        <SpellRenameIcon size={15} />
                        <span>Rename</span>
                      </button>
                      <button
                        type="button"
                        className={'control-button spellbook-delete-btn' + (confirmDeleteId === selected.id ? ' is-confirming' : '')}
                        disabled={!bridge || busy || obscured}
                        title={confirmDeleteId === selected.id ? `Click again to permanently delete ${selected.name}` : `Delete ${selected.kind === 'shout' ? 'shout' : selected.kind === 'spell' ? 'spell' : 'power'} (X / Del)`}
                        onClick={() => {
                          if (confirmDeleteId === selected.id) deleteSpellEntry(selected);
                          else setConfirmDeleteId(selected.id);
                        }}
                      >
                        <SpellDeleteIcon size={15} />
                        <span>{confirmDeleteId === selected.id ? 'Confirm Delete' : 'Delete'}</span>
                      </button>
                      {confirmDeleteId === selected.id && (
                        <button
                          type="button"
                          className="control-button"
                          onClick={() => setConfirmDeleteId(null)}
                          title="Cancel delete"
                        >
                          <span>Cancel</span>
                        </button>
                      )}
                      <button
                        type="button"
                        className={'control-button spellbook-fav-btn' + (selected.favorite ? ' is-favorite' : '')}
                        disabled={!bridge || busy || obscured}
                        aria-pressed={selected.favorite}
                        aria-label={selected.favorite ? 'Remove from favorites' : 'Add to favorites'}
                        title={selected.favorite ? 'Remove from favorites (F)' : 'Add to favorites (F)'}
                        onClick={() => toggleFavorite(selected)}
                      >
                        <HeartIcon filled={selected.favorite} size={16} />
                      </button>
                    </div>
                  </>
                ) : (
                  <div className="spellbook-selection__copy">
                    <span className="eyebrow">SPELLBOOK</span>
                    <h3>Select a spell</h3>
                  </div>
                )}
              </div>
            </>
          )}

          <div className="inventory-notice spellbook-notice" role="status">
            <span>
              {statusMessage ||
                (category === 'effects'
                  ? `${activeEffects.length} active effect${activeEffects.length === 1 ? '' : 's'} on ${playerName}.`
                  : `${filtered.length} spell${filtered.length === 1 ? '' : 's'} · Left-click: Left hand (or equip Shout / Power) · Right-click: Right hand · Q / E · F to favorite`)}
            </span>
          </div>
        </section>
      </div>
    </div>
  );
}
