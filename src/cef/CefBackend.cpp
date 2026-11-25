#include "SkyUltra/CefBackend.h"
#include "SkyUltra/ModManifest.h"
#include "SkyUltra/CefRecoveryPolicy.h"
#include "SkyUltra/FrameTrace.h"
#include "SkyUltra/LiveCompassState.h"
#include "include/cef_crash_util.h"

#include <windows.h>
#include <d3d11_1.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_client.h"
#include "include/cef_download_handler.h"
#include "include/cef_process_message.h"
#include "include/cef_parser.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_render_handler.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace skyultra {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::uint32_t kMaxCefWindowlessFrameRate = 120;
constexpr std::size_t kCefGpuFramePoolSize = 4;
constexpr char kEditableFocusMessage[] = "skyultra_editable_focus";
constexpr char kBridgeBatchAckMessage[] = "skyultra_bridge_batch_ack";
constexpr char kBridgeBatchDeliverMessage[] = "skyultra_bridge_deliver";
constexpr char kRendererBridgeReadyMessage[] = "skyultra_renderer_bridge_ready";
constexpr char kPageBridgeMessage[] = "skyultra_page_bridge";
constexpr char kContextLossMessage[] = "skyultra_webgl_context_lost";
constexpr char kBoostPriorityMessage[] = "skyultra_boost_priority";

constexpr bool IsFatalGpuResourceFailure(HRESULT reason) noexcept
{
    return reason==E_OUTOFMEMORY || reason==DXGI_ERROR_DEVICE_REMOVED || reason==DXGI_ERROR_DEVICE_RESET ||
        reason==DXGI_ERROR_DEVICE_HUNG || reason==DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}
static_assert(IsFatalGpuResourceFailure(E_OUTOFMEMORY));
static_assert(IsFatalGpuResourceFailure(DXGI_ERROR_DEVICE_REMOVED));
static_assert(IsFatalGpuResourceFailure(DXGI_ERROR_DEVICE_RESET));
static_assert(IsFatalGpuResourceFailure(DXGI_ERROR_DEVICE_HUNG));
static_assert(IsFatalGpuResourceFailure(DXGI_ERROR_DRIVER_INTERNAL_ERROR));
static_assert(!IsFatalGpuResourceFailure(S_OK) && !IsFatalGpuResourceFailure(E_INVALIDARG) && !IsFatalGpuResourceFailure(E_NOINTERFACE));

bool IsProcessShutdownInProgress() noexcept
{
    using RtlDllShutdownInProgressFn = BOOLEAN(NTAPI*)();
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return false;
    }
    const auto fn = reinterpret_cast<RtlDllShutdownInProgressFn>(
        GetProcAddress(ntdll, "RtlDllShutdownInProgress"));
    return fn != nullptr && fn() != FALSE;
}

bool IsProcessElevated() noexcept
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) || token == nullptr) {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD returned_size = 0;
    const BOOL ok = GetTokenInformation(
        token, TokenElevation, &elevation, sizeof(elevation), &returned_size);
    CloseHandle(token);
    return ok != FALSE && elevation.TokenIsElevated != 0;
}

const char* CefExitCodeName(int code) noexcept
{
    switch (code) {
    case CEF_RESULT_CODE_NORMAL_EXIT: return "NORMAL_EXIT";
    case CEF_RESULT_CODE_KILLED: return "KILLED";
    case CEF_RESULT_CODE_HUNG: return "HUNG";
    case CEF_RESULT_CODE_KILLED_BAD_MESSAGE: return "KILLED_BAD_MESSAGE";
    case CEF_RESULT_CODE_GPU_DEAD_ON_ARRIVAL: return "GPU_DEAD_ON_ARRIVAL";
#if CEF_API_ADDED(13800)
    case CEF_RESULT_CODE_BAD_PROCESS_TYPE: return "BAD_PROCESS_TYPE";
#endif
    case CEF_RESULT_CODE_MISSING_DATA: return "MISSING_DATA";
    case CEF_RESULT_CODE_UNSUPPORTED_PARAM: return "UNSUPPORTED_PARAM";
    case CEF_RESULT_CODE_PROFILE_IN_USE: return "PROFILE_IN_USE";
    case CEF_RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED: return "NORMAL_EXIT_PROCESS_NOTIFIED";
    case CEF_RESULT_CODE_INVALID_SANDBOX_STATE: return "INVALID_SANDBOX_STATE";
    case CEF_RESULT_CODE_SYSTEM_RESOURCE_EXHAUSTED: return "SYSTEM_RESOURCE_EXHAUSTED";
#if CEF_API_ADDED(13800)
    case CEF_RESULT_CODE_NORMAL_EXIT_AUTO_DE_ELEVATED: return "NORMAL_EXIT_AUTO_DE_ELEVATED";
#endif
#if CEF_API_ADDED(13900)
    case CEF_RESULT_CODE_TERMINATED_BY_OTHER_PROCESS_ON_COMMIT_FAILURE: return "TERMINATED_BY_OTHER_PROCESS_ON_COMMIT_FAILURE";
#endif
#if CEF_API_ADDED(14700)
    case CEF_RESULT_CODE_INVALID_ISOLATED_BROWSER_PROCESS: return "INVALID_ISOLATED_BROWSER_PROCESS";
#endif
    default: return "UNKNOWN";
    }
}

bool IsProcessAlive(DWORD pid) noexcept
{
    if (pid == 0) {
        return false;
    }
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return GetLastError() == ERROR_ACCESS_DENIED;
    }
    DWORD exit_code = 0;
    const BOOL ok = GetExitCodeProcess(process, &exit_code);
    CloseHandle(process);
    return ok != FALSE && exit_code == STILL_ACTIVE;
}

void PruneStaleSessionCacheDirectories(const std::filesystem::path& parent_dir) noexcept
{
    if (parent_dir.empty()) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(parent_dir, ec) || ec ||
        !std::filesystem::is_directory(parent_dir, ec) || ec) {
        return;
    }
    constexpr std::wstring_view kPrefix = L"CEFCache_session_";
    const DWORD self_pid = GetCurrentProcessId();
    for (const auto& entry : std::filesystem::directory_iterator(parent_dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory(ec) || ec) {
            ec.clear();
            continue;
        }
        const auto name = entry.path().filename().wstring();
        if (!name.starts_with(kPrefix) || name.size() <= kPrefix.size()) {
            continue;
        }
        const auto suffix = std::wstring_view(name).substr(kPrefix.size());
        DWORD pid = 0;
        bool valid_pid = true;
        for (const wchar_t ch : suffix) {
            if (ch < L'0' || ch > L'9') {
                valid_pid = false;
                break;
            }
            const auto digit = static_cast<DWORD>(ch - L'0');
            if (pid > (MAXDWORD - digit) / 10U) {
                valid_pid = false;
                break;
            }
            pid = pid * 10U + digit;
        }
        if (!valid_pid || pid == 0 || pid == self_pid || IsProcessAlive(pid)) {
            continue;
        }
        std::filesystem::remove_all(entry.path(), ec);
        ec.clear();
    }
}

bool IsCacheDirectoryLockedByOtherProcess(const std::filesystem::path& cache_dir) noexcept
{
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(cache_dir, ec);
    if (ec || normalized.empty()) {
        ec.clear();
        normalized = std::filesystem::absolute(cache_dir, ec);
    }
    if (!ec && !normalized.empty()) {
        auto window_title = normalized.wstring();
        while (window_title.size() > 3 && (window_title.back() == L'\\' || window_title.back() == L'/')) {
            window_title.pop_back();
        }
        if (!window_title.empty() &&
            FindWindowExW(HWND_MESSAGE, nullptr, nullptr, window_title.c_str()) != nullptr) {
            return true;
        }
    }
    ec.clear();
    const auto lockfile = cache_dir / L"lockfile";
    if (std::filesystem::exists(lockfile, ec) && !ec) {
        const HANDLE handle = CreateFileW(
            lockfile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            return false;
        }
        const DWORD err = GetLastError();
        return err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION;
    }
    return false;
}

