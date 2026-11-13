#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace skyultra {
inline nlohmann::json DefaultRuntimeSettings()
{
    return {{"mainWidth", 0}, {"mainHeight", 0}, {"popupWidth", 420}, {"popupHeight", 240},
        {"renderMode", "gpu"}, {"cefMultithreaded", true}, {"hotReload", false}};
}

inline bool ParseRuntimeSettings(const nlohmann::json& input, const nlohmann::json& current,
    nlohmann::json& output, std::string& error)
{
    if (!input.is_object()) { error = "Expected a runtime settings object."; return false; }
    output = current;
    const auto defaults = DefaultRuntimeSettings();
    for (auto field = input.begin(); field != input.end(); ++field) {
        const auto& key = field.key();
        if (!defaults.contains(key)) { error = "Unknown runtime setting: " + key; return false; }
        if (key == "mainWidth" || key == "mainHeight" || key == "popupWidth" || key == "popupHeight") {
            const auto minimum = key.starts_with("popup") ? 128 : 0;
            if (!field->is_number_integer() || *field < minimum || *field > 4096) {
                error = "Runtime dimensions are outside the supported range."; return false;
            }
        } else if (key == "renderMode") {
            if (!field->is_string() || (*field != "gpu" && *field != "cpu")) {
                error = "Runtime renderMode must be gpu or cpu."; return false;
            }
        } else if (!field->is_boolean()) {
            error = "Runtime " + key + " must be boolean."; return false;
        }
        output[key] = *field;
    }
    return true;
}
} // namespace skyultra
