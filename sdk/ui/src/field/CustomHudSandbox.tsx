import * as React from "react";
import { useEffect, useMemo, useRef } from "react";
import { sanitizeCustomHudCode, type SkyUltraCustomHudCode } from "@skyultra/sdk";
import type { Npc, Player } from "./model.js";

export interface CustomHudLogEntry {
  id: string;
  level: "log" | "warn" | "error";
  message: string;
  time: string;
}

type HudUpdateListener = (player: Record<string, unknown>, nearbyActors: Npc[]) => void;

const DEFAULT_FALLBACK_PLAYER: Record<string, unknown> = {
  available: true,
  playerName: "Dragonborn",
  race: "Nord",
  level: 42,
  location: "Whiterun",
  heading: 35,
  angleZ: 35,
  gold: 18420,
  armorRating: 540,
  activeEffectCount: 4,
  followerCount: 1,
  inventoryItemCount: 68,
  inventoryTypeCount: 34,
  health: { current: 310, max: 350 },
  magicka: { current: 195, max: 240 },
  stamina: { current: 260, max: 280 },
  carryWeight: { current: 142, max: 400 },
  equipment: {},
};

function extractHudParts(rawCode: SkyUltraCustomHudCode): {
  html: string;
  css: string;
  js: string;
} {
  const code = sanitizeCustomHudCode(rawCode);
  if (code.mode === "full-html" && code.fullHtml.trim()) {
    const src = code.fullHtml;
    const styles: string[] = [];
    const scripts: string[] = [];

    src.replace(/<style[^>]*>([\s\S]*?)<\/style>/gi, (_match, cssBlock: string) => {
      if (cssBlock && cssBlock.trim()) styles.push(cssBlock);
      return "";
    });

    src.replace(/<script[^>]*>([\s\S]*?)<\/script>/gi, (_match, jsBlock: string) => {
      if (jsBlock && jsBlock.trim()) scripts.push(jsBlock);
      return "";
    });

    let bodyHtml = src;
    const bodyMatch = /<body[^>]*>([\s\S]*?)<\/body>/i.exec(src);
    if (bodyMatch) {
      bodyHtml = bodyMatch[1];
    } else {
      bodyHtml = bodyHtml
        .replace(/<!doctype[^>]*>/gi, "")
        .replace(/<head[^>]*>[\s\S]*?<\/head>/gi, "")
        .replace(/<\/?(?:html|body|head|meta|title|link)[^>]*>/gi, "");
    }
    bodyHtml = bodyHtml
      .replace(/<style[^>]*>[\s\S]*?<\/style>/gi, "")
      .replace(/<script[^>]*>[\s\S]*?<\/script>/gi, "");

    return {
      html: bodyHtml,
      css: styles.join("\n"),
      js: scripts.join("\n;\n"),
    };
  }

  return {
    html: code.html,
    css: code.css,
    js: code.js,
  };
}

function scopeHudCss(rawCss: string, scopeAttr: string): string {
  const selector = `[data-sky-hud-scope="${scopeAttr}"]`;
  return rawCss.replace(
    /(^|[}\s,;])(?:html|body|:root)(?=\s*[,{])/gi,
    `$1${selector}`,
  );
}

function readDotPath(target: unknown, path: string): unknown {
  if (!target || !path) return undefined;
  const clean = path.replace(/^player\./, "");
  const parts = clean.split(".");
  let cur: unknown = target;
  for (const part of parts) {
    if (cur === null || cur === undefined || typeof cur !== "object") return undefined;
    cur = (cur as Record<string, unknown>)[part];
  }
  return cur;
}

function applyDeclarativeBindings(root: HTMLElement, playerObj: Record<string, unknown>): void {
  const bindNodes = root.querySelectorAll("[data-sky-bind]");
  for (let i = 0; i < bindNodes.length; i += 1) {
    const el = bindNodes[i] as HTMLElement;
    const path = el.getAttribute("data-sky-bind") || "";
    const val = readDotPath(playerObj, path);
    if (val !== undefined && val !== null) {
      el.textContent = typeof val === "number" ? Math.round(val).toLocaleString() : String(val);
    }
  }

  const barNodes = root.querySelectorAll("[data-sky-bar]");
  for (let j = 0; j < barNodes.length; j += 1) {
    const bar = barNodes[j] as HTMLElement;
    const poolKey = bar.getAttribute("data-sky-bar") || "health";
    const pool = playerObj[poolKey] as { current?: number; max?: number } | undefined;
    if (pool && typeof pool === "object") {
      const cur = Number(pool.current ?? 0);
      const max = Math.max(1, Number(pool.max ?? 100));
      const pct = Math.max(0, Math.min(100, (cur / max) * 100));
      bar.style.width = `${pct.toFixed(1)}%`;
    }
  }
}

