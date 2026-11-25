import * as React from "react";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  SKYULTRA_COMPASS_RADIUS_DEFAULT,
  SKYULTRA_COMPASS_RADIUS_MAX,
  SKYULTRA_COMPASS_RADIUS_MIN,
  SKYULTRA_HUD_CODE_TEMPLATES,
  coerceLegacyHudLayout,
  exportSkyUltraHudMod,
  generateCustomHudCodeFromLayout,
  generateSkyUltraHudModFiles,
  persistSkyUltraProfileStore,
  readLocalSkyUltraProfileStore,
  resolveEffectiveSkyUltraProfile,
  sanitizeCustomHudCode,
  sanitizeSkyUltraModId,
  skyUltraPortraitCrestStyles,
  snapSkyUltraHudCoordinate,
  updateProfileForContext,
  type SkyUltraCustomHudCode,
  type SkyUltraPortraitCrestStyle,
  type SkyUltraProfileStoreV2,
  type SkyUltraWidgetAnchor,
  type SkyWebViewAPI,
  type SkyUltraWidgetColors,
  type SkyUltraWidgetPlacement,
  type SkyUltraWidgetPlacements,
} from "@skyultra/sdk";
import { SkyRangeSlider, SkySegmentedControl, SkyToggleSwitch, SkyThemeScope } from "../core.js";
import { SkyColorWheel } from "../ColorWheel.js";
import {
  SkyActionWheel,
  SkyCharacterCard,
  SkyFrame,
  SkyKeybind,
  SkyNotificationStack,
  SkyPagination,
  SkyPopup,
  SkyQuestTracker,
  SkyStepGuide,
  SkyTimeline,
  type SkyNotification,
} from "../widgets.js";
import { SkyHudComposer } from "../composer.js";
import { Icon } from "./icons.js";
import {
  HudWidget,
  defaultWidgetPlacements,
  evaluateHudVisibilityRule,
  hudAnchorOptions,
  hudBindingOptions,
  hudCrestStyleOptions,
  hudFontOptions,
  hudVisibilityOptions,
  resolveHudBindingValue,
  widgetCatalog,
  type ResolvedWidgetPlacement,
  type WidgetKind,
} from "./hudWidgets.js";
import { palettes, type Npc, type ActorResponse, type Palette, type Player, type Route } from "./model.js";
import { CustomHudSandbox, type CustomHudLogEntry } from "./CustomHudSandbox.js";

interface OverlayState {
  ready?: boolean;
  available: boolean;
  enabled?: boolean;
  widgets?: WidgetKind[];
  anchor?: string;
  scale?: number;
  opacity?: number;
  theme?: string;
  placements?: Partial<Record<WidgetKind, Partial<SkyUltraWidgetPlacement>>>;
  customCode?: SkyUltraCustomHudCode;
  message?: string;
}

const presets: Array<{
  name: string;
  tag: string;
  description: string;
  items: WidgetKind[];
  placements: Partial<Record<WidgetKind, Partial<SkyUltraWidgetPlacement>>>;
}> = [
  {
    name: "Dragonborn Sovereign",
    tag: "MULTI-ZONE COMMAND",
    description: "Top-Center compass, Bottom-Center radial vitals, Top-Left 3D portrait crest & Top-Right proximity radar.",
    items: ["compass", "vitals", "character", "radar"],
    placements: {
      compass: { anchor: "top-center", color: "#e8b059", scale: 1, fontStyle: "serif" },
      vitals: { anchor: "bottom-center", color: "#ff5c6c", scale: 1, fontStyle: "serif" },
      character: { anchor: "top-left", color: "#e8b059", scale: 0.95, fontStyle: "serif" },
      radar: { anchor: "top-right", color: "#39ff14", scale: 0.95, fontStyle: "mono" },
    },
  },
  {
    name: "Neon Tactician",
    tag: "COMBAT & TARGET LOCK",
    description: "Radial vitals at bottom-center, target lock & compass at top-center, radar top-right, loadout bottom-left.",
    items: ["compass", "target", "vitals", "radar", "stealth"],
    placements: {
      compass: { anchor: "top-center", color: "#00f0ff", fontStyle: "display" },
      target: { anchor: "top-center", color: "#ff2e93", fontStyle: "display" },
      vitals: { anchor: "bottom-center", color: "#ff2e93", fontStyle: "display" },
      radar: { anchor: "top-right", color: "#39ff14", fontStyle: "mono" },
      stealth: { anchor: "bottom-left", color: "#ffb95c", fontStyle: "display" },
    },
  },
  {
    name: "Arch-Mage Ward",
    tag: "ARCANE TELEMETRY",
    description: "Elemental Ward matrix, signature skills, radial vitals, and 3D character portrait crest.",
    items: ["character", "skills", "vitals", "wards"],
    placements: {
      character: { anchor: "top-left", color: "#be8cff", fontStyle: "serif" },
      skills: { anchor: "bottom-left", color: "#a78bfa", fontStyle: "serif" },
      vitals: { anchor: "bottom-center", color: "#00f0ff", fontStyle: "serif" },
      wards: { anchor: "bottom-right", color: "#ff2e93", fontStyle: "data" },
    },
  },
  {
    name: "Treasure & Armory",
    tag: "EXPEDITION",
    description: "Horizon bearing, carry-weight & Septim ticker, 11-slot armory matrix, and Tamrielic sundial.",
    items: ["compass", "armory", "loot", "clock"],
    placements: {
      compass: { anchor: "top-center", color: "#52e6d1", fontStyle: "data" },
      armory: { anchor: "bottom-left", color: "#38bdf8", fontStyle: "data" },
      loot: { anchor: "bottom-right", color: "#dfa84e", fontStyle: "data" },
      clock: { anchor: "top-right", color: "#70caff", fontStyle: "serif" },
    },
  },
  {
    name: "Engine Benchmarker",
    tag: "PERFORMANCE",
    description: "Live FPS & frame-time monitor at top-right paired with radial vitals and target scanner.",
    items: ["telemetry", "vitals", "target"],
    placements: {
      telemetry: { anchor: "top-right", color: "#39ff14", fontStyle: "mono" },
      vitals: { anchor: "bottom-center", color: "#ff1744", fontStyle: "mono" },
      target: { anchor: "top-left", color: "#00f0ff", fontStyle: "mono" },
    },
  },
];

const HUD_PREFS_KEY = "skyultra.hudStudio.v3";
const HUD_CUSTOM_CODE_KEY = "skyultra.customHudCode.v1";

const validAnchorSet = new Set<string>(hudAnchorOptions.map((a) => a.value));

