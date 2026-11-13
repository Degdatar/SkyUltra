import * as React from "react";
import { createPortal } from "react-dom";
import { skyStatMaximum, skyStatFraction } from "./scales.js";
export { SkyToggleSwitch, SkyRangeSlider, SkyFilterChips, SkyStatBar, skyStatMaximum, skyStatFraction } from "./controls.js";
import { enterSkyInputScope } from "./interaction.js";

/** Lightweight theme scope. This entry uses React and ReactDOM only, so game HUDs do not
 * load the optional dialog, menu, tooltip, or animation dependencies. */
export function SkyThemeScope({ theme = "obsidian", className = "", ...props }: React.HTMLAttributes<HTMLDivElement> & { theme?: string }): React.JSX.Element {
  return (
    <div
      {...props}
      data-sky-theme={theme}
      data-sky-ui-style={theme === "darkmode" ? "velvet-dark" : "standard"}
      className={"sky-theme" + (theme === "darkmode" ? " sky-theme--darkmode" : "") + (className ? " " + className : "")}
    />
  );
}

export interface SkyRingMeterProps {
  label: string;
  value: number;
  max: number;
  tone?: "accent" | "danger" | "success" | "gold";
  detail?: string;
  compact?: boolean;
}

/** SVG ring with a single dash transition and no animation loop. */
export function SkyRingMeter({ label, value, max, tone = "accent", detail, compact = false }: SkyRingMeterProps): React.JSX.Element {
  const safeValue = Number.isFinite(value) ? value : 0;
  const safeMax = Number.isFinite(max) ? Math.max(1, max) : 1;
  const fraction = Math.max(0, Math.min(1, safeValue / safeMax));
  const ticks=Array.from({length:32},(_,index)=>{const a=(index*360/32-90)*Math.PI/180;return <line key={index} x1={64+Math.cos(a)*57} y1={64+Math.sin(a)*57} x2={64+Math.cos(a)*(index%4===0?62:60)} y2={64+Math.sin(a)*(index%4===0?62:60)} />;});
  return <div className={"sky-ring" + (compact ? " sky-ring--compact" : "")} data-tone={tone} role="meter" aria-label={label} aria-valuemin={0} aria-valuemax={safeMax} aria-valuenow={Math.max(0, Math.min(safeMax, safeValue))}>
    <div className="sky-ring__dial"><svg viewBox="0 0 128 128" aria-hidden="true"><circle className="sky-ring__well" cx="64" cy="64" r="44"/><g className="sky-ring__ticks">{ticks}</g><circle className="sky-ring__track" cx="64" cy="64" r="51"/><circle className="sky-ring__fill" cx="64" cy="64" r="51" strokeDasharray={String(fraction*2*Math.PI*51)+" "+String(2*Math.PI*51)} transform="rotate(-90 64 64)"/><path className="sky-ring__etch" d="M42 26 Q64 13 86 26 M42 102 Q64 115 86 102 M20 59v10 M108 59v10"/><circle className="sky-ring__pin" cx={64+51*Math.sin(fraction*2*Math.PI)} cy={64-51*Math.cos(fraction*2*Math.PI)} r="3"/></svg><span className="sky-ring__number"><em>{Math.round(fraction*100)}<b>%</b></em>{Math.round(safeValue).toLocaleString()}<small>/ {Math.round(Number.isFinite(max) ? max : 0).toLocaleString()}</small></span></div>
    <span className="sky-ring__label">{label}</span>{detail && <small className="sky-ring__detail">{detail}</small>}
  </div>;
}

export interface SkySegmentOption<T extends string = string> { value: T; label: React.ReactNode; ariaLabel?: string; icon?: React.ReactNode; count?: number; disabled?: boolean; }
export function SkySegmentedControl<const T extends string>({ label, value, options, onValueChange, className = "" }: { label: string; value: T; options: readonly SkySegmentOption<T>[]; onValueChange(value: NoInfer<T>): void; className?: string }): React.JSX.Element {
  return <div className={"sky-segments " + className} role="group" aria-label={label}>{options.map((option) => <button key={option.value} type="button" aria-label={option.ariaLabel} aria-pressed={value === option.value} disabled={option.disabled} onClick={() => onValueChange(option.value)}>{option.icon}{option.label}{option.count !== undefined && <span>{option.count}</span>}</button>)}</div>;
}

