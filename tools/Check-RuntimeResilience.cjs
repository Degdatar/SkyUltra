// Production SDK and observer checks executed in memory; no emitted files or game access.
const fs = require('node:fs'), path = require('node:path'), vm = require('node:vm');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const ts = require(path.join(root, 'sdk/ui/node_modules/typescript'));
async function main() {
  const api = {};
  vm.runInNewContext(ts.transpileModule(read('sdk/typescript/src/devtools.ts'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
  }).outputText, { exports: api, require: () => ({}) });
  const calls = [], options = { signal: new AbortController().signal };
  const bridge = { request: async (...args) => {
    calls.push(args); return { accepted: true, modId: args[1].modId, action: args[1].action, affectedViews: 2 };
  }};
  for (const action of ['suspend', 'resume', 'unload']) {
    const result = await api.manageSkyUltraModRuntime(' author.mod ', action, bridge, options);
    assert.equal(JSON.stringify(calls.at(-1).slice(0, 2)), JSON.stringify(['ui.manageModRuntime', { modId: 'author.mod', action }]));
    assert.equal(calls.at(-1)[2], options); assert.equal(result.action, action); assert.equal(result.affectedViews, 2);
  }
  const count = calls.length;
  for (const id of ['', '  ', null, 42]) assert.throws(() => api.manageSkyUltraModRuntime(id, 'suspend', bridge), /mod ID/);
  for (const action of ['stop', '', null]) assert.throws(() => api.manageSkyUltraModRuntime('author.mod', action, bridge), /runtime action/);
  assert.equal(calls.length, count, 'Invalid actions never reach the native bridge');
  await assert.rejects(api.manageSkyUltraModRuntime('author.mod', 'suspend', {
    request: async () => { throw Error('Only the built-in menu may invoke host diagnostics'); },
  }), /Only the built-in menu/, 'Native denials propagate');

  const resources = { maxViews: 16, views: 2, surfaceByteLimit: 134217728, reservedSurfaceBytes: 16588800,
    messageLimit: 4096, messageByteLimit: 134217728, eventCount: 12, eventBytes: 600,
    replyCount: 2, replyBytes: 120, rejected: 1, suspended: true };
  const recovery = { rendererCrashes: 2, contextLosses: 3, recoveryAttempts: 2, stalledBatches: 1,
    recovering: false, recoveryBlocked: true, crashReportsEnabled: false, lastFailure: 'Renderer terminated' };
  const mod = { id: 'author.mod', name: 'Test mod', version: '1.0.0', folderName: 'author.mod', manifestValid: true,
    entryExists: true, missingAssets: [], pageRequests: ['ui.pageReady'], resources, totalAssetBytes: 20 };
  const snapshot = await api.readSkyUltraDeveloperSnapshot({ request: async () => ({ available: true,
    mods: [mod], views: [{ handle: 1, suspended: true, resources, recovery }], bridgeLog: [] }) });
  assert.deepEqual(snapshot.mods[0].resources, resources);
  assert.deepEqual(snapshot.views[0].recovery, recovery); assert.equal(snapshot.views[0].suspended, true);
  assert.equal(snapshot.mods[0].findings[0].code, 'HEALTHY');

  // Execute the actual renderer observer without intercepting a mod's restoration behavior.
  const observer = read('src/cef/CefSubprocess.cpp').match(/context->Eval\("([^"]+)",\s*"skyultra:\/\/context-loss-observer"/);
  assert.ok(observer, 'Missing production WebGL observer');
  const listeners = []; let notifications = 0, prevented = false, modNotifications = 0;
  const document = { addEventListener: (name, callback, capture) => listeners.push({ name, callback, capture }) };
  document.addEventListener('webglcontextlost', event => { modNotifications++; event.preventDefault(); }, false);
  vm.runInNewContext(observer[1], { document, window: { __skyultraReportContextLoss: () => { notifications++; } } });
  assert.equal(listeners.length, 2); assert.equal(listeners[1].capture, true);
  const event = { preventDefault: () => { prevented = true; } };
  listeners[1].callback(event); assert.equal(notifications, 1); assert.equal(prevented, false);
  listeners[0].callback(event); assert.equal(modNotifications, 1); assert.equal(prevented, true);

  const manifest = JSON.parse(read('apps/skyultra-menu/public/manifest.json'));
  assert.ok(manifest.permissions.pageRequests.includes('ui.manageModRuntime'));
  const plugin = read('src/skse/Plugin.cpp');
  assert.match(plugin, /message\.name == "ui\.manageModRuntime"[\s\S]{0,180}message\.view_id != g_menu_view_id/);
  assert.match(plugin, /id == "skyultra\.menu"\) return rejected/);
  const hotkey = plugin.slice(plugin.indexOf('bool ToggleModViewsByHotkey('), plugin.indexOf('bool ToggleModViewsByHotkey(') + 10000);
  assert.match(hotkey, /record\.mod_handle = mod_handle;\s*if \(!BindViewBudget\(record, config\)\)/,
    'Hotkey-created queues must use the registered owner budget, not legacy owner zero');
  const backend = read('src/cef/CefBackend.cpp');
  assert.ok(backend.includes('kMaxCefWindowlessFrameRate = 120;'));
  assert.ok(!backend.includes('AppendSwitch("disable-renderer-backgrounding")'));
  assert.ok(!backend.includes('AppendSwitch("disable-backgrounding-occluded-windows")'));
  assert.ok(read('src/ultralight/UltralightBackend.cpp').includes('microseconds(8333)'));
  assert.equal(read('include/SkyUltra/NativeAPI.h'), read('api/include/SkyUltra/NativeAPI.h'));
  console.log('Runtime resilience checks passed: SDK controls/validation/denials, diagnostic snapshots, non-invasive WebGL observer, host privilege guard, unchanged frame pacing, and matching public API headers. No runtime crash or GPU recovery was exercised.');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
