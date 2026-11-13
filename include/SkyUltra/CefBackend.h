#pragma once

#include "SkyUltra/BrowserBackend.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>

namespace skyultra {

struct CefRuntimeConfig {
    std::filesystem::path runtime_directory;
    std::filesystem::path subprocess_executable;
    std::filesystem::path cache_directory;
    std::filesystem::path log_file;
    bool gpu_rendering{ true };
    bool multi_threaded_message_loop{ true };
    std::function<void(std::string_view)> log_info;
};

class CefRuntime final : public IWebRuntime {
public:
    explicit CefRuntime(CefRuntimeConfig config);
    ~CefRuntime() override;

    CefRuntime(const CefRuntime&) = delete;
    CefRuntime& operator=(const CefRuntime&) = delete;

    [[nodiscard]] BrowserEngine Engine() const noexcept override;
    [[nodiscard]] bool Initialize(std::string& error) override;
    void Shutdown() noexcept override;
    void PumpFrame(void* d3d_device, void* d3d_context) override;
    void PumpMessageLoop() override;
    void SetPageBridgeMessageCallback(PageBridgeMessageCallback callback) override;
    [[nodiscard]] std::unique_ptr<IWebView> CreateView(
        ViewId id, const ViewConfig& config, std::string_view initial_url) override;

private:
    struct State;
    CefRuntimeConfig config_;
    std::shared_ptr<State> state_;
};

}  // namespace skyultra
