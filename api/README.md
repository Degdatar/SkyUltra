# Native C/C++ API

The authoritative contract is [NativeAPI.h](include/SkyUltra/NativeAPI.h), identical to the host's public header. [SkyUltraAPI.hpp](include/SkyUltra/SkyUltraAPI.hpp) is a thin C++20 helper over that contract. Both discover the already-loaded `SkyUltra.dll`; no SkyUltra import library is needed.

## Add it to an SKSE project

Copy `api/include/SkyUltra/` into your include tree, or add this directory with CMake and link the **interface** target `SkyUltra::API`. The consuming SKSE plugin still needs its own CommonLib/SKSE project setup and metadata.

```cmake
add_subdirectory(external/skyultra-api)
target_link_libraries(MyPlugin PRIVATE SkyUltra::API)
```

`Client::Acquire()` resolves **V4** and returns an empty client if unavailable. The separate C exports retain V1–V4 for callers targeting those versions; the helper does not silently fall back to an older table.

| ABI | Added capability |
| --- | --- |
| V1 | View creation/state/destruction, visibility/bounds/navigation, JSON events |
| V2 | Async page requests, cancellation, callback registration and per-view allowlists |
| V3 | Register an installed manifest, create a manifest-scoped view, query storage directory |
| V4 | Reload the original view URL |

## Install a manifest and page

Create `Data/SkyUltra/sample.native/manifest.json`:

```json
{
  "schemaVersion": 1,
  "id": "sample.native",
  "name": "Native API sample",
  "version": "1.0.0",
  "entry": "Files/index.html",
  "permissions": {
    "assetAccess": true,
    "storage": false,
    "networkHosts": [],
    "pageRequests": ["ui.pageReady", "sample.ping"]
  }
}
```

Place `index.html` and a copy of `resources/SkyUltraBridge.js` in that `Files` folder:

```html
<!doctype html>
<meta charset="utf-8">
<p id="status">Connecting…</p>
<script src="./SkyUltraBridge.js"></script>
<script>
  const bridge = window.SkyWebView;
  bridge.on('sample.status', payload => console.log(payload));
  async function start() {
    await bridge.request('ui.pageReady', {});
    const result = await bridge.request('sample.ping', {});
    document.getElementById('status').textContent = result.message;
  }
  start().catch(error => { document.getElementById('status').textContent = error.message; });
</script>
```

The [complete integration example](examples/MinimalSksePlugin.cpp) registers this manifest, creates a view, grants both methods, handles `sample.ping` and sends `sample.status` back. Register the callback and allowlist before the page sends custom requests. The [SDK guide](../docs/SDK_GUIDE.md) covers packaging and startup.

## Create and control a view

```cpp
#include <SkyUltra/SkyUltraAPI.hpp>

auto client = SkyUltra::Client::Acquire();
SkyUltraModHandle mod{};
SkyUltraViewHandle view{};
if (client && client.RegisterMod("sample.native", &mod) == SKYULTRA_OK) {
    const auto result = client.CreateView(mod, "sample.native.panel",
        SkyUltra::Layer::Panel, 1280, 720, SkyUltra::Engine::Default,
        true, true, false, &view);
    // Check result before using view. NOT_INITIALIZED means retry later.
    (void)result;
}
```

Call after SKSE has loaded plugins, and retry from a later normal game update if the SkyUltra host is still starting. Do not block the game thread waiting for it. `SKYULTRA_OK` accepts a queued operation; `get_view_state` reports eventual pending/ready/failed/closing state. **READY does not mean your application has subscribed to events**: use a page-originated ready/custom request before sending initial application data.

`create_mod_view` loads the registered manifest's `entry`. Its `initial_url` must be empty. Use separate manifests if you need independently declared entry pages, or route within one app. `SkyUltraViewConfigV1` supplies pixel dimensions, normalized-independent pixel bounds, opacity/scale, layer, anchor, flags and the trailing `browser_engine`. Set `struct_size = sizeof(config)`.

The helper provides `SetVisible`, `SetBounds`, `GetViewState`, `ReloadView`, `DestroyView` and `SendEventJson`; use `Raw()` for the complete table. Interactive versus click-through behavior follows the view flags and host visibility/input handling. This ABI has no `set_focused`, `call_function`, `create_mod_view_v2`, or `get_view_browser_engine` entry. For native-to-page calls, send an event and subscribe with `SkyWebView.on`.

