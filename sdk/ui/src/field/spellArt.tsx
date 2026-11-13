import * as React from 'react';
import type { SkyUltraSpell } from '@skyultra/sdk';

/** Spellbook rail categories (Skyrim Magic menu tabs + SkyUltra extras). */
export type SpellRailCategory =
  | 'all'
  | 'favorites'
  | 'alteration'
  | 'conjuration'
  | 'destruction'
  | 'illusion'
  | 'restoration'
  | 'powers'
  | 'shouts'
  | 'other'
  | 'effects';

export const spellCategoryLabels: Record<SpellRailCategory, string> = {
  all: 'All Spells',
  favorites: 'Favorites',
  alteration: 'Alteration',
  conjuration: 'Conjuration',
  destruction: 'Destruction',
  illusion: 'Illusion',
  restoration: 'Restoration',
  powers: 'Powers',
  shouts: 'Shouts',
  other: 'Other',
  effects: 'Active Effects',
};

/** Per-school accent colours. Exposed to CSS as --spell-hue / --spell-glow. */
export const spellCategoryColors: Record<SpellRailCategory, { hue: string; glow: string }> = {
  all: { hue: '#d8c49c', glow: '#f3e2b8' },
  favorites: { hue: '#ff5d7a', glow: '#ffb3c1' },
  alteration: { hue: '#e9b949', glow: '#ffe39a' },
  conjuration: { hue: '#9b6bff', glow: '#cdb6ff' },
  destruction: { hue: '#ff5a2a', glow: '#ffb27a' },
  illusion: { hue: '#e45bd6', glow: '#ffb0f4' },
  restoration: { hue: '#5fe3c1', glow: '#fff3c4' },
  powers: { hue: '#8fb4ff', glow: '#dfe9ff' },
  shouts: { hue: '#6fe0ff', glow: '#d4f6ff' },
  other: { hue: '#b9b2a4', glow: '#ece6da' },
  effects: { hue: '#7fd38a', glow: '#d3ffd8' },
};

const elementColors: Record<string, { hue: string; glow: string }> = {
  fire: { hue: '#ff5a1f', glow: '#ffc06a' },
  frost: { hue: '#5fc8ff', glow: '#e2f7ff' },
  shock: { hue: '#8f8bff', glow: '#f1efff' },
  heal: { hue: '#ffd36b', glow: '#fff6d2' },
  ward: { hue: '#62d9ff', glow: '#d8f7ff' },
  sun: { hue: '#ffe07a', glow: '#fffbe0' },
  summon: { hue: '#a46bff', glow: '#e0ceff' },
  bound: { hue: '#7aa2ff', glow: '#dbe6ff' },
  soul: { hue: '#b48bff', glow: '#efe4ff' },
  mind: { hue: '#e85bd2', glow: '#ffc8f5' },
  shadow: { hue: '#8f7dc9', glow: '#d9d0f7' },
  time: { hue: '#d8b4ff', glow: '#f4e8ff' },
  light: { hue: '#fff1b0', glow: '#ffffff' },
  detect: { hue: '#ff86d6', glow: '#ffd6f2' },
  telekinesis: { hue: '#e8b84a', glow: '#fff0c4' },
  armor: { hue: '#e0b25a', glow: '#fff0cc' },
  harm: { hue: '#c94a5e', glow: '#ffb7c2' },
  voice: { hue: '#6fe0ff', glow: '#d4f6ff' },
};

export function spellCategoryOf(spell: SkyUltraSpell): SpellRailCategory {
  return (spell.category as SpellRailCategory) ?? 'other';
}

/** CSS custom properties that colour a spell sigil/tile. */
export function spellColorStyle(spell: SkyUltraSpell | null | undefined, fallback: SpellRailCategory = 'all'): React.CSSProperties {
  const colors = (spell?.element && elementColors[spell.element]) || spellCategoryColors[spell ? spellCategoryOf(spell) : fallback] || spellCategoryColors.all;
  return { '--spell-hue': colors.hue, '--spell-glow': colors.glow } as React.CSSProperties;
}

export function categoryColorStyle(category: SpellRailCategory): React.CSSProperties {
  const colors = spellCategoryColors[category] ?? spellCategoryColors.all;
  return { '--spell-hue': colors.hue, '--spell-glow': colors.glow } as React.CSSProperties;
}

/* ------------------------------------------------------------------------ */
/* School / category glyphs                                                  */
/* ------------------------------------------------------------------------ */

