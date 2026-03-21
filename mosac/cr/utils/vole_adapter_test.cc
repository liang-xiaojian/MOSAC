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


#include "mosac/cr/utils/vole_adapter.h"

#include <future>
#include <set>

#include "gtest/gtest.h"
#include "mosac/context/register.h"
#include "mosac/cr/cr.h"
#include "mosac/cr/utils/ot_adapter.h"
#include "mosac/cr/utils/ot_helper.h"
#include "mosac/cr/utils/vole.h"
#include "mosac/utils/test_util.h"
#include "mosac/utils/vec_op.h"
#include "vole.h"
#include "yacl/crypto/utils/rand.h"

namespace mosac::vole {

namespace yc = yacl::crypto;

struct VoleTestParam {
  size_t num;
};

class VoleAdapterTest : public ::testing::TestWithParam<VoleTestParam> {};

TEST_P(VoleAdapterTest, Work) {
  const size_t vole_num = GetParam().num;
  auto deltas = internal::op::Rand(1);
  auto delta = deltas[0];

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<ot::YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<ot::YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<ot::YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<ot::YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto otSender = ot0.first;

    auto voleSender =
        std::make_shared<WolverineVoleAdapter>(conn, otSender, delta);

    std::vector<internal::PTy> c(vole_num);
    voleSender->rsend(absl::MakeSpan(c));
    return c;
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto otReceiver = ot1.second;

    auto voleReceiver =
        std::make_shared<WolverineVoleAdapter>(conn, otReceiver);
    std::vector<internal::PTy> a(vole_num);
    std::vector<internal::PTy> b(vole_num);
    voleReceiver->rrecv(absl::MakeSpan(a), absl::MakeSpan(b));
    return std::make_pair(a, b);
  });

  auto c = rank0.get();
  auto [a, b] = rank1.get();

  for (size_t i = 0; i < vole_num; ++i) {
    EXPECT_EQ(a[i] * delta + b[i], c[i]);
  }
}

struct SilentVoleTestParam {
  size_t num;
};

class SilentVoleAdapterTest
    : public ::testing::TestWithParam<SilentVoleTestParam> {};

TEST_P(SilentVoleAdapterTest, Work) {
  const size_t vole_num = GetParam().num;
  auto deltas = internal::op::Rand(1);
  auto delta = deltas[0];

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<ot::YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<ot::YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<ot::YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<ot::YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto otSender = ot0.first;

    auto voleSender =
        std::make_shared<SilentVoleAdapter>(conn, otSender, delta);

    std::vector<internal::PTy> c(vole_num);
    voleSender->rsend(absl::MakeSpan(c));
    return c;
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto otReceiver = ot1.second;

    auto voleReceiver = std::make_shared<SilentVoleAdapter>(conn, otReceiver);
    std::vector<internal::PTy> a(vole_num);
    std::vector<internal::PTy> b(vole_num);
    voleReceiver->rrecv(absl::MakeSpan(a), absl::MakeSpan(b));
    return std::make_pair(a, b);
  });

  auto c = rank0.get();
  auto [a, b] = rank1.get();

  for (size_t i = 0; i < vole_num; ++i) {
    EXPECT_EQ(a[i] * delta + b[i], c[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(Works, VoleAdapterTest,
                         testing::Values(VoleTestParam{2}, VoleTestParam{10},
                                         VoleTestParam{1000},
                                         VoleTestParam{1 << 20}));

INSTANTIATE_TEST_SUITE_P(Works, SilentVoleAdapterTest,
                         testing::Values(SilentVoleTestParam{32},
                                         SilentVoleTestParam{64},
                                         SilentVoleTestParam{1 << 14},
                                         SilentVoleTestParam{1 << 16},
                                         SilentVoleTestParam{1 << 18},
                                         SilentVoleTestParam{1 << 20}));

}  // namespace mosac::vole
