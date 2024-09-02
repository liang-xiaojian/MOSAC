#include "mosac/cr/utils/ot_helper.h"

#include <future>

#include "gtest/gtest.h"
#include "mosac/context/register.h"
#include "mosac/cr/cr.h"
#include "mosac/utils/test_util.h"
#include "mosac/utils/vec_op.h"

namespace mosac::ot {

TEST(OtHelperTest, BeaverWork) {
  const size_t num = 10000;

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto ot_sender = ot0.first;
    auto ot_receiver = ot0.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num);
    std::vector<internal::PTy> b(num);
    std::vector<internal::PTy> c(num);
    helper.BeaverTriple(conn, absl::MakeSpan(a), absl::MakeSpan(b),
                        absl::MakeSpan(c));
    return std::make_tuple(a, b, c);
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto ot_sender = ot1.first;
    auto ot_receiver = ot1.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num);
    std::vector<internal::PTy> b(num);
    std::vector<internal::PTy> c(num);
    helper.BeaverTriple(conn, absl::MakeSpan(a), absl::MakeSpan(b),
                        absl::MakeSpan(c));
    return std::make_tuple(a, b, c);
  });

  auto [a0, b0, c0] = rank0.get();
  auto [a1, b1, c1] = rank1.get();

  auto a = internal::op::Add(absl::MakeSpan(a0), absl::MakeSpan(a1));
  auto b = internal::op::Add(absl::MakeSpan(b0), absl::MakeSpan(b1));
  auto c = internal::op::Add(absl::MakeSpan(c0), absl::MakeSpan(c1));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a[i] * b[i], c[i]);
  }
}

TEST(OtHelperTest, BeaverExtendWork) {
  const size_t num = 10000;

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto ot_sender = ot0.first;
    auto ot_receiver = ot0.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num);
    std::vector<internal::PTy> b(num);
    std::vector<internal::PTy> c(num);
    std::vector<internal::PTy> A(num);
    std::vector<internal::PTy> C(num);
    helper.BeaverTripleExtend(conn, absl::MakeSpan(a), absl::MakeSpan(b),
                              absl::MakeSpan(c), absl::MakeSpan(A),
                              absl::MakeSpan(C));
    return std::make_tuple(a, b, c, A, C);
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto ot_sender = ot1.first;
    auto ot_receiver = ot1.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num);
    std::vector<internal::PTy> b(num);
    std::vector<internal::PTy> c(num);
    std::vector<internal::PTy> A(num);
    std::vector<internal::PTy> C(num);
    helper.BeaverTripleExtend(conn, absl::MakeSpan(a), absl::MakeSpan(b),
                              absl::MakeSpan(c), absl::MakeSpan(A),
                              absl::MakeSpan(C));
    return std::make_tuple(a, b, c, A, C);
  });

  auto [a0, b0, c0, A0, C0] = rank0.get();
  auto [a1, b1, c1, A1, C1] = rank1.get();

  auto a = internal::op::Add(absl::MakeSpan(a0), absl::MakeSpan(a1));
  auto b = internal::op::Add(absl::MakeSpan(b0), absl::MakeSpan(b1));
  auto c = internal::op::Add(absl::MakeSpan(c0), absl::MakeSpan(c1));

  auto A = internal::op::Add(absl::MakeSpan(A0), absl::MakeSpan(A1));
  auto C = internal::op::Add(absl::MakeSpan(C0), absl::MakeSpan(C1));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a[i] * b[i], c[i]);
    EXPECT_EQ(A[i] * b[i], C[i]);
  }
}

TEST(OtHelperTest, BaseVoleWork) {
  const size_t num = 10000;

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto ot_sender = ot0.first;
    auto ot_receiver = ot0.second;
    auto helper = OtHelper(ot_sender, ot_receiver);

    auto delta = internal::op::Rand(1)[0];
    std::vector<internal::PTy> c(num);
    helper.BaseVoleSend(conn, delta, absl::MakeSpan(c));
    return std::make_tuple(delta, c);
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto ot_sender = ot1.first;
    auto ot_receiver = ot1.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num);
    std::vector<internal::PTy> b(num);
    helper.BaseVoleRecv(conn, absl::MakeSpan(a), absl::MakeSpan(b));
    return std::make_tuple(a, b);
  });

  auto [delta, c] = rank0.get();
  auto [a, b] = rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a[i] * delta + b[i], c[i]);
  }
}