export function SkyStatTile({ label, value, hint, icon, tone = "accent", className = "" }: { label: string; value: React.ReactNode; hint?: React.ReactNode; icon?: React.ReactNode; tone?: "accent" | "danger" | "success" | "gold"; className?: string }): React.JSX.Element {
  return <article className={"sky-stat-tile " + className} data-tone={tone}>{icon && <span className="sky-stat-tile__icon">{icon}</span>}<div><span className="sky-stat-tile__label">{label}</span><strong>{value}</strong>{hint && <small>{hint}</small>}</div></article>;
}

export interface SkyDataRow { id: string; label: React.ReactNode; value: React.ReactNode; meta?: React.ReactNode; icon?: React.ReactNode; }
export function SkyDataList({ rows, label, className = "", onRowSelect }: { rows: readonly SkyDataRow[]; label: string; className?: string; onRowSelect?(id: string): void }): React.JSX.Element {
  return <dl className={"sky-data-list " + className} aria-label={label}>{rows.map((row) => <div key={row.id} className="sky-data-list__row">{row.icon && <span className="sky-data-list__icon">{row.icon}</span>}<dt>{row.label}</dt><dd>{onRowSelect ? <button type="button" onClick={() => onRowSelect(row.id)}>{row.value}</button> : row.value}{row.meta && <small>{row.meta}</small>}</dd></div>)}</dl>;
}

/** Small static SVG chart; callers choose their sampling cadence. */
export function SkySparkline({ values, label, height = 48 }: { values: readonly number[]; label: string; height?: number }): React.JSX.Element {
  values = values.filter(Number.isFinite);
  const width = 320;
  const maximum = Math.max(1, ...values);
  const minimum = Math.min(0, ...values);
  const range = Math.max(1, maximum - minimum);
  const points = values.map((value, index) => (index * width / Math.max(1, values.length - 1)).toFixed(2) + "," + (height - 4 - (value - minimum) * (height - 8) / range).toFixed(2)).join(" ");
  return <svg className="sky-sparkline" viewBox={"0 0 " + width + " " + height} preserveAspectRatio="none" role="img" aria-label={label}><path className="sky-sparkline__baseline" d={"M0 " + (height - 2) + " H" + width} /><polyline points={points} fill="none" vectorEffect="non-scaling-stroke" /></svg>;
}

/** Static radar chart for small groups of game statistics. */
export function SkyRadarChart({ metrics, label, max }: { metrics: readonly { label: string; value: number }[]; label: string; max?: number }): React.JSX.Element {
  const items = metrics.slice(0, 12);
  const maximum = max !== undefined && Number.isFinite(max) && max > 0 ? max : skyStatMaximum(items.map(entry=>entry.value));
  const point = (index: number, radius: number): string => {
    const angle = -Math.PI / 2 + index * Math.PI * 2 / Math.max(3, items.length);
    return `${140 + Math.cos(angle) * radius},${128 + Math.sin(angle) * radius}`;
  };
  const points = items.map((entry, index) => point(index, 82 * skyStatFraction(entry.value, maximum))).join(" ");
  return <svg className="sky-radar" viewBox="0 0 280 260" role="img" aria-label={label}>
    <title>{label}: {items.map((entry) => `${entry.label} ${entry.value}`).join(", ")}</title>
    {[.25, .5, .75, 1].map((fraction) => <polygon key={fraction} className="sky-radar__guide" points={items.map((_, index) => point(index, 82 * fraction)).join(" ")} />)}
    {items.map((entry, index) => { const [x, y] = point(index, 108).split(",").map(Number); return <g key={entry.label}><line className="sky-radar__axis" x1="140" y1="128" x2={point(index, 82).split(",")[0]} y2={point(index, 82).split(",")[1]} /><text x={x} y={y} textAnchor="middle" dominantBaseline="middle">{entry.label}</text></g>; })}
    <polygon className="sky-radar__values" points={points} />
    {items.map((entry, index) => { const [x, y] = point(index, 82 * skyStatFraction(entry.value, maximum)).split(",").map(Number); return <circle key={entry.label} data-negative={entry.value<0} className="sky-radar__dot" cx={x} cy={y} r="2.5" />; })}
  </svg>;
}

