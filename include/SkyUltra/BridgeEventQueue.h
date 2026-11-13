#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skyultra {

// A view owns accepted messages before it has a browser. Both engines consume
// this same mailbox; no second game-thread task can silently lose acceptance.
class BridgeEventQueue final {
public:
    static constexpr std::size_t kMaxPendingEvents = 1024;
    static constexpr std::size_t kMaxPendingBytes = 64U * 1024U * 1024U;
    static constexpr std::size_t kMaxControlEvents = 16;
    static constexpr std::size_t kMaxControlBytes = 64U * 1024U;
    static constexpr std::size_t kBatchEvents = 8;
    static constexpr std::size_t kBatchBytes = 256U * 1024U;
    using Clock = std::chrono::steady_clock;
    [[nodiscard]] static constexpr bool Fits(std::size_t count, std::size_t used,
        std::size_t bytes, std::size_t replaced_bytes = 0, bool control = false, bool replacing = false) noexcept {
        const auto byte_limit = control ? kMaxControlBytes : kMaxPendingBytes;
        return replaced_bytes <= used && bytes <= byte_limit && used - replaced_bytes <= byte_limit - bytes &&
            (replacing || count < (control ? kMaxControlEvents : kMaxPendingEvents));
    }
    enum class Result { Accepted, Full, Closed };
    struct Event {
        std::string name;
        std::string payload_json;
        Clock::time_point queued_at{Clock::now()};
        bool control{};
        [[nodiscard]] std::size_t Bytes() const noexcept { return name.size() + payload_json.size(); }
    };
    struct Stats {
        std::size_t pending{}, bytes{}, in_flight{}, peak_pending{}, peak_bytes{};
        std::size_t controls{};
        std::uint64_t accepted{}, rejected{}, delivered{}, cancelled{}, coalesced{};
        double oldest_ms{}, max_dispatch_ms{};
    };
private:
    struct State {
        std::mutex mutex;
        std::deque<Event> events, controls;
        std::size_t regular_count{}, regular_bytes{}, control_count{}, control_bytes{}, in_flight{};
        std::uint64_t consumer{};
        bool closed{};
        bool controls_enabled{true};
        Stats totals;
        std::function<void()> wake;
    };
    std::shared_ptr<State> state_{std::make_shared<State>()};
    static bool IsControl(std::string_view name) noexcept {
        // Only complete host state is replaceable. Never coalesce custom
        // commands, deltas, input tokens, or pieces of a chunked transfer.
        return name == "skyultra.visibility" || name == "skyultra.inventoryMode" ||
            name == "skyultra.barterMode" || name == "skyultra.containerMode" ||
            name == "skyultra.magicMode" || name == "skyultra.favoritesMode" ||
            name == "skyultra.mannequinMode" || name == "skyultra.lootMode" ||
            name == "skyultra.pickpocketMode";
    }
public:
    explicit BridgeEventQueue(bool controls = true) { state_->controls_enabled = controls; }
    struct Batch {
        Batch() = default;
        Batch(const Batch&) = delete;
        Batch& operator=(const Batch&) = delete;
        std::vector<Event> events;
        bool executed{};
    private:
        friend class BridgeEventQueue;
        std::shared_ptr<State> owner;
    public:
        ~Batch() {
            if (!owner) return;
            const std::scoped_lock lock(owner->mutex);
            for (const auto& event : events) {
                if (event.control) { --owner->control_count; owner->control_bytes -= event.Bytes(); }
                else { --owner->regular_count; owner->regular_bytes -= event.Bytes(); }
            }
            owner->in_flight -= events.size();
            if (executed) owner->totals.delivered += events.size();
            else owner->totals.cancelled += events.size();
        }
    };
    // Moving a view between engines invalidates the previous consumer. Its late
    // destructor cannot close the new consumer's mailbox or clear its wakeup.
    [[nodiscard]] std::uint64_t Attach(std::function<void()> wake) {
        std::uint64_t consumer{};
        bool pending{};
        {
            const std::scoped_lock lock(state_->mutex);
            if (state_->closed) return 0;
            state_->wake = wake;
            consumer = ++state_->consumer;
            pending = !state_->events.empty() || !state_->controls.empty();
        }
        if (pending && wake) { try { wake(); } catch (...) {} }
        return consumer;
    }
    [[nodiscard]] Result Push(std::string_view name, std::string_view payload) {
        std::function<void()> wake;
        {
            const std::scoped_lock lock(state_->mutex);
            if (state_->closed) return Result::Closed;
            const bool control = state_->controls_enabled && IsControl(name);
            const auto bytes = name.size() + payload.size();
            auto& queue = control ? state_->controls : state_->events;
            auto& count = control ? state_->control_count : state_->regular_count;
            auto& used = control ? state_->control_bytes : state_->regular_bytes;
            auto previous = control ? std::find_if(queue.begin(), queue.end(), [&](const Event& event) {
                return event.name == name;
            }) : queue.end();
            const auto replaced_bytes = previous != queue.end() ? previous->Bytes() : 0U;
            if (!Fits(count, used, bytes, replaced_bytes, control, previous != queue.end())) {
                ++state_->totals.rejected;
                return Result::Full;
            }
            wake = state_->wake; // Any allocating copy fails before ownership changes.
            Event event{std::string(name), std::string(payload), Clock::now(), control};
            if (previous != queue.end()) {
                *previous = std::move(event);
                used = used - replaced_bytes + bytes;
                ++state_->totals.coalesced;
            } else {
                queue.push_back(std::move(event));
                ++count;
                used += bytes;
            }
            ++state_->totals.accepted;
            state_->totals.peak_pending = std::max(state_->totals.peak_pending,
                state_->regular_count + state_->control_count);
            state_->totals.peak_bytes = std::max(state_->totals.peak_bytes,
                state_->regular_bytes + state_->control_bytes);
        }
        // Ownership has transferred. A wakeup allocation failure must not turn
        // an accepted event into a reported failure and cause duplicate retries.
        if (wake) { try { wake(); } catch (...) {} }
        return Result::Accepted;
    }
    [[nodiscard]] bool Pending(std::uint64_t consumer) const {
        const std::scoped_lock lock(state_->mutex);
        return !state_->closed && consumer != 0 && consumer == state_->consumer &&
            (!state_->controls.empty() || !state_->events.empty());
    }
    [[nodiscard]] std::shared_ptr<Batch> Take(std::uint64_t consumer) {
        {
            const std::scoped_lock lock(state_->mutex);
            if (state_->closed || consumer == 0 || consumer != state_->consumer ||
                (state_->controls.empty() && state_->events.empty())) return {};
        }
        auto batch = std::make_shared<Batch>();
        batch->events.reserve(kBatchEvents);
        const std::scoped_lock lock(state_->mutex);
        if (state_->closed || consumer == 0 || consumer != state_->consumer ||
            (state_->controls.empty() && state_->events.empty())) return {};
        batch->owner = state_;
        std::size_t bytes{};
        while (batch->events.size() < kBatchEvents) {
            auto& queue = !state_->controls.empty() ? state_->controls : state_->events;
            if (queue.empty()) break;
            const auto next_bytes = queue.front().Bytes();
            if (!batch->events.empty() && bytes + next_bytes > kBatchBytes) break;
            bytes += next_bytes;
            batch->events.push_back(std::move(queue.front()));
            queue.pop_front();
            ++state_->in_flight;
        }
        // Reservations include in-flight messages until the engine submits or
        // acknowledges this batch, so draining cannot temporarily double memory.
        return batch;
    }
    void RecordDispatch(double milliseconds) {
        const std::scoped_lock lock(state_->mutex);
        state_->totals.max_dispatch_ms = std::max(state_->totals.max_dispatch_ms, milliseconds);
    }
    [[nodiscard]] Stats Snapshot() const {
        const std::scoped_lock lock(state_->mutex);
        auto stats = state_->totals;
        stats.pending = state_->regular_count + state_->control_count;
        stats.controls = state_->control_count;
        stats.bytes = state_->regular_bytes + state_->control_bytes;
        stats.in_flight = state_->in_flight;
        auto oldest = Clock::now();
        if (!state_->events.empty()) oldest = std::min(oldest, state_->events.front().queued_at);
        for (const auto& control : state_->controls) oldest = std::min(oldest, control.queued_at);
        stats.oldest_ms = std::chrono::duration<double, std::milli>(Clock::now() - oldest).count();
        return stats;
    }
    void Close() {
        const std::scoped_lock lock(state_->mutex);
        state_->closed = true;
        const auto discard = [&](std::deque<Event>& queue, std::size_t& count, std::size_t& bytes) {
            for (const auto& event : queue) { --count; bytes -= event.Bytes(); ++state_->totals.cancelled; }
            queue.clear();
        };
        discard(state_->events, state_->regular_count, state_->regular_bytes);
        discard(state_->controls, state_->control_count, state_->control_bytes);
        state_->wake = {};
    }
};

} // namespace skyultra
