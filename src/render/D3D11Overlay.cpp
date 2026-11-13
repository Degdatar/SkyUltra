#include "SkyUltra/D3D11Overlay.h"
#include "SkyUltra/ActorPreview.h"
#include "SkyUltra/VideoSplash.h"
#include "SkyUltra/ViewManager.h"

#include <SKSE/SKSE.h>
#include <RE/B/BSFixedString.h>
#include <RE/I/IMenu.h>
#include <RE/R/Renderer.h>
#include <RE/U/UI.h>
#include <RE/U/UIMessage.h>
#include <RE/U/UIMessageQueue.h>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <WICTextureLoader.h>

#if defined(ENABLE_SKYRIM_VR)
#    include <openvr.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace skyultra {
namespace {

using Microsoft::WRL::ComPtr;
using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(REX::W32::IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFunction = HRESULT(STDMETHODCALLTYPE*)(REX::W32::IDXGISwapChain*, UINT, UINT,
    UINT, DXGI_FORMAT, UINT);

constexpr std::size_t kPresentVtableIndex = 8;
constexpr std::size_t kResizeBuffersVtableIndex = 13;
constexpr std::size_t kSwapChainVtableEntries = 18;
constexpr std::string_view kUiPassMenuName = "SkyUltraOverlay";

std::atomic<IWebRuntime*> g_runtime{};
std::atomic_bool g_visible{};
std::atomic_bool g_ui_pass_composited{};
enum class UiPassMenuRequest : std::uint8_t { Unknown, Hide, Show };
std::atomic<UiPassMenuRequest> g_ui_menu_request{UiPassMenuRequest::Unknown};
std::atomic_bool g_cursor_visible{};
std::atomic<float> g_cursor_x_normalized{ 0.5F };
std::atomic<float> g_cursor_y_normalized{ 0.5F };
std::atomic<std::uint32_t> g_cursor_accent{0xD8C49CU};
std::atomic_bool g_vr_head_tracking{ true };
std::atomic<int> g_vr_spatial_mode_id{ 0 };
std::atomic<float> g_vr_spatial_distance{ 1.35F };
std::atomic<float> g_vr_spatial_scale{ 1.0F };
std::mutex g_overlay_views_mutex;
std::vector<OverlayView> g_overlay_views;
std::mutex g_page_load_mutex;
std::unordered_map<ViewId,bool> g_painted_views;
std::unordered_map<ViewId, std::chrono::steady_clock::time_point> g_page_loads;
std::mutex g_splash_tasks_mutex;
std::unordered_multimap<std::string,std::function<void()>> g_splash_tasks;

constexpr std::size_t kTimingWindow=512;
struct TimingAccumulator {
    std::uint64_t samples{};
    double total_ms{};
    double last_ms{};
    std::array<double,kTimingWindow> window{};
    std::size_t next{},count{};
    std::chrono::steady_clock::time_point last{};
    void ClearWindow() { next=0;count=0;last={};last_ms=0; }
    void Add(double milliseconds) {
        if (!std::isfinite(milliseconds)||milliseconds<0) return;
        ++samples;total_ms+=milliseconds;last_ms=milliseconds;
        window[next]=milliseconds;next=(next+1)%window.size();count=std::min(count+1,window.size());
        last=std::chrono::steady_clock::now();
    }
};
std::mutex g_performance_mutex;
std::atomic<std::int64_t> g_performance_lease_until_ms{};
bool PerformanceMonitoringActive()
{
    auto until=g_performance_lease_until_ms.load(std::memory_order_relaxed);
    if (until==0) return false;
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now<until) return true;
    (void)g_performance_lease_until_ms.compare_exchange_strong(until,0,std::memory_order_relaxed);
    return false;
}
struct PerformanceCounters {
    std::uint64_t generation{};
    bool gpu_available{};
    std::uint64_t skipped_gpu{},invalid_gpu{};
    TimingAccumulator host_gpu,compositor_gpu,callback_cpu,present_interval;
    std::chrono::steady_clock::time_point previous_present{};
} g_performance;

void ResetPerformance(bool gpu_available)
{
    const std::scoped_lock lock(g_performance_mutex);
    const auto generation=g_performance.generation+1;
    g_performance={};g_performance.generation=generation;g_performance.gpu_available=gpu_available;
}

OverlayTimingMetric SummarizeTiming(const TimingAccumulator& metric,std::chrono::steady_clock::time_point now)
{
    OverlayTimingMetric result;
    result.samples=metric.samples;result.total_ms=metric.total_ms;result.last_ms=metric.last_ms;
    result.window_samples=static_cast<std::uint32_t>(metric.count);
    if (!metric.count) return result;
    auto sorted=metric.window;
    std::sort(sorted.begin(),sorted.begin()+metric.count);
    double total{};for (std::size_t index=0;index<metric.count;++index) total+=sorted[index];
    result.mean_ms=total/static_cast<double>(metric.count);
    result.p95_ms=sorted[(metric.count*95+99)/100-1];result.max_ms=sorted[metric.count-1];
    result.age_ms=std::chrono::duration<double,std::milli>(now-metric.last).count();
    return result;
}

class CallbackCpuTiming final {
public:
    explicit CallbackCpuTiming(bool visible):enabled_(visible&&PerformanceMonitoringActive()) {
        if (enabled_) start_=std::chrono::steady_clock::now();
    }
    ~CallbackCpuTiming() {
        if (!enabled_) return;
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start_).count();
        const std::scoped_lock lock(g_performance_mutex);g_performance.callback_cpu.Add(elapsed);
    }
private:
    bool enabled_{};
    std::chrono::steady_clock::time_point start_;
};

void RecordPresentInterval()
{
    if (!PerformanceMonitoringActive()) return;
    const auto now=std::chrono::steady_clock::now();
    const std::scoped_lock lock(g_performance_mutex);
    if (g_performance.previous_present.time_since_epoch().count()!=0)
        g_performance.present_interval.Add(std::chrono::duration<double,std::milli>(now-g_performance.previous_present).count());
    g_performance.previous_present=now;
}

std::atomic_bool g_clear_retained_frames{ false };

bool PageAwaitingReady(ViewId id)
{
    const std::scoped_lock lock(g_page_load_mutex);
    const auto found = g_page_loads.find(id);
    if (found == g_page_loads.end()) return false;
    if(found->second.time_since_epoch().count()==0)found->second=std::chrono::steady_clock::now();
    const bool warm_view = g_painted_views.contains(id);
    const auto timeout = warm_view
        ? std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(180))
        : std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(20));
    if (std::chrono::steady_clock::now() - found->second < timeout) return true;
    g_page_loads.erase(found);
    if (!warm_view) {
        SKSE::log::warn("SkyUltra page {} did not signal readiness within 20 seconds; exposing its HTML recovery screen.", id);
    }
    return false;
}
std::atomic<ViewId> g_vr_keyboard_focus_id{};
std::array<std::atomic_bool, 2> g_vr_trigger_fallback{};
std::array<std::atomic_bool, 2> g_vr_keyboard_button_fallback{};
std::array<std::atomic<int>, 2> g_vr_stick_y_fallback{};
std::array<std::atomic<std::uint64_t>, 2> g_vr_stick_tick{};
std::array<void*, kSwapChainVtableEntries> g_hooked_vtable{};
void** g_original_vtable{};
REX::W32::IDXGISwapChain* g_hooked_swap_chain{};
ComPtr<IDXGISwapChain> g_hooked_swap_chain_ref;
PresentFunction g_original_present{};
ResizeBuffersFunction g_original_resize_buffers{};
std::mutex g_present_hook_mutex;
bool g_ui_menu_registered{};
bool g_logged_missing_ui_message_queue{};
bool g_logged_present_hook_superseded{};

