#include "SkyUltra/ActorPreview.h"
#include "SkyUltra/VideoSplash.h"
#include "ActorAnimation.h"
#include "ActorSkeleton.h"
#include "ActorSkinWeights.h"

#include <SKSE/SKSE.h>
#include <RE/A/Actor.h>
#include <RE/B/BSAtomic.h>
#include <RE/B/BSDynamicTriShape.h>
#include <RE/B/BSDismemberSkinInstance.h>
#include <RE/B/BGSBodyPartData.h>
#include <RE/B/BGSOutfit.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSModelDB.h>
#include <RE/B/BSShaderManager.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSLightingShaderMaterialEnvmap.h>
#include <RE/B/BSLightingShaderMaterialEye.h>
#include <RE/B/BSLightingShaderMaterialFacegen.h>
#include <RE/B/BSLightingShaderMaterialFacegenTint.h>
#include <RE/B/BSLightingShaderMaterialGlowmap.h>
#include <RE/B/BSLightingShaderMaterialHairTint.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSShaderProperty.h>
#include <RE/B/BSTriShape.h>
#include <RE/N/NiAlphaProperty.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSkinData.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiSkinPartition.h>
#include <RE/N/NiSourceTexture.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/R/Renderer.h>
#include <RE/T/TESFile.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESModel.h>
#include <RE/T/TESModelTextureSwap.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/U/UI.h>
#include <RE/RTTI.h>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <filesystem>
#include <fstream>
#include <future>
#include <cstdio>
#include <string_view>
#include <stdexcept>
#include <cfloat>
#include <sstream>
#include <iomanip>

namespace skyultra {
namespace {
using Microsoft::WRL::ComPtr;
using DirectX::XMFLOAT4X4;
using detail::FiniteTransform;
using detail::RootLocalFromLocals;
constexpr std::size_t kMaxBones = 256;
constexpr std::size_t kMaxPackets = 256;
constexpr std::uint32_t kMaxVertices = 100000;
constexpr std::uint32_t kMaxTriangles = 200000;

enum TextureSlot : std::size_t { Diffuse, Normal, FaceTint, FaceDetail, SoftLight, SpecularMap, Environment, EnvironmentMask, Glow, TextureCount };
enum MaterialFlag : std::uint32_t {
    VertexColor = 1U, VertexAlpha = 2U, HairTint = 4U, FaceGen = 8U,
    FaceRgbTint = 16U, ModelSpaceNormal = 32U, SoftLighting = 64U,
    RimLighting = 128U, BackLighting = 256U, SpecularLighting = 512U,
    AlphaBlend = 1024U, AlphaTest = 2048U, PremultipliedTexture = 4096U,
    GlowLighting = 8192U, EnvironmentLighting = 16384U
};

// Only numeric values and retained GPU views cross from the game task to the
// compositor. No material/texture-set pointers are dereferenced while drawing.
struct alignas(16) MaterialConstants {
    DirectX::XMFLOAT4 tint{1,1,1,1};
    DirectX::XMFLOAT4 uv{1,1,0,0};
    DirectX::XMFLOAT4 specular{1,1,1,16};
    DirectX::XMFLOAT4 lighting{}; // specular scale, soft rolloff, rim power, env scale
    DirectX::XMFLOAT4 alpha{1,0,4,0}; // opacity, threshold, Ni test function, reserved
    DirectX::XMFLOAT4 emissive{};
    std::array<std::uint32_t,4> features{}; // material bits, texture mask, sRGB-view mask
};
static_assert(sizeof(MaterialConstants) == 112);

// A packet owns COM references and numeric pose data. It contains no pointers
// into an actor's skeleton, skin partition, materials, or temporary scene graph.
struct DrawPacket {
    std::string shape_name;
    std::vector<std::string> diagnostic_skin_bones;
    ComPtr<ID3D11Buffer> vertices;
    ComPtr<ID3D11Buffer> indices;
    std::array<ComPtr<ID3D11ShaderResourceView>,TextureCount> textures;
    MaterialConstants material;
    std::vector<std::uint8_t> vertex_bytes;
    std::vector<std::uint16_t> index_values;
    std::vector<DirectX::XMFLOAT4> dynamic_positions;
    std::shared_ptr<const std::vector<detail::ActorSkinVertex>> skin_vertices;
    std::array<XMFLOAT4X4, kMaxBones> bones{};
    XMFLOAT4X4 world{};
    std::uint64_t descriptor{};
    std::uint32_t stride{};
    std::uint32_t triangle_count{};
    std::uint32_t uv_offset{};
    std::uint32_t skin_offset{};
    std::uint32_t normal_offset{};
    std::uint32_t color_offset{};
    std::uint32_t clamp_mode{3};
    D3D11_BLEND source_blend{D3D11_BLEND_SRC_ALPHA};
    D3D11_BLEND destination_blend{D3D11_BLEND_INV_SRC_ALPHA};
    DirectX::XMFLOAT3 sort_center{};
    bool skinned{};
    bool has_uv{};
    bool has_normal{};
    bool has_colors{};
    bool two_sided{};
    bool depth_test{true};
    bool depth_write{true};
    bool full_precision{};
    bool volatile_vertices{};
};

struct ItemMeshBuffers;
struct ItemSourceAssets;
struct SceneSnapshot {
    // Keep the engine's model/material/texture owners alive as an opaque lease.
    // Declared before packets so DrawPacket ComPtrs release before the lease.
    std::shared_ptr<const ItemSourceAssets> item_sources;
    std::shared_ptr<const ItemSourceAssets> actor_sources;
    std::vector<DrawPacket> packets;
    bool item_preview{};
    bool ignore_world_fade{};
    bool bind_pose{};
    DirectX::XMFLOAT3 center{};
    DirectX::XMFLOAT3 head{};
    float radius{96.0F};
    float portrait_radius{32.0F};
    float facing_yaw{};
    float portrait_yaw{};
    std::uint32_t reference_id{};
    std::size_t skipped{};
    // Render-thread-only cache. Camera/lighting changes and PNG jobs keep the
    // snapshot, so they use exactly the same private buffers without reloading.
    mutable std::shared_ptr<const ItemMeshBuffers> item_meshes;
};

struct PacketBuffers {
    ComPtr<ID3D11Buffer> vertices;
    ComPtr<ID3D11Buffer> indices;
    ComPtr<ID3D11Buffer> dynamic;
    ComPtr<ID3D11Buffer> skin;
    UINT vertex_capacity{}, index_capacity{}, dynamic_capacity{}, skin_capacity{};
    bool valid{};
};

// Immutable copies for item redraws. Keep these independently of a target's
// dimensions: lighting, resize and PNG export all reuse the same mesh snapshot.
struct ItemPacketResources {
    PacketBuffers mesh;
    std::array<ComPtr<ID3D11ShaderResourceView>,TextureCount> textures;
};
struct ItemMeshBuffers {
    ID3D11Device* device{}; // comparison only; packet buffers retain the device
    std::vector<ItemPacketResources> packets;
};

struct RenderedFrame {
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11Texture2D> linear_color;
    ComPtr<ID3D11RenderTargetView> linear_rtv;
    ComPtr<ID3D11ShaderResourceView> linear_srv;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    SizeU size{};
    std::uint64_t rendered_revision{};
    std::vector<PacketBuffers> packet_buffers;
    // Item redraws do not map/rename the shared actor constant buffer. Each
    // draw packet owns a default-usage buffer on this target's device.
    std::vector<ComPtr<ID3D11Buffer>> item_constants;
    std::shared_ptr<const SceneSnapshot> uploaded_scene;
    ID3D11Device* target_device{}; // comparison only, color/depth own the device lifetime
    bool target_failed{};
};

struct AnimationScene {
    std::shared_ptr<const SceneSnapshot> scene;
    ActorPreviewStatus status;
    std::string clip;
    std::uint32_t frame{};
    std::size_t bytes{};
};
using AnimationSceneCache=std::vector<AnimationScene>;
struct PreviewState {
    ActorPreviewRequest request;
    ActorPreviewStatus status;
    std::shared_ptr<const SceneSnapshot> scene;
    std::shared_ptr<RenderedFrame> frame;
    std::uint64_t revision{};
    bool compositor_visible{};
    std::chrono::steady_clock::time_point last_pose_capture{};
    std::shared_ptr<const detail::CapturedActorAnimations> animations;
    AnimationSceneCache animation_scenes;
};

std::mutex g_mutex;
std::unordered_map<ViewId, PreviewState> g_previews;
std::uint32_t g_full_view_export_size{4096},g_portrait_export_size{4096}; // protected by g_mutex
std::atomic_bool g_tick_queued{};
std::chrono::steady_clock::time_point g_last_tick_request{}; // render thread only
std::uint64_t g_animation_generation{}; // protected by g_mutex

XMFLOAT4X4 Matrix(const RE::NiTransform& transform)
{
    const auto& r = transform.rotate.entry;
    const auto s = transform.scale;
    return XMFLOAT4X4(r[0][0]*s, r[1][0]*s, r[2][0]*s, 0,
        r[0][1]*s, r[1][1]*s, r[2][1]*s, 0,
        r[0][2]*s, r[1][2]*s, r[2][2]*s, 0,
        transform.translate.x, transform.translate.y, transform.translate.z, 1);
}

bool Finite(const XMFLOAT4X4& matrix)
{
    for (const auto& row : matrix.m) for (float value : row) if (!std::isfinite(value)) return false;
    return true;
}

float MaterialNumber(float value, float fallback, float low, float high)
{
    return std::isfinite(value) ? std::clamp(value,low,high) : fallback;
}

DirectX::XMFLOAT4 MaterialColor(const RE::NiColor& value, float alpha = 1)
{
    return {MaterialNumber(value.red,1,0,16),MaterialNumber(value.green,1,0,16),
        MaterialNumber(value.blue,1,0,16),alpha};
}

ComPtr<ID3D11ShaderResourceView> RetainTexture(RE::NiSourceTexture* texture,
    std::vector<RE::NiPointer<RE::NiTexture>>* retained_textures = nullptr)
{
    ComPtr<ID3D11ShaderResourceView> view;
    if (texture != nullptr && texture->rendererTexture != nullptr) {
        view = reinterpret_cast<ID3D11ShaderResourceView*>(texture->rendererTexture->resourceView);
        if (view && retained_textures != nullptr)
            retained_textures->emplace_back(texture);
    }
    return view;
}

bool SRGBView(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB: return true;
    default: return false;
    }
}

D3D11_BLEND AlphaFactor(RE::NiAlphaProperty::AlphaFunction value)
{
    using Function = RE::NiAlphaProperty::AlphaFunction;
    switch (value) {
    case Function::kOne: return D3D11_BLEND_ONE;
    case Function::kZero: return D3D11_BLEND_ZERO;
    case Function::kSrcColor: return D3D11_BLEND_SRC_COLOR;
    case Function::kInvSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
    case Function::kDestColor: return D3D11_BLEND_DEST_COLOR;
    case Function::kInvDestColor: return D3D11_BLEND_INV_DEST_COLOR;
    case Function::kSrcAlpha: return D3D11_BLEND_SRC_ALPHA;
    case Function::kInvSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
    case Function::kDestAlpha: return D3D11_BLEND_DEST_ALPHA;
    case Function::kInvDestAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
    case Function::kSrcAlphaSat: return D3D11_BLEND_SRC_ALPHA_SAT;
    default: return D3D11_BLEND_SRC_ALPHA;
    }
}

constexpr float PreviewOpacity(float property_alpha,float material_alpha,bool ignore_world_fade)
{
    return ignore_world_fade?1.0F:property_alpha*material_alpha;
}
static_assert(PreviewOpacity(.5F,.25F,false)==.125F);
static_assert(PreviewOpacity(.05F,0.0F,true)==1.0F);

void CaptureMaterial(DrawPacket& packet, RE::BSGeometry* geometry, bool ignore_world_fade,
    std::vector<RE::NiPointer<RE::NiTexture>>* retained_textures = nullptr)
{
    const auto& runtime = geometry->GetGeometryRuntimeData();
    auto& data = packet.material;
    auto& flags = data.features[0];
    using PropertyFlag = RE::BSShaderProperty::EShaderPropertyFlag;
    if (auto* property = runtime.shaderProperty.get()) {
        packet.textures[Diffuse] = RetainTexture(property->GetBaseTexture(), retained_textures);
        packet.two_sided = property->flags.any(PropertyFlag::kTwoSided);
        packet.depth_test = property->flags.any(PropertyFlag::kZBufferTest);
        packet.depth_write = property->flags.any(PropertyFlag::kZBufferWrite);
        const auto property_alpha=MaterialNumber(property->alpha,1,0,1);
        data.alpha.x = PreviewOpacity(property_alpha,1,ignore_world_fade);
        const auto capture_flag = [&](PropertyFlag source, MaterialFlag target) {
            if (property->flags.any(source)) flags |= target;
        };
        capture_flag(PropertyFlag::kVertexColors,VertexColor);
        capture_flag(PropertyFlag::kVertexAlpha,VertexAlpha);
        capture_flag(PropertyFlag::kModelSpaceNormals,ModelSpaceNormal);
        capture_flag(PropertyFlag::kSpecular,SpecularLighting);
        capture_flag(PropertyFlag::kSoftLighting,SoftLighting);
        capture_flag(PropertyFlag::kRimLighting,RimLighting);
        capture_flag(PropertyFlag::kBackLighting,BackLighting);
        capture_flag(PropertyFlag::kPremultAlpha,PremultipliedTexture);
        if (auto* material = property->GetBaseMaterial(); material != nullptr &&
            material->GetType() == RE::BSShaderMaterial::Type::kLighting) {
            const auto* lighting = static_cast<RE::BSLightingShaderMaterialBase*>(material);
            if (const auto diffuse = RetainTexture(lighting->diffuseTexture.get(), retained_textures)) packet.textures[Diffuse] = diffuse;
            packet.textures[Normal] = RetainTexture(lighting->normalTexture.get(), retained_textures);
            packet.textures[SoftLight] = RetainTexture(lighting->rimSoftLightingTexture.get(), retained_textures);
            packet.textures[SpecularMap] = RetainTexture(lighting->specularBackLightingTexture.get(), retained_textures);
            packet.clamp_mode = static_cast<std::uint32_t>(std::clamp(lighting->textureClampMode,0,3));
            data.uv = {MaterialNumber(material->texCoordScale[0].x,1,-1024,1024),
                MaterialNumber(material->texCoordScale[0].y,1,-1024,1024),
                MaterialNumber(material->texCoordOffset[0].x,0,-4096,4096),
                MaterialNumber(material->texCoordOffset[0].y,0,-4096,4096)};
            // Skyrim updates materialAlpha at runtime when fading the player.
            // An isolated outfit preview keeps its own scalar opacity. Texture
            // alpha, vertex alpha, NIF blend modes and alpha tests still apply.
            data.alpha.x = PreviewOpacity(property_alpha,MaterialNumber(lighting->materialAlpha,1,0,1),ignore_world_fade);
            data.specular = MaterialColor(lighting->specularColor,MaterialNumber(lighting->specularPower,16,1,512));
            data.lighting = {MaterialNumber(lighting->specularColorScale,0,0,16),
                MaterialNumber(lighting->subSurfaceLightRolloff,0,0,4),
                MaterialNumber(lighting->rimLightPower,0,0,32),0};
            using Feature = RE::BSShaderMaterial::Feature;
            switch (material->GetFeature()) {
            case Feature::kHairTint:
                data.tint = MaterialColor(static_cast<const RE::BSLightingShaderMaterialHairTint*>(material)->tintColor);
                flags |= HairTint; break;
            case Feature::kFaceGen: {
                const auto* face = static_cast<const RE::BSLightingShaderMaterialFacegen*>(material);
                packet.textures[FaceTint] = RetainTexture(face->tintTexture.get(), retained_textures);
                packet.textures[FaceDetail] = RetainTexture(face->detailTexture.get(), retained_textures);
                packet.textures[SoftLight] = RetainTexture(face->subsurfaceTexture.get(), retained_textures);
                flags |= FaceGen; break;
            }
            case Feature::kFaceGenRGBTint:
                data.tint = MaterialColor(static_cast<const RE::BSLightingShaderMaterialFacegenTint*>(material)->tintColor);
                flags |= FaceRgbTint; break;
            case Feature::kEnvironmentMap: {
                const auto* environment = static_cast<const RE::BSLightingShaderMaterialEnvmap*>(material);
                packet.textures[Environment] = RetainTexture(environment->envTexture.get(), retained_textures);
                packet.textures[EnvironmentMask] = RetainTexture(environment->envMaskTexture.get(), retained_textures);
                data.lighting.w = MaterialNumber(environment->envMapScale,0,0,16);
                flags |= EnvironmentLighting; break;
            }
            case Feature::kEye: {
                const auto* eye = static_cast<const RE::BSLightingShaderMaterialEye*>(material);
                packet.textures[Environment] = RetainTexture(eye->envTexture.get(), retained_textures);
                packet.textures[EnvironmentMask] = RetainTexture(eye->envMaskTexture.get(), retained_textures);
                data.lighting.w = MaterialNumber(eye->envMapScale,0,0,16);
                flags |= EnvironmentLighting; break;
            }
            case Feature::kGlowMap:
                packet.textures[Glow] = RetainTexture(static_cast<const RE::BSLightingShaderMaterialGlowmap*>(material)->glowTexture.get(), retained_textures);
                flags |= GlowLighting; break;
            default: break;
            }
            if (const auto* lighting_property = netimmerse_cast<RE::BSLightingShaderProperty*>(property);
                lighting_property != nullptr && lighting_property->emissiveColor != nullptr) {
                data.emissive = MaterialColor(*lighting_property->emissiveColor,MaterialNumber(lighting_property->emissiveMult,0,0,32));
            }
        }
    }
    if (const auto* alpha = runtime.alphaProperty.get()) {
        if (alpha->GetAlphaTesting()) {
            flags |= AlphaTest;
            data.alpha.y = static_cast<float>(alpha->alphaThreshold)/255.0F;
            data.alpha.z = static_cast<float>((alpha->alphaFlags >> 10U) & 7U);
        }
        if (alpha->GetAlphaBlending()) {
            flags |= AlphaBlend;
            packet.source_blend = AlphaFactor(alpha->GetSrcBlendMode());
            packet.destination_blend = AlphaFactor(alpha->GetDestBlendMode());
        }
    }
    if (data.alpha.x < .999F) flags |= AlphaBlend;
    data.features[3] = packet.two_sided ? 1U : 0U;
    for (std::size_t index=0;index<TextureCount;++index) {
        auto& texture = packet.textures[index];
        if (!texture) continue;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{}; texture->GetDesc(&desc);
        if ((index == Environment && desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBE) ||
            (index != Environment && desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)) { texture.Reset(); continue; }
        data.features[1] |= 1U << index;
        if (SRGBView(desc.Format)) data.features[2] |= 1U << index;
    }
}

RE::Actor* FindActor(const ActorPreviewRequest& request)
{
    if (request.reference_id == 0x14 || (request.reference_id == 0 && request.base_form_id == 0x07)) {
        if (auto* player = RE::PlayerCharacter::GetSingleton();
            player != nullptr && (player->Get3D(false) != nullptr || player->Get3D() != nullptr)) {
            return player;
        }
    }
    if (request.reference_id != 0) {
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(request.reference_id);
            actor != nullptr && (actor->Get3D(false) != nullptr || (actor->IsPlayerRef() && actor->Get3D() != nullptr))) return actor;
    }
    if (request.base_form_id == 0) return nullptr;
    if (request.base_form_id == 0x07) {
        if (auto* player = RE::PlayerCharacter::GetSingleton();
            player != nullptr && (player->Get3D(false) != nullptr || player->Get3D() != nullptr)) {
            return player;
        }
    }
    RE::Actor* match{};
    if (auto* lists = RE::ProcessLists::GetSingleton()) {
        lists->ForAllActors([&](RE::Actor* actor) {
            if (actor != nullptr && actor->Get3D(false) != nullptr) {
                const auto* base = actor->GetActorBase();
                if (base != nullptr && base->GetFormID() == request.base_form_id) {
                    match = actor;
                    return RE::BSContainer::ForEachResult::kStop;
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    return match;
}

// Prefer the engine's world transform (keeps physics-driven bones such as SMP hair),
// but fall back to the local chain whenever the world transform is stale or bogus.
RE::NiTransform LiveRootLocal(RE::NiAVObject* object, RE::NiAVObject* root, const RE::NiTransform& root_inverse)
{
    const RE::NiTransform from_world = root_inverse * object->world;
    RE::NiTransform from_locals;
    if (RootLocalFromLocals(object, root, from_locals) && FiniteTransform(from_locals)) {
        if (!FiniteTransform(from_world)) return from_locals;
        const auto delta = from_world.translate - from_locals.translate;
        const float drift = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (drift > 128.0F || std::abs(from_world.scale - from_locals.scale) > 0.25F) return from_locals;
    }
    return from_world;
}

// Pose one captured skeleton. FaceGen, outfits and animation props can have
// different mesh bind spaces: never combine their inverse binds into a new
// skeleton. Only change arm joints, inheriting each delta through its actual
// descendants, so the head, neck and rigid hand attachments remain attached.
// Captured animation frames instead supply model-space bone overrides; unmatched
// face/attachment nodes inherit those overrides through the same parent chain.
class PreviewPose {
public:
    PreviewPose(RE::NiAVObject* root, const RE::NiTransform& inverse, ActorPreviewPose pose,
        const detail::CapturedAnimation* animation=nullptr,std::uint32_t frame=0,bool bind_non_humanoid=false,
        bool is_humanoid_npc=true,bool trace_capture=false) :
        root_(root), inverse_(inverse), pose_(pose), trace_capture_(trace_capture) {
        humanoid_=is_humanoid_npc && detail::HasHumanoidPreviewArms(root_);
        if(animation && frame<animation->frames.size() && animation->bind_pose.size()==animation->bone_names.size() &&
            animation->frames[frame].size()==animation->bone_names.size()){
            has_animation_=true;
            mesh_bind_animation_=animation->mesh_bind_basis;
            const auto& transforms=animation->frames[frame];
            for(std::size_t i=0;i<animation->bone_names.size();++i){
                animated_.emplace(animation->bone_names[i],transforms[i]);
                if(mesh_bind_animation_){
                    XMFLOAT4X4 delta;
                    DirectX::XMStoreFloat4x4(&delta,DirectX::XMMatrixInverse(nullptr,DirectX::XMLoadFloat4x4(&animation->bind_pose[i]))*
                        DirectX::XMLoadFloat4x4(&transforms[i]));
                    animated_deltas_.emplace(animation->bone_names[i],delta);
                    bind_deltas_.push_back({animation->bind_pose[i],delta});
                }
            }
        }else if(bind_non_humanoid && !humanoid_){
            PopulateBindPose(root_);
        }
    }
    bool Supported() const {
        return humanoid_;
    }
    bool BindPose() const {
        return bind_pose_;
    }
    bool HasAnimation() const { return has_animation_; }
    bool MeshBindAnimation() const { return mesh_bind_animation_; }
    bool TraceCapture() const { return trace_capture_; }
    DirectX::XMMATRIX AnimationDelta(RE::NiAVObject* object) const {
        for(unsigned depth=0;object && depth<128;object=object->parent,++depth){
            const auto* name=object->name.c_str();
            if(name)if(const auto found=animated_deltas_.find(name);found!=animated_deltas_.end())
                return DirectX::XMLoadFloat4x4(&found->second);
        }
        return DirectX::XMMatrixIdentity();
    }
    DirectX::XMMATRIX AnimationDeltaForSkinBone(RE::NiAVObject* object,const RE::NiTransform& skin_to_bone,
        const RE::NiTransform& root_to_skin) const {
        if(object)return AnimationDelta(object);
        if(bind_deltas_.empty()||!FiniteTransform(skin_to_bone)||!FiniteTransform(root_to_skin)||
            std::abs(skin_to_bone.scale)<=.0001F||std::abs(root_to_skin.scale)<=.0001F)
            return DirectX::XMMatrixIdentity();
        const auto bind=Matrix((skin_to_bone*root_to_skin).Invert());
        if(!Finite(bind))return DirectX::XMMatrixIdentity();
        const BindDelta* best{};
        float best_score=FLT_MAX;
        for(const auto& candidate:bind_deltas_){
            const float dx=candidate.bind._41-bind._41;
            const float dy=candidate.bind._42-bind._42;
            const float dz=candidate.bind._43-bind._43;
            const float trans_sq=dx*dx+dy*dy+dz*dz;
            if(trans_sq>9.0F)continue;
            float rot_sq=0.0F;
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){
                const float d=candidate.bind.m[r][c]-bind.m[r][c];
                rot_sq+=d*d;
            }
            const float score=trans_sq+rot_sq*4.0F;
            if(score<best_score){best_score=score;best=&candidate;}
        }
        return best?DirectX::XMLoadFloat4x4(&best->delta):DirectX::XMMatrixIdentity();
    }
    DirectX::XMMATRIX World(RE::NiAVObject* object, unsigned depth=0) {
        using namespace DirectX;
        if (!object || object==root_ || depth>128) return XMMatrixIdentity();
        if(!animated_.empty()){
            const auto* name=object->name.c_str();
            if(name)if(const auto found=animated_.find(name);found!=animated_.end())return XMLoadFloat4x4(&found->second);
            if(const auto found=posed_.find(object);found!=posed_.end())return XMLoadFloat4x4(&found->second);
            if((bind_pose_ || !humanoid_) && FiniteTransform(object->local)){
                const auto local_mat=Matrix(object->local);
                if(Finite(local_mat)){
                    const auto result=XMLoadFloat4x4(&local_mat)*World(object->parent,depth+1);
                    XMFLOAT4X4 stored;XMStoreFloat4x4(&stored,result);posed_.emplace(object,stored);
                    return result;
                }
            }
            // FaceGen-only bones and rigid props inherit the nearest sampled
            // ancestor, keeping their captured local attachment transforms.
            const auto result=CapturedWorld(object)*XMMatrixInverse(nullptr,CapturedWorld(object->parent))*World(object->parent,depth+1);
            XMFLOAT4X4 stored;XMStoreFloat4x4(&stored,result);posed_.emplace(object,stored);
            return result;
        }
        if (pose_==ActorPreviewPose::Current || !Supported() || !InArmBranch(object)) {
            return CapturedWorld(object);
        }
        if (auto found=posed_.find(object);found!=posed_.end()) return XMLoadFloat4x4(&found->second);
        const auto original=CapturedWorld(object), parent=CapturedWorld(object->parent);
        auto result=original*XMMatrixInverse(nullptr,parent)*World(object->parent,depth+1);
        const auto* object_name=object->name.c_str();
        const std::string_view name=object_name?object_name:"";
        const bool left=name=="NPC L UpperArm [LUar]" || name=="NPC L Forearm [LLar]";
        const bool upper=name=="NPC L UpperArm [LUar]" || name=="NPC R UpperArm [RUar]";
        const bool forearm=name=="NPC L Forearm [LLar]" || name=="NPC R Forearm [RLar]";
        if (upper || forearm) {
            const char* next=upper?(left?"NPC L Forearm [LLar]":"NPC R Forearm [RLar]"):
                (left?"NPC L Hand [LHnd]":"NPC R Hand [RHnd]");
            if (auto* child=root_->GetObjectByName(RE::BSFixedString(next))) {
                const auto from=XMVector3Normalize(XMVector3TransformNormal(
                    XMVectorSubtract(CapturedWorld(child).r[3],original.r[3]),XMMatrixInverse(nullptr,original)*result));
                // Determine the outward direction from the captured shoulder,
                // rather than assuming a modded skeleton's left/right axis.
                auto* shoulder=root_->GetObjectByName(RE::BSFixedString(left?"NPC L UpperArm [LUar]":"NPC R UpperArm [RUar]"));
                auto* other_shoulder=root_->GetObjectByName(RE::BSFixedString(left?"NPC R UpperArm [RUar]":"NPC L UpperArm [LUar]"));
                const auto side=shoulder && other_shoulder && XMVectorGetX(XMVectorSubtract(CapturedWorld(shoulder).r[3],CapturedWorld(other_shoulder).r[3]))<0?-1.0F:1.0F;
                XMVECTOR to=XMVectorSet(side*.12F,.06F,-1,0);
                if (pose_==ActorPreviewPose::Fighting) to=upper?XMVectorSet(side*.35F,.55F,-.7F,0):XMVectorSet(-side*.15F,.75F,.8F,0);
                if (pose_==ActorPreviewPose::Guard) to=upper?XMVectorSet(side*.15F,.6F,-.6F,0):XMVectorSet(-side*.25F,.4F,1,0);
                if (pose_==ActorPreviewPose::Spellcasting) to=upper?XMVectorSet(side*.35F,1,.05F,0):XMVectorSet(side*.1F,1,.25F,0);
                to=XMVector3Normalize(to);
                const auto dot=std::clamp(XMVectorGetX(XMVector3Dot(from,to)),-1.0F,1.0F);
                auto axis=XMVector3Cross(from,to);
                if (XMVectorGetX(XMVector3LengthSq(axis))<.000001F) axis=XMVector3Orthogonal(from);
                if (std::isfinite(dot)) {
                    const auto pivot=result.r[3];
                    result=result*XMMatrixTranslationFromVector(XMVectorNegate(pivot))*
                        XMMatrixRotationAxis(axis,std::acos(dot))*XMMatrixTranslationFromVector(pivot);
                }
            }
        }
        XMFLOAT4X4 stored;XMStoreFloat4x4(&stored,result);posed_.emplace(object,stored);
        return result;
    }
    bool Active() const {return !animated_.empty() || !posed_.empty() || (pose_!=ActorPreviewPose::Current && Supported());}
private:
    void PopulateBindPose(RE::NiAVObject* root) {
        for (const auto& [bone, transform] : detail::CaptureSkinBindPose(root)) {
            const auto bind_matrix=Matrix(transform);
            posed_[bone]=bind_matrix;
            if (const auto* raw=bone->name.c_str();raw && raw[0]) animated_[raw]=bind_matrix;
            bind_pose_=true;
        }
    }
    bool InArmBranch(RE::NiAVObject* object) const {
        for(unsigned depth=0;object && object!=root_ && depth<128;object=object->parent,++depth){
            const auto* text=object->name.c_str();
            const std::string_view name=text?text:"";
            if(name=="NPC L UpperArm [LUar]"||name=="NPC R UpperArm [RUar]"||
                name=="NPC L Forearm [LLar]"||name=="NPC R Forearm [RLar]")return true;
        }
        return false;
    }
    DirectX::XMMATRIX CapturedWorld(RE::NiAVObject* object) const {
        if (!object || object==root_) return DirectX::XMMatrixIdentity();
        if (const auto found=captured_.find(object);found!=captured_.end()) return DirectX::XMLoadFloat4x4(&found->second);
        const auto matrix=Matrix(LiveRootLocal(object,root_,inverse_));
        captured_.emplace(object,matrix);
        return DirectX::XMLoadFloat4x4(&matrix);
    }
    RE::NiAVObject* root_;
    RE::NiTransform inverse_;
    ActorPreviewPose pose_;
    bool humanoid_{};
    bool bind_pose_{};
    bool has_animation_{};
    bool mesh_bind_animation_{};
    bool trace_capture_{};
    struct BindDelta {
        XMFLOAT4X4 bind;
        XMFLOAT4X4 delta;
    };
    std::unordered_map<RE::NiAVObject*,XMFLOAT4X4> posed_;
    mutable std::unordered_map<RE::NiAVObject*,XMFLOAT4X4> captured_;
    std::unordered_map<std::string,XMFLOAT4X4> animated_;
    std::unordered_map<std::string,XMFLOAT4X4> animated_deltas_;
    std::vector<BindDelta> bind_deltas_;
};

std::shared_ptr<const std::vector<detail::ActorSkinVertex>> CaptureSkinVertices(
    RE::NiSkinInstance* skin,const RE::NiSkinPartition::Partition* partition,std::uint32_t count)
{
    auto vertices=std::make_shared<std::vector<detail::ActorSkinVertex>>(count);
    const auto bone_count=skin->skinData->GetBoneCount();
    for(std::uint32_t bone=0;bone<bone_count;++bone){
        const auto* weights=skin->skinData->GetBoneDataBoneVertData(bone);
        const auto size=skin->skinData->GetBoneDataVerts(bone);
        if(!weights)continue;
        for(std::uint32_t i=0;i<size;++i)if(weights[i].vert<count)
            detail::AddActorSkinWeight((*vertices)[weights[i].vert],bone,weights[i].weight);
    }
    const auto weighted=[&](std::uint32_t index){
        const auto& w=(*vertices)[index].weights;const auto total=w[0]+w[1]+w[2]+w[3];return total>=.98F && total<=1.02F;
    };
    if(std::all_of(vertices->begin(),vertices->end(),[](const auto& vertex){
        const auto& w=vertex.weights;const auto total=w[0]+w[1]+w[2]+w[3];return total>=.98F && total<=1.02F;
    }))return vertices;
    // Some runtime meshes discard the original per-bone weight lists. Their
    // CPU partition metadata still gives an explicit local-to-global mapping.
    if(partition&&partition->weights&&partition->bonePalette&&partition->bones&&
        partition->vertices&&partition->numBones<=bone_count&&
        partition->bonesPerVertex>=1&&partition->bonesPerVertex<=4){
        bool partition_ok=true;
        for(std::uint32_t i=0;i<partition->vertices;++i){
            const auto index=partition->vertexMap?partition->vertexMap[i]:i;
            if(index>=count){partition_ok=false;break;}
            if(weighted(index))continue;
            (*vertices)[index]={};
            for(std::uint32_t j=0;j<partition->bonesPerVertex;++j){
                const auto offset=i*partition->bonesPerVertex+j;
                const auto local=partition->bonePalette[offset];
                if(local<partition->numBones){
                    const auto bone=partition->bones[local];
                    if(bone<bone_count)detail::AddActorSkinWeight((*vertices)[index],bone,partition->weights[offset]);
                }
            }
            if(!weighted(index)){partition_ok=false;break;}
        }
        if(partition_ok && std::all_of(vertices->begin(),vertices->end(),[](const auto& vertex){
            const auto& w=vertex.weights;const auto total=w[0]+w[1]+w[2]+w[3];return total>=.98F && total<=1.02F;
        }))return vertices;
    }
    if(bone_count==1){
        for(std::uint32_t i=0;i<count;++i){
            (*vertices)[i]={};
            detail::AddActorSkinWeight((*vertices)[i],0,1.0F);
        }
        return vertices;
    }
    return {};
}

bool FillPacket(DrawPacket& packet, RE::BSGeometry* geometry,
    RE::BSGraphics::TriShape* buffers, std::uint32_t vertex_count,
    std::uint32_t triangles, const RE::NiTransform& root_inverse,
    const std::vector<DirectX::XMFLOAT4>& dynamic_positions, PreviewPose& pose, bool ignore_world_fade,
    RE::NiAVObject* root, const RE::NiSkinPartition::Partition* partition = nullptr,
    std::vector<RE::NiPointer<RE::NiTexture>>* retained_textures = nullptr)
{
    if (buffers == nullptr || vertex_count == 0 || vertex_count > kMaxVertices ||
        triangles == 0 || triangles > kMaxTriangles) return false;
    const auto& runtime = geometry->GetGeometryRuntimeData();
    const auto vertex_desc = buffers->vertexDesc;
    static_assert(sizeof(packet.descriptor) == sizeof(vertex_desc));
    std::memcpy(&packet.descriptor, &vertex_desc, sizeof(packet.descriptor));
    packet.stride = static_cast<std::uint32_t>(packet.descriptor & 0xFU) * 4U;
    if (packet.stride < 8 || packet.stride > 128) return false;
    if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) && dynamic_positions.empty()) return false;
    auto position_bytes = packet.stride;
    for (unsigned attribute = RE::BSGraphics::Vertex::VA_TEXCOORD0; attribute < RE::BSGraphics::Vertex::VA_COUNT; ++attribute) {
        if (vertex_desc.HasFlag(static_cast<RE::BSGraphics::Vertex::Flags>(1U << attribute))) {
            const auto offset = vertex_desc.GetAttributeOffset(static_cast<RE::BSGraphics::Vertex::Attribute>(attribute));
            if (offset > 0) position_bytes = std::min(position_bytes, offset);
        }
    }
    packet.full_precision = vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC) || position_bytes >= 12;
    packet.has_uv = vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
    packet.uv_offset = vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
    if (packet.has_uv && packet.uv_offset + 4U > packet.stride) return false;
    packet.has_normal = vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
    packet.normal_offset = vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL);
    if (packet.has_normal && packet.normal_offset + 4U > packet.stride) return false;
    packet.has_colors = vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_COLORS);
    packet.color_offset = vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR);
    if (packet.has_colors && packet.color_offset + 4U > packet.stride) return false;
    packet.triangle_count = triangles;
    packet.vertices = reinterpret_cast<ID3D11Buffer*>(buffers->vertexBuffer);
    packet.indices = reinterpret_cast<ID3D11Buffer*>(buffers->indexBuffer);
    const auto vertex_bytes = static_cast<std::size_t>(vertex_count) * packet.stride;
    if (packet.vertices == nullptr) {
        if (buffers->rawVertexData == nullptr) return false;
        packet.vertex_bytes.assign(buffers->rawVertexData, buffers->rawVertexData + vertex_bytes);
    } else {
        D3D11_BUFFER_DESC desc{};
        packet.vertices->GetDesc(&desc);
        if (desc.ByteWidth < vertex_bytes || !(desc.BindFlags & D3D11_BIND_VERTEX_BUFFER)) return false;
        packet.volatile_vertices = desc.Usage == D3D11_USAGE_DYNAMIC || (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0;
    }
    const auto index_count = static_cast<std::size_t>(triangles) * 3;
    if (packet.indices == nullptr) {
        if (buffers->rawIndexData == nullptr) return false;
        packet.index_values.assign(buffers->rawIndexData, buffers->rawIndexData + index_count);
    } else {
        D3D11_BUFFER_DESC desc{};
        packet.indices->GetDesc(&desc);
        if (index_count * sizeof(std::uint16_t) > desc.ByteWidth || !(desc.BindFlags & D3D11_BIND_INDEX_BUFFER)) return false;
    }
    const auto identity = Matrix(RE::NiTransform{});
    packet.skinned = vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED);
    if (pose.BindPose() && packet.skinned) {
        packet.world = identity;
    } else {
        packet.world = Matrix(LiveRootLocal(geometry, root, root_inverse));
        if (pose.Active()) DirectX::XMStoreFloat4x4(&packet.world,pose.World(geometry));
    }
    if (!Finite(packet.world)) return false;
    const auto sort_center = root_inverse * geometry->worldBound.center;
    packet.sort_center = std::isfinite(sort_center.x) && std::isfinite(sort_center.y) && std::isfinite(sort_center.z) ?
        DirectX::XMFLOAT3{sort_center.x,sort_center.y,sort_center.z} : DirectX::XMFLOAT3{packet.world._41,packet.world._42,packet.world._43};
    auto* skin = runtime.skinInstance.get();
    if (packet.skinned) {
        packet.skin_offset = vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
        if (skin == nullptr || skin->skinData == nullptr || skin->bones == nullptr) return false;
        const auto bone_count = skin->skinData->GetBoneCount();
        if (bone_count == 0 || bone_count > kMaxBones || skin->skinData->GetBoneDataAddress(0) == nullptr) return false;
        if(pose.TraceCapture()){
            packet.shape_name=geometry->name.c_str()?geometry->name.c_str():"";
            packet.diagnostic_skin_bones.reserve(bone_count);
            for(std::uint32_t bone=0;bone<bone_count;++bone){
                const auto* name=skin->bones[bone]?skin->bones[bone]->name.c_str():nullptr;
                packet.diagnostic_skin_bones.emplace_back(name?name:"");
            }
        }
        if(pose.HasAnimation() || packet.skin_offset + 12U > packet.stride){
            packet.skin_vertices=CaptureSkinVertices(skin,partition,vertex_count);
        }
        if(!packet.skin_vertices && ((pose.HasAnimation() && packet.skin_offset < 8U) || packet.skin_offset + 12U > packet.stride))return false;
        if (pose.BindPose() || pose.MeshBindAnimation()) {
            const auto skin_to_root = FiniteTransform(skin->skinData->rootParentToSkin) &&
                std::abs(skin->skinData->rootParentToSkin.scale) > .0001F
                ? Matrix(skin->skinData->rootParentToSkin.Invert())
                : identity;
            packet.bones.fill(Finite(skin_to_root) ? skin_to_root : identity);
            // The static creature path already establishes the correct mesh
            // bind space. Apply only a bone's captured motion delta on top of
            // it; another mesh's inverse bind must not reshape this mesh.
            if(pose.MeshBindAnimation())for(std::uint32_t bone=0;bone<bone_count;++bone){
                const auto delta=pose.AnimationDeltaForSkinBone(
                    skin->bones[bone],skin->skinData->GetBoneDataSkinToBone(bone),skin->skinData->rootParentToSkin);
                DirectX::XMStoreFloat4x4(&packet.bones[bone],DirectX::XMLoadFloat4x4(&skin_to_root)*delta);
                if(!Finite(packet.bones[bone]))packet.bones[bone]=skin_to_root;
            }
        } else {
            packet.bones.fill(identity);
            // SSE partitions index a shared full-mesh vertex stream. Its packed bone
            // indices address NiSkinInstance::bones (global), not the partition palette.
            for (std::uint32_t global = 0; global < bone_count; ++global) {
                if (skin->bones[global] == nullptr) continue;
                const auto skin_to_bone_t = skin->skinData->GetBoneDataSkinToBone(global);
                if (pose.Active()) {
                    const auto skin_to_bone=Matrix(skin_to_bone_t);
                    DirectX::XMStoreFloat4x4(&packet.bones[global],DirectX::XMLoadFloat4x4(&skin_to_bone)*pose.World(skin->bones[global]));
                } else {
                    packet.bones[global] = Matrix(LiveRootLocal(skin->bones[global], root, root_inverse) * skin_to_bone_t);
                }
                if (!Finite(packet.bones[global])) packet.bones[global] = identity;
            }
        }
    }
    if (!dynamic_positions.empty()) {
        if (dynamic_positions.size() < vertex_count) return false;
        packet.dynamic_positions.assign(dynamic_positions.begin(), dynamic_positions.begin() + vertex_count);
    }
    CaptureMaterial(packet, geometry, ignore_world_fade, retained_textures);
    return true;
}

