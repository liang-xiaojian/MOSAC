#include "mosac/cr/utils/ot_adapter.h"

namespace mosac::ot {
// ----------- KOS ---------------
void YaclKosOtAdapter::OneTimeSetup() {
  if (is_setup_) {
    return;
  }

  // Sender
  if (is_sender_) {
    auto choices = yc::RandBits<yacl::dynamic_bitset<uint128_t>>(128, true);
    // Generate BaseOT for Kos-OTe
    auto base_ot = yc::BaseOtRecv(ctx_, choices, 128);
    recv_ot_ptr_ = std::make_unique<yc::OtRecvStore>(std::move(base_ot));
    Delta = choices.data()[0];
  }
  // Receiver
  else {
    // Generate BaseOT for Kos-OTe
    auto base_ot = yc::BaseOtSend(ctx_, 128);
    // Random choices for Kos-OTe
    send_ot_ptr_ = std::make_unique<yc::OtSendStore>(std::move(base_ot));
  }

  is_setup_ = true;
}

void YaclKosOtAdapter::send_cot(absl::Span<uint128_t> data) {
  YACL_ENFORCE(is_sender_);
  // [Warning] copy, low efficiency
  std::vector<std::array<uint128_t, 2>> send_blocks(data.size());
  yc::KosOtExtSend(ctx_, *recv_ot_ptr_, absl::MakeSpan(send_blocks), true);
  for (uint64_t i = 0; i < data.size(); ++i) {
    data[i] = send_blocks[i][0];
  }
}

void YaclKosOtAdapter::recv_cot(
    absl::Span<uint128_t> data,
    const yacl::dynamic_bitset<uint128_t>& choices) {
  YACL_ENFORCE(is_sender_ == false);
  yc::KosOtExtRecv(ctx_, *send_ot_ptr_, choices, absl::MakeSpan(data), true);
}

void YaclKosOtAdapter::send_rot(absl::Span<std::array<uint128_t, 2>> data) {
  YACL_ENFORCE(is_sender_);
  yc::KosOtExtSend(ctx_, *recv_ot_ptr_, absl::MakeSpan(data), false);
}

void YaclKosOtAdapter::recv_rot(
    absl::Span<uint128_t> data,
    const yacl::dynamic_bitset<uint128_t>& choices) {
  YACL_ENFORCE(is_sender_ == false);
  yc::KosOtExtRecv(ctx_, *send_ot_ptr_, choices, absl::MakeSpan(data), false);
}

// --------- Softspoken ------------
void YaclSsOtAdapter::OneTimeSetup() {
  if (is_setup_) {
    return;
  }

  if (is_sender_) {
    ss_ot_sender_->OneTimeSetup(ctx_);
    Delta = ss_ot_sender_->GetDelta();
  } else {
    ss_ot_receiver_->OneTimeSetup(ctx_);
  }

  is_setup_ = true;
}

void YaclSsOtAdapter::send_cot(absl::Span<uint128_t> data) {
  YACL_ENFORCE(is_sender_);
  // [Warning] copy, low efficiency
  std::vector<std::array<uint128_t, 2>> send_blocks(data.size());
  ss_ot_sender_->Send(ctx_, absl::MakeSpan(send_blocks), true);
  for (uint64_t i = 0; i < data.size(); ++i) {
    data[i] = send_blocks[i][0];
  }
}

void YaclSsOtAdapter::recv_cot(absl::Span<uint128_t> data,
                               const yacl::dynamic_bitset<uint128_t>& choices) {
  YACL_ENFORCE(is_sender_ == false);
  ss_ot_receiver_->Recv(ctx_, choices, absl::MakeSpan(data), true);
}

void YaclSsOtAdapter::send_rot(absl::Span<std::array<uint128_t, 2>> data) {
  YACL_ENFORCE(is_sender_);
  ss_ot_sender_->Send(ctx_, absl::MakeSpan(data), false);
}

void YaclSsOtAdapter::recv_rot(absl::Span<uint128_t> data,
                               const yacl::dynamic_bitset<uint128_t>& choices) {
  YACL_ENFORCE(is_sender_ == false);
  ss_ot_receiver_->Recv(ctx_, choices, absl::MakeSpan(data), false);
}

};  // namespace mosac::ot
