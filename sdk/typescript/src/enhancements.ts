import { getSkyWebView, type JsonValue, type SkyWebViewAPI } from './index.js';

// ============================================================================
// 1. Native Skyrim Audio Bridge + Declarative data-sky-sound Attributes
// ============================================================================

export const skyUltraSoundPresets = [
  'focus',
  'ok',
  'cancel',
  'open',
  'bladeClose',
  'tab',
  'equip',
  'unequip',
  'gold',
  'potion',
  'magic',
  'shout',
] as const;

export type SkyUltraSoundPreset = (typeof skyUltraSoundPresets)[number];

export interface SkyUltraPlaySoundResult {
  available: boolean;
  played: boolean;
  sound: string;
}

export async function playSkyUltraSound(
  sound: SkyUltraSoundPreset | string = 'ok',
  bridge?: SkyWebViewAPI,
): Promise<SkyUltraPlaySoundResult> {
  const safeSound = String(sound || 'ok').trim() || 'ok';
  try {
    const api = bridge ?? getSkyWebView();
    const res = await api.request<SkyUltraPlaySoundResult>('ui.playSound', {
      sound: safeSound,
    });
    if (res && typeof res === 'object' && typeof res.played === 'boolean') {
      return res;
    }
  } catch {
    // Fallback gracefully in browser preview
  }
  return { available: false, played: false, sound: safeSound };
}

export interface SkyUltraDeclarativeSoundOptions {
  bridge?: SkyWebViewAPI;
  enabled?: boolean;
  hoverCooldownMs?: number;
}

export function connectSkyUltraDeclarativeSounds(
  options: SkyUltraDeclarativeSoundOptions = {},
): () => void {
  if (typeof window === 'undefined' || typeof document === 'undefined') {
    return () => {};
  }
  if (options.enabled === false) {
    return () => {};
  }

  const cooldownMs = Math.max(20, options.hoverCooldownMs ?? 55);
  let lastHoverAt = 0;
  let lastHoverEl: Element | null = null;

  const interactiveSelector =
    '[data-sky-sound],[data-sky-hover-sound],button.control-button,button.primary-button,button.nav-item,button.char-stage-btn,[role="tab"],[role="switch"]';

  const onPointerOver = (ev: Event): void => {
    const target = ev.target as Element | null;
    if (!target || typeof target.closest !== 'function') return;
    const el = target.closest(interactiveSelector);
    if (!el || el === lastHoverEl) return;
    if ((el as HTMLButtonElement).disabled) return;
    lastHoverEl = el;
    const now = typeof performance !== 'undefined' ? performance.now() : Date.now();
    if (now - lastHoverAt < cooldownMs) return;
    lastHoverAt = now;
    const customHover = el.getAttribute('data-sky-hover-sound');
    if (customHover === 'none') return;
    void playSkyUltraSound(customHover || 'focus', options.bridge);
  };

  const onClick = (ev: Event): void => {
    const target = ev.target as Element | null;
    if (!target || typeof target.closest !== 'function') return;
    const el = target.closest(interactiveSelector);
    if (!el || (el as HTMLButtonElement).disabled) return;
    const explicit = el.getAttribute('data-sky-sound');
    if (explicit === 'none') return;
    if (explicit) {
      void playSkyUltraSound(explicit, options.bridge);
      return;
    }
    if (el.classList.contains('sky-menu-close')) {
      void playSkyUltraSound('cancel', options.bridge);
      return;
    }
    if (el.classList.contains('nav-item') || el.getAttribute('role') === 'tab') {
      void playSkyUltraSound('tab', options.bridge);
      return;
    }
    void playSkyUltraSound('ok', options.bridge);
  };

  document.addEventListener('pointerover', onPointerOver, { passive: true });
  document.addEventListener('click', onClick, { passive: true });

  return () => {
    document.removeEventListener('pointerover', onPointerOver);
    document.removeEventListener('click', onClick);
  };
}

// ============================================================================
// 2. 3D Worldspace & Actor-Bone Attached Web Surfaces + VR atGaze & Keyboard
// ============================================================================

export const skyUltraActorBones = [
  'NPC Head [Head]',
  'NPC COM [COM ]',
  'NPC Spine2 [Spn2]',
  'NPC R Hand [RHnd]',
  'NPC L Hand [LHnd]',
  'root',
] as const;