const categoryGlyphs: Record<SpellRailCategory, React.ReactNode> = {
  all: (
    <>
      <path d="M12 2 4 6v6c0 5 3.5 8.5 8 10 4.5-1.5 8-5 8-10V6z" />
      <path d="M12 7v10M7.5 12h9" />
    </>
  ),
  favorites: <path d="M12 20.5C6 15.6 2.5 12.4 2.5 8.6 2.5 5.8 4.7 3.6 7.4 3.6c1.8 0 3.5 1 4.6 2.6 1.1-1.6 2.8-2.6 4.6-2.6 2.7 0 4.9 2.2 4.9 5 0 3.8-3.5 7-9.5 11.9Z" />,
  // Alteration: a shifting crystal lattice
  alteration: (
    <>
      <path d="m12 2 7 5v10l-7 5-7-5V7z" />
      <path d="m12 2v20M5 7l14 10M19 7 5 17" />
    </>
  ),
  // Conjuration: summoning circle / portal
  conjuration: (
    <>
      <circle cx="12" cy="12" r="9" />
      <circle cx="12" cy="12" r="4.5" />
      <path d="M12 3v4.5M12 16.5V21M3 12h4.5M16.5 12H21" />
    </>
  ),
  // Destruction: flame burst
  destruction: (
    <>
      <path d="M12 2c1.2 4.2-3.6 5.4-3.6 10.2A3.6 3.6 0 0 0 12 16a3.6 3.6 0 0 0 3.6-3.8C15.6 10 14 9 14 9s3.8 1.6 3.8 6A5.8 5.8 0 0 1 12 21a5.8 5.8 0 0 1-5.8-5.8C6.2 9 12 7 12 2Z" />
    </>
  ),
  // Illusion: an all-seeing eye
  illusion: (
    <>
      <path d="M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12Z" />
      <circle cx="12" cy="12" r="3.2" />
      <path d="M12 2v2M12 20v2M4.2 4.2l1.4 1.4M18.4 18.4l1.4 1.4M19.8 4.2l-1.4 1.4M5.6 18.4l-1.4 1.4" />
    </>
  ),
  // Restoration: radiant sun
  restoration: (
    <>
      <circle cx="12" cy="12" r="4" />
      <path d="M12 2v4M12 18v4M2 12h4M18 12h4M4.9 4.9l2.8 2.8M16.3 16.3l2.8 2.8M19.1 4.9l-2.8 2.8M7.7 16.3l-2.8 2.8" />
    </>
  ),
  // Powers: a crowned star
  powers: (
    <>
      <path d="m12 3 2.6 5.4 5.9.8-4.3 4.1 1 5.8L12 16.4l-5.2 2.7 1-5.8L3.5 9.2l5.9-.8z" />
    </>
  ),
  // Shouts: dragon voice waves
  shouts: (
    <>
      <path d="M4 9h3l5-4v14l-5-4H4z" />
      <path d="M15.5 8.5a5 5 0 0 1 0 7M18 6a8.5 8.5 0 0 1 0 12" />
    </>
  ),
  other: (
    <>
      <circle cx="12" cy="12" r="8" />
      <path d="M12 7v5l3 3" />
    </>
  ),
  effects: (
    <>
      <path d="M3 12h4l2-6 4 12 2-6h6" />
    </>
  ),
};

export function SpellCategoryGlyph({ category, size = 20 }: { category: SpellRailCategory; size?: number }): React.JSX.Element {
  return (
    <svg
      className={'spellbook-glyph spellbook-glyph--' + category}
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      {categoryGlyphs[category] ?? categoryGlyphs.other}
    </svg>
  );
}

export function HeartIcon({ filled, size = 16 }: { filled?: boolean; size?: number }): React.JSX.Element {
  return (
    <svg
      className={'favorites-heart' + (filled ? ' is-filled' : '')}
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill={filled ? 'currentColor' : 'none'}
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M12 20.5C6 15.6 2.5 12.4 2.5 8.6 2.5 5.8 4.7 3.6 7.4 3.6c1.8 0 3.5 1 4.6 2.6 1.1-1.6 2.8-2.6 4.6-2.6 2.7 0 4.9 2.2 4.9 5 0 3.8-3.5 7-9.5 11.9Z" />
    </svg>
  );
}

