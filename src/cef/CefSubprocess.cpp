#include "SkyUltra/BridgeBatchScript.h"

#include "include/cef_app.h"
#include "include/cef_process_message.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_v8.h"
#include "include/cef_crash_util.h"

#include <windows.h>

#include <string>

namespace {

constexpr char kEditableFocusMessage[] = "skyultra_editable_focus";
constexpr char kBridgeBatchAckMessage[] = "skyultra_bridge_batch_ack";
constexpr char kBridgeBatchDeliverMessage[] = "skyultra_bridge_deliver";
constexpr char kRendererBridgeReadyMessage[] = "skyultra_renderer_bridge_ready";
constexpr char kPageBridgeMessage[] = "skyultra_page_bridge";
constexpr char kContextLossMessage[] = "skyultra_webgl_context_lost";
constexpr char kDispatcherGlobalName[] = "__skyultraDispatchBatch";

class ContextLossReporter final : public CefV8Handler {
public:
    bool Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList&,
        CefRefPtr<CefV8Value>& retval, CefString&) override {
        const auto context = CefV8Context::GetCurrentContext();
        const auto frame = context ? context->GetFrame() : nullptr;
        if (!frame || !frame->IsMain()) return false;
        frame->SendProcessMessage(PID_BROWSER, CefProcessMessage::Create(kContextLossMessage));
        retval = CefV8Value::CreateBool(true);
        return true;
    }
    IMPLEMENT_REFCOUNTING(ContextLossReporter);
};

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

// Direct renderer -> browser IPC bridge avoids console.debug stack-capture overhead.
class PageBridgeSender final : public CefV8Handler {
public:
    bool Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval, CefString&) override
    {
        if (arguments.size() != 1 || !arguments[0]->IsString()) return false;
        const auto payload = arguments[0]->GetStringValue();
        if (payload.empty() || payload.length() > 65536) return false;
        const auto context = CefV8Context::GetCurrentContext();
        const auto frame = context ? context->GetFrame() : nullptr;
        if (!frame || !frame->IsMain()) return false;
        const auto message = CefProcessMessage::Create(kPageBridgeMessage);
        if (!message || !message->GetArgumentList()->SetString(0, payload)) return false;
        frame->SendProcessMessage(PID_BROWSER, message);
        retval = CefV8Value::CreateBool(true);
        return true;
    }
    IMPLEMENT_REFCOUNTING(PageBridgeSender);
};

class SubprocessApp final : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine>) override {
        if (CefCrashReportingEnabled()) CefSetCrashKeyValue("skyultra-process", process_type);
    }

    void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override
    {
        if (!frame || !frame->IsMain() || !context) return;
        if (CefCrashReportingEnabled()) {
            CefSetCrashKeyValue("skyultra-view", std::to_string(browser->GetIdentifier()));
            CefSetCrashKeyValue("skyultra-operation", "document-created");
        }
        const auto attrs = static_cast<cef_v8_propertyattribute_t>(
            V8_PROPERTY_ATTRIBUTE_READONLY | V8_PROPERTY_ATTRIBUTE_DONTENUM | V8_PROPERTY_ATTRIBUTE_DONTDELETE);
        auto global = context->GetGlobal();
        global->SetValue("__skyultraReportContextLoss",
            CefV8Value::CreateFunction("__skyultraReportContextLoss", new ContextLossReporter()), attrs);
        CefRefPtr<CefV8Value> result;
        CefRefPtr<CefV8Exception> exception;
        (void)context->Eval("document.addEventListener('webglcontextlost',function(){window.__skyultraReportContextLoss();},true);",
            "skyultra://context-loss-observer", 1, result, exception);
        global->SetValue("__skyultraAcknowledgeBatch",
            CefV8Value::CreateFunction("__skyultraAcknowledgeBatch", new BatchAcknowledgement()), attrs);
        global->SetValue("__skyultraSendBridge",
            CefV8Value::CreateFunction("__skyultraSendBridge", new PageBridgeSender()), attrs);

        CefRefPtr<CefV8Value> dispatcher;
        CefRefPtr<CefV8Exception> dispatch_exception;
        const std::string dispatcher_source = "(" + std::string(skyultra::kBridgeBatchDispatcher) + ")";
        if (context->Eval(dispatcher_source, "skyultra://bridge-dispatcher", 1, dispatcher, dispatch_exception) &&
            dispatcher && dispatcher->IsFunction()) {
            global->SetValue(kDispatcherGlobalName, dispatcher, attrs);
            frame->SendProcessMessage(PID_BROWSER, CefProcessMessage::Create(kRendererBridgeReadyMessage));
        }
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
        CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override
    {
        if (source_process != PID_BROWSER || !message || message->GetName().ToString() != kBridgeBatchDeliverMessage) {
            return false;
        }
        if (!frame || !frame->IsMain()) return true;
        const auto arguments = message->GetArgumentList();
        if (!arguments || arguments->GetSize() != 3 ||
            arguments->GetType(0) != VTYPE_BOOL ||
            arguments->GetType(1) != VTYPE_STRING ||
            arguments->GetType(2) != VTYPE_LIST) {
            return true;
        }
        auto context = frame->GetV8Context();
        if (!context || !context->Enter()) return true;
        const bool responses = arguments->GetBool(0);
        const auto ack = arguments->GetString(1);
        const auto pairs = arguments->GetList(2);
        const auto pair_count = pairs ? pairs->GetSize() / 2 : 0;
        const auto global = context->GetGlobal();
        const auto dispatcher = global ? global->GetValue(kDispatcherGlobalName) : nullptr;
        if (dispatcher && dispatcher->IsFunction()) {
            auto events_array = CefV8Value::CreateArray(static_cast<int>(pair_count));
            for (std::size_t i = 0; i < pair_count; ++i) {
                auto entry = CefV8Value::CreateArray(2);
                entry->SetValue(0, CefV8Value::CreateString(pairs->GetString(i * 2)));
                entry->SetValue(1, CefV8Value::CreateString(pairs->GetString(i * 2 + 1)));
                events_array->SetValue(static_cast<int>(i), entry);
            }
            CefV8ValueList call_args{
                events_array,
                CefV8Value::CreateString(ack),
                CefV8Value::CreateBool(responses)
            };
            (void)dispatcher->ExecuteFunction(nullptr, call_args);
        }
        context->Exit();
        return true;
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
