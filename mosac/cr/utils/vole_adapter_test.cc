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

INSTANTIATE_TEST_SUITE_P(Works, VoleAdapterTest,
                         testing::Values(VoleTestParam{2}, VoleTestParam{10},
                                         VoleTestParam{1000},
                                         VoleTestParam{1 << 20}));
}  // namespace mosac::vole
