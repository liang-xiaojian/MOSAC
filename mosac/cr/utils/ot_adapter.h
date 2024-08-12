#pragma once

#include "yacl/base/dynamic_bitset.h"
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
      ss_ot_sender_ = std::make_unique<yc::SoftspokenOtExtSender>(2, 0, true);
    } else {
      ss_ot_receiver_ =
          std::make_unique<yc::SoftspokenOtExtReceiver>(2, 0, true);
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

  std::unique_ptr<yc::SoftspokenOtExtSender> ss_ot_sender_{nullptr};

  std::unique_ptr<yc::SoftspokenOtExtReceiver> ss_ot_receiver_{nullptr};
};

}  // namespace mosac::ot
