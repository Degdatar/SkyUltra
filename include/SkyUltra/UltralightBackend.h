#pragma once

#include "SkyUltra/BrowserBackend.h"

#include <filesystem>
#include <memory>

namespace skyultra {

struct UltralightRuntimeConfig {
    std::filesystem::path plugin_directory;
    std::filesystem::path cache_directory;
    bool force_cpu_rendering{};
};

class UltralightRuntime final : public IWebRuntime {
public:
    explicit UltralightRuntime(UltralightRuntimeConfig config);
    ~UltralightRuntime() override;

    UltralightRuntime(const UltralightRuntime&) = delete;
    UltralightRuntime& operator=(const UltralightRuntime&) = delete;

    [[nodiscard]] BrowserEngine Engine() const noexcept override;
    [[nodiscard]] bool Initialize(std::string& error) override;
    void Shutdown() noexcept override;
    void PumpFrame(void* d3d_device, void* d3d_context) override;
    void SetPageBridgeMessageCallback(PageBridgeMessageCallback callback) override;
    [[nodiscard]] std::unique_ptr<IWebView> CreateView(
        ViewId id, const ViewConfig& config, std::string_view initial_url) override;

private:
    struct State;
    UltralightRuntimeConfig config_;
    std::shared_ptr<State> state_;
};

}  // namespace skyultra
