export type JsonValue =
  | null
  | boolean
  | number
  | string
  | JsonValue[]
  | { [key: string]: JsonValue };

export interface SkyUltraRequestOptions {
  signal?: AbortSignal;
}

export type SkyUltraHost = "skyrim" | "preview" | "browser" | "unknown";
export type SkyUltraBrowserEngine = "ultralight" | "cef" | "unknown";

export interface SkyUltraCapabilities {
  events: boolean;
  requests: boolean;
  requestCancellation: boolean;
}

export interface SkyUltraRuntimeInfo {
  host: SkyUltraHost;
  pluginVersion?: string;
  nativeApiVersion?: number;
  bridgeVersion?: number;
  browserEngine?: SkyUltraBrowserEngine;
  capabilities: Readonly<SkyUltraCapabilities>;
}

export type SkyUltraRuntimeInfoHint = Partial<Omit<SkyUltraRuntimeInfo, "capabilities">> & {
  capabilities?: Partial<SkyUltraCapabilities>;
};

export interface SkyWebViewAPI {
  /** Host visibility retained even when the page subscribed after the first event. */
  getVisibility?(): {visible:boolean;visibleSinceMs:number|null};
  readonly bridgeVersion?: number;
  on<T = JsonValue>(name: string, callback: (data: T) => void): () => void;
  off<T = JsonValue>(name: string, callback: (data: T) => void): boolean;
  request<TResponse = JsonValue>(
    name: string,
    params?: JsonValue,
    options?: SkyUltraRequestOptions,
  ): Promise<TResponse>;
  getRuntimeInfo?(): SkyUltraRuntimeInfoHint;
}

type EventName<TEvents extends object> = Extract<keyof TEvents, string> extends never
  ? string
  : Extract<keyof TEvents, string>;

type EventPayload<TEvents extends object, TName extends string> = TName extends keyof TEvents
  ? TEvents[TName]
  : JsonValue;

type RequestName<TRequests extends object> = Extract<keyof TRequests, string> extends never
  ? string
  : Extract<keyof TRequests, string>;

type RequestParams<TRequests extends object, TName extends string> = TName extends keyof TRequests
  ? TRequests[TName] extends { params?: infer TParams }
    ? TParams
    : JsonValue
  : JsonValue;

type RequestArgs<TRequests extends object, TName extends string> = TName extends keyof TRequests
  ? TRequests[TName] extends { params: infer TParams }
    ? [params: TParams, options?: SkyUltraRequestOptions]
    : [params?: RequestParams<TRequests, TName>, options?: SkyUltraRequestOptions]
  : [params?: JsonValue, options?: SkyUltraRequestOptions];

type RequestResponse<TRequests extends object, TName extends string> = TName extends keyof TRequests
  ? TRequests[TName] extends { response: infer TResponse }
    ? TResponse
    : JsonValue
  : JsonValue;

/** A typed view of the page-to-native event and request bridge. */
export interface SkyUltraClient<
  TEvents extends object = Record<string, JsonValue>,
  TRequests extends object = Record<string, { params?: JsonValue; response: JsonValue }>,
> {
  on<TName extends EventName<TEvents>>(
    name: TName,
    callback: (data: EventPayload<TEvents, TName>) => void,
  ): () => void;
  off<TName extends EventName<TEvents>>(
    name: TName,
    callback: (data: EventPayload<TEvents, TName>) => void,
  ): boolean;
  request<TName extends RequestName<TRequests>>(
    name: TName,
    ...args: RequestArgs<TRequests, TName>
  ): Promise<RequestResponse<TRequests, TName>>;
}

declare global {
  interface Window {
    readonly SkyWebView?: SkyWebViewAPI;
    readonly __SKYULTRA_RUNTIME__?: SkyUltraRuntimeInfoHint;
    readonly __SKYULTRA_PREVIEW__?: boolean;
  }
}

function currentWindow(target?: Window): Window | undefined {
  if (target) return target;
  return typeof window === "undefined" ? undefined : window;
}

/** Return the host-injected bridge, or explain how to make it available. */
export function getSkyWebView(target?: Window): SkyWebViewAPI {
  const bridge = currentWindow(target)?.SkyWebView;
  if (!bridge) {
    throw new Error(
      "SkyWebView is unavailable. Include SkyUltraBridge.js in this page before using the SDK.",
    );
  }
  return bridge;
}

