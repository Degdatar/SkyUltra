import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
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

export function SpellHandBadges({ spell }: { spell: SkyUltraSpell }): React.JSX.Element | null {
  if (!spell.equippedLeft && !spell.equippedRight && !spell.equippedVoice) return null;
  return (
    <span className="spellbook-hands" aria-label="Equipped slots">
      {spell.equippedLeft && <b title="Equipped in left hand">L</b>}
      {spell.equippedRight && <b title="Equipped in right hand">R</b>}
      {spell.equippedVoice && <b title="Equipped as power / shout">V</b>}
    </span>
  );
}

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
  pauseWorld = false,
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
  const [hoverId, setHoverId] = useState('');
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [statusMessage, setStatusMessage] = useState('');
  const [revision, setRevision] = useState(0);
  const [renamingSpellId, setRenamingSpellId] = useState<string | null>(null);
  const [renameDraft, setRenameDraft] = useState('');
  const [confirmDeleteId, setConfirmDeleteId] = useState<string | null>(null);
  const renameInputRef = useRef<HTMLInputElement | null>(null);
  const locked = useRef(false);
  const actionController = useRef<AbortController | null>(null);

  useEffect(() => () => actionController.current?.abort(), [bridge]);

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
        const res = await game.spells({ signal: controller.signal });
        if (active) {
          setBook(res);
          if (!res.available && res.message) setStatusMessage(res.message);
        }
      } catch (error) {
        if (active) setStatusMessage(error instanceof Error ? error.message : 'Could not read your spellbook.');
      } finally {
        pending = false;
        if (active) setLoading(false);
      }
    };
    void refresh();
    const interval = window.setInterval(() => void refresh(), 4000);
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

  const spells = book?.spells ?? [];
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

  const selected = filtered.find(s => s.id === selectedId) ?? filtered[0] ?? null;
  const activeEffects = player?.activeEffects ?? [];
  const magicka = book?.magicka ?? player?.magicka ?? { current: 0, max: 1 };
  const magickaPct = Math.max(0, Math.min(100, Math.round((magicka.current / Math.max(1, magicka.max)) * 100)));
  const playerName = player?.playerName || 'Dragonborn';

  async function runAction<T extends { accepted: boolean; message?: string }>(label: string, request: (signal: AbortSignal) => Promise<T>): Promise<void> {
    if (!game || obscured || locked.current) return;
    locked.current = true;
    setBusy(true);
    setStatusMessage(label);
    actionController.current?.abort();
    const controller = new AbortController();
    actionController.current = controller;
    try {
      const result = await request(controller.signal);
      if (!controller.signal.aborted) {
        setStatusMessage(result.message ?? (result.accepted ? 'Done.' : 'Skyrim declined that action.'));
        setRevision(v => v + 1);
      }
    } catch (error) {
      if (!controller.signal.aborted) setStatusMessage(error instanceof Error ? error.message : 'The spell action failed.');
    } finally {
      locked.current = false;
      if (!controller.signal.aborted) setBusy(false);
    }
  }

  const equip = (spell: SkyUltraSpell, hand: SkyUltraSpellHand): void => {
    if (!game) return;
    void runAction(`Readying ${spell.name}…`, signal => game.equipSpell(spell.id, hand, { signal }));
  };
  const toggleFavorite = (spell: SkyUltraSpell): void => {
    if (!game) return;
    void runAction(spell.favorite ? `Removing ${spell.name} from favorites…` : `Adding ${spell.name} to favorites…`, signal =>
      game.setFavorite(spell.id, 'spell', !spell.favorite, { signal }),
    );
  };
  const defaultEquip = (spell: SkyUltraSpell): void => equip(spell, spell.hands.includes('voice') ? 'voice' : spell.twoHanded ? 'both' : 'right');

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

  useEffect(() => {
    const onKey = (event: KeyboardEvent): void => {
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
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  return (
    <div
      className={`page inventory-page inventory-page--armory inventory-page--standalone spellbook-page spellbook-page--${displayMode}`}
      data-container-mode={displayMode}
      style={categoryColorStyle(category)}
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

              <div className={'spellbook-results spellbook-results--' + layout} onMouseLeave={() => setHoverId('')}>
                {filtered.length ? (
                  filtered.map(spell => {
                    const isSelected = spell.id === selected?.id;
                    return (
                      <button
                        type="button"
                        key={spell.id}
                        className={'spellbook-tile' + (isSelected ? ' is-active' : '') + (spell.favorite ? ' is-favorite' : '')}
                        style={spellColorStyle(spell)}
                        aria-pressed={isSelected}
                        title={spell.description || spell.name}
                        onMouseEnter={() => setHoverId(spell.id)}
                        onClick={() => { setSelectedId(spell.id); if (!locked.current) setStatusMessage(''); }}
                        onDoubleClick={() => { if (!busy && !obscured) defaultEquip(spell); }}
                      >
                        <SpellSigil spell={spell} size={layout === 'grid' ? 64 : 40} active={isSelected || hoverId === spell.id} />
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
                  })
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
                  : `${filtered.length} spell${filtered.length === 1 ? '' : 's'} · Double-click or Enter to equip · Q / E for left / right · F to favorite`)}
            </span>
          </div>
        </section>
      </div>
    </div>
  );
}
