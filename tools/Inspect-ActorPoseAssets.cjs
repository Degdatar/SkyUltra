// Read-only metadata inspection of the installed vanilla animation archive.
// No game assets are extracted, changed, or copied into a release package.
const fs=require('node:fs');
const assert=require('node:assert/strict');
function entries(buffer){
 const u=offset=>buffer.readUInt32LE(offset);
 assert.equal(buffer.toString('ascii',0,4),'BSA\0');assert.equal(u(4),105);
 const offset=u(8),folders=u(16);let position=offset+folders*24;const result=[];
 for(let i=0;i<folders;i++){
  const count=u(offset+i*24+8),length=buffer[position++];
  const folder=buffer.toString('utf8',position,position+length).replace(/\0+$/,'');position+=length;
  for(let j=0;j<count;j++){result.push({folder,size:u(position+8),offset:u(position+12)});position+=16;}
 }
 for(const entry of result){const end=buffer.indexOf(0,position);assert.ok(end>=position);entry.path=entry.folder+'\\'+buffer.toString('utf8',position,end);position=end+1;}
 return result;
}
function asset(buffer,entry){
 let offset=entry.offset,size=entry.size&0x3fffffff;
 const compressed=Boolean(buffer.readUInt32LE(12)&4)!==Boolean(entry.size&0x40000000);
 if(!compressed)return buffer.subarray(offset,offset+size);
 const expected=buffer.readUInt32LE(offset);assert.ok(expected>0&&expected<=16*1024*1024);
 const input=buffer.subarray(offset+4,offset+size),output=Buffer.alloc(expected);
 assert.equal(input.readUInt32LE(0),0x184d2204);assert.equal(input[4],0x60); // vanilla independent LZ4 blocks
 let p=7,o=0;
 for(;;){
  let length=input.readUInt32LE(p);p+=4;if(!length)break;
  const plain=Boolean(length&0x80000000);length&=0x7fffffff;const end=p+length;
  assert.ok(end<=input.length);
  if(plain){assert.ok(o+length<=output.length);input.copy(output,o,p,end);o+=length;p=end;continue;}
  while(p<end){
   const token=input[p++];let literals=token>>>4;
   if(literals===15){let extra;do{assert.ok(p<end);extra=input[p++];literals+=extra;}while(extra===255);}
   assert.ok(p+literals<=end&&o+literals<=output.length);input.copy(output,o,p,p+literals);p+=literals;o+=literals;
   if(p===end)break;
   const distance=input.readUInt16LE(p);p+=2;assert.ok(distance>0&&distance<=o);
   let match=(token&15)+4;
   if((token&15)===15){let extra;do{assert.ok(p<end);extra=input[p++];match+=extra;}while(extra===255);}
   assert.ok(o+match<=output.length);
   for(let i=0;i<match;i++)output[o+i]=output[o+i-distance];o+=match;
  }
  assert.equal(p,end);
 }
 assert.equal(o,expected);return output;
}
function metadata(bytes){
 const u=offset=>bytes.readUInt32LE(offset),i=offset=>bytes.readInt32LE(offset);
 assert.equal(u(0),0x57e0e057);assert.equal(u(4),0x10c0c010);assert.equal(u(12),8);
 assert.deepEqual([...bytes.subarray(16,20)],[8,1,0,1]);
 const sections=Array.from({length:u(20)},(_,n)=>{const p=64+n*48;return {start:u(p+20),local:u(p+24),global:u(p+28),virtual:u(p+32),exports:u(p+36),end:u(p+44)};});
 const pointers=new Map(),objects=[];
 const text=p=>bytes.toString('utf8',p,bytes.indexOf(0,p));
 for(const s of sections){
  for(let p=s.start+s.local;p+8<=s.start+s.global;p+=8)if(u(p)!==0xffffffff)pointers.set(s.start+u(p),s.start+u(p+4));
  for(let p=s.start+s.global;p+12<=s.start+s.virtual;p+=12)if(u(p)!==0xffffffff)pointers.set(s.start+u(p),sections[u(p+4)].start+u(p+8));
  for(let p=s.start+s.virtual;p+12<=s.start+s.exports;p+=12)if(u(p)!==0xffffffff){assert.equal(u(p)%16,0);objects.push({offset:s.start+u(p),type:text(sections[u(p+4)].start+u(p+8))});}
 }
 const spline=objects.find(object=>object.type==='hkaSplineCompressedAnimation').offset;
 const binding=objects.find(object=>object.type==='hkaAnimationBinding').offset;
 return {bytes:bytes.length,classes:objects.map(object=>object.type),duration:bytes.readFloatLE(spline+20),tracks:i(spline+24),floats:i(spline+28),
  frames:i(spline+56),blocks:i(spline+60),maxFrames:i(spline+64),maskBytes:i(spline+68),blockDuration:bytes.readFloatLE(spline+72),endian:i(spline+168),
  frameDuration:bytes.readFloatLE(spline+80),arrays:[88,104,120,136,152].map(field=>({field,count:i(spline+field+8),target:pointers.get(spline+field)})),
  skeleton:pointers.has(binding+16)?text(pointers.get(binding+16)):'',mapCount:i(binding+40),
  map:i(binding+40)>0?Array.from({length:i(binding+40)},(_,n)=>bytes.readInt16LE(pointers.get(binding+32)+n*2)):[],blend:bytes[binding+64]};
}
const filename=process.argv[2];if(!filename)throw Error('Pass the path to Skyrim - Animations.bsa');
const bytes=fs.readFileSync(filename);
if(filename.toLowerCase().endsWith('.hkx')){
 console.log(JSON.stringify({path:filename,...metadata(bytes)}));
 console.log('1 loose pose asset inspected; no files written.');
}else{
const catalog=entries(bytes);
const wanted=['male\\mt_idle.hkx','female\\mt_idle.hkx','h2h_idle.hkx','1hm_blockidle.hkx','mrh_aimedconcentration.hkx'];
for(const suffix of wanted){
 const name='meshes\\actors\\character\\animations\\'+suffix;
 const entry=catalog.find(entry=>entry.path.toLowerCase()===name);assert.ok(entry,name+' must exist');
 const info=metadata(asset(bytes,entry));
 assert.equal(info.tracks,99);assert.equal(info.blend,0);assert.equal(info.endian,0);assert.equal(info.mapCount,0);assert.equal(info.skeleton,'NPC Root [Root]');
 assert.ok(info.duration>0&&info.duration<=120);assert.equal(info.blocks,Math.ceil((info.frames-1)/(info.maxFrames-1)));
 assert.equal(info.arrays[0].count,info.blocks);assert.equal(info.arrays[1].count,info.blocks);assert.ok(info.maskBytes>=info.tracks*4+info.floats);
 console.log(JSON.stringify({path:name,...info}));
}
console.log('5 installed pose assets inspected; no files written.');
}
