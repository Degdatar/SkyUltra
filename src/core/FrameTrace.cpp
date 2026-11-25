#include "SkyUltra/FrameTrace.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace skyultra {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kPercentileWindow = 256;
constexpr std::size_t kMaxCompletedTraces = 64;
constexpr std::size_t kReturnedTraces = 32;
constexpr auto kInputSampleInterval = std::chrono::milliseconds(100);
constexpr auto kPendingTraceExpiry = std::chrono::milliseconds(4000);

struct RollingPercentiles {
    std::uint64_t total_samples{};
    double total_ms{};
    double overall_max_ms{};
    std::array<double, kPercentileWindow> window{};
    std::size_t next{};
    std::size_t count{};

    void Add(double ms) noexcept
    {
        if (!std::isfinite(ms) || ms < 0.0) {
            return;
        }
        ++total_samples;
        total_ms += ms;
        overall_max_ms = std::max(overall_max_ms, ms);
        window[next] = ms;
        next = (next + 1U) % window.size();
        count = std::min(count + 1U, window.size());
    }

    [[nodiscard]] PercentileSummary Summarize() const noexcept
    {
        PercentileSummary out;
        out.samples = total_samples;
        out.max_ms = overall_max_ms;
        if (count == 0) {
            return out;
        }
        std::array<double, kPercentileWindow> sorted = window;
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(count));
        double window_sum = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            window_sum += sorted[i];
        }
        const auto pick = [&](std::size_t pct) noexcept -> double {
            const std::size_t idx = (count * pct + 99U) / 100U;
            return sorted[std::min(count - 1U, idx == 0 ? 0U : idx - 1U)];
        };
        out.mean_ms = total_samples > 0 ? (total_ms / static_cast<double>(total_samples)) : (window_sum / static_cast<double>(count));
        out.p50_ms = pick(50U);
        out.p95_ms = pick(95U);
        out.p99_ms = pick(99U);
        out.max_ms = std::max(sorted[count - 1U], overall_max_ms);
        return out;
    }
};

struct TraceMetadata {
    std::string engine{ "unknown" };
    std::string mod_id{ "skyultra" };
};

struct PendingTrace {
    std::uint64_t id{};
    TraceKind kind{ TraceKind::InputClick };
    ViewId view_id{};
    std::string engine;
    std::string mod_id;
    bool visible{};
    bool splash_active{};
    std::uint64_t baseline_paint_sequence{};
    std::uint64_t paint_sequence{};
    Clock::time_point started_at{};
    Clock::time_point dispatched_at{};
    Clock::time_point painted_at{};
    bool has_dispatched{};
    bool has_painted{};
};

double DurationMs(Clock::time_point from, Clock::time_point to) noexcept
{
    if (from.time_since_epoch().count() == 0 || to.time_since_epoch().count() == 0 || to < from) {
        return 0.0;
    }
    return std::chrono::duration<double, std::milli>(to - from).count();
}

struct TraceRegistry {
    std::mutex mutex;
    std::uint64_t next_trace_id{ 1 };
    std::unordered_map<ViewId, TraceMetadata> view_meta;
    std::unordered_map<std::uint64_t, PendingTrace> pending_by_key;
    std::unordered_map<ViewId, Clock::time_point> last_input_sample;
    std::deque<FrameTraceRecord> completed_traces;
    std::array<RollingPercentiles, 6> kind_stats;

    std::uint64_t cef_full_copies{};
    std::uint64_t cef_dirty_copies{};
    std::uint64_t cef_dirty_fallbacks{};
    std::uint64_t cef_overlapping_pixels{};
    RollingPercentiles cef_frame_age;

    std::uint64_t ul_packets_replayed{};
    std::uint64_t ul_backpressure_waits{};
    std::uint64_t ul_total_command_lists{};
    std::uint32_t ul_max_command_lists{};
    double ul_oldest_packet_age_ms{};
    double ul_max_packet_age_ms{};
    RollingPercentiles ul_replay;

    std::uint64_t queue_tasks_executed{};
    std::uint64_t queue_budget_overruns{};
    RollingPercentiles queue_wait;
    RollingPercentiles queue_exec;
    std::string queue_slowest_task;
    std::string queue_slowest_mod;
    double queue_slowest_exec_ms{};

    std::uint64_t thumb_memory_hits{};
    std::uint64_t thumb_disk_hits{};
    std::uint64_t thumb_url_hits{};
    std::uint64_t thumb_gpu_captures{};
    RollingPercentiles thumb_disk_read;
    RollingPercentiles thumb_encode;
    RollingPercentiles thumb_pixel_convert;
    RollingPercentiles thumb_full_export;
    bool thumb_avx2_active{};
};

TraceRegistry& Registry()
{
    static TraceRegistry instance;
    return instance;
}

