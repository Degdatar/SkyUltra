import * as React from "react";
import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { createSkyUltraRenderer, createSkyUltraItemRenderer, type SkyWebViewAPI, type SkyUltraPreviewLighting, type SkyUltraActorAnimationClip, type SkyUltraActorPreviewOptions, type SkyUltraRequestOptions } from "@skyultra/sdk";
import { SkySegmentedControl, SkyChoice } from "../core.js";
import { ActorAnimationTimeline } from "./ActorAnimationTimeline.js";
import { defaultActorCamera, dragActorCamera, wrapAngleRadians } from "./actorCamera.js";
import { Icon } from "./icons.js";
import { previewNavigationEvent } from "./previewLifecycle.js";
import type { Npc, Item } from "./model.js";
interface PreviewStatus { available: boolean; ready: boolean; geometryCount?: number; skippedGeometryCount?: number; message?: string; posesAvailable?: boolean; }
export function ActorStage({ actor, bridge, obscured, onCapturePortrait }: { actor: Npc; bridge: SkyWebViewAPI; obscured: boolean; onCapturePortrait?: () => Promise<string | void> | string | void }): React.JSX.Element {
  return <ModelStage subject={{kind:'actor',actor}} bridge={bridge} obscured={obscured} onCapturePortrait={onCapturePortrait} />;
}
type ModelSubject = {kind:'actor';actor:Npc} | {kind:'item';item:Item};
export function ModelStage({ subject, bridge, obscured, equipment = false, initialMotion = true, hideStudioControls = false, refreshToken, onCapturePortrait }: { subject: ModelSubject; bridge: SkyWebViewAPI; obscured: boolean; equipment?: boolean; initialMotion?: boolean; hideStudioControls?: boolean; refreshToken?: number; onCapturePortrait?: () => Promise<string | void> | string | void }): React.JSX.Element {
  const actorRenderer = React.useMemo(()=>createSkyUltraRenderer(bridge),[bridge]);
  const itemRenderer = React.useMemo(()=>createSkyUltraItemRenderer(bridge),[bridge]);
  const isItem = subject.kind === 'item';
  const renderer = isItem ? itemRenderer : actorRenderer;
  const initialCamera = () => ({...defaultActorCamera(), ...(isItem ? {yaw:.5,pitch:.35} : {})});
  const setPreview = (options: SkyUltraActorPreviewOptions, request?: SkyUltraRequestOptions) => subject.kind === 'item'
    ? itemRenderer.setItem({...options,formId:subject.item.id},request)
    : actorRenderer.setActor({...options,...(equipment ? {ignoreWorldFade:true} : {}),...(subject.actor.referenceId ? {referenceId:subject.actor.referenceId} : {baseFormId:subject.actor.baseFormId})},request);
  const stageRef = useRef<HTMLDivElement>(null);
  const refreshRef = useRef(false);
  const camera = useRef(initialCamera());
  const drag = useRef<{ id: number; x: number; y: number } | null>(null);
  const requestUpdate = useRef<() => void>(() => undefined);
  const [dragging, setDragging] = useState(false);
  const [mode, setMode] = useState<"live" | "portrait">("live");
  const [motion, setMotion] = useState(initialMotion);
  const [pose, setPose] = useState('current');
  const [animationClips, setAnimationClips] = useState<SkyUltraActorAnimationClip[]>([]);
  const [animationFrame, setAnimationFrame] = useState(0);
  const animationFrameRef = useRef(0); animationFrameRef.current = animationFrame;
  const [animationAction, setAnimationAction] = useState<'capture' | 'library' | null>(null);
  const capturing = animationAction !== null;
  const [animationMessage, setAnimationMessage] = useState('');
  const captureAbort = useRef<AbortController | null>(null);
  const selectedAnimation = animationClips.find(clip => pose === 'animation:' + clip.id);
  const [lighting, setLighting] = useState<SkyUltraPreviewLighting>('studio');
  // Read the current light from the active controller. A choice must not tear
  // down its observers/requests or hide and recreate the native stage.
  const lightingRef = useRef(lighting); lightingRef.current = lighting;
  const [dragMode, setDragMode] = useState<'rotate' | 'pan'>('rotate');
  const [saving, setSaving] = useState(false);
  const [saveMessage, setSaveMessage] = useState('');
  const [revision, setRevision] = useState(0);
  const [status, setStatus] = useState<PreviewStatus>({ available: false, ready: false, message: "Preparing the model…" });
  const updateStatus = (next: PreviewStatus): void => setStatus(previous =>
    previous.available === next.available && previous.ready === next.ready &&
    previous.geometryCount === next.geometryCount && previous.skippedGeometryCount === next.skippedGeometryCount &&
    previous.message === next.message && previous.posesAvailable === next.posesAvailable ? previous : next);
  const [hasImage, setHasImage] = useState(false);
  const acceptStatus = (next: PreviewStatus): void => {
    if (next.ready) setHasImage(true);
    updateStatus(next);
  };
  const hintId = React.useId();
  const actorKey = subject.kind === 'item' ? 'item:' + subject.item.id : 'actor:' + (subject.actor.referenceId ?? subject.actor.baseFormId);
  const currentActorKey = useRef(actorKey); currentActorKey.current = actorKey;
  useEffect(() => { captureAbort.current?.abort(); setHasImage(false); camera.current = initialCamera(); drag.current = null; setDragging(false); setPose('current'); setAnimationClips([]); setAnimationFrame(0); setAnimationAction(null); setAnimationMessage(''); setSaveMessage(''); setStatus({ available: false, ready: false, message: "Preparing the model…" }); }, [actorKey]);
  useLayoutEffect(() => {
    let active = true, pending = false, dirty = false, hidden = false, statusPending = false, hostVisible = bridge.getVisibility?.().visible ?? true, scoped = false;
    let timer = 0;
    let layoutKey = '';
    const controller = new AbortController();
    const hide = (): void => { if (hidden && !pending) return; hidden = true; void renderer.hide().catch(() => undefined); };
    const leavePage = (): void => {
      active = false;
      captureAbort.current?.abort();
      controller.abort();
      window.clearTimeout(timer);
      hide();
    };
    window.addEventListener(previewNavigationEvent, leavePage);
    const update = async (): Promise<void> => {
      if (!active) return;
      if (pending) { dirty = true; return; }
      const node = stageRef.current; if (!node) return;
      const rect = node.getBoundingClientRect();
      const header = document.querySelector(".sky-menu-header,.app-topbar")?.getBoundingClientRect().bottom ?? 0;
      const footer = document.querySelector(".app-footer")?.getBoundingClientRect().top ?? window.innerHeight;
      const visible = hostVisible && !scoped && !obscured && !document.hidden && rect.width > 0 && rect.height > 0 && rect.top >= header && rect.bottom <= footer && rect.left >= 0 && rect.right <= window.innerWidth;
      if (!visible) { hide(); return; }
      const pixelScale = Math.min(window.devicePixelRatio || 1, 1024 / Math.max(rect.width, rect.height));
      const refresh = refreshRef.current; refreshRef.current = false;
      pending = true; dirty = false;
      try {
        const result = await setPreview({
          bounds: { x: rect.left / window.innerWidth, y: rect.top / window.innerHeight, width: rect.width / window.innerWidth, height: rect.height / window.innerHeight },
          width: Math.max(128, Math.round(rect.width * pixelScale)), height: Math.max(128, Math.round(rect.height * pixelScale)),
          visible: true, live: mode === "live" && motion && pose === 'current', portrait: mode === "portrait", refresh, pose: 'current', lighting: lightingRef.current,
          ...(selectedAnimation ? { animation: { id: selectedAnimation.id, frame: animationFrameRef.current } } : {}), ...camera.current,
        }, { signal: controller.signal });
        if (active) { hidden = false; acceptStatus(result); }
      } catch (error) { if (active) setStatus({ available: false, ready: false, message: error instanceof Error ? error.message : "Model preview unavailable." }); }
      finally { pending = false; if (active && dirty) timer = window.setTimeout(() => { timer = 0; void update(); }, 33); }
    };
    const cameraUpdate = (): void => { camera.current.yaw = wrapAngleRadians(camera.current.yaw); camera.current.pitch = wrapAngleRadians(camera.current.pitch); dirty = true; if (!timer) timer = window.setTimeout(() => { timer = 0; void update(); }, 33); };
    // Updating layout must not blank a stationary stage. Scroll events bubble
    // through window's capture listener even when only a sibling list scrolls.
    const reposition = (): void => {
      if (!active) return;
      if (document.hidden || !hostVisible || scoped || obscured) { hide(); return; }
      const rect = stageRef.current?.getBoundingClientRect();
      const header = document.querySelector('.sky-menu-header,.app-topbar')?.getBoundingClientRect().bottom ?? 0;
      const footer = document.querySelector('.app-footer')?.getBoundingClientRect().top ?? window.innerHeight;
      if (rect && (rect.top < header || rect.bottom > footer || rect.left < 0 || rect.right > window.innerWidth)) hide();
      if (rect) {
        const nextLayout = [rect.left, rect.top, rect.width, rect.height, window.innerWidth, window.innerHeight, window.devicePixelRatio].join(':');
        if (!hidden && nextLayout === layoutKey) return;
        layoutKey = nextLayout;
      }
      if (!timer) timer = window.setTimeout(() => { timer = 0; void update(); }, 16);
    };
    const scroll = (event: Event): void => {
      const node = stageRef.current, target = event.target;
      if (!node || (target && target !== document && target !== window && !(target as Node).contains?.(node))) return;
      reposition();
    };
    const scopeChange = (event: Event): void => { scoped = ((event as CustomEvent<{depth:number}>).detail?.depth ?? 0) > 0; reposition(); };
    window.addEventListener('skyultra:input-scope', scopeChange);
    requestUpdate.current = cameraUpdate;
    const unsubscribe = bridge.on<{visible:boolean}>("skyultra.visibility",event=>{hostVisible=event.visible;reposition();});
    const wheel = (event: WheelEvent): void => { event.preventDefault(); event.stopPropagation(); camera.current.zoom = Math.max(.55, Math.min(2.2, camera.current.zoom * Math.exp(-Math.max(-400, Math.min(400, event.deltaY * (event.deltaMode === 1 ? 16 : 1))) * .0015))); cameraUpdate(); };
    const node = stageRef.current;
    node?.addEventListener("wheel", wheel, { passive: false });
    const observer = typeof ResizeObserver !== "undefined" ? new ResizeObserver(reposition) : null;
    if (node) observer?.observe(node);
    window.addEventListener("resize", reposition); window.addEventListener("scroll", scroll, true); document.addEventListener("visibilitychange", reposition);
    reposition();
    const statusTimer = window.setInterval(async () => { if (!active || document.hidden || hidden || pending || statusPending) return; statusPending = true; try { const result = await renderer.status({ signal: controller.signal }); if (active) acceptStatus(result); } catch { /* initial request reports failures */ } finally { statusPending = false; } }, 1200);
    return () => { window.removeEventListener(previewNavigationEvent, leavePage); active = false; unsubscribe(); requestUpdate.current = () => undefined; hide(); controller.abort(); observer?.disconnect(); node?.removeEventListener("wheel", wheel); window.clearTimeout(timer); window.clearInterval(statusTimer); window.removeEventListener('skyultra:input-scope', scopeChange); window.removeEventListener("resize", reposition); window.removeEventListener("scroll", scroll, true); document.removeEventListener("visibilitychange", reposition); };
  }, [actorKey, bridge, mode, motion, pose, obscured, revision, equipment]);
  useEffect(() => { requestUpdate.current(); }, [lighting]);
  useEffect(() => { if (refreshToken !== undefined) { refreshRef.current = true; requestUpdate.current(); } }, [refreshToken]);
  useLayoutEffect(() => () => { captureAbort.current?.abort(); void renderer.close().catch(() => undefined); }, [renderer]);
  const captureAnimations = async (library = false): Promise<void> => {
    if (isItem || capturing || saving || !status.ready || obscured) return;
    captureAbort.current?.abort();
    const controller = new AbortController(); captureAbort.current = controller;
    const capturedActorKey = actorKey;
    setAnimationAction(library ? 'library' : 'capture'); setAnimationMessage(library ? 'Loading pose library…' : 'Capturing animation frames…'); setMotion(false); setPose('current');
    try {
      const result = library ? await actorRenderer.loadPoses({ signal: controller.signal }) : await actorRenderer.captureAnimations({ signal: controller.signal });
      if (controller.signal.aborted || currentActorKey.current !== capturedActorKey) return;
      setAnimationClips(result.clips); setAnimationFrame(library && result.clips[0] ? Math.floor((result.clips[0].sampleCount - 1) / 2) : 0);
      setAnimationMessage(result.message ?? (result.available ? 'Choose a captured animation and frame.' : 'No compatible animations are loaded for this actor.'));
      if (result.clips[0]) setPose('animation:' + result.clips[0].id);
    } catch (error) {
      if (!controller.signal.aborted && currentActorKey.current === capturedActorKey) setAnimationMessage(error instanceof Error ? error.message : 'Could not capture animations.');
    } finally { if (!controller.signal.aborted) setAnimationAction(null); }
  };
  const reset = (): void => { camera.current = initialCamera(); requestUpdate.current(); };
  const save = async (): Promise<void> => {
    if (saving || capturing || !status.ready || obscured) return;
    const node = stageRef.current; if (!node) return;
    setSaving(true); setSaveMessage('Saving PNG…');
    const savedActorKey = actorKey;
    try {
      const rect = node.getBoundingClientRect();
      const scale = Math.min(window.devicePixelRatio || 1, 1024 / Math.max(rect.width, rect.height));
      // Flush the latest camera before the native host snapshots the save job.
      await setPreview({
        bounds: { x: rect.left / window.innerWidth, y: rect.top / window.innerHeight, width: rect.width / window.innerWidth, height: rect.height / window.innerHeight },
        width: Math.max(128, Math.round(rect.width * scale)), height: Math.max(128, Math.round(rect.height * scale)),
        visible: true, live: mode === 'live' && motion && pose === 'current', portrait: mode === 'portrait', pose: 'current', lighting,
        ...(selectedAnimation ? { animation: { id: selectedAnimation.id, frame: animationFrame } } : {}), ...camera.current });
      const result = await renderer.save(); if (currentActorKey.current === savedActorKey) setSaveMessage('Saved PNG: ' + result.path);
    } catch (error) { if (currentActorKey.current === savedActorKey) setSaveMessage(error instanceof Error ? error.message : 'Could not save PNG.'); }
    finally { setSaving(false); }
  };
  const capturePortrait = async (): Promise<void> => {
    if (!onCapturePortrait) {
      refreshRef.current = true;
      setRevision(value => value + 1);
      return;
    }
    if (saving || capturing || !status.ready || obscured) return;
    const node = stageRef.current; if (!node) return;
    setSaving(true); setSaveMessage('Capturing portrait and saving to Overwrite cache…');
    const savedActorKey = actorKey;
    try {
      const rect = node.getBoundingClientRect();
      const scale = Math.min(window.devicePixelRatio || 1, 1024 / Math.max(rect.width, rect.height));
      await setPreview({
        bounds: { x: rect.left / window.innerWidth, y: rect.top / window.innerHeight, width: rect.width / window.innerWidth, height: rect.height / window.innerHeight },
        width: Math.max(128, Math.round(rect.width * scale)), height: Math.max(128, Math.round(rect.height * scale)),
        visible: true, live: mode === 'live' && motion && pose === 'current', portrait: mode === 'portrait', pose: 'current', lighting,
        ...(selectedAnimation ? { animation: { id: selectedAnimation.id, frame: animationFrame } } : {}), ...camera.current });
      const detail = await onCapturePortrait();
      if (currentActorKey.current === savedActorKey) setSaveMessage(typeof detail === 'string' && detail ? detail : 'Updated portrait and saved to Overwrite cache.');
    } catch (error) {
      if (currentActorKey.current === savedActorKey) setSaveMessage(error instanceof Error ? error.message : 'Could not capture actor portrait.');
    } finally { setSaving(false); }
  };
  const release = (event: React.PointerEvent<HTMLDivElement>): void => { if (drag.current?.id !== event.pointerId) return; drag.current = null; setDragging(false); if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId); };
  return <div className={"actor-studio" + (isItem ? " item-model-studio" : equipment ? " equipment-model-studio" : "")}><div className="studio-heading"><span className="eyebrow">{isItem ? "ITEM STUDIO" : equipment ? "CURRENT OUTFIT" : "CHARACTER STUDIO"}</span>{!isItem && !equipment && <SkySegmentedControl label="Actor preview mode" value={mode} onValueChange={setMode} options={[{ value: "live", label: "Full model" }, { value: "portrait", label: "Portrait" }]} />}</div>
    <div className="actor-stage"><div ref={stageRef} tabIndex={0} className={"actor-viewport" + (dragging ? " is-dragging" : "")} aria-label={"Model of " + (subject.kind === "item" ? subject.item.name : subject.actor.name)} aria-describedby={hintId}
      onPointerDown={(event) => { if (event.button !== 0) return; event.preventDefault(); event.currentTarget.focus(); event.currentTarget.setPointerCapture(event.pointerId); drag.current = { id: event.pointerId, x: event.clientX, y: event.clientY }; setDragging(true); }}
      onPointerMove={(event) => { const previous = drag.current; if (!previous || previous.id !== event.pointerId) return; const rect = event.currentTarget.getBoundingClientRect(); camera.current = dragActorCamera(camera.current, event.shiftKey ? 'pan' : dragMode, event.clientX - previous.x, event.clientY - previous.y, rect.width, rect.height); drag.current = { id: event.pointerId, x: event.clientX, y: event.clientY }; requestUpdate.current(); }}
      onPointerUp={release} onPointerCancel={release} onLostPointerCapture={() => { drag.current = null; setDragging(false); }} onDoubleClick={reset}
      onKeyDown={(event) => { if (["ArrowLeft","ArrowRight","ArrowUp","ArrowDown","+","=","-","Home"].includes(event.key)) { event.preventDefault(); event.stopPropagation(); if (event.key === "Home") reset(); else { if (event.key.startsWith('Arrow') && (dragMode === 'pan' || event.shiftKey)) camera.current = dragActorCamera(camera.current,'pan',event.key === 'ArrowRight' ? 3 : event.key === 'ArrowLeft' ? -3 : 0,event.key === 'ArrowDown' ? 3 : event.key === 'ArrowUp' ? -3 : 0,100,100); else if (event.key === "ArrowLeft" || event.key === "ArrowRight") camera.current.yaw = wrapAngleRadians(camera.current.yaw + (event.key === "ArrowRight" ? .12 : -.12)); else if (event.key === "ArrowUp" || event.key === "ArrowDown") camera.current.pitch = wrapAngleRadians(camera.current.pitch + (event.key === "ArrowUp" ? .08 : -.08)); else camera.current.zoom = Math.max(.55, Math.min(2.2, camera.current.zoom * (event.key === "-" ? .9 : 1.1))); requestUpdate.current(); } } }}>
      <div className="stage-grid" aria-hidden="true" /><span className="stage-corner stage-corner--tl" /><span className="stage-corner stage-corner--br" />{!status.ready && <div className="stage-status" role="status"><Icon name="actors" size={36} /><span>{status.message ?? "Preparing the preview…"}</span></div>}</div>
      <span className="stage-caption">{equipment ? (motion ? "LIVE OUTFIT" : "POSE HELD") : isItem ? "ITEM MODEL" : selectedAnimation ? 'ANIMATION FRAME ' + (animationFrame + 1) : mode === "portrait" ? "PORTRAIT CAPTURE" : pose !== 'current' ? pose.toUpperCase() + ' POSE' : motion ? "LIVE POSE" : "FROZEN POSE"}{!equipment && <><i />{status.ready ? String(status.geometryCount ?? 0) + " DRAW PACKETS" : "PREPARING MODEL"}</>}</span></div>
    {equipment ? <div className="outfit-dock">
      <SkySegmentedControl label="Model drag action" value={dragMode} onValueChange={setDragMode} options={[{value:'rotate',label:'Rotate'},{value:'pan',label:'Move'}]} />
      <SkyChoice label="Preview lighting" value={lighting} onValueChange={setLighting} options={[{value:'studio',label:'Studio'},{value:'soft',label:'Soft'},{value:'dramatic',label:'Dramatic'},{value:'warm',label:'Warm'},{value:'cool',label:'Cool'}]} />
      <div className="outfit-dock__actions">{!hideStudioControls && <button className={'studio-icon-button' + (motion ? ' is-active' : '')} aria-label={motion ? 'Pause outfit motion' : 'Resume outfit motion'} title={motion ? 'Pause motion' : 'Resume motion'} aria-pressed={motion} onClick={() => setMotion(value => !value)}><Icon name={motion ? 'pause' : 'play'} size={14} /></button>}
        <button className="studio-icon-button" aria-label="Reset outfit view" title="Reset view" onClick={reset}><Icon name="rotate" size={14} /></button>
        {!hideStudioControls && <button className="studio-icon-button" aria-label={onCapturePortrait ? 'Update portrait (saved to Overwrite cache)' : 'Recapture outfit'} title={onCapturePortrait ? 'Update portrait (saved to Overwrite cache)' : 'Recapture model'} disabled={onCapturePortrait ? (!status.ready || saving || capturing || obscured) : false} onClick={() => { void capturePortrait(); }}><Icon name="camera" size={14} /></button>}
        {!hideStudioControls && <button className="control-button outfit-save" disabled={!status.ready || saving || capturing || obscured} onClick={() => { void save(); }}><Icon name="download" size={13} />{saving ? 'Saving…' : 'Save'}</button>}</div>
    </div> : <>
    <div className="studio-controls"><SkySegmentedControl label="Model drag action" value={dragMode} onValueChange={setDragMode} options={[{value:'rotate',label:'Rotate'},{value:'pan',label:'Move'}]} />{!isItem && !equipment && <SkyChoice label="Actor pose" value={pose} disabled={capturing} onValueChange={value => { setPose(value); setAnimationFrame(0); }} options={[{value:'current',label:'Current pose'}, ...animationClips.map(clip => ({value:'animation:' + clip.id,label:clip.preset ? clip.name : 'Animation: ' + clip.name}))]} />}<SkyChoice label="Preview lighting" value={lighting} onValueChange={setLighting} options={[{value:'studio',label:'Studio light'},{value:'soft',label:'Soft light'},{value:'dramatic',label:'Dramatic light'},{value:'warm',label:'Warm light'},{value:'cool',label:'Cool light'}]} /></div>
    {!isItem && !equipment && <ActorAnimationTimeline clip={selectedAnimation} frame={animationFrame} busy={capturing || saving} onFrameChange={value => { setAnimationFrame(value); animationFrameRef.current = value; requestUpdate.current(); }} />}
    <div className="studio-controls">{!isItem && <button className={"control-button" + (motion ? " is-active" : "")} onClick={() => setMotion(value => !value)} disabled={mode === "portrait" || pose !== 'current'}><Icon name={motion ? "pause" : "play"} size={14} />{mode === "portrait" ? "Still portrait" : pose !== 'current' ? 'Static pose' : motion ? "Live motion" : "Pose frozen"}</button>}<button className="control-button" onClick={reset}><Icon name="rotate" size={14} />Reset view</button><button className="control-button" onClick={() => { refreshRef.current = true; setRevision(value => value + 1); }}>Recapture</button>{onCapturePortrait && <button className="control-button" title={isItem ? "Save current view as item portrait in Overwrite cache" : "Update portrait and save to Overwrite cache"} disabled={!status.ready || saving || capturing || obscured} onClick={() => { void capturePortrait(); }}><Icon name="camera" size={14} />{isItem ? "Save as portrait" : "Update portrait"}</button>}{!isItem && !equipment && <><button className="control-button" disabled={!status.ready || saving || capturing || obscured} onClick={() => { void captureAnimations(true); }}>{animationAction === 'library' ? 'Loading poses…' : 'Load poses'}</button><button className="control-button" disabled={!status.ready || saving || capturing || obscured} onClick={() => { void captureAnimations(); }}>{animationAction === 'capture' ? 'Capturing…' : 'Capture animations'}</button></>}<button className="control-button" disabled={!status.ready || saving || capturing || obscured} onClick={() => { void save(); }}>{saving ? 'Saving…' : 'Save'}</button></div>
    </>}
    <p id={hintId} className="studio-gesture"><span>{dragMode === 'pan' ? 'DRAG TO MOVE' : 'DRAG TO ORBIT'}</span><i /><span>SHIFT + DRAG TO MOVE</span><i /><span>SCROLL TO ZOOM</span></p>
    {!isItem && animationMessage && <p className="studio-note" role="status">{animationMessage}</p>}
    {saveMessage && <p className="studio-note studio-save-status" role="status">{saveMessage}</p>}
    {((!hasImage && !status.ready) || !status.available || !!status.skippedGeometryCount) && <p className="studio-note">{status.ready ? status.skippedGeometryCount + " unsupported meshes omitted." : status.message}</p>}
  </div>;
}
