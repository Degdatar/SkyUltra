import * as React from "react";
export type IconName = "archive" | "overview" | "actors" | "inventory" | "components" | "performance" | "search" | "arrow" | "check" | "chevron" | "heart" | "shield" | "rune" | "gold" | "close" | "grid" | "list" | "sword" | "flame" | "book" | "target" | "rotate" | "plus" | "pause" | "play" | "widgets" | "help" | "compass" | "clock" | "pin" | "keyboard" | "bell" | "flask" | "camera" | "download" | "stolen";
const paths: Record<IconName, React.ReactNode> = {
  stolen: <><path d="M18 11V6a1.5 1.5 0 0 0-3 0v4M15 10V4.5a1.5 1.5 0 0 0-3 0V10M12 9.5V3.5a1.5 1.5 0 0 0-3 0V10M9 10V5.5a1.5 1.5 0 0 0-3 0V14"/><path d="M6 12.5 4.3 11a1.5 1.5 0 0 0-2.1 2.1l4.1 5.2A6 6 0 0 0 11 20.5h2a5 5 0 0 0 5-5V11"/><circle cx="12" cy="15.5" r="1.8"/></>,
  camera: <><path d="M3 7h4l2-3h6l2 3h4v13H3z"/><circle cx="12" cy="13" r="4"/></>,
  download: <path d="M12 3v12m-5-5 5 5 5-5M4 17v4h16v-4"/>,
  archive: <path d="M4 6h16v14H4zM3 3h18v4H3zM9 11h6" />,
  widgets: <path d="M3 3h8v6H3zM15 3h6v10h-6zM3 13h8v8H3zM15 17h6v4h-6z" />,
  help: <><circle cx="12" cy="12" r="9"/><path d="M9 8a3 3 0 0 1 6 1c0 2-3 2-3 5M12 18h.01"/></>,
  compass: <><circle cx="12" cy="12" r="9"/><path d="m16 8-3 5-5 3 3-5z"/></>,
  clock: <><circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/></>,
  pin: <path d="m9 3 10 10-4 1-4 4-1-4-5-5 4-1zM10 15l-7 7"/>,
  keyboard: <path d="M2 5h20v14H2zM6 9h1m4 0h1m4 0h1M6 13h1m4 0h1m4 0h1M7 16h10"/>,
  bell: <path d="M5 17h14l-2-3V9a5 5 0 0 0-10 0v5zM10 21h4"/>,
  flask: <path d="M9 2h6M10 2v7L4 19c-1 2 1 3 3 3h10c2 0 4-1 3-3L14 9V2M7 15h10"/>,
  overview: <path d="M3 10 12 3l9 7v11h-6v-7H9v7H3z" />,
  actors: <><circle cx="9" cy="8" r="3" /><path d="M3 21v-3a6 6 0 0 1 12 0v3M16 4a3 3 0 0 1 0 6M17 15a5 5 0 0 1 4 5" /></>,
  inventory: <path d="m12 3 9 5-9 5-9-5zM3 8v9l9 5 9-5V8M12 13v9" />,
  components: <><path d="M4 6h16M4 12h16M4 18h16" /><circle cx="8" cy="6" r="2" /><circle cx="16" cy="12" r="2" /><circle cx="10" cy="18" r="2" /></>,
  performance: <path d="M2 12h5l3-8 4 16 3-8h5" />,
  search: <><circle cx="10" cy="10" r="6" /><path d="m15 15 6 6" /></>,
  arrow: <path d="M4 12h16m-6-6 6 6-6 6" />, check: <path d="m5 12 4 4L20 5" />, chevron: <path d="m8 4 8 8-8 8" />,
  heart: <path d="M20 4c-3-3-7 0-8 2-1-2-5-5-8-2-5 5 2 11 8 16 6-5 13-11 8-16Z" />,
  shield: <path d="m12 3 8 3v6c0 4-4 8-8 10-4-2-8-6-8-10V6z" />,
  rune: <path d="M7 3h10v18H7zM10 6l4 3-4 3 4 3-4 3" />,
  gold: <path d="m3 8 4-5h10l4 5-9 14zM3 8h18M7 3l5 19 5-19" />,
  close: <path d="m5 5 14 14M5 19 19 5" />, grid: <path d="M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3zM14 14h7v7h-7z" />, list: <path d="M8 5h13M8 12h13M8 19h13M3 5h1M3 12h1M3 19h1" />,
  sword: <path d="m4 20 4-4M6 14l4 4M8 16 20 4v6L10 18M14 4l6 6" />,
  flame: <path d="M12 2c1 6-5 5-5 10 0 2 1 3 1 3-3-1-3-4-3-4-5 13 17 14 15 1-1-4-4-5-5-7 0 4-1 4-1 4s1-5-2-7Z" />,
  book: <path d="M12 5c-4-3-8-2-10-1v15c4-2 7-1 10 1 3-2 6-3 10-1V4c-2-1-6-2-10 1Zm0 0v15" />,
  target: <><circle cx="12" cy="12" r="8" /><circle cx="12" cy="12" r="3" /><path d="M12 1v4M12 19v4M1 12h4M19 12h4" /></>,
  rotate: <path d="M20 7V2l-4 4M20 7a9 9 0 1 0 1 9" />, plus: <path d="M12 4v16M4 12h16" />, pause: <path d="M7 4v16M17 4v16" />, play: <path d="m7 3 14 9-14 9z" />,
};
export function Icon({ name, size = 18 }: { name: IconName; size?: number }): React.JSX.Element {
  return <svg width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.35" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">{paths[name]}</svg>;
}
export function itemIcon(category: string): IconName { return ({ weapon: "sword", armor: "shield", potion: "flame", book: "book", ingredient: "flask", ammo: "target", scroll: "book", "soul gem": "gold", key: "archive" } as Record<string, IconName>)[category.toLowerCase()] ?? "inventory"; }

