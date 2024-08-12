
#include "mosac/cr/utils/mpfss.h"

#include "mosac/utils/vec_op.h"
#include "yacl/base/byte_container_view.h"
#include "yacl/crypto/primitives/ot/gywz_ote.h"
#include "yacl/crypto/tools/crhash.h"

namespace mosac::vole {

void MpCotSend(const std::shared_ptr<Connection>& conn,
               const yc::OtSendStore& send_ot, const MpParam& param,
               absl::Span<uint128_t> output) {
  YACL_ENFORCE(output.size() >= param.mp_vole_size_);
  YACL_ENFORCE(send_ot.Size() >= param.require_ot_num_);

  const auto& batch_num = param.noise_num_;
  const auto& batch_size = param.sp_vole_size_;
  const auto& last_batch_size = param.last_sp_vole_size_;
  const auto batch_length = ym::Log2Ceil(batch_size);
  const auto last_batch_length = ym::Log2Ceil(last_batch_size);

  for (size_t i = 0; i < batch_num; ++i) {
    auto this_size = (i == batch_num - 1) ? last_batch_size : batch_size;
    auto this_length = (i == batch_num - 1) ? last_batch_length : batch_length;

    auto this_span = output.subspan(i * batch_size, this_size);
    auto ot_slice =
        send_ot.Slice(i * batch_length, i * batch_length + this_length);

    yc::GywzOtExtSend(conn, ot_slice, this_size, this_span);
  }
}

void MpCotRecv(const std::shared_ptr<Connection>& conn,
               const yc::OtRecvStore& recv_ot, const MpParam& param,
               absl::Span<uint128_t> output) {
  YACL_ENFORCE(output.size() >= param.mp_vole_size_);
  YACL_ENFORCE(recv_ot.Size() >= param.require_ot_num_);

  const auto& batch_num = param.noise_num_;
  const auto& batch_size = param.sp_vole_size_;
  const auto& last_batch_size = param.last_sp_vole_size_;
  const auto& indexes = param.indexes_;
  const auto batch_length = ym::Log2Ceil(batch_size);
  const auto last_batch_length = ym::Log2Ceil(last_batch_size);

  for (size_t i = 0; i < batch_num; ++i) {
    auto this_size = (i == batch_num - 1) ? last_batch_size : batch_size;
    auto this_length = (i == batch_num - 1) ? last_batch_length : batch_length;

    auto this_span = output.subspan(i * batch_size, this_size);
    auto ot_slice =
        recv_ot.Slice(i * batch_length, i * batch_length + this_length);

    yc::GywzOtExtRecv(conn, ot_slice, this_size, indexes[i], this_span);
  }
}

void MpFssSend(const std::shared_ptr<Connection>& conn,
               const yc::OtSendStore& send_ot, const MpParam& param,
               absl::Span<internal::PTy> w, absl::Span<internal::PTy> output) {
  YACL_ENFORCE(output.size() >= param.mp_vole_size_);
  YACL_ENFORCE(w.size() >= param.noise_num_);
  YACL_ENFORCE(send_ot.Size() >= param.require_ot_num_);

  std::vector<uint128_t> ote_buffer(param.mp_vole_size_);
  auto ote_span = absl::MakeSpan(ote_buffer);

  MpCotSend(conn, send_ot, param, ote_span);
  // break correlation
  yc::ParaCrHashInplace_128(ote_span);

  const auto& batch_num = param.noise_num_;
  const auto& batch_size = param.sp_vole_size_;
  const auto& last_batch_size = param.last_sp_vole_size_;

  auto send_msgs = std::vector<internal::PTy>(batch_num, 0);

  for (size_t i = 0; i < batch_num; ++i) {
    auto this_size = (i == batch_num - 1) ? last_batch_size : batch_size;
    auto this_span = ote_span.subspan(i * batch_size, this_size);
    auto this_output = output.subspan(i * batch_size, this_size);

    std::transform(this_span.cbegin(), this_span.cend(), this_output.begin(),
                   [](uint128_t val) { return internal::PTy(val); });
    auto tmp = std::reduce(this_output.cbegin(), this_output.cend(),
                           internal::PTy(0), std::plus<internal::PTy>());
    send_msgs[i] = tmp - w[i];
  }

  conn->SendAsync(
      conn->NextRank(),
      yacl::ByteContainerView(send_msgs.data(),
                              send_msgs.size() * sizeof(internal::PTy)),
      "MpVole");
}

void MpFssRecv(const std::shared_ptr<Connection>& conn,
               const yc::OtRecvStore& recv_ot, const MpParam& param,
               absl::Span<internal::PTy> output) {
  YACL_ENFORCE(output.size() >= param.mp_vole_size_);
  YACL_ENFORCE(recv_ot.Size() >= param.require_ot_num_);

  std::vector<uint128_t> ote_buffer(param.mp_vole_size_);
  auto ote_span = absl::MakeSpan(ote_buffer);

  MpCotRecv(conn, recv_ot, param, ote_span);
  // break correlation
  yc::ParaCrHashInplace_128(ote_span);

  const auto& batch_num = param.noise_num_;
  const auto& batch_size = param.sp_vole_size_;
  const auto& last_batch_size = param.last_sp_vole_size_;
  const auto& indexes = param.indexes_;

  auto recv_buff = conn->Recv(conn->NextRank(), "MpVole");
  auto recv_msgs = absl::MakeSpan(
      reinterpret_cast<internal::PTy*>(recv_buff.data()), batch_num);

  for (size_t i = 0; i < batch_num; ++i) {
    auto this_size = (i == batch_num - 1) ? last_batch_size : batch_size;
    auto this_span = ote_span.subspan(i * batch_size, this_size);
    auto this_output = output.subspan(i * batch_size, this_size);

    std::transform(this_span.cbegin(), this_span.cend(), this_output.begin(),
                   [](uint128_t val) { return internal::PTy(val); });

    auto tmp = std::reduce(this_output.cbegin(), this_output.cend(),
                           internal::PTy(0), std::plus<internal::PTy>());
    recv_msgs[i] = recv_msgs[i] - tmp;
    this_output[indexes[i]] = this_output[indexes[i]] + recv_msgs[i];
  }
}

void MpVoleSend(const std::shared_ptr<Connection>& conn,
                const yc::OtSendStore& send_ot, const MpParam& param,
                absl::Span<internal::PTy> w, absl::Span<internal::PTy> output) {
  YACL_ENFORCE(output.size() >= param.mp_vole_size_);
  YACL_ENFORCE(w.size() >= param.noise_num_);
  YACL_ENFORCE(send_ot.Size() >= param.require_ot_num_);
  // same as MpFssSend
  MpFssSend(conn, send_ot, param, w, output);
}

void MpVoleRecv(const std::shared_ptr<Connection>& conn,
                const yc::OtRecvStore& recv_ot, const MpParam& param,
                absl::Span<internal::PTy> v, absl::Span<internal::PTy> output) {
  YACL_ENFORCE(output.size() >= param.mp_vole_size_);
  YACL_ENFORCE(v.size() >= param.noise_num_);
  YACL_ENFORCE(recv_ot.Size() >= param.require_ot_num_);

  MpFssRecv(conn, recv_ot, param, output);

  const auto& batch_num = param.noise_num_;
  const auto& batch_size = param.sp_vole_size_;
  const auto& last_batch_size = param.last_sp_vole_size_;
  const auto& indexes = param.indexes_;

  for (size_t i = 0; i < batch_num; ++i) {
    auto this_size = (i == batch_num - 1) ? last_batch_size : batch_size;
    auto this_output = output.subspan(i * batch_size, this_size);
    this_output[indexes[i]] = this_output[indexes[i]] + v[i];
  }
}

}  // namespace mosac::vole
