// Source-only React rendering checks. TypeScript is interpreted in memory;
// no files, bundles, native binaries or installation artifacts are emitted.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const React=require(path.join(root,'sdk/ui/node_modules/react'));
const {renderToStaticMarkup}=require(path.join(root,'sdk/ui/node_modules/react-dom/server.js'));
function moduleSource(file,imports){
 const js=ts.transpileModule(fs.readFileSync(path.join(root,file),'utf8'),{
  fileName:file,compilerOptions:{jsx:ts.JsxEmit.React,module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022},
 }).outputText;
 const exports={};vm.runInNewContext(js,{exports,require(name){
  if(name==='react')return React;
  if(Object.hasOwn(imports,name))return imports[name];
  throw Error('Unexpected dependency: '+name);
 }});return exports;
}
const controls=moduleSource('sdk/ui/src/controls.tsx',{'./scales.js':{}});
const {ActorAnimationTimeline}=moduleSource('sdk/ui/src/field/ActorAnimationTimeline.tsx',{'../core.js':controls});
const {defaultActorCamera}=moduleSource('sdk/ui/src/field/actorCamera.ts',{});
const {ActorStage,ModelStage}=moduleSource('sdk/ui/src/field/ActorStage.tsx',{
 '@skyultra/sdk':{createSkyUltraRenderer:()=>({}),createSkyUltraItemRenderer:()=>({})},
 '../core.js':{SkySegmentedControl:()=>null,SkyChoice:()=>null},
 './ActorAnimationTimeline.js':{ActorAnimationTimeline},
 './actorCamera.js':{defaultActorCamera},'./icons.js':{Icon:()=>null},
});
let checks=0;
function render(Component,props){return renderToStaticMarkup(React.createElement(Component,props));}
function range(html){const match=html.match(/<input\b[^>]*type="range"[^>]*>/);assert.ok(match,'Frame slider must be present');checks++;return match[0];}
const stage=render(ActorStage,{actor:{name:'Danica',referenceId:'0x1A69F',baseFormId:'0x13BA5'},bridge:{},obscured:false});
assert.match(range(stage),/\bdisabled=""/);assert.match(stage,/Load poses or capture animations/);assert.match(stage,/>Load poses</);assert.doesNotMatch(stage,/Idle arms/);checks+=4;
const clip={id:'animation-1-0',name:'mt_idle',duration:2,sampleCount:33};
const active=render(ActorAnimationTimeline,{clip,frame:16,busy:false,onFrameChange(){}});
const enabled=range(active);assert.doesNotMatch(enabled,/\bdisabled/);assert.match(enabled,/\bmax="32"/);assert.match(enabled,/\bvalue="16"/);assert.match(active,/1\.00 s/);checks+=4;
assert.match(range(render(ActorAnimationTimeline,{clip,frame:0,busy:true,onFrameChange(){}})),/\bdisabled=""/);checks++;
const empty=range(render(ActorAnimationTimeline,{frame:16,busy:false,onFrameChange(){}}));
assert.match(empty,/\bdisabled=""/);assert.match(empty,/\bvalue="0"/);checks+=2;
const shorter=render(ActorAnimationTimeline,{clip:{...clip,sampleCount:17},frame:8,busy:false,onFrameChange(){}});
assert.match(range(shorter),/\bmax="16"/);assert.match(shorter,/1\.00 s/);checks+=2;
const library=render(ActorAnimationTimeline,{clip:{...clip,preset:'idle',name:'Idle'},frame:16,busy:false,onFrameChange(){}});
assert.doesNotMatch(range(library),/\bdisabled/);assert.match(library,/1\.00 s/);checks+=2;
const item=render(ModelStage,{subject:{kind:'item',item:{id:'0x12EB7',name:'Iron Sword'}},bridge:{},obscured:false});
assert.match(item,/ITEM STUDIO/);assert.match(item,/ITEM MODEL/);assert.match(item,/Model of Iron Sword/);
assert.match(item,/>Save</);assert.match(item,/>Reset view</);checks+=5;
assert.doesNotMatch(item,/type="range"|Load poses|Capture animations|Live motion|Portrait/);checks++;
const outfit=render(ModelStage,{subject:{kind:'actor',actor:{referenceId:'20',baseFormId:'7',name:'Player'}},bridge:{},obscured:false,equipment:true,refreshToken:1});
assert.match(outfit,/CURRENT OUTFIT/);assert.match(outfit,/Model of Player/);assert.match(outfit,/>Save</);checks+=3;
assert.doesNotMatch(outfit,/type="range"|Load poses|Capture animations|Actor preview mode/);checks++;
const {EquipmentGlyph}=moduleSource('sdk/ui/src/field/EquipmentGlyph.tsx',{});
const {EquipmentPanel,EquipmentSelection}=moduleSource('sdk/ui/src/field/EquipmentPanel.tsx',{
 '@skyultra/sdk':{},'./ActorStage.js':{ModelStage},'./EquipmentGlyph.js':{EquipmentGlyph},
 './itemThumbnails.js':{},'./spellArt.js':{HeartIcon:()=>null},
 './ItemThumbnail.js':{ItemThumbnail:({item})=>React.createElement('span',{'data-item-thumbnail':item.id})},'./icons.js':{Icon:()=>null},'./model.js':{},
});
const cape={id:'460',name:'Traveler Cape',category:'armor',count:1,value:50,weight:1,equipped:true,equipSlots:['cape']};
const lantern={...cape,id:'550',name:'Belt Lantern',equipSlots:['lantern']};
const player={available:true,playerName:'Player',level:42,gold:4000,armorRating:230,carryWeight:{current:180,max:300},equipment:{head:null,body:null,hands:null,feet:null,neck:null,ring:null,mainHand:null,offHand:null,ammo:null,cape,lantern}};
const panel=render(EquipmentPanel,{player,bridge:{},obscured:false,selected:cape,dragging:null,hoverSlot:'',busy:false,revision:0,onSelect(){},onChange(){}});
assert.equal((panel.match(/data-equip-slot="(?!auto")/g)||[]).length,11);assert.match(panel,/data-equip-slot="cape"/);assert.match(panel,/data-equip-slot="lantern"/);checks+=3;
assert.match(panel,/slot 46/);assert.match(panel,/slot 49/);assert.match(panel,/data-item-thumbnail="460"/);assert.match(panel,/data-item-thumbnail="550"/);checks+=4;
assert.match(panel,/Unequip Belt Lantern from Lantern/);assert.match(panel,/CHARACTER EQUIPMENT/);assert.match(panel,/LIVE OUTFIT/);checks+=3;
assert.doesNotMatch(panel,/DRAW PACKETS|[✦★☆]|equipment-slot__pin/);checks++;
const selection=render(EquipmentSelection,{player,selected:cape,enabled:true,onChange(){}});
assert.match(selection,/Traveler Cape/);assert.match(selection,/CURRENTLY EQUIPPED/);assert.match(selection,/>Unequip</);checks+=3;
const {ItemImage}=moduleSource('sdk/ui/src/field/ItemThumbnail.tsx',{'./itemThumbnails.js':{},'./model.js':{initials:()=> 'TC'}});
const image=render(ItemImage,{item:cape,src:'data:image/png;base64,actual-model'});
assert.match(image,/<span class="item-thumbnail__media"><img[^>]*src="data:image\/png;base64,actual-model"/);assert.match(image,/alt="Traveler Cape"/);assert.match(image,/draggable="false"/);checks+=3;
const noImage=render(ItemImage,{item:cape});assert.match(noImage,/Item artwork unavailable/);assert.doesNotMatch(noImage,/<img/);checks+=2;
console.log(checks+' model-stage/frame-slider rendering checks passed. No files emitted.');
