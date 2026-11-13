import { defineConfig } from "vite";
import { skyultra } from "../../sdk/vite-plugin/index.js";

export default defineConfig({
  base: "./",
  plugins: [skyultra()],
  resolve: {
    // Local file dependencies can resolve their own peer React install through
    // the symlinked SDK package. Force the app and its components onto one copy.
    dedupe: ["react", "react-dom"],
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
