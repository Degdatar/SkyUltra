// Exercise observatory hooks directly in memory. No bundles or binaries emitted.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const slots=[],effects=[],timers=new Map();
let cursor=0,timerId=0,dirty=false,tree;
const same=(a,b)=>a&&b&&a.length===b.length&&a.every((v,i)=>Object.is(v,b[i]));
const React={
 createElement(type,props,...children){if(props?.ref&&props.className?.startsWith('actor-portrait '))props.ref.current={};return {type,props:{...props,children}};},
 memo(render,compare){return {render,compare};},
 useMemo(fn,deps){const i=cursor++;if(!same(slots[i]?.deps,deps))slots[i]={deps,value:fn()};return slots[i].value;},
 useRef(value){return slots[cursor++]??={current:value};},
 useState(initial){const i=cursor++;slots[i]??={value:typeof initial==='function'?initial():initial};return [slots[i].value,next=>{const previous=slots[i].value;slots[i].value=typeof next==='function'?next(previous):next;dirty||=!Object.is(previous,slots[i].value);}];},
 useEffect(fn,deps){const i=cursor++;if(!same(slots[i]?.deps,deps))effects.push({i,fn,deps});},
};
const window={innerWidth:1920,innerHeight:1080,
 setTimeout(fn,delay){timers.set(++timerId,{fn,delay});return timerId;},clearTimeout(id){timers.delete(id);},
 addEventListener(){},removeEventListener(){},
};
const document={hidden:false};
const actors=[{referenceId:'1',baseFormId:'101',name:'Giant',race:'Giant',sex:'Male'},
 {referenceId:'2',baseFormId:'102',name:'Elk',race:'Deer',sex:'Female'}];
const requests=[];
let cached={available:true,dataUrl:'data:image/png;base64,portrait'};
const queue={peek:()=>cached,subscribe:()=>()=>{},request(key,signal){requests.push({key,signal});return new Promise(()=>{});}};
const imports={
 'react':React,'@skyultra/sdk':{createSkyUltraGame:()=>({inventory:async()=>({available:true,results:[],totalMatches:0})})},
 '../core.js':{SkySegmentedControl:'Segments'},'./ActorStage.js':{ModelStage:'ModelStage'},
 './EquipmentGlyph.js':{EquipmentGlyph:'Glyph'},'./EquipmentPanel.js':{equipmentSlots:[],fitsEquipmentSlot:()=>false},
 './icons.js':{Icon:'Icon'},'./itemDrag.js':{},'./ItemThumbnail.js':{ItemImage:'ItemImage',ItemThumbnail:'ItemThumbnail'},
 './itemThumbnails.js':{actorThumbnailQueue:()=>queue},'./model.js':{formId:String},'./pieces.js':{Empty:'Empty'},
 './useFieldData.js':{useActors:()=>({actors,count:actors.length,results:[],searchCount:0,status:'Loaded actors'})},
};
let observerCallback;
class IntersectionObserver{constructor(callback){observerCallback=callback;}observe(){}disconnect(){}}
const exportsObject={};
const js=ts.transpileModule(fs.readFileSync(path.join(root,'sdk/ui/src/field/ActorsPage.tsx'),'utf8'),{
 fileName:'ActorsPage.tsx',compilerOptions:{jsx:ts.JsxEmit.React,module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022},
}).outputText;
vm.runInNewContext(js,{exports:exportsObject,require(name){if(!(name in imports))throw Error(name);return imports[name];},window,document,AbortController,IntersectionObserver});
const bridge={getVisibility:()=>({visible:true})};
let Component=exportsObject.default,props={bridge,obscured:false};
function render(){dirty=false;cursor=0;tree=Component(props);const changes=effects.splice(0);for(const change of changes)slots[change.i]?.cleanup?.();for(const {i,fn,deps} of changes)slots[i]={deps,cleanup:fn()};}
async function settle(){for(let i=0;i<12;i++){await Promise.resolve();if(dirty)render();}}
async function flush(delay){const batch=[...timers].filter(([,timer])=>timer.delay===delay);for(const [id,{fn}]of batch){if(!timers.delete(id))continue;fn();}await settle();}
function findAll(node,predicate){if(!node||typeof node!=='object')return [];const result=predicate(node)?[node]:[];for(const child of node.props?.children??[])for(const value of Array.isArray(child)?child:[child])result.push(...findAll(value,predicate));return result;}
const portraits=()=>findAll(tree,node=>node.type?.render?.name==='ActorPortrait');
function dispose(){for(const slot of slots)slot?.cleanup?.();slots.length=0;effects.length=0;timers.clear();}
async function main(){
 render();await settle();await flush(0);
 assert.equal(portraits().length,2);assert.ok(portraits().every(node=>node.props.paused));
 await flush(180);assert.ok(portraits().every(node=>!node.props.paused));
 const portraitType=portraits()[0].type;
 const left=()=>findAll(tree,node=>node.props?.className==='actor-left-stack')[0];
 for(let i=0;i<30;i++)left().props.onScrollCapture();
 await settle();assert.ok(portraits().every(node=>node.props.paused));
 assert.equal([...timers.values()].filter(timer=>timer.delay===180).length,1,'Scroll bursts must coalesce into one thumbnail resume');
 await flush(180);assert.ok(portraits().every(node=>!node.props.paused));
 const rows=()=>findAll(tree,node=>node.props?.className?.startsWith('actor-row actor-row--'));
 rows()[1].props.onClick();await settle();
 assert.equal(findAll(tree,node=>node.type==='ModelStage')[0].props.subject.actor.referenceId,'2');
 assert.ok(portraits().every(node=>node.props.paused),'Actor switching gives the stage priority over thumbnails');
 await flush(180);assert.ok(portraits().every(node=>!node.props.paused));
 const previous={actor:actors[0],bridge,paused:false,refreshToken:0};
 assert.ok(portraitType.compare(previous,{...previous,actor:{...actors[0],distance:100}}));
 assert.equal(portraitType.compare(previous,{...previous,actor:{...actors[0],race:'Nord'}}),false);
 assert.equal(portraitType.compare(previous,{...previous,refreshToken:1}),false);
 dispose();Component=portraitType.render;props={actor:actors[0],bridge,paused:false};
 render();await settle();
 assert.equal(findAll(tree,node=>node.type==='img').length,1);
 assert.equal(findAll(tree,node=>node.type?.name==='ActorFaceSvg').length,0,'Cached portraits must not retain hidden SVG trees');
 assert.equal(requests.length,0);
 dispose();cached=undefined;render();await settle();
 observerCallback([{isIntersecting:true}]);await flush(75);
 assert.equal(requests.length,1);assert.equal(requests[0].signal.aborted,false);
 props={...props,paused:true};render();await settle();
 assert.equal(requests[0].signal.aborted,true,'Interaction must abort pending portrait work');
 props={...props,paused:false};render();await settle();observerCallback([{isIntersecting:true}]);await flush(75);
 assert.equal(requests.length,2);assert.equal(requests[1].signal.aborted,false);
 dispose();assert.equal(requests[1].signal.aborted,true);
 console.log('Observatory scroll coalescing, actor-switch priority, portrait memoization/cache and cancellation passed. No files emitted.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