std::uint64_t PendingKey(ViewId view_id, TraceKind kind) noexcept
{
    const std::uint64_t bucket = (kind == TraceKind::WarmMenuOpen || kind == TraceKind::ColdMenuOpen) ? 0ULL : 1ULL;
    return (view_id << 1U) | bucket;
}

void PruneExpiredLocked(TraceRegistry& reg, Clock::time_point now)
{
    for (auto it = reg.pending_by_key.begin(); it != reg.pending_by_key.end();) {
        if (now - it->second.started_at > kPendingTraceExpiry) {
            it = reg.pending_by_key.erase(it);
        } else {
            ++it;
        }
    }
}

void BeginTraceInternal(ViewId view_id, TraceKind kind, bool visible, bool splash_active,
    std::uint64_t baseline_paint_seq, bool enforce_sample_rate)
{
    if (view_id == 0) {
        return;
    }
    const auto now = Clock::now();
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    PruneExpiredLocked(reg, now);

    const auto key = PendingKey(view_id, kind);
    if (enforce_sample_rate) {
        if (reg.pending_by_key.contains(key)) {
            return;
        }
        if (const auto last_it = reg.last_input_sample.find(view_id);
            last_it != reg.last_input_sample.end() && (now - last_it->second) < kInputSampleInterval) {
            return;
        }
        reg.last_input_sample[view_id] = now;
    }

    TraceMetadata meta;
    if (const auto meta_it = reg.view_meta.find(view_id); meta_it != reg.view_meta.end()) {
        meta = meta_it->second;
    }

    PendingTrace entry;
    entry.id = reg.next_trace_id++;
    entry.kind = kind;
    entry.view_id = view_id;
    entry.engine = meta.engine;
    entry.mod_id = meta.mod_id;
    entry.visible = visible;
    entry.splash_active = splash_active;
    entry.baseline_paint_sequence = baseline_paint_seq;
    entry.started_at = now;
    reg.pending_by_key[key] = std::move(entry);
}

}  // namespace

const char* TraceKindName(TraceKind kind) noexcept
{
    switch (kind) {
    case TraceKind::WarmMenuOpen: return "warm-menu-open";
    case TraceKind::ColdMenuOpen: return "cold-menu-open";
    case TraceKind::InputClick: return "input-click";
    case TraceKind::InputWheel: return "input-wheel";
    case TraceKind::InputKey: return "input-key";
    case TraceKind::InputFocus: return "input-focus";
    }
    return "unknown";
}

void RegisterTraceViewMetadata(ViewId view_id, std::string_view engine, std::string_view mod_id)
{
    if (view_id == 0) {
        return;
    }
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    auto& meta = reg.view_meta[view_id];
    if (!engine.empty()) {
        meta.engine.assign(engine);
    }
    if (!mod_id.empty()) {
        meta.mod_id.assign(mod_id);
    }
}

void UnregisterTraceViewMetadata(ViewId view_id)
{
    if (view_id == 0) {
        return;
    }
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    reg.view_meta.erase(view_id);
    reg.last_input_sample.erase(view_id);
    reg.pending_by_key.erase(PendingKey(view_id, TraceKind::WarmMenuOpen));
    reg.pending_by_key.erase(PendingKey(view_id, TraceKind::InputClick));
}

void TraceBeginMenuOpen(ViewId view_id, bool warm, bool splash_active, std::uint64_t baseline_paint_seq)
{
    BeginTraceInternal(
        view_id,
        warm ? TraceKind::WarmMenuOpen : TraceKind::ColdMenuOpen,
        true,
        splash_active,
        baseline_paint_seq,
        false);
}

void TraceBeginInput(ViewId view_id, TraceKind kind, bool visible, std::uint64_t baseline_paint_seq)
{
    if (!visible) {
        return;
    }
    BeginTraceInternal(view_id, kind, visible, false, baseline_paint_seq, true);
}

void TraceMarkDispatched(ViewId view_id, TraceKind kind)
{
    if (view_id == 0) {
        return;
    }
    const auto now = Clock::now();
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    const auto key = PendingKey(view_id, kind);
    const auto it = reg.pending_by_key.find(key);
    if (it != reg.pending_by_key.end() && it->second.kind == kind && !it->second.has_dispatched) {
        it->second.dispatched_at = now;
        it->second.has_dispatched = true;
    }
}

void TraceMarkPainted(ViewId view_id, std::uint64_t paint_sequence)
{
    if (view_id == 0) {
        return;
    }
    const auto now = Clock::now();
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    for (const auto bucket : { 0ULL, 1ULL }) {
        const auto it = reg.pending_by_key.find((view_id << 1U) | bucket);
        if (it == reg.pending_by_key.end()) {
            continue;
        }
        auto& trace = it->second;
        if (!trace.has_painted && paint_sequence > trace.baseline_paint_sequence) {
            if (!trace.has_dispatched) {
                trace.dispatched_at = now;
                trace.has_dispatched = true;
            }
            trace.painted_at = now;
            trace.paint_sequence = paint_sequence;
            trace.has_painted = true;
        }
    }
}

