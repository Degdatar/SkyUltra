// Executes the production shared dispatcher and page bridge in memory.
// Native queue admission/FIFO tests are in BridgeEventQueueTests.cpp.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),read=p=>fs.readFileSync(path.join(root,p),'utf8');
const ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const header=read('include/SkyUltra/BridgeBatchScript.h');
const dispatcher=header.match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const events=new Map(),requests=[],acks=[],errors=[],timers=new Map();let nextTimer=0;
const win={location:{search:'?__skyultra_engine=cef&__skyultra_visible=0'},
 addEventListener(name,fn){if(!events.has(name))events.set(name,new Set());events.get(name).add(fn);},
 dispatchEvent(event){for(const fn of [...(events.get(event.type)??[])])fn(event);return true;}};
class CustomEvent{constructor(type,options){this.type=type;this.detail=options.detail;}}
const context=vm.createContext({window:win,CustomEvent,AbortController,performance,
 console:{debug:text=>{if(text.startsWith('__SKYULTRA_BRIDGE__'))requests.push(JSON.parse(text.slice('__SKYULTRA_BRIDGE__'.length)));},log:text=>acks.push(text),error:(...args)=>errors.push(args)},
 setTimeout:fn=>{timers.set(++nextTimer,fn);return nextTimer;},clearTimeout:id=>timers.delete(id)});
new vm.Script(read('resources/SkyUltraBridge.js')).runInContext(context);
const dispatch=new vm.Script('('+dispatcher+')').runInContext(context);
async function main(){
 const api=win.SkyWebView,received=[];
 const off=api.on('skyos.invokeChunk',payload=>received.push(payload.index));
 for(let first=0;first<1024;first+=8)dispatch(Array.from({length:8},(_,offset)=>['skyos.invokeChunk',JSON.stringify({index:first+offset})]),'ack-'+first,false);
 assert.deepEqual(received,Array.from({length:1024},(_,index)=>index));
 assert.equal(acks.length,128,'One private acknowledgement per batch');
 assert.equal(api.getVisibility().visible,false);
 dispatch([['skyultra.visibility','{"visible":true}']],'visible',false);
 assert.equal(api.getVisibility().visible,true);
 let unicode;
 const stopUnicode=api.on('unicode',payload=>{unicode=payload;});
 dispatch([['invalid','{'],['unicode',JSON.stringify({text:'quote " newline\nالعربية 🌟\u2028\u2029'})]],'after-invalid',false);
 assert.match(unicode.text,/العربية/);assert.equal(acks.at(-1),'after-invalid');
 assert.equal(errors.length,1,'Malformed payload is isolated and acknowledged');
 const stopError=api.on('throwing',()=>{throw Error('App callback failed');});
 dispatch([['throwing','{}'],['skyos.invokeChunk','{"index":1024}']],'after-callback',false);
 assert.equal(received.at(-1),1024);assert.equal(acks.at(-1),'after-callback');
 const reply=api.request('test.reply',{value:1});const request=requests.at(-1);
 dispatch([[request.id,JSON.stringify({id:request.id,ok:true,data:{accepted:true}})]],'reply-ack',true);
 assert.equal((await reply).accepted,true);
 const failure=api.request('test.failure',{}),failed=requests.at(-1);
 const rejection=assert.rejects(failure,/Native denied/);
 dispatch([[failed.id,JSON.stringify({id:failed.id,ok:false,error:'Native denied'})]],'failure-ack',true);
 await rejection;
 const controller=new AbortController();
 const cancelled=api.request('test.cancel',{}, {signal:controller.signal});
 const cancellation=assert.rejects(cancelled,error=>error.name==='AbortError');
 controller.abort();await cancellation;assert.equal(requests.at(-1).kind,'cancel');
 assert.equal(timers.size,0,'Replies and cancellation release request timers');
 const nativeAcks=[];
 win.__skyultraAcknowledgeBatch=token=>{nativeAcks.push(token);return true;};
 const originalLog=context.console.log;context.console.log=()=>{throw Error('App replaced console.log');};
 dispatch([['skyos.invokeChunk','{"index":1025}']],'native-ack',false);
 assert.equal(received.at(-1),1025);assert.deepEqual(nativeAcks,['native-ack']);
 delete win.__skyultraAcknowledgeBatch;context.console.log=originalLog;
 off();stopUnicode();stopError();

 const exports={};
 vm.runInNewContext(ts.transpileModule(read('sdk/typescript/src/devtools.ts'),{
  compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022}}).outputText,{exports,require:()=>({}),console});
 const mod={id:'tests.bridge',name:'Bridge test',version:'1.0.0',folderName:'tests.bridge',registered:true,manifestValid:true,
  entry:'Files/index.html',entryExists:true,hudExists:false,assetCount:1,missingAssets:[],assetAccess:true,
  pageRequests:['ui.pageReady'],bridgeErrorCount:0,deniedPermissionCount:0};
 for(const bytes of [25*1024*1024+1,1024*1024*1024])assert.equal(exports.diagnoseSkyUltraModRecord({...mod,totalAssetBytes:bytes}).some(f=>f.code==='LARGE_ASSET_FOOTPRINT'),false);
 assert.ok(exports.diagnoseSkyUltraModRecord({...mod,totalAssetBytes:1024*1024*1024+1}).some(f=>f.code==='LARGE_ASSET_FOOTPRINT'));
 assert.ok(!fs.existsSync(path.join(root,'resources/SkyUltra.ini')));
 assert.ok(!/GetPrivateProfile|WritePrivateProfile/.test(read('src/skse/Plugin.cpp')),'Settings must use the JSON system');
 assert.ok(!read('CMakeLists.txt').includes('resources/SkyUltra.ini'),'INI is no longer a package input');
 assert.ok(read('src/ultralight/UltralightBackend.cpp').includes('microseconds(8333)'));
 assert.ok(read('src/cef/CefBackend.cpp').includes('CurrentDisplayRefreshRate()'));
 assert.ok(read('src/cef/CefBackend.cpp').includes('kMaxCefWindowlessFrameRate = 120;'));
 assert.ok(!read('src/cef/CefBackend.cpp').includes("1'000'000"));
 console.log('Bridge checks passed: 1,024 ordered chunks, batch acknowledgements, visibility, Unicode, error isolation, replies/cancellation, 1 GB warning boundary, JSON-only configuration, and restored frame pacing with CEF capped at 120 FPS. No files emitted.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
