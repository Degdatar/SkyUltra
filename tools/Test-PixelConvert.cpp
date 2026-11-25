#include "SkyUltra/PixelConvert.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <intrin.h>
#include <vector>

namespace skyultra {

void UnpremultiplyBgraScalar(std::uint8_t* pixels, std::size_t byte_count) noexcept
{
    if (pixels == nullptr || byte_count < 4U) {
        return;
    }
    const std::size_t aligned_bytes = (byte_count / 4U) * 4U;
    for (std::size_t p = 0; p < aligned_bytes; p += 4U) {
        const auto alpha = pixels[p + 3U];
        if (alpha == 0U) {
            pixels[p] = 0U;
            pixels[p + 1U] = 0U;
            pixels[p + 2U] = 0U;
            continue;
        }
        if (alpha == 255U) {
            continue;
        }
        for (unsigned c = 0; c < 3U; ++c) {
            pixels[p + c] = static_cast<std::uint8_t>(
                std::min(255U, (pixels[p + c] * 255U + alpha / 2U) / alpha));
        }
    }
}

bool CpuSupportsAvx2ForPixelConvert() noexcept
{
    int cpu_info[4]{};
    __cpuid(cpu_info, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    __cpuid(cpu_info, 1);
    const bool osxsave = (cpu_info[2] & (1 << 27)) != 0;
    const bool avx = (cpu_info[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) {
        return false;
    }
    if ((_xgetbv(0) & 0x6ULL) != 0x6ULL) {
        return false;
    }
    __cpuidex(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 5)) != 0;
}

}  // namespace skyultra

int main()
{
    if (!skyultra::CpuSupportsAvx2ForPixelConvert()) {
        std::printf("AVX2 unsupported on host CPU; scalar baseline only.\n");
        return 0;
    }

    // 1. Exhaustive bit-exact verification across all 65,536 (channel, alpha) pairs.
    std::vector<std::uint8_t> exhaustive_scalar(65536U * 4U);
    std::vector<std::uint8_t> exhaustive_avx2(65536U * 4U);
    for (std::size_t a = 0; a < 256U; ++a) {
        for (std::size_t c = 0; c < 256U; ++c) {
            const std::size_t idx = (a * 256U + c) * 4U;
            exhaustive_scalar[idx + 0U] = static_cast<std::uint8_t>(c);
            exhaustive_scalar[idx + 1U] = static_cast<std::uint8_t>(c ^ 0x55U);
            exhaustive_scalar[idx + 2U] = static_cast<std::uint8_t>((c + 37U) & 0xFFU);
            exhaustive_scalar[idx + 3U] = static_cast<std::uint8_t>(a);
        }
    }
    exhaustive_avx2 = exhaustive_scalar;
    skyultra::UnpremultiplyBgraScalar(exhaustive_scalar.data(), exhaustive_scalar.size());
    skyultra::UnpremultiplyBgraAvx2(exhaustive_avx2.data(), exhaustive_avx2.size());
    if (exhaustive_scalar != exhaustive_avx2) {
        std::fprintf(stderr, "AVX2 bit-exactness mismatch in exhaustive 65536-pair test.\n");
        return 1;
    }

    // 2. Odd image width boundary test (transparent, semi-transparent, and opaque rows).
    constexpr std::array<std::size_t, 7> kWidths{ 1, 3, 7, 9, 127, 129, 255 };
    for (const std::size_t width : kWidths) {
        std::vector<std::uint8_t> a(width * 4U);
        std::vector<std::uint8_t> b(width * 4U);
        for (std::size_t x = 0; x < width; ++x) {
            a[x * 4U + 0U] = static_cast<std::uint8_t>((x * 13U) & 0xFFU);
            a[x * 4U + 1U] = static_cast<std::uint8_t>((x * 29U) & 0xFFU);
            a[x * 4U + 2U] = static_cast<std::uint8_t>((x * 47U) & 0xFFU);
            a[x * 4U + 3U] = (x % 3U == 0U) ? 0U : ((x % 3U == 1U) ? 255U : static_cast<std::uint8_t>((x * 19U) & 0xFFU));
        }
        b = a;
        skyultra::UnpremultiplyBgraScalar(a.data(), a.size());
        skyultra::UnpremultiplyBgraAvx2(b.data(), b.size());
        if (a != b) {
            std::fprintf(stderr, "AVX2 odd-width mismatch at width=%zu.\n", width);
            return 2;
        }
    }

    // 3. Throughput comparison on a 512x512 portrait buffer.
    constexpr std::size_t kBenchmarkBytes = 512U * 512U * 4U;
    std::vector<std::uint8_t> bench(kBenchmarkBytes, 128U);
    for (std::size_t i = 0; i < bench.size(); i += 4U) {
        bench[i + 3U] = static_cast<std::uint8_t>(((i / 4U) % 253U) + 1U);
    }
    auto t0 = std::chrono::steady_clock::now();
    for (int iter = 0; iter < 50; ++iter) {
        skyultra::UnpremultiplyBgraScalar(bench.data(), bench.size());
    }
    auto t1 = std::chrono::steady_clock::now();
    for (int iter = 0; iter < 50; ++iter) {
        skyultra::UnpremultiplyBgraAvx2(bench.data(), bench.size());
    }
    auto t2 = std::chrono::steady_clock::now();
    const double scalar_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double avx2_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    std::printf("Test-PixelConvert passed (scalar 50x512x512=%.3f ms, avx2=%.3f ms).\n", scalar_ms, avx2_ms);
    return 0;
}
