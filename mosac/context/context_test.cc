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


#include "context.h"

#include <future>

#include "gtest/gtest.h"
#include "mosac/context/register.h"
#include "mosac/utils/test_util.h"
#include "yacl/base/int128.h"
#include "yacl/crypto/utils/rand.h"
#include "yacl/utils/serialize.h"

namespace mosac {

namespace yc = yacl::crypto;

TEST(ContextTest, LinkWork) {
  auto context = MockContext(2);
  uint128_t s_a = yc::SecureRandU128();
  uint128_t s_b = yc::SecureRandU128();

  uint128_t r_a;
  uint128_t r_b;

  auto rank0 = std::async([&] {
    auto lctx = context[0]->GetConnection();
    lctx->SendAsync(lctx->NextRank(), yacl::SerializeUint128(s_a), "s_a");
    auto buff = lctx->Recv(lctx->NextRank(), "s_b");
    return yacl::DeserializeUint128(buff);
  });

  auto rank1 = std::async([&] {
    auto lctx = context[1]->GetConnection();
    auto buff = lctx->Recv(lctx->NextRank(), "s_a");
    lctx->SendAsync(lctx->NextRank(), yacl::SerializeUint128(s_b), "s_b");
    return yacl::DeserializeUint128(buff);
  });

  r_b = rank0.get();
  r_a = rank1.get();

  EXPECT_EQ(s_a, r_a);
  EXPECT_EQ(s_b, r_b);
};

TEST(ContextTest, PrgWork) {
  auto context = MockContext(2);

  auto rank0 = std::async([&] {
    SetupContext(context[0]);
    return context[0]->GetState<Prg>()->Seed();
  });
  auto rank1 = std::async([&] {
    SetupContext(context[1]);
    return context[1]->GetState<Prg>()->Seed();
  });

  auto r_b = rank0.get();
  auto r_a = rank1.get();

  EXPECT_EQ(r_a, r_b);
};

};  // namespace mosac
