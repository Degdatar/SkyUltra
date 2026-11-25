import * as React from "react";
import { useEffect, useId, useMemo, useRef, useState } from "react";
import { enterSkyInputScope } from "./interaction.js";

export interface HsvColor {
  /** Hue in degrees [0, 360). */
  h: number;
  /** Saturation in [0, 1]. */
  s: number;
  /** Brightness / Value in [0, 1]. */
  v: number;
}

function clamp(n: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, n));
}

export function normalizeHexColor(input: string, fallback = "#4ed5e5"): string {
  const cleaned = input.trim().replace(/^#/, "");
  if (/^[0-9a-f]{6}$/i.test(cleaned)) return `#${cleaned.toLowerCase()}`;
  if (/^[0-9a-f]{3}$/i.test(cleaned)) {
    return `#${cleaned[0]}${cleaned[0]}${cleaned[1]}${cleaned[1]}${cleaned[2]}${cleaned[2]}`.toLowerCase();
  }
  return fallback.toLowerCase();
}

export function hexToHsv(hex: string): HsvColor {
  const norm = normalizeHexColor(hex);
  const r = parseInt(norm.slice(1, 3), 16) / 255;
  const g = parseInt(norm.slice(3, 5), 16) / 255;
  const b = parseInt(norm.slice(5, 7), 16) / 255;
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const d = max - min;
  let h = 0;
  if (d > 1e-5) {
    if (max === r) h = ((g - b) / d) % 6;
    else if (max === g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h *= 60;
    if (h < 0) h += 360;
  }
  const s = max > 1e-5 ? d / max : 0;
  return { h, s, v: max };
}

export function hsvToRgb(h: number, s: number, v: number): [number, number, number] {
  const hue = ((h % 360) + 360) % 360;
  const sat = clamp(s, 0, 1);
  const val = clamp(v, 0, 1);
  const c = val * sat;
  const x = c * (1 - Math.abs(((hue / 60) % 2) - 1));
  const m = val - c;
  let r1 = 0;
  let g1 = 0;
  let b1 = 0;
  if (hue < 60) {
    r1 = c;
    g1 = x;
  } else if (hue < 120) {
    r1 = x;
    g1 = c;
  } else if (hue < 180) {
    g1 = c;
    b1 = x;
  } else if (hue < 240) {
    g1 = x;
    b1 = c;
  } else if (hue < 300) {
    r1 = x;
    b1 = c;
  } else {
    r1 = c;
    b1 = x;
  }
  return [
    Math.round((r1 + m) * 255),
    Math.round((g1 + m) * 255),
    Math.round((b1 + m) * 255),
  ];
}

export function hsvToHex(h: number, s: number, v: number): string {
  const [r, g, b] = hsvToRgb(h, s, v);
  return (
    "#" +
    [r, g, b]
      .map((channel) => clamp(channel, 0, 255).toString(16).padStart(2, "0"))
      .join("")
  );
}

export interface SkyColorWheelProps {
  /** Active hex color (e.g. "#68b5ff") or "theme" when matched to active theme. */
  value: string;
  onChange(hex: string): void;
  /** Accessible label & optional header caption. */
  label?: string;
  /** Outer wheel diameter in pixels (default 158). */
  size?: number;
  /** Render inline wheel or a circular trigger orb that opens a floating wheel popover. */
  mode?: "inline" | "popover";
  /** Optional "Match Theme" reset action when the wheel controls a theme-syncable accent. */
  allowThemeMatch?: boolean;
  isThemeMatched?: boolean;
  themeColor?: string;
  onResetTheme?(): void;
  disabled?: boolean;
  className?: string;
  compact?: boolean;
}

/**
 * Outer 360-degree Hue wheel with an inscribed circular Brightness & Saturation wheel.
 * Reusable across settings, HUD studio, developer workbench, and community SkyUltra mods.
 */
export function SkyColorWheel({
  value,
  onChange,
  label = "Color picker",
  size = 158,
  mode = "inline",
  allowThemeMatch = false,
  isThemeMatched = false,
  themeColor = "#4ed5e5",
  onResetTheme,
  disabled = false,
  className = "",
  compact = false,
}: SkyColorWheelProps): React.JSX.Element {
  const effectiveHex = useMemo(
    () =>
      value.toLowerCase() === "theme"
        ? normalizeHexColor(themeColor)
        : normalizeHexColor(value, normalizeHexColor(themeColor)),
    [value, themeColor],
  );

  const [hsv, setHsv] = useState<HsvColor>(() => hexToHsv(effectiveHex));
  const [hexDraft, setHexDraft] = useState<string>(effectiveHex.toUpperCase());
  const [open, setOpen] = useState(false);

  const rootRef = useRef<HTMLDivElement>(null);
  const wheelRef = useRef<HTMLDivElement>(null);
  const innerRef = useRef<HTMLDivElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const dragTargetRef = useRef<"hue" | "inner" | null>(null);
  const lastEmittedRef = useRef<string>(effectiveHex);
  const labelId = useId();

  useEffect(() => {
    setHexDraft(effectiveHex.toUpperCase());
    if (effectiveHex.toLowerCase() !== lastEmittedRef.current.toLowerCase()) {
      setHsv(hexToHsv(effectiveHex));
      lastEmittedRef.current = effectiveHex;
    } else {
      const parsed = hexToHsv(effectiveHex);
      setHsv((prev) => ({
        h: parsed.s > 0.01 ? parsed.h : prev.h,
        s: parsed.s,
        v: parsed.v,
      }));
    }
  }, [effectiveHex]);

  // Paint the inner circular brightness & saturation wheel whenever hue changes.
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    let ctx: CanvasRenderingContext2D | null = null;
    try {
      ctx = canvas.getContext("2d");
    } catch {
      ctx = null;
    }
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    const img = ctx.createImageData(w, h);
    const data = img.data;
    const half = w / 2;
    for (let y = 0; y < h; y++) {
      const ny = (y + 0.5 - half) / half;
      const val = clamp((1 - ny) * 0.5, 0, 1);
      const maxX = Math.sqrt(Math.max(1e-5, 1 - ny * ny));
      for (let x = 0; x < w; x++) {
        const nx = (x + 0.5 - half) / half;
        const dist = Math.hypot(nx, ny);
        const offset = (y * w + x) * 4;
        if (dist > 1.02) {
          data[offset + 3] = 0;
          continue;
        }
        const sat = clamp((nx / maxX + 1) * 0.5, 0, 1);
        const [r, g, b] = hsvToRgb(hsv.h, sat, val);
        const alpha = dist > 0.96 ? clamp((1.02 - dist) / 0.06, 0, 1) * 255 : 255;
        data[offset] = r;
        data[offset + 1] = g;
        data[offset + 2] = b;
        data[offset + 3] = Math.round(alpha);
      }
    }
    ctx.putImageData(img, 0, 0);
  }, [hsv.h, open, mode]);

  // Manage popover dismissal and input scoping when opened in popover mode.
  useEffect(() => {
    if (mode !== "popover" || !open) return;
    const release = enterSkyInputScope();
    const onKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        setOpen(false);
      }
    };
    const onPointerDown = (event: PointerEvent): void => {
      if (!rootRef.current?.contains(event.target as Node | null)) {
        setOpen(false);
      }
    };
    window.addEventListener("keydown", onKeyDown, true);
    window.addEventListener("pointerdown", onPointerDown, true);
    return () => {
      release();
      window.removeEventListener("keydown", onKeyDown, true);
      window.removeEventListener("pointerdown", onPointerDown, true);
    };
  }, [mode, open]);

  const emitHsv = (next: HsvColor): void => {
    setHsv(next);
    const hex = hsvToHex(next.h, next.s, next.v);
    lastEmittedRef.current = hex;
    setHexDraft(hex.toUpperCase());
    onChange(hex);
  };

  const updateHueFromPoint = (clientX: number, clientY: number): void => {
    const el = wheelRef.current;
    if (!el) return;
    const rect = el.getBoundingClientRect();
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    const angle = (Math.atan2(clientY - cy, clientX - cx) * 180) / Math.PI;
    const nextHue = ((angle % 360) + 360) % 360;
    emitHsv({ h: nextHue, s: hsv.s, v: hsv.v });
  };

  const updateInnerFromPoint = (clientX: number, clientY: number): void => {
    const el = innerRef.current;
    if (!el) return;
    const rect = el.getBoundingClientRect();
    const half = rect.width / 2;
    if (half <= 0) return;
    const rawX = (clientX - (rect.left + half)) / half;
    const rawY = (clientY - (rect.top + half)) / half;
    const len = Math.hypot(rawX, rawY);
    const nx = len > 1 ? rawX / len : rawX;
    const ny = len > 1 ? rawY / len : rawY;
    const v = clamp((1 - ny) * 0.5, 0, 1);
    const maxU = Math.sqrt(Math.max(1e-5, 1 - ny * ny));
    const s = clamp((nx / maxU + 1) * 0.5, 0, 1);
    emitHsv({ h: hsv.h, s, v });
  };

  const hueRad = (hsv.h * Math.PI) / 180;
  const hueHandleX = 50 + Math.cos(hueRad) * 42.5;
  const hueHandleY = 50 + Math.sin(hueRad) * 42.5;

  const innerVy = 1 - 2 * hsv.v;
  const innerMaxU = Math.sqrt(Math.max(0, 1 - innerVy * innerVy));
  const innerUx = (2 * hsv.s - 1) * innerMaxU;
  const innerHandleX = clamp(50 + innerUx * 50, 4, 96);
  const innerHandleY = clamp(50 + innerVy * 50, 4, 96);

  const pureHueHex = hsvToHex(hsv.h, 1, 1);

  const wheelBody = (
    <div
      className={"sky-color-wheel__panel" + (compact ? " is-compact" : "")}
      role="group"
      aria-labelledby={labelId}
    >
      <div className="sky-color-wheel__head">
        <span id={labelId} className="sky-color-wheel__label">
          {label}
        </span>
        {allowThemeMatch && onResetTheme && (
          <button
            type="button"
            className={"sky-color-wheel__theme-btn" + (isThemeMatched ? " is-active" : "")}
            disabled={disabled}
            onClick={onResetTheme}
            title="Synchronize color to active theme accent"
          >
            Match Theme
          </button>
        )}
      </div>

      <div
        ref={wheelRef}
        className="sky-color-wheel__dial"
        style={{ width: size, height: size, "--wheel-hue": pureHueHex } as React.CSSProperties}
        onPointerDown={(e) => {
          if (disabled || e.button !== 0) return;
          if (innerRef.current?.contains(e.target as Node | null)) return;
          e.preventDefault();
          dragTargetRef.current = "hue";
          e.currentTarget.setPointerCapture(e.pointerId);
          updateHueFromPoint(e.clientX, e.clientY);
        }}
        onPointerMove={(e) => {
          if (dragTargetRef.current === "hue") {
            updateHueFromPoint(e.clientX, e.clientY);
          }
        }}
        onPointerUp={(e) => {
          if (dragTargetRef.current === "hue") {
            dragTargetRef.current = null;
            if (e.currentTarget.hasPointerCapture(e.pointerId)) {
              e.currentTarget.releasePointerCapture(e.pointerId);
            }
          }
        }}
        onPointerCancel={() => {
          dragTargetRef.current = null;
        }}
      >
        <div className="sky-color-wheel__hue-ring" aria-hidden="true" />
        <span
          className="sky-color-wheel__hue-thumb"
          style={{ left: `${hueHandleX}%`, top: `${hueHandleY}%`, background: pureHueHex }}
          aria-hidden="true"
        />

        <div
          ref={innerRef}
          className="sky-color-wheel__inner"
          onPointerDown={(e) => {
            if (disabled || e.button !== 0) return;
            e.preventDefault();
            e.stopPropagation();
            dragTargetRef.current = "inner";
            e.currentTarget.setPointerCapture(e.pointerId);
            updateInnerFromPoint(e.clientX, e.clientY);
          }}
          onPointerMove={(e) => {
            if (dragTargetRef.current === "inner") {
              e.stopPropagation();
              updateInnerFromPoint(e.clientX, e.clientY);
            }
          }}
          onPointerUp={(e) => {
            if (dragTargetRef.current === "inner") {
              e.stopPropagation();
              dragTargetRef.current = null;
              if (e.currentTarget.hasPointerCapture(e.pointerId)) {
                e.currentTarget.releasePointerCapture(e.pointerId);
              }
            }
          }}
          onPointerCancel={() => {
            dragTargetRef.current = null;
          }}
        >
          <canvas
            ref={canvasRef}
            width={112}
            height={112}
            className="sky-color-wheel__inner-canvas"
            style={{
              background: `linear-gradient(to bottom, transparent 0%, #000 100%), linear-gradient(to right, #fff 0%, ${pureHueHex} 100%)`,
            }}
          />
          <span
            className="sky-color-wheel__inner-thumb"
            style={{ left: `${innerHandleX}%`, top: `${innerHandleY}%`, background: effectiveHex }}
            aria-hidden="true"
          />
        </div>
      </div>

      <div className="sky-color-wheel__footer">
        <span
          className="sky-color-wheel__swatch"
          style={{ background: effectiveHex }}
          aria-hidden="true"
        />
        <input
          type="text"
          className="sky-color-wheel__hex"
          aria-label={`${label} hex value`}
          maxLength={7}
          disabled={disabled}
          value={isThemeMatched ? "THEME" : hexDraft}
          onChange={(e) => {
            const raw = e.currentTarget.value.toUpperCase();
            setHexDraft(raw);
            if (/^#?[0-9A-F]{6}$/.test(raw)) {
              const normalized = raw.startsWith("#") ? raw.toLowerCase() : `#${raw.toLowerCase()}`;
              lastEmittedRef.current = normalized;
              setHsv(hexToHsv(normalized));
              onChange(normalized);
            }
          }}
          onBlur={() => setHexDraft(effectiveHex.toUpperCase())}
        />
        <span className="sky-color-wheel__readout">
          {Math.round(hsv.h)}° · {Math.round(hsv.v * 100)}%
        </span>
      </div>
    </div>
  );

  if (mode === "inline") {
    return (
      <div ref={rootRef} className={"sky-color-wheel sky-color-wheel--inline " + className}>
        {wheelBody}
      </div>
    );
  }

  return (
    <div ref={rootRef} className={"sky-color-wheel sky-color-wheel--popover " + className}>
      <button
        type="button"
        disabled={disabled}
        aria-label={label}
        aria-expanded={open}
        aria-haspopup="dialog"
        className={"sky-color-wheel__trigger" + (open ? " is-open" : "")}
        onClick={() => setOpen((v) => !v)}
        title={`${label}: ${isThemeMatched ? "Match Theme" : effectiveHex.toUpperCase()} (Click to open color wheel)`}
      >
        <span className="sky-color-wheel__trigger-chip" aria-hidden="true">
          <i style={{ background: effectiveHex }} />
        </span>
        {!compact && <code className="sky-color-wheel__trigger-hex">{isThemeMatched ? "THEME" : effectiveHex.toUpperCase()}</code>}
      </button>
      {open && (
        <div className="sky-color-wheel__popover-card" role="dialog" aria-label={label}>
          {wheelBody}
        </div>
      )}
    </div>
  );
}
