#include "mosac/ss/ashare.h"

#include <vector>

#include "mosac/cr/cr.h"
#include "mosac/cr/fake_cr.h"
#include "mosac/ss/protocol.h"
#include "mosac/utils/field.h"
#include "mosac/utils/vec_op.h"

namespace mosac::internal {

// --------------- AA ------------------

std::vector<ATy> AddAA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> lhs, absl::Span<const ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const size_t size = lhs.size();
  std::vector<ATy> ret(size);

  op::Add(
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(lhs.data()), size * 2),
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(rhs.data()), size * 2),
      absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), size * 2));

  return ret;
}

std::vector<ATy> AddAA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const size_t size = lhs.size();
  return std::vector<ATy>(size);
}

std::vector<ATy> SubAA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> lhs, absl::Span<const ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const size_t size = lhs.size();
  std::vector<ATy> ret(size);

  op::Sub(
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(lhs.data()), size * 2),
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(rhs.data()), size * 2),
      absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), size * 2));

  return ret;
}

std::vector<ATy> SubAA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const size_t size = lhs.size();
  return std::vector<ATy>(size);
}

std::vector<ATy> MulAA(std::shared_ptr<Context>& ctx, absl::Span<const ATy> lhs,
                       absl::Span<const ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const size_t size = lhs.size();
  auto [a, b, c] = ctx->GetState<Correlation>()->BeaverTriple(size);
  auto u = SubAA(ctx, lhs, a);  // x-a
  auto v = SubAA(ctx, rhs, b);  // y-b

  // uv appending
  std::copy(v.begin(), v.end(), std::back_inserter(u));
  auto& uv = u;  // rename
  auto uv_p = A2P_delay(ctx, uv);

  auto u_p = absl::MakeConstSpan(uv_p).subspan(0, size);
  auto v_p = absl::MakeConstSpan(uv_p).subspan(size, size);
  // auto u_p = A2P_delay(ctx, u);
  // auto v_p = A2P_delay(ctx, v);

  // ret = c + x(y-b) + (x-a)y - (x-a)(y-b)
  auto xyb = MulAP(ctx, lhs, v_p);
  auto xay = MulPA(ctx, u_p, rhs);
  auto xayb = MulPP(ctx, u_p, v_p);

  c = AddAA(ctx, absl::MakeSpan(c), absl::MakeSpan(xyb));
  c = AddAA(ctx, absl::MakeSpan(c), absl::MakeSpan(xay));
  c = SubAP(ctx, absl::MakeSpan(c), absl::MakeSpan(xayb));
  return c;
}

std::vector<ATy> MulAA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const size_t size = lhs.size();
  ctx->GetState<Correlation>()->BeaverTriple_cache(size);
  std::vector<ATy> a(size);
  std::vector<ATy> b(size);
  std::vector<ATy> c(size);
  auto u = SubAA_cache(ctx, lhs, a);  // x-a
  auto v = SubAA_cache(ctx, rhs, b);  // y-b

  std::copy(v.begin(), v.end(), std::back_inserter(u));
  auto& uv = u;  // rename
  auto uv_p = A2P_delay_cache(ctx, uv);

  auto u_p = absl::MakeConstSpan(uv_p).subspan(0, size);
  auto v_p = absl::MakeConstSpan(uv_p).subspan(size, size);

  // ret = c + x(y-b) + (x-a)y - (x-a)(y-b)
  auto xyb = MulAP_cache(ctx, lhs, v_p);
  auto xay = MulPA_cache(ctx, u_p, rhs);
  auto xayb = MulPP_cache(ctx, u_p, v_p);

  c = AddAA_cache(ctx, absl::MakeSpan(c), absl::MakeSpan(xyb));
  c = AddAA_cache(ctx, absl::MakeSpan(c), absl::MakeSpan(xay));
  c = SubAP_cache(ctx, absl::MakeSpan(c), absl::MakeSpan(xayb));
  return c;
}

std::vector<ATy> DivAA(std::shared_ptr<Context>& ctx, absl::Span<const ATy> lhs,
                       absl::Span<const ATy> rhs) {
  const size_t num = lhs.size();
  YACL_ENFORCE(num == rhs.size());
  auto inv = InvA(ctx, absl::MakeConstSpan(rhs));
  return MulAA(ctx, absl::MakeConstSpan(lhs), absl::MakeConstSpan(inv));
}

std::vector<ATy> DivAA_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const ATy> rhs) {
  const size_t num = lhs.size();
  YACL_ENFORCE(num == rhs.size());
  auto inv = InvA_cache(ctx, absl::MakeConstSpan(rhs));
  return MulAA_cache(ctx, absl::MakeConstSpan(lhs), absl::MakeConstSpan(inv));
}

std::vector<ATy> NegA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                      absl::Span<const ATy> in) {
  const size_t size = in.size();
  std::vector<ATy> ret(size);
  op::Neg(
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(in.data()), 2 * size),
      absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), 2 * size));
  return ret;
}

std::vector<ATy> NegA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                            absl::Span<const ATy> in) {
  const size_t size = in.size();
  return std::vector<ATy>(size);
}

std::vector<ATy> InvA(std::shared_ptr<Context>& ctx, absl::Span<const ATy> in) {
  const size_t num = in.size();
  // r <-- random a-share
  auto r = RandA(ctx, num);
  // r * in
  auto mul = MulAA(ctx, absl::MakeConstSpan(r), absl::MakeConstSpan(in));
  // reveal r * in
  auto pub = A2P_delay(ctx, absl::MakeConstSpan(mul));
  // inv = (r * in)^{-1}
  auto inv = InvP(ctx, absl::MakeConstSpan(pub));
  // r * inv = in^{-1}
  return MulAP(ctx, absl::MakeConstSpan(r), absl::MakeConstSpan(inv));
}

std::vector<ATy> InvA_cache(std::shared_ptr<Context>& ctx,
                            absl::Span<const ATy> in) {
  const size_t num = in.size();
  // r <-- random a-share
  auto r = RandA_cache(ctx, num);
  // r * in
  auto mul = MulAA_cache(ctx, absl::MakeConstSpan(r), absl::MakeConstSpan(in));
  // reveal r * in
  auto pub = A2P_delay_cache(ctx, absl::MakeConstSpan(mul));
  // inv = (r * in)^{-1}
  auto inv = InvP_cache(ctx, absl::MakeConstSpan(pub));
  // r * inv = in^{-1}
  return MulAP_cache(ctx, absl::MakeConstSpan(r), absl::MakeConstSpan(inv));
}

std::vector<ATy> ZerosA(std::shared_ptr<Context>& ctx, size_t num) {
  std::vector<ATy> ret(num);

  auto prg_ptr = ctx->GetState<Prg>();
  op::Rand(*prg_ptr,
           absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), num * 2));
  if (ctx->GetRank() == 0) {
    op::Neg(
        absl::MakeConstSpan(reinterpret_cast<const PTy*>(ret.data()), num * 2),
        absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), num * 2));
  }
  return ret;
}

