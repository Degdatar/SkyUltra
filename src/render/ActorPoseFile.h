#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace RE { class hkaAnimation; class hkQsTransform; }

namespace skyultra::detail {

// Private, non-refcounted storage for one validated 64-bit Skyrim spline clip.
// No Havok destructor runs on these buffers. Only SampleTracks uses the native
// codec; physics, annotations, extracted motion and graph activation are absent.
class ActorPoseFile final {
public:
    ~ActorPoseFile()=default;
    ActorPoseFile(const ActorPoseFile&)=delete;
    ActorPoseFile& operator=(const ActorPoseFile&)=delete;
    ActorPoseFile(ActorPoseFile&&)=delete;
    ActorPoseFile& operator=(ActorPoseFile&&)=delete;
    static std::unique_ptr<ActorPoseFile> Load(const char* path,std::string& error);
    const RE::hkaAnimation* Animation() const noexcept;
    bool Sample(float time,RE::hkQsTransform* transforms,float* floats) const noexcept;
    std::span<const std::int16_t> Tracks() const noexcept {return tracks_;}
    const std::string& Skeleton() const noexcept {return skeleton_;}
private:
    ActorPoseFile()=default;
    struct alignas(16) CodecBlock {std::array<std::byte,16> bytes{};};
    alignas(16) std::array<std::byte,0xB0> spline_{};
    std::array<std::vector<std::uint32_t>,4> offsets_;
    std::vector<CodecBlock> data_;
    std::vector<std::int16_t> tracks_;
    std::string skeleton_;
};

} // namespace skyultra::detail
