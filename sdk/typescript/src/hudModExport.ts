import { getSkyWebView, type JsonValue, type SkyUltraRequestOptions, type SkyWebViewAPI } from './index.js';
import { parseSkyUltraHudLayout, type SkyUltraHudLayout, type SkyUltraWidgetAnchor, type SkyUltraWidgetKind } from './layout.js';

export interface SkyUltraHudModFile {
  path: string;
  language: 'json' | 'html' | 'css' | 'javascript' | 'typescript' | 'svg' | 'markdown';
  description: string;
  content: string;
}

export interface SkyUltraCustomHudCode {
  enabled: boolean;
  mode: 'split' | 'full-html';
  html: string;
  css: string;
  js: string;
  fullHtml: string;
}

export interface SkyUltraHudCodeTemplate {
  id: string;
  name: string;
  tag: string;
  description: string;
  html: string;
  css: string;
  js: string;
}

export interface SkyUltraHudModExportSpec {
  id: string;
  name: string;
  version?: string;
  author?: string;
  hotkey?: string;
  layout: SkyUltraHudLayout;
  customCode?: SkyUltraCustomHudCode;
}

export interface SkyUltraHudModPackage {
  id: string;
  name: string;
  version: string;
  files: SkyUltraHudModFile[];
}

export interface SkyUltraHudModExportResult {
  exported: boolean;
  modId: string;
  directory?: string;
  fileCount?: number;
  message?: string;
}

