import { useEffect, useMemo, useRef, useState } from "react";
import {
  getSkyWebView,
  markSkyUltraPageReady,
  sanitizeCustomHudCode,
  type SkyUltraCustomHudCode,
  type SkyUltraGameTopic,
  type SkyUltraWidgetAnchor,
  type SkyUltraWidgetPlacement,
} from "@skyultra/sdk";
import { SkyThemeScope } from "./core.js";
import {
  HudWidget,
  decodePlacementsParam,
  defaultWidgetPlacements,
  evaluateHudVisibilityRule,
  hudAnchorOptions,
  widgetCatalog,
  type ResolvedWidgetPlacement,
  type WidgetKind,
} from "./field/hudWidgets.js";
import { CustomHudSandbox } from "./field/CustomHudSandbox.js";
import { usePlayer } from "./field/useFieldData.js";
import { palettes, type Npc, type ActorResponse } from "./field/model.js";

interface LiveHudConfigPayload {
  available?: boolean;
  enabled?: boolean;
  widgets?: string[];
  anchor?: string;
  scale?: number;
  opacity?: number;
  theme?: string;
  glow?: number;
  colors?: Record<string, string>;
  placements?: Partial<Record<WidgetKind, Partial<SkyUltraWidgetPlacement>>>;
  customCode?: unknown;
}

const validAnchors = new Set<string>(hudAnchorOptions.map((a) => a.value));

