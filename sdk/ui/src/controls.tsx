import * as React from 'react';
export { skyStatMaximum, skyStatFraction } from './scales.js';

/** Native button semantics with a styled switch; never opens a platform popup. */
export function SkyToggleSwitch({ label, description, checked, onCheckedChange, disabled = false }: {label:string;description?:string;checked:boolean;onCheckedChange(checked:boolean):void;disabled?:boolean}):React.JSX.Element {
  return <button type="button" role="switch" aria-checked={checked} aria-label={label} disabled={disabled} className="sky-toggle" onClick={()=>onCheckedChange(!checked)}><span className="sky-toggle__copy"><strong>{label}</strong>{description&&<small>{description}</small>}</span><span className="sky-toggle__state" aria-hidden="true">{checked?'ON':'OFF'}</span><span className="sky-toggle__track" aria-hidden="true"><i><svg viewBox="0 0 16 16" fill="none" stroke="currentColor"><path d="M8 2v6m-3-4a5 5 0 1 0 6 0"/></svg></i></span></button>;
}

/** Accessible range input with a themed instrument track, keyboard steps and live value. */
export function SkyRangeSlider({label,value,min=0,max=100,step=1,unit='',onValueChange,disabled=false}: {label:string;value:number;min?:number;max?:number;step?:number;unit?:string;onValueChange(value:number):void;disabled?:boolean}):React.JSX.Element {
  const id=React.useId(); const high=Number.isFinite(max)&&max>min?max:min+1;
  const safe=Number.isFinite(value)?Math.max(min,Math.min(high,value)):min;
  return <div className="sky-range" data-disabled={disabled} style={{'--sky-range-fill':`${(safe-min)/(high-min)*100}%`} as React.CSSProperties}><div className="sky-range__heading"><label htmlFor={id}>{label}</label><output htmlFor={id}>{Number(safe.toFixed(2)).toLocaleString()}{unit}</output></div><div className="sky-range__rail"><input id={id} aria-valuetext={`${safe}${unit}`} type="range" min={min} max={high} step={step} value={safe} disabled={disabled} onChange={event=>onValueChange(Number(event.currentTarget.value))}/></div><div className="sky-range__ends" aria-hidden="true"><span>{min}{unit}</span><span>{high}{unit}</span></div></div>;
}

/** Reusable filter chips; selected state is available to assistive technology. */
export function SkyFilterChips<const T extends string>({label,value,options,onValueChange}: {label:string;value:T;options:readonly {value:T;label:string;count?:number}[];onValueChange(value:NoInfer<T>):void}):React.JSX.Element {
  return <div className="sky-filter-chips" role="group" aria-label={label}>{options.map(option=><button type="button" key={option.value} aria-pressed={value===option.value} onClick={()=>onValueChange(option.value)}><i aria-hidden="true"/>{option.label}{option.count!==undefined&&<span>{option.count}</span>}</button>)}</div>;
}

/** All sibling bars should receive the same maximum. Negative values retain their sign. */
export function SkyStatBar({label,value,maximum,unit=''}:{label:string;value:number;maximum:number;unit?:string}):React.JSX.Element {
  const safe=Number.isFinite(value)?value:0;
  const fraction=Number.isFinite(maximum)&&maximum>0?Math.min(1,Math.abs(safe)/maximum):0;
  return <div className="sky-stat-bar" data-negative={safe<0}><div><span>{label}</span><strong>{safe.toLocaleString(undefined,{maximumFractionDigits:1})}{unit}</strong></div><div className="sky-stat-bar__track" role="img" aria-label={`${label}: ${safe}${unit}; scale 0 to ${maximum}${unit}`}><i style={{transform:`scaleX(${fraction})`}}/></div></div>;
}
