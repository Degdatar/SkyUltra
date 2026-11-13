import * as React from "react";
import { enterSkyInputScope } from "./interaction.js";

export type SkyWidgetTone = "accent" | "health" | "magicka" | "stamina" | "gold" | "danger";
export const skyMeterFraction = (value: number, max: number): number => Number.isFinite(value) && Number.isFinite(max) && max > 0 ? Math.max(0, Math.min(1, value / max)) : 0;
const number = (value: number): string => Number.isFinite(value) ? Math.round(value).toLocaleString() : "—";
export interface SkyFrameProps extends Omit<React.HTMLAttributes<HTMLElement>, "title"> { title?: React.ReactNode; eyebrow?: string; actions?: React.ReactNode; footer?: React.ReactNode; }
export function SkyFrame({ title, eyebrow, actions, footer, className = "", children, ...props }: SkyFrameProps): React.JSX.Element {
  return <section {...props} className={"sky-frame " + className}>{(title || eyebrow || actions) && <header className="sky-frame__header"><div>{eyebrow && <span className="sky-frame__eyebrow">{eyebrow}</span>}{title && <h2>{title}</h2>}</div>{actions && <div className="sky-frame__actions">{actions}</div>}</header>}<div className="sky-frame__body">{children}</div>{footer && <footer className="sky-frame__footer">{footer}</footer>}</section>;
}
export interface SkyVital { id: string; label: string; value: number; max: number; tone?: SkyWidgetTone; }
export function SkyMeterBar({ label, value, max, tone = "accent", compact }: Omit<SkyVital,"id"> & { compact?: boolean }): React.JSX.Element {
  return <div className={"sky-meter" + (compact ? " sky-meter--compact" : "")} data-tone={tone}><div className="sky-meter__heading"><span>{label}</span><strong>{number(value)} <small>/ {number(max)}</small></strong></div><div className="sky-meter__track" role="meter" aria-label={label} aria-valuemin={0} aria-valuemax={Math.max(1, Number.isFinite(max) ? max : 1)} aria-valuenow={skyMeterFraction(value,max)*Math.max(1,Number.isFinite(max)?max:1)}><i style={{ transform: `scaleX(${skyMeterFraction(value,max)})` }} /></div></div>;
}
export function SkyVitalsHud({ vitals, label = "Character condition", compact = false }: { vitals: readonly SkyVital[]; label?: string; compact?: boolean }): React.JSX.Element {
  return <section className={"sky-vitals-hud" + (compact ? " sky-vitals-hud--compact" : "")} aria-label={label}><span className="sky-widget-caption">{label}</span>{vitals.map(vital => <SkyMeterBar key={vital.id} {...vital} compact={compact} />)}</section>;
}
export function SkyCompassHud({ heading = 0, location, objective }: { heading?: number; location?: string; objective?: string }): React.JSX.Element {
  const angle = Number.isFinite(heading) ? (heading % 360 + 360) % 360 : 0;
  const directions = ["N","NE","E","SE","S","SW","W","NW"];
  return <section className="sky-compass-hud" aria-label={`Heading ${Math.round(angle)} degrees${location ? ", " + location : ""}`}><div className="sky-compass-hud__location">{location ?? "The open road"}</div><div className="sky-compass-hud__ruler">{[-2,-1,0,1,2].map(offset => <span key={offset} className={offset === 0 ? "is-current" : ""}>{directions[((Math.round(angle / 45) + offset) % 8 + 8) % 8]}</span>)}</div><div className="sky-compass-hud__bearing">{Math.round(angle)}° {objective && <span>· {objective}</span>}</div></section>;
}
export function SkyCharacterCard({ name, subtitle, level, avatar, children }: { name: string; subtitle?: string; level?: number; avatar?: string; children?: React.ReactNode }): React.JSX.Element {
  return <article className="sky-character-card"><div className="sky-character-card__identity">{avatar ? <img src={avatar} alt="" draggable={false} /> : <span className="sky-character-card__monogram">{name.split(/\s+/).map(word => word[0]).slice(0,2).join("")}</span>}<div><h3>{name}</h3>{subtitle && <p>{subtitle}</p>}</div>{level !== undefined && <span className="sky-character-card__level">LVL <strong>{number(level)}</strong></span>}</div>{children && <div className="sky-character-card__content">{children}</div>}</article>;
}
export interface SkyEquipmentHudItem { id: string; label: string; name: string; icon?: React.ReactNode; }
export function SkyEquipmentHud({ items, label = "Readied equipment" }: { items: readonly SkyEquipmentHudItem[]; label?: string }): React.JSX.Element {
  return <section className="sky-equipment-hud" aria-label={label}><span className="sky-widget-caption">{label}</span><div>{items.map(item => <article key={item.id}>{item.icon && <span className="sky-equipment-hud__icon">{item.icon}</span>}<small>{item.label}</small><strong>{item.name}</strong></article>)}</div></section>;
}
export function SkyResourceWidget({ rows, label = "Field supplies" }: { rows: readonly { id: string; label: string; value: React.ReactNode; hint?: string }[]; label?: string }): React.JSX.Element {
  return <section className="sky-resource-widget" aria-label={label}><span className="sky-widget-caption">{label}</span><dl>{rows.map(row => <div key={row.id}><dt>{row.label}</dt><dd>{row.value}{row.hint && <small>{row.hint}</small>}</dd></div>)}</dl></section>;
}
export function SkyClockWidget({ time, date, location }: { time: string; date?: string; location?: string }): React.JSX.Element {
  return <section className="sky-clock-widget" aria-label={"Time " + time}><span className="sky-widget-caption">{location ?? "The field chronicle"}</span><strong>{time}</strong>{date && <span>{date}</span>}</section>;
}
export interface SkyEffectItem { id: string; name: string; detail?: string; hostile?: boolean; }
export function SkyEffectStack({ effects, limit = 4, total = effects.length }: { effects: readonly SkyEffectItem[]; limit?: number; total?: number }): React.JSX.Element {
  return <section className="sky-effect-stack" aria-label="Active effects"><span className="sky-widget-caption">Active influences <b>{total}</b></span>{effects.slice(0,Math.max(1,limit)).map(effect => <article key={effect.id} data-harmful={effect.hostile}><i /><span>{effect.name}</span>{effect.detail && <small>{effect.detail}</small>}</article>)}{!effects.length && <p>No active influences</p>}{total > limit && <small className="sky-effect-stack__more">+ {total-limit} more</small>}</section>;
}
export interface SkyQuestObjective { id: string; label: string; complete?: boolean; current?: number; target?: number; }
export function SkyQuestTracker({ title, subtitle, objectives }: { title: string; subtitle?: string; objectives: readonly SkyQuestObjective[] }): React.JSX.Element {
  return <section className="sky-quest-tracker"><span className="sky-widget-caption">Tracked objective</span><h3>{title}</h3>{subtitle && <p>{subtitle}</p>}<ol>{objectives.map(objective => <li key={objective.id} data-complete={objective.complete}><i aria-hidden="true">{objective.complete ? "✓" : ""}</i><span>{objective.label}</span>{objective.target !== undefined && <small>{number(objective.current ?? 0)} / {number(objective.target)}</small>}</li>)}</ol></section>;
}
export interface SkyNotification { id: string; title: string; description?: string; tone?: SkyWidgetTone; }
export function SkyNotificationStack({ notices, onDismiss, autoDismissMs = 3000 }: { notices: readonly SkyNotification[]; onDismiss?(id: string): void; autoDismissMs?: number }): React.JSX.Element {
  const onDismissRef = React.useRef(onDismiss);
  onDismissRef.current = onDismiss;
  const scheduledRef = React.useRef<Map<string, number>>(new Map());
  React.useEffect(() => {
    if (!onDismissRef.current || autoDismissMs <= 0) return;
    const activeIds = new Set(notices.map(n => n.id));
    for (const [id, timer] of scheduledRef.current.entries()) {
      if (!activeIds.has(id)) {
        window.clearTimeout(timer);
        scheduledRef.current.delete(id);
      }
    }
    for (const notice of notices) {
      if (!scheduledRef.current.has(notice.id)) {
        const timer = window.setTimeout(() => {
          scheduledRef.current.delete(notice.id);
          onDismissRef.current?.(notice.id);
        }, autoDismissMs);
        scheduledRef.current.set(notice.id, timer);
      }
    }
  }, [notices, autoDismissMs]);
  React.useEffect(() => {
    return () => {
      for (const timer of scheduledRef.current.values()) window.clearTimeout(timer);
      scheduledRef.current.clear();
    };
  }, []);
  return <div className="sky-notification-stack" aria-live="polite">{notices.map(notice => <article key={notice.id} data-tone={notice.tone ?? "accent"}><i /><div><strong>{notice.title}</strong>{notice.description && <p>{notice.description}</p>}</div>{onDismiss && <button type="button" aria-label={"Dismiss " + notice.title} onClick={() => onDismiss(notice.id)}>×</button>}</article>)}</div>;
}
export interface SkyWheelAction { id: string; label: string; hint?: string; icon?: React.ReactNode; disabled?: boolean; }
export function SkyActionWheel({ actions, label = "Quick actions", onSelect }: { actions: readonly SkyWheelAction[]; label?: string; onSelect(id: string): void }): React.JSX.Element {
  const [focused, setFocused] = React.useState(0);
  const items = actions.slice(0,8);
  const buttons = React.useRef<Array<HTMLButtonElement | null>>([]);
  return <div className="sky-action-wheel" role="group" aria-label={label} onKeyDown={event => { if (!["ArrowLeft","ArrowRight","ArrowUp","ArrowDown"].includes(event.key) || !items.length) return; event.preventDefault(); event.stopPropagation(); const next = (focused + (event.key === "ArrowRight" || event.key === "ArrowDown" ? 1 : items.length-1)) % items.length; setFocused(next); buttons.current[next]?.focus(); }}><div className="sky-action-wheel__center"><span>{label}</span><small>CHOOSE AN ACTION</small></div>{items.map((action,index) => { const angle = -Math.PI/2 + index * Math.PI*2/Math.max(1,items.length); return <button type="button" key={action.id} ref={node => { buttons.current[index] = node; }} disabled={action.disabled} style={{ left: `${50 + Math.cos(angle)*35}%`, top: `${50 + Math.sin(angle)*35}%` }} onFocus={() => setFocused(index)} onClick={() => onSelect(action.id)}>{action.icon}<strong>{action.label}</strong>{action.hint && <small>{action.hint}</small>}</button>; })}</div>;
}
export interface SkyTimelineEntry { id: string; title: string; detail?: string; time?: string; tone?: SkyWidgetTone; }
export function SkyTimeline({ entries, label = "Recent events" }: { entries: readonly SkyTimelineEntry[]; label?: string }): React.JSX.Element {
  return <ol className="sky-timeline" aria-label={label}>{entries.map(entry => <li key={entry.id} data-tone={entry.tone}><i /><div><strong>{entry.title}</strong>{entry.detail && <p>{entry.detail}</p>}</div>{entry.time && <time>{entry.time}</time>}</li>)}</ol>;
}
export interface SkyGuideStep { id: string; title: string; description: string; }
export function SkyStepGuide({ steps, active, onStepChange }: { steps: readonly SkyGuideStep[]; active: number; onStepChange(index: number): void }): React.JSX.Element {
  return <ol className="sky-step-guide">{steps.map((step,index) => <li key={step.id}><button type="button" aria-current={active === index ? "step" : undefined} onClick={() => onStepChange(index)}><b>{String(index+1).padStart(2,"0")}</b><span><strong>{step.title}</strong><small>{step.description}</small></span></button></li>)}</ol>;
}
export function SkyKeybind({ keys, children }: { keys: readonly string[]; children: React.ReactNode }): React.JSX.Element { return <span className="sky-keybind">{keys.map(key => <kbd key={key}>{key}</kbd>)}<span>{children}</span></span>; }
export function SkySkeleton({ lines = 3, label = "Loading" }: { lines?: number; label?: string }): React.JSX.Element { return <div className="sky-skeleton" role="status" aria-label={label}>{Array.from({ length: Math.max(1,Math.min(8,lines)) },(_,index) => <i key={index} style={{ width: index === lines-1 ? "62%" : "100%" }} />)}</div>; }
export function SkyPagination({ page, pages, onPageChange }: { page: number; pages: number; onPageChange(page: number): void }): React.JSX.Element { const count = Math.max(1,Math.floor(Number.isFinite(pages)?pages:1)); const current = Math.max(1,Math.min(count,Math.floor(Number.isFinite(page)?page:1))); return <nav className="sky-pagination" aria-label="Pagination"><button type="button" disabled={current <= 1} onClick={() => onPageChange(current-1)}>Previous</button><span>{current} <small>/ {count}</small></span><button type="button" disabled={current >= count} onClick={() => onPageChange(current+1)}>Next</button></nav>; }
export interface SkyPopupProps { open: boolean; title: string; description?: string; children?: React.ReactNode; actions?: React.ReactNode; onDismiss(): void; wide?: boolean; }
export function SkyPopup({ open, title, description, children, actions, onDismiss, wide }: SkyPopupProps): React.JSX.Element | null {
  const dialog = React.useRef<HTMLDivElement>(null);
  const dismiss = React.useRef(onDismiss); dismiss.current = onDismiss;
  const id = React.useId();
  React.useEffect(() => { if (!open) return; const releaseScope = enterSkyInputScope(); const previous = document.activeElement as HTMLElement | null; const frame = requestAnimationFrame(() => dialog.current?.querySelector<HTMLElement>('input,button,[tabindex="0"]')?.focus()); const key = (event: KeyboardEvent): void => { if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); dismiss.current(); } else if (event.key === "Tab") { const items = Array.from(dialog.current?.querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),textarea,select,[tabindex="0"]') ?? []).filter(node => node.getClientRects().length > 0); if (!items.length) { event.preventDefault(); dialog.current?.focus(); return; } const first = items[0], last = items[items.length-1]; if (event.shiftKey && (document.activeElement === first || !dialog.current?.contains(document.activeElement))) { event.preventDefault(); last.focus(); } else if (!event.shiftKey && (document.activeElement === last || !dialog.current?.contains(document.activeElement))) { event.preventDefault(); first.focus(); } } }; document.addEventListener("keydown",key,true); return () => { releaseScope(); cancelAnimationFrame(frame); document.removeEventListener("keydown",key,true); if (previous?.isConnected) previous.focus(); }; },[open]);
  if (!open) return null;
  return <div className="sky-popup-backdrop" onPointerDown={event => { if (event.target === event.currentTarget) onDismiss(); }}><div ref={dialog} tabIndex={-1} className={"sky-popup" + (wide ? " sky-popup--wide" : "")} role="dialog" aria-modal="true" aria-labelledby={id} aria-describedby={description ? id+"-description" : undefined}><header><div><span className="sky-widget-caption">SKYULTRA INTERFACE</span><h2 id={id}>{title}</h2></div><button type="button" aria-label="Close dialog" onClick={onDismiss}>×</button></header>{description && <p id={id+"-description"} className="sky-popup__description">{description}</p>}<div className="sky-popup__body">{children}</div>{actions && <footer>{actions}</footer>}</div></div>;
}
export interface SkyCommand { id: string; label: string; detail?: string; keywords?: string; icon?: React.ReactNode; }
export function SkyCommandPalette({ open, commands, onSelect, onDismiss }: { open: boolean; commands: readonly SkyCommand[]; onSelect(id: string): void; onDismiss(): void }): React.JSX.Element {
  const [query,setQuery] = React.useState("");
  React.useEffect(() => { if (open) setQuery(""); },[open]);
  const filtered = commands.filter(command => (command.label+" "+(command.keywords??"")).toLowerCase().includes(query.trim().toLowerCase())).slice(0,30);
  return <SkyPopup open={open} title="Go anywhere" description="Find a section, tool, or interface action." onDismiss={onDismiss}><input className="sky-command-search" aria-label="Search commands" placeholder="Type a command…" value={query} onChange={event=>setQuery(event.currentTarget.value)} onKeyDown={event=>{ if(event.key === "Enter" && filtered[0]) { event.preventDefault(); onSelect(filtered[0].id); } }} /><div className="sky-command-list">{filtered.map(command=><button type="button" key={command.id} onClick={()=>onSelect(command.id)}>{command.icon}<span><strong>{command.label}</strong>{command.detail && <small>{command.detail}</small>}</span><span>↵</span></button>)}{!filtered.length && <p>No matching tools.</p>}</div></SkyPopup>;
}
