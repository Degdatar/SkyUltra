import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { installSkyUltraMock } from "@skyultra/sdk";
import { App } from "./App";
import "@skyultra/ui/styles.css";
import "./app.css";

if (!window.SkyWebView) {
  const previewMock = installSkyUltraMock(window);
  if (import.meta.env.DEV) {
    Object.defineProperty(window, "__SKYULTRA_MOCK__", {
      value: previewMock,
      configurable: true,
    });
  }
}

const rootElement = document.getElementById("root");
if (!rootElement) throw new Error("Missing #root element");

createRoot(rootElement).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
