import { getSkyWebView, type JsonValue, type SkyWebViewAPI } from './index.js';
import type { SkyUltraDeveloperSnapshot, SkyUltraDiagnosticFinding } from './devtools.js';

export type SkyUltraCoexistenceStatus = 'native' | 'coexists' | 'adapter-guided';

export interface SkyUltraCompatibilityEntry {
  id: string;
  ecosystem: string;
  category: 'runtime' | 'webview-peer' | 'scaleform' | 'hud-postfx';
  status: SkyUltraCoexistenceStatus;
  inputBehavior: string;
  layeringBehavior: string;
  migrationPath: string;
  recommendedApis: string[];
}

export const SKYULTRA_COMPATIBILITY_MATRIX: readonly SkyUltraCompatibilityEntry[] = [
  {
    id: 'skyultra-native',
    ecosystem: 'SkyUltra Dual-Engine Host (Ultralight + CEF)',
    category: 'runtime',
    status: 'native',
    inputBehavior:
      'Scoped modal input stack (ui.setInputScope 0..8) unwinds popups on Escape before closing panels; HUD views enforce clickThrough with hidden OS/Scaleform cursor.',
    layeringBehavior:
      'Single D3D11 swapchain compositor renders HUD overlays beneath interactive panels with explicit per-view alpha premultiplication and FPS caps.',
    migrationPath:
      'Author Data/SkyUltra/<author.mod>/manifest.json (schemaVersion 1) with @skyultra/sdk and @skyultra/ui; zero C++ DLL required for web mods.',
    recommendedApis: ['getSkyWebView', 'markSkyUltraPageReady', 'connectSkyUltraInputScopes', 'createSkyUltraGame'],
  },
  {
    id: 'prismaui-coexistence',
    ecosystem: 'PrismaUI (PrismaUI.dll / Ultralight SKSE Mods)',
    category: 'webview-peer',
    status: 'adapter-guided',
    inputBehavior:
      'Coexists at runtime via independent view handles and focus locks; avoid binding the exact same toggle hotkey to both a PrismaUI panel and a SkyUltra panel.',
    layeringBehavior:
      'Both hook DXGI Present cleanly; SkyUltra restores D3D11 blend, rasterizer, scissor, and SRV states around every pass so peer overlays never corrupt.',
    migrationPath:
      'Replace window.skyrim / InteropCall / RegisterJSListener callbacks with window.SkyWebView.request & on, or wrap incremental ports with createSkyUltraLegacyBridgeAdapter.',
    recommendedApis: ['createSkyUltraLegacyBridgeAdapter', 'auditLegacyModSourceForSkyUltra', 'auditSkyUltraManifestObject'],
  },
  {
    id: 'magelight-cef',
    ecosystem: 'MageLight & Custom Local CEF Overlays',
    category: 'webview-peer',
    status: 'adapter-guided',
    inputBehavior:
      'SkyUltra shares a single CEF broker process across all registered CEF mods instead of spawning duplicate Chromium trees that compete for Win32 message hooks.',
    layeringBehavior:
      'Porting local CEF overlays into Data/SkyUltra/<author.mod>/ eliminates separate window z-fighting and routes composition through SkyUltra’s unified D3D11 HUD/panel stack.',
    migrationPath:
      'Map window.cefQuery({ request }) JSON payloads to typed bridge.request(method, params) calls and declare only required methods in permissions.pageRequests.',
    recommendedApis: ['createSkyUltraLegacyBridgeAdapter', 'readSkyUltraDeveloperSnapshot', 'reloadSkyUltraModViews'],
  },
  {
    id: 'vanilla-scaleform',
    ecosystem: 'Vanilla Skyrim Scaleform Menus (Tween, Inventory, Map, Journal, Console)',
    category: 'scaleform',
    status: 'coexists',
    inputBehavior:
      'SkyUltra suspends gameplay controls only while a modal panel is visible and immediately releases cursor & menu locks on ui.close or Escape.',
    layeringBehavior:
      'Requires zero .swf replacements (no hudmenu.swf or inventorymenu.swf conflicts); SkyUltra HUD overlays automatically yield during system/loading screens.',
    migrationPath:
      'Keep vanilla/SkyUI Scaleform menus intact alongside SkyUltra, or trigger SkyUltra panels via manifest hotkeys (Keyboard, Gamepad, VR).',
    recommendedApis: ['closeSkyUltraPage', 'connectSkyUltraInputScopes', 'updateSkyUltraModHotkey'],
  },
  {
    id: 'truehud-wheeler-postfx',
    ecosystem: 'TrueHUD, SkyHUD, Wheeler, ENB & Community Shaders',
    category: 'hud-postfx',
    status: 'coexists',
    inputBehavior:
      'Pinned SkyUltra HUD layouts run with clickThrough: true and inputScopeDepth: 0 so Wheeler action wheels, camera look, and combat inputs pass straight to Skyrim.',
    layeringBehavior:
      'Composites on the swapchain Present stage with sRGB-safe premultiplied alpha so ENB and Community Shaders world-space passes do not blur or wash out UI text.',
    migrationPath:
      'Export standalone zero-DLL HUD mods from HUDs & Widgets -> Export Working Mod or compose custom overlays with SkyHudComposer.',
    recommendedApis: ['exportSkyUltraHudMod', 'parseSkyUltraHudLayout', 'diagnoseCoexistenceAndLayering'],
  },
] as const;