export type SkyUltraActorBone = (typeof skyUltraActorBones)[number];

export const skyUltraSpatialModes = [
  'screen',
  'atGaze',
  'worldLocked',
  'leftWrist',
  'rightWrist',
  'actorBone',
] as const;

export type SkyUltraSpatialMode = (typeof skyUltraSpatialModes)[number];

export interface SkyUltraWorldAnchorQuery {
  id: string;
  referenceId?: string | number;
  bone?: SkyUltraActorBone | string;
  offsetZ?: number;
  x?: number;
  y?: number;
  z?: number;
}

export interface SkyUltraProjectedAnchor {
  id: string;
  available: boolean;
  onScreen: boolean;
  u: number;
  v: number;
  depth: number;
  distance: number;
  bone: string;
  name: string;
  level: number;
  health: number;
  healthMax: number;
  hostile: boolean;
  dead: boolean;
}

export interface SkyUltraWorldProjectionResult {
  available: boolean;
  vr: boolean;
  anchors: SkyUltraProjectedAnchor[];
}

export async function projectSkyUltraWorldAnchors(
  anchors: readonly SkyUltraWorldAnchorQuery[],
  bridge?: SkyWebViewAPI,
): Promise<SkyUltraWorldProjectionResult> {
  const payload = anchors.slice(0, 32).map((a, idx) => ({
    id: String(a.id || idx + 1),
    referenceId: a.referenceId !== undefined ? String(a.referenceId) : undefined,
    bone: a.bone || 'NPC Head [Head]',
    offsetZ: typeof a.offsetZ === 'number' && Number.isFinite(a.offsetZ) ? a.offsetZ : 18,
    x: typeof a.x === 'number' && Number.isFinite(a.x) ? a.x : undefined,
    y: typeof a.y === 'number' && Number.isFinite(a.y) ? a.y : undefined,
    z: typeof a.z === 'number' && Number.isFinite(a.z) ? a.z : undefined,
  }));

  try {
    const api = bridge ?? getSkyWebView();
    const res = await api.request<SkyUltraWorldProjectionResult>('ui.projectWorldAnchors', {
      anchors: payload as unknown as JsonValue,
    });
    if (res && typeof res === 'object' && Array.isArray(res.anchors)) {
      return res;
    }
  } catch {
    // Fallback preview projection for Studio testing
  }

  return {
    available: false,
    vr: false,
    anchors: payload.map((a, idx) => ({
      id: a.id,
      available: true,
      onScreen: true,
      u: Math.min(0.85, Math.max(0.15, 0.32 + idx * 0.22)),
      v: Math.min(0.75, Math.max(0.2, 0.34 + (idx % 2) * 0.14)),
      depth: 0.42,
      distance: 320 + idx * 140,
      bone: a.bone,
      name: idx === 0 ? 'Draugr Deathlord' : `Whiterun Guard #${idx + 1}`,
      level: idx === 0 ? 34 : 20,
      health: idx === 0 ? 640 : 380,
      healthMax: idx === 0 ? 850 : 400,
      hostile: idx === 0,
      dead: false,
    })),
  };
}

export interface SkyUltraSpatialPlacementOptions {
  query?: boolean;
  mode?: SkyUltraSpatialMode;
  distance?: number;
  scale?: number;
  bone?: SkyUltraActorBone | string;
  targetRefId?: string | number;
  virtualKeyboard?: boolean;
}

export interface SkyUltraSpatialPlacementState {
  available: boolean;
  updated: boolean;
  mode: SkyUltraSpatialMode;
  distance: number;
  scale: number;
  bone: string;
  targetRefId: number;
  virtualKeyboard: boolean;
  vrActive: boolean;
}

