import { getSkyWebView, type SkyWebViewAPI } from './index.js';
import {
  parseSkyUltraHudLayout,
  skyUltraWidgetAnchors,
  skyUltraWidgetKinds,
  type SkyUltraHudLayout,
  type SkyUltraWidgetAnchor,
  type SkyUltraWidgetKind,
} from './layout.js';
import {
  defaultSkyUltraUiPreferences,
  readSkyUltraSettings,
  readSkyUltraUiPrefs,
  sanitizeSkyUltraUiPrefs,
  saveSkyUltraHostProfileState,
  writeSkyUltraUiPrefs,
  type SkyUltraUiPreferences,
} from './settings.js';

export const SKYULTRA_PROFILE_SCHEMA_VERSION = 2 as const;
export type SkyUltraProfileMode = 'per-character' | 'global';

export interface SkyUltraSavedLoadoutSlotRecord {
  id: string;
  name: string;
  category: string;
}

export interface SkyUltraSavedLoadoutRecord {
  id: string;
  name: string;
  characterName: string;
  createdAt: string;
  slots: Record<string, SkyUltraSavedLoadoutSlotRecord | null | undefined>;
}

export interface SkyUltraGlobalDefaults {
  theme: string;
  uiPrefs: SkyUltraUiPreferences;
  hudLayout: SkyUltraHudLayout | null;
}

export interface SkyUltraCharacterProfile {
  id: string;
  characterName: string;
  updatedAt: string;
  theme?: string;
  uiPrefs?: Partial<SkyUltraUiPreferences>;
  hudLayout?: SkyUltraHudLayout | null;
  loadouts?: SkyUltraSavedLoadoutRecord[];
}

export interface SkyUltraProfileStoreV2 {
  schemaVersion: 2;
  mode: SkyUltraProfileMode;
  activeProfileId: string;
  globalDefaults: SkyUltraGlobalDefaults;
  profiles: Record<string, SkyUltraCharacterProfile>;
  migratedFromVersion?: number;
}

export interface ResolvedSkyUltraProfile {
  profileId: string;
  characterName: string;
  isCharacterScoped: boolean;
  hasCharacterOverrides: boolean;
  theme: string;
  uiPrefs: SkyUltraUiPreferences;
  hudLayout: SkyUltraHudLayout | null;
  loadouts: SkyUltraSavedLoadoutRecord[];
}

const PROFILE_STORE_LOCAL_KEY = 'skyultra.profileStore.v2';
const LEGACY_HUD_STUDIO_KEY = 'skyultra.hudStudio.v3';
const LEGACY_LOADOUTS_KEY = 'skyultra.character.loadouts.v1';

function sanitizeThemeId(value: unknown, fallback = 'dragonstone'): string {
  if (typeof value === 'string') {
    const trimmed = value.trim().toLowerCase();
    if (/^[a-z][a-z0-9-]{1,31}$/.test(trimmed)) return trimmed;
  }
  return fallback;
}

export function characterProfileIdForName(playerName?: string | null): string {
  if (typeof playerName !== 'string') return 'global';
  const cleaned = playerName
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .slice(0, 40);
  return cleaned ? `char.${cleaned}` : 'global';
}

export function coerceLegacyHudLayout(raw: unknown, fallbackTheme = 'dragonstone'): SkyUltraHudLayout | null {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) return null;
  try {
    return parseSkyUltraHudLayout(raw);
  } catch {
    const record = raw as Record<string, unknown>;
    const rawWidgets = Array.isArray(record.widgets) ? record.widgets : [];
    const widgets = rawWidgets.filter(
      (k): k is SkyUltraWidgetKind => typeof k === 'string' && (skyUltraWidgetKinds as readonly string[]).includes(k),
    );
    const deduped = [...new Set(widgets)].slice(0, 16);
    if (deduped.length === 0) return null;
    const anchorCandidate = typeof record.anchor === 'string' && (skyUltraWidgetAnchors as readonly string[]).includes(record.anchor)
      ? (record.anchor as SkyUltraWidgetAnchor)
      : 'top-left';
    const candidate = {
      version: 1,
      widgets: deduped,
      anchor: anchorCandidate,
      scale: typeof record.scale === 'number' && Number.isFinite(record.scale) ? Math.max(0.7, Math.min(1.5, record.scale)) : 1,
      opacity: typeof record.opacity === 'number' && Number.isFinite(record.opacity) ? Math.max(0.35, Math.min(1, record.opacity)) : 0.94,
      theme: sanitizeThemeId(record.theme, fallbackTheme),
      ...(typeof record.snapGrid === 'number' ? { snapGrid: record.snapGrid } : {}),
      ...(record.placements && typeof record.placements === 'object' ? { placements: record.placements } : {}),
      ...(record.colors && typeof record.colors === 'object' ? { colors: record.colors } : {}),
      ...(typeof record.glow === 'number' ? { glow: record.glow } : {}),
    };
    try {
      return parseSkyUltraHudLayout(candidate);
    } catch {
      return null;
    }
  }
}