bool ContainsAsciiCaseInsensitive(std::string_view text, std::string_view pattern) noexcept
{
    if (pattern.empty() || text.size() < pattern.size()) return false;
    for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < pattern.size(); ++j) {
            auto a = text[i + j];
            auto b = pattern[j];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
            if (a != b) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

bool IsEffectOrFlameGeometry(RE::BSGeometry* geometry, RE::NiAVObject* root) noexcept
{
    if (!geometry) return true;
    const auto& runtime = geometry->GetGeometryRuntimeData();
    auto* property = runtime.shaderProperty.get();
    if (!property) return true;
    auto* material = property->GetBaseMaterial();
    if (!material || material->GetType() != RE::BSShaderMaterial::Type::kLighting) return true;
    using PropertyFlag = RE::BSShaderProperty::EShaderPropertyFlag;
    const bool depth_write = property->flags.any(PropertyFlag::kZBufferWrite);
    const auto* alpha = runtime.alphaProperty.get();
    const bool alpha_blended = alpha && alpha->GetAlphaBlending();
    if (alpha_blended && alpha->GetDestBlendMode() == RE::NiAlphaProperty::AlphaFunction::kOne) {
        return true;
    }
    unsigned depth = 0;
    for (RE::NiAVObject* current = geometry; current && current != root && depth < 64; current = current->parent, ++depth) {
        const auto* raw = current->name.c_str();
        const std::string_view name = raw ? raw : "";
        if (name.empty()) continue;
        if (ContainsAsciiCaseInsensitive(name, "flame") ||
            ContainsAsciiCaseInsensitive(name, "torchfire") ||
            ContainsAsciiCaseInsensitive(name, "smoke") ||
            ContainsAsciiCaseInsensitive(name, "glowcone") ||
            ContainsAsciiCaseInsensitive(name, "glow_cone") ||
            ContainsAsciiCaseInsensitive(name, "flare") ||
            ContainsAsciiCaseInsensitive(name, "ember") ||
            ContainsAsciiCaseInsensitive(name, "spark") ||
            ContainsAsciiCaseInsensitive(name, "steam") ||
            ContainsAsciiCaseInsensitive(name, "particle") ||
            ContainsAsciiCaseInsensitive(name, "billboard") ||
            ContainsAsciiCaseInsensitive(name, "addonnode") ||
            ContainsAsciiCaseInsensitive(name, "lightbeam")) {
            return true;
        }
        if ((!depth_write || alpha_blended) &&
            (ContainsAsciiCaseInsensitive(name, "fire") ||
             ContainsAsciiCaseInsensitive(name, "glow") ||
             ContainsAsciiCaseInsensitive(name, "magic") ||
             ContainsAsciiCaseInsensitive(name, "aura") ||
             ContainsAsciiCaseInsensitive(name, "fx") ||
             ContainsAsciiCaseInsensitive(name, "cone"))) {
            return true;
        }
    }
    return false;
}

void CollectGeometry(RE::NiAVObject* object, RE::NiAVObject* root,
    const RE::NiTransform& inverse, SceneSnapshot& scene, PreviewPose& pose,
    bool skip_effects = true, unsigned depth = 0, bool ignore_hidden_head = false,
    std::vector<RE::NiPointer<RE::NiTexture>>* retained_textures = nullptr)
{
    if (object == nullptr || depth > 128 || scene.packets.size() >= kMaxPackets) return;
    if (object != root && object->GetFlags().all(RE::NiAVObject::Flag::kHidden)) {
        if (!ignore_hidden_head) return;
        const auto* raw = object->name.c_str();
        const std::string_view name = raw ? raw : "";
        // In 1st-person camera Skyrim marks the player's 3rd-person container
        // nodes, FaceGen head, and worn headgear kHidden while BSDismemberSkinInstance
        // still tracks partition visibility under helmets. Skip only hidden helper nodes.
        if (ContainsAsciiCaseInsensitive(name, "marker") ||
            ContainsAsciiCaseInsensitive(name, "collision") ||
            ContainsAsciiCaseInsensitive(name, "shadow") ||
            ContainsAsciiCaseInsensitive(name, "camera") ||
            ContainsAsciiCaseInsensitive(name, "blood") ||
            ContainsAsciiCaseInsensitive(name, "scabbard")) {
            return;
        }
    }
    if (auto* shape = object->AsTriShape()) {
        auto* geometry = static_cast<RE::BSGeometry*>(shape);
        if (skip_effects && IsEffectOrFlameGeometry(geometry, root)) {
            ++scene.skipped;
            return;
        }
        const auto shape_data = shape->GetTrishapeRuntimeData();
        auto* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
        auto* partitions = skin != nullptr ? skin->skinPartition.get() : nullptr;
        const auto full_vertex_count = partitions != nullptr && partitions->vertexCount != 0 ?
            partitions->vertexCount : static_cast<std::uint32_t>(shape_data.vertexCount);
        std::vector<DirectX::XMFLOAT4> positions;
        if (auto* dynamic = object->AsDynamicTriShape()) {
            auto& data = dynamic->GetDynamicTrishapeRuntimeData();
            const auto required = static_cast<std::size_t>(full_vertex_count) * sizeof(DirectX::XMFLOAT4);
            if (full_vertex_count == 0 || required > 16U * kMaxVertices) { ++scene.skipped; return; }
            const RE::BSSpinLockGuard guard(data.lock);
            if (data.dynamicData == nullptr || data.dataSize < required) { ++scene.skipped; return; }
            positions.resize(full_vertex_count);
            std::memcpy(positions.data(), data.dynamicData, required);
        }
        if (partitions != nullptr && partitions->numPartitions > 0 && partitions->numPartitions <= kMaxPackets) {
            const auto count = std::min<std::size_t>(partitions->numPartitions, partitions->partitions.size());
            for (std::uint32_t index = 0; index < count && scene.packets.size() < kMaxPackets; ++index) {
                // Hidden dismember partitions remain in the buffers. Drawing
                // them resurrects hair/body regions suppressed by worn armor.
                if (const auto* dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skin)) {
                    const auto& visibility = dismember->GetRuntimeData();
                    if (visibility.partitions && index < static_cast<std::uint32_t>(std::max(0,visibility.numPartitions)) &&
                        !visibility.partitions[index].visible) continue;
                }
                const auto& part = partitions->partitions[index];
                DrawPacket packet;
                auto* buffers = part.buffData;
                // Some single-part FaceGen shapes keep the draw resource on the
                // geometry. Reuse it only when both vertex/index domains match.
                if (buffers == nullptr && count == 1 && part.vertices == full_vertex_count &&
                    part.triangles == shape_data.triangleCount) buffers = geometry->GetGeometryRuntimeData().rendererData;
                if (part.strips == 0 && FillPacket(packet, geometry, buffers, full_vertex_count, part.triangles, inverse, positions, pose, scene.ignore_world_fade, root, &part, retained_textures)) {
                    if(scene.item_preview)packet.shape_name=geometry->name.c_str();
                    scene.packets.push_back(std::move(packet));
                } else ++scene.skipped;
            }
        } else {
            DrawPacket packet;
            if (FillPacket(packet, geometry, geometry->GetGeometryRuntimeData().rendererData,
                    full_vertex_count, shape_data.triangleCount, inverse, positions, pose, scene.ignore_world_fade, root, nullptr, retained_textures)) {
                if(scene.item_preview)packet.shape_name=geometry->name.c_str();
                scene.packets.push_back(std::move(packet));
            } else ++scene.skipped;
        }
    }
    if (auto* node = object->AsNode()) {
        for (const auto& child : node->GetChildren()) CollectGeometry(child.get(), root, inverse, scene, pose, skip_effects, depth + 1, ignore_hidden_head, retained_textures);
    }
}

