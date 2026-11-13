// Execute production TypeScript in memory. No bundles, binaries or game launch.
const fs = require('node:fs'), path = require('node:path'), vm = require('node:vm');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const ts = require(path.join(root, 'sdk/ui/node_modules/typescript'));
function read(relative) { return fs.readFileSync(path.join(root, relative), 'utf8'); }
function load(relative, requireModule, globals = {}) {
  const exports = {};
  const js = ts.transpileModule(read(relative), { compilerOptions: {
    module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022, jsx: ts.JsxEmit.ReactJSX,
  } }).outputText;
  vm.runInNewContext(js, { exports, require: requireModule, console, setTimeout, clearTimeout, AbortController, Event, ...globals }, { filename: relative });
  return exports;
}
const flush = async () => { for (let i = 0; i < 8; ++i) await Promise.resolve(); };
async function main() {
  const win = new EventTarget(), imports = [];
  const navigation = load('sdk/ui/src/field/menuNavigation.ts', name => { imports.push(name); return { default() {} }; }, { window: win });
  const lifecycle = load('sdk/ui/src/field/previewLifecycle.ts',()=>({}),{window:win});
  const key = (extra = {}) => ({ key: ' ', code: 'Space', target: { closest: () => null }, ...extra });
  assert.equal(navigation.isMenuSearchKey(key()), true);
  assert.equal(navigation.isMenuSearchKey(key({ repeat: true })), true, 'Held Space is still consumed, rather than activating a button');
  for (const extra of [{ctrlKey:true}, {altKey:true}, {shiftKey:true}, {metaKey:true}, {isComposing:true}, {defaultPrevented:true}, {key:'/',code:'Slash'}, {key:'k',code:'KeyK',ctrlKey:true}, {target:{closest:()=>({})}}]) {
    assert.equal(navigation.isMenuSearchKey(key(extra)), false);
  }
  let focused = '';
  const input = (id, flags = {}) => ({ disabled: false, hidden: false, getClientRects: () => [1], focus: () => { focused = id; }, ...flags });
  assert.equal(navigation.focusMenuSearch({ querySelectorAll: () => [input('hidden', {getClientRects:()=>[]}), input('disabled', {disabled:true}), input('visible')] }), true);
  assert.equal(focused, 'visible');
  assert.equal(navigation.focusMenuSearch({querySelectorAll:()=>[]}), false);
  const a = navigation.loadMenuPage('actors');
  assert.equal(a, navigation.loadMenuPage('actors'), 'Intent and React.lazy must share an in-flight module');
  await a; assert.deepEqual(imports, ['./ActorsPage.js']);
  navigation.preloadMenuPage('overview'); navigation.preloadMenuPage('components');
  assert.equal(imports.length, 1);

  // Real ModelStage hook callbacks: navigate while its native show is pending.
  const layouts = [], timers = new Map(), nativeCalls = [], updates = [];
  let nextTimer = 0, refIndex = 0, resolveShow;
  const stage = {getBoundingClientRect:()=>({left:300,top:200,right:800,bottom:700,width:500,height:500}),addEventListener(){},removeEventListener(){}};
  const react = { useMemo: fn=>fn(), useRef: value=>({current:refIndex++===0?stage:value}), useId:()=>'stage',
    useState: value=>[typeof value==='function'?value():value, value=>updates.push(value)],
    useLayoutEffect: fn=>layouts.push(fn), useEffect(){} };
  const renderer = {
    setActor(options, request) { nativeCalls.push({kind:'show',options,signal:request.signal}); return new Promise(resolve=>{resolveShow=resolve;}); },
    hide() {nativeCalls.push({kind:'hide'});return Promise.resolve({available:true,ready:false});},
    close() {nativeCalls.push({kind:'close'});return Promise.resolve({available:true,ready:false});},
    status:async()=>({available:true,ready:true}),
  };
  Object.assign(win, {innerWidth:1920,innerHeight:1080,devicePixelRatio:1,setTimeout:fn=>{timers.set(++nextTimer,fn);return nextTimer;},clearTimeout:id=>timers.delete(id),setInterval:()=>1,clearInterval(){}});
  const doc = new EventTarget(); Object.assign(doc,{hidden:false,querySelector:()=>null});
  const camera = load('sdk/ui/src/field/actorCamera.ts',()=>({}));
  const model = load('sdk/ui/src/field/ActorStage.tsx', name => {
    if(name==='react')return react;
    if(name==='react/jsx-runtime')return {jsx:()=>({}),jsxs:()=>({})};
    if(name==='@skyultra/sdk')return {createSkyUltraRenderer:()=>renderer,createSkyUltraItemRenderer:()=>renderer};
    if(name==='./actorCamera.js')return camera;
    if(name==='./previewLifecycle.js')return lifecycle;
    return {};
  },{window:win,document:doc,ResizeObserver:class{observe(){}disconnect(){}}});
  const bridge={getVisibility:()=>({visible:true}),on:()=>()=>{}};
  model.ModelStage({subject:{kind:'actor',actor:{referenceId:'0x14'}},bridge,obscured:false});
  const dispose = layouts.map(fn=>fn());
  for (const [id,fn] of [...timers]) {timers.delete(id);fn();}
  assert.equal(nativeCalls[0].kind,'show');
  lifecycle.leaveMenuPage();
  assert.equal(nativeCalls.at(-1).kind,'hide','Navigation hides before the next lazy page can load');
  assert.equal(nativeCalls[0].signal.aborted,true);
  win.dispatchEvent(new Event('resize'));
  assert.equal(timers.size,0,'Outgoing layout events must not show the stage again');
  resolveShow({available:true,ready:true}); await flush();
  assert.equal(updates.length,0,'A late show response must not revive an outgoing controller');
  dispose.forEach(fn=>fn?.());
  assert.equal(nativeCalls.at(-1).kind,'close');

  // Use the installed production shims, exercising both host loopback formats.
  const listeners = new Map();
  const host = {on(name,fn){if(!listeners.has(name))listeners.set(name,new Set());listeners.get(name).add(fn);return()=>listeners.get(name).delete(fn);},
    async request(name,payload){assert.equal(name,'ui.interopCall');for(const fn of [...(listeners.get('skyultra.legacy.interop')??[])])fn(payload);return {accepted:true};}};
  const sdk = load('sdk/typescript/src/enhancements.ts',()=>({getSkyWebView:()=>host}));
  const shims = sdk.installSkyUltraCompetitorShims({},host);
  const verify = load('sdk/ui/src/field/verifyShims.ts',()=>({})).verifyActiveShims;
  assert.ok((await verify(shims)).every(result=>result.passed));
  assert.ok((await verify({})).every(result=>!result.passed && /not installed/i.test(result.message)));
  const silent={prismaUI:{RegisterJSListener:()=>()=>{},InteropCall:async()=>({accepted:true})}};
  assert.match((await verify(silent,undefined,1))[0].message,/No host echo/);
  const denied={magelight:{on:()=>()=>{},send:async()=>{throw Error('Permission denied');}}};
  assert.match((await verify(denied))[1].message,/Permission denied/);
  const abort = new AbortController(); const cancelled = verify(silent,abort.signal); abort.abort();
  assert.match((await cancelled)[0].message,/cancelled/);
  assert.ok([...listeners.values()].every(set=>set.size===0),'Verification must release every temporary listener');
  shims.uninstall();

  // SWFs are empty Scaleform launch carriers; web pages own all menu shortcuts.
  for (const menu of ['inventory','barter','magic','favorites','container','mannequin','loot','pickpocket']) {
    const bytes=fs.readFileSync(path.join(root,'resources/skyultra_'+menu+'.swf'));
    assert.equal(bytes.subarray(0,3).toString(),'FWS'); assert.equal(bytes.length,35);
    // Only Stop/End bytecode, with no keyboard handler (parse after RECT/rate/count).
    let offset=8+Math.ceil((5+4*(bytes[8]>>3))/8)+4;
    while(offset<bytes.length){const tag=bytes.readUInt16LE(offset);offset+=2;const code=tag>>6;let size=tag&63;if(size===63){size=bytes.readUInt32LE(offset);offset+=4;}if(code===12)assert.equal(bytes.subarray(offset,offset+size).toString('hex'),'0700');assert.notEqual(code,59);offset+=size;}
  }
  console.log('Menu checks passed: Space routing, shared intent loads, pending preview navigation, shim success/failure/timeout/cancellation, SWF carriers.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
