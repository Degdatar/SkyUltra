#include "SkyUltra/PixelConvert.h"

#include <immintrin.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace skyultra {

void UnpremultiplyBgraAvx2(std::uint8_t* pixels, std::size_t byte_count) noexcept
{
    if (pixels == nullptr || byte_count < 4U) {
        return;
    }

    const std::size_t pixel_bytes = (byte_count / 4U) * 4U;
    std::size_t offset = 0U;

    const __m256 v_255 = _mm256_set1_ps(255.0F);
    const __m256 v_one = _mm256_set1_ps(1.0F);
    const __m256 v_zero_f = _mm256_setzero_ps();
    const __m256i v_zero_i = _mm256_setzero_si256();
    const __m256i v_255_i = _mm256_set1_epi32(255);
    const __m256i v_mask_u8 = _mm256_set1_epi32(0xFF);
    const __m256i v_alpha_mask = _mm256_set1_epi32(static_cast<int>(0xFF000000U));

    for (; offset + 32U <= pixel_bytes; offset += 32U) {
        const __m256i px = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pixels + offset));
        const __m256i a_i = _mm256_srli_epi32(px, 24);

        // Fast path: skip opaque blocks unchanged; zero out fully transparent blocks.
        const __m256i eq_255 = _mm256_cmpeq_epi32(a_i, v_255_i);
        if (_mm256_movemask_epi8(eq_255) == -1) {
            continue;
        }
        const __m256i eq_0 = _mm256_cmpeq_epi32(a_i, v_zero_i);
        if (_mm256_movemask_epi8(eq_0) == -1) {
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(pixels + offset), v_zero_i);
            continue;
        }

        const __m256i half_a = _mm256_srli_epi32(a_i, 1);
        const __m256i safe_a_i = _mm256_max_epi32(a_i, _mm256_set1_epi32(1));
        const __m256 alpha_f = _mm256_max_ps(_mm256_cvtepi32_ps(safe_a_i), v_one);

        const auto unpremult_channel = [&](const __m256i chan_i) noexcept -> __m256i {
            const __m256 chan_f = _mm256_cvtepi32_ps(chan_i);
            const __m256 half_f = _mm256_cvtepi32_ps(half_a);
            const __m256 num_f = _mm256_fmadd_ps(chan_f, v_255, half_f);
            const __m256 q_f = _mm256_div_ps(num_f, alpha_f);
            __m256i q_i = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(q_f, v_zero_f), v_255));
            q_i = _mm256_min_epi32(_mm256_max_epi32(q_i, v_zero_i), v_255_i);
            return _mm256_andnot_si256(eq_0, q_i);
        };

        const __m256i b_i = unpremult_channel(_mm256_and_si256(px, v_mask_u8));
        const __m256i g_i = unpremult_channel(_mm256_and_si256(_mm256_srli_epi32(px, 8), v_mask_u8));
        const __m256i r_i = unpremult_channel(_mm256_and_si256(_mm256_srli_epi32(px, 16), v_mask_u8));

        const __m256i out_px = _mm256_or_si256(
            _mm256_or_si256(b_i, _mm256_slli_epi32(g_i, 8)),
            _mm256_or_si256(_mm256_slli_epi32(r_i, 16), _mm256_and_si256(px, v_alpha_mask)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(pixels + offset), out_px);
    }

    // Scalar tail for remaining < 8 pixels (handles odd widths identically).
    if (offset < pixel_bytes) {
        UnpremultiplyBgraScalar(pixels + offset, pixel_bytes - offset);
    }
}

}  // namespace skyultra
