#pragma once

#include "SkyUltra/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace skyultra {

enum class TraceKind : std::uint8_t {
    WarmMenuOpen = 0,
    ColdMenuOpen = 1,
    InputClick = 2,
    InputWheel = 3,
    InputKey = 4,
    InputFocus = 5,
};

[[nodiscard]] const char* TraceKindName(TraceKind kind) noexcept;

struct PercentileSummary {
    std::uint64_t samples{};
    double mean_ms{};
    double p50_ms{};
    double p95_ms{};
    double p99_ms{};
    double max_ms{};
};

struct FrameTraceRecord {
    std::uint64_t id{};
    TraceKind kind{ TraceKind::InputClick };
    std::string engine;
    ViewId view_id{};
    std::string mod_id;
    bool visible{};
    bool splash_active{};
    std::uint64_t baseline_paint_sequence{};
    std::uint64_t paint_sequence{};
    std::uint64_t composite_frame_sequence{};
    double queue_wait_ms{};
    double dispatch_to_paint_ms{};
    double paint_to_composite_ms{};
    double total_ms{};
    double completed_at_ms{};
};

struct CefCopyTelemetry {
    std::uint64_t full_copies{};
    std::uint64_t dirty_copies{};
    std::uint64_t dirty_fallbacks{};
    std::uint64_t overlapping_pixels_copied{};
    PercentileSummary frame_age;
};

struct UltralightReplayTelemetry {
    std::uint64_t packets_replayed{};
    std::uint64_t backpressure_waits{};
    std::uint32_t max_command_lists{};
    double avg_command_lists{};
    double oldest_packet_age_ms{};
    double max_packet_age_ms{};
    PercentileSummary replay;
};

struct NativeQueueTelemetry {
    std::uint64_t tasks_executed{};
    std::uint64_t budget_overruns{};
    PercentileSummary wait;
    PercentileSummary exec;
    std::string slowest_task;
    std::string slowest_mod;
    double slowest_exec_ms{};
};

struct ThumbnailTelemetry {
    std::uint64_t memory_hits{};
    std::uint64_t disk_hits{};
    std::uint64_t url_hits{};
    std::uint64_t gpu_captures{};
    PercentileSummary disk_read;
    PercentileSummary encode;
    PercentileSummary pixel_convert;
    PercentileSummary full_export;
    bool avx2_active{};
};

struct FrameTraceSnapshot {
    std::vector<FrameTraceRecord> recent_traces;
    PercentileSummary warm_menu_open;
    PercentileSummary cold_menu_open;
    PercentileSummary input_click;
    PercentileSummary input_wheel;
    PercentileSummary input_key;
    PercentileSummary input_focus;
    CefCopyTelemetry cef_copy;
    UltralightReplayTelemetry ultralight_replay;
    NativeQueueTelemetry native_queue;
    ThumbnailTelemetry thumbnails;
};

void RegisterTraceViewMetadata(ViewId view_id, std::string_view engine, std::string_view mod_id);
void UnregisterTraceViewMetadata(ViewId view_id);

void TraceBeginMenuOpen(ViewId view_id, bool warm, bool splash_active, std::uint64_t baseline_paint_seq);
void TraceBeginInput(ViewId view_id, TraceKind kind, bool visible, std::uint64_t baseline_paint_seq);
void TraceMarkDispatched(ViewId view_id, TraceKind kind);
void TraceMarkPainted(ViewId view_id, std::uint64_t paint_sequence);
void TraceMarkComposited(ViewId view_id, std::uint64_t paint_sequence, std::uint64_t composite_frame_seq);

void RecordCefCopyMetrics(bool dirty_copy, bool fallback, std::uint64_t overlapping_pixels);
void RecordCefFrameDisplayAge(double age_ms);

void RecordUltralightReplayMetrics(std::size_t command_lists, double packet_age_ms, double replay_ms);
void RecordUltralightBackpressureWait();

void RecordNativeQueueTask(
    double wait_ms, double exec_ms, std::string_view label, std::string_view mod_id, bool budget_overrun);

void RecordThumbnailHit(bool memory_hit, bool disk_hit, bool url_hit, bool gpu_capture);
void RecordThumbnailTiming(
    double disk_read_ms, double encode_ms, double pixel_convert_ms, double full_export_ms, bool avx2_active);

[[nodiscard]] FrameTraceSnapshot GetFrameTraceSnapshot();

}  // namespace skyultra