#if defined(ENABLE_SKYRIM_VR)
class OpenVrOverlayManager final {
public:
    void Update(const std::vector<OverlayView>& views, SizeU viewport, ID3D11Device* device)
    {
        auto* overlay_api = vr::VROverlay();
        auto* vr_system = vr::VRSystem();
        auto* compositor = vr::VRCompositor();
        if (overlay_api == nullptr || vr_system == nullptr || compositor == nullptr) {
            if (!logged_missing_runtime_) {
                SKSE::log::error("SkyUltra could not access Skyrim VR's OpenVR overlay interfaces.");
                logged_missing_runtime_ = true;
            }
            HideAll();
            return;
        }
        logged_missing_runtime_ = false;

        if (viewport.width == 0 || viewport.height == 0) {
            HideAll();
            return;
        }

        std::erase_if(overlays_, [&](auto& item) {
            const auto still_exists = std::any_of(views.begin(), views.end(), [&](const OverlayView& view) {
                return view.view != nullptr && view.view->Id() == item.first;
            });
            if (!still_exists) {
                ReleaseCapturedView(item.first);
                LeaveHoveredView(item.first);
                if (keyboard_focus_view_ != nullptr && keyboard_focus_view_->Id() == item.first) {
                    CloseVirtualKeyboard(overlay_api);
                    keyboard_focus_view_->SetFocused(InputMode::None);
                    keyboard_focus_view_.reset();
                    g_vr_keyboard_focus_id.store(0, std::memory_order_release);
                }
                if (item.second.visible) {
                    (void)overlay_api->HideOverlay(item.second.handle);
                }
                (void)overlay_api->DestroyOverlay(item.second.handle);
                return true;
            }
            return false;
        });

        for (const auto& overlay_view : views) {
            auto* view = overlay_view.view.get();
            const auto& config = overlay_view.config;
            if (view == nullptr) {
                continue;
            }

            auto& managed_overlay = overlays_[view->Id()];
            if (!config.visible || !g_visible.load(std::memory_order_acquire)) {
                Hide(overlay_api, managed_overlay);
                ReleaseCapturedView(view->Id());
                LeaveHoveredView(view->Id());
                if (keyboard_focus_view_ == overlay_view.view) {
                    SetKeyboardFocus(std::shared_ptr<IWebView>{}, overlay_api);
                }
                continue;
            }

            if (!EnsureOverlay(overlay_api, view->Id(), managed_overlay)) {
                continue;
            }

            const auto frame = view->GetGpuFrame();
            if (!frame || frame->shader_resource_view == nullptr ||
                frame->texture_size.width == 0 || frame->texture_size.height == 0) {
                Hide(overlay_api, managed_overlay);
                if (!logged_missing_gpu_frame_) {
                    SKSE::log::warn("SkyUltra's VR overlay is waiting for a GPU-backed browser view; "
                                    "the VR compositor currently requires GPU frames.");
                    logged_missing_gpu_frame_ = true;
                }
                continue;
            }
            logged_missing_gpu_frame_ = false;

            auto* shader_resource_view =
                static_cast<ID3D11ShaderResourceView*>(frame->shader_resource_view);
            ComPtr<ID3D11Resource> resource;
            ComPtr<ID3D11Texture2D> texture;
            shader_resource_view->GetResource(resource.GetAddressOf());
            if (resource == nullptr || FAILED(resource.As(&texture))) {
                Hide(overlay_api, managed_overlay);
                if (!logged_texture_failure_) {
                    SKSE::log::error("SkyUltra could not expose a D3D11 view texture to OpenVR.");
                    logged_texture_failure_ = true;
                }
                continue;
            }
            logged_texture_failure_ = false;

            managed_overlay.texture = texture;
            vr::Texture_t vr_texture{
                managed_overlay.texture.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
            if (overlay_api->SetOverlayTexture(managed_overlay.handle, &vr_texture) != vr::VROverlayError_None) {
                Hide(overlay_api, managed_overlay);
                if (!logged_texture_submit_failure_) {
                    SKSE::log::error("SkyUltra could not submit a browser texture to the OpenVR compositor.");
                    logged_texture_submit_failure_ = true;
                }
                continue;
            }
            logged_texture_submit_failure_ = false;

            const auto source_x = frame->source_rect.width == 0 ? 0U : frame->source_rect.x;
            const auto source_y = frame->source_rect.height == 0 ? 0U : frame->source_rect.y;
            const auto source_width = frame->source_rect.width == 0 ?
                frame->texture_size.width : frame->source_rect.width;
            const auto source_height = frame->source_rect.height == 0 ?
                frame->texture_size.height : frame->source_rect.height;
            const auto right = std::min<std::uint64_t>(
                static_cast<std::uint64_t>(source_x) + source_width, frame->texture_size.width);
            const auto bottom = std::min<std::uint64_t>(
                static_cast<std::uint64_t>(source_y) + source_height, frame->texture_size.height);
            if (right <= source_x || bottom <= source_y) {
                Hide(overlay_api, managed_overlay);
                continue;
            }
            const auto inverse_width = 1.0F / static_cast<float>(frame->texture_size.width);
            const auto inverse_height = 1.0F / static_cast<float>(frame->texture_size.height);
            const vr::VRTextureBounds_t texture_bounds{
                static_cast<float>(source_x) * inverse_width,
                static_cast<float>(source_y) * inverse_height,
                static_cast<float>(right) * inverse_width,
                static_cast<float>(bottom) * inverse_height,
            };
            (void)overlay_api->SetOverlayTextureBounds(managed_overlay.handle, &texture_bounds);

            const auto bounds = ResolveViewBounds(config, viewport);
            const auto viewport_width = static_cast<float>(viewport.width);
            const auto viewport_height = static_cast<float>(viewport.height);
            const float spatial_scale = std::clamp(g_vr_spatial_scale.load(std::memory_order_relaxed), 0.4F, 2.5F);
            const float spatial_distance = std::clamp(g_vr_spatial_distance.load(std::memory_order_relaxed), 0.35F, 6.0F);
            const int spatial_mode_id = g_vr_spatial_mode_id.load(std::memory_order_relaxed);
            const bool head_tracking = g_vr_head_tracking.load(std::memory_order_relaxed);
            const bool wrist_mode = (spatial_mode_id == 2 || spatial_mode_id == 3) && !config.modal;
            const auto physical_width = (wrist_mode ? 0.42F : 2.0F) * spatial_scale;
            const auto physical_height = physical_width * viewport_height / viewport_width;
            const auto center_x = bounds.x + bounds.width * 0.5F;
            const auto center_y = bounds.y + bounds.height * 0.5F;
            const auto width_in_meters = std::clamp(
                bounds.width / viewport_width * physical_width, 0.05F, 4.5F);
            const auto visible_width = static_cast<float>(right - source_x);
            const auto visible_height = static_cast<float>(bottom - source_y);
            const auto height_in_meters = width_in_meters * visible_height / visible_width;

            (void)overlay_api->SetOverlayWidthInMeters(managed_overlay.handle, width_in_meters);
            vr::HmdMatrix34_t transform{};
            transform.m[0][0] = 1.0F;
            transform.m[1][1] = 1.0F;
            transform.m[2][2] = 1.0F;
            transform.m[0][3] = (center_x / viewport_width - 0.5F) * physical_width;
            transform.m[1][3] = (0.5F - center_y / viewport_height) * physical_height;
            transform.m[2][3] = -(wrist_mode ? 0.12F : spatial_distance) +
                static_cast<float>(std::clamp(config.z_index, -100, 100)) * 0.001F;
            managed_overlay.local_transform = transform;
            managed_overlay.width_in_meters = width_in_meters;
            managed_overlay.height_in_meters = height_in_meters;
            vr::TrackedDeviceIndex_t tracked_device = vr::k_unTrackedDeviceIndex_Hmd;
            if (wrist_mode && vr_system != nullptr) {
                const auto role = spatial_mode_id == 2 ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;
                const auto candidate_device = vr_system->GetTrackedDeviceIndexForControllerRole(role);
                if (candidate_device != vr::k_unTrackedDeviceIndexInvalid &&
                    candidate_device < vr::k_unMaxTrackedDeviceCount) {
                    tracked_device = candidate_device;
                }
            }
            managed_overlay.tracked_device = tracked_device;
            const bool use_world_lock = (!head_tracking || spatial_mode_id == 1) && !wrist_mode;
            if (use_world_lock && compositor != nullptr) {
                if (!managed_overlay.has_world_lock) {
                    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> render_poses{};
                    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> game_poses{};
                    if (compositor->GetLastPoses(render_poses.data(), static_cast<std::uint32_t>(render_poses.size()),
                            game_poses.data(), static_cast<std::uint32_t>(game_poses.size())) == vr::VRCompositorError_None &&
                        PoseUsable(render_poses[vr::k_unTrackedDeviceIndex_Hmd])) {
                        managed_overlay.world_locked_transform =
                            Multiply(render_poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking, transform);
                        managed_overlay.has_world_lock = true;
                    }
                }
                if (managed_overlay.has_world_lock) {
                    (void)overlay_api->SetOverlayTransformAbsolute(
                        managed_overlay.handle, compositor->GetTrackingSpace(), &managed_overlay.world_locked_transform);
                } else {
                    (void)overlay_api->SetOverlayTransformTrackedDeviceRelative(
                        managed_overlay.handle, tracked_device, &transform);
                }
            } else {
                managed_overlay.has_world_lock = false;
                (void)overlay_api->SetOverlayTransformTrackedDeviceRelative(
                    managed_overlay.handle, tracked_device, &transform);
            }

            if (!managed_overlay.visible) {
                if (overlay_api->ShowOverlay(managed_overlay.handle) == vr::VROverlayError_None) {
                    managed_overlay.visible = true;
                    if (!logged_first_overlay_) {
                        SKSE::log::info("SkyUltra displayed its first head-locked OpenVR browser view.");
                        logged_first_overlay_ = true;
                    }
                } else if (!logged_show_failure_) {
                    SKSE::log::error("SkyUltra could not show a browser view in the OpenVR compositor.");
                    logged_show_failure_ = true;
                }
            }
        }

        UpdateControllerInput(overlay_api, vr_system, compositor, views, viewport, device);
    }

    void Shutdown() noexcept
    {
        auto* overlay_api = vr::VROverlay();
        CloseVirtualKeyboard(overlay_api);
        ReleasePointerInput();
        if (overlay_api != nullptr) {
            for (auto& [id, overlay] : overlays_) {
                (void)id;
                if (overlay.handle != vr::k_ulOverlayHandleInvalid) {
                    (void)overlay_api->HideOverlay(overlay.handle);
                    (void)overlay_api->ClearOverlayTexture(overlay.handle);
                    (void)overlay_api->DestroyOverlay(overlay.handle);
                }
            }
            for (auto& laser : lasers_) {
                if (laser.handle != vr::k_ulOverlayHandleInvalid) {
                    (void)overlay_api->HideOverlay(laser.handle);
                    (void)overlay_api->ClearOverlayTexture(laser.handle);
                    (void)overlay_api->DestroyOverlay(laser.handle);
                }
            }
        }
        overlays_.clear();
        lasers_ = {};
        laser_texture_.Reset();
        if (keyboard_focus_view_ != nullptr) {
            try {
                keyboard_focus_view_->SetFocused(InputMode::None);
            } catch (...) {
            }
        }
        keyboard_focus_view_.reset();
        g_vr_keyboard_focus_id.store(0, std::memory_order_release);
    }

    void HideAll() noexcept
    {
        CloseVirtualKeyboard(vr::VROverlay());
        ReleasePointerInput();
        if (keyboard_focus_view_ != nullptr) {
            try {
                keyboard_focus_view_->SetFocused(InputMode::None);
            } catch (...) {
            }
            keyboard_focus_view_.reset();
        }
        g_vr_keyboard_focus_id.store(0, std::memory_order_release);
        auto* overlay_api = vr::VROverlay();
        if (overlay_api == nullptr) {
            for (auto& laser : lasers_) {
                laser.visible = false;
            }
            return;
        }
        for (auto& [id, overlay] : overlays_) {
            (void)id;
            Hide(overlay_api, overlay);
        }
        for (auto& laser : lasers_) {
            HideLaser(overlay_api, laser);
        }
    }

private:
    struct ManagedOverlay {
        vr::VROverlayHandle_t handle{ vr::k_ulOverlayHandleInvalid };
        ComPtr<ID3D11Texture2D> texture;
        vr::HmdMatrix34_t local_transform{};
        vr::HmdMatrix34_t world_locked_transform{};
        vr::TrackedDeviceIndex_t tracked_device{ vr::k_unTrackedDeviceIndex_Hmd };
        float width_in_meters{};
        float height_in_meters{};
        bool has_world_lock{};
        bool visible{};
    };

    struct LaserOverlay {
        vr::VROverlayHandle_t handle{ vr::k_ulOverlayHandleInvalid };
        bool visible{};
    };

    struct AimCache {
        vr::TrackedDeviceIndex_t device{ vr::k_unTrackedDeviceIndexInvalid };
        std::string model;
        vr::HmdMatrix34_t tip{};
        std::uint32_t frames_until_refresh{};
        bool resolved{};
        bool has_tip{};
    };

    struct ControllerPointer {
        AimCache aim;
        bool trigger_down{};
        bool trigger_was_down{};
        bool keyboard_button_down{};
        bool keyboard_button_was_down{};
        bool pose_valid{};
        float stick_y{};
        float scroll_accumulator{};
        vr::HmdMatrix34_t aim_pose{};
    };

    struct PointerCandidate {
        const OverlayView* overlay_view{};
        RectF bounds{};
        vr::HmdMatrix34_t world_transform{};
        float width_in_meters{};
        float height_in_meters{};
        std::size_t order{};
    };

    struct PointerHit {
        std::shared_ptr<IWebView> view;
        ViewId view_id{};
        PointF position{};
        float distance{};
        std::size_t order{};
    };

    bool EnsureOverlay(vr::IVROverlay* overlay_api, ViewId id, ManagedOverlay& managed_overlay)
    {
        if (managed_overlay.handle != vr::k_ulOverlayHandleInvalid) {
            return true;
        }

        const auto key = "skyultra.view." + std::to_string(id);
        if (overlay_api->CreateOverlay(key.c_str(), "SkyUltra WebView", &managed_overlay.handle) !=
            vr::VROverlayError_None) {
            managed_overlay.handle = vr::k_ulOverlayHandleInvalid;
            if (!logged_create_failure_) {
                SKSE::log::error("SkyUltra could not create a head-locked OpenVR browser overlay.");
                logged_create_failure_ = true;
            }
            return false;
        }

        // SkyUltra performs controller ray tests itself. Runtime-generated
        // mouse events are missing on some OpenVR-compatible runtimes.
        (void)overlay_api->SetOverlayInputMethod(managed_overlay.handle, vr::VROverlayInputMethod_None);
        SKSE::log::info("SkyUltra created OpenVR overlay for browser view {}.", id);
        logged_create_failure_ = false;
        return true;
    }

    static void Hide(vr::IVROverlay* overlay_api, ManagedOverlay& managed_overlay) noexcept
    {
        managed_overlay.has_world_lock = false;
        if (managed_overlay.visible && managed_overlay.handle != vr::k_ulOverlayHandleInvalid) {
            (void)overlay_api->HideOverlay(managed_overlay.handle);
            managed_overlay.visible = false;
        }
    }

    static void HideLaser(vr::IVROverlay* overlay_api, LaserOverlay& laser) noexcept
    {
        if (laser.visible && overlay_api != nullptr && laser.handle != vr::k_ulOverlayHandleInvalid) {
            (void)overlay_api->HideOverlay(laser.handle);
            laser.visible = false;
        }
    }

    static bool PoseUsable(const vr::TrackedDevicePose_t& pose) noexcept
    {
        return pose.bDeviceIsConnected && pose.bPoseIsValid &&
            (pose.eTrackingResult == vr::TrackingResult_Running_OK ||
             pose.eTrackingResult == vr::TrackingResult_Running_OutOfRange);
    }

    static vr::HmdMatrix34_t Multiply(
        const vr::HmdMatrix34_t& lhs, const vr::HmdMatrix34_t& rhs) noexcept
    {
        vr::HmdMatrix34_t result{};
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                result.m[row][column] = lhs.m[row][0] * rhs.m[0][column] +
                    lhs.m[row][1] * rhs.m[1][column] + lhs.m[row][2] * rhs.m[2][column];
            }
            result.m[row][3] = lhs.m[row][0] * rhs.m[0][3] + lhs.m[row][1] * rhs.m[1][3] +
                lhs.m[row][2] * rhs.m[2][3] + lhs.m[row][3];
        }
        return result;
    }

    static vr::HmdMatrix34_t ApplyAimPitch(const vr::HmdMatrix34_t& pose, float degrees) noexcept
    {
        const float radians = degrees * 0.01745329252F;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        auto result = pose;
        for (int row = 0; row < 3; ++row) {
            const float y = pose.m[row][1];
            const float z = pose.m[row][2];
            result.m[row][1] = cosine * y + sine * z;
            result.m[row][2] = -sine * y + cosine * z;
        }
        return result;
    }

    static vr::HmdMatrix34_t ResolveAimPose(vr::IVRSystem* system,
        vr::TrackedDeviceIndex_t device, const vr::VRControllerState_t& state,
        const vr::HmdMatrix34_t& controller_pose, ControllerPointer& pointer)
    {
        AimCache& cache = pointer.aim;
        if (cache.device == device && cache.resolved && cache.frames_until_refresh > 0) {
            --cache.frames_until_refresh;
        } else {
            const bool same_device = cache.device == device;
            if (!same_device) {
                cache = {};
                cache.device = device;
            }

            std::array<char, 256> model_name{};
            vr::ETrackedPropertyError property_error = vr::TrackedProp_Success;
            (void)system->GetStringTrackedDeviceProperty(device, vr::Prop_RenderModelName_String,
                model_name.data(), static_cast<std::uint32_t>(model_name.size()), &property_error);
            const std::string model = property_error == vr::TrackedProp_Success ? model_name.data() : "";
            if (cache.resolved && cache.model == model) {
                cache.frames_until_refresh = 90;
            } else {
                cache.model = model;
                cache.resolved = true;
                cache.has_tip = false;
                cache.frames_until_refresh = 90;
                if (!model.empty()) {
                    if (auto* render_models = vr::VRRenderModels(); render_models != nullptr &&
                        render_models->RenderModelHasComponent(model.c_str(), vr::k_pch_Controller_Component_Tip)) {
                        vr::RenderModel_ComponentState_t component{};
                        vr::RenderModel_ControllerMode_State_t mode{};
                        if (render_models->GetComponentState(model.c_str(),
                                vr::k_pch_Controller_Component_Tip, &state, &mode, &component)) {
                            cache.tip = component.mTrackingToComponentLocal;
                            cache.has_tip = true;
                        }
                    }
                }
            }
        }

        return cache.has_tip ? Multiply(controller_pose, cache.tip) : ApplyAimPitch(controller_pose, -35.0F);
    }

    static bool IntersectQuad(const vr::HmdMatrix34_t& quad, float width, float height,
        const vr::HmdMatrix34_t& ray, bool require_inside, float& u, float& v, float& distance) noexcept
    {
        const float ox = quad.m[0][3], oy = quad.m[1][3], oz = quad.m[2][3];
        const float xx = quad.m[0][0], xy = quad.m[1][0], xz = quad.m[2][0];
        const float yx = quad.m[0][1], yy = quad.m[1][1], yz = quad.m[2][1];
        const float nx = quad.m[0][2], ny = quad.m[1][2], nz = quad.m[2][2];
        const float px = ray.m[0][3], py = ray.m[1][3], pz = ray.m[2][3];
        const float dx = -ray.m[0][2], dy = -ray.m[1][2], dz = -ray.m[2][2];
        const float denominator = dx * nx + dy * ny + dz * nz;
        if (std::fabs(denominator) < 1.0e-5F || width <= 0.0F || height <= 0.0F) {
            return false;
        }

        const float t = ((ox - px) * nx + (oy - py) * ny + (oz - pz) * nz) / denominator;
        if (!std::isfinite(t) || t <= 0.0F) {
            return false;
        }
        const float hx = px + dx * t - ox;
        const float hy = py + dy * t - oy;
        const float hz = pz + dz * t - oz;
        const float local_x = hx * xx + hy * xy + hz * xz;
        const float local_y = hx * yx + hy * yy + hz * yz;
        if (require_inside &&
            (std::fabs(local_x) > width * 0.5F || std::fabs(local_y) > height * 0.5F)) {
            return false;
        }
        u = local_x / width + 0.5F;
        v = 0.5F - local_y / height;
        distance = t;
        return std::isfinite(u) && std::isfinite(v);
    }

    static void SendMouseMove(const std::shared_ptr<IWebView>& view, PointF position)
    {
        if (view == nullptr) {
            return;
        }
        MouseInput input{};
        input.position = position;
        input.moved = true;
        view->SendMouse(input);
    }

    void LeaveHoveredView()
    {
        if (hovered_view_ != nullptr) {
            SendMouseMove(hovered_view_, { -1.0F, -1.0F });
        }
        hovered_view_.reset();
        hovered_view_id_ = 0;
        hovered_position_ = {};
    }

    void LeaveHoveredView(ViewId id)
    {
        if (hovered_view_id_ == id) {
            LeaveHoveredView();
        }
    }

    void ReleaseMouseButton()
    {
        if (pressed_view_ != nullptr) {
            MouseInput input{};
            input.position = pressed_position_;
            input.button = MouseButton::Left;
            input.button_action = MouseButtonAction::Up;
            pressed_view_->SendMouse(input);
        }
        pressed_view_.reset();
        pressed_view_id_ = 0;
        pressed_position_ = {};
        pressed_hand_ = -1;
    }

    void ReleaseCapturedView(ViewId id)
    {
        if (pressed_view_id_ == id) {
            ReleaseMouseButton();
        }
    }

    void ReleasePointerInput()
    {
        ReleaseMouseButton();
        LeaveHoveredView();
        active_pointer_hand_ = -1;
        seed_trigger_levels_ = true;
        previous_pointer_tick_ = {};
        for (auto& controller : controllers_) {
            controller.trigger_down = false;
            controller.trigger_was_down = false;
            controller.scroll_accumulator = 0.0F;
        }
    }

    bool EnsureLaserTexture(ID3D11Device* device)
    {
        if (laser_texture_ != nullptr) {
            return true;
        }
        if (device == nullptr) {
            return false;
        }

        constexpr UINT kTextureWidth = 512;
        constexpr UINT kTextureHeight = 2;
        std::array<std::uint32_t, kTextureWidth * kTextureHeight> pixels{};
        for (UINT y = 0; y < kTextureHeight; ++y) {
            for (UINT x = 0; x < kTextureWidth; ++x) {
                const float t = static_cast<float>(x) / static_cast<float>(kTextureWidth - 1U);
                const auto alpha = static_cast<std::uint32_t>(std::lround(220.0F * (1.0F - t * t)));
                pixels[y * kTextureWidth + x] = (alpha << 24U) | 0x00FFFFFFU;
            }
        }

        D3D11_TEXTURE2D_DESC description{};
        description.Width = kTextureWidth;
        description.Height = kTextureHeight;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial_data{};
        initial_data.pSysMem = pixels.data();
        initial_data.SysMemPitch = static_cast<UINT>(kTextureWidth * sizeof(std::uint32_t));
        if (FAILED(device->CreateTexture2D(&description, &initial_data, laser_texture_.GetAddressOf()))) {
            if (!logged_laser_failure_) {
                SKSE::log::warn("SkyUltra could not create its VR controller-laser texture.");
                logged_laser_failure_ = true;
            }
            return false;
        }
        return true;
    }

    bool EnsureLaser(vr::IVROverlay* overlay_api, ID3D11Device* device, int hand)
    {
        auto& laser = lasers_[static_cast<std::size_t>(hand)];
        if (laser.handle != vr::k_ulOverlayHandleInvalid) {
            return true;
        }
        if (overlay_api == nullptr || !EnsureLaserTexture(device)) {
            return false;
        }

        static const std::string session_suffix = std::to_string(GetCurrentProcessId()) + "." +
            std::to_string(GetTickCount64());
        const char* hand_name = hand == 0 ? "left" : "right";
        const std::string key = "skyultra.laser." + session_suffix + "." + hand_name;
        const std::string name = std::string("SkyUltra controller laser ") + hand_name;
        if (overlay_api->CreateOverlay(key.c_str(), name.c_str(), &laser.handle) != vr::VROverlayError_None) {
            laser.handle = vr::k_ulOverlayHandleInvalid;
            if (!logged_laser_failure_) {
                SKSE::log::warn("SkyUltra could not create its VR controller-laser overlay.");
                logged_laser_failure_ = true;
            }
            return false;
        }

        vr::Texture_t texture{ laser_texture_.Get(), vr::TextureType_DirectX, vr::ColorSpace_Gamma };
        if (overlay_api->SetOverlayTexture(laser.handle, &texture) != vr::VROverlayError_None) {
            (void)overlay_api->ClearOverlayTexture(laser.handle);
            (void)overlay_api->DestroyOverlay(laser.handle);
            laser.handle = vr::k_ulOverlayHandleInvalid;
            if (!logged_laser_failure_) {
                SKSE::log::warn("SkyUltra could not attach its texture to the VR controller-laser overlay.");
                logged_laser_failure_ = true;
            }
            return false;
        }
        (void)overlay_api->SetOverlayTextureColorSpace(laser.handle, vr::ColorSpace_Gamma);
        (void)overlay_api->SetOverlayAlpha(laser.handle, 0.9F);
        (void)overlay_api->SetOverlayInputMethod(laser.handle, vr::VROverlayInputMethod_None);
        (void)overlay_api->SetOverlayFlag(laser.handle, vr::VROverlayFlags_SortWithNonSceneOverlays, true);
        (void)overlay_api->SetOverlaySortOrder(laser.handle, 1000U);
        logged_laser_failure_ = false;
        return true;
    }

    void UpdateLaser(vr::IVROverlay* overlay_api, ID3D11Device* device,
        vr::ETrackingUniverseOrigin tracking_space, int hand, const vr::HmdMatrix34_t& ray,
        const vr::HmdMatrix34_t& hmd, float length)
    {
        if (!std::isfinite(length) || length < 0.02F || !EnsureLaser(overlay_api, device, hand)) {
            HideLaser(overlay_api, lasers_[static_cast<std::size_t>(hand)]);
            return;
        }

        const float dx = -ray.m[0][2], dy = -ray.m[1][2], dz = -ray.m[2][2];
        const float px = ray.m[0][3], py = ray.m[1][3], pz = ray.m[2][3];
        const float cx = px + dx * length * 0.5F;
        const float cy = py + dy * length * 0.5F;
        const float cz = pz + dz * length * 0.5F;
        float zx = hmd.m[0][3] - cx;
        float zy = hmd.m[1][3] - cy;
        float zz = hmd.m[2][3] - cz;
        const float along = zx * dx + zy * dy + zz * dz;
        zx -= dx * along;
        zy -= dy * along;
        zz -= dz * along;
        const float z_length = std::sqrt(zx * zx + zy * zy + zz * zz);
        if (z_length < 1.0e-4F) {
            zx = ray.m[0][1];
            zy = ray.m[1][1];
            zz = ray.m[2][1];
        } else {
            zx /= z_length;
            zy /= z_length;
            zz /= z_length;
        }
        const float yx = zy * dz - zz * dy;
        const float yy = zz * dx - zx * dz;
        const float yz = zx * dy - zy * dx;

        vr::HmdMatrix34_t transform{};
        transform.m[0][0] = dx; transform.m[1][0] = dy; transform.m[2][0] = dz;
        transform.m[0][1] = yx; transform.m[1][1] = yy; transform.m[2][1] = yz;
        transform.m[0][2] = zx; transform.m[1][2] = zy; transform.m[2][2] = zz;
        transform.m[0][3] = cx; transform.m[1][3] = cy; transform.m[2][3] = cz;

        auto& laser = lasers_[static_cast<std::size_t>(hand)];
        (void)overlay_api->SetOverlayWidthInMeters(laser.handle, length);
        (void)overlay_api->SetOverlayTransformAbsolute(laser.handle, tracking_space, &transform);
        if (!laser.visible && overlay_api->ShowOverlay(laser.handle) == vr::VROverlayError_None) {
            laser.visible = true;
        }
    }

    std::optional<PointerHit> IntersectCandidates(const vr::HmdMatrix34_t& ray,
        bool require_inside, const std::optional<std::size_t>& modal_floor)
    {
        std::optional<PointerHit> best;
        for (const auto& candidate : pointer_candidates_) {
            const auto& overlay_view = *candidate.overlay_view;
            const auto& config = overlay_view.config;
            if (require_inside && (config.click_through ||
                (modal_floor && candidate.order < *modal_floor))) {
                continue;
            }

            float u{}, v{}, distance{};
            if (!IntersectQuad(candidate.world_transform, candidate.width_in_meters,
                    candidate.height_in_meters, ray, require_inside, u, v, distance)) {
                continue;
            }

            if (best && (distance > best->distance + 1.0e-4F ||
                (std::fabs(distance - best->distance) <= 1.0e-4F && candidate.order < best->order))) {
                continue;
            }
            best = PointerHit{
                overlay_view.view,
                overlay_view.view->Id(),
                { u * static_cast<float>(config.size.width), v * static_cast<float>(config.size.height) },
                distance,
                candidate.order,
            };
        }
        return best;
    }

    const PointerCandidate* FindCandidate(ViewId view_id) const noexcept
    {
        const auto it = std::find_if(pointer_candidates_.begin(), pointer_candidates_.end(),
            [view_id](const PointerCandidate& candidate) {
                return candidate.overlay_view->view->Id() == view_id;
            });
        return it == pointer_candidates_.end() ? nullptr : &*it;
    }

    void UpdateHoveredView(const PointerHit& hit)
    {
        if (hovered_view_id_ != hit.view_id) {
            LeaveHoveredView();
            hovered_view_ = hit.view;
            hovered_view_id_ = hit.view_id;
            hovered_position_ = { -1.0F, -1.0F };
        }
        const auto moved = std::lround(hovered_position_.x) != std::lround(hit.position.x) ||
            std::lround(hovered_position_.y) != std::lround(hit.position.y);
        if (moved) {
            SendMouseMove(hit.view, hit.position);
            hovered_position_ = hit.position;
        }
    }

    void CloseVirtualKeyboard(vr::IVROverlay* overlay_api) noexcept
    {
        if (!keyboard_open_) {
            return;
        }
        if (overlay_api != nullptr) {
            overlay_api->HideKeyboard();
        }
        keyboard_open_ = false;
        keyboard_overlay_view_id_ = 0;
    }

    void SetKeyboardFocus(const std::shared_ptr<IWebView>& view, vr::IVROverlay* overlay_api)
    {
        if (keyboard_focus_view_ == view) {
            if (view != nullptr) {
                view->SetFocused(InputMode::MouseAndKeyboard);
                g_vr_keyboard_focus_id.store(view->Id(), std::memory_order_release);
            }
            return;
        }
        CloseVirtualKeyboard(overlay_api);
        if (keyboard_focus_view_ != nullptr) {
            keyboard_focus_view_->SetFocused(InputMode::None);
        }
        keyboard_focus_view_ = view;
        if (keyboard_focus_view_ != nullptr) {
            keyboard_focus_view_->SetFocused(InputMode::MouseAndKeyboard);
            g_vr_keyboard_focus_id.store(keyboard_focus_view_->Id(), std::memory_order_release);
        } else {
            g_vr_keyboard_focus_id.store(0, std::memory_order_release);
        }
    }

    void ToggleVirtualKeyboard(vr::IVROverlay* overlay_api)
    {
        if (keyboard_open_) {
            CloseVirtualKeyboard(overlay_api);
            return;
        }
        if (overlay_api == nullptr || keyboard_focus_view_ == nullptr) {
            return;
        }
        const auto overlay = overlays_.find(keyboard_focus_view_->Id());
        if (overlay == overlays_.end() || overlay->second.handle == vr::k_ulOverlayHandleInvalid ||
            !overlay->second.visible) {
            return;
        }
        constexpr std::uint32_t kMaximumKeyboardCharacters = 256;
        const auto result = overlay_api->ShowKeyboardForOverlay(
            overlay->second.handle,
            vr::k_EGamepadTextInputModeNormal,
            vr::k_EGamepadTextInputLineModeMultipleLines,
            "SkyUltra text entry",
            kMaximumKeyboardCharacters,
            "",
            true,
            keyboard_focus_view_->Id());
        if (result != vr::VROverlayError_None) {
            if (!logged_keyboard_open_failure_) {
                SKSE::log::warn("SkyUltra could not open the OpenVR keyboard (error {}).", static_cast<int>(result));
                logged_keyboard_open_failure_ = true;
            }
            return;
        }
        keyboard_open_ = true;
        keyboard_overlay_view_id_ = keyboard_focus_view_->Id();
        if (!logged_keyboard_open_) {
            SKSE::log::info("SkyUltra opened the OpenVR text-entry keyboard for view {}.", keyboard_overlay_view_id_);
            logged_keyboard_open_ = true;
        }
    }

    static std::u32string DecodeUtf8(std::string_view text)
    {
        std::u32string result;
        result.reserve(text.size());
        for (std::size_t i = 0; i < text.size();) {
            const auto first = static_cast<unsigned char>(text[i]);
            if (first <= 0x7FU) {
                result.push_back(first);
                ++i;
                continue;
            }
            std::size_t length{};
            char32_t codepoint{};
            char32_t minimum{};
            if ((first & 0xE0U) == 0xC0U) {
                length = 2;
                codepoint = first & 0x1FU;
                minimum = 0x80;
            } else if ((first & 0xF0U) == 0xE0U) {
                length = 3;
                codepoint = first & 0x0FU;
                minimum = 0x800;
            } else if ((first & 0xF8U) == 0xF0U) {
                length = 4;
                codepoint = first & 0x07U;
                minimum = 0x10000;
            } else {
                result.push_back(U'\uFFFD');
                ++i;
                continue;
            }
            if (i + length > text.size()) {
                result.push_back(U'\uFFFD');
                ++i;
                continue;
            }
            bool valid = true;
            for (std::size_t j = 1; j < length; ++j) {
                const auto continuation = static_cast<unsigned char>(text[i + j]);
                if ((continuation & 0xC0U) != 0x80U) {
                    valid = false;
                    break;
                }
                codepoint = (codepoint << 6U) | (continuation & 0x3FU);
            }
            if (!valid || codepoint < minimum || codepoint > 0x10FFFFU ||
                (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
                result.push_back(U'\uFFFD');
                ++i;
                continue;
            }
            result.push_back(codepoint);
            i += length;
        }
        return result;
    }

    void SendVirtualKeyboardInput(const vr::VREvent_t& event)
    {
        if (!keyboard_open_ || keyboard_focus_view_ == nullptr ||
            keyboard_focus_view_->Id() != keyboard_overlay_view_id_ ||
            event.eventType != vr::VREvent_KeyboardCharInput) {
            return;
        }
        const auto& bytes = event.data.keyboard.cNewInput;
        const auto end = std::find(std::begin(bytes), std::end(bytes), '\0');
        const std::string_view text(bytes, static_cast<std::size_t>(end - std::begin(bytes)));
        if (text.empty()) {
            return;
        }
        if (text == "\x1b") {
            CloseVirtualKeyboard(vr::VROverlay());
            return;
        }
        if (text == "\b") {
            KeyInput key{};
            key.virtual_key = VK_BACK;
            key.pressed = true;
            keyboard_focus_view_->SendKey(key);
            key.pressed = false;
            keyboard_focus_view_->SendKey(key);
            return;
        }
        TextInput input{};
        input.text = DecodeUtf8(text);
        input.committed = true;
        if (!input.text.empty()) {
            keyboard_focus_view_->SendText(input);
            if (!logged_keyboard_text_) {
                SKSE::log::info("SkyUltra routed virtual-keyboard text to view {}.", keyboard_overlay_view_id_);
                logged_keyboard_text_ = true;
            }
        }
    }

    void PollVirtualKeyboardEvents(vr::IVROverlay* overlay_api)
    {
        if (!keyboard_open_ || overlay_api == nullptr) {
            return;
        }
        const auto overlay = overlays_.find(keyboard_overlay_view_id_);
        if (overlay == overlays_.end() || overlay->second.handle == vr::k_ulOverlayHandleInvalid ||
            !overlay->second.visible || keyboard_focus_view_ == nullptr ||
            keyboard_focus_view_->Id() != keyboard_overlay_view_id_) {
            CloseVirtualKeyboard(overlay_api);
            return;
        }
        vr::VREvent_t event{};
        // Poll only the focused overlay so we never drain Skyrim's global OpenVR event queue.
        while (overlay_api->PollNextOverlayEvent(overlay->second.handle, &event, sizeof(event))) {
            if (event.eventType == vr::VREvent_KeyboardCharInput) {
                SendVirtualKeyboardInput(event);
            } else if (event.eventType == vr::VREvent_KeyboardClosed ||
                event.eventType == vr::VREvent_KeyboardDone) {
                keyboard_open_ = false;
                keyboard_overlay_view_id_ = 0;
            }
        }
    }

    static float DeadzoneStick(float value) noexcept
    {
        value = std::clamp(value, -1.0F, 1.0F);
        constexpr float kDeadzone = 0.30F;
        if (std::fabs(value) < kDeadzone) {
            return 0.0F;
        }
        return std::copysign((std::fabs(value) - kDeadzone) / (1.0F - kDeadzone), value);
    }

    void SendScroll(const PointerHit& hit, float stick_y, float delta_seconds, int hand)
    {
        auto& controller = controllers_[static_cast<std::size_t>(hand)];
        const float axis = DeadzoneStick(stick_y);
        if (axis == 0.0F) {
            controller.scroll_accumulator = 0.0F;
            return;
        }
        controller.scroll_accumulator += axis * 6.0F * delta_seconds;
        while (std::fabs(controller.scroll_accumulator) >= 1.0F) {
            MouseInput input{};
            input.position = hit.position;
            input.wheel = true;
            input.wheel_y = controller.scroll_accumulator > 0.0F ? 120 : -120;
            hit.view->SendMouse(input);
            controller.scroll_accumulator += controller.scroll_accumulator > 0.0F ? -1.0F : 1.0F;
        }
    }

    void UpdateControllerInput(vr::IVROverlay* overlay_api, vr::IVRSystem* vr_system,
        vr::IVRCompositor* compositor, const std::vector<OverlayView>& views,
        SizeU viewport, ID3D11Device* device)
    {
        std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> render_poses{};
        std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> game_poses{};
        const bool poses_ready = compositor != nullptr &&
            compositor->GetLastPoses(render_poses.data(), static_cast<std::uint32_t>(render_poses.size()),
                game_poses.data(), static_cast<std::uint32_t>(game_poses.size())) == vr::VRCompositorError_None;

        std::array<bool, 2> trigger_edges{};
        std::array<bool, 2> keyboard_button_edges{};
        for (int hand = 0; hand < 2; ++hand) {
            auto& controller = controllers_[static_cast<std::size_t>(hand)];
            const bool previous_trigger = controller.trigger_was_down;
            const bool previous_keyboard_button = controller.keyboard_button_was_down;
            controller.trigger_down = false;
            controller.keyboard_button_down = false;
            controller.pose_valid = false;
            controller.stick_y = 0.0F;

            if (vr_system != nullptr) {
                const auto role = hand == 0 ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;
                const auto device_index = vr_system->GetTrackedDeviceIndexForControllerRole(role);
                if (device_index != vr::k_unTrackedDeviceIndexInvalid &&
                    device_index < vr::k_unMaxTrackedDeviceCount) {
                    vr::VRControllerState_t state{};
                    if (vr_system->GetControllerState(device_index, &state, sizeof(state))) {
                        controller.trigger_down = state.rAxis[1].x > 0.5F ||
                            (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) != 0;
                        controller.keyboard_button_down =
                            (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0;
                        controller.stick_y = std::clamp(state.rAxis[0].y, -1.0F, 1.0F);
                        if (poses_ready && PoseUsable(render_poses[device_index])) {
                            controller.pose_valid = true;
                            controller.aim_pose = ResolveAimPose(vr_system, device_index, state,
                                render_poses[device_index].mDeviceToAbsoluteTracking, controller);
                        }
                    } else {
                        const auto hand_index = static_cast<std::size_t>(hand);
                        controller.trigger_down = g_vr_trigger_fallback[hand_index].load(std::memory_order_acquire);
                        if (GetTickCount64() - g_vr_stick_tick[hand_index].load(std::memory_order_acquire) <= 250U) {
                            controller.stick_y = g_vr_stick_y_fallback[hand_index].load(std::memory_order_acquire) / 1000.0F;
                        }
                        if (poses_ready && PoseUsable(render_poses[device_index])) {
                            controller.pose_valid = true;
                            controller.aim_pose = ApplyAimPitch(
                                render_poses[device_index].mDeviceToAbsoluteTracking, -35.0F);
                        }
                    }
                }
            }
            controller.keyboard_button_down = controller.keyboard_button_down ||
                g_vr_keyboard_button_fallback[static_cast<std::size_t>(hand)].load(std::memory_order_acquire);
            trigger_edges[static_cast<std::size_t>(hand)] = controller.trigger_down && !previous_trigger;
            keyboard_button_edges[static_cast<std::size_t>(hand)] =
                controller.keyboard_button_down && !previous_keyboard_button;
        }

        if (seed_trigger_levels_) {
            for (std::size_t hand = 0; hand < controllers_.size(); ++hand) {
                controllers_[hand].trigger_was_down = controllers_[hand].trigger_down;
                controllers_[hand].keyboard_button_was_down = controllers_[hand].keyboard_button_down;
                trigger_edges[hand] = false;
                keyboard_button_edges[hand] = false;
            }
            seed_trigger_levels_ = false;
        }

        PollVirtualKeyboardEvents(overlay_api);

        if (!poses_ready || !PoseUsable(render_poses[vr::k_unTrackedDeviceIndex_Hmd])) {
            ReleaseMouseButton();
            LeaveHoveredView();
            active_pointer_hand_ = -1;
            for (auto& laser : lasers_) {
                HideLaser(overlay_api, laser);
            }
            for (auto& controller : controllers_) {
                controller.trigger_was_down = controller.trigger_down;
                controller.keyboard_button_was_down = controller.keyboard_button_down;
            }
            return;
        }

        const auto& hmd_pose = render_poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
        const auto tracking_space = compositor->GetTrackingSpace();
        pointer_candidates_.clear();
        if (pointer_candidates_.capacity() < views.size()) {
            pointer_candidates_.reserve(views.size());
        }
        std::optional<std::size_t> modal_floor;
        for (std::size_t i = 0; i < views.size(); ++i) {
            if (views[i].view != nullptr && views[i].config.visible && views[i].config.modal) {
                modal_floor = i;
            }
        }
        for (std::size_t i = 0; i < views.size(); ++i) {
            const auto& overlay_view = views[i];
            if (overlay_view.view == nullptr || !overlay_view.config.visible ||
                overlay_view.config.size.width == 0 || overlay_view.config.size.height == 0) {
                continue;
            }
            const auto found = overlays_.find(overlay_view.view->Id());
            if (found == overlays_.end() || !found->second.visible ||
                found->second.handle == vr::k_ulOverlayHandleInvalid ||
                found->second.width_in_meters <= 0.0F || found->second.height_in_meters <= 0.0F) {
                continue;
            }
            vr::HmdMatrix34_t world_transform = Multiply(hmd_pose, found->second.local_transform);
            if (found->second.has_world_lock) {
                world_transform = found->second.world_locked_transform;
            } else if (found->second.tracked_device != vr::k_unTrackedDeviceIndex_Hmd &&
                       found->second.tracked_device < vr::k_unMaxTrackedDeviceCount &&
                       PoseUsable(render_poses[found->second.tracked_device])) {
                world_transform = Multiply(
                    render_poses[found->second.tracked_device].mDeviceToAbsoluteTracking,
                    found->second.local_transform);
            }
            pointer_candidates_.push_back({
                &overlay_view,
                ResolveViewBounds(overlay_view.config, viewport),
                world_transform,
                found->second.width_in_meters,
                found->second.height_in_meters,
                i,
            });
        }

        if (keyboard_focus_view_ != nullptr &&
            FindCandidate(keyboard_focus_view_->Id()) == nullptr) {
            SetKeyboardFocus(std::shared_ptr<IWebView>{}, overlay_api);
        }

        std::array<std::optional<PointerHit>, 2> hits{};
        for (int hand = 0; hand < 2; ++hand) {
            auto& controller = controllers_[static_cast<std::size_t>(hand)];
            if (controller.pose_valid) {
                if (!logged_first_vr_controller_pose_) {
                    SKSE::log::info("SkyUltra acquired a tracked VR controller pose ({} hand).",
                        hand == 0 ? "left" : "right");
                    logged_first_vr_controller_pose_ = true;
                }
                hits[static_cast<std::size_t>(hand)] = IntersectCandidates(controller.aim_pose, true, modal_floor);
                if (hits[static_cast<std::size_t>(hand)]) {
                    if (!logged_first_vr_ray_hit_) {
                        SKSE::log::info("SkyUltra's VR controller ray first hit view {}.",
                            hits[static_cast<std::size_t>(hand)]->view_id);
                        logged_first_vr_ray_hit_ = true;
                    }
                    UpdateLaser(overlay_api, device, tracking_space, hand, controller.aim_pose,
                        hmd_pose, hits[static_cast<std::size_t>(hand)]->distance);
                } else {
                    HideLaser(overlay_api, lasers_[static_cast<std::size_t>(hand)]);
                }
            } else {
                HideLaser(overlay_api, lasers_[static_cast<std::size_t>(hand)]);
            }
        }

        if (pressed_hand_ >= 0) {
            const auto hand = static_cast<std::size_t>(pressed_hand_);
            const auto* candidate = FindCandidate(pressed_view_id_);
            if (!controllers_[hand].pose_valid || !controllers_[hand].trigger_down || candidate == nullptr) {
                ReleaseMouseButton();
            } else {
                float u{}, v{}, distance{};
                if (IntersectQuad(candidate->world_transform, candidate->width_in_meters,
                        candidate->height_in_meters, controllers_[hand].aim_pose, false, u, v, distance)) {
                    pressed_position_ = {
                        u * static_cast<float>(candidate->overlay_view->config.size.width),
                        v * static_cast<float>(candidate->overlay_view->config.size.height),
                    };
                    MouseInput move{};
                    move.position = pressed_position_;
                    move.moved = true;
                    pressed_view_->SendMouse(move);
                }
            }
        }

        int selected_hand = pressed_hand_;
        if (selected_hand < 0) {
            if (trigger_edges[1] && hits[1]) selected_hand = 1;
            else if (trigger_edges[0] && hits[0]) selected_hand = 0;
            else if (active_pointer_hand_ >= 0 && hits[static_cast<std::size_t>(active_pointer_hand_)]) {
                selected_hand = active_pointer_hand_;
            } else if (hits[1]) selected_hand = 1;
            else if (hits[0]) selected_hand = 0;
        }
        active_pointer_hand_ = selected_hand;

        if (pressed_hand_ < 0) {
            if (selected_hand >= 0 && hits[static_cast<std::size_t>(selected_hand)]) {
                const auto& hit = *hits[static_cast<std::size_t>(selected_hand)];
                UpdateHoveredView(hit);
                if (trigger_edges[static_cast<std::size_t>(selected_hand)] &&
                    controllers_[static_cast<std::size_t>(selected_hand)].trigger_down) {
                    SetKeyboardFocus(hit.view, overlay_api);
                    MouseInput down{};
                    down.position = hit.position;
                    down.button = MouseButton::Left;
                    down.button_action = MouseButtonAction::Down;
                    hit.view->SendMouse(down);
                    pressed_view_ = hit.view;
                    pressed_view_id_ = hit.view_id;
                    pressed_position_ = hit.position;
                    pressed_hand_ = selected_hand;
                    if (!logged_first_vr_click_) {
                        SKSE::log::info("SkyUltra routed its first VR controller click to view {} at ({:.1f}, {:.1f}).",
                            hit.view_id, hit.position.x, hit.position.y);
                        logged_first_vr_click_ = true;
                    }
                }
                if (pressed_hand_ < 0 && !controllers_[static_cast<std::size_t>(selected_hand)].trigger_down) {
                    const auto now = std::chrono::steady_clock::now();
                    const float delta_seconds = previous_pointer_tick_.time_since_epoch().count() == 0 ? 0.0F :
                        std::clamp(std::chrono::duration<float>(now - previous_pointer_tick_).count(), 0.0F, 0.1F);
                    SendScroll(hit, controllers_[static_cast<std::size_t>(selected_hand)].stick_y,
                        delta_seconds, selected_hand);
                }
            } else {
                LeaveHoveredView();
            }
        }

        if (keyboard_button_edges[0] || keyboard_button_edges[1]) {
            ToggleVirtualKeyboard(overlay_api);
        }

        for (auto& controller : controllers_) {
            controller.trigger_was_down = controller.trigger_down;
            controller.keyboard_button_was_down = controller.keyboard_button_down;
        }
        previous_pointer_tick_ = std::chrono::steady_clock::now();
    }

    std::unordered_map<ViewId, ManagedOverlay> overlays_;
    std::array<LaserOverlay, 2> lasers_{};
    std::array<ControllerPointer, 2> controllers_{};
    std::vector<PointerCandidate> pointer_candidates_;
    ComPtr<ID3D11Texture2D> laser_texture_;
    std::shared_ptr<IWebView> keyboard_focus_view_;
    std::shared_ptr<IWebView> hovered_view_;
    ViewId hovered_view_id_{};
    PointF hovered_position_{};
    std::shared_ptr<IWebView> pressed_view_;
    ViewId pressed_view_id_{};
    PointF pressed_position_{};
    int active_pointer_hand_{ -1 };
    int pressed_hand_{ -1 };
    bool seed_trigger_levels_{ true };
    bool keyboard_open_{};
    ViewId keyboard_overlay_view_id_{};
    std::chrono::steady_clock::time_point previous_pointer_tick_{};
    bool logged_missing_runtime_{};
    bool logged_missing_gpu_frame_{};
    bool logged_texture_failure_{};
    bool logged_texture_submit_failure_{};
    bool logged_create_failure_{};
    bool logged_show_failure_{};
    bool logged_first_overlay_{};
    bool logged_laser_failure_{};
    bool logged_first_vr_controller_pose_{};
    bool logged_first_vr_ray_hit_{};
    bool logged_first_vr_click_{};
    bool logged_keyboard_open_{};
    bool logged_keyboard_open_failure_{};
    bool logged_keyboard_text_{};
};
#endif

void DrawUiPassMenuFrame() noexcept;

class SkyUltraOverlayMenu final : public RE::IMenu {
public:
    static RE::IMenu* Create()
    {
        auto* menu = new SkyUltraOverlayMenu();
        menu->depthPriority = 9;
        menu->menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving);
        menu->menuFlags.set(RE::UI_MENU_FLAGS::kCustomRendering);
        // kDontHideCursorWhenTopmost also increments a UI counter checked by
        // MenuOpenHandler. A rendering carrier must not block Skyrim hotkeys.
        return menu;
    }

    void PostDisplay() override
    {
        RE::IMenu::PostDisplay();
        DrawUiPassMenuFrame();
    }
};

