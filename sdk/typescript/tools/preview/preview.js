const entryInput = document.querySelector("#entry");
const widthInput = document.querySelector("#width");
const heightInput = document.querySelector("#height");
const frame = document.querySelector("#page");
const viewport = document.querySelector("#viewport");
const liveState = document.querySelector(".live-state");
const liveLabel = document.querySelector("#live-label");
const sizeLabel = document.querySelector("#size-label");
const eventForm = document.querySelector("#event-form");
const eventStatus = document.querySelector("#event-status");

function boundedNumber(input, minimum, maximum, fallback) {
  const parsed = Number.parseInt(input.value, 10);
  const value = Number.isFinite(parsed) ? Math.min(maximum, Math.max(minimum, parsed)) : fallback;
  input.value = String(value);
  return value;
}

function normalizedEntry(value) {
  const entry = value.trim().replaceAll("\\", "/").replace(/^\/+/, "");
  if (!entry || entry.split("/").some((part) => part === "..")) {
    throw new Error("Enter a page path inside the mod directory, such as /index.html.");
  }
  return `/${entry}`;
}

function fitViewport() {
  const width = boundedNumber(widthInput, 320, 7680, 1920);
  const height = boundedNumber(heightInput, 200, 4320, 1080);
  const scale = Math.min(viewport.clientWidth / width, viewport.clientHeight / height);
  frame.style.width = `${width}px`;
  frame.style.height = `${height}px`;
  frame.style.transform = `scale(${scale})`;
  sizeLabel.textContent = `${width} × ${height} CSS pixels · ${Math.round(scale * 100)}% preview scale`;
}

function loadEntry(entry, reload = false) {
  const target = new URL(entry, window.location.origin);
  if (reload) target.searchParams.set("__skyultra_reload", String(Date.now()));
  frame.src = target.pathname + target.search + target.hash;
}

const currentUrl = new URL(window.location.href);
entryInput.value = currentUrl.searchParams.get("entry") ?? "/index.html";
widthInput.value = currentUrl.searchParams.get("width") ?? "1920";
heightInput.value = currentUrl.searchParams.get("height") ?? "1080";
loadEntry(normalizedEntry(entryInput.value));
fitViewport();
window.addEventListener("resize", fitViewport);
widthInput.addEventListener("input", fitViewport);
heightInput.addEventListener("input", fitViewport);

document.querySelector("#page-form").addEventListener("submit", (event) => {
  event.preventDefault();
  try {
    const entry = normalizedEntry(entryInput.value);
    const width = boundedNumber(widthInput, 320, 7680, 1920);
    const height = boundedNumber(heightInput, 200, 4320, 1080);
    const nextUrl = new URL(window.location.href);
    nextUrl.searchParams.set("entry", entry);
    nextUrl.searchParams.set("width", String(width));
    nextUrl.searchParams.set("height", String(height));
    window.history.replaceState(null, "", nextUrl);
    loadEntry(entry);
    fitViewport();
  } catch (error) {
    liveState.className = "live-state disconnected";
    liveLabel.textContent = error.message;
  }
});

const events = new EventSource("/__skyultra/events");
events.addEventListener("open", () => {
  liveState.className = "live-state connected";
  liveLabel.textContent = "Live reload on";
});
events.addEventListener("error", () => {
  liveState.className = "live-state disconnected";
  liveLabel.textContent = "Reconnecting…";
});
events.addEventListener("reload", () => {
  try {
    loadEntry(normalizedEntry(entryInput.value), true);
  } catch {
    // Keep the current frame if the user is in the middle of editing the path.
  }
});

eventForm.addEventListener("submit", (event) => {
  event.preventDefault();
  try {
    const name = document.querySelector("#event-name").value.trim();
    if (!name) throw new Error("Enter an event name.");
    const data = JSON.parse(document.querySelector("#event-data").value);
    frame.contentWindow.dispatchEvent(new CustomEvent("skyultra:message", {
      detail: { name, data },
    }));
    eventStatus.textContent = `Sent ${name}`;
  } catch (error) {
    eventStatus.textContent = error instanceof SyntaxError ? "Event data must be valid JSON." : error.message;
  }
});