export default function WidgetsPage({
  player,
  bridge,
  native,
  theme,
  onNavigate,
  onInspectMod,
  onPinnedChange,
  showPatterns = true,
}: {
  player: Player | null;
  bridge: SkyWebViewAPI;
  native: boolean;
  theme: Palette;
  onNavigate(route: Route): void;
  onInspectMod?(modId: string): void;
  onPinnedChange(count: number): void;
  showPatterns?: boolean;
}): React.JSX.Element {
  const operation = useRef(0);
  const mounted = useRef(true);
  const applying = useRef<AbortController | null>(null);
  const hasSavedHudRecordRef = useRef(false);
  const didHydrateRef = useRef(false);
  const selfPersistingRef = useRef(false);
  const userChangedHudThemeRef = useRef(false);
  const stageHostRef = useRef<HTMLDivElement | null>(null);
  const [stageScale, setStageScale] = useState(0.42);
  const widgetNodeRefs = useRef<Partial<Record<WidgetKind, HTMLDivElement | null>>>({});
  const dragBadgeRefs = useRef<Partial<Record<WidgetKind, HTMLSpanElement | null>>>({});
  useEffect(() => {
    mounted.current = true;
    return () => {
      mounted.current = false;
      operation.current++;
      applying.current?.abort();
    };
  }, []);

  const [tab, setTab] = useState<"gallery" | "editor" | "presets" | "export" | "patterns">("gallery");
  const [categoryFilter, setCategoryFilter] = useState<"All" | "Combat" | "Navigation" | "Character" | "System">("All");
  const [selected, setSelected] = useState<WidgetKind[]>(() => {
    try {
      const profileResolved = resolveEffectiveSkyUltraProfile(readLocalSkyUltraProfileStore(), player?.playerName);
      if (profileResolved.hudLayout && Array.isArray(profileResolved.hudLayout.widgets)) {
        hasSavedHudRecordRef.current = true;
        return profileResolved.hudLayout.widgets.filter((id) => widgetCatalog.some((w) => w.id === id)) as WidgetKind[];
      }
      const saved = window.localStorage.getItem(HUD_PREFS_KEY);
      if (saved) {
        const parsed = JSON.parse(saved) as { widgets?: WidgetKind[] };
        if (parsed && Array.isArray(parsed.widgets)) {
          hasSavedHudRecordRef.current = true;
          return parsed.widgets.filter((id) => widgetCatalog.some((w) => w.id === id));
        }
      }
    } catch {
      // Ignore storage errors
    }
    return ["compass", "vitals", "character", "radar"];
  });
  const [activeWidget, setActiveWidget] = useState<WidgetKind>(() =>
    selected.includes("compass") ? "compass" : selected[0] ?? "vitals",
  );
  const [placements, setPlacements] = useState<Record<WidgetKind, ResolvedWidgetPlacement>>(() =>
    defaultWidgetPlacements(),
  );
  const [compassRadiusDraft, setCompassRadiusDraft] = useState<string>(() =>
    String(placements.compass?.compassRadius ?? SKYULTRA_COMPASS_RADIUS_DEFAULT),
  );
  useEffect(() => {
    setCompassRadiusDraft(String(placements.compass?.compassRadius ?? SKYULTRA_COMPASS_RADIUS_DEFAULT));
  }, [placements.compass?.compassRadius]);
  const [snapGrid, setSnapGrid] = useState<0 | 8 | 16 | 24>(8);
  const [dragHudState, setDragHudState] = useState<{
    kind: WidgetKind;
    mode: "move" | "resize";
    hoverZone: SkyUltraWidgetAnchor | null;
  } | null>(null);
  const dragHudRef = useRef<{
    kind: WidgetKind;
    mode: "move" | "resize";
    pointerId: number;
    startX: number;
    startY: number;
    baseOffsetX: number;
    baseOffsetY: number;
    baseScale: number;
    liveOffsetX: number;
    liveOffsetY: number;
    liveScale: number;
    hoverZone: SkyUltraWidgetAnchor | null;
    rafId: number;
  } | null>(null);

  const [customHudCode, setCustomHudCode] = useState<SkyUltraCustomHudCode>(() => {
    try {
      const saved = window.localStorage.getItem(HUD_CUSTOM_CODE_KEY);
      if (saved) return sanitizeCustomHudCode(JSON.parse(saved));
    } catch {
      // Ignore storage errors
    }
    return sanitizeCustomHudCode(null);
  });
  const [debouncedHudCode, setDebouncedHudCode] = useState<SkyUltraCustomHudCode>(customHudCode);
  const [editorCodeTab, setEditorCodeTab] = useState<"html" | "css" | "js">("html");
  const [editorStageBg, setEditorStageBg] = useState<"black" | "white" | "transparent">("black");
  const [hudConsoleLogs, setHudConsoleLogs] = useState<CustomHudLogEntry[]>([]);
  const [simTelemetry, setSimTelemetry] = useState<{
    active: boolean;
    hpPct: number;
    mpPct: number;
    spPct: number;
    heading: number;
  }>({ active: false, hpPct: 68, mpPct: 82, spPct: 90, heading: 45 });

  useEffect(() => {
    const timer = window.setTimeout(() => {
      setDebouncedHudCode(customHudCode);
      try {
        window.localStorage.setItem(HUD_CUSTOM_CODE_KEY, JSON.stringify(customHudCode));
      } catch {
        // Ignore storage errors
      }
    }, 120);
    return () => window.clearTimeout(timer);
  }, [customHudCode]);

  const appendHudConsoleLog = React.useCallback((entry: CustomHudLogEntry) => {
    setHudConsoleLogs((prev) => [...prev.slice(-39), entry]);
  }, []);

  const editorPreviewPlayer = useMemo<Player | null>(() => {
    const base: Player = player ?? {
      available: true,
      playerName: "Degdatar",
      level: 48,
      location: "Whiterun",
      heading: 35,
      gold: 24500,
      armorRating: 580,
      activeEffectCount: 5,
      followerCount: 1,
      inventoryItemCount: 84,
      inventoryTypeCount: 42,
      inventoryTruncated: false,
      health: { current: 310, max: 350 },
      magicka: { current: 200, max: 240 },
      stamina: { current: 260, max: 280 },
      carryWeight: { current: 145, max: 400 },
      equipment: {
        head: null,
        body: null,
        hands: null,
        feet: null,
        mainHand: null,
        offHand: null,
      },
      inventory: [],
    };
    if (!simTelemetry.active) return base;
    const hpMax = Math.max(100, base.health.max || 350);
    const mpMax = Math.max(100, base.magicka.max || 240);
    const spMax = Math.max(100, base.stamina.max || 280);
    return {
      ...base,
      heading: simTelemetry.heading,
      health: { current: Math.round((hpMax * simTelemetry.hpPct) / 100), max: hpMax },
      magicka: { current: Math.round((mpMax * simTelemetry.mpPct) / 100), max: mpMax },
      stamina: { current: Math.round((spMax * simTelemetry.spPct) / 100), max: spMax },
    };
  }, [player, simTelemetry]);

  const [exportModId, setExportModId] = useState("custom_hud");
  const [exportModName, setExportModName] = useState("Dragonborn Tactical HUD");
  const [exportModVersion, setExportModVersion] = useState("1.0.0");
  const [exportModAuthor, setExportModAuthor] = useState("Dragonborn");
  const [exportModHotkey, setExportModHotkey] = useState("F8");
  const [exportActiveFile, setExportActiveFile] = useState("manifest.json");
  const [exportStatus, setExportStatus] = useState("");
  const [exportBusy, setExportBusy] = useState(false);
  const [layoutTheme, setLayoutTheme] = useState<string>(theme || "dragonstone");
  useEffect(() => {
    if (theme && !hasSavedHudRecordRef.current && !userChangedHudThemeRef.current) {
      setLayoutTheme(theme);
    }
  }, [theme]);
  const [stageMode, setStageMode] = useState<"viewport" | "inspector">("viewport");
  useEffect(() => {
    const host = stageHostRef.current;
    if (!host || stageMode !== "viewport" || tab !== "gallery") return;
    const measure = (): void => {
      const rect = host.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) return;
      const next = Math.max(0.24, Math.min(1, Math.min((rect.width - 16) / 1920, (rect.height - 16) / 1080)));
      setStageScale((prev) => (Math.abs(prev - next) > 0.002 ? next : prev));
    };
    measure();
    const ro = typeof ResizeObserver !== "undefined" ? new ResizeObserver(measure) : null;
    ro?.observe(host);
    window.addEventListener("resize", measure);
    return () => {
      ro?.disconnect();
      window.removeEventListener("resize", measure);
    };
  }, [stageMode, tab]);
  const [inspectorTab, setInspectorTab] = useState<"transform" | "binding" | "style">("transform");
  const [showSafeGuides, setShowSafeGuides] = useState(false);
  const [showLayoutJson, setShowLayoutJson] = useState(false);
  const [layoutJsonDraft, setLayoutJsonDraft] = useState("");
  const [enabled, setEnabled] = useState(false);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const [nearbyActors, setNearbyActors] = useState<Npc[]>([]);

  const [popup, setPopup] = useState<"confirm" | "quantity" | "transaction" | "picker" | "guide" | null>(null);
  const [quantity, setQuantity] = useState(5);
  const [pickQuery, setPickQuery] = useState("");
  const [pickPage, setPickPage] = useState(1);
  const [guideStep, setGuideStep] = useState(0);
  const [notices, setNotices] = useState<SkyNotification[]>([]);
  const notify = (title: string, description?: string): void => {
    const id = `${performance.now()}-${Math.random().toString(36).slice(2, 7)}`;
    setNotices((current) => [{ id, title, description }, ...current].slice(0, 3));
    window.setTimeout(() => {
      setNotices((current) => current.filter((notice) => notice.id !== id));
    }, 3000);
  };
  useEffect(() => {
    if (!message) return;
    const timer = window.setTimeout(() => setMessage(""), 3000);
    return () => window.clearTimeout(timer);
  }, [message]);

  useEffect(() => {
    const applySavedHudRecord = (parsed: {
      widgets?: WidgetKind[];
      placements?: Partial<Record<WidgetKind, Partial<SkyUltraWidgetPlacement>>>;
      theme?: string;
      snapGrid?: number;
    } | null): void => {
      if (!parsed) return;
      if (Array.isArray(parsed.widgets)) {
        hasSavedHudRecordRef.current = true;
        const valid = parsed.widgets.filter((id) => widgetCatalog.some((w) => w.id === id));
        setSelected(valid);
      }
      if (typeof parsed.snapGrid === "number" && [0, 8, 16, 24].includes(parsed.snapGrid)) {
        setSnapGrid(parsed.snapGrid as 0 | 8 | 16 | 24);
      }
      if (parsed.placements && typeof parsed.placements === "object") {
        setPlacements(() => {
          const next = defaultWidgetPlacements();
          for (const key of Object.keys(parsed.placements!) as WidgetKind[]) {
            if (!next[key]) continue;
            const p = parsed.placements![key];
            if (!p) continue;
            next[key] = {
              anchor: p.anchor && validAnchorSet.has(p.anchor) ? p.anchor : next[key].anchor,
              scale: typeof p.scale === "number" ? Math.max(0.7, Math.min(1.5, p.scale)) : next[key].scale,
              opacity: typeof p.opacity === "number" ? Math.max(0.35, Math.min(1, p.opacity)) : next[key].opacity,
              glow: typeof p.glow === "number" ? Math.max(0, Math.min(150, p.glow)) : next[key].glow,
              color: typeof p.color === "string" ? p.color : next[key].color,
              fontStyle: p.fontStyle ?? next[key].fontStyle,
              theme: typeof p.theme === "string" && palettes.some((pal) => pal.value === p.theme) ? p.theme : undefined,
              offsetX: typeof p.offsetX === "number" ? Math.max(-1600, Math.min(1600, Math.round(p.offsetX))) : 0,
              offsetY: typeof p.offsetY === "number" ? Math.max(-900, Math.min(900, Math.round(p.offsetY))) : 0,
              visibilityRule: p.visibilityRule ?? "always",
              width: typeof p.width === "number" ? p.width : undefined,
              customLabel: typeof p.customLabel === "string" && p.customLabel.trim() ? p.customLabel.trim() : undefined,
              bindingPath: p.bindingPath,
              crestStyle:
                p.crestStyle && (skyUltraPortraitCrestStyles as readonly string[]).includes(p.crestStyle)
                  ? (p.crestStyle as SkyUltraPortraitCrestStyle)
                  : next[key].crestStyle,
              hpColor: typeof p.hpColor === "string" ? p.hpColor : undefined,
              magickaColor: typeof p.magickaColor === "string" ? p.magickaColor : undefined,
              staminaColor: typeof p.staminaColor === "string" ? p.staminaColor : undefined,
              compassRadius:
                key === "compass"
                  ? typeof p.compassRadius === "number" && Number.isFinite(p.compassRadius)
                    ? Math.max(
                        SKYULTRA_COMPASS_RADIUS_MIN,
                        Math.min(SKYULTRA_COMPASS_RADIUS_MAX, Math.round(p.compassRadius)),
                      )
                    : next[key].compassRadius
                  : undefined,
            };
          }
          return next;
        });
      }
      if (!userChangedHudThemeRef.current && parsed.theme && palettes.some((p) => p.value === parsed.theme)) {
        setLayoutTheme(parsed.theme);
      }
    };

    try {
      const profileResolved = resolveEffectiveSkyUltraProfile(readLocalSkyUltraProfileStore(), player?.playerName);
      if (profileResolved.hudLayout) {
        applySavedHudRecord(profileResolved.hudLayout as unknown as {
          widgets?: WidgetKind[];
          placements?: Partial<Record<WidgetKind, Partial<SkyUltraWidgetPlacement>>>;
          theme?: string;
          snapGrid?: number;
        });
      } else {
        const saved = window.localStorage.getItem(HUD_PREFS_KEY);
        if (saved) {
          applySavedHudRecord(JSON.parse(saved));
        }
      }
    } catch {
      // Ignore storage errors
    }

    const onProfileStore = (event: Event): void => {
      if (selfPersistingRef.current) return;
      const detail = (event as CustomEvent<SkyUltraProfileStoreV2>).detail;
      if (!detail) return;
      const resolved = resolveEffectiveSkyUltraProfile(detail, player?.playerName);
      if (resolved.hudLayout) {
        applySavedHudRecord(resolved.hudLayout as unknown as {
          widgets?: WidgetKind[];
          placements?: Partial<Record<WidgetKind, Partial<SkyUltraWidgetPlacement>>>;
          theme?: string;
          snapGrid?: number;
        });
      }
    };
    window.addEventListener("skyultra:profile-store", onProfileStore);
    return () => window.removeEventListener("skyultra:profile-store", onProfileStore);
  }, [player?.playerName]);

  useEffect(() => {
    if (!didHydrateRef.current) {
      didHydrateRef.current = true;
      return;
    }
    try {
      window.localStorage.setItem(
        HUD_PREFS_KEY,
        JSON.stringify({ widgets: selected, placements, theme: layoutTheme, snapGrid }),
      );
    } catch {
      // Ignore storage errors
    }
    const timer = window.setTimeout(() => {
      const coerced = coerceLegacyHudLayout(
        { widgets: selected, placements, theme: layoutTheme, snapGrid },
        layoutTheme || "dragonstone",
      );
      if (!coerced) return;
      const nextStore = updateProfileForContext(readLocalSkyUltraProfileStore(), player?.playerName, {
        hudLayout: coerced,
      });
      selfPersistingRef.current = true;
      void persistSkyUltraProfileStore(nextStore, bridge, player?.playerName).finally(() => {
        selfPersistingRef.current = false;
      });
    }, 550);
    return () => window.clearTimeout(timer);
  }, [selected, placements, layoutTheme, snapGrid, bridge, player?.playerName]);

  useEffect(() => {
    if (!native) return;
    void bridge
      .request<ActorResponse>("game.getNearbyNpcs", { radius: 3000 })
      .then((res) => {
        if (mounted.current && Array.isArray(res?.results)) setNearbyActors(res.results.slice(0, 8));
      })
      .catch(() => undefined);
  }, [bridge, native]);

  useEffect(() => {
    if (!native) return;
    const controller = new AbortController();
    const revision = operation.current;
    void bridge
      .request<OverlayState>("ui.configureWidgets", { query: true }, { signal: controller.signal })
      .then((state) => {
        if (revision !== operation.current || !mounted.current) return;
        setEnabled(!!state.enabled);
        if (Array.isArray(state.widgets) && (state.enabled || (state.widgets.length === 0 && hasSavedHudRecordRef.current))) {
          const valid = state.widgets.filter((id) => widgetCatalog.some((w) => w.id === id));
          setSelected(valid);
        }
        if (typeof state.placements?.compass?.compassRadius === "number") {
          setPlacements((prev) => ({
            ...prev,
            compass: {
              ...prev.compass,
              compassRadius: Math.max(
                SKYULTRA_COMPASS_RADIUS_MIN,
                Math.min(SKYULTRA_COMPASS_RADIUS_MAX, Math.round(state.placements!.compass!.compassRadius!)),
              ),
            },
          }));
        }
        if (state.customCode && typeof state.customCode === "object" && state.customCode.enabled) {
          setCustomHudCode(sanitizeCustomHudCode(state.customCode));
        }
        onPinnedChange(state.enabled ? state.widgets?.length ?? 0 : 0);
      })
      .catch(() => undefined);
    return () => controller.abort();
  }, [bridge, native, onPinnedChange]);

  const updateWidgetPlacement = (kind: WidgetKind, patch: Partial<SkyUltraWidgetPlacement>): void => {
    setPlacements((prev) => ({
      ...prev,
      [kind]: {
        ...(prev[kind] ?? defaultWidgetPlacements()[kind]),
        ...patch,
      },
    }));
  };

  const currentCompassRadius = Math.max(
    SKYULTRA_COMPASS_RADIUS_MIN,
    Math.min(
      SKYULTRA_COMPASS_RADIUS_MAX,
      Math.round(placements.compass?.compassRadius ?? SKYULTRA_COMPASS_RADIUS_DEFAULT),
    ),
  );
  const liveOverlaySyncKey = `${currentCompassRadius}|${selected
    .map((kind) => `${kind}:${placements[kind]?.theme ?? ""}:${placements[kind]?.color ?? ""}`)
    .join(",")}`;
  const lastSyncedHudKeyRef = useRef<string | null>(null);
  const commitCompassRadius = (raw: string | number): void => {
    const parsed = typeof raw === "number" ? raw : Number(raw.trim());
    const clamped = Number.isFinite(parsed)
      ? Math.max(SKYULTRA_COMPASS_RADIUS_MIN, Math.min(SKYULTRA_COMPASS_RADIUS_MAX, Math.round(parsed)))
      : SKYULTRA_COMPASS_RADIUS_DEFAULT;
    updateWidgetPlacement("compass", { compassRadius: clamped });
    setCompassRadiusDraft(String(clamped));
  };

  const focusedEntry = widgetCatalog.find((w) => w.id === activeWidget) ?? widgetCatalog[0];
  const focusedPlacement = placements[focusedEntry.id] ?? defaultWidgetPlacements()[focusedEntry.id];
  const focusedTheme =
    focusedPlacement.theme && palettes.some((p) => p.value === focusedPlacement.theme)
      ? focusedPlacement.theme
      : layoutTheme;

  const syncFocusedStyleToAll = (): void => {
    setPlacements((prev) => {
      const next = { ...prev };
      for (const kind of Object.keys(next) as WidgetKind[]) {
        next[kind] = {
          ...next[kind],
          scale: focusedPlacement.scale,
          opacity: focusedPlacement.opacity,
          glow: focusedPlacement.glow,
          fontStyle: focusedPlacement.fontStyle,
        };
      }
      return next;
    });
    notify(
      "Synced style across instruments",
      `Applied ${Math.round(focusedPlacement.scale * 100)}% size, ${Math.round(focusedPlacement.opacity * 100)}% opacity, and ${focusedPlacement.fontStyle.toUpperCase()} typography to all instruments while preserving each instrument's screen position.`,
    );
  };

  const resetAllPlacements = (): void => {
    setPlacements(defaultWidgetPlacements());
    notify("HUD studio reset", "Restored default screen anchors, colors, and scale for all 12 transparent HUD instruments.");
  };

  const buildColorsMap = (): SkyUltraWidgetColors => {
    const out: SkyUltraWidgetColors = {};
    for (const entry of widgetCatalog) {
      out[entry.id] = placements[entry.id]?.color ?? entry.defaultColor;
    }
    return out;
  };

  const buildPlacementsMap = (): SkyUltraWidgetPlacements => {
    const out: SkyUltraWidgetPlacements = {};
    for (const kind of selected) {
      out[kind] = placements[kind];
    }
    return out;
  };

  useEffect(() => {
    if (!native || !enabled || busy) return;
    if (lastSyncedHudKeyRef.current === null) {
      lastSyncedHudKeyRef.current = liveOverlaySyncKey;
      return;
    }
    if (liveOverlaySyncKey === lastSyncedHudKeyRef.current) return;
    const timer = window.setTimeout(() => {
      lastSyncedHudKeyRef.current = liveOverlaySyncKey;
      void bridge
        .request<OverlayState>("ui.configureWidgets", {
          enabled: true,
          widgets: selected,
          anchor: focusedPlacement.anchor,
          scale: focusedPlacement.scale,
          opacity: focusedPlacement.opacity,
          theme: layoutTheme,
          colors: buildColorsMap(),
          glow: focusedPlacement.glow,
          compassRadius: currentCompassRadius,
          placements: buildPlacementsMap(),
          customCode: {
            enabled: customHudCode.enabled,
            mode: customHudCode.mode,
            html: customHudCode.html,
            css: customHudCode.css,
            js: customHudCode.js,
            fullHtml: customHudCode.fullHtml,
          },
        })
        .catch(() => undefined);
    }, 250);
    return () => window.clearTimeout(timer);
  }, [native, enabled, busy, selected, liveOverlaySyncKey, bridge]);

  const selectHudTheme = (nextTheme: string): void => {
    const pal = palettes.find((p) => p.value === nextTheme);
    userChangedHudThemeRef.current = true;
    hasSavedHudRecordRef.current = true;
    updateWidgetPlacement(focusedEntry.id, {
      theme: nextTheme,
      ...(pal ? { color: pal.color } : {}),
    });
    if (pal) {
      notify("Widget Theme Applied", `${pal.label} (${pal.color.toUpperCase()}) applied to ${focusedEntry.title}.`);
    }
  };

  const applyThemeToAllWidgets = (nextTheme: string): void => {
    const pal = palettes.find((p) => p.value === nextTheme);
    userChangedHudThemeRef.current = true;
    hasSavedHudRecordRef.current = true;
    setLayoutTheme(nextTheme);
    setPlacements((prev) => {
      const next = { ...prev };
      for (const kind of Object.keys(next) as WidgetKind[]) {
        next[kind] = {
          ...next[kind],
          theme: nextTheme,
          ...(pal ? { color: pal.color } : {}),
        };
      }
      return next;
    });
    if (pal) {
      notify("HUD Theme Applied to All", `${pal.label} (${pal.color.toUpperCase()}) applied across all HUD instruments.`);
    }
  };

  const startStagePointerAction = (
    e: React.PointerEvent<HTMLElement>,
    kind: WidgetKind,
    mode: "move" | "resize",
  ): void => {
    if (e.button !== 0) return;
    e.preventDefault();
    e.stopPropagation();
    setActiveWidget(kind);
    const wp = placements[kind] ?? defaultWidgetPlacements()[kind];
    try {
      e.currentTarget.setPointerCapture(e.pointerId);
    } catch {
      // Ignore synthetic events without pointer capture support
    }
    dragHudRef.current = {
      kind,
      mode,
      pointerId: e.pointerId,
      startX: e.clientX,
      startY: e.clientY,
      baseOffsetX: wp.offsetX,
      baseOffsetY: wp.offsetY,
      baseScale: wp.scale,
      liveOffsetX: wp.offsetX,
      liveOffsetY: wp.offsetY,
      liveScale: wp.scale,
      hoverZone: null,
      rafId: 0,
    };
    setDragHudState({ kind, mode, hoverZone: null });
  };

  const handleStagePointerMove = (e: React.PointerEvent<HTMLElement>): void => {
    const session = dragHudRef.current;
    if (!session || session.pointerId !== e.pointerId) return;
    const invScale = stageScale > 0.05 ? 1 / stageScale : 1;
    const dx = (e.clientX - session.startX) * invScale;
    const dy = (e.clientY - session.startY) * invScale;

    if (session.mode === "resize") {
      const deltaScale = (dx + dy) / 360;
      const rawScale = Math.max(0.7, Math.min(1.5, session.baseScale + deltaScale));
      session.liveScale = Math.round(rawScale * 20) / 20;
    } else {
      let nextX = snapSkyUltraHudCoordinate(session.baseOffsetX + dx, snapGrid, -1600, 1600);
      let nextY = snapSkyUltraHudCoordinate(session.baseOffsetY + dy, snapGrid, -900, 900);
      if (snapGrid > 0 && Math.abs(nextX) <= 6) nextX = 0;
      if (snapGrid > 0 && Math.abs(nextY) <= 6) nextY = 0;
      session.liveOffsetX = nextX;
      session.liveOffsetY = nextY;

      let hoverZone: SkyUltraWidgetAnchor | null = null;
      const hits =
        typeof document.elementsFromPoint === "function"
          ? document.elementsFromPoint(e.clientX, e.clientY)
          : typeof document.elementFromPoint === "function"
            ? [document.elementFromPoint(e.clientX, e.clientY)]
            : [];
      for (const hit of hits) {
        if (!(hit instanceof Element)) continue;
        const badge = hit.closest("[data-zone-drop]");
        const z = badge?.getAttribute("data-zone-drop");
        if (z && validAnchorSet.has(z)) {
          hoverZone = z as SkyUltraWidgetAnchor;
          break;
        }
      }
      if (hoverZone !== session.hoverZone) {
        session.hoverZone = hoverZone;
        setDragHudState({ kind: session.kind, mode: "move", hoverZone });
      }
    }

    if (session.rafId === 0) {
      session.rafId = requestAnimationFrame(() => {
        const cur = dragHudRef.current;
        if (!cur) return;
        cur.rafId = 0;
        const el = widgetNodeRefs.current[cur.kind];
        if (el) {
          el.style.transform = `translate3d(${cur.liveOffsetX}px, ${cur.liveOffsetY}px, 0) scale(${cur.liveScale})`;
        }
        const badge = dragBadgeRefs.current[cur.kind];
        if (badge) {
          badge.textContent = `${cur.liveOffsetX}, ${cur.liveOffsetY} · ${Math.round(cur.liveScale * 100)}%`;
        }
      });
    }
  };

  const finishStagePointerAction = (e: React.PointerEvent<HTMLElement>): void => {
    const session = dragHudRef.current;
    if (!session || session.pointerId !== e.pointerId) return;
    if (session.rafId !== 0) {
      cancelAnimationFrame(session.rafId);
      session.rafId = 0;
    }
    try {
      if (e.currentTarget.hasPointerCapture(e.pointerId)) {
        e.currentTarget.releasePointerCapture(e.pointerId);
      }
    } catch {
      // Ignore synthetic events
    }
    if (session.mode === "move" && session.hoverZone) {
      updateWidgetPlacement(session.kind, {
        anchor: session.hoverZone,
        offsetX: 0,
        offsetY: 0,
      });
    } else if (session.mode === "resize") {
      updateWidgetPlacement(session.kind, { scale: session.liveScale });
    } else {
      updateWidgetPlacement(session.kind, {
        offsetX: session.liveOffsetX,
        offsetY: session.liveOffsetY,
      });
    }
    dragHudRef.current = null;
    setDragHudState(null);
  };

  const exportedPackage = useMemo(() => {
    try {
      const safeId = sanitizeSkyUltraModId(exportModId);
      return generateSkyUltraHudModFiles({
        id: safeId,
        name: exportModName.trim() || "Custom SkyUltra HUD",
        version: /^[0-9]+\.[0-9]+\.[0-9]+/.test(exportModVersion.trim()) ? exportModVersion.trim() : "1.0.0",
        author: exportModAuthor.trim() || "Author",
        hotkey: exportModHotkey.trim() || "F8",
        layout: {
          version: 1,
          widgets: selected.length ? selected : ["vitals"],
          anchor: focusedPlacement.anchor,
          scale: focusedPlacement.scale,
          opacity: focusedPlacement.opacity,
          theme: layoutTheme,
          colors: buildColorsMap(),
          glow: focusedPlacement.glow,
          snapGrid,
          placements: buildPlacementsMap(),
        },
        customCode: customHudCode.enabled ? customHudCode : undefined,
      });
    } catch {
      return null;
    }
  }, [
    exportModId,
    exportModName,
    exportModVersion,
    exportModAuthor,
    exportModHotkey,
    selected,
    focusedPlacement,
    layoutTheme,
    snapGrid,
    placements,
    customHudCode,
  ]);

  const handleExportHudModToDisk = async (): Promise<void> => {
    if (!exportedPackage || exportBusy) return;
    if (!native) {
      setExportStatus(
        `Browser preview: prepared ${exportedPackage.files.length} files for '${exportedPackage.id}'. Run inside Skyrim to write directly to Data/SkyUltra/${exportedPackage.id}/, or copy any file below.`,
      );
      return;
    }
    setExportBusy(true);
    setExportStatus("");
    try {
      const res = await exportSkyUltraHudMod(
        {
          id: exportedPackage.id,
          name: exportedPackage.name,
          version: exportedPackage.version,
          author: exportModAuthor.trim() || "Author",
          hotkey: exportModHotkey.trim() || "F8",
          layout: {
            version: 1,
            widgets: selected.length ? selected : ["vitals"],
            anchor: focusedPlacement.anchor,
            scale: focusedPlacement.scale,
            opacity: focusedPlacement.opacity,
            theme: layoutTheme,
            colors: buildColorsMap(),
            glow: focusedPlacement.glow,
            snapGrid,
            placements: buildPlacementsMap(),
          },
          customCode: customHudCode.enabled ? customHudCode : undefined,
        },
        bridge,
      );
      if (res.exported) {
        setExportStatus(
          `Exported working mod '${res.modId}' (${res.fileCount ?? exportedPackage.files.length} files) to ${res.directory ?? `Data/SkyUltra/${res.modId}`}.`,
        );
        notify("HUD Mod Exported", `Wrote ${res.modId} directly to Data/SkyUltra/${res.modId}.`);
      } else {
        setExportStatus(res.message ?? "Could not write HUD mod files.");
      }
    } catch (err) {
      setExportStatus(err instanceof Error ? err.message : String(err));
    } finally {
      setExportBusy(false);
    }
  };

  const apply = async (pin: boolean, useCustomCode = false): Promise<void> => {
    if (busy) return;
    const hasActiveHud = pin && (selected.length > 0 || useCustomCode);
    const compassActive = hasActiveHud && selected.includes("compass");
    const nextCodeState: SkyUltraCustomHudCode = {
      ...customHudCode,
      enabled: pin && useCustomCode,
    };
    setCustomHudCode(nextCodeState);
    try {
      window.localStorage.setItem(
        HUD_PREFS_KEY,
        JSON.stringify({ widgets: selected, placements, theme: layoutTheme, snapGrid }),
      );
      hasSavedHudRecordRef.current = true;
    } catch {
      // Ignore storage errors
    }
    const coercedLayout = coerceLegacyHudLayout(
      { widgets: selected, placements, theme: layoutTheme, snapGrid },
      layoutTheme || "dragonstone",
    );
    if (coercedLayout) {
      const nextStore = updateProfileForContext(readLocalSkyUltraProfileStore(), player?.playerName, {
        hudLayout: coercedLayout,
      });
      selfPersistingRef.current = true;
      void persistSkyUltraProfileStore(nextStore, bridge, player?.playerName).finally(() => {
        selfPersistingRef.current = false;
      });
    }
    if (native) {
      void bridge
        .request("ui.updateSettings", { schemaVersion: 2, replaceSkyrimCompass: compassActive })
        .catch(() => undefined);
    }
    if (typeof window !== "undefined") {
      window.dispatchEvent(
        new CustomEvent("skyultra:settings-updated", { detail: { replaceSkyrimCompass: compassActive } }),
      );
    }
    if (!native) {
      setEnabled(hasActiveHud);
      onPinnedChange(hasActiveHud ? (useCustomCode ? 1 : selected.length) : 0);
      setMessage(
        hasActiveHud
          ? useCustomCode
            ? "Live Custom Code HUD pinned! Every edit in the Live Code Editor now streams in real time."
            : "Preview HUD layout activated! Each instrument is pinned to its chosen screen position over gameplay."
          : pin
            ? "Saved & applied empty HUD layout — Skyrim compass restored."
            : "HUD overlay unpinned — Skyrim compass restored.",
      );
      return;
    }
    const revision = ++operation.current;
    setBusy(true);
    setMessage("");
    const controller = new AbortController();
    applying.current = controller;
    const timeout = window.setTimeout(() => controller.abort(), 15000);
    try {
      let result = await bridge.request<OverlayState>(
        "ui.configureWidgets",
        {
          enabled: hasActiveHud,
          widgets: selected,
          anchor: focusedPlacement.anchor,
          scale: focusedPlacement.scale,
          opacity: focusedPlacement.opacity,
          theme: layoutTheme,
          colors: buildColorsMap(),
          glow: focusedPlacement.glow,
          compassRadius: currentCompassRadius,
          placements: buildPlacementsMap(),
          customCode: {
            enabled: nextCodeState.enabled,
            mode: nextCodeState.mode,
            html: nextCodeState.html,
            css: nextCodeState.css,
            js: nextCodeState.js,
            fullHtml: nextCodeState.fullHtml,
          },
        },
        { signal: controller.signal },
      );
      lastSyncedHudKeyRef.current = liveOverlaySyncKey;
      if (!result.available) throw new Error(result.message ?? "The overlay is unavailable.");
      const deadline = performance.now() + 15000;
      while (hasActiveHud && result.enabled && !result.ready && performance.now() < deadline) {
        await new Promise((resolve) => window.setTimeout(resolve, 120));
        if (!mounted.current || revision !== operation.current) return;
        result = await bridge.request<OverlayState>("ui.configureWidgets", { query: true }, { signal: controller.signal });
      }
      if (!mounted.current || revision !== operation.current) return;
      if (hasActiveHud && (!result.enabled || !result.ready))
        throw new Error("The HUD has not painted yet. Check the page loading log; your layout is retained.");
      setEnabled(!!result.enabled);
      onPinnedChange(result.enabled ? result.widgets?.length ?? selected.length : 0);
      setMessage(
        result.enabled
          ? useCustomCode
            ? "Live Custom Code HUD pinned over Skyrim! Edits stream in real time — press Esc to test in-game."
            : "Multi-zone transparent HUD overlay pinned! Press Esc to play with your custom instruments."
          : pin
            ? "Saved & applied gameplay HUD (0 active instruments — Skyrim compass restored)."
            : "Gameplay HUD overlay unpinned — Skyrim compass restored.",
      );
    } catch (error) {
      if (mounted.current) setMessage(error instanceof Error ? error.message : String(error));
    } finally {
      window.clearTimeout(timeout);
      if (applying.current === controller) applying.current = null;
      if (mounted.current && revision === operation.current) setBusy(false);
    }
  };

  useEffect(() => {
    if (!native || !enabled || !debouncedHudCode.enabled || busy) return;
    void bridge
      .request<OverlayState>("ui.configureWidgets", {
        enabled: true,
        widgets: selected,
        anchor: focusedPlacement.anchor,
        scale: focusedPlacement.scale,
        opacity: focusedPlacement.opacity,
        theme: layoutTheme,
        colors: buildColorsMap(),
        glow: focusedPlacement.glow,
        placements: buildPlacementsMap(),
        customCode: {
          enabled: true,
          mode: debouncedHudCode.mode,
          html: debouncedHudCode.html,
          css: debouncedHudCode.css,
          js: debouncedHudCode.js,
          fullHtml: debouncedHudCode.fullHtml,
        },
      })
      .catch(() => undefined);
  }, [debouncedHudCode, native, enabled]);

  const toggle = (kind: WidgetKind): void => {
    setActiveWidget(kind);
    setSelected((current) => (current.includes(kind) ? current.filter((value) => value !== kind) : [...current, kind]));
  };

  const moveWidget = (kind: WidgetKind, delta: number): void => {
    setSelected((current) => {
      const idx = current.indexOf(kind);
      if (idx < 0) return current;
      const nextIdx = idx + delta;
      if (nextIdx < 0 || nextIdx >= current.length) return current;
      const copy = [...current];
      [copy[idx], copy[nextIdx]] = [copy[nextIdx], copy[idx]];
      return copy;
    });
  };

  const visibleCatalog = widgetCatalog.filter((w) => categoryFilter === "All" || w.category === categoryFilter);
  const inventory = (player?.inventory ?? []).filter((item) => item.name.toLowerCase().includes(pickQuery.toLowerCase()));
  const actions = [
    { id: "inventory", label: "Collection", icon: <Icon name="inventory" /> },
    { id: "actors", label: "People", icon: <Icon name="actors" /> },
    { id: "character", label: "Character", icon: <Icon name="shield" /> },
    { id: "guide", label: "Guide", icon: <Icon name="book" /> },
    { id: "notice", label: "Notify", icon: <Icon name="bell" /> },
    { id: "confirm", label: "Confirm", icon: <Icon name="check" /> },
    { id: "quantity", label: "Quantity", icon: <Icon name="plus" /> },
    { id: "performance", label: "Performance", icon: <Icon name="performance" /> },
  ];

  return (
    <div className="page widgets-page">
      <div className="page-lead page-lead--compact">
        <div>
          <span className="eyebrow">PER-WIDGET 9-ZONE HOLOGRAPHIC STUDIO · {widgetCatalog.length} LIVE INSTRUMENTS</span>
          <h1>
            HUD Studio & Overlays<span className="title-period">.</span>
          </h1>
          <p>
            Select any HUD instrument to place across 9 screen zones (Top, Middle &amp; Bottom Left/Center/Right) or drag
            widgets directly across the 1:1 viewport stage to position anywhere.
          </p>
        </div>
        <div className="hud-studio-header-actions">
          <span className={"hud-status-beacon" + (enabled ? " is-live" : "")}>
            <i />
            {enabled
              ? customHudCode.enabled
                ? "LIVE CODE HUD PINNED"
                : `PINNED IN SKYRIM (${selected.length})`
              : `${selected.length} ACTIVE IN STUDIO`}
          </span>
          <SkySegmentedControl
            label="Widget workspace"
            value={tab}
            onValueChange={(value) => {
              if (
                value === "gallery" ||
                value === "editor" ||
                value === "presets" ||
                value === "export" ||
                value === "patterns"
              ) {
                setTab(value);
              }
            }}
            options={[
              { value: "gallery", label: "HUD Builder" },
              { value: "editor", label: "Live Code Editor" },
              { value: "presets", label: "Loadout Presets" },
              { value: "export", label: "Export Working Mod" },
              { value: "patterns", label: "Popups & Wheels" },
            ].filter((option) => showPatterns || option.value !== "patterns")}
          />
        </div>
      </div>

      {tab === "editor" && (
        <div className="hud-live-editor-layout">
          <div className="hud-live-editor-left">
            <SkyFrame
              eyebrow="REAL-TIME HTML / CSS / JS AUTHORING · ZERO RESTART"
              title="Live HUD Code Editor"
              actions={
                <div className="hud-live-editor-mode-pills">
                  <button
                    type="button"
                    className={"char-stage-btn" + (customHudCode.mode === "split" ? " char-stage-btn--active" : "")}
                    onClick={() => setCustomHudCode((prev) => ({ ...prev, mode: "split" }))}
                  >
                    Split (HTML / CSS / JS)
                  </button>
                  <button
                    type="button"
                    className={"char-stage-btn" + (customHudCode.mode === "full-html" ? " char-stage-btn--active" : "")}
                    onClick={() => {
                      setCustomHudCode((prev) => ({
                        ...prev,
                        mode: "full-html",
                        fullHtml:
                          prev.fullHtml.trim() ||
                          `<!doctype html>\n<html lang="en">\n<head>\n<style>\n${prev.css}\n</style>\n</head>\n<body>\n${prev.html}\n<script>\n${prev.js}\n</script>\n</body>\n</html>`,
                      }));
                    }}
                  >
                    Full HTML Document
                  </button>
                </div>
              }
              footer={
                <span>
                  Bind live Skyrim data with <code>SkyUltraHud.onUpdate((player, actors) =&gt; ...)</code>,{" "}
                  <code>window.SkyWebView</code>, or declarative <code>data-sky-bind=&quot;health.current&quot;</code>{" "}
                  &amp; <code>data-sky-bar=&quot;health&quot;</code>.
                </span>
              }
            >
              <div className="hud-live-template-strip" role="group" aria-label="Starter HUD code templates">
                {SKYULTRA_HUD_CODE_TEMPLATES.map((tpl) => (
                  <button
                    key={tpl.id}
                    type="button"
                    className="hud-live-template-chip"
                    title={tpl.description}
                    onClick={() => {
                      setHudConsoleLogs([]);
                      setCustomHudCode((prev) => ({
                        ...prev,
                        mode: "split",
                        html: tpl.html,
                        css: tpl.css,
                        js: tpl.js,
                      }));
                      notify("Loaded HUD Template", `${tpl.name} loaded into Live Code Editor.`);
                    }}
                  >
                    <small>{tpl.tag}</small>
                    <strong>{tpl.name}</strong>
                  </button>
                ))}
                <button
                  type="button"
                  className="hud-live-template-chip"
                  title="Convert your current Visual HUD Builder widgets and anchors into editable HTML, CSS, and JS"
                  onClick={() => {
                    const generated = generateCustomHudCodeFromLayout({
                      version: 1,
                      widgets: selected.length ? selected : ["vitals"],
                      anchor: focusedPlacement.anchor,
                      scale: focusedPlacement.scale,
                      opacity: focusedPlacement.opacity,
                      theme: layoutTheme,
                      colors: buildColorsMap(),
                      glow: focusedPlacement.glow,
                      snapGrid,
                      placements: buildPlacementsMap(),
                    });
                    setHudConsoleLogs([]);
                    setCustomHudCode((prev) => ({
                      ...prev,
                      mode: "split",
                      html: generated.html,
                      css: generated.css,
                      js: generated.js,
                    }));
                    notify("Imported Visual Layout", "Converted your visual HUD Builder instruments into editable code.");
                  }}
                >
                  <small>FROM BUILDER</small>
                  <strong>Import Visual Layout</strong>
                </button>
                <button
                  type="button"
                  className="hud-live-template-chip"
                  title="Start from a clean scratchpad"
                  onClick={() => {
                    setHudConsoleLogs([]);
                    setCustomHudCode((prev) => ({
                      ...prev,
                      mode: "split",
                      html: `<div class="my-hud">\n  <h2 id="title">Dragonborn</h2>\n  <div class="bar"><i data-sky-bar="health"></i></div>\n</div>`,
                      css: `.my-hud {\n  position: fixed;\n  left: 36px;\n  bottom: 32px;\n  color: #f5efe6;\n  font-family: sans-serif;\n}\n.my-hud h2 { margin: 0 0 8px; font-size: 16px; letter-spacing: 0.1em; }\n.bar { width: 240px; height: 8px; background: rgba(0,0,0,0.65); border-radius: 99px; overflow: hidden; }\n.bar > i { display: block; height: 100%; background: #f43f5e; transition: width 150ms ease; }`,
                      js: `SkyUltraHud.onUpdate((player) => {\n  if (!player) return;\n  document.getElementById('title').textContent = (player.playerName || 'Dragonborn') + ' · LVL ' + (player.level ?? 1);\n});`,
                    }));
                  }}
                >
                  <small>SCRATCHPAD</small>
                  <strong>New Blank HUD</strong>
                </button>
              </div>

              <div className="hud-live-editor-toolbar">
                {customHudCode.mode === "split" ? (
                  <div className="dev-filter-group" role="tablist" aria-label="Code language tabs">
                    {(
                      [
                        { id: "html", label: "HTML Markup", bytes: customHudCode.html.length },
                        { id: "css", label: "CSS Styles", bytes: customHudCode.css.length },
                        { id: "js", label: "JavaScript Logic", bytes: customHudCode.js.length },
                      ] as const
                    ).map((lang) => (
                      <button
                        key={lang.id}
                        type="button"
                        role="tab"
                        aria-selected={editorCodeTab === lang.id}
                        className={"dev-filter-pill dev-filter-pill--compact" + (editorCodeTab === lang.id ? " is-active" : "")}
                        onClick={() => setEditorCodeTab(lang.id)}
                      >
                        <span>{lang.label}</span>
                        <b className="dev-filter-pill__count">{lang.bytes}B</b>
                      </button>
                    ))}
                  </div>
                ) : (
                  <span className="quiet-pill">
                    COMPLETE SINGLE-FILE HTML ({customHudCode.fullHtml.length.toLocaleString()} BYTES)
                  </span>
                )}

                <div className="hud-live-editor-toolbar__actions">
                  <button
                    type="button"
                    className="char-stage-btn char-stage-btn--sm"
                    onClick={async () => {
                      if (!navigator.clipboard?.readText) {
                        notify("Paste Shortcut", "Click inside the code editor and press Ctrl + V to paste your code.");
                        return;
                      }
                      try {
                        const clip = await navigator.clipboard.readText();
                        if (!clip) return;
                        if (/<!doctype|<html|<style|<script/i.test(clip)) {
                          setCustomHudCode((prev) => ({ ...prev, mode: "full-html", fullHtml: clip }));
                          notify("Pasted Full HTML HUD", "Switched to Full HTML mode and rendered your pasted code live.");
                        } else if (customHudCode.mode === "full-html") {
                          setCustomHudCode((prev) => ({ ...prev, fullHtml: clip }));
                        } else {
                          setCustomHudCode((prev) => ({ ...prev, [editorCodeTab]: clip }));
                        }
                      } catch {
                        notify("Paste Shortcut", "Click inside the code editor and press Ctrl + V to paste your code.");
                      }
                    }}
                  >
                    Paste Code
                  </button>
                  <button
                    type="button"
                    className="char-stage-btn char-stage-btn--sm"
                    onClick={() => {
                      const activeText =
                        customHudCode.mode === "full-html"
                          ? customHudCode.fullHtml
                          : customHudCode[editorCodeTab];
                      if (navigator.clipboard) {
                        void navigator.clipboard.writeText(activeText);
                        notify("Copied Code", "Current editor buffer copied to clipboard.");
                      }
                    }}
                  >
                    Copy Buffer
                  </button>
                </div>
              </div>

              {(() => {
                const activeValue =
                  customHudCode.mode === "full-html"
                    ? customHudCode.fullHtml
                    : customHudCode[editorCodeTab];
                const updateActiveValue = (nextVal: string): void => {
                  setCustomHudCode((prev) =>
                    prev.mode === "full-html"
                      ? { ...prev, fullHtml: nextVal }
                      : { ...prev, [editorCodeTab]: nextVal },
                  );
                };
                return (
                  <textarea
                    aria-label={
                      customHudCode.mode === "full-html"
                        ? "Full HTML HUD code editor"
                        : `HUD ${editorCodeTab.toUpperCase()} code editor`
                    }
                    className="hud-live-code-textarea"
                    spellCheck={false}
                    value={activeValue}
                    onChange={(e) => updateActiveValue(e.currentTarget.value)}
                    onKeyDown={(e) => {
                      if (e.key === "Tab") {
                        e.preventDefault();
                        const el = e.currentTarget;
                        const start = el.selectionStart ?? 0;
                        const end = el.selectionEnd ?? 0;
                        const next = activeValue.slice(0, start) + "  " + activeValue.slice(end);
                        updateActiveValue(next);
                        requestAnimationFrame(() => {
                          el.selectionStart = el.selectionEnd = start + 2;
                        });
                      }
                    }}
                    placeholder={
                      customHudCode.mode === "full-html"
                        ? "Paste or write a complete HTML HUD document (<!doctype html>...) with <style> and <script>..."
                        : `Write custom ${editorCodeTab.toUpperCase()} for your HUD...`
                    }
                  />
                );
              })()}

              <div className="hud-live-console">
                <div className="hud-live-console__head">
                  <span>
                    LIVE RUNTIME CONSOLE &amp; ERRORS ({hudConsoleLogs.filter((l) => l.level === "error").length}{" "}
                    ERRORS)
                  </span>
                  {hudConsoleLogs.length > 0 && (
                    <button
                      type="button"
                      className="char-stage-btn char-stage-btn--sm"
                      onClick={() => setHudConsoleLogs([])}
                    >
                      Clear Console
                    </button>
                  )}
                </div>
                <div className="hud-live-console__body">
                  {hudConsoleLogs.length === 0 ? (
                    <span className="hud-live-console__empty">
                      No runtime errors — live HUD sandbox is executing cleanly. Use console.log(...) to inspect values.
                    </span>
                  ) : (
                    hudConsoleLogs.map((entry) => (
                      <div key={entry.id} className={`hud-live-console__row is-${entry.level}`}>
                        <small>{entry.time}</small>
                        <b>{entry.level.toUpperCase()}</b>
                        <span>{entry.message}</span>
                      </div>
                    ))
                  )}
                </div>
              </div>
            </SkyFrame>
          </div>

          <div className="hud-live-editor-right">
            <SkyFrame
              eyebrow={
                enabled && customHudCode.enabled
                  ? "STREAMING LIVE TO SKYRIM GAMEPLAY HUD"
                  : "INSTANT TRANSPARENT 16:9 HUD PREVIEW"
              }
              title="Real-Time HUD Stage"
              actions={
                <div className="hud-live-stage-actions">
                  <button
                    type="button"
                    className="primary-button"
                    disabled={busy}
                    onClick={() => void apply(true, true)}
                  >
                    <Icon name="check" size={14} />
                    {enabled && customHudCode.enabled ? "Syncing Live to Skyrim" : "Pin Live Code HUD to Skyrim"}
                  </button>
                  {enabled && (
                    <button type="button" className="control-button" disabled={busy} onClick={() => void apply(false, false)}>
                      Unpin
                    </button>
                  )}
                  <button
                    type="button"
                    className="control-button"
                    onClick={() => {
                      setCustomHudCode((prev) => ({ ...prev, enabled: true }));
                      setTab("export");
                    }}
                  >
                    <Icon name="download" size={14} /> Export as Mod
                  </button>
                </div>
              }
            >
              {message && <p className="char-loadout-status-banner" style={{ marginBottom: "10px" }}>{message}</p>}

              <div className="hud-live-stage-bg-bar" role="group" aria-label="Stage Preview Background">
                <span className="hud-live-stage-bg-bar__label">
                  STAGE BACKGROUND <small>(PREVIEW ONLY · DOES NOT AFFECT CODE)</small>
                </span>
                <div className="hud-live-stage-bg-bar__pills">
                  <button
                    type="button"
                    className={"char-stage-btn" + (editorStageBg === "black" ? " char-stage-btn--active" : "")}
                    onClick={() => setEditorStageBg("black")}
                  >
                    <i className="hud-stage-bg-swatch hud-stage-bg-swatch--black" aria-hidden="true" />
                    Black
                  </button>
                  <button
                    type="button"
                    className={"char-stage-btn" + (editorStageBg === "white" ? " char-stage-btn--active" : "")}
                    onClick={() => setEditorStageBg("white")}
                  >
                    <i className="hud-stage-bg-swatch hud-stage-bg-swatch--white" aria-hidden="true" />
                    White
                  </button>
                  <button
                    type="button"
                    className={"char-stage-btn" + (editorStageBg === "transparent" ? " char-stage-btn--active" : "")}
                    onClick={() => setEditorStageBg("transparent")}
                  >
                    <i className="hud-stage-bg-swatch hud-stage-bg-swatch--transparent" aria-hidden="true" />
                    Transparent
                  </button>
                </div>
              </div>

              <div className="hud-live-stage-viewport" data-stage-bg={editorStageBg}>
                <div className="hud-live-stage-grid-bg" aria-hidden="true" />
                <CustomHudSandbox
                  code={{ ...debouncedHudCode, enabled: true }}
                  player={editorPreviewPlayer}
                  nearbyActors={nearbyActors}
                  onLog={appendHudConsoleLog}
                />
              </div>

              <div className="hud-live-sim-bar">
                <div className="hud-live-sim-bar__top">
                  <div>
                    <strong>Interactive Telemetry Simulator</strong>
                    <small>
                      {simTelemetry.active
                        ? "Driving HUD with simulated HP / MP / SP / Compass values"
                        : native
                          ? "Connected to live Skyrim player & nearby actor snapshots"
                          : "Using preview character snapshot (toggle simulator to test animations)"}
                    </small>
                  </div>
                  <button
                    type="button"
                    className={"char-stage-btn" + (simTelemetry.active ? " char-stage-btn--active" : "")}
                    onClick={() => setSimTelemetry((s) => ({ ...s, active: !s.active }))}
                  >
                    {simTelemetry.active ? "Simulator: ON" : "Test Sliders: OFF"}
                  </button>
                </div>

                {simTelemetry.active && (
                  <div className="hud-live-sim-sliders">
                    <SkyRangeSlider
                      label="Simulated Health"
                      min={0}
                      max={100}
                      step={1}
                      unit="%"
                      value={simTelemetry.hpPct}
                      onValueChange={(hpPct) => setSimTelemetry((s) => ({ ...s, hpPct }))}
                    />
                    <SkyRangeSlider
                      label="Simulated Magicka"
                      min={0}
                      max={100}
                      step={1}
                      unit="%"
                      value={simTelemetry.mpPct}
                      onValueChange={(mpPct) => setSimTelemetry((s) => ({ ...s, mpPct }))}
                    />
                    <SkyRangeSlider
                      label="Simulated Stamina"
                      min={0}
                      max={100}
                      step={1}
                      unit="%"
                      value={simTelemetry.spPct}
                      onValueChange={(spPct) => setSimTelemetry((s) => ({ ...s, spPct }))}
                    />
                    <SkyRangeSlider
                      label="Compass Heading"
                      min={0}
                      max={359}
                      step={1}
                      unit="°"
                      value={simTelemetry.heading}
                      onValueChange={(heading) => setSimTelemetry((s) => ({ ...s, heading }))}
                    />
                  </div>
                )}
              </div>
            </SkyFrame>
          </div>
        </div>
      )}

      {tab === "export" && (
        <div className="hud-export-studio">
          <SkyFrame
            eyebrow="STANDALONE MOD GENERATOR · MANIFEST, ASSETS & CODE"
            title="Export Working SkyUltra HUD Mod"
            actions={
              <div className="hud-live-editor-mode-pills">
                <button
                  type="button"
                  className={"char-stage-btn" + (!customHudCode.enabled ? " char-stage-btn--active" : "")}
                  onClick={() => setCustomHudCode((prev) => ({ ...prev, enabled: false }))}
                >
                  Source: Visual HUD Builder
                </button>
                <button
                  type="button"
                  className={"char-stage-btn" + (customHudCode.enabled ? " char-stage-btn--active" : "")}
                  onClick={() => setCustomHudCode((prev) => ({ ...prev, enabled: true }))}
                >
                  Source: Live Code Editor
                </button>
              </div>
            }
            footer={
              <span>
                Generates a ready-to-run mod folder for <code>Data/SkyUltra/{exportedPackage?.id ?? "custom_hud"}/</code> with manifest, SVG emblem asset, zero-build HUD runtime, and editable TypeScript SDK project.
              </span>
            }
          >
            <div className="hud-export-form-grid">
              <label className="hud-inspector-field">
                <span>MOD IDENTIFIER (FOLDER NAME, E.G. TEST OR AUTHOR.HUD)</span>
                <input
                  value={exportModId}
                  onChange={(e) => setExportModId(e.currentTarget.value)}
                  placeholder="test"
                />
              </label>
              <label className="hud-inspector-field">
                <span>MOD DISPLAY NAME</span>
                <input
                  value={exportModName}
                  onChange={(e) => setExportModName(e.currentTarget.value)}
                  placeholder="Dragonborn Tactical HUD"
                />
              </label>
              <label className="hud-inspector-field">
                <span>SEMANTIC VERSION</span>
                <input
                  value={exportModVersion}
                  onChange={(e) => setExportModVersion(e.currentTarget.value)}
                  placeholder="1.0.0"
                />
              </label>
              <label className="hud-inspector-field">
                <span>AUTHOR & SUGGESTED HOTKEY</span>
                <div className="hud-export-inline-pair">
                  <input
                    value={exportModAuthor}
                    onChange={(e) => setExportModAuthor(e.currentTarget.value)}
                    placeholder="Author"
                  />
                  <input
                    value={exportModHotkey}
                    onChange={(e) => setExportModHotkey(e.currentTarget.value)}
                    placeholder="F8"
                  />
                </div>
              </label>
            </div>

            <div className="hud-export-actions-bar">
              <button
                type="button"
                className="primary-button"
                disabled={!exportedPackage || exportBusy}
                onClick={() => void handleExportHudModToDisk()}
              >
                <Icon name="download" size={15} />
                {exportBusy ? "Exporting Mod Package…" : `Export Working Mod to Data/SkyUltra/${exportedPackage?.id ?? ""}`}
              </button>
              <button
                type="button"
                className="control-button"
                onClick={() => {
                  const file = exportedPackage?.files.find((f) => f.path === exportActiveFile) ?? exportedPackage?.files[0];
                  if (file && navigator.clipboard) {
                    void navigator.clipboard.writeText(file.content);
                    notify("Copied generated file", `${file.path} copied to clipboard.`);
                  }
                }}
              >
                Copy {exportActiveFile}
              </button>
              <button
                type="button"
                className="control-button"
                onClick={() => {
                  const targetModId = exportedPackage?.id ?? sanitizeSkyUltraModId(exportModId);
                  if (onInspectMod) {
                    onInspectMod(targetModId);
                  } else {
                    onNavigate("devtools");
                  }
                }}
              >
                <Icon name="components" size={14} /> Inspect in Developer Studio
              </button>
            </div>

            {exportStatus && <p className="char-loadout-status-banner">{exportStatus}</p>}

            {exportedPackage && (
              <div className="hud-export-browser">
                <div className="hud-export-file-list" role="tablist" aria-label="Generated mod files">
                  {exportedPackage.files.map((file) => {
                    const active = file.path === exportActiveFile;
                    return (
                      <button
                        key={file.path}
                        type="button"
                        role="tab"
                        aria-selected={active}
                        className={"hud-export-file-tab" + (active ? " is-active" : "")}
                        onClick={() => setExportActiveFile(file.path)}
                      >
                        <strong>{file.path}</strong>
                        <small>{file.description}</small>
                      </button>
                    );
                  })}
                </div>
                <div className="hud-export-code-pane">
                  {(() => {
                    const currentFile =
                      exportedPackage.files.find((f) => f.path === exportActiveFile) ?? exportedPackage.files[0];
                    if (!currentFile) return null;
                    return (
                      <>
                        <div className="hud-export-code-pane__head">
                          <strong>{currentFile.path}</strong>
                          <span>
                            {currentFile.language.toUpperCase()} · {currentFile.content.length.toLocaleString()} bytes
                          </span>
                        </div>
                        <pre className="hud-export-code-pre">
                          <code>{currentFile.content}</code>
                        </pre>
                      </>
                    );
                  })()}
                </div>
              </div>
            )}
          </SkyFrame>
        </div>
      )}

      {tab !== "patterns" && tab !== "export" && tab !== "editor" && (
        <>
          <div className="hud-studio-layout">
            <div className="hud-studio-left">
              <SkyFrame
                title="Transparent HUD Instruments"
                eyebrow="CLICK ANY INSTRUMENT TO EDIT ITS POSITION, SIZE & STYLE"
                className="widget-controls"
                actions={
                  <button type="button" className="control-button" onClick={resetAllPlacements}>
                    Reset All
                  </button>
                }
                footer={
                  <span>
                    {selected.length} of {widgetCatalog.length} instruments active · Each instrument has its own screen anchor & scale
                  </span>
                }
              >
                {tab === "presets" ? (
                  <div className="layout-presets">
                    {presets.map((preset) => (
                      <button
                        type="button"
                        key={preset.name}
                        className="hud-preset-card"
                        onClick={() => {
                          setSelected(preset.items);
                          setPlacements((prev) => {
                            const next = { ...prev };
                            for (const [id, patch] of Object.entries(preset.placements) as Array<
                              [WidgetKind, Partial<SkyUltraWidgetPlacement>]
                            >) {
                              if (next[id] && patch) {
                                next[id] = { ...next[id], ...patch };
                              }
                            }
                            return next;
                          });
                          if (preset.items[0]) setActiveWidget(preset.items[0]);
                          setTab("gallery");
                        }}
                      >
                        <div className="hud-preset-card__top">
                          <span className="hud-preset-card__tag">{preset.tag}</span>
                          <strong>{preset.name}</strong>
                        </div>
                        <small>{preset.description}</small>
                        <span className="layout-mini">
                          {preset.items.map((item) => {
                            const entry = widgetCatalog.find((w) => w.id === item);
                            const p = preset.placements[item];
                            const c = p?.color ?? entry?.defaultColor ?? "#e8b059";
                            return (
                              <i key={item} style={{ borderColor: c, color: c }}>
                                {entry?.title} · {(p?.anchor ?? entry?.defaultAnchor ?? "top-right").toUpperCase()}
                              </i>
                            );
                          })}
                        </span>
                      </button>
                    ))}
                  </div>
                ) : (
                  <>
                    <div className="hud-category-bar" role="tablist" aria-label="Filter HUD categories">
                      {(["All", "Combat", "Navigation", "Character", "System"] as const).map((cat) => (
                        <button
                          key={cat}
                          type="button"
                          className={"hud-category-pill" + (categoryFilter === cat ? " is-active" : "")}
                          onClick={() => setCategoryFilter(cat)}
                        >
                          {cat}
                        </button>
                      ))}
                    </div>

                    <div className="hud-instrument-list">
                      {visibleCatalog.map((widget) => {
                        const isSelected = selected.includes(widget.id);
                        const isFocused = activeWidget === widget.id;
                        const wp = placements[widget.id] ?? defaultWidgetPlacements()[widget.id];
                        const color = wp.color;
                        const orderIdx = selected.indexOf(widget.id);
                        return (
                          <div
                            key={widget.id}
                            className={
                              "hud-instrument-card" +
                              (isSelected ? " is-enabled" : "") +
                              (isFocused ? " is-focused" : "")
                            }
                            style={{ "--card-hud-color": color } as React.CSSProperties}
                            onClick={() => setActiveWidget(widget.id)}
                          >
                            <div className="hud-instrument-card__head">
                              <div className="hud-instrument-card__identity">
                                <span className="hud-instrument-card__icon" style={{ color }}>
                                  <Icon name={widget.icon} size={18} />
                                </span>
                                <div className="hud-instrument-card__titles">
                                  <strong>{widget.title}</strong>
                                  <div className="hud-instrument-card__meta">
                                    <span className="hud-instrument-card__cat">{wp.anchor.toUpperCase()}</span>
                                    <span className="hud-instrument-card__cat">{Math.round(wp.scale * 100)}%</span>
                                    {orderIdx >= 0 && <b className="hud-instrument-card__order">#{orderIdx + 1}</b>}
                                  </div>
                                </div>
                              </div>
                              <div className="hud-instrument-card__controls" onClick={(e) => e.stopPropagation()}>
                                <SkyColorWheel
                                  mode="popover"
                                  compact
                                  size={146}
                                  label={`${widget.title} Accent`}
                                  value={color}
                                  onChange={(hex) => updateWidgetPlacement(widget.id, { color: hex })}
                                />
                                {isSelected && (
                                  <div className="hud-order-mini">
                                    <button
                                      type="button"
                                      aria-label={`Move ${widget.title} up`}
                                      disabled={orderIdx <= 0}
                                      onClick={() => moveWidget(widget.id, -1)}
                                    >
                                      ↑
                                    </button>
                                    <button
                                      type="button"
                                      aria-label={`Move ${widget.title} down`}
                                      disabled={orderIdx < 0 || orderIdx >= selected.length - 1}
                                      onClick={() => moveWidget(widget.id, 1)}
                                    >
                                      ↓
                                    </button>
                                  </div>
                                )}
                                <SkyToggleSwitch
                                  compact
                                  label={widget.title}
                                  checked={isSelected}
                                  onCheckedChange={() => toggle(widget.id)}
                                />
                              </div>
                            </div>
                            <p className="hud-instrument-card__desc">{widget.detail}</p>
                          </div>
                        );
                      })}
                    </div>

                    <div className="hud-action-deck hud-action-deck--top">
                      <button
                        type="button"
                        className="primary-button"
                        disabled={busy}
                        onClick={() => void apply(true)}
                      >
                        <Icon name="pin" size={15} />
                        {busy
                          ? "Applying…"
                          : selected.length === 0
                            ? enabled
                              ? "Save & Clear Pinned HUD"
                              : "Save & Apply Empty HUD"
                            : enabled
                              ? "Update Pinned Gameplay HUD"
                              : "Pin Transparent HUD Over Gameplay"}
                      </button>
                      <button
                        type="button"
                        className="control-button"
                        disabled={busy || !enabled}
                        onClick={() => void apply(false)}
                      >
                        Unpin Overlay
                      </button>
                      <button
                        type="button"
                        className="control-button"
                        disabled={busy}
                        onClick={() => {
                          setSelected(["compass", "vitals", "character", "radar"]);
                          resetAllPlacements();
                        }}
                      >
                        Restore Defaults
                      </button>
                    </div>
                    {message && (
                      <p className="inspector-note" role="status">
                        {message}
                      </p>
                    )}
                  </>
                )}
              </SkyFrame>

              <SkyFrame
                title={`Per-Widget Controls: ${focusedEntry.title}`}
                eyebrow="SELECTED INSTRUMENT POSITION, DATA BINDING & TYPOGRAPHY"
                className="hud-global-controls-frame"
                actions={
                  <button
                    type="button"
                    className="control-button"
                    title="Copy this widget's size, opacity, glow and typography to all instruments"
                    onClick={syncFocusedStyleToAll}
                  >
                    Sync Scale & Font to All
                  </button>
                }
              >
                <div className="hud-focused-banner" style={{ "--focused-hud-color": focusedPlacement.color } as React.CSSProperties}>
                  <div className="hud-focused-banner__left">
                    <span className="hud-focused-banner__icon" style={{ color: focusedPlacement.color }}>
                      <Icon name={focusedEntry.icon} size={18} />
                    </span>
                    <div>
                      <small>EDITING INDIVIDUAL WIDGET</small>
                      <strong>{focusedEntry.title}</strong>
                    </div>
                  </div>
                  <div className="hud-focused-banner__pills">
                    {selected.map((id) => {
                      const entry = widgetCatalog.find((w) => w.id === id);
                      if (!entry) return null;
                      return (
                        <button
                          key={id}
                          type="button"
                          className={"hud-focused-pill" + (activeWidget === id ? " is-active" : "")}
                          onClick={() => setActiveWidget(id)}
                        >
                          <i style={{ background: placements[id]?.color ?? entry.defaultColor }} />
                          {entry.title.split(" ")[0]}
                        </button>
                      );
                    })}
                  </div>
                </div>

                <div className="hud-inspector-tabs" role="tablist" aria-label="Per-widget inspector sections">
                  <button
                    type="button"
                    role="tab"
                    aria-selected={inspectorTab === "transform"}
                    className={"hud-inspector-tab" + (inspectorTab === "transform" ? " is-active" : "")}
                    onClick={() => setInspectorTab("transform")}
                  >
                    Position & Scale
                  </button>
                  <button
                    type="button"
                    role="tab"
                    aria-selected={inspectorTab === "binding"}
                    className={"hud-inspector-tab" + (inspectorTab === "binding" ? " is-active" : "")}
                    onClick={() => setInspectorTab("binding")}
                  >
                    Live Data & Rules
                  </button>
                  <button
                    type="button"
                    role="tab"
                    aria-selected={inspectorTab === "style"}
                    className={"hud-inspector-tab" + (inspectorTab === "style" ? " is-active" : "")}
                    onClick={() => setInspectorTab("style")}
                  >
                    Font & Theme
                  </button>
                </div>

                {inspectorTab === "transform" && (
                  <>
                    <div className="hud-anchor-section">
                      <div className="hud-control-heading-row">
                        <span className="hud-control-heading">
                          WIDGET SCREEN LOCATION ({focusedEntry.title.toUpperCase()})
                        </span>
                        <div className="hud-control-heading-actions">
                          {(focusedPlacement.offsetX !== 0 || focusedPlacement.offsetY !== 0) && (
                            <button
                              type="button"
                              className="char-stage-btn char-stage-btn--sm"
                              onClick={() => updateWidgetPlacement(focusedEntry.id, { offsetX: 0, offsetY: 0 })}
                            >
                              Reset Offset (0, 0)
                            </button>
                          )}
                          <em className="hud-control-badge">{focusedPlacement.anchor.toUpperCase()}</em>
                        </div>
                      </div>
                      <div className="hud-anchor-matrix hud-anchor-matrix--nine" role="radiogroup" aria-label="Gameplay widget position">
                        {hudAnchorOptions.map((pos) => (
                          <button
                            key={pos.value}
                            type="button"
                            role="radio"
                            aria-checked={focusedPlacement.anchor === pos.value}
                            className={"hud-anchor-cell" + (focusedPlacement.anchor === pos.value ? " is-active" : "")}
                            onClick={() => updateWidgetPlacement(focusedEntry.id, { anchor: pos.value })}
                          >
                            <span className={"hud-anchor-cell__dot is-" + pos.value} />
                            <div>
                              <strong>{pos.label}</strong>
                              <small>{pos.sub}</small>
                            </div>
                          </button>
                        ))}
                      </div>
                    </div>

                    <div className="hud-sliders-grid hud-sliders-grid--compact">
                      <SkyRangeSlider
                        label={`${focusedEntry.title} Size`}
                        min={70}
                        max={150}
                        step={5}
                        unit="%"
                        value={Math.round(focusedPlacement.scale * 100)}
                        onValueChange={(value) => updateWidgetPlacement(focusedEntry.id, { scale: value / 100 })}
                      />
                      <SkyRangeSlider
                        label={`${focusedEntry.title} Opacity`}
                        min={35}
                        max={100}
                        step={5}
                        unit="%"
                        value={Math.round(focusedPlacement.opacity * 100)}
                        onValueChange={(value) => updateWidgetPlacement(focusedEntry.id, { opacity: value / 100 })}
                      />
                      <SkyRangeSlider
                        label="Canvas Offset X"
                        min={-960}
                        max={960}
                        step={snapGrid || 4}
                        unit="px"
                        value={focusedPlacement.offsetX}
                        onValueChange={(value) => updateWidgetPlacement(focusedEntry.id, { offsetX: value })}
                      />
                      <SkyRangeSlider
                        label="Canvas Offset Y"
                        min={-540}
                        max={540}
                        step={snapGrid || 4}
                        unit="px"
                        value={focusedPlacement.offsetY}
                        onValueChange={(value) => updateWidgetPlacement(focusedEntry.id, { offsetY: value })}
                      />
                      <div className="hud-slider-span-full">
                        <SkyRangeSlider
                          label={`${focusedEntry.title} Neon & Drop-Glow`}
                          min={0}
                          max={150}
                          step={10}
                          unit="%"
                          value={focusedPlacement.glow}
                          onValueChange={(value) => updateWidgetPlacement(focusedEntry.id, { glow: value })}
                        />
                      </div>
                    </div>

                    {focusedEntry.id === "compass" && (
                      <div className="hud-compass-radius-panel">
                        <div className="hud-control-heading-row">
                          <span className="hud-control-heading">COMPASS MARKER RADIUS (50M – 1000M)</span>
                          <div className="hud-control-heading-actions">
                            {currentCompassRadius !== SKYULTRA_COMPASS_RADIUS_DEFAULT && (
                              <button
                                type="button"
                                className="char-stage-btn char-stage-btn--sm"
                                onClick={() => commitCompassRadius(SKYULTRA_COMPASS_RADIUS_DEFAULT)}
                              >
                                Reset ({SKYULTRA_COMPASS_RADIUS_DEFAULT}m)
                              </button>
                            )}
                            <em className="hud-control-badge">{currentCompassRadius}m</em>
                          </div>
                        </div>
                        <SkyRangeSlider
                          label="POI & Enemy Detection Range"
                          min={SKYULTRA_COMPASS_RADIUS_MIN}
                          max={SKYULTRA_COMPASS_RADIUS_MAX}
                          step={10}
                          unit="m"
                          value={currentCompassRadius}
                          onValueChange={(value) => commitCompassRadius(value)}
                        />
                        <div className="hud-compass-radius-input-row">
                          <label className="hud-compass-radius-input-label" htmlFor="hud-compass-radius-input">
                            <strong>Custom Radius ({SKYULTRA_COMPASS_RADIUS_MIN}–{SKYULTRA_COMPASS_RADIUS_MAX}m)</strong>
                            <span>Type any exact distance in meters</span>
                          </label>
                          <div className="hud-compass-radius-input-controls">
                            <span className="hud-compass-radius-number-wrap">
                              <input
                                id="hud-compass-radius-input"
                                type="number"
                                min={SKYULTRA_COMPASS_RADIUS_MIN}
                                max={SKYULTRA_COMPASS_RADIUS_MAX}
                                step={1}
                                inputMode="numeric"
                                aria-label="Compass radius in meters"
                                value={compassRadiusDraft}
                                onChange={(e) => {
                                  const next = e.currentTarget.value;
                                  setCompassRadiusDraft(next);
                                  const trimmed = next.trim();
                                  if (/^\d+$/.test(trimmed)) {
                                    const num = Number(trimmed);
                                    if (
                                      num >= SKYULTRA_COMPASS_RADIUS_MIN &&
                                      num <= SKYULTRA_COMPASS_RADIUS_MAX
                                    ) {
                                      updateWidgetPlacement("compass", { compassRadius: Math.round(num) });
                                    }
                                  }
                                }}
                                onBlur={() => commitCompassRadius(compassRadiusDraft)}
                                onKeyDown={(e) => {
                                  if (e.key === "Enter") {
                                    e.preventDefault();
                                    commitCompassRadius(compassRadiusDraft);
                                    e.currentTarget.blur();
                                  }
                                }}
                              />
                              <span>m</span>
                            </span>
                            <button
                              type="button"
                              className="char-stage-btn char-stage-btn--sm"
                              onClick={() => commitCompassRadius(compassRadiusDraft)}
                            >
                              Apply
                            </button>
                          </div>
                        </div>
                        <div className="hud-compass-radius-presets" role="group" aria-label="Compass radius presets">
                          {[50, 150, 300, 500, 1000].map((preset) => (
                            <button
                              key={preset}
                              type="button"
                              className={
                                "dev-filter-pill dev-filter-pill--compact" +
                                (currentCompassRadius === preset ? " is-active" : "")
                              }
                              onClick={() => commitCompassRadius(preset)}
                            >
                              {preset}m
                            </button>
                          ))}
                        </div>
                        <p className="hud-compass-radius-note">
                          Controls how far away local POIs, landmarks, doors, and enemies appear on the Sovereign
                          Compass (default 50m, max 1000m). Quests, custom waypoints, and followers remain pinned at
                          any range.
                        </p>
                      </div>
                    )}
                  </>
                )}

                {inspectorTab === "binding" && (
                  <div className="hud-binding-inspector">
                    <div className="hud-control-heading-row">
                      <span className="hud-control-heading">PROPERTY & LIVE GAME-DATA BINDING INSPECTOR</span>
                    </div>

                    <div className="hud-binding-grid">
                      <label className="hud-inspector-field">
                        <span>CUSTOM RIBBON LABEL (OPTIONAL)</span>
                        <input
                          aria-label="Custom widget ribbon label"
                          maxLength={48}
                          placeholder={`e.g. ${focusedEntry.title.toUpperCase()}`}
                          value={focusedPlacement.customLabel ?? ""}
                          onChange={(e) =>
                            updateWidgetPlacement(focusedEntry.id, {
                              customLabel: e.currentTarget.value.trim() ? e.currentTarget.value : undefined,
                            })
                          }
                        />
                      </label>

                      <div className="hud-inspector-field hud-inspector-field--full">
                        <span>LIVE SKYRIM TELEMETRY BINDING</span>
                        <div className="hud-choice-pills" role="group" aria-label="Live game-data binding path">
                          <button
                            type="button"
                            className={"dev-filter-pill dev-filter-pill--compact" + (!focusedPlacement.bindingPath ? " is-active" : "")}
                            onClick={() => updateWidgetPlacement(focusedEntry.id, { bindingPath: undefined })}
                          >
                            Native Default
                          </button>
                          {hudBindingOptions.map((opt) => (
                            <button
                              key={opt.path}
                              type="button"
                              title={`[${opt.topic.toUpperCase()}] ${opt.label} (${opt.path})`}
                              className={
                                "dev-filter-pill dev-filter-pill--compact" +
                                (focusedPlacement.bindingPath === opt.path ? " is-active" : "")
                              }
                              onClick={() => updateWidgetPlacement(focusedEntry.id, { bindingPath: opt.path })}
                            >
                              {opt.label}
                            </button>
                          ))}
                        </div>
                      </div>

                      <div className="hud-inspector-field hud-inspector-field--full">
                        <span>CONDITIONAL VISIBILITY RULE</span>
                        <div className="hud-choice-pills" role="group" aria-label="Widget conditional visibility rule">
                          {hudVisibilityOptions.map((rule) => (
                            <button
                              key={rule.value}
                              type="button"
                              title={rule.detail}
                              className={
                                "dev-filter-pill dev-filter-pill--compact" +
                                ((focusedPlacement.visibilityRule ?? "always") === rule.value ? " is-active" : "")
                              }
                              onClick={() => updateWidgetPlacement(focusedEntry.id, { visibilityRule: rule.value })}
                            >
                              {rule.label}
                            </button>
                          ))}
                        </div>
                      </div>

                      <div className="hud-binding-live-readout">
                        <div>
                          <small>LIVE BOUND VALUE</small>
                          <strong>
                            {focusedPlacement.bindingPath
                              ? resolveHudBindingValue(player, focusedPlacement.bindingPath) ?? "Awaiting snapshot"
                              : "Instrument Native"}
                          </strong>
                        </div>
                        <span
                          className={
                            "hud-rule-status " +
                            (evaluateHudVisibilityRule(player, focusedPlacement.visibilityRule) ? "is-visible" : "is-hidden")
                          }
                        >
                          {evaluateHudVisibilityRule(player, focusedPlacement.visibilityRule) ? "VISIBLE NOW" : "RULE HIDDEN"}
                        </span>
                      </div>
                    </div>
                  </div>
                )}

                {inspectorTab === "style" && (
                  <>
                    <div className="hud-modern-accent-wheel">
                      <div className="hud-control-heading-row">
                        <span className="hud-control-heading">{focusedEntry.title.toUpperCase()} ACCENT COLOR WHEEL</span>
                        <em className="hud-control-badge">{focusedPlacement.color.toUpperCase()}</em>
                      </div>
                      <SkyColorWheel
                        mode="inline"
                        size={156}
                        label={`${focusedEntry.title} Accent`}
                        value={focusedPlacement.color}
                        allowThemeMatch
                        isThemeMatched={
                          focusedPlacement.color.toLowerCase() ===
                          (palettes.find((p) => p.value === layoutTheme)?.color ?? "#e8b059").toLowerCase()
                        }
                        themeColor={palettes.find((p) => p.value === layoutTheme)?.color ?? "#e8b059"}
                        onResetTheme={() =>
                          updateWidgetPlacement(focusedEntry.id, {
                            color: palettes.find((p) => p.value === layoutTheme)?.color ?? "#e8b059",
                          })
                        }
                        onChange={(hex) => updateWidgetPlacement(focusedEntry.id, { color: hex })}
                      />
                    </div>

                    {focusedEntry.id === "character" && (
                      <div className="hud-crest-customizer">
                        <div className="hud-control-heading-row">
                          <span className="hud-control-heading">DRAGONBLADE PORTRAIT CREST STYLE &amp; BAR COLORS</span>
                          <button
                            type="button"
                            className="char-stage-btn char-stage-btn--sm"
                            onClick={() =>
                              updateWidgetPlacement("character", {
                                crestStyle: "dragonblade",
                                hpColor: undefined,
                                magickaColor: undefined,
                                staminaColor: undefined,
                              })
                            }
                          >
                            Reset Bar Colors
                          </button>
                        </div>

                        <div className="hud-crest-style-grid" role="radiogroup" aria-label="Portrait crest blade style">
                          {hudCrestStyleOptions.map((opt) => {
                            const activeStyle = (focusedPlacement.crestStyle ?? "dragonblade") === opt.value;
                            return (
                              <button
                                key={opt.value}
                                type="button"
                                role="radio"
                                aria-checked={activeStyle}
                                title={opt.description}
                                className={"hud-crest-style-card" + (activeStyle ? " is-active" : "")}
                                onClick={() =>
                                  updateWidgetPlacement("character", {
                                    crestStyle: opt.value,
                                    color: opt.colors.trim,
                                    hpColor: undefined,
                                    magickaColor: undefined,
                                    staminaColor: undefined,
                                  })
                                }
                              >
                                <div className="hud-crest-style-card__bars">
                                  <i style={{ background: opt.colors.hp }} />
                                  <i style={{ background: opt.colors.mp }} />
                                  <i style={{ background: opt.colors.sp }} />
                                </div>
                                <div className="hud-crest-style-card__copy">
                                  <strong>{opt.label}</strong>
                                  <small>{opt.tag}</small>
                                </div>
                              </button>
                            );
                          })}
                        </div>

                        {(() => {
                          const activeCrestPreset =
                            hudCrestStyleOptions.find(
                              (opt) => opt.value === (focusedPlacement.crestStyle ?? "dragonblade"),
                            ) ?? hudCrestStyleOptions[0];
                          return (
                            <div className="hud-crest-bar-wheels">
                              <div className="hud-crest-bar-wheels__item">
                                <span>HP Bar</span>
                                <SkyColorWheel
                                  mode="popover"
                                  size={144}
                                  label="Health Bar Color"
                                  value={focusedPlacement.hpColor || activeCrestPreset.colors.hp}
                                  onChange={(hex) => updateWidgetPlacement("character", { hpColor: hex })}
                                />
                              </div>
                              <div className="hud-crest-bar-wheels__item">
                                <span>Magicka Bar</span>
                                <SkyColorWheel
                                  mode="popover"
                                  size={144}
                                  label="Magicka Bar Color"
                                  value={focusedPlacement.magickaColor || activeCrestPreset.colors.mp}
                                  onChange={(hex) => updateWidgetPlacement("character", { magickaColor: hex })}
                                />
                              </div>
                              <div className="hud-crest-bar-wheels__item">
                                <span>Stamina Bar</span>
                                <SkyColorWheel
                                  mode="popover"
                                  size={144}
                                  label="Stamina Bar Color"
                                  value={focusedPlacement.staminaColor || activeCrestPreset.colors.sp}
                                  onChange={(hex) => updateWidgetPlacement("character", { staminaColor: hex })}
                                />
                              </div>
                            </div>
                          );
                        })()}
                      </div>
                    )}

                    <div className="hud-modern-typography">
                      <div className="hud-control-heading-row">
                        <span className="hud-control-heading">MODERN WIDGET TYPOGRAPHY ENGINE</span>
                        <em className="hud-control-badge">{focusedPlacement.fontStyle.toUpperCase()}</em>
                      </div>
                      <div className="hud-font-grid" role="radiogroup" aria-label="Widget typography style">
                        {hudFontOptions.map((font) => {
                          const active = focusedPlacement.fontStyle === font.value;
                          return (
                            <button
                              key={font.value}
                              type="button"
                              role="radio"
                              aria-checked={active}
                              className={"hud-font-card" + (active ? " is-active" : "")}
                              data-font-preview={font.value}
                              onClick={() => updateWidgetPlacement(focusedEntry.id, { fontStyle: font.value })}
                            >
                              <div className="hud-font-card__top">
                                <span className="hud-font-card__tag">{font.tag}</span>
                                <strong className="hud-font-card__sample">{font.sample}</strong>
                              </div>
                              <b className="hud-font-card__label">{font.label}</b>
                              <small className="hud-font-card__desc">{font.description}</small>
                            </button>
                          );
                        })}
                      </div>
                    </div>

                    <div className="hud-modern-theme-scope">
                      <div className="hud-control-heading-row">
                        <span className="hud-control-heading">WIDGET THEME ({focusedEntry.title.toUpperCase()})</span>
                        <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
                          <button
                            type="button"
                            className="char-stage-btn char-stage-btn--sm"
                            onClick={() => applyThemeToAllWidgets(focusedTheme)}
                            title={`Apply ${palettes.find((p) => p.value === focusedTheme)?.label ?? "this"} theme to every HUD instrument`}
                          >
                            Apply to All
                          </button>
                          <em className="hud-control-badge">
                            {palettes.find((p) => p.value === focusedTheme)?.label ?? "Dragonstone"}
                          </em>
                        </div>
                      </div>
                      <div className="hud-theme-chip-grid" role="radiogroup" aria-label={`Theme for ${focusedEntry.title}`}>
                        {palettes.map((p) => {
                          const active = focusedTheme === p.value;
                          return (
                            <button
                              key={p.value}
                              type="button"
                              role="radio"
                              aria-checked={active}
                              className={"hud-theme-chip" + (active ? " is-active" : "")}
                              onClick={() => selectHudTheme(p.value)}
                            >
                              <span className="hud-theme-chip__swatches" aria-hidden="true">
                                <i style={{ background: p.color }} />
                                <i style={{ background: p.secondary }} />
                              </span>
                              <span className="hud-theme-chip__copy">
                                <strong>{p.label}</strong>
                                <small>{p.styleTag}</small>
                              </span>
                            </button>
                          );
                        })}
                      </div>
                    </div>
                  </>
                )}
              </SkyFrame>
            </div>

            <SkyFrame
              title="Visual HUD Builder Canvas (9-Zone · 1:1 Screen Scale)"
              eyebrow="DRAG ANY HUD WIDGET TO PLACE ANYWHERE · DRAG CORNER TO RESIZE · DROP ON ZONE PILL TO RE-ANCHOR"
              className="widget-preview-frame"
              actions={
                <div className="hud-stage-toolbar">
                  <div className="hud-snap-pills" role="group" aria-label="Magnetic grid snap">
                    {([0, 8, 16, 24] as const).map((grid) => (
                      <button
                        key={grid}
                        type="button"
                        className={"hud-category-pill" + (snapGrid === grid ? " is-active" : "")}
                        onClick={() => setSnapGrid(grid)}
                        title={grid === 0 ? "Free pixel dragging" : `Magnetic ${grid}px grid snapping`}
                      >
                        {grid === 0 ? "Snap: Free" : `Snap ${grid}px`}
                      </button>
                    ))}
                  </div>
                  <button
                    type="button"
                    className={"char-stage-btn char-stage-btn--sm" + (showSafeGuides ? " char-stage-btn--active" : "")}
                    onClick={() => setShowSafeGuides((v) => !v)}
                    title="Toggle 90% safe-area perimeter and center crosshair alignment guides"
                  >
                    <Icon name="layers" size={13} /> Safe Guides
                  </button>
                  <button
                    type="button"
                    className={"char-stage-btn char-stage-btn--sm" + (showLayoutJson ? " char-stage-btn--active" : "")}
                    onClick={() => {
                      const snapshotJson = JSON.stringify(
                        {
                          version: 1,
                          widgets: selected,
                          anchor: focusedPlacement.anchor,
                          scale: focusedPlacement.scale,
                          opacity: focusedPlacement.opacity,
                          theme: layoutTheme,
                          colors: buildColorsMap(),
                          glow: focusedPlacement.glow,
                          snapGrid,
                          placements: buildPlacementsMap(),
                        },
                        null,
                        2,
                      );
                      setLayoutJsonDraft(snapshotJson);
                      setShowLayoutJson((v) => !v);
                    }}
                    title="Inspect, copy, or import shared SkyUltra HUD layout JSON"
                  >
                    <Icon name="code" size={13} /> Layout JSON
                  </button>
                  <button type="button" className="char-stage-btn char-stage-btn--sm" onClick={() => setTab("export")}>
                    <Icon name="download" size={13} /> Export Mod
                  </button>
                  <SkySegmentedControl
                    label="Preview stage mode"
                    value={stageMode}
                    onValueChange={(v) => {
                      if (v === "viewport" || v === "inspector") setStageMode(v);
                    }}
                    options={[
                      { value: "viewport", label: "9-Zone Canvas" },
                      { value: "inspector", label: "JSON & Order" },
                    ]}
                  />
                </div>
              }
              footer={
                <div className="hud-stage-footer">
                  <SkyKeybind keys={["Esc"]}>Close menu; pinned transparent HUD stays active in Skyrim</SkyKeybind>
                  <span>
                    Focused: <strong style={{ color: focusedPlacement.color }}>{focusedEntry.title}</strong> (
                    {focusedPlacement.anchor.toUpperCase()} · X {focusedPlacement.offsetX}px, Y {focusedPlacement.offsetY}px ·{" "}
                    {Math.round(focusedPlacement.scale * 100)}%)
                  </span>
                </div>
              }
            >
              {showLayoutJson && (
                <div className="hud-layout-json-drawer">
                  <div className="hud-layout-json-drawer__head">
                    <strong>HUD Layout JSON Import / Export</strong>
                    <div className="hud-layout-json-drawer__actions">
                      <button
                        type="button"
                        className="char-stage-btn char-stage-btn--sm"
                        onClick={() => {
                          if (navigator.clipboard) {
                            void navigator.clipboard.writeText(layoutJsonDraft);
                            notify("Copied HUD Layout JSON", "Shared layout specification copied to clipboard.");
                          }
                        }}
                      >
                        <Icon name="copy" size={12} /> Copy JSON
                      </button>
                      <button
                        type="button"
                        className="char-stage-btn char-stage-btn--sm char-stage-btn--primary"
                        onClick={() => {
                          try {
                            const parsed = JSON.parse(layoutJsonDraft) as unknown;
                            const coerced = coerceLegacyHudLayout(parsed, layoutTheme || "dragonstone");
                            if (!coerced) throw new Error("Invalid HUD layout JSON");
                            setSelected(coerced.widgets.filter((id) => widgetCatalog.some((w) => w.id === id)) as WidgetKind[]);
                            setLayoutTheme(coerced.theme);
                            if (typeof coerced.snapGrid === "number") {
                              setSnapGrid(coerced.snapGrid as 0 | 8 | 16 | 24);
                            }
                            if (coerced.placements) {
                              setPlacements((prev) => {
                                const next = { ...prev };
                                for (const [k, v] of Object.entries(coerced.placements!) as Array<
                                  [WidgetKind, SkyUltraWidgetPlacement]
                                >) {
                                  if (next[k] && v) next[k] = { ...next[k], ...v };
                                }
                                return next;
                              });
                            }
                            notify("Layout JSON Applied", `Loaded ${coerced.widgets.length} HUD widget(s) into stage.`);
                            setShowLayoutJson(false);
                          } catch {
                            notify("Invalid Layout JSON", "Verify syntax matches SkyUltra HUD schema v1.");
                          }
                        }}
                      >
                        <Icon name="check" size={12} /> Apply JSON
                      </button>
                    </div>
                  </div>
                  <textarea
                    aria-label="HUD layout JSON"
                    className="hud-layout-json-drawer__textarea"
                    spellCheck={false}
                    value={layoutJsonDraft}
                    onChange={(e) => setLayoutJsonDraft(e.currentTarget.value)}
                  />
                </div>
              )}

              <div
                ref={stageHostRef}
                className={"widget-preview widget-preview--proportional" + (dragHudState ? " is-dragging-hud" : "")}
                data-anchor={focusedPlacement.anchor}
                data-stage-mode={stageMode}
                data-snap-grid={snapGrid}
              >
                <img src="./SkyUltraLogo.png" alt="" draggable={false} className="widget-preview-watermark" />
                {dragHudState && (
                  <div className="hud-stage-alignment-guides" aria-hidden="true">
                    <i className="hud-guide-x" />
                    <i className="hud-guide-y" />
                  </div>
                )}

                {stageMode === "viewport" ? (
                  <div
                    className="hud-virtual-viewport"
                    style={{
                      width: `${Math.round(1920 * stageScale)}px`,
                      height: `${Math.round(1080 * stageScale)}px`,
                    }}
                  >
                    <SkyThemeScope
                      theme={layoutTheme}
                      className="hud-virtual-screen"
                      style={{ transform: `scale(${stageScale})` }}
                    >
                      {showSafeGuides && (
                        <div className="hud-safe-area-guides" aria-hidden="true">
                          <div className="hud-safe-area-guides__frame">
                            <span>90% ACTION SAFE AREA</span>
                          </div>
                          <i className="hud-safe-area-guides__cross-h" />
                          <i className="hud-safe-area-guides__cross-v" />
                        </div>
                      )}

                      <div className="hud-nine-zone-guides">
                        {hudAnchorOptions.map((zone) => {
                          const zoneWidgets = selected.filter(
                            (kind) => (placements[kind]?.anchor ?? defaultWidgetPlacements()[kind].anchor) === zone.value,
                          );
                          const isFocusedZone = focusedPlacement.anchor === zone.value;
                          const isDropTarget = dragHudState?.hoverZone === zone.value;
                          return (
                            <div
                              key={zone.value}
                              data-hud-zone={zone.value}
                              className={
                                `hud-nine-zone-cell hud-nine-zone-cell--${zone.value}` +
                                (isFocusedZone ? " is-focused-zone" : "") +
                                (isDropTarget ? " is-drop-target" : "") +
                                (zoneWidgets.length > 0 ? " has-widgets" : "")
                              }
                            >
                              <button
                                type="button"
                                data-zone-drop={zone.value}
                                className="hud-stage-zone__badge"
                                title={`Move ${focusedEntry.title} to ${zone.label} (or drop dragged widget here)`}
                                onClick={() =>
                                  updateWidgetPlacement(focusedEntry.id, { anchor: zone.value, offsetX: 0, offsetY: 0 })
                                }
                              >
                                <span className="hud-stage-zone__mark" />
                                <span>{zone.shortLabel}</span>
                                {zoneWidgets.length > 0 && <b>{zoneWidgets.length}</b>}
                              </button>
                            </div>
                          );
                        })}
                      </div>

                      <div className="hud-virtual-screen-hud sky-hud-screen-grid">
                        {hudAnchorOptions.map((zone) => {
                          const zoneWidgets = selected.filter(
                            (kind) => (placements[kind]?.anchor ?? defaultWidgetPlacements()[kind].anchor) === zone.value,
                          );
                          if (!zoneWidgets.length) return null;
                          return (
                            <div key={zone.value} className={`sky-hud-zone sky-hud-zone--${zone.value}`}>
                              {zoneWidgets.map((kind) => {
                                const wp = placements[kind] ?? defaultWidgetPlacements()[kind];
                                const isInteracting = dragHudState?.kind === kind;
                                return (
                                  <div
                                    key={kind}
                                    ref={(el) => {
                                      widgetNodeRefs.current[kind] = el;
                                    }}
                                    className={
                                      "sky-hud-zone__item hud-viewport-item" +
                                      (activeWidget === kind ? " is-selected-hud" : "") +
                                      (isInteracting ? " is-interacting" : "")
                                    }
                                    style={{
                                      opacity: wp.opacity,
                                      transform: `translate3d(${wp.offsetX}px, ${wp.offsetY}px, 0) scale(${wp.scale})`,
                                    }}
                                    onPointerDown={(e) => startStagePointerAction(e, kind, "move")}
                                    onPointerMove={handleStagePointerMove}
                                    onPointerUp={finishStagePointerAction}
                                    onPointerCancel={finishStagePointerAction}
                                    onClick={(e) => {
                                      e.stopPropagation();
                                      setActiveWidget(kind);
                                    }}
                                  >
                                    <div className="hud-viewport-item__gizmo">
                                      <span
                                        className="hud-viewport-item__drag-handle"
                                        title="Drag widget anywhere to position (snaps to grid; drop onto a zone pill to re-anchor)"
                                      >
                                        <Icon name="move" size={18} />
                                        <span
                                          ref={(el) => {
                                            dragBadgeRefs.current[kind] = el;
                                          }}
                                        >
                                          {wp.offsetX}, {wp.offsetY} · {Math.round(wp.scale * 100)}%
                                        </span>
                                      </span>
                                    </div>
                                    <SkyThemeScope
                                      theme={
                                        wp.theme && palettes.some((p) => p.value === wp.theme) ? wp.theme : layoutTheme
                                      }
                                      style={{ background: "transparent" }}
                                    >
                                      <HudWidget
                                        kind={kind}
                                        player={player}
                                        bridge={bridge}
                                        nearbyActors={nearbyActors}
                                        accentColor={wp.color}
                                        glow={wp.glow}
                                        fontStyle={wp.fontStyle}
                                        customLabel={wp.customLabel}
                                        bindingPath={wp.bindingPath}
                                        crestStyle={wp.crestStyle}
                                        hpColor={wp.hpColor}
                                        magickaColor={wp.magickaColor}
                                        staminaColor={wp.staminaColor}
                                        compassRadius={wp.compassRadius}
                                      />
                                    </SkyThemeScope>
                                    <span
                                      className="hud-viewport-item__resize-handle"
                                      title="Drag corner to resize instrument"
                                      onPointerDown={(e) => startStagePointerAction(e, kind, "resize")}
                                      onPointerMove={handleStagePointerMove}
                                      onPointerUp={finishStagePointerAction}
                                      onPointerCancel={finishStagePointerAction}
                                    >
                                      <Icon name="resizeCorner" size={16} />
                                    </span>
                                  </div>
                                );
                              })}
                            </div>
                          );
                        })}
                      </div>

                      {!selected.length && (
                        <div className="empty-state hud-stage-empty">
                          <Icon name="widgets" size={38} />
                          <strong>No HUD Instruments Active</strong>
                          <span>Toggle one or more transparent HUD instruments on the left to preview them here.</span>
                        </div>
                      )}
                    </SkyThemeScope>
                  </div>
                ) : (
                  <div className="widget-preview-grid">
                    <SkyThemeScope theme={layoutTheme}>
                      <SkyHudComposer
                        value={{
                          version: 1,
                          widgets: selected,
                          anchor: focusedPlacement.anchor,
                          scale: focusedPlacement.scale,
                          opacity: focusedPlacement.opacity,
                          theme: layoutTheme,
                          colors: buildColorsMap(),
                          glow: focusedPlacement.glow,
                          placements: buildPlacementsMap(),
                        }}
                        activeWidget={activeWidget}
                        onSelectWidget={setActiveWidget}
                        onValueChange={(layout) => {
                          setSelected(layout.widgets);
                          setLayoutTheme(layout.theme);
                          if (layout.placements) {
                            setPlacements((prev) => {
                              const next = { ...prev };
                              for (const [k, v] of Object.entries(layout.placements!) as Array<
                                [WidgetKind, SkyUltraWidgetPlacement]
                              >) {
                                if (next[k] && v) next[k] = { ...next[k], ...v };
                              }
                              return next;
                            });
                          }
                        }}
                        renderWidget={(kind) => {
                          const wp = placements[kind] ?? defaultWidgetPlacements()[kind];
                          return (
                            <HudWidget
                              kind={kind}
                              player={player}
                              bridge={bridge}
                              nearbyActors={nearbyActors}
                              accentColor={wp.color}
                              glow={wp.glow}
                              fontStyle={wp.fontStyle}
                              crestStyle={wp.crestStyle}
                              hpColor={wp.hpColor}
                              magickaColor={wp.magickaColor}
                              staminaColor={wp.staminaColor}
                            />
                          );
                        }}
                      />
                    </SkyThemeScope>
                  </div>
                )}
              </div>
            </SkyFrame>
          </div>

          <div className="widget-bottom-note">
            <Icon name="performance" size={17} />
            <span>
              Zero-container transparent rendering: all HUD widgets float directly over Skyrim with independent 9-zone
              positioning, radial Overview vitals, and live 3D player portrait.
            </span>
            <button className="text-action" onClick={() => onNavigate("guide")}>
              Read the SkyUltra Guide <Icon name="arrow" size={13} />
            </button>
          </div>
        </>
      )}

      {tab === "patterns" && (
        <div className="pattern-grid">
          <SkyFrame
            eyebrow="EIGHT DIRECTIONS"
            title="The quick wheel"
            footer="Pointer and keyboard navigation. Actions here open kit sections or UI demonstrations."
          >
            <SkyActionWheel
              label="Field tools"
              actions={actions}
              onSelect={(id) => {
                if (id === "notice")
                  notify(
                    "A message for your journey",
                    "Notifications share the same frame language as the rest of the kit.",
                  );
                else if (id === "confirm" || id === "quantity") setPopup(id);
                else onNavigate(id as Route);
              }}
            />
          </SkyFrame>
          <SkyFrame eyebrow="DECISIONS, WITHOUT FRICTION" title="Popup collection">
            <p className="pattern-copy">
              A consistent place for confirmations, quantities, payments, searchable picks, and guided setup.
            </p>
            <div className="popup-catalog">
              {(
                [
                  { id: "confirm", title: "Confirmation", description: "Clear consequence, clear action", icon: "check" },
                  { id: "quantity", title: "Quantity picker", description: "Bounded slider and precise amount", icon: "plus" },
                  { id: "transaction", title: "Transaction card", description: "Source, destination, and total", icon: "gold" },
                  { id: "picker", title: "Searchable item picker", description: "Real inventory with pagination", icon: "inventory" },
                  { id: "guide", title: "Guided setup", description: "A focused sequence of steps", icon: "book" },
                ] as const
              ).map((item) => (
                <button type="button" key={item.id} onClick={() => setPopup(item.id)}>
                  <Icon name={item.icon} />
                  <span>
                    <strong>{item.title}</strong>
                    <small>{item.description}</small>
                  </span>
                  <Icon name="arrow" size={14} />
                </button>
              ))}
            </div>
          </SkyFrame>
          <SkyFrame
            eyebrow="MOD-PROVIDED DATA"
            title="Objective tracker"
            footer="This objective is a component fixture. A mod supplies its own quest data."
          >
            <SkyQuestTracker
              title="The northern passage"
              subtitle="Make ready before crossing the pass."
              objectives={[
                { id: "map", label: "Study the route", complete: true },
                { id: "supplies", label: "Gather provisions", current: 3, target: 5 },
                { id: "leave", label: "Reach the watchtower" },
              ]}
            />
          </SkyFrame>
          <SkyFrame eyebrow="THE SESSION LEDGER" title="Events & notifications">
            <SkyTimeline
              entries={[
                { id: "open", title: "SkyUltra interface opened", detail: "Your current character snapshot is connected.", time: "This session" },
                { id: "layout", title: enabled ? "Gameplay layout pinned" : "Layout in preview", detail: selected.length + " selected instruments.", time: "Now" },
              ]}
            />
            <button
              className="control-button"
              onClick={() => notify("Your layout is ready", "A compact notification with a dismiss action.")}
            >
              Preview notification
            </button>
            <SkyNotificationStack
              notices={notices}
              onDismiss={(id) => setNotices((current) => current.filter((notice) => notice.id !== id))}
            />
          </SkyFrame>
          <SkyFrame
            eyebrow="COMPACT PARTY SURFACES"
            title="Character & condition card"
            footer="Character cards accept any mod-provided identity, portrait, stats, and actions."
          >
            <SkyCharacterCard name={player?.playerName ?? "Your character"} level={player?.level} subtitle={player?.location}>
              <div className="button-row">
                <button className="control-button" onClick={() => onNavigate("character")}>
                  Character sheet
                </button>
                <button className="control-button" onClick={() => onNavigate("inventory")}>
                  Equipment
                </button>
              </div>
            </SkyCharacterCard>
          </SkyFrame>
        </div>
      )}

      <SkyPopup
        open={popup !== null}
        title={
          popup === "quantity"
            ? "Choose an amount"
            : popup === "transaction"
              ? "Review the exchange"
              : popup === "picker"
                ? "Choose from your inventory"
                : popup === "guide"
                  ? "Make the kit your own"
                  : "Confirm your choice"
        }
        description={
          popup === "picker"
            ? "Select an entry from your current inventory snapshot."
            : "Interactive UI-kit preview. These examples do not change your inventory, gold, or NPCs."
        }
        onDismiss={() => setPopup(null)}
        actions={
          <>
            <button className="control-button" onClick={() => setPopup(null)}>
              Back
            </button>
            <button
              className="primary-button"
              onClick={() => {
                notify(
                  "Preview confirmed",
                  popup === "quantity"
                    ? quantity + " selected in the quantity component."
                    : "The confirmation callback was received.",
                );
                setPopup(null);
              }}
            >
              Confirm preview
            </button>
          </>
        }
      >
        {popup === "quantity" && (
          <div className="quantity-preview">
            <strong>{quantity}</strong>
            <input
              aria-label="Selected quantity"
              type="range"
              min={1}
              max={99}
              value={quantity}
              onChange={(event) => setQuantity(Number(event.currentTarget.value))}
            />
            <div className="button-row">
              {[1, 5, 10, 25, 99].map((value) => (
                <button className="control-button" key={value} onClick={() => setQuantity(value)}>
                  {value === 99 ? "All" : value}
                </button>
              ))}
            </div>
          </div>
        )}
        {popup === "transaction" && (
          <div className="transaction-preview">
            <div>
              <span>Item</span>
              <strong>Field provisions</strong>
            </div>
            <div>
              <span>Quantity</span>
              <strong>{quantity}</strong>
            </div>
            <div>
              <span>Unit price</span>
              <strong>12 G</strong>
            </div>
            <div className="transaction-total">
              <span>Preview total</span>
              <strong>{quantity * 12} G</strong>
            </div>
          </div>
        )}
        {popup === "picker" && (
          <>
            <label className="page-search">
              <Icon name="search" />
              <input
                aria-label="Filter item picker"
                value={pickQuery}
                placeholder="Search item names…"
                onChange={(event) => {
                  setPickQuery(event.currentTarget.value);
                  setPickPage(1);
                }}
              />
            </label>
            <div className="item-picker-list">
              {inventory.slice((pickPage - 1) * 6, pickPage * 6).map((item) => (
                <button
                  key={item.id}
                  onClick={() => {
                    notify("Item selected", item.name);
                    setPopup(null);
                  }}
                >
                  <span>{item.name}</span>
                  <small>
                    ×{item.count} · {item.value} G
                  </small>
                </button>
              ))}
            </div>
            <SkyPagination page={pickPage} pages={Math.ceil(inventory.length / 6)} onPageChange={setPickPage} />
          </>
        )}
        {popup === "guide" && (
          <SkyStepGuide
            active={guideStep}
            onStepChange={setGuideStep}
            steps={[
              { id: "choose", title: "Choose your instruments", description: "Pick the transparent widgets that help you play." },
              { id: "frame", title: "Place & style each widget", description: "Assign custom screen positions, sizes, colors, and typography per widget." },
              { id: "pin", title: "Preview, then pin", description: "Keep the layout inside the studio or pin it over Skyrim gameplay." },
            ]}
          />
        )}
        {popup === "confirm" && (
          <div className="confirm-preview">
            <Icon name="shield" size={40} />
            <h3>A considered decision</h3>
            <p>
              Use confirmations for actions that need context. Escape returns to the panel, and keyboard focus stays inside
              this popup.
            </p>
          </div>
        )}
      </SkyPopup>

      {notices.length > 0 && tab !== "patterns" && (
        <div className="widget-notification-preview">
          <SkyNotificationStack
            notices={notices}
            onDismiss={(id) => setNotices((current) => current.filter((notice) => notice.id !== id))}
          />
        </div>
      )}
    </div>
  );
}
