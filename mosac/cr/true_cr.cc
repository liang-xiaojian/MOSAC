#include "mosac/cr/true_cr.h"

#include "mosac/cr/utils/ot_helper.h"
#include "mosac/ss/type.h"
#include "mosac/utils/vec_op.h"
#include "yacl/math/gadget.h"

namespace mosac {

namespace {
const std::map<size_t, size_t> kExtend = {
    {1 << 4, 9},  {1 << 5, 8},  {1 << 6, 7},  {1 << 7, 6},  {1 << 8, 6},
    {1 << 9, 5},  {1 << 10, 5}, {1 << 11, 5}, {1 << 12, 4}, {1 << 13, 4},
    {1 << 14, 4}, {1 << 15, 4}, {1 << 16, 4}, {1 << 17, 4}, {1 << 18, 4},
    {1 << 19, 3}, {1 << 20, 3}};

size_t findB(size_t num) {
  const size_t kMagicSize = 1 << 12;
  if (num <= kMagicSize) {
    return kExtend.at(kMagicSize);
  }

  auto it = kExtend.lower_bound(num);
  if (it == kExtend.end()) {
    SPDLOG_INFO("[Warning] num is too large");
    return kExtend.crbegin()->second;
  }
  return it->second;
}

}  // namespace

void TrueCorrelation::BeaverTriple(absl::Span<internal::ATy> a,
                                   absl::Span<internal::ATy> b,
                                   absl::Span<internal::ATy> c) {
  const size_t num = c.size();
  YACL_ENFORCE(num == a.size());
  YACL_ENFORCE(num == b.size());

  auto p_abcAC = internal::op::Zeros(num * 5);
  auto p_abcAC_span = absl::MakeSpan(p_abcAC);
  auto p_a = p_abcAC_span.subspan(0 * num, num);
  auto p_b = p_abcAC_span.subspan(1 * num, num);
  auto p_c = p_abcAC_span.subspan(2 * num, num);
  auto p_A = p_abcAC_span.subspan(3 * num, num);
  auto p_C = p_abcAC_span.subspan(4 * num, num);

  internal::op::Rand(absl::MakeSpan(p_b));

  auto conn = ctx_->GetConnection();
  ot::OtHelper(ot_sender_, ot_receiver_)
      .BeaverTripleExtendWithChosenB(conn, p_a, p_b, p_c, p_A, p_C);

  std::vector<internal::ATy> auth_abcAC(num * 5);
  auto auth_abcAC_span = absl::MakeSpan(auth_abcAC);

  std::vector<internal::ATy> remote_auth_abcAC(num * 5);
  auto remote_auth_abcAC_span = absl::MakeSpan(remote_auth_abcAC);

  if (ctx_->GetRank() == 0) {
    AuthSet(p_abcAC_span, auth_abcAC_span);
    AuthGet(remote_auth_abcAC_span);
  } else {
    AuthGet(remote_auth_abcAC_span);
    AuthSet(p_abcAC_span, auth_abcAC_span);
  }

  // length double
  internal::op::Add(
      absl::MakeConstSpan(
          reinterpret_cast<const internal::PTy*>(auth_abcAC.data()), 10 * num),
      absl::MakeConstSpan(
          reinterpret_cast<const internal::PTy*>(remote_auth_abcAC.data()),
          10 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_abcAC.data()),
                     10 * num));

  // return value
  auto auth_a = auth_abcAC_span.subspan(0 * num, num);
  auto auth_b = auth_abcAC_span.subspan(1 * num, num);
  auto auth_c = auth_abcAC_span.subspan(2 * num, num);
  memcpy(a.data(), auth_a.data(), num * sizeof(internal::ATy));
  memcpy(b.data(), auth_b.data(), num * sizeof(internal::ATy));
  memcpy(c.data(), auth_c.data(), num * sizeof(internal::ATy));

  // ---- consistency check ----
  auto auth_A = auth_abcAC_span.subspan(3 * num, num);
  auto auth_C = auth_abcAC_span.subspan(4 * num, num);
  auto seed = conn->SyncSeed();
  auto p_coef = internal::op::Rand(seed, num);
  std::vector<internal::ATy> coef(num, {0, 0});
  std::transform(p_coef.cbegin(), p_coef.cend(), coef.begin(),
                 [](const internal::PTy& val) -> internal::ATy {
                   return {val, val};
                 });

  internal::op::Mul(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_A.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(coef.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_A.data()), 2 * num));

  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_a.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_A.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_A.data()), 2 * num));

  internal::op::Mul(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_C.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(coef.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_C.data()), 2 * num));

  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_c.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_C.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_C.data()), 2 * num));

  // internal::PTy type
  auto aA_cC = OpenAndDelayCheck(auth_abcAC_span.subspan(3 * num, 2 * num));
  auto aA_cC_span = absl::MakeSpan(aA_cC);
  auto aA = aA_cC_span.subspan(0, num);
  auto cC = aA_cC_span.subspan(num, num);

  internal::op::Mul(aA, p_b, aA);

  auto buf = conn->Exchange(
      yacl::ByteContainerView(aA.data(), num * sizeof(internal::PTy)));
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) ==
               num * sizeof(internal::PTy));
  auto remote_aA =
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(buf.data()), num);

  internal::op::Add(aA, remote_aA, aA);
  for (size_t i = 0; i < num; ++i) {
    YACL_ENFORCE(cC[i] == aA[i], "{} : cC is {}", i, cC[i].GetVal());
  }
  // ---- consistency check ----
}

void TrueCorrelation::RandomSet(absl::Span<internal::ATy> out) {
  const size_t num = out.size();
  std::vector<internal::PTy> a(num);
  std::vector<internal::PTy> b(num);
  // a * remote_key + b = remote_c
  vole_receiver_->rrecv(absl::MakeSpan(a), absl::MakeSpan(b));
  // mac = a * key_
  auto mac = internal::op::ScalarMul(key_, absl::MakeConstSpan(a));
  // a's mac = a * local_key - b
  internal::op::Sub(absl::MakeConstSpan(mac), absl::MakeConstSpan(b),
                    absl::MakeSpan(mac));
  // Pack
  internal::Pack(absl::MakeConstSpan(a), absl::MakeConstSpan(mac),
                 absl::MakeSpan(out));
}