std::string LowerAscii(std::string value)
{
    for (auto& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

bool HasResidualPercentEscape(std::string_view value) noexcept
{
    const auto is_hex = [](char character) noexcept {
        return (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
    };
    for (std::size_t index = 0; index + 2 < value.size(); ++index) {
        if (value[index] == '%' && is_hex(value[index + 1]) && is_hex(value[index + 2])) {
            return true;
        }
    }
    return false;
}

bool EqualsOrdinalInsensitive(std::wstring_view left, std::wstring_view right) noexcept
{
    return left.size() == right.size() &&
        CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
            right.data(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool HasParentTraversal(const std::filesystem::path& path)
{
    for (const auto& part : path) {
        const auto& native = part.native();
        if (native == L".." || native.find(L'\0') != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

std::filesystem::path NormalizeDirPath(std::filesystem::path path)
{
    path = path.lexically_normal();
    while (!path.empty() && (path.filename().empty() || path.filename() == L".")) {
        const auto parent = path.parent_path();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return path;
}

bool IsCanonicalThumbnailCacheLeaf(
    const std::filesystem::path& canonical_file,
    std::wstring_view expected_subdir,
    std::wstring_view expected_filename)
{
    std::array<std::wstring, 4> tail{};
    std::size_t total{};
    for (const auto& part : canonical_file) {
        const auto& native = part.native();
        if (native.empty() || native == L".") {
            continue;
        }
        tail[0] = std::move(tail[1]);
        tail[1] = std::move(tail[2]);
        tail[2] = std::move(tail[3]);
        tail[3] = native;
        ++total;
    }
    return total >= 4U &&
        EqualsOrdinalInsensitive(tail[0], L"SkyUltra") &&
        EqualsOrdinalInsensitive(tail[1], L"Cache") &&
        EqualsOrdinalInsensitive(tail[2], expected_subdir) &&
        EqualsOrdinalInsensitive(tail[3], expected_filename);
}

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
        if (CompareStringOrdinal(root_text.c_str(), static_cast<int>(root_text.size()),
                path_text.c_str(), static_cast<int>(path_text.size()), TRUE) != CSTR_EQUAL) {
            return false;
        }
    }
    return true;
}

bool IsThumbnailCacheRequestAllowed(
    const std::filesystem::path& cache_root,
    const std::filesystem::path& raw_path,
    const std::filesystem::path& canonical_file)
{
    if (cache_root.empty()) {
        return false;
    }
    if (!EqualsOrdinalInsensitive(canonical_file.extension().native(), L".png")) {
        return false;
    }
    std::error_code error;
    if (std::filesystem::is_symlink(raw_path, error) && !error) {
        return false;
    }
    if (IsPathWithin(cache_root, canonical_file)) {
        return true;
    }
    error.clear();
    const auto canonical_root = std::filesystem::canonical(cache_root, error);
    if (!error && IsPathWithin(canonical_root, canonical_file)) {
        return true;
    }

    // MO2 USVFS redirects reads from Data\SkyUltra\Cache\<Items|Portraits>\*.png
    // to Overwrite\ or a mod folder, so canonical(raw_path) resolves outside the
    // virtual Data\ root. Validate the unredirected path + canonical leaf shape.
    if (HasParentTraversal(raw_path)) {
        return false;
    }
    const auto norm_root = NormalizeDirPath(cache_root);
    const auto norm_raw = raw_path.lexically_normal();
    if (norm_root.empty() || !IsPathWithin(norm_root, norm_raw)) {
        return false;
    }
    const auto rel = norm_raw.lexically_relative(norm_root);
    std::vector<std::wstring> parts;
    for (const auto& part : rel) {
        const auto& native = part.native();
        if (native.empty() || native == L".") {
            continue;
        }
        if (native == L"..") {
            return false;
        }
        parts.push_back(native);
    }
    if (parts.size() != 2U) {
        return false;
    }
    if (!EqualsOrdinalInsensitive(parts[0], L"Items") &&
        !EqualsOrdinalInsensitive(parts[0], L"Portraits")) {
        return false;
    }
    const auto rel_ext = std::filesystem::path(parts[1]).extension().native();
    if (!EqualsOrdinalInsensitive(rel_ext, L".png")) {
        return false;
    }
    return IsCanonicalThumbnailCacheLeaf(canonical_file, parts[0], parts[1]);
}

bool IsModRequestAllowed(const ViewConfig& config, const CefString& url)
{
    if (!config.mod_content_policy) {
        return true;
    }
    const auto& policy = *config.mod_content_policy;
    CefURLParts parts;
    if (!CefParseURL(url, parts)) {
        return false;
    }
    if (!CefString(&parts.username).ToString().empty() ||
        !CefString(&parts.password).ToString().empty()) {
        return false;
    }
    const auto scheme = LowerAscii(CefString(&parts.scheme).ToString());
    if (scheme == "data" || scheme == "blob" || scheme == "about") {
        return true;
    }
    if (scheme == "http" || scheme == "https") {
        const auto host = LowerAscii(CefString(&parts.host).ToString());
        return IsModNetworkHostAllowed(policy, scheme, host);
    }
    if (scheme != "file" || !CefString(&parts.host).ToString().empty()) {
        return false;
    }

    const auto escaped_path = CefString(&parts.path);
    const auto unescape_rule = static_cast<cef_uri_unescape_rule_t>(UU_NORMAL | UU_SPACES);
    const auto decoded_path = CefURIDecode(escaped_path, true, unescape_rule).ToString();
    if (HasResidualPercentEscape(decoded_path)) {
        return false;
    }
    auto local_path = decoded_path;
    if (local_path.size() >= 3 && local_path[0] == '/' &&
        ((local_path[1] >= 'A' && local_path[1] <= 'Z') ||
         (local_path[1] >= 'a' && local_path[1] <= 'z')) && local_path[2] == ':') {
        local_path.erase(local_path.begin());
    }
    std::error_code error;
    const auto raw_requested_path = std::filesystem::u8path(local_path);
    const auto requested_file = std::filesystem::canonical(raw_requested_path, error);
    if (error || !std::filesystem::is_regular_file(requested_file, error) || error) {
        return false;
    }
    const auto requested_native = requested_file.native();
    const auto entry_native = policy.entry_file.native();
    if (requested_native.size() == entry_native.size() &&
        CompareStringOrdinal(requested_native.c_str(), static_cast<int>(requested_native.size()),
            entry_native.c_str(), static_cast<int>(entry_native.size()), TRUE) == CSTR_EQUAL) {
        return true;
    }
    if (!policy.thumbnail_cache_directory.empty() &&
        IsThumbnailCacheRequestAllowed(policy.thumbnail_cache_directory, raw_requested_path, requested_file)) {
        return true;
    }
    if (!policy.asset_access_enabled) {
        return false;
    }
    return IsPathWithin(policy.root_directory, requested_file);
}

bool EnvFlagEnabled(const char* name) noexcept
{
    char value[16]{};
    const auto len = GetEnvironmentVariableA(name, value, sizeof(value));
    if (len == 0 || len >= sizeof(value)) {
        return false;
    }
    return std::strcmp(value, "1") == 0 ||
        _stricmp(value, "true") == 0 ||
        _stricmp(value, "yes") == 0;
}

HWND FindCurrentProcessMainWindow() noexcept
{
    const auto pid = GetCurrentProcessId();
    const auto foreground = GetForegroundWindow();
    if (foreground != nullptr) {
        DWORD fg_pid{};
        GetWindowThreadProcessId(foreground, &fg_pid);
        if (fg_pid == pid && IsWindowVisible(foreground)) {
            return foreground;
        }
    }
    struct EnumState {
        DWORD pid{};
        HWND best{};
    } enum_state{ pid, nullptr };
    EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
        auto* s = reinterpret_cast<EnumState*>(lparam);
        DWORD win_pid{};
        GetWindowThreadProcessId(hwnd, &win_pid);
        if (win_pid != s->pid || !IsWindowVisible(hwnd)) {
            return TRUE;
        }
        if (GetWindow(hwnd, GW_OWNER) == nullptr) {
            s->best = hwnd;
            return FALSE;
        }
        if (s->best == nullptr) {
            s->best = hwnd;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&enum_state));
    return enum_state.best;
}

std::int64_t SteadyNowMs() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::int64_t SteadyNowUs() noexcept
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::uint32_t CurrentDisplayRefreshRate() noexcept
{
    const auto refresh_rate_for = [](const wchar_t* device_name) -> std::uint32_t {
        DEVMODEW display_mode{};
        display_mode.dmSize = sizeof(display_mode);
        if (!EnumDisplaySettingsW(device_name, ENUM_CURRENT_SETTINGS, &display_mode) ||
            display_mode.dmDisplayFrequency <= 1) {
            return 0;
        }
        return std::clamp(static_cast<std::uint32_t>(display_mode.dmDisplayFrequency),
            1U, kMaxCefWindowlessFrameRate);
    };

    const auto window = FindCurrentProcessMainWindow();
    if (window != nullptr) {
        const auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        if (monitor != nullptr && GetMonitorInfoW(
                monitor, reinterpret_cast<LPMONITORINFO>(&monitor_info))) {
            if (const auto refresh = refresh_rate_for(monitor_info.szDevice); refresh != 0) {
                return refresh;
            }
        }
    }
    if (const auto refresh = refresh_rate_for(nullptr); refresh != 0) {
        return refresh;
    }
    return 60;
}

int CefWindowlessFrameRate(std::uint32_t fps) noexcept
{
    // Automatic rate follows Skyrim's monitor clamped to kMaxCefWindowlessFrameRate,
    // with SKYULTRA_CEF_FORCE_120=1 preserved for A/B pacing comparison.
    if (fps == 0) {
        if (EnvFlagEnabled("SKYULTRA_CEF_FORCE_120")) {
            return static_cast<int>(kMaxCefWindowlessFrameRate);
        }
        return static_cast<int>(std::clamp(CurrentDisplayRefreshRate(), 1U, kMaxCefWindowlessFrameRate));
    }
    return static_cast<int>(std::clamp(fps, 1U, kMaxCefWindowlessFrameRate));
}

struct ProtectedD3DContext {
    ComPtr<ID3D11Multithread> multithread;
    BOOL previously_protected{};
};

struct BrowserState;
struct RuntimeState {
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic_bool initialized{};
    std::atomic_bool stopping{};
    std::atomic_bool begin_frame_dispatch_queued{};
    std::atomic_uint32_t visible_browser_count{ 0 };
    std::atomic<std::int64_t> last_begin_frame_dispatch_us{ 0 };
    std::atomic_bool world_paused{ true };
    std::atomic<std::uint32_t> live_host_fps{ 60U };
    std::atomic<std::int64_t> last_pump_frame_us{ 0 };
    std::atomic<std::uint32_t> ema_host_frame_us{ 16667U };
    std::atomic<std::int64_t> last_live_rate_sync_ms{ 0 };
    std::atomic_bool ui_thread_priority_boosted{ false };
    std::vector<std::shared_ptr<BrowserState>> ui_tick_browsers;  // CEF UI thread only.
    std::size_t pending_creates{};
    std::size_t open_browsers{};
    std::vector<std::weak_ptr<BrowserState>> browsers;
    std::weak_ptr<BrowserState> active_hidden_prime;                 // CEF UI thread only.
    std::deque<std::weak_ptr<BrowserState>> hidden_prime_queue;      // CEF UI thread only.
    std::chrono::steady_clock::time_point active_hidden_prime_started{};  // CEF UI thread only.
    std::chrono::steady_clock::time_point hidden_prime_defer_until{};     // CEF UI thread only.
    bool hidden_prime_timer_pending{};                                    // CEF UI thread only.
    PageBridgeMessageCallback page_bridge_callback;
    std::mutex d3d_mutex;
    std::atomic<void*> cached_device_ptr{};
    std::atomic<void*> cached_context_ptr{};
    std::atomic_bool shared_texture_ready_atomic{ false };
    std::atomic_bool use_helper_device_atomic{ false };
    ComPtr<ID3D11Device> d3d_device;
    ComPtr<ID3D11Device1> d3d_device1;
    ComPtr<ID3D11DeviceContext> d3d_context;
    ComPtr<ID3D11Device> helper_d3d_device;
    ComPtr<ID3D11Device1> helper_d3d_device1;
    ComPtr<ID3D11DeviceContext> helper_d3d_context;
    bool use_helper_device{};
    std::vector<ProtectedD3DContext> protected_contexts;
    std::uint64_t d3d_generation{};
    bool shared_texture_ready{};
    bool gpu_rendering{ true };
    bool crash_reports_enabled{};
    std::atomic_bool gpu_suspended{};
    bool logged_shared_texture_ready{};
    bool logged_shared_texture_unavailable{};
    std::thread::id application_thread;
    std::function<void(std::string_view)> log_info;
};

int CefModifiers(std::uint32_t modifiers)
{
    int result{};
    if (modifiers & kModifierShift) result |= EVENTFLAG_SHIFT_DOWN;
    if (modifiers & kModifierControl) result |= EVENTFLAG_CONTROL_DOWN;
    if (modifiers & kModifierAlt) result |= EVENTFLAG_ALT_DOWN;
    if (modifiers & kModifierMeta) result |= EVENTFLAG_COMMAND_DOWN;
    return result;
}

int CefNativeKeyCode(const KeyInput& input) noexcept
{
    const auto mapped_scan = MapVirtualKeyW(input.virtual_key, MAPVK_VK_TO_VSC_EX);
    const auto scan_code = mapped_scan != 0 ? mapped_scan & 0xFFU : input.scan_code & 0xFFU;
    const bool extended = (mapped_scan & 0xFF00U) != 0;

    // CefKeyEvent.native_key_code on Windows is the native message LPARAM,
    // which includes the scan code and key transition state. Skyrim exposes a
    // DirectInput scan-code ID, so translate it instead of passing that ID raw.
    std::uint32_t native_key_code = 1U | (scan_code << 16U);
    if (extended) native_key_code |= 1U << 24U;
    if (input.system_key) native_key_code |= 1U << 29U;
    if (!input.pressed || input.repeat) native_key_code |= 1U << 30U;
    if (!input.pressed) native_key_code |= 1U << 31U;
    return static_cast<int>(native_key_code);
}

struct CefGpuFrame {
    ComPtr<ID3D11Texture2D> cef_texture;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> shader_resource_view;
    ComPtr<ID3D11Query> copy_done_query;
    std::atomic_bool copy_pending{ false };
    std::atomic_bool display_age_recorded{ false };
    std::chrono::steady_clock::time_point published_at{};
    std::uint64_t paint_sequence{};
    SizeU size{};
    std::uint64_t device_generation{};
    std::vector<CefRect> pending_dirty_rects;  // CEF UI thread only.
    bool initialized{};
};

bool CreateSharedFramePair(
    ID3D11Device1* helper_device1,
    ID3D11Device1* game_device1,
    const D3D11_TEXTURE2D_DESC& template_desc,
    ComPtr<ID3D11Texture2D>& out_helper_tex,
    ComPtr<ID3D11Texture2D>& out_game_tex,
    HRESULT& out_hr) noexcept
{
    out_helper_tex.Reset();
    out_game_tex.Reset();
    out_hr = E_FAIL;
    if (helper_device1 == nullptr || game_device1 == nullptr) {
        return false;
    }

    auto desc = template_desc;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    out_hr = helper_device1->CreateTexture2D(&desc, nullptr, out_helper_tex.GetAddressOf());
    if (SUCCEEDED(out_hr) && out_helper_tex != nullptr) {
        ComPtr<IDXGIResource1> dxgi_resource1;
        out_hr = out_helper_tex.As(&dxgi_resource1);
        if (SUCCEEDED(out_hr) && dxgi_resource1 != nullptr) {
            HANDLE shared_handle = nullptr;
            out_hr = dxgi_resource1->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &shared_handle);
            if (SUCCEEDED(out_hr) && shared_handle != nullptr) {
                out_hr = game_device1->OpenSharedResource1(
                    shared_handle, IID_PPV_ARGS(out_game_tex.GetAddressOf()));
                CloseHandle(shared_handle);
                if (SUCCEEDED(out_hr) && out_game_tex != nullptr) {
                    return true;
                }
            }
        }
    }

    out_helper_tex.Reset();
    out_game_tex.Reset();
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    HRESULT legacy_hr = helper_device1->CreateTexture2D(&desc, nullptr, out_helper_tex.GetAddressOf());
    if (SUCCEEDED(legacy_hr) && out_helper_tex != nullptr) {
        ComPtr<IDXGIResource> dxgi_resource;
        legacy_hr = out_helper_tex.As(&dxgi_resource);
        if (SUCCEEDED(legacy_hr) && dxgi_resource != nullptr) {
            HANDLE legacy_handle = nullptr;
            legacy_hr = dxgi_resource->GetSharedHandle(&legacy_handle);
            if (SUCCEEDED(legacy_hr) && legacy_handle != nullptr) {
                legacy_hr = game_device1->OpenSharedResource(
                    legacy_handle, IID_PPV_ARGS(out_game_tex.GetAddressOf()));
                if (SUCCEEDED(legacy_hr) && out_game_tex != nullptr) {
                    out_hr = S_OK;
                    return true;
                }
            }
        }
    }
    if (FAILED(legacy_hr)) {
        out_hr = legacy_hr;
    }
    out_helper_tex.Reset();
    out_game_tex.Reset();
    return false;
}

bool CreateCefHelperD3D11Device(
    ID3D11Device* game_device,
    ID3D11Device1* game_device1,
    ComPtr<ID3D11Device>& out_device,
    ComPtr<ID3D11Device1>& out_device1,
    ComPtr<ID3D11DeviceContext>& out_context) noexcept
{
    out_device.Reset();
    out_device1.Reset();
    out_context.Reset();
    if (game_device == nullptr || game_device1 == nullptr) {
        return false;
    }

    ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(game_device->QueryInterface(IID_PPV_ARGS(dxgi_device.GetAddressOf()))) ||
        dxgi_device == nullptr) {
        return false;
    }
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(adapter.GetAddressOf())) || adapter == nullptr) {
        return false;
    }

    HMODULE d3d11_module = GetModuleHandleW(L"d3d11.dll");
    if (d3d11_module == nullptr) {
        d3d11_module = LoadLibraryW(L"d3d11.dll");
    }
    if (d3d11_module == nullptr) {
        return false;
    }
    const auto create_device = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
        GetProcAddress(d3d11_module, "D3D11CreateDevice"));
    if (create_device == nullptr) {
        return false;
    }

    constexpr std::array<D3D_FEATURE_LEVEL, 2> kLevels11_1{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    constexpr std::array<D3D_FEATURE_LEVEL, 1> kLevels11_0{
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL achieved_level{};
    HRESULT hr = create_device(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        kLevels11_1.data(),
        static_cast<UINT>(kLevels11_1.size()),
        D3D11_SDK_VERSION,
        out_device.GetAddressOf(),
        &achieved_level,
        out_context.GetAddressOf());
    if (hr == E_INVALIDARG) {
        out_device.Reset();
        out_context.Reset();
        hr = create_device(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            kLevels11_0.data(),
            static_cast<UINT>(kLevels11_0.size()),
            D3D11_SDK_VERSION,
            out_device.GetAddressOf(),
            &achieved_level,
            out_context.GetAddressOf());
    }
    if (FAILED(hr) || out_device == nullptr || out_context == nullptr) {
        out_device.Reset();
        out_context.Reset();
        return false;
    }
    if (FAILED(out_device->QueryInterface(IID_PPV_ARGS(out_device1.GetAddressOf()))) ||
        out_device1 == nullptr) {
        out_device1.Reset();
        out_device.Reset();
        out_context.Reset();
        return false;
    }

    ComPtr<IDXGIDevice> helper_dxgi_device;
    if (SUCCEEDED(out_device->QueryInterface(IID_PPV_ARGS(helper_dxgi_device.GetAddressOf()))) &&
        helper_dxgi_device != nullptr) {
        if (FAILED(helper_dxgi_device->SetGPUThreadPriority(7))) {
            if (FAILED(helper_dxgi_device->SetGPUThreadPriority(2))) {
                (void)helper_dxgi_device->SetGPUThreadPriority(1);
            }
        }
    }

    D3D11_TEXTURE2D_DESC probe_desc{};
    probe_desc.Width = 4;
    probe_desc.Height = 4;
    probe_desc.MipLevels = 1;
    probe_desc.ArraySize = 1;
    probe_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    probe_desc.SampleDesc.Count = 1;
    ComPtr<ID3D11Texture2D> probe_helper_tex;
    ComPtr<ID3D11Texture2D> probe_game_tex;
    HRESULT probe_hr = E_FAIL;
    if (!CreateSharedFramePair(
            out_device1.Get(), game_device1, probe_desc,
            probe_helper_tex, probe_game_tex, probe_hr)) {
        out_device1.Reset();
        out_device.Reset();
        out_context.Reset();
        return false;
    }
    return true;
}

class LambdaTask final : public CefTask {
public:
    explicit LambdaTask(std::function<void()> callback)
        : callback_(std::move(callback))
    {
    }

    void Execute() override
    {
        if (callback_) {
            callback_();
        }
    }

private:
    std::function<void()> callback_;
    IMPLEMENT_REFCOUNTING(LambdaTask);
};

bool PostToCefUI(std::function<void()> callback)
{
    return CefPostTask(TID_UI, new LambdaTask(std::move(callback)));
}

// Optional hooks exported by SkyNet. Other browser views keep their own request policy.
struct SkyNetWebHooks {
    using BlockRequest = BOOL (WINAPI*)(const char*);
    using DocumentScript = const char* (WINAPI*)(const char*);
    using SoftwareDecode = BOOL (WINAPI*)();
    BlockRequest block{};
    DocumentScript script{};
    SoftwareDecode software_decode{};
};

SkyNetWebHooks GetSkyNetWebHooks()
{
    static std::atomic<bool> resolved{ false };
    static std::mutex resolve_mutex;
    static SkyNetWebHooks cached{};
    if (resolved.load(std::memory_order_acquire)) {
        return cached;
    }
    const std::scoped_lock lock(resolve_mutex);
    if (resolved.load(std::memory_order_relaxed)) {
        return cached;
    }
    if (const auto module = GetModuleHandleW(L"SkyNetBrowser.dll")) {
        cached.block = reinterpret_cast<SkyNetWebHooks::BlockRequest>(GetProcAddress(module, "SkyNet_BlockWebRequest"));
        cached.script = reinterpret_cast<SkyNetWebHooks::DocumentScript>(GetProcAddress(module, "SkyNet_GetDocumentShield"));
        cached.software_decode = reinterpret_cast<SkyNetWebHooks::SoftwareDecode>(GetProcAddress(module, "SkyNet_UseSoftwareVideoDecode"));
        resolved.store(true, std::memory_order_release);
    }
    return cached;
}

class CefAppHost final : public CefApp, public CefBrowserProcessHandler {
public:
    CefAppHost(bool gpu, bool software_decode) : gpu_(gpu), software_decode_(software_decode) {}
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    void OnBeforeCommandLineProcessing(
        const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override
    {
        if (process_type.empty() && command_line) {
            // Vite production pages load sibling JS/CSS files from file:// URLs.
            // The per-view request handler still confines those reads to the
            // registered mod root.
            command_line->AppendSwitch("allow-file-access-from-files");
            command_line->AppendSwitch("disable-background-networking");
            command_line->AppendSwitch("disable-component-update");
            command_line->AppendSwitch("no-pings");
            if (EnvFlagEnabled("SKYULTRA_CEF_DISABLE_GPU_VSYNC")) {
                command_line->AppendSwitch("disable-gpu-vsync");
            }
            if (EnvFlagEnabled("SKYULTRA_CEF_DISABLE_FRAME_RATE_LIMIT")) {
                command_line->AppendSwitch("disable-frame-rate-limit");
            }
            command_line->AppendSwitch("do-not-de-elevate");
            command_line->AppendSwitch("disable-extensions");
            command_line->AppendSwitch("disable-sync");
            command_line->AppendSwitch("disable-default-apps");
            command_line->AppendSwitch("disable-spell-checking");
            command_line->AppendSwitch("disable-domain-reliability");
            command_line->AppendSwitch("disable-client-side-phishing-detection");
            command_line->AppendSwitch("metrics-recording-only");
            command_line->AppendSwitch("no-first-run");
            command_line->AppendSwitch("no-default-browser-check");
            if (!gpu_) {
                command_line->AppendSwitch("disable-gpu");
                command_line->AppendSwitch("disable-gpu-compositing");
            }
            if (software_decode_) command_line->AppendSwitch("disable-accelerated-video-decode");
            command_line->AppendSwitchWithValue(
                "disable-features",
                "Translate,OptimizationHints,MediaRouter,CalculateNativeWinOcclusion,AutoDeElevate,"
                "AutofillServerCommunication,SafeBrowsingEnhancedProtection,InterestFeedContentSuggestions,"
                "UseEcoQoSForBackgroundProcess,IntensiveWakeUpThrottling");
            // Honor Chromium background scheduling; visibility is controlled via WasHidden.
            // Keep Chromium's driver/feature selection. Forcing rasterization,
            // zero-copy and hardware overlays bypasses its compatibility choices
            // and adds GPU pressure alongside Skyrim on older display drivers.
        }
    }

    void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line) override
    {
        (void)skyultra::EnsureLiveCompassSharedMemory();
        if (command_line) {
            command_line->AppendSwitchWithValue(
                skyultra::kLiveCompassPidSwitch,
                std::to_string(GetCurrentProcessId()));
        }
    }

    bool OnAlreadyRunningAppRelaunch(
        CefRefPtr<CefCommandLine>, const CefString&) override
    {
        return true;
    }

private:
    const bool gpu_;
    const bool software_decode_;
    IMPLEMENT_REFCOUNTING(CefAppHost);
};

using CefInputCommand = std::variant<InputMode, MouseInput, KeyInput, TextInput>;

struct QueuedCefInputCommand {
    CefInputCommand command;
    std::chrono::steady_clock::time_point queued_at;
};

struct BrowserState {
    BrowserState(std::shared_ptr<RuntimeState> runtime_state, ViewId view_id, ViewConfig view_config) :
        runtime(std::move(runtime_state)), id(view_id), config(std::move(view_config)),
        visible_atomic(config.visible), max_fps_atomic(config.max_fps) {
        if (!config.bridge_event_queue) config.bridge_event_queue = std::make_shared<BridgeEventQueue>();
        if (!config.bridge_response_queue) config.bridge_response_queue = std::make_shared<BridgeEventQueue>(false);
    }

    std::shared_ptr<RuntimeState> runtime;
    std::atomic<ViewId> id{};
    [[nodiscard]] ViewId Id() const noexcept { return id.load(std::memory_order_relaxed); }
    mutable std::mutex mutex;
    ViewConfig config;
    CefRefPtr<CefBrowser> browser;  // Used only from CEF's UI thread.
    std::atomic_bool focus_on_editable_field{};
    std::atomic_bool logged_editable_focus{};
    std::deque<QueuedCefInputCommand> pending_input;
    bool input_drain_scheduled{};
    std::chrono::steady_clock::time_point last_slow_input_log{};  // CEF UI thread only.
    std::chrono::steady_clock::time_point last_slow_paint_log{};  // CEF UI thread only.
    std::atomic_uint64_t bridge_consumer{}, response_consumer{};
    std::atomic_bool bridge_drain_scheduled{};
    std::shared_ptr<BridgeEventQueue::Batch> bridge_in_flight; // CEF UI thread
    std::string bridge_ack;
    std::uint64_t bridge_sequence{};
    std::chrono::steady_clock::time_point bridge_started{}, last_slow_bridge_log{};
    std::shared_ptr<BridgeEventQueue::Batch> response_in_flight;
    std::string response_ack;
    std::chrono::steady_clock::time_point response_started{};
    bool bridge_watchdog_scheduled{}; // CEF UI thread only; one timer per view.
    std::string warned_bridge_ack, warned_response_ack;
    PageBridgeMessageCallback page_bridge_callback;
    std::vector<std::uint8_t> shadow;
    SizeU shadow_size{};
    std::vector<CefRect> pending_software_dirty_rects;
    bool has_pending_software_frame{ false };
    bool software_full_upload_needed{ true };
    bool shadow_valid{ false };
    std::shared_ptr<CefGpuFrame> gpu_frame;
    std::array<std::shared_ptr<CefGpuFrame>, kCefGpuFramePoolSize> gpu_frame_pool{};
    std::size_t next_gpu_pool_slot{};
    std::atomic_bool dirty_history_reset{ true };
    std::atomic_uint64_t paint_sequence{0};
    std::atomic_bool visible_atomic{};
    std::atomic<std::uint32_t> max_fps_atomic{};
    std::atomic_bool display_occluded{};
    std::atomic_bool counted_visible{ false };
    int applied_windowless_fps{ 0 };  // CEF UI thread only.
    std::mutex visibility_count_mutex;
    std::atomic_bool awaiting_shell_ready{};
    std::atomic<std::int64_t> awaiting_shell_ready_since_ms{};
    bool v8_bridge_ready{};
    std::atomic_bool logged_accelerated_paint{};
    bool logged_accelerated_frame{};
    bool logged_accelerated_failure{};
    std::atomic_bool logged_policy_block{};
    std::atomic_bool logged_software_frame{};
    std::uint32_t console_errors_logged{};  // CEF UI thread only.
    bool app_shell_ready{};        // CEF UI thread only.
    std::chrono::steady_clock::time_point first_prime_paint_at{};
    std::chrono::steady_clock::time_point last_begin_frame_at{};  // CEF UI thread only.
    std::chrono::steady_clock::time_point startup_prime_started{};  // CEF UI thread only.
    bool prime_hidden_frame{};      // Used only from CEF's UI thread.
    bool pending_dormant_hide{};    // Used only from CEF's UI thread.
    std::uint64_t dormant_hide_generation{}; // Used only from CEF's UI thread.
    bool prime_startup_frame{};     // Used only from CEF's UI thread.
    bool queued_for_hidden_prime{}; // Used only from CEF's UI thread.
    bool hidden_prime_timer_scheduled{};  // Used only from CEF's UI thread.
    std::uint64_t next_frame_sequence{ 1 };
    int held_mouse_modifiers{};     // Used only from CEF's UI thread.
    bool close_requested{};         // Used only from CEF's UI thread.
    bool main_frame_ready{};        // Used only from CEF's UI thread.
    bool spare_browser{};           // Used only from CEF's UI thread.
    bool renderer_ready{};          // Used only from CEF's UI thread.
    bool browser_create_dispatched{};  // Protected by mutex.
    std::string pending_create_url;    // Protected by mutex.
    std::string adopted_initial_url;   // Used only from CEF's UI thread.
    BrowserHealth health; // Protected by mutex.
    CefRecoveryPolicy recovery_policy; // CEF UI thread only.
    std::uint64_t recovery_generation{};
    std::string recovery_url;
    std::chrono::steady_clock::time_point last_context_loss{};
    std::atomic_bool creation_failed{};
    std::atomic_bool closed{};
    std::chrono::steady_clock::time_point requested_at{ std::chrono::steady_clock::now() };
};

int EffectiveVisibleWindowlessFrameRate(
    const RuntimeState& runtime, const ViewConfig& config) noexcept
{
    const int configured = CefWindowlessFrameRate(config.max_fps);
    if (runtime.world_paused.load(std::memory_order_acquire) || config.layer == ViewLayer::Hud) {
        return configured;
    }
    const auto host_fps = std::clamp(
        runtime.live_host_fps.load(std::memory_order_relaxed), 30U, 60U);
    return std::min(configured, static_cast<int>(host_fps));
}

void ApplyWindowlessFrameRateUI(
    const std::shared_ptr<BrowserState>& state, bool visible,
    const ViewConfig& config, bool force = false) noexcept
{
    CEF_REQUIRE_UI_THREAD();
    if (!state || !state->runtime || !state->browser || state->close_requested) {
        return;
    }
    if (!state->runtime->ui_thread_priority_boosted.exchange(true, std::memory_order_relaxed)) {
        (void)::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    }
    const int target_fps = visible
        ? EffectiveVisibleWindowlessFrameRate(*state->runtime, config)
        : std::min(15, CefWindowlessFrameRate(config.max_fps));
    if (force || state->applied_windowless_fps != target_fps) {
        state->browser->GetHost()->SetWindowlessFrameRate(target_fps);
        state->applied_windowless_fps = target_fps;
    }
}

void SyncLiveWindowlessFrameRates(const std::shared_ptr<RuntimeState>& runtime, bool force)
{
    if (!runtime || !runtime->initialized.load(std::memory_order_acquire) ||
        runtime->stopping.load(std::memory_order_acquire)) {
        return;
    }
    (void)PostToCefUI([runtime, force] {
        if (runtime->stopping.load(std::memory_order_acquire) ||
            runtime->gpu_suspended.load(std::memory_order_acquire)) {
            return;
        }
        std::vector<std::shared_ptr<BrowserState>> browsers;
        {
            const std::scoped_lock lock(runtime->mutex);
            browsers.reserve(runtime->browsers.size());
            for (const auto& weak : runtime->browsers) {
                if (auto view = weak.lock()) {
                    browsers.push_back(std::move(view));
                }
            }
        }
        for (const auto& view : browsers) {
            if (!view->browser || view->close_requested ||
                view->closed.load(std::memory_order_acquire) ||
                view->display_occluded.load(std::memory_order_acquire)) {
                continue;
            }
            ViewConfig cfg;
            bool visible{};
            {
                const std::scoped_lock lock(view->mutex);
                cfg = view->config;
                visible = view->config.visible;
            }
            if (!visible || cfg.layer == ViewLayer::Hud) {
                continue;
            }
            const int desired = EffectiveVisibleWindowlessFrameRate(*runtime, cfg);
            const int delta = desired >= view->applied_windowless_fps
                ? (desired - view->applied_windowless_fps)
                : (view->applied_windowless_fps - desired);
            if (force || view->applied_windowless_fps == 0 || delta >= 4) {
                view->browser->GetHost()->SetWindowlessFrameRate(desired);
                view->applied_windowless_fps = desired;
            }
        }
    });
}

void ClearCefDocumentState(const std::shared_ptr<BrowserState>& state)
{
    CEF_REQUIRE_UI_THREAD();
    ++state->recovery_generation;
    state->bridge_in_flight.reset(); state->bridge_ack.clear();
    state->response_in_flight.reset(); state->response_ack.clear();
    state->main_frame_ready = false;
    state->v8_bridge_ready = false;
    state->dirty_history_reset.store(true, std::memory_order_release);
    state->pending_dormant_hide = false;
    ++state->dormant_hide_generation;
    state->awaiting_shell_ready.store(false, std::memory_order_relaxed);
    state->awaiting_shell_ready_since_ms.store(0, std::memory_order_relaxed);
    const std::scoped_lock lock(state->mutex);
    state->pending_input.clear(); state->held_mouse_modifiers = 0;
    state->gpu_frame.reset(); state->gpu_frame_pool = {}; state->next_gpu_pool_slot = 0;
    state->pending_software_dirty_rects.clear();
    state->has_pending_software_frame = false;
    state->software_full_upload_needed = true;
    state->shadow_valid = false;
    state->shadow.clear(); state->shadow_size = {};
}

void SyncBrowserVisibleCount(const std::shared_ptr<BrowserState>& state) noexcept
{
    if (!state || !state->runtime) {
        return;
    }
    const std::scoped_lock lock(state->visibility_count_mutex);
    const bool should_count =
        !state->closed.load(std::memory_order_acquire) &&
        !state->runtime->stopping.load(std::memory_order_acquire) &&
        !state->runtime->gpu_suspended.load(std::memory_order_acquire) &&
        state->visible_atomic.load(std::memory_order_acquire) &&
        !state->display_occluded.load(std::memory_order_acquire);
    const bool previous = state->counted_visible.exchange(should_count, std::memory_order_acq_rel);
    if (previous != should_count) {
        if (should_count) {
            state->runtime->visible_browser_count.fetch_add(1U, std::memory_order_release);
        } else {
            auto current = state->runtime->visible_browser_count.load(std::memory_order_acquire);
            while (current > 0U && !state->runtime->visible_browser_count.compare_exchange_weak(
                current, current - 1U, std::memory_order_acq_rel, std::memory_order_acquire)) {}
        }
    }
}

// Rely on CEF's internal BeginFrame scheduling (external_begin_frame_enabled = FALSE)
// so rapid route/tab paints never queue overlapping IssueExternalBeginFrame IPCs in Viz.
[[maybe_unused]] void ScheduleStartupPrimeBeginFrames(const std::shared_ptr<BrowserState>&)
{
    CEF_REQUIRE_UI_THREAD();
}

[[maybe_unused]] void TickVisibleBeginFrames(const std::shared_ptr<RuntimeState>&)
{
}

void EnqueueHiddenPrimeLockedUI(const std::shared_ptr<BrowserState>& state)
{
    CEF_REQUIRE_UI_THREAD();
    if (!state || !state->runtime || state->queued_for_hidden_prime ||
        state->close_requested || state->closed.load(std::memory_order_acquire)) {
        return;
    }
    state->queued_for_hidden_prime = true;
    const bool is_menu = state->config.id == "SkyUltra.Menu";
    if (is_menu) {
        state->runtime->hidden_prime_queue.emplace_front(state);
    } else {
        state->runtime->hidden_prime_queue.emplace_back(state);
    }
}

void PumpHiddenPrimeQueue(const std::shared_ptr<RuntimeState>& runtime);

void InterruptActiveHiddenPrime(
    const std::shared_ptr<RuntimeState>& runtime,
    std::chrono::milliseconds defer_for = std::chrono::milliseconds(500))
{
    CEF_REQUIRE_UI_THREAD();
    if (!runtime || runtime->stopping.load(std::memory_order_acquire)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto target_defer = now + defer_for;
    if (target_defer > runtime->hidden_prime_defer_until) {
        runtime->hidden_prime_defer_until = target_defer;
    }
    if (const auto active = runtime->active_hidden_prime.lock()) {
        runtime->active_hidden_prime.reset();
        if (!active->close_requested && !active->closed.load(std::memory_order_acquire) &&
            active->prime_hidden_frame &&
            !active->visible_atomic.load(std::memory_order_acquire)) {
            if (active->browser) {
                active->browser->GetHost()->WasHidden(true);
            }
            EnqueueHiddenPrimeLockedUI(active);
        }
    }
}

void ScheduleHiddenPrimePump(
    const std::shared_ptr<RuntimeState>& runtime,
    std::chrono::milliseconds delay)
{
    CEF_REQUIRE_UI_THREAD();
    if (!runtime || runtime->stopping.load(std::memory_order_acquire) ||
        runtime->hidden_prime_timer_pending) {
        return;
    }
    runtime->hidden_prime_timer_pending = true;
    const auto clamped_ms = std::max<std::int64_t>(16, delay.count());
    if (!CefPostDelayedTask(TID_UI, new LambdaTask([weak = std::weak_ptr<RuntimeState>(runtime)] {
        if (const auto rt = weak.lock()) {
            rt->hidden_prime_timer_pending = false;
            PumpHiddenPrimeQueue(rt);
        }
    }), clamped_ms)) {
        runtime->hidden_prime_timer_pending = false;
    }
}

void PumpHiddenPrimeQueue(const std::shared_ptr<RuntimeState>& runtime)
{
    CEF_REQUIRE_UI_THREAD();
    if (!runtime || runtime->stopping.load(std::memory_order_acquire) ||
        runtime->gpu_suspended.load(std::memory_order_acquire)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (const auto active = runtime->active_hidden_prime.lock()) {
        const bool still_valid =
            !active->close_requested && !active->closed.load(std::memory_order_acquire) &&
            active->browser && active->prime_hidden_frame &&
            !active->visible_atomic.load(std::memory_order_acquire);
        if (still_valid && now - runtime->active_hidden_prime_started < std::chrono::seconds(4)) {
            ScheduleHiddenPrimePump(runtime, std::chrono::milliseconds(500));
            return;
        }
        if (active->browser && !active->visible_atomic.load(std::memory_order_acquire) &&
            !active->close_requested) {
            active->prime_hidden_frame = false;
            active->browser->GetHost()->WasHidden(true);
        }
        runtime->active_hidden_prime.reset();
    }

    if (now < runtime->hidden_prime_defer_until) {
        const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
            runtime->hidden_prime_defer_until - now);
        ScheduleHiddenPrimePump(runtime, wait);
        return;
    }

    // Do not steal GPU/compositor budget for speculative hidden warm-ups while an interactive
    // SkyUltra.Menu view is open on screen.
    bool interactive_menu_open{};
    {
        const std::scoped_lock lock(runtime->mutex);
        for (const auto& weak : runtime->browsers) {
            if (const auto view = weak.lock()) {
                if (!view->closed.load(std::memory_order_acquire) &&
                    view->visible_atomic.load(std::memory_order_acquire) &&
                    !view->display_occluded.load(std::memory_order_acquire) &&
                    view->config.id == "SkyUltra.Menu") {
                    interactive_menu_open = true;
                    break;
                }
            }
        }
    }
    if (interactive_menu_open) {
        return;
    }

    while (!runtime->hidden_prime_queue.empty()) {
        const auto candidate = runtime->hidden_prime_queue.front().lock();
        runtime->hidden_prime_queue.pop_front();
        if (!candidate) {
            continue;
        }
        candidate->queued_for_hidden_prime = false;
        if (candidate->close_requested || candidate->closed.load(std::memory_order_acquire) ||
            !candidate->browser || !candidate->prime_hidden_frame ||
            candidate->visible_atomic.load(std::memory_order_acquire)) {
            continue;
        }
        std::uint32_t max_fps{};
        {
            const std::scoped_lock lock(candidate->mutex);
            max_fps = candidate->config.max_fps;
        }
        runtime->active_hidden_prime = candidate;
        runtime->active_hidden_prime_started = now;
        auto host = candidate->browser->GetHost();
        host->SetWindowlessFrameRate(std::min(15, CefWindowlessFrameRate(max_fps)));
        host->WasHidden(false);
        host->WasResized();
        host->Invalidate(PET_VIEW);
        ScheduleHiddenPrimePump(runtime, std::chrono::milliseconds(1500));
        return;
    }
}

void AccumulateDirtyRects(std::vector<CefRect>& dst, const std::vector<CefRect>& src, SizeU size)
{
    if (src.empty() || size.width == 0 || size.height == 0) {
        return;
    }
    const int max_w = static_cast<int>(size.width);
    const int max_h = static_cast<int>(size.height);
    for (const auto& rect : src) {
        const int left = std::clamp(rect.x, 0, max_w);
        const int top = std::clamp(rect.y, 0, max_h);
        const int right = static_cast<int>(std::clamp<std::int64_t>(
            static_cast<std::int64_t>(rect.x) + rect.width, left, static_cast<std::int64_t>(max_w)));
        const int bottom = static_cast<int>(std::clamp<std::int64_t>(
            static_cast<std::int64_t>(rect.y) + rect.height, top, static_cast<std::int64_t>(max_h)));
        if (right <= left || bottom <= top) {
            continue;
        }
        CefRect clamped;
        clamped.x = left;
        clamped.y = top;
        clamped.width = right - left;
        clamped.height = bottom - top;
        dst.push_back(clamped);
    }
    if (dst.size() <= 1U) {
        return;
    }
    constexpr int kMergePad = 16;
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t i = 0; i < dst.size(); ++i) {
            for (std::size_t j = i + 1; j < dst.size(); ) {
                const int il = dst[i].x;
                const int it = dst[i].y;
                const int ir = il + dst[i].width;
                const int ib = it + dst[i].height;
                const int jl = dst[j].x;
                const int jt = dst[j].y;
                const int jr = jl + dst[j].width;
                const int jb = jt + dst[j].height;
                if (il <= jr + kMergePad && jl <= ir + kMergePad &&
                    it <= jb + kMergePad && jt <= ib + kMergePad) {
                    const int ml = std::min(il, jl);
                    const int mt = std::min(it, jt);
                    const int mr = std::max(ir, jr);
                    const int mb = std::max(ib, jb);
                    dst[i].x = ml;
                    dst[i].y = mt;
                    dst[i].width = mr - ml;
                    dst[i].height = mb - mt;
                    dst[j] = dst.back();
                    dst.pop_back();
                    merged = true;
                } else {
                    ++j;
                }
            }
        }
    }
    if (dst.size() > 16U) {
        int bl = dst[0].x;
        int bt = dst[0].y;
        int br = bl + dst[0].width;
        int bb = bt + dst[0].height;
        for (std::size_t i = 1; i < dst.size(); ++i) {
            bl = std::min(bl, dst[i].x);
            bt = std::min(bt, dst[i].y);
            br = std::max(br, dst[i].x + dst[i].width);
            bb = std::max(bb, dst[i].y + dst[i].height);
        }
        dst.resize(1);
        dst[0].x = bl;
        dst[0].y = bt;
        dst[0].width = br - bl;
        dst[0].height = bb - bt;
    }
}

std::size_t MergeDirtyBoxes(D3D11_BOX* boxes, std::size_t count, UINT pad = 16U) noexcept
{
    if (boxes == nullptr || count <= 1U) {
        return count;
    }
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ) {
                const bool overlap_or_near =
                    boxes[i].left <= boxes[j].right + pad &&
                    boxes[j].left <= boxes[i].right + pad &&
                    boxes[i].top <= boxes[j].bottom + pad &&
                    boxes[j].top <= boxes[i].bottom + pad;
                if (overlap_or_near) {
                    boxes[i].left = std::min(boxes[i].left, boxes[j].left);
                    boxes[i].top = std::min(boxes[i].top, boxes[j].top);
                    boxes[i].right = std::max(boxes[i].right, boxes[j].right);
                    boxes[i].bottom = std::max(boxes[i].bottom, boxes[j].bottom);
                    boxes[j] = boxes[count - 1U];
                    --count;
                    merged = true;
                } else {
                    ++j;
                }
            }
        }
    }
    return count;
}