const MOD_ID_PATTERN = /^[a-z0-9][a-z0-9._-]{0,63}$/;
const RESERVED_MOD_IDS = new Set([
  'skyultra.menu',
  'actors',
  'items',
  'exports',
  'screenshots',
  'cache',
  'logs',
]);
const VERSION_PATTERN = /^[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?$/;

export const SKYULTRA_HUD_CODE_TEMPLATES: readonly SkyUltraHudCodeTemplate[] = [
  {
    id: 'sovereign-glass',
    name: 'Sovereign Glass HUD',
    tag: 'NORDIC GOLD',
    description: 'Floating compass ribbon at top-center with animated crimson/azure/emerald vitals at bottom-left.',
    html: `<div class="sov-hud">
  <header class="sov-compass">
    <span class="sov-hold" id="hud-location">Whiterun</span>
    <strong class="sov-deg" id="hud-bearing">N · 0°</strong>
    <span class="sov-lvl" id="hud-identity">Dragonborn · LVL 1</span>
  </header>

  <section class="sov-vitals">
    <div class="sov-bar sov-bar--hp">
      <div class="sov-bar__meta"><span>HEALTH</span><b id="hp-text">100 / 100</b></div>
      <div class="sov-bar__track"><i id="hp-fill" style="width:100%"></i></div>
    </div>
    <div class="sov-bar sov-bar--mp">
      <div class="sov-bar__meta"><span>MAGICKA</span><b id="mp-text">100 / 100</b></div>
      <div class="sov-bar__track"><i id="mp-fill" style="width:100%"></i></div>
    </div>
    <div class="sov-bar sov-bar--sp">
      <div class="sov-bar__meta"><span>STAMINA</span><b id="sp-text">100 / 100</b></div>
      <div class="sov-bar__track"><i id="sp-fill" style="width:100%"></i></div>
    </div>
  </section>
</div>`,
    css: `.sov-hud {
  position: fixed;
  inset: 0;
  padding: 26px 34px;
  pointer-events: none;
  font-family: 'Bahnschrift', 'Segoe UI', sans-serif;
  color: #f6efe2;
}
.sov-compass {
  position: absolute;
  top: 22px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  align-items: center;
  gap: 18px;
  padding: 8px 22px;
  border: 1px solid rgba(232, 176, 89, 0.45);
  border-radius: 999px;
  background: linear-gradient(180deg, rgba(16, 14, 12, 0.82), rgba(10, 9, 8, 0.62));
  box-shadow: 0 10px 28px rgba(0, 0, 0, 0.55);
}
.sov-hold { font-size: 11px; letter-spacing: 0.16em; text-transform: uppercase; color: #e8b059; }
.sov-deg { font-size: 15px; letter-spacing: 0.08em; }
.sov-lvl { font-size: 11px; opacity: 0.78; letter-spacing: 0.08em; }
.sov-vitals {
  position: absolute;
  left: 34px;
  bottom: 30px;
  width: 270px;
  display: grid;
  gap: 10px;
}
.sov-bar__meta {
  display: flex;
  justify-content: space-between;
  font-size: 10px;
  letter-spacing: 0.14em;
  margin-bottom: 4px;
  text-shadow: 0 2px 6px rgba(0, 0, 0, 0.9);
}
.sov-bar__track {
  height: 7px;
  border-radius: 999px;
  background: rgba(8, 8, 10, 0.72);
  border: 1px solid rgba(255, 255, 255, 0.14);
  overflow: hidden;
}
.sov-bar__track > i {
  display: block;
  height: 100%;
  border-radius: inherit;
  transition: width 160ms ease-out;
}
.sov-bar--hp i { background: linear-gradient(90deg, #b91c1c, #fb7185); box-shadow: 0 0 12px #fb7185; }
.sov-bar--mp i { background: linear-gradient(90deg, #0284c7, #38bdf8); box-shadow: 0 0 12px #38bdf8; }
.sov-bar--sp i { background: linear-gradient(90deg, #15803d, #4ade80); box-shadow: 0 0 12px #4ade80; }`,
    js: `const dirs = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
const pct = (cur, max) => Math.max(0, Math.min(100, Math.round((cur / Math.max(1, max)) * 100)));

SkyUltraHud.onUpdate((player) => {
  if (!player) return;
  const deg = ((Math.round(player.angleZ ?? 0) % 360) + 360) % 360;
  const cardinal = dirs[Math.round(deg / 45) % 8];

  document.getElementById('hud-location').textContent = player.location || 'Skyrim';
  document.getElementById('hud-bearing').textContent = cardinal + ' · ' + deg + '°';
  document.getElementById('hud-identity').textContent = (player.playerName || 'Dragonborn') + ' · LVL ' + (player.level ?? 1);

  const setBar = (prefix, pool) => {
    const cur = Math.round(pool?.current ?? 100);
    const max = Math.round(pool?.max ?? 100);
    document.getElementById(prefix + '-text').textContent = cur.toLocaleString() + ' / ' + max.toLocaleString();
    document.getElementById(prefix + '-fill').style.width = pct(cur, max) + '%';
  };
  setBar('hp', player.health);
  setBar('mp', player.magicka);
  setBar('sp', player.stamina);
});`,
  },
  {
    id: 'cyber-target',
    name: 'Cyber Tactical Telemetry',
    tag: 'NEON HUD',
    description: 'Sci-fi angled HUD with live actor radar scanner, armor/gold counters, and neon vitality readouts.',
    html: `<div class="cyber-hud">
  <div class="cyber-top-right">
    <div class="cyber-tag">NEARBY SIGNATURES</div>
    <div id="cyber-actors" class="cyber-actor-list">Scanning range…</div>
  </div>
  <div class="cyber-bottom-center">
    <div class="cyber-stat"><small>HP</small><strong id="c-hp">100%</strong><div class="cyber-line"><i id="c-hp-b"></i></div></div>
    <div class="cyber-stat cyber-stat--mp"><small>MP</small><strong id="c-mp">100%</strong><div class="cyber-line"><i id="c-mp-b"></i></div></div>
    <div class="cyber-stat cyber-stat--sp"><small>SP</small><strong id="c-sp">100%</strong><div class="cyber-line"><i id="c-sp-b"></i></div></div>
  </div>
  <div class="cyber-bottom-left">
    <span id="c-meta">ARMOR 0 · 0 GOLD</span>
  </div>
</div>`,
    css: `.cyber-hud {
  position: fixed;
  inset: 0;
  pointer-events: none;
  font-family: Consolas, 'Cascadia Mono', monospace;
  color: #e2f8ff;
}
.cyber-top-right {
  position: absolute;
  top: 26px;
  right: 32px;
  min-width: 220px;
  padding: 10px 14px;
  background: rgba(5, 12, 24, 0.76);
  border-right: 3px solid #00f0ff;
  box-shadow: 0 0 22px rgba(0, 240, 255, 0.2);
}
.cyber-tag { font-size: 10px; letter-spacing: 0.18em; color: #00f0ff; margin-bottom: 6px; }
.cyber-actor-list { display: grid; gap: 4px; font-size: 11px; }
.cyber-bottom-center {
  position: absolute;
  bottom: 28px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  gap: 18px;
}
.cyber-stat { width: 132px; padding: 8px 10px; background: rgba(6, 10, 20, 0.78); border-top: 2px solid #ff2e93; }
.cyber-stat--mp { border-top-color: #00f0ff; }
.cyber-stat--sp { border-top-color: #39ff14; }
.cyber-stat small { font-size: 10px; opacity: 0.75; margin-right: 8px; }
.cyber-stat strong { font-size: 16px; }
.cyber-line { height: 4px; background: rgba(255,255,255,0.12); margin-top: 6px; }
.cyber-line > i { display: block; height: 100%; background: #ff2e93; transition: width 140ms ease; }
.cyber-stat--mp .cyber-line > i { background: #00f0ff; }
.cyber-stat--sp .cyber-line > i { background: #39ff14; }
.cyber-bottom-left {
  position: absolute;
  left: 32px;
  bottom: 30px;
  font-size: 11px;
  letter-spacing: 0.14em;
  color: #00f0ff;
  text-shadow: 0 2px 8px #000;
}`,
    js: `const ratio = (v) => Math.max(0, Math.min(100, Math.round(((v?.current ?? 100) / Math.max(1, v?.max ?? 100)) * 100)));

SkyUltraHud.onUpdate((player, actors) => {
  if (player) {
    const hp = ratio(player.health), mp = ratio(player.magicka), sp = ratio(player.stamina);
    document.getElementById('c-hp').textContent = hp + '%';
    document.getElementById('c-mp').textContent = mp + '%';
    document.getElementById('c-sp').textContent = sp + '%';
    document.getElementById('c-hp-b').style.width = hp + '%';
    document.getElementById('c-mp-b').style.width = mp + '%';
    document.getElementById('c-sp-b').style.width = sp + '%';
    document.getElementById('c-meta').textContent =
      'ARMOR ' + Math.round(player.armorRating ?? 0).toLocaleString() +
      ' · ' + Math.round(player.gold ?? 0).toLocaleString() + ' SEPTIMS';
  }
  const box = document.getElementById('cyber-actors');
  if (actors && actors.length) {
    box.innerHTML = actors.slice(0, 4).map((a) =>
      '<div>• ' + (a.displayName || a.name || 'Actor') + ' <span style="opacity:.65">LVL ' + (a.level ?? 1) + '</span></div>'
    ).join('');
  } else {
    box.textContent = 'No nearby actors in scan cone';
  }
});`,
  },
  {
    id: 'arcane-rings',
    name: 'Arcane Radial Rings',
    tag: 'SVG RINGS',
    description: 'Pure SVG circular gauges for Health, Magicka, and Stamina with active spell/effect resonance counter.',
    html: `<div class="arc-stage">
  <div class="arc-cluster">
    <div class="arc-ring">
      <svg viewBox="0 0 80 80"><circle cx="40" cy="40" r="32" class="bg"/><circle id="r-hp" cx="40" cy="40" r="32" class="fg hp"/></svg>
      <div class="arc-val"><b id="v-hp">100</b><small>HP</small></div>
    </div>
    <div class="arc-ring">
      <svg viewBox="0 0 80 80"><circle cx="40" cy="40" r="32" class="bg"/><circle id="r-mp" cx="40" cy="40" r="32" class="fg mp"/></svg>
      <div class="arc-val"><b id="v-mp">100</b><small>MP</small></div>
    </div>
    <div class="arc-ring">
      <svg viewBox="0 0 80 80"><circle cx="40" cy="40" r="32" class="bg"/><circle id="r-sp" cx="40" cy="40" r="32" class="fg sp"/></svg>
      <div class="arc-val"><b id="v-sp">100</b><small>SP</small></div>
    </div>
  </div>
  <div class="arc-caption" id="arc-fx">0 ACTIVE EFFECTS</div>
</div>`,
    css: `.arc-stage {
  position: fixed;
  bottom: 28px;
  left: 50%;
  transform: translateX(-50%);
  display: grid;
  justify-items: center;
  gap: 8px;
  pointer-events: none;
  font-family: Georgia, serif;
  color: #f8f1e5;
}
.arc-cluster { display: flex; gap: 18px; }
.arc-ring { position: relative; width: 78px; height: 78px; }
.arc-ring svg { width: 100%; height: 100%; transform: rotate(-90deg); }
.arc-ring circle { fill: rgba(10, 10, 16, 0.68); stroke-width: 5; }
.arc-ring .bg { stroke: rgba(255, 255, 255, 0.14); }
.arc-ring .fg { stroke-dasharray: 201; stroke-dashoffset: 0; stroke-linecap: round; transition: stroke-dashoffset 180ms ease; fill: none; }
.arc-ring .hp { stroke: #fb7185; filter: drop-shadow(0 0 6px #fb7185); }
.arc-ring .mp { stroke: #a78bfa; filter: drop-shadow(0 0 6px #a78bfa); }
.arc-ring .sp { stroke: #34d399; filter: drop-shadow(0 0 6px #34d399); }
.arc-val { position: absolute; inset: 0; display: grid; place-content: center; text-align: center; }
.arc-val b { font-size: 14px; line-height: 1.1; }
.arc-val small { font-size: 9px; letter-spacing: 0.14em; opacity: 0.72; font-family: sans-serif; }
.arc-caption { font-family: sans-serif; font-size: 10px; letter-spacing: 0.18em; color: #c4b5fd; text-shadow: 0 2px 8px #000; }`,
    js: `const CIRC = 2 * Math.PI * 32;

SkyUltraHud.onUpdate((player) => {
  if (!player) return;
  const setRing = (id, vid, pool) => {
    const cur = Math.round(pool?.current ?? 100);
    const max = Math.max(1, pool?.max ?? 100);
    const ratio = Math.max(0, Math.min(1, cur / max));
    const el = document.getElementById(id);
    if (el) el.style.strokeDashoffset = String(CIRC * (1 - ratio));
    const tx = document.getElementById(vid);
    if (tx) tx.textContent = cur > 9999 ? Math.round(cur / 1000) + 'k' : String(cur);
  };
  setRing('r-hp', 'v-hp', player.health);
  setRing('r-mp', 'v-mp', player.magicka);
  setRing('r-sp', 'v-sp', player.stamina);
  document.getElementById('arc-fx').textContent =
    (player.activeEffectCount ?? player.activeEffects?.length ?? 0) + ' ACTIVE ARCANE INFLUENCES';
});`,
  },
  {
    id: 'minimal-immersive',
    name: 'Minimalist Soulslike Bars',
    tag: 'CLEAN & SUBTLE',
    description: 'Ultra-thin stacked combat bars at top-left with carry-weight badge at bottom-right.',
    html: `<div class="soul-hud">
  <div class="soul-bars">
    <div class="soul-row soul-row--hp"><i data-sky-bar="health"></i></div>
    <div class="soul-row soul-row--mp"><i data-sky-bar="magicka"></i></div>
    <div class="soul-row soul-row--sp"><i data-sky-bar="stamina"></i></div>
  </div>
  <div class="soul-pack">
    <span data-sky-bind="location">Whiterun</span> ·
    <b id="soul-wt">0 / 300 WT</b>
  </div>
</div>`,
    css: `.soul-hud { position: fixed; inset: 0; pointer-events: none; font-family: 'Segoe UI', sans-serif; color: #ece6dc; }
.soul-bars { position: absolute; top: 30px; left: 34px; display: grid; gap: 5px; }
.soul-row { height: 6px; background: rgba(0,0,0,0.68); border: 1px solid rgba(255,255,255,0.18); overflow: hidden; }
.soul-row--hp { width: 260px; }
.soul-row--mp { width: 190px; }
.soul-row--sp { width: 225px; }
.soul-row > i { display: block; height: 100%; width: 100%; transition: width 150ms ease; }
.soul-row--hp > i { background: #dc2626; }
.soul-row--mp > i { background: #2563eb; }
.soul-row--sp > i { background: #16a34a; }
.soul-pack {
  position: absolute;
  right: 34px;
  bottom: 28px;
  font-size: 11px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  text-shadow: 0 2px 8px #000;
}`,
    js: `SkyUltraHud.onUpdate((player) => {
  if (!player) return;
  const cur = Math.round(player.carryWeight?.current ?? 0);
  const max = Math.round(player.carryWeight?.max ?? 300);
  document.getElementById('soul-wt').textContent = cur + ' / ' + max + ' WT';
});`,
  },
];

export function sanitizeCustomHudCode(raw: unknown): SkyUltraCustomHudCode {
  const def = SKYULTRA_HUD_CODE_TEMPLATES[0];
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) {
    return {
      enabled: false,
      mode: 'split',
      html: def.html,
      css: def.css,
      js: def.js,
      fullHtml: '',
    };
  }
  const obj = raw as Record<string, unknown>;
  return {
    enabled: Boolean(obj.enabled),
    mode: obj.mode === 'full-html' ? 'full-html' : 'split',
    html: typeof obj.html === 'string' ? obj.html.slice(0, 196608) : def.html,
    css: typeof obj.css === 'string' ? obj.css.slice(0, 196608) : def.css,
    js: typeof obj.js === 'string' ? obj.js.slice(0, 196608) : def.js,
    fullHtml: typeof obj.fullHtml === 'string' ? obj.fullHtml.slice(0, 262144) : '',
  };
}

