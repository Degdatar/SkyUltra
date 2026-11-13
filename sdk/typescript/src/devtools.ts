import { getSkyWebView, type JsonValue, type SkyUltraRequestOptions, type SkyWebViewAPI } from './index.js';

export const skyUltraDevToolsMethods = [
  'ui.getDeveloperSnapshot',
  'ui.reloadModViews',
  'ui.exportHudMod',
  'ui.manageDiagnosticSample',
] as const;

export type SkyUltraDevToolsMethod = (typeof skyUltraDevToolsMethods)[number];

export interface SkyUltraDiagnosticFinding {
  severity: 'error' | 'warning' | 'info';
  code: string;
  title: string;
  detail: string;
  fixSnippet?: string;
}

export interface SkyUltraDeveloperViewRecord {
  handle: number;
  viewId: number;
  configId: string;
  modId: string;
  engine: string;
  layer: 'hud' | 'panel';
  visible: boolean;
  transparent: boolean;
  clickThrough: boolean;
  modal: boolean;
  width: number;
  height: number;
  fpsLimit: number;
  inputScopeDepth: number;
  pendingRequests: number;
  surfaceBytes: number;
  allowedMethods: string[];
  initialUrl: string;
}

export interface SkyUltraDeveloperBridgeEntry {
  seq: number;
  timeMs: number;
  viewId: number;
  modId: string;
  kind: 'request' | 'event' | 'cancel' | 'permission-denied' | 'error';
  method: string;
  status: 'ok' | 'error' | 'denied' | 'pending';
  durationMs: number;
  payloadBytes: number;
  message?: string;
}

export interface SkyUltraDeveloperModRecord {
  id: string;
  name: string;
  version: string;
  folderName: string;
  registered: boolean;
  manifestValid: boolean;
  manifestError?: string;
  entry: string;
  entryExists: boolean;
  hudExists: boolean;
  assetCount: number;
  totalAssetBytes: number;
  missingAssets: string[];
  storage: boolean;
  assetAccess: boolean;
  networkHosts: string[];
  pageRequests: string[];
  activeViewCount: number;
  estimatedSurfaceBytes: number;
  bridgeRequestCount: number;
  bridgeErrorCount: number;
  deniedPermissionCount: number;
  hotReloadCount: number;
  findings: SkyUltraDiagnosticFinding[];
}

export interface SkyUltraDeveloperSnapshot {
  available: boolean;
  capturedAtMs: number;
  processWorkingSetBytes: number;
  processPrivateBytes: number;
  hotReloadEnabled: boolean;
  hotReloadTotalCount: number;
  lastReloadedMod: string;
  hostGpuMs: number;
  compositorGpuMs: number;
  callbackCpuMs: number;
  frameCount: number;
  activeSubscriptionCount: number;
  views: SkyUltraDeveloperViewRecord[];
  mods: SkyUltraDeveloperModRecord[];
  bridgeLog: SkyUltraDeveloperBridgeEntry[];
}

export interface SkyUltraReloadModResult {
  reloaded: boolean;
  reloadedViewCount: number;
  hotReloadEnabled: boolean;
  message?: string;
}

export interface SkyUltraDiagnosticSampleResult {
  accepted: boolean;
  action: 'deploy-broken' | 'repair-and-reload' | 'remove';
  modId: string;
  viewHandle?: number;
  message?: string;
}

