#pragma once
#include <cstdint>

namespace skyultra {
// Three bounded reload attempts in a 60-second recovery window. A page that keeps
// crashing is left stopped so it cannot reload-loop all other browser views.
struct CefRecoveryPolicy {
    std::uint64_t window_start{};
    unsigned attempts{};
    bool started{};
    [[nodiscard]] constexpr unsigned NextDelayMs(std::uint64_t now) noexcept {
        if (!started || now - window_start >= 60'000) { started = true; window_start = now; attempts = 0; }
        if (attempts >= 3) return 0;
        constexpr unsigned delays[]{250, 1000, 4000};
        return delays[attempts++];
    }
};
} // namespace skyultra
