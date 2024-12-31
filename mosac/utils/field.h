#pragma once

// #include "gmp.h"
#include "mosac/utils/config.h"
#include "yacl/base/int128.h"
#include "yacl/crypto/utils/rand.h"

namespace mosac {

namespace {

uint64_t inline exgcd64(uint64_t a, uint64_t b, uint64_t &x, uint64_t &y) {
  uint64_t x1 = 1, x2 = 0, x3 = 0, x4 = 1;
  while (b != 0) {
    uint64_t c = a / b;
    std::tie(x1, x2, x3, x4, a, b) =
        std::make_tuple(x3, x4, x1 - x3 * c, x2 - x4 * c, b, a - b * c);
  }
  x = x1, y = x2;
  return a;
}

uint128_t inline exgcd128(uint128_t a, uint128_t b, uint128_t &x,
                          uint128_t &y) {
  uint128_t x1 = 1, x2 = 0, x3 = 0, x4 = 1;
  while (b != static_cast<uint128_t>(0)) {
    uint128_t c = a / b;
    std::tie(x1, x2, x3, x4, a, b) =
        std::make_tuple(x3, x4, x1 - x3 * c, x2 - x4 * c, b, a - b * c);
  }
  x = x1;
  y = x2;
  return a;
}

// Invert function in GMP is not as efficient as repect
// 10000 times "Div" ( GMP 27ms v.s. exgcd 12ms )

// uint64_t inline gmp_invert(uint64_t in) {
//   mpz_t tmp;
//   mpz_init(tmp);
//   mpz_set_ui(tmp, in);
//   mpz_t ret;
//   mpz_init(ret);
//   mpz_invert(ret, tmp, GMP_Prime64);
//   return mpz_get_ui(ret);
// }

}  // namespace

class kFp64 {
 public:
  kFp64() : val_(0) {}

  kFp64(int val) { val_ = val % Prime64; }

  kFp64(uint64_t val) {
    auto tmp = (val & Prime64) + (val >> 61);
    tmp == Prime64 ? val_ = 0 : val_ = tmp;
  }

  kFp64(uint128_t val) {
    auto tmp = (val & yacl::MakeUint128(0, Prime64)) + (val >> 61);
    tmp = (tmp & yacl::MakeUint128(0, Prime64)) + (tmp >> 61);
    auto [high, low] = yacl::DecomposeUInt128(tmp);
    YACL_ENFORCE(high == 0);
    low == Prime64 ? val_ = 0 : val_ = low;
  }

  kFp64 operator+(const kFp64 &rhs) const { return kFp64(val_ + rhs.val_); }

  kFp64 operator-(const kFp64 &rhs) const {
    return kFp64(val_ - rhs.val_ + Prime64);
  }

  kFp64 operator*(const kFp64 &rhs) const {
    uint128_t l = val_;
    uint128_t r = rhs.val_;
    return kFp64(l * r);
  }

  kFp64 operator/(const kFp64 &rhs) const { return (*this) * Inv(rhs); }

  bool operator==(const kFp64 &rhs) const { return this->val_ == rhs.val_; }

  bool operator!=(const kFp64 &rhs) const { return !(*this == rhs); }

  uint64_t GetVal() const { return val_; }

  static kFp64 Add(const kFp64 &lhs, const kFp64 &rhs) { return lhs + rhs; }

  static kFp64 Sub(const kFp64 &lhs, const kFp64 &rhs) { return lhs - rhs; }

  static kFp64 Mul(const kFp64 &lhs, const kFp64 &rhs) { return lhs * rhs; }

  static kFp64 Div(const kFp64 &lhs, const kFp64 &rhs) { return lhs / rhs; }

  static kFp64 Inv(const kFp64 &in) {
    // uint64_t result = gmp_invert(in.val_);
    uint64_t result = 0, _ = 0;
    uint64_t check = exgcd64(in.val_, Prime64, result, _);
    YACL_ENFORCE(check == 1);
    return kFp64(result + Prime64);
  }

