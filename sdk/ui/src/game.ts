/** Optional game surfaces. Data, events and styling are explicit; no theme is injected. */
export { ActorStage as SkyActorStage } from './field/ActorStage.js';
export { ItemStage as SkyItemStage } from './field/ItemStage.js';
export { default as SkyActorDirectory } from './field/ActorsPage.js';
export { default as SkyInventoryBrowser } from './field/InventoryPage.js';
export { default as SkyCharacterSheet } from './field/CharacterPage.js';
export { default as SkyHudEditor } from './field/WidgetsPage.js';
export { default as SkyComponentGallery } from './field/FeatureLab.js';
export { default as SkyPerformanceLab } from './field/PerformancePage.js';
export { default as SkyFieldGuide } from './field/GuidePage.js';
export { default as SkyCommandOverlay } from './field/FieldOverlays.js';
export { HudWidget, HudWidget as SkyGameWidget, widgetCatalog } from './field/hudWidgets.js';
export type { WidgetKind } from './field/hudWidgets.js';
export type { Player, Item, Npc, Palette } from './field/model.js';
export { usePlayer as useSkyPlayer, useActors as useSkyActors, useCadence as useSkyCadence } from './field/useFieldData.js';

export { SkyHudComposer } from './composer.js';
export type { SkyHudComposerProps } from './composer.js';

export { SkyGameplayHud } from "./gameplay.js";
