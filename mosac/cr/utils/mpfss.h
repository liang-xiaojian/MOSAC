#pragma once

#include "mosac/context/state.h"
#include "mosac/ss/type.h"
#include "mosac/utils/field.h"
#include "yacl/crypto/primitives/ot/ot_store.h"
#include "yacl/math/gadget.h"

namespace mosac::vole {

namespace ym = yacl::math;
namespace yc = yacl::crypto;

// Multi-Points Parameters, including MpCot, MpFss, MpVole
struct MpParam {
  size_t sp_vole_size_;
  size_t last_sp_vole_size_;

  size_t mp_vole_size_;
  size_t noise_num_;
  size_t require_ot_num_;
  std::vector<size_t> indexes_;

  MpParam(size_t mp_vole_size, size_t noise_num) {
    YACL_ENFORCE(mp_vole_size >= 2 * noise_num);
    mp_vole_size_ = mp_vole_size;
    noise_num_ = noise_num;
    //
    sp_vole_size_ = mp_vole_size_ / noise_num_;
    last_sp_vole_size_ = mp_vole_size_ - sp_vole_size_ * (noise_num_ - 1);
    require_ot_num_ = ym::Log2Ceil(sp_vole_size_) +
                      ym::Log2Ceil(last_sp_vole_size_) * (noise_num_ - 1);
  };

  // [Warning] indexes is not strictly uniform
  void GenIndexes() {
    indexes_ = yc::RandVec<size_t>(noise_num_);
    for (size_t i = 0; i < noise_num_ - 1; ++i) {
      indexes_[i] %= sp_vole_size_;
    }
    indexes_[noise_num_ - 1] %= last_sp_vole_size_;
  }

  // [Warning] it won't check whether the range of indexes is correct
  void SetIndexes(absl::Span<const size_t> indexes) {
    YACL_ENFORCE(indexes.size() >= noise_num_);
    for (size_t i = 0; i < noise_num_ - 1; ++i) {
      indexes_[i] = indexes[i] % sp_vole_size_;
    }
    indexes_[noise_num_ - 1] = indexes[noise_num_ - 1] % last_sp_vole_size_;
  }
};

// Multi-points Cot
void MpCotSend(const std::shared_ptr<Connection>& conn,
               const yc::OtSendStore& send_ot, const MpParam& param,
               absl::Span<uint128_t> output);

void MpCotSend(const std::shared_ptr<Connection>& conn,
               const yc::OtRecvStore& recv_ot, const MpParam& param,
               absl::Span<uint128_t> output);

// Multi-points Fss
void MpFssSend(const std::shared_ptr<Connection>& conn,
               const yc::OtSendStore& send_ot, const MpParam& param,
               absl::Span<internal::PTy> w, absl::Span<internal::PTy> output);

void MpFssRecv(const std::shared_ptr<Connection>& conn,
               const yc::OtRecvStore& recv_ot, const MpParam& param,
               absl::Span<internal::PTy> output);

// Multi-points Vole
void MpVoleSend(const std::shared_ptr<Connection>& conn,
                const yc::OtSendStore& send_ot, const MpParam& param,
                absl::Span<internal::PTy> w, absl::Span<internal::PTy> output);
void MpVoleRecv(const std::shared_ptr<Connection>& conn,
                const yc::OtRecvStore& recv_ot, const MpParam& param,
                absl::Span<internal::PTy> v, absl::Span<internal::PTy> output);

}  // namespace mosac::vole