export type SkillIconName =
  | "heavyArmor"
  | "lightArmor"
  | "oneHanded"
  | "twoHanded"
  | "archery"
  | "block"
  | "smithing"
  | "alteration"
  | "conjuration"
  | "destruction"
  | "illusion"
  | "restoration"
  | "enchanting"
  | "sneak"
  | "lockpicking"
  | "pickpocket"
  | "speech"
  | "alchemy";

const skillPaths: Record<SkillIconName, React.ReactNode> = {
  // 1. Heavy Armor — Horned Nordic plate helm & cheekguards
  heavyArmor: (
    <>
      <path d="M7 9c0-3.3 2.2-5.5 5-5.5s5 2.2 5 5.5v7l-2.5 3.5h-5L7 16V9Z" />
      <path d="M7 10C4 9.5 2.5 7 3 4c1.8 1.5 3 2.5 4.3 3.2" />
      <path d="M17 10c3-.5 4.5-3 4-6-1.8 1.5-3 2.5-4.3 3.2" />
      <path d="M12 3.5v16M7 12h10M9.5 14.5h1.5m2 0h1.5" />
    </>
  ),
  // 2. Light Armor — Agile ranger cuirass with winged shoulder guards
  lightArmor: (
    <>
      <path d="M8 4 4 6.5 5.5 11 8 9.5V19l4 2 4-2V9.5L18.5 11 20 6.5 16 4l-2 2.5h-4L8 4Z" />
      <path d="M8 12.5 12 15l4-2.5M8 16l4 2 4-2M12 6.5V15" />
    </>
  ),
  // 3. One-Handed — Single-handed Nordic broadsword
  oneHanded: (
    <>
      <path d="M19.5 4.5 15 5l-8.5 8.5 3 3L18 8l1.5-3.5Z" />
      <path d="M5 12l7 7M6.5 15.5 4 18l2 2 2.5-2.5" />
      <circle cx="3.5" cy="20.5" r="1.2" />
      <path d="M16.5 7.5 9 15" />
    </>
  ),
  // 4. Two-Handed — Double-bitted Nordic greataxe
  twoHanded: (
    <>
      <path d="M12 3v18" />
      <path d="M12 6C8.5 4.5 5.5 5.5 4 8.5c1.5 1.8 1.5 4.2 0 6 1.5 3 4.5 4 8 2.5" />
      <path d="M12 6c3.5-1.5 6.5-.5 8 2.5-1.5 1.8-1.5 4.2 0 6-1.5 3-4.5 4-8 2.5" />
      <circle cx="12" cy="11.5" r="1.6" />
    </>
  ),
  // 5. Archery — Recurved bow & nocked arrow
  archery: (
    <>
      <path d="M6 3c7 1.5 11 5.5 12.5 12.5" />
      <path d="M6 3 17.5 18.5M5 19 19 5M14.5 5H19v4.5" />
      <path d="M5 19l-2 1m2-1 1 2" />
    </>
  ),
  // 6. Block — Heraldic kite shield with central boss
  block: (
    <>
      <path d="M12 2.5 19.5 5.5v6.5c0 5-3.4 8.2-7.5 10-4.1-1.8-7.5-5-7.5-10V5.5L12 2.5Z" />
      <circle cx="12" cy="11.5" r="2.5" />
      <path d="M12 2.5V9m0 5v8M4.5 11.5H9.5m5 0h5" />
    </>
  ),
  // 7. Smithing — Blacksmith anvil & raised forge hammer
  smithing: (
    <>
      <path d="M3 14h13l3 2.5-3 1.5h-3l1.5 3h-8L8 18H5v-4Z" />
      <path d="m13 4 4.5 4.5-2.2 2.2L10.8 6.2 13 4ZM15.2 8.5 20 13.2" />
      <path d="M6 9.5 7.5 11M9 7.5l.5 2" />
    </>
  ),
  // 8. Alteration — Transmutation prism & living branches
  alteration: (
    <>
      <path d="M12 3 21 19.5H3L12 3Z" />
      <path d="M12 9v10.5M12 13l-3-2.5M12 14.5l3-2.5" />
      <circle cx="12" cy="7.5" r="1.2" />
    </>
  ),
  // 9. Conjuration — Oblivion gate sigil & summoning star
  conjuration: (
    <>
      <path d="M5.5 20V11a6.5 6.5 0 0 1 13 0v9" />
      <path d="M3 20h18M8.5 20v-7.5a3.5 3.5 0 0 1 7 0V20" />
      <path d="M12 2v3M7 4l1.5 2M17 4l-1.5 2M12 10.5l1.5 3-3-1.8h3l-3 1.8 1.5-3Z" />
    </>
  ),
  // 10. Destruction — Arcane fireball & lightning bolt burst
  destruction: (
    <>
      <path d="M12 2.5c1.5 4.5-3.5 4.5-3.5 8.5 0 2.2 1.6 4 3.5 4s3.5-1.8 3.5-4c0-2-1.2-3.3-2-4.8 3.5 1 6 4.2 6 8.3a7.5 7.5 0 1 1-15 0c0-3.4 2-6.5 5-7.8" />
      <path d="m13 9-2.5 4h3l-2 4.5" />
    </>
  ),
  // 11. Illusion — Hypnotic mystic eye & mind waves
  illusion: (
    <>
      <path d="M2 12s3.8-6.5 10-6.5S22 12 22 12s-3.8 6.5-10 6.5S2 12 2 12Z" />
      <circle cx="12" cy="12" r="3.5" />
      <circle cx="12" cy="12" r="1" />
      <path d="M12 2.5v2M7 3.8l1 1.7M17 3.8l-1 1.7M12 21.5v-2" />
    </>
  ),
  // 12. Restoration — Eight-point sunburst healing star
  restoration: (
    <>
      <path d="m12 2 2.4 5.6L20 6l-1.6 5.6L24 14l-5.6 2.4L20 22l-5.6-1.6L12 26" transform="scale(0.84) translate(2.3, 1.2)" />
      <circle cx="12" cy="12" r="4.2" />
      <path d="M12 9.5v5M9.5 12h5" />
    </>
  ),
  // 13. Enchanting — Faceted soul gem & arcane circle
  enchanting: (
    <>
      <polygon points="12 2.5 17.5 8.5 15 21 9 21 6.5 8.5" />
      <path d="M6.5 8.5h11M12 2.5v18.5M12 8.5 9 21m3-12.5L15 21" />
      <circle cx="12" cy="12" r="9.2" strokeDasharray="2.2 2.8" />
    </>
  ),
  // 14. Sneak — Shadowed rogue cowl & crossed daggers
  sneak: (
    <>
      <path d="M12 3C7.5 3 4.5 6.8 4 12l3.5 2.5L12 13l4.5 1.5L20 12c-.5-5.2-3.5-9-8-9Z" />
      <path d="M8.5 9.5 12 11l3.5-1.5" />
      <path d="m6 20.5 11-6.5M18 20.5 7 14" />
    </>
  ),
  // 15. Lockpicking — Tumbler keyhole & inserted tension pick
  lockpicking: (
    <>
      <rect x="4" y="4" width="14" height="16" rx="3" />
      <circle cx="11" cy="10.5" r="2.2" />
      <path d="M9.8 12.3 9 16.5h4l-.8-4.2" />
      <path d="m13 9.5 8-5.5v2.5l-2 1.2" />
    </>
  ),
  // 16. Pickpocket — Tied coin pouch & rising Septim
  pickpocket: (
    <>
      <path d="M8.5 9h7l1.8-3.5-2.8 1-2.5-1.5-2.5 1.5-2.8-1L8.5 9Z" />
      <path d="M8.5 9C5.5 10.8 4.5 14 5 17.2 5.5 19.8 8 21 12 21s6.5-1.2 7-3.8c.5-3.2-.5-6.4-3.5-8.2" />
      <circle cx="18.5" cy="5.5" r="2.8" />
      <path d="M12 13.5v4M10.5 15h3" />
    </>
  ),
  // 17. Speech — Imperial laurel wreath & proclamation scroll
  speech: (
    <>
      <path d="M12 19.5c-4.5-1-7.5-4.8-7.5-9.5 0-3 1.5-5.5 4-7" />
      <path d="M12 19.5c4.5-1 7.5-4.8 7.5-9.5 0-3-1.5-5.5-4-7" />
      <path d="M4.5 11.5 7 10M5.2 15l2.6-1M19.5 11.5 17 10M18.8 15l-2.6-1" />
      <path d="M9 9.5h6M9 12.5h6M10.5 15.5h3" />
    </>
  ),
  // 18. Alchemy — Bubbling alembic potion flask & botanical leaf
  alchemy: (
    <>
      <path d="M9.5 3h5M10.5 3v5.2L5.2 16.8A2.8 2.8 0 0 0 7.6 21h8.8a2.8 2.8 0 0 0 2.4-4.2L13.5 8.2V3" />
      <path d="M6.8 15.5c2.2 1 3.5-1 5.5 0s3.2 1 5 0" />
      <circle cx="10" cy="18" r="1" />
      <circle cx="13.8" cy="17.2" r="1.2" />
    </>
  ),
};

