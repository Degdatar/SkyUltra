#!/usr/bin/env node

import { access, mkdir, readdir, readFile, stat, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const packageRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const templateRoot = path.join(packageRoot, "templates", "vanilla");
const idPattern = /^[a-z0-9][a-z0-9._-]{0,63}$/;

function usage() {
  process.stdout.write(`Create a SkyUltra Vite mod project\n\n` +
    `Usage:\n` +
    `  npm create skyultra-mod <ModName> [-- --dir <path>] [--id <mod-id>] [--local-sdk]\n\n` +
    `Options:\n` +
    `  --dir <path>   Project destination (defaults to ./<mod-id>)\n` +
    `  --id <mod-id>  Override the lowercase ID used by the native manifest\n` +
    `  --local-sdk    Use the vite-plugin package from this SkyUltra checkout\n` +
    `  --help         Show this help\n`);
}

function slugify(name) {
  return name.normalize("NFKD").replace(/[\u0300-\u036f]/g, "")
    .toLowerCase().replace(/[^a-z0-9._-]+/g, "-")
    .replace(/^[^a-z0-9]+/, "").replace(/[-._]+$/, "").slice(0, 64);
}

function htmlEscape(value) {
  return value.replace(/[&<>"']/g, (character) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;",
  })[character]);
}

async function parseArguments(args) {
  const positional = [];
  let destination;
  let id;
  let localSdk = false;

  for (let index = 0; index < args.length; index += 1) {
    const argument = args[index];
    if (argument === "--help" || argument === "-h") {
      usage();
      process.exit(0);
    } else if (argument === "--dir") {
      destination = args[++index];
      if (!destination) throw new Error("--dir requires a path.");
    } else if (argument === "--id") {
      id = args[++index];
      if (!id) throw new Error("--id requires an ID.");
    } else if (argument === "--local-sdk") {
      localSdk = true;
    } else if (argument.startsWith("-")) {
      throw new Error(`Unknown option '${argument}'. Use --help to see available options.`);
    } else {
      positional.push(argument);
    }
  }

  if (positional.length !== 1) {
    usage();
    throw new Error("Provide one mod name.");
  }
  const name = positional[0].trim();
  if (!name || Buffer.byteLength(name, "utf8") > 128) {
    throw new Error("The mod name must contain 1 to 128 UTF-8 bytes.");
  }
  id ??= slugify(name);
  if (!idPattern.test(id)) {
    throw new Error("Mod IDs must be 1 to 64 lowercase letters, digits, dots, underscores, or hyphens, and begin with a letter or digit.");
  }

  return { name, id, destination, localSdk };
}

async function copyTemplate(sourceDirectory, targetDirectory, replacements) {
  await mkdir(targetDirectory, { recursive: true });
  for (const item of await readdir(sourceDirectory, { withFileTypes: true })) {
    const sourcePath = path.join(sourceDirectory, item.name);
    const targetPath = path.join(targetDirectory, item.name);
    if (item.isDirectory()) {
      await copyTemplate(sourcePath, targetPath, replacements);
      continue;
    }
    if (!item.isFile()) continue;

    const contents = await readFile(sourcePath, "utf8");
    const rendered = replacements.reduce(
      (text, [token, value]) => text.replaceAll(token, value),
      contents,
    );
    await writeFile(targetPath, rendered, "utf8");
  }
}

async function main() {
  const options = await parseArguments(process.argv.slice(2));
  const destination = path.resolve(process.cwd(), options.destination ?? options.id);
  const parentInfo = await stat(path.dirname(destination)).catch(() => undefined);
  if (parentInfo && !parentInfo.isDirectory()) {
    throw new Error(`The parent of '${destination}' is not a directory.`);
  }

  const destinationInfo = await stat(destination).catch(() => undefined);
  if (destinationInfo) {
    if (!destinationInfo.isDirectory()) throw new Error(`'${destination}' already exists and is not a directory.`);
    if ((await readdir(destination)).length > 0) {
      throw new Error(`'${destination}' already exists and is not empty. Choose another --dir path.`);
    }
  }

  const localPluginPath = path.resolve(packageRoot, "../vite-plugin");
  const hasLocalPlugin = await access(path.join(localPluginPath, "package.json")).then(() => true, () => false);
  if (options.localSdk && !hasLocalPlugin) {
    throw new Error("--local-sdk requires the SkyUltra sdk/vite-plugin package alongside this CLI.");
  }
  const pluginDependency = options.localSdk || hasLocalPlugin
    ? `file:${path.relative(destination, localPluginPath).split(path.sep).join("/")}`
    : "^0.1.0";
  const replacements = [
    ["__MOD_NAME_JSON__", JSON.stringify(options.name).slice(1, -1)],
    ["__MOD_NAME_HTML__", htmlEscape(options.name)],
    ["__MOD_NAME__", options.name],
    ["__MOD_ID__", options.id],
    ["__PLUGIN_DEPENDENCY__", pluginDependency],
  ];

  await mkdir(destination, { recursive: true });
  await copyTemplate(templateRoot, destination, replacements);

  process.stdout.write(`\nCreated ${options.name} (${options.id}) at ${destination}\n\n` +
    `Next steps:\n` +
    `  cd "${destination}"\n` +
    `  npm install\n` +
    `  npm run dev\n\n` +
    `Production files will be written to dist/ (manifest) and dist/Files/ (page assets).\n` +
    `Copy dist/ into Data/SkyUltra/ModName/ and register ID '${options.id}' with SkyUltra.\n`);
}

main().catch((error) => {
  process.stderr.write(`create-skyultra-mod: ${error.message}\n`);
  process.exitCode = 1;
});
