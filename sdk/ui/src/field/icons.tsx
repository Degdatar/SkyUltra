import * as React from "react";

export type IconName =
  | "archive"
  | "overview"
  | "actors"
  | "inventory"
  | "components"
  | "performance"
  | "search"
  | "arrow"
  | "check"
  | "chevron"
  | "heart"
  | "shield"
  | "rune"
  | "gold"
  | "close"
  | "grid"
  | "list"
  | "sword"
  | "flame"
  | "book"
  | "bookRead"
  | "target"
  | "rotate"
  | "plus"
  | "pause"
  | "play"
  | "widgets"
  | "help"
  | "compass"
  | "clock"
  | "pin"
  | "keyboard"
  | "bell"
  | "flask"
  | "camera"
  | "download"
  | "stolen"
  | "refresh"
  | "pose"
  | "film"
  | "camBust"
  | "camThreeQuarter"
  | "camFullBody"
  | "camProfile"
  | "potion"
  | "ingredient"
  | "ammo"
  | "scroll"
  | "soulGem"
  | "key"
  | "misc"
  | "enchant"
  | "weight"
  | "lantern"
  | "outfit"
  | "palette"
  | "droplet"
  | "sliders"
  | "layers"
  | "copy"
  | "alertTriangle"
  | "bug"
  | "terminal"
  | "code"
  | "cpu"
  | "gamepad"
  | "eye"
  | "eyeOff"
  | "lock"
  | "unlock"
  | "sun"
  | "moon"
  | "filter"
  | "trash"
  | "crown"
  | "bolt"
  | "upload"
  | "move"
  | "resizeCorner"
  | "anvil"
  | "smithIron"
  | "smithSteel"
  | "smithAdvanced"
  | "smithImperial"
  | "smithHide"
  | "smithLeather"
  | "smithDunmer"
  | "smithElven"
  | "smithDwarven"
  | "smithOrcish"
  | "smithNordic"
  | "smithGlass"
  | "smithEbony"
  | "smithStalhrim"
  | "smithDaedric"
  | "smithDragon"
  | "smithJewelry"
  | "smithIngot"
  | "smithPelt"
  | "smithMisc"
  | "pen";

