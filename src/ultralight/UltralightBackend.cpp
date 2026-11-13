#include "SkyUltra/UltralightBackend.h"
#include "SkyUltra/ModManifest.h"

#include <SKSE/SKSE.h>

#include <Platform/Factories.h>
#include <Platform/gpu/d3d11/GPUContextD3D11.h>
#include <Platform/gpu/d3d11/GPUDriverD3D11.h>
#include <Ultralight/KeyEvent.h>
#include <Ultralight/Buffer.h>
#include <Ultralight/Listener.h>
#include <Ultralight/MouseEvent.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/ScrollEvent.h>
#include <Ultralight/URL.h>
#include <Ultralight/View.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Surface.h>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace skyultra {
namespace {

using ultralight::RefPtr;
using ultralight::View;
using ultralight::platform::GPUContextD3D11;
using Microsoft::WRL::ComPtr;

class UltralightFileMountRegistry final {
public:
    [[nodiscard]] std::string AddMount(
        const std::filesystem::path& root_directory,
        const std::filesystem::path& entry_file,
        bool asset_access_enabled)
    {
        std::error_code error;
        const auto root = std::filesystem::canonical(root_directory, error);
        if (error || !std::filesystem::is_directory(root, error) || error) {
            return {};
        }
        const auto entry = std::filesystem::canonical(entry_file, error);
        if (error || !std::filesystem::is_regular_file(entry, error) || error ||
            !IsPathWithin(root, entry)) {
            return {};
        }

        try {
            std::random_device random;
            constexpr char kHex[] = "0123456789abcdef";
            for (unsigned attempt = 0; attempt < 8; ++attempt) {
                std::string token;
                token.reserve(32);
                for (unsigned index = 0; index < 16; ++index) {
                    const auto value = static_cast<unsigned>(random());
                    token.push_back(kHex[(value >> 4U) & 0x0FU]);
                    token.push_back(kHex[value & 0x0FU]);
                }
                const std::scoped_lock lock(mutex_);
                if (mounts_.emplace(token, Mount{ root, entry, asset_access_enabled }).second) {
                    return token;
                }
            }
        } catch (...) {
            return {};
        }
        return {};
    }

    [[nodiscard]] std::optional<std::string> RewriteFileUrl(
        std::string_view token, std::string_view url_text) const
    {
        const auto token_text = std::string(token);
        Mount mount;
        {
            const std::scoped_lock lock(mutex_);
            const auto found = mounts_.find(token_text);
            if (found == mounts_.end()) {
                return std::nullopt;
            }
            mount = found->second;
        }

        const ultralight::URL url(ultralight::String(url_text.data(), url_text.size()));
        if (!url || url.scheme() != "file" || !url.host().empty() ||
            !url.username().empty() || !url.password().empty()) {
            return std::nullopt;
        }
        const auto path_text = url.ToFilePath().utf8();
        if (path_text.empty()) {
            return std::nullopt;
        }

        std::error_code error;
        const auto requested = std::filesystem::canonical(
            std::filesystem::u8path(std::string(path_text.data(), path_text.length())), error);
        if (error || !std::filesystem::is_regular_file(requested, error) || error ||
            !IsPathWithin(mount.root_directory, requested) ||
            (!mount.asset_access_enabled && !IsSamePath(requested, mount.entry_file))) {
            return std::nullopt;
        }

        const auto relative = requested.lexically_relative(mount.root_directory);
        const auto relative_utf8 = relative.generic_u8string();
        const std::string_view relative_text(
            reinterpret_cast<const char*>(relative_utf8.data()), relative_utf8.size());
        if (relative_text.empty() || relative_text.starts_with("../") ||
            relative_text == ".." || relative_text.find('\\') != std::string_view::npos) {
            return std::nullopt;
        }

        std::string result = "file:///SkyUltraMods/" + token_text + "/" +
            EncodeUrlPath(relative_text);
        const auto query = url.query().utf8();
        const auto fragment = url.fragment().utf8();
        if (!query.empty()) {
            result.push_back('?');
            result.append(query.data(), query.length());
        }
        if (!fragment.empty()) {
            result.push_back('#');
            result.append(fragment.data(), fragment.length());
        }
        return result;
    }

    [[nodiscard]] std::optional<std::filesystem::path> Resolve(
        std::string_view token, const std::filesystem::path& relative) const
    {
        Mount mount;
        {
            const std::scoped_lock lock(mutex_);
            const auto found = mounts_.find(std::string(token));
            if (found == mounts_.end()) {
                return std::nullopt;
            }
            mount = found->second;
        }
        std::error_code error;
        const auto path = std::filesystem::canonical(mount.root_directory / relative, error);
        if (error || !std::filesystem::is_regular_file(path, error) || error ||
            !IsPathWithin(mount.root_directory, path) ||
            (!mount.asset_access_enabled && !IsSamePath(path, mount.entry_file))) {
            return std::nullopt;
        }
        return path;
    }

    void Remove(std::string_view token)
    {
        if (token.empty()) {
            return;
        }
        const std::scoped_lock lock(mutex_);
        mounts_.erase(std::string(token));
    }

    void Clear()
    {
        const std::scoped_lock lock(mutex_);
        mounts_.clear();
    }

private:
    struct Mount {
        std::filesystem::path root_directory;
        std::filesystem::path entry_file;
        bool asset_access_enabled{};
    };

    static bool IsPathWithin(
        const std::filesystem::path& root, const std::filesystem::path& path)
    {
        auto root_part = root.begin();
        auto path_part = path.begin();
        for (; root_part != root.end(); ++root_part, ++path_part) {
            if (path_part == path.end()) {
                return false;
            }
            const auto& root_text = root_part->native();
            const auto& path_text = path_part->native();
            if (CompareStringOrdinal(root_text.c_str(), static_cast<int>(root_text.size()),
                    path_text.c_str(), static_cast<int>(path_text.size()), TRUE) != CSTR_EQUAL) {
                return false;
            }
        }
        return path_part != path.end();
    }

    static bool IsSamePath(
        const std::filesystem::path& left, const std::filesystem::path& right)
    {
        const auto& left_text = left.native();
        const auto& right_text = right.native();
        return left_text.size() == right_text.size() &&
            CompareStringOrdinal(left_text.c_str(), static_cast<int>(left_text.size()),
                right_text.c_str(), static_cast<int>(right_text.size()), TRUE) == CSTR_EQUAL;
    }

    static std::string EncodeUrlPath(std::string_view path)
    {
        constexpr char kHex[] = "0123456789ABCDEF";
        std::string encoded;
        encoded.reserve(path.size());
        for (const auto byte : path) {
            const auto value = static_cast<unsigned char>(byte);
            const bool unreserved = (value >= 'a' && value <= 'z') ||
                (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
                value == '-' || value == '_' || value == '.' || value == '~' || value == '/';
            if (unreserved) {
                encoded.push_back(static_cast<char>(value));
            } else {
                encoded.push_back('%');
                encoded.push_back(kHex[value >> 4U]);
                encoded.push_back(kHex[value & 0x0FU]);
            }
        }
        return encoded;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Mount> mounts_;
};

class SkyUltraUltralightFileSystem final : public ultralight::FileSystem {
public:
    SkyUltraUltralightFileSystem(
        std::unique_ptr<ultralight::FileSystem> file_system,
        std::shared_ptr<UltralightFileMountRegistry> mounts) :
        file_system_(std::move(file_system)), mounts_(std::move(mounts))
    {}

    bool FileExists(const ultralight::String& file_path) override
    {
        if (const auto resolved = ResolveVirtualPath(file_path); resolved.reserved) {
            return resolved.path.has_value();
        }
        return IsRuntimeResourcePath(file_path) &&
            file_system_->FileExists(MapRuntimeResourceName(file_path));
    }

    ultralight::String GetFileMimeType(const ultralight::String& file_path) override
    {
        if (const auto resolved = ResolveVirtualPath(file_path); resolved.reserved) {
            return resolved.path ? GetMountedFileMimeType(*resolved.path) :
                ultralight::String("application/unknown");
        }
        return IsRuntimeResourcePath(file_path) ?
            file_system_->GetFileMimeType(MapRuntimeResourceName(file_path)) :
            ultralight::String("application/unknown");
    }

    ultralight::String GetFileCharset(const ultralight::String& file_path) override
    {
        if (const auto resolved = ResolveVirtualPath(file_path); resolved.reserved) {
            return ultralight::String("utf-8");
        }
        return IsRuntimeResourcePath(file_path) ?
            file_system_->GetFileCharset(MapRuntimeResourceName(file_path)) :
            ultralight::String("utf-8");
    }

    RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& file_path) override
    {
        const auto resolved = ResolveVirtualPath(file_path);
        if (!resolved.reserved) {
            return IsRuntimeResourcePath(file_path) ?
                file_system_->OpenFile(MapRuntimeResourceName(file_path)) : nullptr;
        }
        if (!resolved.path) {
            SKSE::log::error("SkyUltra mod file request was blocked or missing (url='{}').",
                file_path.utf8().data());
            return nullptr;
        }
        std::ifstream file(*resolved.path, std::ios::binary);
        if (!file) {
            SKSE::log::error("SkyUltra could not open a mounted mod file (url='{}').",
                file_path.utf8().data());
            return nullptr;
        }
        const std::vector<char> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        return ultralight::Buffer::CreateFromCopy(bytes.data(), bytes.size());
    }

private:
    static ultralight::String GetMountedFileMimeType(const std::filesystem::path& path)
    {
        const auto extension = path.extension().generic_u8string();
        std::string ascii_extension;
        ascii_extension.reserve(extension.size());
        for (const auto character : extension) {
            auto ascii = static_cast<char>(character);
            if (ascii >= 'A' && ascii <= 'Z') {
                ascii = static_cast<char>(ascii - 'A' + 'a');
            }
            ascii_extension.push_back(ascii);
        }

        if (ascii_extension == ".css") return ultralight::String("text/css");
        if (ascii_extension == ".js" || ascii_extension == ".mjs") {
            return ultralight::String("text/javascript");
        }
        if (ascii_extension == ".json" || ascii_extension == ".map") {
            return ultralight::String("application/json");
        }
        if (ascii_extension == ".html" || ascii_extension == ".htm") {
            return ultralight::String("text/html");
        }
        if (ascii_extension == ".txt") return ultralight::String("text/plain");
        if (ascii_extension == ".svg") return ultralight::String("image/svg+xml");
        if (ascii_extension == ".png") return ultralight::String("image/png");
        if (ascii_extension == ".jpg" || ascii_extension == ".jpeg") {
            return ultralight::String("image/jpeg");
        }
        if (ascii_extension == ".webp") return ultralight::String("image/webp");
        if (ascii_extension == ".gif") return ultralight::String("image/gif");
        if (ascii_extension == ".ico") return ultralight::String("image/x-icon");
        if (ascii_extension == ".woff2") return ultralight::String("font/woff2");
        if (ascii_extension == ".woff") return ultralight::String("font/woff");
        if (ascii_extension == ".ttf") return ultralight::String("font/ttf");
        if (ascii_extension == ".otf") return ultralight::String("font/otf");
        if (ascii_extension == ".wasm") return ultralight::String("application/wasm");
        return ultralight::String("application/unknown");
    }

    struct ResolvedVirtualPath {
        bool reserved{};
        std::optional<std::filesystem::path> path;
    };

    static bool StartsWithAsciiCaseInsensitive(std::string_view value, std::string_view prefix)
    {
        if (value.size() < prefix.size()) {
            return false;
        }
        for (std::size_t index = 0; index < prefix.size(); ++index) {
            auto left = value[index];
            auto right = prefix[index];
            if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
            if (left != right) return false;
        }
        return true;
    }

    static bool IsRuntimeResourcePath(const ultralight::String& file_path)
    {
        const auto utf8 = file_path.utf8();
        const std::string_view path(utf8.data(), utf8.length());
        return path.starts_with("resources/SkyUltra/Ultralight/");
    }

    ResolvedVirtualPath ResolveVirtualPath(const ultralight::String& file_path) const
    {
        const auto utf8 = file_path.utf8();
        std::string_view path(utf8.data(), utf8.length());
        constexpr std::string_view kPrefix = "SkyUltraMods/";
        if (!path.empty() && path.front() == '/') {
            path.remove_prefix(1);
        }
        if (!StartsWithAsciiCaseInsensitive(path, kPrefix)) {
            return {};
        }
        ResolvedVirtualPath result{ true, std::nullopt };
        path.remove_prefix(kPrefix.size());
        const auto separator = path.find('/');
        if (separator == std::string_view::npos || separator == 0 || separator + 1 >= path.size()) {
            return result;
        }
        const std::string token(path.substr(0, separator));
        path.remove_prefix(separator + 1);

        std::string decoded;
        decoded.reserve(path.size());
        for (std::size_t index = 0; index < path.size(); ++index) {
            if (path[index] != '%') {
                decoded.push_back(path[index]);
                continue;
            }
            if (index + 2 >= path.size()) {
                return result;
            }
            const auto hex = [](char value) noexcept -> int {
                if (value >= '0' && value <= '9') return value - '0';
                if (value >= 'a' && value <= 'f') return value - 'a' + 10;
                if (value >= 'A' && value <= 'F') return value - 'A' + 10;
                return -1;
            };
            const auto high = hex(path[index + 1]);
            const auto low = hex(path[index + 2]);
            if (high < 0 || low < 0) {
                return result;
            }
            const auto value = static_cast<char>((high << 4) | low);
            if (value == '\0' || value == '/' || value == '\\') {
                return result;
            }
            decoded.push_back(value);
            index += 2;
        }
        if (decoded.empty() || decoded.find('\\') != std::string::npos ||
            decoded.find(':') != std::string::npos) {
            return result;
        }
        const auto relative = std::filesystem::u8path(decoded);
        if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
            return result;
        }
        for (const auto& part : relative) {
            if (part.empty() || part == L"." || part == L"..") {
                return result;
            }
        }

        const auto physical = mounts_->Resolve(token, relative);
        if (physical) {
            result.path = *physical;
        }
        return result;
    }

