import "./style.css";

const runtimeState = document.querySelector("#runtime-state");
const runtimeDetails = document.querySelector("#runtime-details");
const runtime = globalThis.SkyWebView?.getRuntimeInfo?.();

if (runtime) {
  const isPreviewBridge = runtime.host === "preview";
  runtimeState.textContent = isPreviewBridge ? "PREVIEW BRIDGE" : "SKYULTRA CONNECTED";
  runtimeState.classList.add(isPreviewBridge ? "preview" : "connected");
  runtimeDetails.textContent = `${runtime.host} · ${runtime.browserEngine ?? "engine unknown"} · bridge ${runtime.bridgeVersion}`;
} else {
  const isViteDev = import.meta.env.DEV;
  runtimeState.textContent = isViteDev ? "VITE DEV PREVIEW" : "BROWSER PREVIEW";
  runtimeDetails.textContent = isViteDev
    ? "Vite is serving this page · open it in Skyrim to connect the game runtime."
    : "This page is open outside Skyrim · the native bridge is unavailable.";
}
