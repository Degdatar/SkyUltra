#pragma once

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>

namespace skyultra {

inline constexpr char kLiveCompassPidSwitch[] = "skyultra-live-hud-pid";

[[nodiscard]] inline std::wstring LiveCompassMappingName(DWORD host_pid)
{
    return L"Local\\SkyUltra_LiveHud_" + std::to_wstring(host_pid);
}

struct alignas(64) LiveCompassSharedBlock {
    std::atomic<std::uint64_t> seq;
    double heading_deg;
    double timestamp_ms;
    std::uint64_t frame_index;
    std::uint32_t valid;
    std::uint32_t reserved;
    std::array<std::uint8_t, 24> padding;
};

static_assert(sizeof(LiveCompassSharedBlock) == 64);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

struct LiveCompassSample {
    double heading_deg{};
    double timestamp_ms{};
    std::uint64_t frame_index{};
    bool valid{};
};

namespace detail {

[[nodiscard]] inline bool ReadLiveCompassBlock(
    const LiveCompassSharedBlock* block, LiveCompassSample& out) noexcept
{
    out = {};
    if (block == nullptr) {
        return false;
    }
    for (int attempt = 0; attempt < 4; ++attempt) {
        const auto s1 = block->seq.load(std::memory_order_acquire);
        if ((s1 & 1ULL) != 0ULL) {
            continue;
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        const double heading = block->heading_deg;
        const double timestamp_ms = block->timestamp_ms;
        const std::uint64_t frame_index = block->frame_index;
        const std::uint32_t valid_flag = block->valid;
        std::atomic_thread_fence(std::memory_order_acquire);
        const auto s2 = block->seq.load(std::memory_order_acquire);
        if (s1 == s2) {
            out.heading_deg = heading;
            out.timestamp_ms = timestamp_ms;
            out.frame_index = frame_index;
            out.valid = valid_flag != 0U && std::isfinite(heading);
            return out.valid;
        }
    }
    return false;
}

class HostLiveCompassStorage final {
public:
    HostLiveCompassStorage() noexcept = default;

    // Deliberately leak mapping/view at process exit so late reader threads never fault.
    ~HostLiveCompassStorage() = default;

    HostLiveCompassStorage(const HostLiveCompassStorage&) = delete;
    HostLiveCompassStorage& operator=(const HostLiveCompassStorage&) = delete;

    [[nodiscard]] bool EnsureSharedMemory() noexcept
    {
        if (mapping_ready_.load(std::memory_order_acquire)) {
            return true;
        }
        const std::scoped_lock lock(mutex_);
        if (mapping_ready_.load(std::memory_order_relaxed)) {
            return true;
        }
        const auto name = LiveCompassMappingName(GetCurrentProcessId());
        HANDLE mapping = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(sizeof(LiveCompassSharedBlock)), name.c_str());
        if (mapping == nullptr) {
            return false;
        }
        void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LiveCompassSharedBlock));
        if (view == nullptr) {
            CloseHandle(mapping);
            return false;
        }
        auto* block = static_cast<LiveCompassSharedBlock*>(view);
        block->seq.store(0ULL, std::memory_order_relaxed);
        block->heading_deg = 0.0;
        block->timestamp_ms = 0.0;
        block->frame_index = 0ULL;
        block->valid = 0U;
        block->reserved = 0U;
        block->padding.fill(0U);