void TrueCorrelation::RandomGet(absl::Span<internal::ATy> out) {
  const size_t num = out.size();
  std::vector<internal::PTy> c(num);
  // remote_a * key_ + remote_b = c
  vole_sender_->rsend(absl::MakeSpan(c));
  // Pack
  auto zeros = internal::op::Zeros(num);
  internal::Pack(absl::MakeConstSpan(zeros), absl::MakeConstSpan(c),
                 absl::MakeSpan(out));
}

void TrueCorrelation::RandomAuth(absl::Span<internal::ATy> out) {
  const size_t num = out.size();
  std::vector<internal::ATy> zeros(num);
  std::vector<internal::ATy> rands(num);
  if (ctx_->GetRank() == 0) {
    RandomSet(absl::MakeSpan(rands));
    RandomGet(absl::MakeSpan(zeros));
  } else {
    RandomGet(absl::MakeSpan(zeros));
    RandomSet(absl::MakeSpan(rands));
  }
  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(zeros.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(rands.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(out.data()), 2 * num));
}

void TrueCorrelation::ShuffleSet(absl::Span<const size_t> perm,
                                 absl::Span<internal::PTy> delta,
                                 size_t repeat) {
  auto conn = ctx_->GetConnection();
  const size_t batch_size = perm.size();
  const size_t full_size = delta.size();
  YACL_ENFORCE(full_size == batch_size * repeat);

  ot::OtHelper(ot_sender_, ot_receiver_).ShuffleSend(conn, perm, delta, repeat);
}

void TrueCorrelation::ShuffleGet(absl::Span<internal::PTy> a,
                                 absl::Span<internal::PTy> b, size_t repeat) {
  auto conn = ctx_->GetConnection();

  const size_t full_size = a.size();
  const size_t batch_size = a.size() / repeat;
  YACL_ENFORCE(full_size == b.size());
  YACL_ENFORCE(full_size == batch_size * repeat);

  ot::OtHelper(ot_sender_, ot_receiver_).ShuffleRecv(conn, a, b, repeat);
}

void TrueCorrelation::BatchShuffleSet(
    size_t batch_num, size_t per_size, size_t repeat,
    const std::vector<std::vector<size_t>>& perms,
    std::vector<std::vector<internal::PTy>>& vec_delta) {
  auto conn = ctx_->GetConnection();
  vec_delta.clear();

  ot::OtHelper(ot_sender_, ot_receiver_)
      .BatchShuffleSend(conn, batch_num, per_size, repeat, perms, vec_delta);

  // for (size_t i = 0; i < batch_num; ++i) {
  //   auto& perm = perms[i];
  //   std::vector<internal::PTy> tmp_delta(per_size * repeat);
  //   ShuffleSet(absl::MakeConstSpan(perm), absl::MakeSpan(tmp_delta), repeat);

  //   vec_delta.emplace_back(std::move(tmp_delta));
  // }
}

void TrueCorrelation::BatchShuffleGet(
    size_t batch_num, size_t per_size, size_t repeat,
    std::vector<std::vector<internal::PTy>>& vec_a,
    std::vector<std::vector<internal::PTy>>& vec_b) {
  auto conn = ctx_->GetConnection();
  vec_a.clear();
  vec_b.clear();

  ot::OtHelper(ot_sender_, ot_receiver_)
      .BatchShuffleRecv(conn, batch_num, per_size, repeat, vec_a, vec_b);

  // for (size_t i = 0; i < batch_num; ++i) {
  //   std::vector<internal::PTy> tmp_a(per_size * repeat);
  //   std::vector<internal::PTy> tmp_b(per_size * repeat);
  //   ShuffleGet(absl::MakeSpan(tmp_a), absl::MakeSpan(tmp_b), repeat);

  //   vec_a.emplace_back(std::move(tmp_a));
  //   vec_b.emplace_back(std::move(tmp_b));
  // }
}

// AST
std::vector<size_t> TrueCorrelation::ASTSet(absl::Span<internal::ATy> a,
                                            absl::Span<internal::ATy> b) {
  const size_t num = a.size();
  YACL_ENFORCE(num == b.size());
  auto conn = ctx_->GetConnection();

  const size_t B = 10;
  auto perm = GenPerm(num);
  std::vector<internal::ATy> rand(num);
  RandomAuth(absl::MakeSpan(rand));
  ot::OtHelper(ot_sender_, ot_receiver_).ASTSend(conn, perm, rand, a, b);

  std::vector<internal::ATy> zeros(B);
  std::vector<internal::ATy> xs(B);
  RandomAuth(absl::MakeSpan(xs));

  for (size_t i = 0; i < B; ++i) {
    auto _perm = GenPerm(num);
    std::vector<internal::ATy> _rand(num);
    std::vector<internal::ATy> _a(num);
    std::vector<internal::ATy> _b(num);

    RandomAuth(absl::MakeSpan(_rand));
    ot::OtHelper(ot_sender_, ot_receiver_)
        .ASTSend(conn, _perm, _rand, absl::MakeSpan(_a), absl::MakeSpan(_b));

    internal::op::SubInplace(
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(b.data()),
                       b.size() * 2),
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(_a.data()),
                       _a.size() * 2));
    auto p = OpenAndDelayCheck(absl::MakeConstSpan(b));
    std::vector<internal::PTy> shuffle_p(num);
    std::vector<size_t> shuffle_perm(num);
    for (size_t i = 0; i < num; ++i) {
      shuffle_p[_perm[i]] = p[i];
      shuffle_perm[i] = _perm[perm[i]];
    }

    std::vector<internal::ATy> shuffle_p_A(num);
    AuthSet(absl::MakeConstSpan(shuffle_p), absl::MakeSpan(shuffle_p_A));

    internal::op::AddInplace(
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(_b.data()),
                       _b.size() * 2),
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(shuffle_p_A.data()),
                       shuffle_p_A.size() * 2));

    auto _b_x = _Func(absl::MakeConstSpan(_b), xs[i]);
    auto a_x = _Func(absl::MakeConstSpan(a), xs[i]);
    zeros[i] = {_b_x.val - a_x.val, _b_x.mac - a_x.mac};

    std::copy(_b.begin(), _b.end(), b.begin());
    std::swap(perm, shuffle_perm);
  }

  auto o = OpenAndDelayCheck(absl::MakeConstSpan(zeros));
  for (size_t i = 0; i < B; ++i) {
    YACL_ENFORCE(o[i] == internal::PTy(0));
  }
  return perm;
}

