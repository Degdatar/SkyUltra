// Read-only TypeScript validation. No emission, dependency installation, bundling or game access.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..');
const ts=require(path.join(root,'sdk/ui/node_modules/typescript'));
const paths={
 '@skyultra/sdk':[path.join(root,'sdk/typescript/src/index.ts')],
 '@skyultra/ui':[path.join(root,'sdk/ui/src/index.ts')],
 '@skyultra/ui/field/*':[path.join(root,'sdk/ui/src/field/*')],
 '@skyultra/ui/*':[path.join(root,'sdk/ui/src/*')],
 'react':[path.join(root,'sdk/ui/node_modules/@types/react/index.d.ts')],
 'react/jsx-runtime':[path.join(root,'sdk/ui/node_modules/@types/react/jsx-runtime.d.ts')],
 'react-dom/client':[path.join(root,'sdk/ui/node_modules/@types/react-dom/client.d.ts')],
 'vite/client':[path.join(root,'samples/ui-kit-test-mod/node_modules/vite/client.d.ts')],
};
let count=0;
const supported=['sdk/typescript','sdk/react','sdk/ui','apps/skyultra-menu','samples/ui-kit-test-mod'];
const projects=process.argv.length>2?process.argv.slice(2):supported;
for(const project of projects){
 if(!supported.includes(project))throw Error('Unknown frontend project: '+project);
 const filename=path.join(root,project,'tsconfig.json');
 const json=ts.readConfigFile(filename,ts.sys.readFile);if(json.error)throw Error('Cannot read '+filename);
 const parsed=ts.parseJsonConfigFileContent(json.config,ts.sys,path.dirname(filename));
 const options={...parsed.options,noEmit:true,rootDir:root,baseUrl:root,paths,typeRoots:[path.join(root,'sdk/ui/node_modules/@types')]};
 const files=project==='apps/skyultra-menu'?[...parsed.fileNames,path.join(root,'samples/ui-kit-test-mod/node_modules/vite/client.d.ts')]:parsed.fileNames;
 const program=ts.createProgram(files,options);
 const diagnostics=ts.getPreEmitDiagnostics(program);
 console.log(project+': '+diagnostics.length+' diagnostic(s)');
 for(const d of diagnostics){console.log(ts.flattenDiagnosticMessageText(d.messageText,'\n'));if(d.file){const lc=d.file.getLineAndCharacterOfPosition(d.start??0);console.log(path.relative(root,d.file.fileName)+':'+(lc.line+1)+':'+(lc.character+1));}}
 count+=diagnostics.length;
}
process.exitCode=count?1:0;
