(() => {
  if (window.parent === window) {
    const reloadEvents = new EventSource("/__skyultra/events");
    reloadEvents.addEventListener("reload", () => {
      const url = new URL(window.location.href);
      url.searchParams.set("__skyultra_reload", String(Date.now()));
      window.location.replace(url);
    });
  }

  if (Object.prototype.hasOwnProperty.call(window, "SkyWebView")) return;

  const listeners = new Map();
  const runtimeInfo = Object.freeze({
    host: "preview",
    bridgeVersion: 1,
    browserEngine: "unknown",
    capabilities: Object.freeze({ events: true, requests: true, requestCancellation: true }),
  });
  const bridge = {
    bridgeVersion: runtimeInfo.bridgeVersion,
    getRuntimeInfo() {
      return runtimeInfo;
    },
    on(name, callback) {
      if (typeof name !== "string" || !name || typeof callback !== "function") {
        throw new TypeError("SkyWebView.on expects an event name and a callback");
      }
      let callbacks = listeners.get(name);
      if (!callbacks) listeners.set(name, callbacks = new Set());
      callbacks.add(callback);
      return () => bridge.off(name, callback);
    },
    off(name, callback) {
      const callbacks = listeners.get(name);
      if (!callbacks) return false;
      const removed = callbacks.delete(callback);
      if (callbacks.size === 0) listeners.delete(name);
      return removed;
    },
    request(name, params, options = {}) {
      const signal = options?.signal;
      const abortError = () => {
        const error = new Error("The request was cancelled");
        error.name = "AbortError";
        return error;
      };
      if (signal?.aborted) return Promise.reject(abortError());
      return new Promise((resolve, reject) => {
        let settled = false;
        const cleanup = () => signal?.removeEventListener("abort", onAbort);
        const onAbort = () => {
          if (settled) return;
          settled = true;
          cleanup();
          reject(abortError());
        };
        signal?.addEventListener("abort", onAbort, { once: true });
        Promise.resolve().then(() => {
          if (settled) return;
          settled = true;
          cleanup();
          resolve({ preview: true, name, params: params ?? null });
        });
      });
    },
  };

  window.addEventListener("skyultra:message", (event) => {
    const { name, data } = event.detail ?? {};
    for (const callback of [...(listeners.get(name) ?? [])]) callback(data);
  });
  Object.defineProperty(window, "SkyWebView", {
    value: Object.freeze(bridge),
    writable: false,
    configurable: false,
  });
  Object.defineProperty(window, "__SKYULTRA_RUNTIME__", {
    value: runtimeInfo,
    writable: false,
    configurable: false,
  });
  Object.defineProperty(window, "__SKYULTRA_PREVIEW__", {
    value: true,
    writable: false,
    configurable: false,
  });
  console.info("SkyUltra preview mode: SkyWebView requests return simulated responses.");
})();