export function skillIcon(skillIdOrLabel: string): SkillIconName {
  const key = skillIdOrLabel.trim().toLowerCase().replace(/[\s_-]+/g, "");
  const map: Record<string, SkillIconName> = {
    heavyarmor: "heavyArmor",
    lightarmor: "lightArmor",
    onehanded: "oneHanded",
    twohanded: "twoHanded",
    archery: "archery",
    marksman: "archery",
    block: "block",
    smithing: "smithing",
    alteration: "alteration",
    conjuration: "conjuration",
    destruction: "destruction",
    illusion: "illusion",
    restoration: "restoration",
    enchanting: "enchanting",
    sneak: "sneak",
    lockpicking: "lockpicking",
    pickpocket: "pickpocket",
    speech: "speech",
    speechcraft: "speech",
    alchemy: "alchemy",
  };
  return map[key] ?? "enchanting";
}

export function SkillIcon({ skill, size = 20 }: { skill: string; size?: number }): React.JSX.Element {
  const name = skillIcon(skill);
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.45"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      className={`skill-svg skill-svg--${name}`}
    >
      {skillPaths[name]}
    </svg>
  );
}

export type EffectIconName =
  | "alchemy"
  | "fire"
  | "frost"
  | "shock"
  | "ward"
  | "combat"
  | "stealth"
  | "restoration"
  | "conjuration"
  | "illusion"
  | "arcane"
  | "affliction";