std::uint64_t ComputeUnionPixelArea(const D3D11_BOX* boxes, std::size_t count) noexcept
{
    if (boxes == nullptr || count == 0) {
        return 0;
    }
    std::array<UINT, 96> ys{};
    std::size_t y_count{};
    for (std::size_t i = 0; i < count && y_count + 2 <= ys.size(); ++i) {
        ys[y_count++] = boxes[i].top;
        ys[y_count++] = boxes[i].bottom;
    }
    std::sort(ys.begin(), ys.begin() + static_cast<std::ptrdiff_t>(y_count));
    y_count = static_cast<std::size_t>(
        std::unique(ys.begin(), ys.begin() + static_cast<std::ptrdiff_t>(y_count)) - ys.begin());

    std::uint64_t union_area{};
    std::array<std::pair<UINT, UINT>, 48> xs{};
    for (std::size_t yi = 0; yi + 1 < y_count; ++yi) {
        const UINT y0 = ys[yi];
        const UINT y1 = ys[yi + 1];
        if (y1 <= y0) {
            continue;
        }
        std::size_t x_count{};
        for (std::size_t i = 0; i < count && x_count < xs.size(); ++i) {
            if (boxes[i].top <= y0 && boxes[i].bottom >= y1 && boxes[i].right > boxes[i].left) {
                xs[x_count++] = { boxes[i].left, boxes[i].right };
            }
        }
        if (x_count == 0) {
            continue;
        }
        std::sort(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(x_count),
            [](const auto& a, const auto& b) {
                return a.first < b.first || (a.first == b.first && a.second < b.second);
            });
        UINT cur_left = xs[0].first;
        UINT cur_right = xs[0].second;
        std::uint64_t strip_width{};
        for (std::size_t i = 1; i < x_count; ++i) {
            if (xs[i].first <= cur_right) {
                cur_right = std::max(cur_right, xs[i].second);
            } else {
                strip_width += static_cast<std::uint64_t>(cur_right - cur_left);
                cur_left = xs[i].first;
                cur_right = xs[i].second;
            }
        }
        strip_width += static_cast<std::uint64_t>(cur_right - cur_left);
        union_area += strip_width * static_cast<std::uint64_t>(y1 - y0);
    }
    return union_area;
}

void ScheduleCefBridgeDrain(const std::shared_ptr<BrowserState>& state);
void ScheduleCefBridgeWatchdog(const std::shared_ptr<BrowserState>& state)
{
    CEF_REQUIRE_UI_THREAD();
    if (state->bridge_watchdog_scheduled || state->close_requested) return;
    const auto now = std::chrono::steady_clock::now();
    std::int64_t delay = 10'000;
    bool pending{};
    const auto consider = [&](const auto& flight, const auto& ack, const auto& warned, const auto& started) {
        if (!flight || ack == warned) return;
        pending = true;
        delay = std::min(delay, std::max<std::int64_t>(1, 10'000 -
            std::chrono::duration_cast<std::chrono::milliseconds>(now - started).count()));
    };
    consider(state->bridge_in_flight, state->bridge_ack, state->warned_bridge_ack, state->bridge_started);
    consider(state->response_in_flight, state->response_ack, state->warned_response_ack, state->response_started);
    if (!pending) return;
    state->bridge_watchdog_scheduled = true;
    if (!CefPostDelayedTask(TID_UI, new LambdaTask([weak = std::weak_ptr<BrowserState>(state)] {
        const auto current = weak.lock();
        if (!current) return;
        current->bridge_watchdog_scheduled = false;
        if (current->close_requested || current->closed.load() || current->runtime->stopping.load()) return;
        const auto inspect = [&](const auto& flight, const auto& ack, auto& warned, const auto& started) {
            if (!flight || ack == warned || std::chrono::steady_clock::now() - started < std::chrono::seconds(10)) return;
            warned = ack;
            { const std::scoped_lock lock(current->mutex); ++current->health.stalled_batches;
              current->health.last_failure = "Bridge acknowledgement exceeded 10 seconds."; }
            if (current->runtime->log_info) current->runtime->log_info("CEF stalled bridge batch on view " + std::to_string(current->Id()) +
                "; accepted commands were not replayed. Suspend or reload the mod in Developer Studio.");
        };
        inspect(current->bridge_in_flight, current->bridge_ack, current->warned_bridge_ack, current->bridge_started);
        inspect(current->response_in_flight, current->response_ack, current->warned_response_ack, current->response_started);
        ScheduleCefBridgeWatchdog(current);
    }), delay)) state->bridge_watchdog_scheduled = false;
}
void DrainCefBridge(const std::shared_ptr<BrowserState>& state)
{
    CEF_REQUIRE_UI_THREAD();
    state->bridge_drain_scheduled.store(false);
    if (!state->browser || state->close_requested || !state->main_frame_ready) return;
    const auto frame = state->browser->GetMainFrame();
    if (!frame) return;
    const auto submit = [&](bool responses) {
        auto& flight = responses ? state->response_in_flight : state->bridge_in_flight;
        if (flight) return;
        std::shared_ptr<BridgeEventQueue> queue;
        { const std::scoped_lock lock(state->mutex); queue = responses ? state->config.bridge_response_queue : state->config.bridge_event_queue; }
        const auto batch = queue->Take(responses ? state->response_consumer.load() : state->bridge_consumer.load(), 32);
        if (!batch) return;
        auto& ack = responses ? state->response_ack : state->bridge_ack;
        ack = std::string(responses ? "__SKYULTRA_REPLY_ACK__" : "__SKYULTRA_BATCH_ACK__") + std::to_string(state->Id()) + ":" + std::to_string(++state->bridge_sequence);
        flight = batch;
        (responses ? state->response_started : state->bridge_started) = std::chrono::steady_clock::now();
        if (state->v8_bridge_ready) {
            auto message = CefProcessMessage::Create(kBridgeBatchDeliverMessage);
            auto args = message ? message->GetArgumentList() : nullptr;
            auto list = CefListValue::Create();
            if (args && list && args->SetBool(0, responses) && args->SetString(1, ack) &&
                list->SetSize(batch->events.size() * 2)) {
                for (std::size_t i = 0; i < batch->events.size(); ++i) {
                    list->SetString(i * 2, batch->events[i].name);
                    list->SetString(i * 2 + 1, batch->events[i].payload_json);
                }
                args->SetList(2, list);
                frame->SendProcessMessage(PID_RENDERER, message);
                ScheduleCefBridgeWatchdog(state);
                return;
            }
        }
        frame->ExecuteJavaScript(BuildBridgeEventBatchScript(*batch, ack, responses), frame->GetURL(), 1);
        ScheduleCefBridgeWatchdog(state);
    };
    submit(true);
    submit(false);
}
void ScheduleCefBridgeDrain(const std::shared_ptr<BrowserState>& state)
{
    if (state->bridge_drain_scheduled.exchange(true)) return;
    try {
        if (!PostToCefUI([state] { DrainCefBridge(state); })) state->bridge_drain_scheduled.store(false);
    } catch (...) { state->bridge_drain_scheduled.store(false); throw; }
}

bool AcknowledgeCefBridgeBatch(const std::shared_ptr<BrowserState>& state, std::string_view token)
{
    CEF_REQUIRE_UI_THREAD();
    const bool reply = !state->response_ack.empty() && token == state->response_ack;
    if (!reply && (state->bridge_ack.empty() || token != state->bridge_ack)) return false;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double, std::milli>(now -
        (reply ? state->response_started : state->bridge_started)).count();
    std::shared_ptr<BridgeEventQueue> queue;
    { const std::scoped_lock lock(state->mutex);
      queue = reply ? state->config.bridge_response_queue : state->config.bridge_event_queue; }
    queue->RecordDispatch(elapsed);
    auto& flight = reply ? state->response_in_flight : state->bridge_in_flight;
    if (flight) flight->executed = true;
    if (elapsed >= 100 && now - state->last_slow_bridge_log >= std::chrono::seconds(10)) {
        state->last_slow_bridge_log = now;
        const auto stats = queue->Snapshot();
        if (state->runtime->log_info) state->runtime->log_info("CEF bridge view " + std::to_string(state->Id()) +
            ": " + std::to_string(elapsed) + " ms; pending " + std::to_string(stats.pending) +
            ", bytes " + std::to_string(stats.bytes) + ", rejected " + std::to_string(stats.rejected));
    }
    flight.reset();
    (reply ? state->response_ack : state->bridge_ack).clear();
    ScheduleCefBridgeDrain(state);
    return true;
}

