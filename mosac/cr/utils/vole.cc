#include "mosac/cr/utils/vole.h"

#include "mosac/cr/utils/linear_code.h"
#include "mosac/utils/vec_op.h"
#include "yacl/base/byte_container_view.h"

namespace mosac::vole {

void WolverineVoleSend(const std::shared_ptr<Connection>& conn,
                       const yc::OtSendStore& send_ot, const VoleParam& param,
                       [[maybe_unused]] internal::PTy delta,
                       absl::Span<internal::PTy> pre_c,
                       absl::Span<internal::PTy> c) {
  auto& lpn_param = param.lpn_param_;
  auto& mp_param = param.mp_param_;

  YACL_ENFORCE(pre_c.size() >= param.base_vole_num_);
  YACL_ENFORCE(c.size() >= param.vole_num_);

  YACL_ENFORCE(send_ot.Size() >= param.mp_vole_ot_num_);
  MpVoleSend(conn, send_ot, mp_param, pre_c, c);

  // ---- consistency check ----
  if (param.is_mal_) {
    auto seed = conn->SyncSeed();
    auto uhash = UniversalHash(seed, c.subspan(0, param.vole_num_));
    auto buf = conn->Recv(conn->NextRank(), "MalVole");
    YACL_ENFORCE(buf.size() == sizeof(internal::PTy));
    internal::PTy diff;
    memcpy(&diff, buf.data(), buf.size());
    uhash = uhash - delta * diff + pre_c.back();

    auto hash =
        yacl::crypto::Sm3(yacl::ByteContainerView(&uhash, sizeof(uhash)));
    conn->SendAsync(conn->NextRank(), yacl::ByteContainerView(hash),
                    "MalVoleHash");
  }
  // ---- consistency check ----

  auto seed = conn->SyncSeed();
  auto llc = code::LocalLinearCode<10>(seed, lpn_param.n_, lpn_param.k_);
  llc.Encode(pre_c.subspan(0, lpn_param.k_), c.subspan(0, lpn_param.n_));
}

void WolverineVoleRecv(const std::shared_ptr<Connection>& conn,
                       const yc::OtRecvStore& recv_ot, const VoleParam& param,
                       absl::Span<internal::PTy> pre_a,
                       absl::Span<internal::PTy> pre_b,
                       absl::Span<internal::PTy> a,
                       absl::Span<internal::PTy> b) {
  auto& lpn_param = param.lpn_param_;
  auto& mp_param = param.mp_param_;

  YACL_ENFORCE(pre_a.size() >= param.base_vole_num_);
  YACL_ENFORCE(pre_b.size() >= param.base_vole_num_);
  YACL_ENFORCE(a.size() >= param.vole_num_);
  YACL_ENFORCE(b.size() >= param.vole_num_);

  YACL_ENFORCE(recv_ot.Size() >= param.mp_vole_ot_num_);

  std::for_each(a.begin(), a.end(), [](internal::PTy& e) { e = 0; });

  std::vector<size_t> indexes;
  for (size_t i = 0; i < mp_param.noise_num_; ++i) {
    size_t tmp = i * mp_param.sp_vole_size_ + mp_param.indexes_[i];
    a[tmp] = pre_a[i];
    indexes.emplace_back(tmp);
  }
  MpVoleRecv(conn, recv_ot, mp_param, pre_b, b);

  // ---- consistency check ----
  if (param.is_mal_) {
    auto seed = conn->SyncSeed();
    auto uhash = UniversalHash(seed, b.subspan(0, param.vole_num_));
    auto coef = ExtractCeof(seed, absl::MakeConstSpan(indexes));
    auto diff = internal::op::InPro(absl::MakeConstSpan(coef),
                                    pre_a.subspan(0, coef.size()));
    diff = diff + pre_a.back();
    uhash = uhash + pre_b.back();
    conn->Send(conn->NextRank(), yacl::ByteContainerView(&diff, sizeof(diff)),
               "MalVole");

    auto hash =
        yacl::crypto::Sm3(yacl::ByteContainerView(&uhash, sizeof(uhash)));
    auto remote_hash = conn->Recv(conn->NextRank(), "MalVoleHash");
    YACL_ENFORCE(yacl::ByteContainerView(hash) ==
                 yacl::ByteContainerView(remote_hash));
  }
  // ---- consistency check ----

  auto seed = conn->SyncSeed();
  auto llc = code::LocalLinearCode<10>(seed, lpn_param.n_, lpn_param.k_);
  llc.Encode2(pre_a.subspan(0, lpn_param.k_), a.subspan(0, lpn_param.n_),
              pre_b.subspan(0, lpn_param.k_), b.subspan(0, lpn_param.n_));
}

}  // namespace mosac::vole
