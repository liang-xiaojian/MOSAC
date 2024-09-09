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

// --------- Ferret ------------
void YaclFerretOtAdapter::OneTimeSetup() {
  if (is_setup_) {
    return;
  }

  uint128_t pre_lpn_num = FerretCotHelper(pre_lpn_param_, 0, true);

  if (is_sender_) {
    auto ss_sender = yc::SoftspokenOtExtSender();
    ss_sender.OneTimeSetup(ctx_);

    auto ss_send_blocks =
        yacl::AlignedVector<std::array<uint128_t, 2>>(pre_lpn_num, {0, 0});
    ss_sender.Send(ctx_, absl::MakeSpan(ss_send_blocks), true);

    auto ss_send_block0 = yacl::AlignedVector<uint128_t>(pre_lpn_num, 0);
    std::transform(
        ss_send_blocks.cbegin(), ss_send_blocks.cend(), ss_send_block0.begin(),
        [&one = std::as_const(one)](const std::array<uint128_t, 2>& blocks) {
          return blocks[0] & one;
        });

    Delta = ss_sender.GetDelta() | ~one;
    auto pre_ferret_sent_ot =
        yc::MakeCompactOtSendStore(std::move(ss_send_block0), Delta);

    // pre ferret OTe
    FerretOtExtSend_mal(
        ctx_, pre_ferret_sent_ot, pre_lpn_param_, pre_lpn_param_.n,
        absl::MakeSpan(ot_buff_.data<uint128_t>(), pre_lpn_param_.n));

  } else {
    auto ss_receiver = yc::SoftspokenOtExtReceiver();
    ss_receiver.OneTimeSetup(ctx_);

    auto ss_choices =
        yc::RandBits<yacl::dynamic_bitset<uint128_t>>(pre_lpn_num, true);
    auto ss_recv_blocks = yacl::AlignedVector<uint128_t>(pre_lpn_num, 0);

    ss_receiver.Recv(ctx_, ss_choices, absl::MakeSpan(ss_recv_blocks), true);

    for (uint64_t i = 0; i < pre_lpn_num; ++i) {
      ss_recv_blocks[i] = (ss_recv_blocks[i] & one) | ss_choices[i];
    }

    yc::OtRecvStore pre_ferret_recv_ot =
        yc::MakeCompactOtRecvStore(std::move(ss_recv_blocks));

    // pre ferret OTe
    FerretOtExtRecv_mal(
        ctx_, pre_ferret_recv_ot, pre_lpn_param_, pre_lpn_param_.n,
        absl::MakeSpan(ot_buff_.data<uint128_t>(), pre_lpn_param_.n));
  }

  is_setup_ = true;
  buff_used_num_ = reserve_num_;
  buff_upper_bound_ = pre_lpn_param_.n;
  // Delay boostrap
  // Bootstrap();
}

void YaclFerretOtAdapter::Bootstrap() {
  if (is_sender_) {
    yacl::AlignedVector<uint128_t> send_ot(
        ot_buff_.data<uint128_t>(), ot_buff_.data<uint128_t>() + reserve_num_);
    auto send_ot_store = yc::MakeCompactOtSendStore(std::move(send_ot), Delta);
    FerretOtExtSend_mal(
        ctx_, send_ot_store, lpn_param_, lpn_param_.n,
        absl::MakeSpan(ot_buff_.data<uint128_t>(), lpn_param_.n));
  } else {
    yacl::AlignedVector<uint128_t> recv_ot(
        ot_buff_.data<uint128_t>(), ot_buff_.data<uint128_t>() + reserve_num_);
    auto recv_ot_store = yc::MakeCompactOtRecvStore(std::move(recv_ot));
    FerretOtExtRecv_mal(
        ctx_, recv_ot_store, lpn_param_, lpn_param_.n,
        absl::MakeSpan(ot_buff_.data<uint128_t>(), lpn_param_.n));
  }
  // Notice that, we will reserve the some OT instances for boostrapping in
  // Ferret OTe protocol
  buff_used_num_ = reserve_num_;
  buff_upper_bound_ = lpn_param_.n;
}

void YaclFerretOtAdapter::BootstrapInplace(absl::Span<uint128_t> ot,
                                           absl::Span<uint128_t> data) {
  YACL_ENFORCE(ot.size() == reserve_num_);
  YACL_ENFORCE(data.size() == lpn_param_.n);

  yacl::AlignedVector<uint128_t> ot_tmp(ot.data(), ot.data() + reserve_num_);
  if (is_sender_) {
    auto send_ot_store = yc::MakeCompactOtSendStore(std::move(ot_tmp), Delta);
    FerretOtExtSend_mal(ctx_, send_ot_store, lpn_param_, lpn_param_.n, data);
  } else {
    auto recv_ot_store = yc::MakeCompactOtRecvStore(std::move(ot_tmp));
    FerretOtExtRecv_mal(ctx_, recv_ot_store, lpn_param_, lpn_param_.n, data);
  }
}

