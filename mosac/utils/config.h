#pragma once

#include "field.h"
// #include "gmp.h"
#include "mosac/utils/field.h"
#include "yacl/base/int128.h"

namespace mosac {

constexpr static uint64_t Prime64 = 2305843009213697249;  // safe prime

// prime order for secp128r2
constexpr static uint128_t Prime128 =
    yacl::MakeUint128(0x3FFFFFFF7FFFFFFF, 0xBE0024720613B5A3);  //

// static mpz_t GMP_Prime64;

// bool inline GlobalInit() {
//   mpz_init(GMP_Prime64);
//   mpz_set_ui(GMP_Prime64, Prime64);
//   return true;
// }

// const static bool init_flag = GlobalInit();

};  // namespace mosac