/** Pure diagnostic analyzer for mod records + recent bridge entries (usable both client-side and in tests). */
export function diagnoseSkyUltraModRecord(
  mod: Omit<SkyUltraDeveloperModRecord, 'findings'>,
  bridgeLog: readonly SkyUltraDeveloperBridgeEntry[] = [],
): SkyUltraDiagnosticFinding[] {
  const findings: SkyUltraDiagnosticFinding[] = [];
  if (!mod.manifestValid) {
    findings.push({
      severity: 'error',
      code: 'MANIFEST_INVALID',
      title: 'Manifest schema validation failed',
      detail: mod.manifestError || `Data/SkyUltra/${mod.folderName}/manifest.json could not be parsed as schemaVersion 1.`,
      fixSnippet: `{\n  "schemaVersion": 1,\n  "id": "${mod.folderName.replace(/[^a-z0-9._-]/gi, '_').toLowerCase()}",\n  "name": "${mod.name || mod.folderName}",\n  "version": "0.1.0",\n  "entry": "index.html",\n  "permissions": {\n    "storage": false,\n    "assetAccess": true,\n    "networkHosts": [],\n    "pageRequests": ["ui.pageReady"]\n  }\n}`,
    });
    return findings;
  }

  if (!mod.entryExists) {
    findings.push({
      severity: 'error',
      code: 'ENTRY_MISSING',
      title: `Missing entry document '${mod.entry}'`,
      detail: `manifest.json points to '${mod.entry}', but that file does not exist inside Data/SkyUltra/${mod.folderName}/.`,
      fixSnippet: `"entry": "index.html"`,
    });
  }

  for (const missing of mod.missingAssets) {
    findings.push({
      severity: 'error',
      code: 'ASSET_MISSING',
      title: `Broken package asset reference '${missing}'`,
      detail: `HTML/CSS inside '${mod.id}' references '${missing}', which is missing from the mod package.`,
      fixSnippet: `Verify '${missing}' is copied into public/ or dist/ during vite build.`,
    });
  }

  if (!mod.pageRequests.includes('ui.pageReady')) {
    findings.push({
      severity: 'warning',
      code: 'MISSING_PAGE_READY_PERMISSION',
      title: "Missing 'ui.pageReady' in permissions.pageRequests",
      detail: 'Views should declare and invoke ui.pageReady after first paint so SkyUltra knows startup succeeded without blank-frame stalls.',
      fixSnippet: `"pageRequests": [\n  "ui.pageReady",\n  "game.subscribe"\n]`,
    });
  }

  const deniedMethods = new Set<string>();
  for (const entry of bridgeLog) {
    if (
      entry.modId === mod.id &&
      (entry.kind === 'permission-denied' || entry.status === 'denied') &&
      entry.method &&
      !mod.pageRequests.includes(entry.method)
    ) {
      deniedMethods.add(entry.method);
    }
  }
  if (deniedMethods.size > 0) {
    const merged = Array.from(new Set([...mod.pageRequests, ...deniedMethods]));
    findings.push({
      severity: 'error',
      code: 'PERMISSION_DENIED_RUNTIME',
      title: `Blocked bridge request(s): ${Array.from(deniedMethods).join(', ')}`,
      detail: `At runtime, '${mod.id}' called ${Array.from(deniedMethods).join(', ')} without declaring it in manifest.json permissions.pageRequests.`,
      fixSnippet: `"permissions": {\n  "pageRequests": ${JSON.stringify(merged, null, 2).replace(/\n/g, '\n  ')}\n}`,
    });
  }

  if (mod.totalAssetBytes > 25 * 1024 * 1024) {
    findings.push({
      severity: 'warning',
      code: 'LARGE_ASSET_FOOTPRINT',
      title: 'Large package asset footprint (> 25 MB)',
      detail: `Package contains ${(mod.totalAssetBytes / (1024 * 1024)).toFixed(1)} MB across ${mod.assetCount} files. Compress large textures/videos to keep startup fast.`,
    });
  }

  if (findings.length === 0) {
    findings.push({
      severity: 'info',
      code: 'HEALTHY',
      title: 'All manifest, asset & bridge checks passed',
      detail: `${mod.id} v${mod.version} passed static package validation and has zero runtime permission denials.`,
    });
  }
  return findings;
}

const RESERVED_HOST_OUTPUT_FOLDERS = new Set([
  'actors',
  'items',
  'exports',
  'screenshots',
  'cache',
  'logs',
]);

export async function readSkyUltraDeveloperSnapshot(
  bridge: SkyWebViewAPI = getSkyWebView(),
  options?: SkyUltraRequestOptions,
): Promise<SkyUltraDeveloperSnapshot> {
  const raw = await bridge.request<SkyUltraDeveloperSnapshot>('ui.getDeveloperSnapshot', {}, options);
  if (!raw || typeof raw !== 'object' || typeof raw.available !== 'boolean') {
    throw new Error('Invalid developer snapshot response.');
  }
  const bridgeLog = Array.isArray(raw.bridgeLog) ? raw.bridgeLog : [];
  const mods = (Array.isArray(raw.mods) ? raw.mods : [])
    .filter((mod) => {
      const folderKey = String(mod.folderName || mod.id || '').trim().toLowerCase();
      if (!mod.manifestValid && !mod.entryExists && RESERVED_HOST_OUTPUT_FOLDERS.has(folderKey)) {
        return false;
      }
      return true;
    })
    .map((mod) => ({
      ...mod,
      findings:
        Array.isArray(mod.findings) && mod.findings.length > 0
          ? mod.findings
          : diagnoseSkyUltraModRecord(mod, bridgeLog),
    }));
  return {
    ...raw,
    views: Array.isArray(raw.views) ? raw.views : [],
    mods,
    bridgeLog,
  };
}

export async function reloadSkyUltraModViews(
  params: { modId?: string; setHotReloadWatcher?: boolean },
  bridge: SkyWebViewAPI = getSkyWebView(),
  options?: SkyUltraRequestOptions,
): Promise<SkyUltraReloadModResult> {
  const payload: Record<string, JsonValue> = {};
  if (typeof params.modId === 'string' && params.modId.trim()) {
    payload.modId = params.modId.trim();
  }
  if (typeof params.setHotReloadWatcher === 'boolean') {
    payload.setHotReloadWatcher = params.setHotReloadWatcher;
  }
  return bridge.request<SkyUltraReloadModResult>('ui.reloadModViews', payload, options);
}

export async function manageSkyUltraDiagnosticSample(
  action: 'deploy-broken' | 'repair-and-reload' | 'remove',
  bridge: SkyWebViewAPI = getSkyWebView(),
  options?: SkyUltraRequestOptions,
): Promise<SkyUltraDiagnosticSampleResult> {
  if (!['deploy-broken', 'repair-and-reload', 'remove'].includes(action)) {
    throw new Error(`Unknown diagnostic sample action '${String(action)}'.`);
  }
  return bridge.request<SkyUltraDiagnosticSampleResult>('ui.manageDiagnosticSample', { action }, options);
}
