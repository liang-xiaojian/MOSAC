#include "mosac/cr/utils/ferret_ot.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "yacl/base/aligned_vector.h"
#include "yacl/crypto/base/hash/hash_utils.h"
#include "yacl/crypto/primitives/code/linear_code.h"
#include "yacl/crypto/primitives/ot/gywz_ote.h"
#include "yacl/crypto/tools/common.h"
#include "yacl/math/gadget.h"
#include "yacl/utils/serialize.h"

namespace mosac::ot {

namespace ym = yacl::math;

namespace {

uint128_t Gf128Pack(absl::Span<const uint128_t> data) {
  auto length = data.size();
  YACL_ENFORCE(length <= 128);
  auto basis = yacl::gf128_basis;
  return yacl::GfMul128(data, absl::MakeSpan(basis).subspan(0, length));
}

inline uint64_t MpCotRNHelper(uint64_t idx_num, uint64_t idx_range,
                              bool mal = false) {
  const auto batch_size = (idx_range + idx_num - 1) / idx_num;
  const auto last_size = idx_range - batch_size * (idx_num - 1);
  const auto check_size = (mal == true) ? 128 : 0;
  return ym::Log2Ceil(batch_size) * (idx_num - 1) + ym::Log2Ceil(last_size) +
         check_size;
}

inline void MpCotRNSend(const std::shared_ptr<yacl::link::Context>& ctx,
                        const yc::OtSendStore& cot, uint64_t idx_range,
                        uint64_t idx_num, uint64_t spcot_size,
                        absl::Span<uint128_t> out, bool mal = true) {
  const uint64_t full_size = idx_range;
  const uint64_t batch_size = spcot_size;
  const uint64_t batch_num = ym::DivCeil(full_size, batch_size);
  YACL_ENFORCE(batch_num <= idx_num);

  const uint64_t last_size = full_size - (batch_num - 1) * batch_size;
  const uint64_t batch_length = ym::Log2Ceil(batch_size);
  const uint64_t last_length = ym::Log2Ceil(last_size);

  // for each bin, call single-point cot
  for (uint64_t i = 0; i < batch_num - 1; ++i) {
    const auto& cot_slice =
        cot.Slice(i * batch_length, i * batch_length + batch_length);
    yc::GywzOtExtSend_ferret(ctx, cot_slice, batch_size,
                             out.subspan(i * batch_size, batch_size));
  }
  // deal with last batch
  if (last_size == 1) {
    out[(batch_num - 1) * batch_size] =
        cot.GetBlock((batch_num - 1) * batch_length, 0);
  } else {
    const auto& cot_slice =
        cot.Slice((batch_num - 1) * batch_length,
                  (batch_num - 1) * batch_length + last_length);
    yc::GywzOtExtSend_ferret(
        ctx, cot_slice, last_size,
        out.subspan((batch_num - 1) * batch_size, last_size));
  }

  if (mal) {
    // COT for Consistency check
    auto check_cot =
        cot.Slice(batch_length * (batch_num - 1) + last_length,
                  batch_length * (batch_num - 1) + last_length + 128);

    auto seed = yc::SyncSeedSend(ctx);
    auto uhash = ym::UniversalHash<uint128_t>(seed, out);

    auto recv_buf = ctx->Recv(ctx->NextRank(), "FerretCheck: masked choices");
    auto choices = yacl::dynamic_bitset<uint128_t>(0);
    choices.append(DeserializeUint128(recv_buf));

    std::array<uint128_t, 128> check_cot_data;
    for (size_t i = 0; i < 128; ++i) {
      check_cot_data[i] = check_cot.GetBlock(i, choices[i]);
    }
    auto diff = Gf128Pack(absl::MakeSpan(check_cot_data));
    uhash = uhash ^ diff;

    auto hash = yc::Blake3(yacl::SerializeUint128(uhash));
    ctx->SendAsync(ctx->NextRank(), yacl::ByteContainerView(hash),
                   "FerretCheck: hash value");
  }
}

inline void MpCotRNRecv(const std::shared_ptr<yacl::link::Context>& ctx,
                        const yc::OtRecvStore& cot, uint64_t idx_range,
                        uint64_t idx_num, uint64_t spcot_size,
                        absl::Span<uint128_t> out, bool mal = true) {
  const uint64_t full_size = idx_range;
  const uint64_t batch_size = spcot_size;
  const uint64_t batch_num = ym::DivCeil(full_size, batch_size);
  YACL_ENFORCE(batch_num <= idx_num);

  const uint64_t last_size = full_size - (batch_num - 1) * batch_size;
  const uint64_t batch_length = ym::Log2Ceil(batch_size);
  const uint64_t last_length = ym::Log2Ceil(last_size);

  // for each bin, call single-point cot
  for (uint64_t i = 0; i < batch_num - 1; ++i) {
    const auto cot_slice =
        cot.Slice(i * batch_length, i * batch_length + batch_length);
    yc::GywzOtExtRecv_ferret(ctx, cot_slice, batch_size,
                             out.subspan(i * batch_size, batch_size));
  }
  // deal with last batch
  if (last_size == 1) {
    out[(batch_num - 1) * batch_size] =
        cot.GetBlock((batch_num - 1) * batch_length);
  } else {
    const auto& cot_slice =
        cot.Slice((batch_num - 1) * batch_length,
                  (batch_num - 1) * batch_length + last_length);
    yc::GywzOtExtRecv_ferret(
        ctx, cot_slice, last_size,
        out.subspan((batch_num - 1) * batch_size, last_size));
  }
  // malicious: consistency check
  if (mal) {
    // COT for consistency check
    auto check_cot =
        cot.Slice(batch_length * (batch_num - 1) + last_length,
                  batch_length * (batch_num - 1) + last_length + 128);
    auto seed = yc::SyncSeedRecv(ctx);
    auto uhash = ym::UniversalHash<uint128_t>(seed, out);

    // [Warning] low efficency
    auto _choices = yacl::dynamic_bitset<uint128_t>(128);
    for (size_t i = 0; i < 128; ++i) {
      _choices[i] = check_cot.GetChoice(i);
    }
    uint128_t choices = _choices.data()[0];

    auto check_cot_data = check_cot.CopyBlocks();
    auto diff = Gf128Pack(absl::MakeSpan(check_cot_data));
    uhash = uhash ^ diff;

    // find punctured indexes
    std::vector<uint64_t> indexes;
    for (size_t i = 0; i < out.size(); ++i) {
      if (out[i] & 0x1) {
        indexes.push_back(i);
      }
    }
    // extract the coefficent for universal hash
    auto ceof = ym::ExtractHashCoef(seed, absl::MakeConstSpan(indexes));
    choices = std::accumulate(ceof.cbegin(), ceof.cend(), choices,
                              std::bit_xor<uint128_t>());

    ctx->SendAsync(ctx->NextRank(), yacl::SerializeUint128(choices),
                   "FerretCheck: masked choices");

    auto hash = yc::Blake3(yacl::SerializeUint128(uhash));
    auto buf = ctx->Recv(ctx->NextRank(), "FerretCheck: hash value");

    YACL_ENFORCE(yacl::ByteContainerView(hash) == yacl::ByteContainerView(buf),
                 "FerretCheck: fail");
  }
}

}  // namespace

uint64_t FerretCotHelper(const yc::LpnParam& lpn_param, uint64_t /*ot_num*/,
                         bool mal) {
  uint64_t mpcot_cot = 0;
  if (lpn_param.noise_asm == yc::LpnNoiseAsm::RegularNoise) {
    // for each mpcot invocation,
    //  idx_num = lpn_param.t (the number of non-zeros)
    //  idx_range = lpn_param.n the idx range for each index
    mpcot_cot = MpCotRNHelper(lpn_param.t, lpn_param.n);
  } else {
    YACL_THROW("Not Implemented!");
    // for each mpcot invocation,
    //  idx_num = lpn_param.t (the number of non-zeros)
    //  idx_range = lpn_param.n the idx range for each index
    // mpcot_cot = MpCotUNHelper(lpn_param.t, lpn_param.n);
  }

  // The required cots are used as:
  // (1) expansion seed: kFerret_lpnK
  // (2) mpcot cot: mp_option.cot_num (just for the first batch)
  // (3) mpcot malicious check: 128 (fixed)
  uint64_t check_cot = (mal == true) ? 128 : 0;
  return lpn_param.k + mpcot_cot + check_cot;
}

void FerretOtExtSend_mal(const std::shared_ptr<yacl::link::Context>& ctx,
                         const yc::OtSendStore& base_cot,
                         const yc::LpnParam& lpn_param, uint64_t ot_num,
                         absl::Span<uint128_t> out) {
  SPDLOG_INFO("call ferret with lpn N {} && ot_num {}", lpn_param.n, ot_num);
  YACL_ENFORCE(ctx->WorldSize() == 2);  // Make sure that OT has two parties
  YACL_ENFORCE(base_cot.Type() == yc::OtStoreType::Compact);
  YACL_ENFORCE(base_cot.Size() >= FerretCotHelper(lpn_param, ot_num, true));

  // get constants: the number of cot needed for mpcot phase
  const auto mpcot_cot_num = MpCotRNHelper(lpn_param.t, lpn_param.n, true);

  // get constants: batch information
  const uint64_t cache_size = lpn_param.k + mpcot_cot_num;
  const uint64_t batch_size = lpn_param.n - cache_size;
  const uint64_t batch_num = (ot_num + batch_size - 1) / batch_size;
  // const uint128_t delta = base_cot.GetDelta();

  // prepare v (before silent expansion), where w = v ^ u * delta
  auto cot_seed = base_cot.Slice(0, lpn_param.k);
  auto cot_mpcot = base_cot.Slice(lpn_param.k, lpn_param.k + mpcot_cot_num);
  auto working_v = cot_seed.CopyCotBlocks();

  // get lpn public matrix A
  uint128_t seed = yc::SyncSeedSend(ctx);
  yc::LocalLinearCode<10> llc(seed, lpn_param.n, lpn_param.k);

  // placeholder for the outputs
  YACL_ENFORCE(out.size() == ot_num);
  // AlignedVector<uint128_t> out(ot_num);
  auto out_span = out;

  auto spcot_size = lpn_param.n / lpn_param.t;
  for (uint64_t i = 0; i < batch_num; ++i) {
    // the ot generated by this batch (including the seeds for next batch if
    // necessary)
    auto batch_ot_num = std::min(lpn_param.n, ot_num - i * batch_size);
    auto working_s = out_span.subspan(i * batch_size, batch_ot_num);

    auto idx_num = lpn_param.t;
    auto idx_range = batch_ot_num;
    if (lpn_param.noise_asm == yc::LpnNoiseAsm::RegularNoise) {
      MpCotRNSend(ctx, cot_mpcot, idx_range, idx_num, spcot_size, working_s);
    } else {
      YACL_THROW("Not Implemented!");
      // MpCotUNSend(ctx, cot_mpcot, simple_map, option, working_s);
    }

    // use lpn to calculate v*A
    // llc.Encode(in,out) would calculate out = out + in * A
    llc.Encode(working_v, working_s);

    // bool is_last_batch = (i == batch_num - 1);
    // update v (first lpn_k of va^s)
    if ((ot_num - i * batch_size) > batch_ot_num) {
      // update v for the next batch
      memcpy(working_v.data(), working_s.data() + batch_size,
             lpn_param.k * sizeof(uint128_t));

      // manually set the cot for next batch mpcot
      yacl::AlignedVector<uint128_t> mpcot_data(mpcot_cot_num);
      memcpy(mpcot_data.data(), working_s.data() + batch_size + lpn_param.k,
             mpcot_cot_num * sizeof(uint128_t));
      cot_mpcot = yc::MakeCompactOtSendStore(std::move(mpcot_data),
                                             base_cot.GetDelta());
    } else {
      break;
    }
  }
}

void FerretOtExtRecv_mal(const std::shared_ptr<yacl::link::Context>& ctx,
                         const yc::OtRecvStore& base_cot,
                         const yc::LpnParam& lpn_param, uint64_t ot_num,
                         absl::Span<uint128_t> out) {
  YACL_ENFORCE(ctx->WorldSize() == 2);  // Make sure that OT has two parties
  YACL_ENFORCE(base_cot.Type() == yc::OtStoreType::Compact);
  YACL_ENFORCE(base_cot.Size() >= FerretCotHelper(lpn_param, ot_num, true));

  // get constants: the number of cot needed for mpcot phase
  const auto mpcot_cot_num = MpCotRNHelper(lpn_param.t, lpn_param.n, true);

  // get constants: batch information
  const uint64_t cache_size = lpn_param.k + mpcot_cot_num;
  const uint64_t batch_size = lpn_param.n - cache_size;
  const uint64_t batch_num = (ot_num + batch_size - 1) / batch_size;

  // F2, but we store it in uint128_t
  yacl::AlignedVector<uint128_t> u(lpn_param.k);

  // prepare u, w, where w = v ^ u * delta
  auto cot_seed = base_cot.Slice(0, lpn_param.k);
  auto cot_mpcot = base_cot.Slice(lpn_param.k, lpn_param.k + mpcot_cot_num);
  auto working_w = cot_seed.CopyBlocks();

  // get lpn public matrix A
  uint128_t seed = yc::SyncSeedRecv(ctx);
  yc::LocalLinearCode<10> llc(seed, lpn_param.n, lpn_param.k);

  // placeholder for the outputs
  // AlignedVector<uint128_t> out(ot_num);
  YACL_ENFORCE(out.size() == ot_num);
  auto out_span = out;

  auto spcot_size = lpn_param.n / lpn_param.t;
  for (uint64_t i = 0; i < batch_num; ++i) {
    // the ot generated by this batch (including the seeds for next batch if
    // necessary)
    auto batch_ot_num = std::min(lpn_param.n, ot_num - i * batch_size);
    auto working_r = out_span.subspan(i * batch_size, batch_ot_num);

    // run mpcot (get r)
    auto idx_num = lpn_param.t;
    auto idx_range = batch_ot_num;

    if (lpn_param.noise_asm == yc::LpnNoiseAsm::RegularNoise) {
      MpCotRNRecv(ctx, cot_mpcot, idx_range, idx_num, spcot_size, working_r);
    } else {
      YACL_THROW("Not Implemented!");
      // MpCotUNRecv(ctx, cot_mpcot, simple_map, option, e, working_r);
    }

    // use lpn to calculate w*A, and u*A
    // llc.Encode(in,out) would calculate out = out + in * A
    llc.Encode(working_w, working_r);

    // bool is_last_batch = (i == batch_num - 1);
    if ((ot_num - i * batch_size) > batch_ot_num) {
      // update u, w (first lpn_k of va^s)
      memcpy(working_w.data(), working_r.data() + batch_size,
             lpn_param.k * sizeof(uint128_t));

      // manually set the cot for next batch mpcot
      yacl::AlignedVector<uint128_t> mpcot_data(mpcot_cot_num);
      memcpy(mpcot_data.data(), working_r.data() + batch_size + lpn_param.k,
             mpcot_cot_num * sizeof(uint128_t));
      cot_mpcot = yc::MakeCompactOtRecvStore(std::move(mpcot_data));
    } else {
      break;
    }
  }
}

}  // namespace mosac::ot
