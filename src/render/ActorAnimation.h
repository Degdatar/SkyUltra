#pragma once

#include "SkyUltra/ActorPreview.h"
#include <DirectXMath.h>
#include <atomic>
#include <memory>

namespace RE { class Actor; class NiAVObject; class NiTransform; }

namespace skyultra::detail {

struct CapturedAnimation {
    ActorPreviewAnimationClip info;
    bool mesh_bind_basis{};
    std::vector<std::string> bone_names;
    // The same captured basis as frames, used to apply model-space motion to
    // each mesh's own bind space without mixing inverse binds across outfits.
    std::vector<DirectX::XMFLOAT4X4> bind_pose;
    // Model-space matrices, one complete private pose per sampled frame.
    std::vector<std::vector<DirectX::XMFLOAT4X4>> frames;
    // Each clip reports its first rendered skin palette once, even when frame
    // zero is revisited through resizing, camera changes or the scrub cache.
    std::shared_ptr<std::atomic_bool> skin_diagnostic_written=std::make_shared<std::atomic_bool>(false);
};
struct CapturedActorAnimations {
    std::uint32_t reference_id{};
    std::vector<CapturedAnimation> clips;
    std::string message;
};

std::shared_ptr<const CapturedActorAnimations> CaptureActorAnimations(
    RE::Actor* actor, RE::NiAVObject* root, const RE::NiTransform& inverse, std::uint64_t generation);
std::shared_ptr<const CapturedActorAnimations> LoadActorPoseLibrary(
    RE::Actor* actor, RE::NiAVObject* root, const RE::NiTransform& inverse, std::uint64_t generation);

} // namespace skyultra::detail
