#include "SkyUltra/CefBackend.h"
#include "SkyUltra/ModManifest.h"

#include <windows.h>
#include <d3d11_1.h>
#include <d3d11_4.h>
#include <wrl/client.h>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_client.h"
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

constexpr std::uint32_t kMaxCefWindowlessFrameRate = 240;
constexpr char kEditableFocusMessage[] = "skyultra_editable_focus";

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
    const auto requested_file = std::filesystem::canonical(
        std::filesystem::u8path(local_path), error);
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
    if (!policy.asset_access_enabled) {
        return false;
    }
    return IsPathWithin(policy.root_directory, requested_file);
}

std::uint32_t CurrentDisplayRefreshRate() noexcept
{
    const auto refresh_rate_for = [](const wchar_t* device_name) -> std::uint32_t {
        DEVMODEW display_mode{};
        display_mode.dmSize = sizeof(display_mode);
        if (!EnumDisplaySettingsW(device_name, ENUM_CURRENT_SETTINGS, &display_mode) ||
            display_mode.dmDisplayFrequency == 0) {
            return 0;
        }
        return std::clamp(static_cast<std::uint32_t>(display_mode.dmDisplayFrequency),
            1U, kMaxCefWindowlessFrameRate);
    };

    const auto window = GetForegroundWindow();
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
    // CEF requires a positive rate. Follow the active display when SkyUltra is uncapped.
    return static_cast<int>(fps == 0 ? CurrentDisplayRefreshRate() :
        std::clamp(fps, 1U, kMaxCefWindowlessFrameRate));
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
    std::size_t pending_creates{};
    std::size_t open_browsers{};
    std::vector<std::weak_ptr<BrowserState>> browsers;
    PageBridgeMessageCallback page_bridge_callback;
    std::mutex d3d_mutex;
    ComPtr<ID3D11Device> d3d_device;
    ComPtr<ID3D11Device1> d3d_device1;
    ComPtr<ID3D11DeviceContext> d3d_context;
    std::vector<ProtectedD3DContext> protected_contexts;
    std::uint64_t d3d_generation{};
    bool shared_texture_ready{};
    bool gpu_rendering{ true };
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

std::uint64_t CurrentThreadCpuTime100ns() noexcept
{
    FILETIME creation_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (!GetThreadTimes(GetCurrentThread(), &creation_time, &exit_time, &kernel_time, &user_time)) {
        return 0;
    }

    ULARGE_INTEGER kernel_ticks{};
    kernel_ticks.LowPart = kernel_time.dwLowDateTime;
    kernel_ticks.HighPart = kernel_time.dwHighDateTime;
    ULARGE_INTEGER user_ticks{};
    user_ticks.LowPart = user_time.dwLowDateTime;
    user_ticks.HighPart = user_time.dwHighDateTime;
    return kernel_ticks.QuadPart + user_ticks.QuadPart;
}

class LambdaTask final : public CefTask {
public:
    explicit LambdaTask(std::function<void()> callback) : callback_(std::move(callback)) {}

    void Execute() override { callback_(); }

    IMPLEMENT_REFCOUNTING(LambdaTask);

private:
    std::function<void()> callback_;
};

struct CefGpuFrame {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> shader_resource_view;
    SizeU size{};
    std::uint64_t device_generation{};
};

bool PostToCefUI(std::function<void()> callback)
{
    return CefPostTask(TID_UI, new LambdaTask(std::move(callback)));
}

class CefAppHost final : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    void OnBeforeCommandLineProcessing(
        const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override
    {
        if (process_type.empty() && command_line) {
            // Vite production pages load sibling JS/CSS files from file:// URLs.
            // The per-view request handler still confines those reads to the
            // registered mod root.
            command_line->AppendSwitch("allow-file-access-from-files");
            // OSR has no ordinary visible Chrome window. Keep its renderer at
            // normal priority; WasHidden still stops rendering hidden views.
            command_line->AppendSwitch("disable-renderer-backgrounding");
            command_line->AppendSwitch("disable-backgrounding-occluded-windows");
            // Keep Chromium's driver/feature selection. Forcing rasterization,
            // zero-copy and hardware overlays bypasses its compatibility choices
            // and adds GPU pressure alongside Skyrim on older display drivers.
        }
    }

    IMPLEMENT_REFCOUNTING(CefAppHost);
};

using CefInputCommand = std::variant<InputMode, MouseInput, KeyInput, TextInput>;

struct QueuedCefInputCommand {
    CefInputCommand command;
    std::chrono::steady_clock::time_point queued_at;
};

struct BrowserState {
    BrowserState(std::shared_ptr<RuntimeState> runtime_state, ViewId view_id, ViewConfig view_config) :
        runtime(std::move(runtime_state)), id(view_id), config(std::move(view_config)) {}

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
    std::deque<SoftwareFrame> frames;
    std::deque<std::pair<std::string, std::string>> pending_bridge_events;
    std::deque<PageBridgeResponse> pending_bridge_responses;
    PageBridgeMessageCallback page_bridge_callback;
    std::vector<std::uint8_t> shadow;
    SizeU shadow_size{};
    std::shared_ptr<CefGpuFrame> gpu_frame;
    std::array<std::shared_ptr<CefGpuFrame>, 2> gpu_frame_pool{};
    std::size_t next_gpu_pool_slot{};
    std::atomic_bool logged_accelerated_paint{};
    bool logged_accelerated_frame{};
    bool logged_accelerated_failure{};
    std::atomic_bool logged_policy_block{};
    std::atomic_bool logged_software_frame{};
    std::uint32_t console_errors_logged{};  // CEF UI thread only.
    bool app_shell_ready{};        // CEF UI thread only.
    std::chrono::steady_clock::time_point first_prime_paint_at{};
    bool prime_hidden_frame{};      // Used only from CEF's UI thread.
    std::uint64_t next_frame_sequence{ 1 };
    int held_mouse_modifiers{};     // Used only from CEF's UI thread.
    bool close_requested{};         // Used only from CEF's UI thread.
    bool main_frame_ready{};        // Used only from CEF's UI thread.
    bool spare_browser{};           // Used only from CEF's UI thread.
    bool renderer_ready{};          // Used only from CEF's UI thread.
    std::string adopted_initial_url; // Used only from CEF's UI thread.
    std::atomic_bool creation_failed{};
    std::atomic_bool closed{};
    std::chrono::steady_clock::time_point requested_at{ std::chrono::steady_clock::now() };
};

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
    double max_dispatch_cpu_ms{};
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
        const auto dispatch_cpu_start = CurrentThreadCpuTime100ns();
        DispatchCefInput(state, queued.command);
        const auto dispatch_cpu_end = CurrentThreadCpuTime100ns();
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
            max_dispatch_cpu_ms = dispatch_cpu_start != 0 && dispatch_cpu_end >= dispatch_cpu_start ?
                static_cast<double>(dispatch_cpu_end - dispatch_cpu_start) / 10000.0 : 0.0;
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
            std::to_string(static_cast<int>(max_dispatch_ms)) + " ms wall / " +
            std::to_string(static_cast<int>(max_dispatch_cpu_ms)) + " ms thread CPU (" +
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
        } else if (IsPureMouseMove(queued.command) && !state->pending_input.empty() &&
            IsPureMouseMove(state->pending_input.back().command)) {
            state->pending_input.back() = std::move(queued);
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
                               public CefRequestHandler,
                               public CefResourceRequestHandler {
public:
    explicit CefBrowserClient(std::shared_ptr<BrowserState> state) : state_(std::move(state)) {}

    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override
    {
        return this;
    }

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>,
        bool, bool, const CefString&, bool&) override
    {
        const std::scoped_lock lock(state_->mutex);
        return state_->config.mod_content_policy ? this : nullptr;
    }

    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
        CefRefPtr<CefRequest> request, bool, bool) override
    {
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

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
        CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override
    {
        if (source_process != PID_RENDERER || !message ||
            message->GetName().ToString() != kEditableFocusMessage) {
            return false;
        }
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
        if (frame && frame->IsMain()) {
            state_->main_frame_ready = false;
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
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF renderer terminated for view " +
                std::to_string(state_->id) + " (browser " +
                std::to_string(browser ? browser->GetIdentifier() : 0) +
                ", status " + std::to_string(static_cast<int>(status)) +
                ", error " + std::to_string(error_code) + "): " +
                error_string.ToString());
        }
    }

    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int http_status_code) override
    {
        CEF_REQUIRE_UI_THREAD();
        if (!frame || !frame->IsMain()) {
            return;
        }
        state_->main_frame_ready = true;
        LogStartupStage("navigation finished");
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF load finished for view " +
                std::to_string(state_->id) + " with HTTP status " +
                std::to_string(http_status_code) + ": " + frame->GetURL().ToString());
        }
        FlushPendingBridge(frame);
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
        if (state_->page_bridge_callback) {
            PageBridgeMessage parsed;
            if (ParseBridgeConsoleMessage(state_->id, text, parsed)) {
                // A request proves the bridge is installed. Replies need not wait
                // for load-end (which also waits for unrelated page resources).
                if (!state_->main_frame_ready && browser) {
                    state_->main_frame_ready = true;
                    FlushPendingBridge(browser->GetMainFrame());
                    LogStartupStage("page bridge ready");
                }
                if(parsed.name=="ui.pageReady"){
                    state_->app_shell_ready=true;LogStartupStage("application shell mounted");
                    if(browser)browser->GetHost()->Invalidate(PET_VIEW);
                }
                state_->page_bridge_callback(std::move(parsed));
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

    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList& dirty_rects,
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

        const auto row_bytes = static_cast<std::size_t>(width) * 4U;
        const auto image_bytes = row_bytes * static_cast<std::size_t>(height);
        if (image_bytes > 128U * 1024U * 1024U) {
            return;
        }
        const auto* source = static_cast<const std::uint8_t*>(buffer);

        const std::scoped_lock lock(state_->mutex);
        const SizeU paint_size{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };
        if (state_->shadow_size.width != paint_size.width || state_->shadow_size.height != paint_size.height) {
            state_->shadow_size = paint_size;
            state_->shadow.assign(image_bytes, 0);
            state_->frames.clear();
        }

        SoftwareFrame frame;
        frame.size = paint_size;
        frame.sequence = state_->next_frame_sequence++;
        for (const auto& dirty : dirty_rects) {
            const auto left = std::clamp(dirty.x, 0, width);
            const auto top = std::clamp(dirty.y, 0, height);
            const auto right = static_cast<int>(std::clamp<std::int64_t>(
                static_cast<std::int64_t>(dirty.x) + dirty.width, left, width));
            const auto bottom = static_cast<int>(std::clamp<std::int64_t>(
                static_cast<std::int64_t>(dirty.y) + dirty.height, top, height));
            if (right == left || bottom == top) {
                continue;
            }

            PixelPatch patch;
            patch.rect = {
                static_cast<std::uint32_t>(left),
                static_cast<std::uint32_t>(top),
                static_cast<std::uint32_t>(right - left),
                static_cast<std::uint32_t>(bottom - top),
            };
            patch.stride = patch.rect.width * 4U;
            patch.bgra8.resize(static_cast<std::size_t>(patch.stride) * patch.rect.height);
            for (std::uint32_t row = 0; row < patch.rect.height; ++row) {
                const auto source_offset = static_cast<std::size_t>(top + static_cast<int>(row)) * row_bytes +
                    static_cast<std::size_t>(left) * 4U;
                const auto patch_offset = static_cast<std::size_t>(row) * patch.stride;
                std::memcpy(patch.bgra8.data() + patch_offset, source + source_offset, patch.stride);
                std::memcpy(state_->shadow.data() + source_offset, source + source_offset, patch.stride);
            }
            frame.patches.push_back(std::move(patch));
        }

        if (frame.patches.empty()) {
            return;
        }
        if (state_->frames.size() >= 2) {
            frame.patches.clear();
            PixelPatch full;
            full.rect = { 0, 0, paint_size.width, paint_size.height };
            full.stride = paint_size.width * 4U;
            full.bgra8 = state_->shadow;
            frame.patches.push_back(std::move(full));
            state_->frames.clear();
        }
        state_->frames.push_back(std::move(frame));
    }

    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
        const RectList&, const CefAcceleratedPaintInfo& info) override
    {
        CEF_REQUIRE_UI_THREAD();
        if (type != PET_VIEW || state_->runtime->gpu_suspended.load(std::memory_order_acquire)) {
            return;
        }
        if (!state_->logged_accelerated_paint.exchange(true, std::memory_order_relaxed) &&
            state_->runtime->log_info) {
            LogStartupStage("first accelerated paint callback");
            state_->runtime->log_info("CEF sent its first accelerated paint callback for view " +
                std::to_string(state_->id) + ".");
        }
        if (info.shared_texture_handle == nullptr) {
            LogAcceleratedFailure("CEF did not provide a D3D11 shared-texture handle");
            return;
        }

        ComPtr<ID3D11Device1> device;
        ComPtr<ID3D11DeviceContext> context;
        std::uint64_t device_generation{};
        bool shared_texture_ready{};
        {
            const std::scoped_lock lock(state_->runtime->d3d_mutex);
            shared_texture_ready = state_->runtime->shared_texture_ready;
            if (shared_texture_ready) {
                device = state_->runtime->d3d_device1;
                context = state_->runtime->d3d_context;
                device_generation = state_->runtime->d3d_generation;
            }
        }
        if (!shared_texture_ready) {
            LogAcceleratedFailure("received a frame before Skyrim's D3D11 shared-texture path was ready");
            return;
        }
        if (device == nullptr || context == nullptr) {
            LogAcceleratedFailure("received a frame before its D3D11 context was available");
            return;
        }
        if (const auto removed=device->GetDeviceRemovedReason(); FAILED(removed)) {
            SuspendGpuRendering(removed,"D3D11 device state");
            return;
        }

        ComPtr<ID3D11Texture2D> shared_texture;
        const auto open_start = std::chrono::steady_clock::now();
        const auto open_cpu_start = CurrentThreadCpuTime100ns();
        const auto open_result = device->OpenSharedResource1(
            info.shared_texture_handle, IID_PPV_ARGS(shared_texture.GetAddressOf()));
        const auto open_cpu_end = CurrentThreadCpuTime100ns();
        const auto open_end = std::chrono::steady_clock::now();
        if (FAILED(open_result)) {
            SuspendGpuRendering(open_result,"opening a shared frame");
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
            LogAcceleratedFailure("CEF supplied an unsupported shared-texture description");
            return;
        }

        const SizeU size{ texture_desc.Width, texture_desc.Height };
        std::shared_ptr<CefGpuFrame> frame;
        std::size_t pool_slot{};
        {
            const std::scoped_lock lock(state_->mutex);
            pool_slot = state_->next_gpu_pool_slot % state_->gpu_frame_pool.size();
            const auto& candidate = state_->gpu_frame_pool[pool_slot];
            if (candidate != nullptr && candidate->size.width == size.width &&
                candidate->size.height == size.height &&
                candidate->device_generation == device_generation) {
                frame = candidate;
            }
        }

        if (frame == nullptr) {
            auto owned_texture_desc = texture_desc;
            owned_texture_desc.Usage = D3D11_USAGE_DEFAULT;
            owned_texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            owned_texture_desc.CPUAccessFlags = 0;
            owned_texture_desc.MiscFlags = 0;

            frame = std::make_shared<CefGpuFrame>();
            frame->size = size;
            frame->device_generation = device_generation;
            const auto create_texture_result = device->CreateTexture2D(
                &owned_texture_desc, nullptr, frame->texture.GetAddressOf());
            if (FAILED(create_texture_result)) {
                SuspendGpuRendering(create_texture_result,"allocating a frame texture");
                LogAcceleratedFailure("could not create an owned D3D11 texture (HRESULT " +
                    std::to_string(static_cast<unsigned long>(create_texture_result)) + ")");
                return;
            }
            const auto create_view_result = device->CreateShaderResourceView(
                frame->texture.Get(), nullptr, frame->shader_resource_view.GetAddressOf());
            if (FAILED(create_view_result)) {
                SuspendGpuRendering(create_view_result,"allocating a frame shader view");
                LogAcceleratedFailure("could not create an owned D3D11 shader view (HRESULT " +
                    std::to_string(static_cast<unsigned long>(create_view_result)) + ")");
                return;
            }
        }

        // CEF recycles the source handle as soon as this callback returns. Copy the full
        // frame into the back-buffer slot of our owned 2-texture pool so D3D11 never stalls
        // on a Read-After-Write hazard while the compositor samples the front texture.
        const auto copy_start = std::chrono::steady_clock::now();
        const auto copy_cpu_start = CurrentThreadCpuTime100ns();
        context->CopyResource(frame->texture.Get(), shared_texture.Get());
        const auto copy_cpu_end = CurrentThreadCpuTime100ns();
        const auto copy_end = std::chrono::steady_clock::now();
        const auto open_ms = std::chrono::duration<double, std::milli>(open_end - open_start).count();
        const auto open_cpu_ms = open_cpu_start != 0 && open_cpu_end >= open_cpu_start ?
            static_cast<double>(open_cpu_end - open_cpu_start) / 10000.0 : 0.0;
        const auto copy_ms = std::chrono::duration<double, std::milli>(copy_end - copy_start).count();
        const auto copy_cpu_ms = copy_cpu_start != 0 && copy_cpu_end >= copy_cpu_start ?
            static_cast<double>(copy_cpu_end - copy_cpu_start) / 10000.0 : 0.0;
        const auto slow_paint_time = std::chrono::steady_clock::now();
        if (state_->runtime->log_info && (open_ms >= 50.0 || copy_ms >= 50.0) &&
            (state_->last_slow_paint_log.time_since_epoch().count() == 0 ||
             slow_paint_time - state_->last_slow_paint_log >= std::chrono::seconds(1))) {
            state_->last_slow_paint_log = slow_paint_time;
            state_->runtime->log_info("CEF accelerated paint for view " + std::to_string(state_->id) +
                ": shared-texture open " + std::to_string(static_cast<int>(open_ms)) +
                " ms wall / " + std::to_string(static_cast<int>(open_cpu_ms)) +
                " ms thread CPU; copy " + std::to_string(static_cast<int>(copy_ms)) +
                " ms wall / " + std::to_string(static_cast<int>(copy_cpu_ms)) + " ms thread CPU.");
        }
        bool log_accelerated_frame{};
        bool hide_after_prime{};
        {
            const std::scoped_lock d3d_lock(state_->runtime->d3d_mutex);
            if (!state_->runtime->shared_texture_ready ||
                state_->runtime->d3d_generation != device_generation) {
                return;
            }
            const std::scoped_lock state_lock(state_->mutex);
            state_->gpu_frame_pool[pool_slot] = frame;
            state_->next_gpu_pool_slot = (pool_slot + 1) % state_->gpu_frame_pool.size();
            state_->gpu_frame = std::move(frame);
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
            const bool shell_complete=state_->config.id!="SkyUltra.Menu"||state_->app_shell_ready||
                now-state_->first_prime_paint_at>std::chrono::seconds(20);
            if (state_->prime_hidden_frame && shell_complete) {
                state_->prime_hidden_frame = false;
                hide_after_prime = !state_->config.visible;
            }
        }
        if (hide_after_prime && browser) {
            // Keep the first GPU frame cached, then stop CEF's background
            // rendering until this initially hidden view is shown by the user.
            browser->GetHost()->WasHidden(true);
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
        {
            const std::scoped_lock lock(state_->mutex);
            visible = state_->config.visible;
            max_fps = state_->config.max_fps;
            state_->prime_hidden_frame = !visible;
        }
        if (state_->runtime->log_info) {
            state_->runtime->log_info("CEF created browser " +
                std::to_string(browser->GetIdentifier()) + " for view " +
                std::to_string(state_->id) + ".");
        }
        browser->GetHost()->SetWindowlessFrameRate(visible ? CefWindowlessFrameRate(max_fps) :
            std::min(30, CefWindowlessFrameRate(max_fps)));
        // Render one frame even for initially hidden OSR views. OnAcceleratedPaint
        // caches it and hides the browser again, so opening the overlay does not
        // wait for CEF's hidden-to-visible paint scheduling.
        browser->GetHost()->WasHidden(false);
        browser->GetHost()->WasResized();
        browser->GetHost()->Invalidate(PET_VIEW);
        if (!state_->adopted_initial_url.empty()) {
            auto url = std::move(state_->adopted_initial_url);
            state_->adopted_initial_url.clear();
            state_->main_frame_ready = false;
            browser->GetMainFrame()->LoadURL(url);
        }
        if (state_->close_requested) {
            browser->GetHost()->CloseBrowser(true);
        }
    }

    void OnBeforeClose(CefRefPtr<CefBrowser>) override
    {
        CEF_REQUIRE_UI_THREAD();
        state_->browser = nullptr;
        state_->closed.store(true, std::memory_order_release);
        state_->main_frame_ready = false;
        state_->pending_bridge_events.clear();
        state_->pending_bridge_responses.clear();
        {
            const std::scoped_lock lock(state_->runtime->mutex);
            if (state_->runtime->open_browsers > 0) {
                --state_->runtime->open_browsers;
            }
        }
        state_->runtime->changed.notify_all();
    }

    IMPLEMENT_REFCOUNTING(CefBrowserClient);

private:
    void SuspendGpuRendering(HRESULT reason,std::string_view operation)
    {
        if(!IsFatalGpuResourceFailure(reason))return;
        if(state_->runtime->gpu_suspended.exchange(true,std::memory_order_acq_rel))return;
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
        while (!state_->pending_bridge_events.empty()) {
            auto event = std::move(state_->pending_bridge_events.front());
            state_->pending_bridge_events.pop_front();
            DispatchBridgeEvent(frame, event.first, event.second);
        }
        while (!state_->pending_bridge_responses.empty()) {
            auto response = std::move(state_->pending_bridge_responses.front());
            state_->pending_bridge_responses.pop_front();
            DispatchBridgeResponse(frame, response);
        }
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
        state_(std::move(state)), client_(std::move(client)) {}

    ~CefView() override { Close(); }

    [[nodiscard]] ViewId Id() const noexcept override { return state_->id; }

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
        const auto bounded_fps = fps == 0 ? 0U : std::clamp(fps, 1U, kMaxCefWindowlessFrameRate);
        {
            const std::scoped_lock lock(state_->mutex);
            state_->config.max_fps = bounded_fps;
        }
        PostToCefUI([state = state_, bounded_fps] {
            if (state->browser && !state->close_requested) {
                state->browser->GetHost()->SetWindowlessFrameRate(
                    CefWindowlessFrameRate(bounded_fps));
            }
        });
    }

    void SetVisible(bool visible) override
    {
        std::uint32_t max_fps{};
        {
            const std::scoped_lock lock(state_->mutex);
            state_->config.visible = visible;
            max_fps = state_->config.max_fps;
            if (!visible) {
                state_->gpu_frame.reset();
                state_->frames.clear();
            }
        }
        PostToCefUI([state = state_, visible, max_fps] {
            if (state->browser && !state->close_requested) {
                auto host = state->browser->GetHost();
                if(visible)state->prime_hidden_frame=false;
                const bool suspended=state->runtime->gpu_suspended.load(std::memory_order_acquire);
                host->SetWindowlessFrameRate(visible ? CefWindowlessFrameRate(max_fps) :
                    std::min(30, CefWindowlessFrameRate(max_fps)));
                host->WasHidden(suspended || (!visible&&!state->prime_hidden_frame));
                if (visible && !suspended) {
                    // A hidden windowless browser stops painting. Request a new
                    // frame as soon as it is shown instead of waiting for CEF's
                    // next scheduled paint.
                    host->Invalidate(PET_VIEW);
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

    [[nodiscard]] bool SendEventJson(
        std::string_view event_name, std::string_view payload_json) override
    {
        {
            const std::scoped_lock lock(state_->mutex);
            if (!state_->config.enable_javascript) {
                return false;
            }
        }
        const std::string name(event_name);
        const std::string payload(payload_json);
        return PostToCefUI([state = state_, name, payload] {
            if (!state->browser || state->close_requested) {
                return;
            }
            const auto frame = state->browser->GetMainFrame();
            if (!frame) {
                return;
            }
            if (!state->main_frame_ready) {
                constexpr std::size_t kMaxPendingBridgeEvents = 128;
                if (state->pending_bridge_events.size() < kMaxPendingBridgeEvents) {
                    state->pending_bridge_events.emplace_back(name, payload);
                }
                return;
            }
            const auto script = BuildBridgeEventScript(name, payload);
            frame->ExecuteJavaScript(script, frame->GetURL(), 1);
        });
    }

    [[nodiscard]] bool ResolveRequest(
        std::string_view request_id, bool success, std::string_view payload_json,
        std::string_view error_message) override
    {
        {
            const std::scoped_lock lock(state_->mutex);
            if (!state_->config.enable_javascript) {
                return false;
            }
        }
        PageBridgeResponse response{ std::string(request_id), success,
            std::string(payload_json), std::string(error_message) };
        return PostToCefUI([state = state_, response = std::move(response)] {
            if (!state->browser || state->close_requested) {
                return;
            }
            const auto frame = state->browser->GetMainFrame();
            if (!frame) {
                return;
            }
            if (!state->main_frame_ready) {
                constexpr std::size_t kMaxPendingBridgeResponses = 128;
                if (state->pending_bridge_responses.size() < kMaxPendingBridgeResponses) {
                    state->pending_bridge_responses.push_back(response);
                }
                return;
            }
            const auto script = BuildBridgeResponseScript(response.request_id, response.success,
                response.payload_json, response.error_message);
            frame->ExecuteJavaScript(script, frame->GetURL(), 1);
        });
    }

    [[nodiscard]] std::optional<SoftwareFrame> TakeFrame() override
    {
        const std::scoped_lock lock(state_->mutex);
        if (state_->frames.empty()) {
            return std::nullopt;
        }
        auto frame = std::move(state_->frames.front());
        state_->frames.pop_front();
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
    CefWindowInfo window_info;
    window_info.SetAsWindowless(nullptr);
    window_info.shared_texture_enabled = runtime->gpu_rendering ? TRUE : FALSE;

    if (PostToCefUI([runtime, browser_state, client, window_info,
                       browser_settings, url = std::move(url)]() mutable {
            if (runtime->stopping.load() || runtime->gpu_suspended.load(std::memory_order_acquire)) {
                browser_state->creation_failed.store(true, std::memory_order_release);
                CompleteBrowserCreate(runtime);
                return;
            }
            if (!CefBrowserHost::CreateBrowser(
                    window_info, client, url, browser_settings, nullptr, nullptr)) {
                browser_state->creation_failed.store(true, std::memory_order_release);
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

void CefRuntime::PumpFrame(void* d3d_device, void* d3d_context)
{
    if (!config_.gpu_rendering || d3d_device == nullptr || d3d_context == nullptr ||
        !state_->initialized.load() || state_->stopping.load() || state_->gpu_suspended.load(std::memory_order_acquire)) {
        return;
    }

    auto* device = static_cast<ID3D11Device*>(d3d_device);
    auto* context = static_cast<ID3D11DeviceContext*>(d3d_context);
    bool changed{};
    bool shared_texture_ready{};
    std::string diagnostic_message;
    {
        const std::scoped_lock lock(state_->d3d_mutex);
        if (state_->d3d_device.Get() == device && state_->d3d_context.Get() == context) {
            return;
        }

        changed = true;
        ComPtr<ID3D11Device1> device1;
        ComPtr<ID3D11Multithread> multithread;
        const auto single_threaded =
            (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0;
        if (!single_threaded &&
            SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(device1.GetAddressOf()))) &&
            SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(multithread.GetAddressOf())))) {
            const auto was_protected = multithread->GetMultithreadProtected();
            if (!was_protected) {
                multithread->SetMultithreadProtected(TRUE);
                const auto already_recorded = std::any_of(
                    state_->protected_contexts.begin(), state_->protected_contexts.end(),
                    [&multithread](const ProtectedD3DContext& item) {
                        return item.multithread.Get() == multithread.Get();
                    });
                if (!already_recorded) {
                    state_->protected_contexts.push_back({ multithread, FALSE });
                }
            }
            shared_texture_ready = multithread->GetMultithreadProtected() != FALSE;
        }

        state_->d3d_device = device;
        state_->d3d_device1 = std::move(device1);
        state_->d3d_context = context;
        state_->shared_texture_ready = shared_texture_ready;
        ++state_->d3d_generation;
        if (shared_texture_ready && !state_->logged_shared_texture_ready) {
            state_->logged_shared_texture_ready = true;
            diagnostic_message = "CEF D3D11 shared-texture path is ready.";
        } else if (!shared_texture_ready && !state_->logged_shared_texture_unavailable) {
            state_->logged_shared_texture_unavailable = true;
            diagnostic_message = single_threaded ?
                "CEF shared textures are unavailable because Skyrim's D3D11 device is single-threaded." :
                "CEF shared textures are unavailable because D3D11.1 or multithread protection is missing.";
        }
    }

    if (!diagnostic_message.empty() && state_->log_info) {
        state_->log_info(diagnostic_message);
    }

    if (!changed) {
        return;
    }

    std::vector<std::shared_ptr<BrowserState>> browsers;
    {
        const std::scoped_lock lock(state_->mutex);
        for (const auto& weak_browser : state_->browsers) {
            if (auto browser = weak_browser.lock()) {
                browsers.push_back(std::move(browser));
            }
        }
    }
    for (const auto& browser : browsers) {
        const std::scoped_lock lock(browser->mutex);
        browser->gpu_frame.reset();
        browser->gpu_frame_pool = {};
        browser->next_gpu_pool_slot = 0;
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

void CefRuntime::PumpMessageLoop()
{
    if (!config_.multi_threaded_message_loop && state_->initialized.load() &&
        state_->application_thread == std::this_thread::get_id()) {
        CefDoMessageLoopWork();
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
        !std::filesystem::exists(config_.runtime_directory / L"resources.pak")) {
        error = "CEF .pak resources are missing beside libcef.dll.";
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
    settings.log_severity = LOGSEVERITY_INFO;
    const auto log_file = config_.log_file.empty() ?
        config_.cache_directory / L"SkyUltra_CEF.log" : config_.log_file;
    std::error_code filesystem_error;
    std::filesystem::create_directories(config_.cache_directory, filesystem_error);
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
    CefString(&settings.browser_subprocess_path) = config_.subprocess_executable.wstring();
    CefString(&settings.cache_path) = config_.cache_directory.wstring();
    CefString(&settings.log_file) = log_file.wstring();
    state_->app = new CefAppHost();

    const auto initialize_start = std::chrono::steady_clock::now();
    if (!CefInitialize(main_args, settings, state_->app, nullptr)) {
        error = "CefInitialize failed; inspect " + log_file.string() + ".";
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
    state_->application_thread = std::this_thread::get_id();
    state_->stopping.store(false);
    state_->gpu_suspended.store(false,std::memory_order_release);
    state_->initialized.store(true);

    // Initialize the first renderer process while the game is still loading.
    // This single transparent browser is donated to the real menu/HUD; it is
    // not an extra permanently running page or a pool that grows over time.
    ViewConfig spare_config;
    spare_config.size = { 64, 64 };
    spare_config.visible = false;
    spare_config.transparent = true;
    spare_config.enable_javascript = true;
    spare_config.max_fps = 1;
    auto spare_browser = std::make_shared<BrowserState>(state_, 0, spare_config);
    spare_browser->spare_browser = true;
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
    if (config_.multi_threaded_message_loop) {
        std::unique_lock lock(state_->mutex);
        state_->changed.wait(lock, browsers_closed);
    } else {
        for (;;) {
            {
                const std::scoped_lock lock(state_->mutex);
                if (browsers_closed()) {
                    break;
                }
            }
            if (std::this_thread::get_id() == state_->application_thread) {
                CefDoMessageLoopWork();
            }
            std::unique_lock lock(state_->mutex);
            state_->changed.wait_for(lock, std::chrono::milliseconds(1), browsers_closed);
        }
    }
    {
        const std::scoped_lock lock(state_->mutex);
        state_->spare_client = nullptr;
        state_->spare_browser.reset();
        state_->browsers.clear();
    }
    CefShutdown();
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
        state_->shared_texture_ready = false;
    }
    state_->app = nullptr;
    state_->initialized.store(false);
    state_->stopping.store(false);
    if (state_->cef_library != nullptr) {
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
        {
            const std::scoped_lock lock(browser_state->mutex);
            browser_state->id.store(id, std::memory_order_relaxed);
            browser_state->config = config;
            browser_state->requested_at = requested_at;
            browser_state->gpu_frame.reset();
            browser_state->frames.clear();
            browser_state->shadow.clear();
            browser_state->shadow_size = {};
            browser_state->logged_accelerated_frame = false;
            browser_state->logged_accelerated_paint.store(false, std::memory_order_relaxed);
            browser_state->logged_accelerated_failure = false;
            browser_state->logged_software_frame.store(false, std::memory_order_relaxed);
        }
        if (!PostToCefUI([browser_state, page_bridge_callback = std::move(page_bridge_callback),
                            url = std::string(initial_url)]() mutable {
                browser_state->spare_browser = false;
                browser_state->first_prime_paint_at = {};
                browser_state->app_shell_ready = false;
                browser_state->page_bridge_callback = std::move(page_bridge_callback);
                browser_state->main_frame_ready = false;
                browser_state->pending_bridge_events.clear();
                browser_state->pending_bridge_responses.clear();
                browser_state->adopted_initial_url = url;
                if (browser_state->browser && !browser_state->close_requested &&
                    !browser_state->runtime->gpu_suspended.load(std::memory_order_acquire)) {
                    auto host = browser_state->browser->GetHost();
                    bool visible{};
                    std::uint32_t max_fps{};
                    {
                        const std::scoped_lock lock(browser_state->mutex);
                        visible = browser_state->config.visible;
                        max_fps = browser_state->config.max_fps;
                    }
                    browser_state->prime_hidden_frame = !visible;
                    host->SetWindowlessFrameRate(visible ? CefWindowlessFrameRate(max_fps) :
                        std::min(30, CefWindowlessFrameRate(max_fps)));
                    host->WasHidden(false);
                    host->WasResized();
                    browser_state->adopted_initial_url.clear();
                    browser_state->browser->GetMainFrame()->LoadURL(url);
                    host->Invalidate(PET_VIEW);
                }
                if (browser_state->runtime->log_info) {
                    browser_state->runtime->log_info("CEF donated its startup renderer to view " +
                        std::to_string(browser_state->Id()) +
                        (browser_state->renderer_ready ? " (renderer already ready)." :
                            " (renderer still starting)."));
                }
            })) {
            return {};
        }
    } else if (!QueueBrowserCreation(state_, browser_state, client, std::string(initial_url))) {
        return {};
    }

    return std::make_unique<CefView>(std::move(browser_state), std::move(client));
}

}  // namespace skyultra
