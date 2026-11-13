import { defineConfig } from "vite";
import { skyultra } from "../../sdk/vite-plugin/index.js";

export default defineConfig({
  base: "./",
  plugins: [skyultra({ devBridge: false })],
  resolve: { dedupe: ["react", "react-dom"] },
  build: {
    outDir: "dist",
    emptyOutDir: true,
    target: "es2022",
    modulePreload: { polyfill: false },
    rollupOptions: { input: { main: "index.html", hud: "hud.html" } },
  },
});