class OverlayRenderer final {
public:
    void DrawPresent(REX::W32::IDXGISwapChain* game_swap_chain)
    {
#if defined(ENABLE_SKYRIM_VR)
        if (REL::Module::IsVR()) {
            DrawVr();
            if (game_swap_chain != nullptr) {
                DrawIntoTarget(game_swap_chain, false, true);
            }
            return;
        }
#endif
        auto* render_window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
        if (render_window != nullptr && render_window->swapChain != game_swap_chain) {
            return;
        }
        RecordPresentInterval();
        // The UI-pass callback runs earlier in this frame. Consume its marker so Present
        // is used only when Skyrim did not run the carrier menu's UI pass.
        if (g_ui_pass_composited.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        DrawIntoTarget(game_swap_chain, false);
    }

    [[nodiscard]] bool DrawUiPass()
    {
#if defined(ENABLE_SKYRIM_VR)
        if (REL::Module::IsVR()) {
            return false;
        }
#endif
        return DrawIntoTarget(nullptr, true);
    }

    void ShutdownVR() noexcept
    {
#if defined(ENABLE_SKYRIM_VR)
        const std::scoped_lock draw_lock(draw_mutex_);
        vr_overlays_.Shutdown();
#endif
    }

    void ReleaseBackBufferForResize(REX::W32::IDXGISwapChain* swap_chain) noexcept
    {
        const std::scoped_lock draw_lock(draw_mutex_);
        if (back_buffer_swap_chain_ != swap_chain) {
            return;
        }
        back_buffer_rtv_.Reset();
        back_buffer_.Reset();
        back_buffer_swap_chain_ = nullptr;
        back_buffer_width_ = 0;
        back_buffer_height_ = 0;
    }

private:
    class SkyrimRendererScope final {
    public:
        explicit SkyrimRendererScope(RE::BSGraphics::Renderer* renderer) : renderer_(renderer) { renderer_->Lock(); }
        SkyrimRendererScope(const SkyrimRendererScope&) = delete;
        SkyrimRendererScope& operator=(const SkyrimRendererScope&) = delete;
        ~SkyrimRendererScope() { renderer_->Unlock(); }
    private:
        RE::BSGraphics::Renderer* renderer_;
    };

    class ContextThreadScope final {
    public:
        explicit ContextThreadScope(ID3D11Multithread* multithread) : multithread_(multithread) {
            if(multithread_)multithread_->Enter();
        }
        ContextThreadScope(const ContextThreadScope&) = delete;
        ContextThreadScope& operator=(const ContextThreadScope&) = delete;
        ~ContextThreadScope() { if(multithread_)multithread_->Leave(); }
    private:
        ID3D11Multithread* multithread_;
    };

    struct GpuTimestampSlot {
        ComPtr<ID3D11Query> disjoint;
        ComPtr<ID3D11Query> start;
        ComPtr<ID3D11Query> end;
        ComPtr<ID3D11Query> compositor_start;
        bool compositor_started{};
        bool pending{};
        bool ended{};
    };

    class GpuTimingScope final {
    public:
        GpuTimingScope() = default;
        GpuTimingScope(OverlayRenderer* owner, ID3D11DeviceContext* context, std::size_t slot) :
            owner_(owner), context_(context), slot_(slot) {}
        GpuTimingScope(const GpuTimingScope&) = delete;
        GpuTimingScope& operator=(const GpuTimingScope&) = delete;
        GpuTimingScope(GpuTimingScope&& other) noexcept :
            owner_(std::exchange(other.owner_, nullptr)), context_(other.context_), slot_(other.slot_) {}
        GpuTimingScope& operator=(GpuTimingScope&&) = delete;
        ~GpuTimingScope()
        {
            if (owner_ != nullptr) {
                owner_->EndGpuTiming(context_, slot_);
            }
        }
        void BeginComposite() {
            if (owner_!=nullptr) owner_->BeginCompositeTiming(context_,slot_);
        }

    private:
        OverlayRenderer* owner_{};
        ID3D11DeviceContext* context_{};
        std::size_t slot_{};
    };

    class ContextStateScope final {
    public:
        ContextStateScope(ID3D11DeviceContext1* context, ID3DDeviceContextState* isolated_state) :
            context_(context)
        {
            if (context_ == nullptr || isolated_state == nullptr) {
                context_ = nullptr;
                return;
            }
            context_->SwapDeviceContextState(isolated_state, previous_state_.GetAddressOf());
            context_->ClearState();
        }

        ContextStateScope(const ContextStateScope&) = delete;
        ContextStateScope& operator=(const ContextStateScope&) = delete;
        ~ContextStateScope()
        {
            if (context_ != nullptr && previous_state_ != nullptr) {
                context_->SwapDeviceContextState(previous_state_.Get(), nullptr);
            }
        }

        [[nodiscard]] bool Active() const noexcept { return context_ != nullptr; }

    private:
        ID3D11DeviceContext1* context_{};
        ComPtr<ID3DDeviceContextState> previous_state_;
    };

#if defined(ENABLE_SKYRIM_VR)
    void DrawVr()
    {
        const std::scoped_lock draw_lock(draw_mutex_);
        auto* device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetDevice());
        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (device == nullptr || renderer == nullptr) {
            vr_overlays_.HideAll();
            return;
        }
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
        if (context == nullptr) {
            vr_overlays_.HideAll();
            return;
        }

        ComPtr<ID3D11RenderTargetView> target;
        ComPtr<ID3D11DepthStencilView> depth_stencil;
        context->OMGetRenderTargets(1, target.GetAddressOf(), depth_stencil.GetAddressOf());
        std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
        UINT viewport_count = static_cast<UINT>(viewports.size());
        context->RSGetViewports(&viewport_count, viewports.data());
        (void)EnsureContextState(device, context);
        ContextStateScope context_state(context1_.Get(), isolated_context_state_.Get());
        auto* runtime = g_runtime.load(std::memory_order_acquire);
        if (runtime != nullptr) {
            runtime->PumpFrame(device, context);
        }
        if (!context_state.Active()) {
            context->OMSetRenderTargets(1, target.GetAddressOf(), depth_stencil.Get());
            context->RSSetViewports(viewport_count, viewports.data());
        }

        const std::scoped_lock views_lock(g_overlay_views_mutex);
        const auto& views = g_overlay_views;
        if (!g_visible.load(std::memory_order_acquire)) {
            WarmupFrameworkSplash(device, context);
            vr_overlays_.HideAll();
            return;
        }
        std::vector<ViewId> preview_owners;
        for (const auto& overlay_view : views) {
            if (overlay_view.view != nullptr && overlay_view.config.visible && overlay_view.config.opacity > 0.0F) {
                preview_owners.push_back(overlay_view.view->Id());
            }
        }
        if (!preview_owners.empty()) {
            RenderActorPreviews(device, context, preview_owners);
        }
        const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
        vr_overlays_.Update(views, SizeU{ screen.width, screen.height }, device);
    }
#endif