export async function configureSkyUltraSpatialPlacement(
  options: SkyUltraSpatialPlacementOptions = {},
  bridge?: SkyWebViewAPI,
): Promise<SkyUltraSpatialPlacementState> {
  try {
    const api = bridge ?? getSkyWebView();
    const res = await api.request<SkyUltraSpatialPlacementState>(
      'ui.setSpatialPlacement',
      options as unknown as JsonValue,
    );
    if (res && typeof res === 'object' && typeof res.mode === 'string') {
      return res;
    }
  } catch {
    // Fallback in preview mode
  }
  return {
    available: true,
    updated: !options.query,
    mode: options.mode ?? 'screen',
    distance: options.distance ?? 1.25,
    scale: options.scale ?? 1.0,
    bone: options.bone ?? 'NPC Head [Head]',
    targetRefId: Number(options.targetRefId ?? 0) || 0,
    virtualKeyboard: options.virtualKeyboard ?? true,
    vrActive: false,
  };
}

// ============================================================================
// 3. Embedded WebKit / CEF DOM Inspector Toggle & Live Tree Snapshot
// ============================================================================

export interface SkyUltraDomInspectorResult {
  available: boolean;
  viewId: number;
  inspectorOpen: boolean;
  message: string;
}

export async function toggleSkyUltraDomInspector(
  options: { viewId?: number; open?: boolean } = {},
  bridge?: SkyWebViewAPI,
): Promise<SkyUltraDomInspectorResult> {
  const viewId = options.viewId ?? 1;
  try {
    const api = bridge ?? getSkyWebView();
    const res = await api.request<SkyUltraDomInspectorResult>('ui.toggleDomInspector', {
      viewId,
      ...(typeof options.open === 'boolean' ? { open: options.open } : {}),
    });
    if (res && typeof res === 'object' && typeof res.inspectorOpen === 'boolean') {
      return res;
    }
  } catch {
    // Preview fallback
  }
  const nextOpen = options.open ?? true;
  return {
    available: true,
    viewId,
    inspectorOpen: nextOpen,
    message: nextOpen
      ? 'Embedded WebKit/CEF DOM Inspector attached (Studio mode).'
      : 'DOM Inspector detached.',
  };
}

export interface SkyUltraDomNodeSummary {
  id: string;
  depth: number;
  tag: string;
  domId: string;
  classes: string;
  role: string;
  textPreview: string;
  rect: { x: number; y: number; width: number; height: number };
  computed: {
    display: string;
    position: string;
    color: string;
    background: string;
    font: string;
    zIndex: string;
  };
  childCount: number;
}

export function captureLiveDomInspectorTree(
  root?: Element | null,
  maxNodes = 64,
): SkyUltraDomNodeSummary[] {
  if (typeof document === 'undefined') return [];
  const startNode = root ?? document.body;
  if (!startNode) return [];

  const nodes: SkyUltraDomNodeSummary[] = [];
  const walk = (el: Element, depth: number, indexPath: string): void => {
    if (nodes.length >= maxNodes || depth > 7) return;
    const tag = el.tagName.toLowerCase();
    if (tag === 'script' || tag === 'style' || tag === 'svg' || tag === 'path') return;

    const rect =
      typeof el.getBoundingClientRect === 'function'
        ? el.getBoundingClientRect()
        : { x: 0, y: 0, width: 0, height: 0 };
    const cs =
      typeof window !== 'undefined' && typeof window.getComputedStyle === 'function'
        ? window.getComputedStyle(el)
        : null;

    let directText = '';
    for (let i = 0; i < el.childNodes.length; i += 1) {
      const n = el.childNodes[i];
      if (n && n.nodeType === 3 && n.textContent) {
        directText += n.textContent.trim() + ' ';
      }
    }

    nodes.push({
      id: indexPath,
      depth,
      tag,
      domId: el.id || '',
      classes:
        typeof el.className === 'string'
          ? el.className
              .trim()
              .split(/\s+/)
              .filter(Boolean)
              .slice(0, 4)
              .join('.')
          : '',
      role: el.getAttribute('role') || el.getAttribute('data-sky-theme') || '',
      textPreview: directText.trim().slice(0, 48),
      rect: {
        x: Math.round(rect.x),
        y: Math.round(rect.y),
        width: Math.round(rect.width),
        height: Math.round(rect.height),
      },
      computed: {
        display: cs?.display || 'block',
        position: cs?.position || 'static',
        color: cs?.color || 'inherit',
        background: cs?.backgroundColor || 'transparent',
        font: cs ? `${cs.fontWeight} ${cs.fontSize} ${cs.fontFamily.split(',')[0]}` : 'inherit',
        zIndex: cs?.zIndex || 'auto',
      },
      childCount: el.children.length,
    });

    for (let i = 0; i < el.children.length; i += 1) {
      if (nodes.length >= maxNodes) break;
      walk(el.children[i], depth + 1, `${indexPath}.${i}`);
    }
  };

  walk(startNode, 0, '0');
  return nodes;
}

