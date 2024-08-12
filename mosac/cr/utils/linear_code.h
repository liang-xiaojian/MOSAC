#pragma once

#include <algorithm>
#include <array>
#include <vector>

#include "absl/types/span.h"
#include "mosac/ss/type.h"
#include "yacl/base/exception.h"
#include "yacl/base/int128.h"
#include "yacl/crypto/tools/rp.h"

#ifndef __aarch64__
// sse
#include <emmintrin.h>
#include <smmintrin.h>
// pclmul
#include <wmmintrin.h>
#else
#include "sse2neon.h"
#endif

namespace mosac::code {

namespace yc = yacl::crypto;

constexpr uint32_t kLcBatchSize = 1024;  // linear code batch size

// Linear code interface in F2k
class LinearCodeInterface {
 public:
  LinearCodeInterface(const LinearCodeInterface &) = delete;
  LinearCodeInterface &operator=(const LinearCodeInterface &) = delete;
  LinearCodeInterface() = default;
  virtual ~LinearCodeInterface() = default;

  // Get the dimention / length
  virtual uint32_t GetDimention() const = 0;
  virtual uint32_t GetLength() const = 0;
};

template <size_t d = 10>
class LocalLinearCode : LinearCodeInterface {
 public:
  // constructor
  LocalLinearCode(uint128_t seed, size_t n, size_t k)
      : n_(n), k_(k), rp_(yc::SymmetricCrypto::CryptoType::AES128_ECB, seed) {
    // YACL_ENFORCE(n % kLcBatchSize == 0);
    mask_ = 1;
    while (mask_ < k) {
      mask_ <<= 1;
      mask_ = mask_ | 0x1;
    }

    uint64_t mask64 = ((uint64_t)mask_ << 32 | mask_);
    extend_mask_ = yacl::MakeUint128(mask64, mask64);

    uint64_t k64 = ((uint64_t)k_ << 32 | k_);
    extend_k_ = yacl::MakeUint128(k64, k64);

    uint64_t cmp64 = ((uint64_t)(k_ - 1) << 32 | (k_ - 1));
    extend_cmp_ = yacl::MakeUint128(cmp64, cmp64);
  }

  // override functions
  uint32_t GetDimention() const override { return k_; }
  uint32_t GetLength() const override { return n_; }

  // Encode a message (input) into a codeword (output)
  void Encode(absl::Span<const internal::PTy> in,
              absl::Span<internal::PTy> out) {
    YACL_ENFORCE_EQ(in.size(), k_);
    // YACL_ENFORCE_EQ(out.size(), n_);

    constexpr uint32_t tmp_size = kLcBatchSize * d / 4;
    alignas(16) std::array<uint128_t, tmp_size> tmp;

    auto mask_tmp =
        _mm_loadu_si128((reinterpret_cast<__m128i *>(&extend_mask_)));
    auto k_tmp = _mm_loadu_si128((reinterpret_cast<__m128i *>(&extend_k_)));
    auto cmp_tmp = _mm_loadu_si128((reinterpret_cast<__m128i *>(&extend_cmp_)));

    for (uint32_t i = 0; i < out.size(); i += kLcBatchSize) {
      const uint32_t limit =
          std::min(kLcBatchSize, static_cast<uint32_t>(out.size()) - i);
      const uint32_t block_num = limit * d / 4;

      for (uint32_t j = 0; j < block_num; ++j) {
        _mm_store_si128(reinterpret_cast<__m128i *>(&tmp[j]),
                        _mm_set_epi32(i, 0, j, 0));
      }

      rp_.GenInplace(absl::MakeSpan(reinterpret_cast<uint128_t *>(tmp.data()),
                                    block_num));  // kBatchSize * 10 / 4

      // SIMD
      for (uint32_t j = 0; j < block_num; ++j) {
        auto idx128 = _mm_load_si128(reinterpret_cast<__m128i *>(&tmp[j]));
        idx128 = _mm_and_si128(idx128, mask_tmp);
        // compare idx128 and cmp_tmp
        // return 0xFFFF if true, return 0x0000 otherwise.
        auto sub = _mm_cmpgt_epi32(idx128, cmp_tmp);
        // return k_tmp if idx128 greater than or equal to k
        // return 0x0000 otherwise
        sub = _mm_and_si128(sub, k_tmp);
        idx128 = _mm_sub_epi32(idx128, sub);
        _mm_store_si128(reinterpret_cast<__m128i *>(&tmp[j]), idx128);
      }

      // core
      auto *ptr = reinterpret_cast<uint32_t *>(tmp.data());
      for (uint32_t j = 0; j < limit; ++j) {
        auto tmp = out[i + j];
        for (uint32_t k = 0; k < d; ++k, ++ptr) {
          tmp = tmp + in[*ptr];
        }
        out[i + j] = tmp;
      }
    }
  }