    static ultralight::String MapRuntimeResourceName(const ultralight::String& file_path)
    {
        const auto utf8 = file_path.utf8();
        const std::string_view name(utf8.data(), utf8.length());
        constexpr std::string_view icu_name =
            "resources/SkyUltra/Ultralight/icudt67l.dat";
        constexpr std::string_view mapped_icu_name =
            "resources/SkyUltra/Ultralight/SkyUltra_icudt67l.dat";
        constexpr std::string_view certificate_name =
            "resources/SkyUltra/Ultralight/cacert.pem";
        constexpr std::string_view mapped_certificate_name =
            "resources/SkyUltra/Ultralight/SkyUltra_cacert.pem";
        if (name == icu_name) {
            return ultralight::String(mapped_icu_name.data(), mapped_icu_name.size());
        }
        if (name == certificate_name) {
            return ultralight::String(mapped_certificate_name.data(), mapped_certificate_name.size());
        }
        return file_path;
    }

    std::unique_ptr<ultralight::FileSystem> file_system_;
    std::shared_ptr<UltralightFileMountRegistry> mounts_;
};

enum class ViewCommandStage : std::size_t {
    Snapshot,
    Config,
    Navigation,
    Input,
    Bridge,
    Count,
};

constexpr std::size_t kViewCommandStageCount =
    static_cast<std::size_t>(ViewCommandStage::Count);

class SkyUltraLoadListener final : public ultralight::LoadListener {
public:
    SkyUltraLoadListener(ViewId view_id, std::chrono::steady_clock::time_point requested_at) :
        view_id_(view_id), requested_at_(requested_at) {}

    void OnDOMReady(View* caller, std::uint64_t, bool is_main_frame,
        const ultralight::String& url) override
    {
        if (!is_main_frame) {
            return;
        }
        main_frame_finished_.store(true, std::memory_order_release);
        bridge_ready_.store(true, std::memory_order_release);
        if (logged_dom_ready_) {
            return;
        }
        logged_dom_ready_ = true;
        dom_ready_at_=std::chrono::steady_clock::now();
        LogStartupStage("DOM ready");
        const auto title = caller->title();
        SKSE::log::info("SkyUltra Ultralight page DOM ready (title='{}', url='{}').",
            title.utf8().data(), url.utf8().data());
    }

    [[nodiscard]] bool MainFrameFinished() const noexcept
    {
        return main_frame_finished_.load(std::memory_order_acquire);
    }
    void ResetMainFrameFinished() noexcept
    {
        logged_dom_ready_=false;page_mounted_=false;dom_ready_at_={};
        main_frame_finished_.store(false, std::memory_order_release);
        bridge_ready_.store(false, std::memory_order_release);
    }

    void MarkPageMounted() {page_mounted_=true;LogStartupStage("application shell mounted");}
    [[nodiscard]] bool HiddenWarmupComplete() const {return MainFrameFinished() && (page_mounted_ || (dom_ready_at_.time_since_epoch().count()!=0 && std::chrono::steady_clock::now()-dom_ready_at_>std::chrono::seconds(5)));}

    void MarkBridgeReady()
    {
        if (!bridge_ready_.exchange(true, std::memory_order_acq_rel)) {
            LogStartupStage("page bridge ready");
        }
    }
    [[nodiscard]] bool BridgeReady() const noexcept
    {
        return bridge_ready_.load(std::memory_order_acquire);
    }
    void LogStartupStage(std::string_view stage) const
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - requested_at_).count();
        SKSE::log::info("SkyUltra Ultralight startup view {}: {} at +{:.3f} ms.",
            view_id_, stage, elapsed);
    }

    void OnFailLoading(View*, std::uint64_t, bool is_main_frame,
        const ultralight::String& url, const ultralight::String& description,
        const ultralight::String& error_domain, int error_code) override
    {
        if (is_main_frame) {
            main_frame_finished_.store(true, std::memory_order_release);
            SKSE::log::error("SkyUltra Ultralight page load failed (url='{}', error='{}', domain='{}', code={}).",
                url.utf8().data(), description.utf8().data(), error_domain.utf8().data(), error_code);
        }
    }

private:
    const ViewId view_id_;
    const std::chrono::steady_clock::time_point requested_at_;
    bool logged_dom_ready_{};
    bool page_mounted_{};
    std::chrono::steady_clock::time_point dom_ready_at_{};
    std::atomic_bool main_frame_finished_{};
    std::atomic_bool bridge_ready_{};
};

class SkyUltraBridgeListener final : public ultralight::ViewListener {
public:
    SkyUltraBridgeListener(ViewId view_id, PageBridgeMessageCallback callback,
        SkyUltraLoadListener* load_listener) :
        view_id_(view_id), callback_(std::move(callback)), load_listener_(load_listener) {}

    void OnAddConsoleMessage(
        View*, const ultralight::ConsoleMessage& console_message) override
    {
        if (console_message.level() == ultralight::kMessageLevel_Error ||
            console_message.level() == ultralight::kMessageLevel_Warning) {
            const auto source = console_message.source_id().utf8();
            const auto message = console_message.message().utf8();
            if (console_message.level() == ultralight::kMessageLevel_Error) {
                SKSE::log::error(
                    "SkyUltra browser console error (view {}, source='{}', line {}): {}",
                    view_id_, source.data(), console_message.line_number(), message.data());
            } else {
                SKSE::log::warn(
                    "SkyUltra browser console warning (view {}, source='{}', line {}): {}",
                    view_id_, source.data(), console_message.line_number(), message.data());
            }
        }
        if (console_message.source() != ultralight::kMessageSource_ConsoleAPI || !callback_) {
            return;
        }
        PageBridgeMessage message;
        const std::string text = console_message.message().utf8().data();
        if (ParseBridgeConsoleMessage(view_id_, text, message)) {
            if (load_listener_) {load_listener_->MarkBridgeReady();if(message.name=="ui.pageReady")load_listener_->MarkPageMounted();}
            callback_(std::move(message));
        }
    }

private:
    ViewId view_id_{};
    PageBridgeMessageCallback callback_;
    SkyUltraLoadListener* load_listener_{};
};

class SkyUltraModNetworkListener final : public ultralight::NetworkListener {
public:
    explicit SkyUltraModNetworkListener(ModContentPolicy policy) : policy_(std::move(policy)) {}

    bool OnNetworkRequest(ultralight::View*, ultralight::NetworkRequest& request) override
    {
        const ultralight::URL url(request.url());
        if (!url || !url.username().empty() || !url.password().empty()) {
            return false;
        }
        const auto scheme = url.scheme().utf8();
        const std::string_view scheme_view = scheme.empty() ? std::string_view{} :
            std::string_view(scheme.data(), scheme.length());
        if (scheme_view == "data" || scheme_view == "blob" || scheme_view == "about" ||
            (scheme_view == "file" && url.host().empty())) {
            return true;
        }
        const auto host = url.host().utf8();
        const std::string_view host_view = host.empty() ? std::string_view{} :
            std::string_view(host.data(), host.length());
        return IsModNetworkHostAllowed(policy_, scheme_view, host_view);
    }

private:
    ModContentPolicy policy_;
};

constexpr std::string_view kHtmlDataUrlPrefix = "data:text/html;charset=utf-8,";

int HexDigit(char value) noexcept
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool DecodeHtmlDataUrl(std::string_view url, std::string& html)
{
    if (!url.starts_with(kHtmlDataUrlPrefix)) {
        return false;
    }

    const auto encoded = url.substr(kHtmlDataUrlPrefix.size());
    html.clear();
    html.reserve(encoded.size());
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] != '%') {
            html.push_back(encoded[i]);
            continue;
        }
        if (i + 2 >= encoded.size()) {
            html.clear();
            return false;
        }
        const auto high = HexDigit(encoded[i + 1]);
        const auto low = HexDigit(encoded[i + 2]);
        if (high < 0 || low < 0) {
            html.clear();
            return false;
        }
        html.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }
    return true;
}

void LoadNavigation(View& view, std::string_view url,
    std::string_view file_mount_token, const UltralightFileMountRegistry& file_mounts)
{
    std::string html;
    if (DecodeHtmlDataUrl(url, html)) {
        view.LoadHTML(ultralight::String(html.c_str(), html.size()));
    } else {
        const ultralight::URL parsed(ultralight::String(url.data(), url.size()));
        if (parsed && parsed.scheme() == "file") {
            const auto mounted_url = file_mounts.RewriteFileUrl(file_mount_token, url);
            if (!mounted_url) {
                SKSE::log::warn("SkyUltra blocked a local file navigation outside this view's mounted directory.");
                return;
            }
            view.LoadURL(ultralight::String(mounted_url->data(), mounted_url->size()));
            return;
        }
        view.LoadURL(ultralight::String(url.data(), url.size()));
    }
}

class SkyUltraGPUDriver final : public ultralight::platform::GPUDriverD3D11 {
public:
    using GPUDriverD3D11::GPUDriverD3D11;

    [[nodiscard]] ID3D11ShaderResourceView* GetViewTexture(std::uint32_t texture_id)
    {
        return GetTextureSRV(texture_id);
    }

    void Flush() override
    {
        ComPtr<ID3D11CommandList> part;
        const auto result = context_->immediate_context()->FinishCommandList(
            FALSE, part.GetAddressOf());
        if (FAILED(result)) {
            if (!logged_split_failure_) {
                SKSE::log::error(
                    "SkyUltra could not split an Ultralight D3D11 command list at a GPU barrier (HRESULT 0x{:08X}).",
                    static_cast<std::uint32_t>(result));
                logged_split_failure_ = true;
            }
            return;
        }
        if (part != nullptr) {
            split_command_lists_.push_back(std::move(part));
        }
        // FinishCommandList(FALSE) resets the deferred context to default state.
        // Force the D3D11 driver's bind cache to rebuild state in the next part.
        bound_.valid = false;
    }

    [[nodiscard]] std::vector<ComPtr<ID3D11CommandList>> TakeSplitCommandLists()
    {
        std::vector<ComPtr<ID3D11CommandList>> result;
        result.swap(split_command_lists_);
        return result;
    }

private:
    std::vector<ComPtr<ID3D11CommandList>> split_command_lists_;
    bool logged_split_failure_{};
};

using InputCommand = std::variant<MouseInput, KeyInput, TextInput>;

