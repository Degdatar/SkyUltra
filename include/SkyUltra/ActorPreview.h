#pragma once

#include "SkyUltra/Types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <functional>
#include <vector>

namespace skyultra {

enum class ActorPreviewPose { Current, Idle, Fighting, Guard, Spellcasting };
enum class ActorPreviewLighting { Studio, Soft, Dramatic, Warm, Cool };

// Bounds are normalized to the owning browser's content viewport, including
// negative/offscreen positions while a page scrolls. No browser-specific API.
struct ActorPreviewRequest {
    std::uint32_t reference_id{};
    std::uint32_t base_form_id{};
    RectF normalized_bounds{};
    SizeU size{512, 640};
    bool visible{true};
    bool live{true};
    bool portrait{};
    bool refresh{}; // recapture a frozen pose; ordinary bounds/camera updates reuse it
    float yaw{};  // radians around the actor's up axis
    float pitch{};
    float zoom{1.0F};
    float pan_x{}; // fractions of the image; positive moves the actor right/down
    float pan_y{};
    ActorPreviewPose pose{ActorPreviewPose::Current};
    // Private preview lights, shared by the stage and PNG export.
    ActorPreviewLighting lighting{ActorPreviewLighting::Studio};
    // Opaque, owner-scoped ID returned by CaptureActorPreviewAnimations.
    std::string animation_id;
    std::uint32_t animation_frame{};
    bool item_preview{}; // actor/item stages share one surface per owning view
    std::uint32_t item_form_id{};
    bool ignore_world_fade{}; // isolate actor scalar opacity from camera/effect fades
};

struct ActorPreviewAnimationClip {
    std::string id;
    std::string name;
    float duration{};
    std::uint32_t sample_count{};
    std::string preset; // empty for an animation captured from the live graph
};
struct ActorPreviewAnimations {
    bool available{};
    std::vector<ActorPreviewAnimationClip> clips;
    std::string message;
};

struct ActorPreviewStatus {
    bool available{};
    bool ready{};
    std::uint32_t reference_id{};
    std::size_t geometry_count{};
    std::size_t skipped_geometry_count{};
    std::string message;
    bool poses_available{};
    bool item_preview{};
    std::uint32_t item_form_id{};
};

struct ActorPreviewFrame {
    RectF normalized_bounds{};
    SizeU size{};
    void* shader_resource_view{};  // ID3D11ShaderResourceView, retained by lifetime
    std::shared_ptr<const void> lifetime;
};

// Set/Tick read Skyrim scene data and must be invoked on a game task.
[[nodiscard]] ActorPreviewStatus SetActorPreview(ViewId owner, const ActorPreviewRequest& request);
// Explicit game-task operation. Samples compatible active clips into owned
// numeric poses; no Havok/actor pointers survive the capture or reach drawing.
[[nodiscard]] ActorPreviewAnimations CaptureActorPreviewAnimations(ViewId owner,bool pose_library=false);
void TickActorPreviews();
[[nodiscard]] ActorPreviewStatus GetActorPreviewStatus(ViewId owner);
void RemoveActorPreview(ViewId owner) noexcept;
void ClearActorPreviews() noexcept;
// Captures the requested camera/pose on the render thread, then writes a
// transparent PNG asynchronously. The host chooses a unique filename.
using ActorPreviewSaveCallback = std::function<void(bool, std::string)>;
// Game-task capture, rendered offscreen without replacing the owner's stage.
// Returns an in-memory PNG data URL; does not write a file.
void CaptureItemThumbnail(ViewId owner, std::uint32_t form_id, ActorPreviewSaveCallback callback, bool use_preview = false);
void CaptureActorThumbnail(ViewId owner, std::uint32_t reference_id, std::uint32_t base_form_id, ActorPreviewSaveCallback callback, bool use_preview = false);
void ConfigureActorPreviewExport(std::uint32_t full_view, std::uint32_t portrait);
// A zero long_edge selects the saved resolution for this preview's mode.
void SaveActorPreviewPng(ViewId owner, ActorPreviewSaveCallback callback, std::uint32_t long_edge = 0);

// Runs inside SkyUltra's isolated D3D11 context state. Uses private render/depth
// targets and retained GPU mesh resources; never reparents or changes an actor.
void WarmupActorPreviewRenderer(void* device);
void RenderActorPreviews(void* device, void* context, std::span<const ViewId> visible_owners);
[[nodiscard]] std::optional<ActorPreviewFrame> GetActorPreviewFrame(ViewId owner);

}  // namespace skyultra
