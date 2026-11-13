import type { SkyWebViewAPI } from "./index.js";

export interface SkyUltraTimingMetric {
  samples: number;
  totalMs: number;
  lastMs: number;
  windowSamples: number;
  meanMs: number;
  p95Ms: number;
  maxMs: number;
  ageMs: number | null;
}

/** Completed host measurements. capturedAtMs uses the host's monotonic clock. */
export interface SkyUltraPerformanceSnapshot {
  schemaVersion: 1;
  available: boolean;
  scope: "host-overlay";
  generation: number;
  capturedAtMs: number;
  gpuTimingAvailable: boolean;
  skippedGpuFrames: number;
  invalidGpuSamples: number;
  hostGpu: SkyUltraTimingMetric;
  compositorGpu: SkyUltraTimingMetric;
  callbackCpu: SkyUltraTimingMetric;
  presentInterval: SkyUltraTimingMetric;
}

export interface SkyUltraTimingDelta { samples: number; meanMs: number | null; }
export type SkyUltraNativeCapture = { available: false; reason: string } | {
  available: true;
  generation: number;
  durationMs: number;
  gpuTimingAvailable: boolean;
  skippedGpuFrames: number;
  invalidGpuSamples: number;
  hostGpu: SkyUltraTimingDelta;
  compositorGpu: SkyUltraTimingDelta;
  callbackCpu: SkyUltraTimingDelta;
  presentInterval: SkyUltraTimingDelta;
};

/** Requires ui.getPerformanceSnapshot in the manifest and per-view allowlist. */
export async function readSkyUltraPerformance(bridge: SkyWebViewAPI, signal?: AbortSignal): Promise<SkyUltraPerformanceSnapshot> {
  const value=await bridge.request<SkyUltraPerformanceSnapshot>("ui.getPerformanceSnapshot", {}, { signal });
  const metric=(entry:SkyUltraTimingMetric|undefined):boolean=>!!entry &&
    Number.isSafeInteger(entry.samples)&&entry.samples>=0&&Number.isFinite(entry.totalMs)&&entry.totalMs>=0&&
    Number.isSafeInteger(entry.windowSamples)&&entry.windowSamples>=0&&entry.windowSamples<=512&&
    [entry.lastMs,entry.meanMs,entry.p95Ms,entry.maxMs].every(number=>Number.isFinite(number)&&number>=0)&&
    (entry.ageMs===null||(Number.isFinite(entry.ageMs)&&entry.ageMs>=0));
  if (!value || value.schemaVersion!==1 || value.scope!=="host-overlay" || typeof value.available!=="boolean" ||
      !Number.isSafeInteger(value.generation)||value.generation<0||!Number.isFinite(value.capturedAtMs)||
      typeof value.gpuTimingAvailable!=="boolean"||!Number.isSafeInteger(value.skippedGpuFrames)||value.skippedGpuFrames<0||
      !Number.isSafeInteger(value.invalidGpuSamples)||value.invalidGpuSamples<0||
      !metric(value.hostGpu)||!metric(value.compositorGpu)||!metric(value.callbackCpu)||!metric(value.presentInterval))
    throw new Error("Unsupported native performance response. Update the DLL, quest script, and web manifest together.");
  return value;
}

/** Exact means from cumulative completed-sample counters, not from averaging poll averages. */
export function compareSkyUltraPerformance(before: SkyUltraPerformanceSnapshot, after: SkyUltraPerformanceSnapshot): SkyUltraNativeCapture {
  const unavailable = (reason: string): SkyUltraNativeCapture => ({ available: false, reason });
  if (!before.available || !after.available || before.schemaVersion !== 1 || after.schemaVersion !== 1 ||
      before.scope !== "host-overlay" || after.scope !== "host-overlay") return unavailable("Native timing is unavailable for this capture.");
  if (before.generation !== after.generation || before.generation === 0) return unavailable("The graphics device changed during the capture; repeat the run.");
  const durationMs = after.capturedAtMs - before.capturedAtMs;
  if (!Number.isFinite(durationMs) || durationMs <= 0) return unavailable("The native timing window did not advance.");
  const delta = (start: SkyUltraTimingMetric, end: SkyUltraTimingMetric): SkyUltraTimingDelta | null => {
    if (!start || !end) return null;
    const samples = end.samples - start.samples, total = end.totalMs - start.totalMs;
    if (!Number.isSafeInteger(samples) || samples < 0 || !Number.isFinite(total) || total < 0) return null;
    return { samples, meanMs: samples > 0 ? total / samples : null };
  };
  const hostGpu=delta(before.hostGpu,after.hostGpu),compositorGpu=delta(before.compositorGpu,after.compositorGpu);
  const callbackCpu=delta(before.callbackCpu,after.callbackCpu),presentInterval=delta(before.presentInterval,after.presentInterval);
  if (!hostGpu || !compositorGpu || !callbackCpu || !presentInterval) return unavailable("Native timing counters reset during the capture.");
  return { available:true,generation:after.generation,durationMs,gpuTimingAvailable:before.gpuTimingAvailable&&after.gpuTimingAvailable,
    skippedGpuFrames:Math.max(0,after.skippedGpuFrames-before.skippedGpuFrames),invalidGpuSamples:Math.max(0,after.invalidGpuSamples-before.invalidGpuSamples),
    hostGpu,compositorGpu,callbackCpu,presentInterval };
}