struct ViewMailbox {
    explicit ViewMailbox(ViewId view_id, const ViewConfig& view_config, std::string initial_url,
        std::string file_mount_token,
        std::function<void()> wake_async_worker,
        std::chrono::steady_clock::time_point requested_at) :
        id(view_id), wake_async_worker(std::move(wake_async_worker)),
        config(view_config), current_url(initial_url), file_mount_token(std::move(file_mount_token)),
        load_listener(view_id, requested_at)
    {
        bridge_events = config.bridge_event_queue ? config.bridge_event_queue : std::make_shared<BridgeEventQueue>();
        config.bridge_event_queue = bridge_events;
        bridge_responses = config.bridge_response_queue ? config.bridge_response_queue : std::make_shared<BridgeEventQueue>(false);
        config.bridge_response_queue = bridge_responses;
        if (!initial_url.empty()) {
            pending_navigations.push_back(std::move(initial_url));
        }
    }

    const ViewId id;
    const std::function<void()> wake_async_worker;
    mutable std::mutex mutex;
    ViewConfig config;
    std::string current_url;
    std::string file_mount_token;
    std::deque<std::string> pending_navigations;
    std::deque<InputCommand> pending_input;
    std::shared_ptr<BridgeEventQueue> bridge_events;
    std::atomic_uint64_t bridge_consumer{};
    std::shared_ptr<BridgeEventQueue> bridge_responses;
    std::atomic_uint64_t response_consumer{};
    std::chrono::steady_clock::time_point last_slow_bridge_log{};
    PageBridgeMessageCallback page_bridge_callback;
    InputMode focus_mode{ InputMode::None };
    SkyUltraLoadListener load_listener;
    GpuFrame frame;
    bool has_frame{};
    SoftwareFrame software_frame;
    bool has_software_frame{};
    bool close_requested{};
};

struct PendingGpuFrame {
    std::shared_ptr<ViewMailbox> mailbox;
    GpuFrame frame;
};

struct RenderFramePacket {
    ComPtr<ID3D11Device> device;
    std::vector<ComPtr<ID3D11CommandList>> command_lists;
    std::vector<PendingGpuFrame> gpu_frames;
};

class UltralightView final : public IWebView {
public:
    explicit UltralightView(std::shared_ptr<ViewMailbox> mailbox) : mailbox_(std::move(mailbox)) {
        mailbox_->bridge_consumer = mailbox_->bridge_events->Attach([weak = std::weak_ptr<ViewMailbox>(mailbox_)] {
            if (const auto mailbox = weak.lock()) mailbox->wake_async_worker();
        });
        mailbox_->response_consumer = mailbox_->bridge_responses->Attach([weak = std::weak_ptr<ViewMailbox>(mailbox_)] {
            if (const auto mailbox = weak.lock()) mailbox->wake_async_worker();
        });
    }

    ~UltralightView() override
    {
        const std::scoped_lock lock(mailbox_->mutex);
        mailbox_->close_requested = true;
        mailbox_->has_frame = false;
    }

    [[nodiscard]] ViewId Id() const noexcept override { return mailbox_->id; }

    void Navigate(std::string_view url) override
    {
        const std::scoped_lock lock(mailbox_->mutex);
        mailbox_->current_url.assign(url);
        mailbox_->pending_navigations.emplace_back(url);
        mailbox_->load_listener.ResetMainFrameFinished();
    }

    void Resize(SizeU size) override
    {
        const std::scoped_lock lock(mailbox_->mutex);
        mailbox_->config.size = size;
    }

    void SetScale(float scale) override
    {
        if (!std::isfinite(scale) || scale <= 0.0F) {
            return;
        }
        const std::scoped_lock lock(mailbox_->mutex);
        mailbox_->config.scale = std::clamp(scale, 0.25F, 4.0F);
    }

    void SetMaxFPS(std::uint32_t fps) override
    {
        const std::scoped_lock lock(mailbox_->mutex);
        mailbox_->config.max_fps = fps == 0 ? 0U : std::clamp(fps, 1U, 240U);
    }

    void SetVisible(bool visible) override
    {
        bool changed{};
        {
            const std::scoped_lock lock(mailbox_->mutex);
            changed = mailbox_->config.visible != visible;
            if (changed) {
                mailbox_->config.visible = visible;
            }
        }
        if (changed) {
            WakeAsyncWorker();
        }
    }

    void SetFocused(InputMode mode) override
    {
        {
            const std::scoped_lock lock(mailbox_->mutex);
            mailbox_->focus_mode = mode;
        }
        WakeAsyncWorker();
    }

    void SendMouse(const MouseInput& input) override
    {
        {
            const std::scoped_lock lock(mailbox_->mutex);
            bool coalesced{};
            if (input.moved && input.button_action == MouseButtonAction::None && !input.wheel &&
                !mailbox_->pending_input.empty() &&
                std::holds_alternative<MouseInput>(mailbox_->pending_input.back())) {
                const auto& last = std::get<MouseInput>(mailbox_->pending_input.back());
                if (last.moved && last.button_action == MouseButtonAction::None && !last.wheel) {
                    mailbox_->pending_input.back() = input;
                    coalesced = true;
                }
            }
            if (!coalesced) {
                EnqueueInputLocked(input);
            }
        }
        WakeAsyncWorker();
    }

    void SendKey(const KeyInput& input) override
    {
        {
            const std::scoped_lock lock(mailbox_->mutex);
            EnqueueInputLocked(input);
        }
        WakeAsyncWorker();
    }

    void SendText(const TextInput& input) override
    {
        {
            const std::scoped_lock lock(mailbox_->mutex);
            EnqueueInputLocked(input);
        }
        WakeAsyncWorker();
    }

    [[nodiscard]] bool SendEventJson(
        std::string_view event_name, std::string_view payload_json) override
    {
        {
            const std::scoped_lock lock(mailbox_->mutex);
            if (!mailbox_->config.enable_javascript || mailbox_->close_requested) {
                return false;
            }
        }
        return mailbox_->bridge_events->Push(event_name, payload_json) == BridgeEventQueue::Result::Accepted;
    }
    [[nodiscard]] std::shared_ptr<BridgeEventQueue> BridgeEvents() const override { return mailbox_->bridge_events; }
    [[nodiscard]] std::shared_ptr<BridgeEventQueue> BridgeResponses() const override { return mailbox_->bridge_responses; }

    [[nodiscard]] bool ResolveRequest(
        std::string_view request_id, bool success, std::string_view payload_json,
        std::string_view error_message) override
    {
        {
            const std::scoped_lock lock(mailbox_->mutex);
            if (!mailbox_->config.enable_javascript || mailbox_->close_requested) {
                return false;
            }
        }
        return mailbox_->bridge_responses->Push(request_id,
            BuildBridgeResponseJson(request_id, success, payload_json, error_message)) == BridgeEventQueue::Result::Accepted;
    }

    [[nodiscard]] std::optional<SoftwareFrame> TakeFrame() override
    {
        const std::scoped_lock lock(mailbox_->mutex);
        if (!mailbox_->has_software_frame) {
            return std::nullopt;
        }
        mailbox_->has_software_frame = false;
        return std::exchange(mailbox_->software_frame, SoftwareFrame{});
    }

    [[nodiscard]] std::optional<GpuFrame> GetGpuFrame() override
    {
        const std::scoped_lock lock(mailbox_->mutex);
        return mailbox_->has_frame ? std::optional<GpuFrame>(mailbox_->frame) : std::nullopt;
    }

private:
    void WakeAsyncWorker() const
    {
        if (mailbox_->wake_async_worker) {
            mailbox_->wake_async_worker();
        }
    }

    template <class T>
    void EnqueueInputLocked(T&& input)
    {
        constexpr std::size_t kMaxPendingInput = 1024;
        if (mailbox_->pending_input.size() == kMaxPendingInput) {
            mailbox_->pending_input.pop_front();
        }
        mailbox_->pending_input.emplace_back(std::forward<T>(input));
    }

    std::shared_ptr<ViewMailbox> mailbox_;
};

struct ViewSnapshot {
    ViewConfig config;
    std::string file_mount_token;
    std::deque<std::string> navigations;
    InputMode focus_mode{ InputMode::None };
    bool close_requested{};
};

ViewSnapshot TakeSnapshot(const std::shared_ptr<ViewMailbox>& mailbox)
{
    const std::scoped_lock lock(mailbox->mutex);
    ViewSnapshot snapshot;
    snapshot.config = mailbox->config;
    snapshot.file_mount_token = mailbox->file_mount_token;
    snapshot.focus_mode = mailbox->focus_mode;
    snapshot.navigations.swap(mailbox->pending_navigations);
    snapshot.close_requested = mailbox->close_requested;
    return snapshot;
}

double DispatchBridgeEvents(View& view, const std::shared_ptr<ViewMailbox>& mailbox, bool responses = false)
{
    const auto& queue = responses ? mailbox->bridge_responses : mailbox->bridge_events;
    const auto batch = queue->Take(responses ? mailbox->response_consumer.load() : mailbox->bridge_consumer.load());
    if (!batch) return 0;
    const auto started = std::chrono::steady_clock::now();
    const auto script = BuildBridgeEventBatchScript(*batch, {}, responses);
    ultralight::String exception;
    view.EvaluateScript(ultralight::String(script.c_str(), script.size()), &exception);
    batch->executed = true;
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    queue->RecordDispatch(elapsed);
    if (elapsed >= 100 && std::chrono::steady_clock::now() - mailbox->last_slow_bridge_log >= std::chrono::seconds(10)) {
        mailbox->last_slow_bridge_log = std::chrono::steady_clock::now();
        const auto stats = queue->Snapshot();
        SKSE::log::warn("SkyUltra bridge view {} ({}): {} messages starting '{}', {:.1f} ms; pending {}, bytes {}, rejected {}, oldest {:.1f} ms.",
            mailbox->id, responses ? "replies" : "events", batch->events.size(), batch->events.front().name,
            elapsed, stats.pending, stats.bytes, stats.rejected, stats.oldest_ms);
    }
    if (!exception.empty()) {
        SKSE::log::warn("SkyUltra bridge batch failed for view {}: {}.", mailbox->id, exception.utf8().data());
    }
    return elapsed;
}

unsigned UltralightModifiers(std::uint32_t modifiers)
{
    unsigned result{};
    if ((modifiers & kModifierAlt) != 0) result |= ultralight::KeyEvent::kMod_AltKey;
    if ((modifiers & kModifierControl) != 0) result |= ultralight::KeyEvent::kMod_CtrlKey;
    if ((modifiers & kModifierMeta) != 0) result |= ultralight::KeyEvent::kMod_MetaKey;
    if ((modifiers & kModifierShift) != 0) result |= ultralight::KeyEvent::kMod_ShiftKey;
    return result;
}

ultralight::MouseEvent::Button UltralightMouseButton(MouseButton button)
{
    switch (button) {
    case MouseButton::Left: return ultralight::MouseEvent::kButton_Left;
    case MouseButton::Middle: return ultralight::MouseEvent::kButton_Middle;
    case MouseButton::Right: return ultralight::MouseEvent::kButton_Right;
    }
    return ultralight::MouseEvent::kButton_None;
}

