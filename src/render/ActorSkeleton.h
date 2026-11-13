#pragma once

#include <RE/B/BSGeometry.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiSkinData.h>
#include <RE/N/NiSkinInstance.h>
#include <cmath>
#include <unordered_map>

namespace skyultra::detail {

inline bool HasHumanoidPreviewArms(RE::NiAVObject* root)
{
    return root && root->GetObjectByName(RE::BSFixedString("NPC L UpperArm [LUar]")) &&
        root->GetObjectByName(RE::BSFixedString("NPC R UpperArm [RUar]"));
}

inline bool FiniteTransform(const RE::NiTransform& t)
{
    if (!std::isfinite(t.scale) || !std::isfinite(t.translate.x) || !std::isfinite(t.translate.y) || !std::isfinite(t.translate.z)) return false;
    for (const auto& row : t.rotate.entry) for (float v : row) if (!std::isfinite(v)) return false;
    return true;
}

// Distant creatures can retain stale world matrices after animation LOD skips
// their update. Local transforms still describe the skeleton hierarchy.
inline bool RootLocalFromLocals(RE::NiAVObject* object, RE::NiAVObject* root, RE::NiTransform& out)
{
    if (!object || !root) return false;
    if (object == root) { out = RE::NiTransform{}; return true; }
    auto accum = object->local;
    unsigned depth{};
    for (auto* parent = object->parent; parent && depth < 256; parent = parent->parent, ++depth) {
        if (parent == root) { out = accum; return FiniteTransform(out); }
        accum = parent->local * accum;
    }
    return false;
}

// Keep the renderer and animation sampler in the same mesh bind space. Prefer
// the most complete skin when FaceGen or attachments share skeleton bones.
using SkinBindPose = std::unordered_map<RE::NiAVObject*, RE::NiTransform>;
inline SkinBindPose CaptureSkinBindPose(RE::NiAVObject* root)
{
    SkinBindPose result;
    std::unordered_map<RE::NiAVObject*, std::uint32_t> counts;
    const auto visit = [&](auto&& self, RE::NiAVObject* object, unsigned depth) -> void {
        if (!object || depth > 128 || (depth > 0 && object->GetFlags().all(RE::NiAVObject::Flag::kHidden))) return;
        if (auto* geometry = object->AsGeometry()) {
            auto* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
            if (skin && skin->skinData && skin->bones) {
                const auto count = skin->skinData->GetBoneCount();
                if (count > 0 && count <= 256 && skin->skinData->GetBoneDataAddress(0)) {
                    const auto& raw = skin->skinData->rootParentToSkin;
                    const auto root_to_skin = FiniteTransform(raw) && std::abs(raw.scale) > .0001F ? raw : RE::NiTransform{};
                    for (std::uint32_t i = 0; i < count; ++i) {
                        auto* bone = skin->bones[i];
                        if (!bone) continue;
                        if (const auto prev = counts.find(bone); prev != counts.end() && prev->second >= count) continue;
                        const auto skin_to_bone = skin->skinData->GetBoneDataSkinToBone(i);
                        if (!FiniteTransform(skin_to_bone) || std::abs(skin_to_bone.scale) < .0001F) continue;
                        const auto bind = (skin_to_bone * root_to_skin).Invert();
                        if (!FiniteTransform(bind)) continue;
                        result[bone] = bind;
                        counts[bone] = count;
                    }
                }
            }
        }
        if (auto* node = object->AsNode()) for (const auto& child : node->GetChildren()) self(self, child.get(), depth + 1);
    };
    visit(visit, root, 0);
    return result;
}

} // namespace skyultra::detail
