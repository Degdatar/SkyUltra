import * as React from 'react';
import type { SkyUltraEquipmentSlot } from '@skyultra/sdk';

/** Slot silhouettes, deliberately separate from the rendered item artwork. */
const paths: Partial<Record<SkyUltraEquipmentSlot, React.ReactNode>> = {
    head: <><path d="M9 26V14a9 9 0 0 1 18 0v12l-6 3v-9h-6v9zM9 14l6 4m12-4-6 4M18 5V2"/><path d="M5 13l4 3m18 0 4-3"/></>,
    body: <path d="m10 7 5-3h6l5 3 5 3-4 9-3-1 2 13H10l2-13-3 1-4-9zM15 4l3 8 3-8M12 22h12M18 12v17"/>,
    hands: <path d="m10 30-3-11 1-5 3 1V7l3-1 1 10V4l3-1 1 13 1-10 3 1v11l2-6 3 1-1 10-7 7zM10 26h13"/>,
    feet: <path d="m13 4 12 1-1 12-4 8 10 2v4H6v-6l6-8zM12 17h12M8 26h12M14 8l9 1"/>,
    cape: <path d="m13 5 5-2 5 2 7 25-12-4-12 4zM13 5l5 5 5-5M18 10v16M10 17l3-5m13 5-3-5"/>,
    neck: <><path d="M9 5C2 24 34 24 27 5M12 5c-3 13 15 13 12 0"/><path d="m18 19 5 6-5 7-5-7z"/></>,
    ring: <><circle cx="18" cy="23" r="9"/><path d="m11 8 3-5h8l3 5-7 7zM11 8h14M14 3l4 12 4-12"/></>,
    mainHand: <path d="m8 29 5-5m-4-3 6 6m-2-3L29 8V3h-5L8 19m12-12 8 8M5 32l3-3"/>,
    offHand: <path d="m18 3 12 5v10c0 7-12 14-12 14S6 25 6 18V8zM18 8v18M11 12h14"/>,
    ammo: <path d="m8 29 17-17M7 22l6 1 1 6m4-21 12-5-5 12-1-7zM5 31l3-2"/>,
    lantern: <><path d="M10 13h16l-2 16H12zM8 13h20M12 9h12l-6-5zM18 4V2M11 30h14M18 16v9"/><circle cx="18" cy="21" r="3"/></>,
  };
export function EquipmentGlyph({ slot }: { slot: SkyUltraEquipmentSlot }): React.JSX.Element {
  return <svg className="equipment-glyph" viewBox="0 0 36 36" fill="none" stroke="currentColor" strokeWidth="1.15" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">{paths[slot]}</svg>;
}