void DispatchInput(View& view, const InputCommand& command, ultralight::MouseEvent::Button& held_button)
{
    std::visit([&view, &held_button](const auto& input) {
        using T = std::decay_t<decltype(input)>;
        if constexpr (std::is_same_v<T, MouseInput>) {
            if (input.moved) {
                ultralight::MouseEvent moved{};
                moved.type = ultralight::MouseEvent::kType_MouseMoved;
                moved.x = static_cast<int>(std::lround(input.position.x));
                moved.y = static_cast<int>(std::lround(input.position.y));
                moved.button = held_button;
                view.FireMouseEvent(moved);
            }
            if (input.button_action != MouseButtonAction::None) {
                const auto mapped_button = UltralightMouseButton(input.button);
                if (input.button_action == MouseButtonAction::Down) {
                    held_button = mapped_button;
                }
                ultralight::MouseEvent button{};
                button.type = input.button_action == MouseButtonAction::Down ?
                    ultralight::MouseEvent::kType_MouseDown : ultralight::MouseEvent::kType_MouseUp;
                button.x = static_cast<int>(std::lround(input.position.x));
                button.y = static_cast<int>(std::lround(input.position.y));
                button.button = mapped_button;
                view.FireMouseEvent(button);
                if (input.button_action == MouseButtonAction::Up && held_button == mapped_button) {
                    held_button = ultralight::MouseEvent::kButton_None;
                }
            }
            if (input.wheel) {
                ultralight::ScrollEvent scroll{};
                scroll.type = ultralight::ScrollEvent::kType_ScrollByPixel;
                scroll.delta_x = input.wheel_x / 3;
                scroll.delta_y = input.wheel_y / 3;
                scroll.x = static_cast<int>(std::lround(input.position.x));
                scroll.y = static_cast<int>(std::lround(input.position.y));
                view.FireScrollEvent(scroll);
            }
        } else if constexpr (std::is_same_v<T, KeyInput>) {
            ultralight::KeyEvent event;
            event.type = input.pressed ? ultralight::KeyEvent::kType_RawKeyDown :
                ultralight::KeyEvent::kType_KeyUp;
            event.modifiers = UltralightModifiers(input.modifiers);
            event.virtual_key_code = static_cast<int>(input.virtual_key);
            event.native_key_code = static_cast<int>(input.scan_code);
            event.is_auto_repeat = input.repeat;
            event.is_system_key = input.system_key;
            ultralight::GetKeyIdentifierFromVirtualKeyCode(event.virtual_key_code, event.key_identifier);
            view.FireKeyEvent(event);
        } else if constexpr (std::is_same_v<T, TextInput>) {
            if (!input.committed || input.composition) {
                return;
            }
            for (const auto codepoint : input.text) {
                ultralight::String32 value(&codepoint, 1);
                ultralight::KeyEvent event;
                event.type = ultralight::KeyEvent::kType_Char;
                event.text = ultralight::String(value);
                event.unmodified_text = event.text;
                view.FireKeyEvent(event);
            }
        }
    }, command);
}

const char* InputCommandName(const InputCommand& command) noexcept
{
    if (const auto* mouse = std::get_if<MouseInput>(&command)) {
        if (mouse->wheel) return "mouse-wheel";
        if (mouse->button_action == MouseButtonAction::Down) return "mouse-down";
        if (mouse->button_action == MouseButtonAction::Up) return "mouse-up";
        return "mouse-move";
    }
    if (const auto* key = std::get_if<KeyInput>(&command)) {
        if (!key->pressed) return "key-up";
        return key->repeat ? "key-repeat" : "key-down";
    }
    if (std::get<TextInput>(command).composition) return "text-composition";
    return "text";
}

void SetGpuFrame(const std::shared_ptr<ViewMailbox>& mailbox, const GpuFrame& frame, bool valid)
{
    const std::scoped_lock lock(mailbox->mutex);
    mailbox->frame = frame;
    mailbox->has_frame = valid;
    mailbox->software_frame = {};
    mailbox->has_software_frame = false;
}

void SetSoftwareFrame(const std::shared_ptr<ViewMailbox>& mailbox, SoftwareFrame frame)
{
    if (frame.patches.empty()) {
        return;
    }
    const std::scoped_lock lock(mailbox->mutex);
    mailbox->frame = {};
    mailbox->has_frame = false;
    mailbox->software_frame = std::move(frame);
    mailbox->has_software_frame = true;
}

}  // namespace

struct UltralightRuntime::State {
    struct LiveView {
        std::shared_ptr<ViewMailbox> mailbox;
        std::unique_ptr<SkyUltraBridgeListener> bridge_listener;
        std::unique_ptr<SkyUltraModNetworkListener> network_listener;
        RefPtr<View> view;
        ViewConfig applied_config;
        InputMode applied_focus{ InputMode::None };
        bool accelerated{};
        bool warmup_pending{};
        std::uint64_t software_frame_sequence{};
        SizeU software_size{};
        bool logged_target_failure{};
        bool logged_output_ready{};
        std::uint32_t warmup_frames{};
        ultralight::MouseEvent::Button held_mouse_button{ ultralight::MouseEvent::kButton_None };
    };

    // Ultralight records D3D11 work on a deferred context. Skyrim executes each
    // completed command list and reuses the last published frame while page work runs.
    std::mutex pump_mutex;
    bool host_pump_window_started{};
    std::chrono::steady_clock::time_point host_pump_window_start{};
    std::uint64_t host_pump_count{};
    double host_pump_total_ms{};
    double host_pump_max_ms{};
    std::uint64_t host_pump_deferred_count{};
    std::mutex worker_mutex;
    std::condition_variable worker_wakeup;
    std::thread worker;
    ID3D11Device* requested_device{};
    ID3D11DeviceContext* requested_context{};
    bool frame_pending{};
    bool async_view_work_pending{};
    bool worker_busy{};
    bool stop_requested{};
    bool initialization_failed{};
    std::atomic_bool gpu_enabled{};
    bool deferred_gpu_enabled{};
    bool logged_deferred_finish_failure{};
    ID3D11Device* removed_device{};
    bool logged_gpu_command_dispatch{};
    bool performance_window_started{};
    std::chrono::steady_clock::time_point performance_window_start{};
    std::uint64_t performance_frame_count{};
    double performance_cpu_total_ms{};
    double performance_cpu_max_ms{};
    double performance_prepare_total_ms{};
    double performance_prepare_max_ms{};
    double performance_view_create_total_ms{};
    double performance_view_create_max_ms{};
    double performance_view_commands_total_ms{};
    double performance_view_commands_max_ms{};
    double performance_async_view_work_total_ms{};
    std::size_t next_async_view{};
    double performance_async_view_work_max_ms{};
    std::array<double, kViewCommandStageCount> performance_command_stage_total_ms{};
    std::array<double, kViewCommandStageCount> performance_command_stage_max_ms{};
    std::array<ViewId, kViewCommandStageCount> performance_command_stage_max_view{};
    std::size_t performance_input_batch_max{};
    ViewId performance_input_batch_max_view{};
    double performance_input_command_max_ms{};
    ViewId performance_input_command_max_view{};
    const char* performance_input_command_max_name{ "none" };
    double performance_config_operation_max_ms{};
    ViewId performance_config_operation_max_view{};
    const char* performance_config_operation_max_name{ "none" };
    double performance_update_total_ms{};
    double performance_update_max_ms{};
    double performance_refresh_display_total_ms{};
    double performance_refresh_display_max_ms{};
    double performance_render_total_ms{};
    double performance_render_max_ms{};
    double performance_gpu_submit_total_ms{};
    double performance_gpu_submit_max_ms{};
    double performance_collect_total_ms{};
    double performance_collect_max_ms{};
    UltralightRuntimeConfig config;
    std::mutex callback_mutex;
    PageBridgeMessageCallback page_bridge_callback;
    std::mutex pending_mutex;
    std::vector<std::shared_ptr<ViewMailbox>> pending_views;
    std::vector<LiveView> live_views;
    std::shared_ptr<UltralightFileMountRegistry> file_mounts{
        std::make_shared<UltralightFileMountRegistry>() };
    std::unique_ptr<ultralight::FileSystem> file_system;
    std::unique_ptr<ultralight::FontLoader> font_loader;
    std::unique_ptr<GPUContextD3D11> gpu_context;
    std::unique_ptr<SkyUltraGPUDriver> gpu_driver;
    ComPtr<ID3D11DeviceContext> deferred_context;
    std::unique_ptr<RenderFramePacket> in_progress_frame;
    std::unique_ptr<RenderFramePacket> completed_frame;
    RefPtr<ultralight::Renderer> renderer;
    ID3D11Device* host_device{};
    ID3D11DeviceContext* host_context{};
    std::thread::id owner_thread;
    bool initialized{};
    std::atomic_bool has_active_views{ false };
    std::chrono::steady_clock::time_point last_idle_pump{};

    void RequestAsyncViewWork();
    [[nodiscard]] bool HasPendingAsyncViewWork();
    void ProcessAsyncViewWork();
    void ProcessFrame(ID3D11Device* device, ID3D11DeviceContext* context);
    void RestoreViewsAfterDeviceChange();
    void WorkerMain();
    void Cleanup() noexcept;
};

UltralightRuntime::UltralightRuntime(UltralightRuntimeConfig config) :
    config_(std::move(config)), state_(std::make_shared<State>())
{}

UltralightRuntime::~UltralightRuntime()
{
    Shutdown();
}

BrowserEngine UltralightRuntime::Engine() const noexcept
{
    return BrowserEngine::Ultralight;
}

void UltralightRuntime::SetPageBridgeMessageCallback(PageBridgeMessageCallback callback)
{
    const std::scoped_lock lock(state_->callback_mutex);
    state_->page_bridge_callback = std::move(callback);
}

bool UltralightRuntime::Initialize(std::string& error)
{
    std::error_code filesystem_error;
    if (!std::filesystem::is_directory(config_.plugin_directory, filesystem_error) || filesystem_error) {
        error = "Ultralight plugin directory does not exist: " + config_.plugin_directory.string();
        return false;
    }
    const auto resource_directory = config_.plugin_directory / L"resources" / L"SkyUltra" / L"Ultralight";
    const auto has_icu_data = std::filesystem::is_regular_file(
        resource_directory / L"SkyUltra_icudt67l.dat", filesystem_error) && !filesystem_error;
    filesystem_error.clear();
    const auto has_ca_bundle = std::filesystem::is_regular_file(
        resource_directory / L"SkyUltra_cacert.pem", filesystem_error) && !filesystem_error;
    if (!has_icu_data || !has_ca_bundle) {
        error = "SkyUltra's isolated Ultralight resources are missing from resources\\SkyUltra\\Ultralight.";
        return false;
    }
    std::filesystem::create_directories(config_.cache_directory, filesystem_error);
    if (filesystem_error) {
        error = "Could not create the Ultralight cache directory: " + filesystem_error.message();
        return false;
    }

    state_->config = config_;
    try {
        state_->worker = std::thread([state = state_] { state->WorkerMain(); });
    } catch (const std::exception& exception) {
        error = std::string("Could not start the Ultralight WebView thread: ") + exception.what();
        return false;
    }
    return true;
}

void UltralightRuntime::State::RequestAsyncViewWork()
{
    has_active_views.store(true, std::memory_order_relaxed);
    {
        const std::scoped_lock lock(worker_mutex);
        if (stop_requested) {
            return;
        }
        async_view_work_pending = true;
    }
    worker_wakeup.notify_one();
}

bool UltralightRuntime::State::HasPendingAsyncViewWork()
{
    for (const auto& live : live_views) {
        const std::scoped_lock lock(live.mailbox->mutex);
        // Navigation/resize/teardown need a render-frame pass. Do not busy-loop
        // the async worker while it is waiting for those commands to be applied.
        if (live.mailbox->close_requested || !live.mailbox->pending_navigations.empty() ||
            live.mailbox->config.size.width != live.applied_config.size.width ||
            live.mailbox->config.size.height != live.applied_config.size.height ||
            live.mailbox->config.scale != live.applied_config.scale) continue;
        if (!live.mailbox->pending_input.empty() ||
            live.mailbox->focus_mode != live.applied_focus ||
            live.mailbox->config.visible != live.applied_config.visible ||
            (live.mailbox->load_listener.BridgeReady() &&
                (live.mailbox->bridge_events->Pending(live.mailbox->bridge_consumer.load()) ||
                 live.mailbox->bridge_responses->Pending(live.mailbox->response_consumer.load())))) {
            return true;
        }
    }
    return false;
}

