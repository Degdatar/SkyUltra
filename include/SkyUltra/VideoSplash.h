#pragma once
#include <string>
#include <string_view>
#include <cstdint>

// Keep Windows/D3D macros out of consumers that have not included CommonLib yet.
// The compositor includes SDK headers after CommonLib; the video implementation
// is compiled independently and never includes CommonLib.
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace Microsoft::WRL {
template <typename T> class ComPtr;
}

namespace skyultra {
struct VideoSplashSettings {
    bool enabled{true}; bool full_duration{true}; bool audio{true};
    float volume{0.8F};
    std::string replay{"session"};
};
void ConfigureVideoSplash(VideoSplashSettings settings);
// Explicit previews bypass the replay schedule and play the full splash screen.
// This does not change the saved playback setting for normal menu openings.
void BeginFrameworkSplash(std::string view_key, std::string mod_id, bool page_ready = false, bool force_replay = false);
struct VideoSplashStatus {
    bool active{}, has_frame{}, failed{};
    std::uint32_t error_code{}, media_error{};
    std::string stage{"Not started"};
};
[[nodiscard]] VideoSplashStatus GetFrameworkSplashStatus();
void CompleteFrameworkSplash(std::string_view view_key);
void CancelFrameworkSplash(std::string_view view_key = {});
// Render-thread only. Host immediate-context protection is unchanged.
// Prefers a separate media device and zero-timeout keyed mutexes. If GPU sharing
// fails before the first frame, reopens the same MP4 in WIC mode with native audio
// and uploads its software frames only while the splash is active.
void WarmupFrameworkSplash(ID3D11Device* host, ID3D11DeviceContext* context);
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TickFrameworkSplash(ID3D11Device* host, ID3D11DeviceContext* context);
bool FrameworkSplashActive();
void ShutdownFrameworkSplash();
}