const paths: Record<IconName, React.ReactNode> = {
  refresh: (
    <>
      <path d="M20.5 11.5a8.5 8.5 0 0 0-14.9-5.6L3.5 8" />
      <path d="M3.5 3.5V8H8" />
      <path d="M3.5 12.5a8.5 8.5 0 0 0 14.9 5.6l2.1-2.1" />
      <path d="M16 16h4.5v4.5" />
    </>
  ),
  pose: (
    <>
      <circle cx="12" cy="5.2" r="2.2" fill="currentColor" fillOpacity=".16" />
      <path d="M12 8.6v5.2m-4.8-3 4.8-1.4 4.8 1.4M12 13.8l-3.2 6.4m3.2-6.4 3.2 6.4" />
      <path d="M6.8 11.4l-.8 3M17.2 11.4l.8 3" strokeOpacity=".65" />
    </>
  ),
  film: (
    <>
      <rect x="3" y="4.8" width="18" height="14.4" rx="2.2" />
      <rect x="7" y="4.8" width="10" height="14.4" fill="currentColor" fillOpacity=".12" stroke="none" />
      <path d="M7.2 4.8v14.4M16.8 4.8v14.4M3 9.4h4.2m-4.2 5.2h4.2M16.8 9.4H21m-4.2 5.2H21" />
    </>
  ),
  camBust: (
    <>
      <path d="M3.5 7V4h3M17.5 4h3v3M3.5 17v3h3M17.5 20h3v-3" />
      <circle cx="12" cy="9.8" r="2.6" fill="currentColor" fillOpacity=".16" />
      <path d="M7.4 18.5a4.8 4.8 0 0 1 9.2 0" />
    </>
  ),
  camThreeQuarter: (
    <>
      <path d="M4 7V4h3M17 4h3v3M4 17v3h3M17 20h3v-3" />
      <circle cx="10.8" cy="9.8" r="2.4" fill="currentColor" fillOpacity=".16" />
      <path d="M6.8 18.5c.8-2.6 2.8-3.8 5-3.8 1.8 0 3.2.8 4.2 2.4" />
    </>
  ),
  camFullBody: (
    <>
      <path d="M5 4h2M5 20h2M17 4h2M17 20h2M5 4v3M19 4v3M5 17v3M19 17v3" />
      <circle cx="12" cy="6.8" r="1.8" fill="currentColor" fillOpacity=".16" />
      <path d="M12 9.4v5.2M8.8 11l3.2-1.2 3.2 1.2M12 14.6l-2.3 4.6M12 14.6l2.3 4.6" />
    </>
  ),
  camProfile: (
    <>
      <path d="M3.5 7V4h3M17.5 4h3v3M3.5 17v3h3M17.5 20h3v-3" />
      <path d="M10.5 7.4a3.5 3.5 0 0 1 3.6 3.5c0 .9-.3 1.4.8 2.1.4.3.2.8-.4.9h-.8v1.3c0 .9-.6 1.5-1.5 1.5H10l-1 1.7" />
    </>
  ),
  stolen: (
    <>
      <path d="M18 11V6a1.4 1.4 0 0 0-2.8 0v4M15.2 10V4.5a1.4 1.4 0 0 0-2.8 0V10M12.4 9.5V3.5a1.4 1.4 0 0 0-2.8 0V10M9.6 10V5.5a1.4 1.4 0 0 0-2.8 0V14" />
      <path d="M6.8 12.6 4.7 11a1.5 1.5 0 0 0-2.1 2.1l4.1 5.2A6 6 0 0 0 11.4 20.5h1.6a5 5 0 0 0 5-5V11" />
      <circle cx="12.2" cy="15.5" r="1.8" fill="currentColor" fillOpacity=".2" />
    </>
  ),
  camera: (
    <>
      <path d="M3 7.5h3.8l1.8-3h6.8l1.8 3H21v12H3z" fill="currentColor" fillOpacity=".12" />
      <path d="M3 7.5h3.8l1.8-3h6.8l1.8 3H21v12H3z" />
      <circle cx="12" cy="13.2" r="3.6" fill="currentColor" fillOpacity=".14" />
      <circle cx="12" cy="13.2" r="1.4" />
      <path d="M17.5 10h.5" strokeWidth="1.8" />
    </>
  ),
  download: (
    <>
      <path d="M12 3.5v11.2m-4.6-4.6 4.6 4.6 4.6-4.6" />
      <path d="M4.5 15.5v3.2a1.8 1.8 0 0 0 1.8 1.8h11.4a1.8 1.8 0 0 0 1.8-1.8v-3.2" />
    </>
  ),
  archive: (
    <>
      <path d="M4.5 7.5h15v12.5h-15z" fill="currentColor" fillOpacity=".12" />
      <path d="M4.5 7.5h15v12.5h-15zM3 4h18v3.5H3zM9.2 12h5.6" />
    </>
  ),
  widgets: (
    <>
      <rect x="3" y="3" width="8" height="6.2" rx="1.4" fill="currentColor" fillOpacity=".15" />
      <rect x="13.8" y="3" width="7.2" height="9.4" rx="1.4" />
      <rect x="3" y="11.6" width="8" height="9.4" rx="1.4" />
      <rect x="13.8" y="14.8" width="7.2" height="6.2" rx="1.4" fill="currentColor" fillOpacity=".15" />
    </>
  ),
  help: (
    <>
      <circle cx="12" cy="12" r="9" fill="currentColor" fillOpacity=".1" />
      <path d="M9.3 9a2.8 2.8 0 0 1 5.4 1c0 1.9-2.7 2.2-2.7 4.4M12 17.6h.01" />
    </>
  ),
  compass: (
    <>
      <circle cx="12" cy="12" r="9" />
      <polygon points="15.8 8.2 13.4 13.4 8.2 15.8 10.6 10.6" fill="currentColor" fillOpacity=".2" />
      <path d="M12 3v1.6M12 19.4V21M3 12h1.6M19.4 12H21" strokeOpacity=".6" />
    </>
  ),
  clock: (
    <>
      <circle cx="12" cy="12" r="9" fill="currentColor" fillOpacity=".1" />
      <path d="M12 6.8V12l3.8 2.2" />
      <path d="M12 5v.8M12 18.2V19M5 12h.8M18.2 12H19" strokeOpacity=".55" />
    </>
  ),
  pin: (
    <>
      <path d="m14.2 3.4 6.4 6.4-2.1 2.1-3.4-.8-3.6 3.6.7 3.3-1.8 1.8L6 15.4 3.5 20.5 8.6 18l-4.4-4.4 1.8-1.8 3.3.7 3.6-3.6-.8-3.4z" fill="currentColor" fillOpacity=".14" />
      <path d="m14.2 3.4 6.4 6.4-2.1 2.1-3.4-.8-3.6 3.6.7 3.3-1.8 1.8L6 15.4 3.5 20.5 8.6 18l-4.4-4.4 1.8-1.8 3.3.7 3.6-3.6-.8-3.4z" />
    </>
  ),
  keyboard: (
    <>
      <rect x="2.5" y="5.5" width="19" height="13" rx="2" fill="currentColor" fillOpacity=".1" />
      <path d="M6 9.5h1.2M10 9.5h1.2M14 9.5h1.2M18 9.5h.1M6 12.8h1.2M10 12.8h1.2M14 12.8h4M7.2 15.8h9.6" />
    </>
  ),
  bell: (
    <>
      <path d="M6 16.5h12l-1.6-2.8V9.5a4.4 4.4 0 0 0-8.8 0v4.2z" fill="currentColor" fillOpacity=".15" />
      <path d="M5 16.5h14l-2-3V9.5a5 5 0 0 0-10 0v4zM10 19.6a2 2 0 0 0 4 0" />
    </>
  ),
  flask: (
    <>
      <path d="M9 2.5h6M10.2 2.5v6.2L4.6 18.2c-.8 1.5.2 3.3 1.9 3.3h11c1.7 0 2.7-1.8 1.9-3.3l-5.6-9.5V2.5" />
      <path d="M6.4 15.2h11.2l1.6 2.8a1.5 1.5 0 0 1-1.3 2.3H6.1a1.5 1.5 0 0 1-1.3-2.3z" fill="currentColor" fillOpacity=".18" stroke="none" />
      <path d="M7.2 15h9.6" />
    </>
  ),
  potion: (
    <>
      <path d="M9.6 2.5h4.8v2.2H9.6z" fill="currentColor" fillOpacity=".2" />
      <path d="M9.6 2.5h4.8M10 4.8v2.7L5.8 11.4a5.4 5.4 0 0 0-1.3 3.5v1.3A4.8 4.8 0 0 0 9.3 21h5.4a4.8 4.8 0 0 0 4.8-4.8v-1.3a5.4 5.4 0 0 0-1.3-3.5L14 7.5V4.8" />
      <path d="M5.4 14.2c1.8-1 3.8.8 6.6 0s4.8-1 6.6 0v2a3.8 3.8 0 0 1-3.8 3.8H9.2a3.8 3.8 0 0 1-3.8-3.8z" fill="currentColor" fillOpacity=".22" stroke="none" />
      <path d="M5.6 14c1.8-.9 3.8.8 6.4 0s4.6-.9 6.4 0" />
      <path d="M8.5 10.6a2.4 2.4 0 0 0-1.1 2" strokeOpacity=".6" />
    </>
  ),
  ingredient: (
    <>
      <path d="M4.5 20C6.5 15 11.2 10.8 18.8 5" />
      <path d="M18.8 5c.6 5.2-1.8 9.6-6.2 10.6-1.8.4-3.4-.2-4.6-1.4C7 12.8 6.8 10.6 7.8 9 9.5 6.2 13.6 4.6 18.8 5Z" fill="currentColor" fillOpacity=".16" />
      <path d="M18.8 5c.6 5.2-1.8 9.6-6.2 10.6-1.8.4-3.4-.2-4.6-1.4C7 12.8 6.8 10.6 7.8 9 9.5 6.2 13.6 4.6 18.8 5Z" />
      <path d="M11.6 11.6 10.2 8.2M14.4 9.2l-1-2.6" strokeOpacity=".7" />
      <path d="M4.5 16.5a2.8 2.8 0 0 1 3.8 1.2" />
    </>
  ),
  ammo: (
    <>
      <path d="M5.5 18.5 18.2 5.8" />
      <polygon points="20.5 3.5 14.5 5.2 16.8 7.2 18.8 9.5" fill="currentColor" fillOpacity=".22" />
      <polygon points="20.5 3.5 14.5 5.2 16.8 7.2 18.8 9.5" />
      <path d="M4 16l2.5.4.4 2.5L4.5 21 3 19.5z" fill="currentColor" fillOpacity=".16" />
      <path d="M7.5 14l1.2 2.4L11 17.6" strokeOpacity=".75" />
    </>
  ),
  scroll: (
    <>
      <path d="M8 4.2h10.5a2.2 2.2 0 0 1 0 4.4H17v10.2a2.2 2.2 0 0 1-2.2 2.2H5.5a2.2 2.2 0 0 1 0-4.4H7V6.4A2.2 2.2 0 0 1 8 4.2Z" fill="currentColor" fillOpacity=".12" />
      <path d="M7 6.4A2.2 2.2 0 0 1 9.2 4.2h9.1a2.2 2.2 0 1 1 0 4.4H17v9.2a2.2 2.2 0 0 1-2.2 2.2H5.7a2.2 2.2 0 1 1 0-4.4H7V6.4Z" />
      <path d="M16.2 4.2a2.2 2.2 0 0 0-2.2 2.2v2.2h3M7 15.6H5.7M9.6 9.8h4.8M9.6 13h4.2" />
    </>
  ),
  soulGem: (
    <>
      <polygon points="12 2.4 17.6 8.2 15.6 19.6 12 21.8 8.4 19.6 6.4 8.2" fill="currentColor" fillOpacity=".18" />
      <polygon points="12 2.4 17.6 8.2 15.6 19.6 12 21.8 8.4 19.6 6.4 8.2" />
      <path d="M6.4 8.2 12 10.6l5.6-2.4M12 2.4v19.4M8.4 19.6 12 10.6l3.6 9" strokeOpacity=".68" />
    </>
  ),
  key: (
    <>
      <circle cx="8.2" cy="9.2" r="4.2" fill="currentColor" fillOpacity=".16" />
      <circle cx="8.2" cy="9.2" r="4.2" />
      <circle cx="8.2" cy="9.2" r="1.4" />
      <path d="m11.4 12.2 8.8 8.6M16.2 17l2.4-2.2M18.6 19.4l2.4-2.2M14.2 15l1.6-1.5" />
    </>
  ),
  misc: (
    <>
      <path d="M4 9.5a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v8.2a2.2 2.2 0 0 1-2.2 2.2H6.2A2.2 2.2 0 0 1 4 17.7z" fill="currentColor" fillOpacity=".13" />
      <path d="M4 9.5a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v8.2a2.2 2.2 0 0 1-2.2 2.2H6.2A2.2 2.2 0 0 1 4 17.7z" />
      <path d="M8.5 7.5V5.6A1.6 1.6 0 0 1 10.1 4h3.8a1.6 1.6 0 0 1 1.6 1.6v1.9M4 12.5h16M10.4 12.5v2h3.2v-2" />
    </>
  ),
  enchant: (
    <>
      <polygon points="12 2.8 19.8 7.4 19.8 16.6 12 21.2 4.2 16.6 4.2 7.4" fill="currentColor" fillOpacity=".1" />
      <polygon points="12 2.8 19.8 7.4 19.8 16.6 12 21.2 4.2 16.6 4.2 7.4" />
      <polygon points="12 6.8 16.4 12 12 17.2 7.6 12" fill="currentColor" fillOpacity=".18" />
      <polygon points="12 6.8 16.4 12 12 17.2 7.6 12" />
      <circle cx="12" cy="12" r="1.4" fill="currentColor" />
    </>
  ),
  weight: (
    <>
      <circle cx="12" cy="5.8" r="2.1" />
      <path d="M7.2 9.4h9.6l2.4 9.6a1.4 1.4 0 0 1-1.4 1.8H6.2A1.4 1.4 0 0 1 4.8 19z" fill="currentColor" fillOpacity=".16" />
      <path d="M7.2 9.4h9.6l2.4 9.6a1.4 1.4 0 0 1-1.4 1.8H6.2A1.4 1.4 0 0 1 4.8 19z" />
      <path d="M9.6 14.8h4.8" strokeOpacity=".75" />
    </>
  ),
  lantern: (
    <>
      <path d="M9.2 4.8h5.6M12 2.5v2.3M8 7.8h8l-1.4-3h-5.2z" />
      <polygon points="7.4 7.8 16.6 7.8 15.2 18.8 8.8 18.8" fill="currentColor" fillOpacity=".15" />
      <polygon points="7.4 7.8 16.6 7.8 15.2 18.8 8.8 18.8" />
      <path d="M12 10.6c1 1.4 1.6 2.3 1.6 3.4a1.6 1.6 0 0 1-3.2 0c0-1.1.6-2 1.6-3.4Z" fill="currentColor" fillOpacity=".32" />
      <path d="M8.2 21h7.6M10.4 7.8v11M13.6 7.8v11" strokeOpacity=".75" />
    </>
  ),
  outfit: (
    <>
      <path d="M8.5 4.2 4.2 7.2 6 12l2.5-1.4v9.6h7v-9.6L18 12l1.8-4.8-4.3-3-1.8 2.4h-3.4z" fill="currentColor" fillOpacity=".14" />
      <path d="M8.5 4.2 4.2 7.2 6 12l2.5-1.4v9.6h7v-9.6L18 12l1.8-4.8-4.3-3-1.8 2.4h-3.4z" />
      <path d="M12 6.6v13.6" strokeOpacity=".6" />
    </>
  ),
  overview: (
    <>
      <path d="M3.5 10.2 12 3.2l8.5 7v10.6H15v-6.4H9v6.4H3.5z" fill="currentColor" fillOpacity=".12" />
      <path d="M3.5 10.2 12 3.2l8.5 7v10.6H15v-6.4H9v6.4H3.5z" />
    </>
  ),
  actors: (
    <>
      <circle cx="9.2" cy="8" r="3" fill="currentColor" fillOpacity=".16" />
      <path d="M3.2 20.8v-2.6a6 6 0 0 1 12 0v2.6M16 4.6a2.8 2.8 0 0 1 0 5.6M17.2 15.2a4.6 4.6 0 0 1 3.8 4.6" />
    </>
  ),
  inventory: (
    <>
      <polygon points="12 3 20.5 7.8 12 12.6 3.5 7.8" fill="currentColor" fillOpacity=".16" />
      <path d="m12 3 8.5 4.8L12 12.6 3.5 7.8zM3.5 7.8v8.6l8.5 4.6 8.5-4.6V7.8M12 12.6V21" />
    </>
  ),
  components: (
    <>
      <path d="M4 6h16M4 12h16M4 18h16" />
      <circle cx="8" cy="6" r="2.1" fill="currentColor" fillOpacity=".2" />
      <circle cx="16" cy="12" r="2.1" fill="currentColor" fillOpacity=".2" />
      <circle cx="10" cy="18" r="2.1" fill="currentColor" fillOpacity=".2" />
    </>
  ),
  performance: <path d="M2.5 12h4.6l2.8-7.8 4 15.6 2.8-7.8H21.5" />,
  search: (
    <>
      <circle cx="10.2" cy="10.2" r="6.2" fill="currentColor" fillOpacity=".09" />
      <circle cx="10.2" cy="10.2" r="6.2" />
      <path d="m15 15 5.6 5.6" />
    </>
  ),
  arrow: <path d="M4 12h16m-6-6 6 6-6 6" />,
  check: <path d="m5 12.2 4.2 4.2L19.5 5.5" />,
  chevron: <path d="m8.5 4.5 7.5 7.5-7.5 7.5" />,
  heart: (
    <path
      d="M19.8 5.4c-2.6-2.5-6.2-.8-7.8 1.5-1.6-2.3-5.2-4-7.8-1.5-3.8 3.6.8 9.6 7.8 14.8 7-5.2 11.6-11.2 7.8-14.8Z"
      fill="currentColor"
      fillOpacity=".16"
    />
  ),
  shield: (
    <>
      <path d="m12 2.8 7.8 3.1v5.8c0 4.4-3.4 8-7.8 9.7-4.4-1.7-7.8-5.3-7.8-9.7V5.9z" fill="currentColor" fillOpacity=".15" />
      <path d="m12 2.8 7.8 3.1v5.8c0 4.4-3.4 8-7.8 9.7-4.4-1.7-7.8-5.3-7.8-9.7V5.9z" />
      <path d="M12 6.8v11M8.2 10.6h7.6" strokeOpacity=".65" />
    </>
  ),
  rune: (
    <>
      <rect x="6.5" y="3" width="11" height="18" rx="1.6" fill="currentColor" fillOpacity=".12" />
      <path d="M6.5 3h11v18h-11zM10 6.5l4 2.8-4 2.8 4 2.8-4 2.8" />
    </>
  ),
  gold: (
    <>
      <polygon points="3.5 8.5 7.2 3.5 16.8 3.5 20.5 8.5 12 21" fill="currentColor" fillOpacity=".18" />
      <path d="m3.5 8.5 3.7-5h9.6l3.7 5L12 21zM3.5 8.5h17M7.2 3.5 12 21l4.8-17.5" />
    </>
  ),
  close: <path d="m5.5 5.5 13 13M5.5 18.5 18.5 5.5" />,
  grid: (
    <>
      <rect x="3.2" y="3.2" width="7.2" height="7.2" rx="1.4" fill="currentColor" fillOpacity=".14" />
      <rect x="13.6" y="3.2" width="7.2" height="7.2" rx="1.4" />
      <rect x="3.2" y="13.6" width="7.2" height="7.2" rx="1.4" />
      <rect x="13.6" y="13.6" width="7.2" height="7.2" rx="1.4" fill="currentColor" fillOpacity=".14" />
    </>
  ),
  list: <path d="M8.5 5.5H20.5M8.5 12H20.5M8.5 18.5H20.5M3.5 5.5h1.5M3.5 12h1.5M3.5 18.5h1.5" />,
  sword: (
    <>
      <path d="m4 20 3.8-3.8M5.8 14.2l4 4M8.5 15.5 20 4v5L10.5 17.5" fill="currentColor" fillOpacity=".16" />
      <path d="m4 20 3.8-3.8M5.8 14.2l4 4M8.5 15.5 20 4v5L10.5 17.5M14.2 5l4.8 4.8" />
      <path d="M10.8 14.8 17.6 8" strokeOpacity=".6" />
    </>
  ),
  flame: (
    <path
      d="M12.2 2.4c1 5.6-4.4 5-4.4 9.6 0 1.8.8 3 1 3.2-2.6-.9-2.8-3.5-2.8-3.5C2 19 8.8 22 14.2 20.6c4.2-1 6-5.2 4.8-9.2-1-3.4-3.6-4.8-4.5-6.6 0 3.4-1 3.8-1 3.8s.8-4.4-1.3-6.2Z"
      fill="currentColor"
      fillOpacity=".16"
    />
  ),
  book: (
    <>
      <path d="M12 5.4c-3.8-2.6-7.6-1.8-9.4-.8v14.8c3.6-1.7 6.8-.9 9.4.9 2.6-1.8 5.8-2.6 9.4-.9V4.6c-1.8-1-5.6-1.8-9.4.8Z" fill="currentColor" fillOpacity=".13" />
      <path d="M12 5.4c-3.8-2.6-7.6-1.8-9.4-.8v14.8c3.6-1.7 6.8-.9 9.4.9 2.6-1.8 5.8-2.6 9.4-.9V4.6c-1.8-1-5.6-1.8-9.4.8Zm0 0v14.9" />
      <path d="M5.5 9.6l3.4.5M5.5 13.2l3.4.5M18.5 9.6l-3.4.5M18.5 13.2l-3.4.5" strokeOpacity=".6" />
    </>
  ),
  bookRead: (
    <>
      <path d="M9 7.5c-2.1-.7-4.2-.7-6 .1V19c3.1-1.1 6.1-.6 9 1.5 2.9-2.1 5.9-2.6 9-1.5V7.6c-1.8-.8-3.9-.8-6-.1M12 10v10.5M5.5 11.5l3 .6M5.5 14.5l3 .6" />
      <path d="M9.5 3.5h5V9L12 7.4 9.5 9Z" fill="currentColor" fillOpacity=".18" />
      <path d="m14.5 13 1.7 1.7 3-3.4" strokeWidth="1.7" />
    </>
  ),
  target: (
    <>
      <circle cx="12" cy="12" r="7.8" fill="currentColor" fillOpacity=".09" />
      <circle cx="12" cy="12" r="3" />
      <path d="M12 1.8v3.6M12 18.6v3.6M1.8 12h3.6M18.6 12h3.6" />
    </>
  ),
  rotate: <path d="M20 7.2V2.6l-4.2 4.2M20 7.2a8.6 8.6 0 1 0 1 8.8" />,
  plus: <path d="M12 4.5v15M4.5 12h15" />,
  pause: <path d="M7.5 4.5v15M16.5 4.5v15" />,
  play: <polygon points="7 4 20 12 7 20" fill="currentColor" fillOpacity=".2" />,
  palette: (
    <>
      <path d="M12 3a9 9 0 0 0 0 18 2 2 0 0 0 2-2 1.8 1.8 0 0 1 1.8-1.8H18a3 3 0 0 0 3-3A9 9 0 0 0 12 3Z" fill="currentColor" fillOpacity=".12" />
      <path d="M12 3a9 9 0 0 0 0 18 2 2 0 0 0 2-2 1.8 1.8 0 0 1 1.8-1.8H18a3 3 0 0 0 3-3A9 9 0 0 0 12 3Z" />
      <circle cx="8" cy="11" r="1.2" fill="currentColor" />
      <circle cx="11" cy="7.5" r="1.2" fill="currentColor" />
      <circle cx="15.5" cy="9" r="1.2" fill="currentColor" />
    </>
  ),
  droplet: (
    <>
      <path d="M12 2.8 6.4 10.6a6.4 6.4 0 1 0 11.2 0Z" fill="currentColor" fillOpacity=".18" />
      <path d="M12 2.8 6.4 10.6a6.4 6.4 0 1 0 11.2 0Z" />
      <path d="M9.5 15.2a3.2 3.2 0 0 0 2.6 2.2" strokeOpacity=".65" />
    </>
  ),
  sliders: (
    <>
      <path d="M4 7h16M4 17h16M4 12h16" strokeOpacity=".65" />
      <circle cx="9" cy="7" r="2.2" fill="currentColor" fillOpacity=".22" />
      <circle cx="15" cy="12" r="2.2" fill="currentColor" fillOpacity=".22" />
      <circle cx="8" cy="17" r="2.2" fill="currentColor" fillOpacity=".22" />
    </>
  ),
  layers: (
    <>
      <polygon points="12 3 21 7.8 12 12.6 3 7.8" fill="currentColor" fillOpacity=".18" />
      <polygon points="12 3 21 7.8 12 12.6 3 7.8" />
      <path d="m3 12.2 9 4.8 9-4.8M3 16.5l9 4.8 9-4.8" />
    </>
  ),
  copy: (
    <>
      <rect x="8.5" y="8.5" width="11.5" height="12" rx="2" fill="currentColor" fillOpacity=".14" />
      <rect x="8.5" y="8.5" width="11.5" height="12" rx="2" />
      <path d="M15.5 8.5V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v9.5a2 2 0 0 0 2 2h2.5" />
    </>
  ),
  alertTriangle: (
    <>
      <path d="m10.4 4.2-7.2 12.6A1.8 1.8 0 0 0 4.8 19.5h14.4a1.8 1.8 0 0 0 1.6-2.7L13.6 4.2a1.8 1.8 0 0 0-3.2 0Z" fill="currentColor" fillOpacity=".16" />
      <path d="m10.4 4.2-7.2 12.6A1.8 1.8 0 0 0 4.8 19.5h14.4a1.8 1.8 0 0 0 1.6-2.7L13.6 4.2a1.8 1.8 0 0 0-3.2 0Z" />
      <path d="M12 9.4v4.4M12 16.6h.01" strokeWidth="1.7" />
    </>
  ),
  bug: (
    <>
      <ellipse cx="12" cy="14" rx="5" ry="6.2" fill="currentColor" fillOpacity=".15" />
      <ellipse cx="12" cy="14" rx="5" ry="6.2" />
      <path d="M9.5 8.2a2.8 2.8 0 0 1 5 0M12 8v12M4.5 11.5l2.8 1.5M19.5 11.5l-2.8 1.5M4 16l3-.8M20 16l-3-.8M5.5 20l2.2-1.8M18.5 20l-2.2-1.8" />
    </>
  ),
  terminal: (
    <>
      <rect x="3" y="4.5" width="18" height="15" rx="2.4" fill="currentColor" fillOpacity=".12" />
      <rect x="3" y="4.5" width="18" height="15" rx="2.4" />
      <path d="m7.5 9.5 3 2.5-3 2.5M12.5 14.5h4" />
    </>
  ),
  code: (
    <>
      <path d="m8.2 7.5-4.7 4.5 4.7 4.5M15.8 7.5l4.7 4.5-4.7 4.5M13.5 5.2l-3 13.6" />
    </>
  ),
  cpu: (
    <>
      <rect x="6" y="6" width="12" height="12" rx="2" fill="currentColor" fillOpacity=".14" />
      <rect x="6" y="6" width="12" height="12" rx="2" />
      <rect x="9.5" y="9.5" width="5" height="5" rx=".8" fill="currentColor" fillOpacity=".24" />
      <path d="M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3" />
    </>
  ),
  gamepad: (
    <>
      <path d="M6.5 8.5h11a3.8 3.8 0 0 1 3.7 4.5l-.9 4.3a2.4 2.4 0 0 1-4.2 1l-1.9-2.3H9.8l-1.9 2.3a2.4 2.4 0 0 1-4.2-1L2.8 13a3.8 3.8 0 0 1 3.7-4.5Z" fill="currentColor" fillOpacity=".14" />
      <path d="M6.5 8.5h11a3.8 3.8 0 0 1 3.7 4.5l-.9 4.3a2.4 2.4 0 0 1-4.2 1l-1.9-2.3H9.8l-1.9 2.3a2.4 2.4 0 0 1-4.2-1L2.8 13a3.8 3.8 0 0 1 3.7-4.5Z" />
      <path d="M7.8 11.2v3.2M6.2 12.8h3.2" />
      <circle cx="15.2" cy="11.8" r=".8" fill="currentColor" />
      <circle cx="17.2" cy="13.6" r=".8" fill="currentColor" />
    </>
  ),
  eye: (
    <>
      <path d="M2.5 12S6 6 12 6s9.5 6 9.5 6-3.5 6-9.5 6S2.5 12 2.5 12Z" fill="currentColor" fillOpacity=".11" />
      <path d="M2.5 12S6 6 12 6s9.5 6 9.5 6-3.5 6-9.5 6S2.5 12 2.5 12Z" />
      <circle cx="12" cy="12" r="3" fill="currentColor" fillOpacity=".22" />
      <circle cx="12" cy="12" r="3" />
    </>
  ),
  eyeOff: (
    <>
      <path d="M4.2 7.5A12.4 12.4 0 0 1 12 6c6 0 9.5 6 9.5 6a14 14 0 0 1-3.2 3.8M14.2 17.7A10 10 0 0 1 12 18c-6 0-9.5-6-9.5-6a13 13 0 0 1 2.2-2.9" />
      <path d="m4 4 16 16" />
    </>
  ),
  lock: (
    <>
      <rect x="5" y="10.5" width="14" height="10" rx="2.4" fill="currentColor" fillOpacity=".16" />
      <rect x="5" y="10.5" width="14" height="10" rx="2.4" />
      <path d="M8 10.5V7.6a4 4 0 0 1 8 0v2.9M12 14.5v2.2" />
    </>
  ),
  unlock: (
    <>
      <rect x="5" y="10.5" width="14" height="10" rx="2.4" fill="currentColor" fillOpacity=".14" />
      <rect x="5" y="10.5" width="14" height="10" rx="2.4" />
      <path d="M8 10.5V7.6a4 4 0 0 1 7.6-1.8M12 14.5v2.2" />
    </>
  ),
  sun: (
    <>
      <circle cx="12" cy="12" r="4.2" fill="currentColor" fillOpacity=".2" />
      <circle cx="12" cy="12" r="4.2" />
      <path d="M12 2.6v2.2M12 19.2v2.2M2.6 12h2.2M19.2 12h2.2M5.4 5.4l1.5 1.5M17.1 17.1l1.5 1.5M18.6 5.4l-1.5 1.5M6.9 17.1l-1.5 1.5" />
    </>
  ),
  moon: (
    <path
      d="M19.8 14.6A7.8 7.8 0 0 1 9.4 4.2a8.2 8.2 0 1 0 10.4 10.4Z"
      fill="currentColor"
      fillOpacity=".18"
    />
  ),
  filter: (
    <>
      <polygon points="3.5 5 20.5 5 14 12.6 14 19 10 20.5 10 12.6" fill="currentColor" fillOpacity=".15" />
      <polygon points="3.5 5 20.5 5 14 12.6 14 19 10 20.5 10 12.6" />
    </>
  ),
  trash: (
    <>
      <path d="M5 7h14M9 7V4.5h6V7M7.2 7l.8 13h8l.8-13" fill="currentColor" fillOpacity=".12" />
      <path d="M4.5 7h15M9 7V4.5h6V7M7 7l1 13h8l1-13M10.4 10.5v6M13.6 10.5v6" />
    </>
  ),
  crown: (
    <>
      <path d="m4 17.5-1.2-8.8L8 12l4-6.5L16 12l5.2-3.3-1.2 8.8Z" fill="currentColor" fillOpacity=".18" />
      <path d="m4 17.5-1.2-8.8L8 12l4-6.5L16 12l5.2-3.3-1.2 8.8ZM5 20h14" />
    </>
  ),
  bolt: (
    <polygon
      points="13.4 2.5 5.6 13 11.4 13 9.8 21.5 18.4 10.6 12.6 10.6"
      fill="currentColor"
      fillOpacity=".2"
    />
  ),
  upload: (
    <>
      <path d="M12 15.5V4.3m-4.6 4.6L12 4.3l4.6 4.6" />
      <path d="M4.5 15.5v3.2a1.8 1.8 0 0 0 1.8 1.8h11.4a1.8 1.8 0 0 0 1.8-1.8v-3.2" />
    </>
  ),
  move: (
    <>
      <path d="M12 3v18M3 12h18" />
      <path d="m9.2 5.8 2.8-2.8 2.8 2.8M9.2 18.2 12 21l2.8-2.8M5.8 9.2 3 12l2.8 2.8M18.2 9.2 21 12l-2.8 2.8" />
    </>
  ),
  resizeCorner: (
    <>
      <path d="M14.5 4.5h5v5M19.5 4.5l-6 6M9.5 19.5h-5v-5M4.5 19.5l6-6" />
    </>
  ),
  anvil: (
    <>
      <path d="M3 10.5h4.5V8h9.5l4-1.5v2.5c0 2-1.8 3.5-4.2 3.5H15v4l2.5 3H6.5l2.5-3v-4H7.2C4.8 12.5 3 11.7 3 10.5Z" />
      <path d="M5 19.5h14" />
    </>
  ),
  // Iron — classic banded iron nasal helm with cheek guards
  smithIron: (
    <>
      <path d="M6.5 11.5C6.5 7 8.8 4.5 12 4.5s5.5 2.5 5.5 7v5.5L15 20v-4.5h-6V20l-2.5-3v-5.5Z" />
      <path d="M6.5 11.5h11M12 4.5v10" />
    </>
  ),
  // Steel — heavy plated barbute / T-visor knight helm
  smithSteel: (
    <>
      <path d="M6 10c0-3.5 2.6-6 6-6s6 2.5 6 6v8.5l-6 2.5-6-2.5V10Z" />
      <path d="M8.5 10.5h7M12 10.5v7.5M12 4v4" />
    </>
  ),
  // Advanced — layered steel-plate / scaled breastplate with pauldrons
  smithAdvanced: (
    <>
      <path d="M7.5 4.5 4 7l1.5 4.5L8 10v9.5l4 1.5 4-1.5V10l2.5 1.5L20 7l-3.5-2.5-2.5 2h-4l-2.5-2Z" />
      <path d="M8 13h8M8 16.5l4 1.5 4-1.5" />
    </>
  ),
  // Imperial — Roman centurion galeated helm with sweeping plume crest
  smithImperial: (
    <>
      <path d="M7.5 4c2.8-1.5 6.2-1.5 9 0l-.8 2.8H8.3L7.5 4Z" fill="currentColor" fillOpacity=".14" />
      <path d="M7 11c0-2.6 2.2-4.2 5-4.2s5 1.6 5 4.2v6l-2.5 3v-5h-5v5L7 17v-6Z" />
      <path d="M7 11.5h10" />
    </>
  ),
  // Hide — rough stretched animal pelt with fur trim
  smithHide: (
    <>
      <path d="M7 4.5 9.5 6h5L17 4.5l2 3.5-1.8 4L19 16.5 16 19.5l-4-1.5-4 1.5-3-3L6.8 12 5 8l2-3.5Z" />
      <path d="M12 6.5v11.5M9.5 10l5 4" strokeOpacity=".6" />
    </>
  ),
  // Leather — scout studded brigandine cuirass with cross straps
  smithLeather: (
    <>
      <path d="M8 5 6 8v10.5l6 2 6-2V8l-2-3-2 2h-4L8 5Z" />
      <path d="M6.5 12.5h11M8 8.5l8 7M16 8.5l-8 7" />
    </>
  ),
  // Dunmer — Morrowind Chitin / Bonemold masked guard helm
  smithDunmer: (
    <>
      <path d="M8 6.5 12 3.5l4 3 1.5 5.5-2.2 6-3.3 2.5-3.3-2.5-2.2-6L8 6.5Z" />
      <path d="M8.5 10.5 12 12l3.5-1.5M12 12v8.5M9 14.5h2m2 0h2" />
    </>
  ),
  // Elven — swept Altmeri eagle-wing golden helm
  smithElven: (
    <>
      <path d="M12 3 9 7.5 5 5l1.5 6.5 5.5 9 5.5-9L19 5l-4 2.5L12 3Z" />
      <path d="M12 3v17.5M8.5 11.5l3.5 2 3.5-2" />
    </>
  ),
  // Dwarven — mechanical Dwemer brass gear & slotted visor
  smithDwarven: (
    <>
      <circle cx="12" cy="12" r="5" />
      <circle cx="12" cy="12" r="1.8" />
      <path d="M12 3v3M12 18v3M3 12h3M18 12h3M5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1M18.4 5.6l-2.1 2.1M7.7 16.3l-2.1 2.1" />
    </>
  ),
  // Orcish — angular jagged-cheek Orsimer warhelm with brow spikes
  smithOrcish: (
    <>
      <path d="M5.5 6 9 8.5l3-4.5 3 4.5L18.5 6l-.5 6.5-3 7.5H9L6 12.5 5.5 6Z" />
      <path d="M7 11.5h10M10 15.5h4M12 4v7.5" />
    </>
  ),
  // Nordic — carved Nordic bear/wolf-ear heavy warhelm
  smithNordic: (
    <>
      <path d="M5 10.5C3.2 8.2 3.8 5 6 4c1.2 1 2 2.2 2.5 3.5h7C16 6.2 16.8 5 18 4c2.2 1 2.8 4.2 1 6.5v6.5L15 20l-3-2-3 2-4-3v-6.5Z" />
      <path d="M12 7.5v10.5M7 12.5h10" />
    </>
  ),
  // Glass — sharp malachite crystalline prism shard
  smithGlass: (
    <>
      <path d="M12 2.5 18.5 9 15 20.5H9L5.5 9 12 2.5Z" fill="currentColor" fillOpacity=".12" />
      <path d="M12 2.5v18M5.5 9h13M9 20.5l3-11.5 3 11.5" />
    </>
  ),
  // Ebony — pointed tall volcanic-obsidian sovereign knight helm
  smithEbony: (
    <>
      <path d="M12 2.5 16.5 8v8.5L12 21.5 7.5 16.5V8L12 2.5Z" />
      <path d="M8.5 10.5 12 12l3.5-1.5M9.5 14.5 12 16l2.5-1.5M12 2.5v19" />
    </>
  ),
  // Stalhrim — jagged Solstheim enchanted frost crystal cluster
  smithStalhrim: (
    <>
      <path d="M12 2 16 8.5l3.5-1.5-2 10-5.5 4.5L6.5 17l-2-10L8 8.5 12 2Z" />
      <path d="M12 2v19.5M8 8.5l4 4 4-4" />
    </>
  ),
  // Daedric — sweeping twin-horned demonic warhelm
  smithDaedric: (
    <>
      <path d="M3.5 3.5C4 8 6 10.5 8 11.5v5.5L12 21l4-4v-5.5c2-1 4-3.5 4.5-8-2.2 2-3.8 3.2-5 3.8L12 4.5 8.5 7.3C7.3 6.7 5.7 5.5 3.5 3.5Z" />
      <path d="M9.5 12.5 12 14l2.5-1.5M12 4.5v16.5" />
    </>
  ),
  // Dragon — dragon-bone horned skull crest & wings
  smithDragon: (
    <>
      <path d="M4 5.5c1.5 2.8 3 4.2 5 4.5l3-4 3 4c2-.3 3.5-1.7 5-4.5-.5 4.5-2.2 7.2-4.5 8.5l-1 6h-5l-1-6C6.2 12.7 4.5 10 4 5.5Z" />
      <path d="M9.5 14h5M12 6v14" />
    </>
  ),
  // Jewelry — diamond gemstone set on a circlet ring
  smithJewelry: (
    <>
      <path d="M12 3.5 15.5 7 12 10.5 8.5 7 12 3.5Z" fill="currentColor" fillOpacity=".16" />
      <circle cx="12" cy="15" r="5.5" />
      <path d="M9.5 10.2h5" />
    </>
  ),
  // Ingots & Metals — stacked smelted metal bars
  smithIngot: (
    <>
      <path d="M4 18.5 6.5 13h11l2.5 5.5H4Z" />
      <path d="M6.5 13 8.5 8h7l2 5" />
      <path d="M9.5 8l1-3h3l1 3" />
    </>
  ),
  // Leather & Pelts — tanning rack frame with stretched hide
  smithPelt: (
    <>
      <path d="M4 4.5h16M5.5 4.5v15M18.5 4.5v15M4 18.5h16" />
      <path d="M8 7.5h8l1.2 4.5-1.5 4H8.3l-1.5-4L8 7.5Z" fill="currentColor" fillOpacity=".14" />
    </>
  ),
  // Misc — crossed blacksmith hammer and tongs
  smithMisc: (
    <>
      <path d="m14.5 4.5 5 5-2.2 2.2-5-5 2.2-2.2Z" />
      <path d="M14.8 9.2 5.5 18.5" />
      <path d="m5.5 5.5 13 13M4.5 7.5l3-3M16.5 19.5l3-3" />
    </>
  ),
  pen: (
    <>
      <path d="M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4Z" />
      <path d="m13.5 6.5 3 3" />
    </>
  ),
};