    bool DrawIntoTarget(REX::W32::IDXGISwapChain* game_swap_chain, bool ui_pass, bool already_pumped = false)
    {
        [[maybe_unused]] CallbackCpuTiming callback_cpu_timing(g_visible.load(std::memory_order_relaxed));
        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if(renderer==nullptr)return false;
        // Lock order is engine renderer -> SkyUltra draw -> overlay views ->
        // D3D context. The engine lock is recursive when its UI pass owns it.
        // Taking draw_mutex_ first could deadlock a concurrent Present callback
        // which already owns the engine renderer lock.
        const SkyrimRendererScope renderer_scope(renderer);
        // A UI pass may have completed while Present waited for the engine
        // lock. Consume its marker here as well as at the fast Present entry.
        if(!ui_pass && g_ui_pass_composited.exchange(false,std::memory_order_acq_rel))return false;
        const std::scoped_lock draw_lock(draw_mutex_);
        if (ui_pass && !g_visible.load(std::memory_order_acquire)) {
            return false;
        }

        const std::scoped_lock views_lock(g_overlay_views_mutex);
        const auto& views = g_overlay_views;
        std::erase_if(software_textures_, [&views](const auto& texture) {
            return std::none_of(views.begin(), views.end(), [&texture](const OverlayView& view) {
                return view.view != nullptr && view.view->Id() == texture.first;
            });
        });
        auto* game_device = RE::BSGraphics::Renderer::GetDevice();
        auto* render_window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
        if (game_device == nullptr || renderer == nullptr || (!ui_pass && game_swap_chain == nullptr)) {
            if (g_visible.load(std::memory_order_acquire) && !logged_missing_device_) {
                SKSE::log::error("SkyUltra overlay is visible but Skyrim's D3D11 renderer is unavailable.");
                logged_missing_device_ = true;
            }
            return false;
        }

        auto* device = reinterpret_cast<ID3D11Device*>(game_device);
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
        if (context == nullptr) {
            if (g_visible.load(std::memory_order_acquire) && !logged_missing_context_) {
                SKSE::log::error("SkyUltra overlay is visible but Skyrim's D3D11 context is unavailable.");
                logged_missing_context_ = true;
            }
            return false;
        }

        const auto visible = g_visible.load(std::memory_order_acquire);
        auto* runtime = g_runtime.load(std::memory_order_acquire);
        EnsureContextThreadProtection(device,context);
        // Keep capture, state swaps, browser command-list execution, model
        // redraw/export, sprite composition and restoration in one transaction.
        const ContextThreadScope context_thread_scope(context_multithread_.Get());
        (void)EnsureContextState(device, context);
        if (!visible) {
            WarmupFrameworkSplash(device, context);
            WarmupActorPreviewRenderer(device);
            if (!already_pumped && runtime != nullptr) {
                ComPtr<ID3D11RenderTargetView> target;
                ComPtr<ID3D11DepthStencilView> depth_stencil;
                context->OMGetRenderTargets(1, target.GetAddressOf(), depth_stencil.GetAddressOf());
                std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
                UINT viewport_count = static_cast<UINT>(viewports.size());
                context->RSGetViewports(&viewport_count, viewports.data());
                ContextStateScope context_state(context1_.Get(), isolated_context_state_.Get());
                runtime->PumpFrame(device, context);
                if (!context_state.Active()) {
                    context->OMSetRenderTargets(1, target.GetAddressOf(), depth_stencil.Get());
                    context->RSSetViewports(viewport_count, viewports.data());
                }
            }
            return false;
        }
        if (views.empty()) {
            if (!logged_missing_view_) {
                SKSE::log::error("SkyUltra overlay is visible but has no registered browser views.");
                logged_missing_view_ = true;
            }
            return false;
        }
        if (!EnsureDevice(device, context)) {
            if (!logged_device_setup_failure_) {
                SKSE::log::error("SkyUltra could not initialize its D3D11 overlay device resources.");
                logged_device_setup_failure_ = true;
            }
            return false;
        }

        ComPtr<ID3D11RenderTargetView> target;
        ComPtr<ID3D11DepthStencilView> depth_stencil;
        std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
        UINT viewport_count = static_cast<UINT>(viewports.size());
        ComPtr<ID3D11RenderTargetView> window_render_target;
        if (ui_pass) {
            // The engine menu callback runs with Skyrim's UI target bound. Composite into it
            // directly instead of trying to infer which swap-chain buffer Present will show.
            context->OMGetRenderTargets(1, target.GetAddressOf(), depth_stencil.GetAddressOf());
            context->RSGetViewports(&viewport_count, viewports.data());
            if (target == nullptr) {
                if (!logged_missing_ui_target_) {
                    SKSE::log::warn("SkyUltra UI-pass menu ran without a bound render target; using Present fallback.");
                    logged_missing_ui_target_ = true;
                }
                return false;
            }
        } else {
            // Capture output references before PumpFrame; Ultralight uses this immediate context
            // for its off-screen work and leaves its own render target bound.
            context->OMGetRenderTargets(1, target.GetAddressOf(), depth_stencil.GetAddressOf());
            context->RSGetViewports(&viewport_count, viewports.data());
            if (render_window != nullptr && render_window->swapChain == game_swap_chain &&
                render_window->renderView != nullptr) {
                window_render_target =
                    reinterpret_cast<ID3D11RenderTargetView*>(render_window->renderView);
            }
        }

        ContextStateScope context_state(context1_.Get(), isolated_context_state_.Get());
        [[maybe_unused]] auto gpu_timing_scope = BeginGpuTiming(context);
        if (!already_pumped && runtime != nullptr) {
            runtime->PumpFrame(device, context);
        }

        if (!g_visible.load(std::memory_order_acquire)) {
            return false;
        }
        if (g_clear_retained_frames.exchange(false, std::memory_order_acq_rel)) {
            retained_config_frames_.clear();
        }
        // Decode/upload splash frame before background actor/item renders so splash playback stays smooth.
        const auto splash_frame = TickFrameworkSplash(device_.Get(), context_.Get());
        if (!already_pumped) {
            std::vector<ViewId> preview_owners;
            for (const auto& overlay_view : views) {
                if (overlay_view.view != nullptr && overlay_view.config.visible && overlay_view.config.opacity > 0.0F) {
                    preview_owners.push_back(overlay_view.view->Id());
                }
            }
            RenderActorPreviews(device, context, preview_owners);
            if (!FrameworkSplashActive()) {
                WarmupFrameworkSplash(device, context);
                WarmupActorPreviewRenderer(device);
            }
        }

        UINT target_width{};
        UINT target_height{};
        if (ui_pass) {
            ComPtr<ID3D11Resource> target_resource;
            ComPtr<ID3D11Texture2D> target_texture;
            target->GetResource(target_resource.GetAddressOf());
            if (target_resource == nullptr || FAILED(target_resource.As(&target_texture))) {
                if (!logged_missing_ui_target_) {
                    SKSE::log::warn("SkyUltra UI-pass render target is not a D3D11 texture; using Present fallback.");
                    logged_missing_ui_target_ = true;
                }
                return false;
            }
            D3D11_TEXTURE2D_DESC target_desc{};
            target_texture->GetDesc(&target_desc);
            target_width = target_desc.Width;
            target_height = target_desc.Height;
        } else {
            if (!EnsureBackBuffer(game_swap_chain, target.Get(), window_render_target.Get())) {
                if (!logged_back_buffer_failure_) {
                    SKSE::log::error("SkyUltra could not create a D3D11 view for Skyrim's back buffer.");
                    logged_back_buffer_failure_ = true;
                }
                return false;
            }
            target = back_buffer_rtv_;
            target_width = back_buffer_width_;
            target_height = back_buffer_height_;
            viewport_count = 1;
            viewports[0] = D3D11_VIEWPORT{
                0.0F, 0.0F, static_cast<float>(target_width), static_cast<float>(target_height), 0.0F, 1.0F };
        }

        if (target_width == 0 || target_height == 0) {
            return false;
        }
        if (viewport_count == 0 || viewports[0].Width <= 0.0F || viewports[0].Height <= 0.0F) {
            viewport_count = 1;
            viewports[0] = D3D11_VIEWPORT{
                0.0F, 0.0F, static_cast<float>(target_width), static_cast<float>(target_height), 0.0F, 1.0F };
        }

        const auto& active_viewport = viewports[0];
        const auto viewport_left = static_cast<LONG>(std::lround(active_viewport.TopLeftX));
        const auto viewport_top = static_cast<LONG>(std::lround(active_viewport.TopLeftY));

        // PumpFrame may have rebound Ultralight's off-screen target; put Skyrim's UI target back.
        context_->OMSetRenderTargets(1, target.GetAddressOf(), ui_pass ? depth_stencil.Get() : nullptr);
        context_->RSSetViewports(viewport_count, viewports.data());
        sprite_batch_->SetViewport(active_viewport);
        std::vector<std::shared_ptr<const void>> preview_frame_lifetimes;
        std::vector<ViewId> painted_view_ids;
        gpu_timing_scope.BeginComposite();
        sprite_batch_->Begin(DirectX::SpriteSortMode_Deferred, blend_state_.Get());
        bool drew_view{};
        bool drew_cursor{};
        std::vector<std::string> splashed_views;
        const SizeU viewport_size{
            static_cast<std::uint32_t>(std::max(0.0F, active_viewport.Width)),
            static_cast<std::uint32_t>(std::max(0.0F, active_viewport.Height)),
        };
        const bool any_interactive_panel_visible = std::ranges::any_of(views, [](const OverlayView& candidate) {
            return candidate.config.visible &&
                candidate.config.opacity > 0.0F &&
                !candidate.config.click_through &&
                candidate.config.layer != ViewLayer::Hud;
        });
        for (const auto& overlay_view : views) {
            auto* view = overlay_view.view.get();
            const auto& config = overlay_view.config;
            if (!config.visible || config.opacity <= 0.0F) {
                continue;
            }
            if (any_interactive_panel_visible && config.layer == ViewLayer::Hud && config.click_through) {
                continue;
            }
            if (view == nullptr) {
                splashed_views.push_back(config.id);
                drew_view = true;
                continue;
            }
            const bool page_awaiting_ready = PageAwaitingReady(view->Id());

            ID3D11ShaderResourceView* shader_resource_view{};
            RECT source{};
            if (auto frame = view->GetGpuFrame(); frame && frame->shader_resource_view != nullptr) {
                shader_resource_view = static_cast<ID3D11ShaderResourceView*>(frame->shader_resource_view);
                source = RECT{
                    static_cast<LONG>(frame->source_rect.x),
                    static_cast<LONG>(frame->source_rect.y),
                    static_cast<LONG>(frame->source_rect.x + frame->source_rect.width),
                    static_cast<LONG>(frame->source_rect.y + frame->source_rect.height),
                };
            } else if (auto frame = view->TakeFrame()) {
                auto& software = software_textures_[view->Id()];
                if (!EnsureViewTexture(software, frame->size)) {
                    if (!logged_software_texture_failure_) {
                        SKSE::log::error("SkyUltra could not create a D3D11 texture for a software-rendered view.");
                        logged_software_texture_failure_ = true;
                    }
                    continue;
                }
                UploadFrame(software, *frame);
                shader_resource_view = software.shader_resource_view.Get();
                source = RECT{ 0, 0, static_cast<LONG>(frame->size.width),
                    static_cast<LONG>(frame->size.height) };
            } else if (const auto software = software_textures_.find(view->Id());
                software != software_textures_.end() && software->second.shader_resource_view != nullptr) {
                shader_resource_view = software->second.shader_resource_view.Get();
                source = RECT{ 0, 0, static_cast<LONG>(software->second.size.width),
                    static_cast<LONG>(software->second.size.height) };
            }

            if (!page_awaiting_ready && shader_resource_view != nullptr &&
                source.right > source.left && source.bottom > source.top && !config.id.empty()) {
                retained_config_frames_[config.id] = RetainedViewFrame{ shader_resource_view, source };
                if (FrameworkSplashActive()) CompleteFrameworkSplash(config.id);
            } else if (!page_awaiting_ready && (shader_resource_view == nullptr ||
                           source.right <= source.left || source.bottom <= source.top) &&
                       (!config.transparent || config.id == "SkyUltra.Menu") && !config.id.empty()) {
                if (const auto retained = retained_config_frames_.find(config.id);
                    retained != retained_config_frames_.end() && retained->second.srv != nullptr &&
                    retained->second.source.right > retained->second.source.left &&
                    retained->second.source.bottom > retained->second.source.top) {
                    shader_resource_view = retained->second.srv.Get();
                    source = retained->second.source;
                }
            }

            if ((!config.transparent || config.id == "SkyUltra.Menu") && FrameworkSplashActive()) {
                if (splash_frame != nullptr || DrawLoadingBackdrop(config, viewport_size, viewport_left, viewport_top)) {
                    drew_view = true;
                }
                if (shader_resource_view != nullptr && !page_awaiting_ready) {
                    painted_view_ids.push_back(view->Id());
                }
                splashed_views.push_back(config.id);
                continue;
            }
            if (page_awaiting_ready) {
                continue;
            }

            if (shader_resource_view == nullptr || source.right <= source.left || source.bottom <= source.top) {
                if (!logged_missing_frame_) {
                    SKSE::log::warn("SkyUltra view {} has not produced a drawable frame yet.", view->Id());
                    logged_missing_frame_ = true;
                }
                continue;
            }
            auto bounds = ResolveViewBounds(config, viewport_size);
            auto clipped = bounds;
            if (config.clip.width > 0.0F && config.clip.height > 0.0F) {
                const auto clip_right = config.clip.x + config.clip.width;
                const auto clip_bottom = config.clip.y + config.clip.height;
                clipped.x = std::max(clipped.x, config.clip.x);
                clipped.y = std::max(clipped.y, config.clip.y);
                clipped.width = std::min(bounds.x + bounds.width, clip_right) - clipped.x;
                clipped.height = std::min(bounds.y + bounds.height, clip_bottom) - clipped.y;
            }
            const auto clipped_left = std::max(0.0F, clipped.x);
            const auto clipped_top = std::max(0.0F, clipped.y);
            const auto clipped_right = std::min(static_cast<float>(viewport_size.width),
                clipped.x + clipped.width);
            const auto clipped_bottom = std::min(static_cast<float>(viewport_size.height),
                clipped.y + clipped.height);
            if (bounds.width <= 0.0F || bounds.height <= 0.0F ||
                clipped_right <= clipped_left || clipped_bottom <= clipped_top) {
                continue;
            }

            const auto source_width = static_cast<float>(source.right - source.left);
            const auto source_height = static_cast<float>(source.bottom - source.top);
            const auto left_fraction = (clipped_left - bounds.x) / bounds.width;
            const auto top_fraction = (clipped_top - bounds.y) / bounds.height;
            const auto right_fraction = (clipped_right - bounds.x) / bounds.width;
            const auto bottom_fraction = (clipped_bottom - bounds.y) / bounds.height;
            const RECT clipped_source{
                source.left + static_cast<LONG>(std::lround(source_width * left_fraction)),
                source.top + static_cast<LONG>(std::lround(source_height * top_fraction)),
                source.left + static_cast<LONG>(std::lround(source_width * right_fraction)),
                source.top + static_cast<LONG>(std::lround(source_height * bottom_fraction)),
            };
            if (clipped_source.right <= clipped_source.left || clipped_source.bottom <= clipped_source.top) {
                continue;
            }

            const RECT destination{
                viewport_left + static_cast<LONG>(std::lround(clipped_left)),
                viewport_top + static_cast<LONG>(std::lround(clipped_top)),
                viewport_left + static_cast<LONG>(std::lround(clipped_right)),
                viewport_top + static_cast<LONG>(std::lround(clipped_bottom)),
            };
            const auto opacity = std::clamp(config.opacity, 0.0F, 1.0F);
            const auto tint = DirectX::XMVectorSet(opacity, opacity, opacity, opacity);
            sprite_batch_->Draw(shader_resource_view, destination, &clipped_source, tint);
            painted_view_ids.push_back(view->Id());
            drew_view = true;
            // Native actor pixels share this view's ordering, opacity and clip.
            // They never pass through a browser CPU bitmap or image encoder.
            if (const auto preview = GetActorPreviewFrame(view->Id()); preview && preview->shader_resource_view != nullptr) {
                const auto& rect = preview->normalized_bounds;
                const RectF actor_bounds{bounds.x + rect.x * bounds.width,
                    bounds.y + rect.y * bounds.height, rect.width * bounds.width, rect.height * bounds.height};
                const auto actor_left = std::max(clipped_left, actor_bounds.x);
                const auto actor_top = std::max(clipped_top, actor_bounds.y);
                const auto actor_right = std::min(clipped_right, actor_bounds.x + actor_bounds.width);
                const auto actor_bottom = std::min(clipped_bottom, actor_bounds.y + actor_bounds.height);
                if (actor_bounds.width > 0.0F && actor_bounds.height > 0.0F && actor_right > actor_left && actor_bottom > actor_top) {
                    const RECT actor_destination{viewport_left + static_cast<LONG>(std::lround(actor_left)),
                        viewport_top + static_cast<LONG>(std::lround(actor_top)),
                        viewport_left + static_cast<LONG>(std::lround(actor_right)),
                        viewport_top + static_cast<LONG>(std::lround(actor_bottom))};
                    const RECT actor_source{
                        static_cast<LONG>(std::lround((actor_left-actor_bounds.x)/actor_bounds.width*preview->size.width)),
                        static_cast<LONG>(std::lround((actor_top-actor_bounds.y)/actor_bounds.height*preview->size.height)),
                        static_cast<LONG>(std::lround((actor_right-actor_bounds.x)/actor_bounds.width*preview->size.width)),
                        static_cast<LONG>(std::lround((actor_bottom-actor_bounds.y)/actor_bounds.height*preview->size.height))};
                    sprite_batch_->Draw(static_cast<ID3D11ShaderResourceView*>(preview->shader_resource_view), actor_destination, &actor_source, tint);
                    preview_frame_lifetimes.push_back(preview->lifetime);
                }
            }
        }
        if (splash_frame) {
            const RECT full_screen{viewport_left,viewport_top,viewport_left+static_cast<LONG>(viewport_size.width),viewport_top+static_cast<LONG>(viewport_size.height)};
            sprite_batch_->Draw(splash_frame.Get(),full_screen,nullptr,DirectX::Colors::White);
            drew_view=true;
        }
        if (!FrameworkSplashActive() && g_visible.load(std::memory_order_acquire) &&
            g_cursor_visible.load(std::memory_order_acquire) && cursor_srv_ != nullptr) {
            constexpr LONG kCursorWidth = 36;
            constexpr LONG kCursorHeight = 42;
            const auto cursor_x = g_cursor_x_normalized.load(std::memory_order_relaxed);
            const auto cursor_y = g_cursor_y_normalized.load(std::memory_order_relaxed);
            const RECT cursor_destination{
                viewport_left + static_cast<LONG>(std::lround(cursor_x * active_viewport.Width)) - 4,
                viewport_top + static_cast<LONG>(std::lround(cursor_y * active_viewport.Height)) - 3,
                viewport_left + static_cast<LONG>(std::lround(cursor_x * active_viewport.Width)) - 4 + kCursorWidth,
                viewport_top + static_cast<LONG>(std::lround(cursor_y * active_viewport.Height)) - 3 + kCursorHeight,
            };
            const auto accent=g_cursor_accent.load(std::memory_order_relaxed);
            sprite_batch_->Draw(cursor_srv_.Get(),cursor_destination,nullptr,DirectX::XMVectorSet(
                static_cast<float>((accent>>16)&255U)/255.0F,static_cast<float>((accent>>8)&255U)/255.0F,static_cast<float>(accent&255U)/255.0F,1.0F));
            drew_cursor = true;
            if (!logged_first_cursor_draw_) {
                SKSE::log::info("SkyUltra drew its native in-game mouse cursor for the first time.");
                logged_first_cursor_draw_ = true;
            }
        }
        sprite_batch_->End();
        if(!painted_view_ids.empty()){
            const std::scoped_lock lock(g_page_load_mutex);
            for(const auto view:painted_view_ids)g_painted_views.try_emplace(view,true);
        }
        // CefInitialize must remain on the host main thread, but the backdrop must
        // reach a GPU submission before that potentially blocking startup.
        if (!splashed_views.empty()) {
            std::vector<std::function<void()>> tasks;
            { const std::scoped_lock lock(g_splash_tasks_mutex);
              for (const auto& key : splashed_views) {
                  const auto [first,last] = g_splash_tasks.equal_range(key);
                  for (auto item=first;item!=last;++item) tasks.push_back(std::move(item->second));
                  g_splash_tasks.erase(first,last);
              } }
            if (auto* queue = SKSE::GetTaskInterface()) for (auto& task : tasks) queue->AddTask(std::move(task));
        }
        if (!drew_view && !logged_missing_texture_) {
            SKSE::log::warn("SkyUltra has no visible browser textures ready for compositing.");
            logged_missing_texture_ = true;
        }

        // Keep the engine's menu pass bound for whatever draws after this menu callback.
        if (ui_pass) {
            context_->OMSetRenderTargets(1, target.GetAddressOf(), depth_stencil.Get());
            context_->RSSetViewports(viewport_count, viewports.data());
        }
        if (drew_view && ui_pass && !logged_first_ui_composite_) {
            SKSE::log::info("SkyUltra composited its first browser frame in Skyrim's UI pass ({}x{} target).",
                target_width, target_height);
            logged_first_ui_composite_ = true;
        } else if (drew_view && !ui_pass && !logged_first_present_composite_) {
            SKSE::log::info("SkyUltra composited its first browser frame at Present fallback ({}x{} target).",
                target_width, target_height);
            logged_first_present_composite_ = true;
        }
        const auto composited=drew_view || drew_cursor;
        // Publish before releasing the engine lock, not afterward in the menu
        // callback, so a waiting Present cannot miss the completed UI pass.
        if(ui_pass && composited)g_ui_pass_composited.store(true,std::memory_order_release);
        return composited;
    }

