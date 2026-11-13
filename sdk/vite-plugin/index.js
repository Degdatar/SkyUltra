import { mkdir, readFile, readdir, realpath, rename, rmdir, stat, writeFile } from "node:fs/promises";
import path from "node:path";

const topLevelKeys = new Set(["schemaVersion", "id", "name", "version", "entry", "permissions", "papyrusPlugin", "browserEngine"]);
const permissionKeys = new Set(["storage", "assetAccess", "networkHosts", "pageRequests"]);
const identifierPattern = /^[a-z0-9][a-z0-9._-]{0,63}$/;
const pageRequestPattern = /^[A-Za-z0-9._:-]{1,128}$/;
const bridgeAssetUrl = new URL("./assets/SkyUltraBridge.js", import.meta.url);

function hasOnlyKeys(value, allowed) {
  return value && typeof value === "object" && !Array.isArray(value) &&
    Object.keys(value).every((key) => allowed.has(key));
}

function byteLength(value) {
  return Buffer.byteLength(value, "utf8");
}

function isValidNetworkHost(host) {
  if (typeof host !== "string" || byteLength(host) > 253 || host.length === 0) return false;
  return host.split(".").every((label) =>
    label.length > 0 && label.length <= 63 &&
    /^[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?$/.test(label),
  );
}

function safeEntryPath(entry) {
  if (typeof entry !== "string" || entry.length === 0 || byteLength(entry) > 512 || entry.includes("\0")) {
    return false;
  }
  if (path.posix.isAbsolute(entry) || path.win32.isAbsolute(entry) || path.win32.parse(entry).root) {
    return false;
  }
  return !entry.split(/[\\/]/).some((part) => part === "." || part === "..");
}

function isWithinDirectory(root, candidate) {
  const relative = path.relative(root, candidate);
  return relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative);
}

async function moveBuildOutput(sourcePath, targetPath) {
  const sourceInfo = await stat(sourcePath);
  if (sourceInfo.isDirectory()) {
    await mkdir(targetPath, { recursive: true });
    for (const name of await readdir(sourcePath)) {
      await moveBuildOutput(path.join(sourcePath, name), path.join(targetPath, name));
    }
    await rmdir(sourcePath);
    return;
  }

  try {
    await stat(targetPath);
    throw new Error(`SkyUltra build output collision at '${targetPath}'. Clear the output directory or remove the duplicate asset.`);
  } catch (error) {
    if (error.code !== "ENOENT") throw error;
  }
  await mkdir(path.dirname(targetPath), { recursive: true });
  await rename(sourcePath, targetPath);
}

function assertManifestShape(manifest, manifestBytes) {
  if (manifestBytes === 0 || manifestBytes > 64 * 1024) {
    throw new Error("SkyUltra manifest must be between 1 byte and 64 KiB.");
  }
  if (!hasOnlyKeys(manifest, topLevelKeys)) {
    throw new Error("SkyUltra manifest must be an object containing only supported top-level fields.");
  }
  if (manifest.browserEngine !== undefined &&
      manifest.browserEngine !== "ultralight" && manifest.browserEngine !== "cef") {
    throw new Error('browserEngine must be either "ultralight" or "cef".');
  }
  if (manifest.schemaVersion !== 1 || typeof manifest.id !== "string" || !identifierPattern.test(manifest.id) ||
      typeof manifest.name !== "string" || manifest.name.length === 0 || byteLength(manifest.name) > 128 ||
      typeof manifest.version !== "string" || manifest.version.length === 0 || byteLength(manifest.version) > 64 ||
      !safeEntryPath(manifest.entry) ||
      (manifest.papyrusPlugin !== undefined &&
        (typeof manifest.papyrusPlugin !== "string" || manifest.papyrusPlugin.length === 0 ||
         byteLength(manifest.papyrusPlugin) > 260 || !/^[A-Za-z0-9_.-]+$/.test(manifest.papyrusPlugin) ||
         manifest.papyrusPlugin === "." || manifest.papyrusPlugin === "..")) ||
      !hasOnlyKeys(manifest.permissions, permissionKeys)) {
    throw new Error("SkyUltra manifest is missing a required field or has a value outside the native loader's supported limits.");
  }

  const permissions = manifest.permissions;
  if (typeof permissions.storage !== "boolean" ||
      (permissions.assetAccess !== undefined && typeof permissions.assetAccess !== "boolean") ||
      !Array.isArray(permissions.pageRequests) || permissions.pageRequests.length > 128 ||
      (permissions.networkHosts !== undefined &&
        (!Array.isArray(permissions.networkHosts) || permissions.networkHosts.length > 128))) {
    throw new Error("SkyUltra permissions must declare storage and pageRequests; assetAccess and networkHosts are optional.");
  }

  const methods = new Set();
  for (const method of permissions.pageRequests) {
    if (typeof method !== "string" || !pageRequestPattern.test(method) || methods.has(method)) {
      throw new Error("permissions.pageRequests must contain unique valid method names (up to 128 characters).");
    }
    methods.add(method);
  }

  const hosts = new Set();
  for (const host of permissions.networkHosts ?? []) {
    const normalized = typeof host === "string" ? host.toLowerCase() : "";
    if (!isValidNetworkHost(host) || hosts.has(normalized)) {
      throw new Error("permissions.networkHosts must contain unique DNS hostnames without wildcards, ports, or URL paths.");
    }
    hosts.add(normalized);
  }
}

