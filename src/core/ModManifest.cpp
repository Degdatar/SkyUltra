#include "SkyUltra/ModManifest.h"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

namespace skyultra {
namespace {

constexpr std::uintmax_t kMaximumManifestBytes = 64U * 1024U;
constexpr std::size_t kMaximumManifestNameBytes = 128;
constexpr std::size_t kMaximumManifestVersionBytes = 64;
constexpr std::size_t kMaximumEntryPathBytes = 512;
constexpr std::uint32_t kMaximumPageRequestMethods = 128;
constexpr std::size_t kMaximumPageRequestMethodBytes = 128;
constexpr std::uint32_t kMaximumNetworkHosts = 128;
constexpr std::size_t kMaximumNetworkHostBytes = 253;
constexpr std::size_t kMaximumPluginNameBytes = 260;

bool IsPathWithin(const std::filesystem::path& root, const std::filesystem::path& path)
{
    auto root_part = root.begin();
    auto path_part = path.begin();
    for (; root_part != root.end(); ++root_part, ++path_part) {
        if (path_part == path.end()) {
            return false;
        }
        const auto& root_text = root_part->native();
        const auto& path_text = path_part->native();
        if (CompareStringOrdinal(
                root_text.c_str(), static_cast<int>(root_text.size()),
                path_text.c_str(), static_cast<int>(path_text.size()), TRUE) != CSTR_EQUAL) {
            return false;
        }
    }
    return path_part != path.end();
}

bool HasReparsePointOnRelativeChain(
    const std::filesystem::path& root_dir,
    const std::filesystem::path& relative_path)
{
    std::filesystem::path current = root_dir;
    for (const auto& part : relative_path) {
        if (part.empty() || part == L".") {
            continue;
        }
        current /= part;
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return true;
        }
    }
    return false;
}

bool IsValidPageRequestMethod(std::string_view name) noexcept
{
    if (name.empty() || name.size() > kMaximumPageRequestMethodBytes) {
        return false;
    }
    for (const auto value : name) {
        const bool valid = (value >= 'a' && value <= 'z') ||
            (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
            value == '.' || value == '_' || value == ':' || value == '-';
        if (!valid) {
            return false;
        }
    }
    return true;
}

bool NormalizeNetworkHost(std::string_view source, std::string& normalized)
{
    if (source.empty() || source.size() > kMaximumNetworkHostBytes) {
        return false;
    }
    normalized.clear();
    normalized.reserve(source.size());
    std::size_t label_length{};
    bool label_starts_with_hyphen{};
    char previous{};
    for (const char value : source) {
        const auto lower = value >= 'A' && value <= 'Z' ?
            static_cast<char>(value - 'A' + 'a') : value;
        if (lower == '.') {
            if (label_length == 0 || label_length > 63 || label_starts_with_hyphen ||
                previous == '-') {
                return false;
            }
            label_length = 0;
            label_starts_with_hyphen = false;
        } else if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9') ||
                   lower == '-') {
            if (label_length == 0) {
                label_starts_with_hyphen = lower == '-';
            }
            ++label_length;
        } else {
            return false;
        }
        normalized.push_back(lower);
        previous = lower;
    }
    return label_length != 0 && label_length <= 63 && !label_starts_with_hyphen &&
        previous != '-';
}

bool EqualsAsciiCaseInsensitive(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        auto character = left[index];
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
        auto expected = right[index];
        if (expected >= 'A' && expected <= 'Z') {
            expected = static_cast<char>(expected - 'A' + 'a');
        }
        if (character != expected) {
            return false;
        }
    }
    return true;
}

bool IsValidPapyrusPluginName(std::string_view name) noexcept
{
    if (name.empty() || name.size() > kMaximumPluginNameBytes ||
        name == "." || name == "..") {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char value) {
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '_' || value == '-' ||
            value == '.';
    });
}