void TraceMarkComposited(ViewId view_id, std::uint64_t paint_sequence, std::uint64_t composite_frame_seq)
{
    if (view_id == 0) {
        return;
    }
    const auto now = Clock::now();
    const auto now_ms = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    for (const auto bucket : { 0ULL, 1ULL }) {
        const auto key = (view_id << 1U) | bucket;
        const auto it = reg.pending_by_key.find(key);
        if (it == reg.pending_by_key.end()) {
            continue;
        }
        auto trace = std::move(it->second);
        const bool is_menu_open = (trace.kind == TraceKind::WarmMenuOpen || trace.kind == TraceKind::ColdMenuOpen);
        const bool ready_to_complete = trace.has_painted && paint_sequence >= trace.paint_sequence;
        const bool warm_unmodified_composite =
            is_menu_open && !trace.has_painted && paint_sequence >= trace.baseline_paint_sequence &&
            (bucket == 0ULL);
        if (!ready_to_complete && !(warm_unmodified_composite && DurationMs(trace.started_at, now) >= 4.0)) {
            it->second = std::move(trace);
            continue;
        }

        if (!trace.has_dispatched) {
            trace.dispatched_at = now;
        }
        if (!trace.has_painted) {
            trace.painted_at = now;
            trace.paint_sequence = paint_sequence;
        }
        reg.pending_by_key.erase(it);

        FrameTraceRecord rec;
        rec.id = trace.id;
        rec.kind = trace.kind;
        rec.engine = std::move(trace.engine);
        rec.view_id = trace.view_id;
        rec.mod_id = std::move(trace.mod_id);
        rec.visible = trace.visible;
        rec.splash_active = trace.splash_active;
        rec.baseline_paint_sequence = trace.baseline_paint_sequence;
        rec.paint_sequence = trace.paint_sequence;
        rec.composite_frame_sequence = composite_frame_seq;
        rec.queue_wait_ms = DurationMs(trace.started_at, trace.dispatched_at);
        rec.dispatch_to_paint_ms = DurationMs(trace.dispatched_at, trace.painted_at);
        rec.paint_to_composite_ms = DurationMs(trace.painted_at, now);
        rec.total_ms = DurationMs(trace.started_at, now);
        rec.completed_at_ms = now_ms;

        const auto kind_idx = static_cast<std::size_t>(rec.kind);
        if (kind_idx < reg.kind_stats.size()) {
            reg.kind_stats[kind_idx].Add(rec.total_ms);
        }
        reg.completed_traces.push_back(std::move(rec));
        while (reg.completed_traces.size() > kMaxCompletedTraces) {
            reg.completed_traces.pop_front();
        }
    }
}

void RecordCefCopyMetrics(bool dirty_copy, bool fallback, std::uint64_t overlapping_pixels)
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    if (dirty_copy) {
        ++reg.cef_dirty_copies;
    } else {
        ++reg.cef_full_copies;
    }
    if (fallback) {
        ++reg.cef_dirty_fallbacks;
    }
    reg.cef_overlapping_pixels += overlapping_pixels;
}

void RecordCefFrameDisplayAge(double age_ms)
{
    if (!std::isfinite(age_ms) || age_ms < 0.0) {
        return;
    }
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    reg.cef_frame_age.Add(age_ms);
}

void RecordUltralightReplayMetrics(std::size_t command_lists, double packet_age_ms, double replay_ms)
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    ++reg.ul_packets_replayed;
    reg.ul_total_command_lists += command_lists;
    reg.ul_max_command_lists = std::max(reg.ul_max_command_lists, static_cast<std::uint32_t>(command_lists));
    if (std::isfinite(packet_age_ms) && packet_age_ms >= 0.0) {
        reg.ul_oldest_packet_age_ms = packet_age_ms;
        reg.ul_max_packet_age_ms = std::max(reg.ul_max_packet_age_ms, packet_age_ms);
    }
    reg.ul_replay.Add(replay_ms);
}

void RecordUltralightBackpressureWait()
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    ++reg.ul_backpressure_waits;
}

void RecordNativeQueueTask(
    double wait_ms, double exec_ms, std::string_view label, std::string_view mod_id, bool budget_overrun)
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    ++reg.queue_tasks_executed;
    if (budget_overrun) {
        ++reg.queue_budget_overruns;
    }
    reg.queue_wait.Add(wait_ms);
    reg.queue_exec.Add(exec_ms);
    if (std::isfinite(exec_ms) && exec_ms >= reg.queue_slowest_exec_ms) {
        reg.queue_slowest_exec_ms = exec_ms;
        reg.queue_slowest_task.assign(label.empty() ? std::string_view("task") : label);
        reg.queue_slowest_mod.assign(mod_id.empty() ? std::string_view("skyultra") : mod_id);
    }
}