function sanitizeLoadoutList(raw: unknown, fallbackCharName = ''): SkyUltraSavedLoadoutRecord[] {
  if (!Array.isArray(raw)) return [];
  const out: SkyUltraSavedLoadoutRecord[] = [];
  for (const item of raw) {
    if (!item || typeof item !== 'object') continue;
    const rec = item as Record<string, unknown>;
    const id = typeof rec.id === 'string' && rec.id.trim() ? rec.id.trim().slice(0, 64) : `loadout-${out.length + 1}`;
    const name = typeof rec.name === 'string' && rec.name.trim() ? rec.name.trim().slice(0, 64) : 'Saved Outfit';
    const characterName =
      typeof rec.characterName === 'string' && rec.characterName.trim()
        ? rec.characterName.trim().slice(0, 64)
        : fallbackCharName;
    const createdAt =
      typeof rec.createdAt === 'string' && rec.createdAt.trim() ? rec.createdAt.trim().slice(0, 40) : new Date(0).toISOString();
    const slots: Record<string, SkyUltraSavedLoadoutSlotRecord | null> = {};
    if (rec.slots && typeof rec.slots === 'object' && !Array.isArray(rec.slots)) {
      for (const [slotKey, slotVal] of Object.entries(rec.slots as Record<string, unknown>)) {
        if (slotVal === null) {
          slots[slotKey] = null;
        } else if (slotVal && typeof slotVal === 'object') {
          const sv = slotVal as Record<string, unknown>;
          if (typeof sv.id === 'string' && typeof sv.name === 'string') {
            slots[slotKey] = {
              id: sv.id.slice(0, 48),
              name: sv.name.slice(0, 96),
              category: typeof sv.category === 'string' ? sv.category.slice(0, 32) : 'Armor',
            };
          }
        }
      }
    }
    out.push({ id, name, characterName, createdAt, slots });
    if (out.length >= 24) break;
  }
  return out;
}

export function createDefaultSkyUltraProfileStore(): SkyUltraProfileStoreV2 {
  return {
    schemaVersion: SKYULTRA_PROFILE_SCHEMA_VERSION,
    mode: 'per-character',
    activeProfileId: 'global',
    globalDefaults: {
      theme: defaultSkyUltraUiPreferences.defaultTheme,
      uiPrefs: { ...defaultSkyUltraUiPreferences },
      hudLayout: null,
    },
    profiles: {},
  };
}