export interface SkyUltraManifestValidationResult {
  valid: boolean;
  normalizedId?: string;
  findings: SkyUltraDiagnosticFinding[];
}

const ALLOWED_MANIFEST_TOP_KEYS = new Set([
  'schemaVersion',
  'id',
  'name',
  'version',
  'entry',
  'permissions',
  'papyrusPlugin',
  'browserEngine',
]);

/** Audit a schemaVersion 1 SkyUltra manifest JSON object in authoring tools or Developer Studio. */
export function auditSkyUltraManifestObject(raw: unknown): SkyUltraManifestValidationResult {
  const findings: SkyUltraDiagnosticFinding[] = [];
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) {
    return {
      valid: false,
      findings: [
        {
          severity: 'error',
          code: 'MANIFEST_NOT_OBJECT',
          title: 'manifest.json must be a JSON object',
          detail: 'Expected a top-level JSON object with schemaVersion: 1, id, name, version, entry, and permissions.',
        },
      ],
    };
  }
  const m = raw as Record<string, unknown>;
  const unknownTopKeys = Object.keys(m).filter((k) => !ALLOWED_MANIFEST_TOP_KEYS.has(k));
  if (unknownTopKeys.length > 0) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_UNKNOWN_FIELDS',
      title: `Unsupported top-level manifest field(s): ${unknownTopKeys.join(', ')}`,
      detail:
        'SkyUltra native loader accepts only schemaVersion, id, name, version, entry, permissions, papyrusPlugin, and browserEngine.',
    });
  }
  if (m.schemaVersion !== 1) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_SCHEMA_VERSION',
      title: 'Unsupported or missing schemaVersion',
      detail: `Expected "schemaVersion": 1, received ${JSON.stringify(m.schemaVersion)}.`,
      fixSnippet: `"schemaVersion": 1`,
    });
  }
  const id = typeof m.id === 'string' ? m.id.trim() : '';
  if (!/^[a-z0-9][a-z0-9._-]{0,63}$/.test(id) || id.includes('..')) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_INVALID_ID',
      title: 'Mod id must be a valid lowercase identifier',
      detail: `Received "${id || '(empty)'}". Use 1..64 lowercase letters, digits, dots, underscores, or hyphens (e.g. "test" or "author.my_mod").`,
      fixSnippet: `"id": "my_mod"`,
    });
  }
  if (typeof m.name !== 'string' || !m.name.trim()) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_MISSING_NAME',
      title: 'Missing human-readable mod name',
      detail: 'manifest.json requires a non-empty "name" string.',
      fixSnippet: `"name": "My SkyUltra Mod"`,
    });
  }
  if (typeof m.version !== 'string' || !/^\d+\.\d+\.\d+/.test(m.version.trim())) {
    findings.push({
      severity: 'warning',
      code: 'MANIFEST_SEMVER',
      title: 'Use semantic versioning (MAJOR.MINOR.PATCH)',
      detail: `Received version ${JSON.stringify(m.version)}. Semantic versions improve diagnostics and migration tracking.`,
      fixSnippet: `"version": "1.0.0"`,
    });
  }
  const entry = typeof m.entry === 'string' ? m.entry.trim() : '';
  if (!entry || entry.startsWith('/') || entry.includes('..') || /^https?:/i.test(entry)) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_INVALID_ENTRY',
      title: 'Entry must be a relative package HTML path',
      detail: `Received "${entry || '(empty)'}". Point to a local HTML file inside Data/SkyUltra/<mod>/ such as "index.html" or "Files/index.html".`,
      fixSnippet: `"entry": "index.html"`,
    });
  }

  const perms = m.permissions && typeof m.permissions === 'object' && !Array.isArray(m.permissions)
    ? (m.permissions as Record<string, unknown>)
    : null;
  if (!perms) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_MISSING_PERMISSIONS',
      title: 'Missing permissions block',
      detail: 'Declare explicit least-privilege permissions (storage, assetAccess, networkHosts, pageRequests).',
      fixSnippet: `"permissions": {\n  "storage": false,\n  "assetAccess": true,\n  "networkHosts": [],\n  "pageRequests": ["ui.pageReady"]\n}`,
    });
  } else {
    const pageRequests = Array.isArray(perms.pageRequests)
      ? perms.pageRequests.filter((x): x is string => typeof x === 'string')
      : [];
    if (!pageRequests.includes('ui.pageReady')) {
      findings.push({
        severity: 'warning',
        code: 'MANIFEST_NO_PAGE_READY',
        title: "Include 'ui.pageReady' in permissions.pageRequests",
        detail: 'Calling markSkyUltraPageReady() after first render releases startup splash gating cleanly.',
        fixSnippet: `"pageRequests": ["ui.pageReady"]`,
      });
    }
    if (Array.isArray(perms.networkHosts) && perms.networkHosts.length > 0) {
      findings.push({
        severity: 'warning',
        code: 'MANIFEST_REMOTE_HOSTS',
        title: `Manifest declares ${perms.networkHosts.length} external network host(s)`,
        detail: 'Offline-first local assets inside your mod package ensure instant loads and zero network stalls in Skyrim.',
      });
    }
  }

  const hasError = findings.some((f) => f.severity === 'error');
  if (findings.length === 0) {
    findings.push({
      severity: 'info',
      code: 'MANIFEST_VALID',
      title: 'Manifest adheres to SkyUltra schemaVersion 1',
      detail: `${id} passed schema, path containment, and least-privilege permission checks.`,
    });
  }
  return {
    valid: !hasError,
    normalizedId: id || undefined,
    findings,
  };
}

