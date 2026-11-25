#pragma once

#include "SkyUltra/NativeAPI.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace RE {
class MapMarkerData;
namespace BSScript {
class IVirtualMachine;
}
}

namespace skyultra {

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);
// Snapshot services shared by the host bridge and Papyrus. Game-thread only.
[[nodiscard]] std::string ReadGameDataJSON(std::string_view method, std::string_view params_json);
void CaptureActiveBarterVendor() noexcept;
void CaptureActiveContainerRef() noexcept;
[[nodiscard]] bool CaptureActiveBlacksmithStation() noexcept;
void ClearActiveBlacksmithStation() noexcept;
[[nodiscard]] bool CaptureActiveEnchantingStation() noexcept;
void ClearActiveEnchantingStation() noexcept;
[[nodiscard]] bool CaptureActiveAlchemyStation() noexcept;
void ClearActiveAlchemyStation() noexcept;
void ReleaseVirtualCraftingProxyNow() noexcept;
void SetSuperEnchantingEnabled(bool enabled) noexcept;
[[nodiscard]] bool IsSuperEnchantingEnabled() noexcept;
void ApplySavedEnchantmentCustomNames(const std::unordered_map<std::uint32_t, std::string>& names);
[[nodiscard]] std::unordered_map<std::uint32_t, std::string> GetSavedEnchantmentCustomNames();
[[nodiscard]] float ReadPlayerEnchantingSkillLevel() noexcept;
[[nodiscard]] bool CaptureActiveBookMenu();
void ClearActiveBookMenu() noexcept;
[[nodiscard]] bool HasActiveBookMenu() noexcept;
[[nodiscard]] bool OpenCapturedBookNativeMenu() noexcept;
void NotifyCompassLocationDiscovered(const RE::MapMarkerData* map_marker_data) noexcept;
void ResetCompassDiscoveryState() noexcept;
[[nodiscard]] bool IsActiveContainerMannequin() noexcept;
[[nodiscard]] bool IsActiveContainerCorpse() noexcept;
[[nodiscard]] bool IsActiveContainerPickpocket() noexcept;
void InvalidateSharedGameDataCache() noexcept;
void InvalidateSharedLightGameDataCache(bool invalidate_effects = false) noexcept;
void LogWearableLightDiagnostic(std::string_view reason) noexcept;
[[nodiscard]] std::optional<float> ReadPlayerCompassHeadingFast() noexcept;
[[nodiscard]] std::optional<double> ReadPlayerCompassHeadingPrecise() noexcept;
void SetCompassMarkerRadiusMeters(float radius_meters) noexcept;
[[nodiscard]] float GetCompassMarkerRadiusMeters() noexcept;
[[nodiscard]] std::string CollectSharedGameDataUpdateJSON(
    bool compact,
    const std::vector<std::string>& subscribed_topics,
    std::unordered_map<std::string, std::string>& in_out_topic_signatures,
    std::uint64_t& out_sequence,
    bool force_full = false,
    bool include_heading_only_updates = true);
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