void UltralightRuntime::State::ProcessAsyncViewWork()
{
    if (owner_thread != std::this_thread::get_id() || initialization_failed) {
        return;
    }
    const auto async_view_work_start = std::chrono::steady_clock::now();
    const auto view_count = live_views.size();
    const auto start_view = next_async_view;
    for (std::size_t offset = 0; offset < view_count; ++offset) {
        if (offset > 0 && std::chrono::steady_clock::now() - async_view_work_start >= std::chrono::milliseconds(4)) break;
        const auto view_index = (start_view + offset) % view_count;
        auto& live = live_views[view_index];
        next_async_view = (view_index + 1) % view_count;
        InputMode focus_mode{};
        bool visible{};
        std::deque<InputCommand> input;
        {
            const std::scoped_lock lock(live.mailbox->mutex);
            const auto& config = live.mailbox->config;
            if (live.mailbox->close_requested ||
                !live.mailbox->pending_navigations.empty() ||
                config.size.width != live.applied_config.size.width ||
                config.size.height != live.applied_config.size.height ||
                config.scale != live.applied_config.scale) {
                continue;
            }
            focus_mode = live.mailbox->focus_mode;
            visible = config.visible;
            for (std::size_t i = 0; i < 1 && !live.mailbox->pending_input.empty(); ++i) {
                input.push_back(std::move(live.mailbox->pending_input.front()));
                live.mailbox->pending_input.pop_front();
            }
        }

        if (visible != live.applied_config.visible) {
            const auto visibility_start = std::chrono::steady_clock::now();
            if (visible) {
                live.view->set_visible(true);
                live.warmup_pending = false;
                has_active_views.store(true, std::memory_order_relaxed);
            } else {
                live.held_mouse_button = ultralight::MouseEvent::kButton_None;
                if (!live.warmup_pending) live.view->set_visible(false);
            }
            live.applied_config.visible = visible;
            const auto visibility_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - visibility_start).count();
            if (visibility_ms > performance_config_operation_max_ms) {
                performance_config_operation_max_ms = visibility_ms;
                performance_config_operation_max_view = live.mailbox->id;
                performance_config_operation_max_name = "visibility";
            }
        }

        if (focus_mode != live.applied_focus) {
            const auto focus_start = std::chrono::steady_clock::now();
            if (focus_mode == InputMode::None) {
                live.held_mouse_button = ultralight::MouseEvent::kButton_None;
                live.view->Unfocus();
            } else {
                live.view->Focus();
            }
            live.applied_focus = focus_mode;
            const auto focus_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - focus_start).count();
            if (focus_ms > performance_config_operation_max_ms) {
                performance_config_operation_max_ms = focus_ms;
                performance_config_operation_max_view = live.mailbox->id;
                performance_config_operation_max_name = "focus";
            }
        }
        const auto deliver_bridge = [&](bool responses) {
            const auto bridge_ms = DispatchBridgeEvents(*live.view, live.mailbox, responses);
            const auto index = static_cast<std::size_t>(ViewCommandStage::Bridge);
            performance_command_stage_total_ms[index] += bridge_ms;
            if (bridge_ms > performance_command_stage_max_ms[index]) {
                performance_command_stage_max_ms[index] = bridge_ms;
                performance_command_stage_max_view[index] = live.mailbox->id;
            }
        };
        // Replies and input precede bulk events and can run without a GPU frame.
        if (live.mailbox->load_listener.BridgeReady()) deliver_bridge(true);
        if (input.empty()) {
            if (live.mailbox->load_listener.BridgeReady() &&
                std::chrono::steady_clock::now() - async_view_work_start < std::chrono::milliseconds(4)) {
                deliver_bridge(false);
            }
            continue;
        }

        if (input.size() > performance_input_batch_max) {
            performance_input_batch_max = input.size();
            performance_input_batch_max_view = live.mailbox->id;
        }
        const auto input_start = std::chrono::steady_clock::now();
        for (const auto& command : input) {
            const auto command_start = std::chrono::steady_clock::now();
            DispatchInput(*live.view, command, live.held_mouse_button);
            const auto command_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - command_start).count();
            if (command_ms > performance_input_command_max_ms) {
                performance_input_command_max_ms = command_ms;
                performance_input_command_max_view = live.mailbox->id;
                performance_input_command_max_name = InputCommandName(command);
            }
        }
        const auto input_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - input_start).count();
        const auto stage_index = static_cast<std::size_t>(ViewCommandStage::Input);
        performance_command_stage_total_ms[stage_index] += input_ms;
        if (input_ms > performance_command_stage_max_ms[stage_index]) {
            performance_command_stage_max_ms[stage_index] = input_ms;
            performance_command_stage_max_view[stage_index] = live.mailbox->id;
        }
        if (live.mailbox->load_listener.BridgeReady() &&
            std::chrono::steady_clock::now() - async_view_work_start < std::chrono::milliseconds(4)) {
            deliver_bridge(false);
        }
    }
    const auto async_view_work_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - async_view_work_start).count();
    performance_async_view_work_total_ms += async_view_work_ms;
    performance_async_view_work_max_ms = std::max(
        performance_async_view_work_max_ms, async_view_work_ms);
}

void UltralightRuntime::Shutdown() noexcept
{
    if (!state_) {
        return;
    }

    const std::scoped_lock pump_lock(state_->pump_mutex);
    {
        const std::scoped_lock worker_lock(state_->worker_mutex);
        state_->stop_requested = true;
    }
    state_->worker_wakeup.notify_one();
    if (state_->worker.joinable() && state_->worker.get_id() != std::this_thread::get_id()) {
        try {
            state_->worker.join();
        } catch (...) {
            SKSE::log::error("SkyUltra could not join its Ultralight WebView thread cleanly.");
        }
    }
}

void UltralightRuntime::PumpFrame(void* native_device, void* native_context)
{
    auto* device = static_cast<ID3D11Device*>(native_device);
    auto* context = static_cast<ID3D11DeviceContext*>(native_context);
    if (device == nullptr || context == nullptr) {
        return;
    }

    const auto pump_start = std::chrono::steady_clock::now();
    const std::scoped_lock pump_lock(state_->pump_mutex);
    if (!state_->worker.joinable()) {
        return;
    }
    std::unique_ptr<RenderFramePacket> completed_frame;
    bool schedule_frame{};
    bool gpu_renderer{};
    {
        const std::scoped_lock worker_lock(state_->worker_mutex);
        if (state_->stop_requested) {
            return;
        }
        gpu_renderer = state_->gpu_enabled.load(std::memory_order_relaxed);
        completed_frame = std::move(state_->completed_frame);
        state_->requested_device = device;
        state_->requested_context = context;
        const bool active_views = state_->has_active_views.load(std::memory_order_relaxed);
        const bool idle_due = !active_views &&
            std::chrono::duration<double, std::milli>(pump_start - state_->last_idle_pump).count() >= 250.0;
        if (!state_->frame_pending && (active_views || (state_->initialized && idle_due))) {
            state_->frame_pending = true;
            state_->last_idle_pump = pump_start;
            schedule_frame = !state_->worker_busy;
        }
        if (state_->worker_busy) {
            ++state_->host_pump_deferred_count;
        }
    }
    if (schedule_frame) {
        state_->worker_wakeup.notify_one();
    }

    // Execute the last finished browser frame before compositing. Page update,
    // layout, script, and GPU recording continue on the worker while Skyrim draws.
    if (completed_frame != nullptr && completed_frame->device.Get() == device) {
        for (const auto& command_list : completed_frame->command_lists) {
            if (command_list != nullptr) {
                context->ExecuteCommandList(command_list.Get(), TRUE);
            }
        }
        for (const auto& update : completed_frame->gpu_frames) {
            SetGpuFrame(update.mailbox, update.frame, true);
        }
    }

    const auto pump_end = std::chrono::steady_clock::now();
    if (!state_->host_pump_window_started) {
        state_->host_pump_window_start = pump_end;
        state_->host_pump_window_started = true;
        return;
    }

    const auto pump_ms = std::chrono::duration<double, std::milli>(pump_end - pump_start).count();
    ++state_->host_pump_count;
    state_->host_pump_total_ms += pump_ms;
    state_->host_pump_max_ms = std::max(state_->host_pump_max_ms, pump_ms);
    const auto window_seconds = std::chrono::duration<double>(
        pump_end - state_->host_pump_window_start).count();
    if (window_seconds >= 10.0) {
        SKSE::log::info(
            "SkyUltra render-thread handoff ({} renderer): {:.1f} callbacks/s, frame handoff avg/max {:.3f}/{:.3f} ms, reused {} frames while WebView work was pending.",
            gpu_renderer ? "GPU" : "CPU",
            static_cast<double>(state_->host_pump_count) / window_seconds,
            state_->host_pump_total_ms / static_cast<double>(state_->host_pump_count),
            state_->host_pump_max_ms, state_->host_pump_deferred_count);
        state_->host_pump_window_start = pump_end;
        state_->host_pump_count = 0;
        state_->host_pump_total_ms = 0.0;
        state_->host_pump_max_ms = 0.0;
        state_->host_pump_deferred_count = 0;
    }
}