// ============================================================================
// 4. Turnkey Gamepad Virtual Cursor & Dynamic Controller Glyph Bar
// ============================================================================

export type SkyUltraControllerGlyphFamily = 'keyboard' | 'xbox' | 'playstation' | 'vr';

export interface SkyUltraGlyphSet {
  family: SkyUltraControllerGlyphFamily;
  label: string;
  confirm: string;
  cancel: string;
  secondary: string;
  tertiary: string;
  prevTab: string;
  nextTab: string;
  menu: string;
  cursor: string;
}

export const SKYULTRA_CONTROLLER_GLYPHS: Record<SkyUltraControllerGlyphFamily, SkyUltraGlyphSet> = {
  keyboard: {
    family: 'keyboard',
    label: 'Keyboard & Mouse',
    confirm: 'Enter',
    cancel: 'Esc',
    secondary: 'Ctrl+K',
    tertiary: 'Ctrl+F3',
    prevTab: 'Shift+Tab',
    nextTab: 'Tab',
    menu: 'Shift+F6',
    cursor: 'Mouse',
  },
  xbox: {
    family: 'xbox',
    label: 'Xbox Controller',
    confirm: 'A',
    cancel: 'B',
    secondary: 'X',
    tertiary: 'Y',
    prevTab: 'LB',
    nextTab: 'RB',
    menu: 'Start',
    cursor: 'L-Stick',
  },
  playstation: {
    family: 'playstation',
    label: 'DualSense / DualShock',
    confirm: '✕',
    cancel: '◯',
    secondary: '□',
    tertiary: '△',
    prevTab: 'L1',
    nextTab: 'R1',
    menu: 'Options',
    cursor: 'L-Stick',
  },
  vr: {
    family: 'vr',
    label: 'OpenVR Motion Controllers',
    confirm: 'Trigger',
    cancel: 'B / Y',
    secondary: 'A / X',
    tertiary: 'Grip',
    prevTab: 'L-Thumb',
    nextTab: 'R-Thumb',
    menu: 'Grip+Trigger',
    cursor: 'Laser',
  },
};

export interface SkyUltraGamepadState {
  family: SkyUltraControllerGlyphFamily;
  glyphs: SkyUltraGlyphSet;
  virtualCursorEnabled: boolean;
  cursorX: number;
  cursorY: number;
  scope: string;
}

export interface SkyUltraGamepadHelper {
  getState(): SkyUltraGamepadState;
  setGlyphFamily(family: SkyUltraControllerGlyphFamily): void;
  setVirtualCursorEnabled(enabled: boolean): void;
  moveVirtualCursor(dx: number, dy: number): { x: number; y: number };
  clickVirtualCursor(): boolean;
  setScope(scope: string): void;
   subscribe(listener: (state: SkyUltraGamepadState) => void): () => void;
}

