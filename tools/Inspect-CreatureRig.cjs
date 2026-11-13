// Read-only NIF/HKX bind-pose inspection. No extracted game assets are written.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const archiveSource=fs.readFileSync(path.join(__dirname,'Inspect-ActorPoseAssets.cjs'),'utf8').split('const filename=process.argv[2]')[0];
const context={require,Buffer};vm.runInNewContext(archiveSource+';this.entries=entries;this.asset=asset;',context);
const data=process.argv[2];if(!data)throw Error('Pass the Skyrim Data directory.');
function load(archive,suffix){
 const fd=fs.openSync(path.join(data,archive),'r');
 try{
  const h=Buffer.alloc(36);fs.readSync(fd,h,0,36,0);
  const length=36+h.readUInt32LE(16)*25+h.readUInt32LE(20)*16+h.readUInt32LE(24)+h.readUInt32LE(28);
  const catalog=Buffer.alloc(length);fs.readSync(fd,catalog,0,length,0);
  const entry=context.entries(catalog).find(entry=>entry.path.toLowerCase().endsWith(suffix));assert.ok(entry,suffix);
  const packed=Buffer.alloc(16+(entry.size&0x3fffffff));h.copy(packed,0,0,16);
  fs.readSync(fd,packed,16,packed.length-16,entry.offset);
  return context.asset(packed,{offset:16,size:entry.size});
 }finally{fs.closeSync(fd);}
}
function nif(bytes){
 let p=bytes.indexOf(10)+1;
 const u=()=>{const value=bytes.readUInt32LE(p);p+=4;return value;};
 const string=()=>{const count=u(),value=bytes.toString('utf8',p,p+count);p+=count;return value;};
 const exportString=()=>{const count=bytes[p++];p+=count;};
 assert.equal(u(),0x14020007);p++;assert.equal(u(),12);const count=u();assert.equal(u(),100);
 exportString();exportString();exportString();
 const typeCount=bytes.readUInt16LE(p);p+=2;
 const types=Array.from({length:typeCount},string),ids=Array.from({length:count},()=>{const value=bytes.readUInt16LE(p);p+=2;return value;});
 const sizes=Array.from({length:count},u),stringCount=u();u();
 const strings=Array.from({length:stringCount},string),groups=u();p+=groups*4;
 const blocks=sizes.map((size,i)=>{const block={type:types[ids[i]],data:bytes.subarray(p,p+size)};p+=size;return block;});
 assert.equal(p,bytes.length-8);return{blocks,strings};
}
function rig(bytes){
 const u=p=>bytes.readUInt32LE(p),text=p=>bytes.toString('utf8',p,bytes.indexOf(0,p));
 const sections=Array.from({length:u(20)},(_,i)=>{const p=64+i*48;return{start:u(p+20),local:u(p+24),global:u(p+28),virtual:u(p+32),exports:u(p+36)};});
 const pointers=new Map(),objects=[];
 for(const s of sections){
  for(let p=s.start+s.local;p+8<=s.start+s.global;p+=8)if(u(p)!==0xffffffff)pointers.set(s.start+u(p),s.start+u(p+4));
  for(let p=s.start+s.global;p+12<=s.start+s.virtual;p+=12)if(u(p)!==0xffffffff)pointers.set(s.start+u(p),sections[u(p+4)].start+u(p+8));
  for(let p=s.start+s.virtual;p+12<=s.start+s.exports;p+=12)if(u(p)!==0xffffffff)objects.push({offset:s.start+u(p),type:text(sections[u(p+4)].start+u(p+8))});
 }
 const skeleton=objects.find(object=>object.type==='hkaSkeleton').offset,count=u(skeleton+48);
 const bones=pointers.get(skeleton+40),poses=pointers.get(skeleton+56),parents=pointers.get(skeleton+24);
 assert.equal(count,u(skeleton+64));
 return Array.from({length:count},(_,i)=>({name:text(pointers.get(bones+i*16)),parent:bytes.readInt16LE(parents+i*2),pose:Array.from({length:12},(_,j)=>bytes.readFloatLE(poses+i*48+j*4))}));
}
const multiply=(a,b)=>Array.from({length:16},(_,i)=>[0,1,2,3].reduce((v,k)=>v+a[(i>>2)*4+k]*b[k*4+(i%4)],0));
function inverse(a){
 const b=[a[0],a[4],a[8],0,a[1],a[5],a[9],0,a[2],a[6],a[10],0,0,0,0,1],scale=a[0]**2+a[1]**2+a[2]**2;
 for(let i=0;i<12;i++)b[i]/=scale;
 for(let j=0;j<3;j++)b[12+j]=-[0,1,2].reduce((v,k)=>v+a[12+k]*b[k*4+j],0);
 return b;
}
function nifMatrix(bytes,p){
 const f=Array.from({length:13},(_,i)=>bytes.readFloatLE(p+i*4)),s=f[12];
 return[f[0]*s,f[3]*s,f[6]*s,0,f[1]*s,f[4]*s,f[7]*s,0,f[2]*s,f[5]*s,f[8]*s,0,f[9],f[10],f[11],1];
}
function localMatrix(f,units){
 const length=Math.hypot(...f.slice(4,8)),[x,y,z,w]=f.slice(4,8).map(value=>value/length),[sx,sy,sz]=f.slice(8,11);
 return[(1-2*y*y-2*z*z)*sx,(2*x*y+2*z*w)*sx,(2*x*z-2*y*w)*sx,0,
  (2*x*y-2*z*w)*sy,(1-2*x*x-2*z*z)*sy,(2*y*z+2*x*w)*sy,0,
  (2*x*z+2*y*w)*sz,(2*y*z-2*x*w)*sz,(1-2*x*x-2*y*y)*sz,0,f[0]*units,f[1]*units,f[2]*units,1];
}
const distance=(a,b)=>Math.hypot(...[0,1,2].map(i=>a[12+i]-b[12+i]));
const bones=rig(load('Skyrim - Animations.bsa','actors\\giant\\character assets\\skeleton.hkx'));
function modelPose(units){const result=[];for(const bone of bones){const local=localMatrix(bone.pose,units);result.push(bone.parent>=0?multiply(local,result[bone.parent]):local);}return result;}
const reference=modelPose(1);
for(const filename of ['giant.nif','giant01.nif','giant02.nif']){
 const mesh=nif(load('Skyrim - Meshes0.bsa','actors\\giant\\character assets\\'+filename)),bind=new Map(),counts=new Map(),skins=[];
 for(const block of mesh.blocks){
  if(!['NiSkinInstance','BSDismemberSkinInstance'].includes(block.type))continue;
  const b=block.data,count=b.readUInt32LE(12),skin=mesh.blocks[b.readUInt32LE(0)].data,root=nifMatrix(skin,0),weights=new Map();let p=57;
  for(let i=0;i<count;i++){
   const node=mesh.blocks[b.readUInt32LE(16+i*4)],name=mesh.strings[node.data.readUInt32LE(0)];
   const matrix=inverse(multiply(root,nifMatrix(skin,p))),vertices=skin.readUInt16LE(p+68);
   if(skin[56])for(let j=0;j<vertices;j++){
    const vertex=skin.readUInt16LE(p+70+j*6),weight=skin.readFloatLE(p+72+j*6);
    assert.ok(weight>0 && weight<=1);weights.set(vertex,(weights.get(vertex)??0)+weight);
   }
   p+=70+(skin[56]?vertices*6:0);
   if(!counts.has(name)||counts.get(name)<count){bind.set(name,matrix);counts.set(name,count);}
  }
  assert.ok(weights.size>0);assert.equal(Math.max(...weights.keys())+1,weights.size,'The explicit global weight stream must cover every vertex');
  for(const sum of weights.values())assert.ok(Math.abs(sum-1)<.001,'Weights must sum to one');
  skins.push({bones:count,weightedVertices:weights.size});
 }
 const ratios=[];
 bones.forEach((bone,i)=>{if(bone.parent<0||!bind.has(bone.name)||!bind.has(bones[bone.parent].name))return;
  const a=distance(reference[i],reference[bone.parent]),b=distance(bind.get(bone.name),bind.get(bones[bone.parent].name));if(a>.0001&&b>.0001)ratios.push(b/a);});
 ratios.sort((a,b)=>a-b);assert.ok(ratios.length>=3);
 const units=ratios[Math.floor(ratios.length/2)],world=modelPose(units),anchor=bones.findIndex(bone=>bind.has(bone.name));
 const alignment=multiply(inverse(world[anchor]),bind.get(bones[anchor].name));
 const errors=bones.flatMap((bone,i)=>bind.has(bone.name)?[{name:bone.name,error:distance(bind.get(bone.name),multiply(world[i],alignment))}]:[]).sort((a,b)=>b.error-a.error);
 assert.ok(Math.abs(units-1)<.001);assert.ok(errors[0].error<.001,'The NIF bind space and HKX reference rig must agree');
 console.log(JSON.stringify({filename,bones:bones.length,mapped:errors.length,units,skins,largestBindError:errors[0].error}));
}
console.log('Three installed giant meshes: compatible reference rigs and complete explicit bone weights. No files written.');
