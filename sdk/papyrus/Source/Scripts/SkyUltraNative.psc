Scriptname SkyUltraNative Hidden

; All returned handles are positive session-local integers. Zero means failure.
; GetViewState returns 0 pending, 1 ready, 2 failed, or 3 closing; -1 means invalid.
Int Function RegisterMod(String modId) Global Native
Int Function RegisterModForQuest(Form ownerQuest) Global Native
String Function GetModFolderName(Int modHandle) Global Native
String Function GetPlayerSnapshotJSON() Global Native
; paramsJson: {"query":"lydia","limit":40}; search needs 3+ characters, max 50.
String Function SearchNpcsJSON(String paramsJson) Global Native
; paramsJson: {"radius":6000}; radius is clamped to 500..20000 game units, max 40 actors.
String Function GetNearbyNpcsJSON(String paramsJson) Global Native
Bool Function IsReady() Global Native
Int Function CreateModView(Int modHandle, String viewName, Int pixelWidth, Int pixelHeight, Float x, Float y, Float width, Float height, Bool visible) Global Native
Int Function CreateFullScreenModView(Int modHandle, String viewName, Bool visible) Global Native
Int Function GetViewState(Int viewHandle) Global Native
Bool Function SetViewVisible(Int viewHandle, Bool visible) Global Native
Bool Function SetViewBounds(Int viewHandle, Float x, Float y, Float width, Float height) Global Native
Bool Function SendEvent(Int viewHandle, String eventName, String json) Global Native
Bool Function AddPageRequestMethod(Int viewHandle, String method) Global Native
Bool Function RemovePageRequestMethod(Int viewHandle, String method) Global Native
Bool Function ReloadView(Int viewHandle) Global Native
Bool Function CloseView(Int viewHandle) Global Native

; Subscribe to SkyUltraPageRequest and SkyUltraPageRequestCancelled with
; RegisterForModEvent. The event's Float token is an exact integer; cast it to Int.
String Function GetPageRequestPayload(Int requestToken) Global Native
Bool Function CompletePageRequest(Int requestToken, String resultJson) Global Native
Bool Function RejectPageRequest(Int requestToken, String error) Global Native

; Renderer: queued game-thread mutation; returns whether accepted, not whether a frame is ready.
; Add renderer methods to manifest and AddPageRequestMethod for this view first.
; The page receives skyultra.renderer; scripts can poll GetActorPreviewStatusJSON.
Bool Function SetActorPreview(Int viewHandle, String paramsJson) Global Native
Bool Function CloseActorPreview(Int viewHandle) Global Native
String Function GetActorPreviewStatusJSON(Int viewHandle) Global Native