    void EnsureContextThreadProtection(ID3D11Device* device,ID3D11DeviceContext* context)
    {
        if(thread_protected_context_.Get()!=context) {
            context_multithread_.Reset();thread_protected_context_=context;
            logged_context_thread_protection_=false;
            if((device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)==0)
                (void)context->QueryInterface(IID_PPV_ARGS(context_multithread_.GetAddressOf()));
        }
        if(context_multithread_ && !context_multithread_->GetMultithreadProtected())
            context_multithread_->SetMultithreadProtected(TRUE);
        if(!logged_context_thread_protection_) {
            SKSE::log::info("SkyUltra render synchronization v3: Skyrim renderer lock active; D3D11 immediate-context protection {}. Full pass guarded through state restoration.",
                context_multithread_&&context_multithread_->GetMultithreadProtected()?"enabled":"unavailable");
            logged_context_thread_protection_=true;
        }
        // Never turn protection off: this immediate context is shared with the
        // engine and other plugins, including CEF's shared-texture path.
    }

    bool EnsureContextState(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (context_state_device_.Get() == device && context_state_context_.Get() == context) {
            return context1_ != nullptr && isolated_context_state_ != nullptr;
        }

        isolated_context_state_.Reset();
        context1_.Reset();
        context_state_device_ = device;
        context_state_context_ = context;
        const auto log_context_state_fallback = [this] {
            if (!logged_context_state_fallback_) {
                SKSE::log::warn("SkyUltra could not create a D3D11.1 context-state object; "
                                "only render-target and viewport state will be restored.");
                logged_context_state_fallback_ = true;
            }
        };

        ComPtr<ID3D11Device1> device1;
        ComPtr<ID3D11DeviceContext1> context1;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(device1.GetAddressOf()))) ||
            FAILED(context->QueryInterface(IID_PPV_ARGS(context1.GetAddressOf())))) {
            log_context_state_fallback();
            return false;
        }

        const auto feature_level = device->GetFeatureLevel();
        D3D_FEATURE_LEVEL selected_feature_level{};
        const UINT flags = (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0 ?
            D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED : 0U;
        if (FAILED(device1->CreateDeviceContextState(flags, &feature_level, 1,
                D3D11_SDK_VERSION, __uuidof(ID3D11Device), &selected_feature_level,
                isolated_context_state_.ReleaseAndGetAddressOf()))) {
            isolated_context_state_.Reset();
            log_context_state_fallback();
            return false;
        }

        context1_ = std::move(context1);
        if (!logged_context_state_ready_) {
            SKSE::log::info("SkyUltra will isolate and restore the complete D3D11 context state around its overlay pass.");
            logged_context_state_ready_ = true;
        }
        return true;
    }

    bool EnsureDevice(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (device_.Get() == device && context_.Get() == context && sprite_batch_ != nullptr &&
            blend_state_.Get() != nullptr) {
            return true;
        }

        sprite_batch_.reset();
        blend_state_.Reset();
        cursor_srv_.Reset();
        cursor_texture_.Reset();
        splash_pixel_.Reset();
        splash_attempted_ = false;
        software_textures_.clear();
        back_buffer_rtv_.Reset();
        back_buffer_.Reset();
        back_buffer_swap_chain_ = nullptr;
        logged_active_back_buffer_match_ = false;
        logged_unmatched_back_buffer_ = false;
        device_ = device;
        context_ = context;
        InitializeGpuTimingQueries();
        D3D11_BLEND_DESC blend_desc{};
        auto& target = blend_desc.RenderTarget[0];
        target.BlendEnable = TRUE;
        target.SrcBlend = D3D11_BLEND_ONE;
        target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOp = D3D11_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
        target.DestBlendAlpha = D3D11_BLEND_ONE;
        target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(device_->CreateBlendState(&blend_desc, blend_state_.GetAddressOf()))) {
            return false;
        }
        sprite_batch_ = std::make_unique<DirectX::SpriteBatch>(context_.Get());
        if (!EnsureCursorTexture() && !logged_cursor_texture_failure_) {
            SKSE::log::warn("SkyUltra could not create its in-game mouse cursor texture.");
            logged_cursor_texture_failure_ = true;
        }
        return true;
    }

    bool DrawLoadingBackdrop(const ViewConfig& config, SizeU viewport, LONG viewport_left, LONG viewport_top)
    {
        if (!splash_attempted_) {
            splash_attempted_ = true;
            const std::uint32_t white = 0xFFFFFFFFU;
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA initial{ &white, 4, 0 };
            ComPtr<ID3D11Texture2D> pixel;
            if (SUCCEEDED(device_->CreateTexture2D(&desc, &initial, pixel.GetAddressOf()))) {
                device_->CreateShaderResourceView(pixel.Get(), nullptr, splash_pixel_.GetAddressOf());
            }
        }
        if (!splash_pixel_) return false;
        auto bounds = ResolveViewBounds(config, viewport);
        if (config.clip.width > 0.0F && config.clip.height > 0.0F) {
            const auto right = std::min(bounds.x+bounds.width,config.clip.x+config.clip.width);
            const auto bottom = std::min(bounds.y+bounds.height,config.clip.y+config.clip.height);
            bounds.x = std::max(bounds.x,config.clip.x); bounds.y = std::max(bounds.y,config.clip.y);
            bounds.width = right-bounds.x; bounds.height = bottom-bounds.y;
        }
        const LONG left = viewport_left + static_cast<LONG>(std::lround(std::max(0.0F, bounds.x)));
        const LONG top = viewport_top + static_cast<LONG>(std::lround(std::max(0.0F, bounds.y)));
        const LONG right = viewport_left + static_cast<LONG>(std::lround(std::min(static_cast<float>(viewport.width), bounds.x + bounds.width)));
        const LONG bottom = viewport_top + static_cast<LONG>(std::lround(std::min(static_cast<float>(viewport.height), bounds.y + bounds.height)));
        if (right <= left || bottom <= top) return false;
        // The opening video owns the splash. Decoder/page startup gets a plain
        // backdrop so the retired logo screen cannot flash before or after it.
        sprite_batch_->Draw(splash_pixel_.Get(), RECT{left,top,right,bottom}, nullptr,
            DirectX::XMVectorSet(0,0,0,1));
        return true;
    }

    void InitializeGpuTimingQueries()
    {
        for (auto& slot : gpu_timing_slots_) {
            slot = {};
        }
        next_gpu_timing_slot_ = 0;
        gpu_timing_sample_count_ = 0;
        gpu_timing_total_ms_ = 0.0;
        gpu_timing_max_ms_ = 0.0;
        gpu_timing_window_started_ = false;
        gpu_timing_enabled_ = true;
        for (auto& slot : gpu_timing_slots_) {
            D3D11_QUERY_DESC desc{};
            desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            if (FAILED(device_->CreateQuery(&desc, slot.disjoint.GetAddressOf()))) {
                gpu_timing_enabled_ = false;
                break;
            }
            desc.Query = D3D11_QUERY_TIMESTAMP;
            if (FAILED(device_->CreateQuery(&desc, slot.start.GetAddressOf())) ||
                FAILED(device_->CreateQuery(&desc, slot.compositor_start.GetAddressOf())) ||
                FAILED(device_->CreateQuery(&desc, slot.end.GetAddressOf()))) {
                gpu_timing_enabled_ = false;
                break;
            }
            slot.pending = false;
            slot.ended = false;
        }
        if (!gpu_timing_enabled_) {
            for (auto& slot : gpu_timing_slots_) {
                slot = {};
            }
            if (!logged_gpu_timing_unavailable_) {
                SKSE::log::warn("SkyUltra could not create D3D11 timestamp queries; GPU timing is unavailable.");
                logged_gpu_timing_unavailable_ = true;
            }
        } else {
            logged_gpu_timing_unavailable_ = false;
        }
        ResetPerformance(gpu_timing_enabled_);
    }

    GpuTimingScope BeginGpuTiming(ID3D11DeviceContext* context)
    {
        if (!gpu_timing_enabled_ || context == nullptr) {
            return {};
        }
        PollGpuTiming(context);
        for (std::size_t offset = 0; offset < gpu_timing_slots_.size(); ++offset) {
            const auto index = (next_gpu_timing_slot_ + offset) % gpu_timing_slots_.size();
            auto& slot = gpu_timing_slots_[index];
            if (slot.pending) {
                continue;
            }
            context->Begin(slot.disjoint.Get());
            context->End(slot.start.Get());
            slot.pending = true;
            slot.ended = false;
            slot.compositor_started=false;
            next_gpu_timing_slot_ = (index + 1) % gpu_timing_slots_.size();
            return GpuTimingScope(this, context, index);
        }
        if (PerformanceMonitoringActive()) { const std::scoped_lock lock(g_performance_mutex);++g_performance.skipped_gpu; }
        return {};
    }

    void BeginCompositeTiming(ID3D11DeviceContext* context,std::size_t index)
    {
        if (!PerformanceMonitoringActive()) return;
        auto& slot=gpu_timing_slots_[index];
        context->End(slot.compositor_start.Get());slot.compositor_started=true;
    }

    void EndGpuTiming(ID3D11DeviceContext* context, std::size_t index)
    {
        auto& slot = gpu_timing_slots_[index];
        context->End(slot.end.Get());
        context->End(slot.disjoint.Get());
        slot.ended = true;
    }

    void PollGpuTiming(ID3D11DeviceContext* context)
    {
        for (auto& slot : gpu_timing_slots_) {
            if (!slot.pending || !slot.ended) {
                continue;
            }
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
            const auto disjoint_result = context->GetData(
                slot.disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (disjoint_result == S_FALSE) {
                continue;
            }
            if (FAILED(disjoint_result)) {
                if (PerformanceMonitoringActive()) { const std::scoped_lock lock(g_performance_mutex);++g_performance.invalid_gpu; }
                slot.pending = false;
                slot.ended = false;
                continue;
            }

            UINT64 start{};
            UINT64 end{};
            const auto start_result = context->GetData(
                slot.start.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            const auto end_result = context->GetData(
                slot.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            UINT64 compositor_start{};
            const auto compositor_result=slot.compositor_started ? context->GetData(slot.compositor_start.Get(),
                &compositor_start,sizeof(compositor_start),D3D11_ASYNC_GETDATA_DONOTFLUSH) : S_OK;
            if (FAILED(start_result) || FAILED(end_result) || FAILED(compositor_result)) {
                if (PerformanceMonitoringActive()) { const std::scoped_lock lock(g_performance_mutex);++g_performance.invalid_gpu; }
                slot.pending = false;
                slot.ended = false;
                continue;
            }
            if (start_result == S_FALSE || end_result == S_FALSE || compositor_result == S_FALSE) {
                continue;
            }
            slot.pending = false;
            slot.ended = false;
            if (disjoint.Disjoint || disjoint.Frequency == 0 || end < start) {
                if (PerformanceMonitoringActive()) { const std::scoped_lock lock(g_performance_mutex);++g_performance.invalid_gpu; }
                continue;
            }
            if (PerformanceMonitoringActive()) { const std::scoped_lock lock(g_performance_mutex);
              g_performance.host_gpu.Add(static_cast<double>(end-start)*1000.0/static_cast<double>(disjoint.Frequency));
              if (slot.compositor_started && compositor_start>=start && compositor_start<=end)
                  g_performance.compositor_gpu.Add(static_cast<double>(end-compositor_start)*1000.0/static_cast<double>(disjoint.Frequency)); }
            RecordGpuTiming(static_cast<double>(end - start) * 1000.0 /
                static_cast<double>(disjoint.Frequency));
        }
    }

    void RecordGpuTiming(double milliseconds)
    {
        const auto now = std::chrono::steady_clock::now();
        if (!gpu_timing_window_started_) {
            gpu_timing_window_started_ = true;
            gpu_timing_window_start_ = now;
        }
        ++gpu_timing_sample_count_;
        gpu_timing_total_ms_ += milliseconds;
        gpu_timing_max_ms_ = std::max(gpu_timing_max_ms_, milliseconds);
        const auto elapsed = std::chrono::duration<double>(now - gpu_timing_window_start_).count();
        if (elapsed < 10.0) {
            return;
        }

        SKSE::log::info("SkyUltra browser+overlay GPU timing: {} samples in {:.1f}s, avg/max {:.3f}/{:.3f} ms.",
            gpu_timing_sample_count_, elapsed,
            gpu_timing_total_ms_ / static_cast<double>(gpu_timing_sample_count_), gpu_timing_max_ms_);
        gpu_timing_window_start_ = now;
        gpu_timing_sample_count_ = 0;
        gpu_timing_total_ms_ = 0.0;
        gpu_timing_max_ms_ = 0.0;
    }

    bool EnsureCursorTexture()
    {
        if (cursor_srv_ != nullptr) {
            return true;
        }

        constexpr UINT kCursorWidth = 36;
        constexpr UINT kCursorHeight = 42;
        constexpr std::array<PointF,4> kOutline{PointF{4,3},PointF{26,24},PointF{17,24},PointF{14,36}};
        constexpr std::array<PointF,4> kFill{PointF{6,7},PointF{23,22},PointF{15,21},PointF{14,30}};
        constexpr std::array<PointF,4> kSeal{PointF{21,29},PointF{26,34},PointF{21,39},PointF{16,34}};
        const auto contains = [](float x, float y, const auto& polygon) {
            bool inside{};
            for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
                const auto& a = polygon[i];
                const auto& b = polygon[j];
                const bool crosses = ((a.y > y) != (b.y > y)) &&
                    (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x);
                if (crosses) {
                    inside = !inside;
                }
            }
            return inside;
        };

        std::vector<std::uint32_t> pixels(kCursorWidth * kCursorHeight, 0U);
        for (UINT y = 0; y < kCursorHeight; ++y) {
            for (UINT x = 0; x < kCursorWidth; ++x) {
                unsigned coverage{},shade{};
                // Four-by-four coverage is computed once. The GPU tints this mask;
                // palette changes never recreate textures or rasterize per frame.
                for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx){
                    const auto px=static_cast<float>(x)+(static_cast<float>(sx)+0.5F)/4.0F;
                    const auto py=static_cast<float>(y)+(static_cast<float>(sy)+0.5F)/4.0F;
                    if(contains(px,py,kOutline)||contains(px,py,kSeal)){++coverage;shade+=contains(px,py,kFill)?255U:75U;}
                }
                const auto alpha=coverage*255U/16U;
                const auto grey=shade/16U;
                pixels[y*kCursorWidth+x]=(alpha<<24)|(grey<<16)|(grey<<8)|grey;
            }
        }

        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = kCursorWidth;
        texture_desc.Height = kCursorHeight;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial_data{};
        initial_data.pSysMem = pixels.data();
        initial_data.SysMemPitch = static_cast<UINT>(kCursorWidth * sizeof(std::uint32_t));
        if (FAILED(device_->CreateTexture2D(&texture_desc, &initial_data, cursor_texture_.GetAddressOf())) ||
            FAILED(device_->CreateShaderResourceView(
                cursor_texture_.Get(), nullptr, cursor_srv_.GetAddressOf()))) {
            cursor_texture_.Reset();
            cursor_srv_.Reset();
            return false;
        }
        return true;
    }

    bool EnsureBackBuffer(
        REX::W32::IDXGISwapChain* game_swap_chain,
        ID3D11RenderTargetView* game_render_target,
        ID3D11RenderTargetView* window_render_target)
    {
        auto* swap_chain = reinterpret_cast<IDXGISwapChain*>(game_swap_chain);

        DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
        if (SUCCEEDED(swap_chain->GetDesc(&swap_chain_desc))) {
            const auto match_render_target = [&](ID3D11RenderTargetView* candidate,
                                                 const char* source) {
                if (candidate == nullptr) {
                    return false;
                }
                ComPtr<ID3D11Resource> bound_resource;
                candidate->GetResource(bound_resource.GetAddressOf());
                ComPtr<ID3D11Texture2D> bound_texture;
                if (bound_resource == nullptr || FAILED(bound_resource.As(&bound_texture))) {
                    return false;
                }
                for (UINT index = 0; index < swap_chain_desc.BufferCount; ++index) {
                    ComPtr<ID3D11Texture2D> swap_chain_buffer;
                    if (SUCCEEDED(swap_chain->GetBuffer(
                            index, IID_PPV_ARGS(swap_chain_buffer.GetAddressOf()))) &&
                        swap_chain_buffer.Get() == bound_texture.Get()) {
                        back_buffer_ = std::move(bound_texture);
                        back_buffer_rtv_ = candidate;
                        back_buffer_swap_chain_ = game_swap_chain;
                        D3D11_TEXTURE2D_DESC texture_desc{};
                        back_buffer_->GetDesc(&texture_desc);
                        back_buffer_width_ = texture_desc.Width;
                        back_buffer_height_ = texture_desc.Height;
                        if (!logged_active_back_buffer_match_) {
                            SKSE::log::info(
                                "SkyUltra matched Skyrim's {} render target to swap-chain buffer {} of {}.",
                                source, index, swap_chain_desc.BufferCount);
                            logged_active_back_buffer_match_ = true;
                        }
                        return back_buffer_width_ > 0 && back_buffer_height_ > 0;
                    }
                }
                return false;
            };

            if (match_render_target(game_render_target, "context")) {
                return true;
            }
            if (window_render_target != game_render_target &&
                match_render_target(window_render_target, "renderer window")) {
                return true;
            }
        }

        // The hooked swap-chain wrapper only exposes the base IDXGISwapChain methods.
        // Keep the established base-interface fallback when the game has unbound its RTV.
        ComPtr<ID3D11Texture2D> current_back_buffer;
        if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(current_back_buffer.GetAddressOf())))) {
            return false;
        }
        if (back_buffer_.Get() != current_back_buffer.Get()) {
            back_buffer_ = std::move(current_back_buffer);
            back_buffer_rtv_.Reset();
            if (FAILED(device_->CreateRenderTargetView(
                    back_buffer_.Get(), nullptr, back_buffer_rtv_.GetAddressOf()))) {
                back_buffer_.Reset();
                return false;
            }
            D3D11_TEXTURE2D_DESC texture_desc{};
            back_buffer_->GetDesc(&texture_desc);
            back_buffer_width_ = texture_desc.Width;
            back_buffer_height_ = texture_desc.Height;
        }
        back_buffer_swap_chain_ = game_swap_chain;
        if (!logged_unmatched_back_buffer_) {
            SKSE::log::warn(
                "SkyUltra could not match Skyrim's context or renderer-window target; using base swap-chain buffer 0.");
            logged_unmatched_back_buffer_ = true;
        }
        return back_buffer_rtv_.Get() != nullptr && back_buffer_width_ > 0 && back_buffer_height_ > 0;
    }

    struct SoftwareTexture {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> shader_resource_view;
        SizeU size{};
    };

    bool EnsureViewTexture(SoftwareTexture& view, SizeU size)
    {
        if (size.width == 0 || size.height == 0 || size.width > 4096 || size.height > 4096) {
            return false;
        }
        if (view.texture.Get() != nullptr && view.size.width == size.width && view.size.height == size.height) {
            return true;
        }

        view.shader_resource_view.Reset();
        view.texture.Reset();
        const auto bytes = static_cast<std::size_t>(size.width) * size.height * 4U;
        std::vector<std::uint8_t> transparent(bytes, 0);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = size.width;
        desc.Height = size.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial_data{};
        initial_data.pSysMem = transparent.data();
        initial_data.SysMemPitch = size.width * 4U;
        initial_data.SysMemSlicePitch = static_cast<UINT>(bytes);
        if (FAILED(device_->CreateTexture2D(&desc, &initial_data, view.texture.GetAddressOf())) ||
            FAILED(device_->CreateShaderResourceView(
                view.texture.Get(), nullptr, view.shader_resource_view.GetAddressOf()))) {
            view.texture.Reset();
            view.shader_resource_view.Reset();
            return false;
        }
        view.size = size;
        return true;
    }

    void UploadFrame(const SoftwareTexture& view, const SoftwareFrame& frame)
    {
        for (const auto& patch : frame.patches) {
            const auto right = static_cast<std::uint64_t>(patch.rect.x) + patch.rect.width;
            const auto bottom = static_cast<std::uint64_t>(patch.rect.y) + patch.rect.height;
            const auto required_bytes = static_cast<std::uint64_t>(patch.stride) * patch.rect.height;
            if (patch.rect.width == 0 || patch.rect.height == 0 || right > view.size.width ||
                bottom > view.size.height || patch.stride < patch.rect.width * 4U ||
                required_bytes > patch.bgra8.size()) {
                continue;
            }

            const D3D11_BOX box{
                patch.rect.x,
                patch.rect.y,
                0,
                static_cast<UINT>(right),
                static_cast<UINT>(bottom),
                1,
            };
            context_->UpdateSubresource(view.texture.Get(), 0, &box, patch.bgra8.data(), patch.stride, 0);
        }
    }

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Device> context_state_device_;
    ComPtr<ID3D11DeviceContext> thread_protected_context_;
    ComPtr<ID3D11Multithread> context_multithread_;
    ComPtr<ID3D11DeviceContext> context_state_context_;
    ComPtr<ID3D11DeviceContext1> context1_;
    ComPtr<ID3DDeviceContextState> isolated_context_state_;
    ComPtr<ID3D11Texture2D> back_buffer_;
    ComPtr<ID3D11RenderTargetView> back_buffer_rtv_;
    REX::W32::IDXGISwapChain* back_buffer_swap_chain_{};
    ComPtr<ID3D11BlendState> blend_state_;
    ComPtr<ID3D11Texture2D> cursor_texture_;
    ComPtr<ID3D11ShaderResourceView> cursor_srv_;
    ComPtr<ID3D11ShaderResourceView> splash_pixel_;
    bool splash_attempted_{};
    struct RetainedViewFrame {
        ComPtr<ID3D11ShaderResourceView> srv;
        RECT source{};
    };
    std::array<GpuTimestampSlot, 8> gpu_timing_slots_{};
    std::unordered_map<ViewId, SoftwareTexture> software_textures_;
    std::unordered_map<std::string, RetainedViewFrame> retained_config_frames_;
    std::unique_ptr<DirectX::SpriteBatch> sprite_batch_;
    std::mutex draw_mutex_;
    std::size_t next_gpu_timing_slot_{};
    std::uint64_t gpu_timing_sample_count_{};
    double gpu_timing_total_ms_{};
    double gpu_timing_max_ms_{};
    std::chrono::steady_clock::time_point gpu_timing_window_start_{};
    bool gpu_timing_enabled_{};
    bool gpu_timing_window_started_{};
    bool logged_gpu_timing_unavailable_{};
    bool logged_missing_device_{};
    bool logged_missing_context_{};
    bool logged_context_state_fallback_{};
    bool logged_context_state_ready_{};
    bool logged_context_thread_protection_{};
    bool logged_missing_view_{};
    bool logged_device_setup_failure_{};
    bool logged_back_buffer_failure_{};
    bool logged_software_texture_failure_{};
    bool logged_cursor_texture_failure_{};
    bool logged_first_cursor_draw_{};
    bool logged_missing_frame_{};
    bool logged_missing_texture_{};
    bool logged_first_ui_composite_{};
    bool logged_first_present_composite_{};
    bool logged_missing_ui_target_{};
    bool logged_active_back_buffer_match_{};
    bool logged_unmatched_back_buffer_{};
    std::uint32_t back_buffer_width_{};
    std::uint32_t back_buffer_height_{};
