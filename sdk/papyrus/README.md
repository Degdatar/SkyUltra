# SkyUltra Papyrus API

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

The renderer is available without a C++ mod: `SetActorPreview(view, json)`,
`GetActorPreviewStatusJSON(view)`, and `CloseActorPreview(view)`. Grant the matching
`game.setActorPreview`, `game.getActorPreviewStatus`, and `game.closeActorPreview`
methods in both the manifest and the view. Mutations queue onto the game thread;
`True` means accepted, not painted. Status reads return a protected JSON snapshot.
The page receives `skyultra.renderer` results for queued changes. See
[renderer examples and onboarding](../README.md).

`Source/Scripts/SkyUltraNative.psc` declares the native functions registered by
SkyUltra. The package includes this source, the sample quest source, and a
precompiled `Data/Scripts/SkyUltraNative.pex`. Recompile that declaration script
when using newly added native functions. Install the matching web content
under `Data/SkyUltra/ModName/` with its manifest at the root and page assets
under `Files/`. Set the manifest's top-level `browserEngine` to `"ultralight"`
or `"cef"` to pin all views for that mod to one backend. If omitted,
the host's configured default is used (the current CMake default is CEF).
Builds can include both backends so separate mods can use different engines
in one session. Settings expose menu/per-mod choices; a manifest pin and the
available backends also affect selection.

## Open and control a view

```papyrus
Scriptname MyModController extends Quest

Int Property SkyUltraMod Auto
Int Property MainView Auto

Event OnPlayerLoadGame()
    RegisterForSingleUpdate(1.0)
EndEvent

Event OnUpdate()
    If !SkyUltraNative.IsReady()
        RegisterForSingleUpdate(1.0)
        Return
    EndIf

    SkyUltraMod = SkyUltraNative.RegisterMod("ModName")
    If SkyUltraMod == 0
        Debug.Trace("SkyUltra could not register modname. Check its manifest and load order.")
        Return
    EndIf

    MainView = SkyUltraNative.CreateModView(SkyUltraMod, "main", 1280, 720, 0.0, 0.0, 1280.0, 720.0, False)
    If MainView != 0
        SkyUltraNative.AddPageRequestMethod(MainView, "sample.getStatus")
    EndIf
EndEvent

Function OpenPanel()
    SkyUltraNative.SetViewVisible(MainView, True)
EndFunction

Function ClosePanel()
    SkyUltraNative.SetViewVisible(MainView, False)
EndFunction

Function ReleaseView()
    If MainView != 0
        SkyUltraNative.CloseView(MainView)
        MainView = 0
    EndIf
EndFunction
```

Use `CreateFullScreenModView(modHandle, viewName, visible)` when a page should
cover the current Skyrim viewport. SkyUltra reads the running game's viewport
size, so the page does not need a hardcoded resolution. The existing
`CreateModView` remains available for panels and popup-sized views. Papyrus
views are opaque by default; native plug-ins that need transparent HUD layers
can set `SKYULTRA_VIEW_TRANSPARENT` through the C ABI.

`viewName` is unique within the mod. SkyUltra prefixes it with the manifest ID
to create the native view ID. The pixel dimensions set the page's browser
viewport; the bounds set its in-game position and size. A view opens from the
manifest's local entry page. JavaScript, controller navigation, and the mod's
manifest permissions are enabled for the view.

`GetViewState` returns `0` while creation is pending, `1` when ready, `2` when
creation failed, and `3` while closing. It returns `-1` if the handle is no
longer valid. `SetViewBounds`, `SetViewVisible`, `SendEvent`, and `ReloadView`
queue the corresponding operation. `SendEvent` dispatches the same
`skyultra:message` event as the native C ABI.

## Page requests

Web code can call `SkyWebView.request("sample.getStatus", {})`. The method must be
listed in the mod manifest's `permissions.pageRequests` and added to this view's
allowlist with `AddPageRequestMethod`. Both checks are required; SkyUltra
rejects requests that fail either check.

After creating the view, call
`AddPageRequestMethod(MainView, "sample.getStatus")` for each method declared in
`permissions.pageRequests`. The call may be made while the view is pending.

Register for the two SKSE ModEvents from your Papyrus quest:

```papyrus
Event OnInit()
    RegisterForModEvent("SkyUltraPageRequest", "OnSkyUltraPageRequest")
    RegisterForModEvent("SkyUltraPageRequestCancelled", "OnSkyUltraPageRequestCancelled")
EndEvent

Event OnSkyUltraPageRequest(String eventName, String method, Float requestToken, Form sender)
    Int token = requestToken as Int
    String params = SkyUltraNative.GetPageRequestPayload(token)

    If method == "sample.getStatus"
        ; Replace this with data collected by your mod. The response must be JSON.
        SkyUltraNative.CompletePageRequest(token, "{\"state\":\"ready\"}")
    Else
        SkyUltraNative.RejectPageRequest(token, "Unsupported request method")
    EndIf
EndEvent

Event OnSkyUltraPageRequestCancelled(String eventName, String method, Float requestToken, Form sender)
    ; The page closed or aborted its pending request. Do not complete it.
EndEvent
```

SKSE ModEvent callbacks receive four arguments in this order: `eventName`,
`strArg`, `numArg`, and `sender`. For SkyUltra page requests, `strArg` is the
request method and `numArg` is the request token. Refresh ModEvent registrations
after each game load and before opening the page that will issue requests.

