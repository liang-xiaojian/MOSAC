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


#include "mosac/cr/utils/linear_code.h"

#include <vector>

#include "gtest/gtest.h"
#include "mosac/ss/type.h"
#include "mosac/utils/vec_op.h"

namespace mosac::code {

TEST(Llc, LlcWorks) {
  // GIVEN
  uint128_t seed = yacl::crypto::SecureRandU128();
  uint32_t n = 102400;
  uint32_t k = 1024;
  LocalLinearCode<10> llc(seed, n, k);
  auto input = internal::op::Rand(k);
  std::vector<internal::PTy> out(n);
  std::vector<internal::PTy> check(n);
  // WHEN
  llc.Encode(input, absl::MakeSpan(out));
  llc.Encode(input, absl::MakeSpan(check));

  uint32_t zero_counter = 0;
  for (uint32_t i = 0; i < n; ++i) {
    EXPECT_EQ(out[i], check[i]);
    if (out[i] == 0) {
      zero_counter++;
    }
  }

  EXPECT_LE(zero_counter, 2);
}

}  // namespace mosac::code