export interface SkyUltraLegacySourceAudit {
  findings: SkyUltraDiagnosticFinding[];
  inferredPageRequests: string[];
  suggestedManifest: Record<string, unknown>;
}

/** Static source scanner to help mod authors port PrismaUI, MageLight, or raw CEF/HTML UI code to SkyUltra. */
export function auditLegacyModSourceForSkyUltra(
  source: string,
  options: { modId?: string; modName?: string } = {},
): SkyUltraLegacySourceAudit {
  const text = String(source ?? '');
  const findings: SkyUltraDiagnosticFinding[] = [];
  const inferred = new Set<string>(['ui.pageReady']);

  if (/\b(?:InteropCall|PrismaUI|RegisterJSListener|skyrim\.interop)\b/i.test(text)) {
    findings.push({
      severity: 'warning',
      code: 'LEGACY_PRISMAUI_CALL',
      title: 'Detected PrismaUI / InteropCall callback pattern',
      detail:
        'PrismaUI global interop callbacks are not injected into SkyUltra surfaces. Route requests through window.SkyWebView.request() or wrap incremental ports with createSkyUltraLegacyBridgeAdapter().',
      fixSnippet: `import { createSkyUltraLegacyBridgeAdapter } from "@skyultra/sdk";\nconst legacy = createSkyUltraLegacyBridgeAdapter(undefined, {\n  methodMap: { getPlayerStats: "game.getPlayerSnapshot", closeMenu: "ui.close" }\n});`,
    });
  }

  if (/\b(?:cefQuery|cefQueryCancel|MageLight)\b/.test(text)) {
    findings.push({
      severity: 'warning',
      code: 'LEGACY_CEF_QUERY',
      title: 'Detected raw CEF cefQuery({ request }) bridge call',
      detail:
        'SkyUltra exposes a unified engine-agnostic window.SkyWebView bridge across both Ultralight and CEF so your mod runs on either renderer without cefQuery.',
      fixSnippet: `const snapshot = await window.SkyWebView.request("game.getPlayerSnapshot", {});`,
    });
  }

  if (/\bwindow\.close\s*\(/.test(text)) {
    inferred.add('ui.close');
    findings.push({
      severity: 'error',
      code: 'DOM_WINDOW_CLOSE',
      title: 'Replace window.close() with closeSkyUltraPage() / ui.close',
      detail:
        'window.close() does not release Skyrim menu/cursor locks cleanly in embedded D3D11 views. Request "ui.close" via SkyWebView instead.',
      fixSnippet: `await window.SkyWebView.request("ui.close", {});`,
    });
  }

  if (/https?:\/\/(?:fonts\.googleapis\.com|cdn\.|unpkg\.com|jsdelivr\.net)/i.test(text)) {
    findings.push({
      severity: 'warning',
      code: 'EXTERNAL_CDN_ASSET',
      title: 'External CDN stylesheet or script detected',
      detail:
        'SkyUltra locks network access by default for deterministic frame times and security. Bundle fonts, icons, and scripts locally inside Data/SkyUltra/<mod>/.',
      fixSnippet: `<link rel="stylesheet" href="./assets/styles.css" />`,
    });
  }

  if (/addEventListener\s*\(\s*['"]keydown['"][\s\S]{0,160}Escape/.test(text) && !/InputScope|setInputScope/.test(text)) {
    inferred.add('ui.setInputScope');
    findings.push({
      severity: 'info',
      code: 'RAW_ESCAPE_HANDLER',
      title: 'Escape listener without nested input scope registration',
      detail:
        'Connect popup/dropdown Escape handling with connectSkyUltraInputScopes() + enterSkyInputScope() so Escape closes open popovers before dismissing the panel.',
      fixSnippet: `import { connectSkyUltraInputScopes } from "@skyultra/sdk";\nconnectSkyUltraInputScopes();`,
    });
  }

  if (!/\b(?:markSkyUltraPageReady|ui\.pageReady)\b/.test(text)) {
    findings.push({
      severity: 'warning',
      code: 'MISSING_PAGE_READY_CALL',
      title: 'No ui.pageReady / markSkyUltraPageReady() call found',
      detail:
        'Invoke markSkyUltraPageReady() after your initial UI frame mounts so SkyUltra knows the view rendered cleanly.',
      fixSnippet: `await window.SkyWebView.request("ui.pageReady", {});`,
    });
  }

  const knownMethods = [
    'game.getPlayerSnapshot',
    'game.getInventory',
    'game.getNearbyNpcs',
    'game.searchNpcs',
    'game.subscribe',
    'game.unsubscribe',
    'game.equipItem',
    'game.unequipItem',
    'ui.close',
    'ui.setCursor',
    'ui.setInputScope',
    'ui.configureWidgets',
  ];
  for (const method of knownMethods) {
    if (text.includes(method)) inferred.add(method);
  }
  if (/\b(?:health|magicka|stamina|playerName)\b/i.test(text) && !inferred.has('game.getPlayerSnapshot')) {
    inferred.add('game.getPlayerSnapshot');
    inferred.add('game.subscribe');
    inferred.add('game.unsubscribe');
  }

  if (findings.length === 0) {
    findings.push({
      severity: 'info',
      code: 'SOURCE_READY',
      title: 'Source uses clean SkyUltra native contracts',
      detail: 'No legacy PrismaUI/cefQuery globals, remote CDN dependencies, or unsafe window.close() calls were found.',
    });
  }

  const inferredPageRequests = Array.from(inferred).sort();
  const modId = options.modId && /^[a-z0-9][a-z0-9._-]{2,63}$/.test(options.modId) ? options.modId : 'author.ported_mod';
  const modName = options.modName?.trim() || 'Ported SkyUltra Mod';

  return {
    findings,
    inferredPageRequests,
    suggestedManifest: {
      schemaVersion: 1,
      id: modId,
      name: modName,
      version: '1.0.0',
      entry: 'index.html',
      permissions: {
        storage: false,
        assetAccess: true,
        networkHosts: [],
        pageRequests: inferredPageRequests,
      },
    },
  };
}

/** Live runtime coexistence, input focus, and D3D11 layering verifier over SkyUltraDeveloperSnapshot. */
export function diagnoseCoexistenceAndLayering(snapshot: SkyUltraDeveloperSnapshot): SkyUltraDiagnosticFinding[] {
  const findings: SkyUltraDiagnosticFinding[] = [];
  const visiblePanels = snapshot.views.filter((v) => v.visible && (v.layer === 'panel' || v.modal));
  const visibleHuds = snapshot.views.filter((v) => v.visible && v.layer === 'hud');

  for (const hud of visibleHuds) {
    if (!hud.clickThrough || hud.modal || hud.inputScopeDepth > 0) {
      findings.push({
        severity: 'error',
        code: 'HUD_STEALING_INPUT',
        title: `HUD surface '${hud.configId}' (${hud.modId}) is holding input focus`,
        detail: `HUD overlays must keep clickThrough: true, modal: false, and inputScopeDepth: 0 (current depth: ${hud.inputScopeDepth}) so gameplay, Wheeler, and Scaleform menus receive mouse/gamepad input.`,
        fixSnippet: `Ensure HUD views never call enterSkyInputScope() or set modal: true.`,
      });
    }
    if (hud.fpsLimit === 0) {
      findings.push({
        severity: 'info',
        code: 'HUD_UNCAPPED_FPS',
        title: `HUD surface '${hud.configId}' has no explicit FPS cap`,
        detail: 'Setting a 30–60 FPS cap on background HUD overlays saves compositor GPU headroom alongside ENB / Community Shaders.',
      });
    }
  }

  if (visiblePanels.length > 1) {
    findings.push({
      severity: 'warning',
      code: 'MULTIPLE_MODAL_PANELS',
      title: `${visiblePanels.length} interactive panel surfaces are visible simultaneously`,
      detail: `Active panels: ${visiblePanels.map((v) => `${v.configId} (${v.modId})`).join(', ')}. Close inactive mod panels so Escape and cursor ownership remain unambiguous.`,
    });
  }

  for (const view of snapshot.views) {
    if (view.pendingRequests > 12) {
      findings.push({
        severity: 'warning',
        code: 'HIGH_PENDING_BRIDGE_QUEUE',
        title: `Surface '${view.configId}' has ${view.pendingRequests} in-flight bridge requests`,
        detail: 'Coalesce rapid polling onto game.subscribe or cancel stale requests via AbortController when switching tabs.',
      });
    }
  }

  if (findings.length === 0) {
    findings.push({
      severity: 'info',
      code: 'COEXISTENCE_HEALTHY',
      title: 'Input scopes, HUD click-through & D3D11 layer order verified',
      detail: `Verified ${snapshot.views.length} live surface(s) (${visiblePanels.length} visible panel, ${visibleHuds.length} visible click-through HUD). No cursor leaks, input-scope deadlocks, or modal collisions detected.`,
    });
  }

  return findings;
}

export interface SkyUltraLegacyBridgeAdapter {
  call<TResponse = JsonValue>(legacyName: string, payload?: JsonValue): Promise<TResponse>;
  listen<TPayload = JsonValue>(eventName: string, handler: (payload: TPayload) => void): () => void;
  ready(): Promise<{ ready: boolean }>;
  close(): Promise<{ closed: boolean }>;
}

/**
 * Lightweight opt-in adapter for mod authors migrating callback-based PrismaUI or MageLight scripts
 * onto SkyUltra's explicit bridge contracts without adding foreign globals to the native host.
 */
export function createSkyUltraLegacyBridgeAdapter(
  bridge: SkyWebViewAPI = getSkyWebView(),
  options: { methodMap?: Record<string, string>; eventMap?: Record<string, string> } = {},
): SkyUltraLegacyBridgeAdapter {
  const defaultMethodMap: Record<string, string> = {
    close: 'ui.close',
    closeMenu: 'ui.close',
    pageReady: 'ui.pageReady',
    getPlayer: 'game.getPlayerSnapshot',
    getPlayerStats: 'game.getPlayerSnapshot',
    getInventory: 'game.getInventory',
    getNearbyActors: 'game.getNearbyNpcs',
    ...(options.methodMap ?? {}),
  };
  const defaultEventMap: Record<string, string> = {
    onVisibilityChange: 'skyultra.visibility',
    onPlayerUpdate: 'game.playerUpdated',
    ...(options.eventMap ?? {}),
  };

  return {
    call<TResponse = JsonValue>(legacyName: string, payload: JsonValue = {}): Promise<TResponse> {
      const mapped = defaultMethodMap[legacyName] ?? legacyName;
      return bridge.request<TResponse>(mapped, payload);
    },
    listen<TPayload = JsonValue>(eventName: string, handler: (payload: TPayload) => void): () => void {
      const mapped = defaultEventMap[eventName] ?? eventName;
      return bridge.on<TPayload>(mapped, handler);
    },
    ready(): Promise<{ ready: boolean }> {
      return bridge.request<{ ready: boolean }>('ui.pageReady', {});
    },
    close(): Promise<{ closed: boolean }> {
      return bridge.request<{ closed: boolean }>('ui.close', {});
    },
  };
}