export const skyIconNames = Object.keys(paths) as readonly IconName[];

export function Icon({ name, size = 18, className }: { name: IconName; size?: number; className?: string }): React.JSX.Element {
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
      className={className}
    >
      {paths[name]}
    </svg>
  );
}

export function itemIcon(category: string): IconName {
  return (
    {
      weapon: "sword",
      armor: "shield",
      potion: "potion",
      ingredient: "ingredient",
      ammo: "ammo",
      book: "book",
      scroll: "scroll",
      "soul gem": "soulGem",
      key: "key",
      miscellaneous: "misc",
    } as Record<string, IconName>
  )[category.toLowerCase()] ?? "inventory";
}

const SMITHING_CATEGORY_ICONS: Record<string, IconName> = {
  iron: "smithIron",
  steel: "smithSteel",
  advanced: "smithAdvanced",
  imperial: "smithImperial",
  hide: "smithHide",
  leather: "smithLeather",
  dunmer: "smithDunmer",
  bonemold: "smithDunmer",
  chitin: "smithDunmer",
  elven: "smithElven",
  dwarven: "smithDwarven",
  dwemer: "smithDwarven",
  orcish: "smithOrcish",
  nordic: "smithNordic",
  glass: "smithGlass",
  ebony: "smithEbony",
  stalhrim: "smithStalhrim",
  daedric: "smithDaedric",
  dragon: "smithDragon",
  jewelry: "smithJewelry",
  ammo: "ammo",
  "ingots & metals": "smithIngot",
  ingots: "smithIngot",
  smelter: "flame",
  "leather & pelts": "smithPelt",
  pelts: "smithPelt",
  misc: "smithMisc",
  miscellaneous: "smithMisc",
};