export function SkyGameplayHud() {
  const bridge = useMemo(() => getSkyWebView(), []);
  const idle = useRef(null);
  const options = useMemo(() => new URLSearchParams(location.search), []);

  const initialKinds = useMemo(
    () =>
      (options.get("widgets") ?? "compass,vitals")
        .split(",")
        .filter((value): value is WidgetKind => widgetCatalog.some((widget) => widget.id === value))
        .slice(0, 16),
    [options],
  );

  const initialColors = useMemo(() => {
    const map: Partial<Record<WidgetKind, string>> = {};
    const raw = options.get("colors") ?? "";
    for (const pair of raw.split(",")) {
      const [id, hex] = pair.split(":");
      if (id && hex && /^[0-9a-f]{3,6}$/i.test(hex)) {
        map[id as WidgetKind] = `#${hex}`;
      }
    }
    return map;
  }, [options]);

  const initialGlow = useMemo(() => {
    const raw = Number(options.get("glow") ?? "100");
    return Number.isFinite(raw) ? Math.max(0, Math.min(150, raw)) : 100;
  }, [options]);

  const fallbackAnchor = useMemo<SkyUltraWidgetAnchor>(() => {
    const side = options.get("side") ?? "right";
    const align = options.get("align") ?? "top";
    if (side === "center") return align === "bottom" ? "bottom-center" : "top-center";
    if (side === "left") return align === "bottom" ? "bottom-left" : "top-left";
    return align === "bottom" ? "bottom-right" : "top-right";
  }, [options]);

  const initialPlacements = useMemo(
    () =>
      decodePlacementsParam(
        options.get("placements"),
        options.get("placements") ? undefined : fallbackAnchor,
        undefined,
        undefined,
        initialGlow,
        initialColors,
      ),
    [options, fallbackAnchor, initialGlow, initialColors],
  );

  const [liveConfig, setLiveConfig] = useState<LiveHudConfigPayload | null>(null);

  useEffect(() => {
    let active = true;
    const applyPayload = (payload: LiveHudConfigPayload | null | undefined): void => {
      if (!active || !payload || typeof payload !== "object" || payload.enabled === false) return;
      setLiveConfig(payload);
    };
    void bridge
      .request<LiveHudConfigPayload>("ui.configureWidgets", { query: true })
      .then(applyPayload)
      .catch(() => undefined);

    const off = bridge.on<LiveHudConfigPayload>("skyultra.hud.config", (payload) => {
      applyPayload(payload);
    });
    return () => {
      active = false;
      off();
    };
  }, [bridge]);

  const kinds = useMemo<WidgetKind[]>(() => {
    if (Array.isArray(liveConfig?.widgets) && liveConfig.widgets.length) {
      const filtered = liveConfig.widgets.filter((value): value is WidgetKind =>
        widgetCatalog.some((widget) => widget.id === value),
      );
      if (filtered.length) return filtered.slice(0, 16);
    }
    return initialKinds;
  }, [liveConfig?.widgets, initialKinds]);

  const customCode = useMemo<SkyUltraCustomHudCode | null>(() => {
    if (!liveConfig?.customCode) return null;
    const parsed = sanitizeCustomHudCode(liveConfig.customCode);
    return parsed.enabled ? parsed : null;
  }, [liveConfig?.customCode]);

  const hudTopics = useMemo<SkyUltraGameTopic[]>(() => {
    if (customCode) {
      return ["vitals", "equipment", "inventory", "effects", "quests", "followers"];
    }
    const set = new Set<SkyUltraGameTopic>(["vitals"]);
    if (
      kinds.includes("equipment") ||
      kinds.includes("stealth") ||
      kinds.includes("armory") ||
      kinds.includes("character")
    ) {
      set.add("equipment");
    }
    if (kinds.includes("effects") || kinds.includes("wards")) {
      set.add("effects");
    }
    if (kinds.includes("loot") || kinds.includes("resources")) {
      set.add("inventory");
    }
    return [...set];
  }, [kinds, customCode]);

  const { player } = usePlayer(bridge, idle, "hud", hudTopics);

  useEffect(() => {
    if (!player) return;
    let second = 0;
    const first = requestAnimationFrame(() => {
      second = requestAnimationFrame(() => void markSkyUltraPageReady(bridge).catch(() => undefined));
    });
    return () => {
      cancelAnimationFrame(first);
      cancelAnimationFrame(second);
    };
  }, [bridge, !!player]);

  const colors = useMemo<Partial<Record<WidgetKind, string>>>(() => {
    if (liveConfig?.colors && typeof liveConfig.colors === "object") {
      const next: Partial<Record<WidgetKind, string>> = { ...initialColors };
      for (const [k, v] of Object.entries(liveConfig.colors)) {
        if (typeof v === "string" && /^#[0-9a-f]{3,6}$/i.test(v)) {
          next[k as WidgetKind] = v;
        }
      }
      return next;
    }
    return initialColors;
  }, [liveConfig?.colors, initialColors]);

  const glow = useMemo(() => {
    if (typeof liveConfig?.glow === "number" && Number.isFinite(liveConfig.glow)) {
      return Math.max(0, Math.min(150, liveConfig.glow));
    }
    return initialGlow;
  }, [liveConfig?.glow, initialGlow]);

  const placements = useMemo<Partial<Record<WidgetKind, ResolvedWidgetPlacement>>>(() => {
    if (liveConfig?.placements && typeof liveConfig.placements === "object") {
      const defaults = defaultWidgetPlacements();
      const next: Partial<Record<WidgetKind, ResolvedWidgetPlacement>> = { ...defaults, ...initialPlacements };
      for (const [k, raw] of Object.entries(liveConfig.placements) as Array<
        [WidgetKind, Partial<SkyUltraWidgetPlacement> | undefined]
      >) {
        if (!raw || !defaults[k]) continue;
        const base = next[k] ?? defaults[k];
        next[k] = {
          anchor: raw.anchor && validAnchors.has(raw.anchor) ? raw.anchor : base.anchor,
          scale: typeof raw.scale === "number" ? Math.max(0.7, Math.min(1.5, raw.scale)) : base.scale,
          opacity: typeof raw.opacity === "number" ? Math.max(0.35, Math.min(1, raw.opacity)) : base.opacity,
          glow: typeof raw.glow === "number" ? Math.max(0, Math.min(150, raw.glow)) : base.glow,
          color: typeof raw.color === "string" ? raw.color : base.color,
          fontStyle: raw.fontStyle ?? base.fontStyle,
          offsetX: typeof raw.offsetX === "number" ? Math.max(-600, Math.min(600, Math.round(raw.offsetX))) : 0,
          offsetY: typeof raw.offsetY === "number" ? Math.max(-400, Math.min(400, Math.round(raw.offsetY))) : 0,
          visibilityRule: raw.visibilityRule ?? base.visibilityRule ?? "always",
          width: typeof raw.width === "number" ? raw.width : base.width,
          customLabel: typeof raw.customLabel === "string" ? raw.customLabel : base.customLabel,
          bindingPath: typeof raw.bindingPath === "string" ? raw.bindingPath : base.bindingPath,
        };
      }
      return next;
    }
    return initialPlacements;
  }, [liveConfig?.placements, initialPlacements]);

  const [nearbyActors, setNearbyActors] = useState<Npc[]>([]);
  const needsNearbyActors = Boolean(customCode) || kinds.includes("radar") || kinds.includes("target");
  useEffect(() => {
    if (!needsNearbyActors) return;
    let active = true;
    const poll = (): void => {
      void bridge
        .request<ActorResponse>("game.getNearbyNpcs", { radius: 3000 })
        .then((res) => {
          if (active && Array.isArray(res?.results)) setNearbyActors(res.results.slice(0, 8));
        })
        .catch(() => undefined);
    };
    poll();
    const timer = window.setInterval(poll, 2500);
    return () => {
      active = false;
      window.clearInterval(timer);
    };
  }, [bridge, needsNearbyActors]);

  const theme = liveConfig?.theme ?? options.get("theme") ?? "dragonstone";

  if (!player || player.available === false) {
    return null;
  }

  return (
    <SkyThemeScope
      theme={palettes.some((palette) => palette.value === theme) ? theme : "dragonstone"}
      className="hud-root hud-root--six-zone"
      data-density={kinds.length > 5 ? "compact" : "comfortable"}
    >
      {customCode ? (
        <CustomHudSandbox code={customCode} player={player} nearbyActors={nearbyActors} />
      ) : (
        <div className="sky-hud-screen-grid">
          {hudAnchorOptions.map((zone) => {
            const zoneKinds = kinds.filter((kind) => (placements[kind]?.anchor ?? fallbackAnchor) === zone.value);
            if (!zoneKinds.length) return null;
            return (
              <div key={zone.value} className={`sky-hud-zone sky-hud-zone--${zone.value}`}>
                {zoneKinds.map((kind) => {
                  const wp = placements[kind];
                  if (!evaluateHudVisibilityRule(player, wp?.visibilityRule)) return null;
                  const ox = wp?.offsetX ?? 0;
                  const oy = wp?.offsetY ?? 0;
                  const sc = wp?.scale ?? 1;
                  return (
                    <div
                      key={kind}
                      className="sky-hud-zone__item"
                      style={{
                        opacity: wp?.opacity ?? 0.96,
                        transform: `translate(${ox}px, ${oy}px) scale(${sc})`,
                      }}
                    >
                      <HudWidget
                        kind={kind}
                        player={player}
                        bridge={bridge}
                        nearbyActors={nearbyActors}
                        accentColor={wp?.color ?? colors[kind]}
                        glow={wp?.glow ?? glow}
                        fontStyle={wp?.fontStyle ?? "theme"}
                        customLabel={wp?.customLabel}
                        bindingPath={wp?.bindingPath}
                      />
                    </div>
                  );
                })}
              </div>
            );
          })}
        </div>
      )}
    </SkyThemeScope>
  );
}
