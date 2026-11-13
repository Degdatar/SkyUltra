// Install the manifest and page shown in api/README.md before using this example.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <SkyUltra/SkyUltraAPI.hpp>
#include <array>
#include <string_view>

namespace {
SkyUltra::Client client;
SkyUltraModHandle mod{};
SkyUltraViewHandle view{};
void SKYULTRA_CALL OnRequest(void*,SkyUltraViewHandle sender,
    const char* id,std::uint32_t id_size,const char* name,std::uint32_t name_size,
    const char*,std::uint32_t,SkyUltraPageRequestEventV2 event)
{
    if(sender!=view || event==SKYULTRA_PAGE_REQUEST_CANCELLED_V2)return;
    const std::string_view request_id(id,id_size),method(name,name_size);
    if(method=="sample.ping"){
        (void)client.CompletePageRequestOk(sender,request_id,R"({"message":"Hello from SKSE"})");
        (void)client.SendEventJson(sender,"sample.status",R"({"connected":true})");
    }else (void)client.CompletePageRequestError(sender,request_id,"Unknown sample method");
}
// Call again on a later game update if the host returns NOT_INITIALIZED.
// Never busy-wait or recursively queue retries on the same game task.
void TryInitialize()
{
    if(view)return;
    client=SkyUltra::Client::Acquire();
    if(!client || client.RegisterMod("sample.native",&mod)!=SKYULTRA_OK)return;
    // This example owns the host's global callback slot. Real consumers must
    // coordinate that single slot; registering another callback replaces it.
    if(client.SetPageRequestCallback(OnRequest)!=SKYULTRA_OK)return;
    if(client.CreateView(mod,"sample.native.panel",SkyUltra::Layer::Panel,1280,720,
        SkyUltra::Engine::Default,true,true,false,&view)!=SKYULTRA_OK)return;
    constexpr std::array<std::string_view,2> methods{"ui.pageReady","sample.ping"};
    if(client.SetPageRequestAllowlist(view,methods)!=SKYULTRA_OK){
        (void)client.DestroyView(view);view=0;
    }
}
void OnMessage(SKSE::MessagingInterface::Message* message)
{
    if(!message)return;
    if(message->type==SKSE::MessagingInterface::kDataLoaded ||
       message->type==SKSE::MessagingInterface::kPostLoadGame ||
       message->type==SKSE::MessagingInterface::kNewGame)
        SKSE::GetTaskInterface()->AddTask(TryInitialize);
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    if(const auto* messaging=SKSE::GetMessagingInterface())messaging->RegisterListener(OnMessage);
    return true;
}
