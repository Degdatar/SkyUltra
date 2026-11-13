#include "ActorAnimation.h"
#include "ActorAnimationTracks.h"
#include "ActorPoseFile.h"
#include "ActorSkeleton.h"

#include <SKSE/SKSE.h>

#include <RE/A/Actor.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BSAtomic.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkaAnimationBinding.h>
#include <RE/H/hkaSkeleton.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbCharacterSetup.h>
#include <RE/H/hkbClipGenerator.h>
#include <RE/N/NiAVObject.h>
#include <RE/T/TESNPC.h>
#include <RE/RTTI.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <string_view>
#include <sstream>
#include <iomanip>

namespace skyultra::detail {
namespace {
using namespace DirectX;
constexpr std::size_t kMaxRigBones=kMaxAnimationRigBones, kMaxClips=4, kMaxActiveNodes=2048;
constexpr std::uint32_t kSamples=33;

XMFLOAT4X4 Matrix(const RE::NiTransform& t)
{
    const auto& r=t.rotate.entry; const auto s=t.scale;
    return {r[0][0]*s,r[1][0]*s,r[2][0]*s,0,
        r[0][1]*s,r[1][1]*s,r[2][1]*s,0,r[0][2]*s,r[1][2]*s,r[2][2]*s,0,
        t.translate.x,t.translate.y,t.translate.z,1};
}
bool Finite(const XMFLOAT4X4& matrix)
{
    for(const auto& row:matrix.m)for(const auto value:row)if(!std::isfinite(value))return false;
    return true;
}
bool LocalMatrix(const RE::hkQsTransform& transform, float units, XMFLOAT4X4& matrix)
{
    const auto rotation=transform.rotation.vec.quad;
    const auto length=XMVectorGetX(XMVector4LengthSq(rotation));
    if(!std::isfinite(length)||length<.00001F)return false;
    const auto scale=transform.scale.quad;
    if(XMVectorGetX(scale)<=.00001F||XMVectorGetY(scale)<=.00001F||XMVectorGetZ(scale)<=.00001F)return false;
    XMStoreFloat4x4(&matrix,XMMatrixScalingFromVector(scale)*
        XMMatrixRotationQuaternion(XMQuaternionNormalize(rotation))*
        XMMatrixTranslationFromVector(XMVectorScale(transform.translation.quad,units)));
    return Finite(matrix);
}
bool ModelPose(const std::vector<RE::hkQsTransform>& local, std::span<const std::int16_t> parents,
    float units, std::vector<XMFLOAT4X4>& world)
{
    world.resize(local.size());
    for(std::size_t i=0;i<local.size();++i){
        const auto parent=parents[i];
        if(parent < -1 || parent>=static_cast<std::int32_t>(i) || !LocalMatrix(local[i],units,world[i]))return false;
        if(parent>=0)XMStoreFloat4x4(&world[i],XMLoadFloat4x4(&world[i])*XMLoadFloat4x4(&world[parent]));
        if(!Finite(world[i]))return false;
    }
    return true;
}

struct RigCapture {
    std::vector<RE::hkQsTransform> baseline;
    std::vector<XMFLOAT4X4> world, captured;
    std::vector<std::string> names;
    std::vector<bool> mapped;
    std::vector<std::int16_t> parents;
    std::vector<bool> locked;
    std::string skeleton;
    float units{1};
    XMFLOAT4X4 alignment{};
    bool mesh_bind_basis{};
};

// Keep one capture's numeric inputs for the log. Writing/formatting happens
// after the graph lock is released, never during playback or frame scrubbing.
struct CreatureCaptureDiagnostic {
    std::string clip;
    RigCapture rig;
    std::vector<RE::hkQsTransform> sampled;
    std::vector<std::int16_t> tracks;
    std::vector<XMFLOAT4X4> frame;
};
std::string TransformText(const RE::hkQsTransform& transform)
{
    XMFLOAT4 translation,rotation,scale;
    XMStoreFloat4(&translation,transform.translation.quad);
    XMStoreFloat4(&rotation,transform.rotation.vec.quad);
    XMStoreFloat4(&scale,transform.scale.quad);
    std::ostringstream text;text<<std::setprecision(8);
    text<<translation.x<<','<<translation.y<<','<<translation.z<<';'
        <<rotation.x<<','<<rotation.y<<','<<rotation.z<<','<<rotation.w<<';'
        <<scale.x<<','<<scale.y<<','<<scale.z;
    return text.str();
}
std::string MatrixText(const XMFLOAT4X4& matrix)
{
    std::ostringstream text;text<<std::setprecision(8);
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
        if(row||column)text<<',';
        text<<matrix.m[row][column];
    }
    return text.str();
}
bool CaptureRig(const RE::hkbCharacter& character, RE::NiAVObject* root,
    const RE::hkaSkeleton& rig, RigCapture& out, const SkinBindPose* bind_pose=nullptr)
{
    const auto count=rig.bones.size();
    out.mesh_bind_basis=bind_pose!=nullptr;
    if(count<=0||count>static_cast<std::int32_t>(kMaxRigBones)||rig.parentIndices.size()!=count)return false;
    if(bind_pose){
        if(bind_pose->empty()||rig.referencePose.size()!=count)return false;
        out.baseline.assign(rig.referencePose.begin(),rig.referencePose.end());
    }else{
        if(!character.poseLocal||character.numPoseLocal!=count)return false;
        out.baseline.assign(character.poseLocal,character.poseLocal+count);
    }
    out.parents.assign(rig.parentIndices.begin(),rig.parentIndices.end());
    out.locked.resize(count);
    const auto* rig_name=rig.name.c_str();out.skeleton=rig_name?rig_name:"";
    if(!ModelPose(out.baseline,out.parents,1,out.world))return false;
    out.captured.resize(count);out.names.resize(count);out.mapped.resize(count);
    std::vector<bool> bound(count);
    std::unordered_set<std::string> names;
    for(std::int32_t i=0;i<count;++i){
        const auto* name=rig.bones[i].name.c_str();
        if(!name||!name[0])return false;
        out.names[i]=name;
        out.locked[i]=rig.bones[i].lockTranslation;
        if(out.names[i].size()>256||!names.emplace(out.names[i]).second)return false;
        if(auto* node=root->GetObjectByName(RE::BSFixedString(name))){
            if(bind_pose){
                if(const auto found=bind_pose->find(node);found!=bind_pose->end()){
                    out.captured[i]=Matrix(found->second);
                    bound[i]=out.mapped[i]=Finite(out.captured[i]);
                }
            }else{
                RE::NiTransform local;
                if(RootLocalFromLocals(node,root,local)){
                    out.captured[i]=Matrix(local);
                    out.mapped[i]=Finite(out.captured[i]);
                }
            }
        }
    }
    // Infer the animation-to-NIF unit conversion from matching bone segments,
    // rather than assuming a 1:1 or 70:1 unit convention for every rig.
    std::vector<float> ratios;
    std::int32_t anchor=-1;
    for(std::int32_t i=0;i<count;++i){
        if(!out.mapped[i])continue;
        if(bind_pose && anchor<0)anchor=i;
        const auto parent=out.parents[i];
        if(parent<0){if(anchor<0)anchor=i;continue;}
        if(!out.mapped[parent])continue;
        const auto a=XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat4x4(&out.world[i]).r[3],XMLoadFloat4x4(&out.world[parent]).r[3])));
        const auto b=XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat4x4(&out.captured[i]).r[3],XMLoadFloat4x4(&out.captured[parent]).r[3])));
        if(a>.0001F&&b>.0001F&&std::isfinite(a)&&std::isfinite(b))ratios.push_back(b/a);
    }
    if(anchor<0||ratios.size()<3)return false;
    std::sort(ratios.begin(),ratios.end());out.units=ratios[ratios.size()/2];
    if(!std::isfinite(out.units)||out.units<.001F||out.units>10000)return false;
    if(!ModelPose(out.baseline,out.parents,out.units,out.world))return false;
    XMVECTOR determinant;
    const auto inverse_anchor=XMMatrixInverse(&determinant,XMLoadFloat4x4(&out.world[anchor]));
    if(std::abs(XMVectorGetX(determinant))<.000001F)return false;
    XMStoreFloat4x4(&out.alignment,inverse_anchor*XMLoadFloat4x4(&out.captured[anchor]));
    // Roots and attachment-only rig bones may not occur in a skin palette.
    // Derive them from the aligned reference rig, never a stale world pose.
    if(bind_pose)for(std::int32_t i=0;i<count;++i)if(!bound[i]){
        XMStoreFloat4x4(&out.captured[i],XMLoadFloat4x4(&out.world[i])*XMLoadFloat4x4(&out.alignment));
        out.mapped[i]=Finite(out.captured[i]);
    }
    return Finite(out.alignment);
}

