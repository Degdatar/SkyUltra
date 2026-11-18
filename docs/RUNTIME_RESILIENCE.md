# Runtime controls, resource budgets and CEF diagnostics

These controls help identify failures and stop one mod's browser workload. They do not provide a separate Chromium GPU process for each mod, and do not establish a fix for an upstream GPU/ANGLE/driver crash.

## Suspend, resume and unload

Developer Studio provides **Suspend mod**, **Resume mod** and **Unload mod** on registered mod/view cards. SkyUltra's own menu cannot suspend or unload itself.

- **Suspend** closes the mod's browser instances, releases their rendering resources as teardown finishes, cancels outstanding page requests and stops subscriptions. Native view handles and their configuration are retained. Page memory, unsaved document state and JavaScript state are lost.
- **Resume** re-enables the mod and creates fresh documents for suspended views. Pages must repeat their ready handshake, event listeners and data subscriptions, as after a reload. Accepted queued events remain bounded; an in-flight batch cancelled with the old document is not replayed.
- **Unload** destroys all the mod's native view handles. The mod remains disabled until Resume re-enables creation. Its hotkey or native/Papyrus owner can then create new views; old handles remain invalid.

These choices apply for the current game process and are not written to persistent settings. Hotkeys and native `set_visible` cannot bypass a suspension. HUDs owned by that mod are included. Other mods and SkyUltra's menu keep their own visibility and hotkeys.

The privileged page method is `ui.manageModRuntime`, granted only to the built-in menu. SDK callers can use:

```ts
import { manageSkyUltraModRuntime } from '@skyultra/sdk/devtools';
await manageSkyUltraModRuntime('author.mod', 'suspend', bridge);
await manageSkyUltraModRuntime('author.mod', 'resume', bridge);
```

Calling from an ordinary mod page cannot grant itself the built-in menu's privilege. Developer snapshot records expose `resources.suspended` and per-view suspension/recovery information.

## Resource budgets

The existing per-view limits remain: **1,024 events / 64 MiB**, a separate reply queue with the same limits, and a reserved, bounded host-control lane. In-flight batches still reserve capacity.

All views owned by one registered mod additionally share:

| Resource | Per-mod limit |
| --- | --- |
| Reserved native views | 16 |
| Primary RGBA surface reservation | 128 MiB, calculated as width × height × 4 |
| Ordinary events across all views | 4,096 messages / 128 MiB |
| Replies across all views | Separate 4,096 messages / 128 MiB |

Reservations include pending views, suspended handles and resources still completing teardown. Legacy unregistered native views share owner `0`'s budget. Reserved host controls retain their separate per-view lane, bounded by the view-count limit.

The surface budget is a logical admission estimate, **not a measurement or hard cap on total browser VRAM**. CEF/ANGLE caches, WebGL allocations, extra host buffers and browser process memory are additional. The queues count names/JSON payloads, not every allocation made when JavaScript parses them. A mod can still have expensive code within these limits.

`SKYULTRA_RESOURCE_LIMIT` (`8`) rejects view/surface admission. `SKYULTRA_QUEUE_FULL` (`7`) rejects queue admission, including aggregate limits. `SKYULTRA_SUSPENDED` (`9`) rejects event delivery or opening a view for a suspended owner. These are additional enum values; existing C ABI function-table layouts and versions remain unchanged. Check every result; retain unaccepted data and retry later with backoff, never by spinning on the game thread.

Developer Studio shows reserved views, surface estimates, event/reply totals and admission rejections. Budget exhaustion is separate from GPU-process failure; the supplied crash logs did not show queue saturation.

## CEF failure handling

Renderer termination now clears obsolete input, in-flight batch acknowledgements and cached host frame/texture-pool references before trying the same document again. Retries use delays of **250 ms, 1 second and 4 seconds**, with at most three automatic retries in a 60-second recovery window. Repeated failure stops automatic reloads; the user can inspect and suspend/reload the mod. A closed or suspended view is not reopened by a stale delayed retry.

A bridge batch still outstanding after 10 seconds is reported as stalled. Accepted commands are not blindly replayed, since that could repeat gameplay actions. Reloading or suspending the document cancels its outstanding batch and releases its reservations.

