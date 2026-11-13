#include "include/cef_app.h"
#include "include/cef_process_message.h"
#include "include/cef_render_process_handler.h"

#include <windows.h>

namespace {

constexpr char kEditableFocusMessage[] = "skyultra_editable_focus";

class SubprocessApp final : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

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
