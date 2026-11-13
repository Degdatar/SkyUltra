import { useEffect, useRef, useState, type MutableRefObject } from "react";
import {
  getSharedGameSnapshot,
  readSkyUltraUiPrefs,
  subscribeSkyUltraGame,
  type SkyUltraGameTopic,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import type { ActorResponse, Npc, Palette, Player, Route, Run, SearchResponse } from "./model.js";
export function usePlayer(
  bridge: SkyWebViewAPI,
  busy: MutableRefObject<unknown>,
  scope: "full" | "hud" = "full",
  topics?: readonly SkyUltraGameTopic[],
) {
  const [player, setPlayer] = useState<Player | null>(() => getSharedGameSnapshot(bridge));
  const [status, setStatus] = useState(() =>
    getSharedGameSnapshot(bridge)?.available ? "Connected" : "Reading character data…",
  );
  const [pollMs, setPollMs] = useState<number>(() => readSkyUltraUiPrefs().pollIntervalMs);
  useEffect(() => {
    const onPrefs = (): void => setPollMs(readSkyUltraUiPrefs().pollIntervalMs);
    window.addEventListener("skyultra:ui-prefs", onPrefs);
    return () => window.removeEventListener("skyultra:ui-prefs", onPrefs);
  }, []);
  const topicsKey = topics ? topics.join(",") : "*";
  useEffect(() => {
    if (busy.current) return;
    const subscription = subscribeSkyUltraGame(
      bridge,
      (event) => {
        if (busy.current) return;
        if (!event.available) {
          setPlayer(null);
          setStatus("Character data is unavailable.");
          return;
        }
        setPlayer(event.snapshot);
        setStatus("Connected");
      },
      {
        scope,
        topics,
        intervalMs: scope === "hud" ? 10 : Math.max(250, pollMs),
      },
    );
    return () => subscription.unsubscribe();
  }, [bridge, busy, scope, pollMs, topicsKey]);
  return { player, status };
}
export function useActors(bridge: SkyWebViewAPI, route: Route, radius: number, mode: "nearby" | "followers" | "search", query: string) {
  const [actors, setActors] = useState<Npc[]>([]); const [count, setCount] = useState(0); const [status, setStatus] = useState("Scanning loaded actors…");
  const [results, setResults] = useState<Npc[]>([]); const [searchCount, setSearchCount] = useState(0); const [searchStatus, setSearchStatus] = useState("Enter at least 3 characters.");
  useEffect(() => {
    if (route !== "actors" || mode === "search") return; let active = true; let pending = false; let hostVisible = bridge?.getVisibility?.().visible ?? true; const controller = new AbortController();
    const refresh = async (): Promise<void> => { if (!active || pending || !hostVisible || document.hidden) return; pending = true; try { const data = await bridge.request<ActorResponse>("game.getNearbyNpcs", { radius }, { signal: controller.signal }); if (!active) return; if (!data?.available) { setActors([]); setCount(0); setStatus("The actor feed is unavailable."); return; } setActors(data.results ?? []); setCount(data.totalNearby ?? data.results?.length ?? 0); setStatus("Loaded actors · " + radius.toLocaleString() + " unit radius"); } catch (error) { if (active) { setActors([]); setCount(0); setStatus(error instanceof Error ? error.message : "Could not read nearby actors."); } } finally { pending = false; } };
    const unsubscribe = bridge.on<{visible:boolean}>("skyultra.visibility",event=>{hostVisible=event.visible;if(hostVisible)void refresh();}); void refresh(); const timer = window.setInterval(() => void refresh(), 4000); return () => { active = false; unsubscribe(); controller.abort(); window.clearInterval(timer); };
  }, [route, radius, mode, bridge]);
  useEffect(() => {
    if (route !== "actors" || mode !== "search") return; const name = query.trim(); if (name.length < 3) { setResults([]); setSearchCount(0); setSearchStatus("Enter at least 3 characters to search NPC records."); return; }
    let active = true; const controller = new AbortController(); setResults([]); setSearchCount(0); setSearchStatus("Searching NPC records…");
    const timer = window.setTimeout(async () => { try { const data = await bridge.request<SearchResponse>("game.searchNpcs", { query: name, limit: 40 }, { signal: controller.signal }); if (!active) return; setResults(data.results ?? []); setSearchCount(data.totalMatches ?? 0); setSearchStatus(!data?.available ? "NPC search is unavailable." : data.results?.length ? "Select an NPC record." : "No NPC names matched."); } catch (error) { if (active) setSearchStatus(error instanceof Error ? error.message : "NPC search is unavailable."); } }, 320);
    return () => { active = false; controller.abort(); window.clearTimeout(timer); };
  }, [route, mode, query, bridge]);
  return { actors, count, status, results, searchCount, searchStatus };
}
let runSequence=0;
function summarize(frames: number[], engine: string, items: number, theme: Palette): Run { const sorted = [...frames].sort((a, b) => a - b); const averageMs = frames.reduce((a, b) => a + b, 0) / Math.max(1, frames.length); return { id:String(++runSequence),engine, items, theme, viewport:{width:window.innerWidth,height:window.innerHeight,devicePixelRatio:window.devicePixelRatio||1}, averageFps: 1000 / Math.max(.01, averageMs), averageMs, p95Ms: sorted[Math.max(0, Math.ceil(sorted.length * .95) - 1)] ?? 0, slowFrames: frames.filter((frame) => frame > 16.7).length, frames: frames.length }; }
export function useCadence(bridge?: SkyWebViewAPI) {
  const [stats, setStats] = useState({ fps: 0, ms: 0, slow: 0, history: [] as number[] });
  const [running, setRunning] = useState(false); const [elapsed, setElapsed] = useState(0); const [runs, setRuns] = useState<Run[]>([]);
  const [preparing,setPreparing]=useState(false);
  const [notice, setNotice] = useState("");
  const hostVisibility=useRef(bridge?.getVisibility?.().visible ?? true);
  const runRef = useRef<{ start: number; frames: number[]; engine: string; items: number; theme: Palette; onComplete?(run:Run):void } | null>(null); const lastSecond = useRef(-1);
  useEffect(() => {
    let frame = 0; let previous = 0; let bucketStart = 0; let frames: number[] = []; let hostVisible = bridge?.getVisibility?.().visible ?? true;
    const syncTick = (): void => {
      const active = !document.hidden && hostVisible;
      if (active && !frame) {
        previous = 0;
        bucketStart = 0;
        frame = requestAnimationFrame(tick);
      } else if (!active && frame) {
        cancelAnimationFrame(frame);
        frame = 0;
      }
    };
    const visibility = (): void => {
      previous = 0; bucketStart = 0; frames = [];
      if ((document.hidden || !hostVisible) && runRef.current) {
        runRef.current = null; setRunning(false);
        setNotice("Capture cancelled because the page became hidden. Keep the same scene visible for the full run.");
      }
      syncTick();
    };
    const tick = (): void => {
      frame = 0;
      const now = performance.now();
      if (document.hidden || !hostVisible) { previous = 0; return; }
      if (previous) {
        const delta = now - previous;
        // Long visible-page stalls are part of the result, not discarded samples.
        if (Number.isFinite(delta) && delta > 0) {
          frames.push(delta);
          const run = runRef.current; if (run && previous >= run.start) run.frames.push(delta);
        }
      }
      previous = now; if (!bucketStart) bucketStart = now;
      if (now - bucketStart >= 1000) {
        // React may run an updater after this callback resets its rolling bucket.
        // Capture numeric samples now instead of closing over mutable counters.
        const interval = now - bucketStart;
        const ms = frames.reduce((a, b) => a + b, 0) / Math.max(1, frames.length);
        const measuredFps = interval > 0 ? frames.length * 1000 / interval : 0;
        const fps = Number.isFinite(measuredFps) ? measuredFps : 0;
        const slow = frames.filter((value) => value > 16.7).length;
        setStats((current) => {
          if (!runRef.current && current.history.length > 0 && Math.abs(current.fps - fps) < 10) {
            return current;
          }
          return { fps, ms, slow, history: [...current.history, ms].slice(-48) };
        });
        frames = []; bucketStart = now;
      }
      const run = runRef.current; if (run) { const second = Math.min(30, Math.floor((now - run.start) / 1000)); if (second !== lastSecond.current) { setElapsed(second); lastSecond.current = second; } if (now - run.start >= 30000) { runRef.current = null; setRunning(false); if (run.frames.length) { const result = summarize(run.frames, run.engine, run.items, run.theme); setRuns((current) => [result, ...current].slice(0, 6)); run.onComplete?.(result); setNotice("Capture completed. Compare the same scene and Skyrim frame times on the other backend."); } else { setNotice("No complete frame intervals were recorded. Try a new capture."); } } }
      frame = requestAnimationFrame(tick);
    };
    const runTimer = window.setInterval(() => {
      const run = runRef.current;
      if (!run) return;
      const now = performance.now();
      const second = Math.min(30, Math.floor((now - run.start) / 1000));
      if (second !== lastSecond.current) {
        setElapsed(second);
        lastSecond.current = second;
      }
      if (now - run.start >= 30000) {
        runRef.current = null;
        setRunning(false);
        if (run.frames.length) {
          const result = summarize(run.frames, run.engine, run.items, run.theme);
          setRuns((current) => [result, ...current].slice(0, 6));
          run.onComplete?.(result);
          setNotice("Capture completed. Compare the same scene and Skyrim frame times on the other backend.");
        } else {
          setNotice("No complete frame intervals were recorded. Try a new capture.");
        }
      }
    }, 250);
    document.addEventListener("visibilitychange", visibility);
    const resize=():void=>{if(!runRef.current)return;runRef.current=null;setRunning(false);setNotice("Capture cancelled because the viewport changed. Keep the same resolution for the full run.");};
    window.addEventListener("resize",resize);
    const unsubscribe = bridge?.on<{visible:boolean}>("skyultra.visibility",event=>{hostVisible=event.visible;hostVisibility.current=event.visible;visibility();});
    syncTick();
    return () => { window.clearInterval(runTimer); unsubscribe?.(); if (frame) cancelAnimationFrame(frame); document.removeEventListener("visibilitychange", visibility); window.removeEventListener("resize",resize); };
  }, [bridge]);
  return { stats, running, preparing, setPreparing, locked:running||preparing, elapsed, runs, runRef, notice, attachNative(id:string,native:NonNullable<Run["native"]>) { setRuns(current=>current.map(run=>run.id===id?{...run,native}:run)); }, start(engine: string, items: number, theme: Palette, onComplete?:(run:Run)=>void) { if (document.hidden || !hostVisibility.current || runRef.current) return; runRef.current = { start: performance.now(), frames: [], engine, items, theme, onComplete }; setNotice(""); setElapsed(0); lastSecond.current = 0; setRunning(true); }, cancel() { runRef.current = null; setRunning(false); setNotice("Capture cancelled. No partial run was saved."); } };
}