export function createSkyUltraGamepadHelper(initial?: {
  family?: SkyUltraControllerGlyphFamily;
  virtualCursorEnabled?: boolean;
}): SkyUltraGamepadHelper {
  let state: SkyUltraGamepadState = {
    family: initial?.family ?? 'keyboard',
    glyphs: SKYULTRA_CONTROLLER_GLYPHS[initial?.family ?? 'keyboard'],
    virtualCursorEnabled: Boolean(initial?.virtualCursorEnabled),
    cursorX: typeof window !== 'undefined' ? Math.round(window.innerWidth * 0.5) : 640,
    cursorY: typeof window !== 'undefined' ? Math.round(window.innerHeight * 0.5) : 360,
    scope: 'root',
  };

  const listeners = new Set<(next: SkyUltraGamepadState) => void>();
  const emit = (): void => {
    for (const fn of listeners) {
      try {
        fn(state);
      } catch {
        // Ignore listener errors
      }
    }
  };

  return {
    getState: () => state,
    setGlyphFamily(family: SkyUltraControllerGlyphFamily) {
      if (!SKYULTRA_CONTROLLER_GLYPHS[family]) return;
      state = {
        ...state,
        family,
        glyphs: SKYULTRA_CONTROLLER_GLYPHS[family],
      };
      emit();
    },
    setVirtualCursorEnabled(enabled: boolean) {
      state = {
        ...state,
        virtualCursorEnabled: Boolean(enabled),
      };
      emit();
    },
    moveVirtualCursor(dx: number, dy: number) {
      const maxW = typeof window !== 'undefined' ? window.innerWidth : 1920;
      const maxH = typeof window !== 'undefined' ? window.innerHeight : 1080;
      const nextX = Math.max(4, Math.min(maxW - 4, Math.round(state.cursorX + dx)));
      const nextY = Math.max(4, Math.min(maxH - 4, Math.round(state.cursorY + dy)));
      state = { ...state, cursorX: nextX, cursorY: nextY };
      emit();
      return { x: nextX, y: nextY };
    },
    clickVirtualCursor() {
      if (typeof document === 'undefined' || typeof document.elementFromPoint !== 'function') {
        return false;
      }
      const el = document.elementFromPoint(state.cursorX, state.cursorY) as HTMLElement | null;
      if (!el) return false;
      const clickable = (el.closest('button,a,input,textarea,select,[role="button"],[tabindex]') as HTMLElement | null) ?? el;
      if (typeof clickable.focus === 'function') clickable.focus();
      if (typeof clickable.click === 'function') clickable.click();
      return true;
    },
    setScope(scope: string) {
      state = { ...state, scope: String(scope || 'root') };
      emit();
    },
    subscribe(listener: (next: SkyUltraGamepadState) => void) {
      listeners.add(listener);
      listener(state);
      return () => {
        listeners.delete(listener);
      };
    },
  };
}

// ============================================================================
// 5. Drop-In Runtime Compatibility Shims for Prisma UI & Magelight UI Mods
// ============================================================================

export interface SkyUltraInstalledShims {
  prismaUI: {
    RegisterJSListener(name: string, callback: (data: unknown) => void): () => void;
    InteropCall(method: string, payload?: unknown): Promise<unknown>;
    Invoke(script: string): unknown;
    SetOrder(zIndex: number): void;
    GetOrder(): number;
  };
  magelight: {
    readonly version: string;
    send(type: string, payload?: unknown): Promise<unknown>;
    on(type: string, callback: (payload: unknown) => void): () => void;
    close(): Promise<void>;
    playSound(sound: string): Promise<SkyUltraPlaySoundResult>;
    vr: {
      atGaze(distance?: number): Promise<SkyUltraSpatialPlacementState>;
      setPlacement(options: SkyUltraSpatialPlacementOptions): Promise<SkyUltraSpatialPlacementState>;
    };
    gamepad: SkyUltraGamepadHelper;
  };
  uninstall(): void;
}

