import * as React from "react";
import { useEffect, useRef, useState } from "react";
import { compareSkyUltraPerformance, type SkyUltraPerformanceSnapshot, type SkyUltraTimingMetric, type SkyWebViewAPI } from "@skyultra/sdk";
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
const deltaValue=(metric:{meanMs:number|null}|undefined):string=>metric?.meanMs===null||metric?.meanMs===undefined?"—":metric.meanMs.toFixed(3)+" ms";
export default function PerformancePage({ showWorkload = true, bridge,native,cadence,engine,theme,openingFrameMs,openingDataMs,firstFrameMs,navigationFrameMs,dataFrameMs,detected,setPreviewEngine }: Props): React.JSX.Element {
  const [count,setCount]=useState(48);
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
    setReport(JSON.stringify({schemaVersion:1,kind:"skyultra-performance-report",createdAt:new Date().toISOString(),engine,engineDetected:detected,nativeHost:native,theme,currentViewport:{width:window.innerWidth,height:window.innerHeight,devicePixelRatio:window.devicePixelRatio||1},
      startup:{openingToDataMs:openingDataMs??null,openingToFrameMs:openingFrameMs??null,appModuleToShellMs:firstFrameMs,navigationToShellMs:navigationFrameMs,navigationToDataMs:dataFrameMs||null},
      scopes:{page:"requestAnimationFrame callback intervals",compositorGpu:"GPU composition after browser and actor rendering; all visible host views",hostGpu:"Work submitted on the host device; excludes CEF subprocess GPU rendering",callbackCpu:"Overlay render callback elapsed CPU time",presentInterval:"Main swap-chain Present call cadence; not GPU execution time"},
      snapshot:telemetry.snapshot,runs},null,2));setCopyStatus("");setReportOpen(true);
  };
  const copyReport=async():Promise<void>=>{
    try{if(navigator.clipboard?.writeText){await navigator.clipboard.writeText(report);setCopyStatus("Report copied.");return;}}catch{/* use editable selection fallback */}
    reportField.current?.focus();reportField.current?.select();
    try{if(document.execCommand("copy")){setCopyStatus("Report copied.");return;}}catch{/* host may not expose clipboard support */}
    setCopyStatus("Report selected. Use Ctrl+C to copy it from the text field.");
  };
  const snapshot=telemetry.snapshot,gpuAvailable=!!snapshot?.available&&snapshot.gpuTimingAvailable;
  const sinceSnapshot=telemetry.receivedAtMs?Math.max(0,performance.now()-telemetry.receivedAtMs):0;
  const timingNote=!native?"Native timing appears when this page runs inside Skyrim.":telemetry.error?telemetry.error:!snapshot?"Waiting for completed native measurements…":snapshot.compositorGpu.windowSamples?`${snapshot.compositorGpu.windowSamples} recent compositor samples · p95 ${snapshot.compositorGpu.p95Ms.toFixed(3)} ms · max ${snapshot.compositorGpu.maxMs.toFixed(3)} ms · ${snapshot.skippedGpuFrames} query slots missed · ${snapshot.invalidGpuSamples} invalid/disjoint samples`:gpuAvailable?"Waiting for completed GPU timestamp samples…":"GPU timestamps are unavailable; host CPU and Present cadence may still be measured.";
  const locked=running||preparing;
  return <div className="page performance-page">
    <div className="page-lead page-lead--compact"><div><span className="eyebrow">REPEATABLE SCENE · HONEST MEASUREMENTS</span><h1>The performance lab<span className="title-period">.</span></h1><p>Read native timing beside page cadence, then capture the same workload on each engine.</p></div><div className="button-row"><button className="control-button" onClick={exportReport} disabled={locked}>Export report</button><button className="primary-button" disabled={locked} onClick={()=>void begin()}><Icon name="performance" size={17}/>{preparing?"Reading native baseline…":running?"Capturing · "+elapsed+" / 30s":"Capture 30 seconds"}</button></div></div>
    <div className="performance-stats"><SkyStatTile label="Page RAF" value={stats.fps.toFixed(0)+" FPS"} hint="Callback cadence, not presented frames" icon={<Icon name="performance"/>}/><SkyStatTile label="Page mean interval" value={stats.ms.toFixed(2)+" ms"} hint={stats.slow+" intervals over 16.7ms this second"} icon={<Icon name="target"/>}/><SkyStatTile label="Opening → first page RAF" value={openingFrameMs===null||openingFrameMs===undefined?"Pending":openingFrameMs.toFixed(1)+" ms"} hint="Host visibility event → next page callback; not native presentation" icon={<Icon name="components"/>}/><SkyStatTile label="Navigation → first RAF" value={navigationFrameMs.toFixed(1)+" ms"} hint="Wall time; includes hidden prewarming and loading" icon={<Icon name="archive"/>}/></div>
    <section className="surface native-timing-surface"><SectionTitle eyebrow="COMPLETED HOST MEASUREMENTS" title="The cost of the interface"><span className="quiet-pill">{native?gpuAvailable?"D3D11 TIMESTAMPS":"NATIVE TIMING":"PREVIEW · NO NATIVE DATA"}</span></SectionTitle><div className="native-timing-grid"><SkyStatTile label="Compositor GPU" value={metricValue(snapshot?.compositorGpu,gpuAvailable,sinceSnapshot)} hint="UI composition after browser/actor rendering" icon={<Icon name="widgets"/>}/><SkyStatTile label="Host-device GPU" value={metricValue(snapshot?.hostGpu,gpuAvailable,sinceSnapshot)} hint="Host work; excludes CEF subprocess rendering" icon={<Icon name="target"/>}/><SkyStatTile label="Render callback CPU" value={metricValue(snapshot?.callbackCpu,!!snapshot?.available,sinceSnapshot)} hint="Elapsed host callback time, including waits" icon={<Icon name="components"/>}/><SkyStatTile label="Present call interval" value={metricValue(snapshot?.presentInterval,!!snapshot?.available,sinceSnapshot)} hint="Swap-chain call cadence, not GPU duration" icon={<Icon name="performance"/>}/></div>
      <p className="inspector-note">{timingNote}</p><p className="native-scope-note">GPU values cover all visible SkyUltra views on the host device. CEF renders its browser content in another process, which the host timer cannot measure. Compositor timing excludes browser rendering on both engines. Keep the same view/widget layout for comparisons.</p></section>
    <section className="surface cadence-surface"><div><span className="eyebrow">LAST 48 SECONDS</span><h3>Page frame interval</h3><span>{engine} · {detected?"Detected backend":"Preview label"}</span></div><SkySparkline values={stats.history} label="Recent page animation-frame intervals"/></section>
    <div className="measurement-note"><Icon name="target" size={18}/><p>Page RAF measures callbacks. Native GPU, host CPU, and Present intervals describe different scopes. Compare cold starts separately from warm opens, and use a game-frame profiler for whole-game performance.</p></div>
    {cadence.notice&&<p className="inspector-note" role="status">{cadence.notice}</p>}
    <section className="surface startup-ledger"><SectionTitle eyebrow="STARTUP MILESTONES" title="From navigation to live data"/><SkyDataList label="Startup milestones" rows={[{id:"opening",label:"Most recent opening → page callback",value:openingFrameMs===null||openingFrameMs===undefined?"Pending":openingFrameMs.toFixed(1)+" ms",meta:"Host visibility → next RAF, separate from cold start"},{id:"openingData",label:"Most recent opening → live-data callback",value:openingDataMs==null?"Pending":openingDataMs.toFixed(1)+" ms",meta:"Host visibility → frame with player data; cached data counts on warm opens"},{id:"shell",label:"App module → shell frame",value:firstFrameMs.toFixed(1)+" ms",meta:"Wall time since module evaluation; can include hidden idle"},{id:"navigation",label:"Navigation → shell frame",value:navigationFrameMs.toFixed(1)+" ms",meta:"Wall time; includes hidden prewarming"},{id:"data",label:"Navigation → live-data frame",value:dataFrameMs?dataFrameMs.toFixed(1)+" ms":"Pending",meta:"Wall time; includes hidden prewarming, title screen and loading the save"}]}/></section>
    {showWorkload&&<section className="surface stress-surface"><SectionTitle eyebrow="THE FIXED WORKLOAD" title="Inventory density"><div className="stress-controls"><SkySegmentedControl label="Stress-grid item count" value={String(count)} onValueChange={value=>{if(!locked)setCount(Number(value));}} options={[24,48,128,240].map(value=>({value:String(value),label:String(value),disabled:locked}))}/>{!detected&&<SkyChoice label="Preview capture label" value={engine} disabled={locked} onValueChange={setPreviewEngine} options={[{value:"Ultralight",label:"Ultralight"},{value:"CEF",label:"CEF"}]}/>}{running&&<button className="control-button" onClick={cadence.cancel}>Cancel</button>}</div></SectionTitle><div className="stress-grid">{Array.from({length:count},(_,index)=><button key={index} className="stress-tile" aria-label={"Inspect synthetic relic "+(index+1)} onClick={event=>{const selected=event.currentTarget.classList.toggle("is-active");event.currentTarget.setAttribute("aria-pressed",String(selected));}}><Icon name={["sword","shield","flame","gold","rune","book"][index%6] as IconName} size={30}/><strong>{["Frostfang","Dawnward","Ember tonic","Moonstone","Aether seal","The northern road"][index%6]}</strong><small>SYNTHETIC · RECORD {String(index+1).padStart(3,"0")}</small></button>)}</div></section>}
    {runs.length>0&&<section className="surface run-surface"><SectionTitle eyebrow="THIS SESSION" title="Capture ledger"/><table className="run-table"><thead><tr><th>Scene</th><th>Page RAF</th><th>Page P95</th><th>Compositor GPU</th><th>Callback CPU</th><th>Present interval</th><th>Page samples</th></tr></thead><tbody>{runs.map(run=><tr key={run.id}><td>{run.engine}<small>{run.items} items · {run.theme}</small><small>{run.native?run.native.available?`${(run.native.durationMs/1000).toFixed(2)}s native window · ${run.native.compositorGpu.samples} GPU samples`:run.native.reason:"Reading native capture…"}</small></td><td>{run.averageFps.toFixed(1)} FPS<small>{run.averageMs.toFixed(2)} ms mean</small></td><td>{run.p95Ms.toFixed(2)} ms<small>{run.slowFrames} slow intervals</small></td><td>{run.native?.available?deltaValue(run.native.compositorGpu):"—"}</td><td>{run.native?.available?deltaValue(run.native.callbackCpu):"—"}</td><td>{run.native?.available?deltaValue(run.native.presentInterval):"—"}</td><td>{run.frames}</td></tr>)}</tbody></table></section>}
    <SkyPopup open={reportOpen} title="Performance report" description="Copy this JSON into a local file to compare runs. It records timing scopes and sample counts alongside the page workload." wide onDismiss={()=>setReportOpen(false)} actions={<><span className="inspector-note" role="status">{copyStatus}</span><button className="control-button" onClick={()=>setReportOpen(false)}>Close</button><button className="primary-button" onClick={()=>void copyReport()}>Copy report</button></>}><textarea ref={reportField} className="performance-report" aria-label="JSON performance report" readOnly value={report} spellCheck={false}/></SkyPopup>
  </div>;
}