void TrueCorrelation::ASTGet(absl::Span<internal::ATy> a,
                             absl::Span<internal::ATy> b) {
  const size_t num = a.size();
  YACL_ENFORCE(num == b.size());
  auto conn = ctx_->GetConnection();

  const size_t B = 10;
  std::vector<internal::ATy> rand(num);
  RandomAuth(absl::MakeSpan(rand));
  ot::OtHelper(ot_sender_, ot_receiver_).ASTRecv(conn, rand, a, b);

  std::vector<internal::ATy> zeros(B);
  std::vector<internal::ATy> xs(B);
  RandomAuth(absl::MakeSpan(xs));

  for (size_t i = 0; i < B; ++i) {
    std::vector<internal::ATy> _rand(num);
    std::vector<internal::ATy> _a(num);
    std::vector<internal::ATy> _b(num);

    RandomAuth(absl::MakeSpan(_rand));
    ot::OtHelper(ot_sender_, ot_receiver_)
        .ASTRecv(conn, _rand, absl::MakeSpan(_a), absl::MakeSpan(_b));

    internal::op::SubInplace(
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(b.data()),
                       b.size() * 2),
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(_a.data()),
                       _a.size() * 2));
    [[maybe_unused]] auto p = OpenAndDelayCheck(absl::MakeConstSpan(b));

    std::vector<internal::ATy> shuffle_p_A(num);
    AuthGet(absl::MakeSpan(shuffle_p_A));

    internal::op::AddInplace(
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(_b.data()),
                       _b.size() * 2),
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(shuffle_p_A.data()),
                       shuffle_p_A.size() * 2));

    auto _b_x = _Func(absl::MakeConstSpan(_b), xs[i]);
    auto a_x = _Func(absl::MakeConstSpan(a), xs[i]);
    zeros[i] = {_b_x.val - a_x.val, _b_x.mac - a_x.mac};

    std::copy(_b.begin(), _b.end(), b.begin());
  }

  auto o = OpenAndDelayCheck(absl::MakeConstSpan(zeros));
  for (size_t i = 0; i < B; ++i) {
    YACL_ENFORCE(o[i] == internal::PTy(0));
  }
}

internal::ATy TrueCorrelation::NMul(absl::Span<internal::ATy> r) {
  RandomAuth(absl::MakeSpan(r));
  std::vector<internal::ATy> tmp(1);
  tmp[0] = _NMul(absl::MakeConstSpan(r));
  return Inv(absl::MakeConstSpan(tmp))[0];
}

void TrueCorrelation::AuthSet(absl::Span<const internal::PTy> in,
                              absl::Span<internal::ATy> out) {
  RandomSet(out);
  auto [val, mac] = internal::Unpack(out);
  // val = in - val
  internal::op::Sub(absl::MakeConstSpan(in), absl::MakeConstSpan(val),
                    absl::MakeSpan(val));

  auto conn = ctx_->GetConnection();
  conn->SendAsync(
      conn->NextRank(),
      yacl::ByteContainerView(val.data(), val.size() * sizeof(internal::PTy)),
      "AuthSet");

  internal::op::Add(
      absl::MakeConstSpan(mac),
      absl::MakeConstSpan(internal::op::ScalarMul(key_, absl::MakeSpan(val))),
      absl::MakeSpan(mac));
  auto ret = internal::Pack(in, mac);
  memcpy(out.data(), ret.data(), out.size() * sizeof(internal::ATy));
}

void TrueCorrelation::AuthGet(absl::Span<internal::ATy> out) {
  RandomGet(out);
  // val = 0
  auto [val, mac] = internal::Unpack(out);

  auto conn = ctx_->GetConnection();
  auto recv_buf = conn->Recv(conn->NextRank(), "AuthSet");

  auto diff = absl::MakeSpan(reinterpret_cast<internal::PTy*>(recv_buf.data()),
                             out.size());

  internal::op::Add(absl::MakeConstSpan(mac),
                    absl::MakeConstSpan(internal::op::ScalarMul(key_, diff)),
                    absl::MakeSpan(mac));
  auto ret = internal::Pack(val, mac);
  memcpy(out.data(), ret.data(), ret.size() * sizeof(internal::ATy));
}

// Copy from A2P
std::vector<internal::PTy> TrueCorrelation::OpenAndCheck(
    absl::Span<const internal::ATy> in) {
  const size_t size = in.size();
  auto [val, mac] = internal::Unpack(absl::MakeSpan(in));
  auto conn = ctx_->GetConnection();
  auto val_bv =
      yacl::ByteContainerView(val.data(), size * sizeof(internal::PTy));
  std::vector<internal::PTy> real_val(size);

  auto buf = conn->Exchange(val_bv);
  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(buf.data()),
                          size),
      absl::MakeConstSpan(val), absl::MakeSpan(real_val));

  // Generate Sync Seed After open Value
  // auto sync_seed = conn->SyncSeed();

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

  auto zero_mac = mac_affine - real_val_affine * key_;

  auto remote_mac_uint = conn->ExchangeWithCommit(zero_mac.GetVal());
  YACL_ENFORCE(zero_mac + internal::PTy(remote_mac_uint) ==
               internal::PTy::Zero());
  return real_val;
}

std::vector<internal::PTy> TrueCorrelation::OpenAndDelayCheck(
    absl::Span<const internal::ATy> in) {
  const size_t size = in.size();
  auto [val, mac] = internal::Unpack(absl::MakeSpan(in));
  auto conn = ctx_->GetConnection();
  auto val_bv =
      yacl::ByteContainerView(val.data(), size * sizeof(internal::PTy));
  std::vector<internal::PTy> real_val(size);

  auto buf = conn->Exchange(val_bv);
  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(buf.data()),
                          size),
      absl::MakeConstSpan(val), absl::MakeSpan(real_val));

  // Generate Sync Seed After open Value
  // auto sync_seed = conn->SyncSeed();

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

  auto zero_mac = mac_affine - real_val_affine * key_;

  if (ctx_->GetRank() == 0) {
    delay_check_buff_.push_back(zero_mac);
  } else {
    delay_check_buff_.push_back(internal::PTy::Neg(zero_mac));
  }

  return real_val;
}

bool TrueCorrelation::DelayCheck() {
  if (delay_check_buff_.size() == 0) {
    return true;
  }

  auto conn = ctx_->GetConnection();

  auto hash_val = yacl::crypto::Sm3(yacl::ByteContainerView(
      delay_check_buff_.data(),
      delay_check_buff_.size() * sizeof(internal::PTy)));

  auto remote_hash_val = conn->ExchangeWithCommit(
      yacl::ByteContainerView(hash_val.data(), hash_val.size()));

  auto flag = (yacl::ByteContainerView(hash_val.data(), hash_val.size()) ==
               yacl::ByteContainerView(remote_hash_val));

  delay_check_buff_.clear();

  SPDLOG_INFO("delay check result: {} ", flag);

  return flag;
}

