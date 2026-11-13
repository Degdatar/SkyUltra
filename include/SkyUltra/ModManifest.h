#pragma once

#include "SkyUltra/Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace skyultra {

struct ModManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string folder_name;
    std::string papyrus_plugin;
    // Optional per-mod backend pin from manifest.json. When absent, views use
    // the host's per-layer setting or an explicit per-view API selection.
    std::optional<BrowserEngine> browser_engine;
    std::filesystem::path root_directory;
    std::filesystem::path entry_file;
    bool storage_enabled{};
    bool asset_access_enabled{ true };
    std::vector<std::string> network_hosts;
    std::unordered_set<std::string> page_request_methods;
};

[[nodiscard]] bool IsValidModIdentifier(std::string_view id) noexcept;
[[nodiscard]] bool IsModNetworkHostAllowed(
    const ModContentPolicy& policy, std::string_view scheme, std::string_view host) noexcept;

[[nodiscard]] std::filesystem::path GetInstalledWebModsDirectory();

[[nodiscard]] std::optional<ModManifest> LoadModManifest(
    const std::filesystem::path& mods_directory,
    std::string_view id,
    std::string& error);

[[nodiscard]] std::optional<ModManifest> FindModManifestForPapyrusPlugin(
    const std::filesystem::path& mods_directory,
    std::string_view plugin_name,
    std::string& error);

}  // namespace skyultra