/** Migrate v0/unversioned, v1, or partial v2 profile state cleanly to schemaVersion 2. */
export function migrateSkyUltraProfileStore(raw: unknown): SkyUltraProfileStoreV2 {
  const base = createDefaultSkyUltraProfileStore();
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) {
    return base;
  }
  const obj = raw as Record<string, unknown>;
  const detectedVersion =
    typeof obj.schemaVersion === 'number'
      ? Math.round(obj.schemaVersion)
      : typeof obj.version === 'number'
        ? Math.round(obj.version)
        : 0;

  if (detectedVersion >= 2 && obj.globalDefaults && typeof obj.globalDefaults === 'object') {
    const gd = obj.globalDefaults as Record<string, unknown>;
    const uiPrefs = sanitizeSkyUltraUiPrefs(gd.uiPrefs);
    const theme = sanitizeThemeId(gd.theme ?? uiPrefs.defaultTheme, 'dragonstone');
    uiPrefs.defaultTheme = theme;
    const hudLayout = coerceLegacyHudLayout(gd.hudLayout, theme);
    const profiles: Record<string, SkyUltraCharacterProfile> = {};
    if (obj.profiles && typeof obj.profiles === 'object' && !Array.isArray(obj.profiles)) {
      for (const [rawKey, rawProf] of Object.entries(obj.profiles as Record<string, unknown>)) {
        if (!rawProf || typeof rawProf !== 'object' || Array.isArray(rawProf)) continue;
        const p = rawProf as Record<string, unknown>;
        const charName =
          typeof p.characterName === 'string' && p.characterName.trim()
            ? p.characterName.trim().slice(0, 64)
            : rawKey.replace(/^char\./, '');
        const id = rawKey.startsWith('char.') ? rawKey.slice(0, 48) : characterProfileIdForName(charName);
        if (id === 'global') continue;
        const profTheme = typeof p.theme === 'string' ? sanitizeThemeId(p.theme, theme) : undefined;
        const profUiPrefs =
          p.uiPrefs && typeof p.uiPrefs === 'object' && !Array.isArray(p.uiPrefs)
            ? sanitizeSkyUltraUiPrefs({ ...uiPrefs, ...(p.uiPrefs as Record<string, unknown>) })
            : undefined;
        const profHud = p.hudLayout !== undefined ? coerceLegacyHudLayout(p.hudLayout, profTheme ?? theme) : undefined;
        const loadouts = sanitizeLoadoutList(p.loadouts, charName);
        profiles[id] = {
          id,
          characterName: charName || id,
          updatedAt: typeof p.updatedAt === 'string' ? p.updatedAt : new Date(0).toISOString(),
          ...(profTheme ? { theme: profTheme } : {}),
          ...(profUiPrefs ? { uiPrefs: profUiPrefs } : {}),
          ...(profHud !== undefined ? { hudLayout: profHud } : {}),
          ...(loadouts.length > 0 ? { loadouts } : {}),
        };
        if (Object.keys(profiles).length >= 32) break;
      }
    }
    const mode: SkyUltraProfileMode = obj.mode === 'global' ? 'global' : 'per-character';
    const activeProfileId =
      typeof obj.activeProfileId === 'string' && (obj.activeProfileId === 'global' || profiles[obj.activeProfileId])
        ? obj.activeProfileId
        : 'global';
    return {
      schemaVersion: 2,
      mode,
      activeProfileId,
      globalDefaults: { theme, uiPrefs, hudLayout },
      profiles,
    };
  }

  // Legacy v0 / v1 migration path (flat preferences, v1 profile bundles, or older HUD/loadout payloads)
  const legacyUiSource =
    obj.uiPrefs && typeof obj.uiPrefs === 'object' ? obj.uiPrefs : obj.preferences && typeof obj.preferences === 'object' ? obj.preferences : obj;
  const migratedUiPrefs = sanitizeSkyUltraUiPrefs(legacyUiSource);
  const migratedTheme = sanitizeThemeId(obj.theme ?? obj.defaultTheme ?? migratedUiPrefs.defaultTheme, 'dragonstone');
  migratedUiPrefs.defaultTheme = migratedTheme;
  const migratedHud = coerceLegacyHudLayout(obj.hudLayout ?? obj.layout ?? obj.hud, migratedTheme);

  const profiles: Record<string, SkyUltraCharacterProfile> = {};
  const legacyLoadouts = sanitizeLoadoutList(obj.loadouts);
  for (const loadout of legacyLoadouts) {
    const pid = characterProfileIdForName(loadout.characterName || 'Adventurer');
    if (pid === 'global') continue;
    if (!profiles[pid]) {
      profiles[pid] = {
        id: pid,
        characterName: loadout.characterName || 'Adventurer',
        updatedAt: loadout.createdAt,
        loadouts: [],
      };
    }
    profiles[pid].loadouts = [...(profiles[pid].loadouts ?? []), loadout].slice(0, 24);
  }

  if (obj.characters && typeof obj.characters === 'object' && !Array.isArray(obj.characters)) {
    for (const [charName, rawChar] of Object.entries(obj.characters as Record<string, unknown>)) {
      if (!rawChar || typeof rawChar !== 'object') continue;
      const c = rawChar as Record<string, unknown>;
      const pid = characterProfileIdForName(charName);
      if (pid === 'global') continue;
      const cTheme = typeof c.theme === 'string' ? sanitizeThemeId(c.theme, migratedTheme) : undefined;
      const cUi = c.uiPrefs && typeof c.uiPrefs === 'object' ? sanitizeSkyUltraUiPrefs({ ...migratedUiPrefs, ...(c.uiPrefs as Record<string, unknown>) }) : undefined;
      const cHud = coerceLegacyHudLayout(c.hudLayout ?? c.layout, cTheme ?? migratedTheme);
      const cLoadouts = sanitizeLoadoutList(c.loadouts, charName);
      profiles[pid] = {
        id: pid,
        characterName: charName,
        updatedAt: new Date(0).toISOString(),
        ...(cTheme ? { theme: cTheme } : {}),
        ...(cUi ? { uiPrefs: cUi } : {}),
        ...(cHud ? { hudLayout: cHud } : {}),
        ...(cLoadouts.length ? { loadouts: cLoadouts } : profiles[pid]?.loadouts ? { loadouts: profiles[pid].loadouts } : {}),
      };
    }
  }

  return {
    schemaVersion: 2,
    mode: obj.mode === 'global' ? 'global' : 'per-character',
    activeProfileId: 'global',
    globalDefaults: {
      theme: migratedTheme,
      uiPrefs: migratedUiPrefs,
      hudLayout: migratedHud,
    },
    profiles,
    migratedFromVersion: detectedVersion,
  };
}

