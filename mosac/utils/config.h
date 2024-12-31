#pragma once

#include "field.h"
// #include "gmp.h"
#include "mosac/utils/field.h"
#include "mosac/utils/uint256.h"
#include "yacl/base/int128.h"

namespace mosac {

using uint256_t = uint256;

// Mersenne prime, M_p = 2^p - 1
// M31 = 2^31 - 1
// M61 = 2^61 - 1
// M127 = 2^127 - 1

constexpr static uint64_t Prime64 = ((uint64_t)1 << 61) - 1;

constexpr static uint128_t Prime128 =
    (yacl::MakeUint128(0x0, 0x1) << 127) - 1;  //

// static mpz_t GMP_Prime64;

// bool inline GlobalInit() {
//   mpz_init(GMP_Prime64);
//   mpz_set_ui(GMP_Prime64, Prime64);
//   return true;
// }

// const static bool init_flag = GlobalInit();

};  // namespace mosac