#if defined(ENABLE_SKYRIM_VR)
    OpenVrOverlayManager vr_overlays_;
#endif
};

OverlayRenderer g_renderer;

void DrawUiPassMenuFrame() noexcept
{
    try {
        (void)InstallD3D11PresentHook();
        (void)g_renderer.DrawUiPass();
    } catch (const std::exception& exception) {
        SKSE::log::error("SkyUltra UI-pass composite failed: {}", exception.what());
    } catch (...) {
        SKSE::log::error("SkyUltra UI-pass composite failed with an unknown exception.");
    }
}

HRESULT STDMETHODCALLTYPE ResizeBuffersHook(
    REX::W32::IDXGISwapChain* swap_chain, UINT buffer_count, UINT width, UINT height,
    DXGI_FORMAT format, UINT swap_chain_flags) noexcept
{
    ResizeBuffersFunction original_resize_buffers{};
    {
        const std::scoped_lock hook_lock(g_present_hook_mutex);
        if (g_hooked_swap_chain == swap_chain) {
            original_resize_buffers = g_original_resize_buffers;
        }
    }
    if (original_resize_buffers == nullptr) {
        return DXGI_ERROR_INVALID_CALL;
    }

    g_renderer.ReleaseBackBufferForResize(swap_chain);
    return original_resize_buffers(
        swap_chain, buffer_count, width, height, format, swap_chain_flags);
}