export function resolveEffectiveSkyUltraProfile(
  store: SkyUltraProfileStoreV2,
  characterName?: string | null,
): ResolvedSkyUltraProfile {
  const derivedId = characterProfileIdForName(characterName);
  const profileId =
    store.mode === 'per-character'
      ? derivedId !== 'global'
        ? derivedId
        : store.activeProfileId.startsWith('char.')
          ? store.activeProfileId
          : 'global'
      : 'global';
  const charRecord =
    profileId !== 'global'
      ? store.profiles[profileId]
      : derivedId !== 'global'
        ? store.profiles[derivedId]
        : undefined;

  const isCharacterScoped = store.mode === 'per-character' && profileId !== 'global';
  const hasCharacterOverrides = Boolean(
    charRecord && (charRecord.theme !== undefined || charRecord.uiPrefs !== undefined || charRecord.hudLayout !== undefined),
  );

  const effectiveUiPrefs = sanitizeSkyUltraUiPrefs({
    ...store.globalDefaults.uiPrefs,
    ...(isCharacterScoped && charRecord?.uiPrefs ? charRecord.uiPrefs : {}),
  });
  const effectiveTheme = sanitizeThemeId(
    isCharacterScoped && charRecord?.theme ? charRecord.theme : store.globalDefaults.theme,
    effectiveUiPrefs.defaultTheme,
  );
  effectiveUiPrefs.defaultTheme = effectiveTheme;
  const effectiveHud =
    isCharacterScoped && charRecord?.hudLayout !== undefined ? charRecord.hudLayout : store.globalDefaults.hudLayout;

  const allLoadouts: SkyUltraSavedLoadoutRecord[] = [];
  for (const prof of Object.values(store.profiles)) {
    for (const item of prof.loadouts ?? []) {
      allLoadouts.push(item);
    }
  }

  return {
    profileId,
    characterName: charRecord?.characterName || (characterName?.trim() ?? 'Global Default'),
    isCharacterScoped,
    hasCharacterOverrides,
    theme: effectiveTheme,
    uiPrefs: effectiveUiPrefs,
    hudLayout: effectiveHud ?? null,
    loadouts: allLoadouts,
  };
}

export interface SkyUltraProfilePatch {
  theme?: string;
  uiPrefs?: Partial<SkyUltraUiPreferences>;
  hudLayout?: SkyUltraHudLayout | null;
  loadouts?: SkyUltraSavedLoadoutRecord[];
  mode?: SkyUltraProfileMode;
  activeProfileId?: string;
}