TEST(OtHelperTest, ShuffleWork) {
  const size_t num = 10000;

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto ot_sender = ot0.first;
    auto ot_receiver = ot0.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    auto perm = GenPerm(num);
    std::vector<internal::PTy> delta(num);
    helper.ShuffleSend(conn, absl::MakeSpan(perm), absl::MakeSpan(delta));
    return std::make_tuple(perm, delta);
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto ot_sender = ot1.first;
    auto ot_receiver = ot1.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num);
    std::vector<internal::PTy> b(num);
    helper.ShuffleRecv(conn, absl::MakeSpan(a), absl::MakeSpan(b));
    return std::make_tuple(a, b);
  });

  auto [perm, delta] = rank0.get();
  auto [a, b] = rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(delta[i] + a[perm[i]] + b[i], internal::PTy(0));
  }
}

TEST(OtHelperTest, RepeatShuffleWork) {
  const size_t num = 1000;
  const size_t repeat = 10;

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto ot_sender = ot0.first;
    auto ot_receiver = ot0.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    auto perm = GenPerm(num);
    std::vector<internal::PTy> delta(num * repeat);
    helper.ShuffleSend(conn, absl::MakeSpan(perm), absl::MakeSpan(delta),
                       repeat);
    return std::make_tuple(perm, delta);
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto ot_sender = ot1.first;
    auto ot_receiver = ot1.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<internal::PTy> a(num * repeat);
    std::vector<internal::PTy> b(num * repeat);
    helper.ShuffleRecv(conn, absl::MakeSpan(a), absl::MakeSpan(b), repeat);
    return std::make_tuple(a, b);
  });

  auto [perm, delta] = rank0.get();
  auto [a, b] = rank1.get();

  for (size_t _ = 0; _ < repeat; ++_) {
    const size_t offset = _ * num;
    for (size_t i = 0; i < num; ++i) {
      EXPECT_EQ(delta[offset + i] + a[offset + perm[i]] + b[offset + i],
                internal::PTy(0));
    }
  }
}

TEST(OtHelperTest, BatchShuffleWork) {
  const size_t total_num = 1 << 4;
  const size_t per_size = 1 << 10;
  const size_t repeat = 2;

  auto lctxs = SetupWorld(2);
  auto prev0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();

    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[0], false);
    otReceiver->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto prev1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();

    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[1], true);
    otSender->OneTimeSetup();

    return std::make_pair(otSender, otReceiver);
  });
  auto ot0 = prev0.get();
  auto ot1 = prev1.get();

  auto rank0 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[0]);
    auto ot_sender = ot0.first;
    auto ot_receiver = ot0.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<std::vector<size_t>> perms;
    for (size_t i = 0; i < total_num; ++i) {
      auto perm = GenPerm(per_size);
      perms.emplace_back(std::move(perm));
    }
    std::vector<std::vector<internal::PTy>> vec_delta;
    helper.BatchShuffleSend(conn, total_num, per_size, repeat, perms,
                            vec_delta);
    return std::make_tuple(perms, vec_delta);
  });
  auto rank1 = std::async([&] {
    auto conn = std::make_shared<Connection>(*lctxs[1]);
    auto ot_sender = ot1.first;
    auto ot_receiver = ot1.second;

    auto helper = OtHelper(ot_sender, ot_receiver);

    std::vector<std::vector<internal::PTy>> vec_a;
    std::vector<std::vector<internal::PTy>> vec_b;
    helper.BatchShuffleRecv(conn, total_num, per_size, repeat, vec_a, vec_b);
    return std::make_tuple(vec_a, vec_b);
  });

  auto [perms, vec_delta] = rank0.get();
  auto [vec_a, vec_b] = rank1.get();

  for (size_t k = 0; k < total_num; ++k) {
    auto& perm = perms[k];
    auto& delta = vec_delta[k];
    auto& a = vec_a[k];
    auto& b = vec_b[k];
    for (size_t i = 0; i < per_size; ++i) {
      EXPECT_EQ(delta[i] + a[perm[i]] + b[i], internal::PTy(0));
    }
  }
}

}  // namespace mosac::ot
