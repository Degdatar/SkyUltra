import * as React from 'react';
import type { SkyUltraEquipmentSlot } from '@skyultra/sdk';

export const skyEquipmentSlotIds: readonly Exclude<SkyUltraEquipmentSlot, 'auto'>[] = [
  'head',
  'body',
  'hands',
  'feet',
  'cape',
  'neck',
  'ring',
  'mainHand',
  'offHand',
  'ammo',
  'lantern',
];

/** Sculpted Nordic armory silhouettes, deliberately separate from rendered 3D item artwork. */
const paths: Partial<Record<SkyUltraEquipmentSlot, React.ReactNode>> = {
  head: (
    <>
      <path d="M9.5 26.5V14.5a8.5 8.5 0 0 1 17 0v12l-5.5 3v-8.8h-6v8.8z" fill="currentColor" fillOpacity=".1" />
      <path d="M9.5 26.5V14.5a8.5 8.5 0 0 1 17 0v12l-5.5 3v-8.8h-6v8.8z" />
      <path d="M18 3.2v3M9.5 14.2 15 17.8m11.5-3.6L21 17.8M5.2 11.8c.6 2.5 2 4 4.3 4.6M30.8 11.8c-.6 2.5-2 4-4.3 4.6" />
      <path d="M12.5 19.6h11" strokeOpacity=".6" strokeWidth="1" />
    </>
  ),
  body: (
    <>
      <path d="m10 7.2 5-3h6l5 3 5 3.4-4 8.2-2.6-1 1.6 13.4H10l1.6-13.4-2.6 1-4-8.2z" fill="currentColor" fillOpacity=".1" />
      <path d="m10 7.2 5-3h6l5 3 5 3.4-4 8.2-2.6-1 1.6 13.4H10l1.6-13.4-2.6 1-4-8.2z" />
      <path d="M15 4.2 18 11l3-6.8M11.6 21.8h12.8M18 11v19.2" />
      <path d="M11 26.2h14" strokeOpacity=".6" strokeWidth="1" />
    </>
  ),
  hands: (
    <>
      <path d="m10 29.8-2.8-10.4.8-5 3 1V7.4l2.8-1 1 9.4V4.4l2.8-.8.9 12.2 1-9.4 2.8 1v10.2l2.1-5.6 2.8 1-1.2 9.8-6.6 7z" fill="currentColor" fillOpacity=".1" />
      <path d="m10 29.8-2.8-10.4.8-5 3 1V7.4l2.8-1 1 9.4V4.4l2.8-.8.9 12.2 1-9.4 2.8 1v10.2l2.1-5.6 2.8 1-1.2 9.8-6.6 7z" />
      <path d="M10 25.5h12.6M10.8 21.5h11.6" strokeOpacity=".65" strokeWidth="1" />
    </>
  ),
  feet: (
    <>
      <path d="m12.5 4.5 11.2 1-.9 11.4-3.8 7.8 9.8 1.8v4.2H6.5v-5.6l5.8-7.8z" fill="currentColor" fillOpacity=".1" />
      <path d="m12.5 4.5 11.2 1-.9 11.4-3.8 7.8 9.8 1.8v4.2H6.5v-5.6l5.8-7.8z" />
      <path d="M12.2 17.2h11.4M8.5 26.2h10.8M13.8 8.6l8.4.8" strokeOpacity=".65" strokeWidth="1" />
    </>
  ),
  cape: (
    <>
      <path d="m12.8 5.4 5.2-2.2 5.2 2.2 6.8 24.4-12-4-12 4z" fill="currentColor" fillOpacity=".1" />
      <path d="m12.8 5.4 5.2-2.2 5.2 2.2 6.8 24.4-12-4-12 4z" />
      <path d="M12.8 5.4 18 10.6l5.2-5.2M18 10.6v15.2M10.2 18.2l3.4-6M25.8 18.2l-3.4-6" strokeOpacity=".72" />
    </>
  ),
  neck: (
    <>
      <path d="M9.5 5.2C3 22.8 33 22.8 26.5 5.2M12.2 5.2c-3 12 14.6 12 11.6 0" />
      <polygon points="18 18.4 23 24.6 18 31.2 13 24.6" fill="currentColor" fillOpacity=".16" />
      <polygon points="18 18.4 23 24.6 18 31.2 13 24.6" />
      <path d="M18 20.6v8.4M15.4 24.6h5.2" strokeOpacity=".6" strokeWidth="1" />
    </>
  ),
  ring: (
    <>
      <circle cx="18" cy="22.8" r="8.6" />
      <circle cx="18" cy="22.8" r="5.4" strokeOpacity=".55" strokeWidth=".95" />
      <polygon points="11.8 8.6 14.6 4.2 21.4 4.2 24.2 8.6 18 14.6" fill="currentColor" fillOpacity=".18" />
      <polygon points="11.8 8.6 14.6 4.2 21.4 4.2 24.2 8.6 18 14.6" />
      <path d="M11.8 8.6h12.4M14.6 4.2 18 14.6 21.4 4.2" strokeOpacity=".65" strokeWidth="1" />
    </>
  ),
  mainHand: (
    <>
      <path d="m11.5 22.8 16.8-16.6v4.6L13.8 25" fill="currentColor" fillOpacity=".14" />
      <path d="m7.8 28.4 5-5m-4.2-3.4 6.2 6.2M11.2 22.6 28.4 5.4V3.6h-1.8L10.4 19.8" />
      <path d="m16.2 18.2 7-7" strokeOpacity=".6" strokeWidth="1" />
      <circle cx="6.2" cy="30" r="1.4" fill="currentColor" fillOpacity=".2" />
    </>
  ),
  offHand: (
    <>
      <path d="m18 3.6 11.4 4.6v9.8c0 6.8-11.4 13.6-11.4 13.6S6.6 24.8 6.6 18V8.2z" fill="currentColor" fillOpacity=".1" />
      <path d="m18 3.6 11.4 4.6v9.8c0 6.8-11.4 13.6-11.4 13.6S6.6 24.8 6.6 18V8.2z" />
      <path d="M18 7.8v19M10.4 13.2h15.2" strokeOpacity=".65" strokeWidth="1" />
      <circle cx="18" cy="14.5" r="2.6" fill="currentColor" fillOpacity=".18" />
    </>
  ),
  ammo: (
    <>
      <path d="m8.2 28.4 17.6-17.6" />
      <polygon points="29 7.6 22.2 9.4 24.8 11.8 27 14.4" fill="currentColor" fillOpacity=".2" />
      <polygon points="29 7.6 22.2 9.4 24.8 11.8 27 14.4" />
      <path d="m6.2 24.6 3.6.6.6 3.6-3.2 2.4-2.4-2.4z" fill="currentColor" fillOpacity=".14" />
      <path d="m10.8 21.8 1.8 3.2 3.2 1.8" strokeOpacity=".7" />
    </>
  ),
  lantern: (
    <>
      <path d="M18 3v2.4M12.4 9.2h11.2L18 5.4zM8.6 12.6h18.8" />
      <polygon points="10.4 12.6 25.6 12.6 23.4 28.2 12.6 28.2" fill="currentColor" fillOpacity=".12" />
      <polygon points="10.4 12.6 25.6 12.6 23.4 28.2 12.6 28.2" />
      <path d="M18 16.2c1.6 2.2 2.6 3.5 2.6 5a2.6 2.6 0 0 1-5.2 0c0-1.5 1-2.8 2.6-5Z" fill="currentColor" fillOpacity=".26" />
      <path d="M14.4 12.6v15.6M21.6 12.6v15.6M11 30.8h14" strokeOpacity=".7" />
    </>
  ),
};

export function EquipmentGlyph({ slot, size, className }: { slot: SkyUltraEquipmentSlot; size?: number; className?: string }): React.JSX.Element {
  return (
    <svg
      width={size}
      height={size}
      className={className ? `equipment-glyph ${className}` : 'equipment-glyph'}
      viewBox="0 0 36 36"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.3"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      {paths[slot]}
    </svg>
  );
}
