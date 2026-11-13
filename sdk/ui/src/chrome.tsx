import * as React from 'react';
import {
  defaultSkyUltraHotkey,
  formatSkyUltraHotkey,
  normalizeSkyUltraHotkey,
  type SkyUltraHotkeyBinding,
  type SkyUltraHotkeyDevice,
  type SkyWebViewAPI,
} from '@skyultra/sdk';
import { enterSkyInputScope } from './interaction.js';

/** CSS-native SVG cursor: no React mousemove loop, RAF, or per-frame bridge calls. */
export function createSkyUltraCursorImage(accent='#d8c49c'):string {
  const color=/^#[0-9a-f]{6}$/i.test(accent)?accent:'#d8c49c';
  return 'data:image/svg+xml,'+encodeURIComponent(`<svg xmlns="http://www.w3.org/2000/svg" width="36" height="42"><path d="M4 3L26 24L17 24L14 36Z" fill="#080b10" stroke="#080b10" stroke-width="4"/><path d="M4 3L26 24L17 24L14 36Z" fill="#080b10" stroke="${color}" stroke-width="1.4"/><path d="M6 7L23 22L15 21L14 30Z" fill="${color}"/><path d="M5 5L15 21L14 30" fill="none" stroke="#fff" stroke-opacity=".8" stroke-width=".8"/><path d="M21 29L26 34L21 39L16 34Z" fill="#080b10" stroke="${color}"/></svg>`);
}
export const skyUltraCursorImage=createSkyUltraCursorImage();
export function SkyCursorScope({ children, className = '', disabled = false, bridge, nativeMode = 'native', accent='#d8c49c' }: {children?: React.ReactNode; className?: string; disabled?: boolean; bridge?:SkyWebViewAPI; nativeMode?:'native'|'hidden';accent?:string}): React.JSX.Element {
  React.useEffect(()=>{
    if(!bridge)return;
    void bridge.request('ui.setCursor',{mode:nativeMode,accent}).catch(()=>undefined);
    return()=>{void bridge.request('ui.setCursor',{mode:'native'}).catch(()=>undefined);};
  },[bridge,nativeMode,accent]);
  return <div className={'sky-cursor-scope '+className} data-sky-cursor={disabled?'system':'aether'} style={disabled?undefined:{'--sky-cursor':`url("${createSkyUltraCursorImage(accent)}") 4 3, auto`} as React.CSSProperties}>{children}</div>;
}
/** Original emblem plus crisp live typography; accepts any author's own artwork. */
export function SkyBrand({emblem, title='SkyUltra',subtitle='Advanced Webview Interface'}:{emblem:string;title?:string;subtitle?:string}):React.JSX.Element {
  return (
    <span className="sky-brand">
      <span className="sky-brand__emblem-wrap">
        <img src={emblem} alt="" draggable={false}/>
      </span>
      <span className="sky-brand__copy">
        <strong>{title}</strong>
        <small>{subtitle}</small>
      </span>
    </span>
  );
}
export interface SkyWelcomeBannerProps {
  title:string;description?:string;eyebrow?:string;
  emblem:string;
  /** Optional viewport into the author's original artwork; no resized bitmap is generated. */
  emblemViewBox?:string;
  emblemWidth?:number;emblemHeight?:number;
  brandName?:string;brandCaption?:string;
  facts?:readonly {id:string;label:string;value:React.ReactNode}[];
  actions?:React.ReactNode;status?:React.ReactNode;className?:string;
}
/** Static themed welcome panel with a framed emblem and live, crisp typography. */
export function SkyWelcomeBanner({title,description,eyebrow='YOUR WORLD, IN FOCUS',emblem,emblemViewBox,emblemWidth=2400,emblemHeight=1341,brandName='SkyUltra',brandCaption='Advanced Webview Interface',facts=[],actions,status,className=''}:SkyWelcomeBannerProps):React.JSX.Element {
  return <section className={'sky-welcome '+className}>
    <div className="sky-welcome__content"><span className="eyebrow">{eyebrow}</span><h1>{title}<span className="title-period">.</span></h1>{description&&<p>{description}</p>}
      {facts.length>0&&<dl className="sky-welcome__facts">{facts.map(fact=><div key={fact.id}><dt>{fact.label}</dt><dd>{fact.value}</dd></div>)}</dl>}
      {actions&&<div className="sky-welcome__actions">{actions}</div>}
    </div>
    <div className="sky-welcome__brand" aria-label={brandName+' '+brandCaption}>
      <div className="sky-welcome__astrolabe"><svg className="sky-welcome__orbit" viewBox="0 0 240 240" fill="none" aria-hidden="true"><circle cx="120" cy="120" r="112"/><circle cx="120" cy="120" r="104"/><path d="M34 65a104 104 0 0 1 172 0M34 175a104 104 0 0 0 172 0"/><path d="M120 0v18M120 222v18M0 120h18M222 120h18M35 35l12 12M193 193l12 12M35 205l12-12M193 47l12-12"/>{Array.from({length:24},(_,index)=><path key={index} d="M120 12v4" transform={`rotate(${index*15} 120 120)`}/>)}</svg>
        {emblemViewBox?<svg className="sky-welcome__emblem" viewBox={emblemViewBox} aria-hidden="true"><image href={emblem} xlinkHref={emblem} width={emblemWidth} height={emblemHeight}/></svg>:<img className="sky-welcome__emblem" src={emblem} alt="" draggable={false}/>}
      </div><strong>{brandName}</strong><span>{brandCaption}</span>
    </div>
    {status&&<footer className="sky-welcome__status"><span>{status}</span><i aria-hidden="true"/><span>{brandCaption}</span></footer>}
  </section>;
}
export interface SkyNavigationItem<T extends string=string>{id:T;title:string;note?:string;icon?:React.ReactNode;shortcut?:string;}
export function SkyNavigationRail<const T extends string>({items,value,onValueChange,onIntent,brand,footer,disabled=false,label='Navigation',caption,className=''}:{items:readonly SkyNavigationItem<T>[];value:T;onValueChange(value:NoInfer<T>):void;onIntent?(value:NoInfer<T>):void;brand?:React.ReactNode;footer?:React.ReactNode;disabled?:boolean;label?:string;caption?:string;className?:string}):React.JSX.Element {
  return (
    <aside className={'sky-navigation-rail '+className}>
      {brand}
      {caption&&<div className="rail-section"><span>{caption}</span><b>{String(items.length).padStart(2,'0')} MODULES</b></div>}
      <nav aria-label={label}>
        {items.map(item=>(
          <button
            type="button"
            key={item.id}
            className={'nav-item'+(value===item.id?' is-active':'')}
            aria-current={value===item.id?'page':undefined}
            disabled={disabled&&value!==item.id}
            onClick={()=>onValueChange(item.id)}
            onPointerEnter={()=>onIntent?.(item.id)} onFocus={()=>onIntent?.(item.id)}
          >
            <span className="nav-item__icon" aria-hidden="true">{item.icon}</span>
            <span className="nav-item__copy">
              <strong>{item.title}</strong>
              {item.note&&<small>{item.note}</small>}
            </span>
            {item.shortcut&&<kbd className="nav-item__key">{item.shortcut}</kbd>}
          </button>
        ))}
      </nav>
      {footer}
    </aside>
  );
}
export function SkyMenuHeader({title, eyebrow, logo, onHelp, helpTitle = 'Contextual Help (Ctrl + F3)', onClose, children}: {title:string;eyebrow?:string;logo?:string;onHelp?:()=>void;helpTitle?:string;onClose():void;children?:React.ReactNode}): React.JSX.Element {
  return (
    <header className="sky-menu-header">
      {logo&&<img src={logo} alt="" draggable={false}/>}
      <div className="sky-menu-heading">
        <small><i className="sky-menu-heading__pip" aria-hidden="true"/>{eyebrow}</small>
        <strong>{title}</strong>
      </div>
      <div className="sky-menu-tools">{children}</div>
      {onHelp && (
        <button type="button" className="sky-menu-help" aria-label="Contextual help" title={helpTitle} onClick={onHelp}>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" aria-hidden="true">
            <circle cx="12" cy="12" r="9"/>
            <path d="M9.6 9.2a2.5 2.5 0 1 1 3.7 2.2c-.9.5-1.3 1-1.3 2M12 16.8h.01"/>
          </svg>
          <span>?</span>
        </button>
      )}
      <button type="button" className="sky-menu-close" aria-label="Close page" title="Close (Escape)" onClick={onClose}>
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"><path d="m7 7 10 10M17 7 7 17"/></svg>
        <span>ESC</span>
      </button>
    </header>
  );
}
export function SkyMenuFooter({children}: {children?:React.ReactNode}):React.JSX.Element {return <footer className="sky-menu-footer">{children}</footer>;}
export function SkySettingsSection({title,description,children}: {title:string;description?:string;children:React.ReactNode}):React.JSX.Element {return <section className="sky-settings-section"><header><h2>{title}</h2>{description&&<p>{description}</p>}</header><div>{children}</div></section>;}

