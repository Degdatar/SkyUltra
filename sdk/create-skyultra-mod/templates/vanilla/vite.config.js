import { defineConfig } from "vite";
import { skyultra } from "@skyultra/vite-plugin";

export default defineConfig({
  plugins: [skyultra()],
  base: "./",
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
