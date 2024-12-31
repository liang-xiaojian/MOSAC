#pragma once

#include "mosac/cr/utils/ferret_ot.h"
#include "yacl/base/buffer.h"
#include "yacl/base/dynamic_bitset.h"
#include "yacl/crypto/base/hash/hash_utils.h"
#include "yacl/crypto/primitives/ot/base_ot.h"
#include "yacl/crypto/primitives/ot/kos_ote.h"
#include "yacl/crypto/primitives/ot/ot_store.h"
#include "yacl/crypto/primitives/ot/softspoken_ote.h"
#include "yacl/crypto/utils/rand.h"

namespace mosac::ot {

namespace yc = yacl::crypto;
namespace yl = yacl::link;

class OtAdapter {
 public:
  OtAdapter() = default;
  virtual ~OtAdapter() = default;

  virtual void send_rcot(absl::Span<uint128_t> data) = 0;
  virtual void recv_rcot(absl::Span<uint128_t> data,
                         yacl::dynamic_bitset<uint128_t>& choices) = 0;
  virtual void send_cot(absl::Span<uint128_t> data) = 0;
  virtual void recv_cot(absl::Span<uint128_t> data,
                        const yacl::dynamic_bitset<uint128_t>& choices) = 0;

  virtual void send_rrot(absl::Span<std::array<uint128_t, 2>> data) = 0;
  virtual void recv_rrot(absl::Span<uint128_t> data,
                         yacl::dynamic_bitset<uint128_t>& choices) = 0;
  virtual void send_rot(absl::Span<std::array<uint128_t, 2>> data) = 0;
  virtual void recv_rot(absl::Span<uint128_t> data,
                        const yacl::dynamic_bitset<uint128_t>& choices) = 0;

  virtual void OneTimeSetup() = 0;

  uint128_t Delta{0};
  virtual uint128_t GetDelta() const { return Delta; }
  virtual bool IsSender() const = 0;
};

class YaclKosOtAdapter : public OtAdapter {
 public:
  YaclKosOtAdapter(const std::shared_ptr<yl::Context> ctx, bool is_sender) {
    ctx_ = ctx;
    is_sender_ = is_sender;
  }

  ~YaclKosOtAdapter() {}

  void OneTimeSetup() override;

  inline void send_rcot(absl::Span<uint128_t> data) override { send_cot(data); }

  inline void send_rrot(absl::Span<std::array<uint128_t, 2>> data) override {
    send_rot(data);
  }

  inline void recv_rcot(absl::Span<uint128_t> data,
                        yacl::dynamic_bitset<uint128_t>& choices) override {
    choices = yc::RandBits<yacl::dynamic_bitset<uint128_t>>(data.size(), true);
    recv_cot(data, choices);
  }

  inline void recv_rrot(absl::Span<uint128_t> data,
                        yacl::dynamic_bitset<uint128_t>& choices) override {
    choices = yc::RandBits<yacl::dynamic_bitset<uint128_t>>(data.size(), true);
    recv_rot(data, choices);
  }

  // KOS ENTRY
  void send_cot(absl::Span<uint128_t> data) override;

  void recv_cot(absl::Span<uint128_t> data,
                const yacl::dynamic_bitset<uint128_t>& choices) override;

  void send_rot(absl::Span<std::array<uint128_t, 2>> data) override;

  void recv_rot(absl::Span<uint128_t> data,
                const yacl::dynamic_bitset<uint128_t>& choices) override;

  uint128_t GetDelta() const override { return Delta; }

  bool IsSender() const override { return is_sender_; }

 private:
  std::shared_ptr<yl::Context> ctx_{nullptr};

  bool is_sender_{false};

  bool is_setup_{false};

  std::unique_ptr<yc::OtSendStore> send_ot_ptr_{nullptr};

  std::unique_ptr<yc::OtRecvStore> recv_ot_ptr_{nullptr};
};

class YaclSsOtAdapter : public OtAdapter {
 public:
  YaclSsOtAdapter(const std::shared_ptr<yl::Context> ctx, bool is_sender) {
    ctx_ = ctx;
    is_sender_ = is_sender;
    if (is_sender) {
      ss_ot_sender_ =
          std::make_unique<yc::SoftspokenOtExtSender>(2, 1024, true);
    } else {
      ss_ot_receiver_ =
          std::make_unique<yc::SoftspokenOtExtReceiver>(2, 1024, true);
    }
  }

  ~YaclSsOtAdapter() {}

  void OneTimeSetup() override;

  inline void send_rcot(absl::Span<uint128_t> data) override { send_cot(data); }

  inline void send_rrot(absl::Span<std::array<uint128_t, 2>> data) override {
    send_rot(data);
  }

  inline void recv_rcot(absl::Span<uint128_t> data,
                        yacl::dynamic_bitset<uint128_t>& choices) override {
    choices = yc::RandBits<yacl::dynamic_bitset<uint128_t>>(data.size(), true);
    recv_cot(data, choices);
  }