RE::NiAVObject* SearchSkeletonHeadNode(RE::NiAVObject* object, unsigned depth = 0)
{
    if (!object || depth > 128) return nullptr;
    auto* node = object->AsNode();
    if (!node) return nullptr;
    const auto* raw = node->name.c_str();
    const std::string_view name = raw ? raw : "";
    if (!name.empty() &&
        (ContainsAsciiCaseInsensitive(name, "head") || ContainsAsciiCaseInsensitive(name, "skull")) &&
        !ContainsAsciiCaseInsensitive(name, "magic") && !ContainsAsciiCaseInsensitive(name, "camera") &&
        !ContainsAsciiCaseInsensitive(name, "weapon") && !ContainsAsciiCaseInsensitive(name, "shield") &&
        !ContainsAsciiCaseInsensitive(name, "target") && !ContainsAsciiCaseInsensitive(name, "collar") &&
        !ContainsAsciiCaseInsensitive(name, "neck") && !ContainsAsciiCaseInsensitive(name, "jaw") &&
        !ContainsAsciiCaseInsensitive(name, "ear") && !ContainsAsciiCaseInsensitive(name, "horn") &&
        !ContainsAsciiCaseInsensitive(name, "eye") && !ContainsAsciiCaseInsensitive(name, "brow") &&
        !ContainsAsciiCaseInsensitive(name, "beak") && !ContainsAsciiCaseInsensitive(name, "snout") &&
        !ContainsAsciiCaseInsensitive(name, "nose") && !ContainsAsciiCaseInsensitive(name, "tongue") &&
        !ContainsAsciiCaseInsensitive(name, "mouth") && !ContainsAsciiCaseInsensitive(name, "lip") &&
        !ContainsAsciiCaseInsensitive(name, "teeth") && !ContainsAsciiCaseInsensitive(name, "feather") &&
        !ContainsAsciiCaseInsensitive(name, "comb") && !ContainsAsciiCaseInsensitive(name, "wattle")) {
        return node;
    }
    for (const auto& child : node->GetChildren()) {
        if (auto* found = SearchSkeletonHeadNode(child.get(), depth + 1)) return found;
    }
    return nullptr;
}

RE::NiAVObject* FindPortraitHead(RE::Actor* actor, RE::NiAVObject* root)
{
    for(const auto* name:{"NPC Head [Head]","Head","Bip01 Head","NPC Head","Head1","Canine_Head","Dog_Head","HorseHead","ChickenHead","CowHead","Goat_Head","Dragon_Head"})
        if(auto* object=root->GetObjectByName(RE::BSFixedString(name)))return object;
    if(const auto* race=actor->GetRace();race && race->bodyPartData){
        if(const auto* head=race->bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kHead]){
            if(!head->nodeName.empty())
                if(auto* object=root->GetObjectByName(head->nodeName))return object;
        }
    }
    if(auto* searched=SearchSkeletonHeadNode(root))return searched;
    if(const auto* race=actor->GetRace();race && race->bodyPartData){
        if(const auto* head=race->bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kHead]){
            if(!head->targetName.empty())
                if(auto* object=root->GetObjectByName(head->targetName))return object;
        }
    }
    return nullptr;
}

bool DescendsFrom(RE::NiAVObject* node, RE::NiAVObject* ancestor)
{
    for(unsigned depth=0;node && depth<128;node=node->parent,++depth)if(node==ancestor)return true;
    return false;
}

struct PortraitBounds {
    DirectX::XMFLOAT3 low{FLT_MAX,FLT_MAX,FLT_MAX},high{-FLT_MAX,-FLT_MAX,-FLT_MAX};
    bool valid{};
    void Add(const DirectX::XMFLOAT3& point,float radius){
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||!std::isfinite(radius)||radius<=0)return;
        low.x=std::min(low.x,point.x-radius);low.y=std::min(low.y,point.y-radius);low.z=std::min(low.z,point.z-radius);
        high.x=std::max(high.x,point.x+radius);high.y=std::max(high.y,point.y+radius);high.z=std::max(high.z,point.z+radius);
        valid=true;
    }
};

void CollectPortraitBounds(RE::NiAVObject* object, RE::NiAVObject* head,
    PreviewPose& pose, PortraitBounds& bounds, unsigned depth=0, bool ignore_hidden=false)
{
    if(!object||depth>128||(depth>0 && !ignore_hidden && object->GetFlags().all(RE::NiAVObject::Flag::kHidden)))return;
    if(auto* geometry=object->AsGeometry()){
        auto* skin=geometry->GetGeometryRuntimeData().skinInstance.get();
        if(skin && skin->skinData && skin->bones){
            const auto count=skin->skinData->GetBoneCount();
            if(count<=kMaxBones && count && skin->skinData->GetBoneDataAddress(0)){
                for(std::uint32_t i=0;i<count;++i){
                    if(!DescendsFrom(skin->bones[i],head))continue;
                    const auto& bound=skin->skinData->GetBoneDataBound(i);
                    const auto world=pose.World(skin->bones[i]);
                    DirectX::XMFLOAT3 center;
                    DirectX::XMStoreFloat3(&center,DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(bound.center.x,bound.center.y,bound.center.z,1),world));
                    const auto scale=DirectX::XMVectorGetX(DirectX::XMVector3Length(world.r[0]));
                    bounds.Add(center,bound.radius*scale);
                }
            }
        }else if(DescendsFrom(object,head) && std::isfinite(object->world.scale) && std::abs(object->world.scale)>.0001F){
            // Rigid antlers/horns/accessories can live below a head node too.
            const auto local=object->world.Invert()*object->worldBound.center;
            DirectX::XMFLOAT3 center;
            const auto world=pose.World(object);
            DirectX::XMStoreFloat3(&center,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(local.x,local.y,local.z,1),world));
            const auto scale=DirectX::XMVectorGetX(DirectX::XMVector3Length(world.r[0]));
            bounds.Add(center,object->worldBound.radius/std::abs(object->world.scale)*scale);
        }
    }
    if(auto* node=object->AsNode())for(const auto& child:node->GetChildren())CollectPortraitBounds(child.get(),head,pose,bounds,depth+1,ignore_hidden);
}

// Tight body bounds from skinned bones, plus rigid attachments (weapons, lanterns,
// helmets) that sit on the body. Third-person scene graphs also carry magic/hand
// art, particle and camera-adjacent geometry whose world bounds are far larger than
// the actor; using root->worldBound there shrinks the actor and makes it float.
struct RigidBound { DirectX::XMFLOAT3 center; float radius; };
void CollectActorBodyBounds(RE::NiAVObject* object, PreviewPose& pose, PortraitBounds& skinned,
    std::vector<RigidBound>& rigid, unsigned depth=0, bool ignore_hidden=false)
{
    if(!object||depth>128||(depth>0 && !ignore_hidden && object->GetFlags().all(RE::NiAVObject::Flag::kHidden)))return;
    if(auto* geometry=object->AsGeometry()){
        auto* skin=geometry->GetGeometryRuntimeData().skinInstance.get();
        if(skin && skin->skinData && skin->bones){
            const auto count=skin->skinData->GetBoneCount();
            if(count<=kMaxBones && count && skin->skinData->GetBoneDataAddress(0)){
                for(std::uint32_t i=0;i<count;++i){
                    if(!skin->bones[i])continue;
                    const auto& bound=skin->skinData->GetBoneDataBound(i);
                    const auto world=pose.World(skin->bones[i]);
                    DirectX::XMFLOAT3 center;
                    DirectX::XMStoreFloat3(&center,DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(bound.center.x,bound.center.y,bound.center.z,1),world));
                    const auto scale=DirectX::XMVectorGetX(DirectX::XMVector3Length(world.r[0]));
                    skinned.Add(center,bound.radius*scale);
                }
            }
        }else if(std::isfinite(object->world.scale) && std::abs(object->world.scale)>.0001F){
            const auto local=object->world.Invert()*object->worldBound.center;
            DirectX::XMFLOAT3 center;
            const auto world=pose.World(object);
            DirectX::XMStoreFloat3(&center,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(local.x,local.y,local.z,1),world));
            const auto scale=DirectX::XMVectorGetX(DirectX::XMVector3Length(world.r[0]));
            const float r=object->worldBound.radius/std::abs(object->world.scale)*scale;
            if(std::isfinite(r)&&r>0)rigid.push_back({center,r});
        }
    }
    if(auto* node=object->AsNode())for(const auto& child:node->GetChildren())CollectActorBodyBounds(child.get(),pose,skinned,rigid,depth+1,ignore_hidden);
}

bool ComputeActorBodyBounds(RE::NiAVObject* root, PreviewPose& pose, bool ignore_hidden,
    DirectX::XMFLOAT3& out_center, float& out_radius)
{
    PortraitBounds skinned;
    std::vector<RigidBound> rigid;
    CollectActorBodyBounds(root,pose,skinned,rigid,0,ignore_hidden);
    if(!skinned.valid)return false;
    const DirectX::XMFLOAT3 c{(skinned.low.x+skinned.high.x)*.5F,(skinned.low.y+skinned.high.y)*.5F,(skinned.low.z+skinned.high.z)*.5F};
    const float dx=skinned.high.x-skinned.low.x,dy=skinned.high.y-skinned.low.y,dz=skinned.high.z-skinned.low.z;
    const float body_r=std::sqrt(dx*dx+dy*dy+dz*dz)*.5F;
    if(!std::isfinite(body_r)||body_r<=1.0F)return false;
    PortraitBounds all=skinned;
    for(const auto& r:rigid){
        const float ox=r.center.x-c.x,oy=r.center.y-c.y,oz=r.center.z-c.z;
        if(std::sqrt(ox*ox+oy*oy+oz*oz)<=body_r*1.15F && r.radius<=body_r*.7F)all.Add(r.center,r.radius);
    }
    out_center={(all.low.x+all.high.x)*.5F,(all.low.y+all.high.y)*.5F,(all.low.z+all.high.z)*.5F};
    const float ax=all.high.x-all.low.x,ay=all.high.y-all.low.y,az=all.high.z-all.low.z;
    out_radius=std::sqrt(ax*ax+ay*ay+az*az)*.5F;
    return std::isfinite(out_radius)&&out_radius>1.0F;
}

void UpdatePrivateItem(RE::NiAVObject* object,const RE::NiTransform& parent,unsigned depth=0)
{
    if(!object||depth>128)return;
    // Update this clone's transforms/bounds without advancing controllers or
    // registering collisions/lights. It never enters the game world.
    object->world=parent*object->local;
    if(auto* node=object->AsNode())for(const auto& child:node->GetChildren())UpdatePrivateItem(child.get(),object->world,depth+1);
    object->UpdateWorldBound();
}

struct ItemSourceAssets {
    RE::NiPointer<RE::NiNode> model;
    RE::NiPointer<RE::NiAVObject> clone;
    std::vector<RE::NiPointer<RE::NiTexture>> alternate_textures;
};

std::shared_ptr<const ItemSourceAssets> LeaseItemSources(
    const RE::NiPointer<RE::NiNode>& model,const RE::NiPointer<RE::NiAVObject>& clone,
    std::vector<RE::NiPointer<RE::NiTexture>> textures={})
{
    return {new ItemSourceAssets{model,clone,std::move(textures)},[](const ItemSourceAssets* assets) noexcept {
        // A retained render/export snapshot can die on the render thread. Do
        // not run Skyrim's NiObject/material destruction on that thread.
        try {
            if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([assets] {
                if(!RE::BSGraphics::Renderer::GetSingleton())return; // renderer teardown: do not call game destructors
                delete assets;
            });
            // With no task interface the engine is shutting down. Leave this
            // lease to process teardown instead of calling unloaded game code.
        }catch(...) { /* also avoid unsafe destruction if shutdown cannot queue */ }
    }};
}

std::vector<RE::NiPointer<RE::NiTexture>> CaptureItemTextureSwaps(
    SceneSnapshot& scene,const RE::TESModelTextureSwap* model)
{
    std::vector<RE::NiPointer<RE::NiTexture>> textures;
    if(!model||!model->alternateTextures||model->numAlternateTextures>1024)return textures;
    using Slots=RE::BSTextureSet::Textures;
    constexpr std::array mapping{
        std::pair{Slots::kDiffuse,Diffuse},std::pair{Slots::kNormal,Normal},
        std::pair{Slots::kSpecular,SpecularMap},std::pair{Slots::kEnvironment,Environment},
        std::pair{Slots::kEnvironmentMask,EnvironmentMask},std::pair{Slots::kGlowMap,Glow}};
    for(std::uint32_t i=0;i<model->numAlternateTextures;++i){
        const auto& swap=model->alternateTextures[i];
        if(!swap.textureSet||swap.name3D.empty())continue;
        if(std::none_of(scene.packets.begin(),scene.packets.end(),[&](const auto& packet){return packet.shape_name==swap.name3D.c_str();}))continue;
        for(const auto& [source,destination]:mapping){
            const auto* path=swap.textureSet->GetTexturePath(source);
            if(!path||!path[0])continue;
            RE::NiPointer<RE::NiTexture> texture;
            RE::BSShaderManager::GetTexture(path,true,texture,false);
            auto view=RetainTexture(netimmerse_cast<RE::NiSourceTexture*>(texture.get()));
            if(!view){SKSE::log::warn("SkyUltra alternate texture could not load: {}",path);continue;}
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};view->GetDesc(&desc);
            if(desc.ViewDimension!=(destination==Environment?D3D11_SRV_DIMENSION_TEXTURECUBE:D3D11_SRV_DIMENSION_TEXTURE2D))continue;
            // Override only our numeric/GPU packet. The cached NIF, shader
            // property and material may be shared with live world objects.
            for(auto& packet:scene.packets)if(packet.shape_name==swap.name3D.c_str()){
                if(destination==Glow && !(packet.material.features[0]&GlowLighting))continue;
                packet.textures[destination]=view;
                packet.material.features[1]|=1U<<destination;
                packet.material.features[2]&=~(1U<<destination);
                if(SRGBView(desc.Format))packet.material.features[2]|=1U<<destination;
                if(destination==Normal){
                    packet.material.features[0]&=~ModelSpaceNormal;
                    if(swap.textureSet->flags.any(RE::BGSTextureSet::Flag::kHasModelSpaceNormalMap))packet.material.features[0]|=ModelSpaceNormal;
                }
            }
            textures.push_back(std::move(texture));
        }
    }
    return textures;
}

