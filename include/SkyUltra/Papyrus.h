#pragma once

#include "SkyUltra/NativeAPI.h"

#include <cstdint>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace RE::BSScript {
class IVirtualMachine;
}

namespace skyultra {

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);
// Snapshot services shared by the host bridge and Papyrus. Game-thread only.
[[nodiscard]] std::string ReadGameDataJSON(std::string_view method, std::string_view params_json);
void CaptureActiveBarterVendor() noexcept;
void CaptureActiveContainerRef() noexcept;
[[nodiscard]] bool IsActiveContainerMannequin() noexcept;
[[nodiscard]] bool IsActiveContainerCorpse() noexcept;
[[nodiscard]] bool IsActiveContainerPickpocket() noexcept;
void InvalidateSharedGameDataCache() noexcept;
[[nodiscard]] std::string CollectSharedGameDataUpdateJSON(
    bool compact,
    const std::vector<std::string>& subscribed_topics,
    std::unordered_map<std::string, std::string>& in_out_topic_signatures,
    std::uint64_t& out_sequence,
    bool force_full = false);
[[nodiscard]] bool PapyrusApiReady() noexcept;

[[nodiscard]] bool IsPapyrusView(SkyUltraViewHandle view) noexcept;

// Called on Skyrim's game thread for requests owned by Papyrus-created views.
// Returns false if Papyrus could not accept a new request event.
[[nodiscard]] bool DispatchPapyrusPageRequest(
    SkyUltraViewHandle view,
    std::string_view request_id,
    std::string_view method,
    std::string_view params_json,
    bool cancelled);

// Releases Papyrus-side pending requests when another native path destroys a view.
void ForgetPapyrusView(SkyUltraViewHandle view);

}  // namespace skyultra
