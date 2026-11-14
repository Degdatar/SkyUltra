#pragma once

#include "SkyUltra/Types.h"
#include <algorithm>
#include <span>

namespace skyultra {

enum class PanelVisibilityAction { Select, Hide, Restore };

struct PanelVisibilityState {
    ViewId id{};
    bool visible{};
    bool managed{}; // HUDs and pinned widgets retain their own visibility.
    bool suspended{}; // Saved only for a temporary Skyrim modal menu.
};

[[nodiscard]] constexpr bool ResolvePanelVisibility(PanelVisibilityAction action,
    const PanelVisibilityState& panel, std::span<const ViewId> selected = {}) noexcept
{
    if (!panel.managed) return panel.visible;
    if (action == PanelVisibilityAction::Hide) return false;
    if (action == PanelVisibilityAction::Restore) return panel.suspended;
    return std::ranges::find(selected, panel.id) != selected.end();
}

} // namespace skyultra