  inline void recv_rrot(absl::Span<uint128_t> data,
                        yacl::dynamic_bitset<uint128_t>& choices) override {
    choices = yc::RandBits<yacl::dynamic_bitset<uint128_t>>(data.size(), true);
    recv_rot(data, choices);
  }

  // SoftspokenOTe ENTRY
  void send_cot(absl::Span<uint128_t> data) override;

  void recv_cot(absl::Span<uint128_t> data,
                const yacl::dynamic_bitset<uint128_t>& choices) override;

  void send_rot(absl::Span<std::array<uint128_t, 2>> data) override;

  void recv_rot(absl::Span<uint128_t> data,
                const yacl::dynamic_bitset<uint128_t>& choices) override;

  uint128_t GetDelta() const override { return Delta; }

  bool IsSender() const override { return is_sender_; }

 private:
  void rcot(absl::Span<uint128_t> data);

  std::shared_ptr<yl::Context> ctx_{nullptr};

  bool is_sender_{false};

  bool is_setup_{false};

  std::unique_ptr<yc::SoftspokenOtExtSender> ss_ot_sender_{nullptr};

  std::unique_ptr<yc::SoftspokenOtExtReceiver> ss_ot_receiver_{nullptr};
};

class YaclFerretOtAdapter : public OtAdapter {
 public:
  YaclFerretOtAdapter(const std::shared_ptr<yl::Context> ctx, bool is_sender) {
    ctx_ = ctx;
    is_sender_ = is_sender;
    is_setup_ = false;

    reserve_num_ = FerretCotHelper(lpn_param_, 0, true);
    ot_buff_ = yacl::Buffer(lpn_param_.n * sizeof(uint128_t));
  }

  ~YaclFerretOtAdapter() {}

  void OneTimeSetup() override;

  // Ferret OTe ENTRY
  inline void send_rcot(absl::Span<uint128_t> data) override { rcot(data); }
  inline void recv_rcot(absl::Span<uint128_t> data,
                        yacl::dynamic_bitset<uint128_t>& choices) override {
    const size_t num = data.size();

    rcot(data);
    choices = yacl::dynamic_bitset<uint128_t>(num);
    for (size_t i = 0; i < num; ++i) {
      choices[i] = data[i] & 0x1;
    }
  }

  inline void send_rrot(absl::Span<std::array<uint128_t, 2>> data) override {
    const size_t num = data.size();
    const auto cur_delta = Delta;

    std::vector<uint128_t> cot_data(num);
    send_rcot(absl::MakeSpan(cot_data));
    std::transform(cot_data.begin(), cot_data.end(), data.begin(),
                   [&cur_delta](uint128_t val) -> std::array<uint128_t, 2> {
                     return {val, val ^ cur_delta};
                   });

    yc::ParaCrHashInplace_128(
        absl::MakeSpan(reinterpret_cast<uint128_t*>(data.data()), num * 2));
  }

  inline void recv_rrot(absl::Span<uint128_t> data,
                        yacl::dynamic_bitset<uint128_t>& choices) override {
    recv_rcot(data, choices);
    yc::ParaCrHashInplace_128(data);
  }

  void send_cot(absl::Span<uint128_t> data) override;
  void recv_cot(absl::Span<uint128_t> data,
                const yacl::dynamic_bitset<uint128_t>& choices) override;

  inline void send_rot(absl::Span<std::array<uint128_t, 2>> data) override {
    const size_t num = data.size();
    const auto cur_delta = Delta;

    std::vector<uint128_t> cot_data(num);
    send_cot(absl::MakeSpan(cot_data));
    std::transform(cot_data.begin(), cot_data.end(), data.begin(),
                   [&cur_delta](uint128_t val) -> std::array<uint128_t, 2> {
                     return {val, val ^ cur_delta};
                   });

    yc::ParaCrHashInplace_128(
        absl::MakeSpan(reinterpret_cast<uint128_t*>(data.data()), num * 2));
  }

  inline void recv_rot(
      absl::Span<uint128_t> data,
      const yacl::dynamic_bitset<uint128_t>& choices) override {
    recv_cot(data, choices);
    yc::ParaCrHashInplace_128(data);
  }

  uint128_t GetDelta() const override { return Delta; }

  bool IsSender() const override { return is_sender_; }

 private:
  std::shared_ptr<yl::Context> ctx_{nullptr};

  bool is_sender_{false};

  bool is_setup_{false};

  yacl::Buffer ot_buff_;

  void rcot(absl::Span<uint128_t> data);
  // Internal bootstrapping
  void Bootstrap();
  void BootstrapInplace(absl::Span<uint128_t> ot, absl::Span<uint128_t> data);

  // ferret states
  uint64_t reserve_num_{0};
  uint64_t buff_used_num_{0};
  uint64_t buff_upper_bound_{0};

  yc::LpnParam pre_lpn_param_{470016, 32768, 918,
                              yc::LpnNoiseAsm::RegularNoise};

  yc::LpnParam lpn_param_{10485760, 452000, 1280,
                          yc::LpnNoiseAsm::RegularNoise};

  static constexpr uint128_t one =
      yacl::MakeUint128(0xffffffffffffffff, 0xfffffffffffffffe);
};

}  // namespace mosac::ot