// Copy from MulAA && MulAP && MulPA
std::vector<internal::ATy> TrueCorrelation::Mul(
    absl::Span<const internal::PTy> lhs, absl::Span<const internal::ATy> rhs) {
  return Mul(rhs, lhs);
}

std::vector<internal::ATy> TrueCorrelation::Mul(
    absl::Span<const internal::ATy> lhs, absl::Span<const internal::PTy> rhs) {
  auto [val, mac] = internal::Unpack(absl::MakeConstSpan(lhs));
  internal::op::MulInplace(absl::MakeSpan(val), absl::MakeConstSpan(rhs));
  internal::op::MulInplace(absl::MakeSpan(mac), absl::MakeConstSpan(rhs));
  return internal::Pack(absl::MakeSpan(val), absl::MakeSpan(mac));
}

std::vector<internal::ATy> TrueCorrelation::Mul(
    absl::Span<const internal::ATy> lhs, absl::Span<const internal::ATy> rhs) {
  YACL_ENFORCE(lhs.size() == rhs.size());
  const auto num = lhs.size();

  auto a = std::vector<internal::ATy>(num, {0, 0});
  auto c = std::vector<internal::ATy>(num, {0, 0});

  BeaverTripleWithChosenB(absl::MakeSpan(a), rhs, absl::MakeSpan(c));

  internal::op::Sub(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(lhs.data()),
                          num * 2),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(a.data()),
                          num * 2),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(a.data()), num * 2));

  auto diff = OpenAndDelayCheck(absl::MakeConstSpan(a));
  auto diff_mul_rhs = Mul(absl::MakeConstSpan(diff), absl::MakeConstSpan(rhs));

  internal::op::AddInplace(
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(c.data()), num * 2),
      absl::MakeConstSpan(
          reinterpret_cast<const internal::PTy*>(diff_mul_rhs.data()),
          num * 2));

  return c;
}

internal::ATy TrueCorrelation::_NMul(absl::Span<const internal::ATy> in) {
  const size_t num = in.size();

  auto lrhs = in;
  size_t remain = num;
  std::vector<internal::ATy> tmp;

  while (remain > 1) {
    size_t half_size = lrhs.size() / 2;
    auto lhs = lrhs.subspan(0, half_size);
    auto rhs = lrhs.subspan(half_size, half_size);
    auto last = lrhs[lrhs.size() - 1];

    tmp = Mul(lhs, rhs);
    if (remain % 2 == 1) {
      tmp.emplace_back(last);
    }
    lrhs = absl::MakeConstSpan(tmp);
    remain = tmp.size();
  }
  YACL_ENFORCE(tmp.size() == 1);
  return tmp[0];
}

internal::ATy TrueCorrelation::_Func(absl::Span<const internal::ATy> in,
                                     const internal::ATy& x) {
  const size_t num = in.size();
  std::vector<internal::ATy> ext_x(num, x);

  internal::op::SubInplace(
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(ext_x.data()), num * 2),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(in.data()),
                          num * 2));
  return _NMul(absl::MakeConstSpan(ext_x));
}

// Copy from InvA
std::vector<internal::ATy> TrueCorrelation::Inv(
    absl::Span<const internal::ATy> in) {
  const size_t num = in.size();
  // r <-- random a-share
  std::vector<internal::ATy> r(num);
  RandomAuth(absl::MakeSpan(r));

  auto mul = Mul(absl::MakeConstSpan(in), absl::MakeConstSpan(r));
  auto pub = OpenAndDelayCheck(absl::MakeConstSpan(mul));
  auto inv = internal::op::Inv(absl::MakeConstSpan(pub));
  return Mul(absl::MakeConstSpan(r), absl::MakeConstSpan(inv));
}

// Beaver Triple With Chosen B
void TrueCorrelation::BeaverTripleWithChosenB(absl::Span<internal::ATy> a,
                                              absl::Span<const internal::ATy> b,
                                              absl::Span<internal::ATy> c) {
  const size_t num = c.size();
  YACL_ENFORCE(num == a.size());
  YACL_ENFORCE(num == b.size());

  auto p_b = internal::ExtractVal(b);

  auto p_acAC = internal::op::Zeros(num * 4);
  auto p_acAC_span = absl::MakeSpan(p_acAC);
  auto p_a = p_acAC_span.subspan(0 * num, num);
  auto p_c = p_acAC_span.subspan(1 * num, num);
  auto p_A = p_acAC_span.subspan(2 * num, num);
  auto p_C = p_acAC_span.subspan(3 * num, num);

  auto conn = ctx_->GetConnection();
  ot::OtHelper(ot_sender_, ot_receiver_)
      .BeaverTripleExtendWithChosenB(conn, p_a, p_b, p_c, p_A, p_C);

  std::vector<internal::ATy> auth_acAC(num * 4);
  auto auth_acAC_span = absl::MakeSpan(auth_acAC);

  std::vector<internal::ATy> remote_auth_acAC(num * 4);
  auto remote_auth_acAC_span = absl::MakeSpan(remote_auth_acAC);

  if (ctx_->GetRank() == 0) {
    AuthSet(p_acAC_span, auth_acAC_span);
    AuthGet(remote_auth_acAC_span);
  } else {
    AuthGet(remote_auth_acAC_span);
    AuthSet(p_acAC_span, auth_acAC_span);
  }

  // length double
  internal::op::Add(
      absl::MakeConstSpan(
          reinterpret_cast<const internal::PTy*>(auth_acAC.data()), 8 * num),
      absl::MakeConstSpan(
          reinterpret_cast<const internal::PTy*>(remote_auth_acAC.data()),
          8 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_acAC.data()),
                     8 * num));

  // return value
  auto auth_a = auth_acAC_span.subspan(0 * num, num);
  auto auth_c = auth_acAC_span.subspan(1 * num, num);
  memcpy(a.data(), auth_a.data(), num * sizeof(internal::ATy));
  memcpy(c.data(), auth_c.data(), num * sizeof(internal::ATy));

  // ---- consistency check ----
  auto auth_A = auth_acAC_span.subspan(2 * num, num);
  auto auth_C = auth_acAC_span.subspan(3 * num, num);
  auto seed = conn->SyncSeed();
  auto p_coef = internal::op::Rand(seed, num);
  std::vector<internal::ATy> coef(num, {0, 0});
  std::transform(p_coef.cbegin(), p_coef.cend(), coef.begin(),
                 [](const internal::PTy& val) -> internal::ATy {
                   return {val, val};
                 });

  internal::op::Mul(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_A.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(coef.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_A.data()), 2 * num));

  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_a.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_A.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_A.data()), 2 * num));

  internal::op::Mul(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_C.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(coef.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_C.data()), 2 * num));

  internal::op::Add(
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_c.data()),
                          2 * num),
      absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(auth_C.data()),
                          2 * num),
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(auth_C.data()), 2 * num));

  // internal::PTy type
  auto aA_cC = OpenAndDelayCheck(auth_acAC_span.subspan(2 * num, 2 * num));
  auto aA_cC_span = absl::MakeSpan(aA_cC);
  auto aA = aA_cC_span.subspan(0, num);
  auto cC = aA_cC_span.subspan(num, num);

  internal::op::Mul(aA, p_b, aA);

  auto buf = conn->Exchange(
      yacl::ByteContainerView(aA.data(), num * sizeof(internal::PTy)));
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) ==
               num * sizeof(internal::PTy));
  auto remote_aA =
      absl::MakeSpan(reinterpret_cast<internal::PTy*>(buf.data()), num);

  internal::op::Add(aA, remote_aA, aA);
  for (size_t i = 0; i < num; ++i) {
    YACL_ENFORCE(cC[i] == aA[i], "{} : cC is {}", i, cC[i].GetVal());
  }
  // ---- consistency check ----
}

