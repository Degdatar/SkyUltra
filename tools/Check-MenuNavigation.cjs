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
  const strayInput = Object.assign(new EventTarget(), {
    disabled: false, hidden: false, getClientRects: () => [1], focus: () => { focused = 'stray'; }, select: () => {},
  });
  assert.equal(navigation.focusMenuSearch({ querySelectorAll: () => [strayInput] }), true);
  assert.equal(focused, 'stray');
  const spaceBefore = Object.assign(new Event('beforeinput', { cancelable: true }), { data: ' ', inputType: 'insertText' });
  strayInput.dispatchEvent(spaceBefore);
  assert.equal(spaceBefore.defaultPrevented, true, 'Immediate paired Skyrim CharEvent space is suppressed');
  const normalChar = Object.assign(new Event('beforeinput', { cancelable: true }), { data: 'a', inputType: 'insertText' });
  strayInput.dispatchEvent(normalChar);
  assert.equal(normalChar.defaultPrevented, false, 'Normal typed characters proceed untouched');
  const intentionalSpace = Object.assign(new Event('beforeinput', { cancelable: true }), { data: ' ', inputType: 'insertText' });
  strayInput.dispatchEvent(intentionalSpace);
  assert.equal(intentionalSpace.defaultPrevented, false, 'Subsequent intentional space is allowed after one-shot guard');
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
  for (const menu of ['inventory','barter','magic','favorites','container','mannequin','loot','pickpocket','blacksmith','enchanting','alchemy','book']) {
    const bytes=fs.readFileSync(path.join(root,'resources/skyultra_'+menu+'.swf'));
    assert.equal(bytes.subarray(0,3).toString(),'FWS'); assert.equal(bytes.length,35);
    // Only Stop/End bytecode, with no keyboard handler (parse after RECT/rate/count).
    let offset=8+Math.ceil((5+4*(bytes[8]>>3))/8)+4;
    while(offset<bytes.length){const tag=bytes.readUInt16LE(offset);offset+=2;const code=tag>>6;let size=tag&63;if(size===63){size=bytes.readUInt32LE(offset);offset+=4;}if(code===12)assert.equal(bytes.subarray(offset,offset+size).toString('hex'),'0700');assert.notEqual(code,59);offset+=size;}
  }
  // Magic & Favorites left/right click equip rules (spells vs powers/shouts vs items).
  const memoReact = { memo: fn => fn };
  const noopProxy = new Proxy({}, { get: () => () => null });
  const magic = load('sdk/ui/src/field/MagicPage.tsx', name => {
    if (name === 'react') return memoReact;
    if (name === 'react/jsx-runtime') return { jsx: () => ({}), jsxs: () => ({}), Fragment: () => ({}) };
    return noopProxy;
  });
  const favorites = load('sdk/ui/src/field/FavoritesPage.tsx', name => {
    if (name === 'react') return memoReact;
    if (name === 'react/jsx-runtime') return { jsx: () => ({}), jsxs: () => ({}), Fragment: () => ({}) };
    if (name === './MagicPage.js') return magic;
    return noopProxy;
  });
  const oneHandSpell = { kind: 'spell', hands: ['left', 'right', 'both'], twoHanded: false };
  const twoHandSpell = { kind: 'spell', hands: ['both'], twoHanded: true };
  const shoutEntry = { kind: 'shout', hands: ['voice'] };
  const powerEntry = { kind: 'power', hands: ['voice'] };
  const lesserPowerEntry = { kind: 'lesserPower', hands: ['voice'] };
  assert.equal(magic.resolveSpellClickHand(oneHandSpell, 'left'), 'left');
  assert.equal(magic.resolveSpellClickHand(oneHandSpell, 'right'), 'right');
  assert.equal(magic.resolveSpellClickHand(twoHandSpell, 'left'), 'both');
  assert.equal(magic.resolveSpellClickHand(twoHandSpell, 'right'), 'both');
  for (const voiceItem of [shoutEntry, powerEntry, lesserPowerEntry]) {
    assert.equal(magic.resolveSpellClickHand(voiceItem, 'left'), 'voice');
    assert.equal(magic.resolveSpellClickHand(voiceItem, 'right'), null);
  }
  const favDualLeft = favorites.resolveFavoriteItemClickAction({ equipSlots: ['mainHand', 'offHand'], equipped: false }, 'left');
  assert.equal(favDualLeft?.kind, 'equip');
  assert.equal(favDualLeft?.slot, 'offHand');
  const favDualRight = favorites.resolveFavoriteItemClickAction({ equipSlots: ['mainHand', 'offHand'], equipped: false }, 'right');
  assert.equal(favDualRight?.kind, 'equip');
  assert.equal(favDualRight?.slot, 'mainHand');
  const favBodyLeft = favorites.resolveFavoriteItemClickAction({ equipSlots: ['body'], equipped: false }, 'left');
  assert.equal(favBodyLeft?.kind, 'equip');
  assert.equal(favBodyLeft?.slot, 'auto');
  assert.equal(favorites.resolveFavoriteItemClickAction({ equipSlots: ['body'], equipped: false }, 'right'), null);
  const favPotionLeft = favorites.resolveFavoriteItemClickAction({ equipSlots: [], equipped: false, useAction: 'drink' }, 'left');
  assert.equal(favPotionLeft?.kind, 'use');
  assert.equal(favorites.resolveFavoriteItemClickAction({ equipSlots: [], equipped: false, useAction: 'drink' }, 'right'), null);

  // Optimistic spell & item equip helpers.
  const optLeft = magic.computeOptimisticSpellEquipped({ left: 'a', right: 'b', voice: 'v' }, 'c', 'left');
  assert.equal(optLeft.left, 'c');
  assert.equal(optLeft.right, 'b');
  assert.equal(optLeft.voice, 'v');
  const optRight = magic.computeOptimisticSpellEquipped(optLeft, 'd', 'right');
  assert.equal(optRight.left, 'c');
  assert.equal(optRight.right, 'd');
  assert.equal(optRight.voice, 'v');
  const optBoth = magic.computeOptimisticSpellEquipped(optRight, 'e', 'both');
  assert.equal(optBoth.left, 'e');
  assert.equal(optBoth.right, 'e');
  assert.equal(optBoth.voice, 'v');
  const optVoice = magic.computeOptimisticSpellEquipped(optBoth, 's', 'voice');
  assert.equal(optVoice.left, 'e');
  assert.equal(optVoice.right, 'e');
  assert.equal(optVoice.voice, 's');

  const favItemsInitial = [{ id: '10', equipped: false }, { id: '20', equipped: true }];
  const favItemsUpdated = favorites.applyFavoriteItemEquipment(favItemsInitial, { mainHand: { id: '10' }, body: null });
  assert.equal(favItemsUpdated[0].equipped, true);
  assert.equal(favItemsUpdated[1].equipped, false);
  const favItemsUnchanged = favorites.applyFavoriteItemEquipment(favItemsUpdated, { mainHand: { id: '10' } });
  assert.equal(favItemsUnchanged, favItemsUpdated, 'applyFavoriteItemEquipment preserves array identity when unchanged');

  const menuCss = read('sdk/ui/src/menu.css');
  assert.ok(menuCss.includes('.favorites-row__art > .item-thumbnail'), 'Favorites list view item thumbnails must size explicitly');
  assert.ok(menuCss.includes('.favorites-selection__item-art > .item-thumbnail'), 'Favorites selection item thumbnails must size explicitly');
  const backdropRule = menuCss.match(/\.spellbook-backdrop\s+i\s*\{[\s\S]*?\}/);
  assert.ok(backdropRule && !/animation\s*:/.test(backdropRule[0]), '.spellbook-backdrop i must not run continuous animations');

  // Arcane Enchanter helpers, CSS theme safety, and manifest permissions.
  const hookReact = {
    useCallback: fn => fn,
    useEffect: () => {},
    useMemo: fn => fn(),
    useRef: v => ({ current: v }),
    useState: v => [typeof v === 'function' ? v() : v, () => {}],
  };
  const enchanting = load('sdk/ui/src/field/EnchantingPage.tsx', name => {
    if (name === 'react') return { ...hookReact, default: hookReact };
    if (name === 'react/jsx-runtime') return { jsx: () => ({}), jsxs: () => ({}), Fragment: () => ({}) };
    if (name === '@skyultra/sdk') return { createSkyUltraGame: () => ({}) };
    if (name === './model.js') return { formId: id => String(id) };
    return noopProxy;
  });
  const weaponEnch = { targetKind: 'weapon', wornRestrictionIds: [], wornRestrictionLabels: [] };
  const armorRingEnch = { targetKind: 'armor', wornRestrictionIds: ['441321'], wornRestrictionLabels: ['Ring'] };
  const armorCuirassItem = { itemKind: 'armor', keywordIds: ['0006C0EC'] };
  const armorRingItem = { itemKind: 'armor', keywordIds: ['0006BBE9'] };
  assert.equal(enchanting.isEnchantmentCompatibleWithItem(weaponEnch, armorRingItem).compatible, false);
  assert.equal(enchanting.isEnchantmentCompatibleWithItem(armorRingEnch, armorCuirassItem).compatible, false);
  assert.equal(enchanting.isEnchantmentCompatibleWithItem(armorRingEnch, armorRingItem).compatible, true);
  assert.equal(enchanting.isEnchantmentCompatibleWithItem(armorRingEnch, armorCuirassItem, true).compatible, true, 'Super Enchanting bypasses armor slot restrictions');
  assert.equal(enchanting.isEnchantmentCompatibleWithItem(weaponEnch, armorRingItem, true).compatible, false, 'Super Enchanting still separates weapons from armor');

  // Strict auto-filled exact-match Soul Gem invariant (cannot drop below or exceed item count).
  const batchGems = [
    { id: 'grand', count: 6, soulLevel: 5, isReusable: false },
    { id: 'greater', count: 5, soulLevel: 4, isReusable: false },
  ];
  const autoTen = enchanting.reconcileExactGemAllocations(batchGems, {}, 10);
  assert.deepEqual({ ...autoTen }, { grand: 6, greater: 4 }, 'Auto-fill must supply all 10 soul gems across stacks');
  const swapUp = enchanting.rebalanceGemAllocation(batchGems, autoTen, 'greater', 1, 10);
  assert.deepEqual({ ...swapUp }, { grand: 5, greater: 5 }, 'Adding another gem type must swap 1:1 while preserving total 10');
  const lockedTen = [{ id: 'grand', count: 10, soulLevel: 5, isReusable: false }];
  const tryUnderfill = enchanting.rebalanceGemAllocation(lockedTen, { grand: 10 }, 'grand', -1, 10);
  assert.deepEqual({ ...tryUnderfill }, { grand: 10 }, 'Cannot reduce allocated soul gems below selected item count (10)');

  const highPower = enchanting.computePreviewWeaponUses(3000, 30, 1.5, 1.0);
  const lowPower = enchanting.computePreviewWeaponUses(3000, 30, 1.5, 0.25);
  assert.ok(lowPower.uses > highPower.uses, 'Lowering weapon charge power must yield more uses');
  assert.ok(highPower.magnitudeScale > lowPower.magnitudeScale, 'Higher power slider must scale magnitude higher');
  assert.equal(enchanting.computePreviewWeaponUses(0, 30, 1.5, 0.8).uses, 0);

  assert.equal(
    enchanting.formatScaledEffectText(
      {
        name: 'Fire Damage',
        descriptionTemplate: 'Burns for <mag> pts over <dur>s.',
        baseMagnitude: 10,
        baseDuration: 2,
        powerAffectsMagnitude: true,
        powerAffectsDuration: false,
        noMagnitude: false,
        perkBonusMult: 1.5,
      },
      2.0,
    ),
    'Burns for 30 pts over 2s.',
  );

  const barterResultsMatch = menuCss.match(/\.inventory-page--armory\s+\.inventory-surface\.barter-surface\s*>\s*\.inventory-results\s*\{[\s\S]*?\}/);
  assert.ok(barterResultsMatch && /grid-row\s*:\s*3\b/.test(barterResultsMatch[0]), 'Barter results panel must occupy row 3 inside .barter-surface');

  assert.ok(menuCss.includes('.atlas.atlas--standalone-enchanting.atlas--enchanting-half'));
  assert.ok(menuCss.includes('.enchanting-page'));
  assert.ok(menuCss.includes('.enchanting-list--grid'), 'Arcane Enchanter Grid View CSS must be present');
  assert.ok(menuCss.includes('.enchanting-list--grid-4') && /repeat\(\s*4\s*,\s*minmax\(\s*0\s*,\s*1fr\s*\)\s*\)/.test(menuCss), 'Enchanting & Disenchanting grids must display 4 cards per row');
  assert.ok(menuCss.includes('.enchanting-row__thumb .item-art'));
  assert.ok(menuCss.includes('.atlas.atlas--standalone-alchemy'), 'Standalone Alchemy CSS must be present');
  assert.ok(menuCss.includes('.alchemy-page'));
  assert.ok(menuCss.includes('.alchemy-list--grid-4') && /grid-template-columns\s*:\s*repeat\(\s*4\s*,\s*minmax\(\s*0\s*,\s*1fr\s*\)\s*\)/.test(menuCss.slice(menuCss.indexOf('.alchemy-list--grid-4'))), 'Alchemy ingredient grid must display 4 cards per row');
  const alchGridRule = menuCss.match(/\.alchemy-list--grid\.alchemy-list--grid-4\s*\{[\s\S]*?\}/);
  assert.ok(alchGridRule && /grid-auto-rows:\s*(2[3-9][0-9]|[3-9][0-9]{2})px\b/.test(alchGridRule[0]), 'Alchemy 4-column grid must define fixed >= 230px row tracks so cards cannot collapse');
  const alchCardRule = menuCss.match(/\.alchemy-grid-card\s*\{[\s\S]*?\}/);
  assert.ok(alchCardRule && /display:\s*grid/.test(alchCardRule[0]) && /height:\s*(2[3-9][0-9]|[3-9][0-9]{2})px\b/.test(alchCardRule[0]) && /grid-template-rows:/.test(alchCardRule[0]), 'Alchemy ingredient cards must use fixed-height CSS grid rows matching Inventory item tiles');
  const alchArtRule = menuCss.match(/\.alchemy-grid-card__art\s*\{[\s\S]*?\}/);
  assert.ok(alchArtRule && /height:\s*(10[0-9]|1[1-9][0-9]|[2-9][0-9]{2})px\b/.test(alchArtRule[0]), 'Alchemy grid card artwork stage must allocate >= 100px vertical prominence');
  const alchRowThumbRule = menuCss.match(/\.alchemy-row__thumb\s*\{[\s\S]*?\}/);
  assert.ok(alchRowThumbRule && /width:\s*52px/.test(alchRowThumbRule[0]) && menuCss.includes('.alchemy-row__thumb .item-art'));
  assert.ok(menuCss.includes('.atlas.atlas--standalone-alchemy.atlas--alchemy-half .app-footer'), 'Alchemy half-panel compact footer CSS must be present');
  assert.ok(menuCss.includes('.atlas.atlas--standalone-alchemy.atlas--alchemy-windowed') && /margin:\s*max\(5vh,/.test(menuCss.slice(menuCss.indexOf('.atlas.atlas--standalone-alchemy.atlas--alchemy-windowed'))), 'Alchemy windowed mode must center both vertically and horizontally');
  assert.ok(menuCss.includes('.atlas.atlas--standalone-book'), 'Standalone world Book Reader CSS must be present');
  const enchCssStart = menuCss.indexOf('.atlas.atlas--standalone-enchanting.atlas--enchanting-half');
  const enchCssBody = enchCssStart >= 0 ? menuCss.slice(enchCssStart).replace(/\/\*[\s\S]*?\*\//g, '') : '';
  assert.ok(enchCssStart >= 0 && !/backdrop-filter|filter\s*:\s*blur|@keyframes/i.test(enchCssBody), 'Enchanting, Alchemy & standalone Book CSS must not use backdrop-filter, filter:blur, or @keyframes');
  const alchemyCssStart = menuCss.indexOf('.atlas.atlas--standalone-alchemy');
  const alchemyCssBody = alchemyCssStart >= 0 ? menuCss.slice(alchemyCssStart).replace(/\/\*[\s\S]*?\*\//g, '') : '';
  assert.ok(alchemyCssStart >= 0 && !/backdrop-filter|filter\s*:\s*blur|@keyframes/i.test(alchemyCssBody), 'Alchemy CSS must not use backdrop-filter, filter:blur, or @keyframes');

  const manifest = JSON.parse(read('apps/skyultra-menu/public/manifest.json'));
  for (const method of ['game.getEnchantingSnapshot', 'game.enchantItem', 'game.disenchantItem', 'game.renameEnchantment', 'game.getAlchemySnapshot', 'game.alchemyCraft', 'game.eatIngredient', 'game.getActiveBookSnapshot', 'game.takeActiveBook', 'game.openNativeActiveBook']) {
    assert.ok(manifest.permissions.pageRequests.includes(method), 'manifest.json must allow ' + method);
  }

  const alchemy = load('sdk/ui/src/field/AlchemyPage.tsx', name => {
    if (name === 'react') return { ...hookReact, default: hookReact };
    if (name === 'react/jsx-runtime') return { jsx: () => ({}), jsxs: () => ({}), Fragment: () => ({}) };
    if (name === '@skyultra/sdk') return { createSkyUltraGame: () => ({}) };
    if (name === '../core.js') return { SkyChoice: () => ({}), SkySegmentedControl: () => ({}) };
    if (name === './ItemThumbnail.js') return { ItemThumbnail: () => ({}) };
    if (name === './icons.js') return { Icon: () => ({}) };
    if (name === './pieces.js') return { Empty: () => ({}) };
    return {};
  });
  const testIngredients = [
    {
      id: '101',
      itemId: '101',
      name: 'Blue Mountain Flower',
      count: 6,
      value: 2,
      weight: 0.1,
      knownCount: 3,
      effects: [
        { slot: 0, known: true, effectId: 'e_res_hp', name: 'Restore Health', descriptionTemplate: 'Restore <mag> pts.', isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 0, baseCost: 1.5, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: 'Restoration', isRestoreVital: true },
        { slot: 1, known: true, effectId: 'e_fort_hp', name: 'Fortify Health', descriptionTemplate: 'Health +<mag> for <dur>s.', isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 60, baseCost: 2.2, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: 'Restoration', isRestoreVital: false },
        { slot: 2, known: true, effectId: 'e_dmg_stam', name: 'Damage Stamina', descriptionTemplate: 'Lose <mag> Stamina.', isHostile: true, isDetrimental: true, baseMagnitude: 3, baseDuration: 0, baseCost: 0.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: 'Destruction', isRestoreVital: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: '102',
      itemId: '102',
      name: 'Wheat',
      count: 4,
      value: 5,
      weight: 0.1,
      knownCount: 2,
      effects: [
        { slot: 0, known: true, effectId: 'e_res_hp', name: 'Restore Health', descriptionTemplate: 'Restore <mag> pts.', isHostile: false, isDetrimental: false, baseMagnitude: 5, baseDuration: 0, baseCost: 1.5, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: 'Restoration', isRestoreVital: true },
        { slot: 1, known: true, effectId: 'e_fort_hp', name: 'Fortify Health', descriptionTemplate: 'Health +<mag> for <dur>s.', isHostile: false, isDetrimental: false, baseMagnitude: 4, baseDuration: 60, baseCost: 2.2, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false, school: 'Restoration', isRestoreVital: false },
        { slot: 2, known: false },
        { slot: 3, known: false },
      ],
    },
    {
      id: '103',
      itemId: '103',
      name: 'Deathbell',
      count: 3,
      value: 4,
      weight: 0.1,
      knownCount: 1,
      effects: [
        { slot: 0, known: true, effectId: 'e_dmg_stam', name: 'Damage Stamina', descriptionTemplate: 'Lose <mag> Stamina.', isHostile: true, isDetrimental: true, baseMagnitude: 3, baseDuration: 0, baseCost: 0.8, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: true, school: 'Destruction', isRestoreVital: false },
        { slot: 1, known: false },
      ],
    },
    {
      id: '104',
      itemId: '104',
      name: 'Mystery Root',
      count: 5,
      value: 1,
      weight: 0.1,
      knownCount: 0,
      effects: [{ slot: 0, known: false, effectId: 'e_hidden', name: 'Hidden', baseMagnitude: 5, baseDuration: 0, baseCost: 1 }],
    },
  ];
  const impureMix = alchemy.computeAlchemyPreview(testIngredients, ['101', '102', '103'], { alchemySkill: 50, hasPurity: false, hasPhysician: true, hasBenefactor: true });
  assert.equal(impureMix.valid, true);
  assert.equal(impureMix.isPoison, false);
  assert.equal(impureMix.effects.length, 3, 'Without Purity, mismatched hostile effect stays attached to beneficial potion');
  assert.equal(impureMix.purifiedCount, 0);
  assert.equal(impureMix.maxCraftable, 3);
  const pureMix = alchemy.computeAlchemyPreview(testIngredients, ['101', '102', '103'], { alchemySkill: 50, hasPurity: true, hasPhysician: true, hasBenefactor: true });
  assert.equal(pureMix.valid, true);
  assert.equal(pureMix.isPoison, false);
  assert.equal(pureMix.purifiedCount, 1, 'Purity must cleanse mismatched hostile effect from potion');
  assert.equal(pureMix.effects.length, 2);
  const experimentalMix = alchemy.computeAlchemyPreview(testIngredients, ['103', '104'], { alchemySkill: 50 });
  assert.equal(experimentalMix.valid, false, 'Unknown effects must not leak into preview');
  assert.equal(experimentalMix.isExperimentalMix, true);
  assert.equal(experimentalMix.maxCraftable, 1, 'Experimental mix clamps craft batch count to 1');
  const recipes = alchemy.computeBrewableRecipes(testIngredients, { alchemySkill: 50, hasPurity: true });
  assert.ok(recipes.length >= 1 && recipes[0].effects.length >= 2, 'computeBrewableRecipes discovers carried combinations');
  assert.equal(alchemy.formatAlchemyEffectText({ name: 'Restore Health', descriptionTemplate: 'Restore <mag> pts over <dur>s.', baseMagnitude: 5, baseDuration: 10, powerAffectsMagnitude: true, powerAffectsDuration: false, noMagnitude: false, noDuration: false }, 24, 10), 'Restore 24 pts over 10s.');

  const alchSrc = read('sdk/ui/src/field/AlchemyPage.tsx');
  assert.ok(!/<circle\b/i.test(alchSrc), 'AlchemyPage must not use <circle> sphere icons');
  assert.ok(!alchSrc.includes('12 2.6 18.6 9.4 12 21.4 5.4 9.4') && !alchSrc.includes('M12 4.5l2 5 5 2.5') && !alchSrc.includes('M12 3l2.2 6.8L21 12'), 'AlchemyPage must not use diamond or 4-point AI sparkle SVG paths');
  const alchAltarIdx = alchSrc.indexOf('<aside className="alchemy-altar"');
  assert.ok(alchAltarIdx > 0 && !/type\s*=\s*"range"/.test(alchSrc.slice(0, alchAltarIdx)), 'AlchemyPage must keep quantity sliders out of item cards (Altar only)');
  assert.ok(alchSrc.includes('alchemy-list--grid alchemy-list--grid-4') && alchSrc.includes('AlchemyPhialShowcase'), 'AlchemyPage must use 4-column ingredient grid and Distillation Altar showcase');
  assert.ok(alchSrc.includes('alchemy-art-known') && alchSrc.includes('alchemy-art-count') && alchSrc.includes('alchemy-trait-grid') && alchSrc.includes('alchemy-traits-untested'), 'AlchemyPage must render large specimen art cards with compact trait grid and single untested bar');

  const enchSrc = read('sdk/ui/src/field/EnchantingPage.tsx');
  const sigilStart = enchSrc.indexOf('function EnchantmentSigil(');
  const sigilEnd = enchSrc.indexOf('function getCompactDisenchantStatus(');
  const sigilBody = sigilStart >= 0 && sigilEnd > sigilStart ? enchSrc.slice(sigilStart, sigilEnd) : enchSrc;
  assert.ok(!sigilBody.includes('12 2.6 18.6 9.4 12 21.4 5.4 9.4'), 'EnchantmentSigil must not use diamond shape');
  assert.ok(!sigilBody.includes('<circle'), 'EnchantmentSigil must not use plain sphere/circle icons');
  assert.ok(!enchSrc.includes('M12 4.5l2 5 5 2.5') && !enchSrc.includes('M12 3l2.2 6.8L21 12'), 'EnchantingPage must not use 4-point AI sparkle star paths');
  assert.ok(enchSrc.includes('name="pen"') && enchSrc.includes('data-tip={statusTip}') && enchSrc.includes('"Bound"') && enchSrc.includes('"Deactivated"'), 'EnchantingPage must use pen rename icon and Bound/Deactivated hover tooltips');
  assert.ok(!enchSrc.includes('enchanting-inline-slider'), 'EnchantingPage must keep quantity sliders out of item cards (Altar only)');
  assert.ok(enchSrc.includes('enchanting-list--grid enchanting-list--grid-4') && enchSrc.includes('enchanting-altar__stage') && enchSrc.includes('enchanting-altar__conduit'), 'EnchantingPage must use 4-column grid everywhere and redesigned Altar stage/conduit');
  assert.ok(read('sdk/ui/src/field/icons.tsx').includes('"pen"'), 'icons.tsx must define pen icon');

  const splashSrc = read('src/render/VideoSplash.cpp');
  assert.ok(splashSrc.includes('MF_MEDIA_ENGINE_EVENT_SEEKED') && splashSrc.includes('GetCurrentTime()'), 'VideoSplash.cpp must await seek completion when rewinding prewarmed splash');
  const d3dOverlaySrc = read('src/render/D3D11Overlay.cpp');
  assert.ok(d3dOverlaySrc.includes('!already_pumped && !FrameworkSplashActive()'), 'D3D11Overlay.cpp must skip 3D actor preview renders while splash is active');
  const ulSrc = read('src/ultralight/UltralightBackend.cpp');
  const refreshIdx = ulSrc.indexOf('renderer->RefreshDisplay(');
  const updateIdx = ulSrc.indexOf('renderer->Update();', refreshIdx > 0 ? refreshIdx : 0);
  const renderIdx = ulSrc.indexOf('renderer->Render();', updateIdx > 0 ? updateIdx : 0);
  assert.ok(ulSrc.includes('THREAD_PRIORITY_ABOVE_NORMAL') && refreshIdx > 0 && updateIdx > refreshIdx && renderIdx > updateIdx, 'UltralightBackend.cpp must prioritize worker thread and call RefreshDisplay -> Update -> Render');
  const stylesCss = read('sdk/ui/src/styles.css');
  const compassMarkerRule = stylesCss.match(/\.hud-sovereign-compass__marker\s*\{[\s\S]*?\}/);
  assert.ok(compassMarkerRule && !/will-change\s*:\s*transform/i.test(compassMarkerRule[0]), '.hud-sovereign-compass__marker must not allocate per-marker compositor layers via will-change:transform');
  const bookSrc = read('sdk/ui/src/field/BookReader.tsx');
  assert.ok(bookSrc.includes('activateKeyLabel') && bookSrc.includes('activateKeyCode'), 'BookReader must support dynamic Skyrim Interact key');
  assert.ok(bookSrc.includes('!standalone') && bookSrc.includes('inventory-book-reader__toolbar'), 'BookReader must hide toolbar buttons in standalone world mode');
  const papyrusSrc = read('src/skse/Papyrus.cpp');
  assert.ok(papyrusSrc.includes('RE::INPUT_DEVICE::kMouse') && papyrusSrc.includes('ignoreKeyboardMouse'), 'Papyrus.cpp must detect mouse vs keyboard vs gamepad Activate key');
  assert.ok(papyrusSrc.includes('ReadAlchemySnapshot') && papyrusSrc.includes('PerformAlchemyCraft') && papyrusSrc.includes('PerformEatIngredient') && papyrusSrc.includes('CaptureActiveAlchemyStation'), 'Papyrus.cpp must implement native Alchemy Lab endpoints');
  assert.ok(papyrusSrc.includes('"ingredientEffects"') && papyrusSrc.includes('"knownEffectCount"') && papyrusSrc.includes('"totalEffectCount"'), 'Papyrus.cpp must serialize ingredientEffects and learned counts in PlayerItemJson');
  assert.ok(papyrusSrc.includes('IsLiveInventoryForm') && papyrusSrc.includes('created_mgr->IncrementRef(potion_item)') && !papyrusSrc.includes('created_potion.reset(ret_poison)'), 'Papyrus.cpp must retain BGSCreatedObjectManager lifetime on brewed AlchemyItem forms and validate dynamic inventory forms');
  const pluginSrc = read('src/skse/Plugin.cpp');
  assert.ok(pluginSrc.includes('DirectInputScanCodeToVirtualKey'), 'Plugin.cpp must translate extended DirectInput scan codes for ArrowLeft/ArrowRight');
  assert.ok(pluginSrc.includes('skyultra.bookActivate'), 'Plugin.cpp must dispatch mouse Activate presses to standalone Book Reader');
  assert.ok(pluginSrc.includes('g_alchemy_replacement_active') && pluginSrc.includes('OpenSkyUltraAlchemyReplacement') && pluginSrc.includes('alchemyDisplayMode'), 'Plugin.cpp must wire skyultra_alchemy.swf replacement hooks');

  const settingsSrc = read('sdk/ui/src/field/SettingsPage.tsx');
  assert.ok(settingsSrc.includes('superEnchanting'), 'SettingsPage must expose Super Enchanting toggle');
  assert.ok(settingsSrc.includes('replaceSkyrimAlchemy') && settingsSrc.includes('alchemyDisplayMode') && settingsSrc.includes('alchemyPauseWorld'), 'SettingsPage must expose Alchemy Lab replacement and layout settings');
  assert.ok(settingsSrc.includes('replaceSkyrimBookReader'), 'SettingsPage must expose Skyrim Book Reader toggle');
  const appSrc = read('sdk/ui/src/field/App.tsx');
  assert.ok(appSrc.includes('standaloneBook') && appSrc.includes('skyultra.bookMode'), 'App must wire standalone world Book Reader session');
  assert.ok(appSrc.includes('standaloneAlchemy') && appSrc.includes('skyultra.alchemyMode') && appSrc.includes('AlchemyPage'), 'App must wire standalone Alchemy Lab session');
  assert.ok(appSrc.includes('playerEnchantingLevel'), 'App must pass playerEnchantingLevel to SettingsPage');

  console.log('Menu checks passed: Space routing, shared intent loads, pending preview navigation, Magic/Favorites click equip & optimistic state, Arcane Enchanter compatibility/Super Enchanting/scaling/CSS/manifest, Alchemy Lab preview/Purity/recipes/CSS/manifest, Book Reader wiring, Barter grid layout, Favorites item thumbnail CSS, shim success/failure/timeout/cancellation, SWF carriers.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
