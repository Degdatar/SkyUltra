#include "SkyUltra/VideoSplash.h"

// This file belongs to skyultra_video, not the Windows 7 CommonLib/PCH target.
#if !defined(WINVER) || WINVER < 0x0602 || !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
#    error "Compile VideoSplash.cpp in the independent skyultra_video target with Windows 8 SDK declarations."
#endif

#include <spdlog/spdlog.h>
#include <Windows.h>
#include <d3d11_1.h>
#include <d3d10.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <algorithm>
#include <utility>

namespace skyultra {
namespace {
using Microsoft::WRL::ComPtr;
struct Signals { std::atomic_bool can_play{}, ended{}, failed{}; std::atomic<DWORD> error_code{}, media_error{}; };
class Notify final : public IMFMediaEngineNotify {
    std::atomic_ulong refs_{1};
    std::shared_ptr<Signals> signals_;
public:
    explicit Notify(std::shared_ptr<Signals> signals):signals_(std::move(signals)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out=nullptr;
        if (iid==__uuidof(IUnknown)||iid==__uuidof(IMFMediaEngineNotify)) { *out=static_cast<IMFMediaEngineNotify*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override {const auto n=--refs_;if(n==0)delete this;return n;}
    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event,DWORD_PTR parameter1,DWORD parameter2) override {
        // No engine calls, UI locks, or D3D operations from Media Foundation's callback.
        if(event==MF_MEDIA_ENGINE_EVENT_CANPLAY||event==MF_MEDIA_ENGINE_EVENT_SEEKED)signals_->can_play=true;
        if(event==MF_MEDIA_ENGINE_EVENT_ENDED)signals_->ended=true;
        if(event==MF_MEDIA_ENGINE_EVENT_ERROR){signals_->media_error=static_cast<DWORD>(parameter1);signals_->error_code=parameter2;signals_->failed=true;}
        return S_OK;
    }
};
class SplashPlayer {
public:
    HMODULE mf_module{};
    decltype(&MFShutdown) shutdown{};
    decltype(&MFCreateDXGIDeviceManager) create_manager{};
    decltype(&MFCreateAttributes) create_attributes{};
    decltype(&MFCreateFile) create_file{};
    ComPtr<IMFMediaEngine> engine;
    ComPtr<IMFByteStream> source_stream;
    ComPtr<IMFDXGIDeviceManager> manager;
    ComPtr<ID3D11Device> media_device;
    ComPtr<ID3D11DeviceContext> media_context;
    ComPtr<ID3D11Texture2D> producer,shared,display,decode_target;
    ComPtr<IWICBitmap> software_bitmap;
    ComPtr<IDXGIKeyedMutex> producer_mutex,consumer_mutex;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11Device> owner;
    std::shared_ptr<Signals> signals=std::make_shared<Signals>();
    bool mf{},playing{},warming_first_frame{},has_frame{},primed{},frame_transferred{},software_transfer{},pending_consumer_copy{};
    const char* stage{"Media Foundation startup"};
    bool Check(HRESULT hr,const char* operation){if(SUCCEEDED(hr))return true;stage=operation;signals->error_code=static_cast<DWORD>(hr);signals->failed=true;return false;}
    ~SplashPlayer(){if(engine){engine->Pause();engine->Shutdown();engine.Reset();}source_stream.Reset();manager.Reset();if(mf)shutdown();if(mf_module)FreeLibrary(mf_module);}
    HRESULT Open(ID3D11Device* host, const VideoSplashSettings& settings, bool compatible = false) {
        owner=host;
        software_transfer=compatible;
        // Load lazily so a Windows installation without Media Foundation can still load SkyUltra.
        mf_module=LoadLibraryExW(L"mfplat.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!mf_module)return HRESULT_FROM_WIN32(GetLastError());
        const auto startup=reinterpret_cast<decltype(&MFStartup)>(GetProcAddress(mf_module,"MFStartup"));
        shutdown=reinterpret_cast<decltype(&MFShutdown)>(GetProcAddress(mf_module,"MFShutdown"));
        create_manager=reinterpret_cast<decltype(&MFCreateDXGIDeviceManager)>(GetProcAddress(mf_module,"MFCreateDXGIDeviceManager"));
        create_attributes=reinterpret_cast<decltype(&MFCreateAttributes)>(GetProcAddress(mf_module,"MFCreateAttributes"));
        create_file=reinterpret_cast<decltype(&MFCreateFile)>(GetProcAddress(mf_module,"MFCreateFile"));
        if(!startup||!shutdown||!create_manager||!create_attributes||!create_file)return E_NOTIMPL;
        auto hr=startup(MF_VERSION,MFSTARTUP_FULL);if(FAILED(hr))return hr;mf=true;
        if(!software_transfer){
            ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;
            stage="Find splash video adapter";
            if(FAILED(hr=host->QueryInterface(IID_PPV_ARGS(&dxgi)))||FAILED(hr=dxgi->GetAdapter(&adapter)))return hr;
            // Only the media device is protected for MF worker-thread access. Never change Skyrim's context.
            stage="Create splash video device";
            if(FAILED(hr=D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
                    &media_device,nullptr,&media_context)))return hr;
            ComPtr<ID3D10Multithread> multithread;
            if(SUCCEEDED(media_context.As(&multithread)))multithread->SetMultithreadProtected(TRUE);
            UINT token{};
            stage="Create splash video device manager";
            if(FAILED(hr=create_manager(&token,&manager))||FAILED(hr=manager->ResetDevice(media_device.Get(),token)))return hr;
        }
        ComPtr<IMFAttributes> attributes;ComPtr<IMFMediaEngineClassFactory> factory;
        ComPtr<IMFMediaEngineNotify> callback;callback.Attach(new Notify(signals));
        stage="Create splash media engine";
        if(FAILED(hr=create_attributes(&attributes,3))||
           FAILED(hr=attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK,callback.Get()))||
           (!software_transfer&&FAILED(hr=attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER,manager.Get())))||
           FAILED(hr=attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT,
               software_transfer?DXGI_FORMAT_B8G8R8X8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM))||
           FAILED(hr=CoCreateInstance(CLSID_MFMediaEngineClassFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
           FAILED(hr=factory->CreateInstance(0,attributes.Get(),&engine)))return hr;
        stage="Configure splash audio";
        if(FAILED(hr=engine->SetMuted(!settings.audio))||FAILED(hr=engine->SetVolume(settings.volume)))return hr;
        if(FAILED(hr=engine->SetPreload(MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC)))return hr;
        // Resolve against Skyrim.exe, not the launcher's working directory. Open the
        // compressed file inside the game process so virtualized mod files stay visible.
        stage="Open SkyUltraSplash.mp4";
        std::wstring executable(32768,L'\0');
        const auto length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
        if(!length||length>=executable.size())return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);
        executable.resize(length);
        const auto data=std::filesystem::path(executable).parent_path()/L"Data";
        auto source=data/L"SkyUltra"/L"skyultra.menu"/L"Files"/L"SkyUltraSplash.mp4";
        hr=create_file(MF_ACCESSMODE_READ,MF_OPENMODE_FAIL_IF_NOT_EXIST,MF_FILEFLAGS_NONE,source.c_str(),&source_stream);
        if(hr==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)||hr==HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)){
            source=data/L"SKSE"/L"Plugins"/L"resources"/L"SkyUltra"/L"Files"/L"SkyUltraSplash.mp4";
            hr=create_file(MF_ACCESSMODE_READ,MF_OPENMODE_FAIL_IF_NOT_EXIST,MF_FILEFLAGS_NONE,source.c_str(),&source_stream);
        }
        if(FAILED(hr))return hr;
        spdlog::info("SkyUltra opened splash MP4 '{}' ({} frames; audio {}; volume {:.0f}%).",source.string(),software_transfer?"compatible software":"shared GPU",settings.audio,settings.volume*100);
        ComPtr<IMFMediaEngineEx> extended;
        if(FAILED(hr=engine.As(&extended)))return hr;
        BSTR url=SysAllocString(source.c_str());if(!url)return E_OUTOFMEMORY;
        stage="SetSourceFromByteStream";
        hr=extended->SetSourceFromByteStream(source_stream.Get(),url);SysFreeString(url);if(FAILED(hr))return hr;
        // SetSourceFromByteStream starts the asynchronous load. Calling Load()
        // again restarts source selection from the URL and loses the supplied
        // stream (the former bare filename failed with HRESULT 0x80070002).
        // Keep the stream alive and wait for CANPLAY instead of reloading it.
        stage="Loading splash screen from stream";
        return S_OK;
    }
    HRESULT MakeSoftwareTargets(DWORD width, DWORD height) {
        // Media Engine's WIC frame-server mode must be created WITHOUT a DXGI
        // manager. It still renders the MP4 audio normally, but needs no shared
        // textures or synchronization changes on Skyrim's graphics device.
        stage="Create compatible video bitmap factory";
        ComPtr<IWICImagingFactory> factory;
        auto hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
        if(FAILED(hr))return hr;
        stage="Create compatible video bitmap";
        if(FAILED(hr=factory->CreateBitmap(width,height,GUID_WICPixelFormat32bppBGR,WICBitmapCacheOnLoad,&software_bitmap)))return hr;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8X8_UNORM;desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        stage="Create compatible game video texture";
        if(FAILED(hr=owner->CreateTexture2D(&desc,nullptr,&display)))return hr;
        stage="Create compatible game video shader resource view";
        if(FAILED(hr=owner->CreateShaderResourceView(display.Get(),nullptr,&srv)))return hr;
        spdlog::info("SkyUltra splash screen compatible targets ready ({}x{}, no shared GPU textures).",width,height);
        return S_OK;
    }
    HRESULT MakeSharedTargets(D3D11_TEXTURE2D_DESC desc, bool nt_handle) {
        // Publish only a complete pair: a failed NT-handle attempt must not
        // leave partial textures/mutexes behind for the compatibility path.
        ComPtr<ID3D11Texture2D> next_producer,next_shared;
        ComPtr<IDXGIKeyedMutex> next_producer_mutex,next_consumer_mutex;
        desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        if(nt_handle)desc.MiscFlags|=D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        stage=nt_handle?"Create NT-handle shared video texture":"Create legacy shared video texture";
        auto hr=media_device->CreateTexture2D(&desc,nullptr,&next_producer);
        if(FAILED(hr))return hr;
        stage="Query producer video keyed mutex";
        if(FAILED(hr=next_producer.As(&next_producer_mutex)))return hr;
        if(nt_handle){
            ComPtr<IDXGIResource1> resource;ComPtr<ID3D11Device1> host1;
            stage="Query NT-handle video resource";
            if(FAILED(hr=next_producer.As(&resource)))return hr;
            stage="Query game D3D11.1 device for video";
            if(FAILED(hr=owner.As(&host1)))return hr;
            HANDLE handle{};
            stage="Create video NT shared handle";
            if(FAILED(hr=resource->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&handle)))return hr;
            stage="Open NT-handle video texture on game device";
            hr=host1->OpenSharedResource1(handle,IID_PPV_ARGS(&next_shared));
            CloseHandle(handle);
        }else{
            ComPtr<IDXGIResource> resource;
            stage="Query legacy shared video resource";
            if(FAILED(hr=next_producer.As(&resource)))return hr;
            HANDLE handle{};
            stage="Get legacy video shared handle";
            if(FAILED(hr=resource->GetSharedHandle(&handle)))return hr;
            // A legacy DXGI handle belongs to the resource; never CloseHandle it.
            stage="Open legacy video texture on game device";
            hr=owner->OpenSharedResource(handle,IID_PPV_ARGS(&next_shared));
        }
        if(FAILED(hr))return hr;
        stage="Query consumer video keyed mutex";
        if(FAILED(hr=next_shared.As(&next_consumer_mutex)))return hr;
        producer=std::move(next_producer);shared=std::move(next_shared);
        producer_mutex=std::move(next_producer_mutex);consumer_mutex=std::move(next_consumer_mutex);
        return S_OK;
    }
    HRESULT MakeTargets() {
        stage="Get native video dimensions";
        DWORD width{},height{};auto hr=engine->GetNativeVideoSize(&width,&height);
        if(FAILED(hr))return hr;
        if(width==0||height==0||width>4096||height>4096)return E_INVALIDARG;
        if(software_transfer)return MakeSoftwareTargets(width,height);
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        // MF owns its own device synchronization. Do not ask TransferVideoFrame to
        // lock a shared resource whose keyed mutex the caller already holds.
        stage="Create private video decode target";
        if(FAILED(hr=media_device->CreateTexture2D(&desc,nullptr,&decode_target)))return hr;
        bool nt_handle=false;
        if(FAILED(hr=MakeSharedTargets(desc,false))){
            spdlog::warn("SkyUltra splash screen legacy sharing failed at {} (HRESULT 0x{:08X}); trying NT-handle GPU sharing.",stage,static_cast<std::uint32_t>(hr));
            nt_handle=true;
            if(FAILED(hr=MakeSharedTargets(desc,true)))return hr;
        }
        desc.MiscFlags=0;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        stage="Create game video display texture";
        if(FAILED(hr=owner->CreateTexture2D(&desc,nullptr,&display)))return hr;
        stage="Create game video shader resource view";
        if(FAILED(hr=owner->CreateShaderResourceView(display.Get(),nullptr,&srv)))return hr;
        spdlog::info("SkyUltra splash screen GPU targets ready ({}x{}, {} sharing).",width,height,nt_handle?"NT-handle":"legacy keyed-mutex");
        return S_OK;
    }
    void MarkFrameDelivered() {
        if(!has_frame)spdlog::info("SkyUltra splash screen delivered its first video frame ({}).",software_transfer?"compatible software":"shared GPU");
        has_frame=true;
        frame_transferred=true;
    }
    void ApplyAudioSettings(const VideoSplashSettings& settings) {
        if(!engine)return;
        (void)engine->SetMuted(!settings.audio);
        (void)engine->SetVolume(settings.volume);
    }
    void RewindAndPause() {
        if(engine){
            (void)engine->Pause();
            (void)engine->SetCurrentTime(0.0);
        }
        playing=false;
        warming_first_frame=false;
        primed=false;
        pending_consumer_copy=false;
        if(signals)signals->ended=false;
    }
    void TransferCurrentFrame(ID3D11DeviceContext* host_context) {
        LONGLONG pts{};
        const auto tick=engine->OnVideoStreamTick(&pts);
        if(!Check(tick,"OnVideoStreamTick"))return;
        if(software_transfer){
            if(tick!=S_OK)return;
            D3D11_TEXTURE2D_DESC desc{};display->GetDesc(&desc);
            const RECT destination{0,0,static_cast<LONG>(desc.Width),static_cast<LONG>(desc.Height)};
            MFARGB border{};border.rgbAlpha=255;
            if(!Check(engine->TransferVideoFrame(software_bitmap.Get(),nullptr,&destination,&border),"Transfer compatible video frame"))return;
            ComPtr<IWICBitmapLock> bitmap_lock;
            if(!Check(software_bitmap->Lock(nullptr,WICBitmapLockRead,&bitmap_lock),"Lock compatible video frame"))return;
            UINT stride{},bytes{};BYTE* pixels{};
            if(!Check(bitmap_lock->GetStride(&stride),"Get compatible video stride")||
               !Check(bitmap_lock->GetDataPointer(&bytes,&pixels),"Get compatible video pixels"))return;
            if(!pixels||stride<desc.Width*4||static_cast<std::uint64_t>(stride)*(desc.Height-1)+desc.Width*4>bytes){
                Check(E_INVALIDARG,"Validate compatible video pixels");return;
            }
            host_context->UpdateSubresource(display.Get(),0,nullptr,pixels,stride,0);
            MarkFrameDelivered();return;
        }
        if(tick==S_OK){
            D3D11_TEXTURE2D_DESC desc{};producer->GetDesc(&desc);
            const RECT destination{0,0,static_cast<LONG>(desc.Width),static_cast<LONG>(desc.Height)};
            MFARGB border{};border.rgbAlpha=255;
            if(!Check(engine->TransferVideoFrame(decode_target.Get(),nullptr,&destination,&border),"TransferVideoFrame"))return;
            if(producer_mutex->AcquireSync(0,0)==S_OK){
                media_context->CopyResource(producer.Get(),decode_target.Get());
                if(!Check(producer_mutex->ReleaseSync(1),"Release producer video keyed mutex"))return;
                media_context->Flush();
                pending_consumer_copy=true;
            }
        }
        // Copy from the shared texture only when a producer frame is waiting.
        if(pending_consumer_copy&&consumer_mutex->AcquireSync(1,0)==S_OK){
            host_context->CopyResource(display.Get(),shared.Get());
            if(!Check(consumer_mutex->ReleaseSync(0),"Release consumer video keyed mutex"))return;
            pending_consumer_copy=false;
            MarkFrameDelivered();
        }
    }
    void PrimeFirstFrame(ID3D11DeviceContext* host_context, const VideoSplashSettings& settings) {
        if(!signals->can_play||signals->failed||primed)return;
        if(!srv){const auto hr=MakeTargets();if(!Check(hr,stage))return;}
        if(!warming_first_frame){
            (void)engine->SetMuted(TRUE);
            if(!Check(engine->Play(),"Prime first video frame"))return;
            warming_first_frame=true;
        }
        frame_transferred=false;
        TransferCurrentFrame(host_context);
        if(frame_transferred){
            if(engine)(void)engine->Pause();
            playing=false;
            warming_first_frame=false;
            primed=true;
            if(signals)signals->ended=false;
            ApplyAudioSettings(settings);
        }
    }
    void Tick(ID3D11DeviceContext* host_context, const VideoSplashSettings& settings) {
        if(!signals->can_play||signals->failed)return;
        // Keep the exact failing operation instead of replacing every target
        // failure with the same generic stage.
        if(!srv){const auto hr=MakeTargets();if(!Check(hr,stage))return;}
        if(warming_first_frame){
            warming_first_frame=false;
            playing=true;
        }
        // Do not start audio until a usable video destination exists.
        if(!playing){
            ApplyAudioSettings(settings);
            if(!Check(engine->Play(),"Play video with audio"))return;
            playing=true;
        }
        TransferCurrentFrame(host_context);
    }
};
std::mutex mutex;
VideoSplashSettings configuration;
std::string active_key;
std::unordered_set<std::string> seen_mods;
bool seen_session{},ready{},start{},preview_full_duration{};
std::string active_mod;
VideoSplashStatus status;
std::atomic_bool active{};
std::chrono::steady_clock::time_point started;
std::unique_ptr<SplashPlayer> splash_player;
std::atomic_bool needs_tick{};
std::atomic<ID3D11Device*> warmup_device{};
std::atomic_bool warmup_done{};
void Stop(){
    active=false;start=false;
    if(splash_player){
        if(!splash_player->signals->failed){
            splash_player->RewindAndPause();
            const bool session_completed = (configuration.replay == "session" && seen_session);
            warmup_done.store(session_completed,std::memory_order_relaxed);
        } else {
            splash_player.reset();
            warmup_done.store(false,std::memory_order_relaxed);
        }
    }
}
}
void ConfigureVideoSplash(VideoSplashSettings settings){
    const std::scoped_lock lock(mutex);
    configuration=std::move(settings);
    if(splash_player)splash_player->ApplyAudioSettings(configuration);
    if(!configuration.enabled)Stop();
}
void EnsureThreadComApartment() noexcept {
    struct ThreadApartment {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~ThreadApartment() {
            if (SUCCEEDED(hr)) {
                CoUninitialize();
            }
        }
    };
    thread_local ThreadApartment s_apartment;
    (void)s_apartment;
}
void WarmupFrameworkSplash(ID3D11Device* host, ID3D11DeviceContext* context){
    if(host==nullptr||context==nullptr||active.load(std::memory_order_relaxed))return;
    if(warmup_done.load(std::memory_order_relaxed)&&warmup_device.load(std::memory_order_relaxed)==host)return;
    EnsureThreadComApartment();
    const std::unique_lock lock(mutex,std::try_to_lock);
    if(!lock.owns_lock()||!configuration.enabled||active.load(std::memory_order_relaxed))return;
    if(configuration.replay=="session"&&seen_session){
        warmup_device.store(host,std::memory_order_relaxed);
        warmup_done.store(true,std::memory_order_relaxed);
        return;
    }
    if(!splash_player||splash_player->owner.Get()!=host){
        splash_player.reset();
        splash_player=std::make_unique<SplashPlayer>();
        auto hr=splash_player->Open(host,configuration,false);
        if(FAILED(hr)){
            splash_player=std::make_unique<SplashPlayer>();
            hr=splash_player->Open(host,configuration,true);
            if(FAILED(hr)){
                splash_player.reset();
                warmup_device.store(host,std::memory_order_relaxed);
                warmup_done.store(true,std::memory_order_relaxed);
                return;
            }
        }
        warmup_device.store(host,std::memory_order_relaxed);
        warmup_done.store(false,std::memory_order_relaxed);
    }
    if(splash_player->signals->failed&&!splash_player->software_transfer&&!splash_player->has_frame){
        splash_player=std::make_unique<SplashPlayer>();
        if(FAILED(splash_player->Open(host,configuration,true))){
            splash_player.reset();
            warmup_done.store(true,std::memory_order_relaxed);
            return;
        }
    }
    if(splash_player->signals->failed){
        warmup_done.store(true,std::memory_order_relaxed);
        return;
    }
    if(splash_player->signals->can_play){
        splash_player->PrimeFirstFrame(context,configuration);
        if(splash_player->signals->failed&&!splash_player->software_transfer&&!splash_player->has_frame){
            splash_player=std::make_unique<SplashPlayer>();
            if(FAILED(splash_player->Open(host,configuration,true))){
                splash_player.reset();
                warmup_done.store(true,std::memory_order_relaxed);
            }
            return;
        }
        if(splash_player->primed||splash_player->signals->failed){
            warmup_done.store(true,std::memory_order_relaxed);
        }
    }
}
void BeginFrameworkSplash(std::string key,std::string mod,bool page_ready,bool force_replay){
    const std::scoped_lock lock(mutex);
    if(!configuration.enabled||active||(!force_replay&&!configuration.full_duration&&page_ready)|| (!force_replay&&((configuration.replay=="session"&&seen_session)||
        (configuration.replay=="per-mod"&&seen_mods.contains(mod)))))return;
    active_mod=std::move(mod);active_key=std::move(key);ready=page_ready;active=start=true;preview_full_duration=force_replay;
    status={};status.stage="Starting video";
    if(splash_player&&splash_player->has_frame&&!splash_player->signals->failed){
        status.has_frame=true;
        status.stage="Playing video";
        splash_player->playing=false;
    }
    needs_tick=true;
    started=std::chrono::steady_clock::now();
    spdlog::info("SkyUltra splash screen begins for '{}' (replay {}; full duration {}; preview {}; audio {}).",active_key,configuration.replay,configuration.full_duration||preview_full_duration,force_replay,configuration.audio);
}
void CompleteFrameworkSplash(std::string_view key){const std::scoped_lock lock(mutex);if(active_key==key)ready=true;}
void CancelFrameworkSplash(std::string_view key){const std::scoped_lock lock(mutex);if(key.empty()||active_key==key)Stop();}
bool FrameworkSplashActive(){return active.load(std::memory_order_acquire);}
VideoSplashStatus GetFrameworkSplashStatus(){const std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock()){VideoSplashStatus result;result.active=active.load();result.stage="Preparing video";return result;}auto result=status;result.active=active.load();return result;}
ComPtr<ID3D11ShaderResourceView> TickFrameworkSplash(ID3D11Device* host,ID3D11DeviceContext* context){
    if(!needs_tick.load(std::memory_order_relaxed))return {};
    EnsureThreadComApartment();
    const std::scoped_lock lock(mutex);
    if(!active){Stop();needs_tick=false;return {};}
    const auto open_player=[&](bool compatible){
        splash_player.reset();status={};
        status.stage=compatible?"Starting compatible video playback":"Starting GPU video playback";
        splash_player=std::make_unique<SplashPlayer>();
        const auto hr=splash_player->Open(host,configuration,compatible);
        if(FAILED(hr)){
            status.failed=true;status.error_code=static_cast<std::uint32_t>(hr);status.stage=splash_player->stage;
            spdlog::warn("SkyUltra splash screen initialization failed at {} (HRESULT 0x{:08X}, compatible {}).",status.stage,status.error_code,compatible);
            return false;
        }
        warmup_device.store(host,std::memory_order_relaxed);
        warmup_done.store(false,std::memory_order_relaxed);
        return true;
    };
    if(!splash_player||splash_player->owner.Get()!=host){
        start=false;
        started=std::chrono::steady_clock::now();
        if(!open_player(false)){
            spdlog::warn("SkyUltra is retrying the splash MP4 with compatible video and native audio.");
            if(!open_player(true)){Stop();splash_player.reset();needs_tick=false;return {};}
        }
    } else if(start){
        start=false;
        started=std::chrono::steady_clock::now();
        splash_player->ApplyAudioSettings(configuration);
        if(splash_player->signals->ended.load()){
            splash_player->RewindAndPause();
        }
    }
    // Retry once before the first delivered frame. Both keyed-mutex sharing
    // modes can be rejected by the driver; changing the HTML cannot fix that.
    if(splash_player->signals->failed&&!splash_player->software_transfer&&!splash_player->has_frame){
        spdlog::warn("SkyUltra GPU splash failed at {} (HRESULT 0x{:08X}); retrying the same MP4 with compatible video and native audio.",
            splash_player->stage,splash_player->signals->error_code.load());
        if(!open_player(true)){Stop();splash_player.reset();needs_tick=false;return {};}
    }
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    const bool startup_stalled=!splash_player->has_frame&&seconds>4.0;
    constexpr double kMinimumSplashDisplaySeconds = 2.0;
    const bool ready_to_dismiss=!configuration.full_duration&&!preview_full_duration&&ready&&seconds>=kMinimumSplashDisplaySeconds;
    if(splash_player->signals->failed||splash_player->signals->ended||seconds>30||startup_stalled||ready_to_dismiss){
        status.failed=splash_player->signals->failed||seconds>30||startup_stalled;status.error_code=splash_player->signals->error_code;status.media_error=splash_player->signals->media_error;
        status.stage=status.failed?splash_player->stage:"Completed";
        if(seconds>30||startup_stalled)status.stage="Video load/play timeout";
        if(status.failed)spdlog::warn("SkyUltra splash screen error at {} (HRESULT 0x{:08X}, media error {}).",status.stage,status.error_code,status.media_error);
        spdlog::info("SkyUltra splash screen ends after {:.3f}s (page ready {}; ended {}; error {}).",seconds,ready,splash_player->signals->ended.load(),splash_player->signals->failed.load());
        Stop();needs_tick=false;return {};
    }
    splash_player->Tick(context,configuration);
    if(splash_player->has_frame){
        status.has_frame=true;
        if(!splash_player->signals->failed)status.stage="Playing video";
        seen_session=true;seen_mods.insert(active_mod);
        warmup_done.store(true,std::memory_order_relaxed);
    }
    return splash_player->has_frame?splash_player->srv:ComPtr<ID3D11ShaderResourceView>{};
}
void ShutdownFrameworkSplash(){
    struct Apartment { HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED); ~Apartment(){if(SUCCEEDED(hr))CoUninitialize();} } apartment;const std::scoped_lock lock(mutex);Stop();splash_player.reset();needs_tick=false;warmup_done.store(false,std::memory_order_relaxed);warmup_device.store(nullptr,std::memory_order_relaxed);}
}
