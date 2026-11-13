import {createRoot} from "react-dom/client";
import {SkyGameplayHud} from "@skyultra/ui/gameplay";
import "@skyultra/ui/styles.css";
import "@skyultra/ui/hud.css";
const root=document.getElementById("root");
if(!root)throw new Error("Missing HUD root");
createRoot(root).render(<SkyGameplayHud/>);