std::vector<ATy> ZerosA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                              size_t num) {
  return std::vector<ATy>(num);
}

std::vector<ATy> RandA(std::shared_ptr<Context>& ctx, size_t num) {
  return ctx->GetState<Correlation>()->RandomAuth(num).data;
}

std::vector<ATy> RandA_cache(std::shared_ptr<Context>& ctx, size_t num) {
  ctx->GetState<Correlation>()->RandomAuth_cache(num);
  return std::vector<ATy>(num);
}

// --------------- AP && PA ------------------

std::vector<ATy> AddAP([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> lhs, absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  auto [val, mac] = Unpack(lhs);
  if (ctx->GetRank() == 0) {
    // val += rhs
    op::AddInplace(absl::MakeSpan(val), absl::MakeConstSpan(rhs));
  }
  std::vector<PTy> rhs_mac(size);
  op::ScalarMul(ctx->GetState<Protocol>()->GetKey(), absl::MakeConstSpan(rhs),
                absl::MakeSpan(rhs_mac));
  // mac += rhs_mac
  op::AddInplace(absl::MakeSpan(mac), absl::MakeConstSpan(rhs_mac));
  return Pack(absl::MakeSpan(val), absl::MakeSpan(mac));
}

std::vector<ATy> AddAP_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  return std::vector<ATy>(size);
}

std::vector<ATy> SubAP([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> lhs, absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  auto neg_rhs = NegP(ctx, rhs);
  return AddAP(ctx, lhs, neg_rhs);
}

std::vector<ATy> SubAP_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  auto neg_rhs = NegP_cache(ctx, rhs);
  return AddAP_cache(ctx, lhs, neg_rhs);
}

std::vector<ATy> MulAP([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> lhs, absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  auto [val, mac] = Unpack(absl::MakeConstSpan(lhs));
  op::MulInplace(absl::MakeSpan(val), absl::MakeConstSpan(rhs));
  op::MulInplace(absl::MakeSpan(mac), absl::MakeConstSpan(rhs));
  return Pack(absl::MakeSpan(val), absl::MakeSpan(mac));
}

std::vector<ATy> MulAP_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  return std::vector<ATy>(size);
}

std::vector<ATy> DivAP([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> lhs, absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  std::vector<PTy> inv(size);
  op::Inv(absl::MakeConstSpan(rhs), absl::MakeSpan(inv));
  return MulAP(ctx, lhs, inv);
}

std::vector<ATy> DivAP_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> lhs,
                             absl::Span<const PTy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  std::vector<PTy> inv(size);
  return MulAP_cache(ctx, lhs, inv);
}

std::vector<ATy> AddPA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const PTy> lhs, absl::Span<const ATy> rhs) {
  return AddAP(ctx, rhs, lhs);
}

std::vector<ATy> AddPA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const ATy> rhs) {
  return AddAP_cache(ctx, rhs, lhs);
}

std::vector<ATy> SubPA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const PTy> lhs, absl::Span<const ATy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  auto neg_rhs = NegA(ctx, rhs);
  return AddPA(ctx, lhs, neg_rhs);
}

std::vector<ATy> SubPA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const ATy> rhs) {
  const size_t size = lhs.size();
  YACL_ENFORCE(size == rhs.size());
  auto neg_rhs = NegA_cache(ctx, rhs);
  return AddPA_cache(ctx, lhs, neg_rhs);
}

std::vector<ATy> MulPA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                       absl::Span<const PTy> lhs, absl::Span<const ATy> rhs) {
  return MulAP(ctx, rhs, lhs);
}

std::vector<ATy> MulPA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const ATy> rhs) {
  return MulAP_cache(ctx, rhs, lhs);
}

std::vector<ATy> DivPA(std::shared_ptr<Context>& ctx, absl::Span<const PTy> lhs,
                       absl::Span<const ATy> rhs) {
  const size_t num = lhs.size();
  YACL_ENFORCE(num == rhs.size());
  auto inv = InvA(ctx, absl::MakeConstSpan(rhs));
  return MulPA(ctx, absl::MakeConstSpan(lhs), absl::MakeConstSpan(inv));
}

std::vector<ATy> DivPA_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const ATy> rhs) {
  const size_t num = lhs.size();
  YACL_ENFORCE(num == rhs.size());
  auto inv = InvA_cache(ctx, absl::MakeConstSpan(rhs));
  return MulPA_cache(ctx, absl::MakeConstSpan(lhs), absl::MakeConstSpan(inv));
}

// --------------- Conversion ------------------

std::vector<PTy> A2P(std::shared_ptr<Context>& ctx, absl::Span<const ATy> in) {
  const size_t size = in.size();
  auto [val, mac] = Unpack(absl::MakeSpan(in));
  auto conn = ctx->GetState<Connection>();
  auto val_bv = yacl::ByteContainerView(val.data(), size * sizeof(PTy));
  std::vector<PTy> real_val(size);

  auto buf = conn->Exchange(val_bv);
  op::Add(absl::MakeConstSpan(reinterpret_cast<const PTy*>(buf.data()), size),
          absl::MakeConstSpan(val), absl::MakeSpan(real_val));

  // Generate Sync Seed After Open Value
  auto sync_seed = conn->SyncSeed();
  auto coef = op::Rand(sync_seed, size);
  // linear combination
  auto real_val_affine =
      op::InPro(absl::MakeSpan(coef), absl::MakeSpan(real_val));
  auto mac_affine = op::InPro(absl::MakeSpan(coef), absl::MakeSpan(mac));

  auto key = ctx->GetState<Protocol>()->GetKey();
  auto zero_mac = mac_affine - real_val_affine * key;

  auto remote_mac_int = conn->ExchangeWithCommit(zero_mac.GetVal());
  YACL_ENFORCE(zero_mac + PTy(remote_mac_int) == PTy::Zero());
  return real_val;
}

std::vector<PTy> A2P_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                           absl::Span<const ATy> in) {
  const size_t size = in.size();
  return std::vector<PTy>(size, 0);
}

