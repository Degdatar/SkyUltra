import { getSkyWebView, type JsonValue, type SkyUltraRequestOptions, type SkyWebViewAPI } from './index.js';

export type SkyUltraFormId = number | string;
export type SkyUltraActorPose = 'current' | 'idle' | 'fighting' | 'guard' | 'spellcasting';
export const skyUltraActorPosePresets = ['idle', 'fighting', 'guard', 'spellcasting'] as const;
export type SkyUltraActorPreset = typeof skyUltraActorPosePresets[number];
export const skyUltraPreviewLightingPresets = ['studio', 'soft', 'dramatic', 'warm', 'cool'] as const;
export type SkyUltraPreviewLighting = typeof skyUltraPreviewLightingPresets[number];
export interface SkyUltraActorPreviewOptions {
  referenceId?: SkyUltraFormId;
  baseFormId?: SkyUltraFormId;
  /** Fractions of the owning view's viewport, not screen pixels. */
  bounds?: { x: number; y: number; width: number; height: number };
  width?: number; height?: number;
  visible?: boolean; live?: boolean; portrait?: boolean; refresh?: boolean;
  /** Ignore runtime scalar camera/effect fading; retain texture/vertex alpha and NIF blending. */
  ignoreWorldFade?: boolean;
  /** Radians. Camera motion never rotates or changes the real actor. */
  yaw?: number; pitch?: number; zoom?: number;
  /** Fractions of the image. Positive values move the actor right/down. */
  panX?: number; panY?: number;
  /** Static preview skeleton presets; never changes the live NPC. */
  pose?: SkyUltraActorPose;
  /** Camera-relative preview lights, also used for PNG export. Defaults to studio. */
  lighting?: SkyUltraPreviewLighting;
  /** A private clip/frame returned by captureAnimations(); excludes arm presets. */
  animation?: { id: string; frame: number };
}
export interface SkyUltraActorAnimationClip {
  id: string; name: string; duration: number; sampleCount: number;
  /** Present for a pose loaded from the independent library. */
  preset?: SkyUltraActorPreset;
}
export interface SkyUltraActorAnimations {
  available: boolean; clips: SkyUltraActorAnimationClip[]; message?: string;
}
export type SkyUltraItemPreviewOptions = Pick<SkyUltraActorPreviewOptions,
  'bounds' | 'width' | 'height' | 'visible' | 'refresh' | 'yaw' | 'pitch' | 'zoom' | 'panX' | 'panY' | 'lighting'> & { formId?: SkyUltraFormId };
