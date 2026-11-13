#include "ActorPoseFile.h"

#include <SKSE/SKSE.h>
#include <RE/B/BSResourceNiBinaryStream.h>
#include <RE/H/hkaSplineCompressedAnimation.h>
#include <RE/Offsets_VTABLE.h>
#include <Windows.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace skyultra::detail {
namespace {
constexpr std::size_t kMaxFile=16*1024*1024;
static_assert(sizeof(void*)==8 && std::endian::native==std::endian::little);
static_assert(sizeof(RE::hkaSplineCompressedAnimation)==0xB0);
static_assert(offsetof(RE::hkaSplineCompressedAnimation,numFrames)==0x38 &&
    offsetof(RE::hkaSplineCompressedAnimation,blockOffsets)==0x58 &&
    offsetof(RE::hkaSplineCompressedAnimation,data)==0x98 &&
    offsetof(RE::hkaSplineCompressedAnimation,endian)==0xA8);

struct Section {std::uint32_t start{},local{},global{},virtuals{},exports{},imports{},end{};};
struct Object {std::uint32_t offset{};std::string name;};
class Packfile {
public:
    explicit Packfile(std::span<const std::byte> bytes):bytes_(bytes) {
        if(bytes.size()<64||bytes.size()>kMaxFile||Read<std::uint32_t>(0)!=0x57e0e057U||
            Read<std::uint32_t>(4)!=0x10c0c010U||Read<std::uint32_t>(12)!=8||
            Read<std::uint32_t>(16)!=0x01000108U||Text(40,16)!="hk_2010.2.0-r1")
            throw std::runtime_error("Pose file must be a 64-bit Skyrim SE Havok 2010 packfile.");
        const auto count=Read<std::uint32_t>(20);
        if(count==0||count>16)throw std::runtime_error("Invalid HKX section count.");
        Require(64,count*48);
        for(std::uint32_t i=0;i<count;++i){
            const auto p=64+i*48;
            Section s{Read<std::uint32_t>(p+20),Read<std::uint32_t>(p+24),Read<std::uint32_t>(p+28),
                Read<std::uint32_t>(p+32),Read<std::uint32_t>(p+36),Read<std::uint32_t>(p+40),Read<std::uint32_t>(p+44)};
            if(s.start<64+count*48||s.local>s.global||s.global>s.virtuals||s.virtuals>s.exports||
                s.exports>s.imports||s.imports>s.end)throw std::runtime_error("Invalid HKX section boundaries.");
            Require(s.start,s.end);
            for(const auto& previous:sections_)if(s.end&&previous.end &&
                s.start<static_cast<std::uint64_t>(previous.start)+previous.end &&
                previous.start<static_cast<std::uint64_t>(s.start)+s.end)
                throw std::runtime_error("Overlapping HKX sections.");
            sections_.push_back(s);
        }
        for(const auto& s:sections_){
            Fixups(s,s.local,s.global,8,[&](std::uint32_t p){Pointer(s,Read<std::uint32_t>(p),s,Read<std::uint32_t>(p+4));});
            Fixups(s,s.global,s.virtuals,12,[&](std::uint32_t p){
                const auto target=Read<std::uint32_t>(p+4);
                if(target>=sections_.size())throw std::runtime_error("Invalid HKX pointer section.");
                Pointer(s,Read<std::uint32_t>(p),sections_[target],Read<std::uint32_t>(p+8));
            });
            Fixups(s,s.virtuals,s.exports,12,[&](std::uint32_t p){
                const auto object=Read<std::uint32_t>(p),section=Read<std::uint32_t>(p+4),name=Read<std::uint32_t>(p+8);
                if(object>=s.local||object%16||section>=sections_.size()||name>=sections_[section].local)
                    throw std::runtime_error("Invalid HKX object descriptor.");
                objects_.push_back({s.start+object,Text(sections_[section].start+name,
                    std::min<std::uint32_t>(128,sections_[section].local-name))});
            });
        }
    }
    template<class T>T Read(std::size_t offset) const {
        Require(offset,sizeof(T));T value;std::memcpy(&value,bytes_.data()+offset,sizeof(T));return value;
    }
    void Require(std::size_t offset,std::size_t size) const {
        if(offset>bytes_.size()||size>bytes_.size()-offset)throw std::runtime_error("Truncated HKX data.");
    }
    void Payload(std::size_t offset,std::size_t size) const {
        for(const auto& s:sections_)if(offset>=s.start&&offset-s.start<=s.local&&size<=s.local-(offset-s.start))return;
        throw std::runtime_error("HKX field points outside object data.");
    }
    std::string Text(std::size_t offset,std::size_t maximum) const {
        Require(offset,maximum);
        const auto* data=reinterpret_cast<const char*>(bytes_.data()+offset);
        const auto* end=static_cast<const char*>(std::memchr(data,0,maximum));
        if(!end)throw std::runtime_error("Unterminated HKX string.");
        return std::string(data,end);
    }
    std::uint32_t One(std::string_view name,std::size_t size) const {
        std::uint32_t result{};bool found{};
        for(const auto& object:objects_)if(object.name==name){
            if(found)throw std::runtime_error("Pose file must contain exactly one animation and binding.");
            result=object.offset;found=true;
        }
        if(!found)throw std::runtime_error("Pose file has an unsupported animation class.");
        Payload(result,size);return result;
    }
    template<class T>std::vector<T> Array(std::size_t field,std::size_t maximum) const {
        Payload(field,16);
        const auto count=Read<std::int32_t>(field+8);
        if(count<0||static_cast<std::size_t>(count)>maximum)throw std::runtime_error("Invalid HKX array size.");
        if(count==0)return {};
        const auto found=pointers_.find(static_cast<std::uint32_t>(field));
        if(found==pointers_.end()||found->second%alignof(T))throw std::runtime_error("HKX array has no valid data pointer.");
        const auto size=static_cast<std::size_t>(count)*sizeof(T);Payload(found->second,size);
        std::vector<T> result(count);std::memcpy(result.data(),bytes_.data()+found->second,size);return result;
    }
    std::string String(std::size_t field) const {
        const auto found=pointers_.find(static_cast<std::uint32_t>(field));
        if(found==pointers_.end())return {};
        for(const auto& s:sections_)if(found->second>=s.start&&found->second-s.start<s.local)
            return Text(found->second,std::min<std::size_t>(256,s.local-(found->second-s.start)));
        throw std::runtime_error("Invalid HKX string pointer.");
    }
    void Copy(std::size_t offset,std::span<std::byte> output) const {Payload(offset,output.size());std::memcpy(output.data(),bytes_.data()+offset,output.size());}
private:
    template<class F>void Fixups(const Section& s,std::uint32_t start,std::uint32_t end,std::uint32_t stride,F visit){
        auto p=start;
        for(;end-p>=stride;p+=stride)if(Read<std::uint32_t>(s.start+p)!=UINT32_MAX)visit(s.start+p);
        for(;p<end;++p)if(Read<std::uint8_t>(s.start+p)!=0xFF)throw std::runtime_error("Invalid HKX fixup padding.");
    }
    void Pointer(const Section& source,std::uint32_t field,const Section& target,std::uint32_t destination){
        if(field%8||field>source.local||source.local-field<8||destination>target.local||
            !pointers_.emplace(source.start+field,target.start+destination).second)
            throw std::runtime_error("Invalid or duplicate HKX pointer fixup.");
    }
    std::span<const std::byte> bytes_;
    std::vector<Section> sections_;
    std::unordered_map<std::uint32_t,std::uint32_t> pointers_;
    std::vector<Object> objects_;
};

template<class T>void Write(std::span<std::byte> storage,std::size_t offset,const T& value){
    std::memcpy(storage.data()+offset,&value,sizeof(T));
}
template<class T>void BindArray(std::span<std::byte> storage,std::size_t offset,const std::vector<T>& data,std::size_t count){
    const auto pointer=data.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(data.data());
    Write(storage,offset,pointer);Write(storage,offset+8,static_cast<std::int32_t>(count));
    Write(storage,offset+12,static_cast<std::uint32_t>(count)|0x80000000U);
}

// Keep SEH in a POD-only helper; C++/Havok ownership remains in the caller.
bool SamplePrivate(const RE::hkaAnimation* animation,float time,RE::hkQsTransform* transforms,float* floats) noexcept {
    __try {animation->SampleTracks(time,transforms,floats,nullptr);return true;}
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION||GetExceptionCode()==EXCEPTION_INT_DIVIDE_BY_ZERO?
        EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
} // namespace

std::unique_ptr<ActorPoseFile> ActorPoseFile::Load(const char* path,std::string& error)
{
    try {
        RE::BSResourceNiBinaryStream stream(path);
        if(!stream.good()||!stream.stream)throw std::runtime_error("Pose asset is not available.");
        const auto size=stream.stream->totalSize;
        if(size<64||size>kMaxFile)throw std::runtime_error("Pose asset size is unsupported.");
        std::vector<std::byte> bytes(size);
        if(!stream.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Could not read pose asset.");
        const Packfile file(bytes);
        const auto object=file.One("hkaSplineCompressedAnimation",0xB0),binding=file.One("hkaAnimationBinding",0x48);
        const auto duration=file.Read<float>(object+0x14);
        const auto tracks=file.Read<std::int32_t>(object+0x18),floats=file.Read<std::int32_t>(object+0x1C);
        const auto frames=file.Read<std::int32_t>(object+0x38),blocks=file.Read<std::int32_t>(object+0x3C),
            per_block=file.Read<std::int32_t>(object+0x40),mask=file.Read<std::int32_t>(object+0x44);
        const auto block_time=file.Read<float>(object+0x48),inverse_time=file.Read<float>(object+0x4C),frame_time=file.Read<float>(object+0x50);
        if(file.Read<std::int32_t>(object+0x10)!=5||!std::isfinite(duration)||duration<=0||duration>120||
            tracks<=0||tracks>512||floats<0||floats>512||frames<2||frames>100000||blocks<1||blocks>1024||
            per_block<2||per_block>1024||blocks!=(frames-2)/(per_block-1)+1||
            !std::isfinite(block_time)||block_time<=0||!std::isfinite(inverse_time)||inverse_time<=0||
            std::abs(block_time*inverse_time-1)>0.01F||!std::isfinite(frame_time)||frame_time<=0||
            std::abs(frame_time*(frames-1)-duration)>std::max(.01F,duration*.01F)||
            file.Read<std::int32_t>(object+0xA8)!=0||
            file.Read<std::uint8_t>(binding+0x40)!=0)
            throw std::runtime_error("Pose asset has unsupported spline data or an additive binding.");
        auto result=std::unique_ptr<ActorPoseFile>(new ActorPoseFile);
        result->skeleton_=file.String(binding+0x10);
        result->tracks_=file.Array<std::int16_t>(binding+0x20,512);
        for(std::size_t i=0;i<4;++i)result->offsets_[i]=file.Array<std::uint32_t>(object+0x58+i*16,4096);
        const auto data=file.Array<std::byte>(object+0x98,kMaxFile);
        const auto data_size=data.size();
        if(result->offsets_[0].size()!=static_cast<std::size_t>(blocks)||
            (floats>0&&result->offsets_[1].size()!=static_cast<std::size_t>(blocks))||
            mask<tracks*4+floats||static_cast<std::size_t>(mask)>data_size)
            throw std::runtime_error("Pose asset has invalid spline block tables.");
        for(const auto& offsets:result->offsets_)for(const auto offset:offsets)
            if(offset>data_size)throw std::runtime_error("Pose spline offset is out of range.");
        for(std::size_t i=0;i<result->offsets_[0].size();++i){
            const auto start=result->offsets_[0][i];
            const auto end=i+1<result->offsets_[0].size()?result->offsets_[0][i+1]:data_size;
            if(end<start||end-start<static_cast<std::size_t>(mask))throw std::runtime_error("Pose spline block is truncated.");
        }
        file.Copy(object,result->spline_);
        // Clear all serialized pointers not needed by the spline codec.
        std::fill(result->spline_.begin(),result->spline_.begin()+0x10,std::byte{});
        std::fill(result->spline_.begin()+0x20,result->spline_.begin()+0x38,std::byte{});
        static REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_hkaSplineCompressedAnimation[0]};
        Write(result->spline_,0,vtable.address());
        for(std::size_t i=0;i<4;++i)BindArray(result->spline_,0x58+i*16,result->offsets_[i],result->offsets_[i].size());
        // Native SIMD sampling can read padded lanes at the end of a block.
        result->data_.resize((data_size+64+15)/16);
        std::memcpy(result->data_.data(),data.data(),data_size);
        BindArray(result->spline_,0x98,result->data_,data_size);
        return result;
    }catch(const std::exception& exception){error=exception.what();return {};}
}

const RE::hkaAnimation* ActorPoseFile::Animation() const noexcept {
    return reinterpret_cast<const RE::hkaAnimation*>(spline_.data());
}
bool ActorPoseFile::Sample(float time,RE::hkQsTransform* transforms,float* floats) const noexcept {
    return SamplePrivate(Animation(),time,transforms,floats);
}
} // namespace skyultra::detail