std::vector<PTy> A2P_delay(std::shared_ptr<Context>& ctx,
                           absl::Span<const ATy> in) {
  const size_t size = in.size();
  auto [val, mac] = Unpack(absl::MakeSpan(in));
  auto conn = ctx->GetState<Connection>();
  auto val_bv = yacl::ByteContainerView(val.data(), size * sizeof(PTy));
  std::vector<PTy> real_val(size);

  auto buf = conn->Exchange(val_bv);
  op::Add(absl::MakeConstSpan(reinterpret_cast<const PTy*>(buf.data()), size),
          absl::MakeConstSpan(val), absl::MakeSpan(real_val));

  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;

  std::vector<INTEGER> randomness(size, 0);
  std::transform(randomness.begin(), randomness.end(),
                 reinterpret_cast<INTEGER*>(val.data()), randomness.begin(),
                 std::bit_xor<INTEGER>());
  std::transform(randomness.begin(), randomness.end(),
                 reinterpret_cast<INTEGER*>(buf.data()), randomness.begin(),
                 std::bit_xor<INTEGER>());
  std::transform(randomness.begin(), randomness.end(),
                 reinterpret_cast<INTEGER*>(real_val.data()),
                 randomness.begin(), std::bit_xor<INTEGER>());

  auto seeds = yacl::crypto::Sm3(yacl::ByteContainerView(
      randomness.data(), randomness.size() * sizeof(INTEGER)));
  uint128_t sync_seed = 0;
  std::memcpy(&seeds, &sync_seed, sizeof(uint128_t));

  auto coef = internal::op::Rand(sync_seed, size);

  // linear combination
  auto real_val_affine =
      internal::op::InPro(absl::MakeSpan(coef), absl::MakeSpan(real_val));
  auto mac_affine =
      internal::op::InPro(absl::MakeSpan(coef), absl::MakeSpan(mac));

  auto key = ctx->GetState<Protocol>()->GetKey();
  auto zero_mac = mac_affine - real_val_affine * key;

  // Append to Buffer
  if (ctx->GetRank() == 0) {
    ctx->GetState<Protocol>()->CheckBufferAppend(zero_mac);
  } else {
    ctx->GetState<Protocol>()->CheckBufferAppend(PTy::Neg(zero_mac));
  }
  return real_val;
}

std::vector<PTy> A2P_delay_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                                 absl::Span<const ATy> in) {
  const size_t size = in.size();
  return std::vector<PTy>(size, 0);
}

std::vector<ATy> P2A(std::shared_ptr<Context>& ctx, absl::Span<const PTy> in) {
  const size_t size = in.size();
  auto zero = ZerosA(ctx, size);
  auto [zero_val, zero_mac] = Unpack(zero);
  if (ctx->GetRank() == 0) {
    // zero_val += in
    op::AddInplace(absl::MakeSpan(zero_val), absl::MakeConstSpan(in));
  }
  auto in_mac = op::ScalarMul(ctx->GetState<Protocol>()->GetKey(), in);
  // zero_mac += in_mac
  op::AddInplace(absl::MakeSpan(zero_mac), absl::MakeConstSpan(in_mac));
  return Pack(absl::MakeSpan(zero_val), absl::MakeSpan(zero_mac));
}

std::vector<ATy> P2A_cache(std::shared_ptr<Context>& ctx,
                           absl::Span<const PTy> in) {
  const size_t size = in.size();
  auto zero = ZerosA(ctx, size);
  auto [zero_val, zero_mac] = Unpack(zero);
  if (ctx->GetRank() == 0) {
    // zero_val += in
    op::AddInplace(absl::MakeSpan(zero_val), absl::MakeConstSpan(in));
  }
  auto in_mac = op::ScalarMul(ctx->GetState<Protocol>()->GetKey(), in);
  // zero_mac += in_mac
  op::AddInplace(absl::MakeSpan(zero_mac), absl::MakeConstSpan(in_mac));
  return Pack(absl::MakeSpan(zero_val), absl::MakeSpan(zero_mac));
}

// --------------- Shuffle Internal ---------------
std::vector<ATy> ShuffleAGet_internal(std::shared_ptr<Context>& ctx,
                                      absl::Span<const ATy> in,
                                      absl::Span<const PTy> ext_a,
                                      absl::Span<const PTy> ext_b) {
  const size_t num = in.size();
  YACL_ENFORCE(ext_a.size() == 2 * num);
  YACL_ENFORCE(ext_b.size() == 2 * num);

  auto val_a = ext_a.subspan(0, num);
  auto val_b = ext_b.subspan(0, num);
  auto mac_a = ext_a.subspan(num, num);
  auto mac_b = ext_b.subspan(num, num);

  auto [val_in, mac_in] = Unpack(absl::MakeConstSpan(in));

  auto val_tmp = op::Add(absl::MakeSpan(val_a), absl::MakeConstSpan(val_in));
  auto mac_tmp = op::Add(absl::MakeSpan(mac_a), absl::MakeConstSpan(mac_in));

  auto conn = ctx->GetConnection();
  conn->SendAsync(
      ctx->NextRank(),
      yacl::ByteContainerView(val_tmp.data(), val_tmp.size() * sizeof(PTy)),
      "send:a+x val");
  conn->SendAsync(
      ctx->NextRank(),
      yacl::ByteContainerView(mac_tmp.data(), mac_tmp.size() * sizeof(PTy)),
      "send:a+x mac");

  return Pack(absl::MakeConstSpan(val_b), absl::MakeConstSpan(mac_b));
}

std::vector<ATy> ShuffleASet_internal(std::shared_ptr<Context>& ctx,
                                      absl::Span<const ATy> in,
                                      absl::Span<const size_t> perm,
                                      absl::Span<const PTy> ext_delta) {
  const size_t num = in.size();
  YACL_ENFORCE(perm.size() == num);
  YACL_ENFORCE(ext_delta.size() == 2 * num);

  auto val_delta = ext_delta.subspan(0, num);
  auto mac_delta = ext_delta.subspan(num, num);

  auto conn = ctx->GetConnection();
  auto val_buf = conn->Recv(ctx->NextRank(), "send:a");
  auto mac_buf = conn->Recv(ctx->NextRank(), "send:b");

  auto val_tmp = absl::MakeSpan(reinterpret_cast<PTy*>(val_buf.data()), num);
  auto mac_tmp = absl::MakeSpan(reinterpret_cast<PTy*>(mac_buf.data()), num);

  auto [val_in, mac_in] = Unpack(absl::MakeConstSpan(in));
  // val_tmp += val_in
  op::AddInplace(absl::MakeSpan(val_tmp), absl::MakeConstSpan(val_in));
  // mac_tmp += mac_in
  op::AddInplace(absl::MakeSpan(mac_tmp), absl::MakeConstSpan(mac_in));

  std::vector<PTy> val_out(num);
  std::vector<PTy> mac_out(num);
  for (size_t i = 0; i < num; ++i) {
    val_out[i] = val_delta[i] + val_tmp[perm[i]];
    mac_out[i] = mac_delta[i] + mac_tmp[perm[i]];
  }
  return Pack(absl::MakeConstSpan(val_out), absl::MakeConstSpan(mac_out));
}

// --------------- Shuffle ---------------
std::vector<ATy> ShuffleAGet(std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> in) {
  const size_t num = in.size();
  // correlation
  // [Warning] low efficiency!!! optimize it
  auto [ext_a, ext_b] = ctx->GetState<Correlation>()->ShuffleGet(num, 2);
  return ShuffleAGet_internal(ctx, in, ext_a, ext_b);
}

std::vector<ATy> ShuffleAGet_cache(std::shared_ptr<Context>& ctx,
                                   absl::Span<const ATy> in) {
  const size_t num = in.size();
  // correlation
  // [Warning] low efficiency!!! optimize it
  ctx->GetState<Correlation>()->ShuffleGet_cache(num, 2);
  return std::vector<ATy>(num);
}

