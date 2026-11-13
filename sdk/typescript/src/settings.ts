import { getSkyWebView, type JsonValue, type SkyWebViewAPI } from './index.js';
export interface SkyUltraSplashSettings {
  enabled: boolean;
  playback: 'until-ready' | 'full-duration';
  replay: 'session' | 'each-open' | 'per-mod';
  audio: boolean; volume: number;
}
export const skyUltraActorExportResolutions = [720, 1080, 1440, 2048, 2160, 4096] as const;
export type SkyUltraActorExportResolution = typeof skyUltraActorExportResolutions[number];
export interface SkyUltraActorExportSettings {
  fullView: SkyUltraActorExportResolution;
  portrait: SkyUltraActorExportResolution;
}
export type SkyUltraHotkeyDevice = 'keyboard' | 'gamepad' | 'vr';
export type SkyUltraHotkeyBinding = {
  keyboard: string[];
  gamepad: string[];
  vr: string[];
};
export type SkyUltraRegisteredMod = {
  id: string;
  name: string;
};
export const defaultSkyUltraHotkey: SkyUltraHotkeyBinding = {
  keyboard: ['Shift', 'F6'],
  gamepad: ['Back', 'RightThumb'],
  vr: ['LeftGrip', 'RightBY'],
};
export const defaultSkyUltraKeyboardHotkey: SkyUltraHotkeyBinding = {
  keyboard: ['Shift', 'K'],
  gamepad: ['LT', 'LeftThumb'],
  vr: ['LeftGrip', 'LeftStickClick'],
};
export type SkyUltraEngineChoice = 'ultralight' | 'cef';
export type SkyUltraDisplayMode = 'fullscreen' | 'windowed';
export type SkyUltraBarterDisplayMode = 'half' | 'full' | 'windowed';
/** Half / full / windowed container layouts shared by the Magic and Favorites menu replacements. */
export type SkyUltraContainerDisplayMode = SkyUltraBarterDisplayMode;
export interface SkyUltraSettings {
  schemaVersion?: number;
  splash: SkyUltraSplashSettings;
  actorExport: SkyUltraActorExportSettings;
  hotkey?: SkyUltraHotkeyBinding;
  keyboardHotkey?: SkyUltraHotkeyBinding;
  menuEngine?: SkyUltraEngineChoice;
  availableEngines?: SkyUltraEngineChoice[];
  modEngines?: Record<string, SkyUltraEngineChoice>;
  vrHeadTracking?: boolean;
  replaceSkyrimInventory?: boolean;
  replaceSkyrimBarter?: boolean;
  replaceSkyrimMagic?: boolean;
  replaceSkyrimFavorites?: boolean;
  replaceSkyrimContainer?: boolean;
  replaceSkyrimMannequin?: boolean;
  replaceSkyrimLoot?: boolean;
  replaceSkyrimPickpocket?: boolean;
  replaceSkyrimCompass?: boolean;
  menuDisplayMode?: SkyUltraDisplayMode;
  inventoryDisplayMode?: SkyUltraDisplayMode;
  barterDisplayMode?: SkyUltraBarterDisplayMode;
  magicDisplayMode?: SkyUltraContainerDisplayMode;
  favoritesDisplayMode?: SkyUltraContainerDisplayMode;
  containerDisplayMode?: SkyUltraContainerDisplayMode;
  mannequinDisplayMode?: SkyUltraContainerDisplayMode;
  lootDisplayMode?: SkyUltraContainerDisplayMode;
  pickpocketDisplayMode?: SkyUltraContainerDisplayMode;
  menuPauseWorld?: boolean;
  inventoryPauseWorld?: boolean;
  barterPauseWorld?: boolean;
  magicPauseWorld?: boolean;
  favoritesPauseWorld?: boolean;
  containerPauseWorld?: boolean;
  mannequinPauseWorld?: boolean;
  lootPauseWorld?: boolean;
  pickpocketPauseWorld?: boolean;
  modHotkeys?: Record<string, SkyUltraHotkeyBinding>;
  registeredMods?: SkyUltraRegisteredMod[];
  profileState?: Record<string, unknown>;
}
export const defaultSkyUltraSettings: SkyUltraSettings = {
  schemaVersion: 2,
  splash: { enabled: true, playback: 'full-duration', replay: 'session', audio: true, volume: 0.8 },
  actorExport: { fullView: 4096, portrait: 4096 },
  hotkey: { keyboard: [...defaultSkyUltraHotkey.keyboard], gamepad: [...defaultSkyUltraHotkey.gamepad], vr: [...defaultSkyUltraHotkey.vr] },
  keyboardHotkey: {
    keyboard: [...defaultSkyUltraKeyboardHotkey.keyboard],
    gamepad: [...defaultSkyUltraKeyboardHotkey.gamepad],
    vr: [...defaultSkyUltraKeyboardHotkey.vr],
  },
  menuEngine: 'cef',
  availableEngines: ['cef', 'ultralight'],
  modEngines: {},
  vrHeadTracking: true,
  replaceSkyrimInventory: true,
  replaceSkyrimBarter: true,
  replaceSkyrimMagic: true,
  replaceSkyrimFavorites: true,
  replaceSkyrimContainer: true,
  replaceSkyrimMannequin: true,
  replaceSkyrimLoot: true,
  replaceSkyrimPickpocket: true,
  replaceSkyrimCompass: true,
  menuDisplayMode: 'fullscreen',
  inventoryDisplayMode: 'fullscreen',
  barterDisplayMode: 'half',
  magicDisplayMode: 'half',
  favoritesDisplayMode: 'half',
  containerDisplayMode: 'half',
  mannequinDisplayMode: 'full',
  lootDisplayMode: 'half',
  pickpocketDisplayMode: 'half',
  menuPauseWorld: false,
  inventoryPauseWorld: false,
  barterPauseWorld: false,
  magicPauseWorld: false,
  favoritesPauseWorld: false,
  containerPauseWorld: false,
  mannequinPauseWorld: false,
  lootPauseWorld: false,
  pickpocketPauseWorld: false,
  modHotkeys: {},
  registeredMods: [],
};
function sanitizeChord(value: unknown, fallback: string[], allowEmpty: boolean): string[] {
  if (!Array.isArray(value)) return [...fallback];
  const tokens: string[] = [];
  for (const item of value) {
    if (typeof item !== 'string') continue;
    const trimmed = item.trim();
    if (!trimmed || trimmed.length > 32 || !/^[A-Za-z0-9_-]+$/.test(trimmed)) continue;
    if (!tokens.includes(trimmed)) tokens.push(trimmed);
    if (tokens.length >= 3) break;
  }
  if (!allowEmpty && tokens.length === 0) return [...fallback];
  return tokens;
}
export function normalizeSkyUltraHotkey(binding?: Partial<SkyUltraHotkeyBinding> | null, allowEmptyKeyboard = false): SkyUltraHotkeyBinding {
  return {
    keyboard: sanitizeChord(binding?.keyboard, allowEmptyKeyboard ? [] : defaultSkyUltraHotkey.keyboard, allowEmptyKeyboard),
    gamepad: sanitizeChord(binding?.gamepad, allowEmptyKeyboard ? [] : defaultSkyUltraHotkey.gamepad, true),
    vr: sanitizeChord(binding?.vr, allowEmptyKeyboard ? [] : defaultSkyUltraHotkey.vr, true),
  };
}
export function formatSkyUltraHotkey(binding?: Partial<SkyUltraHotkeyBinding> | null, device: SkyUltraHotkeyDevice = 'keyboard'): string {
  const normalized = normalizeSkyUltraHotkey(binding, true);
  const chord = normalized[device];
  return chord.length > 0 ? chord.join(' + ') : 'Unbound';
}
function withExportDefaults(settings: SkyUltraSettings): SkyUltraSettings {
  const isExcludedModId = (id: string): boolean => {
    const normalized = id.trim().toLowerCase();
    return (
      !normalized ||
      normalized === 'skyultra.menu' ||
      normalized === 'skyultra.ui_kit_test_mod' ||
      normalized === 'skyultra.uikit'
    );
  };
  const modHotkeys: Record<string, SkyUltraHotkeyBinding> = {};
  if (settings.modHotkeys && typeof settings.modHotkeys === 'object') {
    for (const [modId, binding] of Object.entries(settings.modHotkeys)) {
      if (modId && !isExcludedModId(modId) && typeof binding === 'object' && binding !== null) {
        modHotkeys[modId] = normalizeSkyUltraHotkey(binding, true);
      }
    }
  }
  const registeredMods = Array.isArray(settings.registeredMods)
    ? settings.registeredMods.filter(
        (m): m is SkyUltraRegisteredMod =>
          Boolean(m && typeof m.id === 'string' && typeof m.name === 'string' && !isExcludedModId(m.id)),
      )
    : [];
  const profileState = settings.profileState && typeof settings.profileState === 'object' && !Array.isArray(settings.profileState)
    ? { ...settings.profileState }
    : undefined;
  const rawKeyboardHk = settings.keyboardHotkey;
  const keyboardHotkey: SkyUltraHotkeyBinding = {
    keyboard: sanitizeChord(rawKeyboardHk?.keyboard, defaultSkyUltraKeyboardHotkey.keyboard, false),
    gamepad: sanitizeChord(rawKeyboardHk?.gamepad, defaultSkyUltraKeyboardHotkey.gamepad, true),
    vr: sanitizeChord(rawKeyboardHk?.vr, defaultSkyUltraKeyboardHotkey.vr, true),
  };
  const menuEngine: SkyUltraEngineChoice = settings.menuEngine === 'ultralight' ? 'ultralight' : 'cef';
  const availableEngines: SkyUltraEngineChoice[] = Array.isArray(settings.availableEngines) && settings.availableEngines.length > 0
    ? settings.availableEngines.filter((e): e is SkyUltraEngineChoice => e === 'ultralight' || e === 'cef')
    : ['cef', 'ultralight'];
  const modEngines: Record<string, SkyUltraEngineChoice> = {};
  if (settings.modEngines && typeof settings.modEngines === 'object') {
    for (const [modId, eng] of Object.entries(settings.modEngines)) {
      if (modId && (eng === 'ultralight' || eng === 'cef')) {
        modEngines[modId] = eng;
      }
    }
  }
  const vrHeadTracking = typeof settings.vrHeadTracking === 'boolean' ? settings.vrHeadTracking : true;
  const replaceSkyrimInventory = typeof settings.replaceSkyrimInventory === 'boolean' ? settings.replaceSkyrimInventory : true;
  const replaceSkyrimBarter = typeof settings.replaceSkyrimBarter === 'boolean' ? settings.replaceSkyrimBarter : true;
  const menuDisplayMode: SkyUltraDisplayMode = settings.menuDisplayMode === 'windowed' ? 'windowed' : 'fullscreen';
  const inventoryDisplayMode: SkyUltraDisplayMode = settings.inventoryDisplayMode === 'windowed' ? 'windowed' : 'fullscreen';
  const barterDisplayMode: SkyUltraBarterDisplayMode =
    settings.barterDisplayMode === 'full' || settings.barterDisplayMode === 'windowed' ? settings.barterDisplayMode : 'half';
  const menuPauseWorld = typeof settings.menuPauseWorld === 'boolean' ? settings.menuPauseWorld : false;
  const inventoryPauseWorld = typeof settings.inventoryPauseWorld === 'boolean' ? settings.inventoryPauseWorld : false;
  const barterPauseWorld = typeof settings.barterPauseWorld === 'boolean' ? settings.barterPauseWorld : false;
  const replaceSkyrimMagic = typeof settings.replaceSkyrimMagic === 'boolean' ? settings.replaceSkyrimMagic : true;
  const replaceSkyrimFavorites = typeof settings.replaceSkyrimFavorites === 'boolean' ? settings.replaceSkyrimFavorites : true;
  const replaceSkyrimContainer = typeof settings.replaceSkyrimContainer === 'boolean' ? settings.replaceSkyrimContainer : true;
  const replaceSkyrimMannequin = typeof settings.replaceSkyrimMannequin === 'boolean' ? settings.replaceSkyrimMannequin : true;
  const replaceSkyrimLoot = typeof settings.replaceSkyrimLoot === 'boolean' ? settings.replaceSkyrimLoot : true;
  const replaceSkyrimPickpocket = typeof settings.replaceSkyrimPickpocket === 'boolean' ? settings.replaceSkyrimPickpocket : true;
  const replaceSkyrimCompass = typeof settings.replaceSkyrimCompass === 'boolean' ? settings.replaceSkyrimCompass : true;
  const containerMode = (value: unknown, fallback: SkyUltraContainerDisplayMode = 'half'): SkyUltraContainerDisplayMode =>
    value === 'full' || value === 'windowed' || value === 'half' ? value : value === 'fullscreen' ? 'full' : fallback;
  const magicDisplayMode = containerMode(settings.magicDisplayMode, 'half');
  const favoritesDisplayMode = containerMode(settings.favoritesDisplayMode, 'half');
  const containerDisplayMode = containerMode(settings.containerDisplayMode, 'half');
  const mannequinDisplayMode = containerMode(settings.mannequinDisplayMode, 'full');
  const lootDisplayMode = containerMode(settings.lootDisplayMode, 'half');
  const pickpocketDisplayMode = containerMode(settings.pickpocketDisplayMode, 'half');
  const magicPauseWorld = typeof settings.magicPauseWorld === 'boolean' ? settings.magicPauseWorld : false;
  const favoritesPauseWorld = typeof settings.favoritesPauseWorld === 'boolean' ? settings.favoritesPauseWorld : false;
  const containerPauseWorld = typeof settings.containerPauseWorld === 'boolean' ? settings.containerPauseWorld : false;
  const mannequinPauseWorld = typeof settings.mannequinPauseWorld === 'boolean' ? settings.mannequinPauseWorld : false;
  const lootPauseWorld = typeof settings.lootPauseWorld === 'boolean' ? settings.lootPauseWorld : false;
  const pickpocketPauseWorld = typeof settings.pickpocketPauseWorld === 'boolean' ? settings.pickpocketPauseWorld : false;
  return {
    ...settings,
    schemaVersion: typeof settings.schemaVersion === 'number' && Number.isFinite(settings.schemaVersion) ? Math.max(1, Math.round(settings.schemaVersion)) : 2,
    splash: { ...settings.splash },
    actorExport: { ...defaultSkyUltraSettings.actorExport, ...settings.actorExport },
    hotkey: normalizeSkyUltraHotkey(settings.hotkey ?? defaultSkyUltraHotkey, false),
    keyboardHotkey,
    menuEngine,
    availableEngines,
    modEngines,
    vrHeadTracking,
    replaceSkyrimInventory,
    replaceSkyrimBarter,
    menuDisplayMode,
    inventoryDisplayMode,
    barterDisplayMode,
    menuPauseWorld,
    inventoryPauseWorld,
    barterPauseWorld,
    replaceSkyrimMagic,
    replaceSkyrimFavorites,
    replaceSkyrimContainer,
    replaceSkyrimMannequin,
    replaceSkyrimLoot,
    replaceSkyrimPickpocket,
    replaceSkyrimCompass,
    magicDisplayMode,
    favoritesDisplayMode,
    containerDisplayMode,
    mannequinDisplayMode,
    lootDisplayMode,
    pickpocketDisplayMode,
    magicPauseWorld,
    favoritesPauseWorld,
    containerPauseWorld,
    mannequinPauseWorld,
    lootPauseWorld,
    pickpocketPauseWorld,
    modHotkeys,
    registeredMods,
    ...(profileState ? { profileState } : {}),
  };
}
/** Global settings are writable by the built-in SkyUltra menu; registered mods may update their own hotkey via updateSkyUltraModHotkey. */
export async function readSkyUltraSettings(bridge: SkyWebViewAPI = getSkyWebView()): Promise<SkyUltraSettings> {
  return withExportDefaults(await bridge.request<SkyUltraSettings>('ui.getSettings', {}));
}
export async function updateSkyUltraSettings(settings: SkyUltraSettings, bridge: SkyWebViewAPI = getSkyWebView()): Promise<SkyUltraSettings> {
  const normalized = withExportDefaults(settings);
  for (const value of Object.values(normalized.actorExport)) if (!(skyUltraActorExportResolutions as readonly number[]).includes(value)) throw new Error('Unsupported actor export resolution.');
  const resolvedHotkey = normalized.hotkey ?? defaultSkyUltraHotkey;
  const resolvedKeyboardHotkey = normalized.keyboardHotkey ?? defaultSkyUltraKeyboardHotkey;
  const resolvedModHotkeys: Record<string, { keyboard: string[]; gamepad: string[]; vr: string[] }> = {};
  for (const [k, v] of Object.entries(normalized.modHotkeys ?? {})) {
    resolvedModHotkeys[k] = { keyboard: [...v.keyboard], gamepad: [...v.gamepad], vr: [...v.vr] };
  }
  const payload: Record<string, JsonValue> = {
    schemaVersion: 2,
    splash: { ...normalized.splash },
    actorExport: { ...normalized.actorExport },
    hotkey: { keyboard: [...resolvedHotkey.keyboard], gamepad: [...resolvedHotkey.gamepad], vr: [...resolvedHotkey.vr] },
    keyboardHotkey: {
      keyboard: [...resolvedKeyboardHotkey.keyboard],
      gamepad: [...resolvedKeyboardHotkey.gamepad],
      vr: [...resolvedKeyboardHotkey.vr],
    },
    menuEngine: normalized.menuEngine ?? 'cef',
    modEngines: (normalized.modEngines ?? {}) as Record<string, JsonValue>,
    vrHeadTracking: normalized.vrHeadTracking ?? true,
    replaceSkyrimInventory: normalized.replaceSkyrimInventory ?? true,
    replaceSkyrimBarter: normalized.replaceSkyrimBarter ?? true,
    menuDisplayMode: normalized.menuDisplayMode ?? 'fullscreen',
    inventoryDisplayMode: normalized.inventoryDisplayMode ?? 'fullscreen',
    barterDisplayMode: normalized.barterDisplayMode ?? 'half',
    menuPauseWorld: normalized.menuPauseWorld ?? false,
    inventoryPauseWorld: normalized.inventoryPauseWorld ?? false,
    barterPauseWorld: normalized.barterPauseWorld ?? false,
    replaceSkyrimMagic: normalized.replaceSkyrimMagic ?? true,
    replaceSkyrimFavorites: normalized.replaceSkyrimFavorites ?? true,
    replaceSkyrimContainer: normalized.replaceSkyrimContainer ?? true,
    replaceSkyrimMannequin: normalized.replaceSkyrimMannequin ?? true,
    replaceSkyrimLoot: normalized.replaceSkyrimLoot ?? true,
    replaceSkyrimPickpocket: normalized.replaceSkyrimPickpocket ?? true,
    replaceSkyrimCompass: normalized.replaceSkyrimCompass ?? true,
    magicDisplayMode: normalized.magicDisplayMode ?? 'half',
    favoritesDisplayMode: normalized.favoritesDisplayMode ?? 'half',
    containerDisplayMode: normalized.containerDisplayMode ?? 'half',
    mannequinDisplayMode: normalized.mannequinDisplayMode ?? 'full',
    lootDisplayMode: normalized.lootDisplayMode ?? 'half',
    pickpocketDisplayMode: normalized.pickpocketDisplayMode ?? 'half',
    magicPauseWorld: normalized.magicPauseWorld ?? false,
    favoritesPauseWorld: normalized.favoritesPauseWorld ?? false,
    containerPauseWorld: normalized.containerPauseWorld ?? false,
    mannequinPauseWorld: normalized.mannequinPauseWorld ?? false,
    lootPauseWorld: normalized.lootPauseWorld ?? false,
    pickpocketPauseWorld: normalized.pickpocketPauseWorld ?? false,
    modHotkeys: resolvedModHotkeys,
  };
  if (normalized.profileState) {
    payload.profileState = JSON.parse(JSON.stringify(normalized.profileState)) as JsonValue;
  }
  return withExportDefaults(await bridge.request<SkyUltraSettings>('ui.updateSettings', payload));
}
/** Persist per-character profile layouts, themes, loadouts, and global defaults to SkyUltra.settings.json. */
export async function saveSkyUltraHostProfileState(
  profileState: Record<string, unknown>,
  bridge: SkyWebViewAPI = getSkyWebView(),
): Promise<SkyUltraSettings> {
  return withExportDefaults(await bridge.request<SkyUltraSettings>('ui.updateSettings', {
    schemaVersion: 2,
    profileState: JSON.parse(JSON.stringify(profileState)),
  }));
}
/** Read the configured hotkey chord for a specific SkyUltra mod (or default empty chord if unbound). */
export async function readSkyUltraModHotkey(modId: string, bridge: SkyWebViewAPI = getSkyWebView()): Promise<SkyUltraHotkeyBinding> {
  const settings = await readSkyUltraSettings(bridge);
  return normalizeSkyUltraHotkey(settings.modHotkeys?.[modId], true);
}
/** Update the hotkey chord for a SkyUltra mod (callable from either the main SkyUltra menu or the mod's own view). */
export async function updateSkyUltraModHotkey(modId: string, binding: Partial<SkyUltraHotkeyBinding>, bridge: SkyWebViewAPI = getSkyWebView()): Promise<SkyUltraHotkeyBinding> {
  const normalized = normalizeSkyUltraHotkey(binding, true);
  const current = await readSkyUltraSettings(bridge);
  const resolvedHotkey = current.hotkey ?? defaultSkyUltraHotkey;
  const nextModHotkeys: Record<string, { keyboard: string[]; gamepad: string[]; vr: string[] }> = {};
  for (const [k, v] of Object.entries(current.modHotkeys ?? {})) {
    nextModHotkeys[k] = { keyboard: [...v.keyboard], gamepad: [...v.gamepad], vr: [...v.vr] };
  }
  nextModHotkeys[modId] = { keyboard: [...normalized.keyboard], gamepad: [...normalized.gamepad], vr: [...normalized.vr] };
  const updated = await bridge.request<SkyUltraSettings>('ui.updateSettings', {
    splash: { ...current.splash },
    actorExport: { ...current.actorExport },
    hotkey: { keyboard: [...resolvedHotkey.keyboard], gamepad: [...resolvedHotkey.gamepad], vr: [...resolvedHotkey.vr] },
    modHotkeys: nextModHotkeys,
  });
  const resolved = withExportDefaults(updated);
  return normalizeSkyUltraHotkey(resolved.modHotkeys?.[modId] ?? normalized, true);
}
export function closeSkyUltraPage(bridge: SkyWebViewAPI = getSkyWebView()): Promise<{closed:boolean}> {
  return bridge.request('ui.close', {});
}
/** Hide the native cursor when drawing your own DOM cursor. Grant ui.setCursor. */
export function setSkyUltraCursor(mode:'native'|'hidden',bridge:SkyWebViewAPI=getSkyWebView(),accent?:string):Promise<{updated:boolean}> {
  if(accent!==undefined&&!/^#[0-9a-f]{6}$/i.test(accent))throw new TypeError('Cursor accent must be a six-digit hex color.');
  return bridge.request('ui.setCursor',accent?{mode,accent}:{mode});
}
export interface SkyUltraSplashStatus {active:boolean;hasFrame:boolean;failed:boolean;errorCode:number;mediaError:number;stage:string;}
export function readSkyUltraSplashStatus(bridge:SkyWebViewAPI=getSkyWebView()):Promise<SkyUltraSplashStatus>{return bridge.request('ui.getSplashStatus',{});}
/** Built-in menu only; previews the full splash screen regardless of the saved replay/playback schedule. */
export function replaySkyUltraSplash(bridge:SkyWebViewAPI=getSkyWebView()):Promise<{started:boolean}>{return bridge.request('ui.replaySplash',{});}

export interface SkyUltraUiPreferences {
  defaultTheme: string;
  density: 'compact' | 'comfortable' | 'spacious';
  motion: 'cinematic' | 'snappy' | 'reduced';
  neonGlow: number;
  cursorMode: 'themed' | 'native';
  cursorAccent: string;
  pollIntervalMs: 750 | 1500 | 3000 | 5000;
  studioLighting: 'studio' | 'warm' | 'cool' | 'dramatic';
  backdropGrid: boolean;
  scanlines: boolean;
}
export const defaultSkyUltraUiPreferences: SkyUltraUiPreferences = {
  defaultTheme: 'dragonstone',
  density: 'comfortable',
  motion: 'cinematic',
  neonGlow: 100,
  cursorMode: 'themed',
  cursorAccent: 'theme',
  pollIntervalMs: 1500,
  studioLighting: 'studio',
  backdropGrid: true,
  scanlines: true,
};
const UI_PREFS_STORAGE_KEY = 'skyultra.uiPreferences.v2';
export function sanitizeSkyUltraUiPrefs(raw: unknown): SkyUltraUiPreferences {
  const parsed = raw && typeof raw === 'object' ? (raw as Partial<SkyUltraUiPreferences>) : {};
  return {
    defaultTheme: typeof parsed.defaultTheme === 'string' && parsed.defaultTheme ? parsed.defaultTheme : defaultSkyUltraUiPreferences.defaultTheme,
    density: parsed.density === 'compact' || parsed.density === 'spacious' ? parsed.density : 'comfortable',
    motion: parsed.motion === 'snappy' || parsed.motion === 'reduced' ? parsed.motion : 'cinematic',
    neonGlow: typeof parsed.neonGlow === 'number' && Number.isFinite(parsed.neonGlow) ? Math.max(0, Math.min(150, Math.round(parsed.neonGlow))) : 100,
    cursorMode: parsed.cursorMode === 'native' ? 'native' : 'themed',
    cursorAccent: typeof parsed.cursorAccent === 'string' && (parsed.cursorAccent === 'theme' || /^#[0-9a-f]{6}$/i.test(parsed.cursorAccent)) ? parsed.cursorAccent : 'theme',
    pollIntervalMs: parsed.pollIntervalMs === 750 || parsed.pollIntervalMs === 3000 || parsed.pollIntervalMs === 5000 ? parsed.pollIntervalMs : 1500,
    studioLighting: parsed.studioLighting === 'warm' || parsed.studioLighting === 'cool' || parsed.studioLighting === 'dramatic' ? parsed.studioLighting : 'studio',
    backdropGrid: typeof parsed.backdropGrid === 'boolean' ? parsed.backdropGrid : true,
    scanlines: typeof parsed.scanlines === 'boolean' ? parsed.scanlines : true,
  };
}
export function readSkyUltraUiPrefs(): SkyUltraUiPreferences {
  try {
    if (typeof window === 'undefined' || !window.localStorage) return { ...defaultSkyUltraUiPreferences };
    const raw = window.localStorage.getItem(UI_PREFS_STORAGE_KEY);
    if (!raw) return { ...defaultSkyUltraUiPreferences };
    return sanitizeSkyUltraUiPrefs(JSON.parse(raw));
  } catch {
    return { ...defaultSkyUltraUiPreferences };
  }
}
export function writeSkyUltraUiPrefs(next: Partial<SkyUltraUiPreferences>): SkyUltraUiPreferences {
  const merged: SkyUltraUiPreferences = { ...readSkyUltraUiPrefs(), ...next };
  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      window.localStorage.setItem(UI_PREFS_STORAGE_KEY, JSON.stringify(merged));
      window.dispatchEvent(new CustomEvent('skyultra:ui-prefs', { detail: merged }));
    }
  } catch {
    // Ignore storage errors in restricted environments
  }
  return merged;
}