HRESULT STDMETHODCALLTYPE PresentHook(
    REX::W32::IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) noexcept
{
    PresentFunction original_present{};
    {
        const std::scoped_lock hook_lock(g_present_hook_mutex);
        if (g_hooked_swap_chain == swap_chain) {
            original_present = g_original_present;
        }
    }
    if (original_present == nullptr) {
        return DXGI_ERROR_INVALID_CALL;
    }

    if ((flags & DXGI_PRESENT_TEST) == 0) {
        try {
            g_renderer.DrawPresent(swap_chain);
        } catch (const std::exception& exception) {
            SKSE::log::error("SkyUltra D3D11 overlay failed: {}", exception.what());
            g_visible.store(false, std::memory_order_release);
        } catch (...) {
            SKSE::log::error("SkyUltra D3D11 overlay failed with an unknown exception.");
            g_visible.store(false, std::memory_order_release);
        }
    }
    const auto result = original_present(swap_chain, sync_interval, flags);
    auto* render_window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
    if (render_window != nullptr && render_window->swapChain != swap_chain) {
        (void)InstallD3D11PresentHook();
    }
    return result;
}

}  // namespace

void NoteVRControllerTrigger(int hand, bool down) noexcept
{
    if (hand >= 0 && hand < static_cast<int>(g_vr_trigger_fallback.size())) {
        g_vr_trigger_fallback[static_cast<std::size_t>(hand)].store(down, std::memory_order_release);
    }
}

