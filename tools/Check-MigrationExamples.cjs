// Execute the published sample and guide page in memory. No build/game launch.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const read=file=>fs.readFileSync(path.join(root,file),'utf8');
const flush=async()=>{for(let i=0;i<12;i++)await Promise.resolve();};
function page(html,handler){
 const elements=new Map();
 for(const tag of html.matchAll(/<\w+\b[^>]*\bid=["']([^"']+)["'][^>]*>/gi)){
  assert.ok(!elements.has(tag[1]),'Duplicate element id: '+tag[1]);
  elements.set(tag[1],Object.assign(new EventTarget(),{textContent:'',disabled:/\bdisabled\b/.test(tag[0])}));
 }
 const calls=[],listeners=new Map(),win=new EventTarget();
 const emit=(name,data)=>{for(const callback of [...(listeners.get(name)??[])])callback(data);};
 const bridge={
  on(name,callback){if(!listeners.has(name))listeners.set(name,new Set());listeners.get(name).add(callback);return()=>listeners.get(name).delete(callback);},
  async request(name,params,options){calls.push({name,params,options});return handler(name,params,options,emit,listeners);},
 };
 if(handler)win.SkyWebView=bridge;
 const context=vm.createContext({window:win,document:{getElementById(id){assert.ok(elements.has(id),'Unknown DOM id '+id);return elements.get(id);}},AbortController,console});
 const inline=[...html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)].filter(match=>! /\bsrc\s*=/.test(match[1]));
 assert.ok(html.indexOf('src="./SkyUltraBridge.js"')>=0,'Page must load the real bridge');
 assert.ok(html.indexOf('src="./SkyUltraBridge.js"')<inline[0].index,'Bridge must load before application code');
 return {win,calls,elements,listeners,emit,run(){for(const script of inline)new vm.Script(script[2]).runInContext(context);}};
}
function key(win,value){const event=Object.assign(new Event('keydown',{cancelable:true}),{key:value});win.dispatchEvent(event);return event;}
async function main(){
 const sample=read('samples/legacy-webview-port-sample/index.html');
 const manifest=JSON.parse(read('samples/legacy-webview-port-sample/manifest.json'));
 let resolveSubscription,resolveSnapshot;
 const live=page(sample,async(name,params,options,emit,listeners)=>{
  assert.ok(manifest.permissions.pageRequests.includes(name),'Undeclared sample method '+name);
  if(name==='ui.pageReady'){assert.equal(listeners.get('skyultra.game.update')?.size,1,'Subscribe before ready');return{ready:true};}
  if(name==='game.subscribe'){
   assert.equal(params.scope,'hud');assert.equal(JSON.stringify(params.topics),'["vitals"]');assert.equal(params.intervalMs,250);
   return new Promise(resolve=>{resolveSubscription=resolve;});
  }
  if(name==='game.getPlayerSnapshot'){assert.equal(params.scope,'hud');return new Promise(resolve=>{resolveSnapshot=resolve;});}
  if(name==='game.unsubscribe')return{subscribed:false};
  if(name==='ui.close')return{closed:true};
  throw Error('Unexpected request '+name);
 });
 live.run();await flush();
 const initial={playerName:'Lydia',level:20,health:{current:90,max:100},magicka:{current:40,max:50},stamina:{current:70,max:80}};
 live.emit('skyultra.game.update',{available:true,sequence:9,delta:initial});
 resolveSubscription({subscribed:true,update:{available:true,sequence:8,delta:{...initial,health:{current:1,max:100}}}});
 await flush();assert.equal(live.elements.get('hp').textContent,'90 / 100','Late initial reply must not regress the event');
 live.emit('skyultra.game.update',{available:true,sequence:10,delta:{magicka:{current:0,max:50}}});
 assert.equal(live.elements.get('hp').textContent,'90 / 100');assert.equal(live.elements.get('mp').textContent,'0 / 50');
 live.emit('skyultra.game.update',{available:false,sequence:11,delta:{}});
 assert.match(live.elements.get('title').textContent,/Waiting/);assert.equal(live.elements.get('hp').textContent,'—');
 live.emit('skyultra.game.update',{available:true,sequence:12,delta:initial});
 live.elements.get('refreshBtn').dispatchEvent(new Event('click'));await flush();
 live.emit('skyultra.game.update',{available:true,sequence:14,delta:{health:{current:80,max:100}}});
 resolveSnapshot({...initial,available:true,sequence:13,health:{current:2,max:100}});await flush();
 assert.equal(live.elements.get('hp').textContent,'80 / 100','Stale manual snapshot must not replace a newer event');
 live.elements.get('closeBtn').dispatchEvent(new Event('click'));await flush();
 assert.equal(live.calls.at(-1).name,'ui.close');assert.equal(key(live.win,'Escape').defaultPrevented,true);await flush();
 live.win.dispatchEvent(new Event('pagehide'));await flush();
 assert.equal(live.listeners.get('skyultra.game.update').size,0);
 assert.equal(live.calls.at(-1).name,'game.unsubscribe');
  for(const method of manifest.permissions.pageRequests)assert.ok(live.calls.some(call=>call.name===method),'Unused sample permission '+method);
 assert.ok(live.calls.filter(call=>call.options?.signal).every(call=>call.options.signal.aborted),'Teardown must abort pending reads');
 const previousTitle=live.elements.get('title').textContent,previousCount=live.calls.length;
 live.emit('skyultra.game.update',{available:true,sequence:99,delta:{playerName:'Late event'}});
 key(live.win,'Escape');assert.equal(live.calls.length,previousCount);assert.equal(live.elements.get('title').textContent,previousTitle);
 const preview=page(sample);preview.run();await flush();assert.equal(preview.elements.get('title').textContent,'Browser Preview Mode');
 const denied=page(sample,async name=>{if(name==='game.subscribe')throw Error('Permission denied');return{ready:true};});
 denied.run();await flush();assert.match(denied.elements.get('subtitle').textContent,/Permission denied/);denied.win.dispatchEvent(new Event('pagehide'));

 // Match the guide's copyable page/manifest to the actual native sample source.
 const guide=read('docs/PRISMAUI_MIGRATION.md');
 const wireHtml=guide.match(/```html\s*\n([\s\S]*?)```/)[1];
 const wireManifest=JSON.parse(guide.match(/```json\s*\n([\s\S]*?)```/)[1]);
 const cpp=read('api/examples/MinimalSksePlugin.cpp');
 assert.ok(cpp.includes('RegisterMod("'+wireManifest.id+'"'));
 for(const name of wireManifest.permissions.pageRequests)assert.ok(cpp.includes('"'+name+'"'),'Native sample must grant '+name);
 const nativeReply=JSON.parse(cpp.match(/CompletePageRequestOk\(sender,request_id,R"\(([\s\S]*?)\)"/)[1]);
 const nativeEvent=JSON.parse(cpp.match(/SendEventJson\(sender,"sample.status",R"\(([\s\S]*?)\)"/)[1]);
 const wire=page(wireHtml,async(name,params,options,emit,listeners)=>{
  assert.ok(wireManifest.permissions.pageRequests.includes(name));
  assert.equal(listeners.get('sample.status')?.size,1);
  if(name==='ui.pageReady')return{ready:true};
  if(name==='sample.ping'){emit('sample.status',nativeEvent);return nativeReply;}
 });
 wire.run();await flush();
 assert.equal(wire.elements.get('reply').textContent,nativeReply.message);
 assert.equal(wire.elements.get('event').textContent,'Native event received.');assert.equal(wire.elements.get('ping').disabled,false);
 wire.elements.get('ping').dispatchEvent(new Event('click'));await flush();assert.equal(wire.calls.filter(call=>call.name==='sample.ping').length,2);
 wire.win.dispatchEvent(new Event('pagehide'));assert.equal(wire.listeners.get('sample.status').size,0);
 const failed=page(wireHtml,async name=>{if(name==='sample.ping')throw Error('Native owner unavailable');return{ready:true};});
 failed.run();await flush();assert.match(failed.elements.get('reply').textContent,/Native owner unavailable/);
 const missing=page(wireHtml);assert.throws(()=>missing.run(),/Bridge unavailable/);assert.match(missing.elements.get('reply').textContent,/Bridge unavailable/);
 console.log('Migration examples passed: bridge load order, declared methods, initial/delta data, stale replies, unavailable state, close, teardown, request failures, and documented native reply/event contract. No files emitted.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
