#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace skyultra::detail {

// A private vertex stream whose indices explicitly address NiSkinInstance::bones.
// It does not depend on how a runtime draw buffer packs partition palettes.
struct ActorSkinVertex {
    std::array<float,4> weights{};
    std::array<std::uint8_t,4> indices{};
    bool operator==(const ActorSkinVertex&) const = default;
};
static_assert(sizeof(ActorSkinVertex)==20);

constexpr void AddActorSkinWeight(ActorSkinVertex& vertex,std::uint32_t bone,float weight)
{
    if(bone>=256 || !(weight>0 && weight<=1))return;
    std::size_t smallest{};
    for(std::size_t i=0;i<4;++i){
        if(vertex.weights[i]>0 && vertex.indices[i]==bone){vertex.weights[i]+=weight;return;}
        if(vertex.weights[i]<vertex.weights[smallest])smallest=i;
    }
    if(weight>vertex.weights[smallest]){
        vertex.weights[smallest]=weight;
        vertex.indices[smallest]=static_cast<std::uint8_t>(bone);
    }
}
namespace actor_skin_checks {
constexpr auto explicit_palette=[] {
    ActorSkinVertex vertex;
    // Partition-local slots 0/1 need not be global skeleton bones 0/1.
    constexpr std::array<std::uint16_t,2> palette{63,17};
    AddActorSkinWeight(vertex,palette[0],.75F);
    AddActorSkinWeight(vertex,palette[1],.25F);
    return vertex;
}();
static_assert(explicit_palette.indices[0]==63 && explicit_palette.indices[1]==17);
static_assert(explicit_palette.weights[0]+explicit_palette.weights[1]==1);
constexpr auto invalid=[] {ActorSkinVertex v;AddActorSkinWeight(v,256,1);AddActorSkinWeight(v,2,-1);return v;}();
static_assert(invalid.weights[0]==0);
} // namespace actor_skin_checks
} // namespace skyultra::detail