## Requests, replies and cancellation

The real callback signature is:

```cpp
void SKYULTRA_CALL OnRequest(
    void* user, SkyUltraViewHandle view,
    const char* request_id, uint32_t request_id_length,
    const char* name, uint32_t name_length,
    const char* params_json, uint32_t params_json_length,
    SkyUltraPageRequestEventV2 event);
```

Request IDs are **UTF-8 strings**, not integers. Input spans are borrowed for the duration of the callback: copy anything needed later. Callbacks arrive on the game thread. On `SKYULTRA_PAGE_REQUEST_CANCELLED_V2`, abandon the operation and do not complete it. Reply with `CompletePageRequestOk(view, id, json)` or `CompletePageRequestError(view, id, error)`; delayed completion is allowed from another thread, but game object access still belongs on the game thread. Serialize dynamic data with a JSON library instead of interpolating unescaped strings.

There are two permission checks: `permissions.pageRequests` in the manifest, and the view's `set_page_request_allowlist`. The latter cannot grant methods absent from the manifest. Host-owned `game.*` and supported `ui.*` methods are handled by SkyUltra; custom names such as `sample.ping` reach the native callback.

**Current callback ownership:** the exported host has one global `set_page_request_callback(callback, user_data)` slot. A later registration replaces it. Coordinate a shared dispatcher between native consumers; this is not a per-view/per-mod callback API. Papyrus has its own documented request-event route. Do not promise independent native callback ownership through a shim.

## Engines and lifecycle

- `Engine::Default` follows host configuration; a manifest pin takes precedence when present. `Cef` and `Ultralight` request specific backends included in that host build. Inspect `SkyWebView.getRuntimeInfo()` in the page for the actual engine.
- Events use a name span and a JSON span; both lengths are required by the C API. Native-to-page events accept up to 1 MiB of JSON; page-to-native requests retain the bridge's 64 KiB message budget, including envelope overhead.
- Keep handles session-local. Hide a panel when finished and destroy a view when its owner no longer needs it. Reloading replaces the document, so recreate page event subscriptions and the ready handshake.
- A hidden panel stays hidden when the player opens SkyUltra or another mod. Use the owning view's `SetVisible`/`set_visible` to open it explicitly; that reveals the requested view without restoring unrelated hidden panels. Hotkey recording/input scopes are active only while their owning interactive view is visible.
- Use `get_mod_storage_directory` for persistent data when the manifest grants storage. Query capacity first; the required byte count includes the trailing NUL.

`send_event_json` and `complete_page_request` reserve delivery capacity synchronously. `SKYULTRA_OK` means the host owns the message, not that a page handler has finished. Each view has a 1,024-message / 64 MiB event queue and a separate reply queue with the same limits, counting in-flight batches. `SKYULTRA_QUEUE_FULL` (`7`) means no ownership transferred; retain the message and retry later without blocking the game thread. Other failures still require checking their result code. Do not retry an accepted command. Navigation and view destruction have explicit lifecycle limits; see [delivery details and diagnostics](../docs/BRIDGE_DELIVERY.md).

The `max_fps` field retains its existing behavior and ABI position: `0` selects the native API's 60 FPS default; positive values request a per-view cap. CEF clamps requests to 1–120 FPS, and Ultralight clamps positive requests to 1–240 FPS subject to its runtime edition's limit. `SKYULTRA_FPS_UNCAPPED` selects the backend default: display refresh within 120 FPS for CEF, or Ultralight's runtime default. It does not bypass CEF's host ceiling. No C function-table layout or API version changed.

## Source-only checks

```powershell
./tools/Check-NativeSource.ps1 -Source api/examples/MinimalSksePlugin.cpp -Include api/include
```

Run from the project root. This uses an existing compiler configuration and MSVC `/Zs`; it does not link, build a release, install or launch Skyrim. In-game behavior and the interaction with other native callback owners need separate validation.

See [SDK usage](../docs/SDK_GUIDE.md), [PrismaUI migration](../docs/PRISMAUI_MIGRATION.md), [Magelight UI migration](../docs/MAGELIGHT_UI_MIGRATION.md), and [library credits](../docs/THIRD_PARTY_LIBRARIES.md).
