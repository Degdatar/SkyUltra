import { useCallback, useEffect, useRef, useState } from "react";
import { readSkyUltraPerformance, type SkyUltraPerformanceSnapshot, type SkyWebViewAPI } from "@skyultra/sdk";

/** One lazy-page poll per second. Reads completed counters; it never asks the GPU to flush. */
export function useNativePerformance(bridge:SkyWebViewAPI,native:boolean) {
  const [snapshot,setSnapshot]=useState<SkyUltraPerformanceSnapshot|null>(null);
  const [error,setError]=useState("");
  const receivedAt=useRef(0);
  const active=useRef(false),hostVisible=useRef(true);
  const inFlight=useRef<Promise<SkyUltraPerformanceSnapshot>|null>(null);
  const controllers=useRef(new Set<AbortController>());
  const read=useCallback(async(fresh=false):Promise<SkyUltraPerformanceSnapshot>=>{
    if(inFlight.current){if(!fresh)return inFlight.current;await inFlight.current.catch(()=>undefined);}
    if(!native||!active.current)throw new Error("Native measurements are available inside Skyrim.");
    const controller=new AbortController();controllers.current.add(controller);
    const timeout=window.setTimeout(()=>controller.abort(),1500);
    const promise=readSkyUltraPerformance(bridge,controller.signal);
    inFlight.current=promise;
    try { const value=await promise;if(active.current){receivedAt.current=performance.now();setSnapshot(value);setError(value.available?"":"The native timing scope is not initialized on this renderer.");}return value; }
    catch(failure){if(active.current)setError(failure instanceof Error?failure.message:"Native timing request failed.");throw failure;}
    finally {window.clearTimeout(timeout);controllers.current.delete(controller);if(inFlight.current===promise)inFlight.current=null;}
  },[bridge,native]);
  useEffect(()=>{
    active.current=true;
    const poll=():void=>{if(native&&hostVisible.current&&!document.hidden&&!inFlight.current)void read().catch(()=>undefined);};
    const unsubscribe=bridge.on<{visible:boolean}>("skyultra.visibility",event=>{hostVisible.current=event.visible;if(event.visible)poll();});
    document.addEventListener("visibilitychange",poll);poll();
    const timer=window.setInterval(poll,1000);
    return()=>{active.current=false;unsubscribe();window.clearInterval(timer);document.removeEventListener("visibilitychange",poll);for(const controller of controllers.current)controller.abort();};
  },[bridge,native,read]);
  return {snapshot,error,read,receivedAtMs:receivedAt.current};
}