The request token is passed through a ModEvent float, but SkyUltra keeps it in
the exact-integer range and Papyrus can safely cast it back to `Int`. Request
payloads and responses are JSON strings. Return valid JSON from
`CompletePageRequest`; use `RejectPageRequest` for errors. A request may be
cancelled when the page unloads, its `AbortSignal` fires, or the view closes.
Papyrus request events are routed for Papyrus-created views. C ABI-created
views continue to use the C ABI request callback.

## Limits

- `RegisterMod` takes the mod's stable identifier, not its display name. The
  native API normalizes ASCII letter case, so `RegisterMod("ModName")` resolves
  the manifest with lowercase ID `modname`. Keep manifest IDs to lowercase
  letters, digits, dots, underscores, and hyphens; choose a slug for each mod
  and replace the sample ID consistently in its manifest and folder. The
  manifest must be installed and valid. Papyrus callers are not
  authenticated to a specific mod ID, so only grant scripts you trust access
  to SkyUltra's Papyrus functions.
- `RegisterModForQuest(Self)` discovers the content folder from the quest's
  owning plugin. Add `"papyrusPlugin": "YourMod.esp"` to that mod's manifest,
  using the ESP/ESM/ESL filename that contains the quest. The native side scans
  `Data/SkyUltra/` for the one manifest associated with that plugin and rejects
  ambiguous matches. `GetModFolderName(handle)` returns the discovered folder
  name for messages such as `SkyUltra Mod : Degdatar`. The MO2 left-pane name
  does not identify a manifest folder by itself.
- The view uses the manifest entry page; Papyrus cannot navigate it to an
  arbitrary URL.
- Page requests need both manifest declaration and per-view allowlisting.
- `SendEvent` takes a JSON object or array string accepted by the native bridge.
- `CloseView` destroys the view and releases its Papyrus handle. Create another
  view if you want to open it again.
- The Papyrus bindings are implemented in source and need a Skyrim runtime
  check after building the plugin.
- `Examples/SkyUltraPapyrusSampleQuest.psc` opens the included `Degdatar` sample
  manifest by matching its `papyrusPlugin` value to the quest's owning plugin,
  then answers `game.getPlayerSnapshot` with live player vitals, equipment, and
  inventory from `SkyUltraNative.GetPlayerSnapshotJSON()`. It also exposes
  `game.getNearbyNpcs` and `game.searchNpcs` through the corresponding native
  JSON functions. Nearby data covers loaded actors inside a requested radius;
  name search covers loaded NPC base records, which may not be spawned nearby.
  These functions do not capture portraits or render a 3D actor preview.

## Test in Skyrim

The UI kit quest also allowlists `game.setActorPreview`,
`game.getActorPreviewStatus`, `game.closeActorPreview`, `game.saveActorPreview`,
`game.captureActorAnimations`, and `game.loadActorPoses`. SkyUltra handles
these directly after the manifest and per-view permission checks; they do not
need new Papyrus native declarations or a separate plugin API. Their target is
a real loaded actor reference (or a base ID resolving to a loaded actor).
Portrait/live view, bounds, rotation, zoom, panning, poses and PNG saves are
page-controlled. Animation capture and independent pose loading use the page bridge and return private
clip IDs/frame counts for the selected model; the existing Papyrus renderer
mutation helper does not synchronously capture clips. PNG save uses the page request/response bridge rather than a
new Papyrus native declaration. See the
TypeScript SDK README for the request schema and material/geometry limits.

For the UI kit development package, the complete user-run build sequence is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\SkyUltra\skyrim\tools\Build-UiKitTest.ps1
```

It compiles both Papyrus scripts, configures/builds both browser backends,
builds the SDK/UI/web packages, and stages both PEX files under
`build/package/Data/Scripts`. It does not install into a game or mod manager.

1. Build SkyUltra with `cmake --build --preset local-release` from
   `D:\SkyUltra\skyrim`. The package should contain
   `Data\Scripts\SkyUltraNative.pex`, `Data\Source\Scripts\SkyUltraNative.psc`,
   and `Data\Source\Scripts\SkyUltraPapyrusSampleQuest.psc`. Install the
   resulting package into the active MO2 profile.
2. Compile `SkyUltraNative.psc`, then `SkyUltraPapyrusSampleQuest.psc`, with the
   Papyrus compiler and attach the sample quest script to a quest in a test
   plugin. Install both resulting PEX files under `Data/Scripts`.
3. Copy `samples/mod-manifest/SkyUltra/Degdatar/` to
   `Data/SkyUltra/Degdatar/`. Keep its `manifest.json` and `Files/` directory
   together. Its manifest ID is `degdatar`, and it already declares
   `game.getPlayerSnapshot`. Set `papyrusPlugin` to the filename of the plugin that
   contains your sample quest.
4. Launch Skyrim through SKSE and load a save. The sample waits for
   `SkyUltraNative.IsReady()`, opens the page, and answers its player snapshot
   request. The page should show live game state. Papyrus
   views are opaque by default. Click the page to interact; press **Escape** or
   **Shift+F6** to hide the browser overlay and return mouse and keyboard
   control to Skyrim. Press **Shift+F6** again to reopen it.

The SKSE log should include `SkyUltra registered its Papyrus view and page
bridge functions.`