std::shared_ptr<const SceneSnapshot> CaptureItemScene(const ActorPreviewRequest& request,ActorPreviewStatus& status)
{
    status.item_preview=true;status.item_form_id=request.item_form_id;
    auto* form=RE::TESForm::LookupByID(request.item_form_id);
    if(!form||!form->IsInventoryObject()){status.message="Choose a valid inventory item form.";return {};}
    RE::TESModel* model{};
    if(auto* armor=form->As<RE::TESObjectARMO>()){
        for(auto& ground:armor->worldModels)if(const auto* path=ground.GetModel();path&&path[0]){model=&ground;break;}
    }else model=skyrim_cast<RE::TESModel*>(form);
    const auto* path=model?model->GetModel():nullptr;
    if(!path||!path[0]){status.message="This item has no world model to preview.";return {};}
    auto* renderer=RE::BSGraphics::Renderer::GetSingleton();
    if(!renderer){status.message="The game renderer is unavailable.";return {};}
    RE::NiPointer<RE::NiNode> cached;
    const RE::BSModelDB::DBTraits::ArgsType args{};
    const auto loaded=RE::BSModelDB::Demand(path,cached,args);
    if(loaded!=RE::BSResource::ErrorCode::kNone||!cached){
        status.message="The item's model asset could not be loaded. Try Recapture.";return {};
    }
    RE::NiPointer<RE::NiObject> clone(cached->Clone());
    RE::NiPointer<RE::NiAVObject> root(netimmerse_cast<RE::NiAVObject*>(clone.get()));
    if(!root||root.get()==cached.get()){status.message="Could not create a private item model.";return {};}
    UpdatePrivateItem(root.get(),RE::NiTransform{});
    const auto& bound=root->worldBound;
    if(!std::isfinite(bound.center.x)||!std::isfinite(bound.center.y)||!std::isfinite(bound.center.z)||
        !std::isfinite(bound.radius)||bound.radius<=0){status.message="The item model has no usable bounds.";return {};}
    auto scene=std::make_shared<SceneSnapshot>();scene->reference_id=request.item_form_id;scene->item_preview=true;
    scene->center={bound.center.x,bound.center.y,bound.center.z};scene->head=scene->center;
    scene->radius=std::clamp(bound.radius,.01F,4096.0F);scene->portrait_radius=scene->radius;
    PreviewPose pose(root.get(),RE::NiTransform{},ActorPreviewPose::Current);
    std::vector<RE::NiPointer<RE::NiTexture>> retained_textures;
    {
        struct RendererGuard {RE::BSGraphics::Renderer* renderer;~RendererGuard(){renderer->Unlock();}} guard{renderer};
        renderer->Lock();
        CollectGeometry(root.get(),root.get(),RE::NiTransform{},*scene,pose,true,0,false,&retained_textures);
        if(scene->packets.empty()){
            scene->skipped=0;
            retained_textures.clear();
            CollectGeometry(root.get(),root.get(),RE::NiTransform{},*scene,pose,false,0,false,&retained_textures);
        }
    }
    if(!scene->packets.empty() && scene->skipped>0){
        PortraitBounds item_bounds;
        for(const auto& pkt:scene->packets)item_bounds.Add(pkt.sort_center,std::max(.5F,bound.radius*.35F));
        if(item_bounds.valid){
            scene->center={(item_bounds.low.x+item_bounds.high.x)*.5F,(item_bounds.low.y+item_bounds.high.y)*.5F,(item_bounds.low.z+item_bounds.high.z)*.5F};
            scene->head=scene->center;
        }
    }
    status.available=!scene->packets.empty();status.geometry_count=scene->packets.size();status.skipped_geometry_count=scene->skipped;
    status.message=status.available?"World model preview.":"The item model contains no supported triangle geometry.";
    if(status.available){
        auto swap_textures=CaptureItemTextureSwaps(*scene,model->GetAsModelTextureSwap());
        for(auto& tex:swap_textures)retained_textures.push_back(std::move(tex));
        scene->item_sources=LeaseItemSources(cached,root,std::move(retained_textures));
    }
    return status.available?scene:nullptr;
}

std::shared_ptr<const SceneSnapshot> CaptureUnloadedNpcScene(const ActorPreviewRequest& request, ActorPreviewStatus& status)
{
    if (!request.base_form_id) return {};
    auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(request.base_form_id);
    if (!npc) return {};
    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer) return {};
    std::string candidate_path;
    if (const auto* file = npc->GetFile(0); file && file->GetFilename().data() && file->GetFilename()[0]) {
        const std::uint32_t local_id = file->IsLight() ? (npc->GetFormID() & 0x00000FFFU) : (npc->GetFormID() & 0x00FFFFFFU);
        char hex_buf[16]{};
        std::snprintf(hex_buf, sizeof(hex_buf), "%08X", local_id);
        candidate_path = std::string("Actors\\Character\\FaceGenData\\FaceGeom\\") + std::string(file->GetFilename()) + "\\" + hex_buf + ".nif";
    }
    RE::NiPointer<RE::NiNode> cached;
    const RE::BSModelDB::DBTraits::ArgsType args{};
    bool loaded_ok = false;
    if (!candidate_path.empty()) {
        loaded_ok = RE::BSModelDB::Demand(candidate_path.c_str(), cached, args) == RE::BSResource::ErrorCode::kNone && cached != nullptr;
    }
    if (!loaded_ok && npc->defaultOutfit) {
        for (const auto* item : npc->defaultOutfit->outfitItems) {
            if (auto* armor = item ? item->As<RE::TESObjectARMO>() : nullptr) {
                for (auto& ground : armor->worldModels) {
                    if (const auto* path = ground.GetModel(); path && path[0]) {
                        if (RE::BSModelDB::Demand(path, cached, args) == RE::BSResource::ErrorCode::kNone && cached != nullptr) {
                            loaded_ok = true;
                            break;
                        }
                    }
                }
                if (loaded_ok) break;
            }
        }
    }
    if (!loaded_ok || !cached) return {};
    RE::NiPointer<RE::NiObject> clone(cached->Clone());
    RE::NiPointer<RE::NiAVObject> root(netimmerse_cast<RE::NiAVObject*>(clone.get()));
    if (!root || root.get() == cached.get()) return {};
    UpdatePrivateItem(root.get(), RE::NiTransform{});
    const auto& bound = root->worldBound;
    if (!std::isfinite(bound.center.x) || !std::isfinite(bound.center.y) || !std::isfinite(bound.center.z) ||
        !std::isfinite(bound.radius) || bound.radius <= 0) return {};
    auto scene = std::make_shared<SceneSnapshot>();
    scene->reference_id = request.reference_id ? request.reference_id : request.base_form_id;
    scene->ignore_world_fade = true;
    scene->center = { bound.center.x, bound.center.y, bound.center.z };
    scene->head = scene->center;
    scene->radius = std::clamp(bound.radius, 4.0F, 4096.0F);
    scene->portrait_radius = scene->radius * 0.72F;
    PreviewPose pose(root.get(), RE::NiTransform{}, ActorPreviewPose::Current);
    std::vector<RE::NiPointer<RE::NiTexture>> retained_textures;
    {
        struct RendererGuard { RE::BSGraphics::Renderer* renderer; ~RendererGuard() { renderer->Unlock(); } } guard{ renderer };
        renderer->Lock();
        CollectGeometry(root.get(), root.get(), RE::NiTransform{}, *scene, pose, true, 0, true, &retained_textures);
        if (scene->packets.empty()) {
            scene->skipped = 0;
            retained_textures.clear();
            CollectGeometry(root.get(), root.get(), RE::NiTransform{}, *scene, pose, false, 0, true, &retained_textures);
        }
    }
    if (scene->packets.empty()) return {};
    scene->item_sources = LeaseItemSources(cached, root, std::move(retained_textures));
    status.available = true;
    status.reference_id = scene->reference_id;
    status.geometry_count = scene->packets.size();
    status.skipped_geometry_count = scene->skipped;
    status.poses_available = false;
    status.message = "Assembled private FaceGen/outfit preview from NPC record assets (unloaded actor).";
    return scene;
}

std::shared_ptr<const SceneSnapshot> CaptureScene(const ActorPreviewRequest& request, ActorPreviewStatus& status,
    const std::shared_ptr<const detail::CapturedActorAnimations>& animations={})
{
    if(request.item_preview)return CaptureItemScene(request,status);
    auto* actor = FindActor(request);
    if (actor == nullptr) {
        if (auto unloaded = CaptureUnloadedNpcScene(request, status)) return unloaded;
        status.message = "No loaded actor instance with 3D is available for this NPC record.";
        return {};
    }
    const bool is_player = actor->IsPlayerRef();
    RE::NiPointer<RE::NiAVObject> root(actor->Get3D(false));
    if (root == nullptr && is_player) root.reset(actor->Get3D());
    if (root == nullptr || !std::isfinite(root->world.scale) || std::abs(root->world.scale) < 0.0001F) {
        status.message = "The actor has no usable third-person 3D.";
        return {};
    }
    const auto inverse = root->world.Invert();
    const detail::CapturedAnimation* animation{};
    if(!request.animation_id.empty()){
        if(animations && animations->reference_id==actor->GetFormID()){
            for(const auto& candidate:animations->clips)if(candidate.info.id==request.animation_id){animation=&candidate;break;}
        }
        if(!animation || request.animation_frame>=animation->frames.size()){
            status.message="This animation capture is no longer available for the selected actor. Capture animations again.";
            return {};
        }
    }
    auto scene = std::make_shared<SceneSnapshot>();
    scene->ignore_world_fade = request.ignore_world_fade || is_player;
    scene->reference_id = actor->GetFormID();
    const auto center = inverse * root->worldBound.center;
    const auto radius = root->worldBound.radius / std::abs(root->world.scale);
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z) || !std::isfinite(radius)) {
        status.message = "The actor has invalid scene bounds.";
        return {};
    }
    scene->center = { center.x, center.y, center.z };
    scene->radius = std::clamp(radius, 16.0F, 4096.0F);
    // A skeleton without a reliable head falls back to a useful whole-model
    // portrait, rather than a guessed humanoid height that clips animal faces.
    scene->head=scene->center;scene->portrait_radius=scene->radius;
    const bool is_humanoid_npc = is_player || actor->HasKeywordString("ActorTypeNPC");
    const bool trace_capture=animation && animation->mesh_bind_basis && request.animation_frame==0 &&
        !animation->skin_diagnostic_written->load();
    PreviewPose pose(root.get(),inverse,request.pose,animation,request.animation_frame,true,is_humanoid_npc,trace_capture);
    scene->bind_pose=pose.BindPose();
    if(animation || pose.BindPose()){
        PortraitBounds bounds;
        CollectPortraitBounds(root.get(),root.get(),pose,bounds,0,is_player);
        if(bounds.valid){
            scene->center={(bounds.low.x+bounds.high.x)*.5F,(bounds.low.y+bounds.high.y)*.5F,(bounds.low.z+bounds.high.z)*.5F};
            const auto x=bounds.high.x-bounds.low.x,y=bounds.high.y-bounds.low.y,z=bounds.high.z-bounds.low.z;
            scene->radius=std::clamp(std::sqrt(x*x+y*y+z*z)*.5F,16.0F,4096.0F);
            scene->head=scene->center;scene->portrait_radius=scene->radius;
        }
    }else{
        DirectX::XMFLOAT3 body_center{};
        float body_radius{};
        if(ComputeActorBodyBounds(root.get(),pose,is_player,body_center,body_radius)){
            scene->center=body_center;
            scene->radius=std::clamp(body_radius,16.0F,4096.0F);
            scene->head=scene->center;scene->portrait_radius=scene->radius;
        }
    }
    status.poses_available=pose.Supported();
    if(!status.poses_available){
        // In actor root-local space, +Y is always forward. Give non-humanoid portraits a clean 3/4 studio angle.
        scene->facing_yaw=0.0F;
        scene->portrait_yaw=0.22F;
        scene->portrait_radius=std::clamp(scene->radius*0.72F, 18.0F, 4096.0F);
    }
    if(auto* head=FindPortraitHead(actor,root.get())){
        DirectX::XMFLOAT3 head_pos{};
        DirectX::XMStoreFloat3(&head_pos,pose.World(head).r[3]);
        if (status.poses_available) {
            // Bust portrait framing: include head, shoulders, and upper chest.
            const float bust_drop_z = std::clamp(scene->radius * 0.055F, 3.0F, 4.8F);
            scene->head = { head_pos.x, head_pos.y, head_pos.z - bust_drop_z };
            const float min_radius = std::clamp(scene->radius * 0.31F, 19.5F, 25.0F);
            const float max_radius = std::clamp(scene->radius * 0.40F, 24.0F, 31.0F);
            scene->portrait_radius = std::clamp(scene->radius * 0.34F, min_radius, max_radius);
            PortraitBounds bounds;
            CollectPortraitBounds(root.get(), head, pose, bounds, 0, is_player);
            if (bounds.valid) {
                const float bx = (bounds.low.x + bounds.high.x) * 0.5F;
                const float by = (bounds.low.y + bounds.high.y) * 0.5F;
                scene->head.x = std::clamp(bx, head_pos.x - 3.0F, head_pos.x + 3.0F);
                scene->head.y = std::clamp(by, head_pos.y - 3.0F, head_pos.y + 3.0F);
                const auto span = std::max({ bounds.high.x - bounds.low.x, bounds.high.y - bounds.low.y, bounds.high.z - bounds.low.z });
                scene->portrait_radius = std::clamp(span * 0.78F, min_radius, max_radius);
            }
        } else {
            // Non-humanoids (creatures, animals, giants, mudcrabs): blend between body center and head
            // with generous radius so the creature's head and upper body silhouette are clearly framed.
            const float hx = head_pos.x - scene->center.x;
            const float hy = head_pos.y - scene->center.y;
            const float hz = head_pos.z - scene->center.z;
            const float head_dist = std::sqrt(hx * hx + hy * hy + hz * hz);
            if (std::isfinite(head_dist) && head_dist <= scene->radius * 1.5F) {
                scene->head = {
                    scene->center.x * 0.45F + head_pos.x * 0.55F,
                    scene->center.y * 0.45F + head_pos.y * 0.55F,
                    scene->center.z * 0.38F + head_pos.z * 0.62F
                };
            }
            scene->portrait_radius = std::clamp(scene->radius * 0.72F, 18.0F, 4096.0F);
        }
    }
    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer) {
        status.message = "The game renderer is unavailable.";
        return {};
    }
    std::vector<RE::NiPointer<RE::NiTexture>> retained_textures;
    {
        struct RendererGuard { RE::BSGraphics::Renderer* renderer; ~RendererGuard() { renderer->Unlock(); } } guard{ renderer };
        renderer->Lock();
        CollectGeometry(root.get(), root.get(), inverse, *scene, pose, true, 0, is_player, &retained_textures);
        if(scene->packets.empty()){
            scene->skipped=0;
            retained_textures.clear();
            CollectGeometry(root.get(), root.get(), inverse, *scene, pose, false, 0, true, &retained_textures);
        }
    }
    status.available = !scene->packets.empty();
    if (status.available) {
        scene->actor_sources = LeaseItemSources({}, root, std::move(retained_textures));
    }
    status.reference_id = scene->reference_id;
    status.geometry_count = scene->packets.size();
    status.skipped_geometry_count = scene->skipped;
    if(animation && request.animation_frame==0){
        const auto skins=std::count_if(scene->packets.begin(),scene->packets.end(),[](const auto& packet){return packet.skinned;});
        const auto explicit_skins=std::count_if(scene->packets.begin(),scene->packets.end(),[](const auto& packet){return packet.skin_vertices!=nullptr;});
        SKSE::log::info("SkyUltra animation frame actor {:08X}: {} explicit skin streams / {} skinned packets, mesh-bind deltas={}",
            scene->reference_id,explicit_skins,skins,animation->mesh_bind_basis);
    }
    if(trace_capture && !scene->packets.empty() && !animation->skin_diagnostic_written->exchange(true)){
      // Bound one-shot diagnostics even for a creature with many modded meshes.
      std::size_t diagnostic_bones{};
      for(std::size_t index=0;index<std::min<std::size_t>(scene->packets.size(),8);++index){
        const auto& packet=scene->packets[index];
        D3D11_BUFFER_DESC buffer{};if(packet.vertices)packet.vertices->GetDesc(&buffer);
        SKSE::log::info("SkyUltra creature skin diagnostic v1 actor {:08X}, clip='{}', packet={} '{}': descriptor={:016X}, stride={}, precision={}, skinOffset={}, vertexBytes={}, usage={}, flags={:X}, dynamicPositions={}, explicitWeights={}",
            scene->reference_id,animation->info.name,index,packet.shape_name,packet.descriptor,packet.stride,
            packet.full_precision?"float":"half",packet.skin_offset,buffer.ByteWidth?buffer.ByteWidth:packet.vertex_bytes.size(),
            static_cast<unsigned>(buffer.Usage),buffer.BindFlags,packet.dynamic_positions.size(),packet.skin_vertices?packet.skin_vertices->size():0);
        std::array<float,kMaxBones> influence{};
        if(packet.skin_vertices)for(const auto& vertex:*packet.skin_vertices)
            for(unsigned slot=0;slot<4;++slot)influence[vertex.indices[slot]]+=vertex.weights[slot];
        for(std::size_t bone=0;bone<packet.diagnostic_skin_bones.size() && diagnostic_bones<512;++bone,++diagnostic_bones){
            std::ostringstream matrix;matrix<<std::setprecision(8);
            for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
                if(row||column)matrix<<',';
                matrix<<packet.bones[bone].m[row][column];
            }
            SKSE::log::info("SkyUltra creature skin packet={} bone={} '{}': influence={}, matrix=[{}]",
                index,bone,packet.diagnostic_skin_bones[bone],influence[bone],matrix.str());
        }
      }
    }
    if(trace_capture)for(auto& packet:scene->packets)packet.diagnostic_skin_bones.clear();
    status.message = status.available ?
        "Loaded actor geometry and materials, including hair/face tint and normal maps, under preview lighting. World post-processing is not replayed." :
        "The actor contains no supported triangle geometry with usable vertex/index resources.";
    if(status.available && request.pose!=ActorPreviewPose::Current && !status.poses_available)
        status.message="This skeleton does not support humanoid pose presets; showing its captured current pose.";
    if(status.available && animation)status.message="Animation frame from "+animation->info.name+". Head, outfit and rigid attachments use the same sampled skeleton.";
    return status.available ? scene : nullptr;
}

struct WorkItem {
    ViewId owner{};
    ActorPreviewRequest request;
    std::shared_ptr<const SceneSnapshot> scene;
    std::shared_ptr<RenderedFrame> frame;
    std::uint64_t revision{};
};

struct PngExport {
    WorkItem work;
    ActorPreviewSaveCallback callback;
    ComPtr<ID3D11Texture2D> staging;
    std::filesystem::path cache_path;
    std::filesystem::path secondary_cache_path;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    std::atomic_bool canceled{};
    bool thumbnail{};
};
std::vector<std::shared_ptr<PngExport>> g_exports; // protected by g_mutex
std::vector<std::future<void>> g_export_workers; // protected by g_mutex

std::string EncodePngBytesToDataUrl(const unsigned char* encoded, std::size_t size)
{
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result = "data:image/png;base64,";
    result.reserve(result.size() + ((size + 2U) / 3U) * 4U);
    for (std::size_t i = 0; i < size; i += 3) {
        const auto value = (static_cast<unsigned>(encoded[i]) << 16) |
            (i + 1 < size ? static_cast<unsigned>(encoded[i + 1]) << 8 : 0) |
            (i + 2 < size ? encoded[i + 2] : 0);
        result += alphabet[(value >> 18) & 63];
        result += alphabet[(value >> 12) & 63];
        result += i + 1 < size ? alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < size ? alphabet[value & 63] : '=';
    }
    return result;
}

std::string ReadCachedThumbnailDataUrl(const std::filesystem::path& path)
{
    if (path.empty()) return {};
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return {};
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec || file_size < 64 || file_size > 45000) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::vector<unsigned char> encoded(static_cast<std::size_t>(file_size));
    in.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    if (!in || static_cast<std::size_t>(in.gcount()) != encoded.size()) return {};
    static constexpr unsigned char kPngSig[8]{ 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (std::memcmp(encoded.data(), kPngSig, sizeof(kPngSig)) != 0) return {};
    return EncodePngBytesToDataUrl(encoded.data(), encoded.size());
}

void WriteBytesToCacheFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    if (path.empty() || bytes.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.flush();
}

std::string WriteActorPng(std::vector<std::uint8_t> pixels, SizeU size, const std::filesystem::path& path,
    bool thumbnail_cache = false, const std::filesystem::path& secondary_cache_path = {})
{
    struct Apartment {
        HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        ~Apartment(){if(SUCCEEDED(result))CoUninitialize();}
    } apartment;
    if(FAILED(apartment.result))throw std::runtime_error("Could not initialize the PNG encoder.");
    const auto check=[](HRESULT hr){if(FAILED(hr))throw std::runtime_error("PNG encoding failed (HRESULT "+std::to_string(static_cast<std::uint32_t>(hr))+").");};
    // The UI render target is premultiplied BGRA; PNG stores straight alpha.
    for(std::size_t p=0;p<pixels.size();p+=4){
        const auto alpha=pixels[p+3];
        for(unsigned c=0;c<3;++c)pixels[p+c]=alpha?static_cast<std::uint8_t>(std::min(255U,(pixels[p+c]*255U+alpha/2U)/alpha)):0;
    }
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IStream> memory;
    if(path.empty() || thumbnail_cache)check(CreateStreamOnHGlobal(nullptr,TRUE,&memory));
    else {check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));}
    check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
    check(encoder->Initialize(memory?memory.Get():stream.Get(),WICBitmapEncoderNoCache));
    check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));
    check(frame->SetSize(size.width,size.height));
    auto format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format));
    if(!IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))throw std::runtime_error("The PNG encoder cannot preserve transparency.");
    check(frame->WritePixels(size.height,size.width*4,static_cast<UINT>(pixels.size()),pixels.data()));
    check(frame->Commit());check(encoder->Commit());
    if(!memory)return {};
    STATSTG stat{};check(memory->Stat(&stat,STATFLAG_NONAME));
    // Thumbnail replies share the bridge's 64 KiB response budget.
    if(stat.cbSize.QuadPart>45000)throw std::runtime_error("The item thumbnail exceeds the response budget.");
    check(memory->Seek({},STREAM_SEEK_SET,nullptr));
    std::vector<unsigned char> encoded(static_cast<std::size_t>(stat.cbSize.QuadPart));
    ULONG read{};check(memory->Read(encoded.data(),static_cast<ULONG>(encoded.size()),&read));
    if(read!=encoded.size())throw std::runtime_error("Could not read the encoded item thumbnail.");
    if(thumbnail_cache){
        WriteBytesToCacheFile(path, encoded);
        WriteBytesToCacheFile(secondary_cache_path, encoded);
    }
    return EncodePngBytesToDataUrl(encoded.data(), encoded.size());
}

std::filesystem::path ThumbnailCacheDirectory(bool item)
{
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) return {};
    executable.resize(length);
    // Use the game's virtual Data path inside the game process so MO2/USVFS
    // writes cached icons and portraits into Overwrite/SkyUltra/Cache/... and
    // reads them back on subsequent launches.
    return std::filesystem::path(executable).parent_path() / L"Data" / L"SkyUltra" / L"Cache" / (item ? L"Items" : L"Portraits");
}

std::string SanitizeCacheToken(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (const unsigned char ch : raw) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_') {
            out.push_back(static_cast<char>(ch));
        } else if (ch >= 'A' && ch <= 'Z') {
            out.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else if (ch == ' ') {
            out.push_back('_');
        }
    }
    while (!out.empty() && (out.front() == '.' || out.front() == '_')) out.erase(out.begin());
    while (!out.empty() && (out.back() == '.' || out.back() == '_')) out.pop_back();
    if (out.size() > 64) out.resize(64);
    return out;
}