const KEYBOARD_TOKEN_PRESETS: readonly string[] = [
  'Shift', 'Ctrl', 'Alt', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12',
  'Home', 'Insert', 'Delete', 'PageUp', 'PageDown', 'Tab', 'Backquote',
  'K', 'M', 'O', 'P', 'U', 'I', 'J', 'L',
];

const GAMEPAD_TOKEN_PRESETS: readonly { token: string; label: string }[] = [
  { token: 'Back', label: 'Back / View' },
  { token: 'Start', label: 'Start / Menu' },
  { token: 'LB', label: 'LB (L1)' },
  { token: 'RB', label: 'RB (R1)' },
  { token: 'LT', label: 'LT (L2)' },
  { token: 'RT', label: 'RT (R2)' },
  { token: 'LeftThumb', label: 'L-Stick Click (L3)' },
  { token: 'RightThumb', label: 'R-Stick Click (R3)' },
  { token: 'A', label: 'A / Cross' },
  { token: 'B', label: 'B / Circle' },
  { token: 'X', label: 'X / Square' },
  { token: 'Y', label: 'Y / Triangle' },
  { token: 'DPadUp', label: 'D-Pad Up' },
  { token: 'DPadDown', label: 'D-Pad Down' },
  { token: 'DPadLeft', label: 'D-Pad Left' },
  { token: 'DPadRight', label: 'D-Pad Right' },
];