export function generateCustomHudCodeFromLayout(layoutInput: SkyUltraHudLayout): {
  html: string;
  css: string;
  js: string;
} {
  const layout = parseSkyUltraHudLayout(layoutInput);
  const zones = ['top-left', 'top-center', 'top-right', 'bottom-left', 'bottom-center', 'bottom-right'] as const;
  const zoneHtml = zones
    .map((zone) => {
      const items = layout.widgets.filter((k) => (layout.placements?.[k]?.anchor ?? layout.anchor) === zone);
      if (!items.length) return `  <div class="hud-zone hud-zone--${zone}"></div>`;
      const children = items
        .map((k) => {
          const wp = layout.placements?.[k];
          const color = wp?.color ?? layout.colors?.[k] ?? '#e8b059';
          const scale = wp?.scale ?? layout.scale ?? 1;
          const opacity = wp?.opacity ?? layout.opacity ?? 0.96;
          const ox = wp?.offsetX ?? 0;
          const oy = wp?.offsetY ?? 0;
          const label = wp?.customLabel || k.toUpperCase();
          return `    <div class="hud-card" data-widget="${k}" style="--accent:${color};opacity:${opacity};transform:translate(${ox}px,${oy}px) scale(${scale})">
      <small class="hud-card__label">${label}</small>
      <strong class="hud-card__val" id="val-${k}">—</strong>
    </div>`;
        })
        .join('\n');
      return `  <div class="hud-zone hud-zone--${zone}">\n${children}\n  </div>`;
    })
    .join('\n');

  const html = `<div class="hud-grid">\n${zoneHtml}\n</div>`;
  const css = `.hud-grid {
  position: fixed;
  inset: 0;
  padding: 28px 34px;
  display: grid;
  grid-template-columns: 1fr auto 1fr;
  grid-template-rows: 1fr 1fr;
  pointer-events: none;
  font-family: 'Bahnschrift', 'Segoe UI', sans-serif;
  color: #f5efe6;
}
.hud-zone { display: flex; flex-direction: column; gap: 12px; }
.hud-zone--top-left { grid-column: 1; grid-row: 1; align-items: flex-start; justify-content: flex-start; }
.hud-zone--top-center { grid-column: 2; grid-row: 1; align-items: center; justify-content: flex-start; }
.hud-zone--top-right { grid-column: 3; grid-row: 1; align-items: flex-end; justify-content: flex-start; }
.hud-zone--bottom-left { grid-column: 1; grid-row: 2; align-items: flex-start; justify-content: flex-end; }
.hud-zone--bottom-center { grid-column: 2; grid-row: 2; align-items: center; justify-content: flex-end; }
.hud-zone--bottom-right { grid-column: 3; grid-row: 2; align-items: flex-end; justify-content: flex-end; }
.hud-card {
  display: grid;
  gap: 3px;
  text-shadow: 0 2px 10px rgba(0,0,0,0.92), 0 0 14px var(--accent, #e8b059);
}
.hud-card__label {
  font-size: 9px;
  letter-spacing: 0.16em;
  text-transform: uppercase;
  color: var(--accent, #e8b059);
}
.hud-card__val { font-size: 20px; font-variant-numeric: tabular-nums; }`;

  const js = `SkyUltraHud.onUpdate((player, actors) => {
  if (!player) return;
  const set = (id, text) => {
    const el = document.getElementById('val-' + id);
    if (el) el.textContent = text;
  };
  set('vitals', 'HP ' + Math.round(player.health?.current ?? 0) + ' · MP ' + Math.round(player.magicka?.current ?? 0) + ' · SP ' + Math.round(player.stamina?.current ?? 0));
  set('compass', (player.location || 'Skyrim') + ' · ' + Math.round(player.angleZ ?? 0) + '°');
  set('character', (player.playerName || 'Dragonborn') + ' · LVL ' + (player.level ?? 1));
  set('radar', (actors?.length ?? 0) + ' NEARBY ACTORS');
  set('target', actors?.[0] ? (actors[0].displayName || actors[0].name) : 'NO TARGET');
  set('loot', Math.round(player.carryWeight?.current ?? 0) + '/' + Math.round(player.carryWeight?.max ?? 300) + ' WT · ' + (player.gold ?? 0).toLocaleString() + ' G');
  set('armory', 'ARMOR ' + Math.round(player.armorRating ?? 0).toLocaleString());
  set('stealth', 'ARMOR ' + Math.round(player.armorRating ?? 0).toLocaleString());
  set('wards', (player.activeEffectCount ?? 0) + ' ACTIVE EFFECTS');
  set('effects', (player.activeEffectCount ?? 0) + ' ACTIVE EFFECTS');
  set('clock', player.calendarDate || 'Tamriel');
  set('telemetry', 'LIVE HUD');
  set('skills', 'LVL ' + (player.level ?? 1));
});`;
  return { html, css, js };
}

