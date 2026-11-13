#include "include/cef_app.h"
#include "include/cef_process_message.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_v8.h"

#include <windows.h>

namespace {

constexpr char kEditableFocusMessage[] = "skyultra_editable_focus";
constexpr char kBridgeBatchAckMessage[] = "skyultra_bridge_batch_ack";

// Host flow control must continue even if an application replaces console.log.
class BatchAcknowledgement final : public CefV8Handler {
public:
    bool Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval, CefString&) override
    {
        if (arguments.size() != 1 || !arguments[0]->IsString()) return false;
        const auto token = arguments[0]->GetStringValue();
        if (token.length() > 128) return false;
        const auto context = CefV8Context::GetCurrentContext();
        const auto frame = context ? context->GetFrame() : nullptr;
        if (!frame || !frame->IsMain()) return false;
        const auto message = CefProcessMessage::Create(kBridgeBatchAckMessage);
        if (!message || !message->GetArgumentList()->SetString(0, token)) return false;
        frame->SendProcessMessage(PID_BROWSER, message);
        retval = CefV8Value::CreateBool(true);
        return true;
    }
    IMPLEMENT_REFCOUNTING(BatchAcknowledgement);
};

class SubprocessApp final : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override
    {
        if (!frame || !frame->IsMain() || !context) return;
        context->GetGlobal()->SetValue("__skyultraAcknowledgeBatch",
            CefV8Value::CreateFunction("__skyultraAcknowledgeBatch", new BatchAcknowledgement()),
            static_cast<cef_v8_propertyattribute_t>(V8_PROPERTY_ATTRIBUTE_READONLY |
                V8_PROPERTY_ATTRIBUTE_DONTENUM | V8_PROPERTY_ATTRIBUTE_DONTDELETE));
    }

    void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        CefRefPtr<CefDOMNode> node) override
    {
        if (!browser) {
            return;
        }
        auto target_frame = frame ? frame : browser->GetMainFrame();
        if (!target_frame) {
            return;
        }

        auto message = CefProcessMessage::Create(kEditableFocusMessage);
        const auto arguments = message ? message->GetArgumentList() : nullptr;
        if (!arguments || !arguments->SetSize(1)) {
            return;
        }
        if (!arguments->SetBool(0, node && node->IsEditable())) {
            return;
        }
        target_frame->SendProcessMessage(PID_BROWSER, message);
    }

    IMPLEMENT_REFCOUNTING(SubprocessApp);
};

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int)
{
    CefMainArgs main_args(instance);
    CefRefPtr<SubprocessApp> app = new SubprocessApp();
    return CefExecuteProcess(main_args, app, nullptr);
}
