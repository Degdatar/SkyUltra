import {copyFile,readFile,appendFile} from "node:fs/promises";
await Promise.all(["menu.css","hud.css","chrome.css","controls.css"].map(name=>copyFile("src/"+name,"dist/"+name)));
// The standard stylesheet includes the controls; the standalone entry remains
// available to authors who supply their own theme and minimal CSS bundle.
await appendFile("dist/styles.css", "\n"+await readFile("src/controls.css","utf8"));