function buildSkyUltraHudBootstrapScript(standalone: boolean): string {
  return `<script>
(function () {
  var listeners = [];
  var bridgeEvents = {};
  var state = {
    player: {
      available: true,
      playerName: 'Dragonborn',
      level: 42,
      location: 'Whiterun',
      angleZ: 35,
      gold: 12500,
      armorRating: 480,
      activeEffectCount: 4,
      followerCount: 1,
      health: { current: 295, max: 320 },
      magicka: { current: 185, max: 220 },
      stamina: { current: 215, max: 240 },
      carryWeight: { current: 138, max: 350 }
    },
    nearbyActors: []
  };

  function postLog(level, parts) {
    if (${standalone ? 'true' : 'false'}) return;
    try {
      var msg = Array.prototype.map.call(parts, function (p) {
        if (typeof p === 'string') return p;
        try { return JSON.stringify(p); } catch (e) { return String(p); }
      }).join(' ');
      window.parent.postMessage({ __skyultraHudLog: true, level: level, message: msg.slice(0, 600) }, '*');
    } catch (e) {}
  }

  if (!${standalone ? 'true' : 'false'}) {
    ['log', 'warn', 'error'].forEach(function (lvl) {
      var orig = console[lvl];
      console[lvl] = function () {
        postLog(lvl, arguments);
        if (orig) return orig.apply(console, arguments);
      };
    });
    window.addEventListener('error', function (ev) {
      postLog('error', [ev.message || 'Script error', ev.lineno ? '(line ' + ev.lineno + ')' : '']);
    });
    window.addEventListener('unhandledrejection', function (ev) {
      postLog('error', ['Unhandled promise rejection:', ev.reason && ev.reason.message ? ev.reason.message : String(ev.reason)]);
    });
  }

  function applyDeclarativeBindings() {
    var p = state.player;
    if (!p) return;
    var binds = document.querySelectorAll('[data-sky-bind]');
    for (var i = 0; i < binds.length; i++) {
      var path = binds[i].getAttribute('data-sky-bind') || '';
      var parts = path.split('.');
      var cur = p;
      for (var j = 0; j < parts.length; j++) {
        if (cur == null || typeof cur !== 'object') { cur = null; break; }
        cur = cur[parts[j]];
      }
      if (typeof cur === 'number') binds[i].textContent = Math.round(cur).toLocaleString();
      else if (typeof cur === 'string') binds[i].textContent = cur;
    }
    var bars = document.querySelectorAll('[data-sky-bar]');
    for (var k = 0; k < bars.length; k++) {
      var poolKey = bars[k].getAttribute('data-sky-bar') || '';
      var pool = p[poolKey];
      if (pool && typeof pool.current === 'number' && typeof pool.max === 'number') {
        var pct = Math.max(0, Math.min(100, Math.round((pool.current / Math.max(1, pool.max)) * 100)));
        bars[k].style.width = pct + '%';
      }
    }
  }

  function notifyAll() {
    applyDeclarativeBindings();
    for (var i = 0; i < listeners.length; i++) {
      try { listeners[i](state.player, state.nearbyActors); } catch (err) { postLog('error', [err && err.message ? err.message : String(err)]); }
    }
    if (typeof window.onSkyUltraHudUpdate === 'function') {
      try { window.onSkyUltraHudUpdate(state.player, state.nearbyActors); } catch (err) { postLog('error', [err && err.message ? err.message : String(err)]); }
    }
    try {
      window.dispatchEvent(new CustomEvent('skyultra:hud-update', { detail: { player: state.player, nearbyActors: state.nearbyActors } }));
    } catch (e) {}
    var bus = bridgeEvents['skyultra.game.update'] || [];
    for (var b = 0; b < bus.length; b++) {
      try { bus[b]({ snapshot: state.player }); } catch (e) {}
    }
  }

  window.SkyUltraHud = {
    get player() { return state.player; },
    get nearbyActors() { return state.nearbyActors; },
    onUpdate: function (fn) {
      if (typeof fn !== 'function') return function () {};
      listeners.push(fn);
      try { fn(state.player, state.nearbyActors); } catch (err) { postLog('error', [err && err.message ? err.message : String(err)]); }
      return function () {
        var idx = listeners.indexOf(fn);
        if (idx >= 0) listeners.splice(idx, 1);
      };
    }
  };

  if (!window.SkyWebView) {
    window.SkyWebView = {
      bridgeVersion: 1,
      on: function (name, cb) {
        if (!bridgeEvents[name]) bridgeEvents[name] = [];
        bridgeEvents[name].push(cb);
        return function () {
          var arr = bridgeEvents[name] || [];
          var idx = arr.indexOf(cb);
          if (idx >= 0) arr.splice(idx, 1);
        };
      },
      off: function (name, cb) {
        var arr = bridgeEvents[name] || [];
        var idx = arr.indexOf(cb);
        if (idx >= 0) { arr.splice(idx, 1); return true; }
        return false;
      },
      request: function (name) {
        if (name === 'game.getPlayerSnapshot') return Promise.resolve(state.player);
        if (name === 'game.subscribe') return Promise.resolve({ subscribed: true, sequence: 1, update: { snapshot: state.player } });
        if (name === 'game.getNearbyNpcs' || name === 'game.getNearbyActors') return Promise.resolve({ results: state.nearbyActors });
        if (name === 'ui.pageReady') return Promise.resolve({ ready: true });
        return Promise.resolve({});
      }
    };
  }

  window.addEventListener('message', function (ev) {
    var d = ev && ev.data;
    if (!d || typeof d !== 'object' || !d.__skyultraHudPush) return;
    if (d.player && typeof d.player === 'object') state.player = Object.assign({}, state.player, d.player);
    if (Array.isArray(d.nearbyActors)) state.nearbyActors = d.nearbyActors;
    notifyAll();
  });

  window.addEventListener('DOMContentLoaded', function () {
    notifyAll();
    if (!${standalone ? 'true' : 'false'}) {
      try { window.parent.postMessage({ __skyultraHudReady: true }, '*'); } catch (e) {}
    } else if (window.SkyWebView && typeof window.SkyWebView.request === 'function') {
      window.SkyWebView.request('game.subscribe', { topics: ['vitals', 'equipment', 'inventory', 'effects', 'followers'], intervalMs: 250, scope: 'hud' })
        .then(function (sub) {
          if (sub && sub.update && sub.update.snapshot) {
            state.player = Object.assign({}, state.player, sub.update.snapshot);
            notifyAll();
          }
        }).catch(function () {});
      window.SkyWebView.on('skyultra.game.update', function (ev) {
        if (ev && ev.snapshot) state.player = Object.assign({}, state.player, ev.snapshot);
        else if (ev && ev.delta) state.player = Object.assign({}, state.player, ev.delta);
        notifyAll();
      });
      var pollActors = function () {
        window.SkyWebView.request('game.getNearbyNpcs', { radius: 3000 }).then(function (res) {
          if (res && Array.isArray(res.results)) { state.nearbyActors = res.results.slice(0, 8); notifyAll(); }
        }).catch(function () {});
      };
      pollActors();
      setInterval(pollActors, 2500);
      requestAnimationFrame(function () {
        window.SkyWebView.request('ui.pageReady', {}).catch(function () {});
      });
    }
  });
})();
</script>`;
}