const effectPaths: Record<EffectIconName, React.ReactNode> = {
  // Alchemical Alembic & Botanical Elixir
  alchemy: (
    <>
      <path d="M9.5 3h5M10.5 3v4.8L5 16.5A2.8 2.8 0 0 0 7.4 21h9.2a2.8 2.8 0 0 0 2.4-4.5L13.5 7.8V3" />
      <path d="M6.6 15.2c2.2 1.2 3.8-1.2 5.6 0s3.4 1.2 5.2 0" />
      <circle cx="9.8" cy="18" r="1.1" />
      <circle cx="13.8" cy="17.2" r="1.3" />
      <path d="M15.5 5.5c1.8-.8 3.5 0 3.5 1.8-1.8.8-3.5 0-3.5-1.8Z" />
    </>
  ),
  // Destruction: Solar & Dragonfire Crest
  fire: (
    <>
      <path d="M12 2.5c1.5 3.6 5.8 5.4 5.8 10.5a5.8 5.8 0 0 1-11.6 0c0-3.2 1.8-5.6 3.3-7.3.3 1.9 1.6 2.9 2.5 3.3-.4-2.4.2-4.6 0-6.5Z" />
      <path d="M12 12.2c1 1.4 2.3 2.3 2.3 4.2a2.3 2.3 0 0 1-4.6 0c0-1.5.9-2.8 2.3-4.2Z" />
    </>
  ),
  // Destruction: Nordic Glacial Crystal
  frost: (
    <>
      <path d="M12 2.5v19M4.2 7l15.6 10M19.8 7 4.2 17" />
      <polygon points="12 6.5 15.5 10 15.5 14 12 17.5 8.5 14 8.5 10" />
      <path d="m10 4 2 2 2-2M10 20l2-2 2 2" />
    </>
  ),
  // Destruction: Aetherial Storm Bolt
  shock: (
    <>
      <polygon points="13.5 2.5 5.5 13 11.5 13 9.8 21.5 18.5 10.5 12.5 10.5" />
      <path d="M4 6.5 6.5 8M20 16l-2.5-1.5" />
    </>
  ),
  // Alteration / Ward: Runed Aegis Bastion
  ward: (
    <>
      <path d="M12 2.5 4.5 5.8v5.7c0 5.4 3.4 8.7 7.5 10.3 4.1-1.6 7.5-4.9 7.5-10.3V5.8L12 2.5Z" />
      <polygon points="12 6.8 15.6 12 12 17.2 8.4 12" />
      <circle cx="12" cy="12" r="1.4" />
    </>
  ),
  // Martial Enchantment: Crossed Greatsword & War Crest
  combat: (
    <>
      <path d="m4.5 19.5 14-14M14.5 5.5h4v4" />
      <path d="m6.2 14.2 3.6 3.6M4 20l2.5-2.5" />
      <path d="m19.5 19.5-14-14M9.5 5.5h-4v4" />
      <circle cx="12" cy="12" r="2.6" />
    </>
  ),
  // Shadow / Carry / Deftness: Nocturnal Crescent & Feather
  stealth: (
    <>
      <path d="M15.8 4.2a7.8 7.8 0 1 0 4 13.6A6.8 6.8 0 0 1 15.8 4.2Z" />
      <path d="m9.5 9.5 9-5-5 9-4-4Z" />
      <circle cx="12.5" cy="11.5" r="1.1" />
    </>
  ),
  // Restoration / Divine Blessing: Sunburst Chalice & Wings
  restoration: (
    <>
      <circle cx="12" cy="12" r="5" />
      <path d="M12 9.2v5.6M9.2 12h5.6" />
      <path d="M12 2.5v2.3M12 19.2v2.3M2.5 12h2.3M19.2 12h2.3M5.3 5.3 7 7M17 17l1.7 1.7M18.7 5.3 17 7M7 17l-1.7 1.7" />
    </>
  ),
  // Conjuration / Soul Binding: Oblivion Gate & Soul Gem
  conjuration: (
    <>
      <polygon points="12 3 17.2 9 14.8 20.5 9.2 20.5 6.8 9" />
      <path d="M6.8 9h10.4M12 3v17.5" />
      <path d="M4.2 14.5A8.5 8.5 0 0 1 12 4a8.5 8.5 0 0 1 7.8 10.5" strokeDasharray="2 2.2" />
    </>
  ),
  // Illusion / Speech / Mind: Mystic All-Seeing Eye
  illusion: (
    <>
      <path d="M2.5 12S6.2 6.2 12 6.2 21.5 12 21.5 12 17.8 17.8 12 17.8 2.5 12 2.5 12Z" />
      <circle cx="12" cy="12" r="3.2" />
      <path d="M12 3v1.6M6.5 4.5l1 1.4M17.5 4.5l-1 1.4" />
    </>
  ),
  // Arcane / Alteration / Enchanting: Sovereign Octagram
  arcane: (
    <>
      <rect x="5.5" y="5.5" width="13" height="13" rx="1.2" />
      <rect x="5.5" y="5.5" width="13" height="13" rx="1.2" transform="rotate(45 12 12)" />
      <circle cx="12" cy="12" r="2.8" />
    </>
  ),
  // Harmful Affliction / Poison / Curse: Barbed Blight Sigil
  affliction: (
    <>
      <polygon points="12 2.5 15 8.5 21.5 9.2 16.5 13.8 18 20.5 12 17 6 20.5 7.5 13.8 2.5 9.2 9 8.5" />
      <circle cx="12" cy="12.2" r="2.4" />
      <path d="M12 9.8v4.8" />
    </>
  ),
};