The CEF subprocess observes WebGL context-loss notifications without cancelling or replacing the mod's own event handling. Context loss is not proof of a GPU-process crash: a page can intentionally lose a WebGL context. Developer Studio therefore reports renderer crashes and context-loss signals separately. Chromium's GPU process remains shared, so these counters do not promise complete per-mod GPU isolation.

SkyUltra no longer overrides Chromium's renderer/background-window scheduling with blanket background-priority switches. Hidden views still use `WasHidden`, and explicit Suspend unloads pages so their scripts stop too. CEF's **120 FPS maximum**, normal lower per-view caps, and Ultralight pacing remain unchanged. These changes need in-game profiling; source checks alone do not establish a performance improvement.

## Compact CEF runtime layout

Packaged releases trim the CEF runtime under `SKSE/Plugins/SkyUltra/CEF/` to **16 runtime files** by keeping only `locales/en-US.pak` (aligned with `settings.locale = "en-US"`) and staging `SkyUltraCefSubprocess.pdb` separately under `Diagnostics/`. Keeping `SkyUltraCefSubprocess.pdb` alongside release artifacts outside the player's `Data/` mod tree lets developers symbolicate crash dumps without shipping extra locales or PDB bloat inside normal gameplay installs.

## Optional local CEF crash dumps

CEF/Crashpad reads `crash_reporter.cfg` beside the **game executable**, not SkyUltra's settings JSON or the plugin DLL. Because that location is shared with other integrations, crash reporting is an explicit optional setup. SkyUltra does not silently overwrite another application's configuration. `SkyUltra.ini` remains retired.

With Skyrim closed and the updated CEF runtime installed, run from the source checkout (or use the copy shipped under the release's `Diagnostics` directory):

```powershell
./tools/Enable-CefCrashReports.ps1 -GameDirectory 'D:\Steam\steamapps\common\Skyrim Special Edition'
```

For MO2, run where the subprocess is visible, or supply its physical path:

```powershell
./tools/Enable-CefCrashReports.ps1 -GameDirectory 'D:\Steam\steamapps\common\Skyrim Special Edition' -SubprocessPath 'D:\MO2\mods\SkyUltra\SKSE\Plugins\SkyUltra\CEF\SkyUltraCefSubprocess.exe'
```

The tool verifies the game/helper paths, refuses to replace an unrecognized existing configuration, sets an explicit external crash handler, leaves `ServerURL` empty, and limits retention to **128 MB / 7 days**. Reports stay under `%LOCALAPPDATA%\SkyUltraCEF\User Data`; nothing is uploaded by this configuration. Dumps may contain process memory and file paths; review them before sharing.

Restart Skyrim and check `SkyUltra.log` for `CEF crash reporting enabled=true`. A missing configuration or an existing third-party configuration may behave differently; the enabled flag alone does not prove a dump has been captured. Chromium's own standard metadata identifies the subprocess; SkyUltra adds process/view/operation keys when available. Use matching CEF and SkyUltra symbols to analyze a dump. Release builds retain symbols for the subprocess too.

Disable only the tool's managed configuration with:

```powershell
./tools/Enable-CefCrashReports.ps1 -GameDirectory 'D:\Steam\steamapps\common\Skyrim Special Edition' -Disable
```

No game-root configuration or registry setting is changed merely by building the project. Source behavior follows the bundled [CEF crash-reporting API](https://github.com/chromiumembedded/cef/blob/14c5a08/include/cef_crash_util.h) and [official crash-reporting documentation](https://chromiumembedded.github.io/cef/crash_reporting).

## Source-only verification

```powershell
./tools/Check-CefCrashSetup.ps1
node tools/Check-RuntimeResilience.cjs
node tools/Check-FrontendSource.cjs sdk/typescript sdk/ui apps/skyultra-menu
./tools/Check-NativeSource.ps1 -Source src/core/ViewManager.cpp,src/cef/CefBackend.cpp,src/cef/CefSubprocess.cpp,src/skse/Plugin.cpp,tools/ResourceBudgetTests.cpp
```

The setup test uses empty workspace-only placeholder files; it does not launch Skyrim or CEF or alter the real game directory. The JavaScript checks execute SDK code in memory. `/Zs` evaluates the native constexpr admission/retry cases without emitting a binary. The additional native runtime scenarios in `ResourceBudgetTests.cpp` need a separately authorized executable build. GPU-crash recovery, dump generation and performance still need a rebuilt installation and in-game verification.
