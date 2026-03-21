// Copyright 2026 Ant International, Ant Group Co., Ltd.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Author (Xiaojian Liang)


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