export interface SkyUltraItemPreviewStatus {
  available: boolean; ready: boolean; formId?: string;
  geometryCount?: number; skippedGeometryCount?: number; message?: string;
}
export const skyUltraItemRendererMethods = ['game.setItemPreview', 'game.getItemPreviewStatus', 'game.closeItemPreview', 'game.saveItemPreview'] as const;
export interface SkyUltraItemRenderer {
  setItem(options: SkyUltraItemPreviewOptions, request?: SkyUltraRequestOptions): Promise<SkyUltraItemPreviewStatus>;
  status(request?: SkyUltraRequestOptions): Promise<SkyUltraItemPreviewStatus>;
  hide(request?: SkyUltraRequestOptions): Promise<SkyUltraItemPreviewStatus>;
  close(request?: SkyUltraRequestOptions): Promise<SkyUltraItemPreviewStatus>;
  /** Save under Data/SkyUltra/Items using the full-view export preference. */
  save(options?: SkyUltraActorSaveOptions): Promise<SkyUltraActorSaveResult>;
}
export interface SkyUltraActorPreviewStatus {
  available: boolean; ready: boolean; referenceId?: string;
  geometryCount?: number; skippedGeometryCount?: number; message?: string;
  posesAvailable?: boolean;
}
export interface SkyUltraActorSaveResult { saved: true; path: string; }
export interface SkyUltraActorSaveOptions extends SkyUltraRequestOptions {
  /** Override the saved resolution with a long-edge size (720–4096). */
  longEdge?: number;
}
export const skyUltraRendererMethods = ['game.setActorPreview', 'game.getActorPreviewStatus', 'game.closeActorPreview', 'game.saveActorPreview', 'game.captureActorAnimations', 'game.loadActorPoses'] as const;
export interface SkyUltraRenderer {
  setActor(options: SkyUltraActorPreviewOptions, request?: SkyUltraRequestOptions): Promise<SkyUltraActorPreviewStatus>;
  status(request?: SkyUltraRequestOptions): Promise<SkyUltraActorPreviewStatus>;
  hide(request?: SkyUltraRequestOptions): Promise<SkyUltraActorPreviewStatus>;
  close(request?: SkyUltraRequestOptions): Promise<SkyUltraActorPreviewStatus>;
  /** Explicitly capture compatible active clips for the view's selected actor. */
  captureAnimations(request?: SkyUltraRequestOptions): Promise<SkyUltraActorAnimations>;
  /** Load independent Idle/Fighting/Guard/Spellcasting clips for a standard humanoid rig. */
  loadPoses(request?: SkyUltraRequestOptions): Promise<SkyUltraActorAnimations>;
  /** Save using the configured portrait/full-view resolution under Data/SkyUltra/Actors. */
  save(options?: SkyUltraActorSaveOptions): Promise<SkyUltraActorSaveResult>;
}
function status(value: unknown, kind = 'actor'): SkyUltraActorPreviewStatus {
  if (!value || typeof value !== 'object' || typeof (value as SkyUltraActorPreviewStatus).available !== 'boolean' || typeof (value as SkyUltraActorPreviewStatus).ready !== 'boolean') throw new Error('Invalid ' + kind + ' renderer response.');
  return value as SkyUltraActorPreviewStatus;
}
function animationCatalog(input: unknown): SkyUltraActorAnimations {
  const value = input as SkyUltraActorAnimations | null;
  if (!value || typeof value.available !== 'boolean' || !Array.isArray(value.clips) || value.clips.length > 4 ||
      value.available !== (value.clips.length > 0) || value.clips.some(clip => !clip || typeof clip.id !== 'string' || !clip.id || clip.id.length > 64 || typeof clip.name !== 'string' || !clip.name || !Number.isFinite(clip.duration) || clip.duration <= 0 || clip.duration > 120 || !Number.isInteger(clip.sampleCount) || clip.sampleCount < 2 || clip.sampleCount > 33 || (clip.preset !== undefined && !skyUltraActorPosePresets.includes(clip.preset))) || new Set(value.clips.map(clip => clip.id)).size !== value.clips.length) throw new Error('Invalid actor animation capture response.');
  return value;
}
/** Engine-neutral, framework-neutral renderer. One actor OR item stage per owning view.
 * Put skyUltraRendererMethods in the manifest and the view's method allowlist.
 * close() on unmount; the host also releases GPU resources when the view closes.
 */
