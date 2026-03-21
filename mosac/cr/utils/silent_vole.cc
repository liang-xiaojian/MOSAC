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


#include "mosac/cr/utils/silent_vole.h"

#include <algorithm>
#include <memory>

#include "mosac/cr/utils/ea_code.h"
#include "mosac/cr/utils/mpfss.h"
#include "yacl/base/aligned_vector.h"
#include "yacl/base/dynamic_bitset.h"
#include "yacl/base/int128.h"

namespace mosac::vole {

namespace {

uint64_t inline GenRegNoiseWeight(double min_dist_ratio, uint64_t sec) {
  if (min_dist_ratio > 0.5 || min_dist_ratio <= 0) {
    YACL_THROW("mini distance too small, rate {}", min_dist_ratio);
  }

  auto d = std::log2(1 - 2 * min_dist_ratio);
  auto t = std::max<uint64_t>(128, -double(sec) / d);

  return yacl::math::RoundUpTo(t, 8);
}

//  minimum distance for dual-LPN code
static std::map<CodeType, double> kMinDistanceRatio = {
    {CodeType::ExAcc7, 0.05},
    {CodeType::ExAcc11, 0.1},
    {CodeType::ExAcc21, 0.1},
    {CodeType::ExAcc40, 0.2}};

uint64_t GetNoiseNum(CodeType code) {
  double min_dist_ratio = kMinDistanceRatio[code];
  return GenRegNoiseWeight(min_dist_ratio, 128);
}

struct VoleParam {
  bool is_mal_{true};
  uint64_t vole_num_;   // vole num
  uint64_t code_size_;  // code size

  CodeType codetype_;        // code type, e.g. silver5
  MpParam mp_param_;         // mp vole parameter
  uint64_t mp_vole_ot_num_;  // mp vole (cot/rot-based)

