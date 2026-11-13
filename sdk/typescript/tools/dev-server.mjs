import { watch } from "node:fs";
import { readFile, realpath, stat } from "node:fs/promises";
import { createServer } from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";

const HOST = "127.0.0.1";
const TOOL_DIRECTORY = path.dirname(fileURLToPath(import.meta.url));
const PREVIEW_DIRECTORY = path.join(TOOL_DIRECTORY, "preview");
const IGNORED_WATCH_DIRECTORIES = new Set([".git", "node_modules", "dist"]);
const MIME_TYPES = new Map([
  [".avif", "image/avif"],
  [".css", "text/css; charset=utf-8"],
  [".gif", "image/gif"],
  [".html", "text/html; charset=utf-8"],
  [".ico", "image/x-icon"],
  [".jpeg", "image/jpeg"],
  [".jpg", "image/jpeg"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".map", "application/json; charset=utf-8"],
  [".png", "image/png"],
  [".svg", "image/svg+xml"],
  [".txt", "text/plain; charset=utf-8"],
  [".webp", "image/webp"],
  [".woff", "font/woff"],
  [".woff2", "font/woff2"],
]);

function parseArguments(args) {
  const values = new Map();
  for (let index = 0; index < args.length; index += 1) {
    const argument = args[index];
    if (argument === "--help" || argument === "-h") {
      values.set("help", true);
      continue;
    }
    if (!["--root", "--port"].includes(argument)) {
      throw new Error(`Unknown option: ${argument}`);
    }
    const value = args[index + 1];
    if (!value || value.startsWith("--")) {
      throw new Error(`${argument} requires a value`);
    }
    values.set(argument.slice(2), value);
    index += 1;
  }
  return values;
}

function printHelp() {
  process.stdout.write(
    "SkyUltra page preview server\n\n" +
      "Usage: node tools/dev-server.mjs [--root <mod-page-directory>] [--port <port>]\n" +
      "The server binds to 127.0.0.1 and reloads the preview when files change.\n",
  );
}

function isInsideRoot(root, candidate) {
  const relative = path.relative(root, candidate);
  return relative === "" || (!relative.startsWith(`..${path.sep}`) && relative !== ".." && !path.isAbsolute(relative));
}

function isIgnoredWatchPath(filename) {
  return filename
    .split(/[\\/]+/)
    .some((part) => IGNORED_WATCH_DIRECTORIES.has(part));
}

function isPrivatePath(filename) {
  return filename
    .split(/[\\/]+/)
    .some((part) => part.startsWith(".") || part.toLowerCase() === "node_modules");
}

function injectPreviewClient(html) {
  const script = '<script src="/__skyultra/client.js"></script>';
  if (html.includes("/__skyultra/client.js")) return html;
  const head = /<head(?:\s[^>]*)?>/i;
  if (head.test(html)) return html.replace(head, (openingTag) => `${openingTag}${script}`);
  return `${script}${html}`;
}

function send(response, status, body, contentType = "text/plain; charset=utf-8") {
  response.writeHead(status, {
    "Cache-Control": "no-store",
    "Content-Type": contentType,
    "X-Content-Type-Options": "nosniff",
  });
  response.end(body);
}

const args = parseArguments(process.argv.slice(2));
if (args.has("help")) {
  printHelp();
  process.exit(0);
}

const rootDirectory = await realpath(path.resolve(args.get("root") ?? process.cwd()));
const port = Number(args.get("port") ?? "4173");
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error("--port must be an integer from 1 to 65535");
}

const eventClients = new Set();
let reloadTimer;
let watcher;

function notifyPageChange(filename) {
  if (filename && isIgnoredWatchPath(filename)) return;
  clearTimeout(reloadTimer);
  reloadTimer = setTimeout(() => {
    const data = JSON.stringify({ changed: filename ? filename.replaceAll("\\", "/") : "" });
    for (const client of eventClients) client.write(`event: reload\ndata: ${data}\n\n`);
  }, 120);
}