std::string ToUtf8(const std::u32string& text)
{
    std::string result;
    for (auto codepoint : text) {
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            codepoint = 0xFFFD;
        }
        if (codepoint <= 0x7F) {
            result.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
    return result;
}

std::uint32_t Utf16Length(const std::u32string& text)
{
    std::uint32_t length{};
    for (const auto codepoint : text) {
        length += codepoint > 0xFFFF && codepoint <= 0x10FFFF ? 2U : 1U;
    }
    return length;
}

bool IsPureMouseMove(const CefInputCommand& command)
{
    const auto* mouse = std::get_if<MouseInput>(&command);
    return mouse != nullptr && mouse->moved &&
        mouse->button_action == MouseButtonAction::None && !mouse->wheel;
}

bool IsPureMouseWheel(const CefInputCommand& command) noexcept
{
    const auto* mouse = std::get_if<MouseInput>(&command);
    return mouse != nullptr && mouse->wheel && mouse->button_action == MouseButtonAction::None;
}

std::optional<TraceKind> CefInputTraceKind(const CefInputCommand& command) noexcept
{
    if (std::holds_alternative<InputMode>(command)) {
        return TraceKind::InputFocus;
    }
    if (const auto* mouse = std::get_if<MouseInput>(&command)) {
        if (mouse->button_action == MouseButtonAction::Down) {
            return TraceKind::InputClick;
        }
        if (mouse->wheel) {
            return TraceKind::InputWheel;
        }
        return std::nullopt;
    }
    if (const auto* key = std::get_if<KeyInput>(&command)) {
        if (key->pressed && !key->repeat) {
            return TraceKind::InputKey;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

const char* CefInputCommandName(const CefInputCommand& command) noexcept
{
    if (const auto* focus = std::get_if<InputMode>(&command)) {
        return *focus == InputMode::None ? "focus-loss" : "focus-gain";
    }
    if (const auto* mouse = std::get_if<MouseInput>(&command)) {
        if (mouse->button_action == MouseButtonAction::Down) return "mouse-down";
        if (mouse->button_action == MouseButtonAction::Up) return "mouse-up";
        if (mouse->wheel) return "mouse-wheel";
        return "mouse-move";
    }
    if (const auto* key = std::get_if<KeyInput>(&command)) {
        if (!key->pressed) return "key-up";
        return key->repeat ? "key-repeat" : "key-down";
    }
    return "text";
}

void DispatchCefInput(const std::shared_ptr<BrowserState>& state, const CefInputCommand& command)
{
    CEF_REQUIRE_UI_THREAD();
    if (!state->browser || state->close_requested) {
        return;
    }

    auto host = state->browser->GetHost();
    std::visit([&](const auto& input) {
        using T = std::decay_t<decltype(input)>;
        if constexpr (std::is_same_v<T, InputMode>) {
            state->focus_on_editable_field.store(false, std::memory_order_relaxed);
            if (input == InputMode::None) {
                state->held_mouse_modifiers = 0;
            }
            host->SetFocus(input != InputMode::None);
        } else if constexpr (std::is_same_v<T, MouseInput>) {
            const int button_flag = input.button == MouseButton::Middle ? EVENTFLAG_MIDDLE_MOUSE_BUTTON :
                input.button == MouseButton::Right ? EVENTFLAG_RIGHT_MOUSE_BUTTON : EVENTFLAG_LEFT_MOUSE_BUTTON;
            CefMouseEvent event;
            event.x = static_cast<int>(std::lround(input.position.x));
            event.y = static_cast<int>(std::lround(input.position.y));
            event.modifiers = CefModifiers(input.modifiers) | state->held_mouse_modifiers;
            if (input.moved) {
                host->SendMouseMoveEvent(event, false);
            }
            if (input.wheel) {
                host->SendMouseWheelEvent(event, input.wheel_x, input.wheel_y);
            }
            if (input.button_action == MouseButtonAction::Down) {
                state->held_mouse_modifiers |= button_flag;
                event.modifiers |= button_flag;
                host->SendMouseClickEvent(event, input.button == MouseButton::Middle ? MBT_MIDDLE :
                    input.button == MouseButton::Right ? MBT_RIGHT : MBT_LEFT, false, 1);
            } else if (input.button_action == MouseButtonAction::Up) {
                host->SendMouseClickEvent(event, input.button == MouseButton::Middle ? MBT_MIDDLE :
                    input.button == MouseButton::Right ? MBT_RIGHT : MBT_LEFT, true, 1);
                state->held_mouse_modifiers &= ~button_flag;
            }
        } else if constexpr (std::is_same_v<T, KeyInput>) {
            CefKeyEvent event;
            event.type = input.pressed ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
            event.windows_key_code = static_cast<int>(input.virtual_key);
            event.native_key_code = CefNativeKeyCode(input);
            event.modifiers = CefModifiers(input.modifiers);
            event.is_system_key = input.system_key;
            event.focus_on_editable_field =
                state->focus_on_editable_field.load(std::memory_order_relaxed) ? 1 : 0;
            host->SendKeyEvent(event);
        } else if constexpr (std::is_same_v<T, TextInput>) {
            CefString cef_text(ToUtf8(input.text));
            if (input.composition && input.committed) {
                host->ImeCommitText(cef_text, CefRange::InvalidRange(), 0);
                return;
            }
            if (input.composition) {
                const auto cursor = static_cast<int>(Utf16Length(input.text));
                host->ImeSetComposition(cef_text, {}, CefRange::InvalidRange(), CefRange(cursor, cursor));
                return;
            }
            if (!input.committed) {
                return;
            }
            for (const auto codepoint : input.text) {
                const auto focus_on_editable_field =
                    state->focus_on_editable_field.load(std::memory_order_relaxed);
                const auto send_char = [host, focus_on_editable_field](std::uint16_t character) {
                    CefKeyEvent event;
                    event.type = KEYEVENT_CHAR;
                    // On Windows CEF uses windows_key_code to carry the WM_CHAR
                    // value. Supplying only character leaves that key code at 0,
                    // so Chromium receives no text for this synthesized event.
                    event.windows_key_code = static_cast<int>(character);
                    event.character = character;
                    event.unmodified_character = character;
                    event.focus_on_editable_field = focus_on_editable_field ? 1 : 0;
                    host->SendKeyEvent(event);
                };
                if (codepoint <= 0xFFFF) {
                    send_char(static_cast<std::uint16_t>(codepoint));
                } else if (codepoint <= 0x10FFFF) {
                    const auto value = codepoint - 0x10000;
                    send_char(static_cast<std::uint16_t>(0xD800 + (value >> 10)));
                    send_char(static_cast<std::uint16_t>(0xDC00 + (value & 0x3FF)));
                }
            }
        }
    }, command);
}

void DrainCefInput(const std::shared_ptr<BrowserState>& state)
{
    CEF_REQUIRE_UI_THREAD();
    constexpr std::size_t kMaxInputCommandsPerTask = 64;
    std::deque<QueuedCefInputCommand> batch;
    double max_queue_wait_ms{};
    const char* max_queue_wait_command{ "none" };
    double max_dispatch_ms{};
    const char* max_dispatch_command{ "none" };
    {
        const std::scoped_lock lock(state->mutex);
        const auto count = std::min(kMaxInputCommandsPerTask, state->pending_input.size());
        for (std::size_t index = 0; index < count; ++index) {
            batch.emplace_back(std::move(state->pending_input.front()));
            state->pending_input.pop_front();
        }
    }

    for (const auto& queued : batch) {
        const auto dispatch_start = std::chrono::steady_clock::now();
        if (const auto trace_kind = CefInputTraceKind(queued.command)) {
            TraceMarkDispatched(state->Id(), *trace_kind);
        }
        DispatchCefInput(state, queued.command);
        const auto dispatch_end = std::chrono::steady_clock::now();
        const auto queue_wait_ms = std::chrono::duration<double, std::milli>(
            dispatch_start - queued.queued_at).count();
        const auto dispatch_ms = std::chrono::duration<double, std::milli>(
            dispatch_end - dispatch_start).count();
        if (queue_wait_ms > max_queue_wait_ms) {
            max_queue_wait_ms = queue_wait_ms;
            max_queue_wait_command = CefInputCommandName(queued.command);
        }
        if (dispatch_ms > max_dispatch_ms) {
            max_dispatch_ms = dispatch_ms;
            max_dispatch_command = CefInputCommandName(queued.command);
        }
    }

    // Avoid writing a log line for every repeated key while the CEF UI thread
    // is behind. Logging itself can add latency to the input queue.
    const auto log_time = std::chrono::steady_clock::now();
    if (state->runtime->log_info && (max_queue_wait_ms >= 100.0 || max_dispatch_ms >= 100.0) &&
        (state->last_slow_input_log.time_since_epoch().count() == 0 ||
         log_time - state->last_slow_input_log >= std::chrono::seconds(1))) {
        state->last_slow_input_log = log_time;
        state->runtime->log_info("CEF input batch for view " + std::to_string(state->id) +
            ": max queue wait " + std::to_string(static_cast<int>(max_queue_wait_ms)) +
            " ms (" + max_queue_wait_command + "), max dispatch " +
            std::to_string(static_cast<int>(max_dispatch_ms)) + " ms wall (" +
            max_dispatch_command + ").");
    }

    bool more_pending{};
    {
        const std::scoped_lock lock(state->mutex);
        more_pending = !state->pending_input.empty();
        if (!more_pending) {
            state->input_drain_scheduled = false;
        }
    }
    if (more_pending && !PostToCefUI([state] { DrainCefInput(state); })) {
        const std::scoped_lock lock(state->mutex);
        state->pending_input.clear();
        state->input_drain_scheduled = false;
    }
}

void QueueCefInput(const std::shared_ptr<BrowserState>& state, CefInputCommand command)
{
    constexpr std::size_t kMaxPendingInput = 1024;
    if (const auto trace_kind = CefInputTraceKind(command)) {
        TraceBeginInput(
            state->Id(),
            *trace_kind,
            state->visible_atomic.load(std::memory_order_acquire),
            state->paint_sequence.load(std::memory_order_acquire));
    }
    bool schedule_drain{};
    QueuedCefInputCommand queued{ std::move(command), std::chrono::steady_clock::now() };
    {
        const std::scoped_lock lock(state->mutex);
        // CEF's UI loop can fall behind Skyrim's mouse polling. Coalesce only
        // adjacent moves so clicks and keyboard events keep their original order.
        if (const auto* focus = std::get_if<InputMode>(&queued.command)) {
            // Keep only the latest focus state. Focus gain goes first so a text
            // field is ready before the click and the first character arrive.
            std::erase_if(state->pending_input, [](const QueuedCefInputCommand& pending) {
                return std::holds_alternative<InputMode>(pending.command);
            });
            if (*focus != InputMode::None) {
                std::erase_if(state->pending_input, [](const QueuedCefInputCommand& pending) {
                    return IsPureMouseMove(pending.command);
                });
                state->pending_input.emplace_front(std::move(queued));
            } else {
                state->pending_input.emplace_back(std::move(queued));
            }
        } else if (const auto* mouse = std::get_if<MouseInput>(&queued.command);
            mouse != nullptr && mouse->button_action == MouseButtonAction::Down) {
            // The click carries its own coordinates, so earlier hover positions
            // are stale and should not delay it in CEF's UI queue.
            std::erase_if(state->pending_input, [](const QueuedCefInputCommand& pending) {
                return IsPureMouseMove(pending.command);
            });
            state->pending_input.emplace_back(std::move(queued));
        } else if (std::holds_alternative<KeyInput>(queued.command) ||
            std::holds_alternative<TextInput>(queued.command)) {
            // Hover events after a click do not affect text entry. Remove them
            // before queueing the first key so it follows focus and click directly.
            std::erase_if(state->pending_input, [](const QueuedCefInputCommand& pending) {
                return IsPureMouseMove(pending.command);
            });
            state->pending_input.emplace_back(std::move(queued));
        } else if (IsPureMouseMove(queued.command)) {
            std::erase_if(state->pending_input, [](const QueuedCefInputCommand& pending) {
                return IsPureMouseMove(pending.command);
            });
            if (state->pending_input.size() == kMaxPendingInput) {
                state->pending_input.pop_front();
            }
            state->pending_input.emplace_back(std::move(queued));
        } else if (!state->pending_input.empty() &&
            IsPureMouseWheel(queued.command) &&
            IsPureMouseWheel(state->pending_input.back().command)) {
            auto& back_mouse = std::get<MouseInput>(state->pending_input.back().command);
            const auto& new_mouse = std::get<MouseInput>(queued.command);
            if (back_mouse.modifiers == new_mouse.modifiers) {
                back_mouse.wheel_x += new_mouse.wheel_x;
                back_mouse.wheel_y += new_mouse.wheel_y;
                back_mouse.position = new_mouse.position;
                state->pending_input.back().queued_at = queued.queued_at;
            } else {
                if (state->pending_input.size() == kMaxPendingInput) {
                    state->pending_input.pop_front();
                }
                state->pending_input.emplace_back(std::move(queued));
            }
        } else {
            if (state->pending_input.size() == kMaxPendingInput) {
                state->pending_input.pop_front();
            }
            state->pending_input.emplace_back(std::move(queued));
        }
        if (!state->input_drain_scheduled) {
            state->input_drain_scheduled = true;
            schedule_drain = true;
        }
    }

    if (schedule_drain && !PostToCefUI([state] { DrainCefInput(state); })) {
        const std::scoped_lock lock(state->mutex);
        state->pending_input.clear();
        state->input_drain_scheduled = false;
    }
}

void CompleteBrowserCreate(const std::shared_ptr<RuntimeState>& runtime)
{
    {
        const std::scoped_lock lock(runtime->mutex);
        if (runtime->pending_creates > 0) {
            --runtime->pending_creates;
        }
    }
    runtime->changed.notify_all();
}

class CefBrowserClient final : public CefClient,
                               public CefRenderHandler,
                               public CefLifeSpanHandler,
                               public CefLoadHandler,
                               public CefDisplayHandler,
                               public CefDownloadHandler,
                               public CefRequestHandler,
                               public CefResourceRequestHandler {
public:
    explicit CefBrowserClient(std::shared_ptr<BrowserState> state) : state_(std::move(state)) {}

    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override
    {
        return this;
    }

    bool CanDownload(CefRefPtr<CefBrowser>, const CefString&, const CefString&) override
    {
        return false;
    }

    bool OnBeforeDownload(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem>,
        const CefString&, CefRefPtr<CefBeforeDownloadCallback>) override
    {
        return true;
    }

    void OnDownloadUpdated(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem>,
        CefRefPtr<CefDownloadItemCallback> callback) override
    {
        if (callback) {
            callback->Cancel();
        }
    }

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>,
        bool, bool, const CefString&, bool&) override
    {
        const std::scoped_lock lock(state_->mutex);
        return state_->config.mod_content_policy || state_->config.id == "skynet_webview_shielded" ? this : nullptr;
    }

    bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString&,
        const CefString&, CefLifeSpanHandler::WindowOpenDisposition, bool, const CefPopupFeatures&, CefWindowInfo&,
        CefRefPtr<CefClient>&, CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*) override
    {
        const std::scoped_lock lock(state_->mutex);
        return state_->config.id == "skynet_webview_shielded";
    }

    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
        CefRefPtr<CefRequest> request, bool, bool) override
    {
        if (request && BlockSkyNetRequest(request->GetURL())) return true;
        const bool blocked = !request || !IsRequestAllowed(request->GetURL());
        if (blocked) {
            LogPolicyBlock(request ? request->GetURL().ToString() : "<missing URL>");
        }
        return blocked;
    }

    CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(
        CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback>) override
    {
        if (request && BlockSkyNetRequest(request->GetURL())) return RV_CANCEL;
        if (request && IsRequestAllowed(request->GetURL())) {
            return RV_CONTINUE;
        }
        if (request) {
            LogPolicyBlock(request->GetURL().ToString());
        }
        return RV_CANCEL;
    }

    void OnResourceRedirect(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
        CefRefPtr<CefRequest>, CefRefPtr<CefResponse>, CefString& new_url) override
    {
        if (BlockSkyNetRequest(new_url)) {
            new_url = "data:text/plain,";
            return;
        }
        if (!IsRequestAllowed(new_url)) {
            LogPolicyBlock(new_url.ToString());
            new_url = "data:text/plain,Blocked%20by%20SkyUltra%20manifest%20policy";
        }
    }

    void OnProtocolExecution(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
        CefRefPtr<CefRequest>, bool& allow_os_execution) override
    {
        const std::scoped_lock lock(state_->mutex);
        if (state_->config.mod_content_policy) {
            allow_os_execution = false;
        }
    }

    void HandleParsedPageBridgeMessage(CefRefPtr<CefBrowser> browser, PageBridgeMessage parsed)
    {
        if (!state_->main_frame_ready && browser) {
            state_->main_frame_ready = true;
            FlushPendingBridge(browser->GetMainFrame());
            LogStartupStage("page bridge ready");
        }
        if (parsed.name == "ui.pageReady") {
            const bool first_mount = !state_->app_shell_ready;
            state_->app_shell_ready = true;
            if (first_mount) {
                LogStartupStage("application shell mounted");
            }
            if (browser && state_->prime_hidden_frame) {
                browser->GetHost()->Invalidate(PET_VIEW);
            }
        }
        if (state_->page_bridge_callback) {
            state_->page_bridge_callback(std::move(parsed));
        }
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override
    {
        if (source_process != PID_RENDERER || !message) return false;
        const auto name = message->GetName().ToString();
        if (name == kRendererBridgeReadyMessage) {
            if (frame && frame->IsMain()) {
                state_->v8_bridge_ready = true;
                FlushPendingBridge(frame);
            }
            return true;
        }
        if (name == kPageBridgeMessage) {
            const auto arguments = message->GetArgumentList();
            if (frame && frame->IsMain() && arguments && arguments->GetSize() == 1 &&
                arguments->GetType(0) == VTYPE_STRING) {
                PageBridgeMessage parsed;
                if (ParseBridgePayloadJson(state_->id, arguments->GetString(0).ToString(), parsed)) {
                    HandleParsedPageBridgeMessage(browser, std::move(parsed));
                }
            }
            return true;
        }
        if (name == kContextLossMessage) {
            if (!frame || !frame->IsMain()) return true;
            const auto now = std::chrono::steady_clock::now();
            if (now - state_->last_context_loss < std::chrono::seconds(1)) return true;
            state_->last_context_loss = now;
            { const std::scoped_lock lock(state_->mutex); ++state_->health.context_losses;
              state_->health.last_failure = "WebGL context lost; host texture pool discarded.";
              state_->gpu_frame_pool = {}; state_->next_gpu_pool_slot = 0; }
            state_->dirty_history_reset.store(true, std::memory_order_release);
            if (state_->runtime->log_info) state_->runtime->log_info("CEF WebGL context loss on view " + std::to_string(state_->Id()) +
                ". This is a context-loss signal, not proof of a GPU-process crash.");
            return true;
        }
        if (name == kBridgeBatchAckMessage) {
            const auto arguments = message->GetArgumentList();
            if (frame && frame->IsMain() && arguments && arguments->GetSize() == 1 &&
                arguments->GetType(0) == VTYPE_STRING) {
                (void)AcknowledgeCefBridgeBatch(state_, arguments->GetString(0).ToString());
            }
            return true;
        }
        if (name != kEditableFocusMessage) return false;
        const auto arguments = message->GetArgumentList();
        if (!arguments || arguments->GetSize() < 1) {
            return false;
        }
        const auto editable = arguments->GetBool(0);
        state_->focus_on_editable_field.store(editable, std::memory_order_relaxed);
        if (editable && !state_->logged_editable_focus.exchange(true, std::memory_order_relaxed) &&
            state_->runtime->log_info) {
            state_->runtime->log_info("CEF renderer reported focus on an editable page element.");
        }
        return true;
    }

    void OnLoadStart(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
        TransitionType) override
    {
        CEF_REQUIRE_UI_THREAD();
        InstallSkyNetDocumentShield(frame);
        if (frame && frame->IsMain()) {
            ClearCefDocumentState(state_);
            const auto url = frame->GetURL().ToString();
            if (!url.empty() && !url.starts_with("chrome-error:")) state_->recovery_url = url;
            state_->app_shell_ready=false;state_->first_prime_paint_at={};
            LogStartupStage("navigation started");
            if (state_->runtime->log_info) {
                state_->runtime->log_info("CEF load started for view " +
                    std::to_string(state_->id) + ": " + frame->GetURL().ToString());
            }
        }
    }

    void OnRenderViewReady(CefRefPtr<CefBrowser> browser) override
    {
        CEF_REQUIRE_UI_THREAD();
        state_->renderer_ready = true;
        LogStartupStage("renderer ready");
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF renderer became ready for view " +
                std::to_string(state_->id) + " (browser " +
                std::to_string(browser ? browser->GetIdentifier() : 0) + ").");
        }
    }

    void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
        TerminationStatus status, int error_code, const CefString& error_string) override
    {
        CEF_REQUIRE_UI_THREAD();
        state_->renderer_ready = false;
        ClearCefDocumentState(state_);
        if (state_->close_requested || state_->runtime->stopping.load()) return;
        const auto now = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        const auto delay = state_->recovery_policy.NextDelayMs(now);
        { const std::scoped_lock lock(state_->mutex); ++state_->health.renderer_crashes;
          state_->health.last_failure = "Renderer terminated: " + std::to_string(error_code) + " " + error_string.ToString();
          state_->health.recovering = delay != 0 && browser && !state_->recovery_url.empty();
          state_->health.recovery_blocked = !state_->health.recovering;
          if (delay) ++state_->health.recovery_attempts; }
        if (state_->runtime->log_info) state_->runtime->log_info("CEF renderer terminated for view " + std::to_string(state_->Id()) +
            ", status " + std::to_string(static_cast<int>(status)) + ", error " + std::to_string(error_code) +
            (delay ? "; retry in " + std::to_string(delay) + " ms." : "; automatic recovery stopped after repeated failures."));
        if (!delay || !browser || state_->recovery_url.empty()) return;
        const auto generation = state_->recovery_generation;
        const auto url = state_->recovery_url;
        if (!CefPostDelayedTask(TID_UI, new LambdaTask([weak = std::weak_ptr<BrowserState>(state_), generation, url] {
            const auto current = weak.lock();
            if (!current || current->close_requested || current->closed.load() || current->recovery_generation != generation ||
                current->runtime->stopping.load() || current->runtime->gpu_suspended.load() || !current->browser) return;
            { const std::scoped_lock lock(current->mutex);
              if (current->config.resource_budget && current->config.resource_budget->IsSuspended()) return; }
            if (const auto frame = current->browser->GetMainFrame()) frame->LoadURL(url);
        }), delay)) {
            const std::scoped_lock lock(state_->mutex);
            state_->health.recovering = false;
            state_->health.recovery_blocked = true;
            state_->health.last_failure += "; recovery task could not be scheduled";
        }
    }

    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int http_status_code) override
    {
        CEF_REQUIRE_UI_THREAD();
        if (!frame || !frame->IsMain()) {
            return;
        }
        state_->main_frame_ready = true;
        { const std::scoped_lock lock(state_->mutex); state_->health.recovering = false; state_->health.recovery_blocked = false; }
        LogStartupStage("navigation finished");
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF load finished for view " +
                std::to_string(state_->id) + " with HTTP status " +
                std::to_string(http_status_code) + ": " + frame->GetURL().ToString());
        }
        FlushPendingBridge(frame);
        if (browser && !state_->close_requested &&
            !state_->runtime->gpu_suspended.load(std::memory_order_acquire) &&
            (state_->visible_atomic.load(std::memory_order_acquire) ||
             state_->prime_hidden_frame || state_->prime_startup_frame)) {
            browser->GetHost()->Invalidate(PET_VIEW);
        }
    }

    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
        ErrorCode error_code, const CefString& error_text,
        const CefString& failed_url) override
    {
        CEF_REQUIRE_UI_THREAD();
        if (!frame || !frame->IsMain() || error_code == ERR_ABORTED) {
            return;
        }
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF load failed for view " +
                std::to_string(state_->id) + " (error " +
                std::to_string(static_cast<int>(error_code)) + "): " +
                error_text.ToString() + " [" + failed_url.ToString() + "]");
        }
    }

    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t severity,
        const CefString& message, const CefString& source_url, int line) override
    {
        CEF_REQUIRE_UI_THREAD();
        const auto text = message.ToString();
        // Compatibility fallback for an older subprocess; current renderers use IPC.
        if (AcknowledgeCefBridgeBatch(state_, text)) return true;
        if (state_->page_bridge_callback) {
            PageBridgeMessage parsed;
            if (ParseBridgeConsoleMessage(state_->id, text, parsed)) {
                HandleParsedPageBridgeMessage(browser, std::move(parsed));
                return true;
            }
        }
        if (severity == LOGSEVERITY_ERROR && state_->console_errors_logged < 32 &&
            state_->runtime->log_info) {
            ++state_->console_errors_logged;
            state_->runtime->log_info("CEF JavaScript error in view " +
                std::to_string(state_->id) + " (" + source_url.ToString() + ":" +
                std::to_string(line) + "): " + text);
        }
        return false;
    }

    void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override
    {
        const std::scoped_lock lock(state_->mutex);
        rect = CefRect(0, 0,
            static_cast<int>(std::clamp(state_->config.size.width, 1U, 16384U)),
            static_cast<int>(std::clamp(state_->config.size.height, 1U, 16384U)));
    }

    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirty_rects,
        const void* buffer, int width, int height) override
    {
        if (type != PET_VIEW || buffer == nullptr || width <= 0 || height <= 0 ||
            width > 16384 || height > 16384) {
            return;
        }

        if (state_->runtime->gpu_rendering) {
            if (!state_->logged_software_frame.exchange(true, std::memory_order_relaxed) &&
                state_->runtime->log_info) {
                state_->runtime->log_info(
                    "CEF software paint was ignored for view " + std::to_string(state_->id) +
                    " because GPU rendering is required; waiting for an accelerated frame.");
            }
            return;
        }

        if (!state_->logged_software_frame.exchange(true, std::memory_order_relaxed) &&
            state_->runtime->log_info) {
            state_->runtime->log_info("CEF produced a software paint for view " +
                std::to_string(state_->id) + " (" + std::to_string(width) + "x" +
                std::to_string(height) + ").");
        }

        if (state_->pending_dormant_hide && !state_->visible_atomic.load(std::memory_order_acquire)) {
            state_->pending_dormant_hide = false;
            ++state_->dormant_hide_generation;
            {
                const std::scoped_lock lock(state_->mutex);
                state_->gpu_frame.reset();
                state_->pending_software_dirty_rects.clear();
                state_->has_pending_software_frame = false;
                state_->software_full_upload_needed = true;
                state_->shadow_valid = false;
                state_->dirty_history_reset.store(true, std::memory_order_release);
            }
            if (browser) {
                browser->GetHost()->WasHidden(true);
            }
            PostToCefUI([runtime = state_->runtime] { PumpHiddenPrimeQueue(runtime); });
            return;
        }

        if (state_->awaiting_shell_ready.load(std::memory_order_acquire) &&
            !state_->prime_hidden_frame) {
            const auto since_ms = state_->awaiting_shell_ready_since_ms.load(std::memory_order_relaxed);
            const auto elapsed_ms = since_ms > 0 ? (SteadyNowMs() - since_ms) : 0;
            if (elapsed_ms < 1200) {
                return;
            }
            state_->awaiting_shell_ready.store(false, std::memory_order_release);
        }

        const auto row_bytes = static_cast<std::size_t>(width) * 4U;
        const auto image_bytes = row_bytes * static_cast<std::size_t>(height);
        if (image_bytes > 128U * 1024U * 1024U) {
            return;
        }
        const auto* source = static_cast<const std::uint8_t*>(buffer);
        const SizeU paint_size{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };

        bool hide_after_prime{};
        std::uint64_t new_paint_seq{};
        {
            const std::scoped_lock lock(state_->mutex);
            const bool size_changed =
                state_->shadow_size.width != paint_size.width ||
                state_->shadow_size.height != paint_size.height ||
                state_->shadow.size() != image_bytes;
            if (size_changed) {
                state_->shadow_size = paint_size;
                state_->shadow.resize(image_bytes);
                state_->pending_software_dirty_rects.clear();
                state_->software_full_upload_needed = true;
                state_->shadow_valid = false;
            }

            const bool full_dirty_rect = dirty_rects.size() == 1U &&
                dirty_rects[0].x <= 0 && dirty_rects[0].y <= 0 &&
                static_cast<std::int64_t>(dirty_rects[0].x) + dirty_rects[0].width >= width &&
                static_cast<std::int64_t>(dirty_rects[0].y) + dirty_rects[0].height >= height;
            const bool copy_full = size_changed || !state_->shadow_valid ||
                state_->software_full_upload_needed || dirty_rects.empty() || full_dirty_rect;
            if (copy_full) {
                std::memcpy(state_->shadow.data(), source, image_bytes);
                state_->pending_software_dirty_rects.clear();
                state_->software_full_upload_needed = true;
                state_->shadow_valid = true;
            } else {
                bool any_copied{};
                for (const auto& dirty : dirty_rects) {
                    const auto left = std::clamp(dirty.x, 0, width);
                    const auto top = std::clamp(dirty.y, 0, height);
                    const auto right = static_cast<int>(std::clamp<std::int64_t>(
                        static_cast<std::int64_t>(dirty.x) + dirty.width, left, width));
                    const auto bottom = static_cast<int>(std::clamp<std::int64_t>(
                        static_cast<std::int64_t>(dirty.y) + dirty.height, top, height));
                    if (right <= left || bottom <= top) {
                        continue;
                    }
                    any_copied = true;
                    const auto copy_bytes = static_cast<std::size_t>(right - left) * 4U;
                    const auto rows = static_cast<std::uint32_t>(bottom - top);
                    for (std::uint32_t row = 0; row < rows; ++row) {
                        const auto offset =
                            static_cast<std::size_t>(top + static_cast<int>(row)) * row_bytes +
                            static_cast<std::size_t>(left) * 4U;
                        std::memcpy(state_->shadow.data() + offset, source + offset, copy_bytes);
                    }
                }
                if (!any_copied) {
                    return;
                }
                AccumulateDirtyRects(state_->pending_software_dirty_rects, dirty_rects, paint_size);
            }

            state_->has_pending_software_frame = true;
            state_->awaiting_shell_ready.store(false, std::memory_order_release);
            new_paint_seq = state_->paint_sequence.fetch_add(1, std::memory_order_release) + 1;
            const auto now = std::chrono::steady_clock::now();
            if (state_->first_prime_paint_at.time_since_epoch().count() == 0) {
                state_->first_prime_paint_at = now;
            }
            const bool shell_complete = state_->config.id != "SkyUltra.Menu" || state_->app_shell_ready ||
                now - state_->first_prime_paint_at > std::chrono::seconds(20);
            if (state_->prime_hidden_frame && shell_complete) {
                state_->prime_hidden_frame = false;
                hide_after_prime = !state_->config.visible;
            }
            if (state_->prime_startup_frame && state_->app_shell_ready) {
                state_->prime_startup_frame = false;
            }
        }

        TraceMarkPainted(state_->Id(), new_paint_seq);
        if (hide_after_prime && browser) {
            browser->GetHost()->WasHidden(true);
            if (state_->runtime->active_hidden_prime.lock() == state_) {
                state_->runtime->active_hidden_prime.reset();
            }
            PostToCefUI([runtime = state_->runtime] { PumpHiddenPrimeQueue(runtime); });
            if (state_->runtime->log_info) {
                state_->runtime->log_info("CEF primed the initial hidden frame for view " +
                    std::to_string(state_->id) + ".");
            }
        }
    }

    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
        const RectList& dirty_rects, const CefAcceleratedPaintInfo& info) override
    {
        CEF_REQUIRE_UI_THREAD();
        if (type != PET_VIEW || state_->runtime->gpu_suspended.load(std::memory_order_acquire)) {
            return;
        }
        if (state_->pending_dormant_hide && !state_->visible_atomic.load(std::memory_order_acquire)) {
            state_->pending_dormant_hide = false;
            ++state_->dormant_hide_generation;
            {
                const std::scoped_lock lock(state_->mutex);
                state_->gpu_frame.reset();
                state_->pending_software_dirty_rects.clear();
                state_->has_pending_software_frame = false;
                state_->software_full_upload_needed = true;
                state_->shadow_valid = false;
                state_->dirty_history_reset.store(true, std::memory_order_release);
            }
            if (browser) {
                browser->GetHost()->WasHidden(true);
            }
            PostToCefUI([runtime = state_->runtime] { PumpHiddenPrimeQueue(runtime); });
            return;
        }
        if (state_->awaiting_shell_ready.load(std::memory_order_acquire) &&
            !state_->prime_hidden_frame) {
            const auto since_ms = state_->awaiting_shell_ready_since_ms.load(std::memory_order_relaxed);
            const auto elapsed_ms = since_ms > 0 ? (SteadyNowMs() - since_ms) : 0;
            if (elapsed_ms < 1200) {
                state_->dirty_history_reset.store(true, std::memory_order_release);
                return;
            }
            state_->awaiting_shell_ready.store(false, std::memory_order_release);
        }
        if (!state_->logged_accelerated_paint.exchange(true, std::memory_order_relaxed) &&
            state_->runtime->log_info) {
            LogStartupStage("first accelerated paint callback");
            state_->runtime->log_info("CEF sent its first accelerated paint callback for view " +
                std::to_string(state_->id) + ".");
        }
        if (info.shared_texture_handle == nullptr) {
            state_->dirty_history_reset.store(true, std::memory_order_release);
            LogAcceleratedFailure("CEF did not provide a D3D11 shared-texture handle");
            return;
        }

        ComPtr<ID3D11Device1> game_device1;
        ComPtr<ID3D11Device1> helper_device1;
        ComPtr<ID3D11DeviceContext> helper_context;
        ComPtr<ID3D11DeviceContext> fallback_context;
        bool use_helper_device{};
        std::uint64_t device_generation{};
        bool shared_texture_ready{};
        {
            const std::scoped_lock lock(state_->runtime->d3d_mutex);
            shared_texture_ready = state_->runtime->shared_texture_ready;
            if (shared_texture_ready) {
                game_device1 = state_->runtime->d3d_device1;
                use_helper_device = state_->runtime->use_helper_device;
                helper_device1 = state_->runtime->helper_d3d_device1;
                helper_context = state_->runtime->helper_d3d_context;
                fallback_context = state_->runtime->d3d_context;
                device_generation = state_->runtime->d3d_generation;
            }
        }
        if (!shared_texture_ready) {
            state_->dirty_history_reset.store(true, std::memory_order_release);
            LogAcceleratedFailure("received a frame before Skyrim's D3D11 shared-texture path was ready");
            return;
        }
        ID3D11Device1* open_device = use_helper_device ? helper_device1.Get() : game_device1.Get();
        ID3D11DeviceContext* copy_context = use_helper_device ? helper_context.Get() : fallback_context.Get();
        if (game_device1 == nullptr || open_device == nullptr || copy_context == nullptr) {
            state_->dirty_history_reset.store(true, std::memory_order_release);
            LogAcceleratedFailure("received a frame before its D3D11 context was available");
            return;
        }
        if (const auto removed = game_device1->GetDeviceRemovedReason(); FAILED(removed)) {
            SuspendGpuRendering(removed, "D3D11 device state");
            return;
        }
        if (use_helper_device) {
            if (const auto removed = helper_device1->GetDeviceRemovedReason(); FAILED(removed)) {
                SuspendGpuRendering(removed, "CEF helper D3D11 device state");
                return;
            }
        }

        ComPtr<ID3D11Texture2D> shared_texture;
        const auto open_start = std::chrono::steady_clock::now();
        const auto open_result = open_device->OpenSharedResource1(
            info.shared_texture_handle, IID_PPV_ARGS(shared_texture.GetAddressOf()));
        const auto open_end = std::chrono::steady_clock::now();
        if (FAILED(open_result)) {
            state_->dirty_history_reset.store(true, std::memory_order_release);
            SuspendGpuRendering(open_result, "opening a shared frame");
            LogAcceleratedFailure("could not open CEF's shared texture (HRESULT " +
                std::to_string(static_cast<unsigned long>(open_result)) + ")");
            return;
        }

        D3D11_TEXTURE2D_DESC texture_desc{};
        shared_texture->GetDesc(&texture_desc);
        if (texture_desc.Width == 0 || texture_desc.Height == 0 ||
            texture_desc.Width > 16384 || texture_desc.Height > 16384 ||
            texture_desc.MipLevels != 1 || texture_desc.ArraySize != 1 ||
            texture_desc.SampleDesc.Count != 1 ||
            (texture_desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
             texture_desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) {
            state_->dirty_history_reset.store(true, std::memory_order_release);
            LogAcceleratedFailure("CEF supplied an unsupported shared-texture description");
            return;
        }

        const SizeU size{ texture_desc.Width, texture_desc.Height };
        std::shared_ptr<CefGpuFrame> frame;
        std::array<std::shared_ptr<CefGpuFrame>, kCefGpuFramePoolSize> pool_snapshot{};
        std::size_t pool_slot{};
        {
            const std::scoped_lock lock(state_->mutex);
            pool_snapshot = state_->gpu_frame_pool;
            const auto pool_size = state_->gpu_frame_pool.size();
            const auto start_slot = state_->next_gpu_pool_slot % pool_size;
            const CefGpuFrame* current_published = state_->gpu_frame.get();
            bool found_slot{};
            // Pass 1 (preferred): free slot or completed copy that is not currently published.
            for (std::size_t attempt = 0; attempt < pool_size; ++attempt) {
                const auto candidate_slot = (start_slot + attempt) % pool_size;
                const auto& candidate = state_->gpu_frame_pool[candidate_slot];
                if (candidate == nullptr) {
                    pool_slot = candidate_slot;
                    found_slot = true;
                    break;
                }
                if (candidate.get() == current_published) {
                    continue;
                }
                if (candidate->device_generation == device_generation &&
                    candidate->copy_pending.load(std::memory_order_acquire) &&
                    candidate->copy_done_query != nullptr) {
                    const HRESULT query_hr = copy_context->GetData(
                        candidate->copy_done_query.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
                    if (query_hr == S_OK || FAILED(query_hr)) {
                        candidate->copy_pending.store(false, std::memory_order_release);
                    } else if (query_hr == S_FALSE) {
                        continue;
                    }
                }
                pool_slot = candidate_slot;
                found_slot = true;
                if (candidate->size.width == size.width &&
                    candidate->size.height == size.height &&
                    candidate->device_generation == device_generation &&
                    (!use_helper_device || candidate->cef_texture != nullptr)) {
                    frame = candidate;
                }
                break;
            }
            // Pass 2 (fallback — never drop): reuse the oldest non-published slot;
            // copy_context is FIFO so queuing on it never stalls the CPU.
            if (!found_slot) {
                for (std::size_t attempt = 0; attempt < pool_size; ++attempt) {
                    const auto candidate_slot = (start_slot + attempt) % pool_size;
                    const auto& candidate = state_->gpu_frame_pool[candidate_slot];
                    if (candidate != nullptr && candidate.get() == current_published) {
                        continue;
                    }
                    pool_slot = candidate_slot;
                    found_slot = true;
                    if (candidate != nullptr &&
                        candidate->size.width == size.width &&
                        candidate->size.height == size.height &&
                        candidate->device_generation == device_generation &&
                        (!use_helper_device || candidate->cef_texture != nullptr)) {
                        frame = candidate;
                    }
                    break;
                }
            }
            if (!found_slot) {
                pool_slot = start_slot;
            }
        }

        if (frame == nullptr) {
            frame = std::make_shared<CefGpuFrame>();
            frame->size = size;
            frame->device_generation = device_generation;

            if (use_helper_device) {
                HRESULT create_texture_result = S_OK;
                if (!CreateSharedFramePair(
                        helper_device1.Get(), game_device1.Get(), texture_desc,
                        frame->cef_texture, frame->texture, create_texture_result)) {
                    state_->dirty_history_reset.store(true, std::memory_order_release);
                    SuspendGpuRendering(create_texture_result, "allocating a shared frame pair");
                    LogAcceleratedFailure("could not create a shared helper D3D11 texture (HRESULT " +
                        std::to_string(static_cast<unsigned long>(create_texture_result)) + ")");
                    return;
                }
            } else {
                auto owned_texture_desc = texture_desc;
                owned_texture_desc.Usage = D3D11_USAGE_DEFAULT;
                owned_texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                owned_texture_desc.CPUAccessFlags = 0;
                owned_texture_desc.MiscFlags = 0;

                const auto create_texture_result = game_device1->CreateTexture2D(
                    &owned_texture_desc, nullptr, frame->texture.GetAddressOf());
                if (FAILED(create_texture_result)) {
                    state_->dirty_history_reset.store(true, std::memory_order_release);
                    SuspendGpuRendering(create_texture_result, "allocating a frame texture");
                    LogAcceleratedFailure("could not create an owned D3D11 texture (HRESULT " +
                        std::to_string(static_cast<unsigned long>(create_texture_result)) + ")");
                    return;
                }
            }
            const auto create_view_result = game_device1->CreateShaderResourceView(
                frame->texture.Get(), nullptr, frame->shader_resource_view.GetAddressOf());
            if (FAILED(create_view_result)) {
                state_->dirty_history_reset.store(true, std::memory_order_release);
                SuspendGpuRendering(create_view_result, "allocating a frame shader view");
                LogAcceleratedFailure("could not create an owned D3D11 shader view (HRESULT " +
                    std::to_string(static_cast<unsigned long>(create_view_result)) + ")");
                return;
            }
            D3D11_QUERY_DESC query_desc{};
            query_desc.Query = D3D11_QUERY_EVENT;
            (void)open_device->CreateQuery(&query_desc, frame->copy_done_query.GetAddressOf());
        }

        // CEF recycles the source handle as soon as this callback returns. Copy into a
        // slot of our owned 4-texture pool so D3D11 never stalls on RAW hazards;
        // when this slot is initialized, update only merged per-slot dirty regions (< 60% area).
        const auto copy_start = std::chrono::steady_clock::now();
        ID3D11Texture2D* copy_target = use_helper_device ? frame->cef_texture.Get() : frame->texture.Get();
        const bool slot_was_initialized = frame->initialized;
        const bool force_full_reset =
            state_->dirty_history_reset.exchange(false, std::memory_order_acq_rel) || dirty_rects.empty();
        bool copied_regions{};
        std::uint64_t overlapping_pixels_copied{};
        if (!force_full_reset && frame->initialized) {
            AccumulateDirtyRects(frame->pending_dirty_rects, dirty_rects, size);
            std::array<D3D11_BOX, 16> boxes{};
            std::size_t box_count{};
            const auto total_pixels = static_cast<std::uint64_t>(size.width) * size.height;
            for (const auto& rect : frame->pending_dirty_rects) {
                if (box_count >= boxes.size()) break;
                const auto left = std::clamp(rect.x, 0, static_cast<int>(size.width));
                const auto top = std::clamp(rect.y, 0, static_cast<int>(size.height));
                const auto right = static_cast<int>(std::clamp<std::int64_t>(
                    static_cast<std::int64_t>(rect.x) + rect.width, left, static_cast<std::int64_t>(size.width)));
                const auto bottom = static_cast<int>(std::clamp<std::int64_t>(
                    static_cast<std::int64_t>(rect.y) + rect.height, top, static_cast<std::int64_t>(size.height)));
                if (right <= left || bottom <= top) continue;
                boxes[box_count++] = D3D11_BOX{
                    static_cast<UINT>(left), static_cast<UINT>(top), 0,
                    static_cast<UINT>(right), static_cast<UINT>(bottom), 1
                };
            }
            box_count = MergeDirtyBoxes(boxes.data(), box_count, 16U);
            std::uint64_t raw_copied_pixels{};
            for (std::size_t i = 0; i < box_count; ++i) {
                raw_copied_pixels +=
                    static_cast<std::uint64_t>(boxes[i].right - boxes[i].left) *
                    static_cast<std::uint64_t>(boxes[i].bottom - boxes[i].top);
            }
            const auto union_pixels = ComputeUnionPixelArea(boxes.data(), box_count);
            if (box_count > 0U && union_pixels < (total_pixels * 3U) / 5U) {
                overlapping_pixels_copied =
                    raw_copied_pixels > union_pixels ? (raw_copied_pixels - union_pixels) : 0ULL;
                for (std::size_t i = 0; i < box_count; ++i) {
                    copy_context->CopySubresourceRegion(
                        copy_target, 0, boxes[i].left, boxes[i].top, 0, shared_texture.Get(), 0, &boxes[i]);
                }
                copied_regions = true;
            }
        }
        if (!copied_regions) {
            copy_context->CopyResource(copy_target, shared_texture.Get());
            frame->initialized = true;
        }
        frame->pending_dirty_rects.clear();
        for (std::size_t idx = 0; idx < pool_snapshot.size(); ++idx) {
            if (idx == pool_slot) {
                continue;
            }
            const auto& other = pool_snapshot[idx];
            if (other == nullptr) {
                continue;
            }
            if (force_full_reset || other->size.width != size.width ||
                other->size.height != size.height || other->device_generation != device_generation) {
                other->initialized = false;
                other->pending_dirty_rects.clear();
            } else if (other->initialized) {
                AccumulateDirtyRects(other->pending_dirty_rects, dirty_rects, size);
            }
        }
        if (frame->copy_done_query != nullptr) {
            copy_context->End(frame->copy_done_query.Get());
            frame->copy_pending.store(true, std::memory_order_release);
        }
        if (use_helper_device) {
            // Flush the helper context before publishing the shared frame so WDDM submits
            // the write command buffer prior to Skyrim sampling the shader resource view.
            copy_context->Flush();
        }
        RecordCefCopyMetrics(
            copied_regions,
            slot_was_initialized && !copied_regions,
            overlapping_pixels_copied);
        const auto copy_end = std::chrono::steady_clock::now();
        frame->published_at = copy_end;
        frame->display_age_recorded.store(false, std::memory_order_release);
        const auto open_ms = std::chrono::duration<double, std::milli>(open_end - open_start).count();
        const auto copy_ms = std::chrono::duration<double, std::milli>(copy_end - copy_start).count();
        const auto slow_paint_time = std::chrono::steady_clock::now();
        if (state_->runtime->log_info && (open_ms >= 50.0 || copy_ms >= 50.0) &&
            (state_->last_slow_paint_log.time_since_epoch().count() == 0 ||
             slow_paint_time - state_->last_slow_paint_log >= std::chrono::seconds(1))) {
            state_->last_slow_paint_log = slow_paint_time;
            state_->runtime->log_info("CEF accelerated paint for view " + std::to_string(state_->id) +
                ": shared-texture open " + std::to_string(static_cast<int>(open_ms)) +
                " ms wall; copy " + std::to_string(static_cast<int>(copy_ms)) + " ms wall.");
        }
        bool log_accelerated_frame{};
        bool hide_after_prime{};
        std::uint64_t new_paint_seq{};
        {
            const std::scoped_lock d3d_lock(state_->runtime->d3d_mutex);
            if (!state_->runtime->shared_texture_ready ||
                state_->runtime->d3d_generation != device_generation) {
                return;
            }
            const std::scoped_lock state_lock(state_->mutex);
            new_paint_seq = state_->paint_sequence.fetch_add(1, std::memory_order_release) + 1;
            frame->paint_sequence = new_paint_seq;
            state_->gpu_frame_pool[pool_slot] = frame;
            state_->next_gpu_pool_slot = (pool_slot + 1) % state_->gpu_frame_pool.size();
            state_->gpu_frame = std::move(frame);
            state_->awaiting_shell_ready.store(false, std::memory_order_release);
            if (!state_->logged_accelerated_frame) {
                state_->logged_accelerated_frame = true;
                log_accelerated_frame = true;
            }
            // The first paint can be the boot HTML before the application mounts.
            // Keep only SkyUltra's menu warm until its ready callback, with a
            // bound for broken scripts. Other mods retain their one-frame path.
            const auto now = std::chrono::steady_clock::now();
            if (state_->first_prime_paint_at.time_since_epoch().count() == 0)
                state_->first_prime_paint_at = now;
            // Renderer launch/navigation can consume a creation-time deadline
            // before any page frame exists. Budget from the first actual paint.
            const bool shell_complete = state_->config.id != "SkyUltra.Menu" || state_->app_shell_ready ||
                now - state_->first_prime_paint_at > std::chrono::seconds(20);
            if (state_->prime_hidden_frame && shell_complete) {
                state_->prime_hidden_frame = false;
                hide_after_prime = !state_->config.visible;
            }
            if (state_->prime_startup_frame && state_->app_shell_ready) {
                state_->prime_startup_frame = false;
            }
        }
        TraceMarkPainted(state_->Id(), new_paint_seq);
        if (hide_after_prime && browser) {
            // Keep the first GPU frame cached, then stop CEF's background
            // rendering until this initially hidden view is shown by the user.
            browser->GetHost()->WasHidden(true);
            if (state_->runtime->active_hidden_prime.lock() == state_) {
                state_->runtime->active_hidden_prime.reset();
            }
            PostToCefUI([runtime = state_->runtime] { PumpHiddenPrimeQueue(runtime); });
        }
        if (log_accelerated_frame && state_->runtime->log_info) {
            const auto message = "CEF accelerated shared-texture frame copied for view " +
                std::to_string(state_->id) + " (" + std::to_string(size.width) + "x" +
                std::to_string(size.height) + ").";
            state_->runtime->log_info(message);
        }
        if (hide_after_prime && state_->runtime->log_info) {
            state_->runtime->log_info("CEF primed the initial hidden frame for view " +
                std::to_string(state_->id) + ".");
        }
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
    {
        CEF_REQUIRE_UI_THREAD();
        state_->browser = browser;
        LogStartupStage("browser created");
        {
            const std::scoped_lock lock(state_->runtime->mutex);
            if (state_->runtime->pending_creates > 0) {
                --state_->runtime->pending_creates;
            }
            ++state_->runtime->open_browsers;
        }
        state_->runtime->changed.notify_all();
        if(state_->runtime->gpu_suspended.load(std::memory_order_acquire)){
            state_->close_requested=true;
            browser->GetHost()->WasHidden(true);
            browser->GetHost()->CloseBrowser(true);
            return;
        }
        bool visible{};
        std::uint32_t max_fps{};
        ViewConfig config{};
        {
            const std::scoped_lock lock(state_->mutex);
            visible = state_->config.visible;
            max_fps = state_->config.max_fps;
            config = state_->config;
            state_->prime_hidden_frame = !visible;
            state_->prime_startup_frame = true;
            state_->startup_prime_started = std::chrono::steady_clock::now();
        }
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF created browser " +
                std::to_string(browser->GetIdentifier()) + " for view " +
                std::to_string(state_->id) + ".");
        }
        ApplyWindowlessFrameRateUI(state_, visible, config, true);
        state_->visible_atomic.store(visible, std::memory_order_release);
        state_->max_fps_atomic.store(max_fps, std::memory_order_release);
        SyncBrowserVisibleCount(state_);
        browser->GetHost()->WasResized();
        if (visible) {
            InterruptActiveHiddenPrime(state_->runtime);
            browser->GetHost()->WasHidden(false);
        } else {
            // Keep initial hidden views paused at creation and stagger one warm-up paint at a time
            // so startup HUD/menu views are not contended by simultaneous offscreen paints.
            browser->GetHost()->WasHidden(true);
        }
        if (!state_->adopted_initial_url.empty()) {
            auto url = std::move(state_->adopted_initial_url);
            state_->adopted_initial_url.clear();
            state_->main_frame_ready = false;
            browser->GetMainFrame()->LoadURL(url);
        }
        if (state_->close_requested) {
            browser->GetHost()->CloseBrowser(true);
        } else if (visible) {
            browser->GetHost()->Invalidate(PET_VIEW);
        } else {
            EnqueueHiddenPrimeLockedUI(state_);
            PumpHiddenPrimeQueue(state_->runtime);
        }
    }

    void OnBeforeClose(CefRefPtr<CefBrowser>) override
    {
        CEF_REQUIRE_UI_THREAD();
        state_->browser = nullptr;
        state_->closed.store(true, std::memory_order_release);
        SyncBrowserVisibleCount(state_);
        ClearCefDocumentState(state_);
        const bool was_active_prime = state_->runtime->active_hidden_prime.lock() == state_;
        if (was_active_prime) {
            state_->runtime->active_hidden_prime.reset();
        }

        {
            const std::scoped_lock lock(state_->runtime->mutex);
            if (state_->runtime->open_browsers > 0) {
                --state_->runtime->open_browsers;
            }
        }
        state_->runtime->changed.notify_all();
        if (was_active_prime) {
            PumpHiddenPrimeQueue(state_->runtime);
        }
    }

    IMPLEMENT_REFCOUNTING(CefBrowserClient);

private:
    void SuspendGpuRendering(HRESULT reason,std::string_view operation)
    {
        if(!IsFatalGpuResourceFailure(reason))return;
        if(state_->runtime->gpu_suspended.exchange(true,std::memory_order_acq_rel))return;
        state_->runtime->visible_browser_count.store(0U, std::memory_order_release);
        std::vector<std::shared_ptr<BrowserState>> browsers;
        {
            const std::scoped_lock lock(state_->runtime->mutex);
            for(const auto& weak:state_->runtime->browsers)if(auto view=weak.lock())browsers.push_back(std::move(view));
        }
        for(const auto& view:browsers){
            {
                const std::scoped_lock lock(view->mutex);
                view->gpu_frame.reset();view->gpu_frame_pool={};
                view->next_gpu_pool_slot=0;
            }
            view->close_requested=true;
            if(view->browser){
                view->browser->GetHost()->WasHidden(true);
                view->browser->GetHost()->CloseBrowser(true);
            }
        }
        if(state_->runtime->log_info){
            char code[16]{};
            std::snprintf(code,sizeof(code),"0x%08X",static_cast<unsigned>(reason));
            state_->runtime->log_info("CEF GPU rendering suspended for this session after "+std::string(operation)+
                " failed ("+code+"). Browser painting and new GPU views are stopped; restart the game after resolving GPU resource/driver pressure.");
        }
    }

    bool UsesSkyNetShield() const
    {
        const std::scoped_lock lock(state_->mutex);
        return state_->config.id == "skynet_webview_shielded";
    }

    bool BlockSkyNetRequest(const CefString& url) const
    {
        if (!UsesSkyNetShield()) return false;
        const auto block = GetSkyNetWebHooks().block;
        return block && block(url.ToString().c_str());
    }

    void InstallSkyNetDocumentShield(CefRefPtr<CefFrame> frame) const
    {
        if (!frame || !frame->IsMain() || !UsesSkyNetShield()) return;
        const auto get_script = GetSkyNetWebHooks().script;
        const auto url = frame->GetURL();
        if (const auto script = get_script ? get_script(url.ToString().c_str()) : nullptr)
            frame->ExecuteJavaScript(script, url, 1);
    }

    [[nodiscard]] bool IsRequestAllowed(const CefString& url) const
    {
        ViewConfig config;
        {
            const std::scoped_lock lock(state_->mutex);
            config = state_->config;
        }
        return IsModRequestAllowed(config, url);
    }

    void LogStartupStage(std::string_view stage) const
    {
        if (!state_->runtime->log_info) return;
        std::chrono::steady_clock::time_point requested_at;
        {
            const std::scoped_lock lock(state_->mutex);
            requested_at = state_->requested_at;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - requested_at).count();
        state_->runtime->log_info("CEF startup view " + std::to_string(state_->Id()) +
            ": " + std::string(stage) + " at +" + std::to_string(elapsed) + " ms.");
    }

    void FlushPendingBridge(CefRefPtr<CefFrame> frame)
    {
        if (!frame) return;
        ScheduleCefBridgeDrain(state_);
    }

    void LogAcceleratedFailure(std::string message)
    {
        bool should_log{};
        {
            const std::scoped_lock lock(state_->mutex);
            if (!state_->logged_accelerated_failure) {
                state_->logged_accelerated_failure = true;
                should_log = true;
            }
        }
        if (should_log && state_->runtime->log_info) {
            state_->runtime->log_info("CEF accelerated shared-texture path: " + message + ".");
        }
    }

    void LogPolicyBlock(const std::string& url)
    {
        if (!state_->logged_policy_block.exchange(true, std::memory_order_relaxed) &&
            state_->runtime->log_info) {
            state_->runtime->log_info("SkyUltra manifest policy blocked a CEF request for view " +
                std::to_string(state_->id) + ": " + url);
        }
    }

    static void DispatchBridgeEvent(CefRefPtr<CefFrame> frame,
        const std::string& name, const std::string& payload)
    {
        if (!frame) {
            return;
        }
        const auto script = BuildBridgeEventScript(name, payload);
        frame->ExecuteJavaScript(script, frame->GetURL(), 1);
    }

    static void DispatchBridgeResponse(CefRefPtr<CefFrame> frame,
        const PageBridgeResponse& response)
    {
        if (!frame) {
            return;
        }
        const auto script = BuildBridgeResponseScript(response.request_id, response.success,
            response.payload_json, response.error_message);
        frame->ExecuteJavaScript(script, frame->GetURL(), 1);
    }

    std::shared_ptr<BrowserState> state_;
};