/** Release a view's opt-in native startup splash after the interface has mounted. */
export function markSkyUltraPageReady(bridge: SkyWebViewAPI = getSkyWebView()): Promise<{ ready: boolean }> {
  return bridge.request("ui.pageReady", {});
}

/** Connect the UI kit's popup scopes so Escape dismisses the popup before its game view. */
export function connectSkyUltraInputScopes(bridge: SkyWebViewAPI = getSkyWebView(), target: Window = window): () => void {
  const send = (depth: number): void => { void bridge.request("ui.setInputScope", { depth: Number.isFinite(depth) ? Math.max(0,Math.min(8,depth)) : 0 }).catch(() => undefined); };
  const scope = (event: Event): void => send(Number((event as CustomEvent<{depth?: number}>).detail?.depth ?? 0));
  target.addEventListener("skyultra:input-scope",scope);
  send(0);
  return () => { target.removeEventListener("skyultra:input-scope",scope); send(0); };
}

/** Return a typed client for this page's SkyUltra bridge. */
export function getSkyUltraClient<
  TEvents extends object = Record<string, JsonValue>,
  TRequests extends object = Record<string, { params?: JsonValue; response: JsonValue }>,
>(target?: Window): SkyUltraClient<TEvents, TRequests> {
  return getSkyWebView(target) as unknown as SkyUltraClient<TEvents, TRequests>;
}

/** Inspect the detected host, bridge capabilities, and optional native version metadata. */
export function getSkyUltraRuntimeInfo(target?: Window): SkyUltraRuntimeInfo {
  const hostWindow = currentWindow(target);
  const bridge = hostWindow?.SkyWebView;
  const hint = bridge?.getRuntimeInfo?.() ?? hostWindow?.__SKYULTRA_RUNTIME__;

  const detectedHost: SkyUltraHost = hint?.host ?? (
    hostWindow?.__SKYULTRA_PREVIEW__
      ? "preview"
      : bridge
        ? "unknown"
        : hostWindow
          ? "browser"
          : "unknown"
  );

  return {
    ...hint,
    host: detectedHost,
    bridgeVersion: hint?.bridgeVersion ?? bridge?.bridgeVersion,
    browserEngine: hint?.browserEngine ?? "unknown",
    capabilities: {
      events: hint?.capabilities?.events ?? typeof bridge?.on === "function",
      requests: hint?.capabilities?.requests ?? typeof bridge?.request === "function",
      requestCancellation: hint?.capabilities?.requestCancellation ?? false,
    },
  };
}

export type SkyUltraMockRequestHandler = (
  name: string,
  params: JsonValue,
  signal?: AbortSignal,
) => JsonValue | Promise<JsonValue>;

export interface SkyUltraMockOptions {
  requestHandler?: SkyUltraMockRequestHandler;
  runtimeInfo?: SkyUltraRuntimeInfoHint;
}

export interface SkyUltraMock<
  TEvents extends object = Record<string, JsonValue>,
  _TRequests extends object = Record<string, { params?: JsonValue; response: JsonValue }>,
> {
  readonly api: SkyWebViewAPI;
  readonly runtimeInfo: SkyUltraRuntimeInfo;
  emit<TName extends EventName<TEvents>>(
    name: TName,
    data: EventPayload<TEvents, TName>,
  ): void;
  setRequestHandler(handler: SkyUltraMockRequestHandler): void;
}

function createAbortError(): Error {
  const error = new Error("The request was cancelled");
  error.name = "AbortError";
  return error;
}

/** Create a local bridge mock for browser previews and page development. */
export function createSkyUltraMock<
  TEvents extends object = Record<string, JsonValue>,
  TRequests extends object = Record<string, { params?: JsonValue; response: JsonValue }>,