void YaclFerretOtAdapter::rcot(absl::Span<uint128_t> data) {
  if (is_setup_ == false) {
    OneTimeSetup();
  }

  uint64_t data_offset = 0;
  uint64_t require_num = data.size();
  uint64_t remain_num = buff_upper_bound_ - buff_used_num_;
  // When require_num is greater than lpn_param.n
  // call FerretOTe with data's subspan to avoid memory copy
  {
    uint32_t bootstrap_inplace_counter = 0;
    absl::Span<uint128_t> ot_span =
        absl::MakeSpan(ot_buff_.data<uint128_t>(), reserve_num_);
    while (require_num >= lpn_param_.n) {
      // avoid memory copy
      BootstrapInplace(ot_span, data.subspan(data_offset, lpn_param_.n));

      data_offset += (lpn_param_.n - reserve_num_);
      require_num -= (lpn_param_.n - reserve_num_);
      // Next Round
      ot_span = data.subspan(data_offset, reserve_num_);
    }
    if (bootstrap_inplace_counter != 0) {
      std::memcpy(reinterpret_cast<uint128_t*>(ot_buff_.data<uint128_t>()),
                  ot_span.data(), reserve_num_ * sizeof(uint128_t));
    }
  }

  uint64_t ot_num = std::min(remain_num, require_num);

  std::memcpy(data.data() + data_offset,
              ot_buff_.data<uint128_t>() + buff_used_num_,
              ot_num * sizeof(uint128_t));

  buff_used_num_ += ot_num;
  // add state
  data_offset += ot_num;

  // In the case of running out of ot_buff_
  if (ot_num < require_num) {
    require_num -= ot_num;
    Bootstrap();

    // Worst Case
    // Require_num is greater then "buff_upper_bound_ - reserve_num_"
    // which means that an extra "Bootstrap" is needed.
    if (require_num > (buff_upper_bound_ - reserve_num_)) {
      SPDLOG_WARN("[YACL] Worst Case!!! current require_num {}", require_num);
      // Bootstrap would reset buff_used_num_
      memcpy(data.data() + data_offset,
             ot_buff_.data<uint128_t>() + reserve_num_,
             (buff_upper_bound_ - reserve_num_) * sizeof(uint128_t));
      require_num -= (buff_upper_bound_ - reserve_num_);
      data_offset += (buff_upper_bound_ - reserve_num_);

      // Bootstrap would reset buff_used_num_
      Bootstrap();
    }
    memcpy(data.data() + data_offset,
           ot_buff_.data<uint128_t>() + buff_used_num_,
           require_num * sizeof(uint128_t));
    buff_used_num_ += require_num;
  }
}

void YaclFerretOtAdapter::recv_cot(
    absl::Span<uint128_t> data,
    const yacl::dynamic_bitset<uint128_t>& choices) {
  YACL_ENFORCE(is_sender_ == false);
  uint64_t num = data.size();

  rcot(data);

  // Warning: low efficiency
  auto flip_choices = choices;
  for (uint64_t i = 0; i < num; ++i) {
    flip_choices[i] = choices[i] ^ (data[i] & 0x1);
  }

  auto bv =
      yacl::ByteContainerView(reinterpret_cast<uint8_t*>(flip_choices.data()),
                              flip_choices.num_blocks() * sizeof(uint128_t));

  ctx_->SendAsync(ctx_->NextRank(), bv, "ferret_recv_cot:flip");
}

void YaclFerretOtAdapter::send_cot(absl::Span<uint128_t> data) {
  YACL_ENFORCE(is_sender_ == true);
  uint64_t num = data.size();

  rcot(data);

  auto bv = ctx_->Recv(ctx_->NextRank(), "ferret_send_cot:flip");
  auto flip_choices_span = absl::MakeSpan(
      reinterpret_cast<uint128_t*>(bv.data()), bv.size() / sizeof(uint128_t));

  yacl::dynamic_bitset<uint128_t> choices;
  choices.append(flip_choices_span.data(),
                 flip_choices_span.data() + flip_choices_span.size());

  for (uint64_t i = 0; i < num; ++i) {
    data[i] = data[i] ^ (choices[i] * Delta);
  }
}

};  // namespace mosac::ot