const VR_TOKEN_PRESETS: readonly { token: string; label: string }[] = [
  { token: 'LeftGrip', label: 'Left Grip' },
  { token: 'RightGrip', label: 'Right Grip' },
  { token: 'LeftTrigger', label: 'Left Trigger' },
  { token: 'RightTrigger', label: 'Right Trigger' },
  { token: 'LeftBY', label: 'Left B / Y' },
  { token: 'RightBY', label: 'Right B / Y' },
  { token: 'LeftAX', label: 'Left A / X' },
  { token: 'RightAX', label: 'Right A / X' },
  { token: 'LeftThumbstick', label: 'Left Thumbstick Click' },
  { token: 'RightThumbstick', label: 'Right Thumbstick Click' },
  { token: 'HeadsetProximity', label: 'VR Headset Button / Proximity' },
  { token: 'LeftSystem', label: 'Left System / Menu' },
  { token: 'RightSystem', label: 'Right System / Menu' },
];

function domKeyboardEventToToken(event: KeyboardEvent): string | null {
  if (event.key === 'Escape') return null;
  if (event.key === 'Shift') return 'Shift';
  if (event.key === 'Control') return 'Ctrl';
  if (event.key === 'Alt') return 'Alt';
  if (/^F([1-9]|1[0-2])$/i.test(event.key)) return event.key.toUpperCase();
  if (/^[a-z0-9]$/i.test(event.key)) return event.key.toUpperCase();
  const map: Record<string, string> = {
    ' ': 'Space',
    Tab: 'Tab',
    Enter: 'Enter',
    Backspace: 'Backspace',
    Insert: 'Insert',
    Delete: 'Delete',
    Home: 'Home',
    End: 'End',
    PageUp: 'PageUp',
    PageDown: 'PageDown',
    ArrowUp: 'Up',
    ArrowDown: 'Down',
    ArrowLeft: 'Left',
    ArrowRight: 'Right',
    '`': 'Backquote',
    '~': 'Backquote',
    '-': 'Minus',
    '=': 'Equals',
    '[': 'BracketLeft',
    ']': 'BracketRight',
    '\\': 'Backslash',
    ';': 'Semicolon',
    "'": 'Quote',
    ',': 'Comma',
    '.': 'Period',
    '/': 'Slash',
  };
  return map[event.key] ?? null;
}

export interface SkyHotkeyEditorProps {
  value: SkyUltraHotkeyBinding;
  onChange(next: SkyUltraHotkeyBinding): void;
  bridge?: SkyWebViewAPI;
  disabled?: boolean;
  allowEmptyKeyboard?: boolean;
  defaultBinding?: SkyUltraHotkeyBinding;
  label?: string;
}

/**
 * Multi-device hotkey chord editor (up to 3 inputs per chord) for Keyboard,
 * Gamepad Controllers, and VR Headset / Controllers. Reusable by any SkyUltra mod.
 */