/** Validates a schema-version-1 manifest using SkyUltra's native loader rules. */
export async function validateSkyUltraManifest(manifestPath, projectRoot = process.cwd()) {
  const root = await realpath(projectRoot);
  const fullManifestPath = path.resolve(root, manifestPath);
  const relativeManifestPath = path.relative(root, fullManifestPath);
  if (!relativeManifestPath || !isWithinDirectory(root, fullManifestPath)) {
    throw new Error("SkyUltra manifest must be inside the Vite project root.");
  }

  let canonicalManifestPath;
  try {
    canonicalManifestPath = await realpath(fullManifestPath);
  } catch (error) {
    throw new Error(`Cannot resolve SkyUltra manifest at ${path.relative(root, fullManifestPath)}: ${error.message}`);
  }
  if (!isWithinDirectory(root, canonicalManifestPath)) {
    throw new Error("SkyUltra manifest resolves outside the Vite project root.");
  }

  let source;
  try {
    source = await readFile(canonicalManifestPath);
  } catch (error) {
    throw new Error(`Cannot read SkyUltra manifest at ${path.relative(root, fullManifestPath)}: ${error.message}`);
  }
  let manifest;
  try {
    manifest = JSON.parse(source.toString("utf8"));
  } catch (error) {
    throw new Error(`SkyUltra manifest contains invalid JSON: ${error.message}`);
  }
  assertManifestShape(manifest, source.byteLength);

  const entryPath = path.resolve(root, manifest.entry);
  if (!isWithinDirectory(root, entryPath) || path.relative(root, entryPath) === "") {
    throw new Error("The manifest entry must resolve to a file inside the Vite project root.");
  }

  let canonicalEntry;
  try {
    canonicalEntry = await realpath(entryPath);
  } catch {
    throw new Error(`The manifest entry '${manifest.entry}' does not exist.`);
  }
  const canonicalRelativeEntry = path.relative(root, canonicalEntry);
  if (!isWithinDirectory(root, canonicalEntry)) {
    throw new Error("The manifest entry resolves outside the Vite project root.");
  }
  const entryInfo = await stat(canonicalEntry);
  if (!entryInfo.isFile()) throw new Error(`The manifest entry '${manifest.entry}' is not a file.`);

  return manifest;
}