struct ClipSource {
    const RE::hkaAnimationBinding* binding{};
    bool mapped{};
    const ActorPoseFile* file{};
};
ClipSource PlaybackSource(const RE::hkbClipGenerator& clip)
{
    // The playback control may own the effective binding, including a runtime
    // replacement. Generator mapper metadata alone is not an active retarget.
    if(clip.animationControl && clip.animationControl->binding && clip.animationControl->binding->animation)
        return {clip.animationControl->binding,clip.animationControl->mapper!=nullptr};
    return {clip.binding,clip.mapperData!=nullptr};
}

bool SampleClip(std::uint8_t flags, const ClipSource& source,
    const RigCapture& capture, CapturedAnimation& out, std::string_view& failure,
    CreatureCaptureDiagnostic* diagnostic=nullptr)
{
    const auto reject=[&](const char* reason){failure=reason;return false;};
    const auto* binding=source.binding;
    if(!source.file && (!binding||!binding->animation))return reject("Active clip has no loaded animation binding");
    if(source.mapped)return reject("Active clip needs retargeting, which is not supported yet");
    if(!source.file && binding->blendHint!=RE::hkaAnimationBinding::BlendHint::kNormal)
        return reject("Active clip is additive, which is not supported yet");
    const auto* animation=source.file?source.file->Animation():binding->animation.get();
    const auto tracks=animation->numberOfTransformTracks, floats=animation->numberOfFloatTracks;
    if(tracks<=0||tracks>static_cast<std::int32_t>(kMaxRigBones)||floats<0||floats>512||
        !std::isfinite(animation->duration)||animation->duration<=0||animation->duration>120)
        return reject("Active clip has unsupported track counts or duration");
    if(animation->type==RE::hkaAnimation::AnimationType::kMirroredAnimation || (flags&4U)!=0)
        return reject("Active clip is mirrored, which is not supported yet");
    // No retargeting or additive reference frame is guessed in this first slice.
    const auto* source_rig=source.file?source.file->Skeleton().c_str():binding->originalSkeletonName.c_str();
    const auto* target_rig=capture.skeleton.c_str();
    if(source_rig && source_rig[0] && target_rig && target_rig[0] && std::string_view(source_rig)!=target_rig)
        return reject("Active clip targets a different animation skeleton");
    std::span<const std::int16_t> indices;
    if(source.file)indices=source.file->Tracks();
    else {
        const auto& map=binding->transformTrackToBoneIndices;
        if(map.size()<0)return reject("Active clip has an invalid bone map");
        indices={map.data(),static_cast<std::size_t>(map.size())};
    }
    const auto mapping=ResolveAnimationTracks(indices,tracks,static_cast<std::int32_t>(capture.names.size()));
    if(mapping.error)return reject(mapping.error);
    out.mesh_bind_basis=capture.mesh_bind_basis;

    std::vector<RE::hkQsTransform> sampled(tracks),local=capture.baseline;
    std::vector<float> float_tracks(static_cast<std::size_t>(floats));
    std::vector<XMFLOAT4X4> world;
    std::vector<XMFLOAT4X4> correction(capture.names.size());
    const auto alignment=XMLoadFloat4x4(&capture.alignment);
    for(std::size_t i=0;i<capture.names.size();++i)if(capture.mapped[i]){
        XMVECTOR determinant;
        const auto baseline_inverse=XMMatrixInverse(&determinant,XMLoadFloat4x4(&capture.world[i])*alignment);
        if(std::abs(XMVectorGetX(determinant))<.000001F)return reject("Captured skeleton has a singular bone transform");
        XMStoreFloat4x4(&correction[i],XMLoadFloat4x4(&capture.captured[i])*baseline_inverse);
        out.bone_names.push_back(capture.names[i]);
        out.bind_pose.push_back(capture.captured[i]);
    }
    out.frames.reserve(kSamples);
    for(std::uint32_t frame=0;frame<kSamples;++frame){
        const auto time=std::min(animation->duration*static_cast<float>(frame)/(kSamples-1),
            std::nextafter(animation->duration,0.0F));
        // SampleTracks is read-only; it writes only our aligned output buffers.
        // No generator update/activation, annotations, events or root motion run.
        if(source.file){
            if(!source.file->Sample(time,sampled.data(),float_tracks.empty()?nullptr:float_tracks.data()))
                return reject("Pose asset could not be sampled by the animation codec");
        }else animation->SampleTracks(time,sampled.data(),float_tracks.empty()?nullptr:float_tracks.data(),nullptr);
        if(diagnostic && frame==0){
            diagnostic->rig=capture;
            diagnostic->sampled=sampled;
            diagnostic->tracks.assign(mapping.bones.begin(),mapping.bones.begin()+tracks);
        }
        for(std::int32_t track=0;track<tracks;++track){
            const auto bone=mapping.bones[track];
            if(bone<0)continue;
            // Anchor roots to the capture; a timeline cannot move the real actor
            // or slide the preview out of its framing through extracted motion.
            if(capture.parents[bone]>=0){
                local[bone]=sampled[track];
                if(capture.locked[bone])local[bone].translation=capture.baseline[bone].translation;
            }
        }
        if(!ModelPose(local,capture.parents,capture.units,world))return reject("Sampled animation contains invalid bone transforms");
        std::vector<XMFLOAT4X4> pose;pose.reserve(out.bone_names.size());
        for(std::size_t i=0;i<capture.names.size();++i)if(capture.mapped[i]){
            XMFLOAT4X4 matrix;
            XMStoreFloat4x4(&matrix,XMLoadFloat4x4(&correction[i])*XMLoadFloat4x4(&world[i])*alignment);
            if(!Finite(matrix))return reject("Sampled animation contains an invalid model transform");
            pose.push_back(matrix);
        }
        if(diagnostic && frame==0)diagnostic->frame=pose;
        out.frames.push_back(std::move(pose));
    }
    out.info.duration=animation->duration;out.info.sample_count=kSamples;
    return true;
}
} // namespace

