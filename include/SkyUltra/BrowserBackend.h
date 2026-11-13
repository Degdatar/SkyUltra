#pragma once

#include "SkyUltra/Types.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skyultra {

enum InputModifier : std::uint32_t {
    kModifierShift = 1U << 0,
    kModifierControl = 1U << 1,
    kModifierAlt = 1U << 2,
    kModifierMeta = 1U << 3,
};

[[nodiscard]] std::string BuildBridgeEventScript(
    std::string_view event_name, std::string_view payload_json);
[[nodiscard]] std::string BuildBridgeResponseScript(
    std::string_view request_id, bool success, std::string_view payload_json,
    std::string_view error_message);

enum class PageBridgeMessageKind : std::uint8_t {
    Request,
    Cancel,
};

struct PageBridgeMessage {
    ViewId view_id{};
    PageBridgeMessageKind kind{ PageBridgeMessageKind::Request };
    std::string request_id;
    std::string name;
    std::string payload_json;
};

using PageBridgeMessageCallback = std::function<void(PageBridgeMessage)>;

struct PageBridgeResponse {
    std::string request_id;
    bool success{};
    std::string payload_json;
    std::string error_message;
};

[[nodiscard]] bool ParseBridgeConsoleMessage(
    ViewId view_id, std::string_view console_message, PageBridgeMessage& out_message);

enum class MouseButton : std::uint8_t {
    Left,
    Middle,
    Right,
};

enum class MouseButtonAction : std::uint8_t {
    None,
    Down,
    Up,
};

struct MouseInput {
    PointF position{};  // View-local logical coordinates.
    std::uint32_t modifiers{};
    MouseButton button{ MouseButton::Left };
    MouseButtonAction button_action{ MouseButtonAction::None };
    std::int32_t wheel_x{};
    std::int32_t wheel_y{};
    bool moved{};
    bool wheel{};
};

struct KeyInput {
    std::uint32_t virtual_key{};
    std::uint32_t scan_code{};
    std::uint32_t modifiers{};
    bool pressed{};
    bool repeat{};
    bool system_key{};
};

struct TextInput {
    std::u32string text;
    bool composition{};
    bool committed{};
};

struct RectU {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct PixelPatch {
    RectU rect;
    std::uint32_t stride{};
    std::vector<std::uint8_t> bgra8;
};

struct SoftwareFrame {
    SizeU size;
    std::uint64_t sequence{};
    std::vector<PixelPatch> patches;
};

// Native texture view for the current frame. A non-empty lifetime token keeps an owned
// backend texture alive while the overlay consumes the frame; otherwise the view is borrowed.
struct GpuFrame {
    void* shader_resource_view{};
    SizeU texture_size{};
    RectU source_rect{};
    std::shared_ptr<void> lifetime;
};

class IWebView {
public:
    virtual ~IWebView() = default;

    [[nodiscard]] virtual ViewId Id() const noexcept = 0;
    virtual void Navigate(std::string_view url) = 0;
    virtual void Resize(SizeU size) = 0;
    virtual void SetScale(float scale) = 0;
    virtual void SetMaxFPS(std::uint32_t fps) = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void SetFocused(InputMode mode) = 0;
    virtual void SendMouse(const MouseInput& input) = 0;
    virtual void SendKey(const KeyInput& input) = 0;
    virtual void SendText(const TextInput& input) = 0;
    [[nodiscard]] virtual bool SendEventJson(
        std::string_view event_name, std::string_view payload_json) = 0;
    [[nodiscard]] virtual bool ResolveRequest(
        std::string_view request_id, bool success, std::string_view payload_json,
        std::string_view error_message) = 0;
    [[nodiscard]] virtual std::optional<SoftwareFrame> TakeFrame() = 0;
    [[nodiscard]] virtual std::optional<GpuFrame> GetGpuFrame() { return std::nullopt; }
};

class IWebRuntime {
public:
    virtual ~IWebRuntime() = default;

    [[nodiscard]] virtual BrowserEngine Engine() const noexcept = 0;
    [[nodiscard]] virtual bool SupportsEngine(BrowserEngine engine) const noexcept
    {
        return Engine() == engine;
    }
    [[nodiscard]] virtual bool Initialize(std::string& error) = 0;
    virtual void Shutdown() noexcept = 0;
    // Called from Skyrim's serialized render-thread callback before compositing.
    // Backends may use this to finish work before the overlay is drawn.
    virtual void PumpFrame(void* d3d_device, void* d3d_context)
    {
        (void)d3d_device;
        (void)d3d_context;
    }
    // Called from the SKSE task thread for backends using an integrated loop.
    virtual void PumpMessageLoop() {}
    virtual void SetPageBridgeMessageCallback(PageBridgeMessageCallback callback)
    {
        (void)callback;
    }
    [[nodiscard]] virtual std::unique_ptr<IWebView> CreateView(
        ViewId id, const ViewConfig& config, std::string_view initial_url) = 0;
};

}  // namespace skyultra