  // Constructor
  VoleParam(CodeType code, uint64_t vole_num) {
    codetype_ = code;
    vole_num_ = vole_num;

    // default
    uint64_t code_scaler = 2;
    // check
    YACL_ENFORCE(
        kMinDistanceRatio.count(code),
        "Error: could not found the minimum distance for current code.");
    double min_dist_ratio = kMinDistanceRatio[code];

    auto noise_num = GenRegNoiseWeight(min_dist_ratio, 128);
    // Note that: the size of SpVole must be greater than one.
    // because 1-out-of-1 Vole/OT is meaningless
    auto sp_vole_size =
        std::max(yacl::math::DivCeil(vole_num * code_scaler, noise_num),
                 static_cast<uint64_t>(2));
    auto mp_vole_size = sp_vole_size * noise_num;

    // initialize parameters for MpVole
    mp_param_ = MpParam(mp_vole_size, noise_num);

    code_size_ = mp_param_.mp_vole_size_ / code_scaler;
    // base_vole + mp_vole
    mp_vole_ot_num_ = mp_param_.require_ot_num_;  // mp_vole (cot/rot-based)
  }
};

// Get Dual LPN Encoder, e.g., ExAccCode
std::shared_ptr<code::LinearCodeInterface> GetEncoder(const VoleParam& param) {
  std::shared_ptr<code::LinearCodeInterface> encoder{nullptr};

  const auto codetype = param.codetype_;
  //   const auto code_size = param.code_size_;
  const auto vole_num = param.vole_num_;
  const auto mp_vole_size = param.mp_param_.mp_vole_size_;

  switch (codetype) {
    // the size of ExAccCode is ( code_size * 2, vole_num  )
    case CodeType::ExAcc7:
      encoder = std::make_shared<code::ExAccCode<7>>(vole_num, mp_vole_size);
      break;
    case CodeType::ExAcc11:
      encoder = std::make_shared<code::ExAccCode<11>>(vole_num, mp_vole_size);
      break;
    case CodeType::ExAcc21:
      encoder = std::make_shared<code::ExAccCode<21>>(vole_num, mp_vole_size);
      break;
    case CodeType::ExAcc40:
      encoder = std::make_shared<code::ExAccCode<40>>(vole_num, mp_vole_size);
      break;
    default:
      YACL_ENFORCE(true, "Silent VOLE support ExAcc Code Only");
      break;
  }
  return encoder;
}

void DualLpnEncode(const VoleParam& param, absl::Span<internal::PTy> in,
                   absl::Span<internal::PTy> out) {
  auto encoder = GetEncoder(param);
  if (std::dynamic_pointer_cast<code::ExAccCodeInterface>(encoder)) {
    std::dynamic_pointer_cast<code::ExAccCodeInterface>(encoder)->DualEncode(
        in, out);
  } else {
    YACL_THROW("Did not implement");
  }
}

void DualLpnEncode2(const VoleParam& param, absl::Span<internal::PTy> in0,
                    absl::Span<internal::PTy> out0,
                    absl::Span<internal::PTy> in1,
                    absl::Span<internal::PTy> out1) {
  auto encoder = GetEncoder(param);
  if (std::dynamic_pointer_cast<code::ExAccCodeInterface>(encoder)) {
    std::dynamic_pointer_cast<code::ExAccCodeInterface>(encoder)->DualEncode2(
        in0, out0, in1, out1);
  } else {
    YACL_THROW("Did not implement");
  }
}

// consistency check tools
inline internal::PTy UniversalHash(internal::PTy seed,
                                   absl::Span<const internal::PTy> in) {
  internal::PTy result(0);
  std::for_each(in.rbegin(), in.rend(),
                [&result, &seed](const internal::PTy& val) {
                  result = (result + val) * seed;
                });
  return result;
}

inline std::vector<internal::PTy> ExtractCeof(
    internal::PTy seed, absl::Span<const size_t> indexes) {
  auto max_index = indexes.back();
  auto bits = yacl::math::Log2Ceil(max_index + 1);

  std::array<internal::PTy, 64> buf;
  buf[0] = seed;
  for (size_t i = 1; i < 64 && i <= bits; ++i) {
    buf[i] = buf[i - 1] * buf[i - 1];
  }

  std::vector<internal::PTy> ceof;
  for (const auto& index : indexes) {
    auto index_plus_one = index + 1;
    size_t mask = 1;

    internal::PTy tmp(1);
    for (size_t i = 0; i < 64 && mask <= index_plus_one; ++i) {
      if (mask & index_plus_one) {
        tmp = tmp * buf[i];
      }
      mask <<= 1;
    }
    ceof.emplace_back(tmp);
  }

  return ceof;
}

}  // namespace

void SilentVoleSender::OneTimeSetup(std::shared_ptr<Connection>& conn) {
  if (is_inited_ == true) {
    return;
  }
  auto noise_num = GetNoiseNum(codetype_);
  auto ot_helper = ot::OtHelper(ot_ptr_, nullptr);
  pre_c_ = std::vector<internal::PTy>(noise_num, 0);

  ot_helper.BaseVoleSend(conn, delta_, absl::MakeSpan(pre_c_));
  is_inited_ = true;
}

void SilentVoleSender::Send(std::shared_ptr<Connection>& conn,
                            absl::Span<internal::PTy> c) {
  if (!is_inited_) {
    OneTimeSetup(conn);
  }

  const auto vole_num = c.size();
  auto param = VoleParam(codetype_, vole_num);
  auto& mp_param = param.mp_param_;

  // [Warning] copy, low efficiency
  auto mp_vole_cot = param.mp_vole_ot_num_;
  std::vector<uint128_t> send_msgs(mp_vole_cot);
  ot_ptr_->send_rcot(absl::MakeSpan(send_msgs));
  auto send_store =
      yc::MakeCompactOtSendStore(std::move(send_msgs), ot_ptr_->GetDelta());
  // mp_vole output
  // UninitAlignedVector<K> mp_vole_output(mp_param.mp_vole_size_);
  auto buf = yacl::Buffer(mp_param.mp_vole_size_ * sizeof(internal::PTy));
  auto mp_vole_output =
      absl::MakeSpan(buf.data<internal::PTy>(), mp_param.mp_vole_size_);
  MpVoleSend(conn, send_store, mp_param, absl::MakeSpan(pre_c_),
             absl::MakeSpan(mp_vole_output));

  // ---- consistency check ----
  if (param.is_mal_) {
    auto seed = conn->SyncSeed();
    auto uhash =
        UniversalHash(seed, mp_vole_output.subspan(0, mp_param.mp_vole_size_));
    auto buf = conn->Recv(conn->NextRank(), "MalVole");
    YACL_ENFORCE(buf.size() == sizeof(internal::PTy));
    internal::PTy diff;
    memcpy(&diff, buf.data(), buf.size());

    uhash = uhash - delta_ * diff + pre_c_.back();

    auto hash =
        yacl::crypto::Sm3(yacl::ByteContainerView(&uhash, sizeof(uhash)));
    conn->SendAsync(conn->NextRank(), yacl::ByteContainerView(hash),
                    "MalVoleHash");
  }
  // ---- consistency check ----

  // dual LPN
  // compressing mp_vole_output into c
  DualLpnEncode(param, mp_vole_output, c);
}

void SilentVoleReceiver::OneTimeSetup(std::shared_ptr<Connection>& conn) {
  if (is_inited_ == true) {
    return;
  }
  auto noise_num = GetNoiseNum(codetype_);
  auto ot_helper = ot::OtHelper(nullptr, ot_ptr_);

  pre_a_ = std::vector<internal::PTy>(noise_num, 0);
  pre_b_ = std::vector<internal::PTy>(noise_num, 0);

  ot_helper.BaseVoleRecv(conn, absl::MakeSpan(pre_a_), absl::MakeSpan(pre_b_));
  is_inited_ = true;
}

void SilentVoleReceiver::Recv(std::shared_ptr<Connection>& conn,
                              absl::Span<internal::PTy> a,
                              absl::Span<internal::PTy> b) {
  if (!is_inited_) {
    OneTimeSetup(conn);
  }

  const auto vole_num = a.size();
  YACL_ENFORCE(vole_num == b.size());
  auto param = VoleParam(codetype_, vole_num);
  auto& mp_param = param.mp_param_;
  auto mp_vole_cot = param.mp_vole_ot_num_;

  std::vector<uint128_t> recv_msgs(mp_vole_cot);
  yacl::dynamic_bitset<uint128_t> choices(mp_vole_cot);
  ot_ptr_->recv_rcot(absl::MakeSpan(recv_msgs), choices);
  auto recv_store = yc::MakeOtRecvStore(choices, std::move(recv_msgs));

  // generate punctured indexes for MpVole
  mp_param.GenIndexes();

  // sparse_noise && mp_vole output
  std::vector<internal::PTy> sparse_noise(mp_param.mp_vole_size_, 0);
  // UninitAlignedVector<K> mp_vole_output(mp_param.mp_vole_size_);
  auto buf = yacl::Buffer(mp_param.mp_vole_size_ * sizeof(internal::PTy));
  auto mp_vole_output =
      absl::MakeSpan(buf.data<internal::PTy>(), mp_param.mp_vole_size_);
  // generate the choices of base VOLE
  std::vector<size_t> indexes;
  for (size_t i = 0; i < mp_param.noise_num_; ++i) {
    size_t tmp = i * mp_param.sp_vole_size_ + mp_param.indexes_[i];
    sparse_noise[tmp] = pre_a_[i];
    indexes.emplace_back(tmp);
  }
  MpVoleRecv(conn, recv_store, mp_param, absl::MakeSpan(pre_b_),
             absl::MakeSpan(mp_vole_output));

  // ---- consistency check ----
  if (param.is_mal_) {
    auto seed = conn->SyncSeed();
    auto uhash =
        UniversalHash(seed, mp_vole_output.subspan(0, mp_param.mp_vole_size_));
    auto coef = ExtractCeof(seed, absl::MakeConstSpan(indexes));
    auto diff =
        internal::op::InPro(absl::MakeConstSpan(coef),
                            absl::MakeSpan(pre_a_).subspan(0, coef.size()));
    diff = diff + pre_a_.back();
    uhash = uhash + pre_b_.back();
    conn->Send(conn->NextRank(), yacl::ByteContainerView(&diff, sizeof(diff)),
               "MalVole");
    auto hash =
        yacl::crypto::Sm3(yacl::ByteContainerView(&uhash, sizeof(uhash)));
    auto remote_hash = conn->Recv(conn->NextRank(), "MalVoleHash");
    YACL_ENFORCE(yacl::ByteContainerView(hash) ==
                 yacl::ByteContainerView(remote_hash));
  }
  // ---- consistency check ----

  // dual LPN
  // compressing sparse_noise into a, mp_vole_output into b
  DualLpnEncode2(param, absl::MakeSpan(sparse_noise), absl::MakeSpan(a),
                 absl::MakeSpan(mp_vole_output), absl::MakeSpan(b));
}

}  // namespace mosac::vole
