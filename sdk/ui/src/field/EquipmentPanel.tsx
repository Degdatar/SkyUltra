import * as React from 'react';
import type { SkyUltraEquipmentSlot, SkyUltraItemAction, SkyWebViewAPI } from '@skyultra/sdk';
import { createSkyUltraGame } from '@skyultra/sdk';
import { ModelStage } from './ActorStage.js';
import { actorThumbnailQueue, PLAYER_PORTRAIT_QUEUE_KEY } from './itemThumbnails.js';
import { ItemThumbnail } from './ItemThumbnail.js';
import { EquipmentGlyph } from './EquipmentGlyph.js';
import { Icon } from './icons.js';
import { HeartIcon } from './spellArt.js';
import type { Item, Npc, Player } from './model.js';

export const equipmentSlots: Array<{ id: Exclude<SkyUltraEquipmentSlot, 'auto'>; label: string; biped?: string }> = [
  { id: 'head', label: 'Head' }, { id: 'body', label: 'Body' }, { id: 'hands', label: 'Hands' }, { id: 'feet', label: 'Feet' },
  { id: 'cape', label: 'Cape', biped: '46' }, { id: 'neck', label: 'Neck' }, { id: 'ring', label: 'Ring' },
  { id: 'mainHand', label: 'Right hand' }, { id: 'offHand', label: 'Left hand' }, { id: 'ammo', label: 'Ammo' }, { id: 'lantern', label: 'Lantern', biped: '49' },
];
export function fitsEquipmentSlot(item: Item | null, slot: string): boolean {
  if (!item?.equipSlots?.length) return false;
  if (slot === 'auto') return true;
  return item.equipSlots.some(value => value === slot);
}

export function formatItemCombatMeta(item: Item | null | undefined): string {
  if (!item) return '';
  const parts: string[] = [];
  if (item.armorTypeLabel) parts.push(item.armorTypeLabel);
  else if (item.weaponType) parts.push(item.weaponType);
  if (typeof item.damage === 'number' && item.damage > 0) parts.push(`DMG ${Math.round(item.damage)}`);
  if (typeof item.armorRating === 'number' && item.armorRating > 0) parts.push(`AR ${Math.round(item.armorRating)}`);
  return parts.join(' · ');
}

