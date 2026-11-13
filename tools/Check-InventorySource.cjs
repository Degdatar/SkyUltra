// Interpret source in memory; no bundling, DLLs, game process or installation changes.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
let checks=0;
function moduleSource(file,imports,globals={}){
 const output=ts.transpileModule(fs.readFileSync(path.join(root,file),'utf8'),{fileName:file,compilerOptions:{jsx:ts.JsxEmit.React,module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022}}).outputText;
 const exports={};vm.runInNewContext(output,{exports,require(name){if(!(name in imports))throw Error(name);return imports[name];},...globals});return exports;
}
const sdk=moduleSource('sdk/typescript/src/game.ts',{'./index.js':{}});
const calls=[],bridge={request:async(...args)=>{calls.push(args);return {available:true};}};
const game=sdk.createSkyUltraGame(bridge),signal=new AbortController().signal;
async function contracts(){
 for(const id of [20,'20','0x020176F1',0xffffffff])for(const slot of sdk.skyUltraEquipmentSlots){
  await game.equipItem(id,slot,{signal});const call=calls.at(-1);assert.equal(call[0],'game.equipItem');assert.equal(call[1].formId,id);assert.equal(call[1].slot,slot);assert.equal(call[2].signal,signal);checks+=4;
 }
 for(const id of [0,-1,2.5,Infinity,NaN,0x100000000,'',' ', '0x','20abc','-2','1e2','4294967296']){
  assert.throws(()=>game.equipItem(id),/form ID/);assert.throws(()=>game.thumbnail(id),/form ID/);checks+=2;
 }
 assert.throws(()=>game.equipItem(1,'script'),/slot/);checks++;
 await game.unequipItem('20','offHand',{signal});assert.equal(calls.at(-1)[0],'game.unequipItem');checks++;
 for(const action of sdk.skyUltraItemActions){
  await game.itemAction({formId:'20',action,count:2},{signal});
  const call=calls.at(-1);assert.equal(call[0],'game.itemAction');assert.equal(call[1].formId,'20');assert.equal(call[1].action,action);assert.equal(call[1].count,2);checks+=4;
 }
 assert.throws(()=>game.itemAction({formId:'20',action:'unknown'}),/action/);checks++;
 assert.throws(()=>game.itemAction({formId:'20',action:'drop',count:0}),/count/);checks++;
 await game.barterSnapshot('0x0001A672',{signal});assert.equal(calls.at(-1)[0],'game.getBarterSnapshot');assert.equal(calls.at(-1)[1].vendorReferenceId,'0x0001A672');checks+=2;
 await game.barterTransaction({action:'buy',formId:'20',count:8,vendorReferenceId:'0x0001A672'},{signal});assert.equal(calls.at(-1)[0],'game.barterTransaction');assert.equal(calls.at(-1)[1].action,'buy');assert.equal(calls.at(-1)[1].count,8);checks+=3;
 assert.throws(()=>game.barterTransaction({action:'steal',formId:'20'}),/action/);checks++;
 assert.throws(()=>game.barterTransaction({action:'sell',formId:'20',count:0}),/count/);checks++;
 await game.thumbnail('20',{signal});assert.equal(calls.at(-1)[0],'game.getItemThumbnail');checks++;
 await game.inventory({query:'sword',equipment:'carried',offset:1088,limit:64,sort:'value',category:'weapon'},{signal});
 assert.equal(calls.at(-1)[1].offset,1088);assert.equal(calls.at(-1)[1].query,'sword');checks+=2;
 for(const options of [{offset:-1},{offset:1.1},{offset:Infinity},{limit:0},{limit:65},{sort:'script'},{equipment:'whatever'},{query:'x'.repeat(257)}]){assert.throws(()=>game.inventory(options),/options/);checks++;}
 for(const file of ['apps/skyultra-menu/public/manifest.json','samples/ui-kit-test-mod/public/manifest.json']){
  const manifest=JSON.parse(fs.readFileSync(path.join(root,file),'utf8'));
  for(const method of sdk.skyUltraInventoryMethods){assert.ok(manifest.permissions.pageRequests.includes(method));checks++;}
 }
}
async function queueChecks(){
 const {createItemThumbnailQueue}=moduleSource('sdk/ui/src/field/itemThumbnails.ts',{'@skyultra/sdk':{}});
 const loads=[],finish=[];const queue=createItemThumbnailQueue(id=>{loads.push(id);return new Promise(resolve=>finish.push(resolve));});
 const a=new AbortController(),b=new AbortController(),c=new AbortController();
 const first=queue.request('1',a.signal),duplicate=queue.request('1',b.signal),second=queue.request('2',b.signal),abandoned=queue.request('3',c.signal);c.abort();
 assert.deepEqual(loads,['1']);checks++;
 finish.shift()({available:true,dataUrl:'data:image/png;base64,first'});await first;await duplicate;
 assert.deepEqual(loads,['1','2']);checks++;
 finish.shift()({available:true,dataUrl:'data:image/png;base64,second'});await second;await abandoned;
 assert.deepEqual(loads,['1','2']);checks++;
 assert.equal((await queue.request('1',b.signal)).dataUrl,'data:image/png;base64,first');assert.equal(loads.length,2);checks+=2;
 const invalid=createItemThumbnailQueue(async()=>({available:true,dataUrl:'https://remote/image.png'}));
 assert.equal((await invalid.request('1',b.signal)).available,false);checks++;
 let count=0;const bounded=createItemThumbnailQueue(async id=>{count++;return {available:true,dataUrl:'data:image/png;base64,'+id};});
 for(let i=0;i<130;i++)await bounded.request(String(i),b.signal);
 await bounded.request('129',b.signal);assert.equal(count,130);checks++;
 await bounded.request('0',b.signal);assert.equal(count,131);checks++;
 let attempts=0;const retry=createItemThumbnailQueue(async()=>++attempts===1?{available:false,message:'Model captures are busy.'}:{available:true,dataUrl:'data:image/png;base64,ok'});
 await retry.request('1',b.signal);assert.equal((await retry.request('1',b.signal)).available,true);checks++;
 // Regression check: aborting waiter1 mid-flight must NOT poison cache for concurrent waiter2 (PlayerPortrait race)
 let actorLoads=0;
 const actorQueue=createItemThumbnailQueue(async(_id,sig)=>{
  actorLoads++;
  await Promise.resolve();
  if(sig.aborted)return {available:false};
  return {available:true,dataUrl:'data:image/png;base64,degdatar'};
 },{cacheFailures:false});
 const mountAbort=new AbortController(),liveSignal=new AbortController();
 const p1=actorQueue.request('0x00000014:0x00000007',mountAbort.signal);
 const p2=actorQueue.request('0x00000014:0x00000007',liveSignal.signal);
 mountAbort.abort();
 const [r1,r2]=await Promise.all([p1,p2]);
 assert.equal(r1.available,false);
 assert.equal(r2.available,true);
 assert.equal(r2.dataUrl,'data:image/png;base64,degdatar');
 assert.equal(actorQueue.peek('0x00000014:0x00000007')?.dataUrl,'data:image/png;base64,degdatar');
 assert.equal(actorLoads,2);
 checks+=5;
 // Verify { cacheFailures: false } never caches { available: false } even when not aborted
 let transientCalls=0;
 const noCacheFailQueue=createItemThumbnailQueue(async()=>++transientCalls===1?{available:false}:{available:true,dataUrl:'data:image/png;base64,recovered'},{cacheFailures:false});
 assert.equal((await noCacheFailQueue.request('player',liveSignal.signal)).available,false);
 assert.equal(noCacheFailQueue.peek('player'),undefined);
 assert.equal((await noCacheFailQueue.request('player',liveSignal.signal)).available,true);
 checks+=3;
}
async function pageChecks(){
 const slots=[],effects=[],timers=new Map();let cursor=0,dirty=false,tree,timerId=0,captured,target='';
 const same=(a,b)=>a&&b&&a.length===b.length&&a.every((value,index)=>Object.is(value,b[index]));
 const ghostElement={style:{transform:''}};
 const React={Fragment:'Fragment',createElement(type,props,...children){if(props?.className==='equipment-drag-ghost')props.ref.current=ghostElement;return {type,props:{...props,children}};},
  useMemo(fn,deps){const i=cursor++;if(!same(slots[i]?.deps,deps))slots[i]={deps,value:fn()};return slots[i].value;},
  useRef(value){return slots[cursor++]??={current:value};},
  useState(value){const i=cursor++;slots[i]??={value};return [slots[i].value,next=>{const previous=slots[i].value;slots[i].value=typeof next==='function'?next(previous):next;dirty||=!Object.is(previous,slots[i].value);}];},
  useEffect(fn,deps){const index=cursor++;if(!same(slots[index]?.deps,deps))effects.push({index,fn,deps});},
 };
 const window={innerWidth:1280,innerHeight:800,setTimeout(fn){timers.set(++timerId,fn);return timerId;},clearTimeout(id){timers.delete(id);},setInterval(){return -1;},clearInterval(){}};
 const document={hidden:false,elementFromPoint:()=>target?{closest:()=>({dataset:{equipSlot:target}})}:null,addEventListener(){},removeEventListener(){}};
 const item={id:'123',name:'Test Sword',category:'weapon',equipSlots:['mainHand','offHand'],count:1,value:10,weight:4,equipped:false};
 const helmet={...item,id:'124',name:'Test Hood',category:'armor',equipSlots:['head'],enchanted:true,enchantmentName:'Peerless Magicka',enchantmentCharge:80,enchantments:[{name:'Fortify Magicka',description:'Increases your Magicka by 60 points.',magnitude:60,duration:0}]};
 let snapshot={available:true,playerName:'Player',level:10,gold:50,armorRating:10,equipment:{head:null,body:null,hands:null,feet:null,mainHand:null,offHand:null},inventory:[item,helmet],inventoryTypeCount:1163,inventoryTruncated:true};
 const native=[];
 const api={on:()=>()=>{},getVisibility:()=>({visible:true}),async request(method,params){native.push({method,params});
  if(method==='game.getInventory')return {available:true,results:params.offset?Array.from({length:64},(_,i)=>({...item,id:String(i+500),name:'Page 2 item '+i})): [item,helmet],totalTypes:1163,totalMatches:1163,categories:['armor','weapon'],offset:params.offset};
  if(method==='game.equipItem'){snapshot={...snapshot,equipment:{...snapshot.equipment,[params.slot]:item}};return {accepted:true,slot:params.slot};}
  if(method==='game.unequipItem'){snapshot={...snapshot,equipment:{...snapshot.equipment,[params.slot]:null}};return {accepted:true,slot:params.slot};}
  if(method==='game.getPlayerSnapshot')return snapshot;
  throw Error(method);
 }};
 const panel=moduleSource('sdk/ui/src/field/EquipmentPanel.tsx',{'react':React,'@skyultra/sdk':{},'./ActorStage.js':{ModelStage:'ModelStage'},'./itemThumbnails.js':{actorThumbnailQueue:()=>({set(){}}),PLAYER_PORTRAIT_QUEUE_KEY:'0x00000014:0x00000007'},'./ItemThumbnail.js':{ItemThumbnail:'Thumbnail'},'./EquipmentGlyph.js':{EquipmentGlyph:'EquipmentGlyph'},'./icons.js':{Icon:'Icon'},'./model.js':{formId:String}});
 const dragPosition=moduleSource('sdk/ui/src/field/itemDrag.ts',{});
 const page=moduleSource('sdk/ui/src/field/InventoryPage.tsx',{'react':React,'@skyultra/sdk':sdk,'./ItemStage.js':{ItemStage:'ItemStage'},'./ItemThumbnail.js':{ItemThumbnail:'Thumbnail',ItemImage:'ItemImage'},'./itemDrag.js':dragPosition,'./EquipmentPanel.js':{...panel,EquipmentPanel:'EquipmentPanel'},'../core.js':{SkyChoice:'Choice',SkyDataList:'DataList',SkySegmentedControl:'Segments'},'./icons.js':{Icon:'Icon',itemIcon:()=> 'inventory'},'./pieces.js':{Empty:'Empty'},'./model.js':{formId:String}},{window,document,AbortController});
 const props={player:snapshot,bridge:api,obscured:false};
 function render(){dirty=false;cursor=0;tree=page.default(props);const changes=effects.splice(0);for(const change of changes)slots[change.index]?.cleanup?.();for(const {index,fn,deps} of changes)slots[index]={deps,cleanup:fn()};}
 async function settle(){for(let i=0;i<20;i++){await Promise.resolve();if(dirty)render();}}
 async function flush(){for(let i=0;i<20&&timers.size;i++){const batch=[...timers.values()];timers.clear();batch.forEach(fn=>fn());await settle();}}
 function findAll(node,predicate,out=[]){if(Array.isArray(node)){node.forEach(child=>findAll(child,predicate,out));return out;}if(!node||typeof node!=='object')return out;if(predicate(node))out.push(node);findAll(node.props?.children,predicate,out);return out;}
 const find=predicate=>findAll(tree,predicate)[0];const tile=()=>find(node=>node.type==='button'&&node.props.className?.includes('item-tile'));
 const thumbnailSrc='data:image/png;base64,existing-item-thumbnail';
 const pointer={setPointerCapture(id){captured=id;},hasPointerCapture:id=>id===captured,releasePointerCapture(){captured=undefined;},querySelector:()=>({getAttribute:()=>thumbnailSrc})};
 const event=(x=0,y=0)=>({pointerId:1,button:0,clientX:x,clientY:y,currentTarget:pointer,preventDefault(){}});
 render();await settle();await flush();
 assert.ok(find(node=>node.type==='EquipmentPanel'));checks++;
 const workspace=find(node=>node.props.className==='inventory-outfit-workbench');
 const columns=workspace.props.children.flat().filter(value=>value&&typeof value==='object');
 assert.equal(columns[0].props.className,'surface inventory-surface');assert.equal(columns[1].type,'EquipmentPanel');checks+=2;
 assert.ok(panel.equipmentSlots.some(slot=>slot.id==='cape'&&slot.biped==='46'));assert.ok(panel.equipmentSlots.some(slot=>slot.id==='lantern'&&slot.biped==='49'));checks+=2;
 assert.equal(panel.fitsEquipmentSlot({...item,equipSlots:['cape']},'cape'),true);assert.equal(panel.fitsEquipmentSlot({...item,equipSlots:['lantern']},'lantern'),true);assert.equal(panel.fitsEquipmentSlot({...item,equipSlots:['cape']},'lantern'),false);checks+=3;
 assert.equal(native.find(call=>call.method==='game.getInventory').params.limit,64);checks++;
 const segment=find(node=>node.type==='Segments'&&node.props.label==='Inventory workspace');
 segment.props.onValueChange('studio');render();await settle();assert.ok(find(node=>node.type==='ItemStage'));assert.ok(!find(node=>node.type==='EquipmentPanel'));checks+=2;
 find(node=>node.type==='Segments'&&node.props.label==='Inventory workspace').props.onValueChange('inventory');render();await settle();
 tile().props.onPointerDown(event());tile().props.onPointerUp(event());await settle();assert.ok(!native.some(call=>call.method==='game.equipItem'));checks++;
 tile().props.onPointerDown(event());target='head';tile().props.onPointerMove(event(10,10));render();
 const ghost=find(node=>node.props.className==='equipment-drag-ghost');assert.ok(ghost);checks++;
 const artwork=findAll(ghost,node=>node.type==='ItemImage')[0];assert.equal(artwork.props.src,thumbnailSrc);assert.equal(artwork.props.item.name,item.name);checks+=2;
 assert.equal(findAll(ghost,node=>node.type==='strong')[0].props.children[0],item.name);checks++;
 const requestsBeforeMoving=native.length,oldPosition=ghostElement.style.transform;
 tile().props.onPointerMove(event(110,100));assert.equal(dirty,false);assert.notEqual(ghostElement.style.transform,oldPosition);assert.equal(native.length,requestsBeforeMoving);checks+=3;
 tile().props.onPointerUp(event(110,100));await settle();assert.ok(!native.some(call=>call.method==='game.equipItem'));checks++;
 for(const [x,y] of [[0,0],[1279,0],[0,799],[1279,799]]){const point=dragPosition.itemDragPosition(x,y,1280,800);assert.ok(point.x>=10&&point.x+248<=1270);assert.ok(point.y>=10&&point.y+78<=790);checks+=2;}
 tile().props.onPointerDown(event());target='offHand';tile().props.onPointerMove(event(10,10));render();tile().props.onPointerUp(event(10,10));await settle();await flush();
 assert.equal(native.filter(call=>call.method==='game.equipItem').length,1);assert.equal(native.find(call=>call.method==='game.equipItem').params.slot,'offHand');checks+=2;
 const outfit=find(node=>node.type==='EquipmentPanel');outfit.props.onChange(item,'offHand',true);await settle();await flush();
 assert.equal(native.filter(call=>call.method==='game.unequipItem').length,1);checks++;
 tile().props.onPointerDown(event());target='mainHand';tile().props.onPointerMove(event(10,10));render();tile().props.onPointerCancel(event(10,10));await settle();assert.equal(native.filter(call=>call.method==='game.equipItem').length,1);checks++;
 find(node=>node.type==='button'&&node.props.children?.includes('Next')).props.onClick();render();await settle();await flush();
 assert.equal(findAll(tree,node=>node.type==='button'&&node.props.className?.includes('item-tile')).length,64);checks++;
 assert.equal(native.filter(call=>call.method==='game.getInventory').at(-1).params.offset,2);checks++;
 // The category rail and selected-item shortcut use the same inventory state.
 const rail=find(node=>node.props['aria-label']==='Inventory categories');
 const armor=rail.props.children.flat(Infinity).find(node=>node?.type==='button'&&node.props.title==='Armor');
 armor.props.onClick();render();await settle();await flush();
 assert.equal(native.filter(call=>call.method==='game.getInventory').at(-1).params.category,'armor');checks++;
 const search=find(node=>node.type==='input'&&node.props['aria-label']==='Search inventory');
 search.props.onChange({currentTarget:{value:'hood'}});render();await settle();await flush();
 assert.equal(native.filter(call=>call.method==='game.getInventory').at(-1).params.query,'hood');checks++;
 find(node=>node.type==='button'&&node.props['aria-label']==='Clear inventory search').props.onClick();render();await settle();await flush();
 assert.equal(native.filter(call=>call.method==='game.getInventory').at(-1).params.query,'');checks++;
 const summary=find(node=>node.type===panel.EquipmentSelection);assert.ok(summary);checks++;
 summary.props.onInspect();render();await settle();assert.ok(find(node=>node.type==='ItemStage'));checks++;
 // Verify standalone Player Inventory replacement mode (inventory + 3D player renderer without Item studio).
 let closedStandalone=false,toggledDisplay=false,toggledPause=false;
 props.standalone=true;props.displayMode='windowed';props.pauseWorld=true;
 props.onToggleDisplayMode=()=>{toggledDisplay=true;};props.onTogglePauseWorld=()=>{toggledPause=true;};
 props.onClose=()=>{closedStandalone=true;};
 render();await settle();
 assert.ok(find(node=>node.type==='EquipmentPanel'));
 assert.ok(!find(node=>node.type==='ItemStage'));
 assert.ok(!find(node=>node.type==='Segments'&&node.props.label==='Inventory workspace'));
 assert.equal(find(node=>node.type===panel.EquipmentSelection).props.onInspect,undefined);
 const modeButtons=findAll(tree,node=>node.type==='button'&&node.props.className?.includes('armory-mode-btn'));
 assert.equal(modeButtons.length,2);
 modeButtons[0].props.onClick();modeButtons[1].props.onClick();
 assert.equal(toggledDisplay,true);assert.equal(toggledPause,true);
 const closeBtn=find(node=>node.type==='button'&&node.props['aria-label']==='Close inventory');
 assert.ok(closeBtn);closeBtn.props.onClick();assert.equal(closedStandalone,true);
 // Verify enchanted item tile badge and selected item card enchantment effects + action buttons.
 const selTree=panel.EquipmentSelection({player:snapshot,selected:helmet,enabled:true,bridge:api,obscured:false,onChange(){},onItemAction(){}});
 assert.ok(selTree.props.className.includes('is-enchanted'));
 assert.ok(findAll(selTree,node=>node.props?.className==='selection-art__enchant-badge').length===1);
 assert.ok(findAll(selTree,node=>node.props?.className==='selection-enchantments').length===1);
 assert.equal(panel.resolveItemUseAction({id:'1',name:'Restore Health',category:'potion',count:1,value:10,weight:0.5,equipped:false}),'drink');
 assert.equal(panel.resolveItemUseAction({id:'2',name:'Sweetroll',category:'potion',count:1,value:2,weight:0.1,equipped:false,useAction:'eat'}),'eat');
 assert.equal(panel.resolveItemUseAction({id:'3',name:'Nirnroot',category:'ingredient',count:1,value:10,weight:0.2,equipped:false}),'eat');
 assert.equal(panel.resolveItemUseAction({id:'4',name:'Spell Tome: Flames',category:'book',count:1,value:50,weight:1,equipped:false}),'learn');
 assert.equal(panel.resolveItemUseAction({id:'5',name:'Breezehome Guide',category:'book',count:1,value:5,weight:1,equipped:false}),'read');
 assert.equal(panel.resolveItemUseAction({id:'6',name:'Storage Gem',category:'miscellaneous',count:1,value:100,weight:0.5,equipped:false}),'activate');
 checks+=19;

 // Verify BarterPage has no EquipmentPanel, renders bottom Vendor/Player switcher tabs, and shows quantity slider when count > 5.
 const barterPage=moduleSource('sdk/ui/src/field/BarterPage.tsx',{'react':React,'@skyultra/sdk':sdk,'./ItemThumbnail.js':{ItemThumbnail:'Thumbnail'},'../core.js':{SkyChoice:'Choice',SkySegmentedControl:'Segments'},'./icons.js':{Icon:'Icon',itemIcon:()=> 'inventory'},'./pieces.js':{Empty:'Empty'},'./model.js':{formId:String}},{window,document,AbortController});
 const barterStackItem={id:'777',name:'Iron Arrow',category:'ammo',count:25,value:3,weight:0.1,equipped:false};
 const barterTree=barterPage.default({player:{...snapshot,inventory:[barterStackItem]},bridge:api,obscured:false,displayMode:'half',pauseWorld:false});
 assert.ok(!findAll(barterTree,node=>node.type==='EquipmentPanel').length);
 assert.equal(findAll(barterTree,node=>node.type==='button'&&node.props?.role==='tab').length,2);
 checks+=2;

 for(const slot of slots)slot?.cleanup?.();
}
(async()=>{await contracts();await queueChecks();await pageChecks();console.log('Inventory API, thumbnail queue, paging, drag/drop and unequip: '+checks+' checks passed.');})().catch(error=>{console.error(error);process.exitCode=1;});
