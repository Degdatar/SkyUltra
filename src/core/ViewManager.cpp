#include "SkyUltra/ViewManager.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

namespace skyultra {
namespace {

bool IsFiniteRect(const RectF& rect) noexcept
{
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
        std::isfinite(rect.width) && std::isfinite(rect.height);
}

}  // namespace

RectF ResolveViewBounds(const ViewConfig& config, SizeU viewport) noexcept
{
    auto bounds = config.bounds;
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
        const auto scale = std::isfinite(config.scale) && config.scale > 0.0F ? config.scale : 1.0F;
        bounds.width = static_cast<float>(config.size.width) / scale;
        bounds.height = static_cast<float>(config.size.height) / scale;
    }

    const auto viewport_width = static_cast<float>(viewport.width);
    const auto viewport_height = static_cast<float>(viewport.height);
    switch (config.anchor) {
    case Anchor::TopLeft:
        break;
    case Anchor::TopRight:
        bounds.x = viewport_width - bounds.width - bounds.x;
        break;
    case Anchor::BottomLeft:
        bounds.y = viewport_height - bounds.height - bounds.y;
        break;
    case Anchor::BottomRight:
        bounds.x = viewport_width - bounds.width - bounds.x;
        bounds.y = viewport_height - bounds.height - bounds.y;
        break;
    case Anchor::Center:
        bounds.x = (viewport_width - bounds.width) * 0.5F + bounds.x;
        bounds.y = (viewport_height - bounds.height) * 0.5F + bounds.y;
        break;
    }
    return bounds;
}

ViewId ViewManager::Add(ViewConfig config, const ViewFactory& create_view)
{
    if (!create_view || config.id.empty() || config.size.width == 0 || config.size.height == 0 ||
        !std::isfinite(config.opacity) || !std::isfinite(config.scale) || config.scale <= 0.0F ||
        !IsFiniteRect(config.bounds) || !IsFiniteRect(config.clip)) {
        return 0;
    }

    if (std::ranges::any_of(views_, [&config](const auto& item) {
            return item.second.config.id == config.id;
        })) {
        return 0;
    }

    config.opacity = std::clamp(config.opacity, 0.0F, 1.0F);
    config.scale = std::clamp(config.scale, 0.25F, 4.0F);
    if (config.max_fps != 0) {
        config.max_fps = std::clamp(config.max_fps, 1U,
            config.browser_engine == BrowserEngine::CEF ? 120U : 240U);
    }
    if (config.bounds.width <= 0.0F || config.bounds.height <= 0.0F) {
        config.bounds.width = static_cast<float>(config.size.width) / config.scale;
        config.bounds.height = static_cast<float>(config.size.height) / config.scale;
    }
    const auto id = next_id_++;
    auto view = create_view(id);
    if (!view || view->Id() != id) {
        return 0;
    }
    std::shared_ptr<IWebView> shared_view(std::move(view));
    shared_view->Resize(config.size);
    shared_view->SetScale(config.scale);
    shared_view->SetMaxFPS(config.max_fps);
    shared_view->SetVisible(config.visible);
    views_.emplace(id, Entry{ std::move(config), std::move(shared_view), next_creation_order_++ });
    return id;
}

bool ViewManager::Remove(ViewId id)
{
    if (!views_.contains(id)) {
        return false;
    }
    ReleaseFocus(id);
    views_.erase(id);
    return true;
}

bool ViewManager::UpdateConfig(ViewId id, const ViewConfig& config)
{
    const auto it = views_.find(id);
    if (it == views_.end() || config.size.width == 0 || config.size.height == 0 ||
        !std::isfinite(config.opacity) || !std::isfinite(config.scale) || config.scale <= 0.0F ||
        !IsFiniteRect(config.bounds) || !IsFiniteRect(config.clip)) {
        return false;
    }
    if (config.id != it->second.config.id) {
        return false;
    }

    if (std::ranges::any_of(views_, [&config, id](const auto& item) {
            return item.first != id && item.second.config.id == config.id;
        })) {
        return false;
    }

    auto updated = config;
    updated.opacity = std::clamp(updated.opacity, 0.0F, 1.0F);
    updated.scale = std::clamp(updated.scale, 0.25F, 4.0F);
    if (updated.max_fps != 0) {
        updated.max_fps = std::clamp(updated.max_fps, 1U,
            updated.browser_engine == BrowserEngine::CEF ? 120U : 240U);
    }
    if (updated.bounds.width <= 0.0F || updated.bounds.height <= 0.0F) {
        updated.bounds.width = static_cast<float>(updated.size.width) / updated.scale;
        updated.bounds.height = static_cast<float>(updated.size.height) / updated.scale;
    }

    if (it->second.config.size.width != updated.size.width ||
        it->second.config.size.height != updated.size.height) {
        it->second.view->Resize(updated.size);
    }
    if (it->second.config.scale != updated.scale) {
        it->second.view->SetScale(updated.scale);
    }
    if (it->second.config.max_fps != updated.max_fps) {
        it->second.view->SetMaxFPS(updated.max_fps);
    }
    if (it->second.config.visible != updated.visible) {
        it->second.view->SetVisible(updated.visible);
        (void)it->second.view->SendEventJson("skyultra.visibility",updated.visible ? R"({"visible":true})" : R"({"visible":false})");
    }

    it->second.config = std::move(updated);
    if (!it->second.config.visible) {
        ReleaseFocus(id);
    }
    return true;
}