export function ItemCombatBadges({ item, compact = false }: { item: Item | null | undefined; compact?: boolean }): React.JSX.Element | null {
  if (!item) return null;
  const hasDamage = typeof item.damage === 'number' && item.damage > 0;
  const hasArmor = typeof item.armorRating === 'number' && item.armorRating > 0;
  const weightClass = item.armorWeightClass;
  const weightLabel =
    weightClass === 'heavy' ? (compact ? 'Heavy' : 'Heavy Armor') :
    weightClass === 'light' ? (compact ? 'Light' : 'Light Armor') :
    weightClass === 'shield' ? 'Shield' :
    weightClass === 'clothing' ? 'Clothing' :
    null;
  const weaponLabel = !weightLabel && item.weaponType ? item.weaponType : null;
  if (!hasDamage && !hasArmor && !weightLabel && !weaponLabel) return null;
  return (
    <span className={'item-combat-badges' + (compact ? ' item-combat-badges--compact' : '')}>
      {weightLabel && (
        <b className={`item-combat-badge item-combat-badge--armor-class item-combat-badge--${weightClass ?? 'armor'}`}>
          {weightLabel}
        </b>
      )}
      {!compact && weaponLabel && (
        <b className="item-combat-badge item-combat-badge--weapon-type">
          {weaponLabel}
        </b>
      )}
      {hasDamage && (
        <b className="item-combat-badge item-combat-badge--dmg" title={`Damage: ${Math.round(item.damage!)}${item.weaponType ? ' (' + item.weaponType + ')' : ''}`}>
          DMG {Math.round(item.damage!)}
        </b>
      )}
      {hasArmor && (
        <b className="item-combat-badge item-combat-badge--ar" title={`Armor Rating: ${Math.round(item.armorRating!)}${item.armorTypeLabel ? ' (' + item.armorTypeLabel + ')' : ''}`}>
          AR {Math.round(item.armorRating!)}
        </b>
      )}
    </span>
  );
}
export function resolveItemUseAction(item: Item | null): SkyUltraItemAction | null {
  if (!item) return null;
  if (item.useAction) return item.useAction;
  const cat = item.category.toLowerCase();
  const lowerName = item.name.toLowerCase();
  if (cat === 'potion') {
    const isFood = /\b(bread|cheese|meat|beef|venison|salmon|apple|cabbage|potato|leek|carrot|pie|stew|soup|sweetroll|taffy|cookie|dumpling|crostata|pheasant|rabbit|horker|charred|cooked|grilled|baked|roasted)\b/i.test(lowerName)
      && !/\b(mead|wine|ale|brandy|beer|cider|drink|milk|water|juice|tea|skooma|sujamma|flin|mazte|shein|potion|poison|elixir|draught|philter)\b/i.test(lowerName);
    return isFood ? 'eat' : 'drink';
  }
  if (cat === 'ingredient' || cat === 'food') return 'eat';
  if (cat === 'book') {
    if (item.teachesSpell || /^spell tome\b/i.test(lowerName)) return 'learn';
    return 'read';
  }
  if (cat === 'miscellaneous' || cat === 'scroll' || cat === 'soul gem' || cat === 'key') {
    return 'activate';
  }
  return null;
}
const actionButtonLabels: Record<SkyUltraItemAction, string> = {
  drink: 'Drink',
  eat: 'Eat',
  read: 'Read',
  learn: 'Learn',
  activate: 'Activate',
  charge: 'Charge',
  drop: 'Drop',
};
export function EquipmentSelection({ player, selected, enabled, bridge, obscured = false, onInspect, onChange, onItemAction, onToggleFavorite, transferLabel, onTransfer }: {
  player: Player | null; selected: Item | null; enabled: boolean; bridge?: SkyWebViewAPI; obscured?: boolean; onInspect?(): void;
  onChange(item: Item, slot: SkyUltraEquipmentSlot, unequip?: boolean): void;
  onItemAction?(item: Item, action: SkyUltraItemAction, count?: number): void;
  onToggleFavorite?(item: Item): void;
  transferLabel?: string;
  onTransfer?(item: Item): void;
}): React.JSX.Element {
  const isEnchanted = Boolean(selected?.enchanted || selected?.enchantments?.length);
  const enchantTitle = selected?.enchantmentName || selected?.enchantments?.map(e => e.name).join(' · ') || 'Enchanted';
  const useAction = resolveItemUseAction(selected);
  const canEquip = Boolean(selected?.equipSlots?.length);
  const canCharge = Boolean(selected && (selected.canCharge || selected.enchantmentCharge !== undefined || (selected.category === 'weapon' && isEnchanted)));
  const canDrop = Boolean(selected && selected.canDrop !== false && !onTransfer);
  const enchantCount = selected?.enchantments?.length ?? 0;
  const hasDamage = typeof selected?.damage === 'number' && selected.damage > 0;
  const hasArmor = typeof selected?.armorRating === 'number' && selected.armorRating > 0;
  const typeDetail = selected?.armorTypeLabel || selected?.weaponType || selected?.category;
  return <div className={'equipment-selection' + (selected?.equipped ? ' is-equipped' : '') + (isEnchanted ? ' is-enchanted' : '')}>
    <div className="selection-art" aria-hidden="true">{selected ? <ItemThumbnail item={selected} bridge={bridge} paused={obscured} /> : <EquipmentGlyph slot="mainHand" />}{selected && isEnchanted && <span className="selection-art__enchant-badge" title={enchantTitle}><Icon name="flame" size={12} /></span>}</div>
    <div className="selection-copy">
      <div className="selection-copy__top">
        <span className="eyebrow">
          {selected?.equipped ? 'CURRENTLY EQUIPPED' : 'SELECTED ITEM'}
          {selected?.stolen && (
            <em className="selection-stolen-pill">
              <Icon name="stolen" size={10} />
              STOLEN
            </em>
          )}
          {isEnchanted && (
            <em className="selection-enchant-pill">
              <Icon name="flame" size={10} />
              ENCHANTED
              {selected?.enchantmentCharge !== undefined ? ` · ${selected.enchantmentCharge}%` : ''}
              {enchantCount > 1 ? ` · ${enchantCount} EFFECTS` : ''}
            </em>
          )}
        </span>
        {selected && (
          <div className="selection-properties">
            {hasDamage && <span><Icon name="target" size={11} /><strong>{Math.round(selected.damage!)}</strong><small>DMG</small></span>}
            {hasArmor && <span><Icon name="shield" size={11} /><strong>{Math.round(selected.armorRating!)}</strong><small>{selected.armorWeightClass === 'heavy' ? 'HEAVY AR' : selected.armorWeightClass === 'light' ? 'LIGHT AR' : 'ARMOR'}</small></span>}
            <span><Icon name="gold" size={11} /><strong>{selected.value.toLocaleString()}</strong><small>VALUE</small></span>
            <span><strong>{selected.weight}</strong><small>WEIGHT</small></span>
            <span><strong>{selected.count.toLocaleString()}</strong><small>OWNED</small></span>
          </div>
        )}
      </div>
      <div className="selection-copy__headline">
        <h3 title={selected?.name}>{isEnchanted && <Icon name="flame" size={13} />}{selected?.name ?? 'Discover what you carry'}</h3>
        <span className="selection-category">
          {typeDetail ?? 'Choose an item from your pack'}
          {selected?.speed ? ` · Speed ${selected.speed}` : ''}
          {selected?.enchantmentName ? ` · ${selected.enchantmentName}` : ''}
          {selected?.teachesSpell ? ` · Teaches ${selected.teachesSpell}` : ''}
        </span>
      </div>
      {selected && isEnchanted && (
        <div className="selection-enchantments" aria-label="Item enchantments" tabIndex={0}>
          {selected.enchantments?.length ? selected.enchantments.map((eff, idx) => (
            <div className="selection-enchantment" key={`${eff.name}-${idx}`}>
              <Icon name="flame" size={11} />
              <strong>{eff.name}</strong>
              {eff.description && <span>{eff.description}</span>}
            </div>
          )) : (
            <div className="selection-enchantment">
              <Icon name="flame" size={11} />
              <strong>{selected.enchantmentName || 'Enchanted Item'}</strong>
              <span>Arcane enchantment active</span>
            </div>
          )}
        </div>
      )}
    </div>
    {selected && <div className="equipment-selection__actions">
      {canEquip && (
        <button type="button" className="control-button is-active" disabled={!enabled} onClick={() => onChange(selected, selected.equipSlots?.[0] ?? 'auto')}>
          Equip <span aria-hidden="true">↗</span>
        </button>
      )}
      {selected.equipped && canEquip && (
        <button type="button" className="control-button" disabled={!enabled} onClick={() => onChange(selected, equipmentSlots.find(slot => player?.equipment[slot.id]?.id.split(':')[0] === selected.id.split(':')[0])?.id ?? selected.equipSlots?.[0] ?? 'auto', true)}>
          Unequip
        </button>
      )}
      {onTransfer && transferLabel && (
        <button type="button" className="control-button" disabled={!enabled} onClick={() => onTransfer(selected)}>
          {transferLabel}
        </button>
      )}
      {useAction && !onTransfer && (
        <button type="button" className={'control-button selection-action-btn selection-action-btn--' + useAction + (!canEquip ? ' is-active' : '')} disabled={!enabled} onClick={() => onItemAction?.(selected, useAction, 1)}>
          {actionButtonLabels[useAction]}
        </button>
      )}
      {canCharge && (
        <button type="button" className="control-button selection-action-btn selection-action-btn--charge" disabled={!enabled} onClick={() => onItemAction?.(selected, 'charge', 1)} title="Recharge this enchanted item using a filled Soul Gem">
          <Icon name="flame" size={11} /> Charge
        </button>
      )}
      {canDrop && (
        <button type="button" className="control-button selection-action-btn selection-action-btn--drop" disabled={!enabled} onClick={() => onItemAction?.(selected, 'drop', 1)} title={`Drop ${selected.name} from inventory`}>
          Drop
        </button>
      )}
      {onToggleFavorite && (
        <button
          type="button"
          className={'control-button spellbook-fav-btn' + (selected.favorite ? ' is-favorite' : '')}
          disabled={!enabled}
          aria-pressed={!!selected.favorite}
          aria-label={selected.favorite ? 'Remove from favorites' : 'Add to favorites'}
          title={selected.favorite ? 'Remove from favorites' : 'Add to favorites'}
          onClick={() => onToggleFavorite(selected)}
        >
          <HeartIcon filled={!!selected.favorite} size={15} />
        </button>
      )}
      {onInspect && <button type="button" className="selection-inspect" disabled={obscured} onClick={onInspect}>Item studio <Icon name="arrow" size={12} /></button>}
    </div>}
  </div>;
}
const vitals = [{ id: 'health', label: 'Health' }, { id: 'magicka', label: 'Magicka' }, { id: 'stamina', label: 'Stamina' }] as const;
export function EquipmentPanel({
  player,
  bridge,
  obscured,
  selected,
  dragging,
  hoverSlot,
  busy,
  revision,
  actorOverride,
  eyebrowOverride,
  subtitleOverride,
  hideVitals = false,
  initialMotion = true,
  hideStudioControls = false,
  onSelect,
  onChange,
}: {
  player: Player | null;
  bridge?: SkyWebViewAPI;
  obscured: boolean;
  selected: Item | null;
  dragging: Item | null;
  hoverSlot: string;
  busy: boolean;
  revision: number;
  actorOverride?: Npc | null;
  eyebrowOverride?: string;
  subtitleOverride?: string;
  hideVitals?: boolean;
  initialMotion?: boolean;
  hideStudioControls?: boolean;
  onSelect(item: Item): void;
  onChange(item: Item, slot: SkyUltraEquipmentSlot, unequip?: boolean): void;
}): React.JSX.Element {
  const slotButton = ({ id, label, biped }: typeof equipmentSlots[number]): React.JSX.Element => {
    const item = player?.equipment[id] ?? null;
    const itemEnchanted = Boolean(item?.enchanted || item?.enchantments?.length);
    const fits = fitsEquipmentSlot(dragging ?? selected, id);
    const meta = formatItemCombatMeta(item);
    return <div key={id} className={'equipment-target' + (item ? ' is-occupied' : '') + (itemEnchanted ? ' is-enchanted' : '') + (dragging && fits ? ' can-drop' : '') + (hoverSlot === id ? fits ? ' is-over' : ' is-invalid' : '')}>
      <button type="button" className="equipment-slot" data-equip-slot={id} disabled={busy || obscured} aria-label={label + ': ' + (item?.name ?? 'Empty') + (fits ? '. Equip selected item' : '')}
        title={label + (biped ? ' · slot ' + biped : '') + '\n' + (item?.name ?? 'Empty') + (meta ? '\n' + meta : '') + (fits ? '\nEquip ' + (dragging ?? selected)?.name : '')}
        onClick={() => { if (selected && fits) onChange(selected, id); else if (item) onSelect(item); }}>
        <span className="equipment-slot__label">{label}{biped && <em>{biped}</em>}</span><span className="equipment-slot__bezel"><span className="equipment-slot__art"><EquipmentGlyph slot={id} />{item && <ItemThumbnail item={item} bridge={bridge} paused={obscured} />}{itemEnchanted && <span className="equipment-slot__enchant" title={item?.enchantmentName || 'Enchanted'}><Icon name="flame" size={9} /></span>}</span></span>
        <strong>{item?.name ?? 'Not equipped'}</strong>
      </button>
      {item && !!item.equipSlots?.length && <button className="equipment-unequip" disabled={busy || obscured || !bridge} onClick={() => onChange(item, id, true)} aria-label={'Unequip ' + item.name + ' from ' + label} title={'Unequip ' + item.name}>×</button>}
    </div>;
  };
  const actor: Npc = actorOverride ?? { referenceId: '20', baseFormId: '7', name: player?.playerName ?? 'Player', race: '', sex: '' };
  const occupied = equipmentSlots.filter(slot => !!player?.equipment[slot.id]).length;
  const canDropOnModel = fitsEquipmentSlot(dragging ?? selected, 'auto');
  return <section className="surface outfit-panel" aria-label="Outfit and equipment">
    <div className="outfit-heading"><div><span className="eyebrow">{eyebrowOverride ?? 'CHARACTER EQUIPMENT'}</span><h2 title={actorOverride?.name ?? player?.playerName}>{actorOverride?.name ?? player?.playerName ?? 'Your character'}</h2><p>{subtitleOverride ?? player?.location ?? 'Your journey, your equipment'}<span>·</span>{occupied} / {equipmentSlots.length} slots equipped</p></div>{!actorOverride && <span className="outfit-level"><small>LEVEL</small><strong>{player?.level ?? '—'}</strong></span>}</div>
    {!hideVitals && <div className="outfit-vitals" aria-label="Character vital signs">{vitals.map(vital => {
      const current = player?.[vital.id]?.current ?? 0, maximum = player?.[vital.id]?.max ?? 0;
      const percentage = maximum > 0 ? Math.max(0, Math.min(100, current / maximum * 100)) : 0;
      return <div className={'outfit-vital outfit-vital--' + vital.id} key={vital.id} role="meter" aria-label={vital.label} aria-valuemin={0} aria-valuemax={Math.max(1, maximum)} aria-valuenow={Math.max(0, Math.min(current, maximum))} title={`${vital.label}: ${current.toLocaleString()} / ${maximum.toLocaleString()}`}><span>{vital.label}<b>{maximum > 0 ? Math.round(current).toLocaleString() : '—'}</b></span><div><i style={{ width: percentage + '%' }} /></div></div>;
    })}</div>}
    <div className="outfit-doll"><div className="equipment-rail equipment-rail--armor">{equipmentSlots.slice(0, 5).map(slotButton)}</div>
      <div className={'outfit-model' + (dragging && canDropOnModel ? ' can-drop' : '') + (hoverSlot === 'auto' ? (canDropOnModel ? ' is-over' : ' is-invalid') : '')} data-equip-slot="auto">{bridge && player?.available ? <ModelStage subject={{ kind: 'actor', actor }} bridge={bridge} obscured={obscured} equipment initialMotion={initialMotion} hideStudioControls={hideStudioControls} refreshToken={revision} onCapturePortrait={actorOverride ? undefined : async () => {
        // Camera button: snapshot a portrait (head, shoulders & upper body) and persist it to Overwrite/SkyUltra/Cache/Portraits/.
        // Native broadcasts skyultra.portraitUpdated to every view, so the HUD portrait,
        // overview/player card and character sheet all refresh at once.
        const result = await createSkyUltraGame(bridge).actorThumbnail({ referenceId: '20', baseFormId: '7', usePreview: true });
        if (!result.available || !result.dataUrl) throw new Error(result.message ?? 'Could not capture a portrait from the current view.');
        const queue = actorThumbnailQueue(bridge);
        [PLAYER_PORTRAIT_QUEUE_KEY, '20:7', '20:'].forEach(key => queue.set(key, result.dataUrl!));
        return 'Portrait updated and saved to Overwrite cache.';
      }} /> : <div className="outfit-placeholder">{actorOverride ? 'Mannequin model appears inside Skyrim.' : 'Player model appears inside Skyrim.'}</div>}</div>
      <div className="equipment-rail equipment-rail--accessories">{equipmentSlots.slice(5).map(slotButton)}</div>
    </div>
    <div className="outfit-stats"><span><Icon name="shield" size={17} /><small>ARMOR RATING</small><strong>{Math.round(player?.armorRating ?? 0).toLocaleString()}</strong></span><span><Icon name="inventory" size={17} /><small>CARRY WEIGHT</small><strong>{Math.round(player?.carryWeight?.current ?? 0).toLocaleString()} <em>/ {Math.round(player?.carryWeight?.max ?? 0).toLocaleString()}</em></strong></span><span><Icon name="gold" size={17} /><small>GOLD</small><strong>{player?.gold.toLocaleString() ?? '—'}</strong></span></div>
  </section>;
}