export function updateProfileForContext(
  store: SkyUltraProfileStoreV2,
  characterName: string | null | undefined,
  patch: SkyUltraProfilePatch,
): SkyUltraProfileStoreV2 {
  const nextMode = patch.mode ?? store.mode;
  const derivedCharId = characterProfileIdForName(characterName);
  const targetId =
    patch.activeProfileId ??
    (nextMode === 'per-character' && derivedCharId !== 'global' ? derivedCharId : store.activeProfileId);

  const nextProfiles: Record<string, SkyUltraCharacterProfile> = { ...store.profiles };
  let nextGlobal: SkyUltraGlobalDefaults = {
    theme: store.globalDefaults.theme,
    uiPrefs: { ...store.globalDefaults.uiPrefs },
    hudLayout: store.globalDefaults.hudLayout,
  };

  if (patch.loadouts !== undefined) {
    // Partition loadouts per character profile so outfit presets stay cleanly scoped
    const grouped = new Map<string, { charName: string; list: SkyUltraSavedLoadoutRecord[] }>();
    for (const entry of sanitizeLoadoutList(patch.loadouts, characterName ?? 'Adventurer')) {
      const pid = characterProfileIdForName(entry.characterName || characterName || 'Adventurer');
      const key = pid === 'global' ? 'char.adventurer' : pid;
      const bucket = grouped.get(key) ?? { charName: entry.characterName || characterName || 'Adventurer', list: [] };
      bucket.list.push(entry);
      grouped.set(key, bucket);
    }
    for (const [pid, prof] of Object.entries(nextProfiles)) {
      nextProfiles[pid] = { ...prof, loadouts: grouped.get(pid)?.list ?? [] };
    }
    for (const [pid, bucket] of grouped.entries()) {
      if (!nextProfiles[pid]) {
        nextProfiles[pid] = {
          id: pid,
          characterName: bucket.charName,
          updatedAt: new Date().toISOString(),
          loadouts: bucket.list,
        };
      }
    }
  }

  const editingCharacter = nextMode === 'per-character' && targetId.startsWith('char.');
  if (editingCharacter) {
    const existing = nextProfiles[targetId];
    const baseUi = sanitizeSkyUltraUiPrefs({
      ...nextGlobal.uiPrefs,
      ...(existing?.uiPrefs ?? {}),
      ...(patch.uiPrefs ?? {}),
    });
    const nextTheme = patch.theme
      ? sanitizeThemeId(patch.theme, baseUi.defaultTheme)
      : patch.uiPrefs?.defaultTheme
        ? sanitizeThemeId(patch.uiPrefs.defaultTheme, baseUi.defaultTheme)
        : existing?.theme ?? nextGlobal.theme;
    baseUi.defaultTheme = nextTheme;
    nextProfiles[targetId] = {
      id: targetId,
      characterName: existing?.characterName || characterName?.trim() || targetId.replace(/^char\./, ''),
      updatedAt: new Date().toISOString(),
      theme: nextTheme,
      uiPrefs: baseUi,
      hudLayout: patch.hudLayout !== undefined ? patch.hudLayout : existing?.hudLayout ?? nextGlobal.hudLayout,
      loadouts: nextProfiles[targetId]?.loadouts ?? existing?.loadouts ?? [],
    };
  } else if (patch.theme !== undefined || patch.uiPrefs !== undefined || patch.hudLayout !== undefined) {
    const mergedUi = sanitizeSkyUltraUiPrefs({
      ...nextGlobal.uiPrefs,
      ...(patch.uiPrefs ?? {}),
    });
    const nextTheme = patch.theme
      ? sanitizeThemeId(patch.theme, mergedUi.defaultTheme)
      : patch.uiPrefs?.defaultTheme
        ? sanitizeThemeId(patch.uiPrefs.defaultTheme, mergedUi.defaultTheme)
        : nextGlobal.theme;
    mergedUi.defaultTheme = nextTheme;
    nextGlobal = {
      theme: nextTheme,
      uiPrefs: mergedUi,
      hudLayout: patch.hudLayout !== undefined ? patch.hudLayout : nextGlobal.hudLayout,
    };
  }

  return {
    schemaVersion: 2,
    mode: nextMode,
    activeProfileId: editingCharacter ? targetId : 'global',
    globalDefaults: nextGlobal,
    profiles: nextProfiles,
  };
}

export function promoteProfileToGlobalDefaults(
  store: SkyUltraProfileStoreV2,
  characterName?: string | null,
): SkyUltraProfileStoreV2 {
  const resolved = resolveEffectiveSkyUltraProfile(store, characterName);
  return {
    ...store,
    globalDefaults: {
      theme: resolved.theme,
      uiPrefs: { ...resolved.uiPrefs, defaultTheme: resolved.theme },
      hudLayout: resolved.hudLayout,
    },
  };
}

export function resetSkyUltraProfile(
  store: SkyUltraProfileStoreV2,
  scope: 'character' | 'all',
  characterName?: string | null,
): SkyUltraProfileStoreV2 {
  if (scope === 'all') {
    return createDefaultSkyUltraProfileStore();
  }
  const pid = characterProfileIdForName(characterName);
  const target = pid !== 'global' ? pid : store.activeProfileId;
  if (!target.startsWith('char.') || !store.profiles[target]) {
    return store;
  }
  const existing = store.profiles[target];
  const nextProfiles = { ...store.profiles };
  if (existing.loadouts && existing.loadouts.length > 0) {
    nextProfiles[target] = {
      id: existing.id,
      characterName: existing.characterName,
      updatedAt: new Date().toISOString(),
      loadouts: existing.loadouts,
    };
  } else {
    delete nextProfiles[target];
  }
  return {
    ...store,
    profiles: nextProfiles,
  };
}

