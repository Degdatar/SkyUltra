// Interpret production SDK source in memory; no bundle, native binary or game.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const listeners=new Map(),calls=[];
const emit=(name,value)=>{for(const callback of [...(listeners.get(name)??[])])callback(value);};
const bridge={
 on(name,callback){if(!listeners.has(name))listeners.set(name,new Set());listeners.get(name).add(callback);return()=>listeners.get(name).delete(callback);},
 async request(name,params){calls.push({name,params});
  if(name==='ui.interopCall'){emit('skyultra.legacy.interop',params);return{accepted:true,framework:params.framework,method:params.method};}
  if(name==='ui.setSpatialPlacement')return{available:true,updated:true,mode:params.mode,distance:params.distance};
  if(name==='ui.playSound')return{available:true,played:true,sound:params.sound};
  return{ok:true};
 },
};
const exportsObject={};
const js=ts.transpileModule(fs.readFileSync(path.join(root,'sdk/typescript/src/enhancements.ts'),'utf8'),{
 compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022},
}).outputText;
vm.runInNewContext(js,{exports:exportsObject,require:()=>({getSkyWebView:()=>bridge}),console});
async function main(){
 const original={name:'previous page API'},win={magelight:original};
 const shims=exportsObject.installSkyUltraCompetitorShims(win,bridge);
 const prisma=[],magelight=[];
 const offPrisma=win.prismaUI.RegisterJSListener('status',value=>prisma.push(value));
 const offMagelight=win.magelight.on('status',value=>magelight.push(value));
 await win.prismaUI.InteropCall('status',{hp:75});
 assert.equal(prisma[0],'{"hp":75}');assert.equal(magelight.length,0,'Prisma envelope must not reach Magelight listeners');
 await win.magelight.send('status',{hp:50});
 assert.equal(magelight[0].hp,50,'Custom send must reach on through the native interop envelope');
 assert.equal(prisma.length,1,'Magelight envelope must not reach Prisma listeners');
 emit('status',{hp:90});assert.equal(prisma[1],'{"hp":90}');assert.equal(magelight[1].hp,90);
 await win.magelight.send('game.getPlayerSnapshot',{});assert.equal(calls.at(-1).name,'game.getPlayerSnapshot');
 await win.magelight.vr.atGaze(1.8);assert.equal(calls.at(-1).name,'ui.setSpatialPlacement');assert.equal(calls.at(-1).params.distance,1.8);
 await win.magelight.playSound('ok');assert.equal(calls.at(-1).name,'ui.playSound');
 await win.magelight.close();assert.equal(calls.at(-1).name,'ui.close');
 win.prismaUI.SetOrder(7.2);assert.equal(win.prismaUI.GetOrder(),7);
 offPrisma();offMagelight();emit('status',{hp:1});assert.equal(prisma.length,2);assert.equal(magelight.length,2);
 shims.uninstall();assert.equal(win.magelight,original);assert.equal('prismaUI' in win,false);
 assert.ok([...listeners.values()].every(set=>set.size===0),'Uninstall must release both subscriptions');
 shims.uninstall();assert.equal(win.magelight,original);
 const second=exportsObject.installSkyUltraCompetitorShims(win,bridge),replacement={name:'newer page API'};
 win.magelight=replacement;second.uninstall();assert.equal(win.magelight,replacement,'Cleanup must preserve a later replacement');
 console.log('Legacy shim behavior passed: Prisma/Magelight routing, framework isolation, direct events, cleanup, global restoration, sound and spatial requests. No files emitted.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