class CefView final : public IWebView {
public:
    CefView(std::shared_ptr<BrowserState> state, CefRefPtr<CefBrowserClient> client) :
        state_(std::move(state)), client_(std::move(client)) {
        state_->bridge_consumer = state_->config.bridge_event_queue->Attach([weak = std::weak_ptr<BrowserState>(state_)] {
            if (const auto state = weak.lock()) ScheduleCefBridgeDrain(state);
        });
        state_->response_consumer = state_->config.bridge_response_queue->Attach([weak = std::weak_ptr<BrowserState>(state_)] {
            if (const auto state = weak.lock()) ScheduleCefBridgeDrain(state);
        });
        ScheduleCefBridgeDrain(state_);
    }

    ~CefView() override { Close(); }

    [[nodiscard]] BrowserHealth Health() const override {
        const std::scoped_lock lock(state_->mutex);
        auto health = state_->health; health.crash_reports_enabled = state_->runtime->crash_reports_enabled; return health;
    }
    [[nodiscard]] ViewId Id() const noexcept override { return state_->id; }
    [[nodiscard]] std::shared_ptr<BridgeEventQueue> BridgeEvents() const override {
        const std::scoped_lock lock(state_->mutex); return state_->config.bridge_event_queue;
    }
    [[nodiscard]] std::shared_ptr<BridgeEventQueue> BridgeResponses() const override {
        const std::scoped_lock lock(state_->mutex); return state_->config.bridge_response_queue;
    }