std::filesystem::path ItemThumbnailCachePath(std::uint32_t form_id)
{
    const auto dir = ThumbnailCacheDirectory(true);
    if (dir.empty() || form_id == 0) return {};
    char hex_buf[32]{};
    if (auto* form = RE::TESForm::LookupByID(form_id)) {
        if (const auto* file = form->GetFile(0); file && file->GetFilename().data() && file->GetFilename()[0]) {
            const auto plugin = SanitizeCacheToken(file->GetFilename());
            if (!plugin.empty()) {
                const std::uint32_t local_id = file->IsLight() ? (form_id & 0x00000FFFU) : (form_id & 0x00FFFFFFU);
                std::snprintf(hex_buf, sizeof(hex_buf), "%08X", local_id);
                return dir / ("item-" + plugin + "-" + hex_buf + ".png");
            }
        }
        RE::TESModel* model{};
        if (auto* armor = form->As<RE::TESObjectARMO>()) {
            for (auto& ground : armor->worldModels) {
                if (const auto* path = ground.GetModel(); path && path[0]) { model = &ground; break; }
            }
        } else {
            model = skyrim_cast<RE::TESModel*>(form);
        }
        if (const auto* path = model ? model->GetModel() : nullptr; path && path[0]) {
            std::uint64_t hash = 14695981039346656037ULL;
            for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p) {
                const unsigned char c = (*p >= 'A' && *p <= 'Z') ? static_cast<unsigned char>(*p - 'A' + 'a') : (*p == '/' ? '\\' : *p);
                hash ^= c;
                hash *= 1099511628211ULL;
            }
            std::snprintf(hex_buf, sizeof(hex_buf), "%016llX", static_cast<unsigned long long>(hash));
            return dir / (std::string("item-model-") + hex_buf + ".png");
        }
    }
    std::snprintf(hex_buf, sizeof(hex_buf), "%08X", form_id);
    return dir / (std::string("item-form-") + hex_buf + ".png");
}

struct ActorPortraitCachePaths {
    std::filesystem::path primary;
    std::filesystem::path fallback;
    std::string key;
};

ActorPortraitCachePaths ResolveActorPortraitCachePaths(std::uint32_t reference_id, std::uint32_t base_form_id)
{
    ActorPortraitCachePaths result;
    const auto dir = ThumbnailCacheDirectory(false);
    if (dir.empty()) return result;
    const bool is_player = (reference_id == 0x14U) || (reference_id == 0 && base_form_id == 0x07U);
    if (is_player) {
        result.fallback = dir / "portrait-bust-player.png";
        result.primary = result.fallback;
        result.key = "portrait-bust-player";
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (const auto* name = player->GetName(); name && name[0]) {
                const auto token = SanitizeCacheToken(name);
                if (!token.empty()) {
                    result.primary = dir / ("portrait-bust-player-" + token + ".png");
                    result.key = "portrait-bust-player-" + token;
                }
            }
        }
        return result;
    }
    RE::Actor* actor = nullptr;
    RE::TESNPC* npc = nullptr;
    if (reference_id != 0) {
        actor = RE::TESForm::LookupByID<RE::Actor>(reference_id);
        if (actor != nullptr) {
            npc = actor->GetActorBase();
        }
    }
    if (npc == nullptr && base_form_id != 0) {
        npc = RE::TESForm::LookupByID<RE::TESNPC>(base_form_id);
    }
    std::string identity_tag;
    if (const auto* race = actor ? actor->GetRace() : (npc ? npc->GetRace() : nullptr)) {
        const char* race_name = race->GetFormEditorID();
        if (!race_name || !race_name[0]) race_name = race->GetName();
        if (race_name && race_name[0]) identity_tag = SanitizeCacheToken(race_name);
    }
    if (const char* disp_name = actor ? actor->GetName() : (npc ? npc->GetName() : nullptr); disp_name && disp_name[0]) {
        const auto name_token = SanitizeCacheToken(disp_name);
        if (!name_token.empty() && name_token != identity_tag) {
            if (!identity_tag.empty()) identity_tag += "-";
            identity_tag += name_token;
        }
    }
    char hex_buf[48]{};
    if (npc != nullptr) {
        const RE::TESNPC* source_npc = npc;
        if (source_npc->GetFile(0) == nullptr) {
            if (const auto* root_face = source_npc->GetRootFaceNPC(); root_face && root_face->GetFile(0)) {
                source_npc = root_face;
            }
        }
        const auto npc_id = source_npc->GetFormID();
        if (const auto* file = source_npc->GetFile(0); file && file->GetFilename().data() && file->GetFilename()[0] && (npc_id >> 24U) != 0xFFU) {
            const auto plugin = SanitizeCacheToken(file->GetFilename());
            if (!plugin.empty()) {
                const std::uint32_t local_id = file->IsLight() ? (npc_id & 0x00000FFFU) : (npc_id & 0x00FFFFFFU);
                std::snprintf(hex_buf, sizeof(hex_buf), "%08X", local_id);
                result.key = "portrait-v5-" + plugin + "-" + (identity_tag.empty() ? std::string() : identity_tag + "-") + hex_buf;
                result.primary = dir / (result.key + ".png");
                return result;
            }
        }
        if (reference_id != 0) {
            std::snprintf(hex_buf, sizeof(hex_buf), "%08X-%08X", reference_id, npc->GetFormID());
        } else {
            std::snprintf(hex_buf, sizeof(hex_buf), "%08X", npc->GetFormID());
        }
        result.key = std::string("portrait-v5-npc-") + (identity_tag.empty() ? std::string() : identity_tag + "-") + hex_buf;
        result.primary = dir / (result.key + ".png");
        return result;
    }
    std::snprintf(hex_buf, sizeof(hex_buf), "%08X-%08X", reference_id, base_form_id);
    result.key = std::string("portrait-v5-ref-") + (identity_tag.empty() ? std::string() : identity_tag + "-") + hex_buf;
    result.primary = dir / (result.key + ".png");
    return result;
}

std::filesystem::path ActorExportDirectory(bool item)
{
    std::wstring executable(32768,L'\0');
    const auto length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!length||length>=executable.size())throw std::runtime_error("Could not locate Skyrim's Data directory.");
    executable.resize(length);
    // Use the game's virtual Data path from inside the game process. Active
    // MO2/USVFS redirects NEW files to Overwrite (or its configured output mod).
    // Vortex and unmanaged launches write to the physical Data directory.
    return std::filesystem::path(executable).parent_path()/L"Data"/L"SkyUltra"/(item?L"Items":L"Actors");
}

std::filesystem::path WrittenActorPngPath(const std::filesystem::path& virtual_path)
{
    const auto handle=CreateFileW(virtual_path.c_str(),FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return virtual_path;
    std::wstring path(32768,L'\0');
    const auto length=GetFinalPathNameByHandleW(handle,path.data(),static_cast<DWORD>(path.size()),FILE_NAME_NORMALIZED);
    CloseHandle(handle);
    if(!length||length>=path.size())return virtual_path;
    path.resize(length);
    if(path.starts_with(L"\\\\?\\UNC\\"))path=L"\\\\"+path.substr(8);
    else if(path.starts_with(L"\\\\?\\"))path.erase(0,4);
    return std::filesystem::path(path);
}

bool SameScenePose(const SceneSnapshot& left, const SceneSnapshot& right)
{
    if (left.reference_id != right.reference_id || left.packets.size() != right.packets.size() || left.skipped != right.skipped ||
        left.radius != right.radius || left.portrait_radius!=right.portrait_radius || std::memcmp(&left.center,&right.center,sizeof(left.center)) != 0 ||
        std::memcmp(&left.head,&right.head,sizeof(left.head)) != 0) return false;
    for (std::size_t index = 0; index < left.packets.size(); ++index) {
        const auto& a = left.packets[index]; const auto& b = right.packets[index];
        for (std::size_t slot=0;slot<TextureCount;++slot)
            if (a.textures[slot].Get() != b.textures[slot].Get()) return false;
        if (a.volatile_vertices || b.volatile_vertices || a.vertices.Get() != b.vertices.Get() || a.indices.Get() != b.indices.Get() ||
            a.descriptor != b.descriptor || a.triangle_count != b.triangle_count ||
            a.clamp_mode != b.clamp_mode || a.source_blend != b.source_blend || a.destination_blend != b.destination_blend ||
            a.two_sided != b.two_sided || a.depth_test != b.depth_test || a.depth_write != b.depth_write ||
            std::memcmp(&a.material,&b.material,sizeof(a.material)) != 0 ||
            std::memcmp(&a.sort_center,&b.sort_center,sizeof(a.sort_center)) != 0 ||
            a.vertex_bytes != b.vertex_bytes || a.index_values != b.index_values ||
            a.dynamic_positions.size() != b.dynamic_positions.size() ||
            std::memcmp(&a.world,&b.world,sizeof(a.world)) != 0 ||
            std::memcmp(a.bones.data(),b.bones.data(),sizeof(a.bones)) != 0 ||
            (!a.dynamic_positions.empty() && std::memcmp(a.dynamic_positions.data(),b.dynamic_positions.data(),a.dynamic_positions.size()*sizeof(DirectX::XMFLOAT4)) != 0)) return false;
        if(bool(a.skin_vertices)!=bool(b.skin_vertices) ||
            (a.skin_vertices && *a.skin_vertices!=*b.skin_vertices))return false;
    }
    return true;
}

constexpr char kShader[] = R"hlsl(
cbuffer DrawConstants : register(b0) {
    row_major float4x4 ViewProjection;
    row_major float4x4 World;
    row_major float4x4 Bones[256];
    float4 Eye;
    float4 Tint;
    float4 UVTransform;
    float4 Specular;
    float4 Lighting;
    float4 Alpha;
    float4 Emissive;
    uint4 Features;
    float4 PreviewKeyDirection;
    float4 PreviewFillDirection;
    float4 PreviewAmbient;
    float4 PreviewKey;
    float4 PreviewFill;
};
struct Input {
    float4 position : POSITION;
#if HAS_UV
    float2 uv : TEXCOORD0;
#endif
#if HAS_NORMAL
    float4 normal : NORMAL;
#endif
#if HAS_COLOR
    float4 color : COLOR0;
#endif
#if SKINNED
    float4 weights : BLENDWEIGHT;
    uint4 indices : BLENDINDICES;
#endif
};
struct Output {
    float4 position : SV_POSITION;
    float3 world : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float3 axisX : TEXCOORD3;
    float3 axisY : TEXCOORD4;
    float3 axisZ : TEXCOORD5;
    float4 color : COLOR0;
};
Output VSMain(Input input) {
    row_major float4x4 transform = World;
#if SKINNED
    float total = max(dot(input.weights, float4(1,1,1,1)), 0.00001);
    float4 w = input.weights / total;
    transform = Bones[input.indices.x]*w.x + Bones[input.indices.y]*w.y +
        Bones[input.indices.z]*w.z + Bones[input.indices.w]*w.w;
#endif
    float4 p = mul(float4(input.position.xyz,1),transform);
    Output output;
    output.position = mul(p, ViewProjection);
    output.world = p.xyz;
    output.axisX = transform[0].xyz;
    output.axisY = transform[1].xyz;
    output.axisZ = transform[2].xyz;
#if HAS_NORMAL
    output.normal = mul(float4(input.normal.xyz*2-1,0),transform).xyz;
#else
    output.normal = 0;
#endif
#if HAS_UV
    output.uv = input.uv * UVTransform.xy + UVTransform.zw;
#else
    output.uv = 0;
#endif
#if HAS_COLOR
    output.color = input.color;
#else
    output.color = 1;
#endif
    return output;
}
Texture2D Diffuse : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D FaceTintTexture : register(t2);
Texture2D DetailTexture : register(t3);
Texture2D SoftTexture : register(t4);
Texture2D SpecularTexture : register(t5);
TextureCube EnvironmentTexture : register(t6);
Texture2D EnvironmentMaskTexture : register(t7);
Texture2D GlowTexture : register(t8);
SamplerState MaterialSampler : register(s0);
bool Enabled(uint flag) { return (Features.x & flag) != 0; }
bool HasTexture(uint slot) { return (Features.y & (1u << slot)) != 0; }
float3 ToLinear(float3 color) {
    color = max(color,0);
    return lerp(color/12.92,pow((color+0.055)/1.055,2.4),step(0.04045,color));
}
float3 ToDisplay(float3 color) {
    color = max(color,0);
    return lerp(color*12.92,1.055*pow(color,1.0/2.4)-0.055,step(0.0031308,color));
}
float3 Authored(float3 sampled, uint slot) {
    // sRGB SRVs have already decoded their RGB; data maps and material tint
    // operations need the authored values, not a second hardware conversion.
    return (Features.z & (1u << slot)) != 0 ? ToDisplay(sampled) : sampled;
}
float3 Unit(float3 direction, float3 fallback) {
    return dot(direction,direction)>0.000001 ? normalize(direction) : fallback;
}
void TestAlpha(float value) {
    if (!Enabled(2048u)) return;
    int comparison = (int)Alpha.z;
    bool accepted = comparison == 0 ||
        (comparison == 1 && value < Alpha.y) ||
        (comparison == 2 && value == Alpha.y) ||
        (comparison == 3 && value <= Alpha.y) ||
        (comparison == 4 && value > Alpha.y) ||
        (comparison == 5 && value != Alpha.y) ||
        (comparison == 6 && value >= Alpha.y);
    if (!accepted) discard;
}
float3 SurfaceNormal(Output input, float3 normal) {
    if (!HasTexture(1)) return normal;
    float3 mapped = Authored(NormalTexture.Sample(MaterialSampler,input.uv).rgb,1);
    if (Enabled(32u)) {
        // Skyrim model-space normal textures store their Y/Z axes exchanged.
        mapped = mapped.xzy*2-1;
        return Unit(mapped.x*input.axisX + mapped.y*input.axisY + mapped.z*input.axisZ,normal);
    }
    mapped = mapped*2-1;
    // Derive the tangent frame from actual UVs. This also handles mirrored UVs
    // and dynamic FaceGen positions without guessing the packed tangent layout.
    float3 dx = ddx(input.world), dy = ddy(input.world);
    float2 tx = ddx(input.uv), ty = ddy(input.uv);
    float uvArea = tx.x*ty.y-tx.y*ty.x;
    if (abs(uvArea)<0.0000001) return normal;
    float3 tangent = (dx*ty.y-dy*tx.y)/uvArea;
    float3 bitangent = (dy*tx.x-dx*ty.x)/uvArea;
    tangent = Unit(tangent-normal*dot(normal,tangent),float3(1,0,0));
    float handedness = dot(cross(normal,tangent),bitangent)<0 ? -1 : 1;
    bitangent = Unit(cross(normal,tangent),float3(0,1,0))*handedness;
    return Unit(tangent*mapped.x+bitangent*mapped.y+normal*mapped.z,normal);
}
float4 PSMain(Output input, bool front : SV_IsFrontFace) : SV_TARGET {
    float4 texel = HasTexture(0) ? Diffuse.Sample(MaterialSampler,input.uv) : float4(0.65,0.71,0.76,1);
    float3 albedo = Authored(texel.rgb,0);
    if (Enabled(4096u) && texel.a>0.00001) albedo /= texel.a;
    if (Enabled(4u)) {
        float tintWeight = Enabled(1u) ? saturate(input.color.g) : 1;
        albedo *= lerp(float3(1,1,1),Tint.rgb,tintWeight);
    } else if (Enabled(8u) || Enabled(16u)) {
        float3 tint = Enabled(8u) ? (HasTexture(2) ? Authored(FaceTintTexture.Sample(MaterialSampler,input.uv).rgb,2) : float3(.5,.5,.5)) : Tint.rgb;
        // FaceGen uses a soft-light color blend and its quantized detail map.
        albedo = lerp(albedo*albedo,1-(1-albedo)*(1-albedo),tint);
        if (Enabled(8u) && HasTexture(3))
            albedo *= (Authored(DetailTexture.Sample(MaterialSampler,input.uv).rgb,3)*255+1)/64;
    } else if (Enabled(1u)) albedo *= input.color.rgb;
    float opacity = Alpha.x;
    if (Enabled(1024u) || Enabled(2048u)) opacity *= texel.a;
    if (Enabled(2u)) opacity *= input.color.a;
    float3 geometric = Unit(input.normal,Unit(cross(ddy(input.world),ddx(input.world)),float3(0,0,1)));
    float3 normal = SurfaceNormal(input,geometric);
    TestAlpha(opacity);
    if (!Enabled(1024u)) opacity = 1;
    clip(opacity-0.00001);
    if (!front && Features.w != 0) normal = -normal;
    float3 view = Unit(Eye.xyz-input.world,float3(0,1,0));
    float3 light = Unit(view * PreviewKeyDirection.w + PreviewKeyDirection.xyz,view);
    float3 fillDir = Unit(PreviewFillDirection.xyz + view * 0.25, view);
    float rawKey = dot(normal, light);
    float key = lerp(saturate(rawKey), saturate(rawKey * 0.78 + 0.22), 0.38);
    float rawFill = dot(normal, fillDir);
    float fill = lerp(saturate(rawFill), saturate(rawFill * 0.65 + 0.35), 0.55);
    float hemi = lerp(0.86, 1.14, saturate(normal.z * 0.5 + 0.5));
    float fresnel = pow(1.0 - saturate(dot(normal, view)), 2.5);
    float studioRim = fresnel * saturate(0.45 + 0.55 * normal.z) * (0.28 + PreviewFill.w * 0.85);
    float3 rimTint = lerp(PreviewFill.rgb, PreviewKey.rgb, 0.45);
    float3 diffuseLight = PreviewAmbient.rgb * hemi + PreviewKey.rgb * key + PreviewFill.rgb * fill + rimTint * studioRim;
    float3 mask = HasTexture(4) ? Authored(SoftTexture.Sample(MaterialSampler,input.uv).rgb,4) : 0;
    if (Enabled(64u)) diffuseLight += mask * max(0,(rawKey+Lighting.y)/(1+Lighting.y)-saturate(rawKey))*PreviewKey.w;
    if (Enabled(128u)) diffuseLight += mask*pow(1-saturate(dot(normal,view)),max(1,Lighting.z))*PreviewFill.w;
    if (Enabled(256u) && HasTexture(5)) diffuseLight += SpecularTexture.Sample(MaterialSampler,input.uv).rgb*saturate(-rawKey)*.25;
    float3 color = ToLinear(albedo)*diffuseLight;
    if (Enabled(512u)) {
        float specularMask = Enabled(32u) ? (HasTexture(5) ? SpecularTexture.Sample(MaterialSampler,input.uv).r : 0) : (HasTexture(1) ? NormalTexture.Sample(MaterialSampler,input.uv).a : 0);
        float gloss = max(2.0, Specular.w);
        float keySpec = pow(saturate(dot(normal,Unit(view+light,view))),gloss);
        float fillSpec = pow(saturate(dot(normal,Unit(view+fillDir,view))),max(2.0, gloss * 0.65)) * 0.32;
        color += Specular.rgb*Lighting.x*specularMask*(keySpec + fillSpec)*PreviewAmbient.w;
    }
    if (Enabled(16384u) && HasTexture(6)) {
        float maskValue = HasTexture(7) ? EnvironmentMaskTexture.Sample(MaterialSampler,input.uv).r : 1;
        color += ToLinear(Authored(EnvironmentTexture.Sample(MaterialSampler,reflect(-view,normal)).rgb,6))*maskValue*Lighting.w;
    }
    if (Enabled(8192u) && HasTexture(8)) color += ToLinear(Authored(GlowTexture.Sample(MaterialSampler,input.uv).rgb,8))*ToLinear(Emissive.rgb)*Emissive.w;
    if (Enabled(4096u)) color *= opacity;
    return float4(color,opacity);
}
struct ResolveOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
ResolveOutput ResolveVS(uint index : SV_VertexID) {
    ResolveOutput output;
    output.uv = float2((index << 1) & 2,index & 2);
    output.position = float4(output.uv*float2(2,-2)+float2(-1,1),0,1);
    return output;
}
float4 ResolvePS(ResolveOutput input) : SV_TARGET {
    float4 color = Diffuse.Sample(MaterialSampler,input.uv);
    float alpha = saturate(color.a);
    // Blend the actor in linear light, then encode and premultiply for the
    // existing UI compositor. Encoding a premultiplied value causes halos.
    return float4(alpha>0.00001 ? ToDisplay(color.rgb/alpha)*alpha : 0,alpha);
}
)hlsl";

struct alignas(16) PreviewLightingConstants {
    // Camera coordinates: right, up, toward the camera (+z is toward camera). w is view-direction bias.
    DirectX::XMFLOAT4 key_direction{-.42F,.52F,.74F,.48F};
    DirectX::XMFLOAT4 fill_direction{.58F,.20F,.68F,0};
    DirectX::XMFLOAT4 ambient{.38F,.40F,.44F,.58F}; // w: specular strength
    DirectX::XMFLOAT4 key{1.16F,1.12F,1.06F,.62F}; // w: material soft-light strength
    DirectX::XMFLOAT4 fill{.36F,.40F,.46F,.26F}; // w: material rim strength
};
static_assert(sizeof(PreviewLightingConstants) == 80);

PreviewLightingConstants PreviewLights(ActorPreviewLighting preset, const DirectX::XMMATRIX& view)
{
    PreviewLightingConstants lights;
    switch (preset) {
    case ActorPreviewLighting::Soft:
        lights.key_direction={-.24F,.42F,.82F,.75F};
        lights.fill_direction={.45F,.24F,.78F,0};
        lights.ambient={.52F,.54F,.57F,.34F};
        lights.key={.82F,.81F,.79F,.48F};
        lights.fill={.38F,.40F,.44F,.18F};
        break;
    case ActorPreviewLighting::Dramatic:
        lights.key_direction={-.78F,.68F,.45F,.22F};
        lights.fill_direction={.65F,.12F,.52F,0};
        lights.ambient={.16F,.18F,.24F,.72F};
        lights.key={1.38F,1.24F,1.10F,.45F};
        lights.fill={.14F,.18F,.28F,.38F};
        break;
    case ActorPreviewLighting::Warm:
        lights.key_direction={-.40F,.50F,.74F,.45F};
        lights.fill_direction={.56F,.18F,.66F,0};
        lights.ambient={.40F,.32F,.25F,.52F};
        lights.key={1.26F,.96F,.68F,.60F};
        lights.fill={.26F,.30F,.38F,.22F};
        break;
    case ActorPreviewLighting::Cool:
        lights.key_direction={-.40F,.50F,.74F,.45F};
        lights.fill_direction={.56F,.18F,.66F,0};
        lights.ambient={.28F,.36F,.46F,.55F};
        lights.key={.82F,1.04F,1.28F,.58F};
        lights.fill={.28F,.34F,.44F,.26F};
        break;
    case ActorPreviewLighting::Studio:
        break;
    }
    // Transform directions only. The lights follow the preview camera without
    // moving world lights or retaining anything from Skyrim's scene graph.
    const auto inverse_view=DirectX::XMMatrixInverse(nullptr,view);
    const auto world_direction=[&](const DirectX::XMFLOAT4& direction) {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result,DirectX::XMVector3TransformNormal(
            DirectX::XMVectorSet(direction.x,direction.y,-direction.z,0),inverse_view));
        result.w=direction.w;
        return result;
    };
    lights.key_direction=world_direction(lights.key_direction);
    lights.fill_direction=world_direction(lights.fill_direction);
    return lights;
}