export function createSkyUltraRenderer(bridge: SkyWebViewAPI = getSkyWebView()): SkyUltraRenderer {
  const call = async (method: string, params: JsonValue, options?: SkyUltraRequestOptions) => status(await bridge.request(method, params, options));
  return {
    setActor(options, request) {
      if (options.ignoreWorldFade !== undefined && typeof options.ignoreWorldFade !== 'boolean') return Promise.reject(new Error('ignoreWorldFade must be a boolean.'));
      for (const value of [options.width, options.height, options.yaw, options.pitch, options.zoom, options.panX, options.panY, ...Object.values(options.bounds ?? {})]) {
        if (value !== undefined && !Number.isFinite(value)) return Promise.reject(new Error('Actor preview numbers must be finite.'));
      }
      if (options.pose !== undefined && !['current','idle','fighting','guard','spellcasting'].includes(options.pose)) return Promise.reject(new Error('Unknown actor preview pose.'));
      if (options.lighting !== undefined && !skyUltraPreviewLightingPresets.includes(options.lighting)) return Promise.reject(new Error('Unknown actor preview lighting preset.'));
      if (options.animation !== undefined) {
        const animation = options.animation;
        if (!animation || typeof animation.id !== 'string' || !animation.id || animation.id.length > 64 || !Number.isInteger(animation.frame) || animation.frame < 0 || animation.frame >= 33) return Promise.reject(new Error('Invalid actor animation id or frame.'));
        if (options.pose !== undefined && options.pose !== 'current') return Promise.reject(new Error('Choose either an animation frame or an arm preset.'));
      }
      return call('game.setActorPreview', JSON.parse(JSON.stringify(options)) as JsonValue, request);
    },
    status: request => call('game.getActorPreviewStatus', {}, request),
    hide: request => call('game.setActorPreview', { visible: false }, request),
    close: request => call('game.closeActorPreview', {}, request),
    async captureAnimations(request) {
      return animationCatalog(await bridge.request('game.captureActorAnimations', {}, request));
    },
    async loadPoses(request) {
      return animationCatalog(await bridge.request('game.loadActorPoses', {}, request));
    },
    async save(options) {
      const longEdge = options?.longEdge;
      if (longEdge !== undefined && (!Number.isInteger(longEdge) || longEdge < 720 || longEdge > 4096)) throw new Error('Actor PNG longEdge must be an integer from 720 to 4096.');
      const value = await bridge.request('game.saveActorPreview', longEdge === undefined ? {} : { longEdge }, options?.signal ? {signal:options.signal} : undefined) as { saved?: boolean; path?: string; message?: string } | null;
      if (!value || value.saved !== true || typeof value.path !== 'string' || !value.path) throw new Error(value?.message ?? 'Could not save the actor PNG.');
      return { saved: true, path: value.path };
    },
  };
}

/** One actor OR item stage per owning view. Uses record model assets, without
 * opening the vanilla inventory renderer or changing inventory/equipment. */
export function createSkyUltraItemRenderer(bridge: SkyWebViewAPI = getSkyWebView()): SkyUltraItemRenderer {
  const call = async (method: string, params: JsonValue, options?: SkyUltraRequestOptions) => status(await bridge.request(method, params, options), 'item') as SkyUltraItemPreviewStatus;
  return {
    setItem(options, request) {
      const id = typeof options.formId === 'string' ? options.formId.split(':')[0]! : options.formId;
      if (options.visible !== false && !(typeof id === 'number' ? Number.isInteger(id) && id > 0 && id <= 0xffffffff : typeof id === 'string' && /^(?:0x[0-9a-f]+|[0-9]+)$/i.test(id) && Number(id) > 0 && Number(id) <= 0xffffffff)) return Promise.reject(new Error('A valid item formId is required.'));
      for (const value of [options.width, options.height, options.yaw, options.pitch, options.zoom, options.panX, options.panY, ...Object.values(options.bounds ?? {})]) {
        if (value !== undefined && !Number.isFinite(value)) return Promise.reject(new Error('Item preview numbers must be finite.'));
      }
      if (options.lighting !== undefined && !skyUltraPreviewLightingPresets.includes(options.lighting)) return Promise.reject(new Error('Unknown item preview lighting preset.'));
      // Explicit selection prevents JS callers from passing actor-only controls
      // or arbitrary file paths through this facade.
      const {bounds,width,height,visible,refresh,yaw,pitch,zoom,panX,panY,lighting}=options;
      return call('game.setItemPreview',JSON.parse(JSON.stringify({formId:id,bounds,width,height,visible,refresh,yaw,pitch,zoom,panX,panY,lighting})) as JsonValue,request);
    },
    status: request => call('game.getItemPreviewStatus',{},request),
    hide: request => call('game.setItemPreview',{visible:false},request),
    close: request => call('game.closeItemPreview',{},request),
    async save(options) {
      const longEdge=options?.longEdge;
      if(longEdge!==undefined&&(!Number.isInteger(longEdge)||longEdge<720||longEdge>4096))throw new Error('Item PNG longEdge must be an integer from 720 to 4096.');
      const value=await bridge.request('game.saveItemPreview',longEdge===undefined?{}:{longEdge},options?.signal?{signal:options.signal}:undefined) as {saved?:boolean;path?:string;message?:string}|null;
      if(!value||value.saved!==true||typeof value.path!=='string'||!value.path)throw new Error(value?.message??'Could not save the item PNG.');
      return {saved:true,path:value.path};
    },
  };
}
