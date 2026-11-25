import * as React from 'react';
import { useEffect, useMemo, useState } from 'react';
import {
  characterProfileIdForName,
  defaultSkyUltraHotkey,
  defaultSkyUltraKeyboardHotkey,
  defaultSkyUltraSettings,
  defaultSkyUltraUiPreferences,
  exportSkyUltraProfileBundle,
  formatSkyUltraHotkey,
  importSkyUltraProfileBundle,
  loadSkyUltraProfileStore,
  normalizeSkyUltraHotkey,
  persistSkyUltraProfileStore,
  promoteProfileToGlobalDefaults,
  readLocalSkyUltraProfileStore,
  readSkyUltraSettings,
  readSkyUltraSplashStatus,
  readSkyUltraUiPrefs,
  resetSkyUltraProfile,
  resolveEffectiveSkyUltraProfile,
  skyUltraActorExportResolutions,
  updateProfileForContext,
  updateSkyUltraSettings,
  writeSkyUltraUiPrefs,
  type SkyUltraActorExportResolution,
  type SkyUltraBarterDisplayMode,
  type SkyUltraDisplayMode,
  type SkyUltraEngineChoice,
  type SkyUltraHotkeyBinding,
  type SkyUltraProfileMode,
  type SkyUltraProfileStoreV2,
  type SkyUltraRegisteredMod,
  type SkyUltraSettings,
  type SkyUltraSplashStatus,
  type SkyUltraUiPreferences,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { SkyChoice, SkyToggleSwitch, SkyRangeSlider, SkyFilterChips } from '../core.js';
import { SkyColorWheel } from '../ColorWheel.js';
import { SkyHotkeyEditor, SkySettingsSection } from '../chrome.js';
import { Icon } from './icons.js';
import { palettes, type Palette } from './model.js';

const exportOptions = [...skyUltraActorExportResolutions]
  .reverse()
  .map((value) => ({ value: String(value), label: value.toLocaleString() + ' px' }));

const emptyModHotkey: SkyUltraHotkeyBinding = { keyboard: [], gamepad: [], vr: [] };

type SettingsCategory = 'all' | 'inventory' | 'magic' | 'profiles' | 'themes' | 'hotkeys' | 'engine' | 'cursor' | 'splash';

export default function SettingsPage({
  bridge,
  native,
  theme = 'dragonstone',
  onThemeChange,
  playerName,
  playerEnchantingLevel = 0,
}: {
  bridge: SkyWebViewAPI;
  native: boolean;
  theme?: Palette;
  onThemeChange?(next: Palette): void;
  playerName?: string | null;
  playerEnchantingLevel?: number;
}): React.JSX.Element {
  const [category, setCategory] = useState<SettingsCategory>(() => {
    try {
      const raw = window.localStorage.getItem('skyultra.switchState');
      if (raw) {
        const parsed = JSON.parse(raw) as { category?: SettingsCategory; ts?: number };
        if (parsed.ts && Date.now() - parsed.ts < 15000 && parsed.category) {
          window.localStorage.removeItem('skyultra.switchState');
          return parsed.category;
        }
      }
    } catch {
      // Ignore storage access errors
    }
    return 'inventory';
  });
  const [settings, setSettings] = useState<SkyUltraSettings>(defaultSkyUltraSettings);
  const [profileStore, setProfileStore] = useState<SkyUltraProfileStoreV2>(() => readLocalSkyUltraProfileStore());
  const [profileJsonOpen, setProfileJsonOpen] = useState(false);
  const [profileJsonDraft, setProfileJsonDraft] = useState('');
  const [uiPrefs, setUiPrefs] = useState<SkyUltraUiPreferences>(() => readSkyUltraUiPrefs());
  const [loaded, setLoaded] = useState(!native);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('');
  const [status, setStatus] = useState<SkyUltraSplashStatus | null>(null);
  const [selectedModId, setSelectedModId] = useState<string>('');
  const [customModInput, setCustomModInput] = useState<string>('');
  const [settingsQuery, setSettingsQuery] = useState<string>('');

  useEffect(() => {
    let active = true;
    void readSkyUltraSettings(bridge)
      .then((value) => {
        if (active) {
          setSettings(value);
          setLoaded(true);
        }
      })
      .catch((error) => {
        if (active) setMessage(String(error));
      });
    void loadSkyUltraProfileStore(bridge, playerName).then((store) => {
      if (active) {
        setProfileStore(store);
        const effective = resolveEffectiveSkyUltraProfile(store, playerName);
        setUiPrefs(effective.uiPrefs);
      }
    });
    const onProfileStore = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraProfileStoreV2>).detail;
      if (detail && active) {
        setProfileStore(detail);
        setUiPrefs(resolveEffectiveSkyUltraProfile(detail, playerName).uiPrefs);
      }
    };
    const onSettingsUpdated = (event: Event): void => {
      const detail = (event as CustomEvent<Partial<SkyUltraSettings>>).detail;
      if (detail && active) {
        setSettings((current) => ({ ...current, ...detail }));
      }
    };
    window.addEventListener('skyultra:profile-store', onProfileStore);
    window.addEventListener('skyultra:settings-updated', onSettingsUpdated);
    const refresh = (): void => {
      if (document.hidden || !(bridge.getVisibility?.().visible ?? true)) return;
      if (native)
        void readSkyUltraSplashStatus(bridge)
          .then((value) => {
            if (active) setStatus(value);
          })
          .catch(() => undefined);
    };
    refresh();
    const timer = window.setInterval(refresh, 2000);
    return () => {
      active = false;
      window.removeEventListener('skyultra:profile-store', onProfileStore);
      window.removeEventListener('skyultra:settings-updated', onSettingsUpdated);
      window.clearInterval(timer);
    };
  }, [bridge, native, playerName]);

  const effectiveProfile = useMemo(
    () => resolveEffectiveSkyUltraProfile(profileStore, playerName),
    [profileStore, playerName],
  );

  const updateUiPref = (patch: Partial<SkyUltraUiPreferences>): void => {
    const next = writeSkyUltraUiPrefs(patch);
    setUiPrefs(next);
    const nextStore = updateProfileForContext(profileStore, playerName, {
      ...(patch.defaultTheme ? { theme: patch.defaultTheme } : {}),
      uiPrefs: next,
    });
    setProfileStore(nextStore);
    void persistSkyUltraProfileStore(nextStore, bridge, playerName);
  };

  const handleProfileModeChange = (mode: SkyUltraProfileMode): void => {
    const charId = characterProfileIdForName(playerName);
    const nextStore = updateProfileForContext(profileStore, playerName, {
      mode,
      activeProfileId: mode === 'per-character' && charId !== 'global' ? charId : 'global',
    });
    setProfileStore(nextStore);
    const resolved = resolveEffectiveSkyUltraProfile(nextStore, playerName);
    setUiPrefs(resolved.uiPrefs);
    if (palettes.some((p) => p.value === resolved.theme)) {
      onThemeChange?.(resolved.theme as Palette);
    }
    void persistSkyUltraProfileStore(nextStore, bridge, playerName);
    setMessage(
      mode === 'per-character'
        ? `Switched to Per-Character Profile mode (${resolved.characterName}).`
        : 'Switched to Global Shared Defaults mode across all saves.',
    );
  };

  const handlePromoteGlobalDefaults = (): void => {
    const nextStore = promoteProfileToGlobalDefaults(profileStore, playerName);
    setProfileStore(nextStore);
    void persistSkyUltraProfileStore(nextStore, bridge, playerName);
    setMessage(`Saved current ${effectiveProfile.theme} theme, UI preferences, and HUD layout as Global Defaults.`);
  };

  const handleResetCharacterProfile = (): void => {
    const nextStore = resetSkyUltraProfile(profileStore, 'character', playerName);
    setProfileStore(nextStore);
    const resolved = resolveEffectiveSkyUltraProfile(nextStore, playerName);
    setUiPrefs(resolved.uiPrefs);
    if (palettes.some((p) => p.value === resolved.theme)) {
      onThemeChange?.(resolved.theme as Palette);
    }
    void persistSkyUltraProfileStore(nextStore, bridge, playerName);
    setMessage(`Cleared character overrides for ${resolved.characterName}; inherited Global Defaults (${resolved.theme}).`);
  };

  const handleExportProfilesJson = (): void => {
    setProfileJsonDraft(exportSkyUltraProfileBundle(profileStore));
    setProfileJsonOpen(true);
    setMessage('Generated schemaVersion 2 Profile Bundle JSON below for backup or sharing.');
  };

  const handleImportProfilesJson = (): void => {
    try {
      const imported = importSkyUltraProfileBundle(profileJsonDraft);
      setProfileStore(imported);
      const resolved = resolveEffectiveSkyUltraProfile(imported, playerName);
      setUiPrefs(resolved.uiPrefs);
      if (palettes.some((p) => p.value === resolved.theme)) {
        onThemeChange?.(resolved.theme as Palette);
      }
      void persistSkyUltraProfileStore(imported, bridge, playerName);
      setMessage(
        imported.migratedFromVersion !== undefined
          ? `Migrated legacy v${imported.migratedFromVersion} settings bundle to v2 and applied ${Object.keys(imported.profiles).length} character profile(s).`
          : `Imported profile bundle (${Object.keys(imported.profiles).length} character profile(s), global default: ${imported.globalDefaults.theme}).`,
      );
    } catch (err) {
      setMessage(`Profile import failed: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  const knownMods = useMemo<SkyUltraRegisteredMod[]>(() => {
    const isExcludedMod = (id: string): boolean => {
      const normalized = id.trim().toLowerCase();
      return (
        !normalized ||
        normalized === 'skyultra.menu' ||
        normalized === 'skyultra.ui_kit_test_mod' ||
        normalized === 'skyultra.uikit'
      );
    };
    const map = new Map<string, string>();
    for (const mod of settings.registeredMods ?? []) {
      if (mod.id && !isExcludedMod(mod.id) && mod.name !== 'SkyUltra UI Kit Sample Mod') {
        map.set(mod.id, mod.name || mod.id);
      }
    }
    for (const modId of Object.keys(settings.modHotkeys ?? {})) {
      if (modId && !isExcludedMod(modId) && !map.has(modId)) {
        map.set(modId, modId);
      }
    }
    return Array.from(map.entries()).map(([id, name]) => ({ id, name }));
  }, [settings.registeredMods, settings.modHotkeys]);

  const activeModId =
    selectedModId && knownMods.some((m) => m.id === selectedModId) ? selectedModId : knownMods[0]?.id ?? '';
  const activeModBinding = useMemo(
    () => normalizeSkyUltraHotkey(settings.modHotkeys?.[activeModId] ?? emptyModHotkey, true),
    [settings.modHotkeys, activeModId],
  );

  const splash = settings.splash;
  const change = (patch: Partial<SkyUltraSettings['splash']>): void =>
    setSettings((current) => ({ ...current, splash: { ...current.splash, ...patch } }));
  const changeExport = (key: 'fullView' | 'portrait', value: string): void =>
    setSettings((current) => ({
      ...current,
      actorExport: { ...current.actorExport, [key]: Number(value) as SkyUltraActorExportResolution },
    }));
  const changeMainHotkey = (hotkey: SkyUltraHotkeyBinding): void =>
    setSettings((current) => ({ ...current, hotkey }));
  const changeKeyboardHotkey = (keyboardHotkey: SkyUltraHotkeyBinding): void =>
    setSettings((current) => ({ ...current, keyboardHotkey }));
  const changeMenuEngine = (menuEngine: SkyUltraEngineChoice): void => {
    setSettings((current) => ({ ...current, menuEngine }));
    window.dispatchEvent(new CustomEvent('skyultra:engine-updated', { detail: menuEngine }));
  };
  const changeModEngine = (modId: string, engineValue: string): void => {
    if (!modId) return;
    setSettings((current) => {
      const nextModEngines = { ...(current.modEngines ?? {}) };
      if (engineValue === 'ultralight' || engineValue === 'cef') {
        nextModEngines[modId] = engineValue;
      } else {
        delete nextModEngines[modId];
      }
      return { ...current, modEngines: nextModEngines };
    });
  };
  const changeVrHeadTracking = (vrHeadTracking: boolean): void =>
    setSettings((current) => ({ ...current, vrHeadTracking }));
  const applyLiveSettingsPatch = (patch: Partial<SkyUltraSettings>, statusMessage?: string): void => {
    const nextSettings: SkyUltraSettings = {
      ...settings,
      ...patch,
      schemaVersion: 2,
      profileState: profileStore as unknown as Record<string, unknown>,
    };
    setSettings(nextSettings);
    window.dispatchEvent(new CustomEvent('skyultra:settings-updated', { detail: nextSettings }));
    if (statusMessage) setMessage(statusMessage);
    void updateSkyUltraSettings(nextSettings, bridge)
      .then((persisted) => {
        setSettings(persisted);
        window.dispatchEvent(new CustomEvent('skyultra:settings-updated', { detail: persisted }));
      })
      .catch(() => undefined);
  };
  const changeReplaceSkyrimInventory = (replaceSkyrimInventory: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimInventory },
      replaceSkyrimInventory
        ? 'Skyrim Player Inventory replacement enabled — opening your inventory in-game now opens SkyUltra Player Inventory & 3D Equip Stage.'
        : 'Skyrim Player Inventory replacement disabled — opening your inventory in-game now uses the normal Skyrim / SkyUI InventoryMenu.',
    );
  };
  const changeMenuDisplayMode = (menuDisplayMode: SkyUltraDisplayMode): void => {
    applyLiveSettingsPatch(
      { menuDisplayMode },
      `SkyUltra Menu display mode set to ${menuDisplayMode === 'windowed' ? 'Windowed' : 'Full Screen'}.`,
    );
  };
  const changeInventoryDisplayMode = (inventoryDisplayMode: SkyUltraDisplayMode): void => {
    applyLiveSettingsPatch(
      { inventoryDisplayMode },
      `Player Inventory display mode set to ${inventoryDisplayMode === 'windowed' ? 'Windowed' : 'Full Screen'}.`,
    );
  };
  const changeMenuPauseWorld = (menuPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { menuPauseWorld },
      menuPauseWorld
        ? 'SkyUltra Menu will now pause the Skyrim world while open.'
        : 'SkyUltra Menu will now keep the Skyrim world live (unpaused) while open.',
    );
  };
  const changeInventoryPauseWorld = (inventoryPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { inventoryPauseWorld },
      inventoryPauseWorld
        ? 'Player Inventory will now pause the Skyrim world while open.'
        : 'Player Inventory will now keep the Skyrim world live (unpaused) while open.',
    );
  };
  const changeReplaceSkyrimBarter = (replaceSkyrimBarter: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimBarter },
      replaceSkyrimBarter
        ? 'Skyrim Vendor Barter replacement enabled — trading with merchants now opens the SkyUltra Barter Container.'
        : 'Skyrim Vendor Barter replacement disabled — trading with merchants now uses the normal Skyrim / SkyUI BarterMenu.',
    );
  };
  const changeBarterDisplayMode = (barterDisplayMode: SkyUltraBarterDisplayMode): void => {
    const label =
      barterDisplayMode === 'half'
        ? 'Half Container (Left 50%)'
        : barterDisplayMode === 'full'
          ? 'Full Container'
          : 'Windowed Container';
    applyLiveSettingsPatch({ barterDisplayMode }, `Vendor Barter container mode set to ${label}.`);
  };
  const changeBarterPauseWorld = (barterPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { barterPauseWorld },
      barterPauseWorld
        ? 'Vendor Barter container will now pause the Skyrim world while trading.'
        : 'Vendor Barter container will now keep the Skyrim world live while trading.',
    );
  };
  const containerModeLabel = (mode: SkyUltraBarterDisplayMode): string =>
    mode === 'half' ? 'Half Container (Left side)' : mode === 'full' ? 'Full Container' : 'Windowed Container';
  const changeReplaceSkyrimMagic = (replaceSkyrimMagic: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimMagic },
      replaceSkyrimMagic
        ? 'Skyrim Magic menu replacement enabled — opening Magic now shows the SkyUltra Spellbook.'
        : 'Skyrim Magic menu replacement disabled — opening Magic now uses the normal Skyrim / SkyUI MagicMenu.',
    );
  };
  const changeMagicDisplayMode = (magicDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch({ magicDisplayMode }, `Spellbook container mode set to ${containerModeLabel(magicDisplayMode)}.`);
  };
  const changeMagicPauseWorld = (magicPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { magicPauseWorld },
      magicPauseWorld ? 'Spellbook will now pause the Skyrim world while open.' : 'Spellbook will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimFavorites = (replaceSkyrimFavorites: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimFavorites },
      replaceSkyrimFavorites
        ? 'Skyrim Favorites menu replacement enabled — the favorites key (Q) now opens SkyUltra Favorites.'
        : 'Skyrim Favorites menu replacement disabled — the favorites key now uses the normal Skyrim / SkyUI FavoritesMenu.',
    );
  };
  const changeFavoritesDisplayMode = (favoritesDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch({ favoritesDisplayMode }, `Favorites container mode set to ${containerModeLabel(favoritesDisplayMode)}.`);
  };
  const changeFavoritesPauseWorld = (favoritesPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { favoritesPauseWorld },
      favoritesPauseWorld ? 'Favorites will now pause the Skyrim world while open.' : 'Favorites will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimContainer = (replaceSkyrimContainer: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimContainer },
      replaceSkyrimContainer
        ? 'Skyrim Storage Container replacement enabled — chests, safes, urns, barrels, and containers now open the SkyUltra Storage Container.'
        : 'Skyrim Storage Container replacement disabled — containers now use the normal Skyrim / SkyUI ContainerMenu.',
    );
  };
  const changeContainerDisplayMode = (containerDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch({ containerDisplayMode }, `Storage Container mode set to ${containerModeLabel(containerDisplayMode)}.`);
  };
  const changeContainerPauseWorld = (containerPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { containerPauseWorld },
      containerPauseWorld
        ? 'Storage Containers will now pause the Skyrim world while open.'
        : 'Storage Containers will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimMannequin = (replaceSkyrimMannequin: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimMannequin },
      replaceSkyrimMannequin
        ? 'Skyrim Armor Mannequin replacement enabled — activating a mannequin now opens the SkyUltra Mannequin 3D Dress-Up Stage.'
        : 'Skyrim Armor Mannequin replacement disabled — mannequins now use the normal Skyrim / SkyUI ContainerMenu.',
    );
  };
  const changeMannequinDisplayMode = (mannequinDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch({ mannequinDisplayMode }, `Mannequin display mode set to ${containerModeLabel(mannequinDisplayMode)}.`);
  };
  const changeMannequinPauseWorld = (mannequinPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { mannequinPauseWorld },
      mannequinPauseWorld
        ? 'Mannequin menu will now pause the Skyrim world while open.'
        : 'Mannequin menu will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimLoot = (replaceSkyrimLoot: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimLoot },
      replaceSkyrimLoot
        ? 'Skyrim Corpse & Monster Looting replacement enabled — looting dead NPCs and monsters now opens the SkyUltra Loot Container (skyultra_loot.swf).'
        : 'Skyrim Corpse & Monster Looting replacement disabled — looting corpses now uses the normal Skyrim / SkyUI ContainerMenu.',
    );
  };
  const changeLootDisplayMode = (lootDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch({ lootDisplayMode }, `Corpse & Monster Looting display mode set to ${containerModeLabel(lootDisplayMode)}.`);
  };
  const changeLootPauseWorld = (lootPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { lootPauseWorld },
      lootPauseWorld
        ? 'Corpse & Monster Looting menu will now pause the Skyrim world while open.'
        : 'Corpse & Monster Looting menu will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimPickpocket = (replaceSkyrimPickpocket: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimPickpocket },
      replaceSkyrimPickpocket
        ? 'Skyrim NPC Pickpocketing replacement enabled — pickpocketing an NPC now opens the SkyUltra Pickpocket Container (skyultra_pickpocket.swf) with Steal & Plant actions.'
        : 'Skyrim NPC Pickpocketing replacement disabled — pickpocketing now uses the normal Skyrim / SkyUI ContainerMenu.',
    );
  };
  const changePickpocketDisplayMode = (pickpocketDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch(
      { pickpocketDisplayMode },
      `NPC Pickpocketing display mode set to ${containerModeLabel(pickpocketDisplayMode)}.`,
    );
  };
  const changePickpocketPauseWorld = (pickpocketPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { pickpocketPauseWorld },
      pickpocketPauseWorld
        ? 'NPC Pickpocketing menu will now pause the Skyrim world while open.'
        : 'NPC Pickpocketing menu will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimBlacksmith = (replaceSkyrimBlacksmith: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimBlacksmith },
      replaceSkyrimBlacksmith
        ? 'Skyrim Blacksmith replacement enabled — Forge, Skyforge, Tanning Rack, Smelter, Grindstone & Workbench now open skyultra_blacksmith.swf.'
        : 'Returned Blacksmith stations (Forge, Tanning Rack, Smelter, Grindstone, Workbench) to Vanilla Skyrim / SkyUI Crafting Menu.',
    );
  };
  const changeBlacksmithDisplayMode = (blacksmithDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch(
      { blacksmithDisplayMode },
      `Blacksmith layout mode set to ${containerModeLabel(blacksmithDisplayMode)}.`,
    );
  };
  const changeBlacksmithPauseWorld = (blacksmithPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { blacksmithPauseWorld },
      blacksmithPauseWorld
        ? 'Blacksmith menu will now pause the Skyrim world while open.'
        : 'Blacksmith menu will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimEnchanting = (replaceSkyrimEnchanting: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimEnchanting },
      replaceSkyrimEnchanting
        ? 'Skyrim Arcane Enchanter replacement enabled — Enchanting tables now open skyultra_enchanting.swf.'
        : 'Returned Arcane Enchanter stations to Vanilla Skyrim / SkyUI Enchanting Menu.',
    );
  };
  const changeEnchantingDisplayMode = (enchantingDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch(
      { enchantingDisplayMode },
      `Arcane Enchanter layout mode set to ${containerModeLabel(enchantingDisplayMode)}.`,
    );
  };
  const changeEnchantingPauseWorld = (enchantingPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { enchantingPauseWorld },
      enchantingPauseWorld
        ? 'Arcane Enchanter menu will now pause the Skyrim world while open.'
        : 'Arcane Enchanter menu will now keep the Skyrim world live while open.',
    );
  };
  const changeSuperEnchanting = (superEnchanting: boolean): void => {
    applyLiveSettingsPatch(
      { superEnchanting },
      superEnchanting
        ? 'Super Enchanting enabled: unique disenchants, unlimited effects, unrestricted slots, and batch enchanting unlocked.'
        : 'Super Enchanting disabled: returned to standard enchanting rules.',
    );
  };
  const changeReplaceSkyrimAlchemy = (replaceSkyrimAlchemy: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimAlchemy },
      replaceSkyrimAlchemy
        ? 'Skyrim Alchemy Lab menu will automatically open inside SkyUltra Alchemy.'
        : 'Skyrim Alchemy Lab menu restored to Vanilla / SkyUI Crafting Menu.',
    );
  };
  const changeAlchemyDisplayMode = (alchemyDisplayMode: SkyUltraBarterDisplayMode): void => {
    applyLiveSettingsPatch(
      { alchemyDisplayMode },
      alchemyDisplayMode === 'half'
        ? 'Alchemy Lab set to Half-Screen Side Panel (keeps the 3D Alchemy Lab & satchel visible).'
        : alchemyDisplayMode === 'windowed'
          ? 'Alchemy Lab set to Centered Floating Window.'
          : 'Alchemy Lab set to Full Screen.',
    );
  };
  const changeAlchemyPauseWorld = (alchemyPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { alchemyPauseWorld },
      alchemyPauseWorld
        ? 'Alchemy Lab menu will now pause the Skyrim world while open.'
        : 'Alchemy Lab menu will now keep the Skyrim world live while open.',
    );
  };
  const changeReplaceSkyrimBookReader = (replaceSkyrimBookReader: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimBookReader },
      replaceSkyrimBookReader
        ? 'Skyrim Book Reader will automatically open inside SkyUltra Book Reader.'
        : 'Skyrim Book Reader restored to Vanilla / SkyUI Book Menu.',
    );
  };
  const changeBookPauseWorld = (bookPauseWorld: boolean): void => {
    applyLiveSettingsPatch(
      { bookPauseWorld },
      bookPauseWorld
        ? 'SkyUltra Book Reader will pause the Skyrim world while reading.'
        : 'SkyUltra Book Reader will keep the Skyrim world live while reading.',
    );
  };
  const changeReplaceSkyrimCompass = (replaceSkyrimCompass: boolean): void => {
    applyLiveSettingsPatch(
      { replaceSkyrimCompass },
      replaceSkyrimCompass
        ? 'Skyrim Compass HUD replacement enabled (skyultra_compass.swf) — custom icons for Homes, Inns, Castles, Blacksmiths, Vendors & Quest Markers are now active.'
        : 'Skyrim Compass HUD replacement disabled — restored vanilla Skyrim Compass HUD.',
    );
  };
  const changeAllInGameInventoryMenus = (enableSkyUltraMenus: boolean): void => {
    applyLiveSettingsPatch(
      {
        replaceSkyrimInventory: enableSkyUltraMenus,
        replaceSkyrimBarter: enableSkyUltraMenus,
        replaceSkyrimMagic: enableSkyUltraMenus,
        replaceSkyrimFavorites: enableSkyUltraMenus,
        replaceSkyrimContainer: enableSkyUltraMenus,
        replaceSkyrimMannequin: enableSkyUltraMenus,
        replaceSkyrimLoot: enableSkyUltraMenus,
        replaceSkyrimPickpocket: enableSkyUltraMenus,
        replaceSkyrimBlacksmith: enableSkyUltraMenus,
        replaceSkyrimEnchanting: enableSkyUltraMenus,
        replaceSkyrimAlchemy: enableSkyUltraMenus,
        replaceSkyrimBookReader: enableSkyUltraMenus,
        replaceSkyrimCompass: enableSkyUltraMenus,
      },
      enableSkyUltraMenus
        ? 'All SkyUltra In-Game Menus & Compass HUD enabled (Player Inventory, Barter, Storage Containers, Corpse Looting, Pickpocketing, Mannequins, Blacksmith Stations, Arcane Enchanter, Alchemy Lab, Spellbook, Favorites, Sovereign Compass).'
        : 'Returned all In-Game Menus & Compass HUD to Skyrim Vanilla / SkyUI.',
    );
  };
  const changeModHotkey = (modId: string, binding: SkyUltraHotkeyBinding): void => {
    if (!modId) return;
    setSettings((current) => ({
      ...current,
      modHotkeys: { ...(current.modHotkeys ?? {}), [modId]: binding },
    }));
  };
  const registerCustomModId = (): void => {
    const cleaned = customModInput.trim().toLowerCase();
    if (!/^[a-z0-9][a-z0-9._-]{1,62}$/.test(cleaned) || cleaned === 'skyultra.menu') {
      setMessage('Enter a valid SkyUltra mod ID (for example: myauthor.mymod).');
      return;
    }
    setSettings((current) => ({
      ...current,
      registeredMods: [...(current.registeredMods ?? []), { id: cleaned, name: cleaned }],
      modHotkeys: {
        ...(current.modHotkeys ?? {}),
        [cleaned]: current.modHotkeys?.[cleaned] ?? { keyboard: [], gamepad: [], vr: [] },
      },
    }));
    setSelectedModId(cleaned);
    setCustomModInput('');
    setMessage(`Added mod "${cleaned}" to hotkey configuration.`);
  };

  const changeRuntime = (
    patch: Partial<NonNullable<SkyUltraSettings['runtime']>>,
    statusText: string,
  ): void => {
    applyLiveSettingsPatch(
      { runtime: { ...(settings.runtime ?? {}), ...patch } },
      statusText,
    );
  };

  const resetAllPreferences = (): void => {
    const factoryStore = resetSkyUltraProfile(profileStore, 'all', playerName);
    setProfileStore(factoryStore);
    void persistSkyUltraProfileStore(factoryStore, bridge, playerName);
    const nextUi = writeSkyUltraUiPrefs(defaultSkyUltraUiPreferences);
    setUiPrefs(nextUi);
    onThemeChange?.('dragonstone');
    setSettings({ ...defaultSkyUltraSettings, profileState: factoryStore as unknown as Record<string, unknown> });
    setMessage('Restored factory Dragonstone theme, character profiles, global defaults, hotkey, splash, and export settings.');
  };

  const save = async (): Promise<boolean> => {
    if (busy || !loaded) return false;
    setBusy(true);
    try {
      try {
        window.localStorage.setItem(
          'skyultra.switchState',
          JSON.stringify({ route: 'settings', category, ts: Date.now() }),
        );
      } catch {
        // Ignore storage errors
      }
      const updated = await updateSkyUltraSettings(
        {
          ...settings,
          schemaVersion: 2,
          profileState: profileStore as unknown as Record<string, unknown>,
        },
        bridge,
      );
      setSettings(updated);
      if (updated.hotkey) {
        window.dispatchEvent(new CustomEvent('skyultra:hotkey-updated', { detail: updated.hotkey }));
      }
      if (updated.keyboardHotkey) {
        window.dispatchEvent(
          new CustomEvent('skyultra:keyboard-hotkey-updated', { detail: updated.keyboardHotkey }),
        );
      }
      if (updated.menuEngine) {
        window.dispatchEvent(new CustomEvent('skyultra:engine-updated', { detail: updated.menuEngine }));
      }
      window.dispatchEvent(new CustomEvent('skyultra:settings-updated', { detail: updated }));
      setMessage(
        native
          ? 'All SkyUltra preferences, character profiles, global defaults, telemetry cadences & hotkeys saved to SkyUltra.settings.json.'
          : 'Browser preview preferences, character profiles, themes & hotkeys saved.',
      );
      return true;
    } catch (error) {
      setMessage(String(error));
      return false;
    } finally {
      setBusy(false);
    }
  };

  const queryTokens = settingsQuery.trim().toLowerCase();
  const activeCat: SettingsCategory = queryTokens ? 'all' : category;
  const matchesQuery = (keywords: string): boolean => !queryTokens || keywords.toLowerCase().includes(queryTokens);
  const resolvedEnchantingLevel = Math.max(playerEnchantingLevel ?? 0, settings.playerEnchantingLevel ?? 0);
  const superEnchantingUnlocked = resolvedEnchantingLevel >= 100;
  const showInventory = (activeCat === 'all' || activeCat === 'inventory') && matchesQuery('inventory barter container mannequin loot pickpocket blacksmith forge anvil tanning smelter grindstone workbench smithing crafting enchanting enchanter arcane super enchanting disenchant soul gem alchemy lab potion poison ingredient brew book reader replacement display pause stolen weight value');
  const showMagic = (activeCat === 'all' || activeCat === 'magic') && matchesQuery('magic spellbook favorites replacement display pause');
  const showProfiles = (activeCat === 'all' || activeCat === 'profiles') && matchesQuery('profile sync character global defaults json import export');
  const showThemes = (activeCat === 'all' || activeCat === 'themes') && matchesQuery('theme visual darkmode dragonstone neon glass density motion splash book reader design parchment scale sound lighting preview menu display pause');
  const showHotkeys = (activeCat === 'all' || activeCat === 'hotkeys') && matchesQuery('hotkey chord keyboard gamepad vr controller virtual');
  const showEngine = (activeCat === 'all' || activeCat === 'engine') && matchesQuery('engine cef ultralight runtime multithreaded cpu gpu hot reload');
  const showCursor = (activeCat === 'all' || activeCat === 'cursor') && matchesQuery('cursor telemetry polling export portrait resolution color wheel');
  const showSplash = (activeCat === 'all' || activeCat === 'splash') && matchesQuery('splash video soundtrack audio replay playback');
  const savedProfileEntries = useMemo(() => Object.values(profileStore.profiles), [profileStore.profiles]);
  const bothMenusEnabled =
    settings.replaceSkyrimInventory !== false &&
    settings.replaceSkyrimBarter !== false &&
    settings.replaceSkyrimContainer !== false &&
    settings.replaceSkyrimMannequin !== false &&
    settings.replaceSkyrimLoot !== false &&
    settings.replaceSkyrimPickpocket !== false &&
    settings.replaceSkyrimBlacksmith !== false &&
    settings.replaceSkyrimEnchanting !== false &&
    settings.replaceSkyrimAlchemy !== false &&
    settings.replaceSkyrimBookReader !== false;
  const bothMenusDisabled =
    settings.replaceSkyrimInventory === false &&
    settings.replaceSkyrimBarter === false &&
    settings.replaceSkyrimContainer === false &&
    settings.replaceSkyrimMannequin === false &&
    settings.replaceSkyrimLoot === false &&
    settings.replaceSkyrimPickpocket === false &&
    settings.replaceSkyrimBlacksmith === false &&
    settings.replaceSkyrimEnchanting === false &&
    settings.replaceSkyrimAlchemy === false &&
    settings.replaceSkyrimBookReader === false;

  return (
    <div className="page settings-page">
      <div className="settings-studio-header">
        <div className="settings-studio-header__title">
          <span className="eyebrow">COMMAND DECK CONFIGURATION · LIVE WIRED CONTROLS</span>
          <h1>
            System Settings<span className="title-period">.</span>
          </h1>
          <p>
            Configure menu replacements, 21 interface themes (5 Eye-Comfort), per-character profiles, input chords, GPU runtime, and Book Reader studio layouts.
          </p>
        </div>
        <div className="settings-lead-actions">
          <button type="button" className="control-button" disabled={!loaded || busy} onClick={resetAllPreferences}>
            Reset Defaults
          </button>
          <button type="button" className="primary-button" disabled={!loaded || busy} onClick={() => void save()}>
            <Icon name="check" size={15} />
            {busy ? 'Saving…' : 'Save Preferences'}
          </button>
        </div>
      </div>

      <div className="settings-overview-strip">
        <div className="settings-kpi-pill">
          <small>PROFILE SCOPE</small>
          <strong>{effectiveProfile.isCharacterScoped ? effectiveProfile.characterName : 'Global Defaults'}</strong>
          <span>
            {profileStore.mode === 'per-character' ? 'Per-Character Auto' : 'Shared Global'} ·{' '}
            {palettes.find((p) => p.value === profileStore.globalDefaults.theme)?.label ?? 'Dragonstone'}
          </span>
        </div>
        <div className="settings-kpi-pill">
          <small>THEME ARCHITECTURE</small>
          <strong>{palettes.find((p) => p.value === theme)?.label ?? 'Dragonstone'}</strong>
          <span>{palettes.find((p) => p.value === theme)?.styleTag ?? 'Imperial Inlay'}</span>
        </div>
        <div className="settings-kpi-pill">
          <small>INPUT CHORDS</small>
          <strong>{formatSkyUltraHotkey(settings.hotkey ?? defaultSkyUltraHotkey, 'keyboard')}</strong>
          <span>
            KBDeck: {formatSkyUltraHotkey(settings.keyboardHotkey ?? defaultSkyUltraKeyboardHotkey, 'keyboard')} · VR:{' '}
            {settings.vrHeadTracking !== false ? 'HMD' : 'Fixed'}
          </span>
        </div>
        <div className="settings-kpi-pill">
          <small>ENGINE &amp; CADENCE</small>
          <strong>
            {(settings.menuEngine ?? 'cef').toUpperCase()} · {(uiPrefs.pollIntervalMs / 1000).toFixed(2)}s
          </strong>
          <span>
            Inventory: {settings.inventoryDisplayMode ?? 'windowed'} · Replacements:{' '}
            {settings.replaceSkyrimInventory !== false ? 'SkyUltra' : 'Vanilla'}
          </span>
        </div>
      </div>

      <div className="settings-studio-shell">
        <aside className="settings-studio-sidebar">
          <div className="settings-sidebar-search">
            <label className="page-search">
              <Icon name="search" size={15} />
              <input
                aria-label="Search settings"
                value={settingsQuery}
                onChange={(e) => setSettingsQuery(e.currentTarget.value)}
                placeholder="Search any setting…"
              />
              {settingsQuery && (
                <button type="button" aria-label="Clear settings search" onClick={() => setSettingsQuery('')}>
                  <Icon name="close" size={12} />
                </button>
              )}
            </label>
          </div>

          <nav className="settings-category-rail" role="tablist" aria-label="Settings categories">
            {(
              [
                { id: 'inventory', label: 'Inventory & Barter', sub: 'Replacements, modes & world pause', icon: 'inventory' as const },
                { id: 'magic', label: 'Magic & Favorites', sub: 'Spellbook & favorites overlays', icon: 'rune' as const },
                { id: 'themes', label: 'Themes & Visuals', sub: '21 palettes, window & glass', icon: 'components' as const },
                { id: 'profiles', label: 'Profiles & Sync', sub: 'Per-character & global JSON', icon: 'actors' as const },
                { id: 'hotkeys', label: 'Hotkeys & VR', sub: 'Keyboard, gamepad & HMD chords', icon: 'target' as const },
                { id: 'engine', label: 'Engine & Runtime', sub: 'CEF, Ultralight GPU/CPU & watcher', icon: 'performance' as const },
                { id: 'cursor', label: 'Cursor & Telemetry', sub: 'Pointer style, polling & 4K export', icon: 'camera' as const },
                { id: 'splash', label: 'Opening Splash', sub: 'Startup video & soundtrack', icon: 'play' as const },
                { id: 'all', label: 'All Settings', sub: 'Show every category at once', icon: 'grid' as const },
              ] as const
            ).map((tab) => (
              <button
                key={tab.id}
                type="button"
                role="tab"
                aria-selected={activeCat === tab.id}
                className={'settings-cat-btn' + (activeCat === tab.id ? ' is-active' : '')}
                onClick={() => {
                  setCategory(tab.id);
                  if (settingsQuery) setSettingsQuery('');
                }}
              >
                <span className="settings-cat-btn__icon">
                  <Icon name={tab.icon} size={15} />
                </span>
                <span className="settings-cat-btn__text">
                  <strong>{tab.label}</strong>
                  <small>{tab.sub}</small>
                </span>
              </button>
            ))}
          </nav>
        </aside>

        <div className="settings-studio-content">
          <fieldset disabled={!loaded || busy} style={{ border: 0, padding: 0, margin: 0 }}>
        {showInventory && (
          <SkySettingsSection
            title="In-Game Inventory & Vendor Barter Menus"
            description="Turn on or off SkyUltra's in-game InventoryMenu and BarterMenu replacements with one click. Disable anytime to immediately return to vanilla Skyrim or SkyUI inventory and merchant menus without restarting the game."
          >
            <div className="settings-choice">
              <span>One-Click In-Game Inventory Menus Mode</span>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '10px', alignItems: 'center' }}>
                <button
                  type="button"
                  className={'control-button' + (bothMenusEnabled ? ' is-active' : '')}
                  onClick={() => changeAllInGameInventoryMenus(true)}
                >
                  <Icon name="check" size={14} />
                  <span>Use SkyUltra Inventory &amp; Barter Menus (ON)</span>
                </button>
                <button
                  type="button"
                  className={'control-button' + (bothMenusDisabled ? ' is-active' : '')}
                  onClick={() => changeAllInGameInventoryMenus(false)}
                >
                  <Icon name="inventory" size={14} />
                  <span>Return to Skyrim / SkyUI Inventory Menus (OFF)</span>
                </button>
              </div>
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Compass HUD with SkyUltra Sovereign Compass (skyultra_compass.swf)"
              description="When enabled, hides Skyrim's vanilla top-screen Scaleform compass and activates SkyUltra's Sovereign Compass HUD (skyultra_compass.swf) with custom icons for Homes, Inns, Castles, Blacksmiths, Vendors, and Quest Markers. Switch off at any time to restore the vanilla Skyrim compass."
              checked={settings.replaceSkyrimCompass !== false}
              onCheckedChange={changeReplaceSkyrimCompass}
            />

            <SkyToggleSwitch
              label="Replace Skyrim Player Inventory with SkyUltra Player Inventory"
              description="When enabled, opening your normal player inventory in-game (I / Tab → Items) replaces Skyrim's InventoryMenu with SkyUltra's standalone Player Inventory & 3D Character Equip Stage. Turn off to return to Skyrim's inventory."
              checked={settings.replaceSkyrimInventory !== false}
              onCheckedChange={changeReplaceSkyrimInventory}
            />

            <div className="settings-choice">
              <span>Player Inventory Display Mode</span>
              <SkyFilterChips
                label="Player inventory display mode"
                value={settings.inventoryDisplayMode ?? 'windowed'}
                onValueChange={(val) => changeInventoryDisplayMode(val as SkyUltraDisplayMode)}
                options={[
                  { value: 'windowed', label: 'Windowed Mode (Default · Floating Overlay)' },
                  { value: 'fullscreen', label: 'Full Screen Mode' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Pause Skyrim World in Player Inventory"
              description="Freeze Skyrim's world simulation while the standalone Player Inventory is open (like vanilla Skyrim), or leave it unchecked to browse and equip items while the world stays live."
              checked={settings.inventoryPauseWorld !== false}
              onCheckedChange={changeInventoryPauseWorld}
            />

            <SkyToggleSwitch
              label="Replace Skyrim Vendor Barter Menu with SkyUltra Barter Container"
              description="When enabled, buying and selling with Skyrim merchants replaces Skyrim's BarterMenu (via skyultra_barter.swf) with SkyUltra's half-screen Barter Container—without the 3D equipment panel, with bottom Vendor/Player switcher tabs and a quantity slider for stacks over 5. Turn off to return to Skyrim's barter menu."
              checked={settings.replaceSkyrimBarter !== false}
              onCheckedChange={changeReplaceSkyrimBarter}
            />

            <div className="settings-choice">
              <span>Vendor Barter Container Mode</span>
              <SkyFilterChips
                label="Vendor barter container mode"
                value={settings.barterDisplayMode ?? 'half'}
                onValueChange={(val) => changeBarterDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Container (Left 50% · Skyrim World Right)' },
                  { value: 'full', label: 'Full Container (Full Screen)' },
                  { value: 'windowed', label: 'Windowed Container (Floating Overlay)' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Vendor Barter World Simulation State</span>
              <SkyFilterChips
                label="Vendor barter world state"
                value={settings.barterPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeBarterPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Storage Containers with SkyUltra Storage Container"
              description="When enabled, opening chests, safes, burial urns, barrels, sacks, and storage containers (via skyultra_container.swf) opens SkyUltra's Storage Container with Take All, Store All, Gold display, single-click transfer, and a quantity slider for stacks above 5."
              checked={settings.replaceSkyrimContainer !== false}
              onCheckedChange={changeReplaceSkyrimContainer}
            />

            <div className="settings-choice">
              <span>Storage Container Layout Mode</span>
              <SkyFilterChips
                label="Storage container layout mode"
                value={settings.containerDisplayMode ?? 'half'}
                onValueChange={(val) => changeContainerDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Container (Default)' },
                  { value: 'windowed', label: 'Windowed Mode' },
                  { value: 'full', label: 'Full Screen Mode' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Storage Container World Simulation State</span>
              <SkyFilterChips
                label="Storage container world state"
                value={settings.containerPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeContainerPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Corpse & Monster Looting with SkyUltra Loot Container"
              description="When enabled, looting dead NPCs and monsters (via skyultra_loot.swf) opens SkyUltra's Corpse & Monster Loot Container in Half Container mode by default with single-click Take & Store, Take All & Store All bulk actions, and Gold displayed in both inventories."
              checked={settings.replaceSkyrimLoot !== false}
              onCheckedChange={changeReplaceSkyrimLoot}
            />

            <div className="settings-choice">
              <span>Corpse &amp; Monster Looting Layout Mode</span>
              <SkyFilterChips
                label="Corpse and monster looting layout mode"
                value={settings.lootDisplayMode ?? 'half'}
                onValueChange={(val) => changeLootDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Container (Default)' },
                  { value: 'windowed', label: 'Windowed Mode' },
                  { value: 'full', label: 'Full Screen Mode' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Corpse &amp; Monster Looting World Simulation State</span>
              <SkyFilterChips
                label="Corpse and monster looting world state"
                value={settings.lootPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeLootPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim NPC Pickpocketing with SkyUltra Pickpocket Container"
              description="When enabled, pickpocketing an NPC (via skyultra_pickpocket.swf) opens SkyUltra's Pickpocket Container in Half Container mode by default with Steal (from targeted NPC to you), Plant (from your inventory to the targeted NPC), EQUIPPED badges on NPC worn items, and Gold displayed in both inventories."
              checked={settings.replaceSkyrimPickpocket !== false}
              onCheckedChange={changeReplaceSkyrimPickpocket}
            />

            <div className="settings-choice">
              <span>NPC Pickpocketing Layout Mode</span>
              <SkyFilterChips
                label="NPC pickpocketing layout mode"
                value={settings.pickpocketDisplayMode ?? 'half'}
                onValueChange={(val) => changePickpocketDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Container (Default)' },
                  { value: 'windowed', label: 'Windowed Mode' },
                  { value: 'full', label: 'Full Screen Mode' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>NPC Pickpocketing World Simulation State</span>
              <SkyFilterChips
                label="NPC pickpocketing world state"
                value={settings.pickpocketPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changePickpocketPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Armor Mannequins with SkyUltra Mannequin 3D Stage"
              description="When enabled, activating an Armor Mannequin (via skyultra_mannequin.swf) opens a Player Inventory-style workbench with the motionless 3D Mannequin on the right so you can drag items onto the mannequin or press Equip and Unequip."
              checked={settings.replaceSkyrimMannequin !== false}
              onCheckedChange={changeReplaceSkyrimMannequin}
            />

            <div className="settings-choice">
              <span>Mannequin Display Mode</span>
              <SkyFilterChips
                label="Mannequin display mode"
                value={settings.mannequinDisplayMode ?? 'full'}
                onValueChange={(val) => changeMannequinDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'full', label: 'Full Screen Mode' },
                  { value: 'windowed', label: 'Windowed Mode (Floating Overlay)' },
                  { value: 'half', label: 'Half Container' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Mannequin World Simulation State</span>
              <SkyFilterChips
                label="Mannequin world state"
                value={settings.mannequinPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeMannequinPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Blacksmith Stations with SkyUltra Blacksmith"
              description="When enabled, Blacksmith Forges, the Skyforge, Tanning Racks, Smelters, Grindstones, and Armor Workbenches open SkyUltra's theme-synchronized Blacksmith interface (skyultra_blacksmith.swf). Disable at any time to return to Vanilla Skyrim / SkyUI Crafting Menu."
              checked={settings.replaceSkyrimBlacksmith !== false}
              onCheckedChange={changeReplaceSkyrimBlacksmith}
            />

            <div className="settings-choice">
              <span>Blacksmith Layout Mode</span>
              <SkyFilterChips
                label="Blacksmith layout mode"
                value={settings.blacksmithDisplayMode ?? 'half'}
                onValueChange={(val) => changeBlacksmithDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Panel (50% · Live World View)' },
                  { value: 'windowed', label: 'Windowed Overlay' },
                  { value: 'full', label: 'Full Screen' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Blacksmith World Simulation State</span>
              <SkyFilterChips
                label="Blacksmith world state"
                value={settings.blacksmithPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeBlacksmithPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Arcane Enchanter with SkyUltra Enchanter"
              description="When enabled, Arcane Enchanter tables open SkyUltra's theme-synchronized Enchanting interface (skyultra_enchanting.swf) for binding and disenchanting. Disable at any time to return to Vanilla Skyrim / SkyUI Enchanting Menu."
              checked={settings.replaceSkyrimEnchanting !== false}
              onCheckedChange={changeReplaceSkyrimEnchanting}
            />

            <div className="settings-choice">
              <span>Arcane Enchanter Layout Mode</span>
              <SkyFilterChips
                label="Arcane Enchanter layout mode"
                value={settings.enchantingDisplayMode ?? 'half'}
                onValueChange={(val) => changeEnchantingDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Panel (50% · Live World View)' },
                  { value: 'windowed', label: 'Windowed Overlay' },
                  { value: 'full', label: 'Full Screen' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Arcane Enchanter World Simulation State</span>
              <SkyFilterChips
                label="Arcane Enchanter world state"
                value={settings.enchantingPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeEnchantingPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            {(superEnchantingUnlocked || Boolean(settings.superEnchanting)) && (
              <SkyToggleSwitch
                label="Super Enchanting"
                description={`Unlocked at Enchanting 100 (current: ${resolvedEnchantingLevel}). Disenchant unique non-quest items, bind unlimited enchantments, remove armor slot restrictions, and batch-enchant multiple armors or weapons at once (requires 1 filled soul gem per piece).`}
                checked={Boolean(settings.superEnchanting)}
                onCheckedChange={changeSuperEnchanting}
              />
            )}

            <SkyToggleSwitch
              label="Replace Skyrim Alchemy Lab with SkyUltra"
              description="Intercept Alchemy Lab stations (skyultra_alchemy.swf) with trait tasting, 4-column ingredient Grid/List views, Known Recipes discovery, Purity perk cleansing, and Distillation Altar batch brewing. Disable anytime to return to Vanilla or SkyUI."
              checked={settings.replaceSkyrimAlchemy !== false}
              onCheckedChange={changeReplaceSkyrimAlchemy}
            />

            <div className="settings-choice">
              <span>Alchemy Lab Menu Layout</span>
              <SkyFilterChips
                label="Alchemy Lab layout"
                value={settings.alchemyDisplayMode === 'windowed' ? 'windowed' : settings.alchemyDisplayMode === 'full' ? 'full' : 'half'}
                onValueChange={(val) => changeAlchemyDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Screen (Default · 3D Lab Visible)' },
                  { value: 'full', label: 'Full Screen' },
                  { value: 'windowed', label: 'Floating Window' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Alchemy Lab World Simulation State</span>
              <SkyFilterChips
                label="Alchemy Lab world state"
                value={settings.alchemyPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeAlchemyPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Book Reader with SkyUltra"
              description="Intercept world books and notes outside your pack in SkyUltra Book Reader with Take / Steal actions. Disable anytime to use Vanilla or SkyUI Book Menu."
              checked={settings.replaceSkyrimBookReader !== false}
              onCheckedChange={changeReplaceSkyrimBookReader}
            />

            <div className="settings-choice">
              <span>Book Reader World Simulation State</span>
              <SkyFilterChips
                label="Book Reader world state"
                value={settings.bookPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeBookPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Show Gold-per-Weight (V/W) efficiency badges"
              description="Highlight high-value loot ratios across Inventory, Loot, Barter, and Storage Container menus."
              checked={uiPrefs.showValuePerWeight}
              onCheckedChange={(showValuePerWeight) => updateUiPref({ showValuePerWeight })}
            />

            <SkyToggleSwitch
              label="Stolen &amp; Pickpocket Protection Guard"
              description="Require confirmation before dropping or selling stolen items and skip owned/stolen items during Smart Loot."
              checked={uiPrefs.stolenProtection}
              onCheckedChange={(stolenProtection) => updateUiPref({ stolenProtection })}
            />
          </SkySettingsSection>
        )}
        {showMagic && (
          <SkySettingsSection
            title="Spellbook (Magic Menu) & Favorites Menu"
            description="Replace Skyrim's MagicMenu with SkyUltra's animated Spellbook (school categories, equip left / right / both, voice powers & shouts, heart favorites) and Skyrim's FavoritesMenu with SkyUltra's quick-access Favorites list of items and spells. Each can be switched off at any time to return to the vanilla / SkyUI menus."
          >
            <SkyToggleSwitch
              label="Replace Skyrim Magic Menu with SkyUltra Spellbook"
              description="When enabled, opening Magic (P / Tab → Magic) shows SkyUltra's Spellbook (via skyultra_magic.swf) instead of Skyrim's MagicMenu."
              checked={settings.replaceSkyrimMagic !== false}
              onCheckedChange={changeReplaceSkyrimMagic}
            />
            <div className="settings-choice">
              <span>Spellbook Container Mode</span>
              <SkyFilterChips
                label="Spellbook container mode"
                value={settings.magicDisplayMode ?? 'half'}
                onValueChange={(val) => changeMagicDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Container (Left · Skyrim World Right)' },
                  { value: 'full', label: 'Full Container (Full Screen)' },
                  { value: 'windowed', label: 'Windowed Container (Floating Overlay)' },
                ]}
              />
            </div>
            <div className="settings-choice">
              <span>Spellbook World Simulation State</span>
              <SkyFilterChips
                label="Spellbook world state"
                value={settings.magicPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeMagicPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Replace Skyrim Favorites Menu with SkyUltra Favorites"
              description="When enabled, the favorites key (Q) shows SkyUltra's Favorites (via skyultra_favorites.swf) with your favorite items and spells, ready to equip left / right or use."
              checked={settings.replaceSkyrimFavorites !== false}
              onCheckedChange={changeReplaceSkyrimFavorites}
            />
            <div className="settings-choice">
              <span>Favorites Container Mode</span>
              <SkyFilterChips
                label="Favorites container mode"
                value={settings.favoritesDisplayMode ?? 'half'}
                onValueChange={(val) => changeFavoritesDisplayMode(val as SkyUltraBarterDisplayMode)}
                options={[
                  { value: 'half', label: 'Half Container (Left · Skyrim World Right)' },
                  { value: 'full', label: 'Full Container (Full Screen)' },
                  { value: 'windowed', label: 'Windowed Container (Floating Overlay)' },
                ]}
              />
            </div>
            <div className="settings-choice">
              <span>Favorites World Simulation State</span>
              <SkyFilterChips
                label="Favorites world state"
                value={settings.favoritesPauseWorld !== false ? 'pause' : 'live'}
                onValueChange={(val) => changeFavoritesPauseWorld(val === 'pause')}
                options={[
                  { value: 'pause', label: 'Pause World (Default · Freeze Simulation)' },
                  { value: 'live', label: 'Live World (Unpaused)' },
                ]}
              />
            </div>
          </SkySettingsSection>
        )}
        {showProfiles && (
          <SkySettingsSection
            title="Persistent Character Profiles & Global Defaults"
            description="Save themes, UI density/glow preferences, HUD widget layouts, and outfit loadouts per Skyrim character in SkyUltra.settings.json (schemaVersion 2), with automatic v1 migrations, explicit global defaults for new characters, and JSON import/export."
          >
            <div className="settings-choice">
              <span>Profile resolution mode</span>
              <SkyFilterChips
                label="Profile resolution mode"
                value={profileStore.mode}
                onValueChange={handleProfileModeChange}
                options={[
                  { value: 'per-character', label: 'Per-Character Automatic (Recommended)' },
                  { value: 'global', label: 'Global Shared Profile' },
                ]}
              />
            </div>

            <div className="settings-mod-blueprint" aria-label="Active character profile status">
              <div className="settings-mod-blueprint__head">
                <span>
                  ACTIVE PROFILE: {effectiveProfile.characterName.toUpperCase()} ({effectiveProfile.profileId})
                </span>
                <code>SkyUltra.settings.json · schemaVersion 2</code>
              </div>
              <div className="settings-mod-blueprint__steps">
                <div className="settings-mod-blueprint__step">
                  <b>CHARACTER OVERRIDES</b>
                  <p>
                    {effectiveProfile.isCharacterScoped
                      ? effectiveProfile.hasCharacterOverrides
                        ? `Custom overrides active for ${effectiveProfile.characterName} (Theme: ${effectiveProfile.theme}).`
                        : `Inheriting Global Defaults until you customize ${effectiveProfile.characterName}.`
                      : 'Global mode active — edits apply across every character save.'}
                  </p>
                </div>
                <div className="settings-mod-blueprint__step">
                  <b>EXPLICIT GLOBAL DEFAULTS</b>
                  <p>
                    New characters start with <code>{profileStore.globalDefaults.theme}</code>,{' '}
                    <code>{profileStore.globalDefaults.uiPrefs.density}</code> density, and{' '}
                    {profileStore.globalDefaults.hudLayout
                      ? `${profileStore.globalDefaults.hudLayout.widgets.length} default HUD widget(s).`
                      : 'standard Dragonborn HUD preset.'}
                  </p>
                </div>
                <div className="settings-mod-blueprint__step">
                  <b>SAVED CHARACTER RECORDS ({savedProfileEntries.length})</b>
                  <p>
                    {savedProfileEntries.length > 0
                      ? savedProfileEntries
                          .map((p) => `${p.characterName} (${p.theme ?? profileStore.globalDefaults.theme})`)
                          .join(' · ')
                      : 'No character-specific overrides recorded yet.'}
                  </p>
                </div>
              </div>
            </div>

            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '10px', alignItems: 'center' }}>
              <button type="button" className="control-button" onClick={handlePromoteGlobalDefaults}>
                Save Current as Global Defaults
              </button>
              <button
                type="button"
                className="control-button"
                disabled={!effectiveProfile.hasCharacterOverrides}
                onClick={handleResetCharacterProfile}
              >
                Reset Character Overrides
              </button>
              <button type="button" className="control-button" onClick={handleExportProfilesJson}>
                Export Profiles JSON
              </button>
              <button
                type="button"
                className="control-button"
                onClick={() => setProfileJsonOpen((open) => !open)}
              >
                {profileJsonOpen ? 'Hide JSON Sync' : 'Import / Paste Bundle JSON'}
              </button>
            </div>

            {profileJsonOpen && (
              <div style={{ display: 'grid', gap: '10px', marginTop: '8px' }}>
                <textarea
                  rows={8}
                  value={profileJsonDraft}
                  onChange={(event) => setProfileJsonDraft(event.currentTarget.value)}
                  placeholder='Paste a SkyUltra Profile Bundle JSON (v1 or schemaVersion 2) here to migrate & apply…'
                  style={{
                    width: '100%',
                    fontFamily: 'var(--sky-font-mono, monospace)',
                    fontSize: '12px',
                    padding: '10px 12px',
                    borderRadius: '6px',
                    border: '1px solid var(--sky-line-strong, rgba(232,176,89,0.32))',
                    background: 'rgba(8, 9, 12, 0.78)',
                    color: 'var(--sky-text, #f5efe6)',
                  }}
                />
                <div style={{ display: 'flex', gap: '10px', justifyContent: 'flex-end' }}>
                  <button
                    type="button"
                    className="primary-button"
                    disabled={!profileJsonDraft.trim()}
                    onClick={handleImportProfilesJson}
                  >
                    Validate, Migrate &amp; Apply JSON
                  </button>
                </div>
              </div>
            )}
          </SkySettingsSection>
        )}

        {showThemes && (
          <SkySettingsSection
            title="Architectural Themes & Eye-Comfort Dark Mode"
            description="Themes transform fonts, panel corner geometry (velvet-slate squircle Dark Mode UI, chamfered cyber clip-paths, double-inlaid Imperial borders, or celestial glass), atmospheric grids, scanlines, and glow. Click any theme to apply it immediately and set it as your startup default."
          >
            <div className="settings-theme-showcase">
              {(
                [
                  { id: 'darkmode', title: 'EYE-COMFORT COLLECTION · 5 LOW-GLARE MATTE VELVET-SLATE THEMES' },
                  { id: 'classic', title: 'CLASSIC TAMRIELIC INLAY · ENGRAVED SERIF + METALLIC BEVELS (DRAGONSTONE DEFAULT)' },
                  { id: 'neon', title: 'NEON & CYBER MATRIX · CHAMFERED HUD + CYBER GRID + MONO DISPLAY' },
                  { id: 'celestial', title: 'CELESTIAL & ARCANE GLASS · STARLIGHT BLOOM + RUNIC TYPOGRAPHY' },
                ] as const
              ).map((group) => (
                <div key={group.id} className="settings-theme-group">
                  <span className="settings-theme-group__title">{group.title}</span>
                  <div className="settings-theme-grid">
                    {palettes
                      .filter((p) => p.family === group.id)
                      .map((p) => {
                        const isCurrent = theme === p.value;
                        const isDefault = uiPrefs.defaultTheme === p.value;
                        return (
                          <button
                            key={p.value}
                            type="button"
                            className={'settings-theme-card' + (isCurrent ? ' is-active' : '')}
                            onClick={() => {
                              onThemeChange?.(p.value);
                              updateUiPref({ defaultTheme: p.value });
                            }}
                          >
                            <div className="settings-theme-card__swatches">
                              <span style={{ background: p.color }} />
                              <span style={{ background: p.secondary }} />
                            </div>
                            <div className="settings-theme-card__body">
                              <strong>{p.label}</strong>
                              <small>{p.styleTag}</small>
                            </div>
                            {isDefault && <b className="settings-theme-card__badge">DEFAULT</b>}
                          </button>
                        );
                      })}
                  </div>
                </div>
              ))}
            </div>

            <div className="settings-choice">
              <span>Interface layout density</span>
              <SkyFilterChips
                label="Interface layout density"
                value={uiPrefs.density}
                onValueChange={(density) => updateUiPref({ density })}
                options={[
                  { value: 'compact', label: 'Compact (High density)' },
                  { value: 'comfortable', label: 'Comfortable (Balanced)' },
                  { value: 'spacious', label: 'Spacious (Cinema scale)' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Motion &amp; animation profile</span>
              <SkyFilterChips
                label="Motion profile"
                value={uiPrefs.motion}
                onValueChange={(motion) => updateUiPref({ motion })}
                options={[
                  { value: 'cinematic', label: 'Cinematic transitions' },
                  { value: 'snappy', label: 'Snappy tactical' },
                  { value: 'reduced', label: 'Reduced motion' },
                ]}
              />
            </div>

            <SkyRangeSlider
              label="Neon bloom & atmospheric glow intensity"
              value={uiPrefs.neonGlow}
              min={0}
              max={150}
              step={10}
              unit="%"
              onValueChange={(neonGlow) => updateUiPref({ neonGlow })}
            />

            <SkyToggleSwitch
              label="Holographic backdrop grid"
              description="Render architectural coordinate grid lines across workspace backdrops."
              checked={uiPrefs.backdropGrid}
              onCheckedChange={(backdropGrid) => updateUiPref({ backdropGrid })}
            />

            <SkyToggleSwitch
              label="Cyber scanline overlay"
              description="Enable subtle horizontal CRT / tactical scanlines on Neon and Cyber themes."
              checked={uiPrefs.scanlines}
              onCheckedChange={(scanlines) => updateUiPref({ scanlines })}
            />

            <div className="settings-choice">
              <span>Tab transition splash mode</span>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, alignItems: 'center' }}>
                <SkyFilterChips
                  label="Tab transition splash"
                  value={uiPrefs.tabTransition}
                  onValueChange={(tabTransition) => updateUiPref({ tabTransition })}
                  options={[
                    { value: 'cinematic', label: 'Cinematic Logo Splash' },
                    { value: 'minimal', label: 'Minimal Fade' },
                    { value: 'instant', label: 'Instant Cut' },
                  ]}
                />
                <button
                  type="button"
                  className="control-button"
                  onClick={() => window.dispatchEvent(new Event('skyultra:preview-tab-splash'))}
                >
                  <Icon name="play" size={12} />
                  <span>Preview Tab Splash</span>
                </button>
              </div>
            </div>

            <div className="settings-choice">
              <span>Book Reader default design (7 architectures)</span>
              <SkyFilterChips
                label="Book Reader default design"
                value={uiPrefs.bookReaderDesign}
                onValueChange={(bookReaderDesign) => updateUiPref({ bookReaderDesign })}
                options={[
                  { value: 'auto', label: 'Auto Match' },
                  { value: 'codex', label: 'Codex' },
                  { value: 'scroll', label: 'Scroll' },
                  { value: 'grimoire', label: 'Grimoire' },
                  { value: 'dwemer', label: 'Dwemer' },
                  { value: 'scholar', label: 'Scholar' },
                  { value: 'datapad', label: 'Datapad' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Synchronize Book Reader colors to active theme"
              description="Match Book Reader covers, folios, and filigree to your active SkyUltra theme (disable for classic aged yellow parchment)."
              checked={uiPrefs.bookThemeSync}
              onCheckedChange={(bookThemeSync) => updateUiPref({ bookThemeSync })}
            />

            <div className="settings-choice">
              <span>Glass backdrop frost &amp; blur intensity</span>
              <SkyFilterChips
                label="Glass intensity"
                value={uiPrefs.glassIntensity}
                onValueChange={(glassIntensity) => updateUiPref({ glassIntensity })}
                options={[
                  { value: 'subtle', label: 'Subtle (Fastest)' },
                  { value: 'balanced', label: 'Balanced' },
                  { value: 'ultra', label: 'Ultra Frosted' },
                ]}
              />
            </div>

            <SkyRangeSlider
              label="Interface zoom scale"
              value={Math.round((uiPrefs.uiScale ?? 1) * 100)}
              min={85}
              max={125}
              step={5}
              unit="%"
              onValueChange={(pct) => updateUiPref({ uiScale: pct / 100 })}
            />

            <div className="settings-choice">
              <span>Default 3D Studio Lighting Rig</span>
              <SkyFilterChips
                label="Studio lighting"
                value={uiPrefs.studioLighting}
                onValueChange={(studioLighting) => updateUiPref({ studioLighting })}
                options={[
                  { value: 'studio', label: 'Studio' },
                  { value: 'warm', label: 'Warm' },
                  { value: 'cool', label: 'Cool' },
                  { value: 'soft', label: 'Soft' },
                  { value: 'dramatic', label: 'Dramatic' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>SkyUltra Menu Display Mode</span>
              <SkyFilterChips
                label="SkyUltra menu display mode"
                value={settings.menuDisplayMode ?? 'fullscreen'}
                onValueChange={(val) => changeMenuDisplayMode(val as SkyUltraDisplayMode)}
                options={[
                  { value: 'fullscreen', label: 'Full Screen Mode' },
                  { value: 'windowed', label: 'Windowed Mode (Floating Overlay)' },
                ]}
              />
            </div>

            <SkyToggleSwitch
              label="Pause Skyrim World in SkyUltra Menu"
              description="Freeze Skyrim's world simulation and combat while the main SkyUltra Command Menu is open, or leave it unchecked to keep the world live and moving behind the interface."
              checked={settings.menuPauseWorld === true}
              onCheckedChange={changeMenuPauseWorld}
            />

            <SkyToggleSwitch
              label="Declarative UI feedback sounds"
              description="Play subtle Skyrim UI audio cues when equipping gear, navigating modules, or changing settings."
              checked={uiPrefs.soundEffects}
              onCheckedChange={(soundEffects) => updateUiPref({ soundEffects })}
            />
          </SkySettingsSection>
        )}

        {showEngine && (
          <>
            <SkySettingsSection
              title="Simultaneous Multi-Engine Browser Runtime (CEF + Ultralight)"
              description="Configure SkyUltra's browser compositing engines only (Chromium CEF GPU shared textures, Ultralight GPU/CPU pipeline, per-mod engine overrides, multithreaded UI message loop, and live hot-reload watcher). Inventory, Barter, Windowed Display, and VR Head Tracking live in their dedicated categories."
            >
              <div className="settings-choice">
                <span>SkyUltra Menu Browser Engine (Independent of other mods)</span>
                <SkyFilterChips
                  label="SkyUltra menu browser engine"
                  value={settings.menuEngine ?? 'cef'}
                  onValueChange={(val) => changeMenuEngine(val as SkyUltraEngineChoice)}
                  options={[
                    { value: 'cef', label: 'Chromium CEF (Default · Double-Buffered GPU Shared Texture)' },
                    { value: 'ultralight', label: 'Ultralight GPU (Ultra-Low Latency · 120Hz)' },
                  ]}
                />
              </div>

              {activeModId && (
                <div className="settings-choice">
                  <span>Per-Mod Browser Engine Override ({activeModId})</span>
                  <SkyFilterChips
                    label={`Browser engine for ${activeModId}`}
                    value={settings.modEngines?.[activeModId] ?? 'default'}
                    onValueChange={(val) => changeModEngine(activeModId, val)}
                    options={[
                      { value: 'default', label: 'Mod Manifest Default (CEF)' },
                      { value: 'cef', label: 'Force CEF' },
                      { value: 'ultralight', label: 'Force Ultralight' },
                    ]}
                  />
                </div>
              )}

              <div className="settings-choice">
                <span>Surface Render Pipeline Mode</span>
                <SkyFilterChips
                  label="Surface render pipeline mode"
                  value={settings.runtime?.renderMode ?? 'gpu'}
                  onValueChange={(val) =>
                    changeRuntime(
                      { renderMode: val as 'gpu' | 'cpu' },
                      `Compositor render pipeline set to ${val.toUpperCase()} mode.`,
                    )
                  }
                  options={[
                    { value: 'gpu', label: 'Hardware GPU Shared Texture (Recommended)' },
                    { value: 'cpu', label: 'Software CPU Staging Fallback' },
                  ]}
                />
              </div>

              <SkyToggleSwitch
                label="CEF Multi-Threaded Message Loop"
                description="Run Chromium CEF's UI and IPC pumps on a dedicated native thread so Skyrim's main game thread never stalls during heavy DOM layouts."
                checked={settings.runtime?.cefMultithreaded !== false}
                onCheckedChange={(cefMultithreaded) =>
                  changeRuntime(
                    { cefMultithreaded },
                    cefMultithreaded
                      ? 'CEF Multi-Threaded Message Loop enabled.'
                      : 'CEF Single-Threaded loop mode selected (restart game to re-bind message pump).',
                  )
                }
              />

              <SkyToggleSwitch
                label="Live Asset &amp; UI Hot-Reload Watcher"
                description="Watch mod dist/ directories and SkyUltra menu packages for file edits and reload active browser views automatically during mod development."
                checked={settings.runtime?.hotReload === true}
                onCheckedChange={(hotReload) =>
                  changeRuntime(
                    { hotReload },
                    hotReload
                      ? 'Live UI Hot-Reload watcher enabled for mod development.'
                      : 'Live UI Hot-Reload watcher disabled for maximum release frame stability.',
                  )
                }
              />

              <div className="settings-runtime-summary">
                <div className="settings-kpi-pill">
                  <small>ACTIVE MENU ENGINE</small>
                  <strong>{(settings.menuEngine ?? 'cef').toUpperCase()}</strong>
                  <span>Independent of third-party mod engines</span>
                </div>
                <div className="settings-kpi-pill">
                  <small>RENDER PIPELINE</small>
                  <strong>{(settings.runtime?.renderMode ?? 'gpu').toUpperCase()} Shared Texture</strong>
                  <span>{settings.runtime?.cefMultithreaded !== false ? 'Multi-threaded loop' : 'Main-thread loop'}</span>
                </div>
                <div className="settings-kpi-pill">
                  <small>HOT-RELOAD WATCHER</small>
                  <strong>{settings.runtime?.hotReload ? 'Watching' : 'Release Lock'}</strong>
                  <span>Zero disk stat overhead when off</span>
                </div>
              </div>
            </SkySettingsSection>
          </>
        )}

        {showCursor && (
          <>
            <SkySettingsSection
              title="Pointer, Live Telemetry & 3D Studio Export"
              description="Wire up your custom SVG cursor with a full 360° color wheel, live Skyrim character snapshot polling cadence, and up to 4K transparent PNG export resolution."
            >
            <SkyToggleSwitch
              label="Custom SkyUltra vector cursor"
              description="Use the high-contrast themed vector blade cursor instead of the system pointer."
              checked={uiPrefs.cursorMode === 'themed'}
              onCheckedChange={(checked) => updateUiPref({ cursorMode: checked ? 'themed' : 'native' })}
            />

            <div className="settings-choice">
              <span>Cursor Accent Color (360° Color Wheel)</span>
              <SkyColorWheel
                mode="inline"
                size={158}
                label="Cursor blade fill &amp; rim glow"
                value={uiPrefs.cursorAccent}
                allowThemeMatch
                isThemeMatched={uiPrefs.cursorAccent === 'theme'}
                themeColor={palettes.find((p) => p.value === theme)?.color ?? '#e8b059'}
                onResetTheme={() => updateUiPref({ cursorAccent: 'theme' })}
                onChange={(hex) => updateUiPref({ cursorAccent: hex })}
              />
            </div>

            <div className="settings-choice">
              <span>Live character snapshot polling cadence</span>
              <SkyFilterChips
                label="Snapshot polling cadence"
                value={String(uiPrefs.pollIntervalMs) as '750' | '1500' | '3000' | '5000'}
                onValueChange={(val) =>
                  updateUiPref({ pollIntervalMs: Number(val) as 750 | 1500 | 3000 | 5000 })
                }
                options={[
                  { value: '750', label: '0.75s (Ultra-Fast)' },
                  { value: '1500', label: '1.5s (Responsive)' },
                  { value: '3000', label: '3.0s (Balanced)' },
                  { value: '5000', label: '5.0s (Low-Overhead)' },
                ]}
              />
            </div>

            <div className="settings-choice">
              <span>Full-view 3D studio export resolution</span>
              <SkyChoice
                label="Full-view export resolution"
                value={String(settings.actorExport.fullView)}
                onValueChange={(value) => changeExport('fullView', value)}
                options={exportOptions}
              />
            </div>

            <div className="settings-choice">
              <span>Portrait 3D studio export resolution</span>
              <SkyChoice
                label="Portrait export resolution"
                value={String(settings.actorExport.portrait)}
                onValueChange={(value) => changeExport('portrait', value)}
                options={exportOptions}
              />
            </div>
          </SkySettingsSection>
          </>
        )}

        {showHotkeys && (
          <>
            <SkySettingsSection
              title="SkyUltra interface hotkey"
              description="Bind up to 3 simultaneous keys or buttons to open and close SkyUltra. Supports Keyboard, Gamepad Controllers, and VR Headset / Controllers."
            >
              <SkyHotkeyEditor
                value={settings.hotkey ?? defaultSkyUltraHotkey}
                onChange={changeMainHotkey}
                bridge={bridge}
                disabled={!loaded || busy}
                defaultBinding={defaultSkyUltraHotkey}
                label="SkyUltra main menu hotkey"
              />
            </SkySettingsSection>

            <SkySettingsSection
              title="Virtual Keyboard Hotkey & VR Head Tracking"
              description="Toggle the full On-Screen Virtual Keyboard (ESC, F1–F12, Shift, Ctrl, Alt, Tab, Caps) at any time—whether the SkyUltra menu is open or closed. Defaults to Shift + K on Keyboard, LT + LeftThumb on Gamepad, and LeftGrip + LeftStickClick in VR."
            >
              <SkyHotkeyEditor
                value={settings.keyboardHotkey ?? defaultSkyUltraKeyboardHotkey}
                onChange={changeKeyboardHotkey}
                bridge={bridge}
                disabled={!loaded || busy}
                defaultBinding={defaultSkyUltraKeyboardHotkey}
                label="Virtual Keyboard toggle hotkey (Shift + K default)"
              />
              <SkyToggleSwitch
                label="VR Head Tracking (Follow Headset Movement)"
                description="Automatically attach open menus to your VR HMD orientation (atGaze) so the interface stays in front of your eyes wherever you turn your head."
                checked={settings.vrHeadTracking !== false}
                onCheckedChange={changeVrHeadTracking}
              />
            </SkySettingsSection>

            <SkySettingsSection
              title="Mod hotkeys for every SkyUltra mod"
              description="Every mod built with SkyUltra is automatically discovered from Data/SkyUltra/<author.modname>/manifest.json and can define or customize its own 1-to-3 input toggle chord across Keyboard, Gamepad, and VR Headset."
            >
              <div className="settings-mod-blueprint" aria-label="How SkyUltra mod hotkeys work">
                <div className="settings-mod-blueprint__head">
                  <span>HOW MOD HOTKEYS WORK &amp; HOW TO ADD YOUR MOD</span>
                  <code>Data/SkyUltra/&lt;author.modname&gt;/manifest.json</code>
                </div>
                <div className="settings-mod-blueprint__steps">
                  <div className="settings-mod-blueprint__step">
                    <b>01 · AUTO-DISCOVERY</b>
                    <p>
                      Place any mod folder in <code>Data/SkyUltra/&lt;author.modname&gt;/manifest.json</code> with{' '}
                      <code>&quot;entry&quot;: &quot;Files/index.html&quot;</code>. SkyUltra discovers it automatically and lists it below.
                    </p>
                  </div>
                  <div className="settings-mod-blueprint__step">
                    <b>02 · DEFAULT MANIFEST CHORD</b>
                    <p>
                      Mod authors can include an optional{' '}
                      <code>&quot;hotkey&quot;: &#123; &quot;keyboard&quot;: [&quot;Ctrl&quot;, &quot;M&quot;] &#125;</code> in{' '}
                      <code>manifest.json</code> to seed a default shortcut on first install.
                    </p>
                  </div>
                  <div className="settings-mod-blueprint__step">
                    <b>03 · ZERO-DLL LAUNCH &amp; SDK</b>
                    <p>
                      Pressing the bound chord in Skyrim automatically creates and toggles the mod&apos;s panel—even with no C++ DLL or ESP—or mods can embed <code>&lt;SkyHotkeyEditor /&gt;</code> directly.
                    </p>
                  </div>
                </div>
              </div>

              <div className="settings-mod-hotkey-bar">
                <div className="settings-choice">
                  <span>Target SkyUltra mod</span>
                  <SkyChoice
                    label="Select SkyUltra mod for hotkey binding"
                    value={activeModId}
                    onValueChange={setSelectedModId}
                    options={
                      knownMods.length > 0
                        ? knownMods.map((mod) => ({
                            value: mod.id,
                            label: `${mod.name} (${formatSkyUltraHotkey(settings.modHotkeys?.[mod.id], 'keyboard')})`,
                          }))
                        : [{ value: '', label: 'No external SkyUltra mods installed — add a mod ID below' }]
                    }
                  />
                </div>
                <div className="settings-mod-custom-add">
                  <input
                    type="text"
                    className="settings-mod-input"
                    aria-label="Add mod identifier for hotkey binding"
                    placeholder="Add mod ID (e.g. author.modname)…"
                    value={customModInput}
                    onChange={(event) => setCustomModInput(event.currentTarget.value)}
                    onKeyDown={(event) => {
                      if (event.key === 'Enter') {
                        event.preventDefault();
                        registerCustomModId();
                      }
                    }}
                  />
                  <button type="button" className="control-button" onClick={registerCustomModId}>
                    Add mod ID
                  </button>
                </div>
              </div>

              {activeModId ? (
                <SkyHotkeyEditor
                  value={activeModBinding}
                  onChange={(binding) => changeModHotkey(activeModId, binding)}
                  bridge={bridge}
                  disabled={!loaded || busy}
                  allowEmptyKeyboard
                  defaultBinding={emptyModHotkey}
                  label={`Hotkey binding for ${activeModId}`}
                />
              ) : (
                <p className="inspector-note">
                  Install a SkyUltra mod under <code>Data/SkyUltra/&lt;author.modname&gt;/manifest.json</code> or enter a mod ID above to configure its shortcut.
                </p>
              )}
            </SkySettingsSection>
          </>
        )}

        {showSplash && (
          <SkySettingsSection
            title="Opening splash screen & soundtrack"
            description="Unified startup video and stereo audio preferences. Once per game session is the default; gameplay HUDs never trigger it, and Escape stops playback immediately."
          >
            <div className="splash-status splash-status--inline">
              <img src="./SkyUltraLogoMark.png" alt="" />
              <div>
                <strong>The SkyUltra splash screen</strong>
                <p>
                  {status?.failed
                    ? 'Playback failed at ' +
                      status.stage +
                      '. Code 0x' +
                      status.errorCode.toString(16).padStart(8, '0').toUpperCase() +
                      ' · media ' +
                      status.mediaError +
                      '. Check SKSE/SkyUltra.log for details.'
                    : status?.active
                      ? 'Playing your full-screen splash screen with native audio.'
                      : status?.hasFrame
                        ? 'Video frames delivered successfully.'
                        : native
                          ? 'The original splash screen plays through the native compositor on both engines.'
                          : 'Video playback is available inside Skyrim.'}
                </p>
              </div>
            </div>
            <SkyToggleSwitch
              label="Splash screen"
              description="Play the SkyUltra video when an eligible menu opens."
              checked={splash.enabled}
              onCheckedChange={(enabled) => change({ enabled })}
            />
            <div className="settings-choice">
              <span>Playback length</span>
              <SkyFilterChips
                label="Splash playback length"
                value={splash.playback}
                onValueChange={(playback) => change({ playback })}
                options={[
                  { value: 'full-duration', label: 'Full splash screen' },
                  { value: 'until-ready', label: 'Until page ready' },
                ]}
              />
            </div>
            <div className="settings-choice">
              <span>Replay schedule</span>
              <SkyChoice
                label="Splash replay"
                value={splash.replay}
                onValueChange={(replay) => change({ replay })}
                options={[
                  { value: 'session', label: 'Once per game session' },
                  { value: 'per-mod', label: 'Once per mod per session' },
                  { value: 'each-open', label: 'Every menu opening' },
                ]}
              />
            </div>
            <SkyToggleSwitch
              label="Original soundtrack"
              description="Play the original splash screen stereo audio."
              checked={splash.audio}
              onCheckedChange={(audio) => change({ audio })}
            />
            <SkyRangeSlider
              label="Splash screen volume"
              value={Math.round(splash.volume * 100)}
              min={0}
              max={100}
              step={1}
              unit="%"
              onValueChange={(volume) => change({ volume: volume / 100 })}
              disabled={!splash.audio}
            />
          </SkySettingsSection>
        )}
      </fieldset>
      <p className="inspector-note" role="status">
        {message ||
          (!loaded
            ? 'Reading preferences…'
            : 'Theme, density, glow, cursor, and polling changes apply live! Click Save Preferences to persist hotkey, splash, and export settings to Skyrim.')}
      </p>
        </div>
      </div>
    </div>
  );
}
