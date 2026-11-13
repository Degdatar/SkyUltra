(() => {
  if (Object.prototype.hasOwnProperty.call(window, "SkyWebView")) return;

  const listeners = new Map();
  const pendingRequests = new Map();
  let nextRequestId = 0;
  const requestNamespace = `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;
  const nativeBridgePrefix = "__SKYULTRA_BRIDGE__";
  let browserEngine = "unknown";
  for (const parameter of window.location.search.slice(1).split("&")) {
    const separator = parameter.indexOf("=");
    if (separator < 0 || parameter.slice(0, separator) !== "__skyultra_engine") continue;
    const detectedEngine = parameter.slice(separator + 1);
    browserEngine = detectedEngine === "cef" || detectedEngine === "ultralight" ? detectedEngine : "unknown";
  }
  let viewVisible = !window.location.search.slice(1).split('&').includes('__skyultra_visible=0');
  let visibleSinceMs = viewVisible ? 0 : null;
  // Keep these values aligned with the SKSE plugin and NativeAPI.h versions.
  const runtimeInfo = Object.freeze({
    host: "skyrim",
    pluginVersion: "0.1.0.0",
    nativeApiVersion: 4,
    bridgeVersion: 1,
    browserEngine,
    capabilities: Object.freeze({ events: true, requests: true, requestCancellation: true }),
  });

  function utf8ByteLength(value) {
    let bytes = 0;
    for (let index = 0; index < value.length; index += 1) {
      const code = value.charCodeAt(index);
      if (code <= 0x7f) bytes += 1;
      else if (code <= 0x7ff) bytes += 2;
      else if (code >= 0xd800 && code <= 0xdbff && index + 1 < value.length &&
          value.charCodeAt(index + 1) >= 0xdc00 && value.charCodeAt(index + 1) <= 0xdfff) {
        bytes += 4;
        index += 1;
      } else bytes += 3;
    }
    return bytes;
  }

  function publishNativeMessage(message) {
    const serialized = JSON.stringify(message);
    if (typeof serialized !== "string") {
      throw new TypeError("SkyWebView request data must be JSON serializable");
    }
    if (utf8ByteLength(serialized) > 65536) {
      throw new RangeError("SkyWebView request messages must be no larger than 64 KiB");
    }
    console.debug(nativeBridgePrefix + serialized);
  }

  function abortError(reason) {
    const error = new Error(typeof reason === "string" && reason ? reason : "The request was cancelled");
    error.name = "AbortError";
    return error;
  }

  function on(name, callback) {
    if (typeof name !== "string" || !name || typeof callback !== "function") {
      throw new TypeError("SkyWebView.on expects an event name and a callback");
    }
    let callbacks = listeners.get(name);
    if (!callbacks) {
      callbacks = new Set();
      listeners.set(name, callbacks);
    }
    callbacks.add(callback);
    return () => off(name, callback);
  }

  function off(name, callback) {
    const callbacks = listeners.get(name);
    if (!callbacks) return false;
    const removed = callbacks.delete(callback);
    if (callbacks.size === 0) listeners.delete(name);
    return removed;
  }

  function request(name, params, options = {}) {
    if (typeof name !== "string" || !name || utf8ByteLength(name) > 128) {
      return Promise.reject(new TypeError("SkyWebView.request expects a non-empty UTF-8 name of at most 128 bytes"));
    }
    if (options === null || typeof options !== "object") {
      return Promise.reject(new TypeError("SkyWebView.request options must be an object"));
    }

    const signal = options.signal;
    if (signal != null && (typeof signal !== "object" || typeof signal.addEventListener !== "function")) {
      return Promise.reject(new TypeError("SkyWebView.request signal must be an AbortSignal"));
    }
    if (signal?.aborted) {
      return Promise.reject(abortError(signal.reason));
    }

    const id = `${requestNamespace}-${(++nextRequestId).toString(36)}`;
    return new Promise((resolve, reject) => {
      const cleanup = () => signal?.removeEventListener("abort", onAbort);
      const onAbort = () => {
        if (!pendingRequests.delete(id)) return;
        cleanup();
        reject(abortError(signal.reason));
        try {
          publishNativeMessage({ kind: "cancel", id });
        } catch (error) {
          console.error("SkyWebView could not notify native code about request cancellation:", error);
        }
      };

      pendingRequests.set(id, { resolve, reject, cleanup });
      signal?.addEventListener("abort", onAbort, { once: true });
      if (signal?.aborted) {
        onAbort();
        return;
      }
      try {
        publishNativeMessage({ kind: "request", id, name, params: params === undefined ? null : params });
      } catch (error) {
        pendingRequests.delete(id);
        cleanup();
        reject(error);
      }
    });
  }

  window.addEventListener("skyultra:message", (event) => {
    const detail = event.detail;
    if (!detail || typeof detail.name !== "string") return;
    if(detail.name==='skyultra.visibility' && detail.data && typeof detail.data.visible==='boolean'){
      if(detail.data.visible&&!viewVisible)visibleSinceMs=performance.now();
      viewVisible=detail.data.visible;
    }
    const callbacks = listeners.get(detail.name);
    if (!callbacks) return;
    for (const callback of [...callbacks]) {
      try {
        callback(detail.data);
      } catch (error) {
        console.error("SkyWebView event handler failed:", error);
      }
    }
  });

  window.addEventListener("skyultra:response", (event) => {
    const detail = event.detail;
    if (!detail || typeof detail.id !== "string") return;
    const pending = pendingRequests.get(detail.id);
    if (!pending) return;
    pendingRequests.delete(detail.id);
    pending.cleanup();
    if (detail.ok === true) {
      pending.resolve(detail.data);
    } else {
      const error = new Error(typeof detail.error === "string" ? detail.error : "The native request failed");
      error.name = "SkyUltraRequestError";
      pending.reject(error);
    }
  });

  window.addEventListener("pagehide", () => {
    for (const [id, pending] of pendingRequests) {
      pendingRequests.delete(id);
      pending.cleanup();
      pending.reject(abortError("The page was unloaded"));
      try {
        publishNativeMessage({ kind: "cancel", id });
      } catch (error) {
        console.error("SkyWebView could not notify native code about an unloaded page request:", error);
      }
    }
  });

  Object.defineProperty(window, "SkyWebView", {
    value: Object.freeze({
      bridgeVersion: runtimeInfo.bridgeVersion,
      on,
      off,
      request,
      getRuntimeInfo: () => runtimeInfo,
      getVisibility: () => ({visible:viewVisible,visibleSinceMs}),
    }),
    writable: false,
    configurable: false,
  });
  if (!Object.prototype.hasOwnProperty.call(window, "__SKYULTRA_RUNTIME__")) {
    Object.defineProperty(window, "__SKYULTRA_RUNTIME__", {
      value: runtimeInfo,
      writable: false,
      configurable: false,
    });
  }
})();