  void Encode2(absl::Span<const internal::PTy> in0,
               absl::Span<internal::PTy> out0,
               absl::Span<const internal::PTy> in1,
               absl::Span<internal::PTy> out1) {
    YACL_ENFORCE_EQ(in0.size(), k_);
    YACL_ENFORCE_EQ(in1.size(), k_);
    YACL_ENFORCE_EQ(out0.size(), out1.size());
    size_t out_size = out0.size();
    // YACL_ENFORCE_EQ(out.size(), n_);

    constexpr uint32_t tmp_size = kLcBatchSize * d / 4;
    alignas(16) std::array<uint128_t, tmp_size> tmp;

    auto mask_tmp =
        _mm_loadu_si128((reinterpret_cast<__m128i *>(&extend_mask_)));
    auto k_tmp = _mm_loadu_si128((reinterpret_cast<__m128i *>(&extend_k_)));
    auto cmp_tmp = _mm_loadu_si128((reinterpret_cast<__m128i *>(&extend_cmp_)));

    for (uint32_t i = 0; i < out_size; i += kLcBatchSize) {
      const uint32_t limit =
          std::min(kLcBatchSize, static_cast<uint32_t>(out_size) - i);
      const uint32_t block_num = limit * d / 4;

      for (uint32_t j = 0; j < block_num; ++j) {
        _mm_store_si128(reinterpret_cast<__m128i *>(&tmp[j]),
                        _mm_set_epi32(i, 0, j, 0));
      }

      rp_.GenInplace(absl::MakeSpan(reinterpret_cast<uint128_t *>(tmp.data()),
                                    block_num));  // kBatchSize * 10 / 4

      // SIMD
      for (uint32_t j = 0; j < block_num; ++j) {
        auto idx128 = _mm_load_si128(reinterpret_cast<__m128i *>(&tmp[j]));
        idx128 = _mm_and_si128(idx128, mask_tmp);
        // compare idx128 and cmp_tmp
        // return 0xFFFF if true, return 0x0000 otherwise.
        auto sub = _mm_cmpgt_epi32(idx128, cmp_tmp);
        // return k_tmp if idx128 greater than or equal to k
        // return 0x0000 otherwise
        sub = _mm_and_si128(sub, k_tmp);
        idx128 = _mm_sub_epi32(idx128, sub);
        _mm_store_si128(reinterpret_cast<__m128i *>(&tmp[j]), idx128);
      }

      // core
      auto *ptr = reinterpret_cast<uint32_t *>(tmp.data());
      for (uint32_t j = 0; j < limit; ++j) {
        auto tmp0 = out0[i + j];
        auto tmp1 = out1[i + j];
        for (uint32_t k = 0; k < d; ++k, ++ptr) {
          tmp0 = tmp0 + in0[*ptr];
          tmp1 = tmp1 + in1[*ptr];
        }
        out0[i + j] = tmp0;
        out1[i + j] = tmp1;
      }
    }
  }

 private:
  uint32_t n_;  // num
  uint32_t k_;  // dimention
  yc::RP rp_;
  uint32_t mask_;
  uint128_t extend_mask_;
  uint128_t extend_k_;
  uint128_t extend_cmp_;
};

}  // namespace mosac::code