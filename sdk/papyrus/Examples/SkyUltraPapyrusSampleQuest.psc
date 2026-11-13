Scriptname SkyUltraPapyrusSampleQuest extends Quest

Int Property SkyUltraMod Auto
Int Property SampleView Auto

Event OnInit()
    RegisterForSingleUpdate(1.0)
EndEvent

Event OnUpdate()
    String modFolderName = ""

    If !SkyUltraNative.IsReady()
        RegisterForSingleUpdate(1.0)
        Return
    EndIf

    ; A saved quest can run an already-queued update without rerunning OnInit.
    ; Subscribe here before the page is created so that its first request has a listener.
    RegisterForModEvent("SkyUltraPageRequest", "OnSkyUltraPageRequest")
    RegisterForModEvent("SkyUltraPageRequestCancelled", "OnSkyUltraPageRequestCancelled")

    If SampleView != 0
        Return
    EndIf

    SkyUltraMod = SkyUltraNative.RegisterModForQuest(Self)
    If SkyUltraMod == 0
        Debug.Trace("SkyUltra Papyrus could not find or register a mod manifest for this quest's plugin.")
        Debug.Notification("SkyUltra Mod registration failed; check SkyUltra.log.")
        Return
    EndIf
    modFolderName = SkyUltraNative.GetModFolderName(SkyUltraMod)
    Debug.Notification("SkyUltra Mod : " + modFolderName + " registered.")

    SampleView = SkyUltraNative.CreateFullScreenModView(SkyUltraMod, "papyrus-sample", True)
    If SampleView == 0
        Debug.Trace("SkyUltra Papyrus sample could not create its view.")
        Debug.Notification("SkyUltra Mod : " + modFolderName + " view creation failed; check SkyUltra.log.")
        Return
    EndIf
    Debug.Notification("SkyUltra Mod : " + modFolderName + " browser view created.")

    If !SkyUltraNative.AddPageRequestMethod(SampleView, "game.getPlayerSnapshot")
        Debug.Trace("SkyUltra Papyrus sample could not allow the game.getPlayerSnapshot request.")
        Debug.Notification("SkyUltra Mod : " + modFolderName + " request setup failed; check SkyUltra.log.")
    EndIf
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.getNearbyNpcs")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.searchNpcs")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.subscribe")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.unsubscribe")
    ; These methods are handled directly by SkyUltra after permission checks.
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.setActorPreview")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.getActorPreviewStatus")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.closeActorPreview")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.saveActorPreview")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.captureActorAnimations")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.loadActorPoses")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.setItemPreview")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.getItemPreviewStatus")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.closeItemPreview")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.saveItemPreview")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.getInventory")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.getItemThumbnail")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.getActorThumbnail")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.equipItem")
    SkyUltraNative.AddPageRequestMethod(SampleView, "game.unequipItem")
    SkyUltraNative.AddPageRequestMethod(SampleView, "ui.pageReady")
    SkyUltraNative.AddPageRequestMethod(SampleView, "ui.configureWidgets")
    SkyUltraNative.AddPageRequestMethod(SampleView, "ui.setInputScope")
    SkyUltraNative.AddPageRequestMethod(SampleView, "ui.getPerformanceSnapshot")
EndEvent

Event OnSkyUltraPageRequest(String eventName, String method, Float requestToken, Form sender)
    Int token = requestToken as Int
    String params = SkyUltraNative.GetPageRequestPayload(token)
    If method == "game.getPlayerSnapshot"
        String snapshot = SkyUltraNative.GetPlayerSnapshotJSON()
        If !SkyUltraNative.CompletePageRequest(token, snapshot)
            Debug.Trace("SkyUltra Papyrus sample could not complete a page request.")
            Debug.Notification("SkyUltra Mod : Could not send the player snapshot.")
        EndIf
    ElseIf method == "game.getNearbyNpcs"
        String actors = SkyUltraNative.GetNearbyNpcsJSON(params)
        SkyUltraNative.CompletePageRequest(token, actors)
    ElseIf method == "game.searchNpcs"
        String npcs = SkyUltraNative.SearchNpcsJSON(params)
        SkyUltraNative.CompletePageRequest(token, npcs)
    Else
        SkyUltraNative.RejectPageRequest(token, "Unsupported sample request")
    EndIf
EndEvent

Event OnSkyUltraPageRequestCancelled(String eventName, String method, Float requestToken, Form sender)
    Debug.Trace("SkyUltra Papyrus sample request was cancelled: " + method)
EndEvent

Function CloseSampleView()
    If SampleView != 0
        SkyUltraNative.CloseView(SampleView)
        SampleView = 0
    EndIf
EndFunction