export function buildCustomHudDocument(
  rawCode: SkyUltraCustomHudCode,
  options?: { standalone?: boolean },
): string {
  const code = sanitizeCustomHudCode(rawCode);
  const bootstrap = buildSkyUltraHudBootstrapScript(Boolean(options?.standalone));
  const baseReset = `<style data-skyultra-reset>
html, body { margin: 0; width: 100%; height: 100%; background: transparent !important; overflow: hidden; user-select: none; }
* { box-sizing: border-box; }
</style>`;

  if (code.mode === 'full-html' && code.fullHtml.trim()) {
    const src = code.fullHtml;
    if (/<head[\s>]/i.test(src)) {
      return src.replace(/<head([^>]*)>/i, `<head$1>\n${baseReset}\n${bootstrap}`);
    }
    if (/<html[\s>]/i.test(src)) {
      return src.replace(/<html([^>]*)>/i, `<html$1><head>${baseReset}${bootstrap}</head>`);
    }
    return `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="UTF-8" />\n${baseReset}\n${bootstrap}\n</head>\n<body>\n${src}\n</body>\n</html>`;
  }

  const safeJs = code.js.replace(/<\/script>/gi, '<\\/script>');
  return `<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    ${baseReset}
    <style>${code.css}</style>
    ${bootstrap}
  </head>
  <body>
    ${code.html}
    <script>
${safeJs}
    </script>
  </body>
</html>
`;
}

export function sanitizeSkyUltraModId(raw: string): string {
  const cleaned = String(raw || '')
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9._-]+/g, '_')
    .replace(/\.{2,}/g, '.')
    .replace(/^[^a-z0-9]+|[._-]+$/g, '');
  if (!cleaned) {
    return 'custom_hud';
  }
  if (RESERVED_MOD_IDS.has(cleaned)) {
    return `${cleaned.replace(/\./g, '_')}_hud`.slice(0, 64);
  }
  return cleaned.slice(0, 64);
}

