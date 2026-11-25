import * as React from "react";
import { useEffect, useRef, useState } from "react";
import { compareSkyUltraPerformance, type SkyUltraPercentileSummary, type SkyUltraPerformanceSnapshot, type SkyUltraTimingMetric, type SkyWebViewAPI } from "@skyultra/sdk";
import { SkyChoice, SkyDataList, SkySegmentedControl, SkySparkline, SkyStatTile } from "../core.js";
import { SkyPopup } from "../widgets.js";
import { Icon, type IconName } from "./icons.js";
import { SectionTitle } from "./pieces.js";
import type { Palette } from "./model.js";
import type { useCadence } from "./useFieldData.js";
import { useNativePerformance } from "./useNativePerformance.js";

interface Props {
  showWorkload?:boolean;
  bridge:SkyWebViewAPI;
  native:boolean;
  cadence: ReturnType<typeof useCadence>;
  engine: string;
  theme: Palette;
  openingFrameMs?:number|null;
  openingDataMs?:number|null;
  firstFrameMs: number;
  navigationFrameMs: number;
  dataFrameMs: number;
  detected: boolean;
  setPreviewEngine(value: string): void;
}
const metricValue=(metric:SkyUltraTimingMetric|undefined,available:boolean,elapsed:number):string=>!available?"Unavailable":!metric?.windowSamples?"Pending":metric.ageMs===null||metric.ageMs+elapsed>2500?"Stale":metric.meanMs.toFixed(3)+" ms";
const metricPercentiles=(metric:SkyUltraTimingMetric|undefined,baseHint:string):string=>metric?.windowSamples?`${baseHint} · p50 ${(metric.p50Ms??metric.meanMs).toFixed(2)} / p95 ${metric.p95Ms.toFixed(2)} / p99 ${(metric.p99Ms??metric.maxMs).toFixed(2)} ms`:baseHint;
const deltaValue=(metric:{meanMs:number|null}|undefined):string=>metric?.meanMs===null||metric?.meanMs===undefined?"—":metric.meanMs.toFixed(3)+" ms";
const fmtMs=(value:number,samples:number):string=>samples?value.toFixed(2)+" ms":"—";
const formatCompactPixels=(px:number):string=>px>=1_000_000?(px/1_000_000).toFixed(1)+"M px":px>=1_000?(px/1_000).toFixed(1)+"K px":px+" px";
export default function PerformancePage({ showWorkload = true, bridge,native,cadence,engine,theme,openingFrameMs,openingDataMs,firstFrameMs,navigationFrameMs,dataFrameMs,detected,setPreviewEngine }: Props): React.JSX.Element {
  const [count,setCount]=useState(48);
  const [tracesOpen,setTracesOpen]=useState(false);
  const [reportOpen,setReportOpen]=useState(false),[report,setReport]=useState(""),[copyStatus,setCopyStatus]=useState("");
  const reportField=useRef<HTMLTextAreaElement>(null),mounted=useRef(false);
  const telemetry=useNativePerformance(bridge,native);
  const {running,preparing,setPreparing,stats,elapsed,runs}=cadence;
  useEffect(()=>{mounted.current=true;return()=>{mounted.current=false;setPreparing(false);};},[setPreparing]);
  const begin=async():Promise<void>=>{
    if(running||preparing)return;
    setPreparing(true);
    let baseline:SkyUltraPerformanceSnapshot|null=null;
    let baselineError=native?"Native measurements were unavailable at capture start.":"Browser preview has no native GPU measurements.";
    if(native)try{baseline=await telemetry.read(true);}catch(error){baselineError=error instanceof Error?error.message:baselineError;}
    if(!mounted.current)return;
    setPreparing(false);
    cadence.start(engine,showWorkload?count:0,theme,result=>{
      if(!baseline){cadence.attachNative(result.id,{available:false,reason:baselineError});return;}
      const start=baseline;
      void telemetry.read(true).then(end=>cadence.attachNative(result.id,compareSkyUltraPerformance(start,end))).catch(error=>cadence.attachNative(result.id,{available:false,reason:error instanceof Error?error.message:"Native measurements were unavailable at capture end."}));
    });
  };
  const exportReport=():void=>{
    const ft=telemetry.snapshot?.frameTrace??null;
    setReport(JSON.stringify({schemaVersion:1,kind:"skyultra-performance-report",createdAt:new Date().toISOString(),engine,engineDetected:detected,nativeHost:native,theme,currentViewport:{width:window.innerWidth,height:window.innerHeight,devicePixelRatio:window.devicePixelRatio||1},
      startup:{openingToDataMs:openingDataMs??null,openingToFrameMs:openingFrameMs??null,appModuleToShellMs:firstFrameMs,navigationToShellMs:navigationFrameMs,navigationToDataMs:dataFrameMs||null},
      scopes:{page:"requestAnimationFrame callback intervals",compositorGpu:"GPU composition after browser and actor rendering; all visible host views",hostGpu:"Work submitted on the host device; excludes CEF subprocess GPU rendering",callbackCpu:"Overlay render callback elapsed CPU time",presentInterval:"Main swap-chain Present call cadence; not GPU execution time",frameTrace:"Sampled request through native queue, browser dispatch, paint, and first Skyrim composite"},
      benchmarkGuidance:ft?.benchmarkGuidance??{presentMonUrl:"https://github.com/GameTechDev/PresentMon",windowsTraceNote:"Pair SkyUltra sampled traces with PresentMon CSV (msBetweenPresents / msUntilDisplayed) and WPR GeneralProfile + GPU ETW traces so CEF subprocess work and WDDM queue wait appear alongside SkyUltra timestamps.",commandExample:"PresentMon.exe -process_name SkyrimSE.exe -output_file skyultra_presentmon.csv -timed 30"},
      snapshot:telemetry.snapshot,frameTrace:ft,runs},null,2));setCopyStatus("");setReportOpen(true);
  };
  const copyReport=async():Promise<void>=>{
    try{if(navigator.clipboard?.writeText){await navigator.clipboard.writeText(report);setCopyStatus("Report copied.");return;}}catch{/* use editable selection fallback */}
    reportField.current?.focus();reportField.current?.select();
    try{if(document.execCommand("copy")){setCopyStatus("Report copied.");return;}}catch{/* host may not expose clipboard support */}
    setCopyStatus("Report selected. Use Ctrl+C to copy it from the text field.");
  };
  const snapshot=telemetry.snapshot,gpuAvailable=!!snapshot?.available&&snapshot.gpuTimingAvailable;
  const sinceSnapshot=telemetry.receivedAtMs?Math.max(0,performance.now()-telemetry.receivedAtMs):0;
  const timingNote=!native?"Native timing appears when this page runs inside Skyrim.":telemetry.error?telemetry.error:!snapshot?"Waiting for completed native measurements…":snapshot.compositorGpu.windowSamples?`${snapshot.compositorGpu.windowSamples} recent compositor samples · p50 ${(snapshot.compositorGpu.p50Ms??snapshot.compositorGpu.meanMs).toFixed(3)} ms · p95 ${snapshot.compositorGpu.p95Ms.toFixed(3)} ms · p99 ${(snapshot.compositorGpu.p99Ms??snapshot.compositorGpu.maxMs).toFixed(3)} ms · max ${snapshot.compositorGpu.maxMs.toFixed(3)} ms · ${snapshot.skippedGpuFrames} query slots missed · ${snapshot.invalidGpuSamples} invalid/disjoint samples`:gpuAvailable?"Waiting for completed GPU timestamp samples…":"GPU timestamps are unavailable; host CPU and Present cadence may still be measured.";
  const frameTrace=snapshot?.frameTrace;
  const recentTraces=frameTrace?.traces?frameTrace.traces.slice(-16).reverse():[];
  const pipelineRows: Array<{ id: string; label: string; summary: SkyUltraPercentileSummary }> = frameTrace ? [
    { id: "warmMenuOpen", label: "Warm menu open", summary: frameTrace.summary.warmMenuOpen },
    { id: "coldMenuOpen", label: "Cold menu open", summary: frameTrace.summary.coldMenuOpen },
    { id: "inputClick", label: "Input · click", summary: frameTrace.summary.inputClick },
    { id: "inputWheel", label: "Input · wheel", summary: frameTrace.summary.inputWheel },
    { id: "inputKey", label: "Input · key", summary: frameTrace.summary.inputKey },
    { id: "inputFocus", label: "Input · focus", summary: frameTrace.summary.inputFocus },
  ] : [];
  const locked=running||preparing;
  return <div className="page performance-page">
    <div className="page-lead page-lead--compact"><div><span className="eyebrow">REPEATABLE SCENE · HONEST MEASUREMENTS</span><h1>The performance lab<span className="title-period">.</span></h1><p>Read native timing beside page cadence, then capture the same workload on each engine.</p></div><div className="button-row"><button className="control-button" onClick={exportReport} disabled={locked}>Export report</button><button className="primary-button" disabled={locked} onClick={()=>void begin()}><Icon name="performance" size={17}/>{preparing?"Reading native baseline…":running?"Capturing · "+elapsed+" / 30s":"Capture 30 seconds"}</button></div></div>
    <div className="performance-stats"><SkyStatTile label="Page RAF" value={stats.fps.toFixed(0)+" FPS"} hint="Callback cadence, not presented frames" icon={<Icon name="performance"/>}/><SkyStatTile label="Page mean interval" value={stats.ms.toFixed(2)+" ms"} hint={stats.slow+" intervals over 16.7ms this second"} icon={<Icon name="target"/>}/><SkyStatTile label="Opening → first page RAF" value={openingFrameMs===null||openingFrameMs===undefined?"Pending":openingFrameMs.toFixed(1)+" ms"} hint="Host visibility event → next page callback; not native presentation" icon={<Icon name="components"/>}/><SkyStatTile label="Navigation → first RAF" value={navigationFrameMs.toFixed(1)+" ms"} hint="Wall time; includes hidden prewarming and loading" icon={<Icon name="archive"/>}/></div>
    <section className="surface native-timing-surface"><SectionTitle eyebrow="COMPLETED HOST MEASUREMENTS" title="The cost of the interface"><span className="quiet-pill">{native?gpuAvailable?"D3D11 TIMESTAMPS":"NATIVE TIMESTAMPS":"PREVIEW · NO NATIVE DATA"}</span></SectionTitle><div className="native-timing-grid"><SkyStatTile label="Compositor GPU" value={metricValue(snapshot?.compositorGpu,gpuAvailable,sinceSnapshot)} hint={metricPercentiles(snapshot?.compositorGpu,"UI composition after browser/actor rendering")} icon={<Icon name="widgets"/>}/><SkyStatTile label="Host-device GPU" value={metricValue(snapshot?.hostGpu,gpuAvailable,sinceSnapshot)} hint={metricPercentiles(snapshot?.hostGpu,"Host work; excludes CEF subprocess rendering")} icon={<Icon name="target"/>}/><SkyStatTile label="Render callback CPU" value={metricValue(snapshot?.callbackCpu,!!snapshot?.available,sinceSnapshot)} hint={metricPercentiles(snapshot?.callbackCpu,"Elapsed host callback time, including waits")} icon={<Icon name="components"/>}/><SkyStatTile label="Present call interval" value={metricValue(snapshot?.presentInterval,!!snapshot?.available,sinceSnapshot)} hint={metricPercentiles(snapshot?.presentInterval,"Swap-chain call cadence, not GPU duration")} icon={<Icon name="performance"/>}/></div>
      <p className="inspector-note">{timingNote}</p><p className="native-scope-note">GPU values cover all visible SkyUltra views on the host device. CEF renders its browser content in another process, which the host timer cannot measure. Compositor timing excludes browser rendering on both engines. Keep the same view/widget layout for comparisons.</p></section>
    {frameTrace&&<section className="surface telemetry-surface"><SectionTitle eyebrow="END-TO-END FRAME TRACE · P50 / P95 / P99" title="Sampled pipeline & subsystem telemetry"><div className="telemetry-header-actions"><span className="quiet-pill">{frameTrace.thumbnails.avx2Active?"AVX2 ACTIVE":"SCALAR"}</span>{recentTraces.length>0&&<button type="button" className="control-button telemetry-toggle" aria-expanded={tracesOpen} onClick={()=>setTracesOpen(open=>!open)}>{tracesOpen?"Hide":"Show"} traces ({recentTraces.length})</button>}</div></SectionTitle>
      <div className="telemetry-kpi-grid">
        <SkyStatTile label="CEF D3D11 copies" value={`${frameTrace.cefCopy.dirtyCopies.toLocaleString()} dirty / ${frameTrace.cefCopy.fullCopies.toLocaleString()} full`} hint={`${frameTrace.cefCopy.dirtyFallbacks} fallbacks · ${formatCompactPixels(frameTrace.cefCopy.overlappingPixelsCopied)} overlap · age p50 ${fmtMs(frameTrace.cefCopy.frameAge.p50Ms,frameTrace.cefCopy.frameAge.samples)} / p95 ${fmtMs(frameTrace.cefCopy.frameAge.p95Ms,frameTrace.cefCopy.frameAge.samples)}`} icon={<Icon name="widgets"/>}/>
        <SkyStatTile label="Ultralight replay" value={`${frameTrace.ultralightReplay.packetsReplayed.toLocaleString()} packets`} hint={`${frameTrace.ultralightReplay.backpressureWaits} waits · avg ${frameTrace.ultralightReplay.avgCommandLists.toFixed(1)} / max ${frameTrace.ultralightReplay.maxCommandLists} lists · replay p95 ${fmtMs(frameTrace.ultralightReplay.replay.p95Ms,frameTrace.ultralightReplay.replay.samples)}`} icon={<Icon name="components"/>}/>
        <SkyStatTile label="Native game queue" value={`${frameTrace.nativeQueue.tasksExecuted.toLocaleString()} tasks`} hint={`${frameTrace.nativeQueue.budgetOverruns} overruns · wait p50 ${fmtMs(frameTrace.nativeQueue.wait.p50Ms,frameTrace.nativeQueue.wait.samples)} / p95 ${fmtMs(frameTrace.nativeQueue.wait.p95Ms,frameTrace.nativeQueue.wait.samples)} · exec p95 ${fmtMs(frameTrace.nativeQueue.exec.p95Ms,frameTrace.nativeQueue.exec.samples)}`} icon={<Icon name="performance"/>}/>
        <SkyStatTile label="Thumbnail pipeline" value={`${frameTrace.thumbnails.urlHits} URL · ${frameTrace.thumbnails.gpuCaptures} GPU`} hint={`${frameTrace.thumbnails.diskHits} disk · ${frameTrace.thumbnails.memoryHits} mem · convert p50 ${fmtMs(frameTrace.thumbnails.pixelConvert.p50Ms,frameTrace.thumbnails.pixelConvert.samples)} · encode p50 ${fmtMs(frameTrace.thumbnails.encode.p50Ms,frameTrace.thumbnails.encode.samples)}`} icon={<Icon name="target"/>}/>
      </div>
      {frameTrace.nativeQueue.slowestTask&&<p className="telemetry-slowest-note">Slowest game-thread task: {frameTrace.nativeQueue.slowestTask} ({frameTrace.nativeQueue.slowestMod||"host"}) · {frameTrace.nativeQueue.slowestExecMs.toFixed(2)} ms</p>}
      <div className="telemetry-table-wrap"><table className="run-table telemetry-matrix"><thead><tr><th>Pipeline</th><th>Samples</th><th>Mean</th><th>P50</th><th>P95</th><th>P99</th><th>Max</th></tr></thead><tbody>{pipelineRows.map(row=><tr key={row.id}><td>{row.label}</td><td>{row.summary.samples.toLocaleString()}</td><td>{fmtMs(row.summary.meanMs,row.summary.samples)}</td><td>{fmtMs(row.summary.p50Ms,row.summary.samples)}</td><td>{fmtMs(row.summary.p95Ms,row.summary.samples)}</td><td>{fmtMs(row.summary.p99Ms,row.summary.samples)}</td><td>{fmtMs(row.summary.maxMs,row.summary.samples)}</td></tr>)}</tbody></table></div>
      {tracesOpen&&recentTraces.length>0&&<div className="telemetry-traces-scroll"><table className="run-table"><thead><tr><th>Kind</th><th>Engine / View</th><th>Queue wait</th><th>Dispatch → paint</th><th>Paint → composite</th><th>Total</th></tr></thead><tbody>{recentTraces.map(item=><tr key={item.id}><td>{item.kind}<small>{item.visible?"visible":"hidden"}{item.splashActive?" · splash":""}</small></td><td>{item.engine||"—"} · #{item.viewId}<small>{item.modId||"host"} · paint #{item.paintSequence} → frame #{item.compositeFrameSequence}</small></td><td>{item.queueWaitMs.toFixed(2)} ms</td><td>{item.dispatchToPaintMs.toFixed(2)} ms</td><td>{item.paintToCompositeMs.toFixed(2)} ms</td><td>{item.totalMs.toFixed(2)} ms</td></tr>)}</tbody></table></div>}
      <div className="telemetry-benchmark"><span>{frameTrace.benchmarkGuidance.windowsTraceNote}</span><code>{frameTrace.benchmarkGuidance.commandExample}</code></div>
    </section>}
    <section className="surface cadence-surface"><div><span className="eyebrow">LAST 48 SECONDS</span><h3>Page frame interval</h3><span>{engine} · {detected?"Detected backend":"Preview label"}</span></div><SkySparkline values={stats.history} label="Recent page animation-frame intervals"/></section>
    <div className="measurement-note"><Icon name="target" size={18}/><p>Page RAF measures callbacks. Native GPU, host CPU, and Present intervals describe different scopes. Compare cold starts separately from warm opens, and use a game-frame profiler for whole-game performance.</p></div>
    {cadence.notice&&<p className="inspector-note" role="status">{cadence.notice}</p>}
    <section className="surface startup-ledger"><SectionTitle eyebrow="STARTUP MILESTONES" title="From navigation to live data"/><SkyDataList label="Startup milestones" rows={[{id:"opening",label:"Most recent opening → page callback",value:openingFrameMs===null||openingFrameMs===undefined?"Pending":openingFrameMs.toFixed(1)+" ms",meta:"Host visibility → next RAF, separate from cold start"},{id:"openingData",label:"Most recent opening → live-data callback",value:openingDataMs==null?"Pending":openingDataMs.toFixed(1)+" ms",meta:"Host visibility → frame with player data; cached data counts on warm opens"},{id:"shell",label:"App module → shell frame",value:firstFrameMs.toFixed(1)+" ms",meta:"Wall time since module evaluation; can include hidden idle"},{id:"navigation",label:"Navigation → shell frame",value:navigationFrameMs.toFixed(1)+" ms",meta:"Wall time; includes hidden prewarming"},{id:"data",label:"Navigation → live-data frame",value:dataFrameMs?dataFrameMs.toFixed(1)+" ms":"Pending",meta:"Wall time; includes hidden prewarming, title screen and loading the save"}]}/></section>
    {showWorkload&&<section className="surface stress-surface"><SectionTitle eyebrow="THE FIXED WORKLOAD" title="Inventory density"><div className="stress-controls"><SkySegmentedControl label="Stress-grid item count" value={String(count)} onValueChange={value=>{if(!locked)setCount(Number(value));}} options={[24,48,128,240].map(value=>({value:String(value),label:String(value),disabled:locked}))}/>{!detected&&<SkyChoice label="Preview capture label" value={engine} disabled={locked} onValueChange={setPreviewEngine} options={[{value:"Ultralight",label:"Ultralight"},{value:"CEF",label:"CEF"}]}/>}{running&&<button className="control-button" onClick={cadence.cancel}>Cancel</button>}</div></SectionTitle><div className="stress-grid">{Array.from({length:count},(_,index)=><button key={index} className="stress-tile" aria-label={"Inspect synthetic relic "+(index+1)} onClick={event=>{const selected=event.currentTarget.classList.toggle("is-active");event.currentTarget.setAttribute("aria-pressed",String(selected));}}><Icon name={["sword","shield","flame","gold","rune","book"][index%6] as IconName} size={30}/><strong>{["Frostfang","Dawnward","Ember tonic","Moonstone","Aether seal","The northern road"][index%6]}</strong><small>SYNTHETIC · RECORD {String(index+1).padStart(3,"0")}</small></button>)}</div></section>}
    {runs.length>0&&<section className="surface run-surface"><SectionTitle eyebrow="THIS SESSION" title="Capture ledger"/><table className="run-table"><thead><tr><th>Scene</th><th>Page RAF</th><th>Page P95</th><th>Compositor GPU</th><th>Callback CPU</th><th>Present interval</th><th>Page samples</th></tr></thead><tbody>{runs.map(run=><tr key={run.id}><td>{run.engine}<small>{run.items} items · {run.theme}</small><small>{run.native?run.native.available?`${(run.native.durationMs/1000).toFixed(2)}s native window · ${run.native.compositorGpu.samples} GPU samples`:run.native.reason:"Reading native capture…"}</small></td><td>{run.averageFps.toFixed(1)} FPS<small>{run.averageMs.toFixed(2)} ms mean</small></td><td>{run.p95Ms.toFixed(2)} ms<small>{run.slowFrames} slow intervals</small></td><td>{run.native?.available?deltaValue(run.native.compositorGpu):"—"}</td><td>{run.native?.available?deltaValue(run.native.callbackCpu):"—"}</td><td>{run.native?.available?deltaValue(run.native.presentInterval):"—"}</td><td>{run.frames}</td></tr>)}</tbody></table></section>}
    <SkyPopup open={reportOpen} title="Performance report" description="Copy this JSON into a local file to compare runs. It records timing scopes and sample counts alongside the page workload." wide onDismiss={()=>setReportOpen(false)} actions={<><span className="inspector-note" role="status">{copyStatus}</span><button className="control-button" onClick={()=>setReportOpen(false)}>Close</button><button className="primary-button" onClick={()=>void copyReport()}>Copy report</button></>}><textarea ref={reportField} className="performance-report" aria-label="JSON performance report" readOnly value={report} spellCheck={false}/></SkyPopup>
  </div>;
}