export function installSkyUltraCompetitorShims(
  target?: Window,
  bridge?: SkyWebViewAPI,
): SkyUltraInstalledShims {
  const hostWin = (target ?? (typeof window !== 'undefined' ? window : undefined)) as
    | (Window & Record<string, unknown>)
    | undefined;
  const gamepadHelper = createSkyUltraGamepadHelper();
  const listenerCleanups: Array<() => void> = [];
  let currentZOrder = 0;

  const resolveBridge = (): SkyWebViewAPI | undefined => {
    if (bridge) return bridge;
    try {
      return getSkyWebView(hostWin as unknown as Window);
    } catch {
      return undefined;
    }
  };

  const prismaUI = {
    RegisterJSListener(name: string, callback: (data: unknown) => void): () => void {
      const api = resolveBridge();
      if (!api) return () => {};
      const offDirect = api.on(name, (data) => {
        callback(typeof data === 'string' ? data : JSON.stringify(data));
      });
      const offInterop = api.on<{ framework?: string; method?: string; payload?: unknown }>(
        'skyultra.legacy.interop',
        (ev) => {
          if (ev?.framework === 'PrismaUI' && ev.method === name) {
            callback(typeof ev.payload === 'string' ? ev.payload : JSON.stringify(ev.payload ?? {}));
          }
        },
      );
      const cleanup = () => {
        offDirect();
        offInterop();
      };
      listenerCleanups.push(cleanup);
      return cleanup;
    },
    async InteropCall(method: string, payload?: unknown): Promise<unknown> {
      const api = resolveBridge();
      if (!api) return { accepted: false };
      return api.request('ui.interopCall', {
        framework: 'PrismaUI',
        method: String(method || 'interop'),
        payload: (payload ?? null) as JsonValue,
      });
    },
    Invoke(script: string): unknown {
      if (!script || typeof script !== 'string') return undefined;
      try {
        return new Function(script)();
      } catch {
        return undefined;
      }
    },
    SetOrder(zIndex: number): void {
      if (typeof zIndex === 'number' && Number.isFinite(zIndex)) {
        currentZOrder = Math.round(zIndex);
      }
    },
    GetOrder(): number {
      return currentZOrder;
    },
  };

  const magelight = {
    version: '1.0.0-skyultra-shim',
    async send(type: string, payload?: unknown): Promise<unknown> {
      const api = resolveBridge();
      if (!api) return { accepted: false };
      if (type.startsWith('game.') || type.startsWith('ui.')) {
        return api.request(type, (payload ?? {}) as JsonValue);
      }
      return api.request('ui.interopCall', {
        framework: 'MagelightUI',
        method: String(type || 'message'),
        payload: (payload ?? null) as JsonValue,
      });
    },
    on(type: string, callback: (payload: unknown) => void): () => void {
      const api = resolveBridge();
      if (!api) return () => {};
      const offDirect = api.on(type, callback);
      const offInterop = api.on<{ framework?: string; method?: string; payload?: unknown }>(
        'skyultra.legacy.interop',
        (event) => {
          if (event?.framework === 'MagelightUI' && event.method === type) callback(event.payload);
        },
      );
      const cleanup = () => { offDirect(); offInterop(); };
      listenerCleanups.push(cleanup);
      return cleanup;
    },
    async close(): Promise<void> {
      const api = resolveBridge();
      if (api) {
        await api.request('ui.close', {}).catch(() => undefined);
      }
    },
    playSound(sound: string): Promise<SkyUltraPlaySoundResult> {
      return playSkyUltraSound(sound, resolveBridge());
    },
    vr: {
      atGaze(distance = 1.25): Promise<SkyUltraSpatialPlacementState> {
        return configureSkyUltraSpatialPlacement({ mode: 'atGaze', distance }, resolveBridge());
      },
      setPlacement(options: SkyUltraSpatialPlacementOptions): Promise<SkyUltraSpatialPlacementState> {
        return configureSkyUltraSpatialPlacement(options, resolveBridge());
      },
    },
    gamepad: gamepadHelper,
  };

  const installedNames = ['prismaUI', 'RegisterJSListener', 'InteropCall', 'magelight', 'SkyUltraGamepad'] as const;
  const previousGlobals = new Map<string, PropertyDescriptor | undefined>();
  if (hostWin) {
    for (const name of installedNames) previousGlobals.set(name, Object.getOwnPropertyDescriptor(hostWin, name));
    hostWin.prismaUI = prismaUI;
    hostWin.RegisterJSListener = prismaUI.RegisterJSListener;
    hostWin.InteropCall = prismaUI.InteropCall;
    hostWin.magelight = magelight;
    hostWin.SkyUltraGamepad = gamepadHelper;
  }

  return {
    prismaUI,
    magelight,
    uninstall() {
      while (listenerCleanups.length > 0) {
        const fn = listenerCleanups.pop();
        if (fn) fn();
      }
      if (hostWin) for (const name of installedNames) {
        const installed = name === 'prismaUI' ? prismaUI : name === 'magelight' ? magelight :
          name === 'RegisterJSListener' ? prismaUI.RegisterJSListener : name === 'InteropCall' ? prismaUI.InteropCall : gamepadHelper;
        if (hostWin[name] !== installed) continue;
        const previous = previousGlobals.get(name);
        if (previous) Object.defineProperty(hostWin, name, previous);
        else delete hostWin[name];
      }
    },
  };
}