std::vector<size_t> TrueCorrelation::ASTSet_2k(size_t T,
                                               absl::Span<internal::ATy> a,
                                               absl::Span<internal::ATy> b) {
  const auto num = a.size();
  YACL_ENFORCE(a.size() == b.size());
  YACL_ENFORCE((num & (num - 1)) == 0);  // num = 2^n
  YACL_ENFORCE((T & (T - 1)) == 0);      // T = 2^m
  YACL_ENFORCE(num >= T);                // num should be greater than T

  const auto num_bits = yacl::math::Log2Ceil(num);
  const auto T_bits = yacl::math::Log2Ceil(T);

  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // const auto depth = 2 * (num_bits - T_bits) + 1;

  std::vector<size_t> perm(num);

  const auto T_num = num / T;

  // for (size_t i = 0; i < T_num; ++i) {
  //   vec_a_T.push_back(std::vector<internal::ATy>(T, {0, 0}));
  //   vec_b_T.push_back(std::vector<internal::ATy>(T, {0, 0}));
  // }

  SPDLOG_INFO("depth {}, T_num {}, total {}", depth, T_num, depth * T_num);

  std::vector<internal::ATy> zeros(depth - 1);
  std::vector<internal::ATy> xs(depth - 1);
  RandomAuth(absl::MakeSpan(xs));

  std::vector<std::vector<internal::ATy>> vec_a_T_all;
  std::vector<std::vector<internal::ATy>> vec_b_T_all;
  auto vec_perm_T_all =
      ASTSet_batch_basic_2k(T_num * depth, T, vec_a_T_all, vec_b_T_all);

  std::vector<std::vector<internal::ATy>> vec_a_T(T_num);
  std::vector<std::vector<internal::ATy>> vec_b_T(T_num);
  std::vector<std::vector<size_t>> vec_perm_T(T_num);

  for (size_t l = 0; l < depth; ++l) {
    // std::vector<std::vector<size_t>> vec_perm_T;
    // for (size_t i = 0; i < T_num; ++i) {
    //   auto perm_T = ASTSet_basic_2k(absl::MakeSpan(vec_a_T[i]),
    //                                 absl::MakeSpan(vec_b_T[i]));
    //   vec_perm_T.push_back(std::move(perm_T));
    // }
    // auto vec_perm_T = ASTSet_batch_basic_2k(T_num, T, vec_a_T, vec_b_T);

    for (size_t i = 0; i < T_num; ++i) {
      vec_perm_T[i] = std::move(vec_perm_T_all[l * T_num + i]);
      vec_a_T[i] = std::move(vec_a_T_all[l * T_num + i]);
      vec_b_T[i] = std::move(vec_b_T_all[l * T_num + i]);
    }

    if (l == 0) {
      compose_vec_2k(l, vec_a_T, absl::MakeSpan(a));
      compose_vec_2k(l, vec_b_T, absl::MakeSpan(b));
      compose_perm_2k(l, vec_perm_T, absl::MakeSpan(perm));
    } else {
      std::vector<internal::ATy> _a(num, {0, 0});
      std::vector<internal::ATy> _b(num, {0, 0});
      std::vector<size_t> _perm(num);

      compose_vec_2k(l, vec_a_T, absl::MakeSpan(_a));
      compose_vec_2k(l, vec_b_T, absl::MakeSpan(_b));
      compose_perm_2k(l, vec_perm_T, absl::MakeSpan(_perm));

      internal::op::SubInplace(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(b.data()),
                         b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_a.data()),
                         _a.size() * 2));
      auto p = OpenAndDelayCheck(absl::MakeConstSpan(b));
      std::vector<internal::PTy> shuffle_p(num);
      std::vector<size_t> shuffle_perm(num);
      for (size_t i = 0; i < num; ++i) {
        shuffle_p[_perm[i]] = p[i];
        shuffle_perm[i] = _perm[perm[i]];
      }

      std::vector<internal::ATy> shuffle_p_A(num);
      AuthSet(absl::MakeConstSpan(shuffle_p), absl::MakeSpan(shuffle_p_A));

      internal::op::AddInplace(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_b.data()),
                         _b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(shuffle_p_A.data()),
                         shuffle_p_A.size() * 2));

      auto _b_x = _Func(absl::MakeConstSpan(_b), xs[l - 1]);
      auto a_x = _Func(absl::MakeConstSpan(a), xs[l - 1]);
      zeros[l - 1] = {_b_x.val - a_x.val, _b_x.mac - a_x.mac};

      std::copy(_b.begin(), _b.end(), b.begin());
      std::swap(perm, shuffle_perm);
    }
  }

  auto o = OpenAndDelayCheck(absl::MakeConstSpan(zeros));
  for (size_t i = 0; i < depth - 1; ++i) {
    YACL_ENFORCE(o[i] == internal::PTy(0));
  }

  return perm;
}

