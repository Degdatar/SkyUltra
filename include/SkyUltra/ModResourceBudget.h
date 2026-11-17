#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace skyultra {

// Shared by every native view belonging to a registered mod (legacy owner 0
// shares one budget). Reservations include pending views and in-flight messages.
class ModResourceBudget {
public:
    static constexpr std::size_t kMaxViews = 16;
    static constexpr std::size_t kMaxSurfaceBytes = 128U * 1024U * 1024U;
    static constexpr std::size_t kMaxMessages = 4096;
    static constexpr std::size_t kMaxMessageBytes = 128U * 1024U * 1024U;
    enum class Admission { Accepted, Full, Suspended };
    struct Stats {
        std::size_t views{}, surface_bytes{};
        std::array<std::size_t, 2> messages{}, bytes{};
        std::uint64_t rejected{};
        bool suspended{};
    };
    [[nodiscard]] static constexpr bool Fits(std::size_t used, std::size_t amount, std::size_t limit) noexcept {
        return amount <= limit && used <= limit - amount;
    }
    [[nodiscard]] static constexpr std::size_t SurfaceBytes(std::uint32_t width, std::uint32_t height) noexcept {
        if (width == 0 || height == 0 || width > 8192 || height > 8192) return kMaxSurfaceBytes + 1;
        return static_cast<std::size_t>(width) * height * 4;
    }
private:
    struct State { std::mutex mutex; Stats stats; std::atomic_bool suspended{}; };
    std::shared_ptr<State> state_{std::make_shared<State>()};
public:
    class ViewLease {
        friend class ModResourceBudget;
        std::shared_ptr<State> owner_;
        std::size_t bytes_{};
    public:
        ViewLease() = default;
        ViewLease(const ViewLease&) = delete;
        ViewLease& operator=(const ViewLease&) = delete;
        ~ViewLease() {
            if (!owner_) return;
            const std::scoped_lock lock(owner_->mutex);
            --owner_->stats.views; owner_->stats.surface_bytes -= bytes_;
        }
        [[nodiscard]] bool Resize(std::uint32_t width, std::uint32_t height) {
            const auto bytes = SurfaceBytes(width, height);
            const std::scoped_lock lock(owner_->mutex);
            auto& stats = owner_->stats;
            if (!Fits(stats.surface_bytes - bytes_, bytes, kMaxSurfaceBytes)) { ++stats.rejected; return false; }
            stats.surface_bytes = stats.surface_bytes - bytes_ + bytes; bytes_ = bytes;
            return true;
        }
    };
    [[nodiscard]] std::shared_ptr<ViewLease> ReserveView(std::uint32_t width, std::uint32_t height) {
        const auto bytes = SurfaceBytes(width, height);
        auto lease = std::make_shared<ViewLease>();
        const std::scoped_lock lock(state_->mutex);
        auto& stats = state_->stats;
        if (stats.suspended || stats.views >= kMaxViews || !Fits(stats.surface_bytes, bytes, kMaxSurfaceBytes)) {
            ++stats.rejected; return {};
        }
        lease->owner_ = state_; lease->bytes_ = bytes;
        ++stats.views; stats.surface_bytes += bytes;
        return lease;
    }
    [[nodiscard]] Admission ReserveMessages(bool replies, std::size_t count, std::size_t bytes) {
        const std::scoped_lock lock(state_->mutex);
        auto& stats = state_->stats;
        if (stats.suspended) return Admission::Suspended;
        const auto lane = replies ? 1U : 0U;
        if (!Fits(stats.messages[lane], count, kMaxMessages) || !Fits(stats.bytes[lane], bytes, kMaxMessageBytes)) {
            ++stats.rejected; return Admission::Full;
        }
        stats.messages[lane] += count; stats.bytes[lane] += bytes;
        return Admission::Accepted;
    }
    void ReleaseMessages(bool replies, std::size_t count, std::size_t bytes) {
        const std::scoped_lock lock(state_->mutex);
        const auto lane = replies ? 1U : 0U;
        state_->stats.messages[lane] -= count; state_->stats.bytes[lane] -= bytes;
    }
    void SetSuspended(bool suspended) {
        const std::scoped_lock lock(state_->mutex);
        state_->stats.suspended = suspended; state_->suspended.store(suspended, std::memory_order_release);
    }
    [[nodiscard]] bool IsSuspended() const noexcept { return state_->suspended.load(std::memory_order_acquire); }
    [[nodiscard]] Stats Snapshot() const { const std::scoped_lock lock(state_->mutex); return state_->stats; }
};
} // namespace skyultra
