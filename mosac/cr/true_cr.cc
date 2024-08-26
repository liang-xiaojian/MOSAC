#include "mosac/cr/true_cr.h"

#include "mosac/cr/utils/ot_helper.h"
#include "mosac/ss/type.h"
#include "mosac/utils/vec_op.h"

namespace mosac {

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
  auto aA_cC = OpenAndCheck(auth_abcAC_span.subspan(3 * num, 2 * num));
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

// TODO: implementation it
std::vector<size_t> TrueCorrelation::ASTSet(absl::Span<internal::ATy> a,
                                            absl::Span<internal::ATy> b) {
  const size_t num = a.size();
  YACL_ENFORCE(num == b.size());
  auto conn = ctx_->GetConnection();

  const size_t B = 40;
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
    auto p = OpenAndCheck(absl::MakeConstSpan(b));
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

  auto o = OpenAndCheck(absl::MakeConstSpan(zeros));
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

  const size_t B = 40;
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
    [[maybe_unused]] auto p = OpenAndCheck(absl::MakeConstSpan(b));

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

  auto o = OpenAndCheck(absl::MakeConstSpan(zeros));
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
  auto sync_seed = conn->SyncSeed();
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

// std::vector<internal::ATy> TrueCorrelation::Mul(
//     absl::Span<const internal::ATy> lhs, absl::Span<const internal::ATy> rhs)
//     {
//   YACL_ENFORCE(lhs.size() == rhs.size());
//   const size_t num = lhs.size();

//   auto a = std::vector<internal::ATy>(num, {0, 0});
//   auto b = std::vector<internal::ATy>(num, {0, 0});
//   auto c = std::vector<internal::ATy>(num, {0, 0});
//   BeaverTriple(absl::MakeSpan(a), absl::MakeSpan(b), absl::MakeSpan(c));

//   auto u = internal::op::Sub(
//       absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(lhs.data()),
//                           2 * num),
//       absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(a.data()),
//                           2 * num));
//   auto v = internal::op::Sub(
//       absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(rhs.data()),
//                           2 * num),
//       absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(b.data()),
//                           2 * num));

//   auto u_p = OpenAndCheck(absl::MakeConstSpan(
//       reinterpret_cast<const internal::ATy*>(u.data()), num));
//   auto v_p = OpenAndCheck(absl::MakeConstSpan(
//       reinterpret_cast<const internal::ATy*>(v.data()), num));

//   auto xyb = Mul(lhs, absl::MakeConstSpan(v_p));
//   auto xay = Mul(absl::MakeConstSpan(u_p), rhs);
//   auto xayb_val =
//       internal::op::Mul(absl::MakeConstSpan(u_p), absl::MakeConstSpan(v_p));
//   auto xayb_mac = internal::op::ScalarMul(key_,
//   absl::MakeConstSpan(xayb_val));

//   if (ctx_->GetRank() != 0) {
//     xayb_val = internal::op::Zeros(num);
//   }

//   auto xayb = internal::Pack(absl::MakeConstSpan(xayb_val),
//                              absl::MakeConstSpan(xayb_mac));

//   internal::op::AddInplace(
//       absl::MakeSpan(reinterpret_cast<internal::PTy*>(c.data()), 2 * num),
//       absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(xyb.data()),
//                           2 * num));

//   internal::op::AddInplace(
//       absl::MakeSpan(reinterpret_cast<internal::PTy*>(c.data()), 2 * num),
//       absl::MakeConstSpan(reinterpret_cast<const internal::PTy*>(xay.data()),
//                           2 * num));

//   internal::op::SubInplace(
//       absl::MakeSpan(reinterpret_cast<internal::PTy*>(c.data()), 2 * num),
//       absl::MakeConstSpan(reinterpret_cast<const
//       internal::PTy*>(xayb.data()),
//                           2 * num));

//   return c;
// }

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

  auto diff = OpenAndCheck(absl::MakeConstSpan(a));
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
  auto pub = OpenAndCheck(absl::MakeConstSpan(mul));
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
  auto aA_cC = OpenAndCheck(auth_acAC_span.subspan(2 * num, 2 * num));
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

}  // namespace mosac