void RecordThumbnailHit(bool memory_hit, bool disk_hit, bool url_hit, bool gpu_capture)
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    if (memory_hit) ++reg.thumb_memory_hits;
    if (disk_hit) ++reg.thumb_disk_hits;
    if (url_hit) ++reg.thumb_url_hits;
    if (gpu_capture) ++reg.thumb_gpu_captures;
}

void RecordThumbnailTiming(
    double disk_read_ms, double encode_ms, double pixel_convert_ms, double full_export_ms, bool avx2_active)
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    if (disk_read_ms >= 0.0) reg.thumb_disk_read.Add(disk_read_ms);
    if (encode_ms >= 0.0) reg.thumb_encode.Add(encode_ms);
    if (pixel_convert_ms >= 0.0) reg.thumb_pixel_convert.Add(pixel_convert_ms);
    if (full_export_ms >= 0.0) reg.thumb_full_export.Add(full_export_ms);
    reg.thumb_avx2_active = avx2_active;
}

FrameTraceSnapshot GetFrameTraceSnapshot()
{
    auto& reg = Registry();
    const std::scoped_lock lock(reg.mutex);
    FrameTraceSnapshot out;
    const std::size_t count = std::min(kReturnedTraces, reg.completed_traces.size());
    out.recent_traces.reserve(count);
    for (auto it = reg.completed_traces.rbegin();
         it != reg.completed_traces.rend() && out.recent_traces.size() < count;
         ++it) {
        out.recent_traces.push_back(*it);
    }
    out.warm_menu_open = reg.kind_stats[static_cast<std::size_t>(TraceKind::WarmMenuOpen)].Summarize();
    out.cold_menu_open = reg.kind_stats[static_cast<std::size_t>(TraceKind::ColdMenuOpen)].Summarize();
    out.input_click = reg.kind_stats[static_cast<std::size_t>(TraceKind::InputClick)].Summarize();
    out.input_wheel = reg.kind_stats[static_cast<std::size_t>(TraceKind::InputWheel)].Summarize();
    out.input_key = reg.kind_stats[static_cast<std::size_t>(TraceKind::InputKey)].Summarize();
    out.input_focus = reg.kind_stats[static_cast<std::size_t>(TraceKind::InputFocus)].Summarize();

    out.cef_copy.full_copies = reg.cef_full_copies;
    out.cef_copy.dirty_copies = reg.cef_dirty_copies;
    out.cef_copy.dirty_fallbacks = reg.cef_dirty_fallbacks;
    out.cef_copy.overlapping_pixels_copied = reg.cef_overlapping_pixels;
    out.cef_copy.frame_age = reg.cef_frame_age.Summarize();

    out.ultralight_replay.packets_replayed = reg.ul_packets_replayed;
    out.ultralight_replay.backpressure_waits = reg.ul_backpressure_waits;
    out.ultralight_replay.max_command_lists = reg.ul_max_command_lists;
    out.ultralight_replay.avg_command_lists = reg.ul_packets_replayed > 0
        ? static_cast<double>(reg.ul_total_command_lists) / static_cast<double>(reg.ul_packets_replayed)
        : 0.0;
    out.ultralight_replay.oldest_packet_age_ms = reg.ul_oldest_packet_age_ms;
    out.ultralight_replay.max_packet_age_ms = reg.ul_max_packet_age_ms;
    out.ultralight_replay.replay = reg.ul_replay.Summarize();

    out.native_queue.tasks_executed = reg.queue_tasks_executed;
    out.native_queue.budget_overruns = reg.queue_budget_overruns;
    out.native_queue.wait = reg.queue_wait.Summarize();
    out.native_queue.exec = reg.queue_exec.Summarize();
    out.native_queue.slowest_task = reg.queue_slowest_task;
    out.native_queue.slowest_mod = reg.queue_slowest_mod;
    out.native_queue.slowest_exec_ms = reg.queue_slowest_exec_ms;

    out.thumbnails.memory_hits = reg.thumb_memory_hits;
    out.thumbnails.disk_hits = reg.thumb_disk_hits;
    out.thumbnails.url_hits = reg.thumb_url_hits;
    out.thumbnails.gpu_captures = reg.thumb_gpu_captures;
    out.thumbnails.disk_read = reg.thumb_disk_read.Summarize();
    out.thumbnails.encode = reg.thumb_encode.Summarize();
    out.thumbnails.pixel_convert = reg.thumb_pixel_convert.Summarize();
    out.thumbnails.full_export = reg.thumb_full_export.Summarize();
    out.thumbnails.avx2_active = reg.thumb_avx2_active;
    return out;
}

}  // namespace skyultra
