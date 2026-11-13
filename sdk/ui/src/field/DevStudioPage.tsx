import { verifyActiveShims, type ShimVerification } from "./verifyShims.js";
import * as React from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  SKYULTRA_COMPATIBILITY_MATRIX,
  auditLegacyModSourceForSkyUltra,
  captureLiveDomInspectorTree,
  configureSkyUltraSpatialPlacement,
  diagnoseCoexistenceAndLayering,
  diagnoseSkyUltraModRecord,
  manageSkyUltraDiagnosticSample,
  playSkyUltraSound,
  projectSkyUltraWorldAnchors,
  readSkyUltraDeveloperSnapshot,
  reloadSkyUltraModViews,
  skyUltraActorBones,
  skyUltraSoundPresets,
  skyUltraSpatialModes,
  toggleSkyUltraDomInspector,
  type SkyUltraDeveloperBridgeEntry,
  type SkyUltraDeveloperModRecord,
  type SkyUltraDeveloperSnapshot,
  type SkyUltraDomNodeSummary,
  type SkyUltraProjectedAnchor,
  type SkyUltraSpatialMode,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import { SkyFrame } from "../widgets.js";
import { Icon } from "./icons.js";
import { Empty, SectionTitle } from "./pieces.js";

type DevTab = "views" | "bridge" | "mods" | "doctor";
type ViewFilter = "all" | "panel" | "hud" | "visible";
type BridgeFilter = "all" | "errors" | "requests" | "events";
type ModFilter = "all" | "healthy" | "faults" | "active";
type DoctorSeverityFilter = "all" | "error" | "warning" | "info";