>(options: SkyUltraMockOptions = {}): SkyUltraMock<TEvents, TRequests> {
  const listeners = new Map<string, Set<(data: JsonValue) => void>>();
  const capabilities: SkyUltraCapabilities = {
    events: true,
    requests: true,
    requestCancellation: true,
  };
  const runtimeInfo: SkyUltraRuntimeInfo = Object.freeze({
    ...options.runtimeInfo,
    host: options.runtimeInfo?.host ?? "preview",
    bridgeVersion: options.runtimeInfo?.bridgeVersion ?? 1,
    browserEngine: options.runtimeInfo?.browserEngine ?? "unknown",
    capabilities: Object.freeze({
      events: options.runtimeInfo?.capabilities?.events ?? capabilities.events,
      requests: options.runtimeInfo?.capabilities?.requests ?? capabilities.requests,
      requestCancellation:
        options.runtimeInfo?.capabilities?.requestCancellation ?? capabilities.requestCancellation,
    }),
  });
  let requestHandler: SkyUltraMockRequestHandler = options.requestHandler ??
    ((name, params) => ({ preview: true, name, params }));

  const on = <T = JsonValue>(name: string, callback: (data: T) => void): (() => void) => {
    let callbacks = listeners.get(name);
    if (!callbacks) {
      callbacks = new Set();
      listeners.set(name, callbacks);
    }
    callbacks.add(callback as (data: JsonValue) => void);
    return () => { off(name, callback); };
  };

  const off = <T = JsonValue>(name: string, callback: (data: T) => void): boolean => {
    const callbacks = listeners.get(name);
    if (!callbacks) return false;
    const removed = callbacks.delete(callback as (data: JsonValue) => void);
    if (callbacks.size === 0) listeners.delete(name);
    return removed;
  };

  const request = <TResponse = JsonValue>(
    name: string,
    params?: JsonValue,
    requestOptions: SkyUltraRequestOptions = {},
  ): Promise<TResponse> => {
    const signal = requestOptions.signal;
    if (signal?.aborted) return Promise.reject(createAbortError());

    return new Promise<TResponse>((resolve, reject) => {
      let settled = false;
      const cleanup = () => signal?.removeEventListener("abort", onAbort);
      const onAbort = () => {
        if (settled) return;
        settled = true;
        cleanup();
        reject(createAbortError());
      };

      signal?.addEventListener("abort", onAbort, { once: true });
      Promise.resolve()
        .then(() => requestHandler(name, params ?? null, signal))
        .then(
          (value) => {
            if (settled) return;
            settled = true;
            cleanup();
            resolve(value as TResponse);
          },
          (error: unknown) => {
            if (settled) return;
            settled = true;
            cleanup();
            reject(error);
          },
        );
    });
  };

  const api: SkyWebViewAPI = Object.freeze({
    bridgeVersion: 1,
    on,
    off,
    request,
    getRuntimeInfo: () => runtimeInfo,
  });

  return {
    api,
    runtimeInfo,
    emit<TName extends EventName<TEvents>>(
      name: TName,
      data: EventPayload<TEvents, TName>,
    ) {
      const callbacks = listeners.get(name);
      if (!callbacks) return;
      for (const callback of [...callbacks]) {
        try {
          callback(data as unknown as JsonValue);
        } catch (error) {
          console.error("SkyUltra mock event handler failed:", error);
        }
      }
    },
    setRequestHandler(handler) {
      requestHandler = handler;
    },
  };
}

/** Install a mock into an otherwise unhosted browser window for local development. */
export function installSkyUltraMock<
  TEvents extends object = Record<string, JsonValue>,
  TRequests extends object = Record<string, { params?: JsonValue; response: JsonValue }>,
>(target: Window, options: SkyUltraMockOptions = {}): SkyUltraMock<TEvents, TRequests> {
  if (target.SkyWebView) {
    throw new Error("A SkyWebView bridge is already installed on this window.");
  }

  const mock = createSkyUltraMock<TEvents, TRequests>(options);
  Object.defineProperty(target, "SkyWebView", {
    value: mock.api,
    configurable: true,
  });
  Object.defineProperty(target, "__SKYULTRA_RUNTIME__", {
    value: mock.runtimeInfo,
    configurable: true,
  });
  Object.defineProperty(target, "__SKYULTRA_PREVIEW__", {
    value: true,
    configurable: true,
  });
  return mock;
}

export { readSkyUltraPerformance, compareSkyUltraPerformance } from "./performance.js";
export type { SkyUltraTimingMetric, SkyUltraTimingDelta, SkyUltraPerformanceSnapshot, SkyUltraNativeCapture } from "./performance.js";

export * from './renderer.js';
export * from './settings.js';

export * from './game.js';

export * from './layout.js';
export * from './hudModExport.js';
export * from './devtools.js';
export * from './profiles.js';
export * from './compatibility.js';
export * from './enhancements.js';
export type { Player as SkyUltraPlayerSnapshot, Item as SkyUltraItem, Npc as SkyUltraNpc } from './game.js';
