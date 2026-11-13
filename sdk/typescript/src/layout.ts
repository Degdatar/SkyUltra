/** Serializable HUD layout. Suitable for any frontend, local presets and visual editors. */
export const skyUltraWidgetKinds = ['vitals','character','equipment','resources','effects','compass','clock','radar','loot','stealth','wards','telemetry','target','skills','armory'] as const;
export type SkyUltraWidgetKind = typeof skyUltraWidgetKinds[number];
export const skyUltraWidgetAnchors = ['top-left','top-center','top-right','bottom-left','bottom-center','bottom-right'] as const;
export type SkyUltraWidgetAnchor = typeof skyUltraWidgetAnchors[number];
export type SkyUltraWidgetFontStyle = 'theme'|'serif'|'display'|'data'|'mono';
export const skyUltraVisibilityRules = ['always','combat-low-hp','encumbered','has-effects','has-followers'] as const;
export type SkyUltraVisibilityRule = typeof skyUltraVisibilityRules[number];
export const skyUltraHudBindingPaths = [
  'health.current',
  'magicka.current',
  'stamina.current',
  'armorRating',
  'gold',
  'carryWeight.current',
  'level',
  'location',
  'activeEffectCount',
  'questCount',
  'followerCount',
] as const;
export type SkyUltraHudBindingPath = typeof skyUltraHudBindingPaths[number];
export type SkyUltraWidgetColors = Partial<Record<SkyUltraWidgetKind, string>>;
export type SkyUltraWidgetPlacement = {
  anchor?: SkyUltraWidgetAnchor;
  scale?: number;
  opacity?: number;
  glow?: number;
  color?: string;
  fontStyle?: SkyUltraWidgetFontStyle;
  offsetX?: number;
  offsetY?: number;
  width?: number;
  customLabel?: string;
  bindingPath?: SkyUltraHudBindingPath;
  visibilityRule?: SkyUltraVisibilityRule;
};
export type SkyUltraWidgetPlacements = Partial<Record<SkyUltraWidgetKind, SkyUltraWidgetPlacement>>;
export interface SkyUltraHudLayout {
  version:1; widgets:SkyUltraWidgetKind[];
  anchor:SkyUltraWidgetAnchor;
  scale:number; opacity:number; theme:string;
  colors?:SkyUltraWidgetColors;
  glow?:number;
  snapGrid?:number;
  placements?:SkyUltraWidgetPlacements;
}
export function snapSkyUltraHudCoordinate(value:number, snapGrid:number, min=-600, max=600):number {
  if(!Number.isFinite(value)) return 0;
  const grid = Number.isFinite(snapGrid) && snapGrid > 1 ? Math.round(snapGrid) : 0;
  const snapped = grid > 0 ? Math.round(value / grid) * grid : Math.round(value);
  return Math.max(min, Math.min(max, snapped === 0 ? 0 : snapped));
}
export function parseSkyUltraHudLayout(input:string|unknown):SkyUltraHudLayout {
  const value=typeof input==='string'?JSON.parse(input):input;
  if(!value||typeof value!=='object')throw new Error('Expected a HUD layout object.');
  const v=value as Record<string,unknown>;
  if(v.version!==1||!Array.isArray(v.widgets)||v.widgets.length>16||v.widgets.some(kind=>!skyUltraWidgetKinds.includes(kind))||new Set(v.widgets).size!==v.widgets.length)throw new Error('Invalid HUD widgets or layout version.');
  if(!skyUltraWidgetAnchors.includes(String(v.anchor) as SkyUltraWidgetAnchor)||typeof v.scale!=='number'||!Number.isFinite(v.scale)||v.scale<.7||v.scale>1.5||typeof v.opacity!=='number'||!Number.isFinite(v.opacity)||v.opacity<.35||v.opacity>1||typeof v.theme!=='string'||!/^[a-z][a-z0-9-]{0,31}$/.test(v.theme))throw new Error('Invalid HUD anchor, size, opacity or theme.');
  const out:SkyUltraHudLayout={version:1,widgets:[...v.widgets],anchor:v.anchor as SkyUltraWidgetAnchor,scale:v.scale,opacity:v.opacity,theme:v.theme};
  if(v.colors&&typeof v.colors==='object'&&!Array.isArray(v.colors)){
    const colors:SkyUltraWidgetColors={};
    for(const [k,hex] of Object.entries(v.colors as Record<string,unknown>)){
      if(skyUltraWidgetKinds.includes(k as SkyUltraWidgetKind)&&typeof hex==='string'&&/^#[0-9a-f]{6}$/i.test(hex))colors[k as SkyUltraWidgetKind]=hex.toLowerCase();
    }
    if(Object.keys(colors).length>0)out.colors=colors;
  }
  if(typeof v.glow==='number'&&Number.isFinite(v.glow))out.glow=Math.max(0,Math.min(150,Math.round(v.glow)));
  if(typeof v.snapGrid==='number'&&Number.isFinite(v.snapGrid)&&[0,8,16,24].includes(Math.round(v.snapGrid)))out.snapGrid=Math.round(v.snapGrid);
  if(v.placements&&typeof v.placements==='object'&&!Array.isArray(v.placements)){
    const placements:SkyUltraWidgetPlacements={};
    for(const [k,raw] of Object.entries(v.placements as Record<string,unknown>)){
      if(!skyUltraWidgetKinds.includes(k as SkyUltraWidgetKind)||!raw||typeof raw!=='object')continue;
      const p=raw as Record<string,unknown>;
      const entry:SkyUltraWidgetPlacement={};
      if(typeof p.anchor==='string'&&skyUltraWidgetAnchors.includes(p.anchor as SkyUltraWidgetAnchor))entry.anchor=p.anchor as SkyUltraWidgetAnchor;
      if(typeof p.scale==='number'&&Number.isFinite(p.scale))entry.scale=Math.max(0.7,Math.min(1.5,p.scale));
      if(typeof p.opacity==='number'&&Number.isFinite(p.opacity))entry.opacity=Math.max(0.35,Math.min(1,p.opacity));
      if(typeof p.glow==='number'&&Number.isFinite(p.glow))entry.glow=Math.max(0,Math.min(150,Math.round(p.glow)));
      if(typeof p.color==='string'&&/^#[0-9a-f]{6}$/i.test(p.color))entry.color=p.color.toLowerCase();
      if(typeof p.fontStyle==='string'&&['theme','serif','display','data','mono'].includes(p.fontStyle))entry.fontStyle=p.fontStyle as SkyUltraWidgetFontStyle;
      if(typeof p.offsetX==='number'&&Number.isFinite(p.offsetX))entry.offsetX=Math.max(-600,Math.min(600,Math.round(p.offsetX)));
      if(typeof p.offsetY==='number'&&Number.isFinite(p.offsetY))entry.offsetY=Math.max(-400,Math.min(400,Math.round(p.offsetY)));
      if(typeof p.width==='number'&&Number.isFinite(p.width)&&p.width>=120&&p.width<=640)entry.width=Math.round(p.width);
      if(typeof p.customLabel==='string'&&p.customLabel.trim().length>0)entry.customLabel=p.customLabel.trim().slice(0,48);
      if(typeof p.bindingPath==='string'&&skyUltraHudBindingPaths.includes(p.bindingPath as SkyUltraHudBindingPath))entry.bindingPath=p.bindingPath as SkyUltraHudBindingPath;
      if(typeof p.visibilityRule==='string'&&skyUltraVisibilityRules.includes(p.visibilityRule as SkyUltraVisibilityRule))entry.visibilityRule=p.visibilityRule as SkyUltraVisibilityRule;
      if(Object.keys(entry).length>0)placements[k as SkyUltraWidgetKind]=entry;
    }
    if(Object.keys(placements).length>0)out.placements=placements;
  }
  return out;
}