void UltralightRuntime::State::ProcessFrame(
    ID3D11Device* device, ID3D11DeviceContext* context)
{
    in_progress_frame.reset();
    if (owner_thread != std::this_thread::get_id()) {
        return;
    }
    if (initialization_failed) {
        Cleanup();
        return;
    }
    if (device == nullptr || context == nullptr) {
        return;
    }
    // Warming the runtime/worker must not allocate a second GPU renderer when
    // all installed views currently use CEF. CreateView wakes us on first use.
    if (!initialized && live_views.empty()) {
        const std::scoped_lock lock(pending_mutex);
        if (pending_views.empty()) {
            has_active_views.store(false,std::memory_order_relaxed);
            return;
        }
    }
    const auto frame_start = std::chrono::steady_clock::now();

    const auto device_status = device->GetDeviceRemovedReason();
    if (FAILED(device_status)) {
        if (removed_device != device) {
            RestoreViewsAfterDeviceChange();
            Cleanup();
            removed_device = device;
            SKSE::log::error(
                "SkyUltra detected a removed D3D11 device (HRESULT 0x{:08X}); waiting for Skyrim to provide a replacement.",
                static_cast<std::uint32_t>(device_status));
        }
        return;
    }
    if (removed_device != nullptr) {
        if (removed_device == device) {
            removed_device = nullptr;
        } else {
            SKSE::log::info("SkyUltra received Skyrim's replacement D3D11 device; restoring browser views.");
            removed_device = nullptr;
        }
    }

    if (initialized && (host_device != device || host_context != context)) {
        SKSE::log::warn("SkyUltra detected a replaced D3D11 device or context; rebuilding browser resources.");
        RestoreViewsAfterDeviceChange();
        Cleanup();
    }

    if (!initialized) {
        auto& platform = ultralight::Platform::instance();
        auto config = ultralight::Config();
        config.cache_path = ultralight::String(this->config.cache_directory.string().c_str());
        config.resource_path_prefix = "resources/SkyUltra/Ultralight/";
        platform.set_config(config);

        const auto plugin_path = this->config.plugin_directory.string();
        auto native_file_system = ultralight::platform::CreateFileSystem(
            ultralight::String(plugin_path.c_str()));
        if (native_file_system) {
            file_system = std::make_unique<SkyUltraUltralightFileSystem>(
                std::move(native_file_system), file_mounts);
        }
        font_loader = ultralight::platform::CreateFontLoader();
        if (!file_system || !font_loader) {
            file_system.reset();
            font_loader.reset();
            initialization_failed = true;
            SKSE::log::error("SkyUltra could not initialize Ultralight's file system or font loader.");
            return;
        }
        platform.set_file_system(file_system.get());
        platform.set_font_loader(font_loader.get());
        platform.set_gpu_driver(nullptr);

        HRESULT deferred_context_result = E_FAIL;
        if (!this->config.force_cpu_rendering) {
            try {
                ComPtr<ID3D11DeviceContext> new_deferred_context;
                deferred_context_result = device->CreateDeferredContext(
                    0, new_deferred_context.GetAddressOf());
                if (SUCCEEDED(deferred_context_result) && new_deferred_context != nullptr) {
                    gpu_context = std::make_unique<GPUContextD3D11>(device, new_deferred_context.Get());
                    if (gpu_context->device() != nullptr && gpu_context->immediate_context() != nullptr) {
                        gpu_driver = std::make_unique<SkyUltraGPUDriver>(gpu_context.get());
                        platform.set_gpu_driver(gpu_driver.get());
                        renderer = ultralight::Renderer::Create();
                        gpu_enabled.store(renderer != nullptr, std::memory_order_relaxed);
                        if (gpu_enabled.load(std::memory_order_relaxed)) {
                            deferred_context = std::move(new_deferred_context);
                            deferred_gpu_enabled = true;
                        }
                    }
                }
            } catch (const std::exception& exception) {
                SKSE::log::warn("SkyUltra GPU renderer setup failed: {}", exception.what());
            } catch (...) {
                SKSE::log::warn("SkyUltra GPU renderer setup failed with an unknown exception.");
            }
        }

        if (!gpu_enabled.load(std::memory_order_relaxed)) {
            platform.set_gpu_driver(nullptr);
            gpu_driver.reset();
            gpu_context.reset();
            deferred_context.Reset();
            deferred_gpu_enabled = false;
            if (!this->config.force_cpu_rendering) {
                platform.set_font_loader(nullptr);
                platform.set_file_system(nullptr);
                font_loader.reset();
                file_system.reset();
                initialization_failed = true;
                if (FAILED(deferred_context_result)) {
                    SKSE::log::error(
                        "SkyUltra GPU mode could not create a D3D11 deferred context (HRESULT 0x{:08X}); refusing CPU fallback.",
                        static_cast<std::uint32_t>(deferred_context_result));
                } else {
                    SKSE::log::error(
                        "SkyUltra GPU mode could not initialize the Ultralight GPU renderer; refusing CPU fallback.");
                }
                return;
            }
            renderer = ultralight::Renderer::Create();
            if (!renderer) {
                platform.set_font_loader(nullptr);
                platform.set_file_system(nullptr);
                font_loader.reset();
                file_system.reset();
                initialization_failed = true;
                SKSE::log::error("SkyUltra could not initialize either its GPU or CPU Ultralight renderer.");
                return;
            }
            SKSE::log::info("SkyUltra CPU renderer initialized{}.",
                this->config.force_cpu_rendering ? " by user setting" : " as a fallback");
        }

        host_device = device;
        host_context = context;
        renderer->SetDisplayRefreshRate(0, 120.0);
        initialized = true;
        if (gpu_enabled.load(std::memory_order_relaxed)) {
            SKSE::log::info("SkyUltra Ultralight GPU renderer initialized on a D3D11 deferred context.");
        }
    }

    const auto view_create_start = std::chrono::steady_clock::now();
    std::vector<std::shared_ptr<ViewMailbox>> new_views;
    {
        const std::scoped_lock lock(pending_mutex);
        new_views.swap(pending_views);
    }
    for (auto& mailbox : new_views) {
        auto snapshot = TakeSnapshot(mailbox);
        if (snapshot.close_requested) {
            file_mounts->Remove(snapshot.file_mount_token);
            continue;
        }
        ultralight::ViewConfig view_config;
        view_config.is_accelerated = gpu_enabled.load(std::memory_order_relaxed);
        view_config.is_transparent = snapshot.config.transparent;
        view_config.enable_javascript = snapshot.config.enable_javascript;
        if (snapshot.config.mod_content_policy) {
            // Mod pages are mounted at file:///SkyUltraMods/<random-token>/... .
            // Ultralight otherwise rejects their local JS/CSS subresources before
            // FileSystem can serve them. Local disk reads remain confined by the
            // per-view mount in SkyUltraUltralightFileSystem, while the network
            // listener still enforces each mod's HTTP(S) host allowlist.
            view_config.allow_universal_access_from_file_urls = true;
        }
        view_config.enable_hidden_timer_throttling = true;
        view_config.max_render_fps = snapshot.config.max_fps;
        view_config.initial_focus = snapshot.focus_mode != InputMode::None;
        auto view = renderer->CreateView(
            std::max(snapshot.config.size.width, 1U),
            std::max(snapshot.config.size.height, 1U), view_config, nullptr);
        const bool accelerated = gpu_enabled.load(std::memory_order_relaxed);
        if (!view) {
            SKSE::log::error(
                "SkyUltra failed to create {} Ultralight view {}; refusing to fall back to another renderer.",
                accelerated ? "GPU" : "CPU", mailbox->id);
            file_mounts->Remove(snapshot.file_mount_token);
            continue;
        }
        auto bridge_listener = std::make_unique<SkyUltraBridgeListener>(
            mailbox->id, mailbox->page_bridge_callback, &mailbox->load_listener);
        mailbox->load_listener.LogStartupStage("native view created");
        view->set_view_listener(bridge_listener.get());
        std::unique_ptr<SkyUltraModNetworkListener> network_listener;
        if (snapshot.config.mod_content_policy) {
            network_listener = std::make_unique<SkyUltraModNetworkListener>(
                *snapshot.config.mod_content_policy);
            view->set_network_listener(network_listener.get());
        }
        view->set_load_listener(&mailbox->load_listener);
        const bool warmup_pending = accelerated && !snapshot.config.visible &&
            !snapshot.navigations.empty();
        // Keep the hidden GPU warm-up alive through the application shell mount (not
        // just DOMReady, which fires before imported React modules finish). This
        // lets the page execute its readiness RAFs before hiding. Bounded for
        // custom pages that do not opt into ui.pageReady. First-time painting does
        // not happen synchronously when the player opens the overlay.
        view->set_visible(snapshot.config.visible || warmup_pending);
        view->Resize(std::max(snapshot.config.size.width, 1U),
            std::max(snapshot.config.size.height, 1U), snapshot.config.scale);
        view->set_max_render_fps(snapshot.config.max_fps);
        if (snapshot.focus_mode != InputMode::None) view->Focus();
        for (const auto& url : snapshot.navigations) {
            LoadNavigation(*view, url, snapshot.file_mount_token, *file_mounts);
        }
        live_views.push_back({ std::move(mailbox), std::move(bridge_listener),
            std::move(network_listener), std::move(view), snapshot.config,
            snapshot.focus_mode, accelerated, warmup_pending, 0, {}, false, false, 0 });
    }
    const auto view_create_end = std::chrono::steady_clock::now();

    const auto finish_hidden_warmup = [](LiveView& live, bool rendered_frame) {
        if (!live.warmup_pending || !live.mailbox->load_listener.MainFrameFinished()) {
            return;
        }
        if(live.applied_config.id=="SkyUltra.Menu" && (++live.warmup_frames<3 || !live.mailbox->load_listener.HiddenWarmupComplete()))return;
        live.warmup_pending = false;
        if (!live.applied_config.visible) live.view->set_visible(false);
        if (rendered_frame) {
            SKSE::log::info("SkyUltra pre-rendered hidden GPU view {} before it is opened.",
                live.mailbox->id);
        } else {
            SKSE::log::warn("SkyUltra completed hidden GPU view {} warm-up without a drawable frame.",
                live.mailbox->id);
        }
    };

    const auto view_commands_start = std::chrono::steady_clock::now();
    std::array<double, kViewCommandStageCount> command_stage_frame_total_ms{};
    std::array<double, kViewCommandStageCount> command_stage_frame_max_ms{};
    std::array<ViewId, kViewCommandStageCount> command_stage_frame_max_view{};
    const auto record_command_stage = [&](ViewCommandStage stage, ViewId view_id,
                                          std::chrono::steady_clock::time_point start) {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        const auto index = static_cast<std::size_t>(stage);
        command_stage_frame_total_ms[index] += elapsed;
        if (elapsed > command_stage_frame_max_ms[index]) {
            command_stage_frame_max_ms[index] = elapsed;
            command_stage_frame_max_view[index] = view_id;
        }
    };
    const auto record_config_operation = [&](const char* name, ViewId view_id,
                                             std::chrono::steady_clock::time_point start) {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > performance_config_operation_max_ms) {
            performance_config_operation_max_ms = elapsed;
            performance_config_operation_max_view = view_id;
            performance_config_operation_max_name = name;
        }
    };
    for (auto live = live_views.begin(); live != live_views.end();) {
        auto stage_start = std::chrono::steady_clock::now();
        auto snapshot = TakeSnapshot(live->mailbox);
        record_command_stage(ViewCommandStage::Snapshot, live->mailbox->id, stage_start);
        if (snapshot.close_requested) {
            SetGpuFrame(live->mailbox, {}, false);
            file_mounts->Remove(snapshot.file_mount_token);
            live = live_views.erase(live);
            continue;
        }

        auto& view = *live->view;
        const auto old_config = live->applied_config;
        stage_start = std::chrono::steady_clock::now();
        if (snapshot.config.size.width != old_config.size.width ||
            snapshot.config.size.height != old_config.size.height ||
            snapshot.config.scale != old_config.scale) {
            const auto operation_start = std::chrono::steady_clock::now();
            view.Resize(std::max(snapshot.config.size.width, 1U),
                std::max(snapshot.config.size.height, 1U), snapshot.config.scale);
            record_config_operation("resize", live->mailbox->id, operation_start);
        }
        if (snapshot.config.visible) {
            live->warmup_pending = false;
        }
        if (snapshot.config.max_fps != old_config.max_fps) {
            const auto operation_start = std::chrono::steady_clock::now();
            view.set_max_render_fps(snapshot.config.max_fps);
            record_config_operation("max-fps", live->mailbox->id, operation_start);
        }
        live->applied_config = snapshot.config;
        if (snapshot.config.visible != old_config.visible) {
            live->applied_config.visible = old_config.visible;
        }
        record_command_stage(ViewCommandStage::Config, live->mailbox->id, stage_start);

        stage_start = std::chrono::steady_clock::now();
        for (const auto& url : snapshot.navigations) {
            LoadNavigation(view, url, snapshot.file_mount_token, *file_mounts);
        }
        record_command_stage(ViewCommandStage::Navigation, live->mailbox->id, stage_start);

        stage_start = std::chrono::steady_clock::now();
        record_command_stage(ViewCommandStage::Bridge, live->mailbox->id, stage_start);
        ++live;
    }
    const auto view_commands_end = std::chrono::steady_clock::now();
    ProcessAsyncViewWork();

    bool any_active_view = !new_views.empty();
    for (const auto& live : live_views) {
        if (live.applied_config.visible || live.warmup_pending) {
            any_active_view = true;
            break;
        }
    }
    has_active_views.store(any_active_view, std::memory_order_relaxed);
    if (!any_active_view) {
        renderer->Update();
        return;
    }

    const auto update_start = std::chrono::steady_clock::now();
    renderer->Update();
    const auto update_end = std::chrono::steady_clock::now();
    renderer->RefreshDisplay(0);
    const auto refresh_display_end = std::chrono::steady_clock::now();
    renderer->Render();
    const auto render_end = std::chrono::steady_clock::now();
    const auto gpu_submit_start = render_end;
    std::vector<PendingGpuFrame> pending_gpu_frames;
    bool gpu_commands_pending{};
    if (gpu_enabled.load(std::memory_order_relaxed) && gpu_driver) {
        gpu_commands_pending = gpu_driver->HasCommandsPending();
        if (gpu_commands_pending) {
            gpu_driver->DrawCommandList();
            const auto executed_commands = gpu_driver->batch_count();
            if (!logged_gpu_command_dispatch) {
                SKSE::log::info(
                    "SkyUltra recorded its first Ultralight GPU command list ({} commands) for deferred execution.",
                    executed_commands);
                logged_gpu_command_dispatch = true;
            }
        }
    }
    for (auto& live : live_views) {
        if (!live.accelerated) {
            auto* surface = live.view->surface();
            if (surface == nullptr || surface->width() == 0 || surface->height() == 0 ||
                surface->width() > 4096 || surface->height() > 4096) {
                continue;
            }

            const SizeU surface_size{ surface->width(), surface->height() };
            bool pending_frame{};
            {
                const std::scoped_lock lock(live.mailbox->mutex);
                pending_frame = live.mailbox->has_software_frame;
            }
            const bool resized = live.software_size.width != surface_size.width ||
                live.software_size.height != surface_size.height;
            const bool force_full = pending_frame || resized;
            const auto dirty_rect_count = surface->dirty_rect_count();
            if (dirty_rect_count == 0 && !force_full) {
                continue;
            }

            const auto row_bytes = surface->row_bytes();
            const auto required_bytes = static_cast<std::uint64_t>(row_bytes) * surface->height();
            if (row_bytes < surface->width() * 4U) {
                continue;
            }
            auto pixels = surface->LockPixelsSafe();
            const auto* source = static_cast<const std::uint8_t*>(pixels.data());
            if (source == nullptr || pixels.size() < required_bytes) {
                continue;
            }

            SoftwareFrame frame;
            frame.size = surface_size;
            frame.sequence = live.software_frame_sequence++;
            const auto patch_count = force_full ? 1U : dirty_rect_count;
            for (std::uint32_t i = 0; i < patch_count; ++i) {
                const auto dirty = force_full ?
                    ultralight::IntRect{ 0, 0, static_cast<int>(surface_size.width),
                        static_cast<int>(surface_size.height) } : surface->dirty_rect(i);
                const auto left = std::clamp(dirty.left, 0, static_cast<int>(frame.size.width));
                const auto top = std::clamp(dirty.top, 0, static_cast<int>(frame.size.height));
                const auto right = std::clamp(dirty.right, left, static_cast<int>(frame.size.width));
                const auto bottom = std::clamp(dirty.bottom, top, static_cast<int>(frame.size.height));
                if (right <= left || bottom <= top) {
                    continue;
                }

                PixelPatch patch;
                patch.rect = {
                    static_cast<std::uint32_t>(left),
                    static_cast<std::uint32_t>(top),
                    static_cast<std::uint32_t>(right - left),
                    static_cast<std::uint32_t>(bottom - top),
                };
                patch.stride = patch.rect.width * 4U;
                patch.bgra8.resize(static_cast<std::size_t>(patch.stride) * patch.rect.height);
                for (std::uint32_t row = 0; row < patch.rect.height; ++row) {
                    const auto source_offset =
                        static_cast<std::size_t>(top + static_cast<int>(row)) * row_bytes +
                        static_cast<std::size_t>(left) * 4U;
                    const auto patch_offset = static_cast<std::size_t>(row) * patch.stride;
                    std::memcpy(patch.bgra8.data() + patch_offset, source + source_offset, patch.stride);
                }
                frame.patches.push_back(std::move(patch));
            }
            if (!frame.patches.empty()) {
                surface->ClearDirtyBounds();
                live.software_size = surface_size;
                if (!live.logged_output_ready) {
                    SKSE::log::info("SkyUltra CPU view {} produced its first frame ({}x{}).",
                        live.mailbox->id, frame.size.width, frame.size.height);
                    live.logged_output_ready = true;
                }
                SetSoftwareFrame(live.mailbox, std::move(frame));
            }
            continue;
        }

        const auto target = live.view->render_target();
        if (target.is_empty) {
            if (live.applied_config.visible && !live.logged_target_failure) {
                SKSE::log::error("SkyUltra GPU view {} has an empty render target while visible.",
                    live.mailbox->id);
                live.logged_target_failure = true;
            }
            finish_hidden_warmup(live, false);
            SetGpuFrame(live.mailbox, {}, false);
            continue;
        }
        auto* srv = gpu_driver->GetViewTexture(target.texture_id);
        if (srv == nullptr || target.texture_width == 0 || target.texture_height == 0) {
            if (live.applied_config.visible && !live.logged_target_failure) {
                SKSE::log::error("SkyUltra GPU view {} has no usable texture (id {}, size {}x{}).",
                    live.mailbox->id, target.texture_id, target.texture_width, target.texture_height);
                live.logged_target_failure = true;
            }
            finish_hidden_warmup(live, false);
            SetGpuFrame(live.mailbox, {}, false);
            continue;
        }

        const auto left = std::clamp(static_cast<int>(std::floor(target.uv_coords.left * target.texture_width)),
            0, static_cast<int>(target.texture_width));
        const auto top = std::clamp(static_cast<int>(std::floor(target.uv_coords.top * target.texture_height)),
            0, static_cast<int>(target.texture_height));
        const auto right = std::clamp(static_cast<int>(std::ceil(target.uv_coords.right * target.texture_width)),
            left, static_cast<int>(target.texture_width));
        const auto bottom = std::clamp(static_cast<int>(std::ceil(target.uv_coords.bottom * target.texture_height)),
            top, static_cast<int>(target.texture_height));
        GpuFrame frame;
        frame.shader_resource_view = srv;
        frame.texture_size = { target.texture_width, target.texture_height };
        frame.source_rect = { static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(top),
            static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top) };
        if (frame.source_rect.width == 0 || frame.source_rect.height == 0) {
            if (live.applied_config.visible && !live.logged_target_failure) {
                SKSE::log::error(
                    "SkyUltra GPU view {} produced an empty UV region ({}x{} texture; UV {} {} {} {}).",
                    live.mailbox->id, target.texture_width, target.texture_height,
                    target.uv_coords.left, target.uv_coords.top,
                    target.uv_coords.right, target.uv_coords.bottom);
                live.logged_target_failure = true;
            }
            finish_hidden_warmup(live, false);
            SetGpuFrame(live.mailbox, {}, false);
            continue;
        }
        if (!live.logged_output_ready) {
            live.mailbox->load_listener.LogStartupStage("first GPU frame recorded");
            SKSE::log::info("SkyUltra GPU view {} produced its first frame (texture {}x{}, source {}x{}).",
                live.mailbox->id, target.texture_width, target.texture_height,
                frame.source_rect.width, frame.source_rect.height);
            live.logged_output_ready = true;
        }
        srv->AddRef();
        frame.lifetime = std::shared_ptr<void>(srv, [](void* resource) {
            static_cast<ID3D11ShaderResourceView*>(resource)->Release();
        });
        pending_gpu_frames.push_back({ live.mailbox, std::move(frame) });
        finish_hidden_warmup(live, true);
    }

    if (deferred_gpu_enabled && (gpu_commands_pending || !pending_gpu_frames.empty())) {
        auto packet = std::make_unique<RenderFramePacket>();
        packet->device = device;
        packet->gpu_frames = std::move(pending_gpu_frames);
        packet->command_lists = gpu_driver->TakeSplitCommandLists();
        if (gpu_commands_pending) {
            ComPtr<ID3D11CommandList> final_command_list;
            const auto finish_result = deferred_context->FinishCommandList(
                FALSE, final_command_list.GetAddressOf());
            if (FAILED(finish_result)) {
                if (!logged_deferred_finish_failure) {
                    SKSE::log::error(
                        "SkyUltra could not finish an Ultralight D3D11 command list (HRESULT 0x{:08X}).",
                        static_cast<std::uint32_t>(finish_result));
                    logged_deferred_finish_failure = true;
                }
                packet->command_lists.clear();
                packet->gpu_frames.clear();
            } else if (final_command_list != nullptr) {
                packet->command_lists.push_back(std::move(final_command_list));
            }
        }
        if (!packet->command_lists.empty() || !packet->gpu_frames.empty()) {
            in_progress_frame = std::move(packet);
        }
    }
    const auto gpu_submit_end = std::chrono::steady_clock::now();

    const auto frame_end = std::chrono::steady_clock::now();
    if (!performance_window_started) {
        performance_window_start = frame_end;
        performance_window_started = true;
        return;
    }

    const auto update_ms = std::chrono::duration<double, std::milli>(update_end - update_start).count();
    const auto refresh_display_ms = std::chrono::duration<double, std::milli>(
        refresh_display_end - update_end).count();
    const auto render_ms = std::chrono::duration<double, std::milli>(
        render_end - refresh_display_end).count();
    const auto gpu_submit_ms = std::chrono::duration<double, std::milli>(gpu_submit_end - gpu_submit_start).count();
    const auto cpu_pump_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
    const auto prepare_ms = std::chrono::duration<double, std::milli>(update_start - frame_start).count();
    const auto view_create_ms = std::chrono::duration<double, std::milli>(
        view_create_end - view_create_start).count();
    const auto view_commands_ms = std::chrono::duration<double, std::milli>(
        view_commands_end - view_commands_start).count();
    const auto collect_ms = std::chrono::duration<double, std::milli>(frame_end - gpu_submit_end).count();
    ++performance_frame_count;
    performance_cpu_total_ms += cpu_pump_ms;
    performance_cpu_max_ms = std::max(performance_cpu_max_ms, cpu_pump_ms);
    performance_prepare_total_ms += prepare_ms;
    performance_prepare_max_ms = std::max(performance_prepare_max_ms, prepare_ms);
    performance_view_create_total_ms += view_create_ms;
    performance_view_create_max_ms = std::max(performance_view_create_max_ms, view_create_ms);
    performance_view_commands_total_ms += view_commands_ms;
    performance_view_commands_max_ms = std::max(performance_view_commands_max_ms, view_commands_ms);
    for (std::size_t index = 0; index < kViewCommandStageCount; ++index) {
        performance_command_stage_total_ms[index] += command_stage_frame_total_ms[index];
        if (command_stage_frame_max_ms[index] > performance_command_stage_max_ms[index]) {
            performance_command_stage_max_ms[index] = command_stage_frame_max_ms[index];
            performance_command_stage_max_view[index] = command_stage_frame_max_view[index];
        }
    }
    performance_update_total_ms += update_ms;
    performance_update_max_ms = std::max(performance_update_max_ms, update_ms);
    performance_refresh_display_total_ms += refresh_display_ms;
    performance_refresh_display_max_ms = std::max(
        performance_refresh_display_max_ms, refresh_display_ms);
    performance_render_total_ms += render_ms;
    performance_render_max_ms = std::max(performance_render_max_ms, render_ms);
    performance_gpu_submit_total_ms += gpu_submit_ms;
    performance_gpu_submit_max_ms = std::max(performance_gpu_submit_max_ms, gpu_submit_ms);
    performance_collect_total_ms += collect_ms;
    performance_collect_max_ms = std::max(performance_collect_max_ms, collect_ms);

    const auto window_seconds = std::chrono::duration<double>(
        frame_end - performance_window_start).count();
    if (window_seconds < 10.0) {
        return;
    }

    const auto visible_views = std::count_if(live_views.begin(), live_views.end(),
        [](const LiveView& live) { return live.applied_config.visible; });
    std::string visible_view_sizes;
    for (const auto& live : live_views) {
        if (!live.applied_config.visible) {
            continue;
        }
        if (!visible_view_sizes.empty()) {
            visible_view_sizes += ",";
        }
        visible_view_sizes += std::to_string(live.applied_config.size.width) + "x" +
            std::to_string(live.applied_config.size.height) + "@" +
            std::to_string(live.applied_config.max_fps);
    }

    const auto sample_count = static_cast<double>(performance_frame_count);
    SKSE::log::info(
        "SkyUltra performance ({} renderer, {}/{} visible views [{}]): {:.1f} render callbacks/s; "
        "CPU pump avg/max {:.3f}/{:.3f} ms; Prepare {:.3f}/{:.3f}; "
        "View create {:.3f}/{:.3f}; view commands {:.3f}/{:.3f}; async view work {:.3f}/{:.3f}; "
        "cmd stages snapshot {:.3f}/{:.3f}@{} config {:.3f}/{:.3f}@{} nav {:.3f}/{:.3f}@{} "
        "input {:.3f}/{:.3f}@{} batch max {}@{} slow input {:.3f} ms {}@{} "
        "slow config {:.3f} ms {}@{} bridge {:.3f}/{:.3f}@{}; Update {:.3f}/{:.3f}; "
        "RefreshDisplay {:.3f}/{:.3f}; "
        "Render {:.3f}/{:.3f}; GPU submit {:.3f}/{:.3f}; Collect {:.3f}/{:.3f} ms (avg/max).",
        gpu_enabled.load(std::memory_order_relaxed) ? "GPU" : "CPU",
        visible_views, live_views.size(), visible_view_sizes,
        sample_count / window_seconds, performance_cpu_total_ms / sample_count,
        performance_cpu_max_ms, performance_prepare_total_ms / sample_count,
        performance_prepare_max_ms, performance_view_create_total_ms / sample_count,
        performance_view_create_max_ms, performance_view_commands_total_ms / sample_count,
        performance_view_commands_max_ms, performance_async_view_work_total_ms / sample_count,
        performance_async_view_work_max_ms,
        performance_command_stage_total_ms[static_cast<std::size_t>(ViewCommandStage::Snapshot)] / sample_count,
        performance_command_stage_max_ms[static_cast<std::size_t>(ViewCommandStage::Snapshot)],
        performance_command_stage_max_view[static_cast<std::size_t>(ViewCommandStage::Snapshot)],
        performance_command_stage_total_ms[static_cast<std::size_t>(ViewCommandStage::Config)] / sample_count,
        performance_command_stage_max_ms[static_cast<std::size_t>(ViewCommandStage::Config)],
        performance_command_stage_max_view[static_cast<std::size_t>(ViewCommandStage::Config)],
        performance_command_stage_total_ms[static_cast<std::size_t>(ViewCommandStage::Navigation)] / sample_count,
        performance_command_stage_max_ms[static_cast<std::size_t>(ViewCommandStage::Navigation)],
        performance_command_stage_max_view[static_cast<std::size_t>(ViewCommandStage::Navigation)],
        performance_command_stage_total_ms[static_cast<std::size_t>(ViewCommandStage::Input)] / sample_count,
        performance_command_stage_max_ms[static_cast<std::size_t>(ViewCommandStage::Input)],
        performance_command_stage_max_view[static_cast<std::size_t>(ViewCommandStage::Input)],
        performance_input_batch_max, performance_input_batch_max_view,
        performance_input_command_max_ms, performance_input_command_max_name,
        performance_input_command_max_view,
        performance_config_operation_max_ms, performance_config_operation_max_name,
        performance_config_operation_max_view,
        performance_command_stage_total_ms[static_cast<std::size_t>(ViewCommandStage::Bridge)] / sample_count,
        performance_command_stage_max_ms[static_cast<std::size_t>(ViewCommandStage::Bridge)],
        performance_command_stage_max_view[static_cast<std::size_t>(ViewCommandStage::Bridge)],
        performance_update_total_ms / sample_count,
        performance_update_max_ms, performance_refresh_display_total_ms / sample_count,
        performance_refresh_display_max_ms, performance_render_total_ms / sample_count,
        performance_render_max_ms, performance_gpu_submit_total_ms / sample_count,
        performance_gpu_submit_max_ms, performance_collect_total_ms / sample_count,
        performance_collect_max_ms);

    performance_window_start = frame_end;
    performance_frame_count = 0;
    performance_cpu_total_ms = 0.0;
    performance_cpu_max_ms = 0.0;
    performance_prepare_total_ms = 0.0;
    performance_prepare_max_ms = 0.0;
    performance_view_create_total_ms = 0.0;
    performance_view_create_max_ms = 0.0;
    performance_view_commands_total_ms = 0.0;
    performance_view_commands_max_ms = 0.0;
    performance_async_view_work_total_ms = 0.0;
    performance_async_view_work_max_ms = 0.0;
    performance_command_stage_total_ms.fill(0.0);
    performance_command_stage_max_ms.fill(0.0);
    performance_command_stage_max_view.fill(0);
    performance_input_batch_max = 0;
    performance_input_batch_max_view = 0;
    performance_input_command_max_ms = 0.0;
    performance_input_command_max_view = 0;
    performance_input_command_max_name = "none";
    performance_config_operation_max_ms = 0.0;
    performance_config_operation_max_view = 0;
    performance_config_operation_max_name = "none";
    performance_update_total_ms = 0.0;
    performance_update_max_ms = 0.0;
    performance_refresh_display_total_ms = 0.0;
    performance_refresh_display_max_ms = 0.0;
    performance_render_total_ms = 0.0;
    performance_render_max_ms = 0.0;
    performance_gpu_submit_total_ms = 0.0;
    performance_gpu_submit_max_ms = 0.0;
    performance_collect_total_ms = 0.0;
    performance_collect_max_ms = 0.0;
}