    void Navigate(std::string_view url) override
    {
        const std::string target(url);
        PostToCefUI([state = state_, target] {
            if (state->browser && !state->close_requested) {
                state->browser->GetMainFrame()->LoadURL(target);
            }
        });
    }

    void Resize(SizeU size) override
    {
        {
            const std::scoped_lock lock(state_->mutex);
            state_->config.size = size;
        }
        PostToCefUI([state = state_] {
            if (state->browser && !state->close_requested) {
                state->browser->GetHost()->WasResized();
            }
        });
    }

    void SetScale(float scale) override
    {
        const std::scoped_lock lock(state_->mutex);
        state_->config.scale = scale;
    }

    void SetMaxFPS(std::uint32_t fps) override
    {
        const auto clamped_atomic_fps = fps == 0 ? 0U : std::clamp(fps, 1U, 240U);
        const auto bounded_fps = fps == 0 ? 0U : std::clamp(fps, 1U, kMaxCefWindowlessFrameRate);
        state_->max_fps_atomic.store(clamped_atomic_fps, std::memory_order_release);
        ViewConfig config{};
        bool visible{};
        {
            const std::scoped_lock lock(state_->mutex);
            state_->config.max_fps = clamped_atomic_fps;
            config = state_->config;
            config.max_fps = bounded_fps;
            visible = state_->config.visible;
        }
        PostToCefUI([state = state_, visible, config] {
            ApplyWindowlessFrameRateUI(state, visible, config, true);
        });
    }

    void DiscardCachedFrame() noexcept override
    {
        const std::scoped_lock lock(state_->mutex);
        state_->gpu_frame.reset();
        state_->pending_software_dirty_rects.clear();
        state_->has_pending_software_frame = false;
        state_->software_full_upload_needed = true;
        state_->shadow_valid = false;
        state_->dirty_history_reset.store(true, std::memory_order_release);
        state_->awaiting_shell_ready_since_ms.store(0, std::memory_order_relaxed);
        state_->awaiting_shell_ready.store(false, std::memory_order_release);
    }

    void BeginPageTransition() noexcept override
    {
        const std::scoped_lock lock(state_->mutex);
        state_->gpu_frame.reset();
        state_->pending_software_dirty_rects.clear();
        state_->has_pending_software_frame = false;
        state_->software_full_upload_needed = true;
        state_->shadow_valid = false;
        state_->dirty_history_reset.store(true, std::memory_order_release);
        state_->awaiting_shell_ready_since_ms.store(SteadyNowMs(), std::memory_order_relaxed);
        state_->awaiting_shell_ready.store(true, std::memory_order_release);
    }

    void InvalidateSoftwareSurface() noexcept override
    {
        const std::scoped_lock lock(state_->mutex);
        state_->software_full_upload_needed = true;
        if (state_->shadow_valid && !state_->shadow.empty() &&
            !state_->awaiting_shell_ready.load(std::memory_order_acquire)) {
            state_->has_pending_software_frame = true;
        }
    }