std::shared_ptr<const CapturedActorAnimations> CaptureActorAnimations(
    RE::Actor* actor, RE::NiAVObject* root, const RE::NiTransform&, std::uint64_t generation)
{
    auto result=std::make_shared<CapturedActorAnimations>();result->reference_id=actor->GetFormID();
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if(!actor->GetAnimationGraphManager(manager)||!manager){result->message="This actor has no animation graph.";return result;}
    struct Rejection {
        std::string clip,reason,source_rig,target_rig;
        std::int32_t tracks{},map_size{};
        bool mapped{};
    };
    std::vector<Rejection> rejections;
    CreatureCaptureDiagnostic diagnostic;
    std::string first_failure;
    std::unordered_set<const RE::hkaAnimationBinding*> seen;
    std::size_t valid_rigs{},graphs_seen{},nodes_seen{},clips_seen{},bindings_seen{};
    // Use the renderer's rig classification too: creature rigs can carry the
    // NPC keyword without having the standard humanoid arm hierarchy.
    const bool creature=!(actor->IsPlayerRef()||actor->HasKeywordString("ActorTypeNPC")) || !HasHumanoidPreviewArms(root);
    const auto bind_pose=creature?CaptureSkinBindPose(root):SkinBindPose{};
    {
    const RE::BSSpinLockGuard lock(manager->GetRuntimeData().updateLock);
    for(const auto& graph:manager->graphs){
        if(!graph||result->clips.size()>=kMaxClips)continue;
        ++graphs_seen;
        const auto& character=graph->characterInstance;
        if(!character.setup||!character.setup->animationSkeleton)continue;
        const auto& rig=*character.setup->animationSkeleton;
        RigCapture capture;if(!CaptureRig(character,root,rig,capture,creature?&bind_pose:nullptr))continue;
        ++valid_rigs;
        std::vector<RE::hkbBehaviorGraph*> pending{character.behaviorGraph.get()};
        if(graph->behaviorGraph && graph->behaviorGraph!=character.behaviorGraph.get())pending.push_back(graph->behaviorGraph);
        std::unordered_set<RE::hkbBehaviorGraph*> visited;
        std::size_t nodes{};
        while(!pending.empty()&&visited.size()<32&&result->clips.size()<kMaxClips){
            auto* behavior=pending.back();pending.pop_back();
            if(!behavior||!visited.emplace(behavior).second||!behavior->activeNodes)continue;
            if(behavior->activeNodes->size()<0||behavior->activeNodes->size()>static_cast<std::int32_t>(kMaxActiveNodes))continue;
            for(const auto& node:*behavior->activeNodes){
                if(++nodes>kMaxActiveNodes||result->clips.size()>=kMaxClips)break;
                ++nodes_seen;
                auto* active=node.nodeClone?node.nodeClone:node.nodeTemplate;
                if(!active)continue;
                if(node.behavior && node.behavior!=behavior)pending.push_back(node.behavior);
                if(auto* nested=skyrim_cast<RE::hkbBehaviorGraph*>(active))pending.push_back(nested);
                auto* clip=skyrim_cast<RE::hkbClipGenerator*>(active);
                if(!clip)continue;
                ++clips_seen;
                const auto source=PlaybackSource(*clip);
                if(source.binding && !seen.emplace(source.binding).second)continue;
                if(source.binding && source.binding->animation)++bindings_seen;
                CapturedAnimation sampled;
                const auto* text=clip->animationName.c_str();
                std::string name=text?text:"Animation";
                const auto separator=name.find_last_of("/\\");if(separator!=std::string::npos)name.erase(0,separator+1);
                if(name.size()>4&&name.substr(name.size()-4)==".hkx")name.resize(name.size()-4);
                if(name.empty())name="Animation";
                std::string_view failure;
                CreatureCaptureDiagnostic candidate_diagnostic;
                if(!SampleClip(clip->flags,source,capture,sampled,failure,
                    creature && diagnostic.clip.empty()?&candidate_diagnostic:nullptr)){
                    if(first_failure.empty())first_failure=failure;
                    if(rejections.size()<8){
                        const auto* animation=source.binding?source.binding->animation.get():nullptr;
                        const auto* source_name=source.binding?source.binding->originalSkeletonName.c_str():nullptr;
                        const auto* target_name=rig.name.c_str();
                        rejections.push_back({name.substr(0,128),std::string(failure),
                            source_name?std::string(source_name).substr(0,256):"",target_name?std::string(target_name).substr(0,256):"",
                            animation?animation->numberOfTransformTracks:0,source.binding?source.binding->transformTrackToBoneIndices.size():0,source.mapped});
                    }
                    continue;
                }
                sampled.info.id="animation-"+std::to_string(generation)+"-"+std::to_string(result->clips.size());
                sampled.info.name=name.substr(0,128);
                if(!candidate_diagnostic.sampled.empty()){
                    candidate_diagnostic.clip=sampled.info.name;
                    diagnostic=std::move(candidate_diagnostic);
                }
                result->clips.push_back(std::move(sampled));
            }
            if(nodes>kMaxActiveNodes)break;
        }
    }
    } // Release the animation update lock before writing diagnostics.
    SKSE::log::info("SkyUltra animation capture actor {:08X}: graphs={}, rigs={}, nodes={}, clip nodes={}, loaded bindings={}, captured={}, basis={}",
        result->reference_id,graphs_seen,valid_rigs,nodes_seen,clips_seen,bindings_seen,result->clips.size(),creature?"mesh bind":"current humanoid pose");
    for(const auto& rejected:rejections)
        SKSE::log::info("SkyUltra animation capture skipped '{}': {} (tracks={}, map={}, source rig='{}', target rig='{}', retargeted={})",
            rejected.clip,rejected.reason,rejected.tracks,rejected.map_size,rejected.source_rig,rejected.target_rig,rejected.mapped);
    if(!diagnostic.clip.empty()){
        const auto& captured=diagnostic.rig;
        SKSE::log::info("SkyUltra creature capture diagnostic v1 actor {:08X}, clip='{}', bones={}, tracks={}, units={}, alignment=[{}]. Local transforms are Txyz;Qxyzw;Sxyz; matrices are row-major.",
            result->reference_id,diagnostic.clip,captured.names.size(),diagnostic.tracks.size(),captured.units,MatrixText(captured.alignment));
        std::vector<std::int32_t> bone_tracks(captured.names.size(),-1);
        for(std::size_t track=0;track<diagnostic.tracks.size();++track)
            if(diagnostic.tracks[track]>=0)bone_tracks[diagnostic.tracks[track]]=static_cast<std::int32_t>(track);
        std::size_t mapped{};
        for(std::size_t bone=0;bone<captured.names.size();++bone){
            const auto track=bone_tracks[bone];
            const auto bind=captured.mapped[bone]?MatrixText(captured.captured[bone]):"unmapped";
            const auto frame=captured.mapped[bone]?MatrixText(diagnostic.frame[mapped++]):"unmapped";
            SKSE::log::info("SkyUltra creature bone {} '{}': parent={}, locked={}, track={}, reference=[{}], sample0=[{}], bind=[{}], frame0=[{}]",
                bone,captured.names[bone],captured.parents[bone],captured.locked[bone],track,
                TransformText(captured.baseline[bone]),track>=0?TransformText(diagnostic.sampled[track]):"unmapped",bind,frame);
        }
    }
    if(!result->clips.empty())result->message="Captured animation frames. Choose a clip and frame; the real actor's animation is unchanged.";
    else if(valid_rigs==0)result->message="No complete animation rig pose is available for this actor. Try a nearby loaded actor.";
    else if(!first_failure.empty())result->message="No animation frames captured. "+first_failure+". Capture details are in SkyUltra.log.";
    else result->message="The actor's graph exposed no loaded animation clips. Capture details are in SkyUltra.log.";
    return result;
}
std::shared_ptr<const CapturedActorAnimations> LoadActorPoseLibrary(
    RE::Actor* actor, RE::NiAVObject* root, const RE::NiTransform&, std::uint64_t generation)
{
    auto result=std::make_shared<CapturedActorAnimations>();result->reference_id=actor->GetFormID();
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if(!actor->GetAnimationGraphManager(manager)||!manager){result->message="This actor has no animation rig.";return result;}
    RigCapture capture;bool found{};
    {
        const RE::BSSpinLockGuard lock(manager->GetRuntimeData().updateLock);
        for(const auto& graph:manager->graphs){
            if(!graph||!graph->characterInstance.setup||!graph->characterInstance.setup->animationSkeleton)continue;
            const auto& character=graph->characterInstance;
            const auto& rig=*character.setup->animationSkeleton;
            // Stock humanoid clips use the 99-bone animation rig. Extra NIF
            // attachment bones remain supported through ancestor inheritance.
            if(rig.bones.size()!=99)continue;
            RigCapture candidate;if(!CaptureRig(character,root,rig,candidate))continue;
            const auto has=[&](const char* name){return std::find(candidate.names.begin(),candidate.names.end(),name)!=candidate.names.end();};
            if(candidate.skeleton!="NPC Root [Root]"||!has("NPC Head [Head]")||!has("NPC Pelvis [Pelv]")||
                !has("NPC L UpperArm [LUar]")||!has("NPC R UpperArm [RUar]")||
                !has("NPC L Thigh [LThg]")||!has("NPC R Thigh [RThg]"))continue;
            capture=std::move(candidate);found=true;break;
        }
    }
    if(!found){result->message="The pose library currently supports the standard humanoid animation rig. Capture animations supports other compatible rigs.";return result;}
    // The graph lock is released: file I/O and private sampling use only copied
    // rig data, with no animation events or temporary bindings on the real NPC.
    const auto* base=actor->GetActorBase();
    const bool female=base&&base->GetSex()==RE::SEXES::kFemale;
    struct Preset {const char* id;const char* name;const char* file;};
    const std::array<Preset,4> library{{
        {"idle","Idle",female?"meshes\\actors\\character\\animations\\female\\mt_idle.hkx":"meshes\\actors\\character\\animations\\male\\mt_idle.hkx"},
        {"fighting","Fighting","meshes\\actors\\character\\animations\\h2h_idle.hkx"},
        {"guard","Guard","meshes\\actors\\character\\animations\\1hm_blockidle.hkx"},
        {"spellcasting","Spellcasting","meshes\\actors\\character\\animations\\mrh_aimedconcentration.hkx"}
    }};
    for(const auto& preset:library){
        std::string error;
        auto file=ActorPoseFile::Load(preset.file,error);
        if(file){
            const auto* animation=file->Animation();
            const auto mapping=ResolveAnimationTracks(file->Tracks(),animation->numberOfTransformTracks,99);
            std::array<bool,kMaxAnimationRigBones> covered{};
            if(!mapping.error)for(std::int32_t track=0;track<animation->numberOfTransformTracks;++track)
                if(mapping.bones[track]>=0)covered[mapping.bones[track]]=true;
            bool missing_body{};
            for(std::size_t bone=0;bone<capture.names.size();++bone)
                if(!covered[bone]&&!capture.names[bone].starts_with("Camera"))missing_body=true;
            if(mapping.error||missing_body){
                error="Library pose is missing required humanoid body tracks; camera tracks may be omitted.";
            }else {
                CapturedAnimation pose;std::string_view failure;
                if(SampleClip(0,{nullptr,false,file.get()},capture,pose,failure)){
                    pose.info.id="pose-"+std::to_string(generation)+"-"+preset.id;
                    pose.info.name=preset.name;pose.info.preset=preset.id;
                    result->clips.push_back(std::move(pose));
                }else error=failure;
            }
        }
        if(!error.empty())SKSE::log::warn("SkyUltra pose '{}' unavailable for actor {:08X}: {} ({})",preset.id,result->reference_id,error,preset.file);
    }
    SKSE::log::info("SkyUltra loaded {}/4 independent poses for actor {:08X}.",result->clips.size(),result->reference_id);
    if(result->clips.size()==library.size())result->message="Loaded Idle, Fighting, Guard and Spellcasting. Choose a pose and frame.";
    else if(!result->clips.empty())result->message="Loaded "+std::to_string(result->clips.size())+" of 4 poses. Some assets are unavailable or incompatible; details are in SkyUltra.log.";
    else result->message="No compatible library poses could be loaded. Details are in SkyUltra.log.";
    return result;
}
} // namespace skyultra::detail