void UltralightRuntime::State::RestoreViewsAfterDeviceChange()
{
    std::vector<std::shared_ptr<ViewMailbox>> views_to_restore;
    views_to_restore.reserve(live_views.size());
    for (auto& live : live_views) {
        const auto& mailbox = live.mailbox;
        if (mailbox == nullptr) {
            continue;
        }

        {
            const std::scoped_lock lock(mailbox->mutex);
            mailbox->frame = {};
            mailbox->has_frame = false;
            mailbox->software_frame = {};
            mailbox->has_software_frame = false;
            if (mailbox->close_requested) {
                continue;
            }

            mailbox->pending_navigations.clear();
            if (!mailbox->current_url.empty()) {
                mailbox->pending_navigations.push_back(mailbox->current_url);
            }
            mailbox->pending_input.clear();
        }
        mailbox->load_listener.ResetMainFrameFinished();
        SKSE::log::info("SkyUltra queued browser view {} for recreation at its last requested URL.",
            mailbox->id);
        views_to_restore.push_back(mailbox);
    }

    live_views.clear();
    if (!views_to_restore.empty()) {
        const std::scoped_lock lock(pending_mutex);
        pending_views.insert(pending_views.end(), views_to_restore.begin(), views_to_restore.end());
    }
}

void UltralightRuntime::State::WorkerMain()
{
    owner_thread = std::this_thread::get_id();
    for (;;) {
        std::unique_lock lock(worker_mutex);
        const bool can_self_tick = initialized && !initialization_failed &&
            requested_device != nullptr && requested_context != nullptr &&
            has_active_views.load(std::memory_order_relaxed);
        if (can_self_tick) {
            worker_wakeup.wait_for(lock, std::chrono::microseconds(8333), [this] {
                return frame_pending || async_view_work_pending || stop_requested;
            });
            if (!frame_pending && !async_view_work_pending && !stop_requested &&
                has_active_views.load(std::memory_order_relaxed)) {
                frame_pending = true;
            }
        } else {
            worker_wakeup.wait(lock, [this] {
                return frame_pending || async_view_work_pending || stop_requested;
            });
        }
        if (stop_requested && !frame_pending) {
            break;
        }

        const bool process_frame = frame_pending;
        auto* device = process_frame ? requested_device : nullptr;
        auto* context = process_frame ? requested_context : nullptr;
        if (process_frame) {
            frame_pending = false;
            async_view_work_pending = false;
        } else {
            async_view_work_pending = false;
        }
        worker_busy = true;
        lock.unlock();

        try {
            if (process_frame) {
                ProcessFrame(device, context);
            } else {
                ProcessAsyncViewWork();
            }
        } catch (const std::exception& exception) {
            initialization_failed = true;
            SKSE::log::error("SkyUltra Ultralight WebView thread failed: {}", exception.what());
            if (process_frame) {
                Cleanup();
            }
        } catch (...) {
            initialization_failed = true;
            SKSE::log::error("SkyUltra Ultralight WebView thread failed with an unknown exception.");
            if (process_frame) {
                Cleanup();
            }
        }

        auto produced_frame = process_frame ? std::move(in_progress_frame) : nullptr;

        const bool pending_async_view_work = !initialization_failed &&
            HasPendingAsyncViewWork();
        lock.lock();
        worker_busy = false;
        if (produced_frame != nullptr && !stop_requested) {
            if (completed_frame != nullptr && completed_frame->device == produced_frame->device) {
                for (auto& cmd : produced_frame->command_lists) {
                    completed_frame->command_lists.push_back(std::move(cmd));
                }
                completed_frame->gpu_frames = std::move(produced_frame->gpu_frames);
            } else {
                completed_frame = std::move(produced_frame);
            }
        }
        if (pending_async_view_work && !stop_requested) {
            async_view_work_pending = true;
        }
        lock.unlock();
        if (pending_async_view_work) {
            worker_wakeup.notify_one();
        }
    }

    Cleanup();
}

