#pragma once
#include "mosac/context/state.h"
#include "mosac/cr/utils/mpfss.h"
#include "mosac/ss/type.h"
#include "mosac/utils/field.h"
#include "yacl/crypto/primitives/ot/ot_store.h"
#include "yacl/math/gadget.h"

namespace mosac::vole {

namespace ym = yacl::math;
namespace yc = yacl::crypto;

struct LpnParam {
  size_t n_ = 10485760;
  size_t k_ = 452000;
  size_t t_ = 1280;

  LpnParam() : LpnParam(10485760, 452000, 1280) {}
  LpnParam(size_t n, size_t k, size_t t) : n_(n), k_(k), t_(t) {}

  static LpnParam GetDefault() { return LpnParam(10485760, 452000, 1280); }
  static LpnParam GetPreDefault() { return LpnParam(470016, 32768, 918); }
};

struct VoleParam {
  LpnParam lpn_param_ = LpnParam::GetDefault();
  MpParam mp_param_{10485760, 1280};

  size_t base_vole_num_{0};
  size_t base_vole_ot_num_{0};  // OT for constructing base-Vole
  size_t mp_vole_ot_num_{0};    // OT for constructing mpfss
  size_t require_ot_num_{0};    // total OT number

  size_t vole_num_{0};  // Output size

  bool is_mal_ = false;

  VoleParam() : VoleParam(LpnParam::GetDefault()) {}

  VoleParam(LpnParam lpn_param, bool mal = false) {
    lpn_param_ = lpn_param;
    mp_param_ = MpParam(lpn_param.n_, lpn_param.t_);

    base_vole_num_ = lpn_param_.k_;
    base_vole_ot_num_ = lpn_param_.k_ * sizeof(internal::PTy);
    mp_vole_ot_num_ = mp_param_.require_ot_num_;
    require_ot_num_ = base_vole_num_ + mp_vole_ot_num_;
    vole_num_ = lpn_param_.n_;

    is_mal_ = mal;

    if (is_mal_) {
      base_vole_num_ += 1;
      base_vole_ot_num_ += sizeof(internal::PTy);
      require_ot_num_ += sizeof(internal::PTy);
    }
  }

  void GenIndexes() { mp_param_.GenIndexes(); }

  void SetIndexes(absl::Span<const size_t> indexes) {
    mp_param_.SetIndexes(indexes);
  }
};

// Wolverine Vole
// the following equation would hold:
// > pre_c = pre_a * delta + pre_b
// > c     =     a * delta +     b
// > Sender holds c && delta
// > Receiver holds a && b
void WolverineVoleSend(const std::shared_ptr<Connection>& conn,
                       const yc::OtSendStore& send_ot, const VoleParam& param,
                       internal::PTy delta, absl::Span<internal::PTy> pre_c,
                       absl::Span<internal::PTy> c);

void WolverineVoleRecv(const std::shared_ptr<Connection>& conn,
                       const yc::OtRecvStore& recv_ot, const VoleParam& param,
                       absl::Span<internal::PTy> pre_a,
                       absl::Span<internal::PTy> pre_b,
                       absl::Span<internal::PTy> a,
                       absl::Span<internal::PTy> b);

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

}  // namespace mosac::vole