function formatBytes(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes <= 0) return "0 B";
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${Math.round(bytes)} B`;
}

function createPreviewFixtureSnapshot(sampleState: "none" | "broken" | "repaired"): SkyUltraDeveloperSnapshot {
  const now = Math.round(performance.now());
  const bridgeLog: SkyUltraDeveloperBridgeEntry[] = [
    {
      seq: 1,
      timeMs: Math.max(10, now - 1400),
      viewId: 1,
      modId: "skyultra.menu",
      kind: "request",
      method: "game.subscribe",
      status: "ok",
      durationMs: 1.4,
      payloadBytes: 112,
    },
    {
      seq: 2,
      timeMs: Math.max(20, now - 950),
      viewId: 1,
      modId: "skyultra.menu",
      kind: "event",
      method: "skyultra.game.update",
      status: "ok",
      durationMs: 0.3,
      payloadBytes: 1840,
    },
    {
      seq: 3,
      timeMs: Math.max(30, now - 420),
      viewId: 1,
      modId: "skyultra.menu",
      kind: "request",
      method: "ui.pageReady",
      status: "ok",
      durationMs: 0.4,
      payloadBytes: 2,
    },
  ];

  if (sampleState === "broken") {
    bridgeLog.unshift({
      seq: 4,
      timeMs: now - 60,
      viewId: 2,
      modId: "samples.broken_mod",
      kind: "permission-denied",
      method: "game.getInventory",
      status: "denied",
      durationMs: 0.2,
      payloadBytes: 34,
      message: "The SkyUltra page request method 'game.getInventory' is not allowed for this view.",
    });
  } else if (sampleState === "repaired") {
    bridgeLog.unshift(
      {
        seq: 5,
        timeMs: now - 40,
        viewId: 2,
        modId: "samples.broken_mod",
        kind: "request",
        method: "game.getInventory",
        status: "ok",
        durationMs: 1.8,
        payloadBytes: 640,
      },
      {
        seq: 6,
        timeMs: now - 20,
        viewId: 2,
        modId: "samples.broken_mod",
        kind: "request",
        method: "ui.pageReady",
        status: "ok",
        durationMs: 0.3,
        payloadBytes: 2,
      },
    );
  }

  const baseMods: Array<Omit<SkyUltraDeveloperModRecord, "findings">> = [
    {
      id: "skyultra.menu",
      name: "SkyUltra",
      version: "0.1.0",
      folderName: "SkyUltraMenu",
      registered: true,
      manifestValid: true,
      entry: "index.html",
      entryExists: true,
      hudExists: true,
      assetCount: 9,
      totalAssetBytes: 612 * 1024,
      missingAssets: [],
      storage: false,
      assetAccess: true,
      networkHosts: [],
      pageRequests: [
        "game.getPlayerSnapshot",
        "game.subscribe",
        "game.unsubscribe",
        "game.getInventory",
        "ui.pageReady",
        "ui.configureWidgets",
        "ui.getDeveloperSnapshot",
      ],
      activeViewCount: 1,
      estimatedSurfaceBytes: 1920 * 1080 * 4,
      bridgeRequestCount: 14,
      bridgeErrorCount: 0,
      deniedPermissionCount: 0,
      hotReloadCount: 0,
    },
  ];

  if (sampleState === "broken") {
    baseMods.push({
      id: "samples.broken_mod",
      name: "Broken Diagnostic Sample",
      version: "0.1.0",
      folderName: "samples.broken_mod",
      registered: true,
      manifestValid: true,
      entry: "index.html",
      entryExists: true,
      hudExists: false,
      assetCount: 2,
      totalAssetBytes: 4200,
      missingAssets: ["assets/missing-emblem.svg"],
      storage: false,
      assetAccess: true,
      networkHosts: [],
      pageRequests: ["game.getPlayerSnapshot"],
      activeViewCount: 1,
      estimatedSurfaceBytes: 640 * 360 * 4,
      bridgeRequestCount: 1,
      bridgeErrorCount: 1,
      deniedPermissionCount: 1,
      hotReloadCount: 0,
    });
  } else if (sampleState === "repaired") {
    baseMods.push({
      id: "samples.broken_mod",
      name: "Broken Diagnostic Sample (Repaired)",
      version: "0.1.1",
      folderName: "samples.broken_mod",
      registered: true,
      manifestValid: true,
      entry: "index.html",
      entryExists: true,
      hudExists: false,
      assetCount: 3,
      totalAssetBytes: 5120,
      missingAssets: [],
      storage: false,
      assetAccess: true,
      networkHosts: [],
      pageRequests: ["game.getPlayerSnapshot", "game.getInventory", "ui.pageReady"],
      activeViewCount: 1,
      estimatedSurfaceBytes: 640 * 360 * 4,
      bridgeRequestCount: 3,
      bridgeErrorCount: 0,
      deniedPermissionCount: 0,
      hotReloadCount: 1,
    });
  }

  return {
    available: true,
    capturedAtMs: now,
    processWorkingSetBytes: 2480 * 1024 * 1024,
    processPrivateBytes: 2110 * 1024 * 1024,
    hotReloadEnabled: true,
    hotReloadTotalCount: sampleState === "repaired" ? 1 : 0,
    lastReloadedMod: sampleState === "repaired" ? "samples.broken_mod" : "",
    hostGpuMs: 0.34,
    compositorGpuMs: 0.19,
    callbackCpuMs: 0.42,
    frameCount: 1840,
    activeSubscriptionCount: 1,
    views: [
      {
        handle: 1,
        viewId: 1,
        configId: "skyultra.menu.main",
        modId: "skyultra.menu",
        engine: "ultralight",
        layer: "panel",
        visible: true,
        transparent: true,
        clickThrough: false,
        modal: true,
        width: 1920,
        height: 1080,
        fpsLimit: 60,
        inputScopeDepth: 1,
        pendingRequests: 0,
        surfaceBytes: 1920 * 1080 * 4,
        allowedMethods: ["game.subscribe", "game.getInventory", "ui.pageReady", "ui.getDeveloperSnapshot"],
        initialUrl: "file:///Data/SkyUltra/SkyUltraMenu/index.html",
      },
      ...(sampleState !== "none"
        ? [
            {
              handle: 2,
              viewId: 2,
              configId: "samples.broken_mod.probe",
              modId: "samples.broken_mod",
              engine: "ultralight",
              layer: "hud" as const,
              visible: false,
              transparent: true,
              clickThrough: true,
              modal: false,
              width: 640,
              height: 360,
              fpsLimit: 30,
              inputScopeDepth: 0,
              pendingRequests: 0,
              surfaceBytes: 640 * 360 * 4,
              allowedMethods:
                sampleState === "broken"
                  ? ["game.getPlayerSnapshot"]
                  : ["game.getPlayerSnapshot", "game.getInventory", "ui.pageReady"],
              initialUrl: "file:///Data/SkyUltra/samples.broken_mod/index.html",
            },
          ]
        : []),
    ],
    mods: baseMods.map((m) => ({
      ...m,
      findings: diagnoseSkyUltraModRecord(m, bridgeLog),
    })),
    bridgeLog,
  };
}

export default function DevStudioPage({
  bridge,
  native,
  inspectRequest = null,
}: {
  bridge: SkyWebViewAPI;
  native: boolean;
  inspectRequest?: { modId: string; nonce: number } | null;
}): React.JSX.Element {
  const [tab, setTab] = useState<DevTab>(() => (inspectRequest?.modId ? "doctor" : "views"));
  const [previewSampleState, setPreviewSampleState] = useState<"none" | "broken" | "repaired">("none");
  const [snapshot, setSnapshot] = useState<SkyUltraDeveloperSnapshot>(() => createPreviewFixtureSnapshot("none"));
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [busy, setBusy] = useState(false);
  const [statusNote, setStatusNote] = useState("");
  const [shimResults, setShimResults] = useState<ShimVerification[] | null>(null);
  const [checkingShims, setCheckingShims] = useState(false);
  const shimAbort = useRef<AbortController | null>(null);
  useEffect(() => () => shimAbort.current?.abort(), []);
  const verifyShims = async (): Promise<void> => {
    if (checkingShims) return;
    const controller = new AbortController();
    shimAbort.current = controller;
    setCheckingShims(true);
    setShimResults(null);
    try {
      const result = await verifyActiveShims(window as unknown as Parameters<typeof verifyActiveShims>[0], controller.signal);
      if (!controller.signal.aborted) setShimResults(result);
    } finally {
      if (!controller.signal.aborted) setCheckingShims(false);
    }
  };
  const [viewFilter, setViewFilter] = useState<ViewFilter>("all");
  const [viewQuery, setViewQuery] = useState("");
  const [bridgeFilter, setBridgeFilter] = useState<BridgeFilter>("all");
  const [bridgeModFilter, setBridgeModFilter] = useState<string>("all");
  const [bridgeQuery, setBridgeQuery] = useState("");
  const [modFilter, setModFilter] = useState<ModFilter>("all");
  const [modQuery, setModQuery] = useState("");
  const [selectedDoctorModId, setSelectedDoctorModId] = useState<string>(
    () => inspectRequest?.modId || "samples.broken_mod",
  );
  const [doctorSeverity, setDoctorSeverity] = useState<DoctorSeverityFilter>("all");
  const [doctorPickerOpen, setDoctorPickerOpen] = useState(false);
  const doctorPickerRef = useRef<HTMLDivElement | null>(null);
  const [migrationModId, setMigrationModId] = useState<string>("author.ported_overlay");
  const [migrationSource, setMigrationSource] = useState<string>(
    `<!-- Paste PrismaUI, MageLight, or custom CEF HTML/JS to audit -->\n<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Cinzel" />\n<script>\n  RegisterJSListener("onPlayerUpdate", (stats) => console.log(stats.health));\n  InteropCall("getPlayerStats", "{}");\n  window.addEventListener("keydown", (e) => { if (e.key === "Escape") window.close(); });\n</script>`,
  );
  const [inspectedViewId, setInspectedViewId] = useState<number | null>(null);
  const [domNodes, setDomNodes] = useState<SkyUltraDomNodeSummary[]>([]);
  const [selectedDomNodeId, setSelectedDomNodeId] = useState<string>("0");
  const [domQuery, setDomQuery] = useState<string>("");
  const [replExpr, setReplExpr] = useState<string>("window.SkyWebView?.bridgeVersion ?? 'ready'");
  const [replOutput, setReplOutput] = useState<string>("Click Run Expression to evaluate against the active view.");
  const [spatialMode, setSpatialMode] = useState<SkyUltraSpatialMode>("screen");
  const [spatialBone, setSpatialBone] = useState<string>("NPC Head [Head]");
  const [projectedAnchors, setProjectedAnchors] = useState<SkyUltraProjectedAnchor[]>([]);

  useEffect(() => {
    if (!statusNote) return;
    const timer = window.setTimeout(() => setStatusNote(""), 3000);
    return () => window.clearTimeout(timer);
  }, [statusNote]);

  const handleToggleInspector = async (viewId: number): Promise<void> => {
    const nextOpen = inspectedViewId !== viewId;
    const res = await toggleSkyUltraDomInspector({ viewId, open: nextOpen }, bridge);
    if (nextOpen) {
      const captured = captureLiveDomInspectorTree(document.body, 72);
      setDomNodes(captured);
      if (captured[0]) setSelectedDomNodeId(captured[0].id);
      setInspectedViewId(viewId);
    } else {
      setInspectedViewId(null);
    }
    setStatusNote(res.message);
  };

  const handleRunRepl = (): void => {
    if (!replExpr.trim()) return;
    try {
      const result = new Function(`return (${replExpr})`)();
      setReplOutput(typeof result === "string" ? result : JSON.stringify(result, null, 2));
    } catch (err) {
      setReplOutput(`Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  const handleTestWorldProjection = async (): Promise<void> => {
    const res = await projectSkyUltraWorldAnchors(
      [
        { id: "player-head", referenceId: "0x00000014", bone: spatialBone, offsetZ: 18 },
        { id: "world-marker", x: 120, y: 240, z: 64, bone: "world" },
      ],
      bridge,
    );
    setProjectedAnchors(res.anchors);
    setStatusNote(`Projected ${res.anchors.length} 3D bone/worldspace anchor(s) onto screen coordinates.`);
  };

  const handleSetSpatialMode = async (mode: SkyUltraSpatialMode): Promise<void> => {
    setSpatialMode(mode);
    const res = await configureSkyUltraSpatialPlacement({ mode, bone: spatialBone }, bridge);
    setStatusNote(`Spatial placement mode set to '${res.mode}' (${res.bone}).`);
  };
  const coexistenceFindings = useMemo(() => diagnoseCoexistenceAndLayering(snapshot), [snapshot]);
  const migrationAudit = useMemo(
    () => auditLegacyModSourceForSkyUltra(migrationSource, { modId: migrationModId, modName: "Ported SkyUltra Overlay" }),
    [migrationSource, migrationModId],
  );

  useEffect(() => {
    if (!doctorPickerOpen) return;
    const onPointerDown = (e: PointerEvent) => {
      if (doctorPickerRef.current && !doctorPickerRef.current.contains(e.target as Node)) {
        setDoctorPickerOpen(false);
      }
    };
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") setDoctorPickerOpen(false);
    };
    window.addEventListener("pointerdown", onPointerDown);
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("pointerdown", onPointerDown);
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [doctorPickerOpen]);

  const refreshSnapshot = useCallback(async () => {
    if (!native) {
      setSnapshot(createPreviewFixtureSnapshot(previewSampleState));
      return;
    }
    try {
      const next = await readSkyUltraDeveloperSnapshot(bridge);
      setSnapshot(next);
    } catch (err) {
      setStatusNote(err instanceof Error ? err.message : String(err));
    }
  }, [bridge, native, previewSampleState]);

  useEffect(() => {
    void refreshSnapshot();
    if (!autoRefresh) return;
    const timer = window.setInterval(() => {
      void refreshSnapshot();
    }, 1500);
    return () => window.clearInterval(timer);
  }, [autoRefresh, refreshSnapshot]);

  useEffect(() => {
    if (!inspectRequest?.modId) return;
    const targetId = inspectRequest.modId.trim().toLowerCase();
    if (!targetId) return;
    setTab("doctor");
    setSelectedDoctorModId(targetId);
    setDoctorSeverity("all");
    setStatusNote(`Inspecting mod package '${targetId}' in Broken Mod Doctor.`);
    void refreshSnapshot();
  }, [inspectRequest?.modId, inspectRequest?.nonce, refreshSnapshot]);

  const totalVramBytes = useMemo(
    () => snapshot.views.reduce((sum, v) => sum + (v.surfaceBytes || 0), 0),
    [snapshot.views],
  );
  const errorModCount = useMemo(
    () => snapshot.mods.filter((m) => m.findings.some((f) => f.severity === "error")).length,
    [snapshot.mods],
  );
  const healthyModCount = useMemo(
    () => snapshot.mods.filter((m) => !m.findings.some((f) => f.severity === "error" || f.severity === "warning")).length,
    [snapshot.mods],
  );
  const faultOrWarnModCount = useMemo(
    () => snapshot.mods.filter((m) => m.findings.some((f) => f.severity === "error" || f.severity === "warning")).length,
    [snapshot.mods],
  );
  const activeViewModCount = useMemo(
    () => snapshot.mods.filter((m) => m.activeViewCount > 0).length,
    [snapshot.mods],
  );
  const deniedTotal = useMemo(
    () => snapshot.bridgeLog.filter((e) => e.status === "denied" || e.status === "error").length,
    [snapshot.bridgeLog],
  );
  const requestTotal = useMemo(
    () => snapshot.bridgeLog.filter((e) => e.kind === "request").length,
    [snapshot.bridgeLog],
  );
  const eventTotal = useMemo(
    () => snapshot.bridgeLog.filter((e) => e.kind === "event").length,
    [snapshot.bridgeLog],
  );
  const bridgeMods = useMemo(() => {
    const ids = new Set<string>();
    for (const entry of snapshot.bridgeLog) {
      if (entry.modId) ids.add(entry.modId);
    }
    return Array.from(ids);
  }, [snapshot.bridgeLog]);

  const filteredViews = useMemo(() => {
    const q = viewQuery.trim().toLowerCase();
    return snapshot.views.filter((v) => {
      if (viewFilter === "panel" && v.layer !== "panel") return false;
      if (viewFilter === "hud" && v.layer !== "hud") return false;
      if (viewFilter === "visible" && !v.visible) return false;
      if (!q) return true;
      return (
        v.configId.toLowerCase().includes(q) ||
        v.modId.toLowerCase().includes(q) ||
        v.engine.toLowerCase().includes(q) ||
        v.initialUrl.toLowerCase().includes(q)
      );
    });
  }, [snapshot.views, viewFilter, viewQuery]);

  const filteredBridgeLog = useMemo(() => {
    const q = bridgeQuery.trim().toLowerCase();
    return snapshot.bridgeLog.filter((entry) => {
      if (bridgeFilter === "errors" && entry.status !== "denied" && entry.status !== "error") return false;
      if (bridgeFilter === "requests" && entry.kind !== "request") return false;
      if (bridgeFilter === "events" && entry.kind !== "event") return false;
      if (bridgeModFilter !== "all" && entry.modId !== bridgeModFilter) return false;
      if (!q) return true;
      return (
        entry.method.toLowerCase().includes(q) ||
        entry.modId.toLowerCase().includes(q) ||
        (entry.message ?? "").toLowerCase().includes(q)
      );
    });
  }, [snapshot.bridgeLog, bridgeFilter, bridgeModFilter, bridgeQuery]);

  const filteredMods = useMemo(() => {
    const q = modQuery.trim().toLowerCase();
    return snapshot.mods.filter((mod) => {
      const hasFaultOrWarn = mod.findings.some((f) => f.severity === "error" || f.severity === "warning");
      if (modFilter === "healthy" && hasFaultOrWarn) return false;
      if (modFilter === "faults" && !hasFaultOrWarn) return false;
      if (modFilter === "active" && mod.activeViewCount === 0) return false;
      if (!q) return true;
      return (
        mod.id.toLowerCase().includes(q) ||
        mod.name.toLowerCase().includes(q) ||
        mod.folderName.toLowerCase().includes(q) ||
        mod.pageRequests.some((p) => p.toLowerCase().includes(q))
      );
    });
  }, [snapshot.mods, modFilter, modQuery]);

  const doctorMod = useMemo(() => {
    const needle = selectedDoctorModId.trim().toLowerCase();
    return (
      snapshot.mods.find(
        (m) => m.id.toLowerCase() === needle || m.folderName.toLowerCase() === needle,
      ) ??
      snapshot.mods.find((m) => m.id === "samples.broken_mod") ??
      snapshot.mods[0] ??
      null
    );
  }, [snapshot.mods, selectedDoctorModId]);

  const filteredDoctorFindings = useMemo(() => {
    if (!doctorMod) return [];
    if (doctorSeverity === "all") return doctorMod.findings;
    return doctorMod.findings.filter((f) => f.severity === doctorSeverity);
  }, [doctorMod, doctorSeverity]);

  const handleReloadMod = async (modId?: string, toggleWatcher?: boolean): Promise<void> => {
    if (busy) return;
    if (!native) {
      setStatusNote(
        toggleWatcher !== undefined
          ? `Preview mode: hot-reload watcher set to ${toggleWatcher ? "ENABLED" : "STANDBY"}.`
          : `Preview mode: simulated hot reload for '${modId ?? "all"}'.`,
      );
      return;
    }
    setBusy(true);
    try {
      const res = await reloadSkyUltraModViews(
        { modId, setHotReloadWatcher: toggleWatcher },
        bridge,
      );
      setStatusNote(
        res.message ??
          `Reloaded ${res.reloadedViewCount} view(s)${modId ? ` for ${modId}` : ""} · Hot-reload watcher ${res.hotReloadEnabled ? "ON" : "OFF"}.`,
      );
      await refreshSnapshot();
    } catch (err) {
      setStatusNote(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(false);
    }
  };

  const handleDiagnosticSampleAction = async (action: "deploy-broken" | "repair-and-reload" | "remove"): Promise<void> => {
    if (busy) return;
    if (!native) {
      const nextState = action === "deploy-broken" ? "broken" : action === "repair-and-reload" ? "repaired" : "none";
      setPreviewSampleState(nextState);
      setSnapshot(createPreviewFixtureSnapshot(nextState));
      setSelectedDoctorModId(nextState === "none" ? "skyultra.menu" : "samples.broken_mod");
      setStatusNote(
        action === "deploy-broken"
          ? "Deployed broken sample mod 'samples.broken_mod': caught disallowed 'game.getInventory', missing 'assets/missing-emblem.svg', and missing 'ui.pageReady'!"
          : action === "repair-and-reload"
            ? "Patched manifest.json + restored missing SVG asset + hot-reloaded 'samples.broken_mod' — 0 errors remaining!"
            : "Removed diagnostic sample mod.",
      );
      return;
    }
    setBusy(true);
    try {
      const res = await manageSkyUltraDiagnosticSample(action, bridge);
      setStatusNote(res.message ?? `Completed '${action}' for ${res.modId}.`);
      if (action !== "remove") {
        setSelectedDoctorModId("samples.broken_mod");
      }
      window.setTimeout(() => void refreshSnapshot(), 220);
    } catch (err) {
      setStatusNote(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="page dev-studio-page">
      <div className="page-lead page-lead--compact">
        <div>
          <span className="eyebrow">MODULE 08 · NATIVE HOST INSPECTOR, BRIDGE TELEMETRY & HOT RELOAD</span>
          <h1>
            Developer Studio<span className="title-period">.</span>
          </h1>
          <p>
            Inspect live Ultralight and CEF surfaces, monitor bridge requests and permission denials in real time, audit
            mod manifests and assets, track per-mod memory and GPU budgets, and hot-reload broken mods inside Skyrim.
          </p>
        </div>
        <div className="hud-studio-header-actions">
          <button
            type="button"
            className={"char-stage-btn" + (autoRefresh ? " char-stage-btn--active" : "")}
            onClick={() => setAutoRefresh((v) => !v)}
          >
            {autoRefresh ? "Live 1.5s Poll: ON" : "Live Poll: PAUSED"}
          </button>
          <button type="button" className="char-stage-btn" onClick={() => void refreshSnapshot()}>
            <Icon name="rotate" size={13} /> Refresh Now
          </button>
          <button
            type="button"
            className={"char-stage-btn" + (snapshot.hotReloadEnabled ? " char-stage-btn--primary" : "")}
            disabled={busy}
            onClick={() => void handleReloadMod(undefined, !snapshot.hotReloadEnabled)}
          >
            Watcher: {snapshot.hotReloadEnabled ? "ACTIVE" : "OFF"}
          </button>
        </div>
      </div>

      <div className="settings-overview-strip dev-kpi-strip">
        <div className="settings-kpi-pill">
          <small>ACTIVE BROWSER VIEWS</small>
          <strong>{snapshot.views.length} Surfaces</strong>
          <span>{formatBytes(totalVramBytes)} Est. Surface VRAM</span>
        </div>
        <div className="settings-kpi-pill">
          <small>MOD PACKAGES IN DATA/SKYULTRA</small>
          <strong>{snapshot.mods.length} Mods</strong>
          <span>{errorModCount > 0 ? `${errorModCount} mod(s) with errors` : "All manifests healthy"}</span>
        </div>
        <div className="settings-kpi-pill">
          <small>PROCESS RAM & GPU COMPOSITOR</small>
          <strong>{formatBytes(snapshot.processWorkingSetBytes)}</strong>
          <span>
            Host {snapshot.hostGpuMs.toFixed(2)} ms · Comp {snapshot.compositorGpuMs.toFixed(2)} ms · CPU{" "}
            {snapshot.callbackCpuMs.toFixed(2)} ms
          </span>
        </div>
        <div className="settings-kpi-pill">
          <small>BRIDGE TRAFFIC & HOT RELOAD</small>
          <strong>{snapshot.bridgeLog.length} Entries</strong>
          <span>
            {deniedTotal} errors/denials · {snapshot.hotReloadTotalCount} hot reloads
          </span>
        </div>
      </div>

      <div className="char-tab-rail dev-tab-rail" role="tablist" aria-label="Developer Studio sections">
        {(
          [
            { id: "views", label: "Views & Layers", sub: `${snapshot.views.length} Live Surfaces`, icon: "widgets" },
            { id: "bridge", label: "Bridge & Errors", sub: `${deniedTotal} Denials / ${snapshot.bridgeLog.length} Calls`, icon: "performance" },
            { id: "mods", label: "Per-Mod Diagnostics", sub: `${snapshot.mods.length} Packages & Hot Reload`, icon: "inventory" },
            { id: "doctor", label: "Broken Mod Doctor", sub: "Live In-Game Fault Diagnosis", icon: "shield" },
          ] as const
        ).map((item) => (
          <button
            key={item.id}
            type="button"
            role="tab"
            aria-selected={tab === item.id}
            className={"char-tab-card" + (tab === item.id ? " is-active" : "")}
            onClick={() => setTab(item.id)}
          >
            <span className="char-tab-card__icon">
              <Icon name={item.icon} size={17} />
            </span>
            <span className="char-tab-card__text">
              <strong>{item.label}</strong>
              <small>{item.sub}</small>
            </span>
          </button>
        ))}
      </div>

      {statusNote && <p className="char-loadout-status-banner">{statusNote}</p>}

      {tab === "views" && (
        <section className="surface dev-section">
          <SectionTitle eyebrow="NATIVE VIEW MANAGER & INPUT SCOPES" title="Active Browser Surfaces">
            <span className="quiet-pill">{snapshot.activeSubscriptionCount} ACTIVE SUBSCRIPTION(S)</span>
          </SectionTitle>

          <div className="dev-filter-toolbar">
            <div className="dev-filter-group" role="group" aria-label="Filter views by layer and visibility">
              {(
                [
                  { id: "all", label: "All Surfaces", count: snapshot.views.length },
                  { id: "panel", label: "Panel Layer", count: snapshot.views.filter((v) => v.layer === "panel").length },
                  { id: "hud", label: "HUD Layer", count: snapshot.views.filter((v) => v.layer === "hud").length },
                  { id: "visible", label: "Visible Only", count: snapshot.views.filter((v) => v.visible).length },
                ] as const
              ).map((f) => (
                <button
                  key={f.id}
                  type="button"
                  className={"dev-filter-pill" + (viewFilter === f.id ? " is-active" : "")}
                  onClick={() => setViewFilter(f.id)}
                >
                  <span>{f.label}</span>
                  <b className="dev-filter-pill__count">{f.count}</b>
                </button>
              ))}
            </div>
            <label className="page-search dev-search-box">
              <Icon name="search" size={15} />
              <input
                aria-label="Filter browser surfaces"
                placeholder="Search by configId, modId, engine, or URL…"
                value={viewQuery}
                onChange={(e) => setViewQuery(e.currentTarget.value)}
              />
              {viewQuery && (
                <button type="button" className="dev-search-clear" onClick={() => setViewQuery("")}>
                  Clear
                </button>
              )}
            </label>
          </div>

          {filteredViews.length === 0 ? (
            <Empty
              title="No matching browser surfaces"
              detail={
                snapshot.views.length === 0
                  ? "Views appear here as SkyUltra or installed mods create HUDs and panels."
                  : "Adjust your layer filter or search query to display active browser surfaces."
              }
              icon="widgets"
            />
          ) : (
            <div className="dev-views-list">
              {filteredViews.map((view) => (
                <article key={`${view.handle}-${view.viewId}`} className="dev-card">
                  <div className="dev-card__head">
                    <div>
                      <div className="dev-badges">
                        <span className="char-preset-pill char-preset-pill--active">
                          {view.layer.toUpperCase()} LAYER
                        </span>
                        <span className="char-preset-pill">{view.engine.toUpperCase()}</span>
                        <span className={"char-preset-pill " + (view.visible ? "is-success-pill" : "")}>
                          {view.visible ? "VISIBLE" : "HIDDEN"}
                        </span>
                        {view.clickThrough && <span className="char-preset-pill">CLICK-THROUGH</span>}
                        {view.modal && <span className="char-preset-pill">MODAL INPUT</span>}
                      </div>
                      <h3>
                        {view.configId || `View #${view.handle}`} <small>({view.modId})</small>
                      </h3>
                      <code className="dev-url">{view.initialUrl}</code>
                    </div>
                    <div className="dev-card__actions">
                      <button
                        type="button"
                        className={"char-stage-btn" + (inspectedViewId === view.viewId ? " char-stage-btn--active" : "")}
                        onClick={() => void handleToggleInspector(view.viewId)}
                      >
                        <Icon name="search" size={13} />{" "}
                        {inspectedViewId === view.viewId ? "Close DOM Inspector" : "Open DOM Inspector"}
                      </button>
                      <button
                        type="button"
                        className="char-stage-btn char-stage-btn--primary"
                        disabled={busy}
                        onClick={() => void handleReloadMod(view.modId)}
                      >
                        <Icon name="rotate" size={13} /> Reload View
                      </button>
                    </div>
                  </div>
                  <div className="dev-metrics-row">
                    <span>
                      <small>DIMENSIONS</small>
                      <strong>
                        {view.width} × {view.height}
                      </strong>
                    </span>
                    <span>
                      <small>EST. SURFACE VRAM</small>
                      <strong>{formatBytes(view.surfaceBytes)}</strong>
                    </span>
                    <span>
                      <small>FPS CAP</small>
                      <strong>{view.fpsLimit > 0 ? `${view.fpsLimit} FPS` : "UNCAPPED"}</strong>
                    </span>
                    <span>
                      <small>INPUT SCOPE DEPTH</small>
                      <strong>{view.inputScopeDepth}</strong>
                    </span>
                    <span>
                      <small>IN-FLIGHT REQUESTS</small>
                      <strong>{view.pendingRequests}</strong>
                    </span>
                    <span>
                      <small>ALLOWED METHODS</small>
                      <strong>{view.allowedMethods.length}</strong>
                    </span>
                    {view.eventQueue && <span title={`${view.eventQueue.controlPending} host controls; ${view.eventQueue.inFlight} in flight; ${view.eventQueue.rejected} rejected; oldest queued ${view.eventQueue.oldestMs.toFixed(1)} ms; longest batch ${view.eventQueue.maxDispatchMs.toFixed(1)} ms`}>
                      <small>PENDING EVENTS</small>
                      <strong>{view.eventQueue.pending} / {view.eventQueue.capacity}</strong>
                    </span>}
                    {view.responseQueue && <span title={`${view.responseQueue.pending} pending replies; ${view.responseQueue.rejected} rejected replies`}>
                      <small>BRIDGE BUFFER</small>
                      <strong>{formatBytes((view.eventQueue?.bytes ?? 0) + view.responseQueue.bytes)}</strong>
                    </span>}
                  </div>

                  {inspectedViewId === view.viewId && (() => {
                    const visibleNodes = domNodes.filter((n) => {
                      const q = domQuery.trim().toLowerCase();
                      if (!q) return true;
                      return (
                        n.tag.toLowerCase().includes(q) ||
                        n.domId.toLowerCase().includes(q) ||
                        n.classes.toLowerCase().includes(q) ||
                        n.textPreview.toLowerCase().includes(q)
                      );
                    });
                    const selectedNode =
                      domNodes.find((n) => n.id === selectedDomNodeId) ?? visibleNodes[0] ?? null;

                    return (
                      <div className="dev-dom-inspector">
                        <div className="dev-dom-inspector__head">
                          <div>
                            <span className="eyebrow">
                              EMBEDDED {view.engine.toUpperCase()} DOM, COMPUTED CSS &amp; REPL INSPECTOR
                            </span>
                            <h4>
                              Inspecting {view.configId} ({domNodes.length} Live Nodes)
                            </h4>
                          </div>
                          <div className="dev-card__actions">
                            <input
                              type="text"
                              className="dev-dom-search"
                              placeholder="Filter DOM nodes by tag, #id, or .class…"
                              value={domQuery}
                              onChange={(e) => setDomQuery(e.currentTarget.value)}
                            />
                            <button
                              type="button"
                              className="char-stage-btn"
                              onClick={() => setDomNodes(captureLiveDomInspectorTree(document.body, 72))}
                            >
                              Rescan DOM
                            </button>
                          </div>
                        </div>

                        <div className="dev-dom-inspector__grid">
                          <div className="dev-dom-tree" role="tree" aria-label="Live DOM Tree">
                            {visibleNodes.map((node) => (
                              <button
                                key={node.id}
                                type="button"
                                className={
                                  "dev-dom-tree__row" + (selectedNode?.id === node.id ? " is-selected" : "")
                                }
                                style={{ paddingLeft: `${Math.min(48, 10 + node.depth * 12)}px` }}
                                onClick={() => setSelectedDomNodeId(node.id)}
                              >
                                <code>
                                  &lt;{node.tag}
                                  {node.domId ? `#${node.domId}` : ""}
                                  {node.classes ? `.${node.classes}` : ""}&gt;
                                </code>
                                <small>
                                  {node.rect.width}×{node.rect.height}
                                </small>
                              </button>
                            ))}
                          </div>

                          <div className="dev-dom-details">
                            {selectedNode && (
                              <div className="dev-dom-computed">
                                <span className="eyebrow">COMPUTED STYLES &amp; BOUNDING BOX</span>
                                <div className="dev-metrics-row" style={{ marginTop: "6px" }}>
                                  <span>
                                    <small>SELECTOR</small>
                                    <strong>
                                      {selectedNode.tag}
                                      {selectedNode.domId ? `#${selectedNode.domId}` : ""}
                                    </strong>
                                  </span>
                                  <span>
                                    <small>BOUNDS</small>
                                    <strong>
                                      {selectedNode.rect.width}×{selectedNode.rect.height} @ ({selectedNode.rect.x},{" "}
                                      {selectedNode.rect.y})
                                    </strong>
                                  </span>
                                  <span>
                                    <small>DISPLAY / POS</small>
                                    <strong>
                                      {selectedNode.computed.display} · {selectedNode.computed.position}
                                    </strong>
                                  </span>
                                  <span>
                                    <small>COLOR</small>
                                    <strong>{selectedNode.computed.color}</strong>
                                  </span>
                                  <span>
                                    <small>FONT</small>
                                    <strong>{selectedNode.computed.font}</strong>
                                  </span>
                                  <span>
                                    <small>CHILDREN</small>
                                    <strong>{selectedNode.childCount}</strong>
                                  </span>
                                </div>
                              </div>
                            )}

                            <div className="dev-dom-repl">
                              <span className="eyebrow">LIVE JAVASCRIPT CONSOLE REPL</span>
                              <div className="dev-dom-repl__bar">
                                <input
                                  type="text"
                                  value={replExpr}
                                  onChange={(e) => setReplExpr(e.currentTarget.value)}
                                  onKeyDown={(e) => {
                                    if (e.key === "Enter") {
                                      e.preventDefault();
                                      handleRunRepl();
                                    }
                                  }}
                                  placeholder="Evaluate JS expression (e.g. window.prismaUI, window.magelight.version)…"
                                />
                                <button type="button" className="char-stage-btn char-stage-btn--primary" onClick={handleRunRepl}>
                                  Run Expression
                                </button>
                              </div>
                              <pre className="dev-dom-repl__output">{replOutput}</pre>
                            </div>
                          </div>
                        </div>
                      </div>
                    );
                  })()}
                </article>
              ))}
            </div>
          )}

          <SkyFrame
            eyebrow="NEXT-GEN RUNTIME CAPABILITIES · AUDIO, 3D WORLDSPACE, VR ATGAZE & COMPETITOR SHIMS"
            title="Native Skyrim Audio, 3D Bone Anchors & Drop-In PrismaUI / Magelight Shims"
            style={{ marginTop: "16px" }}
          >
            <div className="dev-capabilities-lab">
              <div className="dev-cap-card">
                <span className="eyebrow">1. NATIVE SKYRIM UI AUDIO BRIDGE (UI.PLAYSOUND)</span>
                <h4>Declarative data-sky-sound &amp; BSAudioManager</h4>
                <p>
                  Plays native Skyrim sound descriptors at the player&apos;s Skyrim UI volume. Add{" "}
                  <code>data-sky-sound=&quot;ok|focus|cancel|equip|gold|shout&quot;</code> to any HTML element.
                </p>
                <div className="hud-live-editor-mode-pills" style={{ marginTop: "8px" }}>
                  {skyUltraSoundPresets.slice(0, 8).map((snd) => (
                    <button
                      key={snd}
                      type="button"
                      className="char-stage-btn"
                      data-sky-sound={snd}
                      onClick={() => {
                        void playSkyUltraSound(snd, bridge).then((res) =>
                          setStatusNote(`Played Skyrim sound '${res.sound}' (${snd}).`),
                        );
                      }}
                    >
                      🔊 {snd}
                    </button>
                  ))}
                </div>
              </div>

              <div className="dev-cap-card">
                <span className="eyebrow">2. 3D WORLDSPACE BONE PROJECTION &amp; VR ATGAZE</span>
                <h4>NiNode Bone Anchors &amp; Spatial Placement</h4>
                <p>
                  Attach web surfaces to 3D actor bones (<code>{spatialBone}</code>) or VR spatial modes (
                  <code>atGaze</code>, <code>worldLocked</code>, <code>leftWrist</code>).
                </p>
                <div className="hud-live-editor-mode-pills" style={{ marginTop: "8px" }}>
                  {skyUltraSpatialModes.map((m) => (
                    <button
                      key={m}
                      type="button"
                      className={"char-stage-btn" + (spatialMode === m ? " char-stage-btn--active" : "")}
                      onClick={() => void handleSetSpatialMode(m)}
                    >
                      {m}
                    </button>
                  ))}
                </div>
                <div className="hud-live-editor-mode-pills" style={{ marginTop: "8px" }}>
                  {skyUltraActorBones.slice(0, 4).map((b) => (
                    <button
                      key={b}
                      type="button"
                      className={"char-stage-btn" + (spatialBone === b ? " char-stage-btn--primary" : "")}
                      onClick={() => setSpatialBone(b)}
                    >
                      {b}
                    </button>
                  ))}
                  <button type="button" className="primary-button" onClick={() => void handleTestWorldProjection()}>
                    Project 3D Bone Anchors
                  </button>
                </div>
                {projectedAnchors.length > 0 && (
                  <div className="dev-metrics-row" style={{ marginTop: "8px" }}>
                    {projectedAnchors.map((a) => (
                      <span key={a.id}>
                        <small>{a.name.toUpperCase()} ({a.bone})</small>
                        <strong>
                          u={a.u.toFixed(2)}, v={a.v.toFixed(2)} · {a.distance}u ({a.onScreen ? "ON SCREEN" : "CLIPPED"})
                        </strong>
                      </span>
                    ))}
                  </div>
                )}
              </div>

              <div className="dev-cap-card">
                <span className="eyebrow">3. DROP-IN PRISMA UI &amp; MAGELIGHT UI SHIMS</span>
                <h4>PrismaUI / MagelightUI Runtime Shims</h4>
                <p>
                  <code>window.prismaUI</code> (<code>InteropCall</code>, <code>RegisterJSListener</code>,{" "}
                  <code>SetOrder</code>) and <code>window.magelight</code> (<code>send</code>, <code>vr.atGaze</code>,{" "}
                  <code>gamepad</code>) are installed in this menu. Other pages opt in through the SDK. Verification checks their host interop and listener round trip.
                </p>
                <div className="hud-live-editor-mode-pills" style={{ marginTop: "8px" }}>
                  <button
                    type="button"
                    className="char-stage-btn char-stage-btn--primary"
                    disabled={checkingShims}
                    onClick={() => void verifyShims()}
                  >
                    {checkingShims ? "Checking shims…" : "Verify Active Shims"}
                  </button>
                  <div role="status" aria-live="polite" aria-atomic="true">
                    {checkingShims && <p>Checking installed APIs and host listener echoes…</p>}
                    {shimResults?.map(result => <p key={result.framework}>
                      <strong>{result.framework}: {result.passed ? "PASS" : "FAILED"}</strong> · {result.message}
                    </p>)}
                  </div>
                </div>
              </div>
            </div>
          </SkyFrame>
        </section>
      )}

      {tab === "bridge" && (
        <section className="surface dev-section">
          <SectionTitle eyebrow="REAL-TIME IPC & PERMISSION LEDGER" title="Bridge Requests, Events & Errors">
            <span className="quiet-pill">{filteredBridgeLog.length} SHOWN</span>
          </SectionTitle>

          <div className="dev-filter-toolbar">
            <div className="dev-filter-group" role="group" aria-label="Filter bridge entries by type">
              {(
                [
                  { id: "all", label: "All Traffic", count: snapshot.bridgeLog.length, danger: false },
                  { id: "errors", label: "Errors & Denied", count: deniedTotal, danger: deniedTotal > 0 },
                  { id: "requests", label: "Requests", count: requestTotal, danger: false },
                  { id: "events", label: "Events", count: eventTotal, danger: false },
                ] as const
              ).map((f) => (
                <button
                  key={f.id}
                  type="button"
                  className={
                    "dev-filter-pill" +
                    (bridgeFilter === f.id ? " is-active" : "") +
                    (f.danger ? " dev-filter-pill--danger" : "")
                  }
                  onClick={() => setBridgeFilter(f.id)}
                >
                  <span>{f.label}</span>
                  <b className="dev-filter-pill__count">{f.count}</b>
                </button>
              ))}
            </div>

            <label className="page-search dev-search-box">
              <Icon name="search" size={15} />
              <input
                aria-label="Filter bridge ledger"
                placeholder="Filter by method (e.g. game.getInventory, skyultra.game.update), modId, or error…"
                value={bridgeQuery}
                onChange={(e) => setBridgeQuery(e.currentTarget.value)}
              />
              {bridgeQuery && (
                <button type="button" className="dev-search-clear" onClick={() => setBridgeQuery("")}>
                  Clear
                </button>
              )}
            </label>
          </div>

          {bridgeMods.length > 1 && (
            <div className="dev-subfilter-row" role="group" aria-label="Filter bridge entries by mod">
              <span className="dev-subfilter-label">MOD SOURCE:</span>
              <button
                type="button"
                className={"dev-filter-pill dev-filter-pill--compact" + (bridgeModFilter === "all" ? " is-active" : "")}
                onClick={() => setBridgeModFilter("all")}
              >
                All Mods
              </button>
              {bridgeMods.map((modId) => (
                <button
                  key={modId}
                  type="button"
                  className={
                    "dev-filter-pill dev-filter-pill--compact" + (bridgeModFilter === modId ? " is-active" : "")
                  }
                  onClick={() => setBridgeModFilter(modId)}
                >
                  {modId}
                </button>
              ))}
            </div>
          )}

          {filteredBridgeLog.length === 0 ? (
            <Empty
              title="No matching bridge entries"
              detail="Trigger UI actions, clear active filters, or deploy the Broken Sample Mod to inspect live bridge traffic."
              icon="performance"
            />
          ) : (
            <div className="dev-bridge-table">
              {filteredBridgeLog.map((entry) => {
                const isBad = entry.status === "denied" || entry.status === "error";
                return (
                  <div key={entry.seq} className={"dev-bridge-row" + (isBad ? " is-error" : "")}>
                    <span className={"dev-status-pill is-" + entry.status}>{entry.status.toUpperCase()}</span>
                    <div className="dev-bridge-row__main">
                      <div>
                        <strong>{entry.method}</strong>
                        <span className="dev-bridge-mod">{entry.modId || `view:${entry.viewId}`}</span>
                        <span className="dev-bridge-kind">{entry.kind}</span>
                      </div>
                      {entry.message && <p className="dev-bridge-error-msg">{entry.message}</p>}
                    </div>
                    <div className="dev-bridge-row__stats">
                      <b>{entry.durationMs.toFixed(2)} ms</b>
                      <small>{formatBytes(entry.payloadBytes)}</small>
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </section>
      )}

      {tab === "mods" && (
        <section className="surface dev-section">
          <SectionTitle eyebrow="PER-MOD ASSETS, PERMISSIONS, MEMORY & HOT RELOAD" title="Installed & Registered SkyUltra Mods">
            <button
              type="button"
              className="char-stage-btn char-stage-btn--primary"
              disabled={busy}
              onClick={() => void handleReloadMod("all")}
            >
              <Icon name="rotate" size={13} /> Hot Reload All Mod Views
            </button>
          </SectionTitle>

          <div className="dev-filter-toolbar">
            <div className="dev-filter-group" role="group" aria-label="Filter mods by diagnostic health">
              {(
                [
                  { id: "all", label: "All Mods", count: snapshot.mods.length, danger: false },
                  { id: "healthy", label: "Healthy", count: healthyModCount, danger: false },
                  { id: "faults", label: "Faults & Warnings", count: faultOrWarnModCount, danger: errorModCount > 0 },
                  { id: "active", label: "Active Views", count: activeViewModCount, danger: false },
                ] as const
              ).map((f) => (
                <button
                  key={f.id}
                  type="button"
                  className={
                    "dev-filter-pill" +
                    (modFilter === f.id ? " is-active" : "") +
                    (f.danger ? " dev-filter-pill--danger" : "")
                  }
                  onClick={() => setModFilter(f.id)}
                >
                  <span>{f.label}</span>
                  <b className="dev-filter-pill__count">{f.count}</b>
                </button>
              ))}
            </div>

            <label className="page-search dev-search-box">
              <Icon name="search" size={15} />
              <input
                aria-label="Filter installed mods"
                placeholder="Search by mod name, id, folder, or permission…"
                value={modQuery}
                onChange={(e) => setModQuery(e.currentTarget.value)}
              />
              {modQuery && (
                <button type="button" className="dev-search-clear" onClick={() => setModQuery("")}>
                  Clear
                </button>
              )}
            </label>
          </div>

          {filteredMods.length === 0 ? (
            <Empty
              title="No matching mod packages"
              detail="Clear the active filter or export a mod from HUDs & Widgets to inspect it here."
              icon="inventory"
            />
          ) : (
            <div className="dev-mods-grid">
              {filteredMods.map((mod) => {
                const hasErrors = mod.findings.some((f) => f.severity === "error");
                const hasWarnings = !hasErrors && mod.findings.some((f) => f.severity === "warning");
                return (
                  <article
                    key={mod.id || mod.folderName}
                    className={"dev-card" + (hasErrors ? " is-faulty" : hasWarnings ? " is-warning" : "")}
                  >
                    <div className="dev-card__head">
                      <div>
                        <div className="dev-badges">
                          <span
                            className={
                              "char-preset-pill " +
                              (hasErrors
                                ? "is-danger-pill"
                                : hasWarnings
                                  ? "is-warning-pill"
                                  : "is-success-pill")
                            }
                          >
                            {hasErrors ? "DIAGNOSTIC FAULT" : hasWarnings ? "WARNING" : "HEALTHY"}
                          </span>
                          <span className="char-preset-pill">v{mod.version || "0.0.0"}</span>
                          <span className="char-preset-pill">
                            {mod.assetCount} FILES ({formatBytes(mod.totalAssetBytes)})
                          </span>
                        </div>
                        <h3>
                          {mod.name} <small>({mod.id})</small>
                        </h3>
                        <code className="dev-url">
                          Data/SkyUltra/{mod.folderName}/{mod.entry}
                        </code>
                      </div>
                      <div className="dev-card__actions">
                        <button
                          type="button"
                          className="char-stage-btn"
                          onClick={() => {
                            setSelectedDoctorModId(mod.id);
                            setDoctorSeverity("all");
                            setTab("doctor");
                          }}
                        >
                          Inspect in Doctor
                        </button>
                        <button
                          type="button"
                          className="char-stage-btn char-stage-btn--primary"
                          disabled={busy}
                          onClick={() => void handleReloadMod(mod.id)}
                        >
                          <Icon name="rotate" size={13} /> Hot Reload
                        </button>
                      </div>
                    </div>

                    <div className="dev-metrics-row">
                      <span>
                        <small>ACTIVE VIEWS</small>
                        <strong>{mod.activeViewCount}</strong>
                      </span>
                      <span>
                        <small>SURFACE MEMORY</small>
                        <strong>{formatBytes(mod.estimatedSurfaceBytes)}</strong>
                      </span>
                      <span>
                        <small>BRIDGE CALLS</small>
                        <strong>{mod.bridgeRequestCount}</strong>
                      </span>
                      <span>
                        <small>DENIED / ERRORS</small>
                        <strong className={mod.deniedPermissionCount + mod.bridgeErrorCount > 0 ? "text-danger" : ""}>
                          {mod.deniedPermissionCount + mod.bridgeErrorCount}
                        </strong>
                      </span>
                      <span>
                        <small>HOT RELOADS</small>
                        <strong>{mod.hotReloadCount}</strong>
                      </span>
                      <span>
                        <small>STORAGE / ASSETS</small>
                        <strong>
                          {mod.storage ? "Storage ON" : "No Storage"} · {mod.assetAccess ? "Package" : "Locked"}
                        </strong>
                      </span>
                    </div>

                    <div className="dev-permissions-strip">
                      <small>DECLARED PAGEREQUEST PERMISSIONS ({mod.pageRequests.length}):</small>
                      <div className="dev-perm-chips">
                        {mod.pageRequests.length > 0 ? (
                          mod.pageRequests.map((perm) => (
                            <code key={perm} className="dev-perm-chip">
                              {perm}
                            </code>
                          ))
                        ) : (
                          <span className="text-danger">None declared</span>
                        )}
                      </div>
                    </div>
                  </article>
                );
              })}
            </div>
          )}
        </section>
      )}

      {tab === "doctor" && (
        <div className="dev-doctor-layout">
          <SkyFrame
            eyebrow="AUTOMATED PACKAGE & RUNTIME AUDITOR"
            title={`Mod Doctor Report: ${doctorMod?.name ?? "Select a Mod"}`}
            actions={
              <div className="dev-mod-selector" ref={doctorPickerRef}>
                <button
                  type="button"
                  aria-haspopup="listbox"
                  aria-expanded={doctorPickerOpen}
                  aria-label="Select mod to diagnose"
                  className={"dev-mod-selector__trigger" + (doctorPickerOpen ? " is-open" : "")}
                  onClick={() => setDoctorPickerOpen((open) => !open)}
                >
                  <span
                    className={
                      "dev-mod-selector__dot " +
                      (doctorMod?.findings.some((f) => f.severity === "error")
                        ? "is-error"
                        : doctorMod?.findings.some((f) => f.severity === "warning")
                          ? "is-warning"
                          : "is-healthy")
                    }
                  />
                  <span className="dev-mod-selector__label">
                    <strong>{doctorMod?.name ?? "Select Mod"}</strong>
                    {doctorMod && <small>{doctorMod.id}</small>}
                  </span>
                  <Icon name="chevron" size={12} />
                </button>

                {doctorPickerOpen && (
                  <div className="dev-mod-selector__menu" role="listbox" aria-label="Installed SkyUltra mods">
                    {snapshot.mods.map((m) => {
                      const modHasErr = m.findings.some((f) => f.severity === "error");
                      const modHasWarn = !modHasErr && m.findings.some((f) => f.severity === "warning");
                      const selected = doctorMod?.id === m.id;
                      return (
                        <button
                          key={m.id || m.folderName}
                          type="button"
                          role="option"
                          aria-selected={selected}
                          className={"dev-mod-selector__option" + (selected ? " is-selected" : "")}
                          onClick={() => {
                            setSelectedDoctorModId(m.id);
                            setDoctorPickerOpen(false);
                          }}
                        >
                          <span
                            className={
                              "dev-mod-selector__dot " +
                              (modHasErr ? "is-error" : modHasWarn ? "is-warning" : "is-healthy")
                            }
                          />
                          <span className="dev-mod-selector__option-copy">
                            <strong>{m.name}</strong>
                            <small>
                              {m.id} · v{m.version || "0.0.0"}
                            </small>
                          </span>
                          <span
                            className={
                              "dev-mod-selector__badge " +
                              (modHasErr ? "is-error" : modHasWarn ? "is-warning" : "is-healthy")
                            }
                          >
                            {modHasErr ? "FAULT" : modHasWarn ? "WARN" : "OK"}
                          </span>
                        </button>
                      );
                    })}
                  </div>
                )}
              </div>
            }
          >
            {selectedDoctorModId &&
              doctorMod &&
              doctorMod.id.toLowerCase() !== selectedDoctorModId.toLowerCase() &&
              doctorMod.folderName.toLowerCase() !== selectedDoctorModId.toLowerCase() && (
                <p className="char-loadout-status-banner" style={{ marginBottom: "12px" }}>
                  Package <code>Data/SkyUltra/{selectedDoctorModId}/</code> was not found on disk yet — make sure to click{" "}
                  <strong>Export Working Mod</strong> first, then click <strong>Refresh Now</strong>. Showing{" "}
                  <code>{doctorMod.id}</code> below.
                </p>
              )}

            {doctorMod ? (
              <>
                <div className="dev-card" style={{ marginBottom: "14px" }}>
                  <div className="dev-card__head">
                    <div>
                      <div className="dev-badges">
                        <span
                          className={
                            "char-preset-pill " +
                            (doctorMod.findings.some((f) => f.severity === "error")
                              ? "is-danger-pill"
                              : doctorMod.findings.some((f) => f.severity === "warning")
                                ? "is-warning-pill"
                                : "is-success-pill")
                          }
                        >
                          {doctorMod.findings.some((f) => f.severity === "error")
                            ? "DIAGNOSTIC FAULT"
                            : doctorMod.findings.some((f) => f.severity === "warning")
                              ? "WARNING"
                              : "HEALTHY PACKAGE"}
                        </span>
                        <span className="char-preset-pill">v{doctorMod.version || "0.0.0"}</span>
                        <span className="char-preset-pill">
                          {doctorMod.hudExists ? "PANEL + HUD.HTML" : "PANEL ENTRY"}
                        </span>
                      </div>
                      <h3>
                        {doctorMod.name} <small>({doctorMod.id})</small>
                      </h3>
                      <code className="dev-url">
                        Data/SkyUltra/{doctorMod.folderName}/{doctorMod.entry}
                      </code>
                    </div>
                    <div className="dev-card__actions">
                      <button
                        type="button"
                        className="char-stage-btn"
                        onClick={() => {
                          setModFilter("all");
                          setModQuery(doctorMod.id);
                          setTab("mods");
                        }}
                      >
                        Show in Installed Mods
                      </button>
                      <button
                        type="button"
                        className="char-stage-btn char-stage-btn--primary"
                        disabled={busy}
                        onClick={() => void handleReloadMod(doctorMod.id)}
                      >
                        <Icon name="rotate" size={13} /> Hot Reload Mod
                      </button>
                    </div>
                  </div>
                  <div className="dev-metrics-row">
                    <span>
                      <small>MANIFEST V1</small>
                      <strong>{doctorMod.manifestValid ? "VALID" : "INVALID"}</strong>
                    </span>
                    <span>
                      <small>ENTRY DOCUMENT</small>
                      <strong>{doctorMod.entryExists ? doctorMod.entry : "MISSING"}</strong>
                    </span>
                    <span>
                      <small>HUD OVERLAY</small>
                      <strong>{doctorMod.hudExists ? "hud.html READY" : "NONE"}</strong>
                    </span>
                    <span>
                      <small>PACKAGE ASSETS</small>
                      <strong>
                        {doctorMod.assetCount} files ({formatBytes(doctorMod.totalAssetBytes)})
                      </strong>
                    </span>
                    <span>
                      <small>ACTIVE VIEWS</small>
                      <strong>{doctorMod.activeViewCount}</strong>
                    </span>
                    <span>
                      <small>PERMISSIONS</small>
                      <strong>{doctorMod.pageRequests.length} methods</strong>
                    </span>
                  </div>
                  {doctorMod.pageRequests.length > 0 && (
                    <div className="dev-permissions-strip">
                      <small>DECLARED PAGEREQUEST PERMISSIONS ({doctorMod.pageRequests.length}):</small>
                      <div className="dev-perm-chips">
                        {doctorMod.pageRequests.map((perm) => (
                          <code key={perm} className="dev-perm-chip">
                            {perm}
                          </code>
                        ))}
                      </div>
                    </div>
                  )}
                </div>

                <div className="dev-doctor-filter-bar" role="group" aria-label="Filter diagnostic findings by severity">
                  {(
                    [
                      { id: "all", label: "All Findings", count: doctorMod.findings.length },
                      {
                        id: "error",
                        label: "Errors",
                        count: doctorMod.findings.filter((f) => f.severity === "error").length,
                      },
                      {
                        id: "warning",
                        label: "Warnings",
                        count: doctorMod.findings.filter((f) => f.severity === "warning").length,
                      },
                      {
                        id: "info",
                        label: "Passed",
                        count: doctorMod.findings.filter((f) => f.severity === "info").length,
                      },
                    ] as const
                  ).map((sev) => (
                    <button
                      key={sev.id}
                      type="button"
                      className={
                        "dev-filter-pill dev-filter-pill--compact" +
                        (doctorSeverity === sev.id ? " is-active" : "") +
                        (sev.id === "error" && sev.count > 0 ? " dev-filter-pill--danger" : "")
                      }
                      onClick={() => setDoctorSeverity(sev.id)}
                    >
                      <span>{sev.label}</span>
                      <b className="dev-filter-pill__count">{sev.count}</b>
                    </button>
                  ))}
                </div>

                {filteredDoctorFindings.length === 0 ? (
                  <Empty
                    title="No findings in this severity tier"
                    detail="Switch back to All Findings to view the full diagnostic audit for this mod."
                    icon="shield"
                  />
                ) : (
                  <div className="dev-findings-list">
                    {filteredDoctorFindings.map((finding, idx) => (
                      <article key={`${finding.code}-${idx}`} className={`dev-finding-card is-${finding.severity}`}>
                        <div className="dev-finding-card__top">
                          <span className={`dev-status-pill is-${finding.severity}`}>
                            {finding.severity.toUpperCase()}
                          </span>
                          <code>{finding.code}</code>
                          <strong>{finding.title}</strong>
                        </div>
                        <p>{finding.detail}</p>
                        {finding.fixSnippet && (
                          <div className="dev-finding-fix">
                            <div className="dev-finding-fix__head">
                              <span>SUGGESTED REMEDIATION</span>
                              <button
                                type="button"
                                className="char-stage-btn char-stage-btn--sm"
                                onClick={() => {
                                  if (navigator.clipboard && finding.fixSnippet) {
                                    void navigator.clipboard.writeText(finding.fixSnippet);
                                    setStatusNote(`Copied remediation snippet for ${finding.code}.`);
                                  }
                                }}
                              >
                                Copy Fix
                              </button>
                            </div>
                            <pre>
                              <code>{finding.fixSnippet}</code>
                            </pre>
                          </div>
                        )}
                      </article>
                    ))}
                  </div>
                )}
              </>
            ) : (
              <Empty title="No mod selected" detail="Deploy the Broken Sample Mod or select an installed mod above." icon="shield" />
            )}
          </SkyFrame>

          <SkyFrame
            eyebrow="LIVE IN-GAME DIAGNOSTIC DEMONSTRATION"
            title="Broken Sample Mod Lab (samples.broken_mod)"
            footer="Deploys a real broken mod into Data/SkyUltra/samples.broken_mod, captures its live native permission failure and missing asset inside Skyrim, then applies the fix and hot-reloads it live."
          >
            <div className="dev-lab-steps">
              <div className="dev-lab-step">
                <span className="dev-lab-step__num">1</span>
                <div>
                  <strong>Deploy & Execute Broken Sample Mod</strong>
                  <p>
                    Writes <code>Data/SkyUltra/samples.broken_mod/</code> missing <code>game.getInventory</code> and{" "}
                    <code>ui.pageReady</code> permissions and referencing a missing <code>assets/missing-emblem.svg</code>,
                    then opens a probe view so Skyrim logs the real failure.
                  </p>
                </div>
                <button
                  type="button"
                  className="char-stage-btn char-stage-btn--primary"
                  disabled={busy}
                  onClick={() => void handleDiagnosticSampleAction("deploy-broken")}
                >
                  Deploy Broken Mod
                </button>
              </div>

              <div className="dev-lab-step">
                <span className="dev-lab-step__num">2</span>
                <div>
                  <strong>Apply Repair & Hot-Reload In-Game</strong>
                  <p>
                    Updates <code>manifest.json</code> with least-privilege permissions, writes the missing SVG asset, adds{" "}
                    <code>ui.pageReady</code> after paint, and triggers native hot reload without restarting Skyrim.
                  </p>
                </div>
                <button
                  type="button"
                  className="char-stage-btn char-stage-btn--active"
                  disabled={busy}
                  onClick={() => void handleDiagnosticSampleAction("repair-and-reload")}
                >
                  Repair & Hot Reload
                </button>
              </div>

              <div className="dev-lab-step">
                <span className="dev-lab-step__num">3</span>
                <div>
                  <strong>Clean Up Sample Package</strong>
                  <p>Destroys the probe surface and removes the temporary diagnostic sample folder from disk.</p>
                </div>
                <button
                  type="button"
                  className="char-stage-btn char-stage-btn--danger"
                  disabled={busy}
                  onClick={() => void handleDiagnosticSampleAction("remove")}
                >
                  Remove Sample
                </button>
              </div>
            </div>
          </SkyFrame>

          <SkyFrame
            eyebrow="LIVE D3D11 LAYERING, INPUT SCOPES & PEER MOD COEXISTENCE"
            title="Runtime Coexistence & Ecosystem Matrix"
          >
            <div className="dev-findings-list" style={{ marginBottom: "14px" }}>
              {coexistenceFindings.map((finding, idx) => (
                <article key={`coexist-${finding.code}-${idx}`} className={`dev-finding-card is-${finding.severity}`}>
                  <div className="dev-finding-card__top">
                    <span className={`dev-status-pill is-${finding.severity}`}>{finding.severity.toUpperCase()}</span>
                    <code>{finding.code}</code>
                    <strong>{finding.title}</strong>
                  </div>
                  <p>{finding.detail}</p>
                </article>
              ))}
            </div>
            <div style={{ display: "grid", gap: "10px" }}>
              {SKYULTRA_COMPATIBILITY_MATRIX.map((entry) => (
                <article
                  key={entry.id}
                  className={
                    "dev-finding-card " + (entry.status === "native" ? "is-info" : entry.status === "coexists" ? "is-info" : "is-warning")
                  }
                >
                  <div className="dev-finding-card__top">
                    <span className={`dev-status-pill ${entry.status === "adapter-guided" ? "is-warning" : "is-info"}`}>
                      {entry.status.toUpperCase()}
                    </span>
                    <code>{entry.category.toUpperCase()}</code>
                    <strong>{entry.ecosystem}</strong>
                  </div>
                  <p>
                    <strong>Input:</strong> {entry.inputBehavior}
                  </p>
                  <p>
                    <strong>Layering:</strong> {entry.layeringBehavior}
                  </p>
                  <p>
                    <strong>Migration / Boundary:</strong> {entry.migrationPath}
                  </p>
                </article>
              ))}
            </div>
          </SkyFrame>

          <SkyFrame
            eyebrow="PORT PRISMAUI, MAGELIGHT & CEF MODS TO SKYULTRA"
            title="Legacy Mod Source & Manifest Migration Scanner"
          >
            <div style={{ display: "grid", gap: "10px", marginBottom: "14px" }}>
              <div style={{ display: "flex", gap: "10px", alignItems: "center", flexWrap: "wrap" }}>
                <label style={{ fontSize: "12px", opacity: 0.85 }} htmlFor="dev-migration-mod-id">
                  Target SkyUltra Mod ID:
                </label>
                <input
                  id="dev-migration-mod-id"
                  type="text"
                  className="settings-mod-input"
                  value={migrationModId}
                  onChange={(e) => setMigrationModId(e.currentTarget.value)}
                  style={{ maxWidth: "260px" }}
                />
                <button
                  type="button"
                  className="char-stage-btn char-stage-btn--sm"
                  onClick={() => {
                    const manifestJson = JSON.stringify(migrationAudit.suggestedManifest, null, 2);
                    if (navigator.clipboard) {
                      void navigator.clipboard.writeText(manifestJson);
                      setStatusNote(`Copied generated manifest.json (${migrationAudit.inferredPageRequests.length} permissions).`);
                    }
                  }}
                >
                  Copy Generated manifest.json
                </button>
              </div>
              <textarea
                rows={6}
                value={migrationSource}
                onChange={(e) => setMigrationSource(e.currentTarget.value)}
                aria-label="Legacy mod HTML or JavaScript source to audit"
                style={{
                  width: "100%",
                  fontFamily: "var(--sky-font-mono, monospace)",
                  fontSize: "12px",
                  padding: "10px 12px",
                  borderRadius: "6px",
                  border: "1px solid var(--sky-line-strong, rgba(232,176,89,0.32))",
                  background: "rgba(8, 9, 12, 0.78)",
                  color: "var(--sky-text, #f5efe6)",
                }}
              />
            </div>

            <div className="dev-findings-list">
              {migrationAudit.findings.map((finding, idx) => (
                <article key={`mig-${finding.code}-${idx}`} className={`dev-finding-card is-${finding.severity}`}>
                  <div className="dev-finding-card__top">
                    <span className={`dev-status-pill is-${finding.severity}`}>{finding.severity.toUpperCase()}</span>
                    <code>{finding.code}</code>
                    <strong>{finding.title}</strong>
                  </div>
                  <p>{finding.detail}</p>
                  {finding.fixSnippet && (
                    <div className="dev-finding-fix">
                      <div className="dev-finding-fix__head">
                        <span>SKYULTRA REPLACEMENT</span>
                      </div>
                      <pre>
                        <code>{finding.fixSnippet}</code>
                      </pre>
                    </div>
                  )}
                </article>
              ))}
            </div>
          </SkyFrame>
        </div>
      )}
    </div>
  );
}
