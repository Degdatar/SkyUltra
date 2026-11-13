// Source-only behavioral checks, interpreted in memory. No emitted files or build commands.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const {stripTypeScriptTypes}=require('node:module');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');let checks=0;
function source(file,names,ctx={}){let text=fs.readFileSync(path.join(root,file),'utf8').replace(/^import\b[\s\S]*?from\s+['"][^'"]+['"];\s*$/gm,'').replace(/^import .*?;\s*$/gm,'');
 text=stripTypeScriptTypes(text,{mode:'strip'}).replace(/^export\s+/gm,'');
 return vm.runInNewContext(text+'\n;({'+names.join(',')+'})',{console,...ctx});}
async function main(){
 const {skyStatMaximum,skyStatFraction}=source('sdk/ui/src/scales.ts',['skyStatMaximum','skyStatFraction']);
 assert.equal(skyStatMaximum([40000,80000]),80000);checks++;
 assert.equal(skyStatFraction(40000,80000),.5);checks++;
 assert.equal(skyStatMaximum([82689,82649,82704,448758,66731,110343]),500000);checks++;
 assert.equal(skyStatFraction(-40000,80000),.5);checks++;
 assert.equal(skyStatMaximum([NaN,Infinity]),100);checks++;
 assert.equal(skyStatFraction(NaN,100),0);checks++;
 assert.equal(skyStatFraction(80,100),.8);checks++;
 const events=new Map();let clock=20;
 const browser={location:{search:'?__skyultra_engine=ultralight&__skyultra_visible=0'},addEventListener:(name,callback)=>events.set(name,callback)};
 vm.runInNewContext(fs.readFileSync(path.join(root,'resources/SkyUltraBridge.js'),'utf8'),{window:browser,console,URLSearchParams,performance:{now:()=>clock}});
 assert.equal(browser.SkyWebView.getVisibility().visible,false);checks++;
 events.get('skyultra:message')({detail:{name:'skyultra.visibility',data:{visible:true}}});
 assert.equal(browser.SkyWebView.getVisibility().visibleSinceMs,20);checks++;
 clock=70;events.get('skyultra:message')({detail:{name:'skyultra.visibility',data:{visible:true}}});
 assert.equal(browser.SkyWebView.getVisibility().visibleSinceMs,20);checks++;
 events.get('skyultra:message')({detail:{name:'skyultra.visibility',data:{visible:false}}});
 clock=90;events.get('skyultra:message')({detail:{name:'skyultra.visibility',data:{visible:true}}});
 assert.equal(browser.SkyWebView.getVisibility().visibleSinceMs,90);checks++;
 const {createSkyUltraRenderer,createSkyUltraItemRenderer,skyUltraItemRendererMethods,skyUltraPreviewLightingPresets,skyUltraActorPosePresets}=source('sdk/typescript/src/renderer.ts',['createSkyUltraRenderer','createSkyUltraItemRenderer','skyUltraItemRendererMethods','skyUltraPreviewLightingPresets','skyUltraActorPosePresets']);
 const calls=[];let response={available:true,ready:false};
 const bridge={request:async(...args)=>{calls.push(args);return response;}};
 const renderer=createSkyUltraRenderer(bridge);
 const signal=new AbortController().signal;
 await renderer.setActor({referenceId:'0x123',portrait:true,bounds:{x:.1,y:.1,width:.4,height:.5},yaw:undefined},{signal});
 assert.equal(calls[0][0],'game.setActorPreview');checks++;
 assert.equal(calls[0][2].signal,signal);checks++;
 assert.equal('yaw' in calls[0][1],false);checks++;
 assert.equal('lighting' in calls[0][1],false);checks++;
 await renderer.hide();assert.equal(JSON.stringify(calls.at(-1).slice(0,2)),'["game.setActorPreview",{"visible":false}]');checks++;
 await renderer.close();assert.equal(calls.at(-1)[0],'game.closeActorPreview');checks++;
 await renderer.status();assert.equal(calls.at(-1)[0],'game.getActorPreviewStatus');checks++;
 for(const options of [{zoom:NaN},{width:Infinity},{bounds:{x:0,y:0,width:NaN,height:.5}}]){await assert.rejects(renderer.setActor(options),/finite/);checks++;}
 response={ready:true};await assert.rejects(renderer.status(),/Invalid actor renderer response/);checks++;
 response={available:true,ready:true};
 await renderer.setActor({pose:'fighting',panX:.25,panY:-.3});
 assert.equal(calls.at(-1)[1].pose,'fighting');assert.equal(calls.at(-1)[1].panX,.25);assert.equal(calls.at(-1)[1].panY,-.3);checks+=3;
 for(const options of [{panX:NaN},{panY:Infinity}]){await assert.rejects(renderer.setActor(options),/finite/);checks++;}
 await assert.rejects(renderer.setActor({pose:'not-a-pose'}),/Unknown actor preview pose/);checks++;
 // Presets are usable by plain JS callers too; reject bad values before dispatch.
 for(const lighting of skyUltraPreviewLightingPresets){
  await renderer.setActor({referenceId:'0x123',lighting,portrait:true,live:false,panX:.25},{signal});
  assert.equal(calls.at(-1)[0],'game.setActorPreview');assert.equal(calls.at(-1)[1].lighting,lighting);
  assert.equal(calls.at(-1)[1].portrait,true);assert.equal(calls.at(-1)[1].panX,.25);assert.equal(calls.at(-1)[2].signal,signal);checks+=5;
 }
 for(const lighting of ['unknown','Studio','',null,0,{},['soft']]){
  const count=calls.length;await assert.rejects(renderer.setActor({lighting}),/Unknown actor preview lighting preset/);
  assert.equal(calls.length,count);checks+=2;
 }
 const animationClip={id:'animation-1-0',name:'mt_idle',duration:2,sampleCount:33};
 response={available:true,clips:[animationClip],message:'Captured'};
 const captured=await renderer.captureAnimations({signal});
 assert.equal(calls.at(-1)[0],'game.captureActorAnimations');assert.equal(JSON.stringify(calls.at(-1)[1]),'{}');assert.equal(calls.at(-1)[2].signal,signal);assert.equal(captured.clips[0].name,'mt_idle');checks+=4;
 response={available:true,ready:true};
 for(const frame of [0,16,32]){
  await renderer.setActor({referenceId:'0x123',animation:{id:animationClip.id,frame},pose:'current',lighting:'warm',portrait:true},{signal});
  assert.equal(calls.at(-1)[1].animation.id,animationClip.id);assert.equal(calls.at(-1)[1].animation.frame,frame);assert.equal(calls.at(-1)[1].lighting,'warm');assert.equal(calls.at(-1)[2].signal,signal);checks+=4;
 }
 for(const animation of [null,{},'',{id:'',frame:0},{id:'x'.repeat(65),frame:0},{id:'ok',frame:-1},{id:'ok',frame:33},{id:'ok',frame:1.5},{id:'ok',frame:NaN},{id:'ok',frame:Infinity},{id:'ok',frame:'1'}]){
  const count=calls.length;await assert.rejects(renderer.setActor({animation}),/animation id or frame/);assert.equal(calls.length,count);checks+=2;
 }
 const beforeConflict=calls.length;await assert.rejects(renderer.setActor({animation:{id:animationClip.id,frame:0},pose:'fighting'}),/either an animation frame or an arm preset/);assert.equal(calls.length,beforeConflict);checks+=2;
 for(const failure of [null,{available:true},{available:true,clips:[]},{available:false,clips:[animationClip]},
  {available:true,clips:[animationClip,animationClip]},
  ...[{id:''},{name:''},{duration:0},{duration:Infinity},{duration:121},{sampleCount:1},{sampleCount:34},{sampleCount:3.5}].map(patch=>({available:true,clips:[{...animationClip,...patch}]})),
  {available:true,clips:Array.from({length:5},(_,i)=>({...animationClip,id:'id-'+i}))}]){
  response=failure;await assert.rejects(renderer.captureAnimations(),/Invalid actor animation capture response/);checks++;
 }
 response={available:false,clips:[],message:'No compatible loaded clips'};
 assert.equal((await renderer.captureAnimations()).available,false);assert.equal((await renderer.captureAnimations()).message,response.message);checks+=2;
 response={available:true,clips:skyUltraActorPosePresets.map((preset,i)=>({...animationClip,id:'pose-2-'+preset,preset,name:preset,duration:i+1}))};
 const library=await renderer.loadPoses({signal});
 assert.equal(calls.at(-1)[0],'game.loadActorPoses');assert.equal(JSON.stringify(calls.at(-1)[1]),'{}');assert.equal(calls.at(-1)[2].signal,signal);assert.equal(library.clips.length,4);checks+=4;
 for(let i=0;i<library.clips.length;i++){assert.equal(library.clips[i].preset,skyUltraActorPosePresets[i]);assert.equal(library.clips[i].sampleCount,33);checks+=2;}
 response={available:true,clips:[{...animationClip,id:'pose-2-idle',preset:'idle'}],message:'One pose available'};
 assert.equal((await renderer.loadPoses()).clips[0].preset,'idle');checks++;
 for(const preset of ['current','unknown','Idle',null,4]){response={available:true,clips:[{...animationClip,preset}]};await assert.rejects(renderer.loadPoses(),/Invalid actor animation capture response/);checks++;}
 response={available:false,clips:[],message:'Unsupported rig'};assert.equal((await renderer.loadPoses()).message,'Unsupported rig');checks++;
 response={saved:true,path:'C:/MO2/overwrite/SkyUltra/Actors/portrait.png'};
 assert.equal((await renderer.save({signal})).path,response.path);assert.equal(calls.at(-1)[0],'game.saveActorPreview');assert.equal(calls.at(-1)[2].signal,signal);checks+=3;
 assert.equal('longEdge' in calls.at(-1)[1],false);checks++;
 await renderer.save({longEdge:2048,signal});assert.equal(calls.at(-1)[1].longEdge,2048);assert.equal(calls.at(-1)[2].signal,signal);checks+=2;
 for(const longEdge of [720,1080,1440,2160,4096]){await renderer.save({longEdge});assert.equal(calls.at(-1)[1].longEdge,longEdge);checks++;}
 for(const longEdge of [NaN,Infinity,0,719,4097,2048.5]){const count=calls.length;await assert.rejects(renderer.save({longEdge}),/longEdge/);assert.equal(calls.length,count);checks+=2;}
 for(const failure of [{saved:false,message:'Disk full'},{saved:true},{available:true},null]){response=failure;await assert.rejects(renderer.save());checks++;}
 const items=createSkyUltraItemRenderer(bridge);
 response={available:true,ready:true,formId:'4839',geometryCount:2};
 for(const formId of [1,0xffffffff,'0x12EB7','4839','00004839']){
  const result=await items.setItem({formId,lighting:'cool',panX:.25,bounds:{x:.1,y:.2,width:.4,height:.5}},{signal});
  assert.equal(calls.at(-1)[0],'game.setItemPreview');assert.equal(calls.at(-1)[1].formId,formId);
  assert.equal(calls.at(-1)[1].panX,.25);assert.equal(calls.at(-1)[1].lighting,'cool');assert.equal(calls.at(-1)[2].signal,signal);assert.equal(result.formId,'4839');checks+=6;
 }
 for(const formId of [undefined,null,'','0','-1','1.5','0xZZ','4294967296','1e3',0,-1,NaN,Infinity,1.5,0x100000000,{},[]]){
  const count=calls.length;await assert.rejects(items.setItem({formId}),/valid item formId/);assert.equal(calls.length,count);checks+=2;
 }
 for(const patch of [{zoom:NaN},{panY:Infinity},{width:NaN},{bounds:{x:0,y:Infinity,width:.2,height:.3}}]){
  const count=calls.length;await assert.rejects(items.setItem({formId:1,...patch}),/finite/);assert.equal(calls.length,count);checks+=2;
 }
 for(const lighting of skyUltraPreviewLightingPresets){await items.setItem({formId:1,lighting});assert.equal(calls.at(-1)[1].lighting,lighting);checks++;}
 await assert.rejects(items.setItem({formId:1,lighting:'unknown'}),/Unknown item preview lighting/);checks++;
 await items.setItem({formId:1,referenceId:'x',path:'arbitrary.nif',animation:{id:'x',frame:1},pose:'idle',portrait:true,live:true});
 assert.equal(JSON.stringify(calls.at(-1)[1]),'{"formId":1}');checks++;
 await items.status({signal});assert.equal(calls.at(-1)[0],'game.getItemPreviewStatus');assert.equal(calls.at(-1)[2].signal,signal);checks+=2;
 await items.hide();assert.equal(JSON.stringify(calls.at(-1).slice(0,2)),'["game.setItemPreview",{"visible":false}]');checks++;
 await items.close();assert.equal(calls.at(-1)[0],'game.closeItemPreview');checks++;
 response={ready:true};await assert.rejects(items.status(),/Invalid item renderer response/);checks++;
 response={saved:true,path:'C:/MO2/overwrite/SkyUltra/Items/item.png'};
 assert.equal((await items.save({signal})).path,response.path);assert.equal(calls.at(-1)[0],'game.saveItemPreview');assert.equal(JSON.stringify(calls.at(-1)[1]),'{}');assert.equal(calls.at(-1)[2].signal,signal);checks+=4;
 for(const longEdge of [720,1080,1440,2048,2160,4096]){await items.save({longEdge});assert.equal(calls.at(-1)[1].longEdge,longEdge);checks++;}
 for(const longEdge of [NaN,Infinity,0,719,4097,2048.5]){const count=calls.length;await assert.rejects(items.save({longEdge}),/longEdge/);assert.equal(calls.length,count);checks+=2;}
 for(const failure of [null,{saved:true},{saved:false,message:'Disk full'},{saved:true,path:''}]){response=failure;await assert.rejects(items.save());checks++;}
 const {readSkyUltraSettings,updateSkyUltraSettings,defaultSkyUltraSettings,skyUltraActorExportResolutions}=source('sdk/typescript/src/settings.ts',['readSkyUltraSettings','updateSkyUltraSettings','defaultSkyUltraSettings','skyUltraActorExportResolutions']);
 let storedSettings={splash:{...defaultSkyUltraSettings.splash,volume:.35,audio:false}};const settingsCalls=[];
 const settingsBridge={request:async(method,params)=>{settingsCalls.push([method,params]);if(method==='ui.updateSettings')storedSettings=JSON.parse(JSON.stringify(params));return JSON.parse(JSON.stringify(storedSettings));}};
 const migrated=await readSkyUltraSettings(settingsBridge);
 assert.equal(migrated.actorExport.fullView,4096);assert.equal(migrated.actorExport.portrait,4096);assert.equal(migrated.splash.volume,.35);assert.equal(migrated.splash.audio,false);assert.equal(migrated.replaceSkyrimInventory,true);checks+=5;
 migrated.actorExport.fullView=720;migrated.actorExport.portrait=1080;migrated.replaceSkyrimInventory=false;
 const updated=await updateSkyUltraSettings(migrated,settingsBridge);
 assert.equal(updated.actorExport.fullView,720);assert.equal(updated.actorExport.portrait,1080);assert.equal(updated.splash.volume,.35);assert.equal(updated.replaceSkyrimInventory,false);assert.equal(defaultSkyUltraSettings.actorExport.fullView,4096);assert.equal(defaultSkyUltraSettings.replaceSkyrimInventory,true);checks+=6;
 updated.actorExport.portrait=4096;const reread=await readSkyUltraSettings(settingsBridge);assert.equal(reread.actorExport.portrait,1080);assert.equal(reread.actorExport.fullView,720);assert.equal(reread.replaceSkyrimInventory,false);checks+=3;
 const swfBytes=fs.readFileSync(path.join(root,'resources/skyultra_inventory.swf'));
 assert.equal(swfBytes.subarray(0,3).toString('ascii'),'FWS');assert.equal(swfBytes[3],8);assert.equal(swfBytes.readUInt32LE(4),swfBytes.length);checks+=3;
 for(const resolution of skyUltraActorExportResolutions){const saved=await updateSkyUltraSettings({...migrated,actorExport:{fullView:resolution,portrait:1080}},settingsBridge);assert.equal(saved.actorExport.fullView,resolution);assert.equal(saved.actorExport.portrait,1080);checks+=2;}
 for(const value of [NaN,Infinity,0,719,1024,1080.5,4097]){const count=settingsCalls.length;await assert.rejects(updateSkyUltraSettings({...migrated,actorExport:{fullView:value,portrait:1080}},settingsBridge),/export resolution/);assert.equal(settingsCalls.length,count);checks+=2;}
 const {defaultActorCamera,dragActorCamera}=source('sdk/ui/src/field/actorCamera.ts',['defaultActorCamera','dragActorCamera']);
 const camera=defaultActorCamera(),panned=dragActorCamera(camera,'pan',100,-50,500,500);
 assert.equal(panned.panX,.2);assert.equal(panned.panY,-.1);assert.equal(panned.yaw,0);assert.equal(panned.pitch,0);assert.equal(panned.zoom,1);checks+=5;
 const scaled=dragActorCamera(camera,'pan',200,-100,1000,1000);assert.equal(JSON.stringify(panned),JSON.stringify(scaled));checks++;
 const orbited=dragActorCamera(panned,'rotate',50,1000,500,500);assert.equal(orbited.panX,.2);assert.equal(orbited.panY,-.1);assert.equal(orbited.zoom,1);checks+=3;
 // Free orbit must preserve orientation through full turns, rather than stop
 // at the former pitch limit. Compare orientations independently of wrapping.
 for(const angle of [Math.PI/2,-Math.PI/2,Math.PI*6+.2,-Math.PI*6-.2]){
  const rotated=dragActorCamera(panned,'rotate',angle/.008,angle/.008,500,500);
  for(const axis of ['yaw','pitch']){
   assert.ok(Math.abs(Math.sin(rotated[axis])-Math.sin(angle))<1e-10,axis+' free-orbit sine');
   assert.ok(Math.abs(Math.cos(rotated[axis])-Math.cos(angle))<1e-10,axis+' free-orbit cosine');
   assert.ok(rotated[axis]>=-Math.PI && rotated[axis]<=Math.PI,axis+' bounded full-turn representation');checks+=3;
  }
 }
 assert.equal(dragActorCamera(camera,'pan',1e6,-1e6,100,100).panX,2);assert.equal(dragActorCamera(camera,'pan',1e6,-1e6,100,100).panY,-2);checks+=2;
 for(const dimensions of [[0,100],[100,0],[NaN,100]]){assert.equal(dragActorCamera(camera,'pan',50,50,...dimensions),camera);checks++;}
 assert.equal(camera.panX,0);assert.equal(camera.panY,0);assert.notEqual(defaultActorCamera(),camera);checks+=3;
 const {parseSkyUltraHudLayout}=source('sdk/typescript/src/layout.ts',['parseSkyUltraHudLayout']);
 const valid={version:1,widgets:['vitals','clock'],anchor:'top-right',scale:1,opacity:1,theme:'aurora'};
 assert.equal(JSON.stringify(parseSkyUltraHudLayout(JSON.stringify(valid))),JSON.stringify(valid));checks++;
 const parsed=parseSkyUltraHudLayout(valid);parsed.widgets.push('effects');assert.equal(valid.widgets.length,2);checks++;
 for(const patch of [{version:2},{widgets:['vitals','vitals']},{widgets:['unknown']},{scale:NaN},{opacity:Infinity},{anchor:'center'},{theme:'../bad'}]){assert.throws(()=>parseSkyUltraHudLayout({...valid,...patch}));checks++;}
 const {validateSkyUltraManifest,skyultra}=await import('../sdk/vite-plugin/index.js');
 const readable={};skyultra().config(readable);
 assert.equal(readable.build.rollupOptions.output.entryFileNames,'assets/[name].js');checks++;
 assert.equal(readable.build.rollupOptions.output.chunkFileNames,'assets/[name].js');checks++;
 assert.equal(readable.build.rollupOptions.output.assetFileNames,'assets/[name][extname]');checks++;
 const outputs=[{format:'es',entryFileNames:'entry/[name].js'},{format:'system'}];
 const custom={build:{assetsDir:'ui',rollupOptions:{input:{main:'index.html'},output:outputs}}};
 const pluginResult=skyultra().config(custom);
 assert.equal(custom.build.rollupOptions.output,outputs);checks++;
 assert.equal(outputs.length,2);checks++;
 assert.equal(outputs[0].entryFileNames,'entry/[name].js');checks++;
 assert.equal(outputs[1].chunkFileNames,'ui/[name].js');checks++;
 assert.equal(custom.build.rollupOptions.input.main,'index.html');checks++;
 assert.equal('rollupOptions' in pluginResult.build,false);checks++;
 const rootAssets={build:{assetsDir:''}};skyultra().config(rootAssets);
 assert.equal(rootAssets.build.rollupOptions.output.assetFileNames,'[name][extname]');checks++;
 const app=path.join(root,'apps/skyultra-menu');const manifest=await validateSkyUltraManifest('public/manifest.json',app);
 assert.equal(manifest.id,'skyultra.menu');assert.equal('papyrusPlugin' in manifest,false);checks+=2;
 for(const name of ['game.setActorPreview','game.getActorPreviewStatus','game.closeActorPreview','game.saveActorPreview','game.captureActorAnimations','game.loadActorPoses','ui.pageReady','ui.close','ui.configureWidgets','ui.getSettings','ui.updateSettings','ui.setCursor','ui.getSplashStatus','ui.replaySplash']){assert.ok(manifest.permissions.pageRequests.includes(name));checks++;}
 const sampleManifest=await validateSkyUltraManifest('public/manifest.json',path.join(root,'samples/ui-kit-test-mod'));
 assert.ok(sampleManifest.permissions.pageRequests.includes('game.captureActorAnimations'));checks++;
 assert.ok(sampleManifest.permissions.pageRequests.includes('game.loadActorPoses'));checks++;
 const sampleQuest=fs.readFileSync(path.join(root,'sdk/papyrus/Examples/SkyUltraPapyrusSampleQuest.psc'),'utf8');
 for(const method of skyUltraItemRendererMethods){
  assert.ok(manifest.permissions.pageRequests.includes(method));assert.ok(sampleManifest.permissions.pageRequests.includes(method));
  assert.ok(sampleQuest.includes('AddPageRequestMethod(SampleView, "'+method+'")'));checks+=3;
 }
 const {
  createSkyUltraGame,
  subscribeSkyUltraGame,
  getSharedGameSnapshot,
  applySkyUltraGameUpdate,
  skyUltraGameTopics,
  skyUltraGameSubscriptionMethods,
 }=source('sdk/typescript/src/game.ts',[
  'createSkyUltraGame',
  'subscribeSkyUltraGame',
  'getSharedGameSnapshot',
  'applySkyUltraGameUpdate',
  'skyUltraGameTopics',
  'skyUltraGameSubscriptionMethods',
 ]);
 assert.equal(JSON.stringify(skyUltraGameTopics),'["vitals","equipment","inventory","effects","quests","followers"]');checks++;
 for(const method of skyUltraGameSubscriptionMethods){
  assert.ok(manifest.permissions.pageRequests.includes(method));
  assert.ok(sampleManifest.permissions.pageRequests.includes(method));
  assert.ok(sampleQuest.includes('AddPageRequestMethod(SampleView, "'+method+'")'));
  checks+=3;
 }
 // Verify incremental delta application & topic diffing
 const initialSnap={
  available:true,sequence:1,snapshotScope:'full',playerName:'Dragonborn',level:42,
  health:{current:300,max:320},stamina:{current:200,max:200},magicka:{current:150,max:150},armorRating:512,gold:9500,
  equipment:{head:null,body:{id:'100',name:'Ebony Armor',category:'armor',count:1,value:1500,weight:38,equipped:true},hands:null,feet:null,mainHand:null,offHand:null},
  inventory:[{id:'100',name:'Ebony Armor',category:'armor',count:1,value:1500,weight:38,equipped:true}],
  inventoryTypeCount:1,inventoryItemCount:1,inventoryTruncated:false,
  activeEffects:[{id:'1:1',name:'Fortify Health',magnitude:50,duration:60,remaining:45,hostile:false}],activeEffectCount:1,
  quests:[{id:'MQ101',formId:'800',name:'Unbound',stage:20,active:true,type:'Main Quest',objectives:[{index:10,text:'Escape Helgen',completed:false,failed:false}]}],questCount:1,
  followers:[{baseFormId:'666',referenceId:'777',name:'Lydia',race:'Nord',sex:'Female',follower:true,health:250,healthMax:250}],followerCount:1,
 };
 const appliedInitial=applySkyUltraGameUpdate(null,initialSnap);
 assert.equal(appliedInitial.sequence,1);
 assert.equal(appliedInitial.changedTopics.length,6);
 assert.equal(appliedInitial.snapshot.quests[0].name,'Unbound');
 assert.equal(appliedInitial.snapshot.followers[0].name,'Lydia');
 checks+=4;
 const appliedVitalsDelta=applySkyUltraGameUpdate(appliedInitial.snapshot,{
  available:true,sequence:2,snapshotScope:'full',changedTopics:['vitals'],
  delta:{health:{current:180,max:320},gold:9750},
 });
 assert.equal(appliedVitalsDelta.sequence,2);
 assert.equal(JSON.stringify(appliedVitalsDelta.changedTopics),'["vitals"]');
 assert.equal(appliedVitalsDelta.snapshot.health.current,180);
 assert.equal(appliedVitalsDelta.snapshot.gold,9750);
 // Unchanged topics must remain intact on the merged snapshot
 assert.equal(appliedVitalsDelta.snapshot.equipment.body.name,'Ebony Armor');
 assert.equal(appliedVitalsDelta.snapshot.quests[0].name,'Unbound');
 assert.equal(appliedVitalsDelta.snapshot.followers[0].name,'Lydia');
 checks+=7;

 // Verify shared per-bridge feed multiplexing across multiple subscribers without duplicate polling
 const subCalls=[];
 const bridgeListeners=new Map();
 let bridgeVisible=true;
 const feedBridge={
  getVisibility:()=>({visible:bridgeVisible,visibleSinceMs:10}),
  on:(name,cb)=>{if(!bridgeListeners.has(name))bridgeListeners.set(name,new Set());bridgeListeners.get(name).add(cb);return ()=>bridgeListeners.get(name)?.delete(cb);},
  off:(name,cb)=>Boolean(bridgeListeners.get(name)?.delete(cb)),
  request:async(method,params)=>{
   subCalls.push([method,params]);
   if(method==='game.subscribe'){
    return {
     subscribed:true,
     sequence:10,
     snapshotScope:params.scope||'full',
     topics:params.topics,
     intervalMs:params.intervalMs,
     update:{available:true,sequence:10,snapshotScope:params.scope||'full',changedTopics:skyUltraGameTopics,delta:initialSnap,snapshot:initialSnap},
    };
   }
   if(method==='game.getPlayerSnapshot'){
    return {...initialSnap,sequence:15,gold:12000};
   }
   if(method==='game.unsubscribe')return {subscribed:false};
   return {};
  },
 };
 const emitBridge=(name,data)=>{for(const cb of [...(bridgeListeners.get(name)||[])])cb(data);};
 const modAVitalsEvents=[];
 const modBQuestEvents=[];
 const subA=subscribeSkyUltraGame(feedBridge,ev=>modAVitalsEvents.push(ev),{topics:['vitals','equipment'],scope:'full',intervalMs:500});
 const subB=subscribeSkyUltraGame(feedBridge,ev=>modBQuestEvents.push(ev),{topics:['quests','followers'],scope:'full',intervalMs:500});
 await new Promise(r=>setTimeout(r,10));
 // Both subscribers share the initial host subscription
 assert.equal(modAVitalsEvents.length,1);
 assert.equal(modBQuestEvents.length,1);
 assert.equal(getSharedGameSnapshot(feedBridge).playerName,'Dragonborn');
 checks+=3;
 // Emit a vitals-only incremental update: only subA should be notified, not subB!
 emitBridge('skyultra.game.update',{
  available:true,sequence:11,snapshotScope:'full',changedTopics:['vitals'],
  delta:{health:{current:120,max:320}},
 });
 assert.equal(modAVitalsEvents.length,2);
 assert.equal(modAVitalsEvents.at(-1).snapshot.health.current,120);
 assert.equal(modBQuestEvents.length,1);
 checks+=3;
 // Emit a quests-only incremental update: only subB should be notified, not subA!
 emitBridge('skyultra.game.update',{
  available:true,sequence:12,snapshotScope:'full',changedTopics:['quests'],
  delta:{quests:[{id:'MQ102',formId:'801',name:'Before the Storm',stage:10,active:true,type:'Main Quest',objectives:[]}],questCount:1},
 });
 assert.equal(modAVitalsEvents.length,2);
 assert.equal(modBQuestEvents.length,2);
 assert.equal(modBQuestEvents.at(-1).snapshot.quests[0].name,'Before the Storm');
 assert.equal(getSharedGameSnapshot(feedBridge).health.current,120);
 checks+=4;
 // Visibility pause & catch-up when shown again
 bridgeVisible=false;
 emitBridge('skyultra.visibility',{visible:false});
 emitBridge('skyultra.game.update',{
  available:true,sequence:13,snapshotScope:'full',changedTopics:['vitals'],
  delta:{health:{current:50,max:320}},
 });
 // Suppressed while hidden
 assert.equal(modAVitalsEvents.length,2);checks++;
 bridgeVisible=true;
 emitBridge('skyultra.visibility',{visible:true});
 await new Promise(r=>setTimeout(r,10));
 assert.equal(getSharedGameSnapshot(feedBridge).gold,12000);checks++;
 // Unsubscribe cleanup: game.unsubscribe called only when last subscriber detaches
 const unsubCountBefore=subCalls.filter(c=>c[0]==='game.unsubscribe').length;
 subA.unsubscribe();
 assert.equal(subCalls.filter(c=>c[0]==='game.unsubscribe').length,unsubCountBefore);checks++;
 subB.unsubscribe();
 assert.equal(subCalls.filter(c=>c[0]==='game.unsubscribe').length,unsubCountBefore+1);checks++;

 // Visual HUD Builder layout extensions & Working Mod Exporter contracts (Priority #3)
 const {snapSkyUltraHudCoordinate}=source('sdk/typescript/src/layout.ts',['snapSkyUltraHudCoordinate']);
 assert.equal(snapSkyUltraHudCoordinate(19,8),16);
 assert.equal(snapSkyUltraHudCoordinate(-21,16),-16);
 assert.equal(snapSkyUltraHudCoordinate(999,0),600);
 checks+=3;
 const builderLayout=parseSkyUltraHudLayout({
  version:1,
  widgets:['vitals','radar','clock'],
  anchor:'top-left',
  scale:1.05,
  opacity:0.95,
  glow:115,
  snapGrid:16,
  theme:'dragonstone',
  placements:{
   vitals:{anchor:'bottom-center',scale:1.1,opacity:0.98,glow:120,color:'#ff6b81',fontStyle:'display',offsetX:32,offsetY:-16,width:320,customLabel:'Dragonborn Vitals',bindingPath:'health.current',visibilityRule:'combat-low-hp'},
   radar:{anchor:'top-right',offsetX:-24,offsetY:16,visibilityRule:'always'},
  },
 });
 assert.equal(builderLayout.snapGrid,16);
 assert.equal(builderLayout.placements.vitals.offsetX,32);
 assert.equal(builderLayout.placements.vitals.offsetY,-16);
 assert.equal(builderLayout.placements.vitals.customLabel,'Dragonborn Vitals');
 assert.equal(builderLayout.placements.vitals.bindingPath,'health.current');
 assert.equal(builderLayout.placements.vitals.visibilityRule,'combat-low-hp');
 checks+=6;
 const sanitizedLayout=parseSkyUltraHudLayout({...builderLayout,snapGrid:7,placements:{vitals:{visibilityRule:'invalid-rule',offsetX:NaN,offsetY:12}}});
 assert.equal('snapGrid' in sanitizedLayout,false);
 assert.equal('visibilityRule' in sanitizedLayout.placements.vitals,false);
 assert.equal('offsetX' in sanitizedLayout.placements.vitals,false);
 assert.equal(sanitizedLayout.placements.vitals.offsetY,12);
 checks+=4;

 const {sanitizeSkyUltraModId,generateSkyUltraHudModFiles,exportSkyUltraHudMod,buildCustomHudDocument,SKYULTRA_HUD_CODE_TEMPLATES}=source(
  'sdk/typescript/src/hudModExport.ts',
  ['sanitizeSkyUltraModId','generateSkyUltraHudModFiles','exportSkyUltraHudMod','buildCustomHudDocument','SKYULTRA_HUD_CODE_TEMPLATES'],
  {parseSkyUltraHudLayout},
 );
 assert.equal(sanitizeSkyUltraModId('Test'),'test');
 assert.equal(sanitizeSkyUltraModId('My Dragon HUD!'),'my_dragon_hud');
 assert.equal(sanitizeSkyUltraModId('Author.NordicHud'),'author.nordichud');
 assert.equal(sanitizeSkyUltraModId('actors'),'actors_hud');
 assert.ok(SKYULTRA_HUD_CODE_TEMPLATES.length>=4);
 checks+=5;
 const dotlessPkg=generateSkyUltraHudModFiles({
  id:'test',
  name:'Test Tactical HUD',
  layout:builderLayout,
  customCode:{enabled:true,mode:'split',html:'<div id="custom-hud-node"></div>',css:'#custom-hud-node{color:#fff}',js:'SkyUltraHud.onUpdate(()=>{});',fullHtml:''},
 });
 assert.equal(JSON.parse(dotlessPkg.files.find(f=>f.path==='manifest.json').content).id,'test');
 assert.ok(dotlessPkg.files.find(f=>f.path==='hud.html').content.includes('custom-hud-node'));
 assert.ok(buildCustomHudDocument({enabled:true,mode:'full-html',html:'',css:'',js:'',fullHtml:'<!doctype html><html><head><title>X</title></head><body><div id="pasted"></div></body></html>'}).includes('id="pasted"'));
 checks+=3;
 const modPkg=generateSkyUltraHudModFiles({
  id:'author.nordic_hud',
  name:'Nordic Tactical HUD',
  version:'1.2.0',
  author:'Author',
  hotkey:'F8',
  layout:builderLayout,
 });
 assert.equal(modPkg.files.length,11);
 const exportedManifest=JSON.parse(modPkg.files.find(f=>f.path==='manifest.json').content);
 assert.equal(exportedManifest.schemaVersion,1);
 assert.equal(exportedManifest.id,'author.nordic_hud');
 assert.equal(exportedManifest.permissions.storage,true);
 assert.equal(exportedManifest.permissions.assetAccess,true);
 assert.ok(exportedManifest.permissions.pageRequests.includes('game.getNearbyNpcs'));
 assert.equal(exportedManifest.permissions.pageRequests.includes('game.getActorThumbnail'),false);
 checks+=7;
 for(const badSpec of [{id:'NoDots',name:'X',layout:builderLayout},{id:'Upper.Case',name:'X',layout:builderLayout},{id:'actors',name:'X',layout:builderLayout},{id:'ok.id',name:'',layout:builderLayout},{id:'ok.id',name:'X',version:'bad',layout:builderLayout}]){
  assert.throws(()=>generateSkyUltraHudModFiles(badSpec));checks++;
 }
 const exportCalls=[];
 const exportRes=await exportSkyUltraHudMod(
  {id:'author.nordic_hud',name:'Nordic Tactical HUD',layout:builderLayout},
  {request:async(m,p)=>{exportCalls.push([m,p]);return {exported:true,modId:p.id,fileCount:p.files.length};}},
 );
 assert.equal(exportCalls[0][0],'ui.exportHudMod');
 assert.equal(exportRes.exported,true);
 assert.equal(exportRes.fileCount,11);
 checks+=3;

 // In-Game Developer Studio & Broken Mod Doctor contracts (Priority #4)
 const {skyUltraDevToolsMethods,diagnoseSkyUltraModRecord,readSkyUltraDeveloperSnapshot,reloadSkyUltraModViews,manageSkyUltraDiagnosticSample}=source(
  'sdk/typescript/src/devtools.ts',
  ['skyUltraDevToolsMethods','diagnoseSkyUltraModRecord','readSkyUltraDeveloperSnapshot','reloadSkyUltraModViews','manageSkyUltraDiagnosticSample'],
 );
 for(const m of skyUltraDevToolsMethods){
  assert.ok(manifest.permissions.pageRequests.includes(m),'Missing devtools method in skyultra.menu manifest: '+m);
  checks++;
 }
 const brokenSampleManifest=await validateSkyUltraManifest('manifest.json',path.join(root,'samples/broken-diagnostic-mod'));
 assert.equal(brokenSampleManifest.id,'samples.broken_mod');
 assert.equal(brokenSampleManifest.permissions.pageRequests.includes('game.getInventory'),false);
 checks+=2;
 const denialLog=[{seq:1,timeMs:50,viewId:2,modId:'samples.broken_mod',kind:'permission-denied',method:'game.getInventory',status:'denied',durationMs:.2,payloadBytes:20}];
 const brokenFindings=diagnoseSkyUltraModRecord({
  id:'samples.broken_mod',name:'Broken Diagnostic Sample',version:'0.1.0',folderName:'samples.broken_mod',
  registered:true,manifestValid:true,entry:'index.html',entryExists:true,hudExists:false,
  assetCount:2,totalAssetBytes:2048,missingAssets:['assets/missing-emblem.svg'],
  storage:false,assetAccess:true,networkHosts:[],pageRequests:['game.getPlayerSnapshot'],
  activeViewCount:1,estimatedSurfaceBytes:921600,bridgeRequestCount:1,bridgeErrorCount:1,deniedPermissionCount:1,hotReloadCount:0,
 },denialLog);
 assert.ok(brokenFindings.some(f=>f.code==='ASSET_MISSING'));
 assert.ok(brokenFindings.some(f=>f.code==='MISSING_PAGE_READY_PERMISSION'));
 assert.ok(brokenFindings.some(f=>f.code==='PERMISSION_DENIED_RUNTIME'));
 checks+=3;
 const repairedFindings=diagnoseSkyUltraModRecord({
  id:'samples.broken_mod',name:'Broken Diagnostic Sample (Repaired)',version:'0.1.1',folderName:'samples.broken_mod',
  registered:true,manifestValid:true,entry:'index.html',entryExists:true,hudExists:false,
  assetCount:3,totalAssetBytes:3072,missingAssets:[],
  storage:false,assetAccess:true,networkHosts:[],pageRequests:['game.getPlayerSnapshot','game.getInventory','ui.pageReady'],
  activeViewCount:1,estimatedSurfaceBytes:921600,bridgeRequestCount:3,bridgeErrorCount:0,deniedPermissionCount:0,hotReloadCount:1,
 },denialLog);
 assert.equal(repairedFindings.length,1);
 assert.equal(repairedFindings[0].code,'HEALTHY');
 checks+=2;
 const devCalls=[];
 const devBridge={request:async(m,p)=>{
  devCalls.push([m,p]);
  if(m==='ui.getDeveloperSnapshot')return {
   available:true,capturedAtMs:10,processWorkingSetBytes:100,processPrivateBytes:90,hotReloadEnabled:true,hotReloadTotalCount:1,lastReloadedMod:'samples.broken_mod',hostGpuMs:.3,compositorGpuMs:.2,callbackCpuMs:.4,frameCount:100,activeSubscriptionCount:1,views:[],
   mods:[
    {id:'actors',name:'actors',version:'0.0.0',folderName:'Actors',registered:false,manifestValid:false,entry:'index.html',entryExists:false,hudExists:false,assetCount:0,totalAssetBytes:0,missingAssets:[],storage:false,assetAccess:false,networkHosts:[],pageRequests:[],activeViewCount:0,estimatedSurfaceBytes:0,bridgeRequestCount:0,bridgeErrorCount:0,deniedPermissionCount:0,hotReloadCount:0,findings:[]},
    {id:'items',name:'items',version:'0.0.0',folderName:'Items',registered:false,manifestValid:false,entry:'index.html',entryExists:false,hudExists:false,assetCount:0,totalAssetBytes:0,missingAssets:[],storage:false,assetAccess:false,networkHosts:[],pageRequests:[],activeViewCount:0,estimatedSurfaceBytes:0,bridgeRequestCount:0,bridgeErrorCount:0,deniedPermissionCount:0,hotReloadCount:0,findings:[]},
    {id:'skyultra.menu',name:'SkyUltra',version:'0.1.0',folderName:'skyultra.menu',registered:true,manifestValid:true,entry:'Files/index.html',entryExists:true,hudExists:true,assetCount:15,totalAssetBytes:1024,missingAssets:[],storage:false,assetAccess:true,networkHosts:[],pageRequests:['ui.pageReady'],activeViewCount:1,estimatedSurfaceBytes:8294400,bridgeRequestCount:5,bridgeErrorCount:0,deniedPermissionCount:0,hotReloadCount:0,findings:[]},
   ],
   bridgeLog:[]
  };
  if(m==='ui.reloadModViews')return {reloaded:true,reloadedViewCount:1,hotReloadEnabled:true};
  if(m==='ui.manageDiagnosticSample')return {accepted:true,action:p.action,modId:'samples.broken_mod'};
  return {};
 }};
 const devSnap=await readSkyUltraDeveloperSnapshot(devBridge);
 assert.equal(devSnap.available,true);
 assert.equal(devSnap.mods.length,1);
 assert.equal(devSnap.mods[0].id,'skyultra.menu');
 assert.equal((await reloadSkyUltraModViews({modId:'samples.broken_mod',setHotReloadWatcher:true},devBridge)).reloaded,true);
 assert.equal((await manageSkyUltraDiagnosticSample('deploy-broken',devBridge)).action,'deploy-broken');
 await assert.rejects(manageSkyUltraDiagnosticSample('invalid-action',devBridge),/Unknown diagnostic sample action/);
 checks+=6;

 // Persistent Character/Profile Layouts contracts (Priority #5)
 const settingsExports=source(
  'sdk/typescript/src/settings.ts',
  ['defaultSkyUltraUiPreferences','sanitizeSkyUltraUiPrefs','readSkyUltraSettings','readSkyUltraUiPrefs','writeSkyUltraUiPrefs','saveSkyUltraHostProfileState'],
  {getSkyWebView:()=>devBridge},
 );
 const layoutExports=source(
  'sdk/typescript/src/layout.ts',
  ['parseSkyUltraHudLayout','skyUltraWidgetAnchors','skyUltraWidgetKinds'],
 );
 const {
  characterProfileIdForName,
  createDefaultSkyUltraProfileStore,
  migrateSkyUltraProfileStore,
  resolveEffectiveSkyUltraProfile,
  updateProfileForContext,
  promoteProfileToGlobalDefaults,
  resetSkyUltraProfile,
  exportSkyUltraProfileBundle,
  importSkyUltraProfileBundle,
  persistSkyUltraProfileStore,
 }=source(
  'sdk/typescript/src/profiles.ts',
  [
   'characterProfileIdForName',
   'createDefaultSkyUltraProfileStore',
   'migrateSkyUltraProfileStore',
   'resolveEffectiveSkyUltraProfile',
   'updateProfileForContext',
   'promoteProfileToGlobalDefaults',
   'resetSkyUltraProfile',
   'exportSkyUltraProfileBundle',
   'importSkyUltraProfileBundle',
   'persistSkyUltraProfileStore',
  ],
  {...settingsExports,...layoutExports,getSkyWebView:()=>devBridge},
 );
 assert.equal(characterProfileIdForName('Degdatar'),'char.degdatar');
 assert.equal(characterProfileIdForName('  Arch-Mage Savos! '),'char.arch_mage_savos');
 assert.equal(characterProfileIdForName(''),'global');
 checks+=3;
 // Legacy v1 migration -> schemaVersion 2
 const migratedFromV1=migrateSkyUltraProfileStore({
  version:1,
  defaultTheme:'sovngarde',
  density:'compact',
  neonGlow:130,
  hudLayout:{widgets:['vitals','compass'],theme:'sovngarde',snapGrid:16},
  loadouts:[{id:'L1',name:'Nightingale Set',characterName:'Degdatar',createdAt:'12:00',slots:{head:{id:'0x100',name:'Shrouded Cowl',category:'armor'}}}],
 });
 assert.equal(migratedFromV1.schemaVersion,2);
 assert.equal(migratedFromV1.migratedFromVersion,1);
 assert.equal(migratedFromV1.globalDefaults.theme,'sovngarde');
 assert.equal(migratedFromV1.globalDefaults.uiPrefs.density,'compact');
 assert.equal(migratedFromV1.globalDefaults.hudLayout.widgets.length,2);
 assert.equal(migratedFromV1.profiles['char.degdatar'].loadouts.length,1);
 checks+=6;
 // Per-character overrides + profile switching between Degdatar and Serana
 let pStore=createDefaultSkyUltraProfileStore();
 assert.equal(pStore.globalDefaults.theme,'dragonstone');
 pStore=updateProfileForContext(pStore,'Degdatar',{theme:'cyberpunk',uiPrefs:{density:'compact',neonGlow:140}});
 const degResolved=resolveEffectiveSkyUltraProfile(pStore,'Degdatar');
 const newCharResolved=resolveEffectiveSkyUltraProfile(pStore,'Ralof');
 assert.equal(degResolved.theme,'cyberpunk');
 assert.equal(degResolved.uiPrefs.density,'compact');
 assert.equal(degResolved.hasCharacterOverrides,true);
 assert.equal(newCharResolved.theme,'dragonstone');
 assert.equal(newCharResolved.hasCharacterOverrides,false);
 checks+=6;
 // Promote Degdatar's layout/theme to Global Defaults, then reset Degdatar's character overrides
 const promotedStore=promoteProfileToGlobalDefaults(pStore,'Degdatar');
 assert.equal(resolveEffectiveSkyUltraProfile(promotedStore,'Ralof').theme,'cyberpunk');
 pStore=updateProfileForContext(promotedStore,'Degdatar',{theme:'bloodmoon'});
 assert.equal(resolveEffectiveSkyUltraProfile(pStore,'Degdatar').theme,'bloodmoon');
 const charResetStore=resetSkyUltraProfile(pStore,'character','Degdatar');
 assert.equal(resolveEffectiveSkyUltraProfile(charResetStore,'Degdatar').theme,'cyberpunk');
 checks+=3;
 // Export / Import roundtrip & factory reset
 const bundleJson=exportSkyUltraProfileBundle(pStore);
 const reimported=importSkyUltraProfileBundle(bundleJson);
 assert.equal(resolveEffectiveSkyUltraProfile(reimported,'Degdatar').theme,'bloodmoon');
 const factoryReset=resetSkyUltraProfile(reimported,'all','Degdatar');
 assert.equal(factoryReset.globalDefaults.theme,'dragonstone');
 assert.equal(Object.keys(factoryReset.profiles).length,0);
 checks+=3;
 // Host persistence via ui.updateSettings (schemaVersion: 2, profileState)
 const persistedCalls=[];
 const profBridge={request:async(m,p)=>{persistedCalls.push([m,p]);return {schemaVersion:2,splash:{enabled:true,playback:'full-duration',replay:'session',audio:true,volume:.8},actorExport:{fullView:4096,portrait:4096},profileState:p.profileState};}};
 await persistSkyUltraProfileStore(reimported,profBridge,'Degdatar');
 assert.equal(persistedCalls.at(-1)[0],'ui.updateSettings');
 assert.equal(persistedCalls.at(-1)[1].schemaVersion,2);
 assert.equal(persistedCalls.at(-1)[1].profileState.profiles['char.degdatar'].theme,'bloodmoon');
 checks+=3;

 // Migration & Runtime Coexistence tools contracts (Priority #6)
 const {
  SKYULTRA_COMPATIBILITY_MATRIX,
  auditSkyUltraManifestObject,
  auditLegacyModSourceForSkyUltra,
  diagnoseCoexistenceAndLayering,
  createSkyUltraLegacyBridgeAdapter,
 }=source(
  'sdk/typescript/src/compatibility.ts',
  [
   'SKYULTRA_COMPATIBILITY_MATRIX',
   'auditSkyUltraManifestObject',
   'auditLegacyModSourceForSkyUltra',
   'diagnoseCoexistenceAndLayering',
   'createSkyUltraLegacyBridgeAdapter',
  ],
  {getSkyWebView:()=>devBridge},
 );
 assert.ok(SKYULTRA_COMPATIBILITY_MATRIX.some(e=>e.id==='prismaui-coexistence'));
 assert.ok(SKYULTRA_COMPATIBILITY_MATRIX.some(e=>e.id==='magelight-cef'));
 assert.ok(SKYULTRA_COMPATIBILITY_MATRIX.some(e=>e.id==='vanilla-scaleform'));
 checks+=3;
 const portedSampleManifest=await validateSkyUltraManifest('manifest.json',path.join(root,'samples/legacy-webview-port-sample'));
 assert.equal(portedSampleManifest.id,'samples.legacy_webview_port');
 assert.equal(auditSkyUltraManifestObject(portedSampleManifest).valid,true);
 assert.equal(auditSkyUltraManifestObject({...portedSampleManifest,unsupportedField:true}).valid,false);
 checks+=3;
 const legacyAudit=auditLegacyModSourceForSkyUltra(
  '<link href="https://fonts.googleapis.com/css" /><script>InteropCall("getPlayerStats"); window.cefQuery({request:"x"}); window.close();</script>',
  {modId:'author.my_port',modName:'My Port'},
 );
 assert.ok(legacyAudit.findings.some(f=>f.code==='LEGACY_PRISMAUI_CALL'));
 assert.ok(legacyAudit.findings.some(f=>f.code==='LEGACY_CEF_QUERY'));
 assert.ok(legacyAudit.findings.some(f=>f.code==='DOM_WINDOW_CLOSE'));
 assert.ok(legacyAudit.findings.some(f=>f.code==='EXTERNAL_CDN_ASSET'));
 assert.equal(auditSkyUltraManifestObject(legacyAudit.suggestedManifest).valid,true);
 checks+=5;
 const badHudSnap={...devSnap,views:[{handle:9,viewId:9,configId:'hud',modId:'author.bad_hud',engine:'ultralight',layer:'hud',visible:true,transparent:true,clickThrough:false,modal:true,width:1920,height:1080,fpsLimit:60,inputScopeDepth:1,pendingRequests:0,surfaceBytes:100,allowedMethods:['ui.pageReady'],initialUrl:'file:///x'}]};
 assert.ok(diagnoseCoexistenceAndLayering(badHudSnap).some(f=>f.code==='HUD_STEALING_INPUT'));
 assert.equal(diagnoseCoexistenceAndLayering(devSnap)[0].code,'COEXISTENCE_HEALTHY');
 checks+=2;
 const adapterCalls=[];
 const legacyAdapter=createSkyUltraLegacyBridgeAdapter({
  on:()=>()=>{},
  off:()=>true,
  request:async(m,p)=>{adapterCalls.push([m,p]);return {ok:true,ready:true,closed:true};},
 });
 await legacyAdapter.ready();
 await legacyAdapter.call('getPlayerStats',{});
 await legacyAdapter.close();
 assert.deepEqual(adapterCalls.map(c=>c[0]),['ui.pageReady','game.getPlayerSnapshot','ui.close']);
 checks+=1;

 // Next-gen roadmap enhancements: Audio bridge, 3D Worldspace/VR, DOM Inspector, Gamepad & Competitor Shims
 const enhCalls=[];
 const enhBridge={
  on:()=>()=>{},
  off:()=>true,
  request:async(m,p)=>{
   enhCalls.push([m,p]);
   if(m==='ui.playSound')return {available:true,played:true,sound:'UIMenuOK'};
   if(m==='ui.projectWorldAnchors')return {available:true,vr:false,anchors:[{id:'a1',available:true,onScreen:true,u:0.5,v:0.5,depth:0.3,distance:220,bone:'NPC Head [Head]',name:'Bandit',level:12,health:120,healthMax:150,hostile:true,dead:false}]};
   if(m==='ui.setSpatialPlacement')return {available:true,updated:true,mode:p.mode||'atGaze',distance:1.25,scale:1,bone:'NPC Head [Head]',targetRefId:0,virtualKeyboard:true,vrActive:false};
   if(m==='ui.toggleDomInspector')return {available:true,viewId:1,inspectorOpen:true,message:'Attached'};
   if(m==='ui.interopCall')return {accepted:true,framework:p.framework,method:p.method};
   return {};
  },
 };
 const {
  playSkyUltraSound,
  projectSkyUltraWorldAnchors,
  configureSkyUltraSpatialPlacement,
  toggleSkyUltraDomInspector,
  createSkyUltraGamepadHelper,
  installSkyUltraCompetitorShims,
 }=source('sdk/typescript/src/enhancements.ts',[
  'playSkyUltraSound',
  'projectSkyUltraWorldAnchors',
  'configureSkyUltraSpatialPlacement',
  'toggleSkyUltraDomInspector',
  'createSkyUltraGamepadHelper',
  'installSkyUltraCompetitorShims',
 ],{getSkyWebView:()=>enhBridge});
 const sndRes=await playSkyUltraSound('ok',enhBridge);
 assert.equal(sndRes.played,true);
 assert.equal(sndRes.sound,'UIMenuOK');
 const projRes=await projectSkyUltraWorldAnchors([{id:'a1',referenceId:'0x14'}],enhBridge);
 assert.equal(projRes.anchors.length,1);
 assert.equal(projRes.anchors[0].name,'Bandit');
 const spatRes=await configureSkyUltraSpatialPlacement({mode:'atGaze'},enhBridge);
 assert.equal(spatRes.mode,'atGaze');
 const domRes=await toggleSkyUltraDomInspector({viewId:1,open:true},enhBridge);
 assert.equal(domRes.inspectorOpen,true);
 const gp=createSkyUltraGamepadHelper({family:'xbox',virtualCursorEnabled:true});
 assert.equal(gp.getState().glyphs.confirm,'A');
 gp.setGlyphFamily('playstation');
 assert.equal(gp.getState().glyphs.confirm,'✕');
 const fakeWin={};
 const shims=installSkyUltraCompetitorShims(fakeWin,enhBridge);
 assert.equal(typeof fakeWin.RegisterJSListener,'function');
 assert.equal(typeof fakeWin.InteropCall,'function');
 assert.equal(typeof fakeWin.magelight.send,'function');
 await shims.prismaUI.InteropCall('ping',{x:1});
 await shims.magelight.vr.atGaze(1.5);
 shims.uninstall();
 assert.equal('prismaUI' in fakeWin,false);
 checks+=12;

 console.log(checks+' renderer/layout/manifest contract checks passed. No files emitted.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