    void MarkPageReady() noexcept override
    {
        state_->awaiting_shell_ready_since_ms.store(0, std::memory_order_relaxed);
        state_->awaiting_shell_ready.store(false, std::memory_order_release);
        PostToCefUI([state = state_] {
            if (!state->browser || state->close_requested || state->closed.load()) return;
            bool visible{};
            {
                const std::scoped_lock lock(state->mutex);
                visible = state->config.visible;
            }
            if (visible && !state->runtime->gpu_suspended.load(std::memory_order_acquire) &&
                !state->display_occluded.load(std::memory_order_acquire)) {
                state->browser->GetHost()->Invalidate(PET_VIEW);
            }
        });
    }

    [[nodiscard]] std::uint64_t PaintSequence() const noexcept override
    {
        return state_->paint_sequence.load(std::memory_order_acquire);
    }

    void SetDisplayOccluded(bool occluded) noexcept override
    {
        const bool previous = state_->display_occluded.exchange(occluded, std::memory_order_acq_rel);
        if (previous == occluded) return;
        SyncBrowserVisibleCount(state_);
        PostToCefUI([state = state_, occluded] {
            if (!state->browser || state->close_requested) return;
            bool visible{};
            {
                const std::scoped_lock lock(state->mutex);
                visible = state->config.visible;
            }
            const bool suspended = state->runtime->gpu_suspended.load(std::memory_order_acquire);
            auto host = state->browser->GetHost();
            const bool is_active_prime =
                !visible && state->prime_hidden_frame &&
                state->runtime->active_hidden_prime.lock() == state;
            host->WasHidden(suspended || occluded || (!visible && !is_active_prime));
            if (!occluded && visible && !suspended && !state->awaiting_shell_ready.load(std::memory_order_acquire)) {
                InterruptActiveHiddenPrime(state->runtime);
                host->Invalidate(PET_VIEW);
            } else if (occluded) {
                if (state->runtime->active_hidden_prime.lock() == state) {
                    state->runtime->active_hidden_prime.reset();
                }
                PumpHiddenPrimeQueue(state->runtime);
            }
        });
    }

    void SetVisible(bool visible) override
    {
        ViewConfig cfg{};
        state_->visible_atomic.store(visible, std::memory_order_release);
        {
            const std::scoped_lock lock(state_->mutex);
            state_->config.visible = visible;
            cfg = state_->config;
            if (!visible) {
                state_->gpu_frame.reset();
                state_->pending_software_dirty_rects.clear();
                state_->has_pending_software_frame = false;
                state_->software_full_upload_needed = true;
                state_->shadow_valid = false;
            } else {
                state_->software_full_upload_needed = true;
            }
        }
        SyncBrowserVisibleCount(state_);
        PostToCefUI([state = state_, visible, cfg] {
            if (state->browser && !state->close_requested) {
                auto host = state->browser->GetHost();
                if (visible) {
                    state->pending_dormant_hide = false;
                    ++state->dormant_hide_generation;
                    state->prime_hidden_frame = false;
                    if (state->runtime->active_hidden_prime.lock() == state) {
                        state->runtime->active_hidden_prime.reset();
                    }
                    InterruptActiveHiddenPrime(state->runtime);
                    if (auto frame = state->browser->GetMainFrame()) {
                        if (auto msg = CefProcessMessage::Create(kBoostPriorityMessage)) {
                            frame->SendProcessMessage(PID_RENDERER, msg);
                        }
                    }
                }
                const bool suspended = state->runtime->gpu_suspended.load(std::memory_order_acquire);
                const bool occluded = state->display_occluded.load(std::memory_order_acquire);
                const bool is_active_prime =
                    !visible && state->prime_hidden_frame &&
                    state->runtime->active_hidden_prime.lock() == state;
                ApplyWindowlessFrameRateUI(state, visible, cfg);
                const bool defer_dormant_hide =
                    !visible && !suspended && !occluded && !is_active_prime &&
                    !state->prime_hidden_frame && state->main_frame_ready &&
                    cfg.id == "SkyUltra.Menu";
                if (defer_dormant_hide) {
                    state->pending_dormant_hide = true;
                    const auto hide_gen = ++state->dormant_hide_generation;
                    host->WasHidden(false);
                    CefPostDelayedTask(TID_UI, new LambdaTask([weak = std::weak_ptr<BrowserState>(state), hide_gen] {
                        const auto current = weak.lock();
                        if (!current || current->close_requested || current->closed.load()) return;
                        if (current->pending_dormant_hide &&
                            current->dormant_hide_generation == hide_gen &&
                            !current->visible_atomic.load(std::memory_order_acquire)) {
                            current->pending_dormant_hide = false;
                            if (current->browser) {
                                current->browser->GetHost()->WasHidden(true);
                            }
                            PumpHiddenPrimeQueue(current->runtime);
                        }
                    }), 140);
                } else {
                    state->pending_dormant_hide = false;
                    ++state->dormant_hide_generation;
                    host->WasHidden(suspended || occluded || (!visible && !is_active_prime));
                }
                if (visible && !suspended && !occluded) {
                    if (!state->awaiting_shell_ready.load(std::memory_order_acquire)) {
                        host->Invalidate(PET_VIEW);
                    } else {
                        CefPostDelayedTask(TID_UI, new LambdaTask([weak = std::weak_ptr<BrowserState>(state)] {
                            const auto current = weak.lock();
                            if (!current || current->close_requested || current->closed.load()) return;
                            if (current->awaiting_shell_ready.exchange(false, std::memory_order_acq_rel) && current->browser) {
                                current->browser->GetHost()->Invalidate(PET_VIEW);
                            }
                        }), 1200);
                    }
                } else if (!visible && !defer_dormant_hide) {
                    PumpHiddenPrimeQueue(state->runtime);
                }
            }
        });
    }

    void SetFocused(InputMode mode) override
    {
        QueueCefInput(state_, mode);
    }

    void SendMouse(const MouseInput& input) override
    {
        QueueCefInput(state_, input);
    }

    void SendKey(const KeyInput& input) override
    {
        QueueCefInput(state_, input);
    }

    void SendText(const TextInput& input) override
    {
        QueueCefInput(state_, input);
    }

    [[nodiscard]] bool SendEventJson(std::string_view name, std::string_view payload) override
    {
        { const std::scoped_lock lock(state_->mutex); if (!state_->config.enable_javascript) return false; }
        return BridgeEvents()->Push(name, payload) == BridgeEventQueue::Result::Accepted;
    }

    [[nodiscard]] bool ResolveRequest(std::string_view id, bool success, std::string_view payload, std::string_view error) override
    {
        {
            const std::scoped_lock lock(state_->mutex);
            if (!state_->config.enable_javascript || state_->closed.load()) return false;
        }
        return BridgeResponses()->Push(id, BuildBridgeResponseJson(id, success, payload, error)) == BridgeEventQueue::Result::Accepted;
    }

    [[nodiscard]] std::optional<SoftwareFrame> TakeFrame() override
    {
        const std::scoped_lock lock(state_->mutex);
        const auto width = state_->shadow_size.width;
        const auto height = state_->shadow_size.height;
        const auto row_bytes = static_cast<std::size_t>(width) * 4U;
        const auto expected_bytes = row_bytes * static_cast<std::size_t>(height);
        if (!state_->has_pending_software_frame || !state_->shadow_valid ||
            width == 0 || height == 0 || state_->shadow.size() < expected_bytes) {
            return std::nullopt;
        }

        SoftwareFrame frame;
        frame.size = state_->shadow_size;
        frame.sequence = state_->next_frame_sequence++;

        bool emit_full = state_->software_full_upload_needed ||
            state_->pending_software_dirty_rects.empty();
        std::array<D3D11_BOX, 16> boxes{};
        std::size_t box_count{};
        if (!emit_full) {
            for (const auto& rect : state_->pending_software_dirty_rects) {
                if (box_count >= boxes.size()) {
                    break;
                }
                const auto left = std::clamp(rect.x, 0, static_cast<int>(width));
                const auto top = std::clamp(rect.y, 0, static_cast<int>(height));
                const auto right = static_cast<int>(std::clamp<std::int64_t>(
                    static_cast<std::int64_t>(rect.x) + rect.width, left, static_cast<std::int64_t>(width)));
                const auto bottom = static_cast<int>(std::clamp<std::int64_t>(
                    static_cast<std::int64_t>(rect.y) + rect.height, top, static_cast<std::int64_t>(height)));
                if (right <= left || bottom <= top) {
                    continue;
                }
                boxes[box_count++] = D3D11_BOX{
                    static_cast<UINT>(left), static_cast<UINT>(top), 0,
                    static_cast<UINT>(right), static_cast<UINT>(bottom), 1
                };
            }
            box_count = MergeDirtyBoxes(boxes.data(), box_count, 16U);
            const auto total_pixels = static_cast<std::uint64_t>(width) * height;
            const auto union_pixels = ComputeUnionPixelArea(boxes.data(), box_count);
            if (box_count == 0U || union_pixels >= (total_pixels * 3U) / 5U) {
                emit_full = true;
            }
        }

        if (emit_full) {
            PixelPatch full;
            full.rect = { 0, 0, width, height };
            full.stride = width * 4U;
            full.bgra8 = state_->shadow;
            frame.patches.push_back(std::move(full));
        } else {
            frame.patches.reserve(box_count);
            for (std::size_t i = 0; i < box_count; ++i) {
                const auto& box = boxes[i];
                PixelPatch patch;
                patch.rect = {
                    static_cast<std::uint32_t>(box.left),
                    static_cast<std::uint32_t>(box.top),
                    static_cast<std::uint32_t>(box.right - box.left),
                    static_cast<std::uint32_t>(box.bottom - box.top),
                };
                patch.stride = patch.rect.width * 4U;
                patch.bgra8.resize(static_cast<std::size_t>(patch.stride) * patch.rect.height);
                for (std::uint32_t row = 0; row < patch.rect.height; ++row) {
                    const auto src_offset =
                        static_cast<std::size_t>(patch.rect.y + row) * row_bytes +
                        static_cast<std::size_t>(patch.rect.x) * 4U;
                    const auto dst_offset = static_cast<std::size_t>(row) * patch.stride;
                    std::memcpy(patch.bgra8.data() + dst_offset, state_->shadow.data() + src_offset, patch.stride);
                }
                frame.patches.push_back(std::move(patch));
            }
        }

        state_->pending_software_dirty_rects.clear();
        state_->software_full_upload_needed = false;
        state_->has_pending_software_frame = false;
        return frame;
    }

    [[nodiscard]] std::optional<GpuFrame> GetGpuFrame() override
    {
        std::uint64_t device_generation{};
        {
            const std::scoped_lock lock(state_->runtime->d3d_mutex);
            if (!state_->runtime->shared_texture_ready) {
                return std::nullopt;
            }
            if(state_->runtime->gpu_suspended.load(std::memory_order_acquire))return std::nullopt;
            device_generation = state_->runtime->d3d_generation;
        }

        std::shared_ptr<CefGpuFrame> frame;
        {
            const std::scoped_lock lock(state_->mutex);
            frame = state_->gpu_frame;
        }
        if (frame == nullptr || frame->device_generation != device_generation ||
            frame->shader_resource_view == nullptr) {
            return std::nullopt;
        }
        if (frame->published_at.time_since_epoch().count() != 0 &&
            !frame->display_age_recorded.exchange(true, std::memory_order_acq_rel)) {
            const auto age_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - frame->published_at).count();
            RecordCefFrameDisplayAge(age_ms);
        }

        return GpuFrame{
            frame->shader_resource_view.Get(),
            frame->size,
            RectU{ 0, 0, frame->size.width, frame->size.height },
            std::move(frame),
        };
    }

private:
    void Close()
    {
        if (!state_->runtime->initialized.load() || state_->runtime->stopping.load()) {
            return;
        }
        state_->visible_atomic.store(false, std::memory_order_release);
        SyncBrowserVisibleCount(state_);
        PostToCefUI([state = state_] {
            state->close_requested = true;
            if (state->browser) {
                state->browser->GetHost()->CloseBrowser(true);
            }
        });
    }

    std::shared_ptr<BrowserState> state_;
    CefRefPtr<CefBrowserClient> client_;
};

bool QueueBrowserCreation(const std::shared_ptr<RuntimeState>& runtime,
    const std::shared_ptr<BrowserState>& browser_state,
    CefRefPtr<CefBrowserClient> client, std::string url)
{
    ViewConfig config;
    {
        const std::scoped_lock lock(browser_state->mutex);
        config = browser_state->config;
    }
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = CefWindowlessFrameRate(config.max_fps);
    browser_settings.javascript = config.enable_javascript ? STATE_ENABLED : STATE_DISABLED;
    browser_settings.background_color = config.transparent ?
        CefColorSetARGB(0, 0, 0, 0) : CefColorSetARGB(255, 0, 0, 0);
    browser_settings.chrome_status_bubble = STATE_DISABLED;
    browser_settings.chrome_zoom_bubble = STATE_DISABLED;
    CefWindowInfo window_info;
    window_info.SetAsWindowless(nullptr);
    window_info.shared_texture_enabled = runtime->gpu_rendering ? TRUE : FALSE;
    // Keep external BeginFrame disabled so CEF's Viz compositor paces frames internally
    // without crashing the GPU process on overlapping IssueExternalBeginFrame calls.
    window_info.external_begin_frame_enabled = FALSE;

    if (PostToCefUI([runtime, browser_state, client, window_info,
                       browser_settings, url = std::move(url)]() mutable {
            {
                const std::scoped_lock lock(browser_state->mutex);
                browser_state->browser_create_dispatched = true;
                if (!browser_state->pending_create_url.empty()) {
                    url = browser_state->pending_create_url;
                }
                browser_settings.windowless_frame_rate =
                    CefWindowlessFrameRate(browser_state->config.max_fps);
                browser_settings.javascript =
                    browser_state->config.enable_javascript ? STATE_ENABLED : STATE_DISABLED;
                browser_settings.background_color = browser_state->config.transparent ?
                    CefColorSetARGB(0, 0, 0, 0) : CefColorSetARGB(255, 0, 0, 0);
            }
            if (runtime->stopping.load() || runtime->gpu_suspended.load(std::memory_order_acquire)) {
                browser_state->creation_failed.store(true, std::memory_order_release);
                browser_state->closed.store(true, std::memory_order_release);
                SyncBrowserVisibleCount(browser_state);
                CompleteBrowserCreate(runtime);
                return;
            }
            if (!CefBrowserHost::CreateBrowser(
                    window_info, client, url, browser_settings, nullptr, nullptr)) {
                browser_state->creation_failed.store(true, std::memory_order_release);
                browser_state->closed.store(true, std::memory_order_release);
                SyncBrowserVisibleCount(browser_state);
                if (runtime->log_info) {
                    runtime->log_info("CEF rejected browser creation for view " +
                        std::to_string(browser_state->Id()) + ": " + url);
                }
                CompleteBrowserCreate(runtime);
            }
        })) {
        return true;
    }
    browser_state->creation_failed.store(true, std::memory_order_release);
    CompleteBrowserCreate(runtime);
    return false;
}

void BindD3D11Device(
    const std::shared_ptr<RuntimeState>& state,
    ID3D11Device* device,
    ID3D11DeviceContext* optional_context)
{
    if (!state || !state->gpu_rendering || device == nullptr ||
        state->gpu_suspended.load(std::memory_order_acquire)) {
        return;
    }

    bool changed{};
    bool shared_texture_ready{};
    std::string diagnostic_message;
    {
        const std::scoped_lock lock(state->d3d_mutex);
        if (state->d3d_device.Get() == device &&
            (state->use_helper_device ||
             (optional_context != nullptr && state->d3d_context.Get() == optional_context))) {
            return;
        }
        if (optional_context == nullptr && state->d3d_device.Get() == device &&
            state->logged_shared_texture_unavailable) {
            return;
        }

        ComPtr<ID3D11Device1> device1;
        const auto single_threaded =
            (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0;
        const bool has_device1 = !single_threaded &&
            SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(device1.GetAddressOf()))) &&
            device1 != nullptr;

        ComPtr<ID3D11Device> helper_device;
        ComPtr<ID3D11Device1> helper_device1;
        ComPtr<ID3D11DeviceContext> helper_context;
        bool using_helper{};
        if (has_device1 &&
            CreateCefHelperD3D11Device(
                device, device1.Get(), helper_device, helper_device1, helper_context)) {
            using_helper = true;
            shared_texture_ready = true;
        } else if (optional_context == nullptr) {
            // Prewarm failed to create a cross-device helper; defer to PumpFrame's
            // fallback path once Skyrim's immediate context is supplied.
            return;
        } else if (has_device1) {
            ComPtr<ID3D11Multithread> multithread;
            if (SUCCEEDED(optional_context->QueryInterface(IID_PPV_ARGS(multithread.GetAddressOf())))) {
                const auto was_protected = multithread->GetMultithreadProtected();
                if (!was_protected) {
                    multithread->SetMultithreadProtected(TRUE);
                    const auto already_recorded = std::any_of(
                        state->protected_contexts.begin(), state->protected_contexts.end(),
                        [&multithread](const ProtectedD3DContext& item) {
                            return item.multithread.Get() == multithread.Get();
                        });
                    if (!already_recorded) {
                        state->protected_contexts.push_back({ multithread, FALSE });
                    }
                }
                shared_texture_ready = multithread->GetMultithreadProtected() != FALSE;
            }
        }

        changed = true;
        state->d3d_device = device;
        state->d3d_device1 = std::move(device1);
        state->d3d_context = optional_context;
        state->helper_d3d_device = std::move(helper_device);
        state->helper_d3d_device1 = std::move(helper_device1);
        state->helper_d3d_context = std::move(helper_context);
        state->use_helper_device = using_helper;
        state->use_helper_device_atomic.store(using_helper, std::memory_order_release);
        state->cached_device_ptr.store(device, std::memory_order_release);
        state->cached_context_ptr.store(optional_context, std::memory_order_release);
        state->shared_texture_ready = shared_texture_ready;
        state->shared_texture_ready_atomic.store(shared_texture_ready, std::memory_order_release);
        ++state->d3d_generation;
        if (shared_texture_ready && !state->logged_shared_texture_ready) {
            state->logged_shared_texture_ready = true;
            diagnostic_message = using_helper
                ? "CEF D3D11 shared-texture path is ready (dedicated helper device; Skyrim immediate context stays lock-free)."
                : "CEF D3D11 shared-texture path is ready (fallback multithreaded context).";
        } else if (!shared_texture_ready && !state->logged_shared_texture_unavailable) {
            state->logged_shared_texture_unavailable = true;
            diagnostic_message = single_threaded ?
                "CEF shared textures are unavailable because Skyrim's D3D11 device is single-threaded." :
                "CEF shared textures are unavailable because D3D11.1 or multithread protection is missing.";
        }
    }

    if (!diagnostic_message.empty() && state->log_info) {
        state->log_info(diagnostic_message);
    }
    if (!changed) {
        return;
    }

    std::vector<std::shared_ptr<BrowserState>> browsers;
    {
        const std::scoped_lock lock(state->mutex);
        for (const auto& weak_browser : state->browsers) {
            if (auto browser = weak_browser.lock()) {
                browsers.push_back(std::move(browser));
            }
        }
    }
    for (const auto& browser : browsers) {
        {
            const std::scoped_lock lock(browser->mutex);
            browser->gpu_frame.reset();
            browser->gpu_frame_pool = {};
            browser->next_gpu_pool_slot = 0;
        }
        browser->dirty_history_reset.store(true, std::memory_order_release);
    }
    if (!shared_texture_ready) {
        return;
    }

    for (const auto& browser : browsers) {
        (void)PostToCefUI([browser] {
            if (browser->browser != nullptr && !browser->close_requested) {
                browser->browser->GetHost()->Invalidate(PET_VIEW);
            }
        });
    }
}

}  // namespace

struct CefRuntime::State : RuntimeState {
    CefRefPtr<CefAppHost> app;
    std::shared_ptr<BrowserState> spare_browser;
    CefRefPtr<CefBrowserClient> spare_client;
    HMODULE cef_library{};
};

CefRuntime::CefRuntime(CefRuntimeConfig config) :
    config_(std::move(config)), state_(std::make_shared<State>())
{
    state_->log_info = config_.log_info;
    state_->gpu_rendering = config_.gpu_rendering;
}

CefRuntime::~CefRuntime()
{
    Shutdown();
}

BrowserEngine CefRuntime::Engine() const noexcept
{
    return BrowserEngine::CEF;
}