export interface EffectVisualMeta {
  icon: EffectIconName;
  schoolLabel: string;
  category: "alchemy" | "arcane" | "combat" | "stealth" | "divine" | "harmful";
  accent: string;
}

export function classifyEffectVisual(name: string, hostile = false, duration = 0): EffectVisualMeta {
  const lower = name.toLowerCase();
  if (hostile || /damage|weakness|poison|disease|drain|ravage|blight|curse|sanguinare|rot/.test(lower)) {
    return { icon: "affliction", schoolLabel: "Harmful Affliction", category: "harmful", accent: "#ff5c7c" };
  }
  if (/alchemy|potion|elixir|regenerate|stamina rate|magicka rate|heal rate|herb/.test(lower)) {
    return { icon: "alchemy", schoolLabel: "Alchemical Influence", category: "alchemy", accent: "#50e3a4" };
  }
  if (/fire|flame|sun|burn|ignite|warm/.test(lower)) {
    return { icon: "fire", schoolLabel: "Solar & Fire Magic", category: "arcane", accent: "#ff8552" };
  }
  if (/frost|ice|cold|chill|glacial|snow/.test(lower)) {
    return { icon: "frost", schoolLabel: "Glacial Frost Magic", category: "arcane", accent: "#63d2ff" };
  }
  if (/shock|lightning|storm|spark|thunder/.test(lower)) {
    return { icon: "shock", schoolLabel: "Storm & Shock Magic", category: "arcane", accent: "#a594ff" };
  }
  if (/armor|resist|ward|shield|block|flesh|absorb|deflect/.test(lower)) {
    return { icon: "ward", schoolLabel: "Protective Ward & Aegis", category: "combat", accent: "#f0b860" };
  }
  if (/one-handed|two-handed|archery|marksman|smithing|unarmed|power|shout|bash|stamina|weapon|damage/.test(lower)) {
    return { icon: "combat", schoolLabel: "Martial Mastery", category: "combat", accent: "#ff7e5f" };
  }
  if (/sneak|muffle|invisib|lockpick|pickpocket|carry|light foot|shadow|deft/.test(lower)) {
    return { icon: "stealth", schoolLabel: "Shadow & Subterfuge", category: "stealth", accent: "#88e878" };
  }
  if (/restoration|heal|health|blessing|shrine|divine|talos|mara|akatosh|arkay|dibella|julianos|kynareth|stendarr|zenithar|cure/.test(lower)) {
    return { icon: "restoration", schoolLabel: "Divine & Restoration", category: "divine", accent: "#ffd166" };
  }
  if (/conjuration|summon|bound|soul|atronach|undead|thrall|familiar/.test(lower)) {
    return { icon: "conjuration", schoolLabel: "Conjuration & Soulcraft", category: "arcane", accent: "#c77dff" };
  }
  if (/illusion|speech|barter|persuasion|calm|fear|fury|courage|clairvoyance|rally/.test(lower)) {
    return { icon: "illusion", schoolLabel: "Illusion & Eloquent Mind", category: "stealth", accent: "#48bfe3" };
  }
  return {
    icon: "arcane",
    schoolLabel: duration > 0 ? "Active Arcane Spell" : "Constant Enchantment",
    category: "arcane",
    accent: "#5ce1ff",
  };
}

export function EffectIcon({
  name,
  hostile = false,
  duration = 0,
  size = 22,
}: {
  name: string;
  hostile?: boolean;
  duration?: number;
  size?: number;
}): React.JSX.Element {
  const meta = classifyEffectVisual(name, hostile, duration);
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      className={`effect-svg effect-svg--${meta.icon}`}
    >
      {effectPaths[meta.icon]}
    </svg>
  );
}