const server = createServer(async (request, response) => {
  if (request.method !== "GET" && request.method !== "HEAD") {
    send(response, 405, "Method not allowed");
    return;
  }

  let url;
  try {
    url = new URL(request.url ?? "/", `http://${HOST}:${port}`);
  } catch {
    send(response, 400, "Invalid request URL");
    return;
  }

  if (url.pathname === "/__skyultra/events") {
    response.writeHead(200, {
      "Cache-Control": "no-cache, no-transform",
      Connection: "keep-alive",
      "Content-Type": "text/event-stream; charset=utf-8",
      "X-Accel-Buffering": "no",
    });
    response.write(": SkyUltra preview connected\n\n");
    eventClients.add(response);
    response.on("close", () => eventClients.delete(response));
    return;
  }

  if (url.pathname.startsWith("/__skyultra/")) {
    const assetName = url.pathname.slice("/__skyultra/".length);
    const allowedAssets = new Map([
      ["client.js", "client.js"],
      ["preview", "preview.html"],
      ["preview.html", "preview.html"],
      ["preview.css", "preview.css"],
      ["preview.js", "preview.js"],
    ]);
    const fileName = allowedAssets.get(assetName);
    if (!fileName) {
      send(response, 404, "Not found");
      return;
    }
    try {
      const asset = await readFile(path.join(PREVIEW_DIRECTORY, fileName));
      const contentType = MIME_TYPES.get(path.extname(fileName)) ?? "application/octet-stream";
      response.writeHead(200, {
        "Cache-Control": "no-store",
        "Content-Type": contentType,
        "X-Content-Type-Options": "nosniff",
      });
      response.end(request.method === "HEAD" ? undefined : asset);
    } catch {
      send(response, 404, "Not found");
    }
    return;
  }

  let decodedPath;
  try {
    decodedPath = decodeURIComponent(url.pathname);
  } catch {
    send(response, 400, "Invalid path encoding");
    return;
  }
  if (decodedPath.includes("\\") || decodedPath.includes("\0")) {
    send(response, 400, "Invalid path");
    return;
  }
  if (isPrivatePath(decodedPath)) {
    send(response, 404, "Not found");
    return;
  }

  let candidate = path.resolve(rootDirectory, `.${decodedPath}`);
  if (!isInsideRoot(rootDirectory, candidate)) {
    send(response, 403, "Path is outside the page directory");
    return;
  }

  try {
    candidate = await realpath(candidate);
    if (!isInsideRoot(rootDirectory, candidate)) {
      send(response, 403, "Path is outside the page directory");
      return;
    }
    if ((await stat(candidate)).isDirectory()) {
      candidate = await realpath(path.join(candidate, "index.html"));
      if (!isInsideRoot(rootDirectory, candidate)) {
        send(response, 403, "Path is outside the page directory");
        return;
      }
    }
    const body = await readFile(candidate);
    const extension = path.extname(candidate).toLowerCase();
    const contentType = MIME_TYPES.get(extension) ?? "application/octet-stream";
    response.writeHead(200, {
      "Cache-Control": "no-store",
      "Content-Type": contentType,
      "X-Content-Type-Options": "nosniff",
    });
    if (request.method === "HEAD") {
      response.end();
    } else if (extension === ".html") {
      response.end(injectPreviewClient(body.toString("utf8")));
    } else {
      response.end(body);
    }
  } catch (error) {
    const status = error?.code === "ENOENT" || error?.code === "ENOTDIR" ? 404 : 500;
    send(response, status, status === 404 ? "Not found" : "Could not read page file");
  }
});

await new Promise((resolve, reject) => {
  server.once("error", reject);
  server.listen(port, HOST, resolve);
});

const previewUrl = `http://${HOST}:${port}/__skyultra/preview?entry=%2Findex.html`;
process.stdout.write(`SkyUltra preview: ${previewUrl}\n`);
process.stdout.write(`Page directory: ${rootDirectory}\n`);
process.stdout.write("The preview uses a simulated SkyWebView bridge and is not a Skyrim runtime test.\n");

try {
  watcher = watch(rootDirectory, { recursive: true }, (_eventType, filename) => {
    notifyPageChange(filename?.toString());
  });
  watcher.on("error", (error) => {
    process.stderr.write(`File watching stopped: ${error.message}\n`);
  });
} catch (error) {
  process.stderr.write(`Live reload could not watch this directory: ${error.message}\n`);
}

function shutdown() {
  watcher?.close();
  for (const client of eventClients) client.end();
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(0), 1000).unref();
}

process.once("SIGINT", shutdown);
process.once("SIGTERM", shutdown);
