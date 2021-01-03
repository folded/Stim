#include "simd_util.h"
#include <sstream>
#include <cassert>

__m256i popcnt2(__m256i r) {
    auto m = _mm256_set1_epi16(0x5555);
    auto low = _mm256_and_si256(m, r);
    auto high = _mm256_srli_epi16(_mm256_andnot_si256(m, r), 1);
    return _mm256_add_epi16(low, high);
}

__m256i popcnt4(__m256i r) {
    r = popcnt2(r);
    auto m = _mm256_set1_epi16(0x3333);
    auto low = _mm256_and_si256(m, r);
    auto high = _mm256_srli_epi16(_mm256_andnot_si256(m, r), 2);
    return _mm256_add_epi16(low, high);
}

__m256i popcnt8(__m256i r) {
    r = popcnt4(r);
    auto m = _mm256_set1_epi16(0x0F0F);
    auto low = _mm256_and_si256(m, r);
    auto high = _mm256_srli_epi16(_mm256_andnot_si256(m, r), 4);
    return _mm256_add_epi16(low, high);
}

__m256i popcnt16(__m256i r) {
    r = popcnt8(r);
    auto m = _mm256_set1_epi16(0x00FF);
    auto low = _mm256_and_si256(m, r);
    auto high = _mm256_srli_epi16(_mm256_andnot_si256(m, r), 8);
    return _mm256_add_epi16(low, high);
}

std::vector<bool> m256i_to_bits(__m256i r) {
    std::vector<bool> result;
    auto u64 = (uint64_t *)&r;
    for (size_t k = 0; k < 4; k++) {
        auto e = u64[k];
        for (size_t i = 0; i < 64; i++) {
            result.push_back((e >> i) & 1);
        }
    }
    return result;
}

__m256i bits_to_m256i(std::vector<bool> data) {
    __m256i result {};
    auto u64 = (uint64_t *)&result;
    for (size_t i = 0; i < data.size(); i++) {
        u64[i >> 6] |= (uint64_t)data[i] << (i & 63);
    }
    return result;
}

std::string hex(__m256i data) {
    std::stringstream out;
    auto chars = ".123456789ABCDEF";
    auto u64 = (uint64_t *)&data;
    for (size_t w = 0; w < 4; w++) {
        if (w) {
            out << " ";
        }
        for (size_t i = 64; i > 0; i -= 4) {
            out << chars[(u64[w] >> (i - 4)) & 0xF];
        }
    }
    return out.str();
}

std::string bin(__m256i data) {
    std::stringstream out;
    auto chars = "01";
    auto u64 = (uint64_t *)&data;
    for (size_t w = 0; w < 4; w++) {
        if (w) {
            out << " ";
        }
        for (size_t i = 64; i > 0; i--) {
            out << chars[(u64[w] >> (i - 1)) & 0x1];
        }
    }
    return out.str();
}

/// Permutes a bit-packed matrix block to swap a column address bit for a row address bit.
///
/// Args:
///    h: Must be a power of two. The weight of the address bit to swap.
///    matrix: Pointer into the matrix bit data.
///    row_stride_256: The distance (in 256 bit words) between rows of the matrix.
///    mask: Precomputed bit pattern for h. The pattern should be
///        0b...11110000111100001111 where you alternate between 1 and 0 every
///        h'th bit.
template <uint8_t h>
void avx_transpose_pass(uint64_t *matrix, size_t row_stride_256, __m256i mask) noexcept {
    auto u256 = (__m256i *) matrix;
    for (size_t col = 0; col < 256; col += h << 1) {
        for (size_t row = col; row < col + h; row++) {
            auto &a = u256[row * row_stride_256];
            auto &b = u256[(row + h) * row_stride_256];
            auto a1 = _mm256_andnot_si256(mask, a);
            auto b0 = _mm256_and_si256(mask, b);
            a = _mm256_and_si256(mask, a);
            b = _mm256_andnot_si256(mask, b);
            a = _mm256_or_si256(a, _mm256_slli_epi64(b0, h));
            b = _mm256_or_si256(_mm256_srli_epi64(a1, h), b);
        }
    }
}

/// Transposes within the 64x64 bit blocks of a 256x256 block subset of a boolean matrix.
///
/// For example, if we were transposing 2x2 blocks inside a 4x4 matrix, the order would go from:
///
///     aA bB cC dD
///     eE fF gG hH
///
///     iI jJ kK lL
///     mM nN oO pP
///
/// To:
///
///     ae bf cg dh
///     AE BF CG DH
///
///     im jn ko lp
///     IM JN KO LP
///
/// Args:
///     matrix: Pointer to the matrix data to transpose.
///     row_stride_256: Distance, in 256 bit words, between matrix rows.
void avx_transpose_64x64s_within_256x256(uint64_t *matrix, size_t row_stride_256) noexcept {
    avx_transpose_pass<1>(matrix, row_stride_256, _mm256_set1_epi8(0x55));
    avx_transpose_pass<2>(matrix, row_stride_256, _mm256_set1_epi8(0x33));
    avx_transpose_pass<4>(matrix, row_stride_256, _mm256_set1_epi8(0xF));
    avx_transpose_pass<8>(matrix, row_stride_256, _mm256_set1_epi16(0xFF));
    avx_transpose_pass<16>(matrix, row_stride_256, _mm256_set1_epi32(0xFFFF));
    avx_transpose_pass<32>(matrix, row_stride_256, _mm256_set1_epi64x(0xFFFFFFFF));
}

void transpose_bit_matrix(uint64_t *matrix, size_t bit_width) noexcept {
    assert((bit_width & 255) == 0);

    // Transpose bits inside each 64x64 bit block.
    size_t stride = bit_width >> 8;
    for (size_t col = 0; col < bit_width; col += 256) {
        for (size_t row = 0; row < bit_width; row += 256) {
            avx_transpose_64x64s_within_256x256(
                    matrix + ((col + row * bit_width) >> 6),
                    stride);
        }
    }

    // Transpose between 64x64 bit blocks.
    size_t u64_width = bit_width >> 6;
    size_t u64_block = u64_width << 6;
    for (size_t block_row = 0; block_row < bit_width; block_row += 64) {
        for (size_t block_col = block_row + 64; block_col < bit_width; block_col += 64) {
            size_t w0 = (block_row * bit_width + block_col) >> 6;
            size_t w1 = (block_col * bit_width + block_row) >> 6;
            for (size_t k = 0; k < u64_block; k += u64_width) {
                std::swap(matrix[w0 + k], matrix[w1 + k]);
            }
        }
    }
}