  static kFp64 Neg(const kFp64 &in) { return kFp64(Prime64 - in.val_); }

  static bool Equal(const kFp64 &lhs, const kFp64 &rhs) { return lhs == rhs; }

  static kFp64 Rand() {
    uint64_t rand_val = yacl::crypto::SecureRandU64();
    return kFp64(rand_val);
  }

  static uint64_t GetPrime() { return Prime64; }

  static kFp64 One() { return kFp64((uint64_t)1); }

  static kFp64 Zero() { return kFp64((uint64_t)0); }

 protected:
  uint64_t val_;
};

class kFp128 {
 public:
  kFp128() : val_(yacl::MakeUint128(0, 0)) {}

  kFp128(int val) { val_ = val % Prime128; }

  kFp128(uint64_t val) : val_(yacl::MakeUint128(0, val)) {}

  kFp128(uint128_t val) {
    uint128_t tmp = (val & Prime128) + (val >> 127);
    tmp == Prime128 ? val_ = 0 : val_ = tmp;
  }

  kFp128(uint256_t val) {
    auto tmp = (val & Prime128) + (val >> 127);
    tmp = (tmp & Prime128) + (tmp >> 127);

    uint128_t low = static_cast<uint128_t>(tmp & Prime128);
    low == Prime128 ? val_ = 0 : val_ = low;
  }

  kFp128 operator+(const kFp128 &rhs) const {
    // auto tmp = static_cast<uint256_t>(val_) +
    // static_cast<uint256_t>(rhs.val_);
    return kFp128(val_ + rhs.val_);
  }

  kFp128 operator-(const kFp128 &rhs) const {
    // auto tmp = static_cast<uint256_t>(val_) + Prime128 -
    //            static_cast<uint256_t>(rhs.val_);
    return kFp128(val_ + Prime128 - rhs.val_);
  }

  kFp128 operator*(const kFp128 &rhs) const {
    uint256_t l = static_cast<uint256_t>(val_);
    uint256_t r = static_cast<uint256_t>(rhs.val_);
    return kFp128(l * r);
  }

  kFp128 operator/(const kFp128 &rhs) const { return (*this) * Inv(rhs); }

  bool operator==(const kFp128 &rhs) const { return this->val_ == rhs.val_; }

  bool operator!=(const kFp128 &rhs) const { return !(*this == rhs); }

  uint128_t GetVal() const { return val_; }

  static kFp128 Add(const kFp128 &lhs, const kFp128 &rhs) { return lhs + rhs; }

  static kFp128 Sub(const kFp128 &lhs, const kFp128 &rhs) { return lhs - rhs; }

  static kFp128 Mul(const kFp128 &lhs, const kFp128 &rhs) { return lhs * rhs; }

  static kFp128 Div(const kFp128 &lhs, const kFp128 &rhs) { return lhs / rhs; }

  static kFp128 Inv(const kFp128 &in) {
    // uint64_t result = gmp_invert(in.val_);
    uint128_t result = 0, _ = 0;
    uint128_t check = exgcd128(in.val_, Prime128, result, _);
    YACL_ENFORCE(check == 1, "current check is {}", check);
    return kFp128(result + Prime128);
  }

  static kFp128 Neg(const kFp128 &in) { return kFp128(Prime128 - in.val_); }

  static bool Equal(const kFp128 &lhs, const kFp128 &rhs) { return lhs == rhs; }

  static kFp128 Rand() {
    uint128_t rand_val = yacl::crypto::SecureRandU128();
    return kFp128(rand_val);
  }

  static uint128_t GetPrime() { return Prime128; }

  static kFp128 One() { return kFp128((uint64_t)1); }

  static kFp128 Zero() { return kFp128((uint64_t)0); }

 protected:
  uint128_t val_;
};

};  // namespace mosac