std::vector<ATy> ShuffleASet(std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> in) {
  const size_t num = in.size();
  // correlation
  // [Warning] low efficiency!!! optimize it
  auto [ext_delta, perm] = ctx->GetState<Correlation>()->ShuffleSet(num, 2);
  return ShuffleASet_internal(ctx, in, perm, ext_delta);
}

std::vector<ATy> ShuffleASet_cache(std::shared_ptr<Context>& ctx,
                                   absl::Span<const ATy> in) {
  const size_t num = in.size();
  // correlation
  // [Warning] low efficiency!!! optimize it
  ctx->GetState<Correlation>()->ShuffleSet_cache(num, 2);
  return std::vector<ATy>(num);
}

std::vector<ATy> ShuffleA(std::shared_ptr<Context>& ctx,
                          absl::Span<const ATy> in) {
  if (ctx->GetRank() == 0) {
    auto tmp = ShuffleASet(ctx, in);
    return ShuffleAGet(ctx, tmp);
  }
  auto tmp = ShuffleAGet(ctx, in);
  return ShuffleASet(ctx, tmp);
}

std::vector<ATy> ShuffleA_cache(std::shared_ptr<Context>& ctx,
                                absl::Span<const ATy> in) {
  if (ctx->GetRank() == 0) {
    auto tmp = ShuffleASet_cache(ctx, in);
    return ShuffleAGet_cache(ctx, tmp);
  }
  auto tmp = ShuffleAGet_cache(ctx, in);
  return ShuffleASet_cache(ctx, tmp);
}

// --------------- NDSS Shuffle ---------------
namespace {
const std::map<size_t, size_t> kExtend4 = {
    {1 << 4, 19},  {1 << 5, 17},  {1 << 6, 16},  {1 << 7, 16},  {1 << 8, 15},
    {1 << 9, 14},  {1 << 10, 14}, {1 << 11, 14}, {1 << 12, 14}, {1 << 13, 13},
    {1 << 14, 13}, {1 << 15, 13}, {1 << 16, 13}, {1 << 17, 13}, {1 << 18, 12},
    {1 << 19, 12}, {1 << 20, 12}};

const std::map<size_t, size_t> kExtend5 = {
    {1 << 4, 17},  {1 << 5, 15},  {1 << 6, 14},  {1 << 7, 14},  {1 << 8, 14},
    {1 << 9, 13},  {1 << 10, 13}, {1 << 11, 12}, {1 << 12, 12}, {1 << 13, 11},
    {1 << 14, 11}, {1 << 15, 11}, {1 << 16, 11}, {1 << 17, 11}, {1 << 18, 11},
    {1 << 19, 10}, {1 << 20, 10}};

const std::map<size_t, size_t> kExtend6 = {
    {1 << 4, 16},  {1 << 5, 14},  {1 << 6, 13},  {1 << 7, 13},  {1 << 8, 12},
    {1 << 9, 12},  {1 << 10, 11}, {1 << 11, 11}, {1 << 12, 11}, {1 << 13, 10},
    {1 << 14, 10}, {1 << 15, 10}, {1 << 16, 10}, {1 << 17, 10}, {1 << 18, 10},
    {1 << 19, 9},  {1 << 20, 9}};

const std::map<size_t, size_t> kExtend7 = {
    {1 << 4, 15}, {1 << 5, 13},  {1 << 6, 12},  {1 << 7, 12},  {1 << 8, 11},
    {1 << 9, 11}, {1 << 10, 10}, {1 << 11, 10}, {1 << 12, 10}, {1 << 13, 9},
    {1 << 14, 9}, {1 << 15, 9},  {1 << 16, 9},  {1 << 17, 9},  {1 << 18, 9},
    {1 << 19, 8}, {1 << 20, 8}};

const std::map<size_t, size_t> kExtend8 = {
    {1 << 4, 14}, {1 << 5, 12}, {1 << 6, 11}, {1 << 7, 11}, {1 << 8, 10},
    {1 << 9, 10}, {1 << 10, 9}, {1 << 11, 9}, {1 << 12, 9}, {1 << 13, 8},
    {1 << 14, 8}, {1 << 15, 8}, {1 << 16, 8}, {1 << 17, 8}, {1 << 18, 8},
    {1 << 19, 7}, {1 << 20, 7}};

const std::map<size_t, size_t> kExtend9 = {
    {1 << 4, 14}, {1 << 5, 12}, {1 << 6, 11}, {1 << 7, 11}, {1 << 8, 10},
    {1 << 9, 10}, {1 << 10, 9}, {1 << 11, 9}, {1 << 12, 9}, {1 << 13, 8},
    {1 << 14, 8}, {1 << 15, 8}, {1 << 16, 8}, {1 << 17, 8}, {1 << 18, 8},
    {1 << 19, 7}, {1 << 20, 7}};

const std::map<size_t, size_t> kExtend10 = {
    {1 << 4, 12}, {1 << 5, 11}, {1 << 6, 10}, {1 << 7, 9},  {1 << 8, 9},
    {1 << 9, 8},  {1 << 10, 8}, {1 << 11, 8}, {1 << 12, 8}, {1 << 13, 7},
    {1 << 14, 7}, {1 << 15, 7}, {1 << 16, 7}, {1 << 17, 7}, {1 << 18, 7},
    {1 << 19, 6}, {1 << 20, 6}};

size_t ShuffleFindB(const std::map<size_t, size_t>& mapping, size_t num) {
  const size_t kMagicSize = 1 << 12;
  if (num <= kMagicSize) {
    return mapping.at(kMagicSize);
  }

  auto it = mapping.lower_bound(num);
  if (it == mapping.end()) {
    SPDLOG_INFO("[Warning] num is too large");
    return mapping.crbegin()->second;
  }
  return it->second;
}

size_t ShuffleFindB(size_t T, size_t num) {
  auto logT = yacl::math::Log2Ceil(T);

  if (logT <= 4) {
    return ShuffleFindB(kExtend4, num);
  }

  size_t B = 1;
  switch (logT) {
    case 5:
      B = ShuffleFindB(kExtend5, num);
      break;
    case 6:
      B = ShuffleFindB(kExtend6, num);
      break;
    case 7:
      B = ShuffleFindB(kExtend7, num);
      break;
    case 8:
      B = ShuffleFindB(kExtend8, num);
      break;
    case 9:
      B = ShuffleFindB(kExtend9, num);
      break;
    default:
      B = ShuffleFindB(kExtend10, num);
      break;
  }
  return B;
}
}  // namespace