struct DrawConstants {
    XMFLOAT4X4 view_projection;
    XMFLOAT4X4 world;
    std::array<XMFLOAT4X4, kMaxBones> bones;
    DirectX::XMFLOAT4 eye;
    MaterialConstants material;
    PreviewLightingConstants preview_lighting;
};
static_assert(offsetof(DrawConstants,preview_lighting) == offsetof(DrawConstants,material)+sizeof(MaterialConstants));
static_assert(sizeof(DrawConstants)%16 == 0 && sizeof(DrawConstants) <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT*16);

struct ShaderVariant {
    ComPtr<ID3D11VertexShader> shader;
    ComPtr<ID3DBlob> bytecode;
    bool attempted{};
};

class PreviewRenderer {
public:
    bool Ensure(ID3D11Device* device)
    {
        if (device_.Get() == device && attempted_) return ready_;
        Reset();
        device_ = device;
        attempted_ = true;
        const auto flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL1;
        ComPtr<ID3DBlob> pixel, errors;
        const auto compile_result = D3DCompile(kShader, sizeof(kShader)-1, "SkyUltraActorPreview", nullptr, nullptr,
                "PSMain", "ps_5_0", flags, 0, pixel.GetAddressOf(), errors.GetAddressOf());
        if (!Check(compile_result, "compile actor pixel shader")) {
            if (errors) SKSE::log::error("SkyUltra actor pixel shader failed: {}", static_cast<const char*>(errors->GetBufferPointer()));
            return false;
        }
        if (!Check(device->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr,
                pixel_shader_.GetAddressOf()), "create actor pixel shader")) return false;
        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth = static_cast<UINT>(sizeof(DrawConstants));
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (!Check(device->CreateBuffer(&cb, nullptr, constants_.GetAddressOf()), "create actor constants")) return false;
        ComPtr<ID3DBlob> resolve_vertex,resolve_pixel;
        if (!Check(D3DCompile(kShader,sizeof(kShader)-1,"SkyUltraActorResolve",nullptr,nullptr,
                "ResolveVS","vs_5_0",flags,0,resolve_vertex.GetAddressOf(),nullptr),"compile actor resolve vertex shader") ||
            !Check(D3DCompile(kShader,sizeof(kShader)-1,"SkyUltraActorResolve",nullptr,nullptr,
                "ResolvePS","ps_5_0",flags,0,resolve_pixel.GetAddressOf(),nullptr),"compile actor resolve pixel shader") ||
            !Check(device->CreateVertexShader(resolve_vertex->GetBufferPointer(),resolve_vertex->GetBufferSize(),nullptr,resolve_vertex_.GetAddressOf()),"create actor resolve vertex shader") ||
            !Check(device->CreatePixelShader(resolve_pixel->GetBufferPointer(),resolve_pixel->GetBufferSize(),nullptr,resolve_pixel_.GetAddressOf()),"create actor resolve pixel shader")) return false;
        for (unsigned index=0;index<raster_.size();++index) {
            D3D11_RASTERIZER_DESC raster{};
            raster.FillMode = D3D11_FILL_SOLID;
            raster.CullMode = index == 0 ? D3D11_CULL_BACK : D3D11_CULL_NONE;
            raster.DepthClipEnable = TRUE;
            if (!Check(device->CreateRasterizerState(&raster,raster_[index].GetAddressOf()),"create actor rasterizer")) return false;
        }
        for (unsigned index=0;index<depth_.size();++index) {
            D3D11_DEPTH_STENCIL_DESC depth{};
            depth.DepthEnable = (index & 1U) != 0;
            depth.DepthWriteMask = (index & 2U) != 0 ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
            if (!Check(device->CreateDepthStencilState(&depth,depth_[index].GetAddressOf()),"create actor depth state")) return false;
        }
        D3D11_BLEND_DESC blend{};
        auto& rt = blend.RenderTarget[0];
        rt.BlendEnable = FALSE;
        rt.SrcBlend = D3D11_BLEND_ONE;
        rt.DestBlend = D3D11_BLEND_ZERO;
        rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_ZERO;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (!Check(device->CreateBlendState(&blend, blend_.GetAddressOf()), "create actor blend state")) return false;
        for (unsigned index=0;index<sampler_.size();++index) {
            D3D11_SAMPLER_DESC sampler{};
            sampler.Filter = D3D11_FILTER_ANISOTROPIC;
            sampler.MaxAnisotropy = 4;
            sampler.AddressU = (index & 2U) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV = (index & 1U) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.MaxLOD = D3D11_FLOAT32_MAX;
            if (!Check(device->CreateSamplerState(&sampler,sampler_[index].GetAddressOf()),"create actor sampler")) return false;
        }
        ready_ = true;
        return ready_;
    }

    void WarmupStep(ID3D11Device* device)
    {
        if (device == nullptr) return;
        if (device_.Get() != device || !attempted_) {
            (void)Ensure(device);
            warmup_variant_ = 0;
            return;
        }
        if (!ready_) return;
        while (warmup_variant_ < variants_.size()) {
            const auto index = warmup_variant_++;
            if (!variants_[index].shader && !variants_[index].attempted) {
                (void)EnsureVariant(index);
                return;
            }
        }
    }

    bool Draw(ID3D11DeviceContext* context, const WorkItem& work, RenderedFrame& frame, bool one_shot_export = false)
    {
        if (!CreateTarget(frame, work.request.size)) return false;
        const bool is_item = work.request.item_preview || work.scene->item_sources != nullptr;
        const bool private_meshes = !one_shot_export && is_item;
        if(private_meshes && (!work.scene->item_meshes || work.scene->item_meshes->device!=device_.Get())) {
            auto meshes=std::make_shared<ItemMeshBuffers>();meshes->device=device_.Get();
            meshes->packets.resize(work.scene->packets.size());
            std::size_t private_textures{},missing_textures{};
            std::unordered_map<ID3D11ShaderResourceView*,ComPtr<ID3D11ShaderResourceView>> texture_copies;
            for(std::size_t index=0;index<meshes->packets.size();++index) {
                const auto& packet=work.scene->packets[index];auto& resources=meshes->packets[index];auto& copy=resources.mesh;
                copy.valid=CopyItemBuffer(context,packet.vertices.Get(),packet.vertex_bytes.data(),packet.vertex_bytes.size(),D3D11_BIND_VERTEX_BUFFER,copy.vertices) &&
                    CopyItemBuffer(context,packet.indices.Get(),packet.index_values.data(),packet.index_values.size()*sizeof(std::uint16_t),D3D11_BIND_INDEX_BUFFER,copy.indices);
                if(copy.valid && !packet.dynamic_positions.empty())copy.valid=CopyItemBuffer(context,nullptr,packet.dynamic_positions.data(),packet.dynamic_positions.size()*sizeof(DirectX::XMFLOAT4),D3D11_BIND_VERTEX_BUFFER,copy.dynamic);
                if(copy.valid)for(std::size_t slot=0;slot<TextureCount;++slot)if(packet.textures[slot]) {
                    auto* source=packet.textures[slot].Get();
                    if(const auto found=texture_copies.find(source);found!=texture_copies.end())resources.textures[slot]=found->second;
                    else {
                        if(CopyItemTexture(context,source,resources.textures[slot]))++private_textures;
                        else ++missing_textures;
                        texture_copies.emplace(source,resources.textures[slot]);
                    }
                }
            }
            const auto valid=std::count_if(meshes->packets.begin(),meshes->packets.end(),[](const auto& packet){return packet.mesh.valid;});
            SKSE::log::info("SkyUltra item GPU snapshot v4 for {:08X}: {} meshes, {} unavailable; {} private textures, {} unavailable; source model leased.",
                work.scene->reference_id,valid,meshes->packets.size()-valid,private_textures,missing_textures);
            work.scene->item_meshes=std::move(meshes);
        }
        if(private_meshes) {
            // The reusable color target can have previously displayed an actor.
            // Do not retain its old uploads while this item uses private meshes.
            frame.packet_buffers.clear();frame.uploaded_scene.reset();
            frame.item_constants.resize(work.scene->packets.size());
        }else frame.item_constants.clear();
        if (!private_meshes && frame.uploaded_scene != work.scene) {
            frame.packet_buffers.resize(work.scene->packets.size());
            for (std::size_t index = 0; index < work.scene->packets.size(); ++index) {
                const auto& packet = work.scene->packets[index];
                auto& cached = frame.packet_buffers[index];
                cached.valid = true;
                if (packet.vertices == nullptr) cached.valid = UpdateBuffer(context, packet.vertex_bytes.data(),
                    packet.vertex_bytes.size(), D3D11_BIND_VERTEX_BUFFER, cached.vertices, cached.vertex_capacity);
                else cached.vertices.Reset();
                if (packet.indices == nullptr) cached.valid = UpdateBuffer(context, packet.index_values.data(),
                    packet.index_values.size()*sizeof(std::uint16_t), D3D11_BIND_INDEX_BUFFER, cached.indices, cached.index_capacity) && cached.valid;
                else cached.indices.Reset();
                if (!packet.dynamic_positions.empty()) cached.valid = UpdateBuffer(context, packet.dynamic_positions.data(),
                    packet.dynamic_positions.size()*sizeof(DirectX::XMFLOAT4), D3D11_BIND_VERTEX_BUFFER, cached.dynamic, cached.dynamic_capacity) && cached.valid;
                else cached.dynamic.Reset();
                if (packet.skin_vertices) cached.valid = UpdateBuffer(context, packet.skin_vertices->data(),
                    packet.skin_vertices->size()*sizeof(detail::ActorSkinVertex), D3D11_BIND_VERTEX_BUFFER, cached.skin, cached.skin_capacity) && cached.valid;
                else cached.skin.Reset();
            }
            frame.uploaded_scene = work.scene;
        }
        const float clear[4]{};
        context->ClearRenderTargetView(frame.linear_rtv.Get(), clear);
        context->ClearDepthStencilView(frame.dsv.Get(), D3D11_CLEAR_DEPTH, 1.0F, 0);
        context->OMSetRenderTargets(1, frame.linear_rtv.GetAddressOf(), frame.dsv.Get());
        const D3D11_VIEWPORT viewport{0,0,static_cast<float>(frame.size.width),static_cast<float>(frame.size.height),0,1};
        context->RSSetViewports(1, &viewport);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->HSSetShader(nullptr,nullptr,0);
        context->DSSetShader(nullptr,nullptr,0);
        context->GSSetShader(nullptr,nullptr,0);
        context->PSSetShader(pixel_shader_.Get(),nullptr,0);
        const auto& scene = *work.scene;
        const auto center = work.request.portrait ? scene.head : scene.center;
        const auto radius = work.request.portrait ? scene.portrait_radius : scene.radius;
        const auto aspect = static_cast<float>(frame.size.width)/frame.size.height;
        const auto distance = radius * 2.65F / work.request.zoom / std::min(1.0F,aspect);
        const auto horizontal = std::cos(work.request.pitch)*distance;
        const auto effective_yaw = (work.request.portrait ? scene.portrait_yaw : scene.facing_yaw) + work.request.yaw;
        const auto target_pt = DirectX::XMVectorSet(center.x, center.y, center.z + radius*0.04F, 1.0F);
        const auto eye = DirectX::XMVectorSet(center.x + std::sin(effective_yaw)*horizontal,
            center.y + std::cos(effective_yaw)*horizontal, center.z + radius*0.04F + std::sin(work.request.pitch)*distance, 1.0F);
        const auto up = DirectX::XMVectorSet(-std::sin(effective_yaw)*std::sin(work.request.pitch),
            -std::cos(effective_yaw)*std::sin(work.request.pitch), std::cos(work.request.pitch), 0.0F);
        const auto view = DirectX::XMMatrixLookAtLH(eye, target_pt, up);
        auto projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(42), aspect,
            is_item?std::max(.0001F,radius*.005F):.5F,
            distance+scene.radius*4.0F+(is_item?.1F:100.0F));
        projection.r[2]=DirectX::XMVectorAdd(projection.r[2],DirectX::XMVectorSet(2*work.request.pan_x,-2*work.request.pan_y,0,0));
        DrawConstants data{};
        DirectX::XMStoreFloat4x4(&data.view_projection, view*projection);
        DirectX::XMStoreFloat4(&data.eye, eye);
        data.preview_lighting=PreviewLights(work.request.lighting,view);
        draw_order_.resize(scene.packets.size());
        for (std::size_t index=0;index<draw_order_.size();++index) draw_order_[index]=index;
        std::stable_sort(draw_order_.begin(),draw_order_.end(),[&](std::size_t left,std::size_t right) {
            const auto& a=scene.packets[left];const auto& b=scene.packets[right];
            const bool a_blended=(a.material.features[0]&AlphaBlend)!=0,b_blended=(b.material.features[0]&AlphaBlend)!=0;
            if (a_blended!=b_blended) return !a_blended;
            if (!a_blended) return false;
            const auto distance_squared=[&](const DirectX::XMFLOAT3& point) {
                const auto x=point.x-data.eye.x,y=point.y-data.eye.y,z=point.z-data.eye.z;
                return x*x+y*y+z*z;
            };
            return distance_squared(a.sort_center)>distance_squared(b.sort_center);
        });
        std::size_t draws{};
        for (const auto index : draw_order_) {
            const auto& packet = scene.packets[index];
            const auto& cached = private_meshes?work.scene->item_meshes->packets[index].mesh:frame.packet_buffers[index];
            if (!cached.valid) continue;
            auto vb = !private_meshes && packet.vertices != nullptr ? packet.vertices : cached.vertices;
            auto ib = !private_meshes && packet.indices != nullptr ? packet.indices : cached.indices;
            if (!vb || !ib || !ResourceOnDevice(vb.Get()) || !ResourceOnDevice(ib.Get())) continue;
            auto dynamic = cached.dynamic;
            const auto variant = static_cast<unsigned>(packet.skinned) | (packet.has_uv ? 2U : 0U) | (packet.has_normal ? 4U : 0U) | (packet.has_colors ? 8U : 0U);
            auto layout = InputLayout(packet, variant, dynamic != nullptr);
            if (layout == nullptr) continue;
            context->IASetInputLayout(layout.Get());
            ID3D11Buffer* buffers[]{vb.Get(),dynamic.Get(),cached.skin.Get()};
            const UINT strides[]{packet.stride,sizeof(DirectX::XMFLOAT4),sizeof(detail::ActorSkinVertex)};
            const UINT offsets[]{0,0,0};
            context->IASetVertexBuffers(0,3,buffers,strides,offsets);
            context->IASetIndexBuffer(ib.Get(),DXGI_FORMAT_R16_UINT,0);
            context->VSSetShader(variants_[variant].shader.Get(),nullptr,0);
            data.world = packet.world;
            data.bones = packet.bones;
            data.material = packet.material;
            std::array<ID3D11ShaderResourceView*,TextureCount> textures{};
            const auto& source_textures=private_meshes?work.scene->item_meshes->packets[index].textures:packet.textures;
            for (std::size_t slot=0;slot<TextureCount;++slot) {
                // Item redraws must never revisit views from the game's model.
                // Only the private snapshot's views reach device checks/binds.
                if (ResourceOnDevice(source_textures[slot].Get()) && (packet.has_uv || slot == Environment))
                    textures[slot]=source_textures[slot].Get();
                if (textures[slot]==nullptr) data.material.features[1] &= ~(1U << slot);
            }
            data.material.features[2] &= data.material.features[1];
            if (!BindMaterialState(context,packet)) continue;
            if(private_meshes) {
                if(!UploadItemConstants(context,data,frame.item_constants[index]))continue;
            }else {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                const auto mapped_result = context->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
                if (FAILED(mapped_result)) {
                    if (!logged_upload_failure_) {Check(mapped_result,"upload actor pose constants");logged_upload_failure_=true;}
                    continue;
                }
                std::memcpy(mapped.pData,&data,sizeof(data));
                context->Unmap(constants_.Get(),0);
                context->VSSetConstantBuffers(0,1,constants_.GetAddressOf());
                context->PSSetConstantBuffers(0,1,constants_.GetAddressOf());
            }
            context->PSSetShaderResources(0,static_cast<UINT>(textures.size()),textures.data());
            context->DrawIndexed(packet.triangle_count*3,0,0);
            ++draws;
        }
        std::array<ID3D11ShaderResourceView*,TextureCount> null_textures{};
        context->PSSetShaderResources(0,static_cast<UINT>(null_textures.size()),null_textures.data());
        // All material blending happened in the float linear target. Resolve
        // once on the GPU to the premultiplied display-color UI texture.
        context->OMSetRenderTargets(1,frame.rtv.GetAddressOf(),nullptr);
        context->RSSetState(raster_[1].Get());
        context->OMSetDepthStencilState(depth_[0].Get(),0);
        context->OMSetBlendState(blend_.Get(),nullptr,0xFFFFFFFFU);
        context->IASetInputLayout(nullptr);
        context->VSSetShader(resolve_vertex_.Get(),nullptr,0);
        context->PSSetShader(resolve_pixel_.Get(),nullptr,0);
        context->PSSetSamplers(0,1,sampler_[0].GetAddressOf());
        context->PSSetShaderResources(0,1,frame.linear_srv.GetAddressOf());
        context->Draw(3,0);
        context->PSSetShaderResources(0,1,null_textures.data());
        context->OMSetRenderTargets(0,nullptr,nullptr);
        if (one_shot_export) {
            frame.packet_buffers.clear();
            frame.uploaded_scene.reset();
        }
        return draws > 0;
    }