export function generateSkyUltraHudModFiles(spec: SkyUltraHudModExportSpec): SkyUltraHudModPackage {
  if (!spec || typeof spec !== 'object') {
    throw new Error('Expected a HUD mod export specification.');
  }
  const id = String(spec.id || '').trim();
  if (!MOD_ID_PATTERN.test(id) || id.includes('..') || RESERVED_MOD_IDS.has(id)) {
    throw new Error("Mod id must be a lowercase identifier such as 'test' or 'author.custom_hud'.");
  }
  const name = String(spec.name || '').trim();
  if (!name || name.length > 80) {
    throw new Error('Mod name must be between 1 and 80 characters.');
  }
  const version = String(spec.version ?? '1.0.0').trim();
  if (!VERSION_PATTERN.test(version)) {
    throw new Error("Mod version must be valid semantic version syntax such as '1.0.0'.");
  }
  const author = String(spec.author ?? id.split('.')[0] ?? 'Author').trim().slice(0, 48) || 'Author';
  const hotkey = String(spec.hotkey ?? 'F8').trim().slice(0, 24) || 'F8';
  const layout = parseSkyUltraHudLayout(spec.layout);
  const customCode = spec.customCode ? sanitizeCustomHudCode(spec.customCode) : null;
  const hasCustomHud = Boolean(customCode?.enabled);

  const pageRequests: string[] = [
    'game.subscribe',
    'game.unsubscribe',
    'game.getPlayerSnapshot',
    'ui.pageReady',
    'ui.configureWidgets',
    'ui.close',
  ];
  if (hasCustomHud || layout.widgets.includes('radar') || layout.widgets.includes('target')) {
    pageRequests.push('game.getNearbyNpcs');
  }
  if (layout.widgets.includes('character')) {
    pageRequests.push('game.getActorThumbnail');
  }

  const manifestObj = {
    schemaVersion: 1,
    id,
    name,
    version,
    entry: 'index.html',
    permissions: {
      storage: true,
      assetAccess: true,
      networkHosts: [] as string[],
      pageRequests,
    },
  };

  const layoutJson = JSON.stringify(layout, null, 2);

  const emblemSvg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <defs>
    <linearGradient id="crest" x1="8" y1="6" x2="56" y2="58" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#f2c97d"/>
      <stop offset="100%" stop-color="#8b5cf6"/>
    </linearGradient>
  </defs>
  <polygon points="32,4 58,18 58,46 32,60 6,46 6,18" stroke="url(#crest)" stroke-width="2.5" fill="rgba(12,14,22,0.75)"/>
  <polygon points="32,12 49,22 49,42 32,52 15,42 15,22" stroke="url(#crest)" stroke-width="1.4" opacity="0.7"/>
  <path d="M32 18 L40 32 L32 46 L24 32 Z" fill="url(#crest)" opacity="0.9"/>
</svg>
`;

  const runtimeCss = `:root {
  color-scheme: dark;
  --mod-accent: #e8b059;
  --mod-text: #f5efe6;
  --mod-muted: rgba(245, 239, 230, 0.72);
}
* { box-sizing: border-box; }
html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  background: transparent;
  color: var(--mod-text);
  font-family: 'Bahnschrift', 'Segoe UI', system-ui, sans-serif;
  overflow: hidden;
  user-select: none;
}
.hud-screen-grid {
  position: fixed;
  inset: 0;
  padding: 28px 34px;
  display: grid;
  grid-template-columns: 1fr auto 1fr;
  grid-template-rows: 1fr 1fr;
  pointer-events: none;
}
.hud-zone { display: flex; flex-direction: column; gap: 12px; }
.hud-zone--top-left { grid-column: 1; grid-row: 1; align-items: flex-start; justify-content: flex-start; }
.hud-zone--top-center { grid-column: 2; grid-row: 1; align-items: center; justify-content: flex-start; }
.hud-zone--top-right { grid-column: 3; grid-row: 1; align-items: flex-end; justify-content: flex-start; }
.hud-zone--bottom-left { grid-column: 1; grid-row: 2; align-items: flex-start; justify-content: flex-end; }
.hud-zone--bottom-center { grid-column: 2; grid-row: 2; align-items: center; justify-content: flex-end; }
.hud-zone--bottom-right { grid-column: 3; grid-row: 2; align-items: flex-end; justify-content: flex-end; }
.hud-widget {
  display: grid;
  gap: 4px;
  text-shadow: 0 2px 10px rgba(0, 0, 0, 0.92), 0 0 var(--glow-px, 14px) var(--widget-color, #e8b059);
}
.hud-widget__label {
  font-size: 9px;
  font-weight: 700;
  letter-spacing: 0.16em;
  text-transform: uppercase;
  color: var(--widget-color, #e8b059);
}
.hud-widget__value {
  font-size: 22px;
  font-weight: 700;
  letter-spacing: 0.02em;
  font-variant-numeric: tabular-nums;
}
.hud-vitals-row { display: flex; align-items: center; gap: 18px; }
.hud-vital-pill { display: grid; gap: 3px; min-width: 96px; }
.hud-vital-bar { height: 5px; border-radius: 999px; background: rgba(0, 0, 0, 0.55); overflow: hidden; }
.hud-vital-bar > i { display: block; height: 100%; background: var(--vital-tone, #ff5c6c); box-shadow: 0 0 10px var(--vital-tone, #ff5c6c); }
.mod-control-shell {
  display: grid;
  place-items: center;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle at 50% 35%, rgba(232, 176, 89, 0.14), rgba(8, 10, 16, 0.88));
}
.mod-control-card {
  width: min(560px, 92vw);
  padding: 26px 28px;
  border: 1px solid rgba(232, 176, 89, 0.42);
  border-radius: 12px;
  background: rgba(12, 15, 23, 0.94);
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.65);
}
.mod-control-head { display: flex; align-items: center; gap: 14px; margin-bottom: 16px; }
.mod-control-head img { width: 44px; height: 44px; }
.mod-control-actions { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 18px; }
.mod-btn {
  padding: 9px 16px;
  border: 1px solid rgba(232, 176, 89, 0.55);
  border-radius: 6px;
  background: rgba(232, 176, 89, 0.16);
  color: var(--mod-text);
  font: 700 11px 'Bahnschrift', sans-serif;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  cursor: pointer;
}
`;

  const runtimeJs = `const EXPORTED_LAYOUT = ${layoutJson};
const ZONES = ['top-left', 'top-center', 'top-right', 'bottom-left', 'bottom-center', 'bottom-right'];

function resolveBinding(player, path) {
  if (!player || !path) return null;
  const parts = String(path).split('.');
  let cur = player;
  for (const part of parts) {
    if (cur == null || typeof cur !== 'object') return null;
    cur = cur[part];
  }
  if (typeof cur === 'number') return Math.round(cur).toLocaleString();
  if (typeof cur === 'string') return cur;
  return null;
}

function passesVisibilityRule(player, rule) {
  if (!rule || rule === 'always' || !player) return true;
  if (rule === 'combat-low-hp') {
    const max = Math.max(1, player.health?.max ?? 100);
    return (player.health?.current ?? max) / max < 0.65;
  }
  if (rule === 'encumbered') {
    const max = Math.max(1, player.carryWeight?.max ?? 300);
    return (player.carryWeight?.current ?? 0) / max >= 0.75;
  }
  if (rule === 'has-effects') return (player.activeEffectCount ?? 0) > 0;
  if (rule === 'has-followers') return (player.followerCount ?? 0) > 0;
  return true;
}

function renderWidgetHtml(kind, player, wp) {
  const label = wp.customLabel || kind.toUpperCase();
  const bound = wp.bindingPath ? resolveBinding(player, wp.bindingPath) : null;
  if (kind === 'vitals') {
    const hp = player?.health ?? { current: 100, max: 100 };
    const mp = player?.magicka ?? { current: 100, max: 100 };
    const sp = player?.stamina ?? { current: 100, max: 100 };
    const pct = (v) => Math.max(0, Math.min(100, Math.round((v.current / Math.max(1, v.max)) * 100)));
    return \`<div class="hud-widget__label">\${label}</div>
      <div class="hud-vitals-row">
        <div class="hud-vital-pill" style="--vital-tone:#ff5c6c"><small>HP \${Math.round(hp.current)}</small><div class="hud-vital-bar"><i style="width:\${pct(hp)}%"></i></div></div>
        <div class="hud-vital-pill" style="--vital-tone:#38bdf8"><small>MP \${Math.round(mp.current)}</small><div class="hud-vital-bar"><i style="width:\${pct(mp)}%"></i></div></div>
        <div class="hud-vital-pill" style="--vital-tone:#39ff14"><small>SP \${Math.round(sp.current)}</small><div class="hud-vital-bar"><i style="width:\${pct(sp)}%"></i></div></div>
      </div>\`;
  }
  const fallback =
    kind === 'compass' ? \`\${player?.location || 'Skyrim'} · \${Math.round(player?.angleZ ?? 0)}°\` :
    kind === 'character' ? \`\${player?.playerName || 'Dragonborn'} · LVL \${player?.level ?? 1}\` :
    kind === 'loot' ? \`\${Math.round(player?.carryWeight?.current ?? 0)}/\${Math.round(player?.carryWeight?.max ?? 300)} WT · \${(player?.gold ?? 0).toLocaleString()} G\` :
    kind === 'armory' || kind === 'stealth' ? \`ARMOR \${Math.round(player?.armorRating ?? 0).toLocaleString()}\` :
    kind === 'wards' || kind === 'effects' ? \`\${player?.activeEffectCount ?? 0} ACTIVE EFFECTS\` :
    \`\${player?.playerName || 'SkyUltra'} · LIVE\`;
  return \`<div class="hud-widget__label">\${label}</div><div class="hud-widget__value">\${bound ?? fallback}</div>\`;
}

export async function bootHudMod(mode) {
  const bridge = window.SkyWebView;
  let player = {
    playerName: 'Dragonborn',
    level: 42,
    location: 'Whiterun',
    gold: 1250,
    armorRating: 340,
    activeEffectCount: 3,
    followerCount: 1,
    health: { current: 280, max: 320 },
    magicka: { current: 180, max: 200 },
    stamina: { current: 210, max: 240 },
    carryWeight: { current: 142, max: 350 },
  };

  const root = document.getElementById('app');
  const draw = () => {
    if (!root) return;
    if (mode === 'panel') {
      root.innerHTML = \`<div class="mod-control-shell"><div class="mod-control-card">
        <div class="mod-control-head"><img src="./assets/emblem.svg" alt="" /><div><div class="hud-widget__label">${id} · v${version}</div><h2 style="margin:4px 0 0">${name.replace(/`/g, '')}</h2></div></div>
        <p style="margin:0;color:var(--mod-muted);font-size:13px">Connected as <b>\${player.playerName}</b> in <b>\${player.location}</b> (\${EXPORTED_LAYOUT.widgets.length} pinned HUD widgets · suggested hotkey ${hotkey}).</p>
        <div class="mod-control-actions">
          <button class="mod-btn" id="btn-pin">Activate HUD Overlay</button>
          <button class="mod-btn" id="btn-unpin">Hide Overlay</button>
          <button class="mod-btn" id="btn-close">Close Panel</button>
        </div>
      </div></div>\`;
      document.getElementById('btn-pin')?.addEventListener('click', () => bridge?.request('ui.configureWidgets', { ...EXPORTED_LAYOUT, enabled: true }));
      document.getElementById('btn-unpin')?.addEventListener('click', () => bridge?.request('ui.configureWidgets', { ...EXPORTED_LAYOUT, enabled: false, widgets: [] }));
      document.getElementById('btn-close')?.addEventListener('click', () => bridge?.request('ui.close', {}));
      return;
    }
    root.innerHTML = '<div class="hud-screen-grid">' + ZONES.map((zone) => {
      const items = EXPORTED_LAYOUT.widgets.filter((k) => (EXPORTED_LAYOUT.placements?.[k]?.anchor || EXPORTED_LAYOUT.anchor) === zone);
      if (!items.length) return \`<div class="hud-zone hud-zone--\${zone}"></div>\`;
      const children = items.map((k) => {
        const wp = EXPORTED_LAYOUT.placements?.[k] || {};
        if (!passesVisibilityRule(player, wp.visibilityRule)) return '';
        const scale = wp.scale ?? EXPORTED_LAYOUT.scale ?? 1;
        const opacity = wp.opacity ?? EXPORTED_LAYOUT.opacity ?? 0.96;
        const ox = wp.offsetX ?? 0;
        const oy = wp.offsetY ?? 0;
        const color = wp.color || EXPORTED_LAYOUT.colors?.[k] || '#e8b059';
        const glow = Math.round(14 * ((wp.glow ?? EXPORTED_LAYOUT.glow ?? 100) / 100));
        const widthCss = wp.width ? \`width:\${wp.width}px;\` : '';
        return \`<div class="hud-widget" style="\${widthCss}opacity:\${opacity};transform:translate(\${ox}px,\${oy}px) scale(\${scale});--widget-color:\${color};--glow-px:\${glow}px">\${renderWidgetHtml(k, player, wp)}</div>\`;
      }).join('');
      return \`<div class="hud-zone hud-zone--\${zone}">\${children}</div>\`;
    }).join('') + '</div>';
  };

  draw();
  if (bridge) {
    try {
      const sub = await bridge.request('game.subscribe', {
        topics: ['vitals', 'equipment', 'inventory', 'effects', 'followers'],
        intervalMs: 250,
        scope: 'hud',
      });
      if (sub?.update?.snapshot) { player = { ...player, ...sub.update.snapshot }; draw(); }
    } catch {
      try {
        const snap = await bridge.request('game.getPlayerSnapshot', { scope: 'hud' });
        if (snap?.available) { player = { ...player, ...snap }; draw(); }
      } catch {}
    }
    bridge.on('skyultra.game.update', (ev) => {
      if (ev?.snapshot) player = { ...player, ...ev.snapshot };
      else if (ev?.delta) player = { ...player, ...ev.delta };
      draw();
    });
    requestAnimationFrame(() => void bridge.request('ui.pageReady', {}).catch(() => undefined));
  }
}
`;

  const indexHtml = `<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>${name}</title>
    <link rel="stylesheet" href="./assets/hud-runtime.css" />
  </head>
  <body>
    <div id="app"></div>
    <script type="module">
      import { bootHudMod } from './assets/hud-runtime.js';
      bootHudMod('panel');
    </script>
  </body>
</html>
`;

  const hudHtml = hasCustomHud && customCode
    ? buildCustomHudDocument(customCode, { standalone: true })
    : `<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>${name} — HUD</title>
    <link rel="stylesheet" href="./assets/hud-runtime.css" />
  </head>
  <body>
    <div id="app"></div>
    <script type="module">
      import { bootHudMod } from './assets/hud-runtime.js';
      bootHudMod('hud');
    </script>
  </body>
</html>
`;

  const srcHudTsx = `import { getSkyWebView, markSkyUltraPageReady, parseSkyUltraHudLayout, subscribeSkyUltraGame } from '@skyultra/sdk';
import rawLayout from './layout.json';

const layout = parseSkyUltraHudLayout(rawLayout);
const bridge = getSkyWebView();

const subscription = subscribeSkyUltraGame(
  bridge,
  ({ snapshot }) => {
    // Live incremental game-data updates pushed directly from SkyUltra native host
    console.debug('[${id}] HUD snapshot seq', snapshot.sequence, layout.widgets);
  },
  { topics: ['vitals', 'equipment', 'inventory', 'effects', 'followers'], scope: 'hud', intervalMs: 250 },
);

requestAnimationFrame(() => {
  void markSkyUltraPageReady(bridge);
});

window.addEventListener('pagehide', () => subscription.unsubscribe());
`;

  const packageJson = JSON.stringify(
    {
      name: id.replace(/\./g, '-'),
      version,
      private: true,
      type: 'module',
      author,
      scripts: {
        dev: 'vite',
        build: 'vite build',
      },
      dependencies: {
        '@skyultra/sdk': '^0.1.0',
      },
      devDependencies: {
        '@skyultra/vite-plugin': '^0.1.0',
        typescript: '^5.7.0',
        vite: '^6.0.0',
      },
    },
    null,
    2,
  );

  const viteConfigTs = `import { defineConfig } from 'vite';
import { skyultra } from '@skyultra/vite-plugin';

export default defineConfig({
  plugins: [skyultra({ manifest: 'manifest.json' })],
  build: {
    rollupOptions: {
      input: {
        main: 'index.html',
        hud: 'hud.html',
      },
    },
  },
});
`;

  const placementLines = layout.widgets.map((kind: SkyUltraWidgetKind) => {
    const wp = layout.placements?.[kind];
    const anchor: SkyUltraWidgetAnchor = wp?.anchor ?? layout.anchor;
    const ox = wp?.offsetX ?? 0;
    const oy = wp?.offsetY ?? 0;
    const bind = wp?.bindingPath ? ` · bound to \`${wp.bindingPath}\`` : '';
    const vis = wp?.visibilityRule && wp.visibilityRule !== 'always' ? ` · rule \`${wp.visibilityRule}\`` : '';
    return `- **${kind}** → \`${anchor}\` (\`offset ${ox}, ${oy}px\`, scale \`${Math.round((wp?.scale ?? layout.scale) * 100)}%\`)${bind}${vis}`;
  });

  const readmeMd = `# ${name} (\`${id}\` v${version})

Generated by **SkyUltra Visual HUD Builder**.

## Included Package Layout
- \`manifest.json\` — SkyUltra schema v1 manifest with least-privilege \`pageRequests\`
- \`index.html\` & \`hud.html\` — Ready-to-run SkyUltra panel and transparent 6-zone HUD overlay
- \`assets/emblem.svg\`, \`assets/hud-runtime.css\`, \`assets/hud-runtime.js\` — Bundled mod assets & live \`skyultra.game.update\` runtime
- \`src/layout.json\` & \`src/hud.tsx\` — Editable \`@skyultra/sdk\` TypeScript source

## Configured Widgets
${placementLines.join('\n') || '- None'}

## Installation
Place this folder at \`Data/SkyUltra/${id}/\` (or inspect/hot-reload it live in **SkyUltra → Developer Studio**).
`;

  return {
    id,
    name,
    version,
    files: [
      { path: 'manifest.json', language: 'json', description: 'SkyUltra v1 mod manifest & permissions', content: JSON.stringify(manifestObj, null, 2) + '\n' },
      { path: 'index.html', language: 'html', description: 'Mod control surface & overlay activator', content: indexHtml },
      { path: 'hud.html', language: 'html', description: 'Transparent 6-zone containerless HUD entry', content: hudHtml },
      { path: 'assets/emblem.svg', language: 'svg', description: 'Vector mod crest asset', content: emblemSvg },
      { path: 'assets/hud-runtime.css', language: 'css', description: '6-zone transparent HUD stylesheet', content: runtimeCss },
      { path: 'assets/hud-runtime.js', language: 'javascript', description: 'Zero-build live SkyWebView HUD runtime', content: runtimeJs },
      { path: 'src/layout.json', language: 'json', description: 'Serializable SkyUltraHudLayout preset', content: layoutJson + '\n' },
      { path: 'src/hud.tsx', language: 'typescript', description: 'Typed @skyultra/sdk subscription entry', content: srcHudTsx },
      { path: 'package.json', language: 'json', description: 'NPM package manifest', content: packageJson + '\n' },
      { path: 'vite.config.ts', language: 'typescript', description: 'Vite + @skyultra/vite-plugin config', content: viteConfigTs },
      { path: 'README.md', language: 'markdown', description: 'Authoring & installation guide', content: readmeMd },
    ],
  };
}

export async function exportSkyUltraHudMod(
  spec: SkyUltraHudModExportSpec,
  bridge: SkyWebViewAPI = getSkyWebView(),
  options?: SkyUltraRequestOptions,
): Promise<SkyUltraHudModExportResult> {
  const pkg = generateSkyUltraHudModFiles(spec);
  const payload = {
    id: pkg.id,
    name: pkg.name,
    version: pkg.version,
    files: pkg.files.map((f) => ({ path: f.path, content: f.content })),
  } as unknown as JsonValue;
  return bridge.request<SkyUltraHudModExportResult>('ui.exportHudMod', payload, options);
}
