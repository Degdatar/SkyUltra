#pragma once
#include <string_view>

namespace skyultra {
// Shared by the two engines; the regression check executes this exact function.
inline constexpr std::string_view kBridgeBatchDispatcher = R"JS((events, acknowledgement, responses) => {
  try {
    for (const [name, json] of events) {
      try {
        const data = JSON.parse(json);
        window.dispatchEvent(new CustomEvent(responses ? "skyultra:response" : "skyultra:message", {
          detail: responses ? data : { name, data }
        }));
      } catch (error) {
        try { console.error("SkyUltra event failed", name, error); } catch (_) {}
      }
    }
  } finally {
    if (acknowledgement) {
      let sent = false;
      try {
        if (typeof window.__skyultraAcknowledgeBatch === "function") {
          sent = window.__skyultraAcknowledgeBatch(acknowledgement) === true;
        }
      } catch (_) {}
      if (!sent) console.log(acknowledgement);
    }
  }
})JS";
} // namespace skyultra
