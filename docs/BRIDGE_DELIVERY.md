# Bridge delivery, performance and runtime settings

SkyUltra owns accepted native messages before a browser is ready and schedules their delivery in bounded batches. Both CEF and Ultralight use the same per-view queue implementation. Existing event names, JSON payloads, SDK subscriptions and legacy shims keep their contracts.

## Acceptance and backpressure

| Channel | Capacity per view | Scheduling |
| --- | --- | --- |
| Ordinary native-to-page events | 1,024 messages and 64 MiB of event-name/JSON bytes | FIFO batches of at most 8 messages / 256 KiB |
| Page-request replies | Separate 1,024 messages and 64 MiB | Reply batches precede ordinary event batches |
| Reserved host state | 16 entries and 64 KiB, separate from ordinary events | Latest complete state replaces an older queued state with the same host name |

Count and byte limits include in-flight messages. They bound owned payloads, rather than total process memory: JavaScript strings, parsed objects and browser resources use additional memory. A single event larger than the batch byte target can travel alone, subject to the API's 1 MiB JSON payload limit. Page-to-native requests retain their existing 64 KiB envelope limit.

The per-view limits additionally share [per-mod resource budgets](RUNTIME_RESILIENCE.md#resource-budgets): separate aggregate event/reply lanes of 4,096 messages / 128 MiB each, 16 reserved views and a 128 MiB primary-surface estimate. `SKYULTRA_RESOURCE_LIMIT` (`8`) reports view admission failure; `SKYULTRA_SUSPENDED` (`9`) reports an owner disabled through Developer Studio. Reserved controls retain their per-view lane.

Only SkyUltra's complete visibility, heading and menu-mode state events use the reserved lane: `skyultra.visibility`, `skyultra.menuSession`, `skyultra.compassHeading`, `skyultra.inventoryMode`, `skyultra.barterMode`, `skyultra.containerMode`, `skyultra.magicMode`, `skyultra.favoritesMode`, `skyultra.mannequinMode`, `skyultra.lootMode`, `skyultra.pickpocketMode` and `skyultra.compassMode`. Custom commands, chunks, regular deltas and input tokens are never coalesced. The reserved lane can overtake ordinary events; FIFO ordering applies within the ordinary channel. Opening or switching SWF menus increments a session transition epoch (`skyultra.menuSession` + `ui.pageReady({ epoch })`) and discards cached host surfaces immediately before displaying the new overlay so adjacent windows do not overlap across frames.

The native API reports acceptance directly:

- `SKYULTRA_OK`: capacity was reserved and the host owns the message. Do not resubmit it just because execution is asynchronous.
- `SKYULTRA_QUEUE_FULL` (`7`): the channel's count or byte budget is full. The message was not accepted. Keep it on the producer side and retry from a later update with backoff; never spin or block Skyrim's game thread.
- Other result codes: handle validation, shutdown, unavailable views or internal failures separately.

Failed reply acceptance leaves the page request eligible for a later completion attempt. The C ABI's function-table layouts, existing numeric result codes and version negotiation remain unchanged; queue-full is an additional result value.

Acceptance guarantees ownership, not application consumption. Wait for your page's ready handshake before sending initial app data. A page with no listener can still receive and discard an event. Reload/navigation replaces its document and may cancel an in-flight batch; destruction, failed creation and shutdown can cancel queued work. Engine switching transfers the pending queues and invalidates the old consumer, while the replacement page must establish its listeners again. Use an application acknowledgement if your protocol needs proof that a particular command was applied.

## Scheduling on each engine

Ultralight wakes its worker for bridge work without creating a game-thread task for every message, drains queued input and bridge batches cleanly (up to 32 items per wake/pass under the pass budget), caches resolved mount paths and charsets, reads local static assets in bulk, and skips idle GPU work when views are hidden or idle. A batch executes synchronously on the worker. A single expensive JavaScript callback cannot be preempted; it can still overrun that budget and delay other Ultralight views sharing the worker.

CEF uses dirty-rect GPU texture updates, keeps at most one submitted event batch and one reply batch per view outstanding, and batches native-to-page dispatches cleanly over V8 IPC. A private renderer-to-host IPC acknowledgement releases capacity before the next batch is submitted, independently of application logging overrides. This prevents a producer from moving an entire native backlog into Chromium's execution queue at once. The acknowledgement means the batch dispatcher ran; it does not wait for asynchronous application work. A console acknowledgement is retained as a compatibility fallback for an older subprocess; ship the plugin and current CEF subprocess together (16 runtime files with `locales/en-US.pak` only, plus `Diagnostics/SkyUltraCefSubprocess.pdb` outside the player runtime folder).

Each message is parsed and dispatched independently, so a malformed payload or synchronous handler failure does not abort the rest of a batch. This isolation does not make slow handlers cheap. The host improvements reduce queue/task overhead and starvation; they do not establish a measured frame-rate improvement or replace profiling of page code.

## Queue diagnostics

Developer Studio displays pending events/capacity and buffered bytes on each native-backed view. `ui.getDeveloperSnapshot` view records expose optional `eventQueue` and `responseQueue` objects, typed by `SkyUltraBridgeQueueStats` in `@skyultra/sdk/devtools`:

| Field | Meaning |
| --- | --- |
| `capacity`, `byteLimit` | Ordinary channel limits: 1,024 and 67,108,864 |
| `pending` | Ordinary accepted messages, including in-flight messages |
| `controlPending` | Reserved host-state entries, including in-flight entries |
| `inFlight` | Submitted messages across the channel's ordinary/control lanes |
| `bytes` | Owned event-name and JSON bytes, including in-flight messages |
| `rejected` | Admission attempts rejected because a budget was full |
| `delivered`, `cancelled`, `coalesced` | Dispatch/lifecycle/replacement counters |
| `oldestMs` | Age of the oldest still-queued message; excludes in-flight work |
| `maxDispatchMs` | Longest batch evaluation time in Ultralight; submission-to-ack wall time in CEF |

Slow batches also produce rate-limited native log entries. CEF wall time includes renderer scheduling/IPC, so its number is not directly comparable to Ultralight's synchronous evaluation time. An empty queue can still have an expensive page, GPU workload or asynchronous callback.

Developer Studio's package asset-size warning now starts **above 1 GiB (1,073,741,824 bytes)**, displayed as “1 GB.” It is advisory package analysis, not a browser persistent-storage quota and not permission to access files outside the mod's allowed paths.

## Visible rendering cadence

CEF relies on its internal Viz BeginFrame scheduler (`external_begin_frame_enabled = FALSE`), keeps its `windowless_frame_rate` ceiling at **120 FPS**, and automatically matches the active monitor refresh rate containing Skyrim's window clamped to `1–120` FPS (with `SKYULTRA_CEF_FORCE_120=1`, `SKYULTRA_CEF_DISABLE_GPU_VSYNC=1`, and `SKYULTRA_CEF_DISABLE_FRAME_RATE_LIMIT=1` available as diagnostic overrides). Explicit per-view `max_fps` values clamp to `1–120` FPS in CEF, and initially hidden views warm up one at a time while yielding priority to visible views and menu opens.

Ultralight prewarms its D3D11 deferred-context renderer at startup, sets its display refresh rate from Skyrim's monitor mode clamped to **30–240 Hz**, honors per-view `max_render_fps` (`1–240` FPS or `0` for uncapped runtime default), drives visual `RefreshDisplay(0, target_timestamp)` + `Render()` frames strictly from Skyrim's `PumpFrame` presentation cadence with bounded in-flight GPU command-list backpressure, and uses its `8,333`-microsecond idle worker cadence alongside immediate wakeups for JS timers, queued input, and bridge work.

Native API views (`max_fps == 0` or `SKYULTRA_FPS_UNCAPPED`) and built-in menus/HUD widgets target the active display refresh rate (or up to 120 FPS in CEF / 240 Hz in Ultralight); explicit positive `max_fps` values clamp to `1–120` FPS in CEF and `1–240` FPS in Ultralight, while the temporary diagnostic probe keeps its 15 FPS cap. Hidden views pause, CEF's staggered hidden warmup uses a 15 FPS cap, and idle compositor frames skip D3D11 work. Actual callback, painting and presentation rates depend on the workload and are not performance guarantees.

## One settings file

`Data/SKSE/Plugins/SkyUltra.settings.json` is the single framework settings file. SkyUltra no longer reads or ships `SkyUltra.ini`; an old installed copy is ignored. Source packaging removes stale INI copies from its generated output. Existing menu, per-mod engine, hotkey, splash and profile settings remain in the JSON file. Both browser engines and in-game engine switching remain available.

Advanced startup options live under the optional top-level `runtime` object. Merge this member into your existing settings, preserving other members:

```json
{
  "runtime": {
    "mainWidth": 0,
    "mainHeight": 0,
    "popupWidth": 420,
    "popupHeight": 240,
    "renderMode": "gpu",
    "cefMultithreaded": true,
    "hotReload": false
  }
}
```

The values shown are the defaults. `mainWidth` and `mainHeight` allow integers 0–4096; zero uses the host's viewport/default sizing. Popup dimensions allow integers 128–4096. `renderMode` accepts `gpu` or the CPU fallback `cpu`; VR uses GPU. `cefMultithreaded` controls CEF's message-loop mode. `hotReload` enables the native source-file watcher. Unknown keys, invalid dimensions and wrong value types are rejected. Runtime options apply after restarting Skyrim; normal in-game engine selection still switches in place.

Old INI overrides are not imported. Defaults apply when the `runtime` member is absent. Use the JSON settings for any startup override you still need; there are no FPS settings to migrate.

## Source validation

From the project root:

```powershell
node tools/Check-BridgeDelivery.cjs
node tools/Check-SdkContracts.cjs
node tools/Check-FrontendSource.cjs sdk/typescript sdk/ui
./tools/Check-NativeSource.ps1 -Source src/core/BridgeScript.cpp,src/core/ViewManager.cpp,src/cef/CefBackend.cpp,src/cef/CefSubprocess.cpp,src/ultralight/UltralightBackend.cpp,src/skse/Plugin.cpp,tools/BridgeEventQueueTests.cpp
```

The JavaScript check executes the shared production batch dispatcher and bridge in memory, including 1,024 ordered chunks, reply/error/cancellation handling and package-warning boundaries. MSVC `/Zs` checks C++ syntax and evaluates the admission guard's `static_assert` cases without emitting a binary. The remaining native regression cases in `BridgeEventQueueTests.cpp` require a separately built executable to run. These checks do not build/install a release or verify in-game performance.