std::vector<PTy> ShuffleCompose_2k(size_t i, size_t num,
                                   const std::vector<std::vector<PTy>>& ins,
                                   size_t repeat) {
  YACL_ENFORCE(repeat > 0);
  YACL_ENFORCE(ins[0].size() % repeat == 0);

  const auto T_num = ins.size();
  const auto T = ins[0].size() / repeat;

  YACL_ENFORCE((T & (T - 1)) == 0);
  YACL_ENFORCE(T * T_num == num);

  const auto num_bits = yacl::math::Log2Ceil(num);
  const auto T_bits = yacl::math::Log2Ceil(T);

  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;

  if (2 * i > depth) {
    i = depth - i - 1;
  }

  std::vector<PTy> out(num * repeat);

  int stride = num_bits - (i + 1) * T_bits;
  if (stride < 0) {
    stride = 0;
  }
  const auto step = 1 << stride;

  uint32_t offset = 0;
  for (size_t i = 0; i < T_num; ++i) {
    for (size_t j = 0; j < T; ++j) {
      for (size_t p = 0; p < repeat; ++p) {
        out[offset + j * step + num * p] = ins[i][j + T * p];
      }
    }

    offset += 1;
    int rest = offset % (T * step);
    if (rest == step) {
      offset += step * (T - 1);
    }
  }

  return out;
}

std::vector<size_t> ShuffleCompose_2k(
    size_t i, size_t num, const std::vector<std::vector<size_t>>& perms) {
  const auto T_num = perms.size();
  const auto T = perms[0].size();

  YACL_ENFORCE(T * T_num == num);

  const auto num_bits = yacl::math::Log2Ceil(num);
  const auto T_bits = yacl::math::Log2Ceil(T);

  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // const auto depth = 2 * (num_bits - T_bits) + 1;

  if (2 * i > depth) {
    i = depth - i - 1;
  }

  std::vector<size_t> out(num);

  int stride = num_bits - (i + 1) * T_bits;
  if (stride < 0) {
    stride = 0;
  }
  const auto step = 1 << stride;

  uint32_t offset = 0;
  for (size_t i = 0; i < T_num; ++i) {
    for (size_t j = 0; j < T; ++j) {
      out[offset + j * step] = offset + perms[i][j] * step;
    }

    offset += 1;
    int rest = offset % (T * step);
    if (rest == step) {
      offset += step * (T - 1);
    }
  }

  return out;
}