private:
    bool UploadItemConstants(ID3D11DeviceContext* context,const DrawConstants& data,ComPtr<ID3D11Buffer>& buffer)
    {
        if(!buffer) {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth=static_cast<UINT>(sizeof(DrawConstants));desc.Usage=D3D11_USAGE_DEFAULT;
            desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            if(!Check(device_->CreateBuffer(&desc,nullptr,buffer.GetAddressOf()),"create item draw constants"))return false;
            if(!logged_item_constants_) {
                SKSE::log::info("SkyUltra item constants v2: per-packet DEFAULT buffers, full UpdateSubresource, no mapped item constants ({} bytes; device flags {:08X}; context type {}).",
                    sizeof(DrawConstants),device_->GetCreationFlags(),static_cast<int>(context->GetType()));
                logged_item_constants_=true;
            }
        }
        // A complete constant-buffer update, with no Map/WRITE_DISCARD or
        // sharing with live actors, other item packets, views or PNG targets.
        // Unbind before upload and explicitly rebind both shader stages after.
        ID3D11Buffer* none{};
        context->VSSetConstantBuffers(0,1,&none);
        context->PSSetConstantBuffers(0,1,&none);
        context->UpdateSubresource(buffer.Get(),0,nullptr,&data,0,0);
        context->VSSetConstantBuffers(0,1,buffer.GetAddressOf());
        context->PSSetConstantBuffers(0,1,buffer.GetAddressOf());
        return true;
    }
    bool BindMaterialState(ID3D11DeviceContext* context,const DrawPacket& packet)
    {
        auto* blend = blend_.Get();
        if ((packet.material.features[0]&AlphaBlend)!=0) {
            const auto key = static_cast<std::uint32_t>(packet.source_blend) |
                (static_cast<std::uint32_t>(packet.destination_blend)<<8U);
            auto found = material_blends_.find(key);
            if (found==material_blends_.end()) {
                D3D11_BLEND_DESC desc{};
                auto& target=desc.RenderTarget[0];
                target.BlendEnable=TRUE;target.SrcBlend=packet.source_blend;target.DestBlend=packet.destination_blend;
                target.BlendOp=D3D11_BLEND_OP_ADD;
                target.SrcBlendAlpha=D3D11_BLEND_ONE;target.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
                target.BlendOpAlpha=D3D11_BLEND_OP_ADD;target.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
                ComPtr<ID3D11BlendState> state;
                (void)Check(device_->CreateBlendState(&desc,state.GetAddressOf()),"create actor material blend state");
                found=material_blends_.emplace(key,std::move(state)).first;
            }
            blend=found->second.Get();
            if (blend==nullptr) return false;
        }
        context->OMSetBlendState(blend,nullptr,0xFFFFFFFFU);
        const auto depth_index=static_cast<unsigned>(packet.depth_test) | (packet.depth_write?2U:0U);
        context->OMSetDepthStencilState(depth_[depth_index].Get(),0);
        context->RSSetState(raster_[packet.two_sided?1:0].Get());
        context->PSSetSamplers(0,1,sampler_[packet.clamp_mode].GetAddressOf());
        return true;
    }
    bool EnsureVariant(unsigned index)
    {
        auto& variant = variants_[index];
        if (variant.shader) return true;
        if (variant.attempted) return false;
        variant.attempted = true;
        const D3D_SHADER_MACRO macros[]{{"SKINNED", (index & 1U) ? "1" : "0"},
            {"HAS_UV", (index & 2U) ? "1" : "0"}, {"HAS_NORMAL", (index & 4U) ? "1" : "0"},
            {"HAS_COLOR", (index & 8U) ? "1" : "0"}, {nullptr,nullptr}};
        ComPtr<ID3DBlob> errors;
        const auto result = D3DCompile(kShader,sizeof(kShader)-1,"SkyUltraActorPreview",macros,nullptr,
            "VSMain","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL1,0,
            variant.bytecode.GetAddressOf(),errors.GetAddressOf());
        if (!Check(result,"compile actor vertex shader")) {
            if (errors) SKSE::log::error("SkyUltra actor vertex shader failed: {}",static_cast<const char*>(errors->GetBufferPointer()));
            return false;
        }
        return Check(device_->CreateVertexShader(variant.bytecode->GetBufferPointer(),variant.bytecode->GetBufferSize(),
            nullptr,variant.shader.GetAddressOf()),"create actor vertex shader");
    }
    bool Check(HRESULT result, const char* operation)
    {
        if (SUCCEEDED(result)) return true;
        SKSE::log::error("SkyUltra could not {} (HRESULT {:08X}).",operation,static_cast<std::uint32_t>(result));
        return false;
    }
    bool ResourceOnDevice(ID3D11DeviceChild* buffer)
    {
        if (buffer == nullptr) return true;
        ComPtr<ID3D11Device> owner;
        buffer->GetDevice(owner.GetAddressOf());
        return owner.Get() == device_.Get();
    }
    bool CopyItemBuffer(ID3D11DeviceContext* context,ID3D11Buffer* source,const void* bytes,std::size_t size,
        UINT bind,ComPtr<ID3D11Buffer>& output)
    {
        D3D11_BUFFER_DESC desc{};
        if(source) {
            if(!ResourceOnDevice(source))return false;
            source->GetDesc(&desc);
        } else {
            if(!bytes||!size||size>std::numeric_limits<UINT>::max())return false;
            desc.ByteWidth=static_cast<UINT>(size);
        }
        // Destination dimensions match the source for CopyResource. Engine
        // buffers may be dynamic; our item copy is never mapped or rewritten.
        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=bind;
        desc.CPUAccessFlags=0;desc.MiscFlags=0;desc.StructureByteStride=0;
        const D3D11_SUBRESOURCE_DATA initial{bytes,0,0};
        if(!Check(device_->CreateBuffer(&desc,source?nullptr:&initial,output.GetAddressOf()),"create private item mesh buffer"))return false;
        if(source)context->CopyResource(output.Get(),source);
        return true;
    }
    bool CopyItemTexture(ID3D11DeviceContext* context,ID3D11ShaderResourceView* source,
        ComPtr<ID3D11ShaderResourceView>& output)
    {
        if(!source || !ResourceOnDevice(source))return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};source->GetDesc(&view);
        if(view.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D && view.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURECUBE)return false;
        ComPtr<ID3D11Resource> resource;source->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;
        if(!resource || FAILED(resource.As(&texture)))return false;
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        if(desc.SampleDesc.Count!=1)return false;
        // Preserve dimensions, mip chain, array size and typeless resource
        // format. The new view keeps the original sRGB/data/cube interpretation.
        desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags&=D3D11_RESOURCE_MISC_TEXTURECUBE;
        ComPtr<ID3D11Texture2D> copy;
        if(!Check(device_->CreateTexture2D(&desc,nullptr,copy.GetAddressOf()),"create private item texture") ||
            !Check(device_->CreateShaderResourceView(copy.Get(),&view,output.GetAddressOf()),"create private item texture view"))return false;
        context->CopyResource(copy.Get(),texture.Get());
        return true;
    }
    bool UpdateBuffer(ID3D11DeviceContext* context, const void* bytes, std::size_t size,
        UINT bind, ComPtr<ID3D11Buffer>& output, UINT& capacity)
    {
        if (bytes == nullptr || size == 0 || size > std::numeric_limits<UINT>::max()) return false;
        if (output == nullptr || capacity < size || !ResourceOnDevice(output.Get())) {
            output.Reset();
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth=static_cast<UINT>(size);desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=bind;
            desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            const auto result = device_->CreateBuffer(&desc,nullptr,output.GetAddressOf());
            if (FAILED(result)) {
                if (!logged_buffer_failure_) {Check(result,"create actor geometry upload buffer");logged_buffer_failure_=true;}
                return false;
            }
            capacity=desc.ByteWidth;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto result = context->Map(output.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        if (FAILED(result)) {
            if (!logged_buffer_failure_) {Check(result,"upload actor geometry");logged_buffer_failure_=true;}
            return false;
        }
        std::memcpy(mapped.pData,bytes,size);
        context->Unmap(output.Get(),0);
        return true;
    }
    ComPtr<ID3D11InputLayout> InputLayout(const DrawPacket& packet, unsigned variant, bool dynamic)
    {
        if (!EnsureVariant(variant)) return {};
        const auto key = packet.descriptor ^ (dynamic ? (1ULL<<63U) : 0) ^ (packet.skin_vertices ? (1ULL<<62U) : 0);
        if (const auto found=layouts_.find(key); found!=layouts_.end()) return found->second;
        std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
        elements.push_back({"POSITION",0,dynamic || packet.full_precision ? DXGI_FORMAT_R32G32B32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT,
            dynamic?1U:0U,0,D3D11_INPUT_PER_VERTEX_DATA,0});
        if (packet.has_uv) elements.push_back({"TEXCOORD",0,DXGI_FORMAT_R16G16_FLOAT,0,packet.uv_offset,D3D11_INPUT_PER_VERTEX_DATA,0});
        if (packet.has_normal) elements.push_back({"NORMAL",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,packet.normal_offset,D3D11_INPUT_PER_VERTEX_DATA,0});
        if (packet.has_colors) elements.push_back({"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,packet.color_offset,D3D11_INPUT_PER_VERTEX_DATA,0});
        if (packet.skinned) {
            if(packet.skin_vertices){
                elements.push_back({"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,2,0,D3D11_INPUT_PER_VERTEX_DATA,0});
                elements.push_back({"BLENDINDICES",0,DXGI_FORMAT_R8G8B8A8_UINT,2,16,D3D11_INPUT_PER_VERTEX_DATA,0});
            }else{
                elements.push_back({"BLENDWEIGHT",0,DXGI_FORMAT_R16G16B16A16_FLOAT,0,packet.skin_offset,D3D11_INPUT_PER_VERTEX_DATA,0});
                elements.push_back({"BLENDINDICES",0,DXGI_FORMAT_R8G8B8A8_UINT,0,packet.skin_offset+8,D3D11_INPUT_PER_VERTEX_DATA,0});
            }
        }
        ComPtr<ID3D11InputLayout> layout;
        const auto& bytecode=variants_[variant].bytecode;
        const auto result = device_->CreateInputLayout(elements.data(),static_cast<UINT>(elements.size()),
                bytecode->GetBufferPointer(),bytecode->GetBufferSize(),layout.GetAddressOf());
        if (FAILED(result)) {
            Check(result,"create actor vertex input layout");
            layouts_.emplace(key,ComPtr<ID3D11InputLayout>{});
            return {};
        }
        layouts_.emplace(key,layout);
        return layout;
    }
    bool CreateTarget(RenderedFrame& frame, SizeU size)
    {
        ComPtr<ID3D11Device> owner;
        if (frame.color) frame.color->GetDevice(owner.GetAddressOf());
        const bool same = frame.target_device==device_.Get() && frame.size.width==size.width && frame.size.height==size.height;
        if (same && frame.target_failed) return false;
        if (same && frame.color && frame.rtv && frame.srv && frame.linear_color && frame.linear_rtv && frame.linear_srv && frame.depth && frame.dsv && owner.Get()==device_.Get()) return true;
        frame=RenderedFrame{};
        frame.size=size;
        frame.target_device=device_.Get();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=size.width;desc.Height=size.height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if (!Check(device_->CreateTexture2D(&desc,nullptr,frame.color.GetAddressOf()),"create actor color target") ||
            !Check(device_->CreateRenderTargetView(frame.color.Get(),nullptr,frame.rtv.GetAddressOf()),"create actor target view") ||
            !Check(device_->CreateShaderResourceView(frame.color.Get(),nullptr,frame.srv.GetAddressOf()),"create actor target texture view")) {
            frame.target_failed=true;
            return false;
        }
        desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (!Check(device_->CreateTexture2D(&desc,nullptr,frame.linear_color.GetAddressOf()),"create linear actor color target") ||
            !Check(device_->CreateRenderTargetView(frame.linear_color.Get(),nullptr,frame.linear_rtv.GetAddressOf()),"create linear actor target view") ||
            !Check(device_->CreateShaderResourceView(frame.linear_color.Get(),nullptr,frame.linear_srv.GetAddressOf()),"create linear actor texture view")) {
            frame.target_failed=true;return false;
        }
        desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        const auto complete = Check(device_->CreateTexture2D(&desc,nullptr,frame.depth.GetAddressOf()),"create actor depth target") &&
            Check(device_->CreateDepthStencilView(frame.depth.Get(),nullptr,frame.dsv.GetAddressOf()),"create actor depth target view");
        frame.target_failed=!complete;
        return complete;
    }
    void Reset()
    {
        ready_=false;attempted_=false;logged_buffer_failure_=false;logged_upload_failure_=false;logged_item_constants_=false;variants_={};pixel_shader_.Reset();resolve_vertex_.Reset();resolve_pixel_.Reset();constants_.Reset();raster_={};depth_={};blend_.Reset();sampler_={};material_blends_.clear();draw_order_.clear();layouts_.clear();device_.Reset();
    }
    ComPtr<ID3D11Device> device_;
    std::array<ShaderVariant,16> variants_;
    ComPtr<ID3D11PixelShader> pixel_shader_;
    ComPtr<ID3D11VertexShader> resolve_vertex_;
    ComPtr<ID3D11PixelShader> resolve_pixel_;
    ComPtr<ID3D11Buffer> constants_;
    std::array<ComPtr<ID3D11RasterizerState>,2> raster_;
    std::array<ComPtr<ID3D11DepthStencilState>,4> depth_;
    ComPtr<ID3D11BlendState> blend_;
    std::array<ComPtr<ID3D11SamplerState>,4> sampler_;
    std::unordered_map<std::uint32_t,ComPtr<ID3D11BlendState>> material_blends_;
    std::vector<std::size_t> draw_order_;
    std::unordered_map<std::uint64_t,ComPtr<ID3D11InputLayout>> layouts_;
    bool ready_{};
    bool attempted_{};
    unsigned warmup_variant_{};
    bool logged_buffer_failure_{}, logged_upload_failure_{}, logged_item_constants_{};
};

PreviewRenderer g_renderer;

void ProcessActorExports(ID3D11Device* device, ID3D11DeviceContext* context,
    const std::vector<std::shared_ptr<PngExport>>& exports)
{
    static std::shared_ptr<RenderedFrame> s_thumbnail_frame;
    static std::uint32_t s_splash_export_frame_counter = 0;
    const bool splash_active = FrameworkSplashActive();
    const bool allow_new_render = !splash_active || ((++s_splash_export_frame_counter % 3U) == 0U);
    bool rendered_new_this_frame = !allow_new_render;
    for(const auto& job:exports){
        if(job->canceled.load())continue;
        const auto finish=[&](bool saved,std::string detail){
            std::shared_ptr<PngExport> removed;
            {
                const std::scoped_lock lock(g_mutex);
                if(const auto it=std::find(g_exports.begin(),g_exports.end(),job);it!=g_exports.end()){
                    removed=std::move(*it);
                    g_exports.erase(it);
                }
            }
            if(!job->canceled.load())job->callback(saved,std::move(detail));
        };
        try {
            if(std::chrono::steady_clock::now()-job->started>std::chrono::seconds(8))throw std::runtime_error("The model PNG capture timed out. Try saving again.");
            if(!job->staging){
                if(rendered_new_this_frame)continue;
                rendered_new_this_frame=true;
                if(job->thumbnail){
                    if(!s_thumbnail_frame)s_thumbnail_frame=std::make_shared<RenderedFrame>();
                    job->work.frame=s_thumbnail_frame;
                }else{
                    job->work.frame=std::make_shared<RenderedFrame>();
                }
                if(!g_renderer.Draw(context,job->work,*job->work.frame,job->thumbnail))throw std::runtime_error("Could not render the actor for saving.");
                D3D11_TEXTURE2D_DESC desc{};job->work.frame->color->GetDesc(&desc);
                desc.BindFlags=0;desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                if(FAILED(device->CreateTexture2D(&desc,nullptr,&job->staging)))throw std::runtime_error("Could not create the PNG capture buffer.");
                context->CopyResource(job->staging.Get(),job->work.frame->color.Get());
                job->work.frame.reset();
                continue;
            }
            ComPtr<ID3D11Device> staging_device;job->staging->GetDevice(&staging_device);
            if(staging_device.Get()!=device)throw std::runtime_error("The graphics device changed during PNG capture.");
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const auto hr=context->Map(job->staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
            if(hr==DXGI_ERROR_WAS_STILL_DRAWING)continue;
            if(FAILED(hr))throw std::runtime_error("Could not read the model PNG capture.");
            const auto size=job->work.request.size;
            std::vector<std::uint8_t> pixels;
            try {
                pixels.resize(static_cast<std::size_t>(size.width)*size.height*4);
                for(std::uint32_t y=0;y<size.height;++y)std::memcpy(pixels.data()+static_cast<std::size_t>(y)*size.width*4,
                    static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(y)*mapped.RowPitch,size.width*4);
            } catch (...) {context->Unmap(job->staging.Get(),0);throw;}
            context->Unmap(job->staging.Get(),0);
            const auto folder=job->thumbnail?std::filesystem::path{}:ActorExportDirectory(job->work.request.item_preview);
            const auto filename=std::string(job->work.request.item_preview?"item-":"actor-")+std::to_string(job->work.scene->reference_id)+(job->work.request.portrait?"-portrait-":"-full-")+
                std::to_string(job->work.owner)+"-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".png";
            const auto path=job->thumbnail?job->cache_path:folder/filename;
            const auto secondary_path=job->thumbnail?job->secondary_cache_path:std::filesystem::path{};
            const auto thumbnail=job->thumbnail;
            const auto callback=job->callback;
            std::vector<std::future<void>> finished_workers;
            std::shared_ptr<PngExport> removed_job;
            {
                const std::scoped_lock lock(g_mutex);
                if(job->canceled.load())continue;
                for(auto it=g_export_workers.begin();it!=g_export_workers.end();){
                    if(it->wait_for(std::chrono::seconds(0))==std::future_status::ready){
                        finished_workers.push_back(std::move(*it));
                        it=g_export_workers.erase(it);
                    }else ++it;
                }
                g_export_workers.push_back(std::async(std::launch::async,[pixels=std::move(pixels),size,path,secondary_path,callback,thumbnail]()mutable{
                    bool saved=false;std::string detail;
                    try {
                        if(thumbnail)detail=WriteActorPng(std::move(pixels),size,path,true,secondary_path);
                        else {
                            std::filesystem::create_directories(path.parent_path());(void)WriteActorPng(std::move(pixels),size,path,false);
                            const auto written=WrittenActorPngPath(path);
                            const auto utf8=written.u8string();detail.assign(reinterpret_cast<const char*>(utf8.data()),utf8.size());
                            SKSE::log::info("SkyUltra saved model PNG {}x{} to '{}'.",size.width,size.height,detail);
                        }
                        saved=true;
                    }
                    catch(const std::exception& error){if(!thumbnail){std::error_code ignored;std::filesystem::remove(path,ignored);}detail=error.what();}
                    callback(saved,std::move(detail));
                }));
                if(const auto it=std::find(g_exports.begin(),g_exports.end(),job);it!=g_exports.end()){
                    removed_job=std::move(*it);
                    g_exports.erase(it);
                }
            }
        }catch(const std::exception& error){finish(false,error.what());}
    }
}
} // namespace

ActorPreviewStatus SetActorPreview(ViewId owner, const ActorPreviewRequest& source)
{
    if (owner==0) return {false,false,0,0,0,"An owning SkyUltra view is required."};
    if (REL::Module::IsVR()) return {false,false,0,0,0,"Actor preview compositing currently supports the flat Skyrim UI pass."};
    if (!source.visible) {
        const std::scoped_lock lock(g_mutex);
        const auto found = g_previews.find(owner);
        if (found == g_previews.end()) return {false,false,0,0,0,"No actor preview is attached to this view."};
        found->second.request.visible = false;
        found->second.compositor_visible = false;
        ++found->second.revision;
        return found->second.status;
    }
    auto request=source;
    if(request.item_preview){request.live=false;request.portrait=false;request.pose=ActorPreviewPose::Current;request.animation_id.clear();request.animation_frame=0;}
    if(!request.animation_id.empty()){
        if(request.animation_id.size()>64 || request.animation_frame>=33 || request.pose!=ActorPreviewPose::Current)
            return {false,false,0,0,0,"Choose either a captured animation frame or an arm preset."};
        request.live=false;
    }
    switch (request.lighting) {
    case ActorPreviewLighting::Studio: case ActorPreviewLighting::Soft:
    case ActorPreviewLighting::Dramatic: case ActorPreviewLighting::Warm: case ActorPreviewLighting::Cool:
        break;
    default:
        return {false,false,0,0,0,"Unknown actor preview lighting preset."};
    }
    request.size.width=std::clamp(request.size.width,128U,1024U);
    request.size.height=std::clamp(request.size.height,128U,1024U);
    request.zoom=std::isfinite(request.zoom)?std::clamp(request.zoom,0.4F,3.0F):1.0F;
    request.yaw=std::isfinite(request.yaw)?std::remainder(request.yaw,DirectX::XM_2PI):0.0F;
    request.pitch=std::isfinite(request.pitch)?std::remainder(request.pitch,DirectX::XM_2PI):0.0F;
    request.pan_x=std::isfinite(request.pan_x)?std::clamp(request.pan_x,-2.0F,2.0F):0.0F;
    request.pan_y=std::isfinite(request.pan_y)?std::clamp(request.pan_y,-2.0F,2.0F):0.0F;
    if (request.pose!=ActorPreviewPose::Current) request.live=false;
    if (!std::isfinite(request.normalized_bounds.x)||!std::isfinite(request.normalized_bounds.y)||
        !std::isfinite(request.normalized_bounds.width)||!std::isfinite(request.normalized_bounds.height)||
        request.normalized_bounds.width<=0||request.normalized_bounds.height<=0||
        std::abs(request.normalized_bounds.x)>16||std::abs(request.normalized_bounds.y)>16||
        request.normalized_bounds.width>8||request.normalized_bounds.height>8) {
        return {false,false,0,0,0,"Actor preview bounds must be a finite positive rectangle."};
    }
    std::shared_ptr<const detail::CapturedActorAnimations> animations;
    std::shared_ptr<const SceneSnapshot> replaced_cached_scene;
    {
        const std::scoped_lock lock(g_mutex);
        if(const auto found=g_previews.find(owner);found!=g_previews.end())animations=found->second.animations;
        const auto animation_valid=request.animation_id.empty() || (animations && std::any_of(
            animations->clips.begin(),animations->clips.end(),[&](const auto& clip){
                return clip.info.id==request.animation_id && request.animation_frame<clip.frames.size();
            }));
        if (auto found=g_previews.find(owner); found!=g_previews.end() && found->second.scene && !request.refresh &&
            animation_valid &&
            found->second.request.item_preview==request.item_preview && found->second.request.item_form_id==request.item_form_id &&
            found->second.request.reference_id==request.reference_id && found->second.request.base_form_id==request.base_form_id &&
            found->second.request.pose==request.pose && found->second.request.ignore_world_fade==request.ignore_world_fade &&
            found->second.request.animation_id==request.animation_id &&
            found->second.request.animation_frame==request.animation_frame &&
            found->second.request.portrait==request.portrait &&
            found->second.status.available) {
            const auto& old = found->second.request;
            const auto changed = old.size.width!=request.size.width || old.size.height!=request.size.height ||
                old.portrait!=request.portrait || old.yaw!=request.yaw || old.pitch!=request.pitch || old.zoom!=request.zoom ||
                old.pan_x!=request.pan_x || old.pan_y!=request.pan_y ||
                old.lighting!=request.lighting ||
                old.live!=request.live || old.visible!=request.visible;
            if(request.item_preview && old.lighting!=request.lighting)
                SKSE::log::info("SkyUltra item {:08X} lighting {} -> {}; reusing the captured mesh snapshot ({} packets).",
                    request.item_form_id,static_cast<int>(old.lighting),static_cast<int>(request.lighting),found->second.scene->packets.size());
            found->second.request=request;
            if (changed) ++found->second.revision;
            return found->second.status;
        }
        if (!g_previews.contains(owner) && g_previews.size()>=4) return {false,false,0,0,0,"At most four model preview owners can be active."};
        if(auto found=g_previews.find(owner);found!=g_previews.end() && !request.refresh &&
            !request.animation_id.empty() && animation_valid &&
            found->second.request.reference_id==request.reference_id && found->second.request.base_form_id==request.base_form_id &&
            found->second.request.ignore_world_fade==request.ignore_world_fade){
            auto& state=found->second;
            if(const auto cached=std::find_if(state.animation_scenes.begin(),state.animation_scenes.end(),[&](const auto& entry){
                return entry.clip==request.animation_id && entry.frame==request.animation_frame;
            });cached!=state.animation_scenes.end()){
                if(cached->scene){
                    const auto ready=state.status.ready && state.frame!=nullptr;
                    replaced_cached_scene=std::move(state.scene);
                    state.scene=cached->scene;state.status=cached->status;state.status.ready=ready;
                    state.request=request;++state.revision;
                    return state.status;
                }
            }
        }
    }
    ActorPreviewStatus status;
    auto scene=CaptureScene(request,status,animations);
    std::size_t scene_bytes{};
    if(scene && !request.animation_id.empty()){
        scene_bytes=sizeof(SceneSnapshot)+scene->packets.size()*sizeof(DrawPacket);
        for(const auto& packet:scene->packets)scene_bytes+=packet.vertex_bytes.size()+packet.index_values.size()*sizeof(std::uint16_t)+
            packet.dynamic_positions.size()*sizeof(DirectX::XMFLOAT4)+
            (packet.skin_vertices?packet.skin_vertices->size()*sizeof(detail::ActorSkinVertex):0);
    }
    std::shared_ptr<const SceneSnapshot> old_scene;
    std::shared_ptr<RenderedFrame> old_frame;
    std::shared_ptr<const detail::CapturedActorAnimations> old_animations;
    AnimationSceneCache old_animation_scenes;
    std::vector<std::shared_ptr<const SceneSnapshot>> evicted_scenes;
    ActorPreviewStatus result_status;
    {
        const std::scoped_lock lock(g_mutex);
        auto& state=g_previews[owner];
        const bool same_subject=state.request.item_preview==request.item_preview &&
            state.request.item_form_id==request.item_form_id && state.request.reference_id==request.reference_id &&
            state.request.base_form_id==request.base_form_id;
        // Keep the displayed frame while replacing the pose of the same actor.
        // A transient "not ready" reply used to add a note, resize the stage,
        // and start another capture through ResizeObserver.
        status.ready=status.available && same_subject && state.status.ready && state.frame!=nullptr;
        if(!same_subject || request.refresh || state.request.ignore_world_fade!=request.ignore_world_fade)
            old_animation_scenes=std::move(state.animation_scenes);
        if(request.item_preview)old_animations=std::move(state.animations);
        if(state.animations && status.reference_id && state.animations->reference_id!=status.reference_id)old_animations=std::move(state.animations);
        old_scene=std::move(state.scene);
        state.request=request;state.status=status;state.scene=std::move(scene);++state.revision;
        // Cache a small scrub window, bounded independently of mesh complexity.
        // Never retain all 33 frames of every clip for a heavily modded actor.
        constexpr std::size_t budget=32U*1024U*1024U;
        if(state.scene && !request.animation_id.empty() && scene_bytes<=budget){
            auto cached_bytes=scene_bytes;
            for(const auto& entry:state.animation_scenes)cached_bytes+=entry.bytes;
            while(!state.animation_scenes.empty() && (state.animation_scenes.size()>=8 || cached_bytes>budget)){
                cached_bytes-=state.animation_scenes.front().bytes;
                evicted_scenes.push_back(std::move(state.animation_scenes.front().scene));
                state.animation_scenes.erase(state.animation_scenes.begin());
            }
            state.animation_scenes.push_back({state.scene,status,request.animation_id,request.animation_frame,scene_bytes});
        }
        state.last_pose_capture=std::chrono::steady_clock::now();
        if (!state.status.available) old_frame=std::move(state.frame);
        result_status=state.status;
    }
    return result_status;
}

ActorPreviewAnimations CaptureActorPreviewAnimations(ViewId owner,bool pose_library)
{
    ActorPreviewRequest request;
    std::uint64_t generation{};
    {
        const std::scoped_lock lock(g_mutex);
        const auto found=g_previews.find(owner);
        if(found==g_previews.end()||!found->second.status.available||found->second.status.item_preview)
            return {false,{},"Select an actor with a ready model before capturing animations."};
        request.reference_id=found->second.status.reference_id;
        generation=++g_animation_generation;
    }
    auto* actor=FindActor(request);
    if(!actor)return {false,{},"The actor's 3D is no longer loaded."};
    RE::NiPointer<RE::NiAVObject> root(actor->Get3D(false));
    if(!root||!std::isfinite(root->world.scale)||std::abs(root->world.scale)<.0001F)
        return {false,{},"The actor has no usable third-person 3D."};
    auto captured=pose_library?detail::LoadActorPoseLibrary(actor,root.get(),root->world.Invert(),generation):
        detail::CaptureActorAnimations(actor,root.get(),root->world.Invert(),generation);
    ActorPreviewAnimations result;
    result.available=!captured->clips.empty();result.message=captured->message;
    for(const auto& clip:captured->clips)result.clips.push_back(clip.info);
    std::shared_ptr<const detail::CapturedActorAnimations> old_animations;
    AnimationSceneCache old_animation_scenes;
    {
        const std::scoped_lock lock(g_mutex);
        const auto found=g_previews.find(owner);
        if(found==g_previews.end()||found->second.status.reference_id!=request.reference_id)
            return {false,{},"The actor selection changed during animation capture."};
        old_animations=std::move(found->second.animations);
        old_animation_scenes=std::move(found->second.animation_scenes);
        found->second.animations=std::move(captured);
    }
    return result;
}

void TickActorPreviews()
{
    // Paused actors have no advancing animation. Explicit refresh/equipment
    // changes still call SetActorPreview and capture immediately.
    if (auto* ui=RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;
    struct Pending {ViewId owner;ActorPreviewRequest request;std::uint64_t revision;};
    std::vector<Pending> pending;
    const auto now=std::chrono::steady_clock::now();
    {
        const std::scoped_lock lock(g_mutex);
        for (auto& [owner,state]:g_previews) if (state.request.visible&&state.request.live&&state.compositor_visible) {
            // Creature previews use a fixed bind pose. Check for equipment/3D
            // changes at 1 Hz instead of rebuilding identical meshes at 30 Hz.
            if(state.scene && state.scene->bind_pose && now-state.last_pose_capture<std::chrono::seconds(1))continue;
            state.last_pose_capture=now;
            pending.push_back({owner,state.request,state.revision});
        }
    }
    for (const auto& item:pending) {
        ActorPreviewStatus status;
        auto scene=CaptureScene(item.request,status);
        std::shared_ptr<const SceneSnapshot> old_scene;
        std::shared_ptr<RenderedFrame> old_frame;
        {
            const std::scoped_lock lock(g_mutex);
            const auto found=g_previews.find(item.owner);
            if (found==g_previews.end()||found->second.revision!=item.revision) continue;
            auto& state=found->second;
            if (scene && state.scene && SameScenePose(*state.scene,*scene)) continue;
            status.ready=state.status.ready&&status.available;
            old_scene=std::move(state.scene);
            state.scene=std::move(scene);state.status=std::move(status);++state.revision;
            if (!state.status.available) old_frame=std::move(state.frame);
        }
    }
}

ActorPreviewStatus GetActorPreviewStatus(ViewId owner)
{
    const std::scoped_lock lock(g_mutex);
    if (const auto found=g_previews.find(owner);found!=g_previews.end()) return found->second.status;
    return {false,false,0,0,0,"No actor preview is attached to this view."};
}

void RemoveActorPreview(ViewId owner) noexcept {
    PreviewState removed;
    {
        const std::scoped_lock lock(g_mutex);
        if(const auto it=g_previews.find(owner);it!=g_previews.end()){
            removed=std::move(it->second);
            g_previews.erase(it);
        }
    }
}
void ClearActorPreviews() noexcept {
    std::unordered_map<ViewId, PreviewState> cleared_previews;
    std::vector<std::shared_ptr<PngExport>> canceled;
    std::vector<std::future<void>> workers;
    {const std::scoped_lock lock(g_mutex);cleared_previews.swap(g_previews);canceled.swap(g_exports);workers.swap(g_export_workers);for(const auto& job:canceled)job->canceled=true;}
    for(const auto& job:canceled)try {job->callback(false,"The model preview was closed before PNG capture completed.");}catch(...){}
    for(auto& worker:workers)if(worker.valid())worker.wait();
}

void ConfigureActorPreviewExport(std::uint32_t full_view, std::uint32_t portrait)
{
    const std::scoped_lock lock(g_mutex);
    g_full_view_export_size=std::clamp(full_view,720U,4096U);
    g_portrait_export_size=std::clamp(portrait,720U,4096U);
}

std::unordered_map<std::uint32_t, std::string> g_item_thumbnail_cache;
std::unordered_map<std::string, std::string> g_actor_thumbnail_cache;

void CaptureItemThumbnail(ViewId owner,std::uint32_t form_id,ActorPreviewSaveCallback callback,bool use_preview)
{
    if(!owner||REL::Module::IsVR()){callback(false,"Item thumbnails require a flat Skyrim view.");return;}
    const auto cache_path = ItemThumbnailCachePath(form_id);
    if (!use_preview) {
        std::string cached_data_url;
        {
            const std::scoped_lock lock(g_mutex);
            if (const auto hit = g_item_thumbnail_cache.find(form_id); hit != g_item_thumbnail_cache.end() && !hit->second.empty()) {
                cached_data_url = hit->second;
            }
        }
        if (!cached_data_url.empty()) {
            callback(true, std::move(cached_data_url));
            return;
        }
        if (auto disk_data_url = ReadCachedThumbnailDataUrl(cache_path); !disk_data_url.empty()) {
            {
                const std::scoped_lock lock(g_mutex);
                if (g_item_thumbnail_cache.size() >= 16384) {
                    g_item_thumbnail_cache.erase(g_item_thumbnail_cache.begin());
                }
                g_item_thumbnail_cache[form_id] = disk_data_url;
            }
            callback(true, std::move(disk_data_url));
            return;
        }
    }
    bool busy{};
    ActorPreviewRequest request;
    std::shared_ptr<const SceneSnapshot> scene;
    std::vector<std::future<void>> finished_workers;
    {
        const std::scoped_lock lock(g_mutex);
        for(auto it=g_export_workers.begin();it!=g_export_workers.end();){
            if(it->wait_for(std::chrono::seconds(0))==std::future_status::ready){
                finished_workers.push_back(std::move(*it));
                it=g_export_workers.erase(it);
            }else ++it;
        }
        busy=g_exports.size()+g_export_workers.size()>=12;
        if(!busy && use_preview){
            if(const auto found=g_previews.find(owner);found!=g_previews.end()&&found->second.request.item_preview&&found->second.scene&&!found->second.scene->packets.empty()){
                request=found->second.request;
                request.size={128,128};
                request.visible=true;
                request.live=false;
                scene=found->second.scene;
            }
        }
    }
    if(busy){callback(false,"Model captures are busy. Try again shortly.");return;}
    if(!scene){
        request.item_preview=true;request.item_form_id=form_id;
        request.size={128,128};request.live=false;request.yaw=.5F;request.pitch=.3F;
        ActorPreviewStatus status;
        scene=CaptureItemScene(request,status);
        if(!scene){callback(false,status.message);return;}
    }
    auto job=std::make_shared<PngExport>();job->work={owner,request,std::move(scene),{},1};
    job->thumbnail=true;
    job->cache_path=cache_path;
    job->callback=[form_id, user_cb=std::move(callback)](bool available, std::string detail) mutable {
        if (available && detail.starts_with("data:image/png;base64,")) {
            const std::scoped_lock lock(g_mutex);
            if (g_item_thumbnail_cache.size() >= 16384) {
                g_item_thumbnail_cache.erase(g_item_thumbnail_cache.begin());
            }
            g_item_thumbnail_cache[form_id] = detail;
        }
        user_cb(available, std::move(detail));
    };
    const std::scoped_lock lock(g_mutex);g_exports.push_back(std::move(job));
}

void CaptureActorThumbnail(ViewId owner,std::uint32_t reference_id,std::uint32_t base_form_id,ActorPreviewSaveCallback callback,bool use_preview)
{
    if(!owner||REL::Module::IsVR()){callback(false,"Actor thumbnails require a flat Skyrim view.");return;}
    const auto cache_paths = ResolveActorPortraitCachePaths(reference_id, base_form_id);
    if (!use_preview && !cache_paths.key.empty()) {
        std::string cached_data_url;
        {
            const std::scoped_lock lock(g_mutex);
            if (const auto hit = g_actor_thumbnail_cache.find(cache_paths.key); hit != g_actor_thumbnail_cache.end() && !hit->second.empty()) {
                cached_data_url = hit->second;
            }
        }
        if (!cached_data_url.empty()) {
            callback(true, std::move(cached_data_url));
            return;
        }
        auto disk_data_url = ReadCachedThumbnailDataUrl(cache_paths.primary);
        if (disk_data_url.empty() && !cache_paths.fallback.empty() && cache_paths.fallback != cache_paths.primary) {
            disk_data_url = ReadCachedThumbnailDataUrl(cache_paths.fallback);
        }
        if (!disk_data_url.empty()) {
            {
                const std::scoped_lock lock(g_mutex);
                if (g_actor_thumbnail_cache.size() >= 1024) {
                    g_actor_thumbnail_cache.erase(g_actor_thumbnail_cache.begin());
                }
                g_actor_thumbnail_cache[cache_paths.key] = disk_data_url;
            }
            callback(true, std::move(disk_data_url));
            return;
        }
    }
    bool busy{};
    ActorPreviewRequest request;
    std::shared_ptr<const SceneSnapshot> scene;
    std::vector<std::future<void>> finished_workers;
    {
        const std::scoped_lock lock(g_mutex);
        for(auto it=g_export_workers.begin();it!=g_export_workers.end();){
            if(it->wait_for(std::chrono::seconds(0))==std::future_status::ready){
                finished_workers.push_back(std::move(*it));
                it=g_export_workers.erase(it);
            }else ++it;
        }
        busy=g_exports.size()+g_export_workers.size()>=4;
        if(!busy && use_preview){
            if(const auto found=g_previews.find(owner);found!=g_previews.end()&&!found->second.request.item_preview&&found->second.scene&&!found->second.scene->packets.empty()){
                const bool stage_was_portrait = found->second.request.portrait;
                request=found->second.request;
                request.size={128,128};
                request.visible=true;
                request.live=false;
                request.portrait=true;
                if(!stage_was_portrait){
                    request.pan_x=0.0F;
                    request.pan_y=0.0F;
                    request.zoom=1.22F;
                }
                scene=found->second.scene;
            }
        }
    }
    if(busy){callback(false,"Model captures are busy. Try again shortly.");return;}
    if(!scene){
        request.reference_id=reference_id;request.base_form_id=base_form_id;
        request.size={128,128};request.live=false;request.portrait=true;request.ignore_world_fade=true;
        request.yaw=.06F;request.pitch=.03F;request.zoom=1.22F;
        ActorPreviewStatus status;
        scene=CaptureScene(request,status);
        if(!scene){callback(false,status.message);return;}
    }
    auto job=std::make_shared<PngExport>();job->work={owner,request,std::move(scene),{},1};
    job->thumbnail=true;
    job->cache_path=cache_paths.primary;
    if(!cache_paths.fallback.empty() && cache_paths.fallback != cache_paths.primary){
        job->secondary_cache_path=cache_paths.fallback;
    }
    job->callback=[cache_key=cache_paths.key, user_cb=std::move(callback)](bool available, std::string detail) mutable {
        if (available && detail.starts_with("data:image/png;base64,") && !cache_key.empty()) {
            const std::scoped_lock lock(g_mutex);
            if (g_actor_thumbnail_cache.size() >= 1024) {
                g_actor_thumbnail_cache.erase(g_actor_thumbnail_cache.begin());
            }
            g_actor_thumbnail_cache[cache_key] = detail;
        }
        user_cb(available, std::move(detail));
    };
    const std::scoped_lock lock(g_mutex);g_exports.push_back(std::move(job));
}

void SaveActorPreviewPng(ViewId owner, ActorPreviewSaveCallback callback, std::uint32_t long_edge)
{
    std::string error;
    std::vector<std::future<void>> finished_workers;
    {
        const std::scoped_lock lock(g_mutex);
        for(auto it=g_export_workers.begin();it!=g_export_workers.end();){
            if(it->wait_for(std::chrono::seconds(0))==std::future_status::ready){
                finished_workers.push_back(std::move(*it));
                it=g_export_workers.erase(it);
            }else ++it;
        }
        const auto found=g_previews.find(owner);
        if(found==g_previews.end()||!found->second.scene||!found->second.status.ready)error="Wait for the model preview before saving.";
        else if(g_exports.size()+g_export_workers.size()>=4||std::any_of(g_exports.begin(),g_exports.end(),[owner](const auto& job){return job->work.owner==owner&&!job->thumbnail;}))error="A model PNG capture is already pending. Try again shortly.";
        else {
            const auto& state=found->second;
            auto job=std::make_shared<PngExport>();job->work={owner,state.request,state.scene,{},state.revision};
            // Render a fresh high-resolution target with the same camera and
            // aspect ratio. Never upscale the small on-screen preview bitmap.
            const auto selected_size=long_edge?long_edge:(state.request.portrait?g_portrait_export_size:g_full_view_export_size);
            const auto scale=static_cast<double>(std::clamp(selected_size,720U,4096U))/
                std::max(state.request.size.width,state.request.size.height);
            job->work.request.size={static_cast<std::uint32_t>(std::lround(state.request.size.width*scale)),
                static_cast<std::uint32_t>(std::lround(state.request.size.height*scale))};
            job->callback=std::move(callback);g_exports.push_back(std::move(job));return;
        }
    }
    callback(false,std::move(error));
}

void WarmupActorPreviewRenderer(void* raw_device)
{
    auto* device = static_cast<ID3D11Device*>(raw_device);
    if (device == nullptr || FrameworkSplashActive()) {
        return;
    }
    g_renderer.WarmupStep(device);
}

void RenderActorPreviews(void* raw_device, void* raw_context, std::span<const ViewId> visible_owners)
{
    auto* device=static_cast<ID3D11Device*>(raw_device);
    auto* context=static_cast<ID3D11DeviceContext*>(raw_context);
    if (device==nullptr||context==nullptr) return;
    std::vector<WorkItem> work;
    std::vector<std::shared_ptr<PngExport>> exports;
    bool live{};
    {
        const std::scoped_lock lock(g_mutex);
        exports=g_exports;
        for (auto& [owner,state]:g_previews) {
            state.compositor_visible = state.request.visible && std::find(visible_owners.begin(),visible_owners.end(),owner)!=visible_owners.end();
            if (!state.compositor_visible) continue;
            live=live||state.request.live;
            if (state.scene) work.push_back({owner,state.request,state.scene,state.frame,state.revision});
        }
    }
    const auto now=std::chrono::steady_clock::now();
    if (live&&!FrameworkSplashActive()&&now-g_last_tick_request>=std::chrono::milliseconds(33)&&!g_tick_queued.exchange(true)) {
        g_last_tick_request=now;
        if (auto* tasks=SKSE::GetTaskInterface()) tasks->AddTask([] {
            try {TickActorPreviews();} catch (const std::exception& error) {SKSE::log::warn("SkyUltra actor pose snapshot failed: {}",error.what());}
            g_tick_queued.store(false);
        }); else g_tick_queued.store(false);
    }
    if (work.empty()&&exports.empty()) return;
    if (!g_renderer.Ensure(device)) {
        std::vector<std::shared_ptr<RenderedFrame>> old_frames;
        std::vector<std::shared_ptr<PngExport>> removed_exports;
        {
            const std::scoped_lock lock(g_mutex);
            for (auto& [owner,state]:g_previews) {state.status.ready=false;state.status.message="Could not initialize the model GPU pipeline.";old_frames.push_back(std::move(state.frame));}
            for(const auto& job:exports){
                if(const auto it=std::find(g_exports.begin(),g_exports.end(),job);it!=g_exports.end()){
                    removed_exports.push_back(std::move(*it));
                    g_exports.erase(it);
                }
            }
        }
        for(const auto& job:exports)if(!job->canceled.load())job->callback(false,"Could not initialize the model GPU pipeline for PNG capture.");
        return;
    }
    for (auto& item:work) {
        auto frame=item.frame?item.frame:std::make_shared<RenderedFrame>();
        ComPtr<ID3D11Device> owner_device;
        if (frame->color) frame->color->GetDevice(owner_device.GetAddressOf());
        if (frame->rendered_revision==item.revision&&owner_device.Get()==device) continue;
        const auto rendered=g_renderer.Draw(context,item,*frame);
        std::shared_ptr<RenderedFrame> old_frame;
        {
            const std::scoped_lock lock(g_mutex);
            const auto found=g_previews.find(item.owner);
            if (found==g_previews.end()||found->second.revision!=item.revision) continue;
            auto& state=found->second;
            const auto first_frame = !state.status.ready;
            state.status.ready=rendered;
            if (rendered) {
                frame->rendered_revision=item.revision;old_frame=std::move(state.frame);state.frame=std::move(frame);
                if (first_frame) {
                    SKSE::log::info("SkyUltra rendered {} {:08X} for view {} ({} draw packets, {} skipped, {}x{} GPU target).",
                        item.request.item_preview?"item":"actor",item.scene->reference_id,item.owner,state.status.geometry_count,state.status.skipped_geometry_count,
                        item.request.size.width,item.request.size.height);
                    const auto count_flag=[&](std::uint32_t flag) {
                        return std::count_if(item.scene->packets.begin(),item.scene->packets.end(),[flag](const auto& packet) {
                            return (packet.material.features[0]&flag)!=0;
                        });
                    };
                    const auto normal_maps=std::count_if(item.scene->packets.begin(),item.scene->packets.end(),[](const auto& packet) {
                        return packet.textures[Normal]!=nullptr;
                    });
                    SKSE::log::info("SkyUltra model material coverage: hair tint {}, FaceGen {}, RGB tint {}, normal maps {}, vertex colors {}, alpha test {}, blended {}. Linear-float GPU resolve enabled.",
                        count_flag(HairTint),count_flag(FaceGen),count_flag(FaceRgbTint),normal_maps,count_flag(VertexColor),count_flag(AlphaTest),count_flag(AlphaBlend));
                    for (const auto& packet : item.scene->packets) {
                        if ((packet.material.features[0]&(HairTint|FaceRgbTint))==0) continue;
                        const auto& tint=packet.material.tint;
                        SKSE::log::info("SkyUltra actor tint: flags {:08X}, RGB {:.4f}/{:.4f}/{:.4f}, texture mask {:03X}, sRGB mask {:03X}.",
                            packet.material.features[0],tint.x,tint.y,tint.z,packet.material.features[1],packet.material.features[2]);
                    }
                }
            }
            else {old_frame=std::move(state.frame);state.frame=std::move(frame);state.status.message="The actor snapshot had no drawable GPU packets on the current device; inspect SkyUltra.log for GPU diagnostics.";}
        }
    }
    ProcessActorExports(device,context,exports);
}

std::optional<ActorPreviewFrame> GetActorPreviewFrame(ViewId owner)
{
    const std::scoped_lock lock(g_mutex);
    const auto found=g_previews.find(owner);
    if (found==g_previews.end()||!found->second.request.visible||!found->second.status.ready||!found->second.frame) return std::nullopt;
    const auto& state=found->second;
    return ActorPreviewFrame{state.request.normalized_bounds,state.frame->size,state.frame->srv.Get(),state.frame};
}

} // namespace skyultra
