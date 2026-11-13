// Exercise the real stage's hooks and bridge calls in memory. No bundling,
// native compilation, graphics-driver calls or installation files are involved.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const slots=[],effects=[],timers=new Map(),intervals=new Map(),listeners=new Map(),calls=[];
const resizeObservers=new Set();
let cursor=0,dirty=false,tree,timerId=0,subscriptions=0,unsubscribes=0,observerDisconnects=0;
const same=(a,b)=>a&&b&&a.length===b.length&&a.every((value,index)=>Object.is(value,b[index]));
let capturedPointer;
let viewportRect={left:600,top:150,right:900,bottom:450,width:300,height:300};
const viewport={getBoundingClientRect:()=>viewportRect,addEventListener(){},removeEventListener(){},focus(){},setPointerCapture(id){capturedPointer=id;},hasPointerCapture(id){return capturedPointer===id;},releasePointerCapture(){capturedPointer=undefined;}};
const React={
 createElement(type,props,...children){if(props?.ref&&props.className?.startsWith('actor-viewport'))props.ref.current=viewport;return {type,props:{...props,children}};},
 Fragment:'Fragment',
 useMemo(fn,deps){const index=cursor++;if(!same(slots[index]?.deps,deps))slots[index]={deps,value:fn()};return slots[index].value;},
 useRef(value){const index=cursor++;return slots[index]??=( {current:value} );},
 useState(value){const index=cursor++;slots[index]??={value};return [slots[index].value,next=>{const previous=slots[index].value;slots[index].value=typeof next==='function'?next(previous):next;dirty||=!Object.is(previous,slots[index].value);}];},
 useId(){cursor++;return 'stage-hint';},
 useEffect(fn,deps){const index=cursor++;if(!same(slots[index]?.deps,deps))effects.push({index,fn,deps});},
};
const window={innerWidth:1200,innerHeight:800,devicePixelRatio:1,
 setTimeout(fn){timers.set(++timerId,fn);return timerId;},clearTimeout(id){timers.delete(id);},setInterval(fn){intervals.set(++timerId,fn);return timerId;},clearInterval(id){intervals.delete(id);},
 addEventListener(name,fn){if(!listeners.has(name))listeners.set(name,new Set());listeners.get(name).add(fn);},
 removeEventListener(name,fn){listeners.get(name)?.delete(fn);},
};
const document={hidden:false,querySelector:()=>null,addEventListener(){},removeEventListener(){}};
const bridge={on(){subscriptions++;return ()=>unsubscribes++;}};
const response={available:true,ready:true,geometryCount:2};
let pending,actorReply;
const renderer={
 setActor(options,request){calls.push({method:'setActor',options,request});return Promise.resolve(actorReply??response);},
 setItem(options,request){calls.push({method:'setItem',options,request});return pending??Promise.resolve(response);},
 hide(){calls.push({method:'hide'});return Promise.resolve(response);},
 close(){calls.push({method:'close'});return Promise.resolve(response);},
 status(){return Promise.resolve(response);},save(){calls.push({method:'save'});return Promise.resolve({saved:true,path:'item.png'});},
 captureAnimations(){calls.push({method:'capture'});return Promise.resolve({available:true,clips:[{id:'giant-walk',name:'WalkForwardLeft',duration:2.666,sampleCount:33}]});},
};
const Choice='Choice';
const cameraModule={};vm.runInNewContext(ts.transpileModule(fs.readFileSync(path.join(root,'sdk/ui/src/field/actorCamera.ts'),'utf8'),{
 compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022},
}).outputText,{exports:cameraModule});
const imports={
 'react':React,'@skyultra/sdk':{createSkyUltraRenderer:()=>renderer,createSkyUltraItemRenderer:()=>renderer},
 '../core.js':{SkyChoice:Choice,SkySegmentedControl:'Segments'},'./ActorAnimationTimeline.js':{ActorAnimationTimeline:'Timeline'},
 './actorCamera.js':cameraModule,'./icons.js':{Icon:'Icon'},
};
const source=ts.transpileModule(fs.readFileSync(path.join(root,'sdk/ui/src/field/ActorStage.tsx'),'utf8'),{
 fileName:'ActorStage.tsx',compilerOptions:{jsx:ts.JsxEmit.React,module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022},
}).outputText;
const exportsObject={};vm.runInNewContext(source,{exports:exportsObject,require(name){if(!(name in imports))throw Error(name);return imports[name];},window,document,AbortController,
 ResizeObserver:class{constructor(fn){this.fn=fn;}observe(){resizeObservers.add(this.fn);}disconnect(){resizeObservers.delete(this.fn);observerDisconnects++;}},
});
const props={subject:{kind:'item',item:{id:'0x020176F1',name:'Test item'}},bridge,obscured:false};
function render(){dirty=false;cursor=0;tree=exportsObject.ModelStage(props);const changes=effects.splice(0);for(const change of changes)slots[change.index]?.cleanup?.();for(const {index,fn,deps} of changes)slots[index]={deps,cleanup:fn()};}
async function settle(){for(let i=0;i<8;i++){await Promise.resolve();if(dirty)render();}}
async function flush(){for(let i=0;i<8&&timers.size;i++){const batch=[...timers.values()];timers.clear();for(const fn of batch)fn();await settle();}}
function find(node,predicate){if(!node||typeof node!=='object')return;if(predicate(node))return node;for(const child of node.props?.children??[]){const found=Array.isArray(child)?child.map(value=>find(value,predicate)).find(Boolean):find(child,predicate);if(found)return found;}}
const choice=node=>find(node,value=>value.type===Choice&&value.props.label==='Preview lighting');
function scope(depth){for(const fn of listeners.get('skyultra:input-scope')??[])fn({detail:{depth}});}
async function choose(light){choice(tree).props.onValueChange(light);render();await settle();}
const count=method=>calls.filter(call=>call.method===method).length;
async function main(){
 render();await settle();await flush();
 assert.equal(count('setItem'),1);assert.equal(calls.find(call=>call.method==='setItem').options.lighting,'studio');
 // Polling unchanged native status must not repaint the stage every 1.2 s.
 for(let i=0;i<5;i++)for(const tick of intervals.values())await tick();
 assert.equal(dirty,false,'Identical preview status must retain the previous React state');
 assert.equal(count('setItem'),1);
 const hides=count('hide');
 for(const lighting of ['warm','cool','soft','dramatic','studio']){
  scope(1);await flush();const hidden=count('hide');
  await choose(lighting);assert.equal(count('hide'),hidden,'Changing light must not restart/hide the controller');
  scope(0);await flush();
  const latest=calls.filter(call=>call.method==='setItem').at(-1);
  assert.equal(latest.options.lighting,lighting);assert.equal(latest.options.formId,'0x020176F1');
  assert.equal(latest.options.refresh,false);assert.equal(latest.options.yaw,.5);assert.equal(latest.options.pitch,.35);
  assert.equal(latest.request.signal.aborted,false);assert.equal(subscriptions,1);assert.equal(unsubscribes,0);assert.equal(observerDisconnects,0);assert.equal(count('close'),0);
 }
 assert.equal(count('hide'),hides+5,'Only the five real popup openings hide the preview');
 // A second choice while a request is pending must queue the newest light,
 // rather than aborting and replacing the active preview controller.
 let release;pending=new Promise(resolve=>{release=resolve;});
 await choose('warm');await flush();await choose('cool');await flush();
 pending=undefined;release(response);await settle();await flush();
 assert.equal(calls.filter(call=>call.method==='setItem').at(-1).options.lighting,'cool');assert.equal(subscriptions,1);assert.equal(unsubscribes,0);
 // Exercise the actual pointer handlers and camera math, including coalescing
 // several movement events into one native request with no model recapture.
 find(tree,node=>node.type==='Segments'&&node.props.label==='Model drag action').props.onValueChange('pan');render();await settle();
 const pointer=(x,y)=>({button:0,pointerId:7,clientX:x,clientY:y,shiftKey:false,currentTarget:viewport,preventDefault(){}});
 const stageNode=()=>find(tree,node=>node.props?.className?.startsWith('actor-viewport'));
 stageNode().props.onPointerDown(pointer(100,100));render();await settle();
 const beforeMove=count('setItem');
 for(let step=1;step<=6;step++)stageNode().props.onPointerMove(pointer(100+step*10,100-step*5));
 assert.equal(count('setItem'),beforeMove,'Pointer motion should be throttled');
 await flush();assert.equal(count('setItem'),beforeMove+1);
 const moved=calls.filter(call=>call.method==='setItem').at(-1).options;
 assert.ok(Math.abs(moved.panX-.2)<1e-8);assert.ok(Math.abs(moved.panY+.1)<1e-8);
 assert.equal(moved.refresh,false);assert.equal(moved.lighting,'cool');assert.equal(moved.yaw,.5);assert.equal(moved.pitch,.35);
 stageNode().props.onPointerUp(pointer(160,70));render();await settle();
 assert.equal(capturedPointer,undefined);assert.equal(subscriptions,1);assert.equal(unsubscribes,0);
 find(tree,node=>node.type==='Segments'&&node.props.label==='Model drag action').props.onValueChange('rotate');render();await settle();
 stageNode().props.onPointerDown(pointer(100,100));render();await settle();
 const beforeRotate=count('setItem');
 for(let step=1;step<=4;step++)stageNode().props.onPointerMove(pointer(100+step*25,100+step*20));
 await flush();assert.equal(count('setItem'),beforeRotate+1);
 const rotated=calls.filter(call=>call.method==='setItem').at(-1).options;
 assert.ok(Math.abs(rotated.yaw-1.3)<1e-8);assert.ok(Math.abs(rotated.pitch-.99)<1e-8);
 assert.ok(Math.abs(rotated.panX-.2)<1e-8);assert.ok(Math.abs(rotated.panY+.1)<1e-8);
 assert.equal(rotated.refresh,false);assert.equal(rotated.lighting,'cool');
 stageNode().props.onPointerUp(pointer(200,180));render();await settle();
 assert.equal(capturedPointer,undefined);assert.equal(subscriptions,1);assert.equal(unsubscribes,0);
 // Scrolling sibling inventory/slot lists must neither hide nor re-submit the stage.
 const beforeScroll={set:count('setItem'),hide:count('hide')};
 for(let i=0;i<30;i++)for(const listener of listeners.get('scroll')??[])listener({target:{contains:()=>false}});
 await flush();assert.equal(count('setItem'),beforeScroll.set);assert.equal(count('hide'),beforeScroll.hide);
 // A real ancestor scroll updates bounds without blanking a fully visible stage.
 viewportRect={...viewportRect,top:160,bottom:460};
 for(const listener of listeners.get('scroll')??[])listener({target:{contains:()=>true}});
 await flush();assert.equal(count('setItem'),beforeScroll.set+1);assert.equal(count('hide'),beforeScroll.hide);
 const savedRect=viewportRect;viewportRect={...savedRect,top:-5,bottom:295};
 for(const listener of listeners.get('scroll')??[])listener({target:document});
 await flush();assert.equal(count('hide'),beforeScroll.hide+1);
 viewportRect=savedRect;
 for(const listener of listeners.get('scroll')??[])listener({target:document});
 await flush();assert.equal(count('setItem'),beforeScroll.set+2);
 for(const slot of slots)slot?.cleanup?.();
 assert.equal(count('close'),1);assert.equal(unsubscribes,1);assert.equal(observerDisconnects,1);
 slots.length=0;effects.length=0;props.subject={kind:'actor',actor:{referenceId:'20',baseFormId:'7',name:'Player'}};props.equipment=true;
 render();await settle();await flush();
 const outfit=calls.filter(call=>call.method==='setActor').at(-1);assert.equal(outfit.options.referenceId,'20');assert.equal(outfit.options.ignoreWorldFade,true);
 // The redesigned dock still flushes the isolated camera before PNG export.
 const exportButton=find(tree,node=>node.props?.className?.includes('outfit-save'));
 assert.equal(exportButton.props.disabled,false,'Outfit must be ready before testing export');
 exportButton.props.onClick();await settle();await flush();
 assert.equal(count('save'),1);assert.equal(calls.filter(call=>call.method==='setActor').at(-1).options.ignoreWorldFade,true);

 // Capture a creature clip, then scrub quickly while native rendering still
 // reports an in-flight frame. This used to toggle the loading note's height.
 props.equipment=false;render();await settle();await flush();
 actorReply={...response,ready:false,message:'Animation frame pending.'};
 const captureButton=find(tree,node=>node.type==='button'&&node.props.children.flat().includes('Capture animations'));
 assert.ok(captureButton);captureButton.props.onClick();await settle();await flush();
 assert.equal(count('capture'),1);
 assert.equal(calls.filter(call=>call.method==='setActor').at(-1).options.animation.id,'giant-walk');
 assert.equal(calls.filter(call=>call.method==='setActor').at(-1).options.live,false);
 const loadingNote=()=>find(tree,node=>node.type==='p'&&node.props.className==='studio-note'&&node.props.children.includes('Animation frame pending.'));
 assert.equal(loadingNote(),undefined,'Pending animation frames must not reinsert a layout-changing loading note');
 const afterCapture=count('setActor'),afterCaptureSubscriptions=subscriptions;
 const timeline=find(tree,node=>node.type==='Timeline');
 for(let frame=1;frame<=22;frame++)timeline.props.onFrameChange(frame);
 await settle();await flush();
 assert.equal(count('setActor'),afterCapture+1,'A burst of frame inputs must send only the latest frame');
 assert.equal(calls.filter(call=>call.method==='setActor').at(-1).options.animation.frame,22);
 assert.equal(subscriptions,afterCaptureSubscriptions,'Frame changes must keep the active native controller');
 assert.equal(loadingNote(),undefined);
 const settledCalls=count('setActor');
 for(let i=0;i<30;i++)for(const resize of resizeObservers)resize();
 await settle();await flush();
 assert.equal(count('setActor'),settledCalls,'Unchanged ResizeObserver bounds must not resubmit animation frames');
 actorReply=undefined;

 for(const slot of slots)slot?.cleanup?.();
 console.log('Animation capture/scrubbing, stable loading layout, resize deduplication, scrolling, lighting and redraw lifecycle passed. No files emitted.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
