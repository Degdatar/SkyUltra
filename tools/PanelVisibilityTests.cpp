// MSVC /Zs evaluates these production-policy scenarios without building a binary.
#include "SkyUltra/PanelVisibility.h"
#include <array>

namespace {
using Action = skyultra::PanelVisibilityAction;
using Panel = skyultra::PanelVisibilityState;
constexpr bool IndependentPanelHotkeys()
{
    std::array panels{
        Panel{1, false, true},  // SkyUltra
        Panel{2, true, true},   // SkyOS
        Panel{3, false, true},  // Another mod
        Panel{4, true, false},  // HUD
        Panel{5, false, false}, // Hidden pinned widget
    };
    const auto apply = [&](Action action, std::span<const skyultra::ViewId> selected = {}) {
        for (auto& panel : panels) panel.visible = skyultra::ResolvePanelVisibility(action, panel, selected);
    };
    // Escape closes SkyOS; the framework hotkey must select SkyUltra afterward.
    apply(Action::Hide);
    const std::array<skyultra::ViewId, 1> framework{1};
    apply(Action::Select, framework);
    if (!panels[0].visible || panels[1].visible || panels[2].visible) return false;
    // A different mod's hotkey selects that mod alone, even while SkyUltra is open.
    const std::array<skyultra::ViewId, 1> other_mod{3};
    apply(Action::Select, other_mod);
    if (panels[0].visible || panels[1].visible || !panels[2].visible) return false;
    // Skyrim modal menus restore precisely the prior selection.
    for (auto& panel : panels) panel.suspended = panel.managed && panel.visible;
    apply(Action::Hide);
    apply(Action::Restore);
    if (panels[0].visible || panels[1].visible || !panels[2].visible) return false;
    // Repeated menu/mod switches retain each HUD's independent visibility.
    apply(Action::Select, framework);
    const std::array<skyultra::ViewId, 1> skyos{2};
    apply(Action::Select, skyos);
    apply(Action::Hide);
    apply(Action::Select, framework);
    return panels[0].visible && !panels[1].visible && !panels[2].visible &&
        panels[3].visible && !panels[4].visible;
}
static_assert(IndependentPanelHotkeys());
static_assert(!skyultra::ResolvePanelVisibility(Action::Restore, Panel{6, false, true, false}));
} // namespace