void TrueCorrelation::ASTGet_2k(size_t T, absl::Span<internal::ATy> a,
                                absl::Span<internal::ATy> b) {
  const auto num = a.size();
  YACL_ENFORCE(a.size() == b.size());
  YACL_ENFORCE((num & (num - 1)) == 0);  // num = 2^n
  YACL_ENFORCE((T & (T - 1)) == 0);      // T = 2^m
  YACL_ENFORCE(num >= T);                // num should be greater than T

  const auto num_bits = yacl::math::Log2Ceil(num);
  const auto T_bits = yacl::math::Log2Ceil(T);

  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // const auto depth = 2 * (num_bits - T_bits) + 1;

  const auto T_num = num / T;

  // for (size_t i = 0; i < T_num; ++i) {
  //   vec_a_T.push_back(std::vector<internal::ATy>(T, {0, 0}));
  //   vec_b_T.push_back(std::vector<internal::ATy>(T, {0, 0}));
  // }

  SPDLOG_INFO("depth {}, T_num {}, total {}", depth, T_num, depth * T_num);

  std::vector<internal::ATy> zeros(depth - 1);
  std::vector<internal::ATy> xs(depth - 1);
  RandomAuth(absl::MakeSpan(xs));

  std::vector<std::vector<internal::ATy>> vec_a_T_all;
  std::vector<std::vector<internal::ATy>> vec_b_T_all;
  ASTGet_batch_basic_2k(T_num * depth, T, vec_a_T_all, vec_b_T_all);

  std::vector<std::vector<internal::ATy>> vec_a_T(T_num);
  std::vector<std::vector<internal::ATy>> vec_b_T(T_num);

  for (size_t l = 0; l < depth; ++l) {
    // ASTGet_batch_basic_2k(T_num, T, vec_a_T, vec_b_T);
    for (size_t i = 0; i < T_num; ++i) {
      vec_a_T[i] = std::move(vec_a_T_all[l * T_num + i]);
      vec_b_T[i] = std::move(vec_b_T_all[l * T_num + i]);
    }

    if (l == 0) {
      compose_vec_2k(l, vec_a_T, absl::MakeSpan(a));
      compose_vec_2k(l, vec_b_T, absl::MakeSpan(b));
    } else {
      std::vector<internal::ATy> _a(num, {0, 0});
      std::vector<internal::ATy> _b(num, {0, 0});

      compose_vec_2k(l, vec_a_T, absl::MakeSpan(_a));
      compose_vec_2k(l, vec_b_T, absl::MakeSpan(_b));

      internal::op::SubInplace(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(b.data()),
                         b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_a.data()),
                         _a.size() * 2));
      [[maybe_unused]] auto p = OpenAndDelayCheck(absl::MakeConstSpan(b));

      std::vector<internal::ATy> shuffle_p_A(num);
      AuthGet(absl::MakeSpan(shuffle_p_A));

      internal::op::AddInplace(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_b.data()),
                         _b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(shuffle_p_A.data()),
                         shuffle_p_A.size() * 2));

      auto _b_x = _Func(absl::MakeConstSpan(_b), xs[l - 1]);
      auto a_x = _Func(absl::MakeConstSpan(a), xs[l - 1]);
      zeros[l - 1] = {_b_x.val - a_x.val, _b_x.mac - a_x.mac};

      std::copy(_b.begin(), _b.end(), b.begin());
    }
  }

  auto o = OpenAndDelayCheck(absl::MakeConstSpan(zeros));
  for (size_t i = 0; i < depth - 1; ++i) {
    YACL_ENFORCE(o[i] == internal::PTy(0));
  }
}

std::vector<size_t> TrueCorrelation::ASTSet_basic_2k(
    absl::Span<internal::ATy> a, absl::Span<internal::ATy> b) {
  const size_t num = a.size();
  YACL_ENFORCE(num == b.size());
  YACL_ENFORCE((num & (num - 1)) == 0);  // num = 2^k

  auto conn = ctx_->GetConnection();

  auto perm = GenPerm(num);
  std::vector<internal::ATy> rand(num);
  RandomAuth(absl::MakeSpan(rand));
  ot::OtHelper(ot_sender_, ot_receiver_).ASTSend(conn, perm, rand, a, b);
  return perm;
}

void TrueCorrelation::ASTGet_basic_2k(absl::Span<internal::ATy> a,
                                      absl::Span<internal::ATy> b) {
  const size_t num = a.size();
  YACL_ENFORCE(num == b.size());
  YACL_ENFORCE((num & (num - 1)) == 0);  // num = 2^k

  auto conn = ctx_->GetConnection();

  std::vector<internal::ATy> rand(num);
  RandomAuth(absl::MakeSpan(rand));
  ot::OtHelper(ot_sender_, ot_receiver_).ASTRecv(conn, rand, a, b);
}

std::vector<std::vector<size_t>> TrueCorrelation::ASTSet_batch_basic_2k(
    size_t num, size_t T, std::vector<std::vector<internal::ATy>>& vec_a,
    std::vector<std::vector<internal::ATy>>& vec_b) {
  // YACL_ENFORCE((num & (num - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);

  auto ext = findB(num);  // NB choose and cut
  auto ext_num = num * ext;

  SPDLOG_INFO("T is {}, num is {}, num extend is {}", T, num, ext_num);

  std::vector<std::vector<size_t>> ext_perms;
  for (size_t i = 0; i < ext_num; ++i) {
    auto tmp_perm = GenPerm(T);
    ext_perms.push_back(std::move(tmp_perm));
  }

  auto conn = ctx_->GetConnection();

  std::vector<internal::ATy> rand(ext_num * T);
  RandomAuth(absl::MakeSpan(rand));

  ot::OtHelper(ot_sender_, ot_receiver_)
      .BatchASTSend(conn, ext_num, T, rand, ext_perms, vec_a, vec_b);

  auto seed = conn->SyncSeed();
  auto shuffle = GenPerm(seed, ext_num);

  std::vector<std::vector<internal::ATy>> vec_shuffle_a(ext_num);
  std::vector<std::vector<internal::ATy>> vec_shuffle_b(ext_num);
  std::vector<std::vector<size_t>> vec_shuffle_perms(ext_num);

  for (size_t i = 0; i < ext_num; ++i) {
    vec_shuffle_a[i] = std::move(vec_a[shuffle[i]]);
    vec_shuffle_b[i] = std::move(vec_b[shuffle[i]]);
    vec_shuffle_perms[i] = std::move(ext_perms[shuffle[i]]);
  }

  vec_a.clear();
  vec_b.clear();
  std::vector<std::vector<size_t>> perms;

  ASTSet_merge(ext_num, num, vec_shuffle_perms, vec_shuffle_a, vec_shuffle_b,
               perms, vec_a, vec_b);

  return perms;
}

