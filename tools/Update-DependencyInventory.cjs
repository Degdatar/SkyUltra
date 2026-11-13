// Read declared dependencies and committed npm locks; never install or build.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const root=path.resolve(__dirname,'..');
const files=cp.execFileSync('rg',['--files','sdk','apps','samples','-g','package.json','-g','package-lock.json','-g','!**/node_modules/**','-g','!**/dist/**'],{cwd:root,encoding:'utf8'}).trim().split(/\r?\n/).map(file=>file.replaceAll('\\','/')).sort();
const declared=[],locked=new Map();
for(const file of files){
 const json=JSON.parse(fs.readFileSync(path.join(root,file),'utf8'));
 if(file.endsWith('package-lock.json')){
  for(const [location,pkg] of Object.entries(json.packages??{})){
   if(!location.includes('node_modules/') || pkg.link)continue;
   const name=pkg.name??location.slice(location.lastIndexOf('node_modules/')+13);
   if(name.startsWith('@skyultra/') || !pkg.version)continue;
   const key=name+'@'+pkg.version;
   if(!locked.has(key))locked.set(key,{name,version:pkg.version,licenses:[],lockfiles:[]});
   const entry=locked.get(key);
   if(pkg.license && !entry.licenses.includes(pkg.license))entry.licenses.push(pkg.license);
   if(!entry.lockfiles.includes(file))entry.lockfiles.push(file);
  }
 }else for(const kind of ['dependencies','devDependencies','peerDependencies'])
  for(const [name,version] of Object.entries(json[kind]??{}))if(!name.startsWith('@skyultra/'))declared.push({name,version,kind,manifest:file});
}
const manifest=JSON.parse(fs.readFileSync(path.join(root,'vcpkg.json'),'utf8'));
const native=manifest.dependencies.map(dep=>typeof dep==='string'?{name:dep}:{name:dep.name,host:!!dep.host});
const document={scope:'Declared project dependencies and all non-local packages in checked-in npm lockfiles. Lockfiles include build tools and platform-specific optional packages; this is not a runtime-bundle inventory.',sources:['vcpkg.json',...files],native,npmDeclared:declared.sort((a,b)=>a.name.localeCompare(b.name)||a.manifest.localeCompare(b.manifest)||a.kind.localeCompare(b.kind)),npmLocked:[...locked.values()].sort((a,b)=>a.name.localeCompare(b.name)||a.version.localeCompare(b.version))};
const output=path.join(root,'docs/dependencies/inventory.json'),text=JSON.stringify(document,null,2)+'\n';
if(process.argv.includes('--check')){if(!fs.existsSync(output)||fs.readFileSync(output,'utf8')!==text)throw Error('Dependency inventory is stale. Run node tools/Update-DependencyInventory.cjs.');}
else{fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,text);}
console.log(`${native.length} native declarations, ${new Set(declared.map(d=>d.name)).size} direct npm packages, ${locked.size} locked npm name/version pairs; no build or install.`);
