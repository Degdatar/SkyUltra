// Documentation/source checks only. No compilation, bundling or installation.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const files=cp.execFileSync('rg',['--files','docs','api','sdk','apps','samples','-g','*.md','-g','!**/node_modules/**','-g','!**/dist/**','-g','!docs/licenses/**','-g','!**/templates/**'],{cwd:root,encoding:'utf8'}).trim().split(/\r?\n/);
files.push('README.md');
const compatibility={};
vm.runInNewContext(ts.transpileModule(fs.readFileSync(path.join(root,'sdk/typescript/src/compatibility.ts'),'utf8'),{
 compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022},
}).outputText,{exports:compatibility,require:()=>({}),console});
const sdkEntry=path.join(root,'sdk/typescript/src/index.ts');
const sdkProgram=ts.createProgram([sdkEntry],{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ESNext,moduleResolution:ts.ModuleResolutionKind.Bundler,skipLibCheck:true,noEmit:true});
const sdkChecker=sdkProgram.getTypeChecker();
const sdkExports=new Set(sdkChecker.getExportsOfModule(sdkChecker.getSymbolAtLocation(sdkProgram.getSourceFile(sdkEntry))).map(symbol=>symbol.name));
const migrationDocs=new Set(['README.md','docs/PRISMAUI_MIGRATION.md','docs/MAGELIGHT_UI_MIGRATION.md','docs/MIGRATION_GUIDE.md','samples/legacy-webview-port-sample/README.md']);
function anchors(file){
 const text=fs.readFileSync(file,'utf8').replace(/```[\s\S]*?```/g,'');
 const ids=new Set(),counts=new Map();
 for(const match of text.matchAll(/^#{1,6}\s+(.+)$/gm)){
  const base=match[1].trim().replace(/\s+#+$/,'').toLowerCase().replace(/[^\p{L}\p{N}_\-\s]/gu,'').replace(/\s/g,'-');
  const count=counts.get(base)??0;counts.set(base,count+1);ids.add(base+(count?'-'+count:''));
 }
 for(const match of text.matchAll(/\bid=["']([^"']+)["']/g))ids.add(match[1]);
 return ids;
}
let links=0,manifests=0,snippets=0,anchorChecks=0,importChecks=0;const problems=[];
function checkScript(file,code,language){
 const fileName='guide.'+(language==='tsx'?'tsx':'ts');
 const parsed=ts.createSourceFile(fileName,code,ts.ScriptTarget.ES2022,true,language==='tsx'?ts.ScriptKind.TSX:ts.ScriptKind.TS);
 for(const node of parsed.statements)if(ts.isImportDeclaration(node)&&node.moduleSpecifier.text==='@skyultra/sdk'){
  const named=node.importClause?.namedBindings;
  if(named&&ts.isNamedImports(named))for(const element of named.elements){
   const name=(element.propertyName??element.name).text;importChecks++;
   if(!sdkExports.has(name))problems.push(`${file}: SDK does not export ${name}`);
  }
 }
 const result=ts.transpileModule(code,{fileName,reportDiagnostics:true,
  compilerOptions:{module:ts.ModuleKind.ESNext,target:ts.ScriptTarget.ES2022,jsx:ts.JsxEmit.ReactJSX}});
 for(const error of result.diagnostics??[])if(error.category===ts.DiagnosticCategory.Error)problems.push(`${file}: ${ts.flattenDiagnosticMessageText(error.messageText,' ')}`);
 snippets++;
}
for(const file of files){const text=fs.readFileSync(path.join(root,file),'utf8');
 for(const match of text.matchAll(/!?\[[^\]]*\]\(([^)]+)\)/g)){
  const fullTarget=match[1].split(/\s+"/)[0].replace(/^<|>$/g,'');
  if(!fullTarget||/^(?:https?:|mailto:|data:)/.test(fullTarget)||fullTarget.includes('__'))continue;
  const [targetText,fragment]=fullTarget.split('#');
  const target=decodeURIComponent(targetText);
  const resolved=target?path.resolve(root,path.dirname(file),target):path.join(root,file);
  if(!fs.existsSync(resolved))problems.push(`${file}: missing link ${target}`);
  else if(fragment&&migrationDocs.has(file.replace(/\\/g,'/'))&&path.extname(resolved)==='.md'){
   anchorChecks++;
   if(!anchors(resolved).has(decodeURIComponent(fragment)))problems.push(`${file}: missing anchor ${fullTarget}`);
  }
  links++;
 }
 for(const match of text.matchAll(/```(json|ts|tsx|js|javascript|html)\s*\n([\s\S]*?)```/g)){
  const [,language,code]=match;
  if(language==='json'){
   try{const value=JSON.parse(code);if(value.schemaVersion===1&&value.entry){const result=compatibility.auditSkyUltraManifestObject(value);assert.equal(result.valid,true,JSON.stringify(result));manifests++;}}
   catch(error){problems.push(`${file}: invalid JSON/manifest: ${error.message}`);}
  }else if(language==='html'){
   for(const script of code.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi))if(!/\bsrc\s*=|application\/json/i.test(script[1])&&script[2].trim())checkScript(file,script[2],'js');
  }else checkScript(file,code,language);
 }
}
if(problems.length)throw Error(problems.join('\n'));
console.log(`${files.length} Markdown files, ${links} local links, ${anchorChecks} migration anchors, ${manifests} schema-v1 manifests, ${snippets} script snippets and ${importChecks} SDK import names passed. No files emitted.`);