/** Validates manifests and prepares Vite output for installation under Data/SkyUltra/<ModName>/Files. */
export function skyultra(options = {}) {
  const manifestOption = options.manifest ?? "public/manifest.json";
  if (path.isAbsolute(manifestOption) || path.win32.isAbsolute(manifestOption)) {
    throw new Error("The SkyUltra manifest option must be a path relative to the Vite project root.");
  }

  let resolvedConfig;
  let manifestText = "";
  let manifest;

  return {
    name: "@skyultra/vite-plugin",
    config(userConfig) {
      // Installed mods use readable asset names. Set defaults here, before
      // Rollup generates imports/preloads, instead of renaming built files.
      // Mutate output entries in place: returning an output array from this
      // hook would concatenate it with the author's existing output array.
      const build = userConfig.build ??= {};
      const rollup = build.rollupOptions ??= {};
      const assets = (build.assetsDir ?? "assets").replace(/\/+$/, "");
      const prefix = assets ? assets + "/" : "";
      const defaults = {
        entryFileNames: prefix + "[name].js",
        chunkFileNames: prefix + "[name].js",
        assetFileNames: prefix + "[name][extname]",
      };
      const applyNames = (output) => {
        for (const [key, value] of Object.entries(defaults)) output[key] ??= value;
        return output;
      };
      if (Array.isArray(rollup.output)) rollup.output.forEach(applyNames);
      else rollup.output = applyNames(rollup.output ?? {});
      const modulePreload = userConfig.build?.modulePreload;
      return {
        ...(userConfig.base === undefined ? { base: "./" } : {}),
        build: {
          target: userConfig.build?.target ?? "es2022",
          modulePreload: {
            ...(modulePreload && typeof modulePreload === "object" ? modulePreload : {}),
            polyfill: false,
          },
        },
      };
    },
    configResolved(config) {
      resolvedConfig = config;
      if (config.command === "build" && config.base !== "./") {
        throw new Error("SkyUltra mods must use Vite base './' so packaged CSS, scripts, fonts, and images resolve from the mod Files directory.");
      }
    },
    async buildStart() {
      const fullPath = path.resolve(resolvedConfig.root, manifestOption);
      manifest = await validateSkyUltraManifest(fullPath, resolvedConfig.root);
      manifestText = await readFile(fullPath, "utf8");
      if (resolvedConfig.command === "build" && manifest.permissions.assetAccess === false) {
        this.warn("permissions.assetAccess is false. SkyUltra may block Vite's emitted JavaScript, stylesheets, fonts, and images when this mod runs in-game.");
      }
    },
    transformIndexHtml: {
      order: "post",
      handler(html) {
        let transformed = html;
        if (resolvedConfig.command === "serve" && options.devBridge !== false &&
            !/\bsrc\s*=\s*["'][^"']*SkyUltraBridge\.js(?:[?#][^"']*)?["']/i.test(transformed)) {
          const headTag = /<head\b[^>]*>/i;
          if (!headTag.test(transformed)) {
            this.error("The HTML entry has no <head> element for the SkyUltra bridge script.");
          }
          transformed = transformed.replace(headTag, '$&\n  <script src="./SkyUltraBridge.js"></script>');
        }
        // Vite adds crossorigin to module scripts and stylesheets. Ultralight serves
        // mod pages from file:// URLs, where CORS-mode module requests can be refused
        // silently, leaving only a blank white view.
        return transformed.replace(/\s+crossorigin(?:=["'][^"']*["'])?/gi, "");
      },
    },
    configureServer(server) {
      const fullPath = path.resolve(resolvedConfig.root, manifestOption);
      server.middlewares.use("/SkyUltraBridge.js", async (_request, response, next) => {
        try {
          response.statusCode = 200;
          response.setHeader("Content-Type", "text/javascript; charset=utf-8");
          response.setHeader("Cache-Control", "no-cache");
          response.end(await readFile(bridgeAssetUrl));
        } catch (error) {
          next(error);
        }
      });
      server.watcher.add(fullPath);
      server.watcher.on("change", async (changedPath) => {
        if (path.resolve(changedPath) !== fullPath) return;
        try {
          manifest = await validateSkyUltraManifest(fullPath, resolvedConfig.root);
          manifestText = await readFile(fullPath, "utf8");
          server.ws.send({ type: "full-reload" });
          server.config.logger.info("SkyUltra manifest validated and preview reloaded.");
        } catch (error) {
          server.config.logger.error(error.message);
        }
      });
    },
    generateBundle() {
      if (!manifest) this.error("SkyUltra manifest was not validated before bundling.");
      const publicManifest = resolvedConfig.publicDir &&
        path.resolve(resolvedConfig.publicDir, "manifest.json");
      const sourceManifest = path.resolve(resolvedConfig.root, manifestOption);
      if (publicManifest !== sourceManifest) {
        this.emitFile({ type: "asset", fileName: "manifest.json", source: manifestText });
      }
    },
    async closeBundle() {
      if (resolvedConfig.command !== "build") return;
      const outputDirectory = path.resolve(resolvedConfig.root, resolvedConfig.build.outDir);
      const builtEntry = path.resolve(outputDirectory, manifest.entry);
      try {
        const entryInfo = await stat(builtEntry);
        if (!entryInfo.isFile()) throw new Error("not a file");
      } catch {
        this.error(`Manifest entry '${manifest.entry}' was not emitted to '${path.relative(resolvedConfig.root, outputDirectory)}'. Add it to Vite's build inputs or place it under public/.`);
      }

      const filesDirectory = path.join(outputDirectory, "Files");
      await mkdir(filesDirectory, { recursive: true });
      for (const name of await readdir(outputDirectory)) {
        if (name === "manifest.json" || name === "Files") continue;
        await moveBuildOutput(path.join(outputDirectory, name), path.join(filesDirectory, name));
      }

      const bridgeTarget = path.join(filesDirectory, "SkyUltraBridge.js");
      const bridgeSource = await readFile(bridgeAssetUrl);
      try {
        const existingBridge = await readFile(bridgeTarget);
        if (!existingBridge.equals(bridgeSource)) {
          this.error("SkyUltraBridge.js is reserved for the SkyUltra native page bridge.");
        }
      } catch (error) {
        if (error.code !== "ENOENT") throw error;
        await writeFile(bridgeTarget, bridgeSource);
      }

      const sourceEntry = manifest.entry.replaceAll("\\", "/");
      const packagedEntry = sourceEntry.startsWith("Files/") ? sourceEntry : `Files/${sourceEntry}`;
      if (byteLength(packagedEntry) > 512) {
        this.error("Packaging the manifest entry under Files/ would exceed SkyUltra's 512-byte entry path limit.");
      }
      const packagedManifest = { ...manifest, entry: packagedEntry };
      await writeFile(path.join(outputDirectory, "manifest.json"), `${JSON.stringify(packagedManifest, null, 2)}\n`, "utf8");

      const packagedPage = path.resolve(outputDirectory, packagedEntry);
      try {
        const entryInfo = await stat(packagedPage);
        if (!entryInfo.isFile()) throw new Error("not a file");
      } catch {
        this.error(`Packaged manifest entry '${packagedEntry}' is missing from the Files directory.`);
      }
      // Multi-page mods (for example a click-through HUD) need the bridge in
      // every emitted page, including pages other than the manifest entry.
      const injectBridge = async (directory) => {
        for (const entry of await readdir(directory, { withFileTypes: true })) {
          const page = path.join(directory, entry.name);
          if (entry.isDirectory()) { await injectBridge(page); continue; }
          if (!entry.isFile() || !/\.html?$/i.test(entry.name)) continue;
          const html = await readFile(page, "utf8");
          if (!/\bsrc\s*=\s*["'][^"']*SkyUltraBridge\.js(?:[?#][^"']*)?["']/i.test(html)) {
            const headTag = /<head\b[^>]*>/i;
            if (!headTag.test(html)) {
              this.error(`The HTML page '${path.relative(outputDirectory,page)}' has no <head> element for the SkyUltra bridge script.`);
            }
            const bridgePath = path.relative(path.dirname(page),bridgeTarget).split(path.sep).join("/");
            await writeFile(page,
              html.replace(headTag, `$&\n  <script src="${bridgePath.startsWith(".") ? bridgePath : "./"+bridgePath}"></script>`), "utf8");
          }
        }
      };
      await injectBridge(filesDirectory);
    },
  };
}
