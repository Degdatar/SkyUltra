#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace skyultra::detail {

inline constexpr std::size_t kMaxAnimationRigBones=512;
struct AnimationTrackMap {
    std::array<std::int16_t,kMaxAnimationRigBones> bones{};
    const char* error{};
};

// Havok permits an empty binding map: track i then targets bone i. Explicit
// maps may contain -1 for tracks that do not target a skeleton bone.
constexpr AnimationTrackMap ResolveAnimationTracks(std::span<const std::int16_t> indices,
    std::int32_t tracks,std::int32_t bone_count)
{
    AnimationTrackMap result;
    if(tracks<=0||tracks>static_cast<std::int32_t>(kMaxAnimationRigBones)||
        bone_count<=0||bone_count>static_cast<std::int32_t>(kMaxAnimationRigBones)){
        result.error="Animation track or skeleton size is unsupported";return result;
    }
    if(!indices.empty()&&indices.size()!=static_cast<std::size_t>(tracks)){
        result.error="Animation bone map length does not match its tracks";return result;
    }
    std::array<bool,kMaxAnimationRigBones> assigned{};
    bool any{};
    for(std::int32_t track=0;track<tracks;++track){
        const auto bone=indices.empty()?static_cast<std::int16_t>(track):indices[track];
        if(bone < -1 || bone>=bone_count){result.error="Animation bone map targets a missing skeleton bone";return result;}
        result.bones[track]=bone;
        if(bone<0)continue;
        if(assigned[bone]){result.error="Animation bone map targets a bone more than once";return result;}
        assigned[bone]=true;any=true;
    }
    if(!any)result.error="Animation has no tracks mapped to skeleton bones";
    return result;
}

// Regression fixtures are evaluated by the user's next native build. In
// particular, an empty map must not discard a normal dense idle animation.
namespace animation_track_checks {
constexpr std::array<std::int16_t,3> reordered{2,0,1},sparse{2,-1,0},
    duplicate{0,0,1},outside{0,1,3},negative{0,-2,1},unmapped{-1,-1,-1};
constexpr std::array<std::int16_t,2> short_map{0,1};
static_assert(ResolveAnimationTracks({},3,3).error==nullptr);
static_assert(ResolveAnimationTracks({},3,3).bones[2]==2);
static_assert(ResolveAnimationTracks({},2,3).error==nullptr);
static_assert(ResolveAnimationTracks({},97,99).error==nullptr);
static_assert(ResolveAnimationTracks({},97,99).bones[96]==96);
static_assert(ResolveAnimationTracks({},4,3).error!=nullptr);
static_assert(ResolveAnimationTracks(reordered,3,3).error==nullptr);
static_assert(ResolveAnimationTracks(reordered,3,3).bones[0]==2);
static_assert(ResolveAnimationTracks(sparse,3,3).error==nullptr);
static_assert(ResolveAnimationTracks(sparse,3,3).bones[1]==-1);
static_assert(ResolveAnimationTracks(duplicate,3,3).error!=nullptr);
static_assert(ResolveAnimationTracks(outside,3,3).error!=nullptr);
static_assert(ResolveAnimationTracks(negative,3,3).error!=nullptr);
static_assert(ResolveAnimationTracks(unmapped,3,3).error!=nullptr);
static_assert(ResolveAnimationTracks(short_map,3,3).error!=nullptr);
static_assert(ResolveAnimationTracks({},0,3).error!=nullptr);
static_assert(ResolveAnimationTracks({},513,513).error!=nullptr);
} // namespace animation_track_checks
} // namespace skyultra::detail