void TrueCorrelation::ASTGet_batch_basic_2k(
    size_t num, size_t T, std::vector<std::vector<internal::ATy>>& vec_a,
    std::vector<std::vector<internal::ATy>>& vec_b) {
  // YACL_ENFORCE((num & (num - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);

  auto ext = findB(num);  // NB choose and cut
  auto ext_num = num * ext;

  SPDLOG_INFO("T is {}, num is {}, num extend is {}", T, num, ext_num);

  auto conn = ctx_->GetConnection();

  std::vector<internal::ATy> rand(ext_num * T);
  RandomAuth(absl::MakeSpan(rand));
  ot::OtHelper(ot_sender_, ot_receiver_)
      .BatchASTRecv(conn, ext_num, T, rand, vec_a, vec_b);

  auto seed = conn->SyncSeed();
  auto shuffle = GenPerm(seed, ext_num);

  std::vector<std::vector<internal::ATy>> vec_shuffle_a(ext_num);
  std::vector<std::vector<internal::ATy>> vec_shuffle_b(ext_num);

  for (size_t i = 0; i < ext_num; ++i) {
    vec_shuffle_a[i] = std::move(vec_a[shuffle[i]]);
    vec_shuffle_b[i] = std::move(vec_b[shuffle[i]]);
  }

  vec_a.clear();
  vec_b.clear();

  ASTGet_merge(ext_num, num, vec_shuffle_a, vec_shuffle_b, vec_a, vec_b);
}

void TrueCorrelation::compose_vec_2k(
    size_t i, const std::vector<std::vector<internal::ATy>>& ins,
    absl::Span<internal::ATy> out) {
  const auto num = out.size();

  const auto T_num = ins.size();
  const auto T = ins[0].size();

  YACL_ENFORCE(T * T_num == num);

  const auto num_bits = yacl::math::Log2Ceil(num);
  const auto T_bits = yacl::math::Log2Ceil(T);

  const auto depth = 2 * yacl::math::DivCeil(num_bits, T_bits) - 1;
  // const auto depth = 2 * (num_bits - T_bits) + 1;

  if (2 * i > depth) {
    i = depth - i - 1;
  }

  // const auto step = 1 << i;

  int stride = num_bits - (i + 1) * T_bits;
  if (stride < 0) {
    stride = 0;
  }
  const auto step = 1 << stride;

  uint32_t offset = 0;
  for (size_t i = 0; i < T_num; ++i) {
    for (size_t j = 0; j < T; ++j) {
      out[offset + j * step] = ins[i][j];
    }

    offset += 1;
    int rest = offset % (T * step);
    if (rest == step) {
      offset += step * (T - 1);
    }
  }
}

void TrueCorrelation::compose_perm_2k(
    size_t i, const std::vector<std::vector<size_t>>& perms,
    absl::Span<size_t> out) {
  const auto num = out.size();

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

  // const auto step = 1 << i;

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
}

void TrueCorrelation::ASTSet_merge(
    size_t in_num, size_t out_num, std::vector<std::vector<size_t>>& in_perms,
    std::vector<std::vector<internal::ATy>>& in_vec_a,
    std::vector<std::vector<internal::ATy>>& in_vec_b,
    std::vector<std::vector<size_t>>& out_perms,
    std::vector<std::vector<internal::ATy>>& out_vec_a,
    std::vector<std::vector<internal::ATy>>& out_vec_b) {
  const size_t ext = in_num / out_num;
  const size_t T = in_vec_a[0].size();

  SPDLOG_INFO("T is {} , ext is {}", T, ext);

  YACL_ENFORCE(in_num == in_perms.size());
  YACL_ENFORCE(in_num == in_vec_a.size());
  YACL_ENFORCE(in_num == in_vec_b.size());
  YACL_ENFORCE(ext * out_num == in_num);

  out_perms.clear();
  out_vec_a.clear();
  out_vec_b.clear();

  std::vector<internal::ATy> zeros(ext - 1);
  std::vector<internal::ATy> xs(ext - 1);
  RandomAuth(absl::MakeSpan(xs));

  for (size_t i = 0; i < out_num; ++i) {
    out_perms.push_back(std::move(in_perms[i]));
    out_vec_a.push_back(std::move(in_vec_a[i]));
    out_vec_b.push_back(std::move(in_vec_b[i]));
  }

  std::vector<internal::ATy> func0_buf(out_num * T);
  std::vector<internal::ATy> func1_buf(out_num * T);
  std::vector<internal::ATy> reveal_buf(out_num * T);
  auto reveal_span = absl::MakeSpan(reveal_buf);

  for (size_t k = 1; k < ext; ++k) {
    for (size_t j = 0; j < out_num; ++j) {
      // std::vector<internal::ATy>& a = out_vec_a[j];
      std::vector<internal::ATy>& b = out_vec_b[j];
      // std::vector<size_t>& perm = out_perms[j];

      std::vector<internal::ATy>& _a = in_vec_a[k * out_num + j];
      // std::vector<internal::ATy>& _b = in_vec_b[k * out_num + j];
      // std::vector<size_t>& _perm = in_perms[k * out_num + j];

      auto tmp_span = reveal_span.subspan(j * T, T);

      internal::op::Sub(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(b.data()),
                         b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_a.data()),
                         _a.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(tmp_span.data()),
                         T * 2));
    }

    auto reveal = OpenAndDelayCheck(reveal_span);

    for (size_t j = 0; j < out_num; ++j) {
      // std::vector<internal::ATy>& a = out_vec_a[j];
      // std::vector<internal::ATy>& b = out_vec_b[j];
      std::vector<size_t>& perm = out_perms[j];

      // std::vector<internal::ATy>& _a = in_vec_a[k * out_num + j];
      // std::vector<internal::ATy>& _b = in_vec_b[k * out_num + j];
      std::vector<size_t>& _perm = in_perms[k * out_num + j];

      auto tmp_span = absl::MakeSpan(reveal).subspan(j * T, T);

      std::vector<internal::PTy> shuffle_p(T);
      std::vector<size_t> shuffle_perm(T);
      for (size_t i = 0; i < T; ++i) {
        shuffle_p[_perm[i]] = tmp_span[i];
        shuffle_perm[i] = _perm[perm[i]];
      }

      std::swap(perm, shuffle_perm);
      std::copy(shuffle_p.begin(), shuffle_p.end(), tmp_span.begin());
    }

    std::vector<internal::ATy> reveal_shuffle(out_num * T);
    AuthSet(absl::MakeConstSpan(reveal), absl::MakeSpan(reveal_shuffle));

    for (size_t j = 0; j < out_num; ++j) {
      std::vector<internal::ATy>& a = out_vec_a[j];
      std::vector<internal::ATy>& b = out_vec_b[j];
      // std::vector<size_t>& perm = out_perms[j];

      // std::vector<internal::ATy>& _a = in_vec_a[k * out_num + j];
      std::vector<internal::ATy>& _b = in_vec_b[k * out_num + j];
      // std::vector<size_t>& _perm = in_perms[k * out_num + j];

      // auto tmp_span = absl::MakeSpan(reveal).subspan(j * T, T);

      // std::vector<internal::PTy> shuffle_p(T);
      // std::vector<size_t> shuffle_perm(T);
      // for (size_t i = 0; i < T; ++i) {
      //   shuffle_p[_perm[i]] = tmp_span[i];
      //   shuffle_perm[i] = _perm[perm[i]];
      // }

      // std::vector<internal::ATy> shuffle_p_A(T);
      // AuthSet(absl::MakeConstSpan(shuffle_p), absl::MakeSpan(shuffle_p_A));

      auto shuffle_p_A = absl::MakeSpan(reveal_shuffle).subspan(j * T, T);

      internal::op::AddInplace(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_b.data()),
                         _b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(shuffle_p_A.data()),
                         shuffle_p_A.size() * 2));

      std::copy(_b.begin(), _b.end(), func0_buf.begin() + j * T);
      std::copy(a.begin(), a.end(), func1_buf.begin() + j * T);

      std::copy(_b.begin(), _b.end(), b.begin());
      // std::swap(perm, shuffle_perm);
    }

    auto _b_x = _Func(absl::MakeConstSpan(func0_buf), xs[k - 1]);
    auto a_x = _Func(absl::MakeConstSpan(func1_buf), xs[k - 1]);
    zeros[k - 1] = {_b_x.val - a_x.val, _b_x.mac - a_x.mac};
  }

  auto o = OpenAndDelayCheck(absl::MakeConstSpan(zeros));
  for (size_t i = 0; i < ext - 1; ++i) {
    YACL_ENFORCE(o[i] == internal::PTy(0));
  }
  SPDLOG_INFO("Check Over");
}

