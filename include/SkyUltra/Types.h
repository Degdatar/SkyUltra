#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace skyultra {

using ViewId = std::uint64_t;

enum class BrowserEngine : std::uint8_t {
    CEF,
    Ultralight,
};

struct PointF {
    float x{};
    float y{};
};

struct SizeU {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct RectF {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] bool Contains(PointF point) const noexcept
    {
        return point.x >= x && point.y >= y && point.x < x + width && point.y < y + height;
    }
};

// Present only on views created from a registered V3 mod manifest. Legacy
// views intentionally keep their historical unrestricted browser behavior.
struct ModContentPolicy {
    std::filesystem::path root_directory;
    std::filesystem::path entry_file;
    bool asset_access_enabled{ true };
    std::vector<std::string> network_hosts;
};

enum class Anchor : std::uint8_t {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
};

enum class ViewLayer : std::uint8_t {
    Hud,
    Panel,
    Popup,
    System,
};

enum class InputMode : std::uint8_t {
    None,
    Mouse,
    Keyboard,
    MouseAndKeyboard,
    Gamepad,
    All,
};

struct ViewConfig {
    std::string id;
    std::optional<ModContentPolicy> mod_content_policy;
    BrowserEngine browser_engine{ BrowserEngine::Ultralight };
    SizeU size{ 1280, 720 };
    RectF bounds{};  // Viewport-relative position and rendered size; x/y are offsets from anchor.
    RectF clip{};    // Optional viewport-relative clip rectangle; non-positive size disables clipping.
    Anchor anchor{ Anchor::TopLeft };
    ViewLayer layer{ ViewLayer::Panel };
    float opacity{ 1.0F };
    float scale{ 1.0F };
    std::int32_t z_index{};
    // Zero follows the browser engine's fastest supported frame rate.
    std::uint32_t max_fps{};
    bool visible{ true };
    bool transparent{ true };
    bool click_through{};
    bool modal{};
    bool enable_javascript{ true };
    bool enable_gamepad{};
};

}  // namespace skyultra