export function exportSkyUltraProfileBundle(store: SkyUltraProfileStoreV2): string {
  return JSON.stringify(
    {
      ...migrateSkyUltraProfileStore(store),
      exportedAt: new Date().toISOString(),
    },
    null,
    2,
  );
}

export function importSkyUltraProfileBundle(jsonText: string): SkyUltraProfileStoreV2 {
  const parsed: unknown = JSON.parse(jsonText);
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    throw new Error('Expected a JSON profile bundle object.');
  }
  return migrateSkyUltraProfileStore(parsed);
}

export function readLocalSkyUltraProfileStore(): SkyUltraProfileStoreV2 {
  try {
    if (typeof window === 'undefined' || !window.localStorage) {
      return createDefaultSkyUltraProfileStore();
    }
    const raw = window.localStorage.getItem(PROFILE_STORE_LOCAL_KEY);
    if (raw) {
      return migrateSkyUltraProfileStore(JSON.parse(raw));
    }
    // Seed from existing v1/v2 local keys on first upgrade!
    const legacyUi = readSkyUltraUiPrefs();
    let legacyHud: SkyUltraHudLayout | null = null;
    const rawHud = window.localStorage.getItem(LEGACY_HUD_STUDIO_KEY);
    if (rawHud) {
      legacyHud = coerceLegacyHudLayout(JSON.parse(rawHud), legacyUi.defaultTheme);
    }
    let legacyLoadouts: unknown = [];
    const rawLoadouts = window.localStorage.getItem(LEGACY_LOADOUTS_KEY);
    if (rawLoadouts) {
      legacyLoadouts = JSON.parse(rawLoadouts);
    }
    return migrateSkyUltraProfileStore({
      version: 1,
      theme: legacyUi.defaultTheme,
      uiPrefs: legacyUi,
      hudLayout: legacyHud,
      loadouts: legacyLoadouts,
    });
  } catch {
    return createDefaultSkyUltraProfileStore();
  }
}

export function writeLocalSkyUltraProfileStore(
  store: SkyUltraProfileStoreV2,
  characterName?: string | null,
): SkyUltraProfileStoreV2 {
  const normalized = migrateSkyUltraProfileStore(store);
  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      window.localStorage.setItem(PROFILE_STORE_LOCAL_KEY, JSON.stringify(normalized));
      const effective = resolveEffectiveSkyUltraProfile(normalized, characterName);
      writeSkyUltraUiPrefs(effective.uiPrefs);
      if (effective.hudLayout) {
        window.localStorage.setItem(
          LEGACY_HUD_STUDIO_KEY,
          JSON.stringify({
            widgets: effective.hudLayout.widgets,
            placements: effective.hudLayout.placements ?? {},
            theme: effective.hudLayout.theme,
            snapGrid: effective.hudLayout.snapGrid ?? 8,
          }),
        );
      }
      window.localStorage.setItem(LEGACY_LOADOUTS_KEY, JSON.stringify(effective.loadouts));
      window.dispatchEvent(new CustomEvent('skyultra:profile-store', { detail: normalized }));
    }
  } catch {
    // Ignore storage restrictions
  }
  return normalized;
}

export async function loadSkyUltraProfileStore(
  bridge: SkyWebViewAPI = getSkyWebView(),
  characterName?: string | null,
): Promise<SkyUltraProfileStoreV2> {
  const local = readLocalSkyUltraProfileStore();
  try {
    const settings = await readSkyUltraSettings(bridge);
    if (settings.profileState && Object.keys(settings.profileState).length > 0) {
      const migrated = migrateSkyUltraProfileStore(settings.profileState);
      return writeLocalSkyUltraProfileStore(migrated, characterName);
    }
  } catch {
    // Preview or restricted bridge fallback
  }
  return writeLocalSkyUltraProfileStore(local, characterName);
}

export async function persistSkyUltraProfileStore(
  store: SkyUltraProfileStoreV2,
  bridge: SkyWebViewAPI = getSkyWebView(),
  characterName?: string | null,
): Promise<SkyUltraProfileStoreV2> {
  const normalized = writeLocalSkyUltraProfileStore(store, characterName);
  try {
    await saveSkyUltraHostProfileState(normalized as unknown as Record<string, unknown>, bridge);
  } catch {
    // Preview mock or restricted view fallback keeps local mirror intact
  }
  return normalized;
}