export function CustomHudSandbox({
  code,
  player,
  nearbyActors = [],
  onLog,
  className = "",
}: {
  code: SkyUltraCustomHudCode;
  player: Player | null;
  nearbyActors?: Npc[];
  onLog?: (entry: CustomHudLogEntry) => void;
  className?: string;
}): React.JSX.Element {
  const scopeId = React.useId().replace(/[^a-zA-Z0-9_-]/g, "");
  const hostRef = useRef<HTMLDivElement | null>(null);
  const onLogRef = useRef(onLog);
  onLogRef.current = onLog;

  const enrichedPlayer = useMemo<Record<string, unknown>>(() => {
    if (!player) return DEFAULT_FALLBACK_PLAYER;
    return {
      ...DEFAULT_FALLBACK_PLAYER,
      ...player,
      angleZ: player.heading ?? 0,
    };
  }, [player]);

  const latestDataRef = useRef<{ player: Record<string, unknown>; nearbyActors: Npc[] }>({
    player: enrichedPlayer,
    nearbyActors,
  });
  latestDataRef.current = { player: enrichedPlayer, nearbyActors };

  const runtimeRef = useRef<{
    listeners: HudUpdateListener[];
    bridgeEvents: Record<string, Array<(payload: unknown) => void>>;
    onUpdateHook: HudUpdateListener | null;
  }>({
    listeners: [],
    bridgeEvents: {},
    onUpdateHook: null,
  });

  const emitLog = (level: "log" | "warn" | "error", args: unknown[]): void => {
    if (!onLogRef.current) return;
    const text = args
      .map((item) => {
        if (typeof item === "string") return item;
        if (item instanceof Error) return item.message || String(item);
        try {
          return JSON.stringify(item);
        } catch {
          return String(item);
        }
      })
      .join(" ");
    const now = new Date();
    const time = `${String(now.getHours()).padStart(2, "0")}:${String(now.getMinutes()).padStart(2, "0")}:${String(now.getSeconds()).padStart(2, "0")}`;
    onLogRef.current({
      id: `${performance.now()}-${Math.random().toString(36).slice(2, 7)}`,
      level,
      message: text,
      time,
    });
  };

  const notifyAll = (): void => {
    const hostEl = hostRef.current;
    if (!hostEl) return;
    const currentPlayer = latestDataRef.current.player;
    const currentActors = latestDataRef.current.nearbyActors;

    applyDeclarativeBindings(hostEl, currentPlayer);

    const rt = runtimeRef.current;
    for (let i = 0; i < rt.listeners.length; i += 1) {
      try {
        rt.listeners[i](currentPlayer, currentActors);
      } catch (err) {
        emitLog("error", [err]);
      }
    }

    if (typeof rt.onUpdateHook === "function") {
      try {
        rt.onUpdateHook(currentPlayer, currentActors);
      } catch (err) {
        emitLog("error", [err]);
      }
    }

    const gameUpdateCbs = rt.bridgeEvents["skyultra.game.update"];
    if (Array.isArray(gameUpdateCbs)) {
      const payload = { available: true, snapshot: currentPlayer, delta: currentPlayer };
      for (let j = 0; j < gameUpdateCbs.length; j += 1) {
        try {
          gameUpdateCbs[j](payload);
        } catch (err) {
          emitLog("error", [err]);
        }
      }
    }
  };

  const parts = useMemo(() => extractHudParts(code), [code]);

  useEffect(() => {
    const hostEl = hostRef.current;
    if (!hostEl) return;

    const intervals: number[] = [];
    const timeouts: number[] = [];
    const rafs: number[] = [];
    const domListeners: Array<{ target: EventTarget; type: string; fn: EventListenerOrEventListenerObject }> = [];

    runtimeRef.current = {
      listeners: [],
      bridgeEvents: {},
      onUpdateHook: null,
    };

    const scopedCss = scopeHudCss(parts.css, scopeId);
    hostEl.innerHTML = `<style data-sky-custom-hud-css>${scopedCss}</style>${parts.html}`;

    const escapeCssId = (id: string): string => {
      if (typeof CSS !== "undefined" && typeof CSS.escape === "function") {
        return CSS.escape(id);
      }
      return id.replace(/([ #;?%&,.+*~':"!^$[\]()=>|/@])/g, "\\$1");
    };

    const scopedDocument = new Proxy(document, {
      get(target, prop) {
        if (prop === "getElementById") {
          return (id: string): HTMLElement | null => {
            if (!id) return null;
            return (
              hostEl.querySelector<HTMLElement>(`#${escapeCssId(id)}`) ??
              target.getElementById(id)
            );
          };
        }
        if (prop === "querySelector") {
          return (sel: string): Element | null => hostEl.querySelector(sel);
        }
        if (prop === "querySelectorAll") {
          return (sel: string): NodeListOf<Element> => hostEl.querySelectorAll(sel);
        }
        if (prop === "body" || prop === "documentElement") {
          return hostEl;
        }
        if (prop === "addEventListener") {
          return (
            type: string,
            listener: EventListenerOrEventListenerObject,
            options?: boolean | AddEventListenerOptions,
          ): void => {
            if (type === "DOMContentLoaded") {
              if (typeof listener === "function") {
                listener(new Event("DOMContentLoaded"));
              }
              return;
            }
            hostEl.addEventListener(type, listener, options);
            domListeners.push({ target: hostEl, type, fn: listener });
          };
        }
        const value = Reflect.get(target, prop);
        return typeof value === "function" ? value.bind(target) : value;
      },
    });

    const skyUltraHudApi = {
      get player() {
        return latestDataRef.current.player;
      },
      get nearbyActors() {
        return latestDataRef.current.nearbyActors;
      },
      onUpdate(fn: HudUpdateListener) {
        if (typeof fn !== "function") return () => {};
        runtimeRef.current.listeners.push(fn);
        try {
          fn(latestDataRef.current.player, latestDataRef.current.nearbyActors);
        } catch (err) {
          emitLog("error", [err]);
        }
        return () => {
          const idx = runtimeRef.current.listeners.indexOf(fn);
          if (idx >= 0) runtimeRef.current.listeners.splice(idx, 1);
        };
      },
    };

    const skyWebViewMock = {
      bridgeVersion: 1,
      on(name: string, cb: (payload: unknown) => void) {
        if (!runtimeRef.current.bridgeEvents[name]) {
          runtimeRef.current.bridgeEvents[name] = [];
        }
        runtimeRef.current.bridgeEvents[name].push(cb);
        return () => {
          const arr = runtimeRef.current.bridgeEvents[name] || [];
          const idx = arr.indexOf(cb);
          if (idx >= 0) arr.splice(idx, 1);
        };
      },
      off(name: string, cb: (payload: unknown) => void) {
        const arr = runtimeRef.current.bridgeEvents[name] || [];
        const idx = arr.indexOf(cb);
        if (idx >= 0) {
          arr.splice(idx, 1);
          return true;
        }
        return false;
      },
      request(name: string) {
        if (name === "game.getPlayerSnapshot") {
          return Promise.resolve(latestDataRef.current.player);
        }
        if (name === "game.subscribe") {
          return Promise.resolve({
            subscribed: true,
            sequence: 1,
            update: { snapshot: latestDataRef.current.player },
          });
        }
        if (name === "game.getNearbyNpcs" || name === "game.getNearbyActors") {
          return Promise.resolve({ results: latestDataRef.current.nearbyActors });
        }
        if (name === "ui.pageReady") {
          return Promise.resolve({ ready: true });
        }
        return Promise.resolve({});
      },
    };

    const scopedConsole = {
      log: (...args: unknown[]) => emitLog("log", args),
      info: (...args: unknown[]) => emitLog("log", args),
      warn: (...args: unknown[]) => emitLog("warn", args),
      error: (...args: unknown[]) => emitLog("error", args),
    };

    const trackedSetInterval = (fn: TimerHandler, ms?: number, ...args: unknown[]): number => {
      const id = window.setInterval(fn, ms, ...args);
      intervals.push(id);
      return id;
    };
    const trackedClearInterval = (id?: number): void => {
      if (id !== undefined) window.clearInterval(id);
    };
    const trackedSetTimeout = (fn: TimerHandler, ms?: number, ...args: unknown[]): number => {
      const id = window.setTimeout(fn, ms, ...args);
      timeouts.push(id);
      return id;
    };
    const trackedClearTimeout = (id?: number): void => {
      if (id !== undefined) window.clearTimeout(id);
    };
    const trackedRaf = (cb: FrameRequestCallback): number => {
      const id = window.requestAnimationFrame(cb);
      rafs.push(id);
      return id;
    };
    const trackedCancelRaf = (id: number): void => {
      window.cancelAnimationFrame(id);
    };

    const scopedWindow = new Proxy(window, {
      get(target, prop) {
        if (prop === "document") return scopedDocument;
        if (prop === "SkyUltraHud") return skyUltraHudApi;
        if (prop === "SkyWebView") return skyWebViewMock;
        if (prop === "console") return scopedConsole;
        if (prop === "setInterval") return trackedSetInterval;
        if (prop === "clearInterval") return trackedClearInterval;
        if (prop === "setTimeout") return trackedSetTimeout;
        if (prop === "clearTimeout") return trackedClearTimeout;
        if (prop === "requestAnimationFrame") return trackedRaf;
        if (prop === "cancelAnimationFrame") return trackedCancelRaf;
        if (prop === "onSkyUltraHudUpdate") return runtimeRef.current.onUpdateHook;
        if (prop === "addEventListener") {
          return (
            type: string,
            listener: EventListenerOrEventListenerObject,
            options?: boolean | AddEventListenerOptions,
          ): void => {
            if (type === "skyultra:hud-update") {
              const wrapped: HudUpdateListener = (p, a) => {
                const ev = new CustomEvent("skyultra:hud-update", {
                  detail: { player: p, nearbyActors: a },
                });
                if (typeof listener === "function") {
                  listener(ev);
                } else if (listener && typeof listener.handleEvent === "function") {
                  listener.handleEvent(ev);
                }
              };
              runtimeRef.current.listeners.push(wrapped);
              return;
            }
            target.addEventListener(type, listener, options);
            domListeners.push({ target, type, fn: listener });
          };
        }
        const value = Reflect.get(target, prop);
        return typeof value === "function" ? value.bind(target) : value;
      },
      set(target, prop, value) {
        if (prop === "onSkyUltraHudUpdate") {
          runtimeRef.current.onUpdateHook = typeof value === "function" ? (value as HudUpdateListener) : null;
          return true;
        }
        return Reflect.set(target, prop, value);
      },
    });

    if (parts.js.trim()) {
      try {
        const runner = new Function(
          "window",
          "document",
          "SkyUltraHud",
          "SkyWebView",
          "console",
          "setInterval",
          "clearInterval",
          "setTimeout",
          "clearTimeout",
          "requestAnimationFrame",
          "cancelAnimationFrame",
          parts.js,
        );
        runner(
          scopedWindow,
          scopedDocument,
          skyUltraHudApi,
          skyWebViewMock,
          scopedConsole,
          trackedSetInterval,
          trackedClearInterval,
          trackedSetTimeout,
          trackedClearTimeout,
          trackedRaf,
          trackedCancelRaf,
        );
      } catch (err) {
        emitLog("error", [err]);
      }
    }

    notifyAll();

    return () => {
      for (const id of intervals) window.clearInterval(id);
      for (const id of timeouts) window.clearTimeout(id);
      for (const id of rafs) window.cancelAnimationFrame(id);
      for (const item of domListeners) {
        item.target.removeEventListener(item.type, item.fn);
      }
      runtimeRef.current.listeners = [];
      runtimeRef.current.bridgeEvents = {};
      runtimeRef.current.onUpdateHook = null;
    };
  }, [parts, scopeId]);

  useEffect(() => {
    notifyAll();
  }, [enrichedPlayer, nearbyActors]);

  return (
    <div
      ref={hostRef}
      data-sky-hud-scope={scopeId}
      className={("sky-custom-hud-frame " + className).trim()}
      style={{
        position: "absolute",
        inset: 0,
        width: "100%",
        height: "100%",
        border: 0,
        background: "transparent",
        overflow: "hidden",
        pointerEvents: "none",
        userSelect: "none",
        transform: "translateZ(0)",
      }}
    />
  );
}