        mapping_ = mapping;
        mapped_ = block;
        active_.store(block, std::memory_order_release);
        mapping_ready_.store(true, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool IsSharedMemoryReady() const noexcept
    {
        return mapping_ready_.load(std::memory_order_acquire);
    }

    void Publish(double heading_deg, bool valid, std::uint64_t frame_index) noexcept
    {
        auto* block = active_.load(std::memory_order_acquire);
        if (block == nullptr) {
            return;
        }
        const bool effective_valid = valid && std::isfinite(heading_deg);
        if (!effective_valid) {
            if (!last_valid_.exchange(false, std::memory_order_acq_rel) && block->valid == 0U) {
                return;
            }
            const auto seq = block->seq.load(std::memory_order_relaxed);
            block->seq.store(seq + 1ULL, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_release);
            block->heading_deg = 0.0;
            block->timestamp_ms = 0.0;
            block->frame_index = frame_index;
            block->valid = 0U;
            std::atomic_thread_fence(std::memory_order_release);
            block->seq.store(seq + 2ULL, std::memory_order_release);
            return;
        }

        double normalized = std::fmod(heading_deg, 360.0);
        if (normalized < 0.0) {
            normalized += 360.0;
        }
        const double ts_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        const auto seq = block->seq.load(std::memory_order_relaxed);
        block->seq.store(seq + 1ULL, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_release);
        block->heading_deg = normalized;
        block->timestamp_ms = ts_ms;
        block->frame_index = frame_index;
        block->valid = 1U;
        std::atomic_thread_fence(std::memory_order_release);
        block->seq.store(seq + 2ULL, std::memory_order_release);
        last_valid_.store(true, std::memory_order_release);
    }

    [[nodiscard]] bool Read(LiveCompassSample& out) const noexcept
    {
        return ReadLiveCompassBlock(active_.load(std::memory_order_acquire), out);
    }

private:
    std::mutex mutex_;
    HANDLE mapping_{};
    LiveCompassSharedBlock* mapped_{};
    LiveCompassSharedBlock fallback_{};
    std::atomic<LiveCompassSharedBlock*> active_{ &fallback_ };
    std::atomic_bool mapping_ready_{ false };
    std::atomic_bool last_valid_{ false };
};

[[nodiscard]] inline HostLiveCompassStorage& GetHostLiveCompassStorage() noexcept
{
    static HostLiveCompassStorage* storage = new HostLiveCompassStorage();
    return *storage;
}

}  // namespace detail

[[nodiscard]] inline bool EnsureLiveCompassSharedMemory() noexcept
{
    return detail::GetHostLiveCompassStorage().EnsureSharedMemory();
}

[[nodiscard]] inline bool IsLiveCompassSharedMemoryReady() noexcept
{
    return detail::GetHostLiveCompassStorage().IsSharedMemoryReady();
}

inline void PublishLiveCompassHeading(
    double heading_deg, bool valid, std::uint64_t frame_index) noexcept
{
    detail::GetHostLiveCompassStorage().Publish(heading_deg, valid, frame_index);
}

[[nodiscard]] inline bool ReadLiveCompassHeadingInProcess(LiveCompassSample& out) noexcept
{
    return detail::GetHostLiveCompassStorage().Read(out);
}

class LiveCompassClient final {
public:
    LiveCompassClient() noexcept = default;
    ~LiveCompassClient() = default;

    LiveCompassClient(const LiveCompassClient&) = delete;
    LiveCompassClient& operator=(const LiveCompassClient&) = delete;

    [[nodiscard]] bool Attach(DWORD host_pid) noexcept
    {
        if (host_pid == 0) {
            return false;
        }
        if (block_.load(std::memory_order_acquire) != nullptr) {
            return true;
        }
        const std::scoped_lock lock(mutex_);
        if (block_.load(std::memory_order_relaxed) != nullptr) {
            return true;
        }
        const auto name = LiveCompassMappingName(host_pid);
        HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
        if (mapping == nullptr) {
            return false;
        }
        const void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(LiveCompassSharedBlock));
        if (view == nullptr) {
            CloseHandle(mapping);
            return false;
        }
        mapping_ = mapping;
        block_.store(static_cast<const LiveCompassSharedBlock*>(view), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool Read(LiveCompassSample& out) const noexcept
    {
        return detail::ReadLiveCompassBlock(block_.load(std::memory_order_acquire), out);
    }

private:
    std::mutex mutex_;
    HANDLE mapping_{};
    std::atomic<const LiveCompassSharedBlock*> block_{ nullptr };
};

}  // namespace skyultra