bool CefRuntime::NeedsImmediateContextPump(void* d3d_device, void* d3d_context) const noexcept
{
    if (!config_.gpu_rendering || d3d_device == nullptr || d3d_context == nullptr ||
        !state_->initialized.load(std::memory_order_acquire) || state_->stopping.load(std::memory_order_acquire) ||
        state_->gpu_suspended.load(std::memory_order_acquire)) {
        return false;
    }
    if (state_->cached_device_ptr.load(std::memory_order_acquire) == d3d_device &&
        state_->shared_texture_ready_atomic.load(std::memory_order_acquire)) {
        return !state_->use_helper_device_atomic.load(std::memory_order_acquire) &&
            state_->cached_context_ptr.load(std::memory_order_acquire) != d3d_context;
    }
    return state_->cached_device_ptr.load(std::memory_order_acquire) != d3d_device ||
        (!state_->use_helper_device_atomic.load(std::memory_order_acquire) &&
         state_->cached_context_ptr.load(std::memory_order_acquire) != d3d_context);
}

void CefRuntime::PrewarmDevice(void* d3d_device)
{
    if (!state_->initialized.load(std::memory_order_acquire) ||
        state_->stopping.load(std::memory_order_acquire)) {
        return;
    }
    BindD3D11Device(state_, static_cast<ID3D11Device*>(d3d_device), nullptr);
}

void CefRuntime::PumpFrame(void* d3d_device, void* d3d_context)
{
    if (!state_->initialized.load(std::memory_order_acquire) ||
        state_->stopping.load(std::memory_order_acquire)) {
        return;
    }
    const auto now_us = SteadyNowUs();
    const auto prev_us = state_->last_pump_frame_us.exchange(now_us, std::memory_order_relaxed);
    if (prev_us > 0 && now_us > prev_us) {
        const auto dt_us = now_us - prev_us;
        if (dt_us >= 4000 && dt_us <= 120000) {
            auto ema = state_->ema_host_frame_us.load(std::memory_order_relaxed);
            ema = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(ema) * 7ULL + static_cast<std::uint64_t>(dt_us)) / 8ULL);
            state_->ema_host_frame_us.store(ema, std::memory_order_relaxed);
            const auto fps = std::clamp(
                static_cast<std::uint32_t>(std::lround(1000000.0 / static_cast<double>(ema))),
                24U, 120U);
            state_->live_host_fps.store(fps, std::memory_order_relaxed);
        }
    }
    if (!state_->world_paused.load(std::memory_order_acquire) &&
        state_->visible_browser_count.load(std::memory_order_relaxed) > 0) {
        const auto now_ms = SteadyNowMs();
        auto last_sync = state_->last_live_rate_sync_ms.load(std::memory_order_relaxed);
        if ((last_sync == 0 || now_ms - last_sync >= 500) &&
            state_->last_live_rate_sync_ms.compare_exchange_strong(
                last_sync, now_ms, std::memory_order_relaxed)) {
            SyncLiveWindowlessFrameRates(state_, false);
        }
    }
    TickVisibleBeginFrames(state_);
    if (!config_.gpu_rendering || d3d_device == nullptr || d3d_context == nullptr ||
        state_->gpu_suspended.load(std::memory_order_acquire)) {
        return;
    }
    if (state_->cached_device_ptr.load(std::memory_order_acquire) == d3d_device &&
        (state_->use_helper_device_atomic.load(std::memory_order_acquire) ||
         state_->cached_context_ptr.load(std::memory_order_acquire) == d3d_context)) {
        return;
    }
    BindD3D11Device(state_, static_cast<ID3D11Device*>(d3d_device),
        static_cast<ID3D11DeviceContext*>(d3d_context));
}

void CefRuntime::PumpMessageLoop()
{
    if (!config_.multi_threaded_message_loop && state_->initialized.load() &&
        state_->application_thread == std::this_thread::get_id()) {
        CefDoMessageLoopWork();
    }
}

void CefRuntime::SetWorldPaused(bool paused) noexcept
{
    if (state_->world_paused.exchange(paused, std::memory_order_acq_rel) != paused &&
        state_->initialized.load(std::memory_order_acquire) &&
        !state_->stopping.load(std::memory_order_acquire)) {
        SyncLiveWindowlessFrameRates(state_, true);
    }
}

void CefRuntime::SetPageBridgeMessageCallback(PageBridgeMessageCallback callback)
{
    const std::scoped_lock lock(state_->mutex);
    state_->page_bridge_callback = std::move(callback);
}

bool CefRuntime::Initialize(std::string& error)
{
    if (state_->initialized.load()) {
        return true;
    }
    if (!std::filesystem::exists(config_.runtime_directory / L"libcef.dll") ||
        !std::filesystem::exists(config_.runtime_directory / L"v8_context_snapshot.bin") ||
        !std::filesystem::exists(config_.subprocess_executable)) {
        error = "CEF runtime library, V8 snapshot, or subprocess executable is missing.";
        return false;
    }
    if (!std::filesystem::exists(config_.runtime_directory / L"icudtl.dat")) {
        error = "CEF icudtl.dat is missing beside libcef.dll.";
        return false;
    }
    if (!std::filesystem::exists(config_.runtime_directory / L"chrome_100_percent.pak") ||
        !std::filesystem::exists(config_.runtime_directory / L"resources.pak") ||
        !std::filesystem::exists(config_.runtime_directory / L"locales" / L"en-US.pak")) {
        error = "CEF .pak resources or locales/en-US.pak are missing beside libcef.dll.";
        return false;
    }

    const auto library_load_start = std::chrono::steady_clock::now();
    state_->cef_library = LoadLibraryExW(
        (config_.runtime_directory / L"libcef.dll").c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (state_->cef_library == nullptr) {
        error = "Could not load libcef.dll from the configured CEF runtime directory.";
        return false;
    }
    if (state_->log_info) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - library_load_start).count();
        state_->log_info("CEF runtime library load took " + std::to_string(elapsed) + " ms.");
    }

    CefMainArgs main_args(GetModuleHandleW(nullptr));
    CefSettings settings;
    settings.multi_threaded_message_loop = config_.multi_threaded_message_loop;
    settings.windowless_rendering_enabled = true;
    settings.external_message_pump = false;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;
    std::error_code filesystem_error;
    auto effective_cache_directory = std::filesystem::absolute(config_.cache_directory, filesystem_error);
    if (filesystem_error || effective_cache_directory.empty()) {
        filesystem_error.clear();
        effective_cache_directory = config_.cache_directory;
    }
    if (effective_cache_directory.filename().empty() && effective_cache_directory.has_parent_path()) {
        effective_cache_directory = effective_cache_directory.parent_path();
    }
    const auto cache_parent = effective_cache_directory.parent_path();
    PruneStaleSessionCacheDirectories(cache_parent);
    if (IsCacheDirectoryLockedByOtherProcess(effective_cache_directory) && !cache_parent.empty()) {
        effective_cache_directory = cache_parent /
            (L"CEFCache_session_" + std::to_wstring(GetCurrentProcessId()));
        if (state_->log_info) {
            state_->log_info(
                "Primary CEF cache directory is locked by another process; using isolated cache '" +
                effective_cache_directory.string() + "'.");
        }
    }
    const auto log_file = config_.log_file.empty() ?
        effective_cache_directory / L"SkyUltra_CEF.log" : config_.log_file;
    filesystem_error.clear();
    std::filesystem::create_directories(effective_cache_directory, filesystem_error);
    if (filesystem_error) {
        error = "Could not create the CEF cache directory.";
        FreeLibrary(state_->cef_library);
        state_->cef_library = nullptr;
        return false;
    }
    if (!log_file.parent_path().empty()) {
        filesystem_error.clear();
        std::filesystem::create_directories(log_file.parent_path(), filesystem_error);
        if (filesystem_error) {
            error = "Could not create the CEF log directory.";
            FreeLibrary(state_->cef_library);
            state_->cef_library = nullptr;
            return false;
        }
    }
    CefString(&settings.resources_dir_path) = config_.runtime_directory.wstring();
    CefString(&settings.locales_dir_path) = (config_.runtime_directory / L"locales").wstring();
    CefString(&settings.locale) = L"en-US";
    CefString(&settings.browser_subprocess_path) = config_.subprocess_executable.wstring();
    CefString(&settings.root_cache_path) = effective_cache_directory.wstring();
    CefString(&settings.cache_path) = effective_cache_directory.wstring();
    CefString(&settings.log_file) = log_file.wstring();
    const auto decode_preference = GetSkyNetWebHooks().software_decode;
    const bool software_decode = decode_preference && decode_preference();
    state_->app = new CefAppHost(config_.gpu_rendering, software_decode);
    if (software_decode && state_->log_info)
        state_->log_info("CEF is using software video decoding for SkyNet playback compatibility.");

    (void)skyultra::EnsureLiveCompassSharedMemory();
    const auto initialize_start = std::chrono::steady_clock::now();
    if (!CefInitialize(main_args, settings, state_->app, nullptr)) {
        const int exit_code = CefGetExitCode();
        const bool elevated = IsProcessElevated();
        error = "CefInitialize failed (exitCode=" + std::to_string(exit_code) +
            " [" + CefExitCodeName(exit_code) + "], elevated=" + (elevated ? "true" : "false") +
            ", mtml=" + (config_.multi_threaded_message_loop ? "true" : "false") +
            ", cache='" + effective_cache_directory.string() + "'); inspect " +
            log_file.string() + ".";
        state_->app = nullptr;
        FreeLibrary(state_->cef_library);
        state_->cef_library = nullptr;
        return false;
    }
    if (state_->log_info) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - initialize_start).count();
        state_->log_info("CEF process/context initialization took " + std::to_string(elapsed) + " ms.");
    }
    state_->crash_reports_enabled = CefCrashReportingEnabled();
    if (state_->log_info) state_->log_info(std::string("CEF crash reporting enabled=") + (state_->crash_reports_enabled ? "true (configuration beside the game executable)." :
        "false. Optional local capture: tools/Enable-CefCrashReports.ps1; requires game restart."));
    if (state_->crash_reports_enabled) {
        CefSetCrashKeyValue("skyultra-process", "browser"); CefSetCrashKeyValue("skyultra-operation", "initialized");
    }
    state_->application_thread = std::this_thread::get_id();
    state_->stopping.store(false);
    state_->gpu_suspended.store(false,std::memory_order_release);
    state_->initialized.store(true);

    // Initialize the first renderer process while the game is still loading.
    // This single transparent browser is donated to the real menu/HUD; it is
    // not an extra permanently running page or a pool that grows over time.
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    ViewConfig spare_config;
    spare_config.size = {
        screen_w > 0 ? static_cast<std::uint32_t>(std::clamp(screen_w, 640, 3840)) : 1920U,
        screen_h > 0 ? static_cast<std::uint32_t>(std::clamp(screen_h, 480, 2160)) : 1080U,
    };
    spare_config.visible = false;
    spare_config.transparent = true;
    spare_config.enable_javascript = true;
    spare_config.max_fps = 1;
    auto spare_browser = std::make_shared<BrowserState>(state_, 0, spare_config);
    spare_browser->spare_browser = true;
    spare_browser->pending_create_url = "about:blank";
    auto spare_client = CefRefPtr<CefBrowserClient>(new CefBrowserClient(spare_browser));
    {
        const std::scoped_lock lock(state_->mutex);
        state_->spare_browser = spare_browser;
        state_->spare_client = spare_client;
        state_->browsers.emplace_back(spare_browser);
        ++state_->pending_creates;
    }
    if (!QueueBrowserCreation(state_, spare_browser, spare_client, "about:blank")) {
        const std::scoped_lock lock(state_->mutex);
        state_->spare_browser.reset();
        state_->spare_client = nullptr;
    } else if (state_->log_info) {
        state_->log_info("CEF queued its single startup renderer for the first transparent JavaScript view.");
    }
    return true;
}

void CefRuntime::Shutdown() noexcept
{
    if (!state_->initialized.load()) {
        return;
    }
    if (IsProcessShutdownInProgress()) {
        state_->stopping.store(true);
        state_->initialized.store(false);
        state_->visible_browser_count.store(0U, std::memory_order_release);
        return;
    }
    state_->stopping.store(true);

    std::vector<std::shared_ptr<BrowserState>> browsers;
    {
        const std::scoped_lock lock(state_->mutex);
        for (auto it = state_->browsers.begin(); it != state_->browsers.end();) {
            if (auto browser = it->lock()) {
                browsers.push_back(std::move(browser));
                ++it;
            } else {
                it = state_->browsers.erase(it);
            }
        }
    }
    for (const auto& browser : browsers) {
        PostToCefUI([browser] {
            browser->close_requested = true;
            if (browser->browser) {
                browser->browser->GetHost()->CloseBrowser(true);
            }
        });
    }

    const auto browsers_closed = [this] {
        return state_->pending_creates == 0 && state_->open_browsers == 0;
    };
    bool all_closed{};
    if (config_.multi_threaded_message_loop) {
        std::unique_lock lock(state_->mutex);
        all_closed = state_->changed.wait_for(lock, std::chrono::seconds(3), browsers_closed);
    } else {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        for (;;) {
            {
                const std::scoped_lock lock(state_->mutex);
                if (browsers_closed()) {
                    all_closed = true;
                    break;
                }
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            if (std::this_thread::get_id() == state_->application_thread) {
                CefDoMessageLoopWork();
            }
            std::unique_lock lock(state_->mutex);
            if (state_->changed.wait_for(lock, std::chrono::milliseconds(1), browsers_closed)) {
                all_closed = true;
                break;
            }
        }
    }
    {
        const std::scoped_lock lock(state_->mutex);
        state_->spare_client = nullptr;
        state_->spare_browser.reset();
        state_->browsers.clear();
    }
    if (all_closed) {
        CefShutdown();
    }
    {
        const std::scoped_lock lock(state_->d3d_mutex);
        for (auto it = state_->protected_contexts.rbegin();
             it != state_->protected_contexts.rend(); ++it) {
            if (it->multithread != nullptr) {
                it->multithread->SetMultithreadProtected(it->previously_protected);
            }
        }
        state_->protected_contexts.clear();
        state_->d3d_device.Reset();
        state_->d3d_device1.Reset();
        state_->d3d_context.Reset();
        state_->helper_d3d_context.Reset();
        state_->helper_d3d_device1.Reset();
        state_->helper_d3d_device.Reset();
        state_->use_helper_device = false;
        state_->use_helper_device_atomic.store(false, std::memory_order_release);
        state_->cached_device_ptr.store(nullptr, std::memory_order_release);
        state_->cached_context_ptr.store(nullptr, std::memory_order_release);
        state_->shared_texture_ready = false;
        state_->shared_texture_ready_atomic.store(false, std::memory_order_release);
    }
    state_->visible_browser_count.store(0U, std::memory_order_release);
    state_->app = nullptr;
    state_->initialized.store(false);
    state_->stopping.store(false);
    if (all_closed && state_->cef_library != nullptr) {
        FreeLibrary(state_->cef_library);
        state_->cef_library = nullptr;
    }
}

std::unique_ptr<IWebView> CefRuntime::CreateView(
    ViewId id, const ViewConfig& config, std::string_view initial_url)
{
    const auto requested_at = std::chrono::steady_clock::now();
    if (!state_->initialized.load() || state_->stopping.load() || id == 0 ||
        config.size.width == 0 || config.size.height == 0 || state_->gpu_suspended.load(std::memory_order_acquire)) {
        return {};
    }

    std::shared_ptr<BrowserState> browser_state;
    CefRefPtr<CefBrowserClient> client;
    PageBridgeMessageCallback page_bridge_callback;
    bool adopted{};
    {
        const std::scoped_lock lock(state_->mutex);
        if (state_->stopping.load()) {
            return {};
        }
        if (state_->spare_browser &&
            (state_->spare_browser->creation_failed.load(std::memory_order_acquire) ||
             state_->spare_browser->closed.load(std::memory_order_acquire))) {
            state_->spare_client = nullptr;
            state_->spare_browser.reset();
        }
        // Javascript and background transparency are browser-creation settings.
        // Match the transparent menu's browser-creation settings so startup
        // does not leave an unused opaque browser consuming GPU resources.
        if (config.enable_javascript && config.transparent && state_->spare_browser &&
            !state_->spare_browser->creation_failed.load(std::memory_order_acquire)) {
            browser_state = std::move(state_->spare_browser);
            client = state_->spare_client;
            state_->spare_client = nullptr;
            adopted = true;
        } else {
            browser_state = std::make_shared<BrowserState>(state_, id, config);
            browser_state->requested_at = requested_at;
            client = new CefBrowserClient(browser_state);
            ++state_->pending_creates;
            state_->browsers.emplace_back(browser_state);
        }
        page_bridge_callback = state_->page_bridge_callback;
        if (!adopted) browser_state->page_bridge_callback = page_bridge_callback;
    }
    if (adopted) {
        bool create_already_dispatched{};
        {
            const std::scoped_lock lock(browser_state->mutex);
            browser_state->id.store(id, std::memory_order_relaxed);
            browser_state->config = config;
            browser_state->visible_atomic.store(config.visible, std::memory_order_release);
            browser_state->max_fps_atomic.store(config.max_fps, std::memory_order_release);
            if (!browser_state->config.bridge_event_queue) browser_state->config.bridge_event_queue = std::make_shared<BridgeEventQueue>();
            if (!browser_state->config.bridge_response_queue) browser_state->config.bridge_response_queue = std::make_shared<BridgeEventQueue>(false);
            browser_state->requested_at = requested_at;
            browser_state->gpu_frame.reset();
            browser_state->pending_software_dirty_rects.clear();
            browser_state->has_pending_software_frame = false;
            browser_state->software_full_upload_needed = true;
            browser_state->shadow_valid = false;
            browser_state->shadow.clear();
            browser_state->shadow_size = {};
            browser_state->logged_accelerated_frame = false;
            browser_state->logged_accelerated_paint.store(false, std::memory_order_relaxed);
            browser_state->logged_accelerated_failure = false;
            browser_state->logged_software_frame.store(false, std::memory_order_relaxed);
            create_already_dispatched = browser_state->browser_create_dispatched;
            if (!create_already_dispatched) {
                browser_state->pending_create_url = std::string(initial_url);
            }
        }
        SyncBrowserVisibleCount(browser_state);
        if (!PostToCefUI([browser_state, page_bridge_callback = std::move(page_bridge_callback),
                            url = std::string(initial_url),
                            load_if_created = create_already_dispatched]() mutable {
                browser_state->spare_browser = false;
                browser_state->first_prime_paint_at = {};
                browser_state->app_shell_ready = false;
                browser_state->page_bridge_callback = std::move(page_bridge_callback);
                browser_state->main_frame_ready = false;
                browser_state->response_in_flight.reset();
                browser_state->response_ack.clear();
                browser_state->bridge_in_flight.reset();
                browser_state->bridge_ack.clear();

                if (load_if_created) {
                    browser_state->adopted_initial_url = url;
                }
                if (browser_state->browser && !browser_state->close_requested &&
                    !browser_state->runtime->gpu_suspended.load(std::memory_order_acquire)) {
                    auto host = browser_state->browser->GetHost();
                    bool visible{};
                    ViewConfig cfg{};
                    {
                        const std::scoped_lock lock(browser_state->mutex);
                        visible = browser_state->config.visible;
                        cfg = browser_state->config;
                    }
                    browser_state->prime_hidden_frame = !visible;
                    browser_state->prime_startup_frame = true;
                    browser_state->startup_prime_started = std::chrono::steady_clock::now();
                    ApplyWindowlessFrameRateUI(browser_state, visible, cfg, true);
                    host->WasResized();
                    if (visible) {
                        InterruptActiveHiddenPrime(browser_state->runtime);
                        host->WasHidden(false);
                    } else {
                        host->WasHidden(true);
                    }
                    if (load_if_created) {
                        browser_state->adopted_initial_url.clear();
                        browser_state->browser->GetMainFrame()->LoadURL(url);
                    }
                    if (visible) {
                        host->Invalidate(PET_VIEW);
                    } else {
                        EnqueueHiddenPrimeLockedUI(browser_state);
                        PumpHiddenPrimeQueue(browser_state->runtime);
                    }
                }
                if (browser_state->runtime->log_info) {
                    browser_state->runtime->log_info("CEF donated its startup renderer to view " +
                        std::to_string(browser_state->Id()) +
                        (browser_state->renderer_ready ? " (renderer already ready)." :
                            " (renderer still starting)."));
                }
            })) {
            browser_state->closed.store(true, std::memory_order_release);
            SyncBrowserVisibleCount(browser_state);
            return {};
        }
    } else {
        SyncBrowserVisibleCount(browser_state);
        if (!QueueBrowserCreation(state_, browser_state, client, std::string(initial_url))) {
            browser_state->closed.store(true, std::memory_order_release);
            SyncBrowserVisibleCount(browser_state);
            return {};
        }
    }

    return std::make_unique<CefView>(std::move(browser_state), std::move(client));
}

}  // namespace skyultra
