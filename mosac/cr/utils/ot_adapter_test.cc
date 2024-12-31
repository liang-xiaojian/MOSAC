#include "mosac/cr/utils/ot_adapter.h"

#include "gtest/gtest.h"
#include "mosac/utils/test_util.h"
#include "yacl/base/dynamic_bitset.h"

namespace mosac::ot {

TEST(KosOtAdapterTest, ROT) {
  size_t num = 640000 * 4;
  std::vector<uint128_t> recv_data(num);
  std::vector<std::array<uint128_t, 2>> send_data(num);
  yacl::dynamic_bitset<uint128_t> choices(num);
  auto lctxs = SetupWorld(2);
  auto rank0 = std::async([&] {
    auto otSender = std::make_shared<YaclKosOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();
    otSender->send_rrot(absl::MakeSpan(send_data));
  });
  auto rank1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclKosOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();
    otReceiver->recv_rrot(absl::MakeSpan(recv_data), choices);
  });
  rank0.get();
  rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(recv_data[i], send_data[i][choices[i]]);
    EXPECT_NE(recv_data[i], send_data[i][1 - choices[i]]);
  }
};

TEST(KosOtAdapterTest, COT) {
  size_t num = 640000 * 4;
  std::vector<uint128_t> recv_data(num);
  std::vector<uint128_t> send_data(num);
  yacl::dynamic_bitset<uint128_t> choices(num);

  auto lctxs = SetupWorld(2);
  auto rank0 = std::async([&] {
    auto otSender = std::make_shared<YaclKosOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();
    otSender->send_rcot(absl::MakeSpan(send_data));
    return otSender->GetDelta();
  });
  auto rank1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclKosOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();
    otReceiver->recv_rcot(absl::MakeSpan(recv_data), choices);
  });
  auto delta = rank0.get();
  rank1.get();

  for (size_t i = 0; i < num; ++i) {
    if (choices[i] == 0) {
      EXPECT_EQ(send_data[i] ^ recv_data[i], uint128_t(0));
    } else {
      EXPECT_EQ(send_data[i] ^ recv_data[i], delta);
    }
  }
};

TEST(SsOtAdapterTest, ROT) {
  size_t num = 640000 * 4;
  std::vector<uint128_t> recv_data(num);
  std::vector<std::array<uint128_t, 2>> send_data(num);
  yacl::dynamic_bitset<uint128_t> choices(num);
  auto lctxs = SetupWorld(2);
  auto rank0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();
    otSender->send_rrot(absl::MakeSpan(send_data));
  });
  auto rank1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();
    otReceiver->recv_rrot(absl::MakeSpan(recv_data), choices);
  });
  rank0.get();
  rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(recv_data[i], send_data[i][choices[i]]);
    EXPECT_NE(recv_data[i], send_data[i][1 - choices[i]]);
  }
};

TEST(SsOtAdapterTest, COT) {
  size_t num = 640000 * 4;
  std::vector<uint128_t> recv_data(num);
  std::vector<uint128_t> send_data(num);
  yacl::dynamic_bitset<uint128_t> choices(num);

  auto lctxs = SetupWorld(2);
  auto rank0 = std::async([&] {
    auto otSender = std::make_shared<YaclSsOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();
    otSender->send_rcot(absl::MakeSpan(send_data));
    return otSender->GetDelta();
  });
  auto rank1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclSsOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();
    otReceiver->recv_rcot(absl::MakeSpan(recv_data), choices);
  });
  auto delta = rank0.get();
  rank1.get();

  for (size_t i = 0; i < num; ++i) {
    if (choices[i] == 0) {
      EXPECT_EQ(send_data[i] ^ recv_data[i], uint128_t(0));
    } else {
      EXPECT_EQ(send_data[i] ^ recv_data[i], delta);
    }
  }
};

TEST(FerretOtAdapterTest, ROT) {
  size_t num = 640000 * 4;
  std::vector<uint128_t> recv_data(num);
  std::vector<std::array<uint128_t, 2>> send_data(num);
  yacl::dynamic_bitset<uint128_t> choices =
      yc::RandBits<yacl::dynamic_bitset<uint128_t>>(num, false);
  auto lctxs = SetupWorld(2);
  auto rank0 = std::async([&] {
    auto otSender = std::make_shared<YaclFerretOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();
    otSender->send_rot(absl::MakeSpan(send_data));
  });
  auto rank1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclFerretOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();
    otReceiver->recv_rot(absl::MakeSpan(recv_data), choices);
  });
  rank0.get();
  rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(recv_data[i], send_data[i][choices[i]]);
    EXPECT_NE(recv_data[i], send_data[i][1 - choices[i]]);
  }
};

TEST(FerretOtAdapterTest, COT) {
  size_t num = 640000 * 4;
  std::vector<uint128_t> recv_data(num);
  std::vector<uint128_t> send_data(num);
  yacl::dynamic_bitset<uint128_t> choices =
      yc::RandBits<yacl::dynamic_bitset<uint128_t>>(num, false);

  auto lctxs = SetupWorld(2);
  auto rank0 = std::async([&] {
    auto otSender = std::make_shared<YaclFerretOtAdapter>(lctxs[0], true);
    otSender->OneTimeSetup();
    otSender->send_cot(absl::MakeSpan(send_data));
    return otSender->GetDelta();
  });
  auto rank1 = std::async([&] {
    auto otReceiver = std::make_shared<YaclFerretOtAdapter>(lctxs[1], false);
    otReceiver->OneTimeSetup();
    otReceiver->recv_cot(absl::MakeSpan(recv_data), choices);
  });
  auto delta = rank0.get();
  rank1.get();

  for (size_t i = 0; i < num; ++i) {
    if (choices[i] == 0) {
      EXPECT_EQ(send_data[i] ^ recv_data[i], uint128_t(0));
    } else {
      EXPECT_EQ(send_data[i] ^ recv_data[i], delta);
    }
  }
};

}  // namespace mosac::ot