export function smithingCategoryIcon(category: string, stationFallback: IconName = "anvil"): IconName {
  const key = category.trim().toLowerCase();
  if (key === "all") return stationFallback;
  return SMITHING_CATEGORY_ICONS[key] ?? "smithMisc";
}

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
  // 9. Conjuration — Oblivion gate sigil & bound diamond prism
  conjuration: (
    <>
      <path d="M5.5 20V11a6.5 6.5 0 0 1 13 0v9" />
      <path d="M3 20h18M8.5 20v-7.5a3.5 3.5 0 0 1 7 0V20" />
      <path d="M12 2v3M7 4l1.5 2M17 4l-1.5 2" />
      <polygon points="12 10 14.2 13 12 16 9.8 13" fill="currentColor" fillOpacity=".18" />
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
  // 12. Restoration — Radiant sanctuary chalice & cross crest
  restoration: (
    <>
      <circle cx="12" cy="12" r="8.2" strokeDasharray="3 2.2" />
      <circle cx="12" cy="12" r="4.6" fill="currentColor" fillOpacity=".16" />
      <path d="M12 9.2v5.6M9.2 12h5.6" />
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
  // Harmful Affliction / Poison / Curse: Shattered Hazard Emblem
  affliction: (
    <>
      <path d="m12 2.8 8 4.4v9.6L12 21.2 4 16.8V7.2z" fill="currentColor" fillOpacity=".14" />
      <path d="m12 2.8 8 4.4v9.6L12 21.2 4 16.8V7.2z" />
      <path d="M12 7.5v5.5M12 16.2h.01" strokeWidth="1.8" />
    </>
  ),
};

export const skySkillIconNames = Object.keys(skillPaths) as readonly SkillIconName[];
export const skyEffectIconNames = Object.keys(effectPaths) as readonly EffectIconName[];

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