void NoteVRControllerKeyboardButton(int hand, bool down) noexcept
{
    if (hand >= 0 && hand < static_cast<int>(g_vr_keyboard_button_fallback.size())) {
        g_vr_keyboard_button_fallback[static_cast<std::size_t>(hand)].store(down, std::memory_order_release);
    }
}

void NoteVRControllerThumbstick(int hand, float y) noexcept
{
    if (hand >= 0 && hand < static_cast<int>(g_vr_stick_y_fallback.size())) {
        const auto index = static_cast<std::size_t>(hand);
        const auto bounded = std::clamp(y, -1.0F, 1.0F);
        g_vr_stick_y_fallback[index].store(static_cast<int>(std::lround(bounded * 1000.0F)),
            std::memory_order_release);
        g_vr_stick_tick[index].store(GetTickCount64(), std::memory_order_release);
    }
}

void SetOverlayViews(std::vector<OverlayView> views) noexcept
{
    const std::scoped_lock lock(g_overlay_views_mutex);
    g_overlay_views = std::move(views);
}

OverlayPerformanceSnapshot GetOverlayPerformanceSnapshot()
{
    const auto now_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto previous_lease=g_performance_lease_until_ms.exchange(now_ms+5000,std::memory_order_relaxed);
    PerformanceCounters counters;
    { const std::scoped_lock lock(g_performance_mutex);
      if (previous_lease<=now_ms) {
          g_performance.previous_present={};g_performance.host_gpu.ClearWindow();g_performance.compositor_gpu.ClearWindow();
          g_performance.callback_cpu.ClearWindow();g_performance.present_interval.ClearWindow();
      }
      counters=g_performance; }
    const auto now=std::chrono::steady_clock::now();
    OverlayPerformanceSnapshot result;
    result.generation=counters.generation;
    result.captured_at_ms=std::chrono::duration<double,std::milli>(now.time_since_epoch()).count();
    result.gpu_timing_available=counters.gpu_available;
    result.skipped_gpu_frames=counters.skipped_gpu;result.invalid_gpu_samples=counters.invalid_gpu;
    result.host_gpu=SummarizeTiming(counters.host_gpu,now);
    result.compositor_gpu=SummarizeTiming(counters.compositor_gpu,now);
    result.callback_cpu=SummarizeTiming(counters.callback_cpu,now);
    result.present_interval=SummarizeTiming(counters.present_interval,now);
    return result;
}

void RunAfterOverlaySplash(std::string view_key, std::function<void()> task)
{
    const std::scoped_lock lock(g_splash_tasks_mutex);
    g_splash_tasks.emplace(std::move(view_key),std::move(task));
}

void CancelOverlaySplash(std::string_view view_key)
{
    const std::scoped_lock lock(g_splash_tasks_mutex);
    g_splash_tasks.erase(std::string(view_key));
}

void CancelOverlayPageLoad(ViewId view)
{
    const std::scoped_lock lock(g_page_load_mutex);
    g_page_loads.erase(view);
    g_painted_views.erase(view);
}

bool OverlayViewHasPainted(ViewId view) {const std::scoped_lock lock(g_page_load_mutex);return g_painted_views.contains(view);}

bool OverlayPageAwaitingReady(ViewId view) { return PageAwaitingReady(view); }

void BeginOverlayPageLoad(ViewId view)
{
    g_clear_retained_frames.store(true, std::memory_order_release);
    const std::scoped_lock lock(g_page_load_mutex);
    g_page_loads[view] = {};
}

void CompleteOverlayPageLoad(ViewId view)
{
    const std::scoped_lock lock(g_page_load_mutex);
    if (const auto found = g_page_loads.find(view); found != g_page_loads.end() && found->second.time_since_epoch().count()!=0) {
        const auto elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-found->second).count();
        SKSE::log::info("SkyUltra page {} signalled interface readiness after {:.3f} ms.", view, elapsed);
    }
    g_page_loads.erase(view);
}

void SetOverlayRuntime(IWebRuntime* runtime) noexcept
{
    g_runtime.store(runtime, std::memory_order_release);
}

void SetOverlayVisible(bool visible) noexcept
{
    g_visible.store(visible, std::memory_order_release);
    if (!visible) {
        CancelFrameworkSplash();
        g_clear_retained_frames.store(true, std::memory_order_release);
        g_cursor_visible.store(false, std::memory_order_release);
        g_ui_pass_composited.store(false, std::memory_order_release);
#if defined(ENABLE_SKYRIM_VR)
        g_vr_keyboard_focus_id.store(0, std::memory_order_release);
        const std::scoped_lock lock(g_overlay_views_mutex);
        for (const auto& overlay_view : g_overlay_views) {
            if (overlay_view.view != nullptr) {
                try {
                    overlay_view.view->SetFocused(InputMode::None);
                } catch (...) {
                }
            }
        }
#endif
    }
}

void SetOverlayCursorAccent(std::uint32_t rgb) noexcept {g_cursor_accent.store(rgb & 0xFFFFFFU,std::memory_order_relaxed);}

void SetOverlaySpatialConfig(std::string_view mode, float distance, float scale, bool head_tracking) noexcept
{
    int mode_id = 0;
    if (mode == "worldLocked") mode_id = 1;
    else if (mode == "leftWrist") mode_id = 2;
    else if (mode == "rightWrist") mode_id = 3;
    g_vr_spatial_mode_id.store(mode_id, std::memory_order_relaxed);
    g_vr_spatial_distance.store(std::clamp(distance, 0.35F, 6.0F), std::memory_order_relaxed);
    g_vr_spatial_scale.store(std::clamp(scale, 0.4F, 2.5F), std::memory_order_relaxed);
    g_vr_head_tracking.store(head_tracking, std::memory_order_relaxed);
}

void SetOverlayCursor(PointF position, SizeU viewport, bool visible) noexcept
{
    if (viewport.width == 0 || viewport.height == 0) {
        g_cursor_visible.store(false, std::memory_order_release);
        return;
    }
    g_cursor_x_normalized.store(std::clamp(position.x / static_cast<float>(viewport.width), 0.0F, 1.0F),
        std::memory_order_relaxed);
    g_cursor_y_normalized.store(std::clamp(position.y / static_cast<float>(viewport.height), 0.0F, 1.0F),
        std::memory_order_relaxed);
    g_cursor_visible.store(visible, std::memory_order_release);
}

bool IsOverlayVisible() noexcept
{
    return g_visible.load(std::memory_order_acquire);
}

ViewId GetVRKeyboardFocus() noexcept
{
    return g_vr_keyboard_focus_id.load(std::memory_order_acquire);
}

bool RegisterD3D11UiPassMenu() noexcept
{
#if defined(ENABLE_SKYRIM_VR)
    if (REL::Module::IsVR()) {
        return false;
    }
#endif
    if (g_ui_menu_registered) {
        return true;
    }
    auto* ui = RE::UI::GetSingleton();
    if (ui == nullptr) {
        return false;
    }

    ui->Register(kUiPassMenuName, SkyUltraOverlayMenu::Create);
    g_ui_menu_registered = true;
    SKSE::log::info("SkyUltra registered its Skyrim UI-pass carrier menu.");
    return true;
}

void SetD3D11UiPassMenuVisible(bool visible) noexcept
{
#if defined(ENABLE_SKYRIM_VR)
    if (REL::Module::IsVR()) {
        return;
    }
#endif
    // Calls come from the game-task publisher, as required by UIMessageQueue.
    // Track requests rather than IsMenuOpen alone: show/hide/show can all be
    // queued before the UI processes any of them and must keep that order.
    const auto request=visible?UiPassMenuRequest::Show:UiPassMenuRequest::Hide;
    const auto previous=g_ui_menu_request.load(std::memory_order_acquire);
    if(previous==request)return;
    if(!visible && previous==UiPassMenuRequest::Unknown){
        auto* ui=RE::UI::GetSingleton();
        if(ui==nullptr || !ui->IsMenuOpen(kUiPassMenuName)){
            g_ui_menu_request.store(request,std::memory_order_release);
            return;
        }
    }
    if (visible && !RegisterD3D11UiPassMenu()) {
        SKSE::log::warn("SkyUltra UI-pass carrier menu is unavailable; the Present fallback will be used.");
        return;
    }
    auto* queue = RE::UIMessageQueue::GetSingleton();
    if (queue == nullptr) {
        if (!g_logged_missing_ui_message_queue) {
            SKSE::log::warn("SkyUltra could not reach Skyrim's UI message queue for its carrier menu.");
            g_logged_missing_ui_message_queue = true;
        }
        return;
    }

    static const RE::BSFixedString menu_name(kUiPassMenuName.data());
    g_ui_menu_request.store(request,std::memory_order_release);
    queue->AddMessage(menu_name,visible?RE::UI_MESSAGE_TYPE::kShow:RE::UI_MESSAGE_TYPE::kForceHide,nullptr);
}

bool InstallD3D11PresentHook() noexcept
{
    auto* render_window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
    if (render_window == nullptr || render_window->swapChain == nullptr) {
        return false;
    }
    auto* swap_chain = render_window->swapChain;
    const std::scoped_lock hook_lock(g_present_hook_mutex);
    if (g_hooked_swap_chain == swap_chain) {
        auto** object_vtable = reinterpret_cast<void***>(swap_chain);
        if (*object_vtable == g_hooked_vtable.data()) {
            return true;
        }
        if (!g_logged_present_hook_superseded) {
            SKSE::log::warn("SkyUltra's Present hook was superseded by another hook and cannot be reinstalled safely.");
            g_logged_present_hook_superseded = true;
        }
        return false;
    }
    const bool rebinding = g_hooked_swap_chain != nullptr;
    if (g_hooked_swap_chain != nullptr) {
        auto** old_object_vtable = reinterpret_cast<void***>(g_hooked_swap_chain);
        if (*old_object_vtable != g_hooked_vtable.data()) {
            if (!g_logged_present_hook_superseded) {
                SKSE::log::warn("SkyUltra's previous swap-chain hook was replaced by another hook; "
                                "it cannot safely rebind to the current swap chain.");
                g_logged_present_hook_superseded = true;
            }
            return false;
        }
        g_renderer.ReleaseBackBufferForResize(g_hooked_swap_chain);
        DWORD old_protection{};
        if (!VirtualProtect(g_hooked_swap_chain, sizeof(void*), PAGE_READWRITE, &old_protection)) {
            SKSE::log::error("SkyUltra could not restore the previous swap-chain vtable during rebind.");
            return false;
        }
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(old_object_vtable), g_original_vtable);
        DWORD ignored{};
        VirtualProtect(g_hooked_swap_chain, sizeof(void*), old_protection, &ignored);
        g_hooked_swap_chain_ref.Reset();
        g_hooked_swap_chain = nullptr;
        g_original_vtable = nullptr;
        g_original_present = nullptr;
        g_original_resize_buffers = nullptr;
    }

    auto* retained_swap_chain = reinterpret_cast<IDXGISwapChain*>(swap_chain);
    retained_swap_chain->AddRef();
    auto** object_vtable = reinterpret_cast<void***>(swap_chain);
    auto* original_vtable = *object_vtable;
    if (original_vtable == nullptr || original_vtable[kPresentVtableIndex] == nullptr ||
        original_vtable[kResizeBuffersVtableIndex] == nullptr) {
        retained_swap_chain->Release();
        return false;
    }
    std::copy_n(original_vtable, kSwapChainVtableEntries, g_hooked_vtable.begin());
    g_original_present = reinterpret_cast<PresentFunction>(original_vtable[kPresentVtableIndex]);
    g_original_resize_buffers =
        reinterpret_cast<ResizeBuffersFunction>(original_vtable[kResizeBuffersVtableIndex]);
    g_hooked_vtable[kPresentVtableIndex] = reinterpret_cast<void*>(&PresentHook);
    g_hooked_vtable[kResizeBuffersVtableIndex] = reinterpret_cast<void*>(&ResizeBuffersHook);

    DWORD old_protection{};
    if (!VirtualProtect(swap_chain, sizeof(void*), PAGE_READWRITE, &old_protection)) {
        g_original_present = nullptr;
        g_original_resize_buffers = nullptr;
        retained_swap_chain->Release();
        return false;
    }
    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(object_vtable), g_hooked_vtable.data());
    DWORD ignored{};
    VirtualProtect(swap_chain, sizeof(void*), old_protection, &ignored);

    g_original_vtable = original_vtable;
    g_hooked_swap_chain = swap_chain;
    g_hooked_swap_chain_ref.Attach(retained_swap_chain);
    g_logged_present_hook_superseded = false;
    if (rebinding) {
        SKSE::log::info("SkyUltra rebound its Present and ResizeBuffers hooks to Skyrim's replacement swap chain.");
    }
    return true;
}

void ShutdownD3D11PresentHook() noexcept
{
    ShutdownFrameworkSplash();
    g_performance_lease_until_ms.store(0,std::memory_order_relaxed);
    ResetPerformance(false);
    ClearActorPreviews();
    { const std::scoped_lock lock(g_page_load_mutex); g_page_loads.clear(); }
    { const std::scoped_lock lock(g_splash_tasks_mutex); g_splash_tasks.clear(); }
    g_renderer.ShutdownVR();
    const std::scoped_lock hook_lock(g_present_hook_mutex);
    if (g_hooked_swap_chain != nullptr && g_original_vtable != nullptr) {
        auto** object_vtable = reinterpret_cast<void***>(g_hooked_swap_chain);
        if (*object_vtable == g_hooked_vtable.data()) {
            DWORD old_protection{};
            if (!VirtualProtect(g_hooked_swap_chain, sizeof(void*), PAGE_READWRITE, &old_protection)) {
                SKSE::log::error("SkyUltra could not restore the swap-chain vtable during shutdown.");
                return;
            }
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(object_vtable), g_original_vtable);
            DWORD ignored{};
            VirtualProtect(g_hooked_swap_chain, sizeof(void*), old_protection, &ignored);
        } else {
            SKSE::log::warn("SkyUltra's swap-chain hook was replaced before shutdown; "
                            "the hook reference was retained.");
            return;
        }
    }
    g_hooked_swap_chain_ref.Reset();
    g_original_present = nullptr;
    g_original_resize_buffers = nullptr;
    g_original_vtable = nullptr;
    g_hooked_swap_chain = nullptr;
}

}  // namespace skyultra