/** Ornate arcane rune-quill icon for renaming spells, shouts, and powers. */
export function SpellRenameIcon({ size = 15 }: { size?: number }): React.JSX.Element {
  return (
    <svg
      className="spellbook-action-icon spellbook-action-icon--rename"
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.75"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M20.2 3.8c-3.6.2-7.3 1.9-10.1 4.7L5.5 13.1 3.5 20.5l7.4-2 4.6-4.6c2.8-2.8 4.5-6.5 4.7-10.1Z" />
      <path d="M13.5 6.5 17.5 10.5" />
      <path d="M3.5 20.5 9.5 14.5" />
      <path d="M6.5 3.5v3M5 5h3" strokeWidth="1.5" />
      <circle cx="19" cy="18.5" r="1.2" fill="currentColor" stroke="none" />
    </svg>
  );
}

/** Arcane banish / shatter sigil icon for deleting spells, shouts, powers, and active effects. */
export function SpellDeleteIcon({ size = 15 }: { size?: number }): React.JSX.Element {
  return (
    <svg
      className="spellbook-action-icon spellbook-action-icon--delete"
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.75"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polygon points="12 2.4 19.6 6.8 19.6 17.2 12 21.6 4.4 17.2 4.4 6.8" strokeDasharray="3.2 1.6" />
      <path d="m8.5 8.5 7 7M15.5 8.5l-7 7" strokeWidth="2" />
      <path d="M12 2.4v2.4M12 19.2v2.4M4.4 12h2.2M17.4 12h2.2" strokeWidth="1.4" />
    </svg>
  );
}

/* ------------------------------------------------------------------------ */
/* Element emblems drawn in the centre of a sigil                            */
/* ------------------------------------------------------------------------ */

const elementGlyphs: Record<string, React.ReactNode> = {
  fire: <path d="M12 3c1 3.5-3 4.6-3 8.6a3 3 0 0 0 6 0c0-1.8-1.3-2.6-1.3-2.6s3.3 1.3 3.3 5A5 5 0 0 1 12 19a5 5 0 0 1-5-5C7 8.6 12 7 12 3Z" />,
  frost: <path d="M12 3v18M4.2 7.5l15.6 9M19.8 7.5l-15.6 9M12 3l-2 2.2M12 3l2 2.2M12 21l-2-2.2M12 21l2-2.2" />,
  shock: <path d="M13.5 2 6 13h5l-1.5 9L18 10.5h-5z" />,
  heal: <path d="M10 4h4v6h6v4h-6v6h-4v-6H4v-4h6z" />,
  ward: <path d="M12 3 5 6v5c0 4.4 3 8 7 10 4-2 7-5.6 7-10V6zM12 8v8M8.5 12h7" />,
  sun: (
    <>
      <circle cx="12" cy="12" r="3.6" />
      <path d="M12 3v3M12 18v3M3 12h3M18 12h3M5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1M18.4 5.6l-2.1 2.1M7.7 16.3l-2.1 2.1" />
    </>
  ),
  summon: (
    <>
      <circle cx="12" cy="12" r="7" />
      <path d="m12 5 6 10.5H6z" />
    </>
  ),
  bound: <path d="m5 19 9.5-9.5M14.5 9.5 19 5M8 14l2 2M16 3l5 5-3 1-3-3z" />,
  soul: <path d="M12 4c3 3 5 5.2 5 8.2A5 5 0 0 1 12 17a5 5 0 0 1-5-4.8C7 9.2 9 7 12 4ZM10 21h4" />,
  mind: (
    <>
      <path d="M3 12s3.2-5.5 9-5.5 9 5.5 9 5.5-3.2 5.5-9 5.5S3 12 3 12Z" />
      <circle cx="12" cy="12" r="2.6" />
    </>
  ),
  shadow: <path d="M18 14.5A7 7 0 0 1 9.5 6 7 7 0 1 0 18 14.5Z" />,
  time: (
    <>
      <path d="M7 3h10M7 21h10M8 3c0 5 8 5 8 9s-8 4-8 9M16 3c0 5-8 5-8 9s8 4 8 9" />
    </>
  ),
  light: (
    <>
      <circle cx="12" cy="10" r="4.5" />
      <path d="M10 17h4M10.5 20h3" />
    </>
  ),
  detect: (
    <>
      <circle cx="11" cy="11" r="6" />
      <path d="m16 16 4 4M11 8v6M8 11h6" />
    </>
  ),
  telekinesis: <path d="M7 12c0-3 2-6 5-6s5 3 5 6-2 6-5 6M12 12h.01M4 8l2 2M20 8l-2 2M4 16l2-2" />,
  armor: <path d="M12 3 5 6v5c0 4.4 3 8 7 10 4-2 7-5.6 7-10V6z" />,
  harm: <path d="M12 3 21 20H3zM12 9v5M12 17h.01" />,
  voice: <path d="M4 9h3l5-4v14l-5-4H4zM15.5 8.5a5 5 0 0 1 0 7M18 6a8.5 8.5 0 0 1 0 12" />,
};

