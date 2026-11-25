import * as React from "react";
import { lazy, Suspense, useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import {
  SKYULTRA_CONTROLLER_GLYPHS,
  connectSkyUltraDeclarativeSounds,
  connectSkyUltraInputScopes,
  createSkyUltraGame,
  getSkyUltraRuntimeInfo,
  getSkyWebView,
  installSkyUltraCompetitorShims,
  loadSkyUltraProfileStore,
  markSkyUltraPageReady,
  closeSkyUltraPage,
  persistSkyUltraProfileStore,
  readLocalSkyUltraProfileStore,
  readSkyUltraSettings,
  resolveEffectiveSkyUltraProfile,
  formatSkyUltraHotkey,
  defaultSkyUltraHotkey,
  defaultSkyUltraKeyboardHotkey,
  defaultSkyUltraSettings,
  readSkyUltraUiPrefs,
  updateProfileForContext,
  updateSkyUltraSettings,
  writeLocalSkyUltraProfileStore,
  type JsonValue,
  type SkyUltraActiveBookSnapshot,
  type SkyUltraControllerGlyphFamily,
  type SkyUltraBarterDisplayMode,
  type SkyUltraContainerDisplayMode,
  type SkyUltraDisplayMode,
  type SkyUltraEngineChoice,
  type SkyUltraHotkeyBinding,
  type SkyUltraProfileStoreV2,
  type SkyUltraSettings,
  type SkyUltraUiPreferences,
} from "@skyultra/sdk";
import { enterSkyInputScope, getSkyInputScopeDepth } from "../interaction.js";
import { SkyRingMeter, SkyThemeScope } from "../core.js";
import { SkyCursorScope, SkyMenuHeader, SkyBrand, SkyNavigationRail, SkyWelcomeBanner } from "../chrome.js";
import { EquipmentGlyph } from "./EquipmentGlyph.js";
import type { ReadingBookState } from "./BookReader.js";
import { equipmentSlots } from "./EquipmentPanel.js";
import { Icon, itemIcon, type IconName } from "./icons.js";
import { ItemThumbnail } from "./ItemThumbnail.js";
import { PlayerPortrait } from "./PlayerPortrait.js";
import { Empty, SectionTitle } from "./pieces.js";
import { palettes, type Palette, type Route } from "./model.js";
import { useCadence, usePlayer } from "./useFieldData.js";

import { focusMenuSearch, isMenuSearchKey, loadMenuPage, preloadMenuPage } from "./menuNavigation.js";
import { leaveMenuPage } from "./previewLifecycle.js";
import { TabSplash } from "./TabSplash.js";

const RawActorsPage = lazy(() => loadMenuPage("actors"));
const ActorsPage = React.memo(
  RawActorsPage,
  (prev, next) =>
    prev.bridge === next.bridge &&
    prev.obscured === next.obscured &&
    (prev.player?.inventory?.length ?? 0) === (next.player?.inventory?.length ?? 0),
);
const InventoryPage = React.memo(lazy(() => loadMenuPage("inventory")));
const BarterPage = lazy(() => import("./BarterPage.js"));
const ContainerPage = lazy(() => import("./ContainerPage.js"));
const MannequinPage = lazy(() => import("./MannequinPage.js"));
const RawMagicPage = lazy(() => import("./MagicPage.js"));
const MagicPage = React.memo(
  RawMagicPage,
  (prev, next) =>
    prev.bridge === next.bridge &&
    prev.obscured === next.obscured &&
    prev.displayMode === next.displayMode &&
    prev.pauseWorld === next.pauseWorld &&
    prev.onChangeDisplayMode === next.onChangeDisplayMode &&
    prev.onTogglePauseWorld === next.onTogglePauseWorld &&
    prev.onClose === next.onClose &&
    prev.player?.playerName === next.player?.playerName &&
    prev.player?.magicka?.current === next.player?.magicka?.current &&
    prev.player?.magicka?.max === next.player?.magicka?.max &&
    prev.player?.activeEffects === next.player?.activeEffects,
);
const RawFavoritesPage = lazy(() => import("./FavoritesPage.js"));
const FavoritesPage = React.memo(
  RawFavoritesPage,
  (prev, next) =>
    prev.bridge === next.bridge &&
    prev.obscured === next.obscured &&
    prev.displayMode === next.displayMode &&
    prev.pauseWorld === next.pauseWorld &&
    prev.onChangeDisplayMode === next.onChangeDisplayMode &&
    prev.onTogglePauseWorld === next.onTogglePauseWorld &&
    prev.onClose === next.onClose &&
    prev.player?.playerName === next.player?.playerName,
);
const RawBlacksmithPage = lazy(() => import("./BlacksmithPage.js"));
const BlacksmithPage = React.memo(
  RawBlacksmithPage,
  (prev, next) =>
    prev.bridge === next.bridge &&
    prev.obscured === next.obscured &&
    prev.displayMode === next.displayMode &&
    prev.pauseWorld === next.pauseWorld &&
    prev.onChangeDisplayMode === next.onChangeDisplayMode &&
    prev.onTogglePauseWorld === next.onTogglePauseWorld &&
    prev.onClose === next.onClose &&
    prev.player?.playerName === next.player?.playerName,
);
const RawEnchantingPage = lazy(() => import("./EnchantingPage.js"));
const EnchantingPage = React.memo(
  RawEnchantingPage,
  (prev, next) =>
    prev.bridge === next.bridge &&
    prev.obscured === next.obscured &&
    prev.displayMode === next.displayMode &&
    prev.pauseWorld === next.pauseWorld &&
    prev.onChangeDisplayMode === next.onChangeDisplayMode &&
    prev.onTogglePauseWorld === next.onTogglePauseWorld &&
    prev.onClose === next.onClose &&
    prev.player?.playerName === next.player?.playerName,
);
const RawAlchemyPage = lazy(() => import("./AlchemyPage.js"));
const AlchemyPage = React.memo(
  RawAlchemyPage,
  (prev, next) =>
    prev.bridge === next.bridge &&
    prev.obscured === next.obscured &&
    prev.displayMode === next.displayMode &&
    prev.pauseWorld === next.pauseWorld &&
    prev.onChangeDisplayMode === next.onChangeDisplayMode &&
    prev.onTogglePauseWorld === next.onTogglePauseWorld &&
    prev.onClose === next.onClose &&
    prev.player?.playerName === next.player?.playerName,
);
const CharacterPage = React.memo(lazy(() => loadMenuPage("character")));
const SettingsPage = React.memo(lazy(() => loadMenuPage("settings")));
const PerformancePage = React.memo(lazy(() => loadMenuPage("performance")));
const WidgetsPage = React.memo(lazy(() => loadMenuPage("widgets")));
const DevStudioPage = React.memo(lazy(() => loadMenuPage("devtools")));
const GuidePage = React.memo(lazy(() => loadMenuPage("guide")));
const FieldOverlays = lazy(() => import("./FieldOverlays.js"));
const StandaloneBookReader = lazy(() =>
  import("./BookReader.js").then((mod) => ({ default: mod.BookReader })),
);
const appModuleStartedAt = performance.now();
const HOST_SETTINGS_CACHE_KEY = "skyultra.hostSettingsCache.v1";

function readCachedHostSettings(): SkyUltraSettings {
  try {
    if (typeof window === "undefined") return defaultSkyUltraSettings;
    const raw = window.localStorage.getItem(HOST_SETTINGS_CACHE_KEY);
    if (!raw) return defaultSkyUltraSettings;
    const parsed = JSON.parse(raw) as Partial<SkyUltraSettings>;
    if (!parsed || typeof parsed !== "object") return defaultSkyUltraSettings;
    return { ...defaultSkyUltraSettings, ...parsed };
  } catch {
    return defaultSkyUltraSettings;
  }
}

function writeCachedHostSettings(settings: SkyUltraSettings): void {
  try {
    if (typeof window === "undefined") return;
    window.localStorage.setItem(HOST_SETTINGS_CACHE_KEY, JSON.stringify(settings));
  } catch {
    // Ignore storage quota or private-mode failures.
  }
}

function SessionReadyBeacon({ epoch, bridge }: { epoch: number; bridge: ReturnType<typeof getSkyWebView> }): null {
  useEffect(() => {
    let cancelled = false;
    let raf1 = 0;
    let raf2 = 0;
    let raf3 = 0;
    let fallback = 0;
    let signaled = false;
    const done = (): void => {
      if (cancelled || signaled) return;
      signaled = true;
      void markSkyUltraPageReady(bridge, epoch > 0 ? { epoch } : undefined).catch(() => undefined);
    };
    raf1 = requestAnimationFrame(() => {
      raf2 = requestAnimationFrame(() => {
        if (epoch > 0) {
          raf3 = requestAnimationFrame(done);
        } else {
          done();
        }
      });
    });
    if (epoch === 0) {
      fallback = window.setTimeout(done, 60);
    }
    return () => {
      cancelled = true;
      cancelAnimationFrame(raf1);
      cancelAnimationFrame(raf2);
      cancelAnimationFrame(raf3);
      if (fallback) window.clearTimeout(fallback);
    };
  }, [bridge, epoch]);
  return null;
}

const navigation: Array<{ id: Route; title: string; note: string; icon: IconName }> = [
  { id: "overview", title: "Overview", note: "Dossier, vitals & armory", icon: "overview" },
  { id: "actors", title: "Actor observatory", note: "Portraits, 3D & dress-up", icon: "actors" },
  { id: "inventory", title: "Inventory", note: "11-slot gear & item studio", icon: "inventory" },
  { id: "character", title: "Character sheet", note: "Build, wards & 18 skills", icon: "shield" },
  { id: "settings", title: "Settings", note: "Hotkeys, themes & studio", icon: "components" },
  { id: "widgets", title: "HUDs & widgets", note: "Visual HUD builder & export", icon: "widgets" },
  { id: "performance", title: "Performance lab", note: "Engine & frame telemetry", icon: "performance" },
  { id: "devtools", title: "Developer Studio", note: "Views, bridge, doctor & reload", icon: "rune" },
  { id: "guide", title: "SkyUltra Guide", note: "Architecture & controls", icon: "book" },
];
export function App(): React.JSX.Element {
  const bridge = useMemo(() => getSkyWebView(), []);
  const runtime = useMemo(() => getSkyUltraRuntimeInfo(), []);
  const [profileStore, setProfileStore] = useState<SkyUltraProfileStoreV2>(() => readLocalSkyUltraProfileStore());
  const [uiPrefs, setUiPrefs] = useState<SkyUltraUiPreferences>(() => readSkyUltraUiPrefs());
  const [sessionDormant, setSessionDormant] = useState(false);
  const [standaloneInventory, setStandaloneInventory] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "inventory";
    } catch {
      return false;
    }
  });
  const [standaloneBarter, setStandaloneBarter] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "barter";
    } catch {
      return false;
    }
  });
  const [standaloneMagic, setStandaloneMagic] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "magic";
    } catch {
      return false;
    }
  });
  const [standaloneFavorites, setStandaloneFavorites] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "favorites";
    } catch {
      return false;
    }
  });
  const [standaloneContainer, setStandaloneContainer] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "container";
    } catch {
      return false;
    }
  });
  const [standaloneMannequin, setStandaloneMannequin] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "mannequin";
    } catch {
      return false;
    }
  });
  const [standaloneLoot, setStandaloneLoot] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "loot";
    } catch {
      return false;
    }
  });
  const [standalonePickpocket, setStandalonePickpocket] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "pickpocket";
    } catch {
      return false;
    }
  });
  const [standaloneBlacksmith, setStandaloneBlacksmith] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "blacksmith";
    } catch {
      return false;
    }
  });
  const [standaloneEnchanting, setStandaloneEnchanting] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "enchanting";
    } catch {
      return false;
    }
  });
  const [standaloneAlchemy, setStandaloneAlchemy] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "alchemy";
    } catch {
      return false;
    }
  });
  const [standaloneBook, setStandaloneBook] = useState<boolean>(() => {
    try {
      return new URLSearchParams(window.location.search).get("mode") === "book";
    } catch {
      return false;
    }
  });
  const [bookLoadEpoch, setBookLoadEpoch] = useState<number>(0);
  const [activeBookSnapshot, setActiveBookSnapshot] = useState<SkyUltraActiveBookSnapshot | null>(null);
  const [activeBookNotice, setActiveBookNotice] = useState<string | null>(null);
  const [busyTakingBook, setBusyTakingBook] = useState<boolean>(false);
  const standaloneBookRef = useRef<boolean>(standaloneBook);
  standaloneBookRef.current = standaloneBook;
  const activeBookSnapshotRef = useRef<SkyUltraActiveBookSnapshot | null>(activeBookSnapshot);
  activeBookSnapshotRef.current = activeBookSnapshot;
  const handleTakeActiveBookRef = useRef<() => void>(() => {});
  const isStandaloneOverlay =
    standaloneInventory ||
    standaloneBarter ||
    standaloneMagic ||
    standaloneFavorites ||
    standaloneContainer ||
    standaloneMannequin ||
    standaloneLoot ||
    standalonePickpocket ||
    standaloneBlacksmith ||
    standaloneEnchanting ||
    standaloneAlchemy ||
    standaloneBook;
  const [route, setRouteState] = useState<Route>(() => {
    try {
      const raw = window.localStorage.getItem("skyultra.switchState");
      if (raw) {
        const parsed = JSON.parse(raw) as { route?: Route; ts?: number };
        if (parsed.ts && Date.now() - parsed.ts < 15000 && parsed.route) {
          return parsed.route;
        }
      }
    } catch {
      // Ignore storage access errors
    }
    return "overview";
  });
  const cadence = useCadence(bridge, !isStandaloneOverlay && route === "performance");
  const { player, status: playerStatus } = usePlayer(bridge, cadence.runRef);
  const [theme, setTheme] = useState<Palette>(() => {
    const saved = readSkyUltraUiPrefs().defaultTheme;
    return palettes.some((p) => p.value === saved) ? (saved as Palette) : "dragonstone";
  });
  const [themeOpen, setThemeOpen] = useState(false);
  const [previewEngine, setPreviewEngine] = useState("CEF");
  const [hostSettings, setHostSettings] = useState<SkyUltraSettings>(() => readCachedHostSettings());
  const [initialItemId, setInitialItemId] = useState("");
  const [initialCategory, setInitialCategory] = useState("all");
  const [inspectModRequest, setInspectModRequest] = useState<{ modId: string; nonce: number } | null>(null);
  const routeRef = useRef(route);
  const bridgeRef = useRef(bridge);
  bridgeRef.current = bridge;
  const sessionEpochRef = useRef(0);
  const [sessionReadyEpoch, setSessionReadyEpoch] = useState(0);
  const [closingStandalone, setClosingStandalone] = useState(false);
  const [tabTransitionSplash, setTabTransitionSplash] = useState<string | null>(null);
  const [previewSplashGate, setPreviewSplashGate] = useState(false);
  const [splashPreviewOpen, setSplashPreviewOpen] = useState(false);
  const tabTransitionTimerRef = useRef(0);
  const previewSplashGateRafRef = useRef(0);
  const splashPreviewWasOpenRef = useRef(false);
  const tabTransitionPrefRef = useRef(uiPrefs.tabTransition);
  const motionPrefRef = useRef(uiPrefs.motion);
  tabTransitionPrefRef.current = uiPrefs.tabTransition;
  motionPrefRef.current = uiPrefs.motion;
  const setRoute = useCallback((next: Route): void => {
    if (next === routeRef.current) return;
    const prevRoute = routeRef.current;
    const nextWillSplash = tabTransitionPrefRef.current === "cinematic" && motionPrefRef.current !== "reduced";
    leaveMenuPage();
    void bridgeRef.current?.request("game.closeItemPreview", {}).catch(() => undefined);
    if (nextWillSplash) {
      void bridgeRef.current?.request("game.closeActorPreview", { hold: true }).catch(() => undefined);
    } else if (prevRoute === "overview" || prevRoute === "actors" || prevRoute === "inventory") {
      void bridgeRef.current?.request("game.closeActorPreview", {}).catch(() => undefined);
    }
    preloadMenuPage(next);
    routeRef.current = next;
    setRouteState(next);
    window.clearTimeout(tabTransitionTimerRef.current);
    cancelAnimationFrame(previewSplashGateRafRef.current);
    if (nextWillSplash) {
      const targetTitle = navigation.find((item) => item.id === next)?.title ?? next;
      setPreviewSplashGate(true);
      setTabTransitionSplash(targetTitle);
      tabTransitionTimerRef.current = window.setTimeout(() => {
        setTabTransitionSplash(null);
      }, 165);
    } else {
      setTabTransitionSplash(null);
      setPreviewSplashGate(false);
    }
  }, []);
  useLayoutEffect(() => {
    if (tabTransitionSplash !== null || !previewSplashGate) return;
    void bridgeRef.current?.request("game.closeActorPreview", { releaseAfterPaint: true }).catch(() => undefined);
    let raf2 = 0;
    const raf1 = requestAnimationFrame(() => {
      raf2 = requestAnimationFrame(() => {
        previewSplashGateRafRef.current = 0;
        setPreviewSplashGate(false);
      });
      previewSplashGateRafRef.current = raf2;
    });
    previewSplashGateRafRef.current = raf1;
    return () => {
      cancelAnimationFrame(raf1);
      cancelAnimationFrame(raf2);
    };
  }, [tabTransitionSplash, previewSplashGate]);
  useLayoutEffect(() => {
    if (splashPreviewOpen) {
      splashPreviewWasOpenRef.current = true;
      void bridgeRef.current?.request("game.closeActorPreview", { hold: true }).catch(() => undefined);
      void bridgeRef.current?.request("game.closeItemPreview", {}).catch(() => undefined);
      return;
    }
    if (splashPreviewWasOpenRef.current) {
      splashPreviewWasOpenRef.current = false;
      void bridgeRef.current?.request("game.closeActorPreview", { releaseAfterPaint: true }).catch(() => undefined);
    }
  }, [splashPreviewOpen]);
  const [openingDataMs, setOpeningDataMs] = useState<number | null>(null);
  const playerAvailable = useRef(false);
  playerAvailable.current = Boolean(player?.available);
  const [firstFrameMs, setFirstFrameMs] = useState(0);
  const [navigationFrameMs, setNavigationFrameMs] = useState(0);
  const [dataFrameMs, setDataFrameMs] = useState(0);
  const [openingFrameMs, setOpeningFrameMs] = useState<number | null>(null);
  const [overlay, setOverlay] = useState<"help" | "commands" | null>(null);
  const [pinnedCount, setPinnedCount] = useState(0);
  const [hotkey, setHotkey] = useState<SkyUltraHotkeyBinding>(defaultSkyUltraHotkey);
  const [keyboardHotkey, setKeyboardHotkey] = useState<SkyUltraHotkeyBinding>(defaultSkyUltraKeyboardHotkey);
  const [glyphFamily, setGlyphFamily] = useState<SkyUltraControllerGlyphFamily>("keyboard");
  const [virtualCursorEnabled, setVirtualCursorEnabled] = useState(false);
  const virtualCursorEnabledRef = useRef(false);
  const virtualCursorStateRef = useRef<{ x: number; y: number; targetLabel: string }>({
    x: 640,
    y: 360,
    targetLabel: "READY",
  });
  const virtualCursorElRef = useRef<HTMLDivElement | null>(null);
  const virtualCursorBadgeRef = useRef<HTMLSpanElement | null>(null);
  const virtualCursorRafRef = useRef<number>(0);
  const virtualCursorLabelDirtyRef = useRef<boolean>(false);
  const scheduleVirtualCursorDomSync = useCallback((resolveLabel = true): void => {
    if (resolveLabel) {
      virtualCursorLabelDirtyRef.current = true;
    }
    if (virtualCursorRafRef.current !== 0) {
      return;
    }
    virtualCursorRafRef.current = requestAnimationFrame(() => {
      virtualCursorRafRef.current = 0;
      if (!virtualCursorEnabledRef.current) {
        return;
      }
      const cur = virtualCursorStateRef.current;
      if (virtualCursorLabelDirtyRef.current) {
        virtualCursorLabelDirtyRef.current = false;
        const hit = document.elementFromPoint(cur.x, cur.y) as HTMLElement | null;
        const interactive = hit?.closest("button,a,input,textarea,select,[role='button'],[role='tab']") as HTMLElement | null;
        cur.targetLabel =
          interactive?.getAttribute("aria-label") ||
          interactive?.textContent?.trim().slice(0, 22) ||
          hit?.tagName.toLowerCase() ||
          "SURFACE";
      }
      if (virtualCursorElRef.current) {
        virtualCursorElRef.current.style.transform = `translate3d(${cur.x}px, ${cur.y}px, 0)`;
      }
      if (virtualCursorBadgeRef.current) {
        virtualCursorBadgeRef.current.textContent = `${cur.targetLabel} · ${cur.x},${cur.y}`;
      }
    });
  }, []);
  const [virtualKeyboardOpen, setVirtualKeyboardOpen] = useState(false);
  const [virtualKeyboardStandalone, setVirtualKeyboardStandalone] = useState(false);
  const [vkShift, setVkShift] = useState(false);
  const [vkCaps, setVkCaps] = useState(false);
  const [vkCtrl, setVkCtrl] = useState(false);
  const [vkAlt, setVkAlt] = useState(false);
  const lastTextInputRef = useRef<HTMLInputElement | HTMLTextAreaElement | null>(null);
  const vkComposerRef = useRef<HTMLTextAreaElement | null>(null);
  const vkDragRef = useRef<{ id: number; dx: number; dy: number } | null>(null);
  const [vkPos, setVkPos] = useState<{ x: number; y: number } | null>(() => {
    try {
      const raw = window.localStorage.getItem("skyultra.virtualKeyboard.pos");
      const parsed = raw ? (JSON.parse(raw) as { x?: unknown; y?: unknown }) : null;
      return parsed && typeof parsed.x === "number" && typeof parsed.y === "number" ? { x: parsed.x, y: parsed.y } : null;
    } catch {
      return null;
    }
  });
  const clampVkPos = (x: number, y: number, el?: HTMLElement | null): { x: number; y: number } => {
    const w = el?.offsetWidth ?? 760, h = el?.offsetHeight ?? 300;
    return { x: Math.max(0, Math.min(window.innerWidth - Math.min(w, window.innerWidth), x)), y: Math.max(0, Math.min(window.innerHeight - Math.min(h, window.innerHeight), y)) };
  };
  const startVkDrag = (event: React.PointerEvent<HTMLElement>): void => {
    if (event.button !== 0 || (event.target as HTMLElement).closest("button")) return;
    const board = (event.currentTarget as HTMLElement).closest<HTMLElement>(".sky-virtual-keyboard");
    if (!board) return;
    const rect = board.getBoundingClientRect();
    vkDragRef.current = { id: event.pointerId, dx: event.clientX - rect.left, dy: event.clientY - rect.top };
    event.currentTarget.setPointerCapture(event.pointerId);
    event.preventDefault();
  };
  const moveVkDrag = (event: React.PointerEvent<HTMLElement>): void => {
    const drag = vkDragRef.current;
    if (!drag || drag.id !== event.pointerId) return;
    const board = (event.currentTarget as HTMLElement).closest<HTMLElement>(".sky-virtual-keyboard");
    setVkPos(clampVkPos(event.clientX - drag.dx, event.clientY - drag.dy, board));
  };
  const endVkDrag = (event: React.PointerEvent<HTMLElement>): void => {
    if (vkDragRef.current?.id !== event.pointerId) return;
    vkDragRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    setVkPos((pos) => {
      if (pos) {
        try { window.localStorage.setItem("skyultra.virtualKeyboard.pos", JSON.stringify(pos)); } catch { /* storage unavailable */ }
      }
      return pos;
    });
  };
  const keyboardOnly = virtualKeyboardOpen && virtualKeyboardStandalone;
  useEffect(() => {
    if (!keyboardOnly) return;
    const frame = requestAnimationFrame(() => vkComposerRef.current?.focus());
    // A keyboard-only session ends whenever the host hides the view (Escape, hotkey),
    // so the next normal menu open never comes back as a bare keyboard.
    const off = bridge.on?.<{ visible: boolean }>("skyultra.visibility", (ev) => {
      if (ev && ev.visible === false) {
        setVirtualKeyboardOpen(false);
        setVirtualKeyboardStandalone(false);
      }
    });
    return () => { cancelAnimationFrame(frame); off?.(); };
  }, [keyboardOnly, bridge]);
  const dataFrameRecorded = useRef(false);
  const pageScroll = useRef<HTMLElement>(null);
  const engineDetected = runtime.browserEngine === "cef" || runtime.browserEngine === "ultralight";
  const engine = previewEngine || (runtime.browserEngine === "ultralight" ? "Ultralight" : "CEF");
  const menuDisplayMode: SkyUltraDisplayMode = hostSettings.menuDisplayMode === "windowed" ? "windowed" : "fullscreen";
  const inventoryDisplayMode: SkyUltraDisplayMode = hostSettings.inventoryDisplayMode === "windowed" ? "windowed" : "fullscreen";
  const barterDisplayMode: SkyUltraBarterDisplayMode =
    hostSettings.barterDisplayMode === "full" || hostSettings.barterDisplayMode === "windowed"
      ? hostSettings.barterDisplayMode
      : "half";
  const menuPauseWorld = hostSettings.menuPauseWorld === true;
  const inventoryPauseWorld = hostSettings.inventoryPauseWorld !== false;
  const barterPauseWorld = hostSettings.barterPauseWorld !== false;
  const containerMode = (value: unknown, fallback: SkyUltraContainerDisplayMode = "half"): SkyUltraContainerDisplayMode =>
    value === "full" || value === "windowed" || value === "half" ? value : fallback;
  const magicDisplayMode = containerMode(hostSettings.magicDisplayMode, "half");
  const favoritesDisplayMode = containerMode(hostSettings.favoritesDisplayMode, "half");
  const containerDisplayMode = containerMode(hostSettings.containerDisplayMode, "half");
  const mannequinDisplayMode = containerMode(hostSettings.mannequinDisplayMode, "full");
  const lootDisplayMode = containerMode(hostSettings.lootDisplayMode, "half");
  const pickpocketDisplayMode = containerMode(hostSettings.pickpocketDisplayMode, "half");
  const blacksmithDisplayMode = containerMode(hostSettings.blacksmithDisplayMode, "half");
  const enchantingDisplayMode = containerMode(hostSettings.enchantingDisplayMode, "half");
  const alchemyDisplayMode = containerMode(hostSettings.alchemyDisplayMode, "half");
  const magicPauseWorld = hostSettings.magicPauseWorld !== false;
  const favoritesPauseWorld = hostSettings.favoritesPauseWorld !== false;
  const containerPauseWorld = hostSettings.containerPauseWorld !== false;
  const mannequinPauseWorld = hostSettings.mannequinPauseWorld !== false;
  const lootPauseWorld = hostSettings.lootPauseWorld !== false;
  const pickpocketPauseWorld = hostSettings.pickpocketPauseWorld !== false;
  const blacksmithPauseWorld = hostSettings.blacksmithPauseWorld !== false;
  const enchantingPauseWorld = hostSettings.enchantingPauseWorld !== false;
  const alchemyPauseWorld = hostSettings.alchemyPauseWorld !== false;
  const activeDisplayMode: SkyUltraDisplayMode = standaloneMagic
    ? magicDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneFavorites
    ? favoritesDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneContainer
    ? containerDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneMannequin
    ? mannequinDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneLoot
    ? lootDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standalonePickpocket
    ? pickpocketDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneBlacksmith
    ? blacksmithDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneEnchanting
    ? enchantingDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneAlchemy
    ? alchemyDisplayMode === "windowed" ? "windowed" : "fullscreen"
    : standaloneBook
    ? "fullscreen"
    : standaloneBarter
    ? barterDisplayMode === "windowed"
      ? "windowed"
      : "fullscreen"
    : standaloneInventory
      ? inventoryDisplayMode
      : menuDisplayMode;

  const hostSettingsRef = useRef(hostSettings);
  hostSettingsRef.current = hostSettings;
  const profileStoreRef = useRef(profileStore);
  profileStoreRef.current = profileStore;
  const playerNameRef = useRef(player?.playerName);
  playerNameRef.current = player?.playerName;
  const cadenceLockedRef = useRef(cadence.locked);
  cadenceLockedRef.current = cadence.locked;

  const updateHostSettingsPatch = useCallback((patch: Partial<SkyUltraSettings>): void => {
    const nextSettings: SkyUltraSettings = {
      ...hostSettingsRef.current,
      ...patch,
      schemaVersion: 2,
      profileState: profileStoreRef.current as unknown as Record<string, unknown>,
    };
    setHostSettings(nextSettings);
    writeCachedHostSettings(nextSettings);
    window.dispatchEvent(new CustomEvent("skyultra:settings-updated", { detail: nextSettings }));
    void updateSkyUltraSettings(nextSettings, bridge)
      .then((persisted) => {
        setHostSettings(persisted);
        writeCachedHostSettings(persisted);
        window.dispatchEvent(new CustomEvent("skyultra:settings-updated", { detail: persisted }));
      })
      .catch(() => undefined);
  }, [bridge]);

  const toggleMenuDisplayMode = useCallback((): void => {
    updateHostSettingsPatch({
      menuDisplayMode: hostSettingsRef.current.menuDisplayMode === "windowed" ? "fullscreen" : "windowed",
    });
  }, [updateHostSettingsPatch]);
  const toggleMenuPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ menuPauseWorld: !(hostSettingsRef.current.menuPauseWorld === true) });
  }, [updateHostSettingsPatch]);
  const toggleInventoryDisplayMode = useCallback((): void => {
    updateHostSettingsPatch({
      inventoryDisplayMode: hostSettingsRef.current.inventoryDisplayMode === "windowed" ? "fullscreen" : "windowed",
    });
  }, [updateHostSettingsPatch]);
  const toggleInventoryPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ inventoryPauseWorld: !(hostSettingsRef.current.inventoryPauseWorld !== false) });
  }, [updateHostSettingsPatch]);
  const changeMagicDisplayMode = useCallback((nextMode: SkyUltraContainerDisplayMode): void => {
    updateHostSettingsPatch({ magicDisplayMode: nextMode });
  }, [updateHostSettingsPatch]);
  const toggleMagicPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ magicPauseWorld: !(hostSettingsRef.current.magicPauseWorld !== false) });
  }, [updateHostSettingsPatch]);
  const changeFavoritesDisplayMode = useCallback((nextMode: SkyUltraContainerDisplayMode): void => {
    updateHostSettingsPatch({ favoritesDisplayMode: nextMode });
  }, [updateHostSettingsPatch]);
  const toggleFavoritesPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ favoritesPauseWorld: !(hostSettingsRef.current.favoritesPauseWorld !== false) });
  }, [updateHostSettingsPatch]);
  const changeBlacksmithDisplayMode = useCallback((nextMode: SkyUltraContainerDisplayMode): void => {
    updateHostSettingsPatch({ blacksmithDisplayMode: nextMode });
  }, [updateHostSettingsPatch]);
  const toggleBlacksmithPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ blacksmithPauseWorld: !(hostSettingsRef.current.blacksmithPauseWorld !== false) });
  }, [updateHostSettingsPatch]);
  const changeEnchantingDisplayMode = useCallback((nextMode: SkyUltraContainerDisplayMode): void => {
    updateHostSettingsPatch({ enchantingDisplayMode: nextMode });
  }, [updateHostSettingsPatch]);
  const toggleEnchantingPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ enchantingPauseWorld: !(hostSettingsRef.current.enchantingPauseWorld !== false) });
  }, [updateHostSettingsPatch]);
  const changeAlchemyDisplayMode = useCallback((nextMode: SkyUltraContainerDisplayMode): void => {
    updateHostSettingsPatch({ alchemyDisplayMode: nextMode });
  }, [updateHostSettingsPatch]);
  const toggleAlchemyPauseWorld = useCallback((): void => {
    updateHostSettingsPatch({ alchemyPauseWorld: !(hostSettingsRef.current.alchemyPauseWorld !== false) });
  }, [updateHostSettingsPatch]);

  useEffect(() => {
    let active = true;
    void loadSkyUltraProfileStore(bridge, player?.playerName).then((loadedStore) => {
      if (!active) return;
      setProfileStore(loadedStore);
      const effective = resolveEffectiveSkyUltraProfile(loadedStore, player?.playerName);
      setUiPrefs(effective.uiPrefs);
      if (palettes.some((p) => p.value === effective.theme)) {
        setTheme(effective.theme as Palette);
      }
    });
    const onProfileStore = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraProfileStoreV2>).detail;
      if (!detail) return;
      setProfileStore(detail);
      const effective = resolveEffectiveSkyUltraProfile(detail, player?.playerName);
      setUiPrefs(effective.uiPrefs);
      if (palettes.some((p) => p.value === effective.theme)) {
        setTheme(effective.theme as Palette);
      }
    };
    window.addEventListener("skyultra:profile-store", onProfileStore);
    return () => {
      active = false;
      window.removeEventListener("skyultra:profile-store", onProfileStore);
    };
  }, [bridge, player?.playerName]);

  useEffect(() => {
    if (!player?.playerName) return;
    const effective = resolveEffectiveSkyUltraProfile(profileStore, player.playerName);
    writeLocalSkyUltraProfileStore(profileStore, player.playerName);
    setUiPrefs(effective.uiPrefs);
    if (palettes.some((p) => p.value === effective.theme)) {
      setTheme(effective.theme as Palette);
    }
  }, [player?.playerName]);

  const selectAndPersistTheme = useCallback((nextTheme: Palette): void => {
    setTheme(nextTheme);
    const name = playerNameRef.current;
    const updatedStore = updateProfileForContext(profileStoreRef.current, name, {
      theme: nextTheme,
      uiPrefs: { defaultTheme: nextTheme },
    });
    setProfileStore(updatedStore);
    void persistSkyUltraProfileStore(updatedStore, bridge, name);
  }, [bridge]);

  const gameApi = useMemo(() => createSkyUltraGame(bridge), [bridge]);

  const playerEnchantingLevel = useMemo(() => {
    const fromSkills = (player?.skills ?? []).find(
      (s) => s.id.toLowerCase() === "enchanting" || s.label.toLowerCase() === "enchanting",
    )?.value;
    const skillNum = typeof fromSkills === "number" && Number.isFinite(fromSkills) ? Math.floor(fromSkills) : 0;
    const hostNum =
      typeof hostSettings.playerEnchantingLevel === "number" && Number.isFinite(hostSettings.playerEnchantingLevel)
        ? Math.floor(hostSettings.playerEnchantingLevel)
        : 0;
    return Math.max(skillNum, hostNum);
  }, [player?.skills, hostSettings.playerEnchantingLevel]);

  useEffect(() => {
    if (!standaloneBook) return;
    let cancelled = false;
    void gameApi
      .getActiveBookSnapshot()
      .then((snap) => {
        if (!cancelled) {
          setActiveBookSnapshot(snap);
        }
      })
      .catch((err) => {
        if (!cancelled) {
          setActiveBookSnapshot({ available: false });
          setActiveBookNotice(err instanceof Error ? err.message : "Failed to load book text.");
        }
      });
    return () => {
      cancelled = true;
    };
  }, [standaloneBook, bookLoadEpoch, gameApi]);

  const closeStandaloneSession = useCallback((): void => {
    // Keep a transparent curtain active while C++ hides the view so Main Menu never flashes underneath.
    setClosingStandalone(true);
    void closeSkyUltraPage(bridge).catch(() => {
      setClosingStandalone(false);
    });
  }, [bridge]);

  const handleTakeActiveBook = useCallback((): void => {
    if (busyTakingBook) return;
    setBusyTakingBook(true);
    setActiveBookNotice(null);
    void gameApi
      .takeActiveBook()
      .then((res) => {
        if (res.accepted || res.taken) {
          closeStandaloneSession();
          return;
        }
        if (res.book) {
          setActiveBookSnapshot(res.book);
        }
        setActiveBookNotice(res.message ?? "Could not take this book.");
      })
      .catch((err) => {
        setActiveBookNotice(err instanceof Error ? err.message : "Could not take this book.");
      })
      .finally(() => {
        setBusyTakingBook(false);
      });
  }, [busyTakingBook, closeStandaloneSession, gameApi]);
  handleTakeActiveBookRef.current = handleTakeActiveBook;

  const standaloneReadingBook = useMemo<ReadingBookState | null>(() => {
    if (!activeBookSnapshot?.available) return null;
    const pages = (
      activeBookSnapshot.pages && activeBookSnapshot.pages.length > 0
        ? activeBookSnapshot.pages
        : [activeBookSnapshot.text ?? ""]
    )
      .map((p) => p.trim())
      .filter(Boolean);
    const images = activeBookSnapshot.images ?? [];
    const safePages = pages.length > 0 ? pages : [activeBookSnapshot.text?.trim() || "No inscribed text."];
    const style =
      activeBookSnapshot.bookStyle ??
      (safePages.length > 1 ? "book" : "note");
    return {
      id: activeBookSnapshot.bookId || activeBookSnapshot.refId || "world-book",
      title: (activeBookSnapshot.title || "Book").replace(/^\$+/, "").trim() || "Book",
      text: activeBookSnapshot.text ?? safePages.join("\n\n"),
      pages: safePages,
      images,
      style,
    };
  }, [activeBookSnapshot]);

  useEffect(() => {
    const onUiPrefs = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraUiPreferences>).detail;
      if (detail) {
        setUiPrefs(detail);
      }
    };
    const onPreviewSplash = (): void => {
      setSplashPreviewOpen(true);
    };
    window.addEventListener("skyultra:ui-prefs", onUiPrefs);
    window.addEventListener("skyultra:preview-tab-splash", onPreviewSplash);
    return () => {
      window.removeEventListener("skyultra:ui-prefs", onUiPrefs);
      window.removeEventListener("skyultra:preview-tab-splash", onPreviewSplash);
    };
  }, []);

  useEffect(() => {
    let active = true;
    if (runtime.browserEngine === "cef") setPreviewEngine("CEF");
    else if (runtime.browserEngine === "ultralight") setPreviewEngine("Ultralight");
    void readSkyUltraSettings(bridge)
      .then((s) => {
        if (!active) return;
        setHostSettings(s);
        writeCachedHostSettings(s);
        if (s.hotkey) setHotkey(s.hotkey);
        if (s.keyboardHotkey) setKeyboardHotkey(s.keyboardHotkey);
        if (s.menuEngine === "cef") setPreviewEngine("CEF");
        else if (s.menuEngine === "ultralight") setPreviewEngine("Ultralight");
      })
      .catch(() => undefined);
    const onHotkeyUpdated = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraHotkeyBinding>).detail;
      if (detail) setHotkey(detail);
    };
    const onKeyboardHotkeyUpdated = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraHotkeyBinding>).detail;
      if (detail) setKeyboardHotkey(detail);
    };
    const onEngineUpdated = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraEngineChoice>).detail;
      if (detail === "cef") setPreviewEngine("CEF");
      else if (detail === "ultralight") setPreviewEngine("Ultralight");
    };
    const onSettingsUpdated = (event: Event): void => {
      const detail = (event as CustomEvent<Partial<SkyUltraSettings>>).detail;
      if (detail) {
        setHostSettings((prev) => {
          const next = { ...prev, ...detail };
          writeCachedHostSettings(next);
          return next;
        });
      }
    };
    window.addEventListener("skyultra:hotkey-updated", onHotkeyUpdated);
    window.addEventListener("skyultra:keyboard-hotkey-updated", onKeyboardHotkeyUpdated);
    window.addEventListener("skyultra:engine-updated", onEngineUpdated);
    window.addEventListener("skyultra:settings-updated", onSettingsUpdated);
    return () => {
      active = false;
      window.removeEventListener("skyultra:hotkey-updated", onHotkeyUpdated);
      window.removeEventListener("skyultra:keyboard-hotkey-updated", onKeyboardHotkeyUpdated);
      window.removeEventListener("skyultra:engine-updated", onEngineUpdated);
      window.removeEventListener("skyultra:settings-updated", onSettingsUpdated);
    };
  }, [bridge, runtime.browserEngine]);

  useEffect(() => {
    let cancelled = false;
    const syncCompassHudOverlay = (forceAllow = false): void => {
      if (!forceAllow && (!player || player.available === false)) {
        return;
      }
      const shouldEnableCompass = hostSettings.replaceSkyrimCompass !== false;
      void bridge
        .request<{
          available?: boolean;
          enabled?: boolean;
          widgets?: string[];
          anchor?: string;
          scale?: number;
          opacity?: number;
          theme?: string;
          glow?: number;
          colors?: Record<string, string>;
          placements?: Record<string, JsonValue>;
          customCode?: Record<string, JsonValue>;
        }>("ui.configureWidgets", { query: true })
        .then((current) => {
          if (cancelled || !current || current.available === false) return;
          const activeWidgets = current.enabled && Array.isArray(current.widgets) ? current.widgets : [];
          const hasCompass = activeWidgets.includes("compass");
          const savedHudLayout = resolveEffectiveSkyUltraProfile(
            readLocalSkyUltraProfileStore(),
            player?.playerName,
          ).hudLayout;
          const savedExcludesCompass = Boolean(
            savedHudLayout &&
              Array.isArray(savedHudLayout.widgets) &&
              !savedHudLayout.widgets.includes("compass"),
          );
          const savedPlacements = savedHudLayout?.placements as Record<string, JsonValue> | undefined;
          const savedCompassRaw = savedPlacements?.compass as Record<string, JsonValue> | undefined;
          const currentPlacementsObj = current.placements as Record<string, Record<string, JsonValue>> | undefined;
          const currentCompassRaw = currentPlacementsObj?.compass;
          const savedCompassRadius =
            savedCompassRaw && typeof savedCompassRaw.compassRadius === "number"
              ? Math.max(50, Math.min(1000, Math.round(savedCompassRaw.compassRadius)))
              : undefined;
          const currentHasCompassRadius =
            currentCompassRaw !== undefined && typeof currentCompassRaw.compassRadius === "number";
          const mergedPlacements =
            current.placements && Object.keys(current.placements).length > 0
              ? current.placements
              : savedPlacements;
          const mergedColors =
            current.colors && Object.keys(current.colors).length > 0
              ? current.colors
              : (savedHudLayout?.colors as Record<string, string> | undefined);
          if (shouldEnableCompass && (!current.enabled || !hasCompass)) {
            if (!forceAllow && savedExcludesCompass) {
              return undefined;
            }
            const nextWidgets = ["compass", ...activeWidgets.filter((w) => w !== "compass")];
            return bridge.request("ui.configureWidgets", {
              enabled: true,
              widgets: nextWidgets,
              anchor: current.anchor ?? savedHudLayout?.anchor ?? "top-center",
              scale: current.scale ?? savedHudLayout?.scale ?? 1,
              opacity: current.opacity ?? savedHudLayout?.opacity ?? 0.98,
              theme,
              glow: current.glow ?? savedHudLayout?.glow ?? uiPrefs.neonGlow ?? 100,
              ...(mergedColors ? { colors: mergedColors } : {}),
              ...(mergedPlacements ? { placements: mergedPlacements } : {}),
              ...(current.customCode ? { customCode: current.customCode } : {}),
            });
          }
          if (
            shouldEnableCompass &&
            current.enabled &&
            hasCompass &&
            savedCompassRadius !== undefined &&
            savedCompassRadius !== 50 &&
            !currentHasCompassRadius
          ) {
            const patchedPlacements = {
              ...(current.placements ?? {}),
              compass: {
                ...(savedCompassRaw ?? {}),
                ...(currentCompassRaw ?? {}),
                compassRadius: savedCompassRadius,
              },
            };
            return bridge.request("ui.configureWidgets", {
              enabled: true,
              widgets: activeWidgets,
              anchor: current.anchor ?? savedHudLayout?.anchor ?? "top-center",
              scale: current.scale ?? savedHudLayout?.scale ?? 1,
              opacity: current.opacity ?? savedHudLayout?.opacity ?? 0.98,
              theme: current.theme ?? theme,
              glow: current.glow ?? savedHudLayout?.glow ?? uiPrefs.neonGlow ?? 100,
              ...(mergedColors ? { colors: mergedColors } : {}),
              placements: patchedPlacements,
              ...(current.customCode ? { customCode: current.customCode } : {}),
            });
          }
          if (!shouldEnableCompass && current.enabled && hasCompass) {
            const remaining = activeWidgets.filter((w) => w !== "compass");
            return bridge.request("ui.configureWidgets", {
              enabled: remaining.length > 0,
              widgets: remaining,
              anchor: current.anchor ?? "top-right",
              scale: current.scale ?? 1,
              opacity: current.opacity ?? 0.96,
              theme: current.theme ?? theme,
              glow: current.glow ?? uiPrefs.neonGlow ?? 100,
              ...(mergedColors ? { colors: mergedColors } : {}),
              ...(mergedPlacements ? { placements: mergedPlacements } : {}),
              ...(current.customCode ? { customCode: current.customCode } : {}),
            });
          }
          return undefined;
        })
        .catch(() => undefined);
    };
    syncCompassHudOverlay(false);
    const offSync = bridge.on("skyultra.syncHudOverlay", () => {
      syncCompassHudOverlay(true);
    });
    return () => {
      cancelled = true;
      offSync();
    };
  }, [bridge, hostSettings.replaceSkyrimCompass, player?.available, player?.playerName, theme, uiPrefs.neonGlow]);

  useLayoutEffect(() => {
    const frame = requestAnimationFrame(() => {
      setFirstFrameMs(performance.now() - appModuleStartedAt);
      setNavigationFrameMs(performance.now());
    });
    return () => cancelAnimationFrame(frame);
  }, []);

  useEffect(() => {
    let frame = 0;
    const capture = (): void => {
      cancelAnimationFrame(frame);
      const state = bridge.getVisibility?.();
      if (state && !state.visible) return;
      const opened = state?.visibleSinceMs ?? appModuleStartedAt;
      frame = requestAnimationFrame(() => setOpeningFrameMs(Math.max(0, performance.now() - opened)));
    };
    const off = bridge.on<{ visible: boolean }>("skyultra.visibility", (event) => {
      if (event.visible) capture();
    });
    capture();
    return () => {
      off();
      cancelAnimationFrame(frame);
    };
  }, [bridge]);

  useEffect(() => {
    let frame = 0;
    const capture = (): void => {
      cancelAnimationFrame(frame);
      const state = bridge.getVisibility?.();
      if (state?.visible === false || document.hidden || !playerAvailable.current) return;
      const opened = state?.visibleSinceMs ?? appModuleStartedAt;
      frame = requestAnimationFrame(() => setOpeningDataMs(Math.max(0, performance.now() - opened)));
    };
    const off = bridge.on<{ visible: boolean }>("skyultra.visibility", event => {
      if (event.visible) { setOpeningDataMs(null); capture(); }
      else cancelAnimationFrame(frame);
    });
    capture();
    return () => { off(); cancelAnimationFrame(frame); };
  }, [bridge, player?.available]);

  useEffect(() => connectSkyUltraInputScopes(bridge), [bridge]);
  useEffect(() => {
    const offSounds = connectSkyUltraDeclarativeSounds({ bridge });
    const shims = installSkyUltraCompetitorShims(window, bridge);
    const offInputToken = bridge.on<{ device?: string }>("skyultra.inputToken", (ev) => {
      if (ev?.device === "vr") setGlyphFamily("vr");
      else if (ev?.device === "gamepad") setGlyphFamily((prev) => (prev === "playstation" ? "playstation" : "xbox"));
    });
    const offVirtualKeyboard = bridge.on<{ open?: boolean; standalone?: boolean }>(
      "skyultra.virtualKeyboard",
      (ev) => {
        const nextOpen = Boolean(ev?.open);
        setVirtualKeyboardOpen(nextOpen);
        setVirtualKeyboardStandalone(Boolean(ev?.standalone && nextOpen));
      },
    );
    const offMenuSession = bridge.on<{
      epoch?: number;
      visible?: boolean;
      virtualKeyboard?: boolean;
      standalone?: boolean;
      inventoryMode?: boolean;
      barterMode?: boolean;
      magicMode?: boolean;
      favoritesMode?: boolean;
      containerMode?: boolean;
      mannequinMode?: boolean;
      lootMode?: boolean;
      pickpocketMode?: boolean;
      blacksmithMode?: boolean;
      enchantingMode?: boolean;
      alchemyMode?: boolean;
      bookMode?: boolean;
      menuDisplayMode?: string;
      inventoryDisplayMode?: string;
      barterDisplayMode?: string;
      magicDisplayMode?: string;
      favoritesDisplayMode?: string;
      containerDisplayMode?: string;
      mannequinDisplayMode?: string;
      lootDisplayMode?: string;
      pickpocketDisplayMode?: string;
      blacksmithDisplayMode?: string;
      enchantingDisplayMode?: string;
      alchemyDisplayMode?: string;
      menuPauseWorld?: boolean;
      inventoryPauseWorld?: boolean;
      barterPauseWorld?: boolean;
      magicPauseWorld?: boolean;
      favoritesPauseWorld?: boolean;
      containerPauseWorld?: boolean;
      mannequinPauseWorld?: boolean;
      lootPauseWorld?: boolean;
      pickpocketPauseWorld?: boolean;
      blacksmithPauseWorld?: boolean;
      enchantingPauseWorld?: boolean;
      alchemyPauseWorld?: boolean;
      bookPauseWorld?: boolean;
    }>("skyultra.menuSession", (ev) => {
      if (typeof ev?.epoch === "number" && Number.isFinite(ev.epoch)) {
        const nextEpoch = Math.trunc(ev.epoch);
        sessionEpochRef.current = nextEpoch;
        setSessionReadyEpoch(nextEpoch);
      }
      setClosingStandalone(false);
      window.clearTimeout(tabTransitionTimerRef.current);
      cancelAnimationFrame(previewSplashGateRafRef.current);
      setTabTransitionSplash(null);
      setPreviewSplashGate(false);
      if (ev?.visible === false) {
        leaveMenuPage();
        setSessionDormant(true);
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setVirtualKeyboardOpen(false);
        setVirtualKeyboardStandalone(false);
        setOverlay(null);
        setThemeOpen(false);
        return;
      }
      setSessionDormant(false);
      const modePatch: Partial<SkyUltraSettings> = {};
      if (ev?.menuDisplayMode === "windowed" || ev?.menuDisplayMode === "fullscreen") modePatch.menuDisplayMode = ev.menuDisplayMode;
      if (ev?.inventoryDisplayMode === "windowed" || ev?.inventoryDisplayMode === "fullscreen") modePatch.inventoryDisplayMode = ev.inventoryDisplayMode;
      if (ev?.barterDisplayMode === "half" || ev?.barterDisplayMode === "full" || ev?.barterDisplayMode === "windowed") modePatch.barterDisplayMode = ev.barterDisplayMode;
      if (ev?.magicDisplayMode === "half" || ev?.magicDisplayMode === "full" || ev?.magicDisplayMode === "windowed") modePatch.magicDisplayMode = ev.magicDisplayMode;
      if (ev?.favoritesDisplayMode === "half" || ev?.favoritesDisplayMode === "full" || ev?.favoritesDisplayMode === "windowed") modePatch.favoritesDisplayMode = ev.favoritesDisplayMode;
      if (ev?.containerDisplayMode === "half" || ev?.containerDisplayMode === "full" || ev?.containerDisplayMode === "windowed") modePatch.containerDisplayMode = ev.containerDisplayMode;
      if (ev?.mannequinDisplayMode === "half" || ev?.mannequinDisplayMode === "full" || ev?.mannequinDisplayMode === "windowed") modePatch.mannequinDisplayMode = ev.mannequinDisplayMode;
      if (ev?.lootDisplayMode === "half" || ev?.lootDisplayMode === "full" || ev?.lootDisplayMode === "windowed") modePatch.lootDisplayMode = ev.lootDisplayMode;
      if (ev?.pickpocketDisplayMode === "half" || ev?.pickpocketDisplayMode === "full" || ev?.pickpocketDisplayMode === "windowed") modePatch.pickpocketDisplayMode = ev.pickpocketDisplayMode;
      if (ev?.blacksmithDisplayMode === "half" || ev?.blacksmithDisplayMode === "full" || ev?.blacksmithDisplayMode === "windowed") modePatch.blacksmithDisplayMode = ev.blacksmithDisplayMode;
      if (ev?.enchantingDisplayMode === "half" || ev?.enchantingDisplayMode === "full" || ev?.enchantingDisplayMode === "windowed") modePatch.enchantingDisplayMode = ev.enchantingDisplayMode;
      if (ev?.alchemyDisplayMode === "half" || ev?.alchemyDisplayMode === "full" || ev?.alchemyDisplayMode === "windowed") modePatch.alchemyDisplayMode = ev.alchemyDisplayMode;
      if (typeof ev?.menuPauseWorld === "boolean") modePatch.menuPauseWorld = ev.menuPauseWorld;
      if (typeof ev?.inventoryPauseWorld === "boolean") modePatch.inventoryPauseWorld = ev.inventoryPauseWorld;
      if (typeof ev?.barterPauseWorld === "boolean") modePatch.barterPauseWorld = ev.barterPauseWorld;
      if (typeof ev?.magicPauseWorld === "boolean") modePatch.magicPauseWorld = ev.magicPauseWorld;
      if (typeof ev?.favoritesPauseWorld === "boolean") modePatch.favoritesPauseWorld = ev.favoritesPauseWorld;
      if (typeof ev?.containerPauseWorld === "boolean") modePatch.containerPauseWorld = ev.containerPauseWorld;
      if (typeof ev?.mannequinPauseWorld === "boolean") modePatch.mannequinPauseWorld = ev.mannequinPauseWorld;
      if (typeof ev?.lootPauseWorld === "boolean") modePatch.lootPauseWorld = ev.lootPauseWorld;
      if (typeof ev?.pickpocketPauseWorld === "boolean") modePatch.pickpocketPauseWorld = ev.pickpocketPauseWorld;
      if (typeof ev?.blacksmithPauseWorld === "boolean") modePatch.blacksmithPauseWorld = ev.blacksmithPauseWorld;
      if (typeof ev?.enchantingPauseWorld === "boolean") modePatch.enchantingPauseWorld = ev.enchantingPauseWorld;
      if (typeof ev?.alchemyPauseWorld === "boolean") modePatch.alchemyPauseWorld = ev.alchemyPauseWorld;
      if (typeof ev?.bookPauseWorld === "boolean") modePatch.bookPauseWorld = ev.bookPauseWorld;
      if (Object.keys(modePatch).length > 0) {
        setHostSettings((prev) => {
          const next = { ...prev, ...modePatch };
          writeCachedHostSettings(next);
          return next;
        });
      }
      const nextInventory = Boolean(ev?.inventoryMode);
      const nextBarter = Boolean(ev?.barterMode);
      const nextMagic = Boolean(ev?.magicMode);
      const nextFavorites = Boolean(ev?.favoritesMode);
      const nextContainer = Boolean(ev?.containerMode);
      const nextMannequin = Boolean(ev?.mannequinMode);
      const nextLoot = Boolean(ev?.lootMode);
      const nextPickpocket = Boolean(ev?.pickpocketMode);
      const nextBlacksmith = Boolean(ev?.blacksmithMode);
      const nextEnchanting = Boolean(ev?.enchantingMode);
      const nextAlchemy = Boolean(ev?.alchemyMode);
      const nextBook = Boolean(ev?.bookMode);
      setStandaloneInventory(nextInventory);
      setStandaloneBarter(nextBarter);
      setStandaloneMagic(nextMagic);
      setStandaloneFavorites(nextFavorites);
      setStandaloneContainer(nextContainer);
      setStandaloneMannequin(nextMannequin);
      setStandaloneLoot(nextLoot);
      setStandalonePickpocket(nextPickpocket);
      setStandaloneBlacksmith(nextBlacksmith);
      setStandaloneEnchanting(nextEnchanting);
      setStandaloneAlchemy(nextAlchemy);
      setStandaloneBook(nextBook);
      if (nextBook) {
        setBookLoadEpoch((n) => n + 1);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
      } else {
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
      }
      setVirtualKeyboardOpen(Boolean(ev?.virtualKeyboard));
      setVirtualKeyboardStandalone(Boolean(ev?.virtualKeyboard && ev?.standalone));
      if (
        nextInventory ||
        nextBarter ||
        nextMagic ||
        nextFavorites ||
        nextContainer ||
        nextMannequin ||
        nextLoot ||
        nextPickpocket ||
        nextBlacksmith ||
        nextEnchanting ||
        nextAlchemy ||
        nextBook
      ) {
        setOverlay(null);
        setThemeOpen(false);
      }
      if (nextInventory) {
        preloadMenuPage("inventory");
        routeRef.current = "inventory";
        setRouteState("inventory");
      }
    });
    const offInventoryMode = bridge.on<{ active?: boolean }>(
      "skyultra.inventoryMode",
      (ev) => {
        const nextActive = Boolean(ev?.active);
        setClosingStandalone(false);
        setStandaloneInventory(nextActive);
        if (nextActive) {
          setStandaloneBarter(false);
          setStandaloneMagic(false);
          setStandaloneFavorites(false);
          setStandaloneContainer(false);
          setStandaloneMannequin(false);
          setStandaloneLoot(false);
          setStandalonePickpocket(false);
          setStandaloneBlacksmith(false);
          setStandaloneEnchanting(false);
          setStandaloneAlchemy(false);
          setStandaloneBook(false);
          setActiveBookSnapshot(null);
          setActiveBookNotice(null);
          setBusyTakingBook(false);
          setOverlay(null);
          setThemeOpen(false);
          setRoute("inventory");
        }
      },
    );
    const offBarterMode = bridge.on<{ active?: boolean }>(
      "skyultra.barterMode",
      (ev) => {
        const nextActive = Boolean(ev?.active);
        setClosingStandalone(false);
        setStandaloneBarter(nextActive);
        if (nextActive) {
          setStandaloneInventory(false);
          setStandaloneMagic(false);
          setStandaloneFavorites(false);
          setStandaloneContainer(false);
          setStandaloneMannequin(false);
          setStandaloneLoot(false);
          setStandalonePickpocket(false);
          setStandaloneBlacksmith(false);
          setStandaloneEnchanting(false);
          setStandaloneAlchemy(false);
          setStandaloneBook(false);
          setActiveBookSnapshot(null);
          setActiveBookNotice(null);
          setBusyTakingBook(false);
          setOverlay(null);
          setThemeOpen(false);
        }
      },
    );
    const offMagicMode = bridge.on<{ active?: boolean }>("skyultra.magicMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneMagic(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offFavoritesMode = bridge.on<{ active?: boolean }>("skyultra.favoritesMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneFavorites(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offContainerMode = bridge.on<{ active?: boolean }>("skyultra.containerMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneContainer(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offMannequinMode = bridge.on<{ active?: boolean }>("skyultra.mannequinMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneMannequin(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offLootMode = bridge.on<{ active?: boolean }>("skyultra.lootMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneLoot(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offPickpocketMode = bridge.on<{ active?: boolean }>("skyultra.pickpocketMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandalonePickpocket(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offBlacksmithMode = bridge.on<{ active?: boolean }>("skyultra.blacksmithMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneBlacksmith(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offEnchantingMode = bridge.on<{ active?: boolean }>("skyultra.enchantingMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneEnchanting(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneAlchemy(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offAlchemyMode = bridge.on<{ active?: boolean }>("skyultra.alchemyMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneAlchemy(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneBook(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setOverlay(null);
        setThemeOpen(false);
      }
    });
    const offBookMode = bridge.on<{ active?: boolean }>("skyultra.bookMode", (ev) => {
      const nextActive = Boolean(ev?.active);
      setClosingStandalone(false);
      setStandaloneBook(nextActive);
      if (nextActive) {
        setStandaloneInventory(false);
        setStandaloneBarter(false);
        setStandaloneMagic(false);
        setStandaloneFavorites(false);
        setStandaloneContainer(false);
        setStandaloneMannequin(false);
        setStandaloneLoot(false);
        setStandalonePickpocket(false);
        setStandaloneBlacksmith(false);
        setStandaloneEnchanting(false);
        setStandaloneAlchemy(false);
        setOverlay(null);
        setThemeOpen(false);
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
        setBookLoadEpoch((n) => n + 1);
      } else {
        setActiveBookSnapshot(null);
        setActiveBookNotice(null);
        setBusyTakingBook(false);
      }
    });
    const offBookActivate = bridge.on("skyultra.bookActivate", () => {
      if (standaloneBookRef.current && activeBookSnapshotRef.current?.canTake && !activeBookSnapshotRef.current?.taken) {
        handleTakeActiveBookRef.current();
      }
    });
    const offGamepadCursor = bridge.on<{ dx?: number; dy?: number; x?: number; y?: number }>(
      "skyultra.gamepadCursor",
      (ev) => {
        if (!virtualCursorEnabledRef.current) return;
        const cur = virtualCursorStateRef.current;
        const nextX =
          typeof ev?.x === "number"
            ? Math.max(8, Math.min(window.innerWidth - 8, Math.round(ev.x)))
            : Math.max(8, Math.min(window.innerWidth - 8, Math.round(cur.x + (ev?.dx ?? 0) * 14)));
        const nextY =
          typeof ev?.y === "number"
            ? Math.max(8, Math.min(window.innerHeight - 8, Math.round(ev.y)))
            : Math.max(8, Math.min(window.innerHeight - 8, Math.round(cur.y + (ev?.dy ?? 0) * 14)));
        cur.x = nextX;
        cur.y = nextY;
        scheduleVirtualCursorDomSync(true);
      },
    );
    const onFocusIn = (ev: FocusEvent): void => {
      const target = ev.target as HTMLElement | null;
      if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
        lastTextInputRef.current = target;
      }
    };
    document.addEventListener("focusin", onFocusIn);
    return () => {
      offSounds();
      offInputToken();
      offVirtualKeyboard();
      offMenuSession();
      offInventoryMode();
      offBarterMode();
      offMagicMode();
      offFavoritesMode();
      offContainerMode();
      offMannequinMode();
      offLootMode();
      offPickpocketMode();
      offBlacksmithMode();
      offEnchantingMode();
      offAlchemyMode();
      offBookMode();
      offBookActivate();
      offGamepadCursor();
      shims.uninstall();
      document.removeEventListener("focusin", onFocusIn);
    };
  }, [bridge, scheduleVirtualCursorDomSync]);

  useEffect(() => {
    virtualCursorEnabledRef.current = virtualCursorEnabled;
    if (!virtualCursorEnabled) {
      if (virtualCursorRafRef.current !== 0) {
        cancelAnimationFrame(virtualCursorRafRef.current);
        virtualCursorRafRef.current = 0;
      }
      return;
    }
    const triggerClickAt = (x: number, y: number): void => {
      const el = document.elementFromPoint(x, y) as HTMLElement | null;
      const target = (el?.closest("button,a,input,textarea,select,[role='button'],[role='tab']") as HTMLElement | null) ?? el;
      if (target) {
        target.focus();
        target.click();
      }
    };
    const onKey = (ev: KeyboardEvent): void => {
      const cur = virtualCursorStateRef.current;
      const step = ev.shiftKey ? 36 : 18;
      if (ev.key === "ArrowLeft") {
        ev.preventDefault();
        cur.x = Math.max(8, cur.x - step);
        scheduleVirtualCursorDomSync(true);
      } else if (ev.key === "ArrowRight") {
        ev.preventDefault();
        cur.x = Math.min(window.innerWidth - 8, cur.x + step);
        scheduleVirtualCursorDomSync(true);
      } else if (ev.key === "ArrowUp") {
        ev.preventDefault();
        cur.y = Math.max(8, cur.y - step);
        scheduleVirtualCursorDomSync(true);
      } else if (ev.key === "ArrowDown") {
        ev.preventDefault();
        cur.y = Math.min(window.innerHeight - 8, cur.y + step);
        scheduleVirtualCursorDomSync(true);
      } else if (ev.key === "Enter") {
        const activeTag = document.activeElement?.tagName.toLowerCase();
        if (activeTag !== "textarea") {
          ev.preventDefault();
          triggerClickAt(cur.x, cur.y);
        }
      }
    };
    const onPointerMove = (ev: PointerEvent): void => {
      const cur = virtualCursorStateRef.current;
      cur.x = Math.max(8, Math.min(window.innerWidth - 8, Math.round(ev.clientX)));
      cur.y = Math.max(8, Math.min(window.innerHeight - 8, Math.round(ev.clientY)));
      scheduleVirtualCursorDomSync(true);
    };
    let rafId = 0;
    let prevAButton = false;
    const pollGamepad = (): void => {
      if (typeof navigator !== "undefined" && typeof navigator.getGamepads === "function") {
        const pads = navigator.getGamepads();
        const pad = pads?.[0] || pads?.[1] || pads?.[2] || pads?.[3];
        if (pad) {
          const lx = pad.axes[0] ?? 0;
          const ly = pad.axes[1] ?? 0;
          const rx = pad.axes[2] ?? 0;
          const ry = pad.axes[3] ?? 0;
          const dpadX = (pad.buttons[15]?.pressed ? 1 : 0) - (pad.buttons[14]?.pressed ? 1 : 0);
          const dpadY = (pad.buttons[13]?.pressed ? 1 : 0) - (pad.buttons[12]?.pressed ? 1 : 0);
          const dx = Math.abs(lx) > 0.16 ? lx * 11 : Math.abs(rx) > 0.16 ? rx * 14 : dpadX * 9;
          const dy = Math.abs(ly) > 0.16 ? ly * 11 : Math.abs(ry) > 0.16 ? ry * 14 : dpadY * 9;
          const cur = virtualCursorStateRef.current;
          if (dx !== 0 || dy !== 0) {
            cur.x = Math.max(8, Math.min(window.innerWidth - 8, Math.round(cur.x + dx)));
            cur.y = Math.max(8, Math.min(window.innerHeight - 8, Math.round(cur.y + dy)));
            scheduleVirtualCursorDomSync(true);
          }
          const aPressed = Boolean(pad.buttons[0]?.pressed);
          if (aPressed && !prevAButton) {
            triggerClickAt(cur.x, cur.y);
          }
          prevAButton = aPressed;
        }
      }
      rafId = requestAnimationFrame(pollGamepad);
    };
    rafId = requestAnimationFrame(pollGamepad);
    window.addEventListener("keydown", onKey);
    window.addEventListener("pointermove", onPointerMove, { passive: true });
    return () => {
      cancelAnimationFrame(rafId);
      if (virtualCursorRafRef.current !== 0) {
        cancelAnimationFrame(virtualCursorRafRef.current);
        virtualCursorRafRef.current = 0;
      }
      window.removeEventListener("keydown", onKey);
      window.removeEventListener("pointermove", onPointerMove);
    };
  }, [virtualCursorEnabled, scheduleVirtualCursorDomSync]);
  useEffect(() => {
    let cancelled = false;
    // Warm up replacement menus and primary command modules staggered during idle frames
    // so navigating between tabs avoids cold JS module stalls while staying non-blocking.
    const warmPages = isStandaloneOverlay || keyboardOnly ? [] : [
      () => loadMenuPage("inventory"),
      () => import("./BarterPage.js"),
      () => import("./ContainerPage.js"),
      () => import("./MannequinPage.js"),
      () => import("./MagicPage.js"),
      () => import("./FavoritesPage.js"),
      () => import("./BlacksmithPage.js"),
      () => import("./EnchantingPage.js"),
      () => import("./AlchemyPage.js"),
      () => import("./BookReader.js"),
      () => loadMenuPage("actors"),
      () => loadMenuPage("character"),
      () => loadMenuPage("settings"),
      () => loadMenuPage("widgets"),
      () => loadMenuPage("performance"),
      () => loadMenuPage("devtools"),
      () => loadMenuPage("guide"),
    ];
    let warmIndex = 0;
    let warmTimer = 0;
    let warmIdle = 0;
    const scheduleWarmup = (): void => {
      if (cancelled || warmIndex >= warmPages.length) return;
      if (typeof window.requestIdleCallback === "function") {
        warmIdle = window.requestIdleCallback(deadline => {
          warmIdle = 0;
          if (deadline.timeRemaining() >= 8) warmNext();
          else warmTimer = window.setTimeout(scheduleWarmup, 200);
        });
      } else warmNext();
    };
    const warmNext = (): void => {
      if (cancelled || warmIndex >= warmPages.length) return;
      void warmPages[warmIndex++]().catch(() => undefined).finally(() => {
        if (!cancelled) warmTimer = window.setTimeout(scheduleWarmup, 200);
      });
    };
    warmTimer = window.setTimeout(scheduleWarmup, 500);
    return () => {
      cancelled = true;
      window.clearTimeout(warmTimer);
      if (warmIdle) window.cancelIdleCallback(warmIdle);
    };
  }, [bridge]);

  useEffect(() => {
    const offVis = bridge.on<{ visible: boolean }>("skyultra.visibility", (ev) => {
      if (ev?.visible) {
        setClosingStandalone(false);
        setSessionDormant(false);
      } else if (ev?.visible === false) {
        leaveMenuPage();
        setClosingStandalone(false);
        window.clearTimeout(tabTransitionTimerRef.current);
        cancelAnimationFrame(previewSplashGateRafRef.current);
        setTabTransitionSplash(null);
        setPreviewSplashGate(false);
        setSessionDormant(true);
      }
    });
    return () => {
      offVis();
    };
  }, [bridge]);

  useEffect(() => {
    if (!themeOpen) return;
    const release = enterSkyInputScope();
    const escape = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        event.stopPropagation();
        setThemeOpen(false);
      }
    };
    window.addEventListener("keydown", escape);
    return () => {
      release();
      window.removeEventListener("keydown", escape);
    };
  }, [themeOpen]);

  useEffect(() => {
    if (!player || dataFrameRecorded.current) return;
    const frame = requestAnimationFrame(() => {
      dataFrameRecorded.current = true;
      setDataFrameMs(performance.now());
    });
    return () => cancelAnimationFrame(frame);
  }, [player]);

  useEffect(() => {
    pageScroll.current?.scrollTo({ top: 0 });
  }, [route]);

  useEffect(() => {
    const keyboard = (event: KeyboardEvent): void => {
      if (!standaloneBook && isMenuSearchKey(event) && !document.querySelector(".sky-hotkey-record.is-active")) {
        // Capture before a focused button can interpret Space as activation.
        event.preventDefault();
        event.stopPropagation();
        if (event.repeat || getSkyInputScopeDepth() > 0 || cadence.locked || overlay || themeOpen) return;
        if (!focusMenuSearch(pageScroll.current ?? document) && !isStandaloneOverlay) setOverlay("commands");
        return;
      }
      if (event.shiftKey && !event.ctrlKey && !event.altKey && !event.metaKey && event.key.toLowerCase() === "k") {
        const activeEl = event.target as HTMLElement | null;
        if (!activeEl?.closest("input,textarea,[contenteditable=true]")) {
          event.preventDefault();
          setVirtualKeyboardOpen((open) => {
            const next = !open;
            if (!next && virtualKeyboardStandalone) {
              setVirtualKeyboardStandalone(false);
              void closeSkyUltraPage(bridge).catch(() => undefined);
            }
            return next;
          });
          return;
        }
      }
      if ((event.ctrlKey || event.metaKey) && (event.key === "F3" || event.code === "F3")) {
        event.preventDefault();
        if (!cadence.locked) setOverlay("help");
        return;
      }
      if (overlay || themeOpen || document.querySelector('[role="dialog"]')) return;
      const target = event.target as HTMLElement | null;
      if (target?.closest("input,textarea,select,[contenteditable=true]") || event.altKey || event.ctrlKey || event.metaKey)
        return;
      if (standaloneBook && !standaloneReadingBook && event.key === "Escape") {
        event.preventDefault();
        closeStandaloneSession();
        return;
      }
      if (isStandaloneOverlay && event.key === "Tab" && !(standaloneBook && activeBookSnapshot?.activateKeyCode === "Tab")) {
        event.preventDefault();
        closeStandaloneSession();
        return;
      }
      const index = Number(event.key) - 1;
      if (!isStandaloneOverlay && index >= 0 && index < navigation.length && !cadence.locked) {
        event.preventDefault();
        setRoute(navigation[index].id);
      }
    };
    window.addEventListener("keydown", keyboard, true);
    return () => window.removeEventListener("keydown", keyboard, true);
  }, [bridge, cadence.locked, closeStandaloneSession, isStandaloneOverlay, overlay, standaloneBook, standaloneReadingBook, activeBookSnapshot?.activateKeyCode, themeOpen, virtualKeyboardStandalone]);
  const categories = useMemo(() => [...new Set((player?.inventory ?? []).map((item) => item.category))].sort(), [player?.inventory]);
  const equippedSlots = useMemo(
    () =>
      equipmentSlots.map((slot) => ({
        ...slot,
        item: player?.equipment[slot.id] ?? null,
      })),
    [player?.equipment],
  );
  const equippedCount = useMemo(() => equippedSlots.filter((slot) => !!slot.item).length, [equippedSlots]);
  const equippedWeight = useMemo(
    () => Math.round(equippedSlots.reduce((sum, slot) => sum + (slot.item?.weight ?? 0), 0) * 10) / 10,
    [equippedSlots],
  );
  const equippedValue = useMemo(
    () => equippedSlots.reduce((sum, slot) => sum + (slot.item?.value ?? 0), 0),
    [equippedSlots],
  );
  const featuredCarried = useMemo(
    () =>
      [...(player?.inventory ?? [])]
        .sort((a, b) => Number(b.equipped) - Number(a.equipped) || b.value - a.value)
        .slice(0, 4),
    [player?.inventory],
  );
  const carryCurrent = Math.round(player?.carryWeight?.current ?? 0);
  const carryMax = Math.max(1, Math.round(player?.carryWeight?.max ?? 300));
  const carryPct = Math.max(0, Math.min(100, Math.round((carryCurrent / carryMax) * 100)));
  const headingDeg = player?.heading !== undefined ? ((Math.round(player.heading) % 360) + 360) % 360 : null;
  const compassDirs = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
  const compassLabel = headingDeg !== null ? `${compassDirs[Math.round(headingDeg / 45) % 8]} · ${headingDeg}°` : "N · 0°";
  const calendarLabel = player?.calendar
    ? `${player.calendar.month} ${player.calendar.day}, 4E ${player.calendar.year}`
    : "4E 201 · Tamriel";
  const activeEffectTotal = player?.activeEffectCount ?? player?.activeEffects?.length ?? 0;
  const hotkeyLabel = formatSkyUltraHotkey(hotkey, "keyboard");
  const navigate = useCallback((next: Route): void => {
    if (!cadenceLockedRef.current) {
      setRoute(next);
      setThemeOpen(false);
      setOverlay(null);
    }
  }, [setRoute]);
  const inspectInventory = useCallback((id = "", category = "all"): void => {
    setInitialItemId(id);
    setInitialCategory(category);
    navigate("inventory");
  }, [navigate]);
  const inspectModInDevStudio = useCallback((modId: string): void => {
    setInspectModRequest({ modId, nonce: Date.now() });
    navigate("devtools");
  }, [navigate]);
  const currentPalette = palettes.find((p) => p.value === theme) ?? palettes[0];
  const cursorAccent = uiPrefs.cursorAccent === "theme" ? currentPalette.color : uiPrefs.cursorAccent;
  const activeRouteIndex = Math.max(0, navigation.findIndex((item) => item.id === route));
  const activeRouteItem = navigation[activeRouteIndex] ?? navigation[0];
  const glowFactor = Math.max(0, Math.min(1.5, (uiPrefs.neonGlow ?? 100) / 100));
  const scaleFactor = Math.max(0.85, Math.min(1.25, uiPrefs.uiScale ?? 1));
  const atlasStyle = {
    "--sky-user-glow": glowFactor.toFixed(2),
    "--sky-ui-scale": scaleFactor.toFixed(2),
    ...(scaleFactor !== 1 ? { zoom: scaleFactor } : {}),
  } as React.CSSProperties;

  const pageObscured =
    sessionDormant ||
    closingStandalone ||
    themeOpen ||
    overlay !== null ||
    splashPreviewOpen ||
    Boolean(tabTransitionSplash) ||
    previewSplashGate;

  return (
    <SkyCursorScope
      bridge={bridge}
      disabled={uiPrefs.cursorMode === "native"}
      accent={cursorAccent}
    >
      <SkyThemeScope
        theme={theme}
        style={
          sessionDormant && !closingStandalone
            ? { ...atlasStyle, visibility: "hidden", pointerEvents: "none" }
            : atlasStyle
        }
        data-session-epoch={sessionReadyEpoch}
        data-theme-family={currentPalette.family}
        data-ui-density={uiPrefs.density}
        data-ui-motion={uiPrefs.motion}
        data-glass-intensity={uiPrefs.glassIntensity}
        data-ui-grid={uiPrefs.backdropGrid ? "on" : "off"}
        data-ui-scanlines={uiPrefs.scanlines ? "on" : "off"}
        data-display-mode={activeDisplayMode}
        data-barter-mode={
          standaloneBarter
            ? barterDisplayMode
            : standaloneContainer
              ? containerDisplayMode
              : standaloneLoot
                ? lootDisplayMode
                : standalonePickpocket
                  ? pickpocketDisplayMode
                  : standaloneBlacksmith
                    ? blacksmithDisplayMode
                    : standaloneEnchanting
                      ? enchantingDisplayMode
                      : standaloneAlchemy
                        ? alchemyDisplayMode
                        : standaloneMannequin
                          ? mannequinDisplayMode
                          : undefined
        }
        className={
          "atlas" +
          (uiPrefs.motion === "reduced" ? " atlas--still" : "") +
          (isStandaloneOverlay ? " atlas--standalone-inventory" : "") +
          (standaloneBarter ? ` atlas--standalone-barter atlas--barter-${barterDisplayMode}` : "") +
          (standaloneContainer ? ` atlas--standalone-barter atlas--standalone-container atlas--barter-${containerDisplayMode}` : "") +
          (standaloneLoot ? ` atlas--standalone-barter atlas--standalone-container atlas--standalone-loot atlas--barter-${lootDisplayMode}` : "") +
          (standalonePickpocket ? ` atlas--standalone-barter atlas--standalone-container atlas--standalone-pickpocket atlas--barter-${pickpocketDisplayMode}` : "") +
          (standaloneBlacksmith ? ` atlas--standalone-blacksmith atlas--blacksmith-${blacksmithDisplayMode}` : "") +
          (standaloneEnchanting ? ` atlas--standalone-enchanting atlas--enchanting-${enchantingDisplayMode}` : "") +
          (standaloneAlchemy ? ` atlas--standalone-alchemy atlas--alchemy-${alchemyDisplayMode}` : "") +
          (standaloneBook ? " atlas--standalone-book" : "") +
          (standaloneMannequin ? ` atlas--standalone-mannequin atlas--mannequin-${mannequinDisplayMode}` : "") +
          (standaloneMagic ? ` atlas--standalone-magic atlas--magic-${magicDisplayMode}` : "") +
          (standaloneFavorites ? ` atlas--standalone-favorites atlas--favorites-${favoritesDisplayMode}` : "") +
          (activeDisplayMode === "windowed" ? " atlas--windowed" : "") +
          (keyboardOnly ? " atlas--keyboard-only" : "") +
          (sessionDormant || closingStandalone ? " atlas--dormant" : "")
        }
      >
        {closingStandalone ? (
          <div className="sky-standalone-closing-curtain" aria-hidden="true" />
        ) : !keyboardOnly && (<>
        {!isStandaloneOverlay && (
          <SkyNavigationRail
            className="app-rail"
            caption="COMMAND MODULES"
            label="Primary navigation"
            items={navigation.map((item, index) => ({
              ...item,
              icon: <Icon name={item.icon} size={18} />,
              shortcut: String(index + 1).padStart(2, "0"),
            }))}
            value={route}
            onValueChange={navigate}
            onIntent={preloadMenuPage}
            disabled={cadence.locked}
            brand={
              <button className="brand" onClick={() => navigate("overview")} aria-label="SkyUltra overview">
                <SkyBrand emblem="./SkyUltraLogoMark.png" title="SKYULTRA" subtitle="ADVANCED WEBVIEW INTERFACE" />
              </button>
            }
            footer={
              <div className="rail-bottom">
                <div className="rail-engine">
                  <span className="rail-engine__beacon">
                    <i />
                  </span>
                  <span className="rail-engine__copy">
                    <small>{runtime.host === "skyrim" ? "SKYRIM LIVE LINK" : "STUDIO PREVIEW"}</small>
                    <strong>{engine}</strong>
                  </span>
                </div>
                <div className="rail-shortcuts">
                  <span>
                    <kbd>1–9</kbd> Module
                  </span>
                  <span>
                    <kbd>Space</kbd> Search
                  </span>
                  <span>
                    <kbd>Enter</kbd> Activate
                  </span>
                </div>
              </div>
            }
          />
        )}

        <div className="app-workspace">
          {!isStandaloneOverlay && (
            <SkyMenuHeader
              title={activeRouteItem.title}
              eyebrow={`MODULE 0${activeRouteIndex + 1} // ${activeRouteItem.note.toUpperCase()}`}
              onHelp={() => {
                if (!cadence.locked) setOverlay("help");
              }}
              helpTitle="Contextual Help & SkyUltra Guide (Ctrl + F3)"
              onClose={() => void closeSkyUltraPage(bridge).catch(() => undefined)}
            >
              <div className="topbar-telemetry-capsule">
                <span className="header-session-mark">
                  <i />
                  {runtime.host === "skyrim" ? "LIVE" : "SIM"}
                </span>
                <span className="topbar-character">
                  <strong>{player?.playerName ?? "Connecting…"}</strong>
                  {player && <em className="topbar-lvl">LVL {player.level}</em>}
                  <i />
                  <span>{player?.location ?? runtime.host.toUpperCase()}</span>
                  {player && (
                    <>
                      <i />
                      <span className="topbar-gold">
                        <Icon name="gold" size={12} /> {player.gold.toLocaleString()}
                      </span>
                    </>
                  )}
                </span>
              </div>

              <div
                className="theme-picker"
                onBlur={(event) => {
                  if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setThemeOpen(false);
                }}
              >
                <button
                  className="theme-trigger"
                  disabled={cadence.locked}
                  onClick={() => setThemeOpen((value) => !value)}
                  aria-expanded={themeOpen}
                  aria-haspopup="true"
                >
                  <span className="theme-trigger__orbs">
                    <i style={{ background: currentPalette.color }} />
                    <i style={{ background: currentPalette.secondary }} />
                  </span>
                  <span className="theme-trigger__copy">
                    <strong>{currentPalette.label}</strong>
                    <small>{currentPalette.family === "darkmode" ? "EYE-COMFORT" : currentPalette.family.toUpperCase()}</small>
                  </span>
                  <Icon name="chevron" size={11} />
                </button>
                {themeOpen && (
                  <div className="theme-menu" role="group" aria-label="Interface palette">
                    {(
                      [
                        { id: "darkmode", title: "EYE-COMFORT COLLECTION · 5 LOW-GLARE MATTE VELVET-SLATE THEMES" },
                        { id: "neon", title: "NEON & CYBER MATRIX (CHAMFERED + SCANLINE GRID)" },
                        { id: "celestial", title: "CELESTIAL & ARCANE GLASS (STARLIGHT + RUNIC)" },
                        { id: "classic", title: "CLASSIC TAMRIELIC INLAY (ENGRAVED SERIF)" },
                      ] as const
                    ).map((group) => (
                      <div key={group.id} className="theme-menu__section">
                        <span className="theme-menu__heading">{group.title}</span>
                        <div className="theme-menu__grid">
                          {palettes
                            .filter((p) => p.family === group.id)
                            .map((palette) => (
                              <button
                                key={palette.value}
                                type="button"
                                className={"theme-menu__card" + (theme === palette.value ? " is-active" : "")}
                                aria-pressed={theme === palette.value}
                                onClick={() => {
                                  selectAndPersistTheme(palette.value);
                                  setThemeOpen(false);
                                }}
                              >
                                <span className="theme-menu__swatches">
                                  <i style={{ background: palette.color }} />
                                  <i style={{ background: palette.secondary }} />
                                </span>
                                <span className="theme-menu__meta">
                                  <strong>{palette.label}</strong>
                                  <small>{palette.styleTag}</small>
                                </span>
                                {theme === palette.value && <Icon name="check" size={13} />}
                              </button>
                            ))}
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>

              <div className="topbar-mode-pills" role="group" aria-label="Menu display and world pause">
                <button
                  type="button"
                  className={"topbar-mode-pill" + (menuDisplayMode === "windowed" ? " is-active" : "")}
                  aria-pressed={menuDisplayMode === "windowed"}
                  title={`Toggle SkyUltra Menu between Full Screen and Windowed mode (Current: ${menuDisplayMode === "windowed" ? "Windowed" : "Full Screen"})`}
                  onClick={() =>
                    updateHostSettingsPatch({
                      menuDisplayMode: menuDisplayMode === "windowed" ? "fullscreen" : "windowed",
                    })
                  }
                >
                  <Icon name="widgets" size={13} />
                  <span>{menuDisplayMode === "windowed" ? "Windowed" : "Full Screen"}</span>
                </button>
                <button
                  type="button"
                  className={"topbar-mode-pill" + (menuPauseWorld ? " is-active" : "")}
                  aria-pressed={menuPauseWorld}
                  title={`Toggle pausing the Skyrim world while SkyUltra Menu is open (Current: ${menuPauseWorld ? "World Paused" : "Live World"})`}
                  onClick={() => updateHostSettingsPatch({ menuPauseWorld: !menuPauseWorld })}
                >
                  <Icon name="clock" size={13} />
                  <span>{menuPauseWorld ? "Pause World" : "Live World"}</span>
                </button>
              </div>

              <button
                className="header-command"
                title="Find a section (Space)"
                onClick={() => setOverlay("commands")}
                disabled={cadence.locked}
              >
                <Icon name="search" size={15} />
                <span>Command</span>
                <kbd>Space</kbd>
              </button>

              <button className="topbar-benchmark" onClick={() => navigate("performance")} title="Open Performance Lab">
                <Icon name="performance" size={14} />
                <strong>
                  {Number.isFinite(cadence.stats.fps) && cadence.stats.fps > 0 ? cadence.stats.fps.toFixed(0) : "—"}
                </strong>
                <small>FPS</small>
              </button>
            </SkyMenuHeader>
          )}
      <main ref={pageScroll} className={"page-scroll" + (isStandaloneOverlay || route === "inventory" || route === "actors" ? " page-scroll--inventory" : "")}>
        {!isStandaloneOverlay && route === "overview" && <div className="page overview-page">
          <SkyWelcomeBanner
            title={player?.playerName ?? "Welcome to Skyrim"}
            eyebrow="YOUR SKYULTRA COMMAND DOSSIER"
            description="Your character, live armory, and surrounding world synchronized through a native GPU interface."
            emblem="./SkyUltraLogo.png"
            emblemViewBox="850 70 700 640"
            brandName="SkyUltra"
            brandCaption="Advanced Webview Interface"
            facts={[
              { id: "level", label: "Character level", value: player?.level ?? "—" },
              { id: "location", label: "Current hold", value: player?.location ?? "Connecting…" },
              { id: "armor", label: "Armor rating", value: player ? Math.round(player.armorRating).toLocaleString() : "—" },
              { id: "gold", label: "Septim treasury", value: player ? player.gold.toLocaleString() : "—" },
            ]}
            actions={
              <>
                <button onClick={() => inspectInventory()}>
                  <Icon name="inventory" size={16} />
                  Open Armory &amp; Inventory
                  <Icon name="arrow" size={14} />
                </button>
                <button onClick={() => navigate("actors")}>
                  <Icon name="actors" size={16} />
                  Actor Observatory
                </button>
                <button onClick={() => navigate("widgets")}>
                  <Icon name="widgets" size={16} />
                  Shape your HUD
                </button>
                <button onClick={() => navigate("guide")}>
                  <Icon name="book" size={16} />
                  SkyUltra Guide
                </button>
              </>
            }
            status={
              <>
                <i className={player ? "status-dot" : "status-dot status-dot--waiting"} />
                {player ? `CONNECTED · ${calendarLabel.toUpperCase()} · BEARING ${compassLabel}` : "CONNECTING TO YOUR WORLD"}
              </>
            }
          />

          <div className="overview-primary">
            <section className="surface vitals-surface">
              <SectionTitle eyebrow="CHARACTER CONDITION" title="The vital signs">
                <span className="quiet-pill">{player ? "LEVEL " + player.level : "AWAITING DATA"}</span>
              </SectionTitle>
              <div className="vital-rings">
                <SkyRingMeter label="Health" tone="danger" value={player?.health.current ?? 0} max={player?.health.max ?? 0} />
                <SkyRingMeter label="Magicka" value={player?.magicka.current ?? 0} max={player?.magicka.max ?? 0} />
                <SkyRingMeter label="Stamina" tone="success" value={player?.stamina.current ?? 0} max={player?.stamina.max ?? 0} />
              </div>
              <div className="vitals-telemetry-strip" aria-label="Combat condition summary">
                <div className="vitals-telemetry-pill">
                  <small>ARMOR RATING</small>
                  <strong>{player ? Math.round(player.armorRating).toLocaleString() : "—"}</strong>
                </div>
                <div className="vitals-telemetry-pill">
                  <small>EQUIPPED SLOTS</small>
                  <strong>{equippedCount} / {equipmentSlots.length}</strong>
                </div>
                <div className="vitals-telemetry-pill">
                  <small>ACTIVE EFFECTS</small>
                  <strong>{activeEffectTotal}</strong>
                </div>
                <div className="vitals-telemetry-pill">
                  <small>ENCUMBRANCE</small>
                  <strong>{carryPct}%</strong>
                </div>
              </div>
              <div className="vitals-footer">
                <span>
                  <i className={player ? "status-dot" : "status-dot status-dot--waiting"} />
                  {player ? "Shared game-data feed synchronized" : playerStatus}
                </span>
                <span>
                  {player
                    ? `SEQ #${player.sequence ?? 1} · ${player.questCount ?? player.quests?.length ?? 0} QUESTS · ${player.followerCount ?? player.followers?.length ?? 0} FOLLOWERS`
                    : "INCREMENTAL TOPIC FEED"}
                </span>
              </div>
            </section>

            <section className="surface session-surface" aria-label="Active session command deck">
              <SectionTitle eyebrow="ACTIVE SESSION" title="At a glance">
                <span className="session-engine-pill">
                  <i className="status-dot" />
                  {engine.toUpperCase()} COMPOSITOR
                </span>
              </SectionTitle>

              <div className="session-identity-card">
                <PlayerPortrait player={player} bridge={bridge} size="md" className="session-identity-portrait" />
                <div className="session-identity-copy">
                  <span className="eyebrow">{calendarLabel}</span>
                  <strong>{player?.playerName ?? "Awaiting character"}</strong>
                  <small>
                    {player?.location ?? "Skyrim"} · Bearing {compassLabel} · Hotkey {hotkeyLabel}
                  </small>
                </div>
                <div className="session-identity-level">
                  <small>LEVEL</small>
                  <b>{player?.level ?? "—"}</b>
                </div>
              </div>

              <div className="session-telemetry-grid" role="list" aria-label="Character statistics">
                <div className="session-stat-card" role="listitem">
                  <div className="session-stat-card__head">
                    <span className="session-stat-card__icon"><Icon name="shield" size={16} /></span>
                    <small>ARMOR &amp; WARD</small>
                  </div>
                  <strong>{player ? Math.round(player.armorRating).toLocaleString() : "—"}</strong>
                  <span>{equippedWeight} wt worn across {equippedCount} slots</span>
                </div>

                <div className="session-stat-card" role="listitem">
                  <div className="session-stat-card__head">
                    <span className="session-stat-card__icon"><Icon name="gold" size={16} /></span>
                    <small>SEPTIM TREASURY</small>
                  </div>
                  <strong>{player?.gold.toLocaleString() ?? "—"}</strong>
                  <span>{equippedValue.toLocaleString()} gold in readied gear</span>
                </div>

                <div className="session-stat-card" role="listitem">
                  <div className="session-stat-card__head">
                    <span className="session-stat-card__icon"><Icon name="inventory" size={16} /></span>
                    <small>CARRY CAPACITY</small>
                  </div>
                  <strong>{player?.carryWeight ? `${carryCurrent} / ${carryMax}` : "—"}</strong>
                  <div className="session-stat-bar" aria-hidden="true">
                    <i style={{ width: `${carryPct}%` }} />
                  </div>
                </div>

                <div className="session-stat-card" role="listitem">
                  <div className="session-stat-card__head">
                    <span className="session-stat-card__icon"><Icon name="rune" size={16} /></span>
                    <small>PACK &amp; EFFECTS</small>
                  </div>
                  <strong>{player?.inventoryItemCount.toLocaleString() ?? "—"} <small>ITEMS</small></strong>
                  <span>{player?.inventoryTypeCount.toLocaleString() ?? "0"} distinct · {activeEffectTotal} active effects</span>
                </div>
              </div>

              <div className="session-foot">
                <span>{runtime.host === "skyrim" ? "Live Skyrim bridge telemetry" : "Preview fixture telemetry"}</span>
                <div className="session-foot__actions">
                  <button type="button" className="text-action" onClick={() => navigate("character")}>
                    Character sheet <Icon name="arrow" size={12} />
                  </button>
                  <button type="button" className="text-action" onClick={() => navigate("settings")}>
                    Hotkeys &amp; settings <Icon name="arrow" size={12} />
                  </button>
                </div>
              </div>
            </section>
          </div>

          <div className="overview-secondary">
            <section className="surface overview-loadout-surface" aria-label="Equipped items showcase">
              <SectionTitle eyebrow="CURRENT LOADOUT" title="Equipped for the road">
                <div className="overview-loadout-actions">
                  <span className="quiet-pill">{equippedCount} / {equipmentSlots.length} SLOTS WORN</span>
                  <button className="text-action" onClick={() => inspectInventory()}>
                    Open Inventory <Icon name="arrow" size={14} />
                  </button>
                </div>
              </SectionTitle>

              {player ? (
                <>
                  <div className="overview-armory-grid" role="list" aria-label="Equipped items">
                    {equippedSlots.map((slot) => {
                      const item = slot.item;
                      return (
                        <button
                          type="button"
                          role="listitem"
                          key={slot.id}
                          className={"overview-armory-card" + (item ? " is-equipped" : " is-empty")}
                          onClick={() => inspectInventory(item?.id ?? "", item?.category ?? "all")}
                          title={
                            item
                              ? `${slot.label}: ${item.name} (${item.value.toLocaleString()} gold · ${item.weight} wt) — Click to open in Inventory`
                              : `${slot.label}${slot.biped ? " · slot " + slot.biped : ""}: Empty slot — Click to equip from Inventory`
                          }
                        >
                          <span className="overview-armory-card__bezel" aria-hidden="true">
                            <span className="overview-armory-card__art">
                              {item ? (
                                <ItemThumbnail item={item} bridge={bridge} paused={pageObscured} />
                              ) : (
                                <EquipmentGlyph slot={slot.id} />
                              )}
                            </span>
                          </span>
                          <span className="overview-armory-card__body">
                            <span className="overview-armory-card__top">
                              <small>
                                {slot.label}
                                {slot.biped && <em>{slot.biped}</em>}
                              </small>
                              <b className={item ? "armory-state armory-state--worn" : "armory-state"}>
                                {item ? "WORN" : "OPEN"}
                              </b>
                            </span>
                            <strong>{item?.name ?? "Unassigned slot"}</strong>
                            <span className="overview-armory-card__meta">
                              {item ? (
                                <>
                                  <span>{item.category}</span>
                                  <span>
                                    <Icon name="gold" size={11} />
                                    {item.value.toLocaleString()}
                                  </span>
                                  <span>{item.weight} wt</span>
                                </>
                              ) : (
                                <span>Click to assign gear in Inventory</span>
                              )}
                            </span>
                          </span>
                        </button>
                      );
                    })}
                  </div>
                  <div className="overview-loadout-footer">
                    <span>
                      Total worn value: <b>{equippedValue.toLocaleString()} Septims</b> · Worn weight: <b>{equippedWeight} wt</b>
                    </span>
                    <span>Click any slot to inspect or swap in Inventory</span>
                  </div>
                </>
              ) : (
                <Empty title="Reading equipment" detail={playerStatus} icon="shield" />
              )}
            </section>

            <section className="surface collection-summary" aria-label="Carried inventory summary">
              <SectionTitle eyebrow="CARRIED ITEMS" title="A place for everything">
                <button className="text-action" onClick={() => inspectInventory()}>
                  Browse all <Icon name="arrow" size={14} />
                </button>
              </SectionTitle>

              <div className="collection-hero">
                <div className="collection-total">
                  <strong>{player?.inventoryTypeCount.toLocaleString() ?? "—"}</strong>
                  <span>
                    distinct item types
                    <small>
                      {player
                        ? `${player.inventoryItemCount.toLocaleString()} total items · ${carryCurrent} / ${carryMax} wt carried`
                        : "Reading your pack…"}
                    </small>
                  </span>
                </div>
                <div className="collection-capacity-badge">
                  <small>LOAD</small>
                  <b>{carryPct}%</b>
                </div>
              </div>

              <div className="category-summary">
                {categories.slice(0, 6).map((name) => {
                  const matching = player?.inventory.filter((item) => item.category === name) ?? [];
                  const totalWt = Math.round(matching.reduce((acc, item) => acc + item.weight * Math.max(1, item.count), 0));
                  return (
                    <button key={name} onClick={() => inspectInventory("", name)}>
                      <Icon name={itemIcon(name)} size={15} />
                      <span>
                        {name}
                        <small>{totalWt} wt</small>
                      </span>
                      <strong>{matching.length}</strong>
                      <Icon name="chevron" size={10} />
                    </button>
                  );
                })}
              </div>

              {featuredCarried.length > 0 && (
                <div className="overview-pack-highlights">
                  <span className="eyebrow">NOTABLE BELONGINGS</span>
                  <div className="overview-pack-highlights__grid">
                    {featuredCarried.map((item) => (
                      <button
                        type="button"
                        key={item.id}
                        className={"overview-pack-item" + (item.equipped ? " is-equipped" : "")}
                        onClick={() => inspectInventory(item.id, item.category)}
                      >
                        <span className="overview-pack-item__art" aria-hidden="true">
                          <ItemThumbnail item={item} bridge={bridge} paused={pageObscured} />
                        </span>
                        <span className="overview-pack-item__copy">
                          <strong>{item.name}</strong>
                          <small>
                            {item.category} · <Icon name="gold" size={10} /> {item.value.toLocaleString()} · ×{item.count}
                          </small>
                        </span>
                      </button>
                    ))}
                  </div>
                </div>
              )}

              <button className="wide-action" onClick={() => navigate("actors")}>
                <Icon name="actors" size={20} />
                <span>
                  Meet the people around you
                  <small>Portrait gallery, 3D Character Studio, and actor dress-up</small>
                </span>
                <Icon name="arrow" size={16} />
              </button>
            </section>
          </div>
        </div>}
        <Suspense
          fallback={
            <TabSplash
              tabOverlay
              title={isStandaloneOverlay ? "Opening Skyrim Interface" : activeRouteItem.title}
              subtitle={isStandaloneOverlay ? "Synchronizing live container & inventory state" : `Initializing ${activeRouteItem.note.toLowerCase()}`}
              stageLabel="LOADING MODULE"
              theme={theme}
              mode={uiPrefs.tabTransition}
              reducedMotion={uiPrefs.motion === "reduced"}
            />
          }
        >
          {!sessionDormant && <SessionReadyBeacon epoch={sessionReadyEpoch} bridge={bridge} />}
          {standaloneMagic ? (
            <MagicPage
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={magicDisplayMode}
              pauseWorld={magicPauseWorld}
              onChangeDisplayMode={changeMagicDisplayMode}
              onTogglePauseWorld={toggleMagicPauseWorld}
              onClose={closeStandaloneSession}
            />
          ) : standaloneFavorites ? (
            <FavoritesPage
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={favoritesDisplayMode}
              pauseWorld={favoritesPauseWorld}
              onChangeDisplayMode={changeFavoritesDisplayMode}
              onTogglePauseWorld={toggleFavoritesPauseWorld}
              onClose={closeStandaloneSession}
            />
          ) : standaloneContainer ? (
            <ContainerPage
              key="container"
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              variant="container"
              displayMode={containerDisplayMode}
              pauseWorld={containerPauseWorld}
              onChangeDisplayMode={(nextMode) => updateHostSettingsPatch({ containerDisplayMode: nextMode })}
              onTogglePauseWorld={() => updateHostSettingsPatch({ containerPauseWorld: !containerPauseWorld })}
              onClose={closeStandaloneSession}
            />
          ) : standaloneLoot ? (
            <ContainerPage
              key="loot"
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              variant="loot"
              displayMode={lootDisplayMode}
              pauseWorld={lootPauseWorld}
              onChangeDisplayMode={(nextMode) => updateHostSettingsPatch({ lootDisplayMode: nextMode })}
              onTogglePauseWorld={() => updateHostSettingsPatch({ lootPauseWorld: !lootPauseWorld })}
              onClose={closeStandaloneSession}
            />
          ) : standaloneBlacksmith ? (
            <BlacksmithPage
              key="blacksmith"
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={blacksmithDisplayMode}
              pauseWorld={blacksmithPauseWorld}
              onChangeDisplayMode={changeBlacksmithDisplayMode}
              onTogglePauseWorld={toggleBlacksmithPauseWorld}
              onClose={closeStandaloneSession}
            />
          ) : standaloneEnchanting ? (
            <EnchantingPage
              key="enchanting"
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={enchantingDisplayMode}
              pauseWorld={enchantingPauseWorld}
              onChangeDisplayMode={changeEnchantingDisplayMode}
              onTogglePauseWorld={toggleEnchantingPauseWorld}
              onClose={closeStandaloneSession}
            />
          ) : standaloneAlchemy ? (
            <AlchemyPage
              key="alchemy"
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={alchemyDisplayMode}
              pauseWorld={alchemyPauseWorld}
              onChangeDisplayMode={changeAlchemyDisplayMode}
              onTogglePauseWorld={toggleAlchemyPauseWorld}
              onClose={closeStandaloneSession}
            />
          ) : standaloneBook ? (
            <div className="book-standalone-host">
              {standaloneReadingBook ? (
                <StandaloneBookReader
                  book={standaloneReadingBook}
                  standalone
                  playerName={player?.playerName}
                  theme={theme}
                  design={uiPrefs.bookReaderDesign ?? "auto"}
                  themeSync={uiPrefs.bookThemeSync !== false}
                  canTake={Boolean(activeBookSnapshot?.canTake)}
                  isStealing={Boolean(activeBookSnapshot?.isStealing)}
                  taken={Boolean(activeBookSnapshot?.taken)}
                  busyTaking={busyTakingBook}
                  onTakeBook={handleTakeActiveBook}
                  bookValue={activeBookSnapshot?.value ?? 0}
                  bookWeight={activeBookSnapshot?.weight ?? 0}
                  teachesSkill={Boolean(activeBookSnapshot?.teachesSkill)}
                  skillName={activeBookSnapshot?.skillName ?? ""}
                  teachesSpell={Boolean(activeBookSnapshot?.teachesSpell)}
                  spellName={activeBookSnapshot?.spellName ?? ""}
                  notice={activeBookNotice}
                  activateKeyLabel={activeBookSnapshot?.activateKeyLabel || "E"}
                  activateKeyCode={activeBookSnapshot?.activateKeyCode ?? ""}
                  activateKey={activeBookSnapshot?.activateKey ?? ""}
                  onClose={closeStandaloneSession}
                />
              ) : activeBookSnapshot && !activeBookSnapshot.available ? (
                <div className="inventory-book-modal">
                  <div className="inventory-book-reader surface" role="dialog" aria-modal="true" aria-label="Book Reader">
                    <header className="inventory-book-reader__header">
                      <div className="inventory-book-reader__title-group">
                        <span className="eyebrow">SKYULTRA CHRONICLE · READER UNAVAILABLE</span>
                        <h2>Book text unavailable</h2>
                        <div className="book-reader-world-banner">
                          {activeBookNotice || "Unable to read the active world book snapshot."}
                        </div>
                      </div>
                      <div className="inventory-book-reader__actions">
                        <button type="button" className="book-action-btn is-close" onClick={closeStandaloneSession}>
                          <span>Close</span>
                          <kbd>ESC</kbd>
                        </button>
                      </div>
                    </header>
                  </div>
                </div>
              ) : null}
            </div>
          ) : standalonePickpocket ? (
            <ContainerPage
              key="pickpocket"
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              variant="pickpocket"
              displayMode={pickpocketDisplayMode}
              pauseWorld={pickpocketPauseWorld}
              onChangeDisplayMode={(nextMode) => updateHostSettingsPatch({ pickpocketDisplayMode: nextMode })}
              onTogglePauseWorld={() => updateHostSettingsPatch({ pickpocketPauseWorld: !pickpocketPauseWorld })}
              onClose={closeStandaloneSession}
            />
          ) : standaloneMannequin ? (
            <MannequinPage
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={mannequinDisplayMode}
              pauseWorld={mannequinPauseWorld}
              onChangeDisplayMode={(nextMode) => updateHostSettingsPatch({ mannequinDisplayMode: nextMode })}
              onTogglePauseWorld={() => updateHostSettingsPatch({ mannequinPauseWorld: !mannequinPauseWorld })}
              onClose={closeStandaloneSession}
            />
          ) : standaloneBarter ? (
            <BarterPage
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              displayMode={barterDisplayMode}
              pauseWorld={barterPauseWorld}
              onChangeDisplayMode={(nextMode) => updateHostSettingsPatch({ barterDisplayMode: nextMode })}
              onTogglePauseWorld={() => updateHostSettingsPatch({ barterPauseWorld: !barterPauseWorld })}
              onClose={closeStandaloneSession}
            />
          ) : standaloneInventory ? (
            <InventoryPage
              player={player}
              bridge={bridge}
              obscured={pageObscured}
              standalone
              displayMode={inventoryDisplayMode}
              pauseWorld={inventoryPauseWorld}
              onToggleDisplayMode={toggleInventoryDisplayMode}
              onTogglePauseWorld={toggleInventoryPauseWorld}
              onClose={closeStandaloneSession}
            />
          ) : (
            <>
              {route === "actors" && <ActorsPage player={player} bridge={bridge} obscured={pageObscured} />}
              {route === "inventory" && (
                <InventoryPage
                  player={player}
                  bridge={bridge}
                  obscured={pageObscured}
                  initialCategory={initialCategory}
                  initialId={initialItemId}
                  displayMode={menuDisplayMode}
                  pauseWorld={menuPauseWorld}
                  onToggleDisplayMode={toggleMenuDisplayMode}
                  onTogglePauseWorld={toggleMenuPauseWorld}
                />
              )}
              {route === "character" && <CharacterPage player={player} bridge={bridge} />}
              {route === "settings" && (
                <SettingsPage
                  bridge={bridge}
                  native={runtime.host === "skyrim"}
                  theme={theme}
                  onThemeChange={selectAndPersistTheme}
                  playerName={player?.playerName}
                  playerEnchantingLevel={playerEnchantingLevel}
                />
              )}
              {route === "performance" && <PerformancePage showWorkload={false} bridge={bridge} native={runtime.host === "skyrim"} cadence={cadence} engine={engine} theme={theme} openingFrameMs={openingFrameMs} openingDataMs={openingDataMs} firstFrameMs={firstFrameMs} navigationFrameMs={navigationFrameMs} dataFrameMs={dataFrameMs} detected={engineDetected} setPreviewEngine={setPreviewEngine} />}
              {route === "widgets" && <WidgetsPage showPatterns={false} player={player} bridge={bridge} native={runtime.host === "skyrim"} theme={theme} onNavigate={navigate} onInspectMod={inspectModInDevStudio} onPinnedChange={setPinnedCount} />}
              {route === "devtools" && <DevStudioPage bridge={bridge} native={runtime.host === "skyrim"} inspectRequest={inspectModRequest} />}
              {route === "guide" && <GuidePage onNavigate={navigate} />}
            </>
          )}
        </Suspense>
        {tabTransitionSplash && !isStandaloneOverlay && uiPrefs.tabTransition === "cinematic" && uiPrefs.motion !== "reduced" && (
          <TabSplash
            tabOverlay
            title={tabTransitionSplash}
            subtitle="Streaming GPU view surface & Skyrim bridge data"
            stageLabel="SWITCHING MODULE"
            theme={theme}
            mode={uiPrefs.tabTransition}
            reducedMotion={false}
          />
        )}
      </main>
      {!standaloneBook && (
      <footer className="app-footer">
        <span className="app-footer__brand">
          <strong>SKYULTRA</strong>
          <i />
          <span>{currentPalette.label.toUpperCase()} · {currentPalette.styleTag.toUpperCase()}</span>
        </span>
        <span className="app-footer__telemetry">
          <i className="status-dot" />
          {runtime.host === "skyrim" ? "LIVE SKYRIM DATA" : "PREVIEW FIXTURE"} · {engine.toUpperCase()}
          {standaloneMagic ? " · SPELLBOOK" : standaloneFavorites ? " · FAVORITES" : standaloneBlacksmith ? " · BLACKSMITH" : standaloneEnchanting ? " · ARCANE ENCHANTER" : standaloneAlchemy ? " · ALCHEMY LAB" : standaloneBook ? " · BOOK READER" : standaloneLoot ? " · CORPSE LOOT" : standalonePickpocket ? " · PICKPOCKET" : standaloneContainer ? " · STORAGE CONTAINER" : standaloneMannequin ? " · MANNEQUIN" : standaloneBarter ? " · VENDOR BARTER" : standaloneInventory ? " · PLAYER INVENTORY" : pinnedCount > 0 ? ` · ${pinnedCount} HUDS PINNED` : " · HUDS STANDBY"}
        </span>
        <div className="app-footer__actions">
          <button
            type="button"
            title="Cycle Input Prompts (Keyboard / Xbox / PlayStation / VR)"
            onClick={() => {
              const order: SkyUltraControllerGlyphFamily[] = ["keyboard", "xbox", "playstation", "vr"];
              const next = order[(order.indexOf(glyphFamily) + 1) % order.length];
              setGlyphFamily(next);
            }}
          >
            <kbd>{SKYULTRA_CONTROLLER_GLYPHS[glyphFamily].family.toUpperCase()}</kbd>{" "}
            {SKYULTRA_CONTROLLER_GLYPHS[glyphFamily].confirm}/{SKYULTRA_CONTROLLER_GLYPHS[glyphFamily].cancel}
          </button>
          <button
            type="button"
            title="Toggle Analog-Stick / D-Pad / Arrow-Key Virtual Cursor"
            onClick={() => {
              const nextEnabled = !virtualCursorEnabledRef.current;
              virtualCursorEnabledRef.current = nextEnabled;
              virtualCursorStateRef.current = {
                x: Math.round(window.innerWidth * 0.5),
                y: Math.round(window.innerHeight * 0.5),
                targetLabel: "CENTER",
              };
              setVirtualCursorEnabled(nextEnabled);
            }}
          >
            <kbd>{virtualCursorEnabled ? "ON" : "OFF"}</kbd> V-CURSOR
          </button>
          <button
            type="button"
            title={`Toggle On-Screen Virtual Keyboard (${formatSkyUltraHotkey(keyboardHotkey, "keyboard")})`}
            onClick={() => {
              setVirtualKeyboardOpen((v) => {
                const next = !v;
                if (!next && virtualKeyboardStandalone) {
                  setVirtualKeyboardStandalone(false);
                  void closeSkyUltraPage(bridge).catch(() => undefined);
                }
                return next;
              });
            }}
          >
            <kbd>{virtualKeyboardOpen ? "ON" : formatSkyUltraHotkey(keyboardHotkey, "keyboard")}</kbd> KEYBOARD
          </button>
          <button type="button" disabled={cadence.locked} onClick={() => setOverlay("help")}>
            <kbd>{SKYULTRA_CONTROLLER_GLYPHS[glyphFamily].tertiary}</kbd> HELP
          </button>
          {!isStandaloneOverlay && (
            <button type="button" disabled={cadence.locked} onClick={() => setOverlay("commands")}>
              <kbd>{SKYULTRA_CONTROLLER_GLYPHS[glyphFamily].secondary}</kbd> COMMANDS
            </button>
          )}
        </div>
        <span className="app-footer__reopen">
          <kbd>{SKYULTRA_CONTROLLER_GLYPHS[glyphFamily].cancel}</kbd> CLOSES ·{" "}
          <kbd>{hotkeyLabel.toUpperCase()}</kbd> {isStandaloneOverlay ? "FULL MENU" : "REOPENS"}
        </span>
      </footer>
      )}
    </div>
    </>)}
    {keyboardOnly && !sessionDormant && <SessionReadyBeacon epoch={sessionReadyEpoch} bridge={bridge} />}
    {virtualCursorEnabled && (
      <div
        ref={virtualCursorElRef}
        className="sky-virtual-cursor"
        style={{
          transform: `translate3d(${virtualCursorStateRef.current.x}px, ${virtualCursorStateRef.current.y}px, 0)`,
        }}
        aria-hidden="true"
      >
        <i />
        <span ref={virtualCursorBadgeRef} className="sky-virtual-cursor__badge">
          {virtualCursorStateRef.current.targetLabel} · {virtualCursorStateRef.current.x},{virtualCursorStateRef.current.y}
        </span>
      </div>
    )}
    {virtualKeyboardOpen && (
      <div
        className={"sky-virtual-keyboard sky-virtual-keyboard--deck" + (keyboardOnly ? " is-standalone" : "") + (vkPos ? " is-placed" : "")}
        role="region"
        aria-label="SkyUltra Full Virtual Keyboard"
        style={vkPos ? { left: `${vkPos.x}px`, top: `${vkPos.y}px` } : undefined}
      >
        <div
          className="sky-virtual-keyboard__head"
          onPointerDown={startVkDrag}
          onPointerMove={moveVkDrag}
          onPointerUp={endVkDrag}
          onPointerCancel={endVkDrag}
          onDoubleClick={() => { setVkPos(null); try { window.localStorage.removeItem("skyultra.virtualKeyboard.pos"); } catch { /* storage unavailable */ } }}
          title="Drag to move · double-click to re-center"
        >
          <span className="sky-virtual-keyboard__grip" aria-hidden="true"><i /><i /><i /></span>
          <span className="sky-virtual-keyboard__title">
            <strong>SKYULTRA</strong> VIRTUAL KEYBOARD · {formatSkyUltraHotkey(keyboardHotkey, "keyboard").toUpperCase()}
          </span>
          <span className="sky-virtual-keyboard__leds" aria-hidden="true">
            <span className={vkCaps ? "is-on" : ""}><i />CAPS</span>
            <span className={vkShift ? "is-on" : ""}><i />SHIFT</span>
            <span className={vkCtrl ? "is-on" : ""}><i />CTRL</span>
            <span className={vkAlt ? "is-on" : ""}><i />ALT</span>
          </span>
          <div style={{ display: "flex", gap: "6px", alignItems: "center" }}>
            <button
              type="button"
              className="sky-virtual-keyboard__close"
              aria-label="Close keyboard"
              onClick={() => {
                setVirtualKeyboardOpen(false);
                if (virtualKeyboardStandalone) {
                  setVirtualKeyboardStandalone(false);
                  void closeSkyUltraPage(bridge).catch(() => undefined);
                }
              }}
            >
              ×
            </button>
          </div>
        </div>
        {keyboardOnly && (
          <div className="sky-virtual-keyboard__composer">
            <textarea
              ref={vkComposerRef}
              rows={1}
              spellCheck={false}
              placeholder="Type with the keys below…"
              aria-label="Virtual keyboard text"
            />
            <button
              type="button"
              onMouseDown={(e) => e.preventDefault()}
              onClick={() => {
                const el = vkComposerRef.current;
                if (!el) return;
                el.select();
                try { document.execCommand("copy"); } catch { /* clipboard unavailable */ }
              }}
            >
              Copy
            </button>
            <button
              type="button"
              onMouseDown={(e) => e.preventDefault()}
              onClick={() => {
                const el = vkComposerRef.current;
                if (el) { el.value = ""; el.focus(); }
              }}
            >
              Clear
            </button>
          </div>
        )}
        <div className="sky-virtual-keyboard__rows">
          {(
            [
              ["ESC", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"],
              vkShift
                ? ["~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "BACKSPACE"]
                : ["`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "BACKSPACE"],
              [
                "TAB",
                ...(vkShift !== vkCaps
                  ? ["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"]
                  : ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"]),
                ...(vkShift ? ["{", "}", "|"] : ["[", "]", "\\"]),
              ],
              [
                "CAPS",
                ...(vkShift !== vkCaps
                  ? ["A", "S", "D", "F", "G", "H", "J", "K", "L"]
                  : ["a", "s", "d", "f", "g", "h", "j", "k", "l"]),
                ...(vkShift ? [":", "\""] : [";", "'"]),
                "ENTER",
              ],
              [
                "SHIFT",
                ...(vkShift !== vkCaps
                  ? ["Z", "X", "C", "V", "B", "N", "M"]
                  : ["z", "x", "c", "v", "b", "n", "m"]),
                ...(vkShift ? ["<", ">", "?"] : [",", ".", "/"]),
                "SHIFT",
              ],
              ["CTRL", "ALT", "SPACE", "LEFT", "UP", "DOWN", "RIGHT"],
            ] as const
          ).map((row, rIdx) => (
            <div key={rIdx} className="sky-virtual-keyboard__row">
              {row.map((keyLabel, kIdx) => {
                const isActiveMod =
                  (keyLabel === "SHIFT" && vkShift) ||
                  (keyLabel === "CAPS" && vkCaps) ||
                  (keyLabel === "CTRL" && vkCtrl) ||
                  (keyLabel === "ALT" && vkAlt);
                const isWide =
                  keyLabel === "BACKSPACE" ||
                  keyLabel === "ENTER" ||
                  keyLabel === "SHIFT" ||
                  keyLabel === "CAPS" ||
                  keyLabel === "TAB";
                const isSpace = keyLabel === "SPACE";
                return (
                  <button
                    key={`${rIdx}-${kIdx}-${keyLabel}`}
                    type="button"
                    className={
                      "sky-virtual-keyboard__key" +
                      (isActiveMod ? " is-active" : "") +
                      (isWide ? " is-wide" : "") +
                      (isSpace ? " is-space" : "") +
                      (keyLabel.length > 1 ? ` is-${keyLabel.toLowerCase()}` : "") +
                      (/^F\d+$/.test(keyLabel) ? " is-fn" : "") +
                      (["LEFT", "UP", "DOWN", "RIGHT"].includes(keyLabel) ? " is-arrow" : "")
                    }
                    data-key={keyLabel}
                    onMouseDown={(e) => e.preventDefault()}
                    onClick={() => {
                      if (keyLabel === "SHIFT") {
                        setVkShift((v) => !v);
                        return;
                      }
                      if (keyLabel === "CAPS") {
                        setVkCaps((v) => !v);
                        return;
                      }
                      if (keyLabel === "CTRL") {
                        setVkCtrl((v) => !v);
                        return;
                      }
                      if (keyLabel === "ALT") {
                        setVkAlt((v) => !v);
                        return;
                      }
                      if (keyLabel === "ESC") {
                        const evDown = new KeyboardEvent("keydown", {
                          key: "Escape",
                          code: "Escape",
                          bubbles: true,
                          cancelable: true,
                        });
                        window.dispatchEvent(evDown);
                        if (overlay) {
                          setOverlay(null);
                        } else if (themeOpen) {
                          setThemeOpen(false);
                        } else if (virtualKeyboardStandalone) {
                          setVirtualKeyboardOpen(false);
                          setVirtualKeyboardStandalone(false);
                          void closeSkyUltraPage(bridge).catch(() => undefined);
                        }
                        return;
                      }
                      if (/^F([1-9]|1[0-2])$/.test(keyLabel)) {
                        const fnEvent = new KeyboardEvent("keydown", {
                          key: keyLabel,
                          code: keyLabel,
                          shiftKey: vkShift,
                          ctrlKey: vkCtrl,
                          altKey: vkAlt,
                          bubbles: true,
                          cancelable: true,
                        });
                        window.dispatchEvent(fnEvent);
                        if (vkCtrl && keyLabel === "F3") {
                          setOverlay("help");
                        } else if (vkShift && keyLabel === "F6") {
                          void closeSkyUltraPage(bridge).catch(() => undefined);
                        }
                        if (vkShift) setVkShift(false);
                        if (vkCtrl) setVkCtrl(false);
                        if (vkAlt) setVkAlt(false);
                        return;
                      }
                      if (vkCtrl || vkAlt) {
                        const chordEv = new KeyboardEvent("keydown", {
                          key: keyLabel.length === 1 ? keyLabel.toLowerCase() : keyLabel,
                          code: keyLabel.length === 1 ? `Key${keyLabel.toUpperCase()}` : keyLabel,
                          shiftKey: vkShift,
                          ctrlKey: vkCtrl,
                          altKey: vkAlt,
                          bubbles: true,
                          cancelable: true,
                        });
                        window.dispatchEvent(chordEv);
                        setVkCtrl(false);
                        setVkAlt(false);
                        if (vkShift) setVkShift(false);
                        return;
                      }

                      const el = keyboardOnly
                        ? vkComposerRef.current
                        : (document.activeElement instanceof HTMLInputElement ||
                        document.activeElement instanceof HTMLTextAreaElement
                          ? document.activeElement
                          : lastTextInputRef.current) ??
                        document.querySelector<HTMLInputElement | HTMLTextAreaElement>(
                          "textarea, input[type='text'], input[type='search'], input:not([type])",
                        );

                      if (keyLabel === "LEFT" || keyLabel === "RIGHT" || keyLabel === "UP" || keyLabel === "DOWN") {
                        const arrowMap: Record<string, string> = {
                          LEFT: "ArrowLeft",
                          RIGHT: "ArrowRight",
                          UP: "ArrowUp",
                          DOWN: "ArrowDown",
                        };
                        const arrowKey = arrowMap[keyLabel] ?? "ArrowRight";
                        window.dispatchEvent(
                          new KeyboardEvent("keydown", {
                            key: arrowKey,
                            code: arrowKey,
                            shiftKey: vkShift,
                            bubbles: true,
                            cancelable: true,
                          }),
                        );
                        if (el && (keyLabel === "LEFT" || keyLabel === "RIGHT")) {
                          el.focus();
                          const pos = el.selectionStart ?? el.value.length;
                          const next = keyLabel === "LEFT" ? Math.max(0, pos - 1) : Math.min(el.value.length, pos + 1);
                          el.selectionStart = el.selectionEnd = next;
                        }
                        return;
                      }

                      if (!el) return;
                      el.focus();
                      const start = el.selectionStart ?? el.value.length;
                      const end = el.selectionEnd ?? el.value.length;
                      let nextVal = el.value;
                      let nextPos = start;
                      if (keyLabel === "BACKSPACE") {
                        if (start !== end) {
                          nextVal = el.value.slice(0, start) + el.value.slice(end);
                          nextPos = start;
                        } else if (start > 0) {
                          nextVal = el.value.slice(0, start - 1) + el.value.slice(end);
                          nextPos = start - 1;
                        }
                      } else if (keyLabel === "TAB") {
                        const ch = el instanceof HTMLTextAreaElement ? "  " : "\t";
                        nextVal = el.value.slice(0, start) + ch + el.value.slice(end);
                        nextPos = start + ch.length;
                      } else {
                        const ch = keyLabel === "SPACE" ? " " : keyLabel === "ENTER" ? "\n" : keyLabel;
                        nextVal = el.value.slice(0, start) + ch + el.value.slice(end);
                        nextPos = start + ch.length;
                      }
                      const proto =
                        el instanceof HTMLTextAreaElement
                          ? window.HTMLTextAreaElement.prototype
                          : window.HTMLInputElement.prototype;
                      const setter = Object.getOwnPropertyDescriptor(proto, "value")?.set;
                      if (setter) {
                        setter.call(el, nextVal);
                      } else {
                        el.value = nextVal;
                      }
                      el.dispatchEvent(new Event("input", { bubbles: true }));
                      el.selectionStart = el.selectionEnd = nextPos;
                      if (vkShift) setVkShift(false);
                    }}
                  >
                    {keyLabel === "LEFT"
                      ? "←"
                      : keyLabel === "UP"
                        ? "↑"
                        : keyLabel === "DOWN"
                          ? "↓"
                          : keyLabel === "RIGHT"
                            ? "→"
                            : keyLabel}
                  </button>
                );
              })}
            </div>
          ))}
        </div>
      </div>
    )}
    {overlay && <Suspense fallback={null}><FieldOverlays mode={overlay} route={route} onDismiss={() => setOverlay(null)} onNavigate={navigate} /></Suspense>}
    {splashPreviewOpen && (
      <TabSplash
        preview
        title={activeRouteItem.title}
        subtitle="Theme-synchronized transition crest & loading bar preview"
        stageLabel="SPLASH PREVIEW"
        theme={theme}
        mode={uiPrefs.tabTransition}
        reducedMotion={uiPrefs.motion === "reduced"}
        onDismiss={() => setSplashPreviewOpen(false)}
      />
    )}
  </SkyThemeScope></SkyCursorScope>);
}
