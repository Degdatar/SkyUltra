import * as React from 'react';
import { useState } from 'react';
import { parseSkyUltraHudLayout, type SkyUltraHudLayout, type SkyUltraWidgetKind } from '@skyultra/sdk';
export interface SkyHudComposerProps {
  value:SkyUltraHudLayout;onValueChange(layout:SkyUltraHudLayout):void;
  renderWidget(kind:SkyUltraWidgetKind):React.ReactNode;
  activeWidget?:SkyUltraWidgetKind|null;
  onSelectWidget?(kind:SkyUltraWidgetKind):void;
}
/** Controlled visual layout editor. No automatic pinning, storage, theme or game mutation. */
export function SkyHudComposer({value,onValueChange,renderWidget,activeWidget,onSelectWidget}:SkyHudComposerProps):React.JSX.Element {
  const [source,setSource]=useState(''),[error,setError]=useState('');
  const move=(index:number,delta:number):void=>{const widgets=[...value.widgets],next=index+delta;if(next<0||next>=widgets.length)return;[widgets[index],widgets[next]]=[widgets[next],widgets[index]];onValueChange({...value,widgets});};
  const remove=(kind:SkyUltraWidgetKind):void=>{onValueChange({...value,widgets:value.widgets.filter(w=>w!==kind)});};
  return <div className="sky-hud-composer"><ol aria-label="HUD layout order">{value.widgets.map((kind,index)=>{const wp=value.placements?.[kind];const anchor=(wp?.anchor??value.anchor).toUpperCase();const scale=Math.round((wp?.scale??value.scale)*100);const color=wp?.color||value.colors?.[kind]||"var(--sky-accent)";return <li key={kind} data-active={activeWidget===kind||undefined} onClick={()=>onSelectWidget?.(kind)}><div className="sky-hud-composer__tools"><span className="sky-hud-composer__tag"><i style={{background:color}}/>{index+1} · {kind.toUpperCase()} · {anchor} · {scale}%</span><div className="sky-hud-composer__btns"><button type="button" aria-label={'Move '+kind+' up'} disabled={index===0} onClick={e=>{e.stopPropagation();move(index,-1);}}>↑</button><button type="button" aria-label={'Move '+kind+' down'} disabled={index===value.widgets.length-1} onClick={e=>{e.stopPropagation();move(index,1);}}>↓</button><button type="button" aria-label={'Remove '+kind} onClick={e=>{e.stopPropagation();remove(kind);}}>×</button></div></div>{renderWidget(kind)}</li>;})}</ol><details className="sky-hud-composer__io"><summary>Import / export layout JSON</summary><textarea aria-label="Layout JSON" spellCheck={false} value={source} placeholder="Paste a version-1 layout JSON object" onChange={e=>setSource(e.currentTarget.value)}/><div className="button-row"><button type="button" className="control-button" onClick={()=>{setSource(JSON.stringify(value,null,2));setError('');}}>Export layout</button><button type="button" className="primary-button" disabled={!source.trim()} onClick={()=>{try{onValueChange(parseSkyUltraHudLayout(source));setError('Layout imported.');}catch(e){setError(e instanceof SyntaxError?'Paste a complete layout JSON object before importing.':e instanceof Error?e.message:'This layout could not be imported.');}}}>Import layout</button></div>{error&&<p role="status">{error}</p>}</details></div>;
}