void UltralightRuntime::State::Cleanup() noexcept
{
    for (auto& live : live_views) {
        if (live.view) {
            live.view->set_visible(false);
        }
    }
    live_views.clear();
    file_mounts->Clear();
    // Destroy views and renderer while their platform services are alive.
    // Renderer teardown may release compositor resources through the GPU driver.
    renderer = nullptr;
    if (gpu_driver || font_loader || file_system) {
        auto& platform = ultralight::Platform::instance();
        platform.set_gpu_driver(nullptr);
        platform.set_font_loader(nullptr);
        platform.set_file_system(nullptr);
    }
    gpu_driver.reset();
    gpu_context.reset();
    deferred_context.Reset();
    deferred_gpu_enabled = false;
    in_progress_frame.reset();
    {
        const std::scoped_lock worker_lock(worker_mutex);
        completed_frame.reset();
    }
    font_loader.reset();
    file_system.reset();
    host_device = nullptr;
    host_context = nullptr;
    initialized = false;
    gpu_enabled.store(false, std::memory_order_relaxed);
    logged_gpu_command_dispatch = false;
    logged_deferred_finish_failure = false;
    performance_window_started = false;
    performance_frame_count = 0;
    performance_cpu_total_ms = 0.0;
    performance_cpu_max_ms = 0.0;
    performance_prepare_total_ms = 0.0;
    performance_prepare_max_ms = 0.0;
    performance_view_create_total_ms = 0.0;
    performance_view_create_max_ms = 0.0;
    performance_view_commands_total_ms = 0.0;
    performance_view_commands_max_ms = 0.0;
    performance_async_view_work_total_ms = 0.0;
    performance_async_view_work_max_ms = 0.0;
    performance_command_stage_total_ms.fill(0.0);
    performance_command_stage_max_ms.fill(0.0);
    performance_command_stage_max_view.fill(0);
    performance_input_batch_max = 0;
    performance_input_batch_max_view = 0;
    performance_input_command_max_ms = 0.0;
    performance_input_command_max_view = 0;
    performance_input_command_max_name = "none";
    performance_config_operation_max_ms = 0.0;
    performance_config_operation_max_view = 0;
    performance_config_operation_max_name = "none";
    performance_update_total_ms = 0.0;
    performance_update_max_ms = 0.0;
    performance_refresh_display_total_ms = 0.0;
    performance_refresh_display_max_ms = 0.0;
    performance_render_total_ms = 0.0;
    performance_render_max_ms = 0.0;
    performance_gpu_submit_total_ms = 0.0;
    performance_gpu_submit_max_ms = 0.0;
    performance_collect_total_ms = 0.0;
    performance_collect_max_ms = 0.0;
}

std::unique_ptr<IWebView> UltralightRuntime::CreateView(
    ViewId id, const ViewConfig& config, std::string_view initial_url)
{
    const auto requested_at = std::chrono::steady_clock::now();
    std::filesystem::path mount_root;
    std::filesystem::path mount_entry;
    bool asset_access_enabled = true;
    bool needs_file_mount{};
    if (config.mod_content_policy) {
        mount_root = config.mod_content_policy->root_directory;
        mount_entry = config.mod_content_policy->entry_file;
        asset_access_enabled = config.mod_content_policy->asset_access_enabled;
        needs_file_mount = true;
    } else if (!initial_url.empty()) {
        const ultralight::URL url(ultralight::String(initial_url.data(), initial_url.size()));
        if (url && url.scheme() == "file") {
            const auto path_text = url.ToFilePath().utf8();
            std::error_code error;
            if (path_text.empty()) {
                SKSE::log::error("SkyUltra rejected local file URL for Ultralight view {}.", id);
                return {};
            }
            mount_entry = std::filesystem::canonical(
                std::filesystem::u8path(std::string(path_text.data(), path_text.length())), error);
            if (error || !std::filesystem::is_regular_file(mount_entry, error) || error) {
                SKSE::log::error("SkyUltra rejected missing local file for Ultralight view {}.", id);
                return {};
            }
            mount_root = mount_entry.parent_path();
            needs_file_mount = true;
        }
    }

    std::string file_mount_token;
    if (needs_file_mount) {
        file_mount_token = state_->file_mounts->AddMount(
            mount_root, mount_entry, asset_access_enabled);
        if (file_mount_token.empty()) {
            SKSE::log::error("SkyUltra could not create a confined Ultralight file mount for view {}.", id);
            return {};
        }
    }

    const std::weak_ptr<State> weak_state = state_;
    auto mailbox = std::make_shared<ViewMailbox>(id, config, std::string(initial_url),
        std::move(file_mount_token),
        [weak_state] {
            if (const auto state = weak_state.lock()) {
                state->RequestAsyncViewWork();
            }
        }, requested_at);
    {
        const std::scoped_lock lock(state_->callback_mutex);
        mailbox->page_bridge_callback = state_->page_bridge_callback;
    }
    {
        const std::scoped_lock lock(state_->pending_mutex);
        state_->pending_views.push_back(mailbox);
    }
    state_->has_active_views.store(true, std::memory_order_relaxed);
    state_->worker_wakeup.notify_one();
    return std::make_unique<UltralightView>(std::move(mailbox));
}

}  // namespace skyultra