function emblemFor(spell: SkyUltraSpell): React.ReactNode {
  if (spell.kind === 'shout') return elementGlyphs.voice;
  if (spell.element && elementGlyphs[spell.element]) return elementGlyphs[spell.element];
  return categoryGlyphs[spellCategoryOf(spell)] ?? categoryGlyphs.other;
}

/**
 * Animated spell sigil, inspired by the slowly rotating effect art in Skyrim's Magic menu.
 * - idle: gentle breathing glow
 * - active (hovered / selected tile): rings rotate, motes orbit
 * - hero: the large selection preview with extra rune ring and floating motes
 * All animation is transform/opacity based so it stays cheap inside the game overlay.
 */
export function SpellSigil({
  spell,
  size = 64,
  active = false,
  hero = false,
}: {
  spell: SkyUltraSpell;
  size?: number;
  active?: boolean;
  hero?: boolean;
}): React.JSX.Element {
  const motes = hero ? 7 : 3;
  return (
    <span
      className={
        'spellbook-sigil' +
        (active ? ' is-active' : '') +
        (hero ? ' is-hero' : '') +
        ` spellbook-sigil--${spell.element ?? spellCategoryOf(spell)}` +
        ` spellbook-sigil--${spell.castingType}`
      }
      style={{ ...spellColorStyle(spell), width: size, height: size, '--sigil-size': `${size}px` } as React.CSSProperties}
      aria-hidden="true"
    >
      <span className="spellbook-sigil__aura" />
      <svg className="spellbook-sigil__ring spellbook-sigil__ring--outer" viewBox="0 0 100 100">
        <circle cx="50" cy="50" r="45" />
        <circle className="spellbook-sigil__dash" cx="50" cy="50" r="40" />
      </svg>
      {hero && (
        <svg className="spellbook-sigil__ring spellbook-sigil__ring--runes" viewBox="0 0 100 100">
          {Array.from({ length: 12 }, (_, i) => {
            const angle = (i / 12) * Math.PI * 2;
            const x = 50 + Math.cos(angle) * 46;
            const y = 50 + Math.sin(angle) * 46;
            return <path key={i} d={`M${x - 2} ${y - 3}l2 6 2-6`} transform={`rotate(${(i / 12) * 360 + 90} ${x} ${y})`} />;
          })}
        </svg>
      )}
      <svg className="spellbook-sigil__ring spellbook-sigil__ring--inner" viewBox="0 0 100 100">
        <path d="M50 18 77.7 66H22.3Z" />
        <path d="M50 82 22.3 34h55.4Z" />
      </svg>
      <span className="spellbook-sigil__orbit">
        {Array.from({ length: motes }, (_, i) => (
          <i key={i} style={{ '--mote-index': i, '--mote-count': motes } as React.CSSProperties} />
        ))}
      </span>
      <svg
        className="spellbook-sigil__emblem"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.6"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        {emblemFor(spell)}
      </svg>
    </span>
  );
}

export function spellCostLabel(spell: SkyUltraSpell): string {
  if (spell.kind === 'shout') return spell.cooldown ? `${Math.round(spell.cooldown)}s recovery` : 'Shout';
  if (spell.kind === 'power') return 'Once per day';
  if (spell.kind === 'lesserPower') return 'At will';
  return spell.castingType === 'concentration' ? `${spell.cost}/s` : `${spell.cost}`;
}

export const spellDeliveryLabels: Record<SkyUltraSpell['delivery'], string> = {
  self: 'Self',
  touch: 'Touch',
  aimed: 'Aimed',
  targetActor: 'Target Actor',
  targetLocation: 'Target Location',
};

export const spellCastingLabels: Record<SkyUltraSpell['castingType'], string> = {
  fireAndForget: 'Fire and Forget',
  concentration: 'Concentration',
  constantEffect: 'Constant Effect',
  scroll: 'Scroll',
};
