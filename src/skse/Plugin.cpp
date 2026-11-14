#include "SkyUltra/RuntimeSettings.h"
#include <SKSE/SKSE.h>
#include <RE/M/MenuCursor.h>
#include <RE/T/TESObjectCELL.h>

#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
#    include "SkyUltra/D3D11Overlay.h"
#    include "SkyUltra/VideoSplash.h"
#    include "SkyUltra/FrameworkServices.h"
#    include "SkyUltra/ActorPreview.h"
#    include "SkyUltra/BrowserBackend.h"
#    include "SkyUltra/GameTaskQueue.h"
#    include "SkyUltra/ModManifest.h"
#    include "SkyUltra/NativeAPI.h"
#    include "SkyUltra/Papyrus.h"
#    include "SkyUltra/PanelVisibility.h"
#    include "SkyUltra/ViewManager.h"
#    if defined(SKYULTRA_WITH_CEF)
#        include "SkyUltra/CefBackend.h"
#    endif
#    if defined(SKYULTRA_WITH_ULTRALIGHT)
#        include "SkyUltra/UltralightBackend.h"
#        include "SkyUltra/UltralightRuntimeLoader.h"
#    endif

#    include <Windows.h>
#    include <Psapi.h>

#    include <nlohmann/json.hpp>

#    include <algorithm>
#    include <array>
#    include <atomic>
#    include <chrono>
#    include <charconv>
#    include <cmath>
#    include <cstddef>
#    include <cstdint>
#    include <cstring>
#    include <deque>
#    include <filesystem>
#    include <fstream>
#    include <functional>
#    include <iterator>
#    include <limits>
#    include <memory>
#    include <mutex>
#    include <optional>
#    include <stop_token>
#    include <stdexcept>
#    include <string>
#    include <string_view>
#    include <thread>
#    include <unordered_map>
#    include <unordered_set>
#    include <utility>
#    include <vector>

#    include <RE/A/Actor.h>
#    include <RE/B/BarterMenu.h>
#    include <RE/C/ContainerMenu.h>
#    include <RE/B/BSInputDeviceManager.h>
#    if defined(ENABLE_SKYRIM_VR)
#        include <RE/B/BSOpenVRControllerDevice.h>
#    endif
#    include <RE/B/BSWin32GamepadDevice.h>
#    include <RE/B/BSKeyboardDevice.h>
#    include <RE/B/BSWin32MouseDevice.h>
#    include <RE/B/ButtonEvent.h>
#    include <RE/C/CharEvent.h>
#    include <RE/G/GFxMovieView.h>
#    include <RE/G/GFxValue.h>
#    include <RE/I/Inventory3DManager.h>
#    include <RE/I/InventoryMenu.h>
#    include <RE/M/MenuOpenCloseEvent.h>
#    include <RE/M/Misc.h>
#    include <RE/M/MouseMoveEvent.h>
#    include <RE/N/NiAVObject.h>
#    include <RE/N/NiCamera.h>
#    include <RE/N/NiNode.h>
#    include <RE/N/NiRTTI.h>
#    include <RE/P/PlayerCamera.h>
#    include <RE/P/PlayerCharacter.h>
#    include <RE/R/Renderer.h>
#    include <RE/T/TESForm.h>
#    include <RE/T/TESObjectREFR.h>
#    include <RE/T/ThumbstickEvent.h>
#    include <RE/U/UI.h>
#    include <RE/U/UIMessageQueue.h>
#endif

using namespace std::literals;

namespace {

#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)

constexpr std::uint32_t kBrowserToggleScanCode = RE::BSKeyboardDevice::Keys::kF6;

struct PerformanceSettings {
    skyultra::SizeU main_size{};
    skyultra::SizeU popup_size{ 420, 240 };
    std::uint32_t main_fps{};
    std::uint32_t popup_fps{};
};

PerformanceSettings g_performance_settings;

enum class BrowserRenderMode : std::uint8_t {
    Gpu,
    Cpu,
};

struct BrowserSettings {
    BrowserRenderMode render_mode{ BrowserRenderMode::Gpu };
    bool cef_multithreaded{ true };
};

BrowserSettings g_browser_settings;

const char* BrowserEngineName(skyultra::BrowserEngine engine) noexcept;

std::unique_ptr<skyultra::IWebRuntime> g_browser_runtime;
std::unique_ptr<skyultra::ViewManager> g_view_manager;
std::string_view g_runtime_name = "unknown runtime";
skyultra::ViewId g_menu_view_id{};
skyultra::ViewId g_window_test_view_id{};
std::atomic_bool g_browser_started{};
std::atomic_bool g_native_api_ready{};
std::jthread g_mod_hot_reload_thread;
skyultra::GameTaskQueue g_game_task_queue;
bool g_input_sink_registered{};
bool g_menu_event_sink_registered{};
bool g_modal_menu_open{ true };
std::atomic_bool g_game_session_active{ false };
bool g_restore_browser_after_menu{};
std::atomic_bool g_browser_overlay_suppressed{};
bool g_mod_hot_reload_enabled{};
std::atomic_bool g_logged_first_mod_hot_reload{};
bool g_render_hook_installed{};
bool g_logged_popup_click{};
bool g_logged_first_browser_key_input{};
bool g_logged_first_browser_focus{};
bool g_logged_first_browser_text_input{};
POINT g_browser_cursor{};
std::array<std::optional<skyultra::ViewId>, 3> g_mouse_capture{};
std::optional<skyultra::ViewId> g_gamepad_stick_owner;
std::uint8_t g_gamepad_stick_keys{};
std::array<bool, 2> g_vr_suppressed_triggers{};
std::array<bool, 2> g_vr_suppressed_exit_buttons{};
std::array<bool, 2> g_vr_suppressed_keyboard_buttons{};

std::shared_ptr<skyultra::BridgeEventQueue> NativeEventsForView(skyultra::ViewId id, bool responses = false)
{
    if (g_view_manager) if (auto* view = g_view_manager->Find(id)) {
        if (auto queue = responses ? view->BridgeResponses() : view->BridgeEvents()) return queue;
    }
    return std::make_shared<skyultra::BridgeEventQueue>(!responses);
}

struct NativeApiViewRecord {
    std::shared_ptr<skyultra::BridgeEventQueue> bridge_events{std::make_shared<skyultra::BridgeEventQueue>()};
    std::shared_ptr<skyultra::BridgeEventQueue> bridge_responses{std::make_shared<skyultra::BridgeEventQueue>(false)};
    SkyUltraViewStateV1 state{ SKYULTRA_VIEW_PENDING };
    std::optional<skyultra::ViewId> manager_id;
    SkyUltraModHandle mod_handle{};
    std::string initial_url;
    skyultra::BrowserEngine browser_engine{ skyultra::BrowserEngine::Ultralight };
    bool javascript_enabled{};
    bool wait_for_page_ready{};
    bool hide_native_cursor{};
    std::uint32_t cursor_accent{0xD8C49CU};
    std::uint32_t input_scope_depth{};
    std::optional<skyultra::ViewConfig> pending_config;
    struct PendingPageRequest {
        SkyUltraPageRequestCallbackV2 callback{};
        void* user_data{};
        bool completion_queued{};
    };
    std::unordered_map<std::string, PendingPageRequest> pending_page_requests;
    std::unordered_set<std::string> allowed_page_request_methods;
};

struct NativeApiModRecord {
    skyultra::ModManifest manifest;
    std::filesystem::path storage_directory;
};

std::atomic_uint64_t g_next_native_view_handle{ 1 };
std::atomic_uint64_t g_next_native_mod_handle{ 1 };
std::mutex g_native_views_mutex;
std::unordered_map<SkyUltraViewHandle, NativeApiViewRecord> g_native_views;
std::mutex g_native_mods_mutex;
std::unordered_map<SkyUltraModHandle, NativeApiModRecord> g_native_mods;
struct WidgetOverlayRecord {
    SkyUltraViewHandle view{};
    std::vector<std::string> widgets;
    std::string anchor{ "top-right" };
    float scale{ 1.0F };
    float opacity{ 1.0F };
    skyultra::ViewId manager_id{};
    std::string theme{ "dragonstone" };
    bool ready{};
    int glow{ 100 };
    nlohmann::json colors = nlohmann::json::object();
    nlohmann::json placements = nlohmann::json::object();
    nlohmann::json custom_code = nlohmann::json::object();
};
std::unordered_map<SkyUltraModHandle, WidgetOverlayRecord> g_widget_overlays; // game thread only
std::unordered_set<skyultra::ViewId> g_suspended_panel_views; // Temporary Skyrim modal restore only; game thread.
struct GameDataSubscriptionRecord {
    std::vector<std::string> topics;
    bool compact{ false };
    std::uint32_t interval_ms{ 750 };
    std::uint64_t last_push_ms{ 0 };
    std::uint64_t last_sequence{ 0 };
    bool needs_catchup{ false };
    std::unordered_map<std::string, std::string> topic_signatures;
};
std::unordered_map<SkyUltraViewHandle, GameDataSubscriptionRecord> g_game_subscriptions; // game thread only
std::atomic_uint32_t g_active_game_subscription_count{ 0 };
std::atomic_bool g_game_subscription_pump_queued{ false };
std::jthread g_game_subscription_thread;
std::unordered_map<std::string, SkyUltraModHandle> g_native_mod_handles_by_id;

struct DevBridgeLogEntry {
    std::uint64_t seq{};
    double time_ms{};
    std::uint32_t view_id{};
    std::string mod_id;
    std::string kind;
    std::string method;
    std::string status;
    double duration_ms{};
    std::size_t payload_bytes{};
    std::string message;
};

struct DevModRuntimeStats {
    std::uint64_t bridge_requests{};
    std::uint64_t bridge_errors{};
    std::uint64_t denied_permissions{};
    std::uint64_t hot_reloads{};
};

constexpr std::size_t kMaxDevBridgeEntries = 160;
std::mutex g_dev_diagnostics_mutex;
std::uint64_t g_next_dev_bridge_seq{ 1 };
std::deque<DevBridgeLogEntry> g_dev_bridge_log;
std::unordered_map<std::string, DevModRuntimeStats> g_dev_mod_stats;
std::atomic_uint64_t g_hot_reload_total_count{ 0 };
std::string g_last_reloaded_mod;
SkyUltraViewHandle g_diagnostic_sample_view{};
std::mutex g_native_api_queue_mutex;
std::mutex g_native_api_enqueue_mutex;
std::deque<std::function<void()>> g_native_api_queue;
bool g_native_api_drain_scheduled{};
std::mutex g_page_request_callback_mutex;
SkyUltraPageRequestCallbackV2 g_page_request_callback{};
void* g_page_request_user_data{};

bool QueueNativeApiTask(std::function<void()> task);
SkyUltraResult SKYULTRA_CALL NativeApiSetVisible(SkyUltraViewHandle handle,std::uint8_t visible);
void OnBrowserPageBridgeMessage(skyultra::PageBridgeMessage message);
SkyUltraResult SKYULTRA_CALL NativeApiCompletePageRequest(
    SkyUltraViewHandle handle, const char* request_id_utf8, std::uint32_t request_id_length,
    std::uint8_t success, const char* result_json_utf8, std::uint32_t result_json_length,
    const char* error_utf8, std::uint32_t error_length);
SkyUltraResult SKYULTRA_CALL NativeApiReloadView(SkyUltraViewHandle handle);
SkyUltraResult SKYULTRA_CALL NativeApiDestroyView(SkyUltraViewHandle handle);
bool HandleHostUiRequest(SkyUltraViewHandle handle, const skyultra::PageBridgeMessage& message);
void StartModHotReloadWatcher();
std::optional<skyultra::ViewId> NativeManagerViewId(SkyUltraViewHandle handle);
bool NativeHandleAcceptsCommands(SkyUltraViewHandle handle);
void PumpGameDataSubscriptions(bool force_immediate = false);
void StartGameDataSubscriptionTicker();

constexpr std::array<std::pair<std::uint8_t, std::uint32_t>, 4> kGamepadStickVirtualKeys{{
    { 1U << 0U, VK_UP },
    { 1U << 1U, VK_DOWN },
    { 1U << 2U, VK_LEFT },
    { 1U << 3U, VK_RIGHT },
}};

std::filesystem::path PluginDirectory()
{
    const auto module = GetModuleHandleW(L"SkyUltra.dll");
    if (module == nullptr) {
        return {};
    }

    std::array<wchar_t, 32768> module_path{};
    const auto length = GetModuleFileNameW(module, module_path.data(),
        static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size()) {
        return {};
    }
    return std::filesystem::path(module_path.data()).parent_path();
}

std::filesystem::path BrowserCacheDirectory(std::wstring_view backend)
{
    std::array<wchar_t, 32768> local_app_data{};
    const auto length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", local_app_data.data(), static_cast<DWORD>(local_app_data.size()));
    if (length > 0 && length < local_app_data.size()) {
        return std::filesystem::path(local_app_data.data()) / L"SkyUltra" / backend;
    }

    std::error_code error;
    const auto temporary = std::filesystem::temp_directory_path(error);
    return error ? std::filesystem::path{} : temporary / L"SkyUltra" / backend;
}

bool IsDirectPathChildNamed(
    const std::filesystem::path& parent,
    const std::filesystem::path& child,
    std::string_view name)
{
    const auto relative = child.lexically_relative(parent);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    auto component = relative.begin();
    if (component == relative.end()) {
        return false;
    }
    const auto expected = std::filesystem::u8path(std::string(name)).native();
    const auto& actual = component->native();
    return CompareStringOrdinal(actual.c_str(), static_cast<int>(actual.size()),
               expected.c_str(), static_cast<int>(expected.size()), TRUE) == CSTR_EQUAL &&
        ++component == relative.end();
}

std::filesystem::path ModStorageDirectory(std::string_view mod_id, std::string& error)
{
    std::array<wchar_t, 32768> local_app_data{};
    const auto length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", local_app_data.data(), static_cast<DWORD>(local_app_data.size()));
    std::error_code filesystem_error;
    std::filesystem::path data_root;
    if (length > 0 && length < local_app_data.size()) {
        data_root = std::filesystem::path(local_app_data.data()) / L"SkyUltra" / L"Mods";
    } else {
        const auto temporary = std::filesystem::temp_directory_path(filesystem_error);
        if (filesystem_error) {
            error = "Windows could not resolve a local storage root: " + filesystem_error.message();
            return {};
        }
        data_root = temporary / L"SkyUltra" / L"Mods";
    }

    std::filesystem::create_directories(data_root, filesystem_error);
    if (filesystem_error) {
        error = "Windows could not create the SkyUltra mod storage root: " + filesystem_error.message();
        return {};
    }
    const auto canonical_root = std::filesystem::canonical(data_root, filesystem_error);
    if (filesystem_error) {
        error = "Windows could not resolve the SkyUltra mod storage root: " + filesystem_error.message();
        return {};
    }
    const auto expected_mod_directory = std::filesystem::u8path(std::string(mod_id));
    const auto requested_mod_directory = canonical_root / expected_mod_directory;
    std::filesystem::create_directories(requested_mod_directory, filesystem_error);
    if (filesystem_error) {
        error = "Windows could not create this mod's storage directory: " + filesystem_error.message();
        return {};
    }
    const auto mod_directory = std::filesystem::canonical(requested_mod_directory, filesystem_error);
    if (filesystem_error || !IsDirectPathChildNamed(canonical_root, mod_directory, mod_id)) {
        error = "This mod's storage directory resolves through a link to another mod.";
        return {};
    }
    const auto requested_storage = mod_directory / L"Storage";
    std::filesystem::create_directories(requested_storage, filesystem_error);
    if (filesystem_error) {
        error = "Windows could not create this mod's storage directory: " + filesystem_error.message();
        return {};
    }
    const auto storage = std::filesystem::canonical(requested_storage, filesystem_error);
    if (filesystem_error) {
        error = "Windows could not resolve this mod's storage directory: " + filesystem_error.message();
        return {};
    }
    if (!IsDirectPathChildNamed(mod_directory, storage, "Storage")) {
        error = "This mod's storage directory resolves through a link to another directory.";
        return {};
    }
    return storage;
}

std::string PathUtf8(const std::filesystem::path& path)
{
    const auto utf8 = path.u8string();
    return { reinterpret_cast<const char*>(utf8.data()), utf8.size() };
}

std::string LocalFileUrl(const std::filesystem::path& path)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    const auto utf8_path = PathUtf8(path);
    std::string result = "file:///";
    result.reserve(result.size() + utf8_path.size() * 3U);
    for (const auto value : utf8_path) {
        const auto normalized = value == '\\' ? '/' : value;
        const auto byte = static_cast<unsigned char>(normalized);
        const bool unreserved =
            (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' ||
            byte == '.' || byte == '~' || byte == '/' || byte == ':';
        if (unreserved) {
            result.push_back(static_cast<char>(byte));
        } else {
            result.push_back('%');
            result.push_back(kHex[byte >> 4U]);
            result.push_back(kHex[byte & 0x0FU]);
        }
    }
    return result;
}

std::string_view RuntimeName(const SKSE::LoadInterface* skse) noexcept
{
    if (REL::Module::IsVR()) {
        return "Skyrim VR";
    }
    if (REL::Module::IsSE()) {
        return "Skyrim SE";
    }
    if (!REL::Module::IsAE()) {
        return "unknown Skyrim runtime";
    }

    // SKSE uses the fourth runtime-version component to distinguish store builds.
    // Check the full known versions so the Epic runtime is not mislabeled as GOG.
    if (skse != nullptr) {
        const auto version = skse->RuntimeVersion();
        if (version == SKSE::RUNTIME_SSE_1_6_659_GOG ||
            version == SKSE::RUNTIME_SSE_1_6_1170_GOG ||
            version == SKSE::RUNTIME_SSE_1_6_1179_GOG) {
            return "Skyrim AE (GOG)";
        }
        if (version == SKSE::RUNTIME_SSE_1_6_678_EPIC) {
            return "Skyrim AE (Epic)";
        }
    }
    return "Skyrim AE";
}

std::string PerformanceFpsLabel(std::uint32_t fps)
{
    return fps == 0 ? "display refresh" : std::to_string(fps) + " FPS";
}

skyultra::VideoSplashSettings g_splash_settings;
nlohmann::json g_runtime_settings = skyultra::DefaultRuntimeSettings();
struct ActorExportSettings {std::uint32_t full_view{4096},portrait{4096};};
ActorExportSettings g_actor_export_settings;

struct HotkeyBinding {
    std::vector<std::string> keyboard{ "Shift", "F6" };
    std::vector<std::string> gamepad{ "Back", "RightThumb" };
    std::vector<std::string> vr{ "LeftGrip", "RightBY" };
};
HotkeyBinding g_skyultra_hotkey;
HotkeyBinding g_keyboard_hotkey{
    { "Shift", "K" },
    { "LT", "LeftThumb" },
    { "LeftGrip", "LeftStickClick" }
};
bool g_virtual_keyboard_open{ false };
bool g_virtual_keyboard_opened_menu{ false };
std::string g_menu_browser_engine{ "cef" };
std::unordered_map<std::string, std::string> g_mod_browser_engines;
bool g_vr_head_tracking{ true };
bool g_replace_skyrim_inventory{ true };
bool g_replace_skyrim_barter{ true };
std::string g_menu_display_mode{ "fullscreen" };
std::string g_inventory_display_mode{ "fullscreen" };
std::string g_barter_display_mode{ "half" };
bool g_menu_pause_world{ false };
bool g_inventory_pause_world{ false };
bool g_barter_pause_world{ false };
bool g_skyultra_world_paused{ false };
bool g_inventory_replacement_active{ false };
bool g_barter_replacement_active{ false };
// Magic (spellbook), Favorites, Container, and Mannequin menu replacements. Display modes mirror barter: half / full / windowed.
struct ExtraMenuSettings {
    bool replace_magic{ true };
    bool replace_favorites{ true };
    bool replace_container{ true };
    bool replace_mannequin{ true };
    bool replace_loot{ true };
    bool replace_pickpocket{ true };
    bool replace_compass{ true };
    std::string magic_display_mode{ "half" };
    std::string favorites_display_mode{ "half" };
    std::string container_display_mode{ "half" };
    std::string mannequin_display_mode{ "full" };
    std::string loot_display_mode{ "half" };
    std::string pickpocket_display_mode{ "half" };
    bool magic_pause_world{ false };
    bool favorites_pause_world{ false };
    bool container_pause_world{ false };
    bool mannequin_pause_world{ false };
    bool loot_pause_world{ false };
    bool pickpocket_pause_world{ false };
};
ExtraMenuSettings g_extra_menus;
bool g_magic_replacement_active{ false };
bool g_favorites_replacement_active{ false };
bool g_container_replacement_active{ false };
bool g_mannequin_replacement_active{ false };
bool g_loot_replacement_active{ false };
bool g_pickpocket_replacement_active{ false };
std::unordered_map<std::string, HotkeyBinding> g_mod_hotkeys;

bool IsBrowserEngineBuilt(skyultra::BrowserEngine engine) noexcept;
void SwitchMenuBrowserEngine(skyultra::BrowserEngine target_engine);
void SwitchModBrowserEngine(std::string_view mod_id, skyultra::BrowserEngine target_engine);
void SyncSkyUltraWorldPause() noexcept;
void SyncVanillaCompassVisibility(bool force = false) noexcept;
void PublishOverlayViews();
bool IsModalOrBlockingMenuOpen(RE::UI* ui) noexcept;
bool ShouldSuppressHudOverlays() noexcept;
bool AnyMenuReplacementActive() noexcept;
void SendMenuReplacementModes();

nlohmann::json HotkeyBindingJson(const HotkeyBinding& binding)
{
    return {
        { "keyboard", binding.keyboard },
        { "gamepad", binding.gamepad },
        { "vr", binding.vr }
    };
}

bool ParseHotkeyChordArray(const nlohmann::json& value, std::vector<std::string>& out, bool allow_empty)
{
    if (!value.is_array() || value.size() > 3 || (!allow_empty && value.empty())) return false;
    std::vector<std::string> parsed;
    parsed.reserve(value.size());
    for (const auto& item : value) {
        if (!item.is_string()) return false;
        const auto& token = item.get_ref<const std::string&>();
        if (token.empty() || token.size() > 32) return false;
        for (const char c : token) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
                return false;
            }
        }
        if (std::find(parsed.begin(), parsed.end(), token) == parsed.end()) {
            parsed.push_back(token);
        }
    }
    if (!allow_empty && parsed.empty()) return false;
    out = std::move(parsed);
    return true;
}

bool ParseHotkeyBinding(const nlohmann::json& input, HotkeyBinding& out, bool allow_empty_keyboard, std::string& error)
{
    if (!input.is_object()) {
        error = "Hotkey binding must be an object.";
        return false;
    }
    HotkeyBinding candidate = out;
    if (const auto kb = input.find("keyboard"); kb != input.end()) {
        if (!ParseHotkeyChordArray(*kb, candidate.keyboard, allow_empty_keyboard)) {
            error = "Keyboard hotkey must contain 1 to 3 valid key tokens.";
            return false;
        }
    }
    if (const auto gp = input.find("gamepad"); gp != input.end()) {
        if (!ParseHotkeyChordArray(*gp, candidate.gamepad, true)) {
            error = "Gamepad hotkey must contain up to 3 valid controller tokens.";
            return false;
        }
    }
    if (const auto vr = input.find("vr"); vr != input.end()) {
        if (!ParseHotkeyChordArray(*vr, candidate.vr, true)) {
            error = "VR hotkey must contain up to 3 valid headset/controller tokens.";
            return false;
        }
    }
    out = std::move(candidate);
    return true;
}

nlohmann::json g_profile_state_json = nlohmann::json::object();

nlohmann::json FrameworkSettingsJson()
{
    nlohmann::json mod_hotkeys = nlohmann::json::object();
    for (const auto& [mod_id, binding] : g_mod_hotkeys) {
        if (mod_id == "skyultra.menu" || mod_id == "skyultra.ui_kit_test_mod" || mod_id == "skyultra.uikit") continue;
        mod_hotkeys[mod_id] = HotkeyBindingJson(binding);
    }
    nlohmann::json mod_engines = nlohmann::json::object();
    for (const auto& [mod_id, engine_name] : g_mod_browser_engines) {
        mod_engines[mod_id] = engine_name;
    }
    nlohmann::json available_engines = nlohmann::json::array();
    if (IsBrowserEngineBuilt(skyultra::BrowserEngine::CEF)) {
        available_engines.push_back("cef");
    }
    if (IsBrowserEngineBuilt(skyultra::BrowserEngine::Ultralight)) {
        available_engines.push_back("ultralight");
    }
    nlohmann::json registered_mods = nlohmann::json::array();
    std::unordered_set<std::string> seen_mods{ "skyultra.menu", "skyultra.ui_kit_test_mod", "skyultra.uikit" };
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        for (const auto& [handle, record] : g_native_mods) {
            (void)handle;
            if (!record.manifest.id.empty() && seen_mods.insert(record.manifest.id).second) {
                registered_mods.push_back({
                    { "id", record.manifest.id },
                    { "name", record.manifest.name.empty() ? record.manifest.id : record.manifest.name }
                });
            }
        }
    }
    if (const auto mods_dir = skyultra::GetInstalledWebModsDirectory(); !mods_dir.empty()) {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(mods_dir, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code dir_ec;
            if (!it->is_directory(dir_ec) || dir_ec) continue;
            auto folder_id = it->path().filename().string();
            for (auto& ch : folder_id) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            }
            if (!skyultra::IsValidModIdentifier(folder_id) || seen_mods.contains(folder_id)) continue;
            std::string manifest_error;
            if (const auto manifest = skyultra::LoadModManifest(mods_dir, folder_id, manifest_error)) {
                if (seen_mods.insert(manifest->id).second) {
                    registered_mods.push_back({
                        { "id", manifest->id },
                        { "name", manifest->name.empty() ? manifest->id : manifest->name }
                    });
                }
            }
        }
    }
    return {{"schemaVersion",2}, {"runtime",g_runtime_settings},
        {"splash",{{"enabled",g_splash_settings.enabled},{"playback",g_splash_settings.full_duration?"full-duration":"until-ready"},
        {"replay",g_splash_settings.replay},{"audio",g_splash_settings.audio},{"volume",g_splash_settings.volume}}},
        {"actorExport",{{"fullView",g_actor_export_settings.full_view},{"portrait",g_actor_export_settings.portrait}}},
        {"hotkey",HotkeyBindingJson(g_skyultra_hotkey)},
        {"keyboardHotkey",HotkeyBindingJson(g_keyboard_hotkey)},
        {"menuEngine",g_menu_browser_engine},
        {"availableEngines",std::move(available_engines)},
        {"modEngines",std::move(mod_engines)},
        {"vrHeadTracking",g_vr_head_tracking},
        {"replaceSkyrimInventory",g_replace_skyrim_inventory},
        {"replaceSkyrimBarter",g_replace_skyrim_barter},
        {"menuDisplayMode",g_menu_display_mode},
        {"inventoryDisplayMode",g_inventory_display_mode},
        {"barterDisplayMode",g_barter_display_mode},
        {"menuPauseWorld",g_menu_pause_world},
        {"inventoryPauseWorld",g_inventory_pause_world},
        {"barterPauseWorld",g_barter_pause_world},
        {"replaceSkyrimMagic",g_extra_menus.replace_magic},
        {"replaceSkyrimFavorites",g_extra_menus.replace_favorites},
        {"replaceSkyrimContainer",g_extra_menus.replace_container},
        {"replaceSkyrimMannequin",g_extra_menus.replace_mannequin},
        {"replaceSkyrimLoot",g_extra_menus.replace_loot},
        {"replaceSkyrimPickpocket",g_extra_menus.replace_pickpocket},
        {"replaceSkyrimCompass",g_extra_menus.replace_compass},
        {"magicDisplayMode",g_extra_menus.magic_display_mode},
        {"favoritesDisplayMode",g_extra_menus.favorites_display_mode},
        {"containerDisplayMode",g_extra_menus.container_display_mode},
        {"mannequinDisplayMode",g_extra_menus.mannequin_display_mode},
        {"lootDisplayMode",g_extra_menus.loot_display_mode},
        {"pickpocketDisplayMode",g_extra_menus.pickpocket_display_mode},
        {"magicPauseWorld",g_extra_menus.magic_pause_world},
        {"favoritesPauseWorld",g_extra_menus.favorites_pause_world},
        {"containerPauseWorld",g_extra_menus.container_pause_world},
        {"mannequinPauseWorld",g_extra_menus.mannequin_pause_world},
        {"lootPauseWorld",g_extra_menus.loot_pause_world},
        {"pickpocketPauseWorld",g_extra_menus.pickpocket_pause_world},
        {"modHotkeys",std::move(mod_hotkeys)},
        {"registeredMods",std::move(registered_mods)},
        {"profileState",g_profile_state_json}};
}
bool ParseFrameworkSettings(const nlohmann::json& input,skyultra::VideoSplashSettings& result,ActorExportSettings& exports,HotkeyBinding& hotkey,HotkeyBinding& keyboard_hotkey,std::string& menu_engine,std::unordered_map<std::string,std::string>& mod_engines,bool& vr_head_tracking,bool& replace_skyrim_inventory,bool& replace_skyrim_barter,std::string& menu_display_mode,std::string& inventory_display_mode,std::string& barter_display_mode,bool& menu_pause_world,bool& inventory_pause_world,bool& barter_pause_world,ExtraMenuSettings& extra_menus,std::unordered_map<std::string,HotkeyBinding>& mod_hotkeys,nlohmann::json& profile_state,std::string& error)
{
    if(!input.is_object()){error="Expected a settings object.";return false;}
    if(const auto sv=input.find("schemaVersion");sv!=input.end()){
        if(!sv->is_number_integer()||(*sv<1||*sv>2)){error="Unsupported SkyUltra settings schemaVersion.";return false;}
    }
    const bool has_profile_state=input.contains("profileState");
    if(input.contains("splash")){
        if(!input["splash"].is_object()){error="Expected a splash settings object.";return false;}
        const auto& splash=input["splash"];
        if(!splash.contains("enabled")||!splash["enabled"].is_boolean()||!splash.contains("audio")||!splash["audio"].is_boolean()||
            !splash.contains("playback")||!splash["playback"].is_string()||!splash.contains("replay")||!splash["replay"].is_string()||
            !splash.contains("volume")||!splash["volume"].is_number()){error="Missing or invalid splash setting.";return false;}
        const auto playback=splash["playback"].get<std::string>();const auto replay=splash["replay"].get<std::string>();
        const auto volume=splash["volume"].get<double>();
        if((playback!="full-duration"&&playback!="until-ready")||(replay!="session"&&replay!="per-mod"&&replay!="each-open")||!std::isfinite(volume)||volume<0||volume>1){error="Splash settings are out of range.";return false;}
        result.enabled=splash["enabled"].get<bool>();result.audio=splash["audio"].get<bool>();result.full_duration=playback=="full-duration";
        result.replay=replay;result.volume=static_cast<float>(volume);
    }else if(!has_profile_state){
        error="Expected a splash settings object.";return false;
    }
    // Older settings files/pages contain only splash preferences. Keep defaults
    // on first load, or retain existing export choices on a splash-only save.
    if(const auto field=input.find("actorExport");field!=input.end()){
        const auto valid=[](const nlohmann::json& value){return value.is_number_integer() &&
            (value==720||value==1080||value==1440||value==2048||value==2160||value==4096);};
        if(!field->is_object()||!field->contains("fullView")||!field->contains("portrait")||
            !valid((*field)["fullView"])||!valid((*field)["portrait"])){error="Invalid actor export resolution.";return false;}
        exports.full_view=(*field)["fullView"].get<std::uint32_t>();exports.portrait=(*field)["portrait"].get<std::uint32_t>();
    }
    if(const auto field=input.find("hotkey");field!=input.end()){
        if(!ParseHotkeyBinding(*field,hotkey,false,error))return false;
    }
    if(const auto field=input.find("keyboardHotkey");field!=input.end()){
        if(!ParseHotkeyBinding(*field,keyboard_hotkey,true,error))return false;
    }
    if(const auto field=input.find("menuEngine");field!=input.end()){
        if(!field->is_string()){error="Invalid menuEngine setting.";return false;}
        auto eng=field->get<std::string>();
        for(auto& ch:eng){if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');}
        if(eng!="ultralight"&&eng!="cef"){error="Unsupported menuEngine setting.";return false;}
        menu_engine=std::move(eng);
    }
    if(const auto field=input.find("modEngines");field!=input.end()){
        if(!field->is_object()||field->size()>64){error="Invalid modEngines map.";return false;}
        std::unordered_map<std::string,std::string> parsed_engines=mod_engines;
        for(auto it=field->begin();it!=field->end();++it){
            if(!skyultra::IsValidModIdentifier(it.key())||!it.value().is_string()){error="Invalid mod identifier or engine in modEngines.";return false;}
            auto eng=it.value().get<std::string>();
            for(auto& ch:eng){if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');}
            if(eng!="ultralight"&&eng!="cef"){error="Unsupported engine in modEngines.";return false;}
            parsed_engines[it.key()]=std::move(eng);
        }
        mod_engines=std::move(parsed_engines);
    }
    if(const auto field=input.find("vrHeadTracking");field!=input.end()){
        if(!field->is_boolean()){error="Invalid vrHeadTracking setting.";return false;}
        vr_head_tracking=field->get<bool>();
    }
    if(const auto field=input.find("replaceSkyrimInventory");field!=input.end()){
        if(!field->is_boolean()){error="Invalid replaceSkyrimInventory setting.";return false;}
        replace_skyrim_inventory=field->get<bool>();
    }
    if(const auto field=input.find("replaceSkyrimBarter");field!=input.end()){
        if(!field->is_boolean()){error="Invalid replaceSkyrimBarter setting.";return false;}
        replace_skyrim_barter=field->get<bool>();
    }
    if(const auto field=input.find("menuDisplayMode");field!=input.end()){
        if(!field->is_string()||(*field!="fullscreen"&&*field!="windowed")){error="Invalid menuDisplayMode setting.";return false;}
        menu_display_mode=field->get<std::string>();
    }
    if(const auto field=input.find("inventoryDisplayMode");field!=input.end()){
        if(!field->is_string()||(*field!="fullscreen"&&*field!="windowed")){error="Invalid inventoryDisplayMode setting.";return false;}
        inventory_display_mode=field->get<std::string>();
    }
    if(const auto field=input.find("barterDisplayMode");field!=input.end()){
        if(!field->is_string()||(*field!="half"&&*field!="full"&&*field!="fullscreen"&&*field!="windowed")){error="Invalid barterDisplayMode setting.";return false;}
        barter_display_mode=(*field=="fullscreen")?"full":field->get<std::string>();
    }
    if(const auto field=input.find("menuPauseWorld");field!=input.end()){
        if(!field->is_boolean()){error="Invalid menuPauseWorld setting.";return false;}
        menu_pause_world=field->get<bool>();
    }
    if(const auto field=input.find("inventoryPauseWorld");field!=input.end()){
        if(!field->is_boolean()){error="Invalid inventoryPauseWorld setting.";return false;}
        inventory_pause_world=field->get<bool>();
    }
    if(const auto field=input.find("barterPauseWorld");field!=input.end()){
        if(!field->is_boolean()){error="Invalid barterPauseWorld setting.";return false;}
        barter_pause_world=field->get<bool>();
    }
    {
        const auto read_bool=[&](const char* key,bool& out)->bool{
            if(const auto field=input.find(key);field!=input.end()){
                if(!field->is_boolean()){error=std::string("Invalid ")+key+" setting.";return false;}
                out=field->get<bool>();
            }
            return true;
        };
        const auto read_container_mode=[&](const char* key,std::string& out)->bool{
            if(const auto field=input.find(key);field!=input.end()){
                if(!field->is_string()||(*field!="half"&&*field!="full"&&*field!="fullscreen"&&*field!="windowed")){error=std::string("Invalid ")+key+" setting.";return false;}
                out=(*field=="fullscreen")?"full":field->get<std::string>();
            }
            return true;
        };
        if(!read_bool("replaceSkyrimMagic",extra_menus.replace_magic)||!read_bool("replaceSkyrimFavorites",extra_menus.replace_favorites)||
            !read_bool("replaceSkyrimContainer",extra_menus.replace_container)||!read_bool("replaceSkyrimMannequin",extra_menus.replace_mannequin)||
            !read_bool("replaceSkyrimLoot",extra_menus.replace_loot)||!read_bool("replaceSkyrimPickpocket",extra_menus.replace_pickpocket)||
            !read_bool("replaceSkyrimCompass",extra_menus.replace_compass)||
            !read_container_mode("magicDisplayMode",extra_menus.magic_display_mode)||!read_container_mode("favoritesDisplayMode",extra_menus.favorites_display_mode)||
            !read_container_mode("containerDisplayMode",extra_menus.container_display_mode)||!read_container_mode("mannequinDisplayMode",extra_menus.mannequin_display_mode)||
            !read_container_mode("lootDisplayMode",extra_menus.loot_display_mode)||!read_container_mode("pickpocketDisplayMode",extra_menus.pickpocket_display_mode)||
            !read_bool("magicPauseWorld",extra_menus.magic_pause_world)||!read_bool("favoritesPauseWorld",extra_menus.favorites_pause_world)||
            !read_bool("containerPauseWorld",extra_menus.container_pause_world)||!read_bool("mannequinPauseWorld",extra_menus.mannequin_pause_world)||
            !read_bool("lootPauseWorld",extra_menus.loot_pause_world)||!read_bool("pickpocketPauseWorld",extra_menus.pickpocket_pause_world))return false;
    }
    if(const auto field=input.find("modHotkeys");field!=input.end()){
        if(!field->is_object()||field->size()>64){error="Invalid modHotkeys map.";return false;}
        std::unordered_map<std::string,HotkeyBinding> parsed_mods=mod_hotkeys;
        for(auto it=field->begin();it!=field->end();++it){
            if(it.key()=="skyultra.menu"||it.key()=="skyultra.ui_kit_test_mod"||it.key()=="skyultra.uikit")continue;
            if(!skyultra::IsValidModIdentifier(it.key())){error="Invalid mod identifier in modHotkeys.";return false;}
            HotkeyBinding mod_binding{{},{},{}};
            if(!ParseHotkeyBinding(it.value(),mod_binding,true,error))return false;
            parsed_mods[it.key()]=std::move(mod_binding);
        }
        mod_hotkeys=std::move(parsed_mods);
    }
    if(const auto field=input.find("profileState");field!=input.end()){
        if(!field->is_object()){error="Invalid profileState object.";return false;}
        const auto dumped=field->dump();
        if(dumped.size()>262144){error="Profile state exceeds the 256 KiB persistence budget.";return false;}
        profile_state=*field;
    }
    return true;
}
void LoadFrameworkSettings()
{
    std::ifstream input(PluginDirectory()/L"SkyUltra.settings.json");
    if(input){const auto json=nlohmann::json::parse(input,nullptr,false);std::string error;skyultra::VideoSplashSettings parsed=g_splash_settings;auto exports=g_actor_export_settings;auto hotkey=g_skyultra_hotkey;auto keyboard_hotkey=g_keyboard_hotkey;auto menu_engine=g_menu_browser_engine;auto mod_engines=g_mod_browser_engines;auto vr_head_tracking=g_vr_head_tracking;auto replace_skyrim_inventory=g_replace_skyrim_inventory;auto replace_skyrim_barter=g_replace_skyrim_barter;auto menu_display_mode=g_menu_display_mode;auto inventory_display_mode=g_inventory_display_mode;auto barter_display_mode=g_barter_display_mode;auto menu_pause_world=g_menu_pause_world;auto inventory_pause_world=g_inventory_pause_world;auto barter_pause_world=g_barter_pause_world;auto extra_menus=g_extra_menus;auto mod_hotkeys=g_mod_hotkeys;auto profile_state=g_profile_state_json;
        if(ParseFrameworkSettings(json,parsed,exports,hotkey,keyboard_hotkey,menu_engine,mod_engines,vr_head_tracking,replace_skyrim_inventory,replace_skyrim_barter,menu_display_mode,inventory_display_mode,barter_display_mode,menu_pause_world,inventory_pause_world,barter_pause_world,extra_menus,mod_hotkeys,profile_state,error)){
            if(json.is_object()&&!json.contains("menuDisplayMode")&&menu_engine=="ultralight"&&IsBrowserEngineBuilt(skyultra::BrowserEngine::CEF)){menu_engine="cef";}
            g_splash_settings=std::move(parsed);g_actor_export_settings=exports;g_skyultra_hotkey=std::move(hotkey);g_keyboard_hotkey=std::move(keyboard_hotkey);g_menu_browser_engine=std::move(menu_engine);g_mod_browser_engines=std::move(mod_engines);g_vr_head_tracking=vr_head_tracking;g_replace_skyrim_inventory=replace_skyrim_inventory;g_replace_skyrim_barter=replace_skyrim_barter;g_menu_display_mode=std::move(menu_display_mode);g_inventory_display_mode=std::move(inventory_display_mode);g_barter_display_mode=std::move(barter_display_mode);g_menu_pause_world=menu_pause_world;g_inventory_pause_world=inventory_pause_world;g_barter_pause_world=barter_pause_world;g_extra_menus=std::move(extra_menus);g_mod_hotkeys=std::move(mod_hotkeys);g_profile_state_json=std::move(profile_state);
        }
        else SKSE::log::warn("SkyUltra settings use defaults: {}",error);}
    if (const auto mods_dir = skyultra::GetInstalledWebModsDirectory(); !mods_dir.empty()) {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(mods_dir, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code dir_ec;
            if (!it->is_directory(dir_ec) || dir_ec) continue;
            auto folder_id = it->path().filename().string();
            for (auto& ch : folder_id) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            }
            if (folder_id == "skyultra.menu" || folder_id == "skyultra.ui_kit_test_mod" || folder_id == "skyultra.uikit" || !skyultra::IsValidModIdentifier(folder_id) || g_mod_hotkeys.contains(folder_id)) continue;
            std::ifstream manifest_stream(it->path() / L"manifest.json");
            if (!manifest_stream) continue;
            const auto manifest_json = nlohmann::json::parse(manifest_stream, nullptr, false);
            if (!manifest_json.is_object()) continue;
            if (const auto hk = manifest_json.find("hotkey"); hk != manifest_json.end() && hk->is_object()) {
                HotkeyBinding default_mod_hk{{},{},{}};
                std::string hk_err;
                if (ParseHotkeyBinding(*hk, default_mod_hk, true, hk_err)) {
                    g_mod_hotkeys.emplace(folder_id, std::move(default_mod_hk));
                }
            }
        }
    }
    skyultra::ConfigureVideoSplash(g_splash_settings);
    skyultra::ConfigureActorPreviewExport(g_actor_export_settings.full_view,g_actor_export_settings.portrait);
    skyultra::SetOverlaySpatialConfig(g_vr_head_tracking ? "atGaze" : "worldLocked", 1.35F, 1.0F, g_vr_head_tracking);
}
bool WriteFrameworkSettingsDisk(std::string& error)
{
    const auto persisted=FrameworkSettingsJson();
    const auto path=PluginDirectory()/L"SkyUltra.settings.json";const auto temporary=PluginDirectory()/L"SkyUltra.settings.json.tmp";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output){error="Could not write SkyUltra settings.";return false;}
        output<<persisted.dump(2);output.flush();if(!output){error="Could not finish writing SkyUltra settings.";return false;}}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Could not replace SkyUltra settings.";return false;}
    return true;
}
bool SaveFrameworkSettings(const nlohmann::json& input,std::string& error)
{
    auto next_runtime = g_runtime_settings;
    if (input.is_object() && input.contains("runtime") &&
        !skyultra::ParseRuntimeSettings(input["runtime"], g_runtime_settings, next_runtime, error)) return false;
    skyultra::VideoSplashSettings parsed=g_splash_settings;auto exports=g_actor_export_settings;auto hotkey=g_skyultra_hotkey;auto keyboard_hotkey=g_keyboard_hotkey;auto menu_engine=g_menu_browser_engine;auto mod_engines=g_mod_browser_engines;auto vr_head_tracking=g_vr_head_tracking;auto replace_skyrim_inventory=g_replace_skyrim_inventory;auto replace_skyrim_barter=g_replace_skyrim_barter;auto menu_display_mode=g_menu_display_mode;auto inventory_display_mode=g_inventory_display_mode;auto barter_display_mode=g_barter_display_mode;auto menu_pause_world=g_menu_pause_world;auto inventory_pause_world=g_inventory_pause_world;auto barter_pause_world=g_barter_pause_world;auto extra_menus=g_extra_menus;auto mod_hotkeys=g_mod_hotkeys;auto profile_state=g_profile_state_json;
    if(!ParseFrameworkSettings(input,parsed,exports,hotkey,keyboard_hotkey,menu_engine,mod_engines,vr_head_tracking,replace_skyrim_inventory,replace_skyrim_barter,menu_display_mode,inventory_display_mode,barter_display_mode,menu_pause_world,inventory_pause_world,barter_pause_world,extra_menus,mod_hotkeys,profile_state,error))return false;
    if (next_runtime != g_runtime_settings) SKSE::log::info("SkyUltra runtime settings saved; restart Skyrim to apply runtime options.");
    g_runtime_settings = std::move(next_runtime);
    const bool engine_changed = (menu_engine != g_menu_browser_engine);
    const auto previous_mod_engines = g_mod_browser_engines;
    g_splash_settings=std::move(parsed);g_actor_export_settings=exports;g_skyultra_hotkey=std::move(hotkey);g_keyboard_hotkey=std::move(keyboard_hotkey);g_menu_browser_engine=menu_engine;g_mod_browser_engines=mod_engines;g_vr_head_tracking=vr_head_tracking;g_replace_skyrim_inventory=replace_skyrim_inventory;g_replace_skyrim_barter=replace_skyrim_barter;g_menu_display_mode=std::move(menu_display_mode);g_inventory_display_mode=std::move(inventory_display_mode);g_barter_display_mode=std::move(barter_display_mode);g_menu_pause_world=menu_pause_world;g_inventory_pause_world=inventory_pause_world;g_barter_pause_world=barter_pause_world;g_extra_menus=std::move(extra_menus);g_mod_hotkeys=std::move(mod_hotkeys);g_profile_state_json=std::move(profile_state);
    if (!g_replace_skyrim_inventory && g_inventory_replacement_active) {
        g_inventory_replacement_active = false;
        if (g_view_manager != nullptr && g_menu_view_id != 0) {
            if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                (void)menu_view->SendEventJson("skyultra.inventoryMode",
                    nlohmann::json{ { "active", false } }.dump());
            }
        }
    }
    if (!g_replace_skyrim_barter && g_barter_replacement_active) {
        g_barter_replacement_active = false;
        if (g_view_manager != nullptr && g_menu_view_id != 0) {
            if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                (void)menu_view->SendEventJson("skyultra.barterMode",
                    nlohmann::json{ { "active", false } }.dump());
            }
        }
    }
    if ((!g_extra_menus.replace_magic && g_magic_replacement_active) ||
        (!g_extra_menus.replace_favorites && g_favorites_replacement_active) ||
        (!g_extra_menus.replace_container && g_container_replacement_active) ||
        (!g_extra_menus.replace_mannequin && g_mannequin_replacement_active) ||
        (!g_extra_menus.replace_loot && g_loot_replacement_active) ||
        (!g_extra_menus.replace_pickpocket && g_pickpocket_replacement_active)) {
        if (!g_extra_menus.replace_magic) g_magic_replacement_active = false;
        if (!g_extra_menus.replace_favorites) g_favorites_replacement_active = false;
        if (!g_extra_menus.replace_container) g_container_replacement_active = false;
        if (!g_extra_menus.replace_mannequin) g_mannequin_replacement_active = false;
        if (!g_extra_menus.replace_loot) g_loot_replacement_active = false;
        if (!g_extra_menus.replace_pickpocket) g_pickpocket_replacement_active = false;
        SendMenuReplacementModes();
    }
    SyncSkyUltraWorldPause();
    SyncVanillaCompassVisibility(true);
    if(!WriteFrameworkSettingsDisk(error))return false;
    skyultra::ConfigureVideoSplash(g_splash_settings);
    skyultra::ConfigureActorPreviewExport(exports.full_view,exports.portrait);
    skyultra::SetOverlaySpatialConfig(g_vr_head_tracking ? "atGaze" : "worldLocked", 1.35F, 1.0F, g_vr_head_tracking);
    if (engine_changed) {
        SwitchMenuBrowserEngine(g_menu_browser_engine == "cef"
            ? skyultra::BrowserEngine::CEF
            : skyultra::BrowserEngine::Ultralight);
    }
    for (const auto& [mod_id, eng] : g_mod_browser_engines) {
        const auto prev_it = previous_mod_engines.find(mod_id);
        if (prev_it == previous_mod_engines.end() || prev_it->second != eng) {
            SwitchModBrowserEngine(mod_id, eng == "cef"
                ? skyultra::BrowserEngine::CEF
                : skyultra::BrowserEngine::Ultralight);
        }
    }
    return true;
}

bool IsBrowserEngineBuilt(skyultra::BrowserEngine engine) noexcept
{
    switch (engine) {
    case skyultra::BrowserEngine::CEF:
#if defined(SKYULTRA_WITH_CEF)
        return true;
#else
        return false;
#endif
    case skyultra::BrowserEngine::Ultralight:
#if defined(SKYULTRA_WITH_ULTRALIGHT)
        return true;
#else
        return false;
#endif
    }
    return false;
}

skyultra::BrowserEngine DefaultBrowserEngine() noexcept
{
#if defined(SKYULTRA_DEFAULT_BACKEND_ULTRALIGHT) && !defined(SKYULTRA_DEFAULT_BACKEND_CEF)
    if (IsBrowserEngineBuilt(skyultra::BrowserEngine::Ultralight)) {
        return skyultra::BrowserEngine::Ultralight;
    }
#endif
    if (IsBrowserEngineBuilt(skyultra::BrowserEngine::CEF)) {
        return skyultra::BrowserEngine::CEF;
    }
    return skyultra::BrowserEngine::Ultralight;
}

void LoadRuntimeSettings()
{
    g_runtime_settings = skyultra::DefaultRuntimeSettings();
    std::ifstream input(PluginDirectory() / L"SkyUltra.settings.json");
    if (input) {
        const auto root = nlohmann::json::parse(input, nullptr, false);
        if (root.is_object() && root.contains("runtime")) {
            nlohmann::json parsed;
            std::string error;
            if (skyultra::ParseRuntimeSettings(root["runtime"], g_runtime_settings, parsed, error))
                g_runtime_settings = std::move(parsed);
            else SKSE::log::warn("SkyUltra runtime settings: {}; using defaults.", error);
        }
    }
    g_performance_settings.main_size = {g_runtime_settings["mainWidth"].get<std::uint32_t>(), g_runtime_settings["mainHeight"].get<std::uint32_t>()};
    g_performance_settings.popup_size = {g_runtime_settings["popupWidth"].get<std::uint32_t>(), g_runtime_settings["popupHeight"].get<std::uint32_t>()};
    g_performance_settings.main_fps = g_performance_settings.popup_fps = 0;
    g_browser_settings.render_mode = g_runtime_settings["renderMode"] == "cpu" ? BrowserRenderMode::Cpu : BrowserRenderMode::Gpu;
    if (REL::Module::IsVR()) g_browser_settings.render_mode = BrowserRenderMode::Gpu;
    g_browser_settings.cef_multithreaded = g_runtime_settings["cefMultithreaded"].get<bool>();
    g_mod_hot_reload_enabled = g_runtime_settings["hotReload"].get<bool>();
    SKSE::log::info("SkyUltra settings: JSON persistence; default browser pacing restored; renderer={}; CEF multithreaded={}; hot reload={}",
        g_browser_settings.render_mode == BrowserRenderMode::Gpu ? "GPU" : "CPU", g_browser_settings.cef_multithreaded, g_mod_hot_reload_enabled);
}

struct ScopedWinHandle final {
    explicit ScopedWinHandle(HANDLE handle_value) noexcept : value(handle_value) {}
    ~ScopedWinHandle()
    {
        if (value != nullptr && value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }
    ScopedWinHandle(const ScopedWinHandle&) = delete;
    ScopedWinHandle& operator=(const ScopedWinHandle&) = delete;

    HANDLE value{};
};

std::optional<std::string> ModIdFromChangedPath(std::wstring_view changed_path)
{
    const std::filesystem::path relative_path{ std::wstring(changed_path) };
    const auto filename = relative_path.filename().native();
    if (relative_path.empty() || relative_path.is_absolute() ||
        CompareStringOrdinal(filename.c_str(), static_cast<int>(filename.size()),
            L"manifest.json", 13, TRUE) == CSTR_EQUAL) {
        return std::nullopt;
    }

    auto component = relative_path.begin();
    if (component == relative_path.end()) {
        return std::nullopt;
    }
    auto mod_id = component->string();
    std::transform(mod_id.begin(), mod_id.end(), mod_id.begin(), [](unsigned char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') :
            static_cast<char>(value);
    });
    ++component;
    if (component == relative_path.end() || !skyultra::IsValidModIdentifier(mod_id)) {
        return std::nullopt;
    }

    for (const auto& part : relative_path) {
        const auto native = part.native();
        if ((!native.empty() && native.front() == L'.') ||
            CompareStringOrdinal(native.c_str(), static_cast<int>(native.size()),
                L"node_modules", 12, TRUE) == CSTR_EQUAL ||
            CompareStringOrdinal(native.c_str(), static_cast<int>(native.size()),
                L"dist", 4, TRUE) == CSTR_EQUAL) {
            return std::nullopt;
        }
    }
    return mod_id;
}

std::optional<SkyUltraModHandle> FindRegisteredMod(std::string_view mod_id)
{
    const std::scoped_lock lock(g_native_mods_mutex);
    const auto found = g_native_mod_handles_by_id.find(std::string(mod_id));
    return found == g_native_mod_handles_by_id.end() ? std::nullopt :
        std::optional<SkyUltraModHandle>(found->second);
}

double DevDiagnosticsNowMs() noexcept
{
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

std::string ModIdForViewHandle(SkyUltraViewHandle handle)
{
    SkyUltraModHandle mod_handle{};
    std::string fallback;
    {
        const std::scoped_lock lock(g_native_views_mutex);
        if (const auto view = g_native_views.find(handle); view != g_native_views.end()) {
            mod_handle = view->second.mod_handle;
            if (view->second.manager_id == g_menu_view_id && g_menu_view_id != 0) {
                fallback = "skyultra.menu";
            }
        }
    }
    if (mod_handle != 0) {
        const std::scoped_lock lock(g_native_mods_mutex);
        if (const auto mod = g_native_mods.find(mod_handle); mod != g_native_mods.end() &&
            !mod->second.manifest.id.empty()) {
            return mod->second.manifest.id;
        }
    }
    return fallback.empty() ? std::string("unregistered") : fallback;
}

void RecordDevBridgeEntry(
    std::uint32_t view_id,
    std::string mod_id,
    std::string_view kind,
    std::string_view method,
    std::string_view status,
    double duration_ms,
    std::size_t payload_bytes,
    std::string message = {})
{
    if (method == "ui.getDeveloperSnapshot") {
        return;
    }
    const auto now_ms = DevDiagnosticsNowMs();
    const std::scoped_lock lock(g_dev_diagnostics_mutex);
    if (!mod_id.empty()) {
        auto& stats = g_dev_mod_stats[mod_id];
        if (kind == "request" || kind == "permission-denied" || (kind == "error" && !method.empty())) {
            ++stats.bridge_requests;
        }
        if (kind == "permission-denied" || status == "denied") {
            ++stats.denied_permissions;
            ++stats.bridge_errors;
        } else if (status == "error") {
            ++stats.bridge_errors;
        }
    }
    if (kind == "event" && method == "skyultra.game.update") {
        std::size_t update_events = 0;
        for (const auto& entry : g_dev_bridge_log) {
            if (entry.kind == "event" && entry.method == "skyultra.game.update") {
                ++update_events;
            }
        }
        if (update_events >= 8) {
            for (auto r_it = g_dev_bridge_log.rbegin(); r_it != g_dev_bridge_log.rend(); ++r_it) {
                if (r_it->kind == "event" && r_it->method == "skyultra.game.update") {
                    g_dev_bridge_log.erase(std::next(r_it).base());
                    break;
                }
            }
        }
    }
    g_dev_bridge_log.push_front(DevBridgeLogEntry{
        g_next_dev_bridge_seq++,
        std::round(now_ms * 10.0) / 10.0,
        view_id,
        std::move(mod_id),
        std::string(kind),
        std::string(method),
        std::string(status),
        std::round(std::max(0.0, duration_ms) * 100.0) / 100.0,
        payload_bytes,
        std::move(message)
    });
    while (g_dev_bridge_log.size() > kMaxDevBridgeEntries) {
        g_dev_bridge_log.pop_back();
    }
}

void RecordDevModHotReload(std::string_view mod_id)
{
    g_hot_reload_total_count.fetch_add(1, std::memory_order_relaxed);
    const std::scoped_lock lock(g_dev_diagnostics_mutex);
    g_last_reloaded_mod.assign(mod_id);
    if (!mod_id.empty()) {
        ++g_dev_mod_stats[std::string(mod_id)].hot_reloads;
    }
}

void QueueHotReloadForMod(SkyUltraModHandle mod_handle, std::string_view mod_id)
{
    std::vector<SkyUltraViewHandle> views;
    {
        const std::scoped_lock lock(g_native_views_mutex);
        for (const auto& [handle, record] : g_native_views) {
            if (record.mod_handle == mod_handle && record.state == SKYULTRA_VIEW_READY) {
                views.push_back(handle);
            }
        }
    }
    for (const auto handle : views) {
        (void)NativeApiReloadView(handle);
    }
    if (!views.empty()) {
        RecordDevModHotReload(mod_id);
        if (!g_logged_first_mod_hot_reload.exchange(true, std::memory_order_relaxed)) {
            SKSE::log::info("SkyUltra queued its first in-game hot reload for mod '{}' ({} live views).",
                mod_id, views.size());
        } else {
            SKSE::log::debug("SkyUltra queued an in-game hot reload for mod '{}' ({} live views).",
                mod_id, views.size());
        }
    }
}

void RunModHotReloadWatcher(std::stop_token stop_token, std::filesystem::path mods_directory)
{
    ScopedWinHandle directory(CreateFileW(mods_directory.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));
    if (directory.value == INVALID_HANDLE_VALUE) {
        SKSE::log::warn("SkyUltra could not watch mod files for hot reload (Win32 error {}).",
            GetLastError());
        return;
    }

    ScopedWinHandle stop_event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    ScopedWinHandle change_event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (stop_event.value == nullptr || change_event.value == nullptr) {
        SKSE::log::warn("SkyUltra could not create mod hot-reload watcher events (Win32 error {}).",
            GetLastError());
        return;
    }
    const std::stop_callback signal_stop(stop_token, [event = stop_event.value] {
        SetEvent(event);
    });

    alignas(DWORD) std::array<std::byte, 64U * 1024U> buffer{};
    std::unordered_set<SkyUltraModHandle> pending_mods;
    std::unordered_map<SkyUltraModHandle, std::string> pending_mod_ids;
    std::optional<std::chrono::steady_clock::time_point> debounce_deadline;
    const std::array<HANDLE, 2> wait_handles{ stop_event.value, change_event.value };

    const auto flush_pending = [&] {
        for (const auto mod_handle : pending_mods) {
            const auto found = pending_mod_ids.find(mod_handle);
            if (found != pending_mod_ids.end()) {
                QueueHotReloadForMod(mod_handle, found->second);
            }
        }
        pending_mods.clear();
        pending_mod_ids.clear();
        debounce_deadline.reset();
    };

    while (!stop_token.stop_requested()) {
        OVERLAPPED operation{};
        operation.hEvent = change_event.value;
        ResetEvent(change_event.value);
        const auto read_started = ReadDirectoryChangesW(directory.value, buffer.data(),
            static_cast<DWORD>(buffer.size()), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            nullptr, &operation, nullptr);
        if (!read_started && GetLastError() != ERROR_IO_PENDING) {
            SKSE::log::warn("SkyUltra mod hot-reload file watching stopped (Win32 error {}).",
                GetLastError());
            return;
        }

        bool read_completed{};
        while (!stop_token.stop_requested() && !read_completed) {
            DWORD timeout = INFINITE;
            if (debounce_deadline) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    *debounce_deadline - std::chrono::steady_clock::now()).count();
                timeout = remaining <= 0 ? 0U : static_cast<DWORD>(remaining);
            }

            const auto wait_result = WaitForMultipleObjects(
                static_cast<DWORD>(wait_handles.size()), wait_handles.data(), FALSE, timeout);
            if (wait_result == WAIT_OBJECT_0) {
                CancelIoEx(directory.value, &operation);
                WaitForSingleObject(change_event.value, INFINITE);
                read_completed = true;
                break;
            }
            if (wait_result == WAIT_TIMEOUT) {
                flush_pending();
                continue;
            }
            if (wait_result != WAIT_OBJECT_0 + 1U) {
                SKSE::log::warn("SkyUltra mod hot-reload watcher wait failed (Win32 error {}).",
                    GetLastError());
                CancelIoEx(directory.value, &operation);
                WaitForSingleObject(change_event.value, INFINITE);
                return;
            }

            DWORD bytes_transferred{};
            if (!GetOverlappedResult(directory.value, &operation, &bytes_transferred, FALSE)) {
                const auto error = GetLastError();
                if (error != ERROR_OPERATION_ABORTED) {
                    SKSE::log::warn("SkyUltra mod hot-reload notification failed (Win32 error {}).", error);
                }
                read_completed = true;
                break;
            }
            read_completed = true;
            if (bytes_transferred == 0) {
                SKSE::log::warn("SkyUltra mod hot-reload notification buffer overflowed; save a page file again to reload it.");
                continue;
            }

            std::size_t offset{};
            while (offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) <= bytes_transferred) {
                const auto* notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                    buffer.data() + offset);
                const auto name_bytes = static_cast<std::size_t>(notification->FileNameLength);
                if ((name_bytes % sizeof(wchar_t)) != 0 ||
                    offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) + name_bytes > bytes_transferred) {
                    break;
                }
                const std::wstring_view name(notification->FileName, name_bytes / sizeof(wchar_t));
                if (const auto mod_id = ModIdFromChangedPath(name)) {
                    if (const auto mod_handle = FindRegisteredMod(*mod_id)) {
                        pending_mods.insert(*mod_handle);
                        pending_mod_ids.insert_or_assign(*mod_handle, *mod_id);
                        debounce_deadline = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(300);
                    }
                }
                if (notification->NextEntryOffset == 0) {
                    break;
                }
                if (notification->NextEntryOffset > bytes_transferred - offset) {
                    break;
                }
                offset += notification->NextEntryOffset;
            }
        }
    }
}

void StartModHotReloadWatcher()
{
    if (!g_mod_hot_reload_enabled || g_mod_hot_reload_thread.joinable()) {
        return;
    }
    const auto mods_directory = skyultra::GetInstalledWebModsDirectory();
    if (mods_directory.empty()) {
        SKSE::log::warn("SkyUltra could not resolve Data\\SkyUltra for mod hot reload.");
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(mods_directory, error);
    if (error) {
        SKSE::log::warn("SkyUltra could not create the mod directory for hot reload: {}.", error.message());
        return;
    }
    g_mod_hot_reload_thread = std::jthread([mods_directory](std::stop_token stop_token) {
        RunModHotReloadWatcher(stop_token, mods_directory);
    });
    SKSE::log::info("SkyUltra is watching manifest mod folders for in-game page hot reload.");
}

void StopModHotReloadWatcher()
{
    if (g_mod_hot_reload_thread.joinable()) {
        g_mod_hot_reload_thread.request_stop();
        g_mod_hot_reload_thread.join();
        SKSE::log::info("SkyUltra stopped watching manifest mod folders for hot reload.");
    }
}

std::uint64_t SubscriptionNowMs() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

void PumpGameDataSubscriptions(bool force_immediate)
{
    g_game_subscription_pump_queued.store(false, std::memory_order_release);
    if (g_game_subscriptions.empty() || g_view_manager == nullptr) {
        g_active_game_subscription_count.store(0, std::memory_order_relaxed);
        return;
    }

    static std::optional<bool> s_last_hud_suppressed;
    const bool hud_suppressed = ShouldSuppressHudOverlays();
    if (!s_last_hud_suppressed.has_value() || *s_last_hud_suppressed != hud_suppressed) {
        s_last_hud_suppressed = hud_suppressed;
        PublishOverlayViews();
        if (!hud_suppressed) {
            SyncVanillaCompassVisibility(true);
        }
    }

    if (hud_suppressed && g_browser_overlay_suppressed.load(std::memory_order_relaxed)) {
        for (auto& [handle, sub] : g_game_subscriptions) {
            (void)handle;
            sub.needs_catchup = true;
        }
        return;
    }

    const auto now_ms = SubscriptionNowMs();
    if (!hud_suppressed && g_extra_menus.replace_compass) {
        SyncVanillaCompassVisibility(false);
    }
    for (auto it = g_game_subscriptions.begin(); it != g_game_subscriptions.end();) {
        const auto handle = it->first;
        auto& sub = it->second;
        if (!NativeHandleAcceptsCommands(handle)) {
            it = g_game_subscriptions.erase(it);
            continue;
        }
        const auto view_id = NativeManagerViewId(handle);
        if (!view_id) {
            ++it;
            continue;
        }
        auto* view = g_view_manager->Find(*view_id);
        const auto* config = g_view_manager->Config(*view_id);
        if (view == nullptr || config == nullptr) {
            it = g_game_subscriptions.erase(it);
            continue;
        }
        const bool pinned_widget = std::ranges::any_of(g_widget_overlays, [&](const auto& item) {
            return item.second.manager_id == *view_id;
        });
        const bool is_hud_widget = pinned_widget || (config->layer == skyultra::ViewLayer::Hud && config->click_through);
        const bool effective_visible = config->visible &&
            (is_hud_widget ? !hud_suppressed : !g_browser_overlay_suppressed.load(std::memory_order_relaxed));
        if (!effective_visible) {
            sub.needs_catchup = true;
            ++it;
            continue;
        }
        if (!force_immediate && !sub.needs_catchup &&
            now_ms >= sub.last_push_ms && (now_ms - sub.last_push_ms) < sub.interval_ms) {
            ++it;
            continue;
        }
        const bool send_full = sub.needs_catchup;
        const auto update_json = skyultra::CollectSharedGameDataUpdateJSON(
            sub.compact, sub.topics, sub.topic_signatures, sub.last_sequence, send_full);
        sub.last_push_ms = now_ms;
        sub.needs_catchup = false;
        if (!update_json.empty()) {
            (void)view->SendEventJson("skyultra.game.update", update_json);
            RecordDevBridgeEntry(*view_id, ModIdForViewHandle(handle), "event",
                "skyultra.game.update", "ok", 0.1, update_json.size());
        }
        ++it;
    }
    g_active_game_subscription_count.store(
        static_cast<std::uint32_t>(g_game_subscriptions.size()), std::memory_order_relaxed);
}

void StartGameDataSubscriptionTicker()
{
    if (g_game_subscription_thread.joinable()) {
        return;
    }
    g_game_subscription_thread = std::jthread([](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (stop_token.stop_requested()) {
                break;
            }
            if (!g_game_session_active.load(std::memory_order_relaxed) ||
                g_active_game_subscription_count.load(std::memory_order_relaxed) == 0 ||
                skyultra::FrameworkSplashActive()) {
                continue;
            }
            bool expected = false;
            if (g_game_subscription_pump_queued.compare_exchange_strong(
                    expected, true, std::memory_order_acq_rel)) {
                if (!QueueNativeApiTask([] { PumpGameDataSubscriptions(false); })) {
                    g_game_subscription_pump_queued.store(false, std::memory_order_release);
                }
            }
        }
    });
}

const skyultra::ViewConfig* FindViewConfig(skyultra::ViewId id)
{
    return g_view_manager == nullptr ? nullptr : g_view_manager->Config(id);
}

std::optional<skyultra::PointF> GetViewLocalCursor(skyultra::ViewId id)
{
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    const auto* config = FindViewConfig(id);
    if (screen.width == 0 || screen.height == 0 || config == nullptr) {
        return std::nullopt;
    }
    const skyultra::SizeU viewport{ screen.width, screen.height };
    const auto bounds = skyultra::ResolveViewBounds(*config, viewport);
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return std::nullopt;
    }
    return skyultra::PointF{
        (static_cast<float>(g_browser_cursor.x) - bounds.x) * config->size.width / bounds.width,
        (static_cast<float>(g_browser_cursor.y) - bounds.y) * config->size.height / bounds.height,
    };
}

std::optional<skyultra::ViewId> GetFocusedView(skyultra::InputMode required_mode)
{
    if (g_view_manager == nullptr) {
        return std::nullopt;
    }
    if (REL::Module::IsVR()) {
        const auto vr_focus = skyultra::GetVRKeyboardFocus();
        if (vr_focus != 0 && g_view_manager->Find(vr_focus) != nullptr) {
            return vr_focus;
        }
    }
    const auto owner = g_view_manager->FocusOwner();
    if (!owner) {
        return std::nullopt;
    }
    const auto mode = g_view_manager->FocusMode(*owner);
    if (!mode || (*mode != skyultra::InputMode::All && *mode != required_mode &&
        !(*mode == skyultra::InputMode::MouseAndKeyboard &&
          (required_mode == skyultra::InputMode::Mouse || required_mode == skyultra::InputMode::Keyboard)))) {
        return std::nullopt;
    }
    return owner;
}

skyultra::InputMode InputFocusForView(
    skyultra::ViewId id, skyultra::InputMode fallback)
{
    const auto* config = FindViewConfig(id);
    return config != nullptr && config->enable_gamepad ? skyultra::InputMode::All : fallback;
}

std::optional<std::uint32_t> GamepadButtonVirtualKey(std::uint32_t id) noexcept
{
    switch (id) {
    case RE::BSWin32GamepadDevice::Keys::kUp: return VK_UP;
    case RE::BSWin32GamepadDevice::Keys::kDown: return VK_DOWN;
    case RE::BSWin32GamepadDevice::Keys::kLeft: return VK_LEFT;
    case RE::BSWin32GamepadDevice::Keys::kRight: return VK_RIGHT;
    case RE::BSWin32GamepadDevice::Keys::kA: return VK_RETURN;
    case RE::BSWin32GamepadDevice::Keys::kB: return VK_ESCAPE;
    case RE::BSWin32GamepadDevice::Keys::kX: return VK_TAB;
    case RE::BSWin32GamepadDevice::Keys::kY: return VK_SPACE;
    case RE::BSWin32GamepadDevice::Keys::kLeftShoulder: return VK_PRIOR;
    case RE::BSWin32GamepadDevice::Keys::kRightShoulder: return VK_NEXT;
    default: return std::nullopt;
    }
}

void SendGamepadKey(skyultra::ViewId id, std::uint32_t virtual_key, bool pressed)
{
    if (g_view_manager == nullptr) {
        return;
    }
    if (auto* view = g_view_manager->Find(id)) {
        skyultra::KeyInput input;
        input.virtual_key = virtual_key;
        input.scan_code = MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC_EX);
        input.pressed = pressed;
        view->SendKey(input);
    }
}

void ReleaseGamepadStickKeys()
{
    if (g_gamepad_stick_owner) {
        for (const auto& [mask, virtual_key] : kGamepadStickVirtualKeys) {
            if ((g_gamepad_stick_keys & mask) != 0) {
                SendGamepadKey(*g_gamepad_stick_owner, virtual_key, false);
            }
        }
    }
    g_gamepad_stick_keys = 0;
    g_gamepad_stick_owner.reset();
}

bool HasVisibleInteractiveView();
void PublishBrowserCursor(bool visible);

bool UpdateGamepadStick(const RE::ThumbstickEvent& event)
{
    if (event.GetDevice() != RE::INPUT_DEVICE::kGamepad) {
        return false;
    }
    const auto stick_id = event.GetIDCode();
    if (stick_id != RE::BSWin32GamepadDevice::Keys::kLeftStick &&
        stick_id != RE::BSWin32GamepadDevice::Keys::kRightStick) {
        return false;
    }

    const auto focused = GetFocusedView(skyultra::InputMode::Gamepad);
    const auto* config = focused ? FindViewConfig(*focused) : nullptr;
    const auto target = config != nullptr && config->enable_gamepad ? focused :
        (HasVisibleInteractiveView() && g_menu_view_id != 0 ? std::optional<skyultra::ViewId>(g_menu_view_id) : std::nullopt);
    if (stick_id == RE::BSWin32GamepadDevice::Keys::kLeftStick && g_gamepad_stick_owner != target) {
        ReleaseGamepadStickKeys();
        g_gamepad_stick_owner = target;
    }
    if (!target) {
        return false;
    }

    const float magnitude = std::hypot(event.xValue, event.yValue);
    if (magnitude > 0.16F && g_view_manager != nullptr) {
        const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
        if (screen.width > 0 && screen.height > 0) {
            const float speed = stick_id == RE::BSWin32GamepadDevice::Keys::kRightStick ? 18.0F : 14.0F;
            const auto next_x = static_cast<std::int64_t>(std::lround(static_cast<float>(g_browser_cursor.x) + event.xValue * speed));
            const auto next_y = static_cast<std::int64_t>(std::lround(static_cast<float>(g_browser_cursor.y) - event.yValue * speed));
            g_browser_cursor.x = static_cast<LONG>(std::clamp<std::int64_t>(next_x, 0, screen.width - 1));
            g_browser_cursor.y = static_cast<LONG>(std::clamp<std::int64_t>(next_y, 0, screen.height - 1));
            PublishBrowserCursor(true);
            if (auto* view = g_view_manager->Find(*target)) {
                skyultra::MouseInput move{};
                move.position = { static_cast<float>(g_browser_cursor.x), static_cast<float>(g_browser_cursor.y) };
                move.moved = true;
                view->SendMouse(move);
                (void)view->SendEventJson("skyultra.gamepadCursor", nlohmann::json{
                    { "dx", event.xValue },
                    { "dy", -event.yValue },
                    { "x", g_browser_cursor.x },
                    { "y", g_browser_cursor.y }
                }.dump());
            }
        }
    }

    if (stick_id != RE::BSWin32GamepadDevice::Keys::kLeftStick) {
        return true;
    }

    constexpr float kPressThreshold = 0.55F;
    std::uint8_t desired{};
    if (event.yValue >= kPressThreshold) desired |= 1U << 0U;
    if (event.yValue <= -kPressThreshold) desired |= 1U << 1U;
    if (event.xValue <= -kPressThreshold) desired |= 1U << 2U;
    if (event.xValue >= kPressThreshold) desired |= 1U << 3U;

    for (const auto& [mask, virtual_key] : kGamepadStickVirtualKeys) {
        const bool was_pressed = (g_gamepad_stick_keys & mask) != 0;
        const bool is_pressed = (desired & mask) != 0;
        if (was_pressed != is_pressed) {
            SendGamepadKey(*target, virtual_key, is_pressed);
        }
    }
    g_gamepad_stick_keys = desired;
    return true;
}

skyultra::HitTestResult HitTestBrowserCursor()
{
    if (g_view_manager == nullptr) {
        return {};
    }
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    if (screen.width == 0 || screen.height == 0) {
        return {};
    }
    const skyultra::SizeU viewport{ screen.width, screen.height };
    return g_view_manager->HitTest(
        { static_cast<float>(g_browser_cursor.x), static_cast<float>(g_browser_cursor.y) }, viewport);
}

bool HasVisibleInteractiveView()
{
    if (g_browser_overlay_suppressed.load(std::memory_order_relaxed) || g_view_manager == nullptr) {
        return false;
    }
    for (const auto& snapshot : g_view_manager->OrderedViews()) {
        if (!snapshot.config.visible || snapshot.config.click_through ||
            snapshot.config.layer == skyultra::ViewLayer::Hud) {
            continue;
        }
        const bool pinned_widget = std::ranges::any_of(g_widget_overlays,
            [&](const auto& item) { return item.second.manager_id == snapshot.id; });
        if (!pinned_widget) {
            return true;
        }
    }
    const std::scoped_lock lock(g_native_views_mutex);
    for (const auto& [handle, record] : g_native_views) {
        (void)handle;
        if (record.state == SKYULTRA_VIEW_PENDING && record.pending_config &&
            record.pending_config->visible && !record.pending_config->click_through &&
            record.pending_config->layer != skyultra::ViewLayer::Hud) {
            return true;
        }
    }
    return false;
}

void SyncSkyUltraWorldPause() noexcept
{
    auto* ui = RE::UI::GetSingleton();
    if (ui == nullptr) {
        return;
    }
    const bool browser_open = HasVisibleInteractiveView();
    const bool pause_pref = g_barter_replacement_active ? g_barter_pause_world :
        g_magic_replacement_active ? g_extra_menus.magic_pause_world :
        g_favorites_replacement_active ? g_extra_menus.favorites_pause_world :
        g_container_replacement_active ? g_extra_menus.container_pause_world :
        g_mannequin_replacement_active ? g_extra_menus.mannequin_pause_world :
        g_loot_replacement_active ? g_extra_menus.loot_pause_world :
        g_pickpocket_replacement_active ? g_extra_menus.pickpocket_pause_world :
        (g_inventory_replacement_active ? g_inventory_pause_world : g_menu_pause_world);
    const bool should_pause = browser_open && pause_pref;
    if (should_pause && !g_skyultra_world_paused) {
        ++ui->numPausesGame;
        g_skyultra_world_paused = true;
    } else if (!should_pause && g_skyultra_world_paused) {
        if (ui->numPausesGame > 0) {
            --ui->numPausesGame;
        }
        g_skyultra_world_paused = false;
    }
}

bool IsModalOrBlockingMenuOpen(RE::UI* ui) noexcept
{
    if (ui == nullptr) {
        return true;
    }
    if (!g_game_session_active.load(std::memory_order_relaxed)) {
        return true;
    }
    if (ui->IsModalMenuOpen() ||
        ui->IsMenuOpen("Main Menu"sv) ||
        ui->IsMenuOpen("Loading Menu"sv) ||
        ui->IsMenuOpen("Mist Menu"sv) ||
        ui->IsMenuOpen("Fader Menu"sv) ||
        ui->IsMenuOpen("RaceSex Menu"sv) ||
        ui->IsMenuOpen("MapMenu"sv) ||
        ui->IsMenuOpen("Journal Menu"sv) ||
        ui->IsMenuOpen("TweenMenu"sv) ||
        ui->IsMenuOpen("StatsMenu"sv) ||
        ui->IsMenuOpen("Sleep/Wait Menu"sv) ||
        ui->IsMenuOpen("Lockpicking Menu"sv) ||
        ui->IsMenuOpen("GiftMenu"sv) ||
        ui->IsMenuOpen("Book Menu"sv) ||
        ui->IsMenuOpen("Crafting Menu"sv) ||
        (!g_container_replacement_active && !g_mannequin_replacement_active &&
         !g_loot_replacement_active && !g_pickpocket_replacement_active && ui->IsMenuOpen("ContainerMenu"sv)) ||
        (!g_replace_skyrim_barter && !g_barter_replacement_active && ui->IsMenuOpen("BarterMenu"sv)) ||
        (!g_replace_skyrim_inventory && !g_inventory_replacement_active && ui->IsMenuOpen("InventoryMenu"sv)) ||
        (!g_extra_menus.replace_magic && !g_magic_replacement_active && ui->IsMenuOpen("MagicMenu"sv))) {
        return true;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr || player->GetParentCell() == nullptr) {
        return true;
    }
    return false;
}

bool ShouldSuppressHudOverlays() noexcept
{
    auto* ui = RE::UI::GetSingleton();
    if (IsModalOrBlockingMenuOpen(ui)) {
        return true;
    }
    if (AnyMenuReplacementActive() || HasVisibleInteractiveView()) {
        return true;
    }
    if (ui->GameIsPaused() ||
        ui->IsMenuOpen("InventoryMenu"sv) ||
        ui->IsMenuOpen("BarterMenu"sv) ||
        ui->IsMenuOpen("MagicMenu"sv) ||
        ui->IsMenuOpen("ContainerMenu"sv) ||
        ui->IsMenuOpen("FavoritesMenu"sv) ||
        ui->IsMenuOpen("Dialogue Menu"sv) ||
        ui->IsMenuOpen("Console"sv) ||
        ui->IsMenuOpen("MessageBoxMenu"sv) ||
        ui->IsMenuOpen("Training Menu"sv) ||
        ui->IsMenuOpen("LevelUp Menu"sv) ||
        ui->IsMenuOpen("Credits Menu"sv) ||
        ui->IsMenuOpen("TitleSequence Menu"sv) ||
        ui->IsMenuOpen("LoadWaitSpinner"sv)) {
        return true;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr || !player->Is3DLoaded()) {
        return true;
    }
    auto* parent_cell = player->GetParentCell();
    if (parent_cell == nullptr || !parent_cell->IsAttached()) {
        return true;
    }
    if (!REL::Module::IsVR()) {
        const auto& rt = player->GetPlayerRuntimeData();
        if (rt.queuedTargetLoc.isValid ||
            rt.playerFlags.isLoading ||
            rt.playerFlags.travelUseDoor ||
            rt.playerFlags.fastTraveling) {
            return true;
        }
    }
    return false;
}

void SyncVanillaCompassVisibility(bool force) noexcept
{
    static std::optional<bool> s_last_applied_show_vanilla;
    auto* ui = RE::UI::GetSingleton();
    if (ui == nullptr || ShouldSuppressHudOverlays()) {
        return;
    }
    const bool show_vanilla = !g_extra_menus.replace_compass;
    if (!force && s_last_applied_show_vanilla.has_value() && *s_last_applied_show_vanilla == show_vanilla) {
        return;
    }
    auto hud_menu = ui->GetMenu("HUD Menu"sv);
    if (!hud_menu) {
        hud_menu = ui->GetMenu("HUDMenu"sv);
    }
    if (!hud_menu || !hud_menu->uiMovie) {
        return;
    }
    const double alpha = show_vanilla ? 100.0 : 0.0;
    hud_menu->uiMovie->SetVariable("_root.HUDMovieBaseInstance.CompassShoutMeterHolder.Compass._visible", RE::GFxValue(show_vanilla));
    hud_menu->uiMovie->SetVariable("_root.HUDMovieBaseInstance.CompassShoutMeterHolder.Compass._alpha", RE::GFxValue(alpha));
    hud_menu->uiMovie->SetVariable("_root.HUDMovieBaseInstance.CompassShoutMeterHolder._visible", RE::GFxValue(show_vanilla));
    hud_menu->uiMovie->SetVariable("_root.HUDMovieBaseInstance.CompassShoutMeterHolder._alpha", RE::GFxValue(alpha));
    s_last_applied_show_vanilla = show_vanilla;
}

void PublishOverlayViews()
{
    if (g_view_manager == nullptr) {
        skyultra::SetOverlayViews({});
        skyultra::SetOverlayVisible(false);
        skyultra::SetD3D11UiPassMenuVisible(false);
        return;
    }

    const bool suppress_hud_widgets = ShouldSuppressHudOverlays();
    std::vector<skyultra::OverlayView> overlay_views;
    bool any_visible{};
    bool any_interactive_visible{};
    for (const auto& snapshot : g_view_manager->OrderedViews()) {
        auto config = snapshot.config;
        const bool pinned_widget = std::ranges::any_of(g_widget_overlays, [&](const auto& item) { return item.second.manager_id == snapshot.id; });
        const bool is_hud_widget = pinned_widget || (config.layer == skyultra::ViewLayer::Hud && config.click_through);
        if (is_hud_widget) {
            if (suppress_hud_widgets) config.visible = false;
        } else if (g_browser_overlay_suppressed.load(std::memory_order_relaxed)) {
            config.visible = false;
        }
        overlay_views.push_back({ snapshot.view, config });
        any_visible = any_visible || config.visible;
        if (config.visible && !config.click_through && config.layer != skyultra::ViewLayer::Hud && !pinned_widget) {
            any_interactive_visible = true;
        }
    }
    {
        const std::scoped_lock lock(g_native_views_mutex);
        for (const auto& [handle, record] : g_native_views) {
            (void)handle;
            if (record.state == SKYULTRA_VIEW_PENDING && record.pending_config) {
                auto config = *record.pending_config;
                const bool pending_hud_widget = config.layer == skyultra::ViewLayer::Hud && config.click_through;
                if (pending_hud_widget ? suppress_hud_widgets : g_browser_overlay_suppressed.load(std::memory_order_relaxed)) {
                    config.visible = false;
                }
                overlay_views.push_back({ {}, config });
                any_visible = any_visible || config.visible;
                if (config.visible && !config.click_through && config.layer != skyultra::ViewLayer::Hud) {
                    any_interactive_visible = true;
                }
            }
        }
    }
    skyultra::SetOverlayViews(std::move(overlay_views));
    const bool overlay_visible = any_visible;
    skyultra::SetOverlayVisible(overlay_visible);
    // HUDs need the engine's current UI target too. Present can select a
    // different swap-chain surface and leave displaced copies during motion.
    // The carrier has no menu-blocking/cursor-preservation flags; opening a
    // Skyrim menu still suppresses the HUD and queues the carrier's hide.
    skyultra::SetD3D11UiPassMenuVisible(overlay_visible);
    if (!any_interactive_visible) {
        skyultra::SetOverlayCursor({}, {}, false);
    }
}

std::atomic_bool g_cached_cursor_visible{ true };

void PublishBrowserCursor(bool visible)
{
    if (!HasVisibleInteractiveView()) {
        visible = false;
    }
    std::uint32_t accent=0xD8C49CU;
    if(visible){const auto hit=HitTestBrowserCursor();const auto target=hit.target?hit.target:GetFocusedView(skyultra::InputMode::Mouse);
        if(target){const std::scoped_lock lock(g_native_views_mutex);for(const auto& [handle,record]:g_native_views){(void)handle;if(record.manager_id==target){visible=!record.hide_native_cursor;accent=record.cursor_accent;break;}}}}
    g_cached_cursor_visible.store(visible, std::memory_order_relaxed);
    skyultra::SetOverlayCursorAccent(accent);
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    skyultra::SetOverlayCursor({ static_cast<float>(g_browser_cursor.x),
                                  static_cast<float>(g_browser_cursor.y) },
        { screen.width, screen.height }, visible);
}

void UpdateBrowserCursor(const RE::MouseMoveEvent& event)
{
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    if (screen.width == 0 || screen.height == 0) {
        return;
    }
    const auto x = static_cast<std::int64_t>(g_browser_cursor.x) + event.mouseInputX;
    const auto y = static_cast<std::int64_t>(g_browser_cursor.y) + event.mouseInputY;
    g_browser_cursor.x = static_cast<LONG>(std::clamp<std::int64_t>(x, 0, screen.width - 1));
    g_browser_cursor.y = static_cast<LONG>(std::clamp<std::int64_t>(y, 0, screen.height - 1));
    skyultra::SetOverlayCursor({ static_cast<float>(g_browser_cursor.x),
                                  static_cast<float>(g_browser_cursor.y) },
        { screen.width, screen.height }, g_cached_cursor_visible.load(std::memory_order_relaxed));
}

std::uint32_t CurrentModifiers()
{
    std::uint32_t modifiers{};
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= skyultra::kModifierShift;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= skyultra::kModifierControl;
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) modifiers |= skyultra::kModifierAlt;
    return modifiers;
}

int VRControllerHand(RE::INPUT_DEVICE device) noexcept
{
#if defined(ENABLE_SKYRIM_VR)
    switch (device) {
    case RE::INPUT_DEVICE::kViveSecondary:
    case RE::INPUT_DEVICE::kOculusSecondary:
    case RE::INPUT_DEVICE::kWMRSecondary:
        return 0;  // left hand
    case RE::INPUT_DEVICE::kVivePrimary:
    case RE::INPUT_DEVICE::kOculusPrimary:
    case RE::INPUT_DEVICE::kWMRPrimary:
        return 1;  // right hand
    default:
        return -1;
    }
#else
    (void)device;
    return -1;
#endif
}

void BeginSplashForView(skyultra::ViewId view)
{
    if(REL::Module::IsVR()||!g_view_manager)return;
    const auto* config=g_view_manager->Config(view);
    if(!config||config->click_through||config->layer==skyultra::ViewLayer::Hud)return;
    std::string mod_id=config->id;
    {const std::scoped_lock views_lock(g_native_views_mutex);for(const auto& [handle,record]:g_native_views){(void)handle;
        if(record.manager_id!=view)continue;const std::scoped_lock mods_lock(g_native_mods_mutex);
        if(const auto mod=g_native_mods.find(record.mod_handle);mod!=g_native_mods.end())mod_id=mod->second.manifest.id;break;}}
    skyultra::BeginFrameworkSplash(config->id,std::move(mod_id),!skyultra::OverlayPageAwaitingReady(view));
}

std::unordered_set<std::uint32_t> g_held_keyboard_scans;
std::unordered_set<std::uint32_t> g_held_gamepad_buttons;
std::unordered_set<std::uint32_t> g_held_vr_buttons;
std::unordered_map<std::uint64_t, std::uint32_t> g_button_repeat_steps;
std::chrono::steady_clock::time_point g_last_hotkey_toggle_time{};

enum class BrowserOpenTarget { MainMenu, CurrentPanels, RestoreSuspended };

void ApplyBrowserPanelVisibility(skyultra::PanelVisibilityAction action,
    std::span<const skyultra::ViewId> selected = {}, SkyUltraModHandle selected_mod = 0,
    bool remember_for_modal = false)
{
    if (action != skyultra::PanelVisibilityAction::Restore) g_suspended_panel_views.clear();
    ReleaseGamepadStickKeys();
    for (const auto& snapshot : g_view_manager->OrderedViews()) {
        const bool pinned = std::ranges::any_of(g_widget_overlays,
            [&](const auto& entry) { return entry.second.manager_id == snapshot.id; });
        const bool managed = !pinned && snapshot.config.layer != skyultra::ViewLayer::Hud &&
            snapshot.id != g_window_test_view_id;
        if (remember_for_modal && managed && snapshot.config.visible) g_suspended_panel_views.insert(snapshot.id);
        const bool next_visible = skyultra::ResolvePanelVisibility(action,
            {snapshot.id, snapshot.config.visible, managed, g_suspended_panel_views.contains(snapshot.id)}, selected);
        // The built-in page receives its mode events before it is shown below.
        if (next_visible && snapshot.id == g_menu_view_id) continue;
        if (next_visible != snapshot.config.visible) {
            (void)g_view_manager->SetVisible(snapshot.id, next_visible);
            if (!next_visible) skyultra::CancelFrameworkSplash(snapshot.config.id);
        }
    }
    if (action == skyultra::PanelVisibilityAction::Restore) g_suspended_panel_views.clear();
    if (action == skyultra::PanelVisibilityAction::Select ||
        (action == skyultra::PanelVisibilityAction::Hide && !remember_for_modal)) {
        // A deferred creation must not reopen a panel after the user selected a
        // different menu. Explicitly hidden creations never become restore entries.
        const std::scoped_lock lock(g_native_views_mutex);
        for (auto& [handle, record] : g_native_views) {
            (void)handle;
            if (record.pending_config && record.pending_config->layer != skyultra::ViewLayer::Hud &&
                (selected_mod == 0 || record.mod_handle != selected_mod)) record.pending_config->visible = false;
        }
    }
    for (auto& capture : g_mouse_capture) capture.reset();
}

void SetBrowserVisible(bool visible, const char* reason = "Shift+F6",
    BrowserOpenTarget target = BrowserOpenTarget::MainMenu)
{
    if (g_view_manager == nullptr) {
        return;
    }
    if (visible && !REL::Module::IsVR()) {
        const auto hook_ready = skyultra::InstallD3D11PresentHook();
        g_render_hook_installed = hook_ready;
        if (!hook_ready) {
            SKSE::log::error(
                "SkyUltra could not attach to Skyrim's D3D11 swap chain while handling {}.", reason);
            return;
        }
    }
    g_held_keyboard_scans.clear();
    g_button_repeat_steps.clear();
    const bool restore_main_menu = visible && target == BrowserOpenTarget::RestoreSuspended &&
        g_suspended_panel_views.contains(g_menu_view_id);
    if (visible && target == BrowserOpenTarget::MainMenu) {
        const std::array<skyultra::ViewId, 1> selected{g_menu_view_id};
        ApplyBrowserPanelVisibility(skyultra::PanelVisibilityAction::Select, selected);
    } else if (visible && target == BrowserOpenTarget::RestoreSuspended) {
        ApplyBrowserPanelVisibility(skyultra::PanelVisibilityAction::Restore);
    } else if (!visible) {
        ApplyBrowserPanelVisibility(skyultra::PanelVisibilityAction::Hide, {}, 0,
            target == BrowserOpenTarget::RestoreSuspended);
    }
    const auto ordered_views = g_view_manager->OrderedViews();
    const auto* main_config = g_view_manager->Config(g_menu_view_id);
    const bool show_main_menu = visible && (target == BrowserOpenTarget::MainMenu || restore_main_menu ||
        (main_config && main_config->visible));
    const bool has_visible_mod_view = visible &&
        std::ranges::any_of(ordered_views, [](const auto& snapshot) {
            return snapshot.id != g_menu_view_id && snapshot.id != g_window_test_view_id &&
                snapshot.config.visible && !snapshot.config.click_through &&
                snapshot.config.layer != skyultra::ViewLayer::Hud && !std::ranges::any_of(g_widget_overlays,
                    [&](const auto& entry) { return entry.second.manager_id == snapshot.id; });
        });
    if (visible) {
        if (show_main_menu && !g_inventory_replacement_active && !g_barter_replacement_active &&
            !g_magic_replacement_active && !g_favorites_replacement_active &&
            !g_container_replacement_active && !g_mannequin_replacement_active &&
            !g_loot_replacement_active && !g_pickpocket_replacement_active &&
            !g_virtual_keyboard_opened_menu) {
            BeginSplashForView(g_menu_view_id);
        } else if (has_visible_mod_view) {
            for (auto view = ordered_views.rbegin(); view != ordered_views.rend(); ++view) {
                if (view->config.visible && !view->config.click_through && view->config.layer != skyultra::ViewLayer::Hud) {
                    BeginSplashForView(view->id);
                    break;
                }
            }
        }
        if (show_main_menu && g_menu_view_id != 0) {
            if (!skyultra::FrameworkSplashActive()) {
                skyultra::BeginOverlayPageLoad(g_menu_view_id);
            }
            if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                // Queue mode events before SetVisible(true) so React switches views before CEF's first paint.
                (void)menu_view->SendEventJson("skyultra.visibility",
                    nlohmann::json{ { "visible", true } }.dump());
                (void)menu_view->SendEventJson("skyultra.virtualKeyboard",
                    nlohmann::json{ { "open", g_virtual_keyboard_open },
                                    { "standalone", g_virtual_keyboard_open && g_virtual_keyboard_opened_menu } }.dump());
                (void)menu_view->SendEventJson("skyultra.inventoryMode",
                    nlohmann::json{ { "active", g_inventory_replacement_active } }.dump());
                (void)menu_view->SendEventJson("skyultra.barterMode",
                    nlohmann::json{ { "active", g_barter_replacement_active } }.dump());
            }
            SendMenuReplacementModes();
            if (!g_view_manager->SetVisible(g_menu_view_id, true)) {
                SKSE::log::error("SkyUltra could not change visibility for its main menu.");
                return;
            }
        }
        ReleaseGamepadStickKeys();
        const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
        g_browser_cursor.x = static_cast<LONG>(screen.width / 2U);
        g_browser_cursor.y = static_cast<LONG>(screen.height / 2U);
    } else {
        if (g_inventory_replacement_active) {
            g_inventory_replacement_active = false;
            if (g_menu_view_id != 0) {
                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                    (void)menu_view->SendEventJson("skyultra.inventoryMode",
                        nlohmann::json{ { "active", false } }.dump());
                }
            }
        }
        if (g_barter_replacement_active) {
            g_barter_replacement_active = false;
            if (g_menu_view_id != 0) {
                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                    (void)menu_view->SendEventJson("skyultra.barterMode",
                        nlohmann::json{ { "active", false } }.dump());
                }
            }
        }
        if (g_magic_replacement_active || g_favorites_replacement_active ||
            g_container_replacement_active || g_mannequin_replacement_active ||
            g_loot_replacement_active || g_pickpocket_replacement_active) {
            g_magic_replacement_active = false;
            g_favorites_replacement_active = false;
            g_container_replacement_active = false;
            g_mannequin_replacement_active = false;
            g_loot_replacement_active = false;
            g_pickpocket_replacement_active = false;
            SendMenuReplacementModes();
        }
        if (g_virtual_keyboard_open || g_virtual_keyboard_opened_menu) {
            g_virtual_keyboard_open = false;
            g_virtual_keyboard_opened_menu = false;
            if (g_menu_view_id != 0) {
                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                    (void)menu_view->SendEventJson("skyultra.virtualKeyboard",
                        nlohmann::json{ { "open", false }, { "standalone", false } }.dump());
                }
            }
        }
        if (!has_visible_mod_view && g_menu_view_id != 0) {
            if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                (void)menu_view->SendEventJson("skyultra.visibility",
                    nlohmann::json{ { "visible", false } }.dump());
            }
            if (!g_view_manager->SetVisible(g_menu_view_id, false)) {
                SKSE::log::error("SkyUltra could not change visibility for its main menu.");
                return;
            }
        }
        g_virtual_keyboard_open = false;
        g_virtual_keyboard_opened_menu = false;
        skyultra::CancelFrameworkSplash();
        ReleaseGamepadStickKeys();
        for (auto& capture : g_mouse_capture) {
            capture.reset();
        }
    }
    g_browser_overlay_suppressed.store(!visible, std::memory_order_relaxed);
    // Do not focus the main WebView just because the overlay opened. It can be
    // expensive on the first focus, and the first key should go to the view the
    // player actually clicks (including a popup on a different browser engine).
    if (!visible) {
        for (const auto& snapshot : g_view_manager->OrderedViews()) {
            g_view_manager->ReleaseFocus(snapshot.id);
        }
    }
    PublishOverlayViews();
    SyncSkyUltraWorldPause();
    PublishBrowserCursor(visible);
    if (visible && !g_game_subscriptions.empty()) {
        if (!QueueNativeApiTask([] { PumpGameDataSubscriptions(true); })) {
            PumpGameDataSubscriptions(true);
        }
    }
    SKSE::log::info("SkyUltra browser {} ({}).", visible ? "shown" : "hidden", reason);
}

void OpenSkyUltraInventoryReplacement()
{
    if (g_view_manager == nullptr || g_menu_view_id == 0) {
        return;
    }
    g_inventory_replacement_active = true;
    g_barter_replacement_active = false;
    g_magic_replacement_active = false;
    g_favorites_replacement_active = false;
    g_container_replacement_active = false;
    g_mannequin_replacement_active = false;
    g_loot_replacement_active = false;
    g_pickpocket_replacement_active = false;
    g_restore_browser_after_menu = false;
    g_modal_menu_open = false;
    if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
        (void)menu_view->SendEventJson("skyultra.barterMode",
            nlohmann::json{ { "active", false } }.dump());
        SendMenuReplacementModes();
        (void)menu_view->SendEventJson("skyultra.inventoryMode",
            nlohmann::json{ { "active", true } }.dump());
    }
    SetBrowserVisible(true, "Skyrim InventoryMenu replacement");
}

void OpenSkyUltraBarterReplacement()
{
    if (g_view_manager == nullptr || g_menu_view_id == 0) {
        return;
    }
    g_barter_replacement_active = true;
    g_inventory_replacement_active = false;
    g_magic_replacement_active = false;
    g_favorites_replacement_active = false;
    g_container_replacement_active = false;
    g_mannequin_replacement_active = false;
    g_loot_replacement_active = false;
    g_pickpocket_replacement_active = false;
    g_restore_browser_after_menu = false;
    g_modal_menu_open = false;
    if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
        (void)menu_view->SendEventJson("skyultra.inventoryMode",
            nlohmann::json{ { "active", false } }.dump());
        SendMenuReplacementModes();
        (void)menu_view->SendEventJson("skyultra.barterMode",
            nlohmann::json{ { "active", true } }.dump());
    }
    SetBrowserVisible(true, "Skyrim BarterMenu replacement");
}

// Publishes the Magic / Favorites / Container / Mannequin / Loot / Pickpocket replacement state to the built-in menu page.
void SendMenuReplacementModes()
{
    if (g_view_manager == nullptr || g_menu_view_id == 0) {
        return;
    }
    if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
        (void)menu_view->SendEventJson("skyultra.magicMode",
            nlohmann::json{ { "active", g_magic_replacement_active } }.dump());
        (void)menu_view->SendEventJson("skyultra.favoritesMode",
            nlohmann::json{ { "active", g_favorites_replacement_active } }.dump());
        (void)menu_view->SendEventJson("skyultra.containerMode",
            nlohmann::json{ { "active", g_container_replacement_active } }.dump());
        (void)menu_view->SendEventJson("skyultra.mannequinMode",
            nlohmann::json{ { "active", g_mannequin_replacement_active } }.dump());
        (void)menu_view->SendEventJson("skyultra.lootMode",
            nlohmann::json{ { "active", g_loot_replacement_active } }.dump());
        (void)menu_view->SendEventJson("skyultra.pickpocketMode",
            nlohmann::json{ { "active", g_pickpocket_replacement_active } }.dump());
    }
}

void OpenSkyUltraMagicOrFavoritesReplacement(bool favorites)
{
    if (g_view_manager == nullptr || g_menu_view_id == 0) {
        return;
    }
    g_magic_replacement_active = !favorites;
    g_favorites_replacement_active = favorites;
    g_container_replacement_active = false;
    g_mannequin_replacement_active = false;
    g_loot_replacement_active = false;
    g_pickpocket_replacement_active = false;
    g_inventory_replacement_active = false;
    g_barter_replacement_active = false;
    g_restore_browser_after_menu = false;
    g_modal_menu_open = false;
    if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
        (void)menu_view->SendEventJson("skyultra.inventoryMode",
            nlohmann::json{ { "active", false } }.dump());
        (void)menu_view->SendEventJson("skyultra.barterMode",
            nlohmann::json{ { "active", false } }.dump());
    }
    SendMenuReplacementModes();
    SetBrowserVisible(true, favorites ? "Skyrim FavoritesMenu replacement" : "Skyrim MagicMenu replacement");
}

enum class ContainerReplacementVariant : std::uint8_t {
    Container,
    Mannequin,
    Loot,
    Pickpocket
};

void OpenSkyUltraContainerVariantReplacement(ContainerReplacementVariant variant)
{
    if (g_view_manager == nullptr || g_menu_view_id == 0) {
        return;
    }
    g_container_replacement_active = (variant == ContainerReplacementVariant::Container);
    g_mannequin_replacement_active = (variant == ContainerReplacementVariant::Mannequin);
    g_loot_replacement_active = (variant == ContainerReplacementVariant::Loot);
    g_pickpocket_replacement_active = (variant == ContainerReplacementVariant::Pickpocket);
    g_magic_replacement_active = false;
    g_favorites_replacement_active = false;
    g_inventory_replacement_active = false;
    g_barter_replacement_active = false;
    g_restore_browser_after_menu = false;
    g_modal_menu_open = false;
    if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
        (void)menu_view->SendEventJson("skyultra.inventoryMode",
            nlohmann::json{ { "active", false } }.dump());
        (void)menu_view->SendEventJson("skyultra.barterMode",
            nlohmann::json{ { "active", false } }.dump());
    }
    SendMenuReplacementModes();
    const char* reason = variant == ContainerReplacementVariant::Mannequin ? "Skyrim Mannequin replacement" :
        (variant == ContainerReplacementVariant::Loot ? "Skyrim Corpse Loot replacement" :
         (variant == ContainerReplacementVariant::Pickpocket ? "Skyrim Pickpocket replacement" :
          "Skyrim ContainerMenu replacement"));
    SetBrowserVisible(true, reason);
}

bool AnyMenuReplacementActive() noexcept
{
    return g_inventory_replacement_active || g_barter_replacement_active ||
        g_magic_replacement_active || g_favorites_replacement_active ||
        g_container_replacement_active || g_mannequin_replacement_active ||
        g_loot_replacement_active || g_pickpocket_replacement_active;
}

bool EqualsTokenCaseInsensitive(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto ca = a[i];
        auto cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

std::string_view KeyboardScanCodeToken(std::uint32_t scan_code) noexcept
{
    using Key = RE::BSKeyboardDevice::Keys::Key;
    switch (scan_code) {
    case Key::kLeftShift: case Key::kRightShift: return "Shift";
    case Key::kLeftControl: case Key::kRightControl: return "Ctrl";
    case Key::kLeftAlt: case Key::kRightAlt: return "Alt";
    case Key::kEscape: return "Escape";
    case Key::kTab: return "Tab";
    case Key::kEnter: case Key::kKP_Enter: return "Enter";
    case Key::kSpacebar: return "Space";
    case Key::kBackspace: return "Backspace";
    case Key::kCapsLock: return "CapsLock";
    case Key::kUp: return "Up";
    case Key::kDown: return "Down";
    case Key::kLeft: return "Left";
    case Key::kRight: return "Right";
    case Key::kHome: return "Home";
    case Key::kEnd: return "End";
    case Key::kPageUp: return "PageUp";
    case Key::kPageDown: return "PageDown";
    case Key::kInsert: return "Insert";
    case Key::kDelete: return "Delete";
    case Key::kPause: return "Pause";
    case Key::kTilde: return "Tilde";
    case Key::kMinus: return "Minus";
    case Key::kEquals: return "Equals";
    case Key::kBracketLeft: return "BracketLeft";
    case Key::kBracketRight: return "BracketRight";
    case Key::kBackslash: return "Backslash";
    case Key::kSemicolon: return "Semicolon";
    case Key::kApostrophe: return "Apostrophe";
    case Key::kComma: return "Comma";
    case Key::kPeriod: return "Period";
    case Key::kSlash: return "Slash";
    case Key::kF1: return "F1";
    case Key::kF2: return "F2";
    case Key::kF3: return "F3";
    case Key::kF4: return "F4";
    case Key::kF5: return "F5";
    case Key::kF6: return "F6";
    case Key::kF7: return "F7";
    case Key::kF8: return "F8";
    case Key::kF9: return "F9";
    case Key::kF10: return "F10";
    case Key::kF11: return "F11";
    case Key::kF12: return "F12";
    case Key::kNum0: case Key::kKP_0: return "0";
    case Key::kNum1: case Key::kKP_1: return "1";
    case Key::kNum2: case Key::kKP_2: return "2";
    case Key::kNum3: case Key::kKP_3: return "3";
    case Key::kNum4: case Key::kKP_4: return "4";
    case Key::kNum5: case Key::kKP_5: return "5";
    case Key::kNum6: case Key::kKP_6: return "6";
    case Key::kNum7: case Key::kKP_7: return "7";
    case Key::kNum8: case Key::kKP_8: return "8";
    case Key::kNum9: case Key::kKP_9: return "9";
    case Key::kA: return "A";
    case Key::kB: return "B";
    case Key::kC: return "C";
    case Key::kD: return "D";
    case Key::kE: return "E";
    case Key::kF: return "F";
    case Key::kG: return "G";
    case Key::kH: return "H";
    case Key::kI: return "I";
    case Key::kJ: return "J";
    case Key::kK: return "K";
    case Key::kL: return "L";
    case Key::kM: return "M";
    case Key::kN: return "N";
    case Key::kO: return "O";
    case Key::kP: return "P";
    case Key::kQ: return "Q";
    case Key::kR: return "R";
    case Key::kS: return "S";
    case Key::kT: return "T";
    case Key::kU: return "U";
    case Key::kV: return "V";
    case Key::kW: return "W";
    case Key::kX: return "X";
    case Key::kY: return "Y";
    case Key::kZ: return "Z";
    default: return {};
    }
}

std::string_view GamepadButtonToken(std::uint32_t id_code) noexcept
{
    using Key = RE::BSWin32GamepadDevice::Keys::Key;
    switch (id_code) {
    case Key::kA: return "A";
    case Key::kB: return "B";
    case Key::kX: return "X";
    case Key::kY: return "Y";
    case Key::kLeftShoulder: return "LB";
    case Key::kRightShoulder: return "RB";
    case Key::kLeftTrigger: return "LT";
    case Key::kRightTrigger: return "RT";
    case Key::kBack: return "Back";
    case Key::kStart: return "Start";
    case Key::kLeftThumb: return "LeftThumb";
    case Key::kRightThumb: return "RightThumb";
    case Key::kUp: return "DPadUp";
    case Key::kDown: return "DPadDown";
    case Key::kLeft: return "DPadLeft";
    case Key::kRight: return "DPadRight";
    default: return {};
    }
}

std::string_view VRButtonToken(int vr_hand, std::uint32_t id_code) noexcept
{
#if defined(ENABLE_SKYRIM_VR)
    using Key = RE::BSOpenVRControllerDevice::Keys::Key;
    if (vr_hand < 0) return {};
    const bool left = vr_hand == 0;
    if (RE::BSOpenVRControllerDevice::IsGripButton(id_code)) return left ? "LeftGrip" : "RightGrip";
    switch (id_code) {
    case Key::kTrigger: return left ? "LeftTrigger" : "RightTrigger";
    case Key::kBY: return left ? "LeftBY" : "RightBY";
    case Key::kXA: return left ? "LeftXA" : "RightXA";
    case Key::kJoystickTrigger: return left ? "LeftStickClick" : "RightStickClick";
    case Key::kTouchpadClick: case Key::kTouchpadAlt: return left ? "LeftTouchpad" : "RightTouchpad";
    default: return left ? "LeftHeadset" : "RightHeadset";
    }
#else
    (void)vr_hand; (void)id_code;
    return {};
#endif
}

bool IsKeyboardModifierToken(std::string_view token) noexcept
{
    return EqualsTokenCaseInsensitive(token, "Shift") ||
        EqualsTokenCaseInsensitive(token, "Ctrl") ||
        EqualsTokenCaseInsensitive(token, "Alt");
}

bool IsKeyboardTokenHeld(std::string_view token) noexcept
{
    if (EqualsTokenCaseInsensitive(token, "Shift")) return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (EqualsTokenCaseInsensitive(token, "Ctrl")) return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (EqualsTokenCaseInsensitive(token, "Alt")) return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    for (auto it = g_held_keyboard_scans.begin(); it != g_held_keyboard_scans.end();) {
        const auto scan = *it;
        const auto vk = MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
        if (vk != 0 && (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) == 0) {
            it = g_held_keyboard_scans.erase(it);
            continue;
        }
        if (EqualsTokenCaseInsensitive(KeyboardScanCodeToken(scan), token)) return true;
        ++it;
    }
    return false;
}

bool IsGamepadTokenHeld(std::string_view token) noexcept
{
    for (const auto code : g_held_gamepad_buttons) {
        if (EqualsTokenCaseInsensitive(GamepadButtonToken(code), token)) return true;
    }
    return false;
}

bool IsVRTokenHeld(std::string_view token) noexcept
{
    for (const auto encoded : g_held_vr_buttons) {
        const int hand = static_cast<int>(encoded >> 16U);
        const std::uint32_t code = encoded & 0xFFFFU;
        if (EqualsTokenCaseInsensitive(VRButtonToken(hand, code), token)) return true;
    }
    return false;
}

bool MatchesHotkeyChordOnDown(const HotkeyBinding& binding, RE::INPUT_DEVICE device, int vr_hand, std::uint32_t id_code) noexcept
{
    if (device == RE::INPUT_DEVICE::kKeyboard) {
        if (binding.keyboard.empty()) return false;
        const auto current = KeyboardScanCodeToken(id_code);
        if (current.empty()) return false;
        const bool has_non_modifier = std::ranges::any_of(binding.keyboard,
            [](const std::string& token) { return !IsKeyboardModifierToken(token); });
        if (has_non_modifier && IsKeyboardModifierToken(current)) {
            return false;
        }
        bool part_of_chord = false;
        for (const auto& token : binding.keyboard) {
            if (EqualsTokenCaseInsensitive(token, current)) part_of_chord = true;
            else if (!IsKeyboardTokenHeld(token)) return false;
        }
        return part_of_chord;
    }
    if (device == RE::INPUT_DEVICE::kGamepad) {
        if (binding.gamepad.empty()) return false;
        const auto current = GamepadButtonToken(id_code);
        if (current.empty()) return false;
        bool part_of_chord = false;
        for (const auto& token : binding.gamepad) {
            if (EqualsTokenCaseInsensitive(token, current)) part_of_chord = true;
            else if (!IsGamepadTokenHeld(token)) return false;
        }
        return part_of_chord;
    }
    if (vr_hand >= 0) {
        if (binding.vr.empty()) return false;
        const auto current = VRButtonToken(vr_hand, id_code);
        if (current.empty()) return false;
        bool part_of_chord = false;
        for (const auto& token : binding.vr) {
            if (EqualsTokenCaseInsensitive(token, current)) part_of_chord = true;
            else if (!IsVRTokenHeld(token)) return false;
        }
        return part_of_chord;
    }
    return false;
}

bool HasActiveInputScope()
{
    const std::scoped_lock lock(g_native_views_mutex);
    return std::ranges::any_of(g_native_views, [](const auto& item) {
        const auto& record = item.second;
        if (record.input_scope_depth == 0 || !record.manager_id || !g_view_manager) return false;
        const auto* config = g_view_manager->Config(*record.manager_id);
        return config && config->visible && !config->click_through && config->layer != skyultra::ViewLayer::Hud;
    });
}

bool ShouldDispatchButtonKey(RE::INPUT_DEVICE device, std::uint32_t id_code, const RE::ButtonEvent* button, bool& out_repeat)
{
    const auto key = (static_cast<std::uint64_t>(device) << 32U) | id_code;
    if (button->IsDown()) {
        g_button_repeat_steps[key] = 0;
        out_repeat = false;
        return true;
    }
    if (button->IsUp()) {
        g_button_repeat_steps.erase(key);
        out_repeat = false;
        return true;
    }
    if (!button->IsPressed()) {
        return false;
    }
    const float held = button->HeldDuration();
    constexpr float kInitialRepeatDelay = 0.42F;
    constexpr float kRepeatStepSeconds = 0.06F;
    if (held < kInitialRepeatDelay) {
        return false;
    }
    const auto step = static_cast<std::uint32_t>((held - kInitialRepeatDelay) / kRepeatStepSeconds) + 1U;
    auto& previous = g_button_repeat_steps[key];
    if (step <= previous) {
        return false;
    }
    previous = step;
    out_repeat = true;
    return true;
}

bool ToggleModViewsByHotkey(std::string_view mod_id)
{
    if (g_view_manager == nullptr || g_browser_runtime == nullptr) return false;
    std::vector<skyultra::ViewId> mod_views;
    SkyUltraModHandle mod_handle{};
    {
        const std::scoped_lock mods_lock(g_native_mods_mutex);
        if (const auto mod_it = g_native_mod_handles_by_id.find(std::string(mod_id)); mod_it != g_native_mod_handles_by_id.end()) {
            mod_handle = mod_it->second;
            const std::scoped_lock views_lock(g_native_views_mutex);
            for (const auto& [handle, record] : g_native_views) {
                (void)handle;
                if (record.mod_handle == mod_handle && record.manager_id && *record.manager_id != g_menu_view_id) {
                    const bool pinned = std::ranges::any_of(g_widget_overlays, [&](const auto& entry) {
                        return entry.second.manager_id == *record.manager_id;
                    });
                    if (!pinned) mod_views.push_back(*record.manager_id);
                }
            }
        }
    }
    if (mod_views.empty()) {
        std::string manifest_error;
        auto manifest = skyultra::LoadModManifest(skyultra::GetInstalledWebModsDirectory(), mod_id, manifest_error);
        if (!manifest) return false;
        if (mod_handle == 0) {
            const std::scoped_lock mods_lock(g_native_mods_mutex);
            if (const auto existing = g_native_mod_handles_by_id.find(manifest->id); existing != g_native_mod_handles_by_id.end()) {
                mod_handle = existing->second;
            } else {
                mod_handle = g_next_native_mod_handle.fetch_add(1, std::memory_order_relaxed);
                std::filesystem::path storage_dir;
                if (manifest->storage_enabled) {
                    std::string storage_err;
                    storage_dir = ModStorageDirectory(manifest->id, storage_err);
                }
                g_native_mod_handles_by_id.emplace(manifest->id, mod_handle);
                g_native_mods.emplace(mod_handle, NativeApiModRecord{ *manifest, std::move(storage_dir) });
            }
        }
        skyultra::ViewConfig config;
        config.id = manifest->id + ".main";
        config.browser_engine = (manifest->browser_engine && IsBrowserEngineBuilt(*manifest->browser_engine)) ? *manifest->browser_engine : DefaultBrowserEngine();
        if (const auto override_it = g_mod_browser_engines.find(manifest->id); override_it != g_mod_browser_engines.end()) {
            const auto req_engine = override_it->second == "cef" ? skyultra::BrowserEngine::CEF : skyultra::BrowserEngine::Ultralight;
            if (IsBrowserEngineBuilt(req_engine)) {
                config.browser_engine = req_engine;
            }
        }
        const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
        config.size = (screen.width > 0 && screen.height > 0) ? skyultra::SizeU{ screen.width, screen.height } : skyultra::SizeU{ 1280, 720 };
        config.bounds = { 0.0F, 0.0F, static_cast<float>(config.size.width), static_cast<float>(config.size.height) };
        config.anchor = skyultra::Anchor::TopLeft;
        config.layer = skyultra::ViewLayer::Panel;
        config.max_fps = g_performance_settings.main_fps;
        config.visible = true;
        config.transparent = true;
        config.enable_javascript = true;
        config.enable_gamepad = true;
        skyultra::ModContentPolicy policy;
        policy.root_directory = manifest->root_directory;
        policy.entry_file = manifest->entry_file;
        policy.asset_access_enabled = manifest->asset_access_enabled;
        policy.network_hosts = manifest->network_hosts;
        config.mod_content_policy = std::move(policy);
        const auto entry_url = LocalFileUrl(manifest->entry_file);
        const auto created_id = g_view_manager->Add(config, [&](skyultra::ViewId id) {
            return g_browser_runtime->CreateView(id, config, entry_url);
        });
        if (!created_id) return false;
        const auto view_handle = g_next_native_view_handle.fetch_add(1, std::memory_order_relaxed);
        NativeApiViewRecord record;
        record.state = SKYULTRA_VIEW_READY;
        record.manager_id = created_id;
        record.bridge_events = NativeEventsForView(created_id);
        record.bridge_responses = NativeEventsForView(created_id, true);
        record.mod_handle = mod_handle;
        record.initial_url = entry_url;
        record.javascript_enabled = true;
        record.allowed_page_request_methods = manifest->page_request_methods;
        record.browser_engine = config.browser_engine;
        {
            const std::scoped_lock views_lock(g_native_views_mutex);
            g_native_views.emplace(view_handle, std::move(record));
        }
        const std::array<skyultra::ViewId, 1> selected{created_id};
        ApplyBrowserPanelVisibility(skyultra::PanelVisibilityAction::Select, selected, mod_handle);
        SetBrowserVisible(true, "mod hotkey", BrowserOpenTarget::CurrentPanels);
        return true;
    }
    bool any_visible = false;
    for (const auto id : mod_views) {
        if (const auto* cfg = g_view_manager->Config(id); cfg && cfg->visible) any_visible = true;
    }
    const bool target_visible = !any_visible || g_browser_overlay_suppressed.load(std::memory_order_relaxed);
    if (target_visible) ApplyBrowserPanelVisibility(skyultra::PanelVisibilityAction::Select, mod_views, mod_handle);
    for (const auto id : mod_views) {
        (void)g_view_manager->SetVisible(id, target_visible);
        if (auto* view = g_view_manager->Find(id)) {
            (void)view->SendEventJson("skyultra.hotkey", nlohmann::json{{"modId", mod_id}, {"visible", target_visible}}.dump());
        }
    }
    SetBrowserVisible(target_visible || HasVisibleInteractiveView(), "mod hotkey", BrowserOpenTarget::CurrentPanels);
    return true;
}

class BrowserInputSink final : public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* event_list,
        RE::BSTEventSource<RE::InputEvent*>*) override
    {
        if (event_list == nullptr) {
            return RE::BSEventNotifyControl::kContinue;
        }

        bool consume_input{};
        for (auto* event = *event_list; event != nullptr; event = event->next) {
            if (auto* button = event->AsButtonEvent()) {
                const int vr_hand = VRControllerHand(button->GetDevice());
                const auto id_code = button->GetIDCode();
                if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                    if (button->IsPressed()) g_held_keyboard_scans.insert(id_code);
                    else if (button->IsUp()) g_held_keyboard_scans.erase(id_code);
                } else if (button->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                    if (button->IsPressed()) g_held_gamepad_buttons.insert(id_code);
                    else if (button->IsUp()) g_held_gamepad_buttons.erase(id_code);
                } else if (vr_hand >= 0) {
                    const auto vr_key = (static_cast<std::uint32_t>(vr_hand) << 16U) | (id_code & 0xFFFFU);
                    if (button->IsPressed()) g_held_vr_buttons.insert(vr_key);
                    else if (button->IsUp()) g_held_vr_buttons.erase(vr_key);
                }

                const bool recording_scope = HasVisibleInteractiveView() && HasActiveInputScope();
                if (!recording_scope && !g_modal_menu_open) {
                    if (MatchesHotkeyChordOnDown(g_skyultra_hotkey, button->GetDevice(), vr_hand, id_code)) {
                        if (button->IsDown() && !button->IsRepeating()) {
                            const auto now = std::chrono::steady_clock::now();
                            if (now - g_last_hotkey_toggle_time >= std::chrono::milliseconds(280)) {
                                g_last_hotkey_toggle_time = now;
                                if (AnyMenuReplacementActive() && HasVisibleInteractiveView()) {
                                    g_inventory_replacement_active = false;
                                    g_barter_replacement_active = false;
                                    g_magic_replacement_active = false;
                                    g_favorites_replacement_active = false;
                                    g_container_replacement_active = false;
                                    g_mannequin_replacement_active = false;
                                    g_loot_replacement_active = false;
                                    g_pickpocket_replacement_active = false;
                                    SendMenuReplacementModes();
                                    if (auto* menu_view = g_view_manager ? g_view_manager->Find(g_menu_view_id) : nullptr) {
                                        (void)menu_view->SendEventJson("skyultra.inventoryMode",
                                            nlohmann::json{ { "active", false } }.dump());
                                        (void)menu_view->SendEventJson("skyultra.barterMode",
                                            nlohmann::json{ { "active", false } }.dump());
                                    }
                                    SetBrowserVisible(true, "configured hotkey");
                                } else {
                                    const auto* main = g_view_manager ? g_view_manager->Config(g_menu_view_id) : nullptr;
                                    const bool main_visible = main && main->visible &&
                                        !g_browser_overlay_suppressed.load(std::memory_order_relaxed);
                                    SetBrowserVisible(!main_visible, "configured hotkey");
                                }
                            }
                        }
                        consume_input = true;
                        continue;
                    }
                    if (MatchesHotkeyChordOnDown(g_keyboard_hotkey, button->GetDevice(), vr_hand, id_code)) {
                        if (button->IsDown() && g_view_manager != nullptr) {
                            const bool menu_was_open = HasVisibleInteractiveView();
                            if (!menu_was_open) {
                                g_virtual_keyboard_open = true;
                                g_virtual_keyboard_opened_menu = true;
                                SetBrowserVisible(true, "virtual keyboard hotkey");
                                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                                    (void)menu_view->SendEventJson("skyultra.virtualKeyboard",
                                        nlohmann::json{ { "open", true }, { "standalone", true } }.dump());
                                }
                            } else if (g_virtual_keyboard_opened_menu && g_virtual_keyboard_open) {
                                g_virtual_keyboard_open = false;
                                g_virtual_keyboard_opened_menu = false;
                                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                                    (void)menu_view->SendEventJson("skyultra.virtualKeyboard",
                                        nlohmann::json{ { "open", false }, { "standalone", false } }.dump());
                                }
                                SetBrowserVisible(false, "virtual keyboard hotkey");
                            } else {
                                g_virtual_keyboard_open = !g_virtual_keyboard_open;
                                const auto payload = nlohmann::json{
                                    { "open", g_virtual_keyboard_open },
                                    { "standalone", false }
                                }.dump();
                                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                                    (void)menu_view->SendEventJson("skyultra.virtualKeyboard", payload);
                                }
                                if (const auto focused = GetFocusedView(skyultra::InputMode::Keyboard);
                                    focused && *focused != g_menu_view_id) {
                                    if (auto* focused_view = g_view_manager->Find(*focused)) {
                                        (void)focused_view->SendEventJson("skyultra.virtualKeyboard", payload);
                                    }
                                }
                            }
                        }
                        consume_input = true;
                        continue;
                    }
                    bool matched_mod_hotkey = false;
                    for (const auto& [mod_id, binding] : g_mod_hotkeys) {
                        if (MatchesHotkeyChordOnDown(binding, button->GetDevice(), vr_hand, id_code)) {
                            if (button->IsDown()) {
                                (void)ToggleModViewsByHotkey(mod_id);
                            }
                            consume_input = true;
                            matched_mod_hotkey = true;
                            break;
                        }
                    }
                    if (matched_mod_hotkey) continue;
                }

                if (!HasVisibleInteractiveView()) {
                    continue;
                }

                if (recording_scope && button->IsDown() && g_view_manager != nullptr) {
                    std::string_view token;
                    std::string_view device_kind;
                    if (button->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                        token = GamepadButtonToken(id_code);
                        device_kind = "gamepad";
                    } else if (vr_hand >= 0) {
                        token = VRButtonToken(vr_hand, id_code);
                        device_kind = "vr";
                    }
                    if (!token.empty()) {
                        if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                            (void)menu_view->SendEventJson("skyultra.inputToken",
                                nlohmann::json{{"device", device_kind}, {"token", token}}.dump());
                        }
                    }
                }

#if defined(ENABLE_SKYRIM_VR)
                const auto vr_code = id_code;
                const bool is_vr = REL::Module::IsVR() && vr_hand >= 0;
                const bool is_vr_keyboard_button = is_vr &&
                    RE::BSOpenVRControllerDevice::IsGripButton(vr_code);
                if (is_vr_keyboard_button) {
                    const auto hand_index = static_cast<std::size_t>(vr_hand);
                    skyultra::NoteVRControllerKeyboardButton(vr_hand, button->Value() > 0.5F);
                    if (g_vr_suppressed_keyboard_buttons[hand_index]) {
                        if (!button->IsPressed()) {
                            g_vr_suppressed_keyboard_buttons[hand_index] = false;
                        }
                        consume_input = true;
                        continue;
                    }
                }
#endif

                if (!HasVisibleInteractiveView() || g_view_manager == nullptr) {
                    continue;
                }

                if(skyultra::FrameworkSplashActive()&&button->GetDevice()!=RE::INPUT_DEVICE::kKeyboard){consume_input=true;continue;}
                if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                    const auto virtual_key = MapVirtualKeyW(
                        button->GetIDCode(), MAPVK_VSC_TO_VK_EX);
                    if(skyultra::FrameworkSplashActive()){
                        if(virtual_key==VK_ESCAPE&&button->IsDown()){
                            skyultra::CancelFrameworkSplash();
                            PublishBrowserCursor(true);
                            SKSE::log::info("SkyUltra splash screen skipped by Escape; showing menu.");
                        }
                        consume_input=true;continue;
                    }
                    const auto target = GetFocusedView(skyultra::InputMode::Keyboard);
                    bool popup_owns_escape{};
                    if (target && virtual_key == VK_ESCAPE) {
                        const std::scoped_lock lock(g_native_views_mutex);
                        popup_owns_escape = std::ranges::any_of(g_native_views, [&](const auto& item) {
                            return item.second.manager_id == target && item.second.input_scope_depth != 0;
                        });
                    }
                    if (button->IsDown() && !button->IsRepeating() && virtual_key == VK_ESCAPE && !popup_owns_escape) {
                        SetBrowserVisible(false, "Escape");
                        consume_input = true;
                        continue;
                    }
                    auto* view = target ? g_view_manager->Find(*target) : nullptr;
                    if (view != nullptr) {
                        bool is_repeat{};
                        if (ShouldDispatchButtonKey(RE::INPUT_DEVICE::kKeyboard, button->GetIDCode(), button, is_repeat)) {
                            skyultra::KeyInput input;
                            input.scan_code = button->GetIDCode();
                            input.virtual_key = MapVirtualKeyW(input.scan_code, MAPVK_VSC_TO_VK_EX);
                            input.modifiers = CurrentModifiers();
                            input.pressed = button->IsPressed();
                            input.repeat = is_repeat;
                            input.system_key = (input.modifiers & skyultra::kModifierAlt) != 0;
                            view->SendKey(input);
                            if (input.pressed && !input.repeat && !g_logged_first_browser_key_input) {
                                SKSE::log::info("SkyUltra routed its first browser key-down to view {} (scan code {}).",
                                    *target, input.scan_code);
                                g_logged_first_browser_key_input = true;
                            }
                        }
                        consume_input = true;
                    } else if (g_view_manager->HasVisibleModalView()) {
                        consume_input = true;
                    }
                }

#if defined(ENABLE_SKYRIM_VR)
                if (REL::Module::IsVR() && vr_hand >= 0) {
                    const auto code = vr_code;
                    if (code == RE::BSOpenVRControllerDevice::Keys::kTrigger) {
                        skyultra::NoteVRControllerTrigger(vr_hand, button->Value() > 0.5F);
                        const auto hand_index = static_cast<std::size_t>(vr_hand);
                        if (g_vr_suppressed_triggers[hand_index]) {
                            consume_input = true;
                            if (!button->IsPressed()) {
                                g_vr_suppressed_triggers[hand_index] = false;
                            }
                            continue;
                        }
                    }
                    if (code == RE::BSOpenVRControllerDevice::Keys::kBY) {
                        const auto hand_index = static_cast<std::size_t>(vr_hand);
                        if (g_vr_suppressed_exit_buttons[hand_index]) {
                            consume_input = true;
                            if (!button->IsPressed()) {
                                g_vr_suppressed_exit_buttons[hand_index] = false;
                            }
                            continue;
                        }
                        if (!skyultra::IsOverlayVisible()) {
                            continue;
                        }
                        if (button->IsDown()) {
                            g_vr_suppressed_exit_buttons[hand_index] = true;
                            SetBrowserVisible(false, "VR controller B/Y");
                        }
                        consume_input = true;
                        continue;
                    }
                    if (code == RE::BSOpenVRControllerDevice::Keys::kTrigger &&
                        skyultra::IsOverlayVisible()) {
                        if (button->IsDown()) {
                            g_vr_suppressed_triggers[static_cast<std::size_t>(vr_hand)] = true;
                        }
                        // The laser owns trigger clicks while the browser is open.
                        consume_input = true;
                        continue;
                    }
                    if (RE::BSOpenVRControllerDevice::IsGripButton(code) &&
                        skyultra::GetVRKeyboardFocus() != 0) {
                        const auto hand_index = static_cast<std::size_t>(vr_hand);
                        if (g_vr_suppressed_keyboard_buttons[hand_index]) {
                            consume_input = true;
                            continue;
                        }
                        if (button->IsDown()) {
                            g_vr_suppressed_keyboard_buttons[hand_index] = true;
                        }
                        // Grip opens the OpenVR keyboard for the currently focused web view.
                        consume_input = true;
                        continue;
                    }
                }
#endif

                if (button->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                    const auto target = GetFocusedView(skyultra::InputMode::Gamepad);
                    const auto* config = target ? FindViewConfig(*target) : nullptr;
                    const auto virtual_key = GamepadButtonVirtualKey(button->GetIDCode());
                    if (target && config != nullptr && config->enable_gamepad) {
                        if (virtual_key) {
                            if (auto* view = g_view_manager->Find(*target)) {
                                bool is_repeat{};
                                if (ShouldDispatchButtonKey(RE::INPUT_DEVICE::kGamepad, button->GetIDCode(), button, is_repeat)) {
                                    skyultra::KeyInput input;
                                    input.virtual_key = *virtual_key;
                                    input.scan_code = MapVirtualKeyW(*virtual_key, MAPVK_VK_TO_VSC_EX);
                                    input.pressed = button->IsPressed();
                                    input.repeat = is_repeat;
                                    view->SendKey(input);
                                }
                            }
                        }
                        // A focused WebView owns controller input; do not leak unhandled
                        // gamepad controls through to Skyrim gameplay while it is open.
                        consume_input = true;
                    }
                }

                if (button->GetDevice() == RE::INPUT_DEVICE::kMouse) {
                    if (REL::Module::IsVR()) {
                        if (skyultra::IsOverlayVisible()) {
                            consume_input = true;
                        }
                        continue;
                    }
                    const auto hit = HitTestBrowserCursor();
                    std::optional<skyultra::MouseButton> mouse_button;
                    std::optional<std::size_t> capture_slot;
                    auto action = skyultra::MouseButtonAction::None;
                    bool wheel{};
                    std::int32_t wheel_y{};
                    switch (button->GetIDCode()) {
                    case RE::BSWin32MouseDevice::Keys::kLeftButton:
                        if (!button->IsDown() && !button->IsUp()) {
                            if (hit.target || hit.block_game_input || g_mouse_capture[0]) {
                                consume_input = true;
                            }
                            continue;
                        }
                        mouse_button = skyultra::MouseButton::Left;
                        capture_slot = 0;
                        action = button->IsDown() ? skyultra::MouseButtonAction::Down :
                                                    skyultra::MouseButtonAction::Up;
                        break;
                    case RE::BSWin32MouseDevice::Keys::kRightButton:
                        if (!button->IsDown() && !button->IsUp()) {
                            if (hit.target || hit.block_game_input || g_mouse_capture[1]) {
                                consume_input = true;
                            }
                            continue;
                        }
                        mouse_button = skyultra::MouseButton::Right;
                        capture_slot = 1;
                        action = button->IsDown() ? skyultra::MouseButtonAction::Down :
                                                    skyultra::MouseButtonAction::Up;
                        break;
                    case RE::BSWin32MouseDevice::Keys::kMiddleButton:
                        if (!button->IsDown() && !button->IsUp()) {
                            if (hit.target || hit.block_game_input || g_mouse_capture[2]) {
                                consume_input = true;
                            }
                            continue;
                        }
                        mouse_button = skyultra::MouseButton::Middle;
                        capture_slot = 2;
                        action = button->IsDown() ? skyultra::MouseButtonAction::Down :
                                                    skyultra::MouseButtonAction::Up;
                        break;
                    case RE::BSWin32MouseDevice::Keys::kWheelUp:
                        if (!button->IsDown()) continue;
                        wheel = true;
                        wheel_y = 120;
                        break;
                    case RE::BSWin32MouseDevice::Keys::kWheelDown:
                        if (!button->IsDown()) continue;
                        wheel = true;
                        wheel_y = -120;
                        break;
                    default:
                        continue;
                    }

                    auto target = hit.target;
                    if (!wheel && capture_slot && action == skyultra::MouseButtonAction::Up &&
                        g_mouse_capture[*capture_slot]) {
                        target = g_mouse_capture[*capture_slot];
                    }
                    if (target) {
                        auto* view = g_view_manager->Find(*target);
                        const auto cursor = GetViewLocalCursor(*target);
                        if (view != nullptr && cursor) {
                            skyultra::MouseInput input;
                            input.position = *cursor;
                            input.moved = false;
                            input.modifiers = CurrentModifiers();
                            if (mouse_button) {
                                input.button = *mouse_button;
                                input.button_action = action;
                            }
                            if (wheel) {
                                input.wheel = true;
                                input.wheel_y = wheel_y;
                            }
                            if (input.button_action == skyultra::MouseButtonAction::Down) {
                                g_mouse_capture[*capture_slot] = *target;
                                ReleaseGamepadStickKeys();
                                if (!g_view_manager->RequestFocus(*target,
                                        InputFocusForView(*target, skyultra::InputMode::MouseAndKeyboard))) {
                                    SKSE::log::warn("SkyUltra could not focus view {} after a mouse click.", *target);
                                } else if (!g_logged_first_browser_focus) {
                                    SKSE::log::info("SkyUltra assigned browser input focus to clicked view {}.", *target);
                                    g_logged_first_browser_focus = true;
                                }
                            }
                            view->SendMouse(input);
                            if (input.button_action == skyultra::MouseButtonAction::Down &&
                                !g_logged_popup_click) {
                                SKSE::log::info("SkyUltra routed its first browser click to view {} at ({:.1f}, {:.1f}).",
                                    *target, input.position.x, input.position.y);
                                g_logged_popup_click = true;
                            }
                        }
                        if (!wheel && capture_slot && action == skyultra::MouseButtonAction::Up) {
                            g_mouse_capture[*capture_slot].reset();
                        }
                        consume_input = true;
                        continue;
                    }
                    if (hit.block_game_input) {
                        consume_input = true;
                    }
                }
            }

            if (!HasVisibleInteractiveView() || g_view_manager == nullptr) {
                continue;
            }

            if (auto* mouse_move = event->AsMouseMoveEvent()) {
                if (REL::Module::IsVR()) {
                    if (skyultra::IsOverlayVisible()) {
                        consume_input = true;
                    }
                    continue;
                }
                UpdateBrowserCursor(*mouse_move);
                if (skyultra::FrameworkSplashActive()) {
                    consume_input = true;
                    continue;
                }
                const auto hit = HitTestBrowserCursor();
                const auto captured = std::find_if(g_mouse_capture.begin(), g_mouse_capture.end(),
                    [](const auto& owner) { return owner.has_value(); });
                const auto target = captured != g_mouse_capture.end() ? *captured :
                    hit.target ? hit.target :
                    GetFocusedView(skyultra::InputMode::Mouse);
                if (target) {
                    if (auto* view = g_view_manager->Find(*target)) {
                        if (const auto cursor = GetViewLocalCursor(*target)) {
                            skyultra::MouseInput input;
                            input.position = *cursor;
                            input.modifiers = CurrentModifiers();
                            input.moved = true;
                            view->SendMouse(input);
                        }
                    }
                }
                if (captured != g_mouse_capture.end() || hit.target || hit.block_game_input) {
                    consume_input = true;
                }
                continue;
            }

            if (auto* thumbstick = event->AsThumbstickEvent()) {
#if defined(ENABLE_SKYRIM_VR)
                const int vr_hand = VRControllerHand(thumbstick->GetDevice());
                if (REL::Module::IsVR() && vr_hand >= 0) {
                    skyultra::NoteVRControllerThumbstick(vr_hand, thumbstick->yValue);
                    if (!skyultra::IsOverlayVisible()) {
                        continue;
                    }
                    // VR thumbsticks are polled by the overlay manager for
                    // page scrolling while this UI is visible.
                    consume_input = true;
                    continue;
                }
#endif
                if (UpdateGamepadStick(*thumbstick)) {
                    consume_input = true;
                }
                continue;
            }

            if (auto* character = event->AsCharEvent()) {
                const auto target = GetFocusedView(skyultra::InputMode::Keyboard);
                auto* view = target ? g_view_manager->Find(*target) : nullptr;
                if (view != nullptr) {
                    if (character->keyCode != 0) {
                        skyultra::TextInput input;
                        input.text.push_back(static_cast<char32_t>(character->keyCode));
                        input.committed = true;
                        view->SendText(input);
                        if (!g_logged_first_browser_text_input) {
                            if (*target == g_window_test_view_id) {
                                SKSE::log::info("SkyUltra routed its first text input to popup view {}.", *target);
                            } else {
                                SKSE::log::info("SkyUltra routed its first browser text input to view {}.", *target);
                            }
                            g_logged_first_browser_text_input = true;
                        }
                    }
                    consume_input = true;
                } else if (g_view_manager->HasVisibleModalView()) {
                    consume_input = true;
                }
            }
        }
        return consume_input ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
    }
};

BrowserInputSink g_browser_input_sink;

class BrowserMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event == nullptr) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (event->menuName == "SkyUltraOverlay"sv || event->menuName == "Cursor Menu"sv) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* ui = RE::UI::GetSingleton();
        if (ui == nullptr) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (event->menuName == "Main Menu"sv) {
            if (event->opening) {
                g_game_session_active.store(false, std::memory_order_relaxed);
                for (const auto& [mod, widgets] : g_widget_overlays) {
                    (void)mod;
                    (void)NativeApiDestroyView(widgets.view);
                }
                g_widget_overlays.clear();
            } else if (auto* player = RE::PlayerCharacter::GetSingleton();
                       player != nullptr && player->GetParentCell() != nullptr && !ui->IsMenuOpen("Loading Menu"sv)) {
                g_game_session_active.store(true, std::memory_order_relaxed);
            }
        } else if (event->menuName == "Loading Menu"sv) {
            if (event->opening) {
                g_game_session_active.store(false, std::memory_order_relaxed);
                PublishOverlayViews();
            } else if (!ui->IsMenuOpen("Main Menu"sv)) {
                if (auto* player = RE::PlayerCharacter::GetSingleton();
                    player != nullptr && player->GetParentCell() != nullptr) {
                    g_game_session_active.store(true, std::memory_order_relaxed);
                    SyncVanillaCompassVisibility(true);
                    if (g_view_manager != nullptr && g_menu_view_id != 0) {
                        if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                            (void)menu_view->SendEventJson("skyultra.syncHudOverlay", R"({"reason":"loadingClosed"})");
                        }
                    }
                }
            }
        } else if (event->menuName == "Fader Menu"sv || event->menuName == "Mist Menu"sv) {
            PublishOverlayViews();
        }
        if (event->menuName == "HUD Menu"sv || event->menuName == "HUDMenu"sv) {
            if (event->opening) {
                SyncVanillaCompassVisibility(true);
            }
        }

        if (g_replace_skyrim_inventory && event->menuName == RE::InventoryMenu::MENU_NAME) {
            if (event->opening) {
                if (auto menu = ui->GetMenu<RE::InventoryMenu>(); menu && menu->uiMovie) {
                    menu->uiMovie->SetBackgroundAlpha(0.0F);
                    menu->uiMovie->SetVariable("_root._visible", RE::GFxValue(false));
                }
                if (auto* inv3d = RE::Inventory3DManager::GetSingleton()) {
                    inv3d->UnloadInventoryItem();
                }
                if (auto* msg_queue = RE::UIMessageQueue::GetSingleton()) {
                    msg_queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    if (ui->IsMenuOpen("TweenMenu"sv)) {
                        msg_queue->AddMessage("TweenMenu"sv, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    }
                }
                OpenSkyUltraInventoryReplacement();
                return RE::BSEventNotifyControl::kContinue;
            }
            if (g_inventory_replacement_active) {
                g_modal_menu_open = ui->IsModalMenuOpen();
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        if (g_replace_skyrim_barter && event->menuName == RE::BarterMenu::MENU_NAME) {
            if (event->opening) {
                skyultra::CaptureActiveBarterVendor();
                if (auto menu = ui->GetMenu<RE::BarterMenu>(); menu && menu->uiMovie) {
                    menu->uiMovie->SetBackgroundAlpha(0.0F);
                    menu->uiMovie->SetVariable("_root._visible", RE::GFxValue(false));
                }
                if (auto* inv3d = RE::Inventory3DManager::GetSingleton()) {
                    inv3d->UnloadInventoryItem();
                }
                if (auto* msg_queue = RE::UIMessageQueue::GetSingleton()) {
                    msg_queue->AddMessage(RE::BarterMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    if (ui->IsMenuOpen("Dialogue Menu"sv)) {
                        msg_queue->AddMessage("Dialogue Menu"sv, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    }
                }
                OpenSkyUltraBarterReplacement();
                return RE::BSEventNotifyControl::kContinue;
            }
            if (g_barter_replacement_active) {
                g_modal_menu_open = ui->IsModalMenuOpen();
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        // Magic (spellbook) and Favorites replacements: hide Skyrim's SWF menu and show ours.
        const bool is_magic_menu = event->menuName == "MagicMenu"sv;
        const bool is_favorites_menu = event->menuName == "FavoritesMenu"sv;
        if ((is_magic_menu && g_extra_menus.replace_magic) || (is_favorites_menu && g_extra_menus.replace_favorites)) {
            if (event->opening) {
                if (auto menu = ui->GetMenu(is_favorites_menu ? "FavoritesMenu"sv : "MagicMenu"sv); menu && menu->uiMovie) {
                    menu->uiMovie->SetBackgroundAlpha(0.0F);
                    menu->uiMovie->SetVariable("_root._visible", RE::GFxValue(false));
                }
                if (auto* msg_queue = RE::UIMessageQueue::GetSingleton()) {
                    msg_queue->AddMessage(event->menuName, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    if (is_magic_menu && ui->IsMenuOpen("TweenMenu"sv)) {
                        msg_queue->AddMessage("TweenMenu"sv, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    }
                }
                OpenSkyUltraMagicOrFavoritesReplacement(is_favorites_menu);
                return RE::BSEventNotifyControl::kContinue;
            }
            if ((is_magic_menu && g_magic_replacement_active) || (is_favorites_menu && g_favorites_replacement_active)) {
                g_modal_menu_open = ui->IsModalMenuOpen();
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        if (event->menuName == RE::ContainerMenu::MENU_NAME) {
            if (event->opening) {
                skyultra::CaptureActiveContainerRef();
                const bool is_mannequin = skyultra::IsActiveContainerMannequin();
                const bool is_corpse = !is_mannequin && skyultra::IsActiveContainerCorpse();
                const bool is_pickpocket = !is_mannequin && !is_corpse && skyultra::IsActiveContainerPickpocket();
                const bool should_replace = is_mannequin ? g_extra_menus.replace_mannequin :
                    (is_corpse ? g_extra_menus.replace_loot :
                     (is_pickpocket ? g_extra_menus.replace_pickpocket : g_extra_menus.replace_container));
                if (should_replace) {
                    if (auto menu = ui->GetMenu<RE::ContainerMenu>(); menu && menu->uiMovie) {
                        menu->uiMovie->SetBackgroundAlpha(0.0F);
                        menu->uiMovie->SetVariable("_root._visible", RE::GFxValue(false));
                    }
                    if (auto* inv3d = RE::Inventory3DManager::GetSingleton()) {
                        inv3d->UnloadInventoryItem();
                    }
                    if (auto* msg_queue = RE::UIMessageQueue::GetSingleton()) {
                        msg_queue->AddMessage(RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    }
                    const auto variant = is_mannequin ? ContainerReplacementVariant::Mannequin :
                        (is_corpse ? ContainerReplacementVariant::Loot :
                         (is_pickpocket ? ContainerReplacementVariant::Pickpocket : ContainerReplacementVariant::Container));
                    OpenSkyUltraContainerVariantReplacement(variant);
                    return RE::BSEventNotifyControl::kContinue;
                }
            } else if (g_container_replacement_active || g_mannequin_replacement_active ||
                       g_loot_replacement_active || g_pickpocket_replacement_active) {
                g_modal_menu_open = ui->IsModalMenuOpen();
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        // When placing or removing a custom map marker in MapMenu, Skyrim opens and closes MessageBoxMenu.
        // MessageBoxMenu queues its cursor hide while closing. Restore on the
        // next game task, after that hide, even if Cursor Menu is still open
        // when the close event fires. The HUD carrier preserves cursor state.
        if (!event->opening && event->menuName == "MessageBoxMenu"sv && ui->IsMenuOpen("MapMenu"sv)) {
            (void)g_game_task_queue.Post([] {
                auto* current_ui = RE::UI::GetSingleton();
                if (current_ui && current_ui->IsMenuOpen("MapMenu"sv) && !current_ui->IsMenuOpen("MessageBoxMenu"sv)) {
                    if (auto* msg_queue = RE::UIMessageQueue::GetSingleton()) {
                        msg_queue->AddMessage("Cursor Menu"sv, RE::UI_MESSAGE_TYPE::kShow, nullptr);
                    }
                }
            });
        }

        const auto modal_menu_open = IsModalOrBlockingMenuOpen(ui);
        if (modal_menu_open == g_modal_menu_open) {
            PublishOverlayViews();
            return RE::BSEventNotifyControl::kContinue;
        }
        g_modal_menu_open = modal_menu_open;

        static bool restore_inventory_replacement = false;
        static bool restore_barter_replacement = false;
        static bool restore_magic_replacement = false;
        static bool restore_favorites_replacement = false;
        static bool restore_container_replacement = false;
        static bool restore_mannequin_replacement = false;
        static bool restore_loot_replacement = false;
        static bool restore_pickpocket_replacement = false;
        if (modal_menu_open) {
            g_restore_browser_after_menu = HasVisibleInteractiveView();
            restore_inventory_replacement = g_restore_browser_after_menu && g_inventory_replacement_active;
            restore_barter_replacement = g_restore_browser_after_menu && g_barter_replacement_active;
            restore_magic_replacement = g_restore_browser_after_menu && g_magic_replacement_active;
            restore_favorites_replacement = g_restore_browser_after_menu && g_favorites_replacement_active;
            restore_container_replacement = g_restore_browser_after_menu && g_container_replacement_active;
            restore_mannequin_replacement = g_restore_browser_after_menu && g_mannequin_replacement_active;
            restore_loot_replacement = g_restore_browser_after_menu && g_loot_replacement_active;
            restore_pickpocket_replacement = g_restore_browser_after_menu && g_pickpocket_replacement_active;
            if (g_restore_browser_after_menu) {
                SetBrowserVisible(false, "Skyrim modal menu opened", BrowserOpenTarget::RestoreSuspended);
            }
        } else if (g_restore_browser_after_menu) {
            g_restore_browser_after_menu = false;
            if (restore_inventory_replacement) {
                restore_inventory_replacement = false;
                OpenSkyUltraInventoryReplacement();
            } else if (restore_barter_replacement) {
                restore_barter_replacement = false;
                OpenSkyUltraBarterReplacement();
            } else if (restore_magic_replacement || restore_favorites_replacement) {
                const bool favorites = restore_favorites_replacement;
                restore_magic_replacement = false;
                restore_favorites_replacement = false;
                OpenSkyUltraMagicOrFavoritesReplacement(favorites);
            } else if (restore_container_replacement || restore_mannequin_replacement ||
                       restore_loot_replacement || restore_pickpocket_replacement) {
                const auto variant = restore_mannequin_replacement ? ContainerReplacementVariant::Mannequin :
                    (restore_loot_replacement ? ContainerReplacementVariant::Loot :
                     (restore_pickpocket_replacement ? ContainerReplacementVariant::Pickpocket : ContainerReplacementVariant::Container));
                restore_container_replacement = false;
                restore_mannequin_replacement = false;
                restore_loot_replacement = false;
                restore_pickpocket_replacement = false;
                OpenSkyUltraContainerVariantReplacement(variant);
            } else {
                SetBrowserVisible(true, "Skyrim modal menu closed", BrowserOpenTarget::RestoreSuspended);
            }
        }
        PublishOverlayViews();
        return RE::BSEventNotifyControl::kContinue;
    }
};

BrowserMenuSink g_browser_menu_sink;

void TryRegisterMenuEventSink()
{
    if (g_menu_event_sink_registered) {
        return;
    }
    auto* ui = RE::UI::GetSingleton();
    if (ui == nullptr) {
        return;
    }
    g_modal_menu_open = IsModalOrBlockingMenuOpen(ui);
    ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_browser_menu_sink);
    g_menu_event_sink_registered = true;
    SKSE::log::info("SkyUltra Skyrim menu integration registered.");
}

void TryRegisterInputSink()
{
    if (g_input_sink_registered) {
        return;
    }
    if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
        input->PrependEventSink(&g_browser_input_sink);
        g_input_sink_registered = true;
        SKSE::log::info("SkyUltra browser input sink registered.");
    }
}

const char* BrowserEngineName(skyultra::BrowserEngine engine) noexcept
{
    return engine == skyultra::BrowserEngine::Ultralight ? "Ultralight" : "CEF";
}

std::string AddBrowserEngineToLocalUrl(
    std::string_view url, skyultra::BrowserEngine engine, bool visible=true)
{
    std::string result(url);
    if (!result.starts_with("file://")) {
        return result;
    }

    const auto fragment = result.find('#');
    const auto url_end = fragment == std::string::npos ? result.size() : fragment;
    const auto has_query = result.find('?', 0) < url_end;
    result.insert(url_end, std::string(has_query ? "&" : "?") +
        "__skyultra_engine=" +
        std::string(engine == skyultra::BrowserEngine::Ultralight ? "ultralight" : "cef")+"&__skyultra_visible="+(visible?"1":"0"));
    return result;
}

skyultra::BrowserEngine EffectiveBrowserEngine(skyultra::BrowserEngine requested) noexcept
{
    return IsBrowserEngineBuilt(requested) ? requested : DefaultBrowserEngine();
}

skyultra::BrowserEngine PluginOwnedBrowserEngine() noexcept
{
    return IsBrowserEngineBuilt(skyultra::BrowserEngine::CEF) ?
        skyultra::BrowserEngine::CEF : DefaultBrowserEngine();
}

bool HasInstalledCefMod()
{
    const auto mods_directory = skyultra::GetInstalledWebModsDirectory();
    if (mods_directory.empty()) {
        return false;
    }

    std::error_code filesystem_error;
    std::filesystem::directory_iterator entry(mods_directory, filesystem_error);
    if (filesystem_error) {
        return false;
    }

    const std::filesystem::directory_iterator end;
    while (entry != end) {
        std::error_code entry_error;
        if (entry->is_directory(entry_error) && !entry_error) {
            auto mod_id = entry->path().filename().string();
            for (auto& character : mod_id) {
                if (character >= 'A' && character <= 'Z') {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }
            if (skyultra::IsValidModIdentifier(mod_id)) {
                std::string manifest_error;
                const auto manifest = skyultra::LoadModManifest(
                    mods_directory, mod_id, manifest_error);
                if (manifest && manifest->browser_engine &&
                    *manifest->browser_engine == skyultra::BrowserEngine::CEF) {
                    return true;
                }
            }
        }

        entry.increment(filesystem_error);
        if (filesystem_error) {
            return false;
        }
    }
    return false;
}

class BrowserRuntimeRouter final : public skyultra::IWebRuntime {
public:
    using InitializationCallback = std::function<void(bool, std::string)>;

    BrowserRuntimeRouter(
        skyultra::BrowserEngine default_engine,
        std::vector<std::unique_ptr<skyultra::IWebRuntime>> runtimes) :
        default_engine_(default_engine), runtimes_(std::move(runtimes)),
        application_thread_(std::this_thread::get_id()) {}

    ~BrowserRuntimeRouter() override
    {
        Shutdown();
    }

    [[nodiscard]] skyultra::BrowserEngine Engine() const noexcept override
    {
        if (IsInitialized(default_engine_)) {
            return default_engine_;
        }
        for (const auto& runtime : runtimes_) {
            if (runtime != nullptr && IsInitialized(runtime->Engine())) {
                return runtime->Engine();
            }
        }
        return runtimes_.empty() ? default_engine_ : runtimes_.front()->Engine();
    }

    [[nodiscard]] bool Initialize(std::string& error) override
    {
        if (EnsureInitialized(default_engine_, error)) {
            for (const auto& runtime : runtimes_) {
                if (runtime == nullptr) continue;
                const auto secondary_engine = runtime->Engine();
                if (!IsInitialized(secondary_engine)) {
                    SKSE::log::info(
                        "SkyUltra is warming {} during startup for instant multi-engine switching.",
                        BrowserEngineName(secondary_engine));
                    std::string secondary_error;
                    if (!EnsureInitialized(secondary_engine, secondary_error)) {
                        SKSE::log::warn(
                            "SkyUltra {} startup warm-up failed: {}",
                            BrowserEngineName(secondary_engine), secondary_error);
                    } else {
                        SKSE::log::info("SkyUltra {} startup warm-up completed.",
                            BrowserEngineName(secondary_engine));
                    }
                }
            }
            return true;
        }

        const auto preferred_error = error;
        for (const auto& runtime : runtimes_) {
            const auto fallback_engine = runtime->Engine();
            if (fallback_engine == default_engine_) {
                continue;
            }
            std::string fallback_error;
            if (EnsureInitialized(fallback_engine, fallback_error)) {
                default_engine_ = fallback_engine;
                SKSE::log::warn("SkyUltra is using {} as its default because the configured backend failed to start.",
                    BrowserEngineName(default_engine_));
                return true;
            }
            if (!fallback_error.empty()) {
                error += "; ";
                error += fallback_error;
            }
        }
        if (error.empty()) {
            error = preferred_error.empty() ? "No browser backend was configured." : preferred_error;
        }
        return false;
    }

    [[nodiscard]] bool InitializeCef(
        skyultra::BrowserEngine engine, InitializationCallback callback)
    {
        if (engine != skyultra::BrowserEngine::CEF || !callback) {
            return false;
        }
        if (Find(engine) == nullptr) {
            callback(false, "The requested CEF backend is not present in this build.");
            return true;
        }
        std::string error;
        const auto initialized = EnsureInitialized(engine, error);
        callback(initialized, std::move(error));
        return true;
    }

    void Shutdown() noexcept override
    {
        for (auto runtime = runtimes_.rbegin(); runtime != runtimes_.rend(); ++runtime) {
            (*runtime)->Shutdown();
        }
        runtimes_.clear();
    }

    void PumpFrame(void* d3d_device, void* d3d_context) override
    {
        if (d3d_device != nullptr) {
            last_d3d_device_.store(d3d_device, std::memory_order_release);
        }
        if (d3d_context != nullptr) {
            last_d3d_context_.store(d3d_context, std::memory_order_release);
        }
        PumpMessageLoop();
        for (const auto& runtime : runtimes_) {
            if (IsInitialized(runtime->Engine())) {
                runtime->PumpFrame(d3d_device, d3d_context);
            }
        }
    }

    void PumpMessageLoop() override
    {
        for (const auto& runtime : runtimes_) {
            if (IsInitialized(runtime->Engine())) {
                runtime->PumpMessageLoop();
            }
        }
    }

    [[nodiscard]] bool SupportsEngine(skyultra::BrowserEngine engine) const noexcept override
    {
        return Find(engine) != nullptr && !IsFailed(engine);
    }

    [[nodiscard]] skyultra::BrowserEngine ResolvedEngine(
        skyultra::BrowserEngine requested) const noexcept
    {
        return IsInitialized(requested) ? requested : Engine();
    }

    void SetPageBridgeMessageCallback(
        skyultra::PageBridgeMessageCallback callback) override
    {
        page_bridge_callback_ = std::move(callback);
        for (const auto& runtime : runtimes_) {
            if (IsInitialized(runtime->Engine())) {
                runtime->SetPageBridgeMessageCallback(page_bridge_callback_);
            }
        }
    }

    [[nodiscard]] std::unique_ptr<skyultra::IWebView> CreateView(
        skyultra::ViewId id, const skyultra::ViewConfig& config,
        std::string_view initial_url) override
    {
        auto selected_config = config;
        auto* runtime = Find(config.browser_engine);
        std::string initialization_error;
        if (runtime == nullptr || !EnsureInitialized(config.browser_engine, initialization_error)) {
            const auto requested_engine = config.browser_engine;
            runtime = Find(Engine());
            if (runtime == nullptr || !IsInitialized(runtime->Engine())) {
                return {};
            }
            selected_config.browser_engine = runtime->Engine();
            const auto index = static_cast<std::size_t>(requested_engine);
            if (index < logged_backend_fallback_.size() && !logged_backend_fallback_[index]) {
                const auto reason = initialization_error.empty() ? std::string{} :
                    std::string(": ") + initialization_error;
                SKSE::log::warn("SkyUltra could not start {} views{}; using {} instead.",
                    BrowserEngineName(requested_engine),
                    reason,
                    BrowserEngineName(runtime->Engine()));
                logged_backend_fallback_[index] = true;
            }
        }
        const auto page_url = AddBrowserEngineToLocalUrl(initial_url, runtime->Engine(),selected_config.visible);
        return runtime->CreateView(id, selected_config, page_url);
    }

private:
    [[nodiscard]] static std::size_t EngineIndex(skyultra::BrowserEngine engine) noexcept
    {
        return static_cast<std::size_t>(engine);
    }

    [[nodiscard]] bool IsInitialized(skyultra::BrowserEngine engine) const noexcept
    {
        const auto index = EngineIndex(engine);
        return index < initialized_.size() && initialized_[index].load(std::memory_order_acquire);
    }

    [[nodiscard]] bool IsFailed(skyultra::BrowserEngine engine) const noexcept
    {
        const auto index = EngineIndex(engine);
        return index < failed_.size() && failed_[index].load(std::memory_order_acquire);
    }

    [[nodiscard]] bool EnsureInitialized(
        skyultra::BrowserEngine engine, std::string& error)
    {
        auto* runtime = Find(engine);
        const auto index = EngineIndex(engine);
        if (runtime == nullptr || index >= initialized_.size()) {
            error = "The requested browser backend is not present in this build.";
            return false;
        }
        if (IsInitialized(engine)) {
            return true;
        }
        if (IsFailed(engine)) {
            error = BrowserEngineName(engine);
            error += " backend initialization already failed.";
            return false;
        }
        if (engine == skyultra::BrowserEngine::CEF &&
            !g_browser_settings.cef_multithreaded &&
            std::this_thread::get_id() != application_thread_) {
            error = "CEF must be initialized on SkyUltra's application thread.";
            return false;
        }

        const std::scoped_lock lock(initialization_mutex_);
        if (IsInitialized(engine)) {
            return true;
        }
        if (IsFailed(engine)) {
            error = BrowserEngineName(engine);
            error += " backend initialization already failed.";
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        std::string runtime_error;
        if (!runtime->Initialize(runtime_error)) {
            runtime->Shutdown();
            failed_[index].store(true, std::memory_order_release);
            error = runtime_error;
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            SKSE::log::warn("SkyUltra {} backend could not start after {} ms: {}",
                BrowserEngineName(engine), elapsed, runtime_error);
            return false;
        }
        runtime->SetPageBridgeMessageCallback(page_bridge_callback_);
        initialized_[index].store(true, std::memory_order_release);
        if (auto* dev = last_d3d_device_.load(std::memory_order_acquire);
            dev != nullptr && last_d3d_context_.load(std::memory_order_acquire) != nullptr) {
            runtime->PumpFrame(dev, last_d3d_context_.load(std::memory_order_acquire));
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        SKSE::log::info("SkyUltra initialized the {} browser backend in {} ms.",
            BrowserEngineName(engine), elapsed);
        return true;
    }

    [[nodiscard]] skyultra::IWebRuntime* Find(skyultra::BrowserEngine engine) const noexcept
    {
        const auto runtime = std::find_if(runtimes_.begin(), runtimes_.end(), [engine](const auto& item) {
            return item != nullptr && item->Engine() == engine;
        });
        return runtime == runtimes_.end() ? nullptr : runtime->get();
    }

    skyultra::BrowserEngine default_engine_;
    std::vector<std::unique_ptr<skyultra::IWebRuntime>> runtimes_;
    skyultra::PageBridgeMessageCallback page_bridge_callback_;
    std::mutex initialization_mutex_;
    std::array<std::atomic_bool, 2> initialized_{};
    std::array<std::atomic_bool, 2> failed_{};
    std::array<bool, 2> logged_backend_fallback_{};
    std::atomic<void*> last_d3d_device_{};
    std::atomic<void*> last_d3d_context_{};
    const std::thread::id application_thread_;
};

void StartBrowser()
{
    if (g_browser_started.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    const auto plugin_directory = PluginDirectory();
    if (plugin_directory.empty()) {
        SKSE::log::error("SkyUltra could not resolve its plugin directory.");
        g_browser_started.store(false, std::memory_order_release);
        return;
    }
    LoadRuntimeSettings();
    std::vector<std::unique_ptr<skyultra::IWebRuntime>> runtimes;
    runtimes.reserve(2);

    const auto add_ultralight = [&] {
#if defined(SKYULTRA_WITH_ULTRALIGHT)
        std::uint32_t runtime_load_error{};
        if (!skyultra::LoadPrivateUltralightRuntime(&runtime_load_error)) {
            SKSE::log::warn("SkyUltra could not load its isolated Ultralight runtime (Win32 error {}).",
                runtime_load_error);
            return;
        }
        skyultra::UltralightRuntimeConfig runtime_config;
        runtime_config.plugin_directory = plugin_directory;
        runtime_config.cache_directory = BrowserCacheDirectory(L"UltralightCache");
        runtime_config.force_cpu_rendering =
            g_browser_settings.render_mode == BrowserRenderMode::Cpu;
        if (runtime_config.cache_directory.empty()) {
            SKSE::log::warn("SkyUltra could not resolve the Ultralight cache directory.");
            return;
        }
        SKSE::log::info("SkyUltra loaded its private Ultralight runtime.");
        runtimes.push_back(std::make_unique<skyultra::UltralightRuntime>(std::move(runtime_config)));
#endif
    };
    const auto add_cef = [&] {
#if defined(SKYULTRA_WITH_CEF)
        const auto cef_directory = plugin_directory / L"SkyUltra" / L"CEF";
        const auto cache_directory = BrowserCacheDirectory(L"CEFCache");
        if (cache_directory.empty()) {
            SKSE::log::warn("SkyUltra could not resolve the CEF cache directory.");
            return;
        }
        skyultra::CefRuntimeConfig runtime_config;
        runtime_config.runtime_directory = cef_directory;
        runtime_config.subprocess_executable = cef_directory / L"SkyUltraCefSubprocess.exe";
        runtime_config.cache_directory = cache_directory;
        runtime_config.gpu_rendering = g_browser_settings.render_mode == BrowserRenderMode::Gpu;
        runtime_config.multi_threaded_message_loop = g_browser_settings.cef_multithreaded;
        if (const auto log_directory = SKSE::log::log_directory()) {
            runtime_config.log_file = *log_directory / L"SkyUltra_CEF.log";
        } else {
            runtime_config.log_file = cache_directory / L"SkyUltra_CEF.log";
            SKSE::log::warn("SkyUltra could not resolve the Documents SKSE log folder; CEF logging will use its cache folder.");
        }
        runtime_config.log_info = [](std::string_view message) {
            SKSE::log::info("SkyUltra CEF: {}", std::string(message));
        };
        runtimes.push_back(std::make_unique<skyultra::CefRuntime>(std::move(runtime_config)));
#endif
    };

    // Start every backend included in the build. Mod manifests select the
    // engine for their own views; unpinned mods use the compile-time default.
    if (DefaultBrowserEngine() == skyultra::BrowserEngine::Ultralight) {
        add_ultralight();
        add_cef();
    } else {
        add_cef();
        add_ultralight();
    }
    LoadFrameworkSettings();

    const auto initial_menu_engine = g_menu_browser_engine == "ultralight"
        ? skyultra::BrowserEngine::Ultralight
        : skyultra::BrowserEngine::CEF;
    g_browser_runtime = std::make_unique<BrowserRuntimeRouter>(
        EffectiveBrowserEngine(initial_menu_engine), std::move(runtimes));
    std::string error;
    if (!g_browser_runtime->Initialize(error)) {
        SKSE::log::error("SkyUltra browser startup failed: {}", error);
        g_browser_runtime.reset();
        g_browser_started.store(false, std::memory_order_release);
        return;
    }
    g_browser_runtime->SetPageBridgeMessageCallback(OnBrowserPageBridgeMessage);

    skyultra::ViewConfig config;
    config.id = "SkyUltra.Menu";
    config.browser_engine = EffectiveBrowserEngine(
        g_menu_browser_engine == "ultralight" ? skyultra::BrowserEngine::Ultralight : skyultra::BrowserEngine::CEF);
    config.size = g_performance_settings.main_size;
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    if (screen.width > 0 && screen.height > 0) {
        if (config.size.width == 0 || config.size.height == 0) {
            config.size = { screen.width, screen.height };
        }
        config.bounds = { 0.0F, 0.0F,
            static_cast<float>(screen.width), static_cast<float>(screen.height) };
    } else {
        if (config.size.width == 0 || config.size.height == 0) {
            config.size = { 1280, 720 };
        }
        config.bounds = { 0.0F, 0.0F,
            static_cast<float>(config.size.width), static_cast<float>(config.size.height) };
    }
    config.anchor = skyultra::Anchor::TopLeft;
    config.layer = skyultra::ViewLayer::Panel;
    config.max_fps = g_performance_settings.main_fps;
    config.visible = false;
    config.transparent = true;
    config.enable_javascript = true;
    config.enable_gamepad = true;

    std::string menu_error;
    auto menu_manifest=skyultra::LoadModManifest(skyultra::GetInstalledWebModsDirectory(),"skyultra.menu",menu_error);
    g_view_manager=std::make_unique<skyultra::ViewManager>();
    if(!menu_manifest){
        SKSE::log::warn("SkyUltra main menu is missing: {}. Other mods remain available. Copy the menu to Data/SkyUltra/skyultra.menu.",menu_error);
    } else {
    if(menu_manifest->browser_engine && g_menu_browser_engine.empty())config.browser_engine=EffectiveBrowserEngine(*menu_manifest->browser_engine);
    skyultra::ModContentPolicy menu_policy;
    menu_policy.root_directory=menu_manifest->root_directory;menu_policy.entry_file=menu_manifest->entry_file;
    menu_policy.asset_access_enabled=true;config.mod_content_policy=std::move(menu_policy);
    const auto menu_url=LocalFileUrl(menu_manifest->entry_file);
    g_menu_view_id=g_view_manager->Add(config,[&](skyultra::ViewId id){return g_browser_runtime->CreateView(id,config,menu_url);});
    if(!g_menu_view_id){SKSE::log::warn("SkyUltra could not create the built-in menu; other mods remain available.");}
    else {
    const auto menu_mod=g_next_native_mod_handle.fetch_add(1);
    const auto menu_handle=g_next_native_view_handle.fetch_add(1);
    NativeApiViewRecord record;
    record.state=SKYULTRA_VIEW_READY;record.manager_id=g_menu_view_id;record.mod_handle=menu_mod;
    record.bridge_events = NativeEventsForView(g_menu_view_id);
    record.bridge_responses = NativeEventsForView(g_menu_view_id, true);
    record.initial_url=menu_url;record.javascript_enabled=true;record.wait_for_page_ready=true;
    menu_manifest->page_request_methods.emplace("game.itemAction");
    for (const auto* method : { "game.getSpells", "game.equipSpell", "game.renameSpell", "game.deleteSpell", "game.getFavorites", "game.setFavorite", "game.getContainerSnapshot", "game.containerTransfer" }) menu_manifest->page_request_methods.emplace(method);
    record.allowed_page_request_methods=menu_manifest->page_request_methods;
    record.browser_engine=static_cast<BrowserRuntimeRouter*>(g_browser_runtime.get())->ResolvedEngine(config.browser_engine);
    {const std::scoped_lock lock(g_native_views_mutex);g_native_views.emplace(menu_handle,std::move(record));}
    {const std::scoped_lock lock(g_native_mods_mutex);g_native_mod_handles_by_id.emplace("skyultra.menu",menu_mod);g_native_mods.emplace(menu_mod,NativeApiModRecord{std::move(*menu_manifest),{}});}
    skyultra::BeginOverlayPageLoad(g_menu_view_id);
    SKSE::log::info("SkyUltra registered its built-in menu; no ESP is required.");
    }
    }
    skyultra::SetOverlaySpatialConfig("atGaze", 1.55F, 1.0F, g_vr_head_tracking);

    skyultra::SetOverlayRuntime(g_browser_runtime.get());
    PublishOverlayViews();
    if (!REL::Module::IsVR() && !skyultra::RegisterD3D11UiPassMenu()) {
        SKSE::log::warn("SkyUltra could not register its UI-pass carrier menu; Present fallback remains available.");
    }
    g_render_hook_installed = skyultra::InstallD3D11PresentHook();
    TryRegisterInputSink();
    TryRegisterMenuEventSink();
    g_native_api_ready.store(true, std::memory_order_release);
    StartModHotReloadWatcher();
    StartGameDataSubscriptionTicker();
    SKSE::log::info("SkyUltra selected the {} render path for {}.",
        REL::Module::IsVR() ? "OpenVR overlay" : "D3D11 UI-pass", g_runtime_name);
    if (g_render_hook_installed) {
        SKSE::log::info("SkyUltra browser ready (default {}; menu requests {}). Press Shift+F6 to open or close the menu.",
            BrowserEngineName(g_browser_runtime->Engine()),
            BrowserEngineName(config.browser_engine));
    } else {
        SKSE::log::warn("SkyUltra browser ready; swap-chain hook will retry on Shift+F6.");
    }
}

void DrainNativeApiQueue()
{
    static std::chrono::steady_clock::time_point s_next_splash_drain_time{};
    const auto start = std::chrono::steady_clock::now();
    std::size_t processed = 0;
    const bool splash_active = skyultra::FrameworkSplashActive();
    if (splash_active && start < s_next_splash_drain_time) {
        {
            const std::scoped_lock lock(g_native_api_queue_mutex);
            if (g_native_api_queue.empty()) {
                g_native_api_drain_scheduled = false;
                return;
            }
        }
        if (g_game_task_queue.Post(DrainNativeApiQueue)) {
            return;
        }
    }
    const std::size_t max_tasks_per_tick = splash_active ? 1U : 3U;
    const auto max_budget_per_tick = splash_active
        ? std::chrono::milliseconds(1)
        : std::chrono::milliseconds(2);
    for (;;) {
        if (processed >= max_tasks_per_tick ||
            (processed > 0 && std::chrono::steady_clock::now() - start >= max_budget_per_tick)) {
            bool has_more = false;
            {
                const std::scoped_lock lock(g_native_api_queue_mutex);
                has_more = !g_native_api_queue.empty();
                if (!has_more) {
                    g_native_api_drain_scheduled = false;
                    return;
                }
            }
            if (g_game_task_queue.Post(DrainNativeApiQueue)) {
                return;
            }
        }
        std::function<void()> task;
        {
            const std::scoped_lock lock(g_native_api_queue_mutex);
            if (g_native_api_queue.empty()) {
                g_native_api_drain_scheduled = false;
                return;
            }
            task = std::move(g_native_api_queue.front());
            g_native_api_queue.pop_front();
        }
        ++processed;
        const auto task_start = std::chrono::steady_clock::now();
        try {
            task();
        } catch (const std::exception& exception) {
            SKSE::log::error("SkyUltra native API task failed: {}", exception.what());
        } catch (...) {
            SKSE::log::error("SkyUltra native API task failed with an unknown exception.");
        }
        if (skyultra::FrameworkSplashActive()) {
            const auto task_end = std::chrono::steady_clock::now();
            if (task_end - task_start >= std::chrono::microseconds(1200)) {
                s_next_splash_drain_time = task_end + std::chrono::milliseconds(40);
            }
        }
    }
}

bool QueueNativeApiTask(std::function<void()> task)
{
    const std::scoped_lock enqueue_lock(g_native_api_enqueue_mutex);
    {
        const std::scoped_lock queue_lock(g_native_api_queue_mutex);
        g_native_api_queue.emplace_back(std::move(task));
        if (g_native_api_drain_scheduled) {
            return true;
        }
        g_native_api_drain_scheduled = true;
    }

    if (g_game_task_queue.Post(DrainNativeApiQueue)) {
        return true;
    }

    const std::scoped_lock queue_lock(g_native_api_queue_mutex);
    g_native_api_drain_scheduled = false;
    g_native_api_queue.clear();
    return false;
}

std::optional<skyultra::ViewId> NativeManagerViewId(SkyUltraViewHandle handle)
{
    const std::scoped_lock lock(g_native_views_mutex);
    const auto it = g_native_views.find(handle);
    return it == g_native_views.end() ? std::nullopt : it->second.manager_id;
}

bool NativeHandleAcceptsCommands(SkyUltraViewHandle handle)
{
    const std::scoped_lock lock(g_native_views_mutex);
    const auto it = g_native_views.find(handle);
    return it != g_native_views.end() &&
        it->second.state != SKYULTRA_VIEW_CLOSING && it->second.state != SKYULTRA_VIEW_FAILED;
}

std::optional<SkyUltraViewHandle> NativeHandleForManagerView(skyultra::ViewId view_id)
{
    const std::scoped_lock lock(g_native_views_mutex);
    for (const auto& [handle, record] : g_native_views) {
        if (record.manager_id == view_id) {
            return handle;
        }
    }
    return std::nullopt;
}

void SwitchMenuBrowserEngine(skyultra::BrowserEngine target_engine)
{
    if (g_view_manager == nullptr || g_browser_runtime == nullptr || g_menu_view_id == 0) {
        return;
    }
    const auto desired = EffectiveBrowserEngine(target_engine);
    (void)QueueNativeApiTask([desired]() {
        if (g_view_manager == nullptr || g_browser_runtime == nullptr || g_menu_view_id == 0) {
            return;
        }
        const auto menu_handle = NativeHandleForManagerView(g_menu_view_id);
        if (!menu_handle) {
            return;
        }
        const auto* current_cfg = g_view_manager->Config(g_menu_view_id);
        if (current_cfg == nullptr) {
            return;
        }
        std::string initial_url;
        skyultra::BrowserEngine current_engine = skyultra::BrowserEngine::Ultralight;
        {
            const std::scoped_lock lock(g_native_views_mutex);
            const auto it = g_native_views.find(*menu_handle);
            if (it == g_native_views.end()) {
                return;
            }
            initial_url = it->second.initial_url;
            current_engine = it->second.browser_engine;
        }
        if (current_engine == desired) {
            return;
        }

        auto next_cfg = *current_cfg;
        next_cfg.bridge_event_queue = NativeEventsForView(g_menu_view_id);
        next_cfg.bridge_response_queue = NativeEventsForView(g_menu_view_id, true);
        next_cfg.browser_engine = desired;
        const auto old_id = g_menu_view_id;
        skyultra::RemoveActorPreview(old_id);
        (void)g_view_manager->Remove(old_id);
        const auto new_id = g_view_manager->Add(next_cfg, [&](skyultra::ViewId id) {
            return g_browser_runtime->CreateView(id, next_cfg, initial_url);
        });
        if (!new_id) {
            SKSE::log::error("SkyUltra could not switch SkyUltra.Menu to {}; restoring previous engine.",
                BrowserEngineName(desired));
            next_cfg.browser_engine = current_engine;
            g_menu_view_id = g_view_manager->Add(next_cfg, [&](skyultra::ViewId id) {
                return g_browser_runtime->CreateView(id, next_cfg, initial_url);
            });
            PublishOverlayViews();
            return;
        }
        g_menu_view_id = new_id;
        skyultra::BeginOverlayPageLoad(new_id);
        const auto* router = dynamic_cast<BrowserRuntimeRouter*>(g_browser_runtime.get());
        const auto resolved = router != nullptr ? router->ResolvedEngine(desired) : desired;
        g_menu_browser_engine = resolved == skyultra::BrowserEngine::CEF ? "cef" : "ultralight";
        g_game_subscriptions.erase(*menu_handle);
        skyultra::InvalidateSharedGameDataCache();
        {
            const std::scoped_lock lock(g_native_views_mutex);
            if (const auto it = g_native_views.find(*menu_handle); it != g_native_views.end()) {
                it->second.manager_id = new_id;
                it->second.browser_engine = resolved;
                it->second.pending_page_requests.clear();
            }
        }
        PublishOverlayViews();
        PublishBrowserCursor(next_cfg.visible);
        SKSE::log::info("SkyUltra switched SkyUltra.Menu engine to {} independently of other mods.",
            BrowserEngineName(resolved));
    });
}

void SwitchModBrowserEngine(std::string_view mod_id_view, skyultra::BrowserEngine target_engine)
{
    if (g_view_manager == nullptr || g_browser_runtime == nullptr || mod_id_view.empty() || mod_id_view == "skyultra.menu") {
        return;
    }
    const std::string mod_id(mod_id_view);
    const auto desired = EffectiveBrowserEngine(target_engine);
    (void)QueueNativeApiTask([mod_id, desired]() {
        if (g_view_manager == nullptr || g_browser_runtime == nullptr) {
            return;
        }
        SkyUltraModHandle mod_handle{};
        {
            const std::scoped_lock lock(g_native_mods_mutex);
            const auto it = g_native_mod_handles_by_id.find(mod_id);
            if (it == g_native_mod_handles_by_id.end()) {
                return;
            }
            mod_handle = it->second;
        }
        struct TargetView {
            SkyUltraViewHandle handle{};
            skyultra::ViewId manager_id{};
            std::string initial_url;
        };
        std::vector<TargetView> targets;
        {
            const std::scoped_lock lock(g_native_views_mutex);
            for (const auto& [handle, rec] : g_native_views) {
                if (rec.mod_handle == mod_handle && rec.manager_id && *rec.manager_id != g_menu_view_id &&
                    rec.browser_engine != desired) {
                    targets.push_back(TargetView{ handle, *rec.manager_id, rec.initial_url });
                }
            }
        }
        if (targets.empty()) {
            return;
        }
        const auto* router = dynamic_cast<BrowserRuntimeRouter*>(g_browser_runtime.get());
        for (const auto& item : targets) {
            const auto* cfg_ptr = g_view_manager->Config(item.manager_id);
            if (cfg_ptr == nullptr) continue;
            auto next_cfg = *cfg_ptr;
            next_cfg.bridge_event_queue = NativeEventsForView(item.manager_id);
            next_cfg.bridge_response_queue = NativeEventsForView(item.manager_id, true);
            next_cfg.browser_engine = desired;
            skyultra::RemoveActorPreview(item.manager_id);
            (void)g_view_manager->Remove(item.manager_id);
            const auto new_id = g_view_manager->Add(next_cfg, [&](skyultra::ViewId id) {
                return g_browser_runtime->CreateView(id, next_cfg, item.initial_url);
            });
            if (!new_id) continue;
            skyultra::BeginOverlayPageLoad(new_id);
            g_game_subscriptions.erase(item.handle);
            skyultra::InvalidateSharedGameDataCache();
            const auto resolved = router != nullptr ? router->ResolvedEngine(desired) : desired;
            {
                const std::scoped_lock lock(g_native_views_mutex);
                if (const auto it = g_native_views.find(item.handle); it != g_native_views.end()) {
                    it->second.manager_id = new_id;
                    it->second.browser_engine = resolved;
                    it->second.pending_page_requests.clear();
                }
            }
            for (auto& [m_handle, overlay] : g_widget_overlays) {
                if (m_handle == mod_handle && overlay.view == item.handle) {
                    overlay.manager_id = new_id;
                }
            }
        }
        PublishOverlayViews();
        SKSE::log::info("SkyUltra switched mod '{}' views to {} independently.",
            mod_id, BrowserEngineName(desired));
    });
}

bool IsItemPreviewRequest(std::string_view name) noexcept
{
    return name=="game.setItemPreview"||name=="game.getItemPreviewStatus"||name=="game.closeItemPreview"||name=="game.saveItemPreview";
}

bool IsActorPreviewRequest(std::string_view name) noexcept
{
    return name == "game.setActorPreview" || name == "game.getActorPreviewStatus" ||
        name == "game.closeActorPreview" || name == "game.saveActorPreview" || name == "game.captureActorAnimations" || name == "game.loadActorPoses" || name == "game.getItemThumbnail" || name == "game.getActorThumbnail" || IsItemPreviewRequest(name);
}

bool IsHostUiRequest(std::string_view name) noexcept
{
    return name == "game.getPlayerSnapshot" || name == "game.subscribe" || name == "game.unsubscribe" || name == "game.getInventory" || name == "game.equipItem" || name == "game.unequipItem" || name == "game.itemAction" || name == "game.getBarterSnapshot" || name == "game.barterTransaction" || name == "game.getContainerSnapshot" || name == "game.containerTransfer" || name == "game.getSpells" || name == "game.equipSpell" || name == "game.renameSpell" || name == "game.deleteSpell" || name == "game.getFavorites" || name == "game.setFavorite" || name == "game.getNearbyNpcs" || name == "game.searchNpcs" ||
        name == "ui.getSplashStatus" || name == "ui.replaySplash" || name == "ui.setCursor" || name == "ui.close" || name == "ui.getSettings" || name == "ui.updateSettings" || name == "ui.pageReady" || name == "ui.configureWidgets" || name == "ui.setInputScope" || name == "ui.getPerformanceSnapshot" ||
        name == "ui.getDeveloperSnapshot" || name == "ui.reloadModViews" || name == "ui.exportHudMod" || name == "ui.manageDiagnosticSample" ||
        name == "ui.playSound" || name == "ui.projectWorldAnchors" || name == "ui.setSpatialPlacement" || name == "ui.toggleDomInspector" || name == "ui.interopCall";
}

std::uint32_t ActorPreviewFormId(const nlohmann::json& params, const char* key)
{
    const auto field = params.find(key);
    if (field == params.end()) return 0;
    if (field->is_number_unsigned()) {
        const auto value = field->get<std::uint64_t>();
        return value <= UINT32_MAX ? static_cast<std::uint32_t>(value) : 0;
    }
    if (field->is_number_integer()) {
        const auto value = field->get<std::int64_t>();
        return value > 0 && value <= UINT32_MAX ? static_cast<std::uint32_t>(value) : 0;
    }
    if (!field->is_string()) return 0;
    auto text = std::string_view(field->get_ref<const std::string&>());
    auto radix = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
        radix = 16;
    }
    std::uint32_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, radix);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? value : 0;
}

double ActorPreviewNumber(const nlohmann::json& params, const char* key,
    double fallback, double minimum, double maximum)
{
    const auto field = params.find(key);
    if (field == params.end() || !field->is_number()) return fallback;
    const auto value = field->get<double>();
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

bool ActorPreviewBoolean(const nlohmann::json& params, const char* key, bool fallback)
{
    const auto field = params.find(key);
    return field != params.end() && field->is_boolean() ? field->get<bool>() : fallback;
}

std::string ApplyActorRenderer(skyultra::ViewId view_id,std::string_view method,std::string_view params_json)
{
    if(method=="game.captureActorAnimations"||method=="game.loadActorPoses"){
        if(!nlohmann::json::parse(params_json,nullptr,false).is_object())
            throw std::invalid_argument("Actor animation capture options must be an object.");
        const auto captured=skyultra::CaptureActorPreviewAnimations(view_id,method=="game.loadActorPoses");
        auto clips=nlohmann::json::array();
        for(const auto& clip:captured.clips){
            auto entry=nlohmann::json{{"id",clip.id},{"name",clip.name},{"duration",clip.duration},{"sampleCount",clip.sample_count}};
            if(!clip.preset.empty())entry["preset"]=clip.preset;
            clips.push_back(std::move(entry));
        }
        return nlohmann::json{{"available",captured.available},{"clips",std::move(clips)},{"message",captured.message}}.dump();
    }
    auto status = skyultra::GetActorPreviewStatus(view_id);
    const auto item=IsItemPreviewRequest(method);
    if (method == "game.closeActorPreview"||method=="game.closeItemPreview") {
        if(status.item_preview==item)skyultra::RemoveActorPreview(view_id);
        status = skyultra::GetActorPreviewStatus(view_id);
    } else if (method == "game.setActorPreview"||method=="game.setItemPreview") {
        const auto params = nlohmann::json::parse(params_json, nullptr, false);
        if (params.is_discarded() || !params.is_object()) {
            throw std::invalid_argument("Actor preview parameters must be a JSON object.");
        }
        skyultra::ActorPreviewRequest request;
        request.item_preview=item;request.item_form_id=item?ActorPreviewFormId(params,"formId"):0;
        request.reference_id = ActorPreviewFormId(params, "referenceId");
        request.base_form_id = ActorPreviewFormId(params, "baseFormId");
        request.visible = ActorPreviewBoolean(params, "visible", true);
        request.live = ActorPreviewBoolean(params, "live", true);
        request.portrait = ActorPreviewBoolean(params, "portrait", false);
        request.refresh = ActorPreviewBoolean(params, "refresh", false);
        request.ignore_world_fade = !item && ActorPreviewBoolean(params, "ignoreWorldFade", false);
        request.yaw = static_cast<float>(ActorPreviewNumber(params, "yaw", 0, -100, 100));
        request.pitch = static_cast<float>(ActorPreviewNumber(params, "pitch", 0, -100, 100));
        request.zoom = static_cast<float>(ActorPreviewNumber(params, "zoom", 1, 0.35, 3));
        request.pan_x = static_cast<float>(ActorPreviewNumber(params, "panX", 0, -2, 2));
        request.pan_y = static_cast<float>(ActorPreviewNumber(params, "panY", 0, -2, 2));
        if(const auto animation=params.find("animation");animation!=params.end()){
            if(!animation->is_object()||!animation->contains("id")||!(*animation)["id"].is_string()||
                !animation->contains("frame")||!(*animation)["frame"].is_number_integer())
                throw std::invalid_argument("Actor animation must contain an id and integer frame.");
            request.animation_id=(*animation)["id"].get<std::string>();
            const auto frame=(*animation)["frame"].get<std::int64_t>();
            if(request.animation_id.empty()||request.animation_id.size()>64||frame<0||frame>=33)
                throw std::invalid_argument("Invalid actor animation id or frame.");
            request.animation_frame=static_cast<std::uint32_t>(frame);
        }
        if (const auto lighting=params.find("lighting");lighting!=params.end()) {
            if(!lighting->is_string())throw std::invalid_argument("Actor preview lighting must be a string.");
            const auto& name=lighting->get_ref<const std::string&>();
            if(name=="studio")request.lighting=skyultra::ActorPreviewLighting::Studio;
            else if(name=="soft")request.lighting=skyultra::ActorPreviewLighting::Soft;
            else if(name=="dramatic")request.lighting=skyultra::ActorPreviewLighting::Dramatic;
            else if(name=="warm")request.lighting=skyultra::ActorPreviewLighting::Warm;
            else if(name=="cool")request.lighting=skyultra::ActorPreviewLighting::Cool;
            else throw std::invalid_argument("Unknown actor preview lighting preset.");
        }
        if (const auto pose=params.find("pose");pose!=params.end()) {
            if(!pose->is_string())throw std::invalid_argument("Actor pose must be a string.");
            const auto value=pose->get<std::string>();
            if(value=="current")request.pose=skyultra::ActorPreviewPose::Current;
            else if(value=="idle")request.pose=skyultra::ActorPreviewPose::Idle;
            else if(value=="fighting")request.pose=skyultra::ActorPreviewPose::Fighting;
            else if(value=="guard")request.pose=skyultra::ActorPreviewPose::Guard;
            else if(value=="spellcasting")request.pose=skyultra::ActorPreviewPose::Spellcasting;
            else throw std::invalid_argument("Unknown actor preview pose.");
        }
        request.size = {
            static_cast<std::uint32_t>(ActorPreviewNumber(params, "width", 512, 128, 1024)),
            static_cast<std::uint32_t>(ActorPreviewNumber(params, "height", 640, 128, 1024))
        };
        if (const auto bounds = params.find("bounds"); bounds != params.end() && bounds->is_object()) {
            request.normalized_bounds = {
                static_cast<float>(ActorPreviewNumber(*bounds, "x", 0, -4, 4)),
                static_cast<float>(ActorPreviewNumber(*bounds, "y", 0, -4, 4)),
                static_cast<float>(ActorPreviewNumber(*bounds, "width", 0, 0, 4)),
                static_cast<float>(ActorPreviewNumber(*bounds, "height", 0, 0, 4))
            };
        }
        if(!request.visible&&status.item_preview!=item){
            status={false,false,0,0,0,item?"No item preview is attached to this view.":"No actor preview is attached to this view."};
        }else {
            status = skyultra::SetActorPreview(view_id, request);
            // Early validation failures have no stored stage. Preserve their
            // message in this item response instead of replacing it below.
            if(item){status.item_preview=true;status.item_form_id=request.item_form_id;}
        }
    }
    if(status.item_preview!=item)status={false,false,0,0,0,item?"No item preview is attached to this view.":"No actor preview is attached to this view."};
    if(item)return nlohmann::json{{"available",status.available},{"ready",status.ready},
        {"formId",std::to_string(status.item_form_id)},{"geometryCount",status.geometry_count},
        {"skippedGeometryCount",status.skipped_geometry_count},{"message",status.message}}.dump();
    return nlohmann::json{
        { "available", status.available }, { "ready", status.ready },
        { "referenceId", std::to_string(status.reference_id) },
        { "geometryCount", status.geometry_count },
        { "skippedGeometryCount", status.skipped_geometry_count },
        { "posesAvailable", status.poses_available },
        { "message", status.message }
    }.dump();
}

bool HandleActorPreviewRequest(SkyUltraViewHandle handle,const skyultra::PageBridgeMessage& message)
{
    if(message.name=="game.getItemThumbnail"){
        const auto params=nlohmann::json::parse(message.payload_json,nullptr,false);
        if(!params.is_object())throw std::invalid_argument("Item thumbnail options must be an object.");
        const auto form_id=ActorPreviewFormId(params,"formId");
        const auto use_preview=ActorPreviewBoolean(params,"usePreview",false);
        if(!form_id)throw std::invalid_argument("An item form ID is required.");
        skyultra::CaptureItemThumbnail(message.view_id,form_id,[handle,id=message.request_id](bool available,std::string detail){
            const auto payload=nlohmann::json{{"available",available},{available?"dataUrl":"message",std::move(detail)}}.dump();
            (void)NativeApiCompletePageRequest(handle,id.data(),static_cast<std::uint32_t>(id.size()),1,payload.data(),static_cast<std::uint32_t>(payload.size()),nullptr,0);
        },use_preview);
        return true;
    }
    if(message.name=="game.getActorThumbnail"){
        const auto params=nlohmann::json::parse(message.payload_json,nullptr,false);
        if(!params.is_object())throw std::invalid_argument("Actor thumbnail options must be an object.");
        const auto reference_id=ActorPreviewFormId(params,"referenceId");
        const auto base_form_id=ActorPreviewFormId(params,"baseFormId");
        const auto use_preview=ActorPreviewBoolean(params,"usePreview",false);
        if(!reference_id&&!base_form_id)throw std::invalid_argument("An actor referenceId or baseFormId is required.");
        // Portraits captured from a studio view (camera button) override the automatic
        // portrait for that actor in every view (HUD, overview, player card, sheets).
        static std::mutex s_portrait_mutex;
        static std::unordered_map<std::uint64_t,std::string> s_portrait_overrides;
        const auto portrait_key=(static_cast<std::uint64_t>(reference_id)<<32U)|base_form_id;
        const auto ref_only_key=static_cast<std::uint64_t>(reference_id)<<32U;
        if(!use_preview){
            std::string cached;
            {
                const std::scoped_lock lock(s_portrait_mutex);
                if(auto it=s_portrait_overrides.find(portrait_key);it!=s_portrait_overrides.end())cached=it->second;
                else if(reference_id){
                    if(auto ref_it=s_portrait_overrides.find(ref_only_key);ref_it!=s_portrait_overrides.end())cached=ref_it->second;
                }
            }
            if(!cached.empty()){
                const auto payload=nlohmann::json{{"available",true},{"dataUrl",std::move(cached)},{"custom",true}}.dump();
                (void)NativeApiCompletePageRequest(handle,message.request_id.data(),static_cast<std::uint32_t>(message.request_id.size()),1,payload.data(),static_cast<std::uint32_t>(payload.size()),nullptr,0);
                return true;
            }
        }
        skyultra::CaptureActorThumbnail(message.view_id,reference_id,base_form_id,[handle,id=message.request_id,use_preview,portrait_key,ref_only_key,reference_id,base_form_id](bool available,std::string detail){
            if(available&&use_preview&&!detail.empty()){
                {
                    const std::scoped_lock lock(s_portrait_mutex);
                    s_portrait_overrides[portrait_key]=detail;
                    if(reference_id)s_portrait_overrides[ref_only_key]=detail;
                }
                if(auto* tasks=SKSE::GetTaskInterface()){
                    tasks->AddTask([reference_id,base_form_id,data_url=detail](){
                        if(g_view_manager==nullptr)return;
                        const auto event=nlohmann::json{
                            {"referenceId",std::to_string(reference_id)},
                            {"baseFormId",std::to_string(base_form_id)},
                            {"isPlayer",reference_id==0x14U},
                            {"dataUrl",data_url}}.dump();
                        for(const auto& snapshot:g_view_manager->OrderedViews()){
                            if(auto* view=g_view_manager->Find(snapshot.id))(void)view->SendEventJson("skyultra.portraitUpdated",event);
                        }
                    });
                }
            }
            const auto payload=nlohmann::json{{"available",available},{available?"dataUrl":"message",std::move(detail)}}.dump();
            (void)NativeApiCompletePageRequest(handle,id.data(),static_cast<std::uint32_t>(id.size()),1,payload.data(),static_cast<std::uint32_t>(payload.size()),nullptr,0);
        },use_preview);
        return true;
    }
    if(message.name=="game.saveActorPreview"||message.name=="game.saveItemPreview"){
        if(skyultra::GetActorPreviewStatus(message.view_id).item_preview!=IsItemPreviewRequest(message.name))
            throw std::invalid_argument("No matching actor/item preview is attached to this view.");
        const auto params=nlohmann::json::parse(message.payload_json,nullptr,false);
        if(!params.is_object())throw std::invalid_argument("Actor PNG options must be an object.");
        const auto long_edge=static_cast<std::uint32_t>(ActorPreviewNumber(params,"longEdge",0,720,4096));
        skyultra::SaveActorPreviewPng(message.view_id,[handle,id=message.request_id](bool saved,std::string detail){
            const auto payload=nlohmann::json{{"saved",saved},{saved?"path":"message",detail}}.dump();
            (void)NativeApiCompletePageRequest(handle,id.data(),static_cast<std::uint32_t>(id.size()),1,payload.data(),static_cast<std::uint32_t>(payload.size()),nullptr,0);
        },long_edge);
        return true;
    }
    const auto payload=ApplyActorRenderer(message.view_id,message.name,message.payload_json);
    (void)NativeApiCompletePageRequest(handle,message.request_id.data(),static_cast<std::uint32_t>(message.request_id.size()),1,payload.data(),static_cast<std::uint32_t>(payload.size()),nullptr,0);
    return true;
}

void ProcessPageBridgeMessage(skyultra::PageBridgeMessage message)
{
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return;
    }
    const auto handle = NativeHandleForManagerView(message.view_id);
    if (!handle || g_view_manager == nullptr) {
        return;
    }

    if (message.kind == skyultra::PageBridgeMessageKind::Cancel) {
        SkyUltraPageRequestCallbackV2 callback{};
        void* user_data{};
        {
            const std::scoped_lock lock(g_native_views_mutex);
            const auto view = g_native_views.find(*handle);
            if (view == g_native_views.end()) {
                return;
            }
            const auto request = view->second.pending_page_requests.find(message.request_id);
            if (request == view->second.pending_page_requests.end() ||
                request->second.completion_queued) {
                return;
            }
            callback = request->second.callback;
            user_data = request->second.user_data;
            view->second.pending_page_requests.erase(request);
        }
        RecordDevBridgeEntry(message.view_id, ModIdForViewHandle(*handle), "cancel",
            message.name.empty() ? message.request_id : message.name, "ok", 0.0, 0);
        if (callback != nullptr) {
            try {
                callback(user_data, *handle, message.request_id.c_str(),
                    static_cast<std::uint32_t>(message.request_id.size()), nullptr, 0, nullptr, 0,
                    SKYULTRA_PAGE_REQUEST_CANCELLED_V2);
            } catch (...) {
                SKSE::log::error("SkyUltra page request cancellation callback threw for '{}' on view {}.",
                    message.request_id, *handle);
            }
        } else if (skyultra::IsPapyrusView(*handle)) {
            (void)skyultra::DispatchPapyrusPageRequest(
                *handle, message.request_id, {}, {}, true);
        }
        return;
    }

    const bool papyrus_view = skyultra::IsPapyrusView(*handle);
    SkyUltraPageRequestCallbackV2 callback{};
    void* user_data{};
    if (!papyrus_view) {
        const std::scoped_lock lock(g_page_request_callback_mutex);
        callback = g_page_request_callback;
        user_data = g_page_request_user_data;
    }
    std::string rejection;
    bool permission_denied{};
    bool duplicate_request_id{};
    {
        constexpr std::size_t kMaxPendingRequestsPerView = 128;
        const std::scoped_lock lock(g_native_views_mutex);
        const auto view = g_native_views.find(*handle);
        if (view == g_native_views.end()) {
            return;
        }
        if (!view->second.javascript_enabled) {
            rejection = "JavaScript is disabled for this SkyUltra view.";
        } else if (view->second.state != SKYULTRA_VIEW_READY) {
            rejection = "The SkyUltra view is not ready.";
        } else if (!view->second.allowed_page_request_methods.contains(message.name)) {
            rejection = "The SkyUltra page request method '" + message.name + "' is not allowed for this view.";
            permission_denied = true;
        } else if (view->second.pending_page_requests.contains(message.request_id)) {
            duplicate_request_id = true;
        } else if (view->second.pending_page_requests.size() >= kMaxPendingRequestsPerView) {
            rejection = "The SkyUltra view already has too many pending requests.";
        } else if (callback == nullptr && !papyrus_view && !IsActorPreviewRequest(message.name) && !IsHostUiRequest(message.name)) {
            rejection = "No SkyUltra page request callback is registered.";
        } else {
            view->second.pending_page_requests.emplace(message.request_id,
                NativeApiViewRecord::PendingPageRequest{ callback, user_data, false });
        }
    }
    if (duplicate_request_id) {
        SKSE::log::warn("SkyUltra ignored duplicate page request ID '{}' on view {}.",
            message.request_id, message.view_id);
        return;
    }
    if (!rejection.empty()) {
        RecordDevBridgeEntry(message.view_id, ModIdForViewHandle(*handle),
            permission_denied ? "permission-denied" : "error", message.name,
            permission_denied ? "denied" : "error", 0.0, message.payload_json.size(), rejection);
        if (auto* view = g_view_manager->Find(message.view_id); view != nullptr) {
            (void)view->ResolveRequest(message.request_id, false, {}, rejection);
        }
        return;
    }

    if (IsActorPreviewRequest(message.name) || IsHostUiRequest(message.name)) {
        const auto started = std::chrono::steady_clock::now();
        try {
            if (IsActorPreviewRequest(message.name)) (void)HandleActorPreviewRequest(*handle, message);
            else (void)HandleHostUiRequest(*handle, message);
            const auto elapsed_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            RecordDevBridgeEntry(message.view_id, ModIdForViewHandle(*handle), "request",
                message.name, "ok", elapsed_ms, message.payload_json.size());
        } catch (const std::exception& exception) {
            const auto elapsed_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            const auto error = std::string("SkyUltra host request failed: ") + exception.what();
            RecordDevBridgeEntry(message.view_id, ModIdForViewHandle(*handle), "error",
                message.name, "error", elapsed_ms, message.payload_json.size(), error);
            (void)NativeApiCompletePageRequest(*handle, message.request_id.data(),
                static_cast<std::uint32_t>(message.request_id.size()), 0, nullptr, 0,
                error.data(), static_cast<std::uint32_t>(error.size()));
        }
        return;
    }

    if (papyrus_view) {
        if (!skyultra::DispatchPapyrusPageRequest(*handle, message.request_id,
                message.name, message.payload_json, false)) {
            static constexpr char kPapyrusFailure[] =
                "SkyUltra could not deliver this request to Papyrus.";
            (void)NativeApiCompletePageRequest(*handle, message.request_id.c_str(),
                static_cast<std::uint32_t>(message.request_id.size()), 0, nullptr, 0,
                kPapyrusFailure, static_cast<std::uint32_t>(sizeof(kPapyrusFailure) - 1));
        }
        return;
    }

    try {
        callback(user_data, *handle, message.request_id.c_str(),
            static_cast<std::uint32_t>(message.request_id.size()), message.name.c_str(),
            static_cast<std::uint32_t>(message.name.size()), message.payload_json.c_str(),
            static_cast<std::uint32_t>(message.payload_json.size()), SKYULTRA_PAGE_REQUEST_V2);
    } catch (...) {
        static constexpr char kCallbackFailure[] = "The native page request callback threw an exception.";
        (void)NativeApiCompletePageRequest(*handle, message.request_id.c_str(),
            static_cast<std::uint32_t>(message.request_id.size()), 0, nullptr, 0,
            kCallbackFailure, static_cast<std::uint32_t>(sizeof(kCallbackFailure) - 1));
        SKSE::log::error("SkyUltra page request callback threw for '{}' on view {}.",
            message.name, *handle);
    }
}

void OnBrowserPageBridgeMessage(skyultra::PageBridgeMessage message)
{
    if (!QueueNativeApiTask([message = std::move(message)]() mutable {
            ProcessPageBridgeMessage(std::move(message));
        })) {
        SKSE::log::warn("SkyUltra could not queue a page request for Skyrim's game thread.");
    }
}

SkyUltraResult SKYULTRA_CALL NativeApiGetViewState(
    SkyUltraViewHandle handle, SkyUltraViewStateV1* out_state)
{
    if (handle == 0 || out_state == nullptr) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    const std::scoped_lock lock(g_native_views_mutex);
    const auto it = g_native_views.find(handle);
    if (it == g_native_views.end()) {
        return SKYULTRA_NOT_FOUND;
    }
    *out_state = it->second.state;
    return SKYULTRA_OK;
}

void SetNativeApiViewFailed(SkyUltraViewHandle handle)
{
    {
        const std::scoped_lock lock(g_native_views_mutex);
        if (const auto it = g_native_views.find(handle); it != g_native_views.end() &&
            it->second.state != SKYULTRA_VIEW_CLOSING) {
            it->second.state = SKYULTRA_VIEW_FAILED;
            it->second.bridge_events->Close();
            it->second.bridge_responses->Close();
            if(it->second.pending_config)skyultra::CancelFrameworkSplash(it->second.pending_config->id);
            it->second.pending_config.reset();
        }
    }
    (void)QueueNativeApiTask([] { PublishOverlayViews(); });
}

void CreateNativeApiViewOnGameThread(
    SkyUltraViewHandle handle, skyultra::ViewConfig config, std::string initial_url)
{
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto it = g_native_views.find(handle);
        if (it == g_native_views.end() || it->second.state == SKYULTRA_VIEW_CLOSING) {
            return;
        }
        config.bridge_event_queue = it->second.bridge_events;
        config.bridge_response_queue = it->second.bridge_responses;
        if (it->second.pending_config) config.visible = it->second.pending_config->visible;
    }

    if (g_view_manager == nullptr || g_browser_runtime == nullptr) {
        SetNativeApiViewFailed(handle);
        return;
    }

    if (config.visible && !config.click_through && config.layer != skyultra::ViewLayer::Hud && !g_modal_menu_open) {
        // An explicit native open reveals that view, without reviving other mods.
        g_browser_overlay_suppressed.store(false, std::memory_order_relaxed);
    }
    const bool suspend_on_create = config.visible && g_browser_overlay_suppressed.load(std::memory_order_relaxed) &&
        !(config.layer == skyultra::ViewLayer::Hud && config.click_through);
    if (suspend_on_create) config.visible = false;
    const auto view_id = g_view_manager->Add(config, [&](skyultra::ViewId id) {
        return g_browser_runtime->CreateView(id, config, initial_url);
    });
    const auto* router = dynamic_cast<BrowserRuntimeRouter*>(g_browser_runtime.get());
    const auto actual_engine = router != nullptr ? router->ResolvedEngine(config.browser_engine) :
        g_browser_runtime->Engine();
    {
        const std::scoped_lock lock(g_native_views_mutex);
        if (const auto it = g_native_views.find(handle); it != g_native_views.end()) {
            it->second.manager_id = view_id == 0 ? std::nullopt :
                std::optional<skyultra::ViewId>(view_id);
            it->second.browser_engine = actual_engine;
            it->second.pending_config.reset();
            if (view_id != 0 && it->second.wait_for_page_ready && !config.transparent) {
                skyultra::BeginOverlayPageLoad(view_id);
            }
            if (it->second.state != SKYULTRA_VIEW_CLOSING) {
                it->second.state = view_id == 0 ? SKYULTRA_VIEW_FAILED : SKYULTRA_VIEW_READY;
                if (view_id == 0) {
                    it->second.bridge_events->Close();
                    it->second.bridge_responses->Close();
                }
            }
        }
    }
    if (view_id == 0) {
        SKSE::log::error("SkyUltra native API could not create view '{}'.", config.id);
        PublishOverlayViews();
        return;
    }
    if (suspend_on_create) {
        if (g_modal_menu_open) g_suspended_panel_views.insert(view_id);
        if (auto* view = g_view_manager->Find(view_id)) (void)view->SendEventJson("skyultra.visibility",R"({"visible":false})");
    }
    PublishOverlayViews();
    SyncSkyUltraWorldPause();
    PublishBrowserCursor(HasVisibleInteractiveView());
}

std::filesystem::path PathFromUtf8(std::string_view utf8)
{
    return std::filesystem::path(std::u8string_view(
        reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

void CollectHtmlMissingAssets(
    const std::filesystem::path& root_dir,
    const std::filesystem::path& html_file,
    std::vector<std::string>& missing_out)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(html_file, ec) || ec) {
        return;
    }
    std::ifstream input(html_file, std::ios::binary);
    if (!input) {
        return;
    }
    std::string html;
    html.resize(131072U);
    input.read(html.data(), static_cast<std::streamsize>(html.size()));
    html.resize(static_cast<std::size_t>(input.gcount()));

    const auto scan_attr = [&](std::string_view needle) {
        std::size_t pos = 0;
        while (pos < html.size() && missing_out.size() < 16) {
            const auto found = html.find(needle, pos);
            if (found == std::string::npos) break;
            const auto val_start = found + needle.size();
            if (val_start >= html.size()) break;
            const char quote = needle.back();
            const auto val_end = html.find(quote, val_start);
            if (val_end == std::string::npos) break;
            pos = val_end + 1;
            std::string_view raw_ref(html.data() + val_start, val_end - val_start);
            if (const auto q = raw_ref.find_first_of("?#"); q != std::string_view::npos) {
                raw_ref = raw_ref.substr(0, q);
            }
            while (!raw_ref.empty() && (raw_ref.front() == ' ' || raw_ref.front() == '\t')) raw_ref.remove_prefix(1);
            while (!raw_ref.empty() && (raw_ref.back() == ' ' || raw_ref.back() == '\t')) raw_ref.remove_suffix(1);
            if (raw_ref.empty() || raw_ref.starts_with("//") || raw_ref.starts_with("/") ||
                raw_ref.find(':') != std::string_view::npos) {
                continue;
            }
            if (raw_ref.starts_with("./")) {
                raw_ref.remove_prefix(2);
            }
            if (raw_ref.empty()) continue;
            std::error_code ref_ec;
            const auto candidate = std::filesystem::weakly_canonical(
                html_file.parent_path() / PathFromUtf8(raw_ref), ref_ec);
            if (ref_ec) continue;
            const auto rel = candidate.lexically_relative(root_dir);
            if (rel.empty() || rel.is_absolute() || *rel.begin() == L"..") {
                continue;
            }
            if (!std::filesystem::exists(candidate, ref_ec)) {
                auto rel_utf8 = PathUtf8(rel);
                std::replace(rel_utf8.begin(), rel_utf8.end(), '\\', '/');
                if (std::find(missing_out.begin(), missing_out.end(), rel_utf8) == missing_out.end()) {
                    missing_out.push_back(std::move(rel_utf8));
                }
            }
        }
    };
    scan_attr("src=\"");
    scan_attr("src='");
    scan_attr("href=\"");
    scan_attr("href='");
}

std::string BuildDeveloperSnapshotJson()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    (void)K32GetProcessMemoryInfo(
        GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));

    const auto perf = skyultra::GetOverlayPerformanceSnapshot();

    struct ViewSnapItem {
        SkyUltraViewHandle handle{};
        std::optional<skyultra::ViewId> manager_id;
        SkyUltraModHandle mod_handle{};
        skyultra::BrowserEngine engine{ skyultra::BrowserEngine::Ultralight };
        std::uint32_t input_scope_depth{};
        std::size_t pending_requests{};
        std::optional<skyultra::ViewConfig> pending_config;
        std::vector<std::string> allowed_methods;
        std::string initial_url;
        skyultra::BridgeEventQueue::Stats event_queue, response_queue;
    };
    std::vector<ViewSnapItem> raw_views;
    {
        const std::scoped_lock lock(g_native_views_mutex);
        raw_views.reserve(g_native_views.size());
        for (const auto& [v_handle, rec] : g_native_views) {
            if (rec.state == SKYULTRA_VIEW_CLOSING) continue;
            std::vector<std::string> methods(
                rec.allowed_page_request_methods.begin(), rec.allowed_page_request_methods.end());
            std::sort(methods.begin(), methods.end());
            raw_views.push_back(ViewSnapItem{
                v_handle,
                rec.manager_id,
                rec.mod_handle,
                rec.browser_engine,
                rec.input_scope_depth,
                rec.pending_page_requests.size(),
                rec.pending_config,
                std::move(methods),
                rec.initial_url,
                rec.bridge_events->Snapshot(),
                rec.bridge_responses->Snapshot()
            });
        }
    }
    std::sort(raw_views.begin(), raw_views.end(),
        [](const ViewSnapItem& a, const ViewSnapItem& b) { return a.handle < b.handle; });

    std::unordered_map<SkyUltraModHandle, skyultra::ModManifest> registered_by_handle;
    std::unordered_map<std::string, skyultra::ModManifest> registered_by_id;
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        for (const auto& [m_handle, m_rec] : g_native_mods) {
            registered_by_handle.emplace(m_handle, m_rec.manifest);
            if (!m_rec.manifest.id.empty()) {
                registered_by_id.emplace(m_rec.manifest.id, m_rec.manifest);
            }
        }
    }

    std::unordered_map<std::string, std::uint32_t> mod_view_counts;
    std::unordered_map<std::string, std::uint64_t> mod_surface_bytes;
    auto views_json = nlohmann::json::array();
    for (const auto& v : raw_views) {
        std::string mod_id = "unregistered";
        if (const auto m_it = registered_by_handle.find(v.mod_handle); m_it != registered_by_handle.end() && !m_it->second.id.empty()) {
            mod_id = m_it->second.id;
        } else if (v.manager_id && *v.manager_id == g_menu_view_id && g_menu_view_id != 0) {
            mod_id = "skyultra.menu";
        }
        const skyultra::ViewConfig* live_cfg =
            (v.manager_id && g_view_manager != nullptr) ? g_view_manager->Config(*v.manager_id) : nullptr;
        const skyultra::ViewConfig fallback_cfg = v.pending_config.value_or(skyultra::ViewConfig{});
        const auto& cfg = live_cfg != nullptr ? *live_cfg : fallback_cfg;
        const std::uint64_t surf_bytes =
            static_cast<std::uint64_t>(cfg.size.width) * static_cast<std::uint64_t>(cfg.size.height) * 4ULL;
        ++mod_view_counts[mod_id];
        mod_surface_bytes[mod_id] += surf_bytes;

        const auto queue_json = [](const skyultra::BridgeEventQueue::Stats& stats) {
            return nlohmann::json{{"capacity", skyultra::BridgeEventQueue::kMaxPendingEvents},
                {"byteLimit", skyultra::BridgeEventQueue::kMaxPendingBytes},
                {"pending", stats.pending - stats.controls}, {"controlPending", stats.controls},
                {"inFlight", stats.in_flight}, {"bytes", stats.bytes},
                {"rejected", stats.rejected}, {"delivered", stats.delivered}, {"cancelled", stats.cancelled},
                {"coalesced", stats.coalesced}, {"oldestMs", stats.oldest_ms}, {"maxDispatchMs", stats.max_dispatch_ms}};
        };
        views_json.push_back({
            { "handle", v.handle },
            { "viewId", v.manager_id.value_or(0) },
            { "configId", cfg.id },
            { "modId", mod_id },
            { "engine", v.engine == skyultra::BrowserEngine::CEF ? "cef" : "ultralight" },
            { "layer", cfg.layer == skyultra::ViewLayer::Hud ? "hud" : "panel" },
            { "visible", cfg.visible },
            { "transparent", cfg.transparent },
            { "clickThrough", cfg.click_through },
            { "modal", cfg.modal },
            { "width", cfg.size.width },
            { "height", cfg.size.height },
            { "fpsLimit", cfg.max_fps },
            { "eventQueue", queue_json(v.event_queue) },
            { "responseQueue", queue_json(v.response_queue) },
            { "inputScopeDepth", v.input_scope_depth },
            { "pendingRequests", v.pending_requests },
            { "surfaceBytes", surf_bytes },
            { "allowedMethods", v.allowed_methods },
            { "initialUrl", v.initial_url }
        });
    }

    std::unordered_map<std::string, DevModRuntimeStats> stats_copy;
    auto bridge_json = nlohmann::json::array();
    std::string last_reloaded_copy;
    {
        const std::scoped_lock lock(g_dev_diagnostics_mutex);
        stats_copy = g_dev_mod_stats;
        last_reloaded_copy = g_last_reloaded_mod;
        for (const auto& entry : g_dev_bridge_log) {
            nlohmann::json item{
                { "seq", entry.seq },
                { "timeMs", entry.time_ms },
                { "viewId", entry.view_id },
                { "modId", entry.mod_id },
                { "kind", entry.kind },
                { "method", entry.method },
                { "status", entry.status },
                { "durationMs", entry.duration_ms },
                { "payloadBytes", entry.payload_bytes }
            };
            if (!entry.message.empty()) {
                item["message"] = entry.message;
            }
            bridge_json.push_back(std::move(item));
        }
    }

    const auto build_mod_json = [&](const skyultra::ModManifest* manifest,
                                    const std::string& fallback_id,
                                    const std::string& folder_name,
                                    bool is_registered,
                                    bool manifest_valid,
                                    const std::string& manifest_error) {
        const std::string id = manifest != nullptr && !manifest->id.empty() ? manifest->id : fallback_id;
        std::string entry_rel = "index.html";
        bool entry_exists = false;
        bool hud_exists = false;
        std::uint32_t asset_count = 0;
        std::uint64_t total_asset_bytes = 0;
        std::vector<std::string> missing_assets;
        std::vector<std::string> page_reqs;
        if (manifest != nullptr) {
            std::error_code ec;
            const auto rel_entry = manifest->entry_file.lexically_relative(manifest->root_directory);
            if (!rel_entry.empty()) {
                entry_rel = PathUtf8(rel_entry);
                std::replace(entry_rel.begin(), entry_rel.end(), '\\', '/');
            }
            entry_exists = std::filesystem::is_regular_file(manifest->entry_file, ec) && !ec;
            const auto hud_path = manifest->entry_file.parent_path() / L"hud.html";
            hud_exists = std::filesystem::is_regular_file(hud_path, ec) && !ec;
            for (std::filesystem::recursive_directory_iterator it(
                     manifest->root_directory, std::filesystem::directory_options::skip_permission_denied, ec),
                 end;
                 !ec && it != end && asset_count < 512;
                 it.increment(ec)) {
                const auto fname = PathUtf8(it->path().filename());
                if (it->is_directory(ec) && (fname == "node_modules" || fname == ".git")) {
                    it.disable_recursion_pending();
                    continue;
                }
                if (it->is_regular_file(ec) && !ec) {
                    ++asset_count;
                    const auto fsize = it->file_size(ec);
                    if (!ec) total_asset_bytes += fsize;
                }
            }
            if (entry_exists) {
                CollectHtmlMissingAssets(manifest->root_directory, manifest->entry_file, missing_assets);
            }
            if (hud_exists) {
                CollectHtmlMissingAssets(manifest->root_directory, hud_path, missing_assets);
            }
            page_reqs.assign(manifest->page_request_methods.begin(), manifest->page_request_methods.end());
            std::sort(page_reqs.begin(), page_reqs.end());
        }
        const auto st_it = stats_copy.find(id);
        const DevModRuntimeStats st = st_it != stats_copy.end() ? st_it->second : DevModRuntimeStats{};
        nlohmann::json item{
            { "id", id },
            { "name", manifest != nullptr && !manifest->name.empty() ? manifest->name : id },
            { "version", manifest != nullptr && !manifest->version.empty() ? manifest->version : "0.0.0" },
            { "folderName", folder_name },
            { "registered", is_registered },
            { "manifestValid", manifest_valid },
            { "entry", entry_rel },
            { "entryExists", entry_exists },
            { "hudExists", hud_exists },
            { "assetCount", asset_count },
            { "totalAssetBytes", total_asset_bytes },
            { "missingAssets", missing_assets },
            { "storage", manifest != nullptr ? manifest->storage_enabled : false },
            { "assetAccess", manifest != nullptr ? manifest->asset_access_enabled : false },
            { "networkHosts", manifest != nullptr ? manifest->network_hosts : std::vector<std::string>{} },
            { "pageRequests", page_reqs },
            { "activeViewCount", mod_view_counts[id] },
            { "estimatedSurfaceBytes", mod_surface_bytes[id] },
            { "bridgeRequestCount", st.bridge_requests },
            { "bridgeErrorCount", st.bridge_errors },
            { "deniedPermissionCount", st.denied_permissions },
            { "hotReloadCount", st.hot_reloads },
            { "findings", nlohmann::json::array() }
        };
        if (!manifest_error.empty()) {
            item["manifestError"] = manifest_error;
        }
        return item;
    };

    auto mods_json = nlohmann::json::array();
    std::unordered_set<std::string> emitted_mods;
    if (const auto mods_dir = skyultra::GetInstalledWebModsDirectory(); !mods_dir.empty()) {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(mods_dir, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code dir_ec;
            if (!it->is_directory(dir_ec) || dir_ec) continue;
            const auto raw_folder = PathUtf8(it->path().filename());
            if (raw_folder.empty() || raw_folder.front() == '.') continue;
            auto lower_id = raw_folder;
            for (auto& ch : lower_id) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            }
            const bool has_manifest_file =
                std::filesystem::is_regular_file(it->path() / L"manifest.json", dir_ec) && !dir_ec;
            const bool has_entry_html =
                (std::filesystem::is_regular_file(it->path() / L"index.html", dir_ec) && !dir_ec) ||
                (std::filesystem::is_regular_file(it->path() / L"Files" / L"index.html", dir_ec) && !dir_ec);
            const bool reserved_output_folder =
                lower_id == "actors" || lower_id == "items" || lower_id == "exports" ||
                lower_id == "screenshots" || lower_id == "cache" || lower_id == "logs";
            if (!has_manifest_file && !has_entry_html &&
                (reserved_output_folder || !skyultra::IsValidModIdentifier(lower_id))) {
                continue;
            }
            std::string manifest_err;
            if (skyultra::IsValidModIdentifier(lower_id)) {
                if (const auto disk_manifest = skyultra::LoadModManifest(mods_dir, lower_id, manifest_err)) {
                    emitted_mods.insert(disk_manifest->id);
                    mods_json.push_back(build_mod_json(
                        &(*disk_manifest),
                        disk_manifest->id,
                        raw_folder,
                        registered_by_id.contains(disk_manifest->id),
                        true,
                        {}));
                    continue;
                }
            } else {
                manifest_err = "Folder name '" + raw_folder + "' is not a valid lowercase mod identifier.";
            }
            emitted_mods.insert(lower_id);
            mods_json.push_back(build_mod_json(
                nullptr, lower_id, raw_folder, registered_by_id.contains(lower_id), false, manifest_err));
        }
    }
    for (const auto& [reg_id, reg_manifest] : registered_by_id) {
        if (emitted_mods.insert(reg_id).second) {
            mods_json.push_back(build_mod_json(
                &reg_manifest,
                reg_id,
                reg_manifest.folder_name.empty() ? reg_id : reg_manifest.folder_name,
                true,
                true,
                {}));
        }
    }

    return nlohmann::json{
        { "available", true },
        { "capturedAtMs", std::round(DevDiagnosticsNowMs() * 10.0) / 10.0 },
        { "processWorkingSetBytes", static_cast<std::uint64_t>(pmc.WorkingSetSize) },
        { "processPrivateBytes", static_cast<std::uint64_t>(pmc.PrivateUsage) },
        { "hotReloadEnabled", g_mod_hot_reload_enabled },
        { "hotReloadTotalCount", g_hot_reload_total_count.load(std::memory_order_relaxed) },
        { "lastReloadedMod", last_reloaded_copy },
        { "hostGpuMs", perf.host_gpu.last_ms },
        { "compositorGpuMs", perf.compositor_gpu.last_ms },
        { "callbackCpuMs", perf.callback_cpu.last_ms },
        { "frameCount", perf.present_interval.samples },
        { "activeSubscriptionCount", g_active_game_subscription_count.load(std::memory_order_relaxed) },
        { "views", std::move(views_json) },
        { "mods", std::move(mods_json) },
        { "bridgeLog", std::move(bridge_json) }
    }.dump();
}

std::string HandleReloadModViewsJson(const nlohmann::json& params)
{
    if (params.is_object()) {
        if (const auto watcher_it = params.find("setHotReloadWatcher");
            watcher_it != params.end() && watcher_it->is_boolean()) {
            const bool enable = watcher_it->get<bool>();
            g_mod_hot_reload_enabled = enable;
            if (enable) {
                StartModHotReloadWatcher();
            } else {
                StopModHotReloadWatcher();
            }
        }
    }

    std::string target_mod;
    if (params.is_object()) {
        if (const auto mod_it = params.find("modId"); mod_it != params.end() && mod_it->is_string()) {
            target_mod = mod_it->get<std::string>();
        }
    }
    if (target_mod.empty()) {
        return nlohmann::json{
            { "reloaded", false },
            { "reloadedViewCount", 0 },
            { "hotReloadEnabled", g_mod_hot_reload_enabled },
            { "message", g_mod_hot_reload_enabled ? "Hot-reload file watcher enabled." : "Hot-reload file watcher paused." }
        }.dump();
    }

    std::vector<std::pair<SkyUltraModHandle, std::string>> targets;
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        for (const auto& [m_handle, m_rec] : g_native_mods) {
            if (target_mod == "all") {
                if (m_rec.manifest.id != "skyultra.menu") {
                    targets.emplace_back(m_handle, m_rec.manifest.id);
                }
            } else if (m_rec.manifest.id == target_mod) {
                targets.emplace_back(m_handle, m_rec.manifest.id);
            }
        }
    }

    const auto mods_dir = skyultra::GetInstalledWebModsDirectory();
    std::uint32_t reloaded_count = 0;
    for (const auto& [m_handle, m_id] : targets) {
        std::optional<skyultra::ModManifest> refreshed;
        if (m_id != "skyultra.menu" && !mods_dir.empty()) {
            std::string err;
            refreshed = skyultra::LoadModManifest(mods_dir, m_id, err);
            if (refreshed) {
                const std::scoped_lock lock(g_native_mods_mutex);
                if (const auto found = g_native_mods.find(m_handle); found != g_native_mods.end()) {
                    found->second.manifest = *refreshed;
                }
            }
        }
        std::vector<SkyUltraViewHandle> handles;
        {
            const std::scoped_lock lock(g_native_views_mutex);
            for (auto& [v_handle, v_rec] : g_native_views) {
                if (v_rec.mod_handle != m_handle || v_rec.state != SKYULTRA_VIEW_READY) continue;
                if (v_rec.manager_id && *v_rec.manager_id == g_menu_view_id) continue;
                if (refreshed) {
                    v_rec.allowed_page_request_methods = refreshed->page_request_methods;
                    v_rec.wait_for_page_ready = refreshed->page_request_methods.contains("ui.pageReady");
                }
                handles.push_back(v_handle);
            }
        }
        for (const auto vh : handles) {
            if (NativeApiReloadView(vh) == SKYULTRA_OK) {
                ++reloaded_count;
            }
        }
        if (!handles.empty()) {
            RecordDevModHotReload(m_id);
        }
    }

    return nlohmann::json{
        { "reloaded", reloaded_count > 0 },
        { "reloadedViewCount", reloaded_count },
        { "hotReloadEnabled", g_mod_hot_reload_enabled },
        { "message", reloaded_count > 0
                ? ("Hot-reloaded " + std::to_string(reloaded_count) + " view(s) for " + target_mod + ".")
                : ("No live non-menu views are currently open for " + target_mod + ".") }
    }.dump();
}

std::string HandleExportHudModJson(const nlohmann::json& params)
{
    if (!params.is_object()) {
        return R"({"exported":false,"modId":"","message":"Export payload must be a JSON object."})";
    }
    const auto id_it = params.find("id");
    const auto files_it = params.find("files");
    if (id_it == params.end() || !id_it->is_string() || files_it == params.end() || !files_it->is_array()) {
        return R"({"exported":false,"modId":"","message":"Export payload requires 'id' and 'files'."})";
    }
    const auto mod_id = id_it->get<std::string>();
    const bool reserved_id =
        mod_id == "skyultra.menu" || mod_id == "actors" || mod_id == "items" ||
        mod_id == "exports" || mod_id == "screenshots" || mod_id == "cache" || mod_id == "logs";
    if (!skyultra::IsValidModIdentifier(mod_id) || reserved_id) {
        return nlohmann::json{
            { "exported", false },
            { "modId", mod_id },
            { "message", "Mod id must be a valid lowercase identifier such as 'test' or 'author.hud' (and cannot use reserved host folder names)." }
        }.dump();
    }
    if (files_it->empty() || files_it->size() > 24) {
        return nlohmann::json{
            { "exported", false },
            { "modId", mod_id },
            { "message", "Mod package must contain between 1 and 24 files." }
        }.dump();
    }

    struct PendingFile {
        std::filesystem::path rel_path;
        std::string content;
    };
    std::vector<PendingFile> validated;
    validated.reserve(files_it->size());
    for (const auto& entry : *files_it) {
        if (!entry.is_object() || !entry.contains("path") || !entry["path"].is_string() ||
            !entry.contains("content") || !entry["content"].is_string()) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Each exported file must provide string 'path' and 'content'." }
            }.dump();
        }
        const auto rel_str = entry["path"].get<std::string>();
        const auto& content_str = entry["content"].get_ref<const std::string&>();
        if (rel_str.empty() || rel_str.size() > 128 || rel_str.find('\0') != std::string::npos ||
            rel_str.find('\\') != std::string::npos || rel_str.find(':') != std::string::npos ||
            content_str.size() > 262144U) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Invalid relative path or file exceeds 256 KB limit: " + rel_str }
            }.dump();
        }
        const auto rel_path = PathFromUtf8(rel_str);
        if (rel_path.empty() || rel_path.is_absolute() || rel_path.has_root_name() || rel_path.has_root_directory()) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Exported file path must be strictly relative: " + rel_str }
            }.dump();
        }
        bool bad_segment = false;
        for (const auto& part : rel_path) {
            const auto part_s = PathUtf8(part);
            if (part_s.empty() || part_s == "." || part_s == "..") {
                bad_segment = true;
                break;
            }
        }
        if (bad_segment) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Path traversal segments are forbidden: " + rel_str }
            }.dump();
        }
        validated.push_back(PendingFile{ rel_path, content_str });
    }

    const auto mods_dir = skyultra::GetInstalledWebModsDirectory();
    if (mods_dir.empty()) {
        return nlohmann::json{
            { "exported", false },
            { "modId", mod_id },
            { "message", "Could not resolve Data/SkyUltra directory." }
        }.dump();
    }
    std::error_code ec;
    const auto normalized_mods = std::filesystem::absolute(mods_dir, ec).lexically_normal();
    if (ec) {
        return nlohmann::json{
            { "exported", false },
            { "modId", mod_id },
            { "message", "Could not resolve Data/SkyUltra: " + ec.message() }
        }.dump();
    }
    std::filesystem::create_directories(normalized_mods, ec);
    const auto requested_root = (normalized_mods / PathFromUtf8(mod_id)).lexically_normal();
    std::filesystem::create_directories(requested_root, ec);
    if (ec || !IsDirectPathChildNamed(normalized_mods, requested_root, mod_id)) {
        return nlohmann::json{
            { "exported", false },
            { "modId", mod_id },
            { "message", "Target mod folder failed Data/SkyUltra containment check." }
        }.dump();
    }
    const auto root_attrs = GetFileAttributesW(requested_root.c_str());
    if (root_attrs == INVALID_FILE_ATTRIBUTES ||
        (root_attrs & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (root_attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return nlohmann::json{
            { "exported", false },
            { "modId", mod_id },
            { "message", "Target mod folder is inaccessible or resolves through a reparse point." }
        }.dump();
    }

    for (const auto& file : validated) {
        const auto dest = (requested_root / file.rel_path).lexically_normal();
        const auto check_rel = dest.lexically_relative(requested_root);
        if (check_rel.empty() || check_rel.is_absolute() || *check_rel.begin() == L"..") {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Destination file escaped mod directory." }
            }.dump();
        }
        std::filesystem::create_directories(dest.parent_path(), ec);
        if (ec) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Could not create subdirectory for " + PathUtf8(file.rel_path) }
            }.dump();
        }
        std::filesystem::path walk = requested_root;
        bool has_reparse = false;
        for (const auto& part : check_rel.parent_path()) {
            if (part.empty() || part == L".") continue;
            walk /= part;
            const auto attrs = GetFileAttributesW(walk.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                has_reparse = true;
                break;
            }
        }
        const auto existing_file_attrs = GetFileAttributesW(dest.c_str());
        if (has_reparse ||
            (existing_file_attrs != INVALID_FILE_ATTRIBUTES &&
             (existing_file_attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0)) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Destination path contains a reparse point." }
            }.dump();
        }
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Could not open file for writing: " + PathUtf8(file.rel_path) }
            }.dump();
        }
        out.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        out.flush();
        if (!out) {
            return nlohmann::json{
                { "exported", false },
                { "modId", mod_id },
                { "message", "Failed while writing file: " + PathUtf8(file.rel_path) }
            }.dump();
        }
    }

    std::error_code display_ec;
    const auto physical_root = std::filesystem::canonical(requested_root, display_ec);
    const auto reported_root = display_ec ? requested_root : physical_root;

    if (!g_mod_hotkeys.contains(mod_id)) {
        std::ifstream manifest_stream(requested_root / L"manifest.json");
        if (manifest_stream) {
            const auto mjson = nlohmann::json::parse(manifest_stream, nullptr, false);
            if (mjson.is_object()) {
                if (const auto hk = mjson.find("hotkey"); hk != mjson.end() && hk->is_object()) {
                    HotkeyBinding default_hk{{}, {}, {}};
                    std::string hk_err;
                    if (ParseHotkeyBinding(*hk, default_hk, true, hk_err)) {
                        g_mod_hotkeys.emplace(mod_id, std::move(default_hk));
                    }
                }
            }
        }
    }

    return nlohmann::json{
        { "exported", true },
        { "modId", mod_id },
        { "directory", PathUtf8(reported_root) },
        { "fileCount", validated.size() },
        { "message", "Exported working mod to Data/SkyUltra/" + mod_id }
    }.dump();
}

std::string HandleManageDiagnosticSampleJson(const nlohmann::json& params)
{
    const std::string sample_id = "samples.broken_mod";
    std::string action;
    if (params.is_object()) {
        if (const auto act_it = params.find("action"); act_it != params.end() && act_it->is_string()) {
            action = act_it->get<std::string>();
        }
    }
    if (action != "deploy-broken" && action != "repair-and-reload" && action != "remove") {
        return nlohmann::json{
            { "accepted", false },
            { "action", action },
            { "modId", sample_id },
            { "message", "Unknown diagnostic sample action." }
        }.dump();
    }

    const auto mods_dir = skyultra::GetInstalledWebModsDirectory();
    if (mods_dir.empty()) {
        return nlohmann::json{
            { "accepted", false },
            { "action", action },
            { "modId", sample_id },
            { "message", "Could not resolve Data/SkyUltra directory." }
        }.dump();
    }
    std::error_code ec;
    std::filesystem::create_directories(mods_dir, ec);
    const auto canonical_mods = std::filesystem::canonical(mods_dir, ec);
    if (ec) {
        return nlohmann::json{
            { "accepted", false },
            { "action", action },
            { "modId", sample_id },
            { "message", "Could not canonicalize Data/SkyUltra." }
        }.dump();
    }

    if (action == "remove") {
        if (g_diagnostic_sample_view != 0) {
            (void)NativeApiDestroyView(g_diagnostic_sample_view);
            g_diagnostic_sample_view = 0;
        }
        {
            const std::scoped_lock lock(g_native_mods_mutex);
            if (const auto found = g_native_mod_handles_by_id.find(sample_id); found != g_native_mod_handles_by_id.end()) {
                g_native_mods.erase(found->second);
                g_native_mod_handles_by_id.erase(found);
            }
        }
        {
            const std::scoped_lock lock(g_dev_diagnostics_mutex);
            g_dev_mod_stats.erase(sample_id);
            std::erase_if(g_dev_bridge_log, [&](const DevBridgeLogEntry& e) { return e.mod_id == sample_id; });
        }
        const auto sample_dir = canonical_mods / PathFromUtf8(sample_id);
        if (std::filesystem::exists(sample_dir, ec)) {
            const auto canon_sample = std::filesystem::canonical(sample_dir, ec);
            if (!ec && IsDirectPathChildNamed(canonical_mods, canon_sample, sample_id)) {
                std::filesystem::remove_all(canon_sample, ec);
            }
        }
        return nlohmann::json{
            { "accepted", true },
            { "action", action },
            { "modId", sample_id },
            { "message", "Removed samples.broken_mod and closed its probe view." }
        }.dump();
    }

    const auto requested_sample = canonical_mods / PathFromUtf8(sample_id);
    std::filesystem::create_directories(requested_sample / L"src", ec);
    std::filesystem::create_directories(requested_sample / L"assets", ec);
    const auto canon_sample = std::filesystem::canonical(requested_sample, ec);
    if (ec || !IsDirectPathChildNamed(canonical_mods, canon_sample, sample_id)) {
        return nlohmann::json{
            { "accepted", false },
            { "action", action },
            { "modId", sample_id },
            { "message", "Diagnostic sample path failed containment check." }
        }.dump();
    }

    const auto write_text = [](const std::filesystem::path& p, std::string_view text) {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        return static_cast<bool>(out);
    };

    constexpr std::string_view kSampleHtml =
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "  <head><meta charset=\"utf-8\" /><title>Diagnostic Sample</title></head>\n"
        "  <body>\n"
        "    <img src=\"./assets/missing-emblem.svg\" alt=\"\" width=\"32\" height=\"32\" />\n"
        "    <script src=\"./src/broken.js\"></script>\n"
        "  </body>\n"
        "</html>\n";

    if (action == "deploy-broken") {
        if (g_diagnostic_sample_view != 0) {
            (void)NativeApiDestroyView(g_diagnostic_sample_view);
            g_diagnostic_sample_view = 0;
        }
        {
            const std::scoped_lock lock(g_dev_diagnostics_mutex);
            g_dev_mod_stats.erase(sample_id);
            std::erase_if(g_dev_bridge_log, [&](const DevBridgeLogEntry& e) { return e.mod_id == sample_id; });
        }
        std::filesystem::remove(canon_sample / L"assets" / L"missing-emblem.svg", ec);

        const auto broken_manifest = nlohmann::json{
            { "schemaVersion", 1 },
            { "id", sample_id },
            { "name", "Broken Diagnostic Sample" },
            { "version", "0.1.0" },
            { "entry", "index.html" },
            { "permissions", {
                { "storage", false },
                { "assetAccess", true },
                { "networkHosts", nlohmann::json::array() },
                { "pageRequests", nlohmann::json::array({ "game.getPlayerSnapshot" }) }
            } }
        }.dump(2);

        constexpr std::string_view kBrokenJs =
            "(function () {\n"
            "  function probe() {\n"
            "    if (!window.SkyWebView || typeof window.SkyWebView.request !== 'function') {\n"
            "      setTimeout(probe, 50);\n"
            "      return;\n"
            "    }\n"
            "    window.SkyWebView.request('game.getInventory', {}).catch(function () {});\n"
            "  }\n"
            "  probe();\n"
            "})();\n";

        if (!write_text(canon_sample / L"manifest.json", broken_manifest) ||
            !write_text(canon_sample / L"index.html", kSampleHtml) ||
            !write_text(canon_sample / L"src" / L"broken.js", kBrokenJs)) {
            return nlohmann::json{
                { "accepted", false },
                { "action", action },
                { "modId", sample_id },
                { "message", "Could not write broken diagnostic sample files." }
            }.dump();
        }
    } else {
        const auto repaired_manifest = nlohmann::json{
            { "schemaVersion", 1 },
            { "id", sample_id },
            { "name", "Broken Diagnostic Sample (Repaired)" },
            { "version", "0.1.1" },
            { "entry", "index.html" },
            { "permissions", {
                { "storage", false },
                { "assetAccess", true },
                { "networkHosts", nlohmann::json::array() },
                { "pageRequests", nlohmann::json::array({ "game.getPlayerSnapshot", "game.getInventory", "ui.pageReady" }) }
            } }
        }.dump(2);

        constexpr std::string_view kEmblemSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 32 32\">\n"
            "  <circle cx=\"16\" cy=\"16\" r=\"14\" fill=\"none\" stroke=\"#d8b36a\" stroke-width=\"2\"/>\n"
            "  <path d=\"M10 16.5l4 4 8-9\" fill=\"none\" stroke=\"#6ee7b7\" stroke-width=\"2.4\" stroke-linecap=\"round\"/>\n"
            "</svg>\n";

        constexpr std::string_view kRepairedJs =
            "(function () {\n"
            "  function probe() {\n"
            "    if (!window.SkyWebView || typeof window.SkyWebView.request !== 'function') {\n"
            "      setTimeout(probe, 50);\n"
            "      return;\n"
            "    }\n"
            "    window.SkyWebView.request('game.getInventory', { limit: 4 }).then(function () {\n"
            "      return window.SkyWebView.request('ui.pageReady', {});\n"
            "    }).catch(function () {});\n"
            "  }\n"
            "  probe();\n"
            "})();\n";

        if (!write_text(canon_sample / L"manifest.json", repaired_manifest) ||
            !write_text(canon_sample / L"index.html", kSampleHtml) ||
            !write_text(canon_sample / L"assets" / L"missing-emblem.svg", kEmblemSvg) ||
            !write_text(canon_sample / L"src" / L"broken.js", kRepairedJs)) {
            return nlohmann::json{
                { "accepted", false },
                { "action", action },
                { "modId", sample_id },
                { "message", "Could not write repaired diagnostic sample files." }
            }.dump();
        }
        {
            const std::scoped_lock lock(g_dev_diagnostics_mutex);
            auto& st = g_dev_mod_stats[sample_id];
            st.denied_permissions = 0;
            st.bridge_errors = 0;
        }
    }

    std::string manifest_err;
    auto loaded_manifest = skyultra::LoadModManifest(canonical_mods, sample_id, manifest_err);
    if (!loaded_manifest) {
        return nlohmann::json{
            { "accepted", false },
            { "action", action },
            { "modId", sample_id },
            { "message", "Sample manifest failed validation: " + manifest_err }
        }.dump();
    }

    SkyUltraModHandle sample_mod_handle{};
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        if (const auto existing = g_native_mod_handles_by_id.find(sample_id);
            existing != g_native_mod_handles_by_id.end()) {
            sample_mod_handle = existing->second;
            g_native_mods[sample_mod_handle].manifest = *loaded_manifest;
        } else {
            sample_mod_handle = g_next_native_mod_handle.fetch_add(1, std::memory_order_relaxed);
            g_native_mod_handles_by_id.emplace(sample_id, sample_mod_handle);
            g_native_mods.emplace(sample_mod_handle, NativeApiModRecord{ *loaded_manifest, {} });
        }
    }

    if (action == "repair-and-reload" && g_diagnostic_sample_view != 0 &&
        NativeHandleAcceptsCommands(g_diagnostic_sample_view)) {
        {
            const std::scoped_lock lock(g_native_views_mutex);
            if (const auto v_it = g_native_views.find(g_diagnostic_sample_view); v_it != g_native_views.end()) {
                v_it->second.allowed_page_request_methods = loaded_manifest->page_request_methods;
                v_it->second.wait_for_page_ready = false;
            }
        }
        (void)NativeApiReloadView(g_diagnostic_sample_view);
        RecordDevModHotReload(sample_id);
        return nlohmann::json{
            { "accepted", true },
            { "action", action },
            { "modId", sample_id },
            { "viewHandle", g_diagnostic_sample_view },
            { "message", "Patched manifest.json + missing SVG asset and hot-reloaded samples.broken_mod in Skyrim." }
        }.dump();
    }

    skyultra::ViewConfig probe_cfg;
    probe_cfg.id = "samples.broken_mod.probe";
    probe_cfg.browser_engine = EffectiveBrowserEngine(DefaultBrowserEngine());
    probe_cfg.size = { 640U, 360U };
    probe_cfg.bounds = { 0.0F, 0.0F, 640.0F, 360.0F };
    probe_cfg.anchor = skyultra::Anchor::TopLeft;
    probe_cfg.layer = skyultra::ViewLayer::Hud;
    probe_cfg.z_index = -200;
    probe_cfg.max_fps = 15;
    probe_cfg.visible = false;
    probe_cfg.transparent = true;
    probe_cfg.click_through = true;
    probe_cfg.modal = false;
    probe_cfg.enable_javascript = true;
    probe_cfg.enable_gamepad = false;
    skyultra::ModContentPolicy sample_policy;
    sample_policy.root_directory = loaded_manifest->root_directory;
    sample_policy.entry_file = loaded_manifest->entry_file;
    sample_policy.asset_access_enabled = loaded_manifest->asset_access_enabled;
    sample_policy.network_hosts = loaded_manifest->network_hosts;
    probe_cfg.mod_content_policy = std::move(sample_policy);

    const auto probe_url = LocalFileUrl(loaded_manifest->entry_file);
    const auto probe_handle = g_next_native_view_handle.fetch_add(1, std::memory_order_relaxed);
    {
        const std::scoped_lock lock(g_native_views_mutex);
        NativeApiViewRecord rec;
        rec.mod_handle = sample_mod_handle;
        rec.initial_url = probe_url;
        rec.javascript_enabled = true;
        rec.pending_config = probe_cfg;
        rec.allowed_page_request_methods = loaded_manifest->page_request_methods;
        g_native_views.emplace(probe_handle, std::move(rec));
    }
    CreateNativeApiViewOnGameThread(probe_handle, probe_cfg, probe_url);
    g_diagnostic_sample_view = probe_handle;
    if (action == "repair-and-reload") {
        RecordDevModHotReload(sample_id);
    }

    return nlohmann::json{
        { "accepted", true },
        { "action", action },
        { "modId", sample_id },
        { "viewHandle", probe_handle },
        { "message", action == "deploy-broken"
                ? "Deployed broken diagnostic sample and mounted live hidden probe view."
                : "Repaired diagnostic sample and reloaded probe view." }
    }.dump();
}

struct SpatialPlacementRecord {
    std::string mode{ "screen" };
    double distance{ 1.25 };
    double scale{ 1.0 };
    std::string bone{ "NPC Head [Head]" };
    std::uint32_t target_ref_id{};
    bool virtual_keyboard{ true };
};

std::unordered_map<SkyUltraViewHandle, SpatialPlacementRecord> g_spatial_placements;
std::unordered_map<skyultra::ViewId, bool> g_dom_inspector_views;

std::string ResolveSkyrimSoundEditorId(std::string_view raw_sound)
{
    if (raw_sound.empty() || raw_sound.size() > 64) {
        return "UIMenuOK";
    }
    if (raw_sound == "focus" || raw_sound == "hover") return "UIMenuFocus";
    if (raw_sound == "ok" || raw_sound == "click" || raw_sound == "select") return "UIMenuOK";
    if (raw_sound == "cancel" || raw_sound == "back" || raw_sound == "close") return "UIMenuCancel";
    if (raw_sound == "open" || raw_sound == "bladeOpen") return "UIMenuBladeOpenSD";
    if (raw_sound == "bladeClose") return "UIMenuBladeCloseSD";
    if (raw_sound == "prevNext" || raw_sound == "tab") return "UIMenuPrevNext";
    if (raw_sound == "equip") return "ITMGenericUpSD";
    if (raw_sound == "unequip") return "ITMGenericDownSD";
    if (raw_sound == "gold" || raw_sound == "coin") return "ITMGoldUpSD";
    if (raw_sound == "potion") return "ITMPotionUse";
    if (raw_sound == "magic" || raw_sound == "spell") return "MAGMysticismCast";
    if (raw_sound == "shout") return "UIWordWallWhispers";

    for (const char ch : raw_sound) {
        const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_';
        if (!valid) {
            return "UIMenuOK";
        }
    }
    return std::string(raw_sound);
}

std::string HandlePlaySoundJson(const nlohmann::json& params)
{
    std::string requested = "ok";
    if (params.is_object() && params.contains("sound") && params["sound"].is_string()) {
        requested = params["sound"].get<std::string>();
    }
    const auto editor_id = ResolveSkyrimSoundEditorId(requested);
    RE::PlaySound(editor_id.c_str());
    return nlohmann::json{
        { "available", true },
        { "played", true },
        { "sound", editor_id }
    }.dump();
}

RE::NiCamera* FindActiveNiCamera(RE::NiAVObject* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    if (auto* cam = netimmerse_cast<RE::NiCamera*>(node)) {
        return cam;
    }
    if (auto* ni_node = node->AsNode()) {
        for (const auto& child : ni_node->GetChildren()) {
            if (auto* found = FindActiveNiCamera(child.get())) {
                return found;
            }
        }
    }
    return nullptr;
}

std::string HandleProjectWorldAnchorsJson(const nlohmann::json& params)
{
    auto results = nlohmann::json::array();
    auto* player_cam = RE::PlayerCamera::GetSingleton();
    auto* ni_cam = player_cam != nullptr ? FindActiveNiCamera(player_cam->cameraRoot.get()) : nullptr;
    auto* player = RE::PlayerCharacter::GetSingleton();
    const RE::NiPoint3 player_pos = player != nullptr ? player->GetPosition() : RE::NiPoint3{};

    if (params.is_object() && params.contains("anchors") && params["anchors"].is_array()) {
        std::size_t count = 0;
        for (const auto& item : params["anchors"]) {
            if (!item.is_object() || count >= 32) break;
            ++count;
            std::string anchor_id = item.contains("id") && item["id"].is_string() ? item["id"].get<std::string>() : std::to_string(count);
            std::string bone_name = item.contains("bone") && item["bone"].is_string() ? item["bone"].get<std::string>() : "NPC Head [Head]";
            const float offset_z = static_cast<float>(ActorPreviewNumber(item, "offsetZ", 18.0, -250.0, 250.0));
            RE::NiPoint3 world_pt{};
            bool has_point = false;
            std::string actor_name;
            int actor_level = 1;
            float hp_cur = 100.0F;
            float hp_max = 100.0F;
            bool hostile = false;
            bool dead = false;

            const auto ref_id = ActorPreviewFormId(item, "referenceId");
            if (ref_id != 0) {
                if (auto* form = RE::TESForm::LookupByID(ref_id)) {
                    if (auto* refr = form->AsReference()) {
                        world_pt = refr->GetPosition();
                        world_pt.z += 68.0F + offset_z;
                        has_point = true;
                        actor_name = refr->GetDisplayFullName() ? refr->GetDisplayFullName() : "Target";
                        if (auto* root_3d = refr->Get3D(false)) {
                            if (!bone_name.empty() && bone_name != "root") {
                                if (auto* bone_obj = root_3d->GetObjectByName(bone_name.c_str())) {
                                    world_pt = bone_obj->world.translate;
                                    world_pt.z += offset_z;
                                }
                            }
                        }
                        if (auto* actor = refr->As<RE::Actor>()) {
                            actor_level = actor->GetLevel();
                            dead = actor->IsDead();
                            if (player != nullptr) {
                                hostile = actor->IsHostileToActor(player);
                            }
                            if (auto* av = actor->AsActorValueOwner()) {
                                hp_cur = av->GetActorValue(RE::ActorValue::kHealth);
                                hp_max = std::max(1.0F, hp_cur - actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth));
                            }
                        }
                    }
                }
            } else if (item.contains("x") && item.contains("y") && item.contains("z")) {
                world_pt.x = static_cast<float>(ActorPreviewNumber(item, "x", 0.0, -1000000.0, 1000000.0));
                world_pt.y = static_cast<float>(ActorPreviewNumber(item, "y", 0.0, -1000000.0, 1000000.0));
                world_pt.z = static_cast<float>(ActorPreviewNumber(item, "z", 0.0, -1000000.0, 1000000.0)) + offset_z;
                has_point = true;
            }

            float sx = 0.5F;
            float sy = 0.5F;
            float sz = 0.0F;
            bool on_screen = false;
            if (has_point && ni_cam != nullptr) {
                on_screen = ni_cam->WorldPtToScreenPt3(world_pt, sx, sy, sz, 1e-5F) &&
                    sz > 0.0F && sz < 1.0F && sx >= -0.15F && sx <= 1.15F && sy >= -0.15F && sy <= 1.15F;
            }
            const float dx = world_pt.x - player_pos.x;
            const float dy = world_pt.y - player_pos.y;
            const float dz = world_pt.z - player_pos.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            results.push_back(nlohmann::json{
                { "id", anchor_id },
                { "available", has_point },
                { "onScreen", on_screen },
                { "u", sx },
                { "v", 1.0F - sy },
                { "depth", sz },
                { "distance", std::round(dist) },
                { "bone", bone_name },
                { "name", actor_name },
                { "level", actor_level },
                { "health", std::round(hp_cur) },
                { "healthMax", std::round(hp_max) },
                { "hostile", hostile },
                { "dead", dead }
            });
        }
    }

    return nlohmann::json{
        { "available", ni_cam != nullptr },
        { "vr", REL::Module::IsVR() },
        { "anchors", std::move(results) }
    }.dump();
}

std::string HandleSetSpatialPlacementJson(SkyUltraViewHandle handle, const nlohmann::json& params)
{
    auto& rec = g_spatial_placements[handle];
    if (params.is_object() && !ActorPreviewBoolean(params, "query", false)) {
        if (params.contains("mode") && params["mode"].is_string()) {
            const auto mode = params["mode"].get<std::string>();
            if (mode == "screen" || mode == "atGaze" || mode == "worldLocked" ||
                mode == "leftWrist" || mode == "rightWrist" || mode == "actorBone") {
                rec.mode = mode;
            }
        }
        rec.distance = ActorPreviewNumber(params, "distance", rec.distance, 0.35, 10.0);
        rec.scale = ActorPreviewNumber(params, "scale", rec.scale, 0.4, 2.5);
        if (params.contains("bone") && params["bone"].is_string()) {
            const auto b = params["bone"].get<std::string>();
            if (!b.empty() && b.size() <= 48) rec.bone = b;
        }
        if (params.contains("targetRefId")) {
            rec.target_ref_id = ActorPreviewFormId(params, "targetRefId");
        }
        if (params.contains("virtualKeyboard")) {
            rec.virtual_keyboard = ActorPreviewBoolean(params, "virtualKeyboard", rec.virtual_keyboard);
        }
        skyultra::SetOverlaySpatialConfig(
            rec.mode, static_cast<float>(rec.distance), static_cast<float>(rec.scale), g_vr_head_tracking);
    }
    return nlohmann::json{
        { "available", true },
        { "updated", true },
        { "mode", rec.mode },
        { "distance", rec.distance },
        { "scale", rec.scale },
        { "bone", rec.bone },
        { "targetRefId", rec.target_ref_id },
        { "virtualKeyboard", rec.virtual_keyboard },
        { "vrActive", REL::Module::IsVR() }
    }.dump();
}

std::string HandleToggleDomInspectorJson(const nlohmann::json& params)
{
    skyultra::ViewId target_view = g_menu_view_id;
    if (params.is_object() && params.contains("viewId") && params["viewId"].is_number_unsigned()) {
        target_view = params["viewId"].get<skyultra::ViewId>();
    }
    const bool current = g_dom_inspector_views[target_view];
    const bool next = params.is_object() && params.contains("open") && params["open"].is_boolean()
        ? params["open"].get<bool>()
        : !current;
    g_dom_inspector_views[target_view] = next;
    return nlohmann::json{
        { "available", true },
        { "viewId", target_view },
        { "inspectorOpen", next },
        { "message", next ? "Embedded WebKit/CEF DOM Inspector attached." : "DOM Inspector detached." }
    }.dump();
}

std::string HandleLegacyInteropCallJson(SkyUltraViewHandle handle, skyultra::ViewId view_id, const nlohmann::json& params)
{
    std::string source_framework = "PrismaUI";
    std::string method_name = "interop";
    nlohmann::json payload = nlohmann::json::object();
    if (params.is_object()) {
        if (params.contains("framework") && params["framework"].is_string()) {
            source_framework = params["framework"].get<std::string>();
        }
        if (params.contains("method") && params["method"].is_string()) {
            method_name = params["method"].get<std::string>();
        }
        if (params.contains("payload")) {
            payload = params["payload"];
        }
    }
    if (g_view_manager != nullptr) {
        if (auto* view = g_view_manager->Find(view_id)) {
            (void)view->SendEventJson("skyultra.legacy.interop", nlohmann::json{
                { "framework", source_framework },
                { "method", method_name },
                { "payload", payload }
            }.dump());
        }
    }
    return nlohmann::json{
        { "accepted", true },
        { "framework", source_framework },
        { "method", method_name },
        { "viewHandle", handle }
    }.dump();
}

bool HandleHostUiRequest(SkyUltraViewHandle handle, const skyultra::PageBridgeMessage& message)
{
    const auto reply = [&](const std::string& payload) {
        (void)NativeApiCompletePageRequest(handle, message.request_id.data(),
            static_cast<std::uint32_t>(message.request_id.size()), 1, payload.data(),
            static_cast<std::uint32_t>(payload.size()), nullptr, 0);
    };
    const auto reject_non_menu = [&]() {
        constexpr auto error = "Only SkyUltra's built-in Developer Studio may invoke host diagnostics."sv;
        (void)NativeApiCompletePageRequest(handle, message.request_id.data(),
            static_cast<std::uint32_t>(message.request_id.size()), 0, nullptr, 0,
            error.data(), static_cast<std::uint32_t>(error.size()));
    };
    if (message.name == "ui.playSound") {
        reply(HandlePlaySoundJson(nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.projectWorldAnchors") {
        reply(HandleProjectWorldAnchorsJson(nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.setSpatialPlacement") {
        reply(HandleSetSpatialPlacementJson(handle, nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.toggleDomInspector") {
        if (message.view_id != g_menu_view_id) { reject_non_menu(); return true; }
        reply(HandleToggleDomInspectorJson(nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.interopCall") {
        reply(HandleLegacyInteropCallJson(handle, message.view_id, nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.getDeveloperSnapshot") {
        if (message.view_id != g_menu_view_id) { reject_non_menu(); return true; }
        reply(BuildDeveloperSnapshotJson());
        return true;
    }
    if (message.name == "ui.reloadModViews") {
        if (message.view_id != g_menu_view_id) { reject_non_menu(); return true; }
        reply(HandleReloadModViewsJson(nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.exportHudMod") {
        if (message.view_id != g_menu_view_id) { reject_non_menu(); return true; }
        reply(HandleExportHudModJson(nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if (message.name == "ui.manageDiagnosticSample") {
        if (message.view_id != g_menu_view_id) { reject_non_menu(); return true; }
        reply(HandleManageDiagnosticSampleJson(nlohmann::json::parse(message.payload_json, nullptr, false)));
        return true;
    }
    if(message.name=="ui.setCursor"){
        const auto params=nlohmann::json::parse(message.payload_json,nullptr,false);
        if(!params.is_object()||!params.contains("mode")||!params["mode"].is_string()||(params["mode"]!="native"&&params["mode"]!="hidden")){reply(R"({"updated":false})");return true;}
        {const std::scoped_lock lock(g_native_views_mutex);if(const auto record=g_native_views.find(handle);record!=g_native_views.end()){record->second.hide_native_cursor=params["mode"]=="hidden";
            if(params.contains("accent")&&params["accent"].is_string()){
                const auto color=params["accent"].get<std::string>();
                if(color.size()==7&&color[0]=='#'){std::uint32_t rgb{};bool valid=true;for(std::size_t i=1;i<7;++i){const char c=color[i];const int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(digit<0){valid=false;break;}rgb=(rgb<<4)|static_cast<std::uint32_t>(digit);}if(valid)record->second.cursor_accent=rgb;}
            }}}
        PublishBrowserCursor(true);reply(R"({"updated":true})");return true;
    }
    if(message.name=="ui.getSplashStatus"){
        const auto value=skyultra::GetFrameworkSplashStatus();
        reply(nlohmann::json{{"active",value.active},{"hasFrame",value.has_frame},{"failed",value.failed},{"errorCode",value.error_code},{"mediaError",value.media_error},{"stage",value.stage}}.dump());return true;
    }
    if(message.name=="ui.replaySplash"){
        if(message.view_id!=g_menu_view_id){reply(R"({"started":false})");return true;}
        if(const auto* config=g_view_manager->Config(message.view_id))skyultra::BeginFrameworkSplash(config->id,"skyultra.menu",true,true);
        reply(nlohmann::json{{"started",skyultra::FrameworkSplashActive()}}.dump());return true;
    }
    if(message.name=="ui.getSettings"){reply(FrameworkSettingsJson().dump());return true;}
    if(message.name=="ui.updateSettings"){
        if(message.view_id!=g_menu_view_id){
            std::string caller_mod_id;
            {
                const std::scoped_lock view_lock(g_native_views_mutex);
                if(const auto view_it=g_native_views.find(handle);view_it!=g_native_views.end()){
                    const std::scoped_lock mod_lock(g_native_mods_mutex);
                    if(const auto mod_it=g_native_mods.find(view_it->second.mod_handle);mod_it!=g_native_mods.end()){
                        caller_mod_id=mod_it->second.manifest.id;
                    }
                }
            }
            const auto input=nlohmann::json::parse(message.payload_json,nullptr,false);
            const nlohmann::json* mod_hotkey_json=nullptr;
            if(input.is_object()){
                if(const auto hk=input.find("hotkey");hk!=input.end())mod_hotkey_json=&(*hk);
                else if(const auto mh=input.find("modHotkeys");mh!=input.end()&&mh->is_object()){
                    if(const auto self_hk=mh->find(caller_mod_id);self_hk!=mh->end())mod_hotkey_json=&(*self_hk);
                }
            }
            if(caller_mod_id.empty()||caller_mod_id=="skyultra.menu"||mod_hotkey_json==nullptr){
                constexpr auto error="Only SkyUltra's main menu may change global settings."sv;
                (void)NativeApiCompletePageRequest(handle,message.request_id.data(),static_cast<std::uint32_t>(message.request_id.size()),0,nullptr,0,error.data(),static_cast<std::uint32_t>(error.size()));return true;
            }
            HotkeyBinding parsed_binding{{},{},{}};
            if(const auto existing=g_mod_hotkeys.find(caller_mod_id);existing!=g_mod_hotkeys.end())parsed_binding=existing->second;
            std::string error;
            if(!ParseHotkeyBinding(*mod_hotkey_json,parsed_binding,true,error)){
                (void)NativeApiCompletePageRequest(handle,message.request_id.data(),static_cast<std::uint32_t>(message.request_id.size()),0,nullptr,0,error.data(),static_cast<std::uint32_t>(error.size()));return true;
            }
            g_mod_hotkeys[caller_mod_id]=std::move(parsed_binding);
            if(!WriteFrameworkSettingsDisk(error)){
                (void)NativeApiCompletePageRequest(handle,message.request_id.data(),static_cast<std::uint32_t>(message.request_id.size()),0,nullptr,0,error.data(),static_cast<std::uint32_t>(error.size()));return true;
            }
            reply(FrameworkSettingsJson().dump());return true;
        }
        const auto input=nlohmann::json::parse(message.payload_json,nullptr,false);
        std::string error;
        if(!SaveFrameworkSettings(input,error)){
            (void)NativeApiCompletePageRequest(handle,message.request_id.data(),static_cast<std::uint32_t>(message.request_id.size()),0,nullptr,0,error.data(),static_cast<std::uint32_t>(error.size()));return true;
        }
        reply(FrameworkSettingsJson().dump());return true;
    }
    if(message.name=="ui.close"){
        reply(R"({"closed":true})");
        if(message.view_id==g_menu_view_id)SetBrowserVisible(false,"main menu close button");
        else (void)NativeApiSetVisible(handle,0);
        skyultra::CancelFrameworkSplash();return true;
    }
    if(message.name=="game.subscribe"){
        const auto params=nlohmann::json::parse(message.payload_json,nullptr,false);
        GameDataSubscriptionRecord sub;
        sub.compact=params.is_object()&&params.contains("scope")&&params["scope"]=="hud";
        sub.interval_ms=static_cast<std::uint32_t>(std::lround(ActorPreviewNumber(params.is_object()?params:nlohmann::json::object(),"intervalMs",sub.compact?10.0:750.0,8.0,5000.0)));
        if(params.is_object()&&params.contains("topics")&&params["topics"].is_array()){
            for(const auto& item:params["topics"]){
                if(!item.is_string())continue;
                const auto topic=item.get<std::string>();
                if((topic=="vitals"||topic=="equipment"||topic=="inventory"||topic=="effects"||topic=="quests"||topic=="followers")&&
                    std::find(sub.topics.begin(),sub.topics.end(),topic)==sub.topics.end()){
                    sub.topics.push_back(topic);
                }
            }
        }
        if(sub.topics.empty()){
            sub.topics={"vitals","equipment","inventory","effects","quests","followers"};
        }
        const auto initial_update_str=skyultra::CollectSharedGameDataUpdateJSON(sub.compact,sub.topics,sub.topic_signatures,sub.last_sequence,true);
        sub.last_push_ms=SubscriptionNowMs();
        sub.needs_catchup=false;
        const auto topics_copy=sub.topics;
        const auto compact_copy=sub.compact;
        const auto interval_copy=sub.interval_ms;
        const auto seq_copy=sub.last_sequence;
        g_game_subscriptions[handle]=std::move(sub);
        g_active_game_subscription_count.store(static_cast<std::uint32_t>(g_game_subscriptions.size()),std::memory_order_relaxed);
        StartGameDataSubscriptionTicker();
        auto parsed_update=nlohmann::json::parse(initial_update_str,nullptr,false);
        reply(nlohmann::json{
            {"subscribed",true},
            {"sequence",seq_copy},
            {"snapshotScope",compact_copy?"hud":"full"},
            {"topics",topics_copy},
            {"intervalMs",interval_copy},
            {"update",parsed_update.is_object()?std::move(parsed_update):nlohmann::json::object()}
        }.dump());
        return true;
    }
    if(message.name=="game.unsubscribe"){
        g_game_subscriptions.erase(handle);
        g_active_game_subscription_count.store(static_cast<std::uint32_t>(g_game_subscriptions.size()),std::memory_order_relaxed);
        reply(R"({"subscribed":false})");
        return true;
    }
    if (message.name.starts_with("game.")) {
        const auto response = skyultra::ReadGameDataJSON(message.name, message.payload_json);
        reply(response);
        if ((message.name == "game.equipItem" || message.name == "game.unequipItem" ||
                message.name == "game.itemAction" || message.name == "game.barterTransaction" ||
                message.name == "game.containerTransfer" || message.name == "game.equipSpell" ||
                message.name == "game.renameSpell" || message.name == "game.deleteSpell" ||
                message.name == "game.setFavorite") &&
            !g_game_subscriptions.empty()) {
            PumpGameDataSubscriptions(true);
        }
        return true;
    }
    if (message.name == "ui.pageReady") {
        skyultra::CompleteOverlayPageLoad(message.view_id);
        if(const auto* config=g_view_manager->Config(message.view_id))skyultra::CompleteFrameworkSplash(config->id);
        for(auto& [mod,widget]:g_widget_overlays){(void)mod;if(widget.manager_id==message.view_id)widget.ready=true;}
        reply(R"({"ready":true})");
        return true;
    }
    if (message.name == "ui.getPerformanceSnapshot") {
        const auto snapshot=skyultra::GetOverlayPerformanceSnapshot();
        const auto metric=[](const skyultra::OverlayTimingMetric& value) {
            return nlohmann::json{{"samples",value.samples},{"totalMs",value.total_ms},{"lastMs",value.last_ms},
                {"windowSamples",value.window_samples},{"meanMs",value.mean_ms},{"p95Ms",value.p95_ms},{"maxMs",value.max_ms},
                {"ageMs",value.age_ms<0 ? nlohmann::json(nullptr) : nlohmann::json(value.age_ms)}};
        };
        reply(nlohmann::json{{"schemaVersion",1},{"available",snapshot.generation!=0 && !REL::Module::IsVR()},
            {"scope","host-overlay"},{"generation",snapshot.generation},{"capturedAtMs",snapshot.captured_at_ms},
            {"gpuTimingAvailable",snapshot.gpu_timing_available},{"skippedGpuFrames",snapshot.skipped_gpu_frames},
            {"invalidGpuSamples",snapshot.invalid_gpu_samples},{"hostGpu",metric(snapshot.host_gpu)},
            {"compositorGpu",metric(snapshot.compositor_gpu)},{"callbackCpu",metric(snapshot.callback_cpu)},
            {"presentInterval",metric(snapshot.present_interval)}}.dump());
        return true;
    }
    if (message.name == "ui.setInputScope") {
        const auto params = nlohmann::json::parse(message.payload_json, nullptr, false);
        if (params.is_object()) {
            const std::scoped_lock lock(g_native_views_mutex);
            if (const auto owner = g_native_views.find(handle); owner != g_native_views.end()) {
                owner->second.input_scope_depth = static_cast<std::uint32_t>(ActorPreviewNumber(params, "depth", 0, 0, 8));
            }
        }
        reply(R"({"updated":true})"); return true;
    }
    SkyUltraModHandle mod_handle{};
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto owner = g_native_views.find(handle);
        if (owner == g_native_views.end()) return true;
        mod_handle = owner->second.mod_handle;
    }
    NativeApiModRecord mod;
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        const auto found = g_native_mods.find(mod_handle);
        if (found == g_native_mods.end()) { reply(R"({"available":false,"message":"A registered mod is required."})"); return true; }
        mod = found->second;
    }
    const auto params = nlohmann::json::parse(message.payload_json, nullptr, false);
    if (!params.is_object()) { reply(R"({"available":false,"message":"Widget options must be an object."})"); return true; }
    const auto build_overlay_json = [](const WidgetOverlayRecord& rec) {
        return nlohmann::json{
            { "available", true },
            { "enabled", true },
            { "ready", rec.ready && skyultra::OverlayViewHasPainted(rec.manager_id) },
            { "widgets", rec.widgets },
            { "anchor", rec.anchor },
            { "scale", rec.scale },
            { "opacity", rec.opacity },
            { "theme", rec.theme },
            { "glow", rec.glow },
            { "colors", rec.colors },
            { "placements", rec.placements },
            { "customCode", rec.custom_code }
        };
    };
    const auto send_state = [&] {
        if (const auto active = g_widget_overlays.find(mod_handle); active != g_widget_overlays.end()) {
            reply(build_overlay_json(active->second).dump());
        } else reply(R"({"available":true,"enabled":false,"widgets":[]})");
    };
    if (ActorPreviewBoolean(params, "query", false)) { send_state(); return true; }
    nlohmann::json custom_code_json = nlohmann::json::object();
    bool custom_code_enabled = false;
    if (const auto cc_it = params.find("customCode"); cc_it != params.end() && cc_it->is_object()) {
        custom_code_enabled = ActorPreviewBoolean(*cc_it, "enabled", false);
        const auto clip_str = [&](const char* key, std::size_t max_len) -> std::string {
            const auto f = cc_it->find(key);
            if (f == cc_it->end() || !f->is_string()) return {};
            const auto& s = f->get_ref<const std::string&>();
            return s.size() <= max_len ? s : s.substr(0, max_len);
        };
        std::string mode = clip_str("mode", 16);
        if (mode != "full-html") mode = "split";
        custom_code_json = {
            { "enabled", custom_code_enabled },
            { "mode", mode },
            { "html", clip_str("html", 196608U) },
            { "css", clip_str("css", 196608U) },
            { "js", clip_str("js", 196608U) },
            { "fullHtml", clip_str("fullHtml", 262144U) }
        };
    }
    std::vector<std::string> widgets;
    if (const auto items = params.find("widgets"); items != params.end() && items->is_array()) {
        for (const auto& item : *items) {
            if (!item.is_string() || widgets.size() >= 16) continue;
            const auto value = item.get<std::string>();
            if ((value == "vitals" || value == "equipment" || value == "character" || value == "effects" ||
                value == "resources" || value == "compass" || value == "clock" || value == "radar" ||
                value == "loot" || value == "stealth" || value == "wards" || value == "telemetry" ||
                value == "target" || value == "skills" || value == "armory") &&
                std::find(widgets.begin(), widgets.end(), value) == widgets.end()) widgets.push_back(value);
        }
    }
    if (custom_code_enabled && widgets.empty()) {
        widgets.emplace_back("vitals");
    }
    if (!ActorPreviewBoolean(params, "enabled", false) || (widgets.empty() && !custom_code_enabled)) {
        if (const auto active = g_widget_overlays.find(mod_handle); active != g_widget_overlays.end()) {
            (void)NativeApiDestroyView(active->second.view);
            g_widget_overlays.erase(active);
        }
        PublishOverlayViews();
        SyncVanillaCompassVisibility(true);
        send_state(); return true;
    }
    if (!g_widget_overlays.contains(mod_handle) && g_widget_overlays.size() >= 8) {
        reply(R"({"available":false,"message":"Eight mods already have pinned widget surfaces."})"); return true;
    }
    std::error_code path_error;
    const auto candidate_hud = (mod.manifest.entry_file.parent_path() / L"hud.html").lexically_normal();
    auto hud_file = std::filesystem::weakly_canonical(candidate_hud, path_error);
    auto relative = hud_file.lexically_relative(mod.manifest.root_directory);
    if (path_error || relative.empty() || *relative.begin() == L"..") {
        path_error.clear();
        const auto hud_attrs = GetFileAttributesW(candidate_hud.c_str());
        if (hud_attrs != INVALID_FILE_ATTRIBUTES && (hud_attrs & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            hud_file = candidate_hud;
            relative = hud_file.lexically_relative(mod.manifest.root_directory);
        }
    }
    if (relative.empty() || *relative.begin() == L".." || !std::filesystem::is_regular_file(hud_file, path_error)) {
        reply(R"({"available":false,"message":"The mod's packaged hud.html entry is missing or outside its asset root."})"); return true;
    }
    auto* owner_config = g_view_manager != nullptr ? g_view_manager->Config(message.view_id) : nullptr;
    if (owner_config == nullptr) { reply(R"({"available":false,"message":"The owning view is unavailable."})"); return true; }
    auto config = *owner_config;
    { const std::scoped_lock lock(g_native_views_mutex);
      if (const auto owner = g_native_views.find(handle); owner != g_native_views.end()) config.browser_engine = owner->second.browser_engine; }
    config.id = mod.manifest.id + ".widgets." + std::to_string(g_next_native_view_handle.load());
    config.transparent = true; config.modal = false; config.click_through = true;
    config.enable_gamepad = false; config.visible = true; config.layer = skyultra::ViewLayer::Hud;
    config.z_index = -100; config.max_fps = 60; config.clip = {};
    const auto base_scale = static_cast<float>(ActorPreviewNumber(params, "scale", 1, .7, 1.5));
    const auto base_opacity = static_cast<float>(ActorPreviewNumber(params, "opacity", 1, .35, 1));
    config.scale = 1.0F;
    config.opacity = 1.0F;
    std::string anchor = "top-right";
    if (const auto value = params.find("anchor"); value != params.end() && value->is_string()) anchor = value->get<std::string>();
    if (anchor != "top-left" && anchor != "top-center" && anchor != "top-right" &&
        anchor != "bottom-left" && anchor != "bottom-center" && anchor != "bottom-right") {
        anchor = "top-right";
    }
    config.anchor = skyultra::Anchor::TopLeft;
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    const std::uint32_t hud_w = screen.width > 0 ? screen.width : 1920U;
    const std::uint32_t hud_h = screen.height > 0 ? screen.height : 1080U;
    config.size = { hud_w, hud_h };
    config.bounds = { 0.0F, 0.0F, static_cast<float>(hud_w), static_cast<float>(hud_h) };
    std::string url = LocalFileUrl(hud_file) + "?widgets=";
    for (const auto& widget : widgets) { if (url.back() != '=') url += ','; url += widget; }
    std::string theme = "dragonstone";
    if (const auto candidate = params.find("theme"); candidate != params.end() && candidate->is_string()) {
        const auto value = candidate->get<std::string>();
        constexpr std::array<std::string_view,17> themes{"obsidian","dragonstone","aurora","ember","glacier","verdant","aether","dwemer","sunspire","frost","arcane","cyberneon","synthwave","venomneon","plasmaneon","crimsonneon","darkmode"};
        if (std::ranges::find(themes,value) != themes.end()) theme = value;
    }
    url += "&theme=" + theme;
    url += "&anchor=" + anchor;
    url += "&scale=" + std::to_string(static_cast<int>(std::lround(base_scale * 100.0F)));
    url += "&opacity=" + std::to_string(static_cast<int>(std::lround(base_opacity * 100.0F)));
    url += anchor.ends_with("left") ? "&side=left" : (anchor.ends_with("center") ? "&side=center" : "&side=right");
    if (anchor.starts_with("bottom")) url += "&align=bottom";
    const auto glow = static_cast<int>(std::lround(ActorPreviewNumber(params, "glow", 100, 0, 150)));
    url += "&glow=" + std::to_string(glow);
    if (const auto colors_it = params.find("colors"); colors_it != params.end() && colors_it->is_object()) {
        std::string color_pairs;
        for (const auto& [k, v] : colors_it->items()) {
            if (!v.is_string() || k.empty() || k.size() > 16) continue;
            std::string hex;
            for (const char ch : v.get<std::string>()) {
                if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) hex.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
            if (hex.size() == 6 || hex.size() == 3) {
                if (!color_pairs.empty()) color_pairs += ',';
                color_pairs += k + ":" + hex;
            }
        }
        if (!color_pairs.empty()) url += "&colors=" + color_pairs;
    }
    if (const auto placements_it = params.find("placements"); placements_it != params.end() && placements_it->is_object()) {
        std::string placement_entries;
        for (const auto& [k, raw] : placements_it->items()) {
            if (!raw.is_object() || k.empty() || k.size() > 16) continue;
            std::string w_anchor = anchor;
            if (const auto a = raw.find("anchor"); a != raw.end() && a->is_string()) {
                const auto cand = a->get<std::string>();
                if (cand == "top-left" || cand == "top-center" || cand == "top-right" ||
                    cand == "bottom-left" || cand == "bottom-center" || cand == "bottom-right") {
                    w_anchor = cand;
                }
            }
            const int w_scale = static_cast<int>(std::lround(ActorPreviewNumber(raw, "scale", base_scale, 0.7, 1.5) * 100.0));
            const int w_opacity = static_cast<int>(std::lround(ActorPreviewNumber(raw, "opacity", base_opacity, 0.35, 1.0) * 100.0));
            const int w_glow = static_cast<int>(std::lround(ActorPreviewNumber(raw, "glow", glow, 0.0, 150.0)));
            std::string w_hex;
            if (const auto c = raw.find("color"); c != raw.end() && c->is_string()) {
                for (const char ch : c->get<std::string>()) {
                    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                        w_hex.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                    }
                }
                if (w_hex.size() != 6) w_hex.clear();
            }
            std::string w_font = "theme";
            if (const auto f = raw.find("fontStyle"); f != raw.end() && f->is_string()) {
                const auto cand = f->get<std::string>();
                if (cand == "theme" || cand == "serif" || cand == "display" || cand == "data" || cand == "mono") {
                    w_font = cand;
                }
            }
            const int w_ox = static_cast<int>(std::lround(ActorPreviewNumber(raw, "offsetX", 0.0, -600.0, 600.0)));
            const int w_oy = static_cast<int>(std::lround(ActorPreviewNumber(raw, "offsetY", 0.0, -400.0, 400.0)));
            std::string w_vis = "always";
            if (const auto vis = raw.find("visibilityRule"); vis != raw.end() && vis->is_string()) {
                const auto cand = vis->get<std::string>();
                if (cand == "always" || cand == "combat-low-hp" || cand == "encumbered" ||
                    cand == "has-effects" || cand == "has-followers") {
                    w_vis = cand;
                }
            }
            std::string w_bind;
            if (const auto bind = raw.find("bindingPath"); bind != raw.end() && bind->is_string()) {
                const auto cand = bind->get<std::string>();
                if (cand.size() <= 32) {
                    bool safe_bind = !cand.empty();
                    for (const char ch : cand) {
                        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                              (ch >= '0' && ch <= '9') || ch == '.')) {
                            safe_bind = false;
                            break;
                        }
                    }
                    if (safe_bind) w_bind = cand;
                }
            }
            if (!placement_entries.empty()) placement_entries += ';';
            placement_entries += k + ":" + w_anchor + ":" + std::to_string(w_scale) + ":" +
                std::to_string(w_opacity) + ":" + std::to_string(w_glow) + ":" + w_hex + ":" + w_font + ":" +
                std::to_string(w_ox) + ":" + std::to_string(w_oy) + ":" + w_vis + ":" + w_bind;
        }
        if (!placement_entries.empty()) url += "&placements=" + placement_entries;
    }
    const nlohmann::json saved_colors =
        (params.contains("colors") && params["colors"].is_object()) ? params["colors"] : nlohmann::json::object();
    const nlohmann::json saved_placements =
        (params.contains("placements") && params["placements"].is_object()) ? params["placements"] : nlohmann::json::object();
    if (custom_code_enabled) {
        url += "&custom=1";
    }
    if (const auto active = g_widget_overlays.find(mod_handle); active != g_widget_overlays.end()) {
        // Startup settings can arrive again before the HUD has painted. Keep
        // that browser and update its state instead of overlapping two full-
        // screen CEF surfaces while the old browser closes asynchronously.
        if (mod.manifest.id == "skyultra.menu" && NativeHandleAcceptsCommands(active->second.view) && g_view_manager != nullptr) {
            const auto* live_config=g_view_manager->Config(active->second.manager_id);
            if (auto* live_view = g_view_manager->Find(active->second.manager_id);
                live_view && live_config && live_config->browser_engine==config.browser_engine) {
                active->second.widgets = std::move(widgets);
                active->second.anchor = anchor;
                active->second.scale = base_scale;
                active->second.opacity = base_opacity;
                active->second.theme = theme;
                active->second.glow = glow;
                active->second.colors = saved_colors;
                active->second.placements = saved_placements;
                active->second.custom_code = std::move(custom_code_json);
                const auto push_payload = build_overlay_json(active->second).dump();
                (void)live_view->SendEventJson("skyultra.hud.config", push_payload);
                RecordDevBridgeEntry(active->second.manager_id, mod.manifest.id, "event",
                    "skyultra.hud.config", "ok", 0.1, push_payload.size());
                PublishOverlayViews();
                SyncVanillaCompassVisibility(true);
                send_state();
                return true;
            }
        }
        (void)NativeApiDestroyView(active->second.view);
    }
    const auto child = g_next_native_view_handle.fetch_add(1, std::memory_order_relaxed);
    {
        const std::scoped_lock lock(g_native_views_mutex);
        NativeApiViewRecord record;
        record.mod_handle = mod_handle; record.initial_url = url;
        record.javascript_enabled = true; record.pending_config = config;
        for (const auto method : {"game.getPlayerSnapshot", "game.subscribe", "game.unsubscribe", "game.getNearbyNpcs", "game.getNearbyActors", "game.getActorThumbnail", "game.getItemThumbnail", "game.getRuntimeMetrics", "ui.pageReady", "ui.configureWidgets", "ui.playSound", "ui.projectWorldAnchors", "ui.setSpatialPlacement", "ui.interopCall"}) {
            if (mod.manifest.page_request_methods.contains(method)) record.allowed_page_request_methods.emplace(method);
        }
        g_native_views.emplace(child, std::move(record));
    }
    CreateNativeApiViewOnGameThread(child, config, std::move(url));
    SkyUltraViewStateV1 state{};
    if (NativeApiGetViewState(child, &state) != SKYULTRA_OK || state == SKYULTRA_VIEW_FAILED) {
        (void)NativeApiDestroyView(child); g_widget_overlays.erase(mod_handle);
        reply(R"({"available":false,"message":"The widget browser surface could not be created."})"); return true;
    }
    WidgetOverlayRecord next_rec{};
    next_rec.view = child;
    next_rec.widgets = std::move(widgets);
    next_rec.anchor = anchor;
    next_rec.scale = base_scale;
    next_rec.opacity = base_opacity;
    next_rec.manager_id = NativeManagerViewId(child).value_or(0);
    next_rec.theme = theme;
    next_rec.glow = glow;
    next_rec.colors = saved_colors;
    next_rec.placements = saved_placements;
    next_rec.custom_code = std::move(custom_code_json);
    g_widget_overlays[mod_handle] = std::move(next_rec);
    PublishOverlayViews();
    SyncVanillaCompassVisibility(true);
    send_state();
    return true;
}

SkyUltraResult NativeApiCreateViewForMod(
    const SkyUltraViewConfigV1* source,
    SkyUltraViewHandle* out_handle,
    SkyUltraModHandle mod_handle)
{
    constexpr auto kBrowserEngineOffset = offsetof(SkyUltraViewConfigV1, browser_engine);
    constexpr auto kLegacyConfigSize = kBrowserEngineOffset;
    constexpr auto kBrowserEngineEnd = kBrowserEngineOffset + sizeof(std::uint32_t);
    const bool has_browser_engine = source != nullptr && source->struct_size >= kBrowserEngineEnd;
    const auto requested_browser_engine = has_browser_engine ? source->browser_engine :
        SKYULTRA_BROWSER_ENGINE_DEFAULT;
    if (source == nullptr || out_handle == nullptr ||
        source->struct_size < kLegacyConfigSize ||
        source->id_utf8 == nullptr || source->id_length == 0 || source->id_length > 128 ||
        (source->initial_url_length != 0 && source->initial_url_utf8 == nullptr) ||
        source->initial_url_length > 65536 || source->pixel_width == 0 ||
        source->pixel_height == 0 || !std::isfinite(source->opacity) ||
        !std::isfinite(source->scale) || source->scale <= 0.0F || source->layer > 3 ||
        source->anchor > 4 || (source->flags & ~0x3FU) != 0 ||
        requested_browser_engine > SKYULTRA_BROWSER_ENGINE_CEF) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }

    NativeApiModRecord mod_record;
    if (mod_handle != 0) {
        const std::scoped_lock lock(g_native_mods_mutex);
        const auto mod = g_native_mods.find(mod_handle);
        if (mod == g_native_mods.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        mod_record = mod->second;
    }

    const std::string_view id(source->id_utf8, source->id_length);
    const std::string_view url(source->initial_url_utf8 != nullptr ? source->initial_url_utf8 : "",
        source->initial_url_length);
    if (id.find('\0') != std::string_view::npos || url.find('\0') != std::string_view::npos ||
        (mod_handle != 0 && !url.empty())) {
        return SKYULTRA_INVALID_ARGUMENT;
    }

    skyultra::ViewConfig config;
    config.id.assign(id);
    if (mod_handle != 0) {
        skyultra::ModContentPolicy policy;
        policy.root_directory = mod_record.manifest.root_directory;
        policy.entry_file = mod_record.manifest.entry_file;
        policy.asset_access_enabled = mod_record.manifest.asset_access_enabled;
        policy.network_hosts = mod_record.manifest.network_hosts;
        config.mod_content_policy = std::move(policy);
    }
    config.size = { source->pixel_width, source->pixel_height };
    config.bounds = { source->bounds.x, source->bounds.y,
        source->bounds.width, source->bounds.height };
    config.clip = { source->clip.x, source->clip.y, source->clip.width, source->clip.height };
    config.opacity = source->opacity;
    config.scale = source->scale;
    config.z_index = source->z_index;
    config.max_fps = source->max_fps == SKYULTRA_FPS_UNCAPPED ? 0U :
        (source->max_fps == 0 ? 60U : std::min(source->max_fps, 240U));
    config.visible = (source->flags & SKYULTRA_VIEW_VISIBLE) != 0;
    config.transparent = (source->flags & SKYULTRA_VIEW_TRANSPARENT) != 0;
    config.click_through = (source->flags & SKYULTRA_VIEW_CLICK_THROUGH) != 0;
    config.modal = (source->flags & SKYULTRA_VIEW_MODAL) != 0;
    config.enable_javascript = (source->flags & SKYULTRA_VIEW_ENABLE_JAVASCRIPT) != 0;
    config.enable_gamepad = (source->flags & SKYULTRA_VIEW_ENABLE_GAMEPAD) != 0;
    config.layer = static_cast<skyultra::ViewLayer>(source->layer);
    config.anchor = static_cast<skyultra::Anchor>(source->anchor);
    config.browser_engine = EffectiveBrowserEngine(DefaultBrowserEngine());
    if (requested_browser_engine == SKYULTRA_BROWSER_ENGINE_ULTRALIGHT) {
        config.browser_engine = EffectiveBrowserEngine(skyultra::BrowserEngine::Ultralight);
    } else if (requested_browser_engine == SKYULTRA_BROWSER_ENGINE_CEF) {
        config.browser_engine = EffectiveBrowserEngine(skyultra::BrowserEngine::CEF);
    }
    // A manifest backend applies consistently to every view created for that
    // registered mod. EffectiveBrowserEngine falls back only if the requested
    // backend was not included in this SkyUltra build.
    if (mod_handle != 0 && mod_record.manifest.browser_engine) {
        config.browser_engine = EffectiveBrowserEngine(*mod_record.manifest.browser_engine);
    }
    if (mod_handle != 0 && !mod_record.manifest.id.empty()) {
        if (const auto override_it = g_mod_browser_engines.find(mod_record.manifest.id);
            override_it != g_mod_browser_engines.end()) {
            config.browser_engine = EffectiveBrowserEngine(
                override_it->second == "cef" ? skyultra::BrowserEngine::CEF : skyultra::BrowserEngine::Ultralight);
        }
    }
    std::string initial_url = mod_handle != 0 ? LocalFileUrl(mod_record.manifest.entry_file) :
                                                std::string(url);

    const auto handle = g_next_native_view_handle.fetch_add(1, std::memory_order_relaxed);
    if (handle == 0) {
        return SKYULTRA_INTERNAL_ERROR;
    }
    {
        const std::scoped_lock lock(g_native_views_mutex);
        auto record = NativeApiViewRecord{};
        record.mod_handle = mod_handle;
        record.initial_url = initial_url;
        record.javascript_enabled = config.enable_javascript;
        record.pending_config = config;
        record.wait_for_page_ready = mod_handle != 0 && mod_record.manifest.page_request_methods.contains("ui.pageReady");
        g_native_views.emplace(handle, std::move(record));
    }

    const bool queued = QueueNativeApiTask([handle, config = std::move(config),
                                               initial_url = std::move(initial_url)]() mutable {
        const bool show_splash_first = config.visible && !config.transparent &&
            config.browser_engine == skyultra::BrowserEngine::CEF && g_render_hook_installed &&
            !REL::Module::IsVR() && !g_browser_overlay_suppressed.load(std::memory_order_relaxed);
        if(config.visible&&!g_browser_overlay_suppressed.load(std::memory_order_relaxed)&&!config.click_through&&!REL::Module::IsVR()&&config.layer!=skyultra::ViewLayer::Hud){
            std::string mod_id=config.id;
            {const std::scoped_lock lock(g_native_views_mutex);if(const auto owner=g_native_views.find(handle);owner!=g_native_views.end()){
                const std::scoped_lock mods_lock(g_native_mods_mutex);if(const auto mod=g_native_mods.find(owner->second.mod_handle);mod!=g_native_mods.end())mod_id=mod->second.manifest.id;}}
            skyultra::BeginFrameworkSplash(config.id,std::move(mod_id));
        }
        auto view_key = config.id;
        auto create = [handle,config = std::move(config),initial_url = std::move(initial_url)]() mutable {
            if (!NativeHandleAcceptsCommands(handle)) return;
            if (g_view_manager == nullptr || g_browser_runtime == nullptr) {
                SetNativeApiViewFailed(handle);
                return;
            }

            if (config.browser_engine == skyultra::BrowserEngine::CEF) {
                auto* router = dynamic_cast<BrowserRuntimeRouter*>(g_browser_runtime.get());
                SKSE::log::info("SkyUltra received a CEF view request for '{}'.", config.id);
                if (router == nullptr || !router->InitializeCef(
                        skyultra::BrowserEngine::CEF,
                        [handle, config = std::move(config), initial_url = std::move(initial_url)](
                            bool initialized, std::string error) mutable {
                            if (!initialized && !error.empty()) {
                                SKSE::log::warn("SkyUltra deferred a CEF view after startup failed: {}", error);
                            }
                            if (!QueueNativeApiTask(
                                    [handle, config = std::move(config),
                                        initial_url = std::move(initial_url)]() mutable {
                                        CreateNativeApiViewOnGameThread(
                                            handle, std::move(config), std::move(initial_url));
                                    })) {
                                SetNativeApiViewFailed(handle);
                                SKSE::log::warn(
                                    "SkyUltra could not queue a CEF view after backend initialization.");
                            }
                        })) {
                    SetNativeApiViewFailed(handle);
                    SKSE::log::error("SkyUltra could not queue asynchronous CEF initialization.");
                }
                return;
            }

            CreateNativeApiViewOnGameThread(handle, std::move(config), std::move(initial_url));
        };
        if (show_splash_first) skyultra::RunAfterOverlaySplash(std::move(view_key),std::move(create));
        else create();
        PublishOverlayViews();
    });
    if (!queued) {
        const std::scoped_lock lock(g_native_views_mutex);
        g_native_views.erase(handle);
        return SKYULTRA_NOT_INITIALIZED;
    }
    *out_handle = handle;
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiCreateView(
    const SkyUltraViewConfigV1* config, SkyUltraViewHandle* out_handle)
{
    return NativeApiCreateViewForMod(config, out_handle, 0);
}

SkyUltraResult SKYULTRA_CALL NativeApiRegisterMod(
    const char* mod_id_utf8,
    std::uint32_t mod_id_length,
    SkyUltraModHandle* out_mod)
{
    constexpr std::uint32_t kMaximumModIdBytes = 64;
    if (mod_id_utf8 == nullptr || out_mod == nullptr || mod_id_length == 0 ||
        mod_id_length > kMaximumModIdBytes) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    std::string owned_id(mod_id_utf8, mod_id_length);
    for (auto& value : owned_id) {
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<char>(value - 'A' + 'a');
        }
    }
    const std::string_view mod_id(owned_id);
    if (mod_id.find('\0') != std::string_view::npos ||
        !skyultra::IsValidModIdentifier(mod_id)) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        if (const auto existing = g_native_mod_handles_by_id.find(owned_id);
            existing != g_native_mod_handles_by_id.end()) {
            *out_mod = existing->second;
            return SKYULTRA_OK;
        }
    }

    std::string error;
    const auto mod_content_directory = skyultra::GetInstalledWebModsDirectory();
    if (mod_content_directory.empty()) {
        SKSE::log::error("SkyUltra could not resolve Data\\SkyUltra for mod registration.");
        return SKYULTRA_INTERNAL_ERROR;
    }
    auto manifest = skyultra::LoadModManifest(mod_content_directory, mod_id, error);
    if (!manifest) {
        SKSE::log::warn("SkyUltra rejected mod manifest '{}': {}", owned_id, error);
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (manifest->browser_engine) {
        const auto requested_engine = *manifest->browser_engine;
        const auto selected_engine = EffectiveBrowserEngine(requested_engine);
        if (selected_engine == requested_engine) {
            SKSE::log::info("SkyUltra mod '{}' pins its views to {}.",
                owned_id, BrowserEngineName(selected_engine));
        } else {
            SKSE::log::warn(
                "SkyUltra mod '{}' requests {}, but that backend is not included in this build; its views will use {}.",
                owned_id, BrowserEngineName(requested_engine), BrowserEngineName(selected_engine));
        }
    }

    std::filesystem::path storage_directory;
    if (manifest->storage_enabled) {
        storage_directory = ModStorageDirectory(manifest->id, error);
        if (storage_directory.empty()) {
            SKSE::log::error("SkyUltra could not register mod '{}' storage: {}", owned_id, error);
            return SKYULTRA_INTERNAL_ERROR;
        }
    }

    auto handle = g_next_native_mod_handle.fetch_add(1, std::memory_order_relaxed);
    if (handle == 0) {
        return SKYULTRA_INTERNAL_ERROR;
    }
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        if (const auto existing = g_native_mod_handles_by_id.find(owned_id);
            existing != g_native_mod_handles_by_id.end()) {
            *out_mod = existing->second;
            return SKYULTRA_OK;
        }
        g_native_mod_handles_by_id.emplace(owned_id, handle);
        g_native_mods.emplace(handle, NativeApiModRecord{
            std::move(*manifest), std::move(storage_directory) });
    }
    *out_mod = handle;
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiCreateModView(
    SkyUltraModHandle mod,
    const SkyUltraViewConfigV1* config,
    SkyUltraViewHandle* out_view)
{
    if (mod == 0) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    return NativeApiCreateViewForMod(config, out_view, mod);
}

SkyUltraResult SKYULTRA_CALL NativeApiGetModStorageDirectory(
    SkyUltraModHandle mod,
    char* utf8_buffer,
    std::uint32_t buffer_capacity,
    std::uint32_t* required_bytes)
{
    if (mod == 0 || required_bytes == nullptr ||
        (utf8_buffer == nullptr && buffer_capacity != 0)) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    NativeApiModRecord record;
    {
        const std::scoped_lock lock(g_native_mods_mutex);
        const auto found = g_native_mods.find(mod);
        if (found == g_native_mods.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        record = found->second;
    }
    if (!record.manifest.storage_enabled || record.storage_directory.empty()) {
        return SKYULTRA_UNSUPPORTED;
    }
    const auto directory = PathUtf8(record.storage_directory);
    if (directory.size() >= std::numeric_limits<std::uint32_t>::max()) {
        return SKYULTRA_INTERNAL_ERROR;
    }
    *required_bytes = static_cast<std::uint32_t>(directory.size() + 1U);
    if (utf8_buffer == nullptr && buffer_capacity == 0) {
        return SKYULTRA_OK;
    }
    if (buffer_capacity < *required_bytes) {
        return SKYULTRA_BUFFER_TOO_SMALL;
    }
    std::memcpy(utf8_buffer, directory.data(), directory.size());
    utf8_buffer[directory.size()] = '\0';
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiDestroyView(SkyUltraViewHandle handle)
{
    if (handle == 0) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto it = g_native_views.find(handle);
        if (it == g_native_views.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        if (it->second.state == SKYULTRA_VIEW_CLOSING) {
            return SKYULTRA_OK;
        }
        it->second.state = SKYULTRA_VIEW_CLOSING;
        it->second.bridge_events->Close();
        it->second.bridge_responses->Close();
    }
    if (!QueueNativeApiTask([handle] {
            g_game_subscriptions.erase(handle);
            g_active_game_subscription_count.store(static_cast<std::uint32_t>(g_game_subscriptions.size()), std::memory_order_relaxed);
            std::erase_if(g_widget_overlays, [handle](const auto& entry) { return entry.second.view == handle; });
            { const std::scoped_lock lock(g_native_views_mutex);
              if (const auto record = g_native_views.find(handle); record != g_native_views.end() && record->second.pending_config)
                  {skyultra::CancelOverlaySplash(record->second.pending_config->id);skyultra::CancelFrameworkSplash(record->second.pending_config->id);} }
            if (const auto view_id = NativeManagerViewId(handle); view_id && g_view_manager != nullptr) {
                if(const auto* config=g_view_manager->Config(*view_id))skyultra::CancelFrameworkSplash(config->id);
                skyultra::RemoveActorPreview(*view_id);
                skyultra::CancelOverlayPageLoad(*view_id);
                g_suspended_panel_views.erase(*view_id);
                if (g_view_manager->Remove(*view_id)) {
                    PublishOverlayViews();
                } else {
                    SKSE::log::warn(
                        "SkyUltra native API could not remove view {} for handle {}.",
                        *view_id, handle);
                }
            }
            std::vector<std::pair<std::string, NativeApiViewRecord::PendingPageRequest>> cancelled;
            {
                const std::scoped_lock lock(g_native_views_mutex);
                if (const auto it = g_native_views.find(handle); it != g_native_views.end()) {
                    cancelled.reserve(it->second.pending_page_requests.size());
                    for (auto& request : it->second.pending_page_requests) {
                        cancelled.emplace_back(request.first, request.second);
                    }
                    g_native_views.erase(it);
                }
            }
            for (const auto& [request_id, request] : cancelled) {
                if (request.callback != nullptr) {
                    try {
                        request.callback(request.user_data, handle, request_id.c_str(),
                            static_cast<std::uint32_t>(request_id.size()), nullptr, 0, nullptr, 0,
                            SKYULTRA_PAGE_REQUEST_CANCELLED_V2);
                    } catch (...) {
                        SKSE::log::error(
                            "SkyUltra page request cancellation callback threw for '{}' on view {}.",
                            request_id, handle);
                    }
                } else if (skyultra::IsPapyrusView(handle)) {
                    (void)skyultra::DispatchPapyrusPageRequest(
                        handle, request_id, {}, {}, true);
                }
            }
            skyultra::ForgetPapyrusView(handle);
        })) {
        const std::scoped_lock lock(g_native_views_mutex);
        if (const auto it = g_native_views.find(handle); it != g_native_views.end()) {
            it->second.state = it->second.manager_id ? SKYULTRA_VIEW_READY : SKYULTRA_VIEW_PENDING;
        }
        return SKYULTRA_NOT_INITIALIZED;
    }
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiSetVisible(SkyUltraViewHandle handle, std::uint8_t visible)
{
    if (handle == 0 || visible > 1) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    if (!NativeHandleAcceptsCommands(handle)) {
        return SKYULTRA_NOT_FOUND;
    }
    if (!QueueNativeApiTask([handle, visible] {
            const auto view_id = NativeManagerViewId(handle);
            { const std::scoped_lock lock(g_native_views_mutex);
              if (const auto record = g_native_views.find(handle); record != g_native_views.end() && record->second.pending_config) record->second.pending_config->visible = visible != 0; }
            if (view_id && visible == 0) g_suspended_panel_views.erase(*view_id);
            if (view_id && g_view_manager != nullptr && g_view_manager->SetVisible(*view_id, visible != 0)) {
                const auto* config = g_view_manager->Config(*view_id);
                if (visible && config && !config->click_through && config->layer != skyultra::ViewLayer::Hud) {
                    if (g_modal_menu_open) g_suspended_panel_views.insert(*view_id);
                    else g_browser_overlay_suppressed.store(false, std::memory_order_relaxed);
                }
                if(visible && !g_browser_overlay_suppressed.load(std::memory_order_relaxed) && !REL::Module::IsVR() && g_view_manager->Config(*view_id) && !g_view_manager->Config(*view_id)->click_through && g_view_manager->Config(*view_id)->layer!=skyultra::ViewLayer::Hud){
                    std::string mod_id=g_view_manager->Config(*view_id)->id;
                    {const std::scoped_lock views_lock(g_native_views_mutex);if(const auto owner=g_native_views.find(handle);owner!=g_native_views.end()){
                        const std::scoped_lock mods_lock(g_native_mods_mutex);if(const auto mod=g_native_mods.find(owner->second.mod_handle);mod!=g_native_mods.end())mod_id=mod->second.manifest.id;}}
                    skyultra::BeginFrameworkSplash(g_view_manager->Config(*view_id)->id,std::move(mod_id),!skyultra::OverlayPageAwaitingReady(*view_id));
                }
                if(!visible && g_view_manager->Config(*view_id))skyultra::CancelFrameworkSplash(g_view_manager->Config(*view_id)->id);
                if (const auto sub = g_game_subscriptions.find(handle); sub != g_game_subscriptions.end()) {
                    sub->second.needs_catchup = true;
                }
                PublishOverlayViews();
                SyncSkyUltraWorldPause();
                PublishBrowserCursor(HasVisibleInteractiveView());
                if (visible && !g_game_subscriptions.empty()) {
                    PumpGameDataSubscriptions(true);
                }
            }
        })) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiSetBounds(SkyUltraViewHandle handle, SkyUltraRectV1 bounds)
{
    if (handle == 0 || !std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
        !std::isfinite(bounds.width) || !std::isfinite(bounds.height)) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    if (!NativeHandleAcceptsCommands(handle)) {
        return SKYULTRA_NOT_FOUND;
    }
    if (!QueueNativeApiTask([handle, bounds] {
            const auto view_id = NativeManagerViewId(handle);
            if (!view_id || g_view_manager == nullptr) {
                return;
            }
            const auto* current = g_view_manager->Config(*view_id);
            if (current == nullptr) {
                return;
            }
            auto updated = *current;
            updated.bounds = { bounds.x, bounds.y, bounds.width, bounds.height };
            if (g_view_manager->UpdateConfig(*view_id, updated)) {
                PublishOverlayViews();
            }
        })) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiNavigate(
    SkyUltraViewHandle handle, const char* url_utf8, std::uint32_t url_length)
{
    if (handle == 0 || url_utf8 == nullptr || url_length == 0 || url_length > 65536) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    if (!NativeHandleAcceptsCommands(handle)) {
        return SKYULTRA_NOT_FOUND;
    }
    const std::string_view url(url_utf8, url_length);
    if (url.find('\0') != std::string_view::npos) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    std::string navigation(url);
    if (!QueueNativeApiTask([handle, navigation = std::move(navigation)] {
            g_game_subscriptions.erase(handle);
            g_active_game_subscription_count.store(static_cast<std::uint32_t>(g_game_subscriptions.size()), std::memory_order_relaxed);
            const auto view_id = NativeManagerViewId(handle);
            auto* view = view_id && g_view_manager != nullptr ? g_view_manager->Find(*view_id) : nullptr;
            if (view != nullptr) {
                skyultra::RemoveActorPreview(*view_id);
                auto engine = skyultra::BrowserEngine::Ultralight;
                {
                    const std::scoped_lock lock(g_native_views_mutex);
                    if (const auto found = g_native_views.find(handle); found != g_native_views.end()) {
                        engine = found->second.browser_engine;
                        found->second.input_scope_depth = 0;
                        if (found->second.wait_for_page_ready) skyultra::BeginOverlayPageLoad(*view_id);
                    }
                }
                view->Navigate(AddBrowserEngineToLocalUrl(navigation, engine));
            }
        })) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiReloadView(SkyUltraViewHandle handle)
{
    if (handle == 0) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    std::string initial_url;
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto view = g_native_views.find(handle);
        if (view == g_native_views.end() || view->second.state == SKYULTRA_VIEW_CLOSING ||
            view->second.state == SKYULTRA_VIEW_FAILED) {
            return SKYULTRA_NOT_FOUND;
        }
        initial_url = view->second.initial_url;
    }
    if (!QueueNativeApiTask([handle, initial_url = std::move(initial_url)] {
            g_game_subscriptions.erase(handle);
            g_active_game_subscription_count.store(static_cast<std::uint32_t>(g_game_subscriptions.size()), std::memory_order_relaxed);
            const auto view_id = NativeManagerViewId(handle);
            auto* view = view_id && g_view_manager != nullptr ? g_view_manager->Find(*view_id) : nullptr;
            if (view != nullptr) {
                skyultra::RemoveActorPreview(*view_id);
                auto engine = skyultra::BrowserEngine::Ultralight;
                {
                    const std::scoped_lock lock(g_native_views_mutex);
                    if (const auto found = g_native_views.find(handle); found != g_native_views.end()) {
                        engine = found->second.browser_engine;
                        found->second.input_scope_depth = 0;
                        if (found->second.wait_for_page_ready) skyultra::BeginOverlayPageLoad(*view_id);
                    }
                }
                view->Navigate(AddBrowserEngineToLocalUrl(initial_url, engine));
            }
        })) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiSendEventJson(
    SkyUltraViewHandle handle,
    const char* event_name_utf8,
    std::uint32_t event_name_length,
    const char* json_utf8,
    std::uint32_t json_length)
{
    if (handle == 0 || event_name_utf8 == nullptr || event_name_length == 0 ||
        event_name_length > 128 || json_utf8 == nullptr || json_length == 0 ||
        json_length > 1048576) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    const std::string_view event_name(event_name_utf8, event_name_length);
    const std::string_view payload(json_utf8, json_length);
    if (event_name.find('\0') != std::string_view::npos ||
        payload.find('\0') != std::string_view::npos ||
        !nlohmann::json::accept(payload.begin(), payload.end())) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!NativeHandleAcceptsCommands(handle)) {
        return SKYULTRA_NOT_FOUND;
    }
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto it = g_native_views.find(handle);
        if (it == g_native_views.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        if (!it->second.javascript_enabled) {
            return SKYULTRA_UNSUPPORTED;
        }
    }
    std::shared_ptr<skyultra::BridgeEventQueue> queue;
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto it = g_native_views.find(handle);
        if (it == g_native_views.end() || it->second.state == SKYULTRA_VIEW_CLOSING ||
            it->second.state == SKYULTRA_VIEW_FAILED) return SKYULTRA_NOT_FOUND;
        queue = it->second.bridge_events;
    }
    try {
        switch (queue->Push(event_name, payload)) {
        case skyultra::BridgeEventQueue::Result::Accepted: return SKYULTRA_OK;
        case skyultra::BridgeEventQueue::Result::Full: return SKYULTRA_QUEUE_FULL;
        case skyultra::BridgeEventQueue::Result::Closed: return SKYULTRA_NOT_FOUND;
        }
    } catch (...) { return SKYULTRA_INTERNAL_ERROR; }
    return SKYULTRA_INTERNAL_ERROR;
}

SkyUltraResult SKYULTRA_CALL NativeApiSetPageRequestCallback(
    SkyUltraPageRequestCallbackV2 callback, void* user_data)
{
    if (callback == nullptr && user_data != nullptr) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    const std::scoped_lock lock(g_page_request_callback_mutex);
    g_page_request_callback = callback;
    g_page_request_user_data = user_data;
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiSetPageRequestAllowlist(
    SkyUltraViewHandle handle,
    const SkyUltraPageRequestMethodV2* methods,
    std::uint32_t method_count)
{
    constexpr std::uint32_t kMaxMethods = 128;
    constexpr std::uint32_t kMaxMethodNameBytes = 128;
    if (handle == 0 || method_count > kMaxMethods ||
        (method_count != 0 && methods == nullptr)) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }
    if (!NativeHandleAcceptsCommands(handle)) {
        return SKYULTRA_NOT_FOUND;
    }

    std::unordered_set<std::string> allowed_methods;
    try {
        allowed_methods.reserve(method_count);
        for (std::uint32_t index = 0; index < method_count; ++index) {
            const auto& method = methods[index];
            if (method.name_utf8 == nullptr || method.name_length == 0 ||
                method.name_length > kMaxMethodNameBytes) {
                return SKYULTRA_INVALID_ARGUMENT;
            }
            const std::string_view name(method.name_utf8, method.name_length);
            if (name.find('\0') != std::string_view::npos) {
                return SKYULTRA_INVALID_ARGUMENT;
            }
            auto owned_name = std::string(name);
            (void)nlohmann::json(owned_name).dump();
            if (!allowed_methods.emplace(std::move(owned_name)).second) {
                return SKYULTRA_INVALID_ARGUMENT;
            }
        }
    } catch (...) {
        return SKYULTRA_INTERNAL_ERROR;
    }

    SkyUltraModHandle mod_handle{};
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto view = g_native_views.find(handle);
        if (view == g_native_views.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        if (view->second.state == SKYULTRA_VIEW_CLOSING ||
            view->second.state == SKYULTRA_VIEW_FAILED) {
            return SKYULTRA_NOT_FOUND;
        }
        if (!view->second.javascript_enabled) {
            return SKYULTRA_UNSUPPORTED;
        }
        mod_handle = view->second.mod_handle;
    }
    if (mod_handle != 0) {
        const std::scoped_lock lock(g_native_mods_mutex);
        const auto mod = g_native_mods.find(mod_handle);
        if (mod == g_native_mods.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        for (const auto& method : allowed_methods) {
            if (!mod->second.manifest.page_request_methods.contains(method)) {
                return SKYULTRA_UNSUPPORTED;
            }
        }
    }
    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto view = g_native_views.find(handle);
        if (view == g_native_views.end() || view->second.mod_handle != mod_handle ||
            view->second.state == SKYULTRA_VIEW_CLOSING ||
            view->second.state == SKYULTRA_VIEW_FAILED) {
            return SKYULTRA_NOT_FOUND;
        }
        view->second.allowed_page_request_methods = std::move(allowed_methods);
    }
    return SKYULTRA_OK;
}

SkyUltraResult SKYULTRA_CALL NativeApiCompletePageRequest(
    SkyUltraViewHandle handle,
    const char* request_id_utf8,
    std::uint32_t request_id_length,
    std::uint8_t success,
    const char* result_json_utf8,
    std::uint32_t result_json_length,
    const char* error_utf8,
    std::uint32_t error_length)
{
    constexpr std::uint32_t kMaxRequestIdBytes = 128;
    constexpr std::uint32_t kMaxPayloadBytes = 1048576;
    constexpr std::uint32_t kMaxErrorBytes = 4096;
    if (handle == 0 || request_id_utf8 == nullptr || request_id_length == 0 ||
        request_id_length > kMaxRequestIdBytes || success > 1 ||
        (result_json_length != 0 && result_json_utf8 == nullptr) ||
        result_json_length > kMaxPayloadBytes || (error_length != 0 && error_utf8 == nullptr) ||
        error_length > kMaxErrorBytes) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (!g_native_api_ready.load(std::memory_order_acquire)) {
        return SKYULTRA_NOT_INITIALIZED;
    }

    const std::string_view request_id(request_id_utf8, request_id_length);
    const std::string_view result_json(result_json_utf8 != nullptr ? result_json_utf8 : "",
        result_json_length);
    const std::string_view error(error_utf8 != nullptr ? error_utf8 : "", error_length);
    if (request_id.find('\0') != std::string_view::npos ||
        (success != 0 && (result_json.empty() || error_length != 0 ||
            !nlohmann::json::accept(result_json.begin(), result_json.end()))) ||
        (success == 0 && (result_json_length != 0 || error.empty() ||
            error.find('\0') != std::string_view::npos))) {
        return SKYULTRA_INVALID_ARGUMENT;
    }
    if (success == 0) {
        try {
            (void)nlohmann::json(error).dump();
        } catch (...) {
            return SKYULTRA_INVALID_ARGUMENT;
        }
    }
    if (!NativeHandleAcceptsCommands(handle)) {
        return SKYULTRA_NOT_FOUND;
    }

    {
        const std::scoped_lock lock(g_native_views_mutex);
        const auto view = g_native_views.find(handle);
        if (view == g_native_views.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        if (!view->second.javascript_enabled) {
            return SKYULTRA_UNSUPPORTED;
        }
        const auto request = view->second.pending_page_requests.find(std::string(request_id));
        if (request == view->second.pending_page_requests.end()) {
            return SKYULTRA_NOT_FOUND;
        }
        if (request->second.completion_queued) {
            return SKYULTRA_NOT_FOUND;
        }
        request->second.completion_queued = true;
    }

    auto accepted = skyultra::BridgeEventQueue::Result::Closed;
    bool internal_error{};
    try {
        std::shared_ptr<skyultra::BridgeEventQueue> queue;
        { const std::scoped_lock lock(g_native_views_mutex); const auto it = g_native_views.find(handle);
          if (it == g_native_views.end()) return SKYULTRA_NOT_FOUND; queue = it->second.bridge_responses; }
        accepted = queue->Push(request_id, skyultra::BuildBridgeResponseJson(request_id, success != 0, result_json, error));
    } catch (...) { internal_error = true; }
    {
        const std::scoped_lock lock(g_native_views_mutex);
        if (const auto owner = g_native_views.find(handle); owner != g_native_views.end()) {
            if (accepted == skyultra::BridgeEventQueue::Result::Accepted) owner->second.pending_page_requests.erase(std::string(request_id));
            else if (const auto request = owner->second.pending_page_requests.find(std::string(request_id)); request != owner->second.pending_page_requests.end()) request->second.completion_queued = false;
        }
    }
    if (internal_error) return SKYULTRA_INTERNAL_ERROR;
    if (accepted == skyultra::BridgeEventQueue::Result::Accepted) return SKYULTRA_OK;
    return accepted == skyultra::BridgeEventQueue::Result::Full ? SKYULTRA_QUEUE_FULL : SKYULTRA_NOT_FOUND;
}

const SkyUltraNativeAPI_V1 g_native_api_v1{
    1,
    static_cast<std::uint32_t>(sizeof(SkyUltraNativeAPI_V1)),
    NativeApiCreateView,
    NativeApiGetViewState,
    NativeApiDestroyView,
    NativeApiSetVisible,
    NativeApiSetBounds,
    NativeApiNavigate,
    NativeApiSendEventJson,
};

const SkyUltraNativeAPI_V2 g_native_api_v2{
    2,
    static_cast<std::uint32_t>(sizeof(SkyUltraNativeAPI_V2)),
    NativeApiCreateView,
    NativeApiGetViewState,
    NativeApiDestroyView,
    NativeApiSetVisible,
    NativeApiSetBounds,
    NativeApiNavigate,
    NativeApiSendEventJson,
    NativeApiSetPageRequestCallback,
    NativeApiCompletePageRequest,
    NativeApiSetPageRequestAllowlist,
};

const SkyUltraNativeAPI_V3 g_native_api_v3{
    3,
    static_cast<std::uint32_t>(sizeof(SkyUltraNativeAPI_V3)),
    NativeApiCreateView,
    NativeApiGetViewState,
    NativeApiDestroyView,
    NativeApiSetVisible,
    NativeApiSetBounds,
    NativeApiNavigate,
    NativeApiSendEventJson,
    NativeApiSetPageRequestCallback,
    NativeApiCompletePageRequest,
    NativeApiSetPageRequestAllowlist,
    NativeApiRegisterMod,
    NativeApiCreateModView,
    NativeApiGetModStorageDirectory,
};

const SkyUltraNativeAPI_V4 g_native_api_v4{
    4,
    static_cast<std::uint32_t>(sizeof(SkyUltraNativeAPI_V4)),
    NativeApiCreateView,
    NativeApiGetViewState,
    NativeApiDestroyView,
    NativeApiSetVisible,
    NativeApiSetBounds,
    NativeApiNavigate,
    NativeApiSendEventJson,
    NativeApiSetPageRequestCallback,
    NativeApiCompletePageRequest,
    NativeApiSetPageRequestAllowlist,
    NativeApiRegisterMod,
    NativeApiCreateModView,
    NativeApiGetModStorageDirectory,
    NativeApiReloadView,
};

#endif

void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
{
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    if (message == nullptr) {
        return;
    }
    if (message->type == SKSE::MessagingInterface::kInputLoaded) {
        TryRegisterInputSink();
    } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
               message->type == SKSE::MessagingInterface::kNewGame) {
        g_game_session_active.store(message->type == SKSE::MessagingInterface::kNewGame, std::memory_order_relaxed);
        skyultra::InvalidateSharedGameDataCache();
        skyultra::ClearActorPreviews();
        for (const auto& [mod, widgets] : g_widget_overlays) { (void)mod; (void)NativeApiDestroyView(widgets.view); }
        g_widget_overlays.clear();
        for (auto& [sub_handle, sub_record] : g_game_subscriptions) {
            (void)sub_handle;
            sub_record.topic_signatures.clear();
            sub_record.needs_catchup = true;
        }
        if (message->type == SKSE::MessagingInterface::kNewGame) {
            (void)QueueNativeApiTask([] {
                g_game_session_active.store(true, std::memory_order_relaxed);
                g_modal_menu_open = IsModalOrBlockingMenuOpen(RE::UI::GetSingleton());
                SyncVanillaCompassVisibility();
                if (g_view_manager != nullptr && g_menu_view_id != 0) {
                    if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                        (void)menu_view->SendEventJson("skyultra.syncHudOverlay", R"({"reason":"newGame"})");
                    }
                }
                PublishOverlayViews();
                PumpGameDataSubscriptions(true);
            });
        }
    } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
        g_game_session_active.store(true, std::memory_order_relaxed);
        (void)QueueNativeApiTask([] {
            g_game_session_active.store(true, std::memory_order_relaxed);
            g_modal_menu_open = IsModalOrBlockingMenuOpen(RE::UI::GetSingleton());
            SyncVanillaCompassVisibility();
            if (g_view_manager != nullptr && g_menu_view_id != 0) {
                if (auto* menu_view = g_view_manager->Find(g_menu_view_id)) {
                    (void)menu_view->SendEventJson("skyultra.syncHudOverlay", R"({"reason":"postLoadGame"})");
                }
            }
            PublishOverlayViews();
            PumpGameDataSubscriptions(true);
        });
    } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        StartBrowser();
    }
#else
    (void)message;
#endif
}

}  // namespace

namespace skyultra {

bool PapyrusApiReady() noexcept
{
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    return g_native_api_ready.load(std::memory_order_acquire);
#else
    return false;
#endif
}

bool QueueActorRendererRequest(SkyUltraViewHandle handle,std::string method,std::string params_json)
{
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    if((method!="game.setActorPreview"&&method!="game.closeActorPreview")||params_json.size()>16384||!g_native_api_ready.load())return false;
    {const std::scoped_lock lock(g_native_views_mutex);const auto found=g_native_views.find(handle);
        if(found==g_native_views.end()||found->second.state!=SKYULTRA_VIEW_READY||!found->second.allowed_page_request_methods.contains(method))return false;}
    return QueueNativeApiTask([handle,method=std::move(method),params_json=std::move(params_json)]{
        const auto view_id=NativeManagerViewId(handle);if(!view_id||!g_view_manager)return;
        auto* view=g_view_manager->Find(*view_id);if(!view)return;
        try{(void)view->SendEventJson("skyultra.renderer",ApplyActorRenderer(*view_id,method,params_json));}
        catch(const std::exception& error){(void)view->SendEventJson("skyultra.renderer",nlohmann::json{{"available",false},{"ready",false},{"message",error.what()}}.dump());}
    });
#else
    (void)handle;(void)method;(void)params_json;return false;
#endif
}
std::string ReadActorRendererStatus(SkyUltraViewHandle handle)
{
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    const auto view=NativeManagerViewId(handle);if(!view)return R"({"available":false,"ready":false})";
    const auto status=GetActorPreviewStatus(*view);
    return nlohmann::json{{"available",status.available},{"ready",status.ready},{"referenceId",std::to_string(status.reference_id)},
        {"geometryCount",status.geometry_count},{"skippedGeometryCount",status.skipped_geometry_count},{"posesAvailable",status.poses_available},{"message",status.message}}.dump();
#else
    (void)handle;return R"({"available":false,"ready":false})";
#endif
}
void RequestCloseInventoryReplacement(std::string_view reason)
{
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    // Deferred so the page request that triggered this resolves before its view is hidden.
    (void)QueueNativeApiTask([reason = std::string(reason)] {
        if ((g_inventory_replacement_active || g_favorites_replacement_active ||
             g_container_replacement_active || g_mannequin_replacement_active ||
             g_loot_replacement_active || g_pickpocket_replacement_active) && HasVisibleInteractiveView()) {
            SetBrowserVisible(false, reason.c_str());
        }
    });
#else
    (void)reason;
#endif
}
}  // namespace skyultra

extern "C" SKYULTRA_EXPORT const SkyUltraNativeAPI_V1* SKYULTRA_CALL SkyUltra_GetAPI_V1(void)
{
    return &g_native_api_v1;
}

extern "C" SKYULTRA_EXPORT const SkyUltraNativeAPI_V2* SKYULTRA_CALL SkyUltra_GetAPI_V2(void)
{
    return &g_native_api_v2;
}

extern "C" SKYULTRA_EXPORT const SkyUltraNativeAPI_V3* SKYULTRA_CALL SkyUltra_GetAPI_V3(void)
{
    return &g_native_api_v3;
}

extern "C" SKYULTRA_EXPORT const SkyUltraNativeAPI_V4* SKYULTRA_CALL SkyUltra_GetAPI_V4(void)
{
    return &g_native_api_v4;
}

SKSEPluginInfo(
    .Version = { 0, 1, 0, 0 },
    .Name = "SkyUltra"sv,
    .Author = "SkyUltra"sv,
    .SupportEmail = ""sv,
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
)

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    g_game_task_queue.Bind(SKSE::GetTaskInterface());
#endif
    g_runtime_name = RuntimeName(skse);
    SKSE::log::info("SkyUltra v0-1-0-0");
    const std::string runtime_version = skse != nullptr ?
        skse->RuntimeVersion().string(".") : std::string("unknown");
    SKSE::log::info("SkyUltra detected runtime: {} ({}).", g_runtime_name, runtime_version);
#if defined(SKYULTRA_WITH_CEF) || defined(SKYULTRA_WITH_ULTRALIGHT)
    const auto* messaging = SKSE::GetMessagingInterface();
    if (messaging == nullptr || !messaging->RegisterListener(OnSKSEMessage)) {
        SKSE::log::error("SkyUltra could not register for SKSE lifecycle messages.");
        return false;
    }
    const auto* papyrus = SKSE::GetPapyrusInterface();
    if (papyrus == nullptr || !papyrus->Register(skyultra::RegisterPapyrusFunctions)) {
        SKSE::log::warn(
            "SkyUltra could not register its Papyrus API; native C ABI access remains available.");
    }
    SKSE::log::info("SkyUltra SKSE host loaded; waiting for DataLoaded to start the browser.");
#else
    SKSE::log::info("SkyUltra SKSE host loaded without a browser backend.");
#endif
    return true;
}
