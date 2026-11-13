#include "SkyUltra/BrowserBackend.h"
#include "SkyUltra/BridgeBatchScript.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>

namespace skyultra {
namespace {

std::string QuoteJavaScriptString(std::string_view value)
{
    constexpr std::array<char, 16> kHexDigits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto byte = static_cast<unsigned char>(value[i]);
        if (byte == 0xE2 && i + 2 < value.size() &&
            static_cast<unsigned char>(value[i + 1]) == 0x80) {
            const auto final_byte = static_cast<unsigned char>(value[i + 2]);
            if (final_byte == 0xA8 || final_byte == 0xA9) {
                result += final_byte == 0xA8 ? "\\u2028" : "\\u2029";
                i += 2;
                continue;
            }
        }
        switch (byte) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (byte < 0x20) {
                result += "\\u00";
                result.push_back(kHexDigits[byte >> 4]);
                result.push_back(kHexDigits[byte & 0x0F]);
            } else {
                result.push_back(static_cast<char>(byte));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

}  // namespace

std::string BuildBridgeEventScript(std::string_view event_name, std::string_view payload_json)
{
    std::string script = "(()=>{const detail={name:";
    script += QuoteJavaScriptString(event_name);
    script += ",data:JSON.parse(";
    script += QuoteJavaScriptString(payload_json);
    script += ")};window.dispatchEvent(new CustomEvent(\"skyultra:message\",{detail}));})();";
    return script;
}

std::string BuildBridgeEventBatchScript(
    const BridgeEventQueue::Batch& batch, std::string_view acknowledgement, bool responses)
{
    std::string script = "(" + std::string(kBridgeBatchDispatcher) + ")([";
    bool first = true;
    for (const auto& event : batch.events) {
        if (!first) script += ',';
        first = false;
        script += '[' + QuoteJavaScriptString(event.name) + ',' + QuoteJavaScriptString(event.payload_json) + ']';
    }
    script += "]," + QuoteJavaScriptString(acknowledgement) + (responses ? ",true);" : ",false);");
    return script;
}

std::string BuildBridgeResponseJson(std::string_view id, bool success,
    std::string_view payload, std::string_view error)
{
    return "{\"id\":" + QuoteJavaScriptString(id) + (success ? ",\"ok\":true,\"data\":" +
        std::string(payload.empty() ? "null" : payload) : ",\"ok\":false,\"error\":" + QuoteJavaScriptString(error)) + "}";
}

std::string BuildBridgeResponseScript(
    std::string_view request_id, bool success, std::string_view payload_json,
    std::string_view error_message)
{
    std::string script = "(()=>{const detail={id:";
    script += QuoteJavaScriptString(request_id);
    script += success ? ",ok:true,data:JSON.parse(" : ",ok:false,error:";
    if (success) {
        script += QuoteJavaScriptString(payload_json);
        script += ")}";
    } else {
        script += QuoteJavaScriptString(error_message);
        script += "}";
    }
    script += ";window.dispatchEvent(new CustomEvent(\"skyultra:response\",{detail}));})();";
    return script;
}

bool ParseBridgeConsoleMessage(
    ViewId view_id, std::string_view console_message, PageBridgeMessage& out_message)
{
    constexpr std::string_view kPrefix = "__SKYULTRA_BRIDGE__";
    constexpr std::size_t kMaxMessageBytes = 65536;
    constexpr std::size_t kMaxRequestIdBytes = 128;
    constexpr std::size_t kMaxNameBytes = 128;
    if (view_id == 0 || !console_message.starts_with(kPrefix) ||
        console_message.size() > kMaxMessageBytes + kPrefix.size()) {
        return false;
    }

    const auto body = console_message.substr(kPrefix.size());
    auto message = nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
    if (message.is_discarded() || !message.is_object()) {
        return false;
    }

    const auto kind_it = message.find("kind");
    const auto id_it = message.find("id");
    if (kind_it == message.end() || !kind_it->is_string() ||
        id_it == message.end() || !id_it->is_string()) {
        return false;
    }
    const auto kind = kind_it->get<std::string>();
    auto request_id = id_it->get<std::string>();
    if (request_id.empty() || request_id.size() > kMaxRequestIdBytes ||
        request_id.find('\0') != std::string::npos) {
        return false;
    }

    PageBridgeMessage parsed;
    parsed.view_id = view_id;
    parsed.request_id = std::move(request_id);
    if (kind == "cancel") {
        parsed.kind = PageBridgeMessageKind::Cancel;
        out_message = std::move(parsed);
        return true;
    }
    if (kind != "request") {
        return false;
    }

    const auto name_it = message.find("name");
    if (name_it == message.end() || !name_it->is_string()) {
        return false;
    }
    parsed.name = name_it->get<std::string>();
    if (parsed.name.empty() || parsed.name.size() > kMaxNameBytes ||
        parsed.name.find('\0') != std::string::npos) {
        return false;
    }

    const auto params_it = message.find("params");
    try {
        parsed.payload_json = params_it == message.end() ? "null" : params_it->dump();
    } catch (...) {
        return false;
    }
    if (parsed.payload_json.empty() || parsed.payload_json.size() > kMaxMessageBytes) {
        return false;
    }

    parsed.kind = PageBridgeMessageKind::Request;
    out_message = std::move(parsed);
    return true;
}

}  // namespace skyultra