void TrueCorrelation::ASTGet_merge(
    size_t in_num, size_t out_num,
    std::vector<std::vector<internal::ATy>>& in_vec_a,
    std::vector<std::vector<internal::ATy>>& in_vec_b,
    std::vector<std::vector<internal::ATy>>& out_vec_a,
    std::vector<std::vector<internal::ATy>>& out_vec_b) {
  const size_t ext = in_num / out_num;
  const size_t T = in_vec_a[0].size();

  YACL_ENFORCE(in_num == in_vec_a.size());
  YACL_ENFORCE(in_num == in_vec_b.size());
  YACL_ENFORCE(ext * out_num == in_num);

  out_vec_a.clear();
  out_vec_b.clear();

  std::vector<internal::ATy> zeros(ext - 1);
  std::vector<internal::ATy> xs(ext - 1);
  RandomAuth(absl::MakeSpan(xs));

  for (size_t i = 0; i < out_num; ++i) {
    out_vec_a.push_back(std::move(in_vec_a[i]));
    out_vec_b.push_back(std::move(in_vec_b[i]));
  }

  std::vector<internal::ATy> func0_buf(out_num * T);
  std::vector<internal::ATy> func1_buf(out_num * T);
  std::vector<internal::ATy> reveal_buf(out_num * T);
  auto reveal_span = absl::MakeSpan(reveal_buf);

  for (size_t k = 1; k < ext; ++k) {
    for (size_t j = 0; j < out_num; ++j) {
      // std::vector<internal::ATy>& a = out_vec_a[j];
      std::vector<internal::ATy>& b = out_vec_b[j];

      std::vector<internal::ATy>& _a = in_vec_a[k * out_num + j];
      // std::vector<internal::ATy>& _b = in_vec_b[k * out_num + j];

      auto tmp_span = reveal_span.subspan(j * T, T);

      internal::op::Sub(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(b.data()),
                         b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_a.data()),
                         _a.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(tmp_span.data()),
                         T * 2));
    }

    [[maybe_unused]] auto reveal = OpenAndDelayCheck(reveal_span);
    std::vector<internal::ATy> reveal_shuffle(out_num * T);
    AuthGet(absl::MakeSpan(reveal_shuffle));

    for (size_t j = 0; j < out_num; ++j) {
      std::vector<internal::ATy>& a = out_vec_a[j];
      std::vector<internal::ATy>& b = out_vec_b[j];

      // std::vector<internal::ATy>& _a = in_vec_a[k * out_num + j];
      std::vector<internal::ATy>& _b = in_vec_b[k * out_num + j];

      // std::vector<internal::ATy> shuffle_p_A(T);
      // AuthGet(absl::MakeSpan(shuffle_p_A));
      auto shuffle_p_A = absl::MakeSpan(reveal_shuffle).subspan(j * T, T);

      internal::op::AddInplace(
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(_b.data()),
                         _b.size() * 2),
          absl::MakeSpan(reinterpret_cast<internal::PTy*>(shuffle_p_A.data()),
                         shuffle_p_A.size() * 2));

      std::copy(_b.begin(), _b.end(), func0_buf.begin() + j * T);
      std::copy(a.begin(), a.end(), func1_buf.begin() + j * T);
      std::copy(_b.begin(), _b.end(), b.begin());
    }

    auto _b_x = _Func(absl::MakeConstSpan(func0_buf), xs[k - 1]);
    auto a_x = _Func(absl::MakeConstSpan(func1_buf), xs[k - 1]);
    zeros[k - 1] = {_b_x.val - a_x.val, _b_x.mac - a_x.mac};
  }

  auto o = OpenAndDelayCheck(absl::MakeConstSpan(zeros));
  for (size_t i = 0; i < ext - 1; ++i) {
    YACL_ENFORCE(o[i] == internal::PTy(0));
  }
}
}  // namespace mosac
