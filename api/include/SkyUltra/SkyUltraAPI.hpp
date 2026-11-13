#pragma once
// Header-only convenience wrapper over the actual exported V4 C ABI.
#include "NativeAPI.h"
#include <cstdint>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
#include <span>
#include <string_view>
#include <vector>

namespace SkyUltra {
enum class Engine : std::uint32_t { Default=SKYULTRA_BROWSER_ENGINE_DEFAULT,
    Ultralight=SKYULTRA_BROWSER_ENGINE_ULTRALIGHT, Cef=SKYULTRA_BROWSER_ENGINE_CEF };
enum class Layer : std::uint32_t { Hud=SKYULTRA_LAYER_HUD, Panel=SKYULTRA_LAYER_PANEL,
    Popup=SKYULTRA_LAYER_POPUP, System=SKYULTRA_LAYER_SYSTEM };

class Client {
public:
    Client() noexcept = default;
    explicit Client(const SkyUltraNativeAPI_V4* api) noexcept : api_(api) {}
    [[nodiscard]] static Client Acquire(const wchar_t* name=L"SkyUltra.dll") noexcept {
#if defined(_WIN32)
        const auto module=GetModuleHandleW(name);
        if(!module)return {};
        using GetAPI=const SkyUltraNativeAPI_V4*(SKYULTRA_CALL*)();
        const auto get=reinterpret_cast<GetAPI>(GetProcAddress(module,"SkyUltra_GetAPI_V4"));
        const auto* api=get?get():nullptr;
        if(api && api->abi_version==4 && api->struct_size>=sizeof(SkyUltraNativeAPI_V4))return Client(api);
#else
        (void)name;
#endif
        return {};
    }
    [[nodiscard]] explicit operator bool() const noexcept { return api_!=nullptr; }
    [[nodiscard]] const SkyUltraNativeAPI_V4* Raw() const noexcept { return api_; }
    // The manifest must already exist at Data/SkyUltra/<id>/manifest.json.
    [[nodiscard]] SkyUltraResult RegisterMod(std::string_view id,SkyUltraModHandle* mod) const noexcept {
        return api_?api_->register_mod(id.data(),static_cast<std::uint32_t>(id.size()),mod):SKYULTRA_NOT_INITIALIZED;
    }
    // A registered view always loads the manifest entry, so initial_url is empty.
    [[nodiscard]] SkyUltraResult CreateView(SkyUltraModHandle mod,std::string_view id,
        Layer layer,std::uint32_t width,std::uint32_t height,Engine engine,
        bool visible,bool transparent,bool click_through,SkyUltraViewHandle* view) const noexcept {
        if(!api_)return SKYULTRA_NOT_INITIALIZED;
        SkyUltraViewConfigV1 config{};config.struct_size=sizeof(config);
        config.id_utf8=id.data();config.id_length=static_cast<std::uint32_t>(id.size());
        config.pixel_width=width;config.pixel_height=height;
        config.bounds={0,0,static_cast<float>(width),static_cast<float>(height)};
        config.opacity=1;config.scale=1;config.max_fps=60;
        config.flags=SKYULTRA_VIEW_ENABLE_JAVASCRIPT | SKYULTRA_VIEW_ENABLE_GAMEPAD |
            (visible?SKYULTRA_VIEW_VISIBLE:0) | (transparent?SKYULTRA_VIEW_TRANSPARENT:0) |
            (click_through?SKYULTRA_VIEW_CLICK_THROUGH:0);
        config.layer=static_cast<std::uint32_t>(layer);config.anchor=SKYULTRA_ANCHOR_TOP_LEFT;
        config.browser_engine=static_cast<std::uint32_t>(engine);
        return api_->create_mod_view(mod,&config,view);
    }
    [[nodiscard]] SkyUltraResult GetViewState(SkyUltraViewHandle view,SkyUltraViewStateV1* state) const noexcept {
        return api_?api_->get_view_state(view,state):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult SetVisible(SkyUltraViewHandle view,bool visible) const noexcept {
        return api_?api_->set_visible(view,visible?1:0):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult SetBounds(SkyUltraViewHandle view,SkyUltraRectV1 bounds) const noexcept {
        return api_?api_->set_bounds(view,bounds):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult ReloadView(SkyUltraViewHandle view) const noexcept {
        return api_?api_->reload_view(view):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult DestroyView(SkyUltraViewHandle view) const noexcept {
        return api_?api_->destroy_view(view):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult SendEventJson(SkyUltraViewHandle view,std::string_view event,std::string_view json) const noexcept {
        return api_?api_->send_event_json(view,event.data(),static_cast<std::uint32_t>(event.size()),
            json.data(),static_cast<std::uint32_t>(json.size())):SKYULTRA_NOT_INITIALIZED;
    }
    // The host currently has ONE callback slot, not one slot per view or mod.
    [[nodiscard]] SkyUltraResult SetPageRequestCallback(SkyUltraPageRequestCallbackV2 callback,void* user=nullptr) const noexcept {
        return api_?api_->set_page_request_callback(callback,user):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult SetPageRequestAllowlist(SkyUltraViewHandle view,std::span<const std::string_view> names) const {
        if(!api_)return SKYULTRA_NOT_INITIALIZED;
        std::vector<SkyUltraPageRequestMethodV2> methods;methods.reserve(names.size());
        for(const auto name:names)methods.push_back({name.data(),static_cast<std::uint32_t>(name.size())});
        return api_->set_page_request_allowlist(view,methods.data(),static_cast<std::uint32_t>(methods.size()));
    }
    [[nodiscard]] SkyUltraResult CompletePageRequestOk(SkyUltraViewHandle view,std::string_view id,std::string_view json) const noexcept {
        return api_?api_->complete_page_request(view,id.data(),static_cast<std::uint32_t>(id.size()),1,
            json.data(),static_cast<std::uint32_t>(json.size()),nullptr,0):SKYULTRA_NOT_INITIALIZED;
    }
    [[nodiscard]] SkyUltraResult CompletePageRequestError(SkyUltraViewHandle view,std::string_view id,std::string_view error) const noexcept {
        return api_?api_->complete_page_request(view,id.data(),static_cast<std::uint32_t>(id.size()),0,
            nullptr,0,error.data(),static_cast<std::uint32_t>(error.size())):SKYULTRA_NOT_INITIALIZED;
    }
private:
    const SkyUltraNativeAPI_V4* api_{};
};
} // namespace SkyUltra
