#pragma once

#include <cstddef>
#include <cstdint>

namespace skyultra {

// Baseline scalar premultiplied-BGRA to straight-BGRA in-place converter.
void UnpremultiplyBgraScalar(std::uint8_t* pixels, std::size_t byte_count) noexcept;

// AVX2-accelerated premultiplied-BGRA to straight-BGRA in-place converter
// (compiled in an isolated /arch:AVX2 translation unit).
void UnpremultiplyBgraAvx2(std::uint8_t* pixels, std::size_t byte_count) noexcept;

// Runtime CPUID + XGETBV (OSXSAVE + YMM state) check for safe AVX2 dispatch.
[[nodiscard]] bool CpuSupportsAvx2ForPixelConvert() noexcept;

// Dispatches to AVX2 when supported by both CPU and OS, otherwise falls back to scalar.
void UnpremultiplyBgraToStraight(
    std::uint8_t* pixels, std::size_t byte_count, bool* out_used_avx2 = nullptr) noexcept;

}  // namespace skyultra
