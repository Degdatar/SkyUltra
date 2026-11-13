#pragma once

#include "SkyUltra/BrowserBackend.h"

#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace skyultra {

struct OverlayView {
    std::shared_ptr<IWebView> view;
    ViewConfig config;
};

struct OverlayTimingMetric {
    std::uint64_t samples{};
    double total_ms{};
    double last_ms{};
    double mean_ms{};
    double p95_ms{};
    double max_ms{};
    double age_ms{-1};
    std::uint32_t window_samples{};
};

struct OverlayPerformanceSnapshot {
    std::uint64_t generation{};
    double captured_at_ms{};
    bool gpu_timing_available{};
    std::uint64_t skipped_gpu_frames{};
    std::uint64_t invalid_gpu_samples{};
    OverlayTimingMetric host_gpu;
    OverlayTimingMetric compositor_gpu;
    OverlayTimingMetric callback_cpu;
    OverlayTimingMetric present_interval;
};

// Copies completed measurements only; never reads a query or waits on the GPU.
[[nodiscard]] OverlayPerformanceSnapshot GetOverlayPerformanceSnapshot();

void SetOverlayViews(std::vector<OverlayView> views) noexcept;
void SetOverlayRuntime(IWebRuntime* runtime) noexcept;
void SetOverlayVisible(bool visible) noexcept;
// Opt-in page readiness keeps the GPU logo visible until ui.pageReady arrives.
void BeginOverlayPageLoad(ViewId view);
[[nodiscard]] bool OverlayPageAwaitingReady(ViewId view);
[[nodiscard]] bool OverlayViewHasPainted(ViewId view);
void CompleteOverlayPageLoad(ViewId view);
void CancelOverlayPageLoad(ViewId view);
void CancelOverlaySplash(std::string_view view_key);
// Starts host work on the game task queue after its pending logo was drawn.
void RunAfterOverlaySplash(std::string view_key, std::function<void()> task);
void SetOverlayCursorAccent(std::uint32_t rgb) noexcept;
void SetOverlayCursor(PointF position, SizeU viewport, bool visible) noexcept;
void SetOverlaySpatialConfig(std::string_view mode, float distance, float scale, bool head_tracking) noexcept;
[[nodiscard]] bool IsOverlayVisible() noexcept;
[[nodiscard]] ViewId GetVRKeyboardFocus() noexcept;
// Skyrim input-sink levels are a fallback when the VR runtime cannot poll a controller.
void NoteVRControllerTrigger(int hand, bool down) noexcept;
void NoteVRControllerKeyboardButton(int hand, bool down) noexcept;
void NoteVRControllerThumbstick(int hand, float y) noexcept;

[[nodiscard]] bool RegisterD3D11UiPassMenu() noexcept;
// Visible panels and HUDs use Skyrim's UI target; Present is the fallback.
void SetD3D11UiPassMenuVisible(bool visible) noexcept;

[[nodiscard]] bool InstallD3D11PresentHook() noexcept;
void ShutdownD3D11PresentHook() noexcept;

}  // namespace skyultra