export function SkyHotkeyEditor({
  value,
  onChange,
  bridge,
  disabled = false,
  allowEmptyKeyboard = false,
  defaultBinding = defaultSkyUltraHotkey,
  label = 'Hotkey binding',
}: SkyHotkeyEditorProps): React.JSX.Element {
  const normalized = React.useMemo(
    () => normalizeSkyUltraHotkey(value, allowEmptyKeyboard),
    [value, allowEmptyKeyboard],
  );
  const [activeDevice, setActiveDevice] = React.useState<SkyUltraHotkeyDevice>('keyboard');
  const [recording, setRecording] = React.useState(false);
  const recordedRef = React.useRef<string[]>([]);

  React.useEffect(() => {
    if (!recording || disabled) return;
    recordedRef.current = [];
    const releaseScope = enterSkyInputScope();

    const appendToken = (device: SkyUltraHotkeyDevice, token: string): void => {
      if (device !== activeDevice) {
        setActiveDevice(device);
      }
      const nextChord = [...recordedRef.current];
      if (!nextChord.includes(token) && nextChord.length < 3) {
        nextChord.push(token);
        recordedRef.current = nextChord;
        onChange(
          normalizeSkyUltraHotkey(
            { ...normalized, [device]: nextChord },
            allowEmptyKeyboard,
          ),
        );
        if (nextChord.length >= 3) {
          setRecording(false);
        }
      }
    };

    const offBridge = bridge?.on<{ device?: string; token?: string }>('skyultra.inputToken', (payload) => {
      if (!payload?.token || typeof payload.token !== 'string') return;
      const dev: SkyUltraHotkeyDevice =
        payload.device === 'gamepad' ? 'gamepad' : payload.device === 'vr' ? 'vr' : 'keyboard';
      appendToken(dev, payload.token);
    });

    const onKeyDown = (event: KeyboardEvent): void => {
      event.preventDefault();
      event.stopPropagation();
      if (event.key === 'Escape') {
        setRecording(false);
        return;
      }
      const token = domKeyboardEventToToken(event);
      if (token) {
        appendToken('keyboard', token);
      }
    };

    window.addEventListener('keydown', onKeyDown, true);
    return () => {
      offBridge?.();
      window.removeEventListener('keydown', onKeyDown, true);
      releaseScope();
    };
  }, [recording, disabled, activeDevice, bridge, normalized, onChange, allowEmptyKeyboard]);

  const currentChord = normalized[activeDevice];

  const toggleToken = (token: string): void => {
    if (disabled) return;
    const exists = currentChord.includes(token);
    let nextChord: string[];
    if (exists) {
      nextChord = currentChord.filter((item) => item !== token);
      if (!allowEmptyKeyboard && activeDevice === 'keyboard' && nextChord.length === 0) {
        return;
      }
    } else {
      if (currentChord.length >= 3) {
        nextChord = [...currentChord.slice(0, 2), token];
      } else {
        nextChord = [...currentChord, token];
      }
    }
    onChange(
      normalizeSkyUltraHotkey(
        { ...normalized, [activeDevice]: nextChord },
        allowEmptyKeyboard,
      ),
    );
  };

  const removeSlot = (index: number): void => {
    if (disabled) return;
    const nextChord = currentChord.filter((_, idx) => idx !== index);
    if (!allowEmptyKeyboard && activeDevice === 'keyboard' && nextChord.length === 0) return;
    onChange(
      normalizeSkyUltraHotkey(
        { ...normalized, [activeDevice]: nextChord },
        allowEmptyKeyboard,
      ),
    );
  };

  return (
    <div className="sky-hotkey-editor" role="group" aria-label={label}>
      <div className="sky-hotkey-devices" role="tablist" aria-label="Input device">
        {(
          [
            { id: 'keyboard', title: 'Keyboard', subtitle: formatSkyUltraHotkey(normalized, 'keyboard') },
            { id: 'gamepad', title: 'Gamepad Controller', subtitle: formatSkyUltraHotkey(normalized, 'gamepad') },
            { id: 'vr', title: 'VR Headset & Controllers', subtitle: formatSkyUltraHotkey(normalized, 'vr') },
          ] as const
        ).map((tab) => (
          <button
            key={tab.id}
            type="button"
            role="tab"
            aria-selected={activeDevice === tab.id}
            className={'sky-hotkey-device-tab' + (activeDevice === tab.id ? ' is-active' : '')}
            disabled={disabled}
            onClick={() => {
              setRecording(false);
              setActiveDevice(tab.id);
            }}
          >
            <small>{tab.title}</small>
            <strong>{tab.subtitle}</strong>
          </button>
        ))}
      </div>

      <div className="sky-hotkey-chord-stage">
        <div className="sky-hotkey-slots" aria-label={`${activeDevice} chord slots (up to 3 inputs)`}>
          {[0, 1, 2].map((slotIndex) => {
            const token = currentChord[slotIndex];
            return (
              <React.Fragment key={slotIndex}>
                {slotIndex > 0 && <span className="sky-hotkey-plus" aria-hidden="true">+</span>}
                <div className={'sky-hotkey-slot' + (token ? ' is-filled' : ' is-empty') + (recording ? ' is-recording' : '')}>
                  <small>INPUT {slotIndex + 1}</small>
                  <strong>{token ?? (recording ? 'Press input…' : 'Optional')}</strong>
                  {token && (allowEmptyKeyboard || activeDevice !== 'keyboard' || currentChord.length > 1) && (
                    <button
                      type="button"
                      className="sky-hotkey-slot__remove"
                      disabled={disabled}
                      aria-label={`Remove ${token}`}
                      title={`Remove ${token}`}
                      onClick={() => removeSlot(slotIndex)}
                    >
                      ×
                    </button>
                  )}
                </div>
              </React.Fragment>
            );
          })}
        </div>

        <div className="sky-hotkey-actions">
          <button
            type="button"
            className={'control-button sky-hotkey-record' + (recording ? ' is-active' : '')}
            disabled={disabled}
            onClick={() => setRecording((prev) => !prev)}
          >
            {recording ? 'Finish recording (Esc)' : 'Record up to 3 inputs'}
          </button>
          {(allowEmptyKeyboard || activeDevice !== 'keyboard') && currentChord.length > 0 && (
            <button
              type="button"
              className="control-button"
              disabled={disabled}
              onClick={() => {
                setRecording(false);
                onChange({ ...normalized, [activeDevice]: [] });
              }}
            >
              Unbind {activeDevice}
            </button>
          )}
          <button
            type="button"
            className="control-button"
            disabled={disabled}
            onClick={() => {
              setRecording(false);
              onChange(
                normalizeSkyUltraHotkey(
                  { ...normalized, [activeDevice]: [...defaultBinding[activeDevice]] },
                  allowEmptyKeyboard,
                ),
              );
            }}
          >
            Reset default
          </button>
        </div>
      </div>

      <div className="sky-hotkey-palette">
        <span className="eyebrow">
          {activeDevice === 'keyboard'
            ? 'CLICK TO ADD OR REMOVE KEYBOARD KEYS (UP TO 3)'
            : activeDevice === 'gamepad'
              ? 'CLICK TO ADD OR REMOVE GAMEPAD CONTROLLER BUTTONS (UP TO 3)'
              : 'CLICK TO ADD OR REMOVE VR HEADSET & CONTROLLER INPUTS (UP TO 3)'}
        </span>
        <div className="sky-hotkey-tokens">
          {activeDevice === 'keyboard' &&
            KEYBOARD_TOKEN_PRESETS.map((token) => (
              <button
                key={token}
                type="button"
                className={'sky-hotkey-token' + (currentChord.includes(token) ? ' is-selected' : '')}
                aria-pressed={currentChord.includes(token)}
                disabled={disabled}
                onClick={() => toggleToken(token)}
              >
                {token}
              </button>
            ))}
          {activeDevice === 'gamepad' &&
            GAMEPAD_TOKEN_PRESETS.map(({ token, label: tokenLabel }) => (
              <button
                key={token}
                type="button"
                className={'sky-hotkey-token' + (currentChord.includes(token) ? ' is-selected' : '')}
                aria-pressed={currentChord.includes(token)}
                disabled={disabled}
                onClick={() => toggleToken(token)}
              >
                {tokenLabel}
              </button>
            ))}
          {activeDevice === 'vr' &&
            VR_TOKEN_PRESETS.map(({ token, label: tokenLabel }) => (
              <button
                key={token}
                type="button"
                className={'sky-hotkey-token' + (currentChord.includes(token) ? ' is-selected' : '')}
                aria-pressed={currentChord.includes(token)}
                disabled={disabled}
                onClick={() => toggleToken(token)}
              >
                {tokenLabel}
              </button>
            ))}
        </div>
      </div>
    </div>
  );
}