export interface SkyChoiceOption<T extends string = string> { value: T; label: string; disabled?: boolean; }
/** Page-rendered select for embedded browsers whose native popup is unavailable. */
export function SkyChoice<const T extends string>({ label, value, options, onValueChange, disabled, className = "" }: { label: string; value: T; options: readonly SkyChoiceOption<T>[]; onValueChange(value: NoInfer<T>): void; disabled?: boolean; className?: string }): React.JSX.Element {
  const id = React.useId();
  const root = React.useRef<HTMLDivElement>(null);
  const trigger = React.useRef<HTMLButtonElement>(null);
  const menu = React.useRef<HTMLDivElement>(null);
  const [open, setOpen] = React.useState(false);
  const [focused, setFocused] = React.useState(0);
  const [placement, setPlacement] = React.useState<React.CSSProperties>({});
  const enabled = options.map((entry, index) => entry.disabled ? -1 : index).filter((index) => index >= 0);
  const openMenu = (): void => {
    const rect = trigger.current?.getBoundingClientRect();
    if (!rect || disabled || !enabled.length) return;
    const width = Math.min(Math.max(rect.width, 180), window.innerWidth - 24);
    const below = window.innerHeight - rect.bottom - 12;
    const above = rect.top - 12;
    const up = below < Math.min(270, options.length * 34 + 12) && above > below;
    setPlacement({ width, left: Math.max(12, Math.min(rect.left, window.innerWidth - width - 12)), maxHeight: Math.min(280, Math.max(90, up ? above : below)), ...(up ? { bottom: window.innerHeight - rect.top + 6 } : { top: rect.bottom + 6 }) });
    setFocused(Math.max(enabled[0], options.findIndex((entry) => entry.value === value && !entry.disabled)));
    setOpen(true);
  };
  React.useEffect(() => {
    if (!open) return;
    const releaseScope = enterSkyInputScope();
    const outside = (event: PointerEvent): void => { if (!root.current?.contains(event.target as Node) && !menu.current?.contains(event.target as Node)) setOpen(false); };
    const close = (): void => setOpen(false);
    const scroll = (event: Event): void => { if (!root.current?.contains(event.target as Node) && !menu.current?.contains(event.target as Node)) setOpen(false); };
    document.addEventListener("pointerdown", outside); window.addEventListener("resize", close); window.addEventListener("scroll", scroll, true);
    return () => { releaseScope(); document.removeEventListener("pointerdown", outside); window.removeEventListener("resize", close); window.removeEventListener("scroll", scroll, true); };
  }, [open]);
  React.useEffect(() => { if (disabled) setOpen(false); }, [disabled]);
  React.useEffect(()=>{if(open)document.getElementById(`${id}-${focused}`)?.scrollIntoView({block:"nearest"});},[open,focused,id]);
  const choose = (index: number): void => { const option = options[index]; if (!option || option.disabled) return; onValueChange(option.value); setOpen(false); trigger.current?.focus(); };
  const keyboard = (event: React.KeyboardEvent): void => {
    if (!enabled.length) return;
    if (event.key === "Escape") { if (open) { event.preventDefault(); event.stopPropagation(); setOpen(false); } return; }
    if (event.key === "Tab") { setOpen(false); return; }
    if (["ArrowDown", "ArrowUp", "Home", "End"].includes(event.key)) {
      event.preventDefault();
      if (!open) { openMenu(); return; }
      const current = Math.max(0, enabled.indexOf(focused));
      setFocused(event.key === "Home" ? enabled[0] : event.key === "End" ? enabled[enabled.length - 1] : enabled[(current + (event.key === "ArrowDown" ? 1 : enabled.length - 1)) % enabled.length]);
    } else if (open && event.key === "Enter") { event.preventDefault(); choose(focused); }
  };
  return <div ref={root} className={"sky-choice " + className} onKeyDown={keyboard} onBlur={(event) => { if (!event.currentTarget.contains(event.relatedTarget as Node | null) && !menu.current?.contains(event.relatedTarget as Node | null)) setOpen(false); }}>
    <button ref={trigger} type="button" role="combobox" aria-label={label} aria-expanded={open} aria-haspopup="listbox" aria-controls={open ? id : undefined} aria-activedescendant={open ? `${id}-${focused}` : undefined} disabled={disabled} className="sky-choice__trigger" onClick={() => open ? setOpen(false) : openMenu()}><span>{options.find((entry) => entry.value === value)?.label ?? value}</span><i aria-hidden="true" /></button>
    {open && createPortal(<div ref={menu} id={id} role="listbox" aria-label={label} className="sky-choice__menu" style={placement}>{options.map((option, index) => <button id={`${id}-${index}`} type="button" role="option" aria-selected={option.value === value} aria-disabled={option.disabled} disabled={option.disabled} tabIndex={-1} key={option.value} className={focused === index ? "is-focused" : ""} onMouseEnter={() => { if (!option.disabled) setFocused(index); }} onMouseDown={(event) => event.preventDefault()} onClick={() => choose(index)}>{option.label}{option.value === value && <span aria-hidden="true">✓</span>}</button>)}</div>, root.current?.closest("[data-sky-theme]") ?? document.body)}
  </div>;
}