bool HasOnlyKeys(const nlohmann::json& value, std::initializer_list<std::string_view> allowed)
{
    if (!value.is_object()) {
        return false;
    }
    for (auto it = value.begin(); it != value.end(); ++it) {
        bool known{};
        for (const auto key : allowed) {
            if (it.key() == key) {
                known = true;
                break;
            }
        }
        if (!known) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool IsValidModIdentifier(std::string_view id) noexcept
{
    if (id.empty() || id.size() > 64 ||
        !((id.front() >= 'a' && id.front() <= 'z') ||
          (id.front() >= '0' && id.front() <= '9'))) {
        return false;
    }
    for (const auto value : id) {
        if (!((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
              value == '.' || value == '_' || value == '-')) {
            return false;
        }
    }
    return true;
}

bool IsModNetworkHostAllowed(
    const ModContentPolicy& policy, std::string_view scheme, std::string_view host) noexcept
{
    const auto equals_ascii_case_insensitive = [](std::string_view lhs,
        std::string_view rhs) noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (std::size_t index = 0; index < lhs.size(); ++index) {
            auto character = lhs[index];
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
            if (character != rhs[index]) {
                return false;
            }
        }
        return true;
    };
    if ((!equals_ascii_case_insensitive(scheme, "http") &&
         !equals_ascii_case_insensitive(scheme, "https")) || host.empty()) {
        return false;
    }
    for (const auto& allowed_host : policy.network_hosts) {
        if (equals_ascii_case_insensitive(host, allowed_host)) {
            return true;
        }
    }
    return false;
}

std::filesystem::path GetInstalledWebModsDirectory()
{
    std::array<wchar_t, 32768> executable_path{};
    const auto length = GetModuleFileNameW(nullptr, executable_path.data(),
        static_cast<DWORD>(executable_path.size()));
    if (length == 0 || length >= executable_path.size()) {
        return {};
    }

    const auto game_directory = std::filesystem::path(executable_path.data()).parent_path();
    return game_directory.empty() ? std::filesystem::path{} :
        game_directory / L"Data" / L"SkyUltra";
}

std::optional<ModManifest> LoadModManifest(
    const std::filesystem::path& mods_directory,
    std::string_view id,
    std::string& error) try
{
    if (!IsValidModIdentifier(id)) {
        error = "Mod IDs must be 1..64 lowercase letters, digits, dots, underscores, or hyphens, and begin with a letter or digit.";
        return std::nullopt;
    }

    std::error_code filesystem_error;
    const auto absolute_mods_directory =
        std::filesystem::absolute(mods_directory, filesystem_error).lexically_normal();
    if (filesystem_error) {
        error = "The SkyUltra Mods directory could not be resolved: " + filesystem_error.message();
        return std::nullopt;
    }
    // `id` is validated above and cannot contain separators or traversal
    // components, so this is always one direct child. Avoid canonicalizing the
    // MO2 virtual parent: merged directories can resolve to different physical
    // mod folders and no longer appear parent/child after canonicalization.
    const auto requested_root = absolute_mods_directory /
        std::filesystem::u8path(std::string(id));
    const auto root_attributes = GetFileAttributesW(requested_root.c_str());
    if (root_attributes == INVALID_FILE_ATTRIBUTES) {
        const auto windows_error = GetLastError();
        error = "The mod directory is not visible at '" + requested_root.string() +
            "' (Win32 error " + std::to_string(windows_error) + ").";
        return std::nullopt;
    }
    if ((root_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        error = "The mod path is not a directory: '" + requested_root.string() + "'.";
        return std::nullopt;
    }
    if ((root_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = "The mod directory cannot be a reparse point: '" + requested_root.string() + "'.";
        return std::nullopt;
    }

    if (HasReparsePointOnRelativeChain(requested_root, L"manifest.json")) {
        error = "The mod manifest is missing or resolves outside the mod directory.";
        return std::nullopt;
    }

    auto root = std::filesystem::canonical(requested_root, filesystem_error);
    if (filesystem_error || !std::filesystem::is_directory(root, filesystem_error) || filesystem_error) {
        filesystem_error.clear();
        root = requested_root.lexically_normal();
        if (!std::filesystem::is_directory(root, filesystem_error) || filesystem_error) {
            error = "The resolved mod path is not a readable directory: '" + requested_root.string() + "'.";
            return std::nullopt;
        }
    }

    auto manifest_path = std::filesystem::canonical(requested_root / L"manifest.json", filesystem_error);
    if (filesystem_error || !IsPathWithin(root, manifest_path)) {
        // Under Mod Organizer 2 usvfs, a merged directory handle and an overwrite
        // file handle can canonicalize to separate physical MO2 directories even
        // when no reparse points or parent traversal exist. Keep both anchored to
        // the reparse-checked virtual mod root in that case.
        filesystem_error.clear();
        root = requested_root.lexically_normal();
        manifest_path = (root / L"manifest.json").lexically_normal();
    }
    if (!IsPathWithin(root, manifest_path) ||
        !std::filesystem::is_regular_file(manifest_path, filesystem_error) || filesystem_error) {
        error = "The mod manifest is missing or resolves outside the mod directory.";
        return std::nullopt;
    }
    const auto manifest_bytes = std::filesystem::file_size(manifest_path, filesystem_error);
    if (filesystem_error || manifest_bytes == 0 || manifest_bytes > kMaximumManifestBytes) {
        error = "The mod manifest must be between 1 byte and 64 KiB.";
        return std::nullopt;
    }

    std::ifstream file(manifest_path, std::ios::binary);
    if (!file) {
        error = "The mod manifest could not be opened.";
        return std::nullopt;
    }
    const std::string contents{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    const auto json = nlohmann::json::parse(contents, nullptr, false);
    if (json.is_discarded() || !HasOnlyKeys(json,
            { "schemaVersion", "id", "name", "version", "entry", "permissions", "papyrusPlugin", "browserEngine" })) {
        error = "The mod manifest is invalid JSON or contains an unsupported top-level field.";
        return std::nullopt;
    }

    if (json.contains("browserEngine") &&
        (!json["browserEngine"].is_string() ||
         (json["browserEngine"].get_ref<const std::string&>() != "ultralight" &&
          json["browserEngine"].get_ref<const std::string&>() != "cef"))) {
        error = "The optional browserEngine field must be either 'ultralight' or 'cef'.";
        return std::nullopt;
    }

    if (!json.contains("schemaVersion") || !json["schemaVersion"].is_number_integer() ||
        json["schemaVersion"].get<std::int64_t>() != 1 ||
        !json.contains("id") || !json["id"].is_string() ||
        json["id"].get<std::string>() != id ||
        !json.contains("name") || !json["name"].is_string() ||
        json["name"].get_ref<const std::string&>().empty() ||
        json["name"].get_ref<const std::string&>().size() > kMaximumManifestNameBytes ||
        !json.contains("version") || !json["version"].is_string() ||
        json["version"].get_ref<const std::string&>().empty() ||
        json["version"].get_ref<const std::string&>().size() > kMaximumManifestVersionBytes ||
        !json.contains("entry") || !json["entry"].is_string() ||
        json["entry"].get_ref<const std::string&>().empty() ||
        json["entry"].get_ref<const std::string&>().size() > kMaximumEntryPathBytes ||
        !json.contains("permissions") ||
        (json.contains("papyrusPlugin") &&
            (!json["papyrusPlugin"].is_string() ||
             !IsValidPapyrusPluginName(json["papyrusPlugin"].get_ref<const std::string&>()) ||
             json["papyrusPlugin"].get_ref<const std::string&>().find('\0') != std::string::npos)) ||
        !HasOnlyKeys(json["permissions"],
            { "storage", "pageRequests", "assetAccess", "networkHosts" })) {
        error = "The mod manifest is missing a required field or has a value outside its supported limits.";
        return std::nullopt;
    }

    const auto& entry_text = json["entry"].get_ref<const std::string&>();
    const auto entry_relative = std::filesystem::u8path(entry_text);
    if (entry_text.find('\0') != std::string::npos || entry_relative.is_absolute() ||
        entry_relative.has_root_name() || entry_relative.has_root_directory()) {
        error = "The manifest entry must be a relative path inside the mod directory.";
        return std::nullopt;
    }
    for (const auto& part : entry_relative) {
        if (part == L".." || part == L".") {
            error = "The manifest entry cannot contain dot or parent-directory components.";
            return std::nullopt;
        }
    }
    if (HasReparsePointOnRelativeChain(requested_root, entry_relative)) {
        error = "The manifest entry is missing or resolves outside the mod directory.";
        return std::nullopt;
    }
    auto entry_file = std::filesystem::canonical(requested_root / entry_relative, filesystem_error);
    if (filesystem_error || !IsPathWithin(root, entry_file)) {
        filesystem_error.clear();
        root = requested_root.lexically_normal();
        entry_file = (root / entry_relative).lexically_normal();
    }
    if (!IsPathWithin(root, entry_file) ||
        !std::filesystem::is_regular_file(entry_file, filesystem_error) || filesystem_error) {
        error = "The manifest entry is missing or resolves outside the mod directory.";
        return std::nullopt;
    }

    const auto& permissions = json["permissions"];
    if (!permissions.contains("storage") || !permissions["storage"].is_boolean() ||
        !permissions.contains("pageRequests") || !permissions["pageRequests"].is_array() ||
        permissions["pageRequests"].size() > kMaximumPageRequestMethods ||
        (permissions.contains("assetAccess") && !permissions["assetAccess"].is_boolean()) ||
        (permissions.contains("networkHosts") &&
            (!permissions["networkHosts"].is_array() ||
             permissions["networkHosts"].size() > kMaximumNetworkHosts))) {
        error = "Manifest permissions must declare storage and pageRequests, with optional assetAccess and at most 128 networkHosts.";
        return std::nullopt;
    }

    ModManifest result;
    result.id.assign(id);
    result.name = json["name"].get<std::string>();
    result.version = json["version"].get<std::string>();
    result.folder_name = requested_root.filename().string();
    if (json.contains("papyrusPlugin")) {
        result.papyrus_plugin = json["papyrusPlugin"].get<std::string>();
    }
    if (json.contains("browserEngine")) {
        result.browser_engine = json["browserEngine"].get<std::string>() == "cef" ?
            BrowserEngine::CEF : BrowserEngine::Ultralight;
    }
    result.root_directory = root;
    result.entry_file = entry_file;
    result.storage_enabled = permissions["storage"].get<bool>();
    if (permissions.contains("assetAccess")) {
        result.asset_access_enabled = permissions["assetAccess"].get<bool>();
    }
    if (permissions.contains("networkHosts")) {
        for (const auto& host : permissions["networkHosts"]) {
            if (!host.is_string()) {
                error = "Every permissions.networkHosts entry must be a hostname string.";
                return std::nullopt;
            }
            std::string normalized_host;
            if (!NormalizeNetworkHost(host.get_ref<const std::string&>(), normalized_host) ||
                std::find(result.network_hosts.begin(), result.network_hosts.end(), normalized_host) !=
                    result.network_hosts.end()) {
                error = "Network host permissions must be unique DNS hostnames (no wildcards, ports, or URL paths).";
                return std::nullopt;
            }
            result.network_hosts.push_back(std::move(normalized_host));
        }
    }
    for (const auto& method : permissions["pageRequests"]) {
        if (!method.is_string()) {
            error = "Every permissions.pageRequests entry must be a string.";
            return std::nullopt;
        }
        const auto& name = method.get_ref<const std::string&>();
        if (!IsValidPageRequestMethod(name) ||
            !result.page_request_methods.emplace(name).second) {
            error = "Page request methods must be unique valid names no longer than 128 bytes.";
            return std::nullopt;
        }
    }
    return result;
}
catch (const std::exception& exception) {
    error = std::string("Manifest loading failed: ") + exception.what();
    return std::nullopt;
}
catch (...) {
    error = "Manifest loading failed with an unknown error.";
    return std::nullopt;
}

std::optional<ModManifest> FindModManifestForPapyrusPlugin(
    const std::filesystem::path& mods_directory,
    std::string_view plugin_name,
    std::string& error) try
{
    if (!IsValidPapyrusPluginName(plugin_name)) {
        error = "The quest's owning plugin name is invalid.";
        return std::nullopt;
    }

    std::error_code filesystem_error;
    std::filesystem::directory_iterator entry(mods_directory, filesystem_error);
    if (filesystem_error) {
        error = "The SkyUltra Mods directory could not be enumerated: " + filesystem_error.message();
        return std::nullopt;
    }

    std::optional<ModManifest> match;
    const std::filesystem::directory_iterator end;
    while (entry != end) {
        std::error_code entry_error;
        if (entry->is_directory(entry_error) && !entry_error) {
            const auto folder_name = entry->path().filename().string();
            auto id = folder_name;
            for (auto& character : id) {
                if (character >= 'A' && character <= 'Z') {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }
            if (IsValidModIdentifier(id)) {
                std::string manifest_error;
                auto manifest = LoadModManifest(mods_directory, id, manifest_error);
                if (manifest && !manifest->papyrus_plugin.empty() &&
                    EqualsAsciiCaseInsensitive(manifest->papyrus_plugin, plugin_name)) {
                    if (match) {
                        error = "More than one SkyUltra manifest is associated with plugin '" +
                            std::string(plugin_name) + "'. Set a unique papyrusPlugin value.";
                        return std::nullopt;
                    }

                    manifest->folder_name = folder_name;
                    match = std::move(manifest);
                }
            }
        }

        entry.increment(filesystem_error);
        if (filesystem_error) {
            error = "The SkyUltra Mods directory could not be enumerated: " + filesystem_error.message();
            return std::nullopt;
        }
    }

    if (!match) {
        error = "No SkyUltra manifest declares papyrusPlugin '" + std::string(plugin_name) + "'.";
        return std::nullopt;
    }
    return match;
}
catch (const std::exception& exception) {
    error = std::string("Papyrus mod discovery failed: ") + exception.what();
    return std::nullopt;
}
catch (...) {
    error = "Papyrus mod discovery failed with an unknown error.";
    return std::nullopt;
}

}  // namespace skyultra
