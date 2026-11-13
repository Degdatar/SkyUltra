#pragma once

#include "SkyUltra/BrowserBackend.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace skyultra {

struct ViewSnapshot {
    ViewId id{};
    ViewConfig config;
    std::uint64_t creation_order{};
    std::shared_ptr<IWebView> view;
};

struct HitTestResult {
    std::optional<ViewId> target;
    bool block_game_input{};
};

[[nodiscard]] RectF ResolveViewBounds(const ViewConfig& config, SizeU viewport) noexcept;

class ViewManager final {
public:
    using ViewFactory = std::function<std::unique_ptr<IWebView>(ViewId)>;

    [[nodiscard]] ViewId Add(ViewConfig config, const ViewFactory& create_view);
    [[nodiscard]] bool Remove(ViewId id);
    [[nodiscard]] bool UpdateConfig(ViewId id, const ViewConfig& config);
    [[nodiscard]] bool SetVisible(ViewId id, bool visible);
    [[nodiscard]] bool RequestFocus(ViewId id, InputMode mode);
    void ReleaseFocus(ViewId id);

    [[nodiscard]] HitTestResult HitTest(PointF screen_position, SizeU viewport) const;
    [[nodiscard]] std::optional<ViewId> FocusOwner() const noexcept;
    [[nodiscard]] std::optional<InputMode> FocusMode(ViewId id) const noexcept;
    [[nodiscard]] bool HasVisibleModalView() const noexcept;
    [[nodiscard]] const ViewConfig* Config(ViewId id) const noexcept;
    [[nodiscard]] IWebView* Find(ViewId id) noexcept;
    [[nodiscard]] const IWebView* Find(ViewId id) const noexcept;
    [[nodiscard]] std::vector<ViewSnapshot> OrderedViews() const;

private:
    struct Entry {
        ViewConfig config;
        std::shared_ptr<IWebView> view;
        std::uint64_t creation_order{};
    };

    [[nodiscard]] static bool Contains(
        const ViewConfig& config, PointF point, SizeU viewport) noexcept;

    std::unordered_map<ViewId, Entry> views_;
    std::vector<std::pair<ViewId, InputMode>> focus_stack_;
    ViewId next_id_{ 1 };
    std::uint64_t next_creation_order_{ 1 };
};

}  // namespace skyultra