bool ViewManager::SetVisible(ViewId id, bool visible)
{
    const auto it = views_.find(id);
    if (it == views_.end()) {
        return false;
    }
    const bool changed = it->second.config.visible != visible;
    it->second.config.visible = visible;
    it->second.view->SetVisible(visible);
    if (changed) (void)it->second.view->SendEventJson("skyultra.visibility",visible ? R"({"visible":true})" : R"({"visible":false})");
    if (!visible) {
        ReleaseFocus(id);
    }
    return true;
}

bool ViewManager::RequestFocus(ViewId id, InputMode mode)
{
    const auto it = views_.find(id);
    if (it == views_.end() || !it->second.config.visible || mode == InputMode::None) {
        return false;
    }

    if (const auto current = FocusOwner()) {
        if (auto* view = Find(*current)) {
            view->SetFocused(InputMode::None);
        }
    }
    ReleaseFocus(id);
    focus_stack_.emplace_back(id, mode);
    it->second.view->SetFocused(mode);
    return true;
}

void ViewManager::ReleaseFocus(ViewId id)
{
    const auto owned_focus = !focus_stack_.empty() && focus_stack_.back().first == id;
    if (owned_focus) {
        if (auto* view = Find(id)) {
            view->SetFocused(InputMode::None);
        }
    }
    std::erase_if(focus_stack_, [id](const auto& entry) { return entry.first == id; });

    if (owned_focus) {
        if (const auto owner = FocusOwner()) {
            if (auto* view = Find(*owner)) {
                const auto it = std::find_if(focus_stack_.rbegin(), focus_stack_.rend(),
                    [owner](const auto& entry) { return entry.first == *owner; });
                view->SetFocused(it == focus_stack_.rend() ? InputMode::None : it->second);
            }
        }
    }
}

HitTestResult ViewManager::HitTest(PointF screen_position, SizeU viewport) const
{
    const auto ordered = OrderedViews();
    const auto modal = std::find_if(ordered.rbegin(), ordered.rend(), [](const ViewSnapshot& view) {
        return view.config.visible && view.config.modal;
    });
    const bool has_modal = modal != ordered.rend();
    const auto first = ordered.rbegin();

    for (auto it = first; it != ordered.rend(); ++it) {
        const auto& entry = *it;
        if (has_modal && (entry.config.layer < modal->config.layer ||
            (entry.config.layer == modal->config.layer &&
             (entry.config.z_index < modal->config.z_index ||
              (entry.config.z_index == modal->config.z_index &&
               entry.creation_order < modal->creation_order))))) {
            break;
        }
        if (entry.config.visible && !entry.config.click_through &&
            Contains(entry.config, screen_position, viewport)) {
            return { entry.id, true };
        }
    }

    return { std::nullopt, has_modal };
}

std::optional<ViewId> ViewManager::FocusOwner() const noexcept
{
    if (focus_stack_.empty()) {
        return std::nullopt;
    }
    return focus_stack_.back().first;
}

std::optional<InputMode> ViewManager::FocusMode(ViewId id) const noexcept
{
    const auto it = std::find_if(focus_stack_.rbegin(), focus_stack_.rend(),
        [id](const auto& entry) { return entry.first == id; });
    return it == focus_stack_.rend() ? std::nullopt : std::optional<InputMode>(it->second);
}

bool ViewManager::HasVisibleModalView() const noexcept
{
    return std::ranges::any_of(views_, [](const auto& item) {
        return item.second.config.visible && item.second.config.modal;
    });
}

const ViewConfig* ViewManager::Config(ViewId id) const noexcept
{
    const auto it = views_.find(id);
    return it == views_.end() ? nullptr : &it->second.config;
}

IWebView* ViewManager::Find(ViewId id) noexcept
{
    const auto it = views_.find(id);
    return it == views_.end() ? nullptr : it->second.view.get();
}

const IWebView* ViewManager::Find(ViewId id) const noexcept
{
    const auto it = views_.find(id);
    return it == views_.end() ? nullptr : it->second.view.get();
}

std::vector<ViewSnapshot> ViewManager::OrderedViews() const
{
    std::vector<ViewSnapshot> ordered;
    ordered.reserve(views_.size());
    for (const auto& [id, entry] : views_) {
        ordered.push_back({ id, entry.config, entry.creation_order, entry.view });
    }
    std::sort(ordered.begin(), ordered.end(), [](const ViewSnapshot& lhs, const ViewSnapshot& rhs) {
        if (lhs.config.layer != rhs.config.layer) {
            return lhs.config.layer < rhs.config.layer;
        }
        if (lhs.config.z_index != rhs.config.z_index) {
            return lhs.config.z_index < rhs.config.z_index;
        }
        return lhs.creation_order < rhs.creation_order;
    });
    return ordered;
}

bool ViewManager::Contains(const ViewConfig& config, PointF point, SizeU viewport) noexcept
{
    const auto bounds = ResolveViewBounds(config, viewport);
    if (!bounds.Contains(point)) {
        return false;
    }
    return config.clip.width <= 0.0F || config.clip.height <= 0.0F || config.clip.Contains(point);
}

}  // namespace skyultra
