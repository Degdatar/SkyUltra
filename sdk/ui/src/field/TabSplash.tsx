import * as React from 'react';
import { palettes, type Palette } from './model.js';

export type TabSplashTransitionMode = 'cinematic' | 'minimal' | 'instant';

export interface TabSplashProps {
  title: string;
  subtitle?: string;
  stageLabel?: string;
  theme?: Palette;
  mode?: TabSplashTransitionMode;
  reducedMotion?: boolean;
  preview?: boolean;
  tabOverlay?: boolean;
  onDismiss?: () => void;
}

export function TabSplash({
  title,
  subtitle = 'Preparing interface layout & D3D11 frame surface',
  stageLabel = 'MOUNTING MODULE',
  theme = 'dragonstone',
  mode = 'cinematic',
  reducedMotion = false,
  preview = false,
  tabOverlay = false,
  onDismiss,
}: TabSplashProps): React.JSX.Element {
  const palette = palettes.find((entry) => entry.value === theme) ?? palettes[0];
  const family = palette.family;
  const still = reducedMotion || mode === 'instant';

  return (
    <div
      className={
        'sky-tab-splash surface' +
        ` sky-tab-splash--${family}` +
        (mode === 'minimal' ? ' is-minimal' : '') +
        (still ? ' is-still' : '') +
        (tabOverlay ? ' is-tab-overlay' : '') +
        (preview ? ' is-preview' : '')
      }
      data-theme-family={family}
      role={preview ? 'dialog' : 'status'}
      aria-live="polite"
      aria-label={`Loading ${title}`}
      onClick={preview ? onDismiss : undefined}
    >
      <div className="sky-tab-splash__ambient" aria-hidden="true" />
      {family === 'neon' && <div className="sky-tab-splash__scan" aria-hidden="true" />}

      <div className="sky-tab-splash__core" onClick={preview ? (e) => e.stopPropagation() : undefined}>
        <div className="sky-tab-splash__emblem" aria-hidden="true">
          <svg className="sky-tab-splash__rings" viewBox="0 0 180 180">
            <circle className="sky-tab-splash__ring sky-tab-splash__ring--outer" cx="90" cy="90" r="82" />
            <circle className="sky-tab-splash__ring sky-tab-splash__ring--mid" cx="90" cy="90" r="68" />
            <circle className="sky-tab-splash__ring sky-tab-splash__ring--inner" cx="90" cy="90" r="54" />
            <g className="sky-tab-splash__ticks">
              <line x1="90" y1="2" x2="90" y2="12" />
              <line x1="90" y1="168" x2="90" y2="178" />
              <line x1="2" y1="90" x2="12" y2="90" />
              <line x1="168" y1="90" x2="178" y2="90" />
            </g>
          </svg>
          <div className="sky-tab-splash__logo-halo" />
          <img
            className="sky-tab-splash__logo"
            src="./SkyUltraLogoMark.png"
            alt="SkyUltra"
            draggable={false}
          />
        </div>

        <div className="sky-tab-splash__copy">
          <span className="sky-tab-splash__eyebrow">
            <i aria-hidden="true" />
            <span>{stageLabel}</span>
            <i aria-hidden="true" />
          </span>
          <h2 className="sky-tab-splash__title">{title}</h2>
          <p className="sky-tab-splash__subtitle">{subtitle}</p>
        </div>

        <div className="sky-tab-splash__progress" aria-hidden="true">
          <div className="sky-tab-splash__bar">
            <span className="sky-tab-splash__fill" />
            <span className="sky-tab-splash__sheen" />
          </div>
          <div className="sky-tab-splash__telemetry">
            <span className="sky-tab-splash__telemetry-chip">{palette.label.toUpperCase()}</span>
            <span className="sky-tab-splash__telemetry-sep" aria-hidden="true">·</span>
            <span className="sky-tab-splash__telemetry-chip">{palette.styleTag.toUpperCase()}</span>
          </div>
        </div>

        {preview && onDismiss && (
          <button type="button" className="control-button sky-tab-splash__dismiss" onClick={onDismiss}>
            Close Preview
          </button>
        )}
      </div>
    </div>
  );
}