std::vector<ATy> ShuffleAGet_2k(std::shared_ptr<Context>& ctx, const size_t T,
                                absl::Span<const ATy> in) {
  const size_t num = in.size();
  YACL_ENFORCE((num & (num - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);
  const size_t T_num = num / T;
  YACL_ENFORCE(num % T == 0);

  const size_t num_bits = yacl::math::Log2Ceil(num);
  const size_t T_bits = yacl::math::Log2Ceil(T);
  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // correlation
  // [Warning] low efficiency!!! optimize it
  auto prot = ctx->GetState<Protocol>();

  std::vector<ATy> ret(num);
  const size_t ext = ShuffleFindB(T, num);
  SPDLOG_INFO("Shuffle2k ext is {} with depth {}", ext, depth);
  for (size_t i = 0; i < depth; ++i) {
    for (size_t _ = 0; _ < ext; ++_) {
      std::vector<std::vector<PTy>> vec_a;
      std::vector<std::vector<PTy>> vec_b;

      for (size_t t = 0; t < T_num; ++t) {
        auto [ext_a, ext_b] = ctx->GetState<Correlation>()->ShuffleGet(T, 2);
        vec_a.emplace_back(std::move(ext_a));
        vec_b.emplace_back(std::move(ext_b));
      }

      auto ext_a = ShuffleCompose_2k(i, num, vec_a, 2);
      auto ext_b = ShuffleCompose_2k(i, num, vec_b, 2);

      YACL_ENFORCE(ext_a.size() == 2 * num);
      YACL_ENFORCE(ext_b.size() == 2 * num);

      ret = ShuffleAGet_internal(ctx, in, ext_a, ext_b);
      in = absl::MakeConstSpan(ret);
      // Delay Check
      prot->NdssBufferAppend(ret);
    }
  }
  return ret;
}

std::vector<ATy> ShuffleAGet_2k_cache(std::shared_ptr<Context>& ctx,
                                      const size_t T,
                                      absl::Span<const ATy> in) {
  const size_t num = in.size();
  YACL_ENFORCE((num & (num - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);
  const size_t T_num = num / T;
  YACL_ENFORCE(num % T == 0);
  const size_t num_bits = yacl::math::Log2Ceil(num);
  const size_t T_bits = yacl::math::Log2Ceil(T);
  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // correlation
  // [Warning] low efficiency!!! optimize it

  const size_t ext = ShuffleFindB(T, num);
  for (size_t i = 0; i < depth; ++i) {
    for (size_t _ = 0; _ < ext; ++_) {
      for (size_t t = 0; t < T_num; ++t) {
        ctx->GetState<Correlation>()->ShuffleGet_cache(T, 2);
      }
    }
  }
  return std::vector<ATy>(num);
}

std::vector<ATy> ShuffleASet_2k(std::shared_ptr<Context>& ctx, const size_t T,
                                absl::Span<const ATy> in) {
  const size_t num = in.size();
  YACL_ENFORCE((num & (num - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);
  const size_t T_num = num / T;
  YACL_ENFORCE(num % T == 0);

  const size_t num_bits = yacl::math::Log2Ceil(num);
  const size_t T_bits = yacl::math::Log2Ceil(T);
  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;

  // correlation
  // [Warning] low efficiency!!! optimize it
  auto prot = ctx->GetState<Protocol>();
  const size_t ext = ShuffleFindB(T, num);
  SPDLOG_INFO("Shuffle2k ext is {} with depth {}", ext, depth);

  std::vector<ATy> ret(num);
  for (size_t i = 0; i < depth; ++i) {
    for (size_t _ = 0; _ < ext; ++_) {
      std::vector<std::vector<PTy>> vec_delta;
      std::vector<std::vector<size_t>> vec_perm;
      for (size_t t = 0; t < T_num; ++t) {
        auto [ext_delta, perm] = ctx->GetState<Correlation>()->ShuffleSet(T, 2);
        vec_delta.emplace_back(std::move(ext_delta));
        vec_perm.emplace_back(std::move(perm));
      }

      auto ext_delta = ShuffleCompose_2k(i, num, vec_delta, 2);
      auto perm = ShuffleCompose_2k(i, num, vec_perm);

      YACL_ENFORCE(ext_delta.size() == 2 * num);
      YACL_ENFORCE(perm.size() == num);

      ret = ShuffleASet_internal(ctx, in, perm, ext_delta);
      in = absl::MakeConstSpan(ret);
      // Delay Check
      prot->NdssBufferAppend(ret);
    }
  }
  return ret;
}

std::vector<ATy> ShuffleASet_2k_cache(std::shared_ptr<Context>& ctx,
                                      const size_t T,
                                      absl::Span<const ATy> in) {
  const size_t num = in.size();
  YACL_ENFORCE((num & (num - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);
  const size_t T_num = num / T;
  YACL_ENFORCE(num % T == 0);
  const size_t num_bits = yacl::math::Log2Ceil(num);
  const size_t T_bits = yacl::math::Log2Ceil(T);
  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // correlation
  // [Warning] low efficiency!!! optimize it
  const size_t ext = ShuffleFindB(T, num);
  for (size_t i = 0; i < depth; ++i) {
    for (size_t _ = 0; _ < ext; ++_) {
      for (size_t t = 0; t < T_num; ++t) {
        ctx->GetState<Correlation>()->ShuffleSet_cache(T, 2);
      }
    }
  }
  return std::vector<ATy>(num);
}

std::vector<ATy> ShuffleA_2k(std::shared_ptr<Context>& ctx, const size_t T,
                             absl::Span<const ATy> in) {
  if (ctx->GetRank() == 0) {
    auto tmp = ShuffleASet_2k(ctx, T, in);
    return ShuffleAGet_2k(ctx, T, tmp);
  }
  auto tmp = ShuffleAGet_2k(ctx, T, in);
  return ShuffleASet_2k(ctx, T, tmp);
}

std::vector<ATy> ShuffleA_2k_cache(std::shared_ptr<Context>& ctx,
                                   const size_t T, absl::Span<const ATy> in) {
  if (ctx->GetRank() == 0) {
    auto tmp = ShuffleASet_2k_cache(ctx, T, in);
    return ShuffleAGet_2k_cache(ctx, T, tmp);
  }
  auto tmp = ShuffleAGet_2k_cache(ctx, T, in);
  return ShuffleASet_2k_cache(ctx, T, tmp);
}

// --------------- Secure Shuffle ---------------
std::vector<ATy> SShuffleASet(std::shared_ptr<Context>& ctx,
                              absl::Span<const ATy> in) {
  const size_t num = in.size();
  auto cr = ctx->GetState<Correlation>();

  auto [perm, _a, _b] = cr->ASTSet(num);
  auto mask_A = SubAA(ctx, absl::MakeConstSpan(in), absl::MakeConstSpan(_a));
  auto mask_P = A2P_delay(ctx, absl::MakeConstSpan(mask_A));

  auto shuffle_mask_P = ZerosP(ctx, num);
  for (size_t i = 0; i < num; ++i) {
    shuffle_mask_P[perm[i]] = mask_P[i];
  }

  auto c = SetA(ctx, absl::MakeConstSpan(shuffle_mask_P));
  auto ret = AddAA(ctx, absl::MakeConstSpan(c), absl::MakeConstSpan(_b));

  auto rand_p = RandP(ctx, 1);
  auto ones_p = OnesP(ctx, num);
  auto ext_rand_p = ScalarMulPP(ctx, rand_p[0], ones_p);
  auto in_sub_rand = SubAP(ctx, in, ext_rand_p);
  auto product_in = NMulA(ctx, in_sub_rand);

  auto ret_sub_rand = SubAP(ctx, ret, ext_rand_p);
  auto product_ret = NMulA(ctx, ret_sub_rand);
  auto check_zeros = SubAA(ctx, product_in, product_ret);

  auto rand_a = RandA(ctx, 1);  // mask
  auto check_zeros_mul = MulAA(ctx, check_zeros, rand_a);
  auto zeros = A2P_delay(ctx, check_zeros_mul);

  YACL_ENFORCE(zeros.size() == 1 && zeros[0] == PTy(0));

  return ret;
}

std::vector<ATy> SShuffleASet_cache(std::shared_ptr<Context>& ctx,
                                    absl::Span<const ATy> in) {
  const size_t num = in.size();
  auto cr = ctx->GetState<Correlation>();

  cr->ASTSet_cache(num);
  auto _a = ZerosA_cache(ctx, num);
  auto _b = ZerosA_cache(ctx, num);
  auto mask_A = SubAA_cache(ctx, absl::MakeSpan(in), absl::MakeConstSpan(_a));
  auto mask_P = A2P_delay_cache(ctx, absl::MakeConstSpan(mask_A));
  auto c = SetA_cache(ctx, mask_P);
  auto ret = AddAA_cache(ctx, absl::MakeConstSpan(c), absl::MakeConstSpan(_b));

  auto rand_p = RandP_cache(ctx, 1);
  auto ones_p = OnesP_cache(ctx, num);
  auto ext_rand_p = ScalarMulPP_cache(ctx, rand_p[0], ones_p);
  auto in_sub_rand = SubAP_cache(ctx, in, ext_rand_p);
  auto product_in = NMulA_cache(ctx, in_sub_rand);

  auto ret_sub_rand = SubAP_cache(ctx, ret, ext_rand_p);
  auto product_ret = NMulA_cache(ctx, ret_sub_rand);
  auto check_zeros = SubAA_cache(ctx, product_in, product_ret);

  auto rand_a = RandA_cache(ctx, 1);
  auto check_zeros_mul = MulAA_cache(ctx, check_zeros, rand_a);
  [[maybe_unused]] auto zeros = A2P_delay_cache(ctx, check_zeros_mul);

  return ret;
}

std::vector<ATy> SShuffleAGet(std::shared_ptr<Context>& ctx,
                              absl::Span<const ATy> in) {
  const size_t num = in.size();
  auto cr = ctx->GetState<Correlation>();

  auto [_a, _b] = cr->ASTGet(num);
  auto mask_A = SubAA(ctx, absl::MakeConstSpan(in), absl::MakeConstSpan(_a));
  auto mask_P = A2P_delay(ctx, absl::MakeConstSpan(mask_A));

  auto c = GetA(ctx, num);
  auto ret = AddAA(ctx, absl::MakeConstSpan(c), absl::MakeConstSpan(_b));

  auto rand_p = RandP(ctx, 1);
  auto ones_p = OnesP(ctx, num);
  auto ext_rand_p = ScalarMulPP(ctx, rand_p[0], ones_p);
  auto in_sub_rand = SubAP(ctx, in, ext_rand_p);
  auto product_in = NMulA(ctx, in_sub_rand);

  auto ret_sub_rand = SubAP(ctx, ret, ext_rand_p);
  auto product_ret = NMulA(ctx, ret_sub_rand);
  auto check_zeros = SubAA(ctx, product_in, product_ret);

  auto rand_a = RandA(ctx, 1);  // mask
  auto check_zeros_mul = MulAA(ctx, check_zeros, rand_a);
  auto zeros = A2P_delay(ctx, check_zeros_mul);

  YACL_ENFORCE(zeros.size() == 1 && zeros[0] == PTy(0));

  return ret;
}

std::vector<ATy> SShuffleAGet_cache(std::shared_ptr<Context>& ctx,
                                    absl::Span<const ATy> in) {
  const size_t num = in.size();
  auto cr = ctx->GetState<Correlation>();

  cr->ASTGet_cache(num);
  auto _a = ZerosA_cache(ctx, num);
  auto _b = ZerosA_cache(ctx, num);
  auto mask_A =
      SubAA_cache(ctx, absl::MakeConstSpan(in), absl::MakeConstSpan(_a));
  auto mask_P = A2P_delay_cache(ctx, absl::MakeConstSpan(mask_A));

  auto c = GetA_cache(ctx, num);
  auto ret = AddAA_cache(ctx, absl::MakeConstSpan(c), absl::MakeConstSpan(_b));

  auto rand_p = RandP_cache(ctx, 1);
  auto ones_p = OnesP_cache(ctx, num);
  auto ext_rand_p = ScalarMulPP_cache(ctx, rand_p[0], ones_p);
  auto in_sub_rand = SubAP_cache(ctx, in, ext_rand_p);
  auto product_in = NMulA_cache(ctx, in_sub_rand);

  auto ret_sub_rand = SubAP_cache(ctx, ret, ext_rand_p);
  auto product_ret = NMulA_cache(ctx, ret_sub_rand);
  auto check_zeros = SubAA_cache(ctx, product_in, product_ret);

  auto rand_a = RandA_cache(ctx, 1);
  auto check_zeros_mul = MulAA_cache(ctx, check_zeros, rand_a);
  [[maybe_unused]] auto zeros = A2P_delay_cache(ctx, check_zeros_mul);

  return ret;
}

std::vector<ATy> SShuffleA(std::shared_ptr<Context>& ctx,
                           absl::Span<const ATy> in) {
  if (ctx->GetRank() == 0) {
    auto tmp = SShuffleASet(ctx, in);
    return SShuffleAGet(ctx, tmp);
  } else {
    auto tmp = SShuffleAGet(ctx, in);
    return SShuffleASet(ctx, tmp);
  }
}

std::vector<ATy> SShuffleA_cache(std::shared_ptr<Context>& ctx,
                                 absl::Span<const ATy> in) {
  if (ctx->GetRank() == 0) {
    auto tmp = SShuffleASet_cache(ctx, in);
    return SShuffleAGet_cache(ctx, tmp);
  } else {
    auto tmp = SShuffleAGet_cache(ctx, in);
    return SShuffleASet_cache(ctx, tmp);
  }
}

std::vector<ATy> NMulA(std::shared_ptr<Context>& ctx,
                       absl::Span<const ATy> in) {
  const size_t num = in.size();
  auto cr = ctx->GetState<Correlation>();

  auto [_r, _mul_inv] = cr->NMul(num);
  auto in_mul_r_A =
      MulAA(ctx, absl::MakeConstSpan(in), absl::MakeConstSpan(_r));
  auto in_mul_r_P = A2P_delay(ctx, in_mul_r_A);

  auto product =
      std::reduce(in_mul_r_P.begin(), in_mul_r_P.end(), PTy(1), PTy::Mul);

  std::vector<ATy> lhs = {_mul_inv};
  std::vector<PTy> rhs = {product};

  return MulAP(ctx, absl::MakeConstSpan(lhs), absl::MakeConstSpan(rhs));
}

std::vector<ATy> NMulA_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const ATy> in) {
  const size_t num = in.size();
  auto cr = ctx->GetState<Correlation>();

  cr->NMul_cache(num);
  std::vector<ATy> _r(num, {PTy(0), PTy(0)});
  auto in_mul_r_A =
      MulAA_cache(ctx, absl::MakeConstSpan(in), absl::MakeConstSpan(_r));
  [[maybe_unused]] auto in_mul_r_P = A2P_delay_cache(ctx, in_mul_r_A);

  std::vector<ATy> lhs = {
      ATy{PTy(1), PTy(ctx->GetState<Protocol>()->GetKey())}};
  std::vector<PTy> rhs = {PTy(1)};

  return MulAP_cache(ctx, absl::MakeConstSpan(lhs), absl::MakeConstSpan(rhs));
}

// --------------- Special ---------------

// A-share Setter, return A-share ( in , in * key + r )
std::vector<ATy> SetA(std::shared_ptr<Context>& ctx, absl::Span<const PTy> in) {
  const size_t num = in.size();
  auto rand = RandASet(ctx, num);
  auto [val, mac] = Unpack(absl::MakeConstSpan(rand));
  // reuse, diff = in - val
  auto diff = op::Sub(absl::MakeConstSpan(in), absl::MakeConstSpan(val));
  ctx->GetConnection()->SendAsync(
      ctx->NextRank(), yacl::ByteContainerView(diff.data(), num * sizeof(PTy)),
      "SetA");
  // extra = diff * key
  auto diff_mac = op::ScalarMul(ctx->GetState<Protocol>()->GetKey(),
                                absl::MakeConstSpan(diff));
  // mac = diff_mac + mac
  op::AddInplace(absl::MakeSpan(mac), absl::MakeConstSpan(diff_mac));
  return Pack(absl::MakeConstSpan(in), absl::MakeConstSpan(mac));
}

std::vector<ATy> SetA_cache(std::shared_ptr<Context>& ctx,
                            absl::Span<const PTy> in) {
  const size_t num = in.size();
  return RandASet_cache(ctx, num);
}
// A-share Getter, return A-share (  0 , in * key - r )
std::vector<ATy> GetA(std::shared_ptr<Context>& ctx, size_t num) {
  auto zero = RandAGet(ctx, num);
  auto [val, mac] = Unpack(absl::MakeConstSpan(zero));
  auto buff = ctx->GetConnection()->Recv(ctx->NextRank(), "SetA");
  // diff
  auto diff = absl::MakeSpan(reinterpret_cast<PTy*>(buff.data()), num);
  auto diff_mac = op::ScalarMul(ctx->GetState<Protocol>()->GetKey(),
                                absl::MakeConstSpan(diff));
  // mac = diff_mac + mac
  op::AddInplace(absl::MakeSpan(mac), absl::MakeConstSpan(diff_mac));
  return Pack(absl::MakeConstSpan(val), absl::MakeConstSpan(mac));
}

std::vector<ATy> GetA_cache(std::shared_ptr<Context>& ctx, size_t num) {
  return RandAGet_cache(ctx, num);
}

std::vector<ATy> RandASet(std::shared_ptr<Context>& ctx, size_t num) {
  return ctx->GetState<Correlation>()->RandomSet(num).data;
}

std::vector<ATy> RandASet_cache(std::shared_ptr<Context>& ctx, size_t num) {
  ctx->GetState<Correlation>()->RandomSet_cache(num);
  return std::vector<ATy>(num);
}

std::vector<ATy> RandAGet(std::shared_ptr<Context>& ctx, size_t num) {
  return ctx->GetState<Correlation>()->RandomGet(num).data;
}

std::vector<ATy> RandAGet_cache(std::shared_ptr<Context>& ctx, size_t num) {
  ctx->GetState<Correlation>()->RandomGet_cache(num);
  return std::vector<ATy>(num);
}

std::vector<ATy> SumA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                      absl::Span<const ATy> in) {
  const size_t num = in.size();
  std::vector<ATy> ret(1, {0, 0});
  for (size_t i = 0; i < num; ++i) {
    ret[0].val = ret[0].val + in[i].val;
    ret[0].mac = ret[0].mac + in[i].mac;
  }
  return ret;
}

std::vector<ATy> SumA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                            absl::Span<const ATy> in) {
  const size_t num = in.size();
  return std::vector<ATy>(num);
}

std::vector<ATy> FilterA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                         absl::Span<const ATy> in,
                         absl::Span<const size_t> indexes) {
  const size_t ret_num = indexes.size();
  const size_t in_num = in.size();
  YACL_ENFORCE(ret_num <= in_num);
  std::vector<ATy> ret(ret_num);
  for (size_t i = 0; i < ret_num; ++i) {
    ret[i] = in[indexes[i]];
  }
  return ret;
}

std::vector<ATy> FilterA_cache([[maybe_unused]] std::shared_ptr<Context>& ctx,
                               [[maybe_unused]] absl::Span<const ATy> in,
                               absl::Span<const size_t> indexes) {
  const size_t ret_num = indexes.size();
  return std::vector<ATy>(ret_num);
}

// one or zero
std::vector<ATy> ZeroOneA(std::shared_ptr<Context>& ctx, size_t num) {
  auto r = RandA(ctx, num);
  // s = r * r
  auto s = MulAA(ctx, r, r);
  // reveal s
  auto p = A2P_delay(ctx, s);
  auto root = op::Sqrt(absl::MakeSpan(p));

  auto tmp = DivAP(ctx, r, root);
  auto one = OnesP(ctx, num);
  auto res = AddAP(ctx, tmp, one);

  auto inv_two = PTy::Inv(PTy(2));
  return ScalarMulPA(ctx, inv_two, res);
}

std::vector<ATy> ZeroOneA_cache(std::shared_ptr<Context>& ctx, size_t num) {
  auto r = RandA_cache(ctx, num);
  // s = r * r
  auto s = MulAA_cache(ctx, r, r);
  // reveal s
  auto p = A2P_delay_cache(ctx, s);
  auto root = op::Sqrt(absl::MakeSpan(p));

  auto tmp = DivAP_cache(ctx, r, root);
  auto one = OnesP_cache(ctx, num);
  auto res = AddAP_cache(ctx, tmp, one);

  auto inv_two = PTy::Inv(PTy(2));
  return ScalarMulPA_cache(ctx, inv_two, res);
}

std::vector<ATy> ScalarMulPA([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             const PTy& scalar, absl::Span<const ATy> in) {
  const size_t num = in.size();
  std::vector<ATy> ret(in.begin(), in.end());
  op::ScalarMulInplace(
      scalar, absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), 2 * num));
  return ret;
}

std::vector<ATy> ScalarMulPA_cache(
    [[maybe_unused]] std::shared_ptr<Context>& ctx,
    [[maybe_unused]] const PTy& scalar, absl::Span<const ATy> in) {
  const size_t num = in.size();
  return std::vector<ATy>(num);
}

std::vector<ATy> ScalarMulAP([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             const ATy& scalar, absl::Span<const PTy> in) {
  auto val = op::ScalarMul(scalar.val, in);
  auto mac = op::ScalarMul(scalar.mac, in);
  return Pack(val, mac);
}

std::vector<ATy> ScalarMulAP_cache(
    [[maybe_unused]] std::shared_ptr<Context>& ctx,
    [[maybe_unused]] const ATy& scalar, absl::Span<const PTy> in) {
  const size_t num = in.size();
  return std::vector<ATy>(num);
}

std::pair<std::vector<ATy>, std::vector<ATy>> RandFairA(
    std::shared_ptr<Context>& ctx, size_t num) {
  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;

  const size_t bits_num = num * sizeof(INTEGER) * 8;
  auto bits = ZeroOneA(ctx, bits_num);
  auto bits_span = absl::MakeSpan(bits);
  std::vector<ATy> ret(num, {PTy::Zero(), PTy::Zero()});

  auto scalar = PTy::One();
  for (size_t i = 0; i < sizeof(INTEGER) * 8; ++i) {
    auto cur_bits = bits_span.subspan(i * num, num);
    auto tmp = ScalarMulPA(ctx, scalar, cur_bits);
    op::AddInplace(
        absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), 2 * num),
        absl::MakeConstSpan(reinterpret_cast<const PTy*>(tmp.data()), 2 * num));
    scalar = scalar * PTy(2);
  }
  return {ret, bits};
}

std::pair<std::vector<ATy>, std::vector<ATy>> RandFairA_cache(
    std::shared_ptr<Context>& ctx, size_t num) {
  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;

  const size_t bits_num = num * sizeof(INTEGER) * 8;
  auto bits = ZeroOneA_cache(ctx, bits_num);
  auto bits_span = absl::MakeSpan(bits);
  std::vector<ATy> ret(num, {PTy::Zero(), PTy::Zero()});

  auto scalar = PTy::One();
  for (size_t i = 0; i < sizeof(INTEGER) * 8; ++i) {
    auto cur_bits = bits_span.subspan(i * num, num);
    [[maybe_unused]] auto tmp = ScalarMulPA_cache(ctx, scalar, cur_bits);
  }
  return {ret, bits};
}

std::vector<PTy> FairA2P(std::shared_ptr<Context>& ctx,
                         absl::Span<const ATy> in, absl::Span<const ATy> bits) {
  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;
  const size_t num = in.size();
  YACL_ENFORCE(num * sizeof(INTEGER) * 8 == bits.size());

  std::vector<PTy> ret(num, PTy::Zero());
  auto scalar = PTy::One();
  for (size_t i = 0; i < sizeof(INTEGER) * 8; ++i) {
    auto bits_p = A2P_delay(ctx, bits.subspan(num * i, num));
    for (const auto& bit_p : bits_p) {
      YACL_ENFORCE(bit_p == PTy::One() || bit_p == PTy::Zero());
    }
    auto tmp = op::ScalarMul(scalar, bits_p);
    scalar = scalar * PTy(2);
    op::AddInplace(absl::MakeSpan(ret), absl::MakeConstSpan(tmp));
  }

  auto check = SubAP(ctx, in, ret);
  auto zeros = A2P_delay(ctx, check);
  for (const auto& zero : zeros) {
    YACL_ENFORCE(zero == PTy::Zero());
  }

  return ret;
}

std::vector<PTy> FairA2P_cache(std::shared_ptr<Context>& ctx,
                               absl::Span<const ATy> in,
                               absl::Span<const ATy> bits) {
  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;
  const size_t num = in.size();
  YACL_ENFORCE(num * sizeof(INTEGER) * 8 == bits.size());

  std::vector<PTy> ret(num, PTy::Zero());
  for (size_t i = 0; i < sizeof(INTEGER) * 8; ++i) {
    [[maybe_unused]] auto bits_p =
        A2P_delay_cache(ctx, bits.subspan(num * i, num));
  }

  auto check = SubAP_cache(ctx, in, ret);
  [[maybe_unused]] auto zeros = A2P_delay_cache(ctx, check);
  return ret;
}

}  // namespace mosac::internal
