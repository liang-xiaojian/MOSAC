#include "mosac/cr/fake_cr.h"

#include "mosac/cr/param.h"
#include "mosac/utils/vec_op.h"

namespace mosac {

void FakeCorrelation::BeaverTriple(absl::Span<internal::ATy> a,
                                   absl::Span<internal::ATy> b,
                                   absl::Span<internal::ATy> c) {
  const size_t num = c.size();
  YACL_ENFORCE(num == a.size());
  YACL_ENFORCE(num == b.size());

  auto a0 = internal::op::Rand(*ctx_->GetState<Prg>(), num);
  auto a1 = internal::op::Rand(*ctx_->GetState<Prg>(), num);
  auto b0 = internal::op::Rand(*ctx_->GetState<Prg>(), num);
  auto b1 = internal::op::Rand(*ctx_->GetState<Prg>(), num);
  auto c0 = internal::op::Rand(*ctx_->GetState<Prg>(), num);

  auto aa = internal::op::Add(absl::MakeConstSpan(a0), absl::MakeConstSpan(a1));
  auto bb = internal::op::Add(absl::MakeConstSpan(b0), absl::MakeConstSpan(b1));
  auto cc = internal::op::Mul(absl::MakeConstSpan(aa), absl::MakeConstSpan(bb));
  auto c1 = internal::op::Sub(absl::MakeConstSpan(cc), absl::MakeConstSpan(c0));

  auto a_mac = internal::op::ScalarMul(key_, absl::MakeConstSpan(aa));
  auto b_mac = internal::op::ScalarMul(key_, absl::MakeConstSpan(bb));
  auto c_mac = internal::op::ScalarMul(key_, absl::MakeConstSpan(cc));

  if (ctx_->GetRank() == 0) {
    internal::Pack(absl::MakeConstSpan(a0), absl::MakeConstSpan(a_mac), a);
    internal::Pack(absl::MakeConstSpan(b0), absl::MakeConstSpan(b_mac), b);
    internal::Pack(absl::MakeConstSpan(c0), absl::MakeConstSpan(c_mac), c);
  } else {
    internal::Pack(absl::MakeConstSpan(a1), absl::MakeConstSpan(a_mac), a);
    internal::Pack(absl::MakeConstSpan(b1), absl::MakeConstSpan(b_mac), b);
    internal::Pack(absl::MakeConstSpan(c1), absl::MakeConstSpan(c_mac), c);
  }
}

void FakeCorrelation::RandomSet(absl::Span<internal::ATy> out) {
  const size_t num = out.size();
  auto rands = internal::op::Rand(*ctx_->GetState<Prg>(), num);
  auto mac = internal::op::ScalarMul(key_, absl::MakeSpan(rands));
  internal::Pack(absl::MakeConstSpan(rands), absl::MakeConstSpan(mac), out);
}

void FakeCorrelation::RandomGet(absl::Span<internal::ATy> out) {
  const size_t num = out.size();
  auto rands = internal::op::Rand(*ctx_->GetState<Prg>(), num);
  auto mac = internal::op::ScalarMul(key_, absl::MakeSpan(rands));
  auto zeros = internal::op::Zeros(num);
  internal::Pack(absl::MakeConstSpan(zeros), absl::MakeConstSpan(mac), out);
}

void FakeCorrelation::RandomAuth(absl::Span<internal::ATy> out) {
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

void FakeCorrelation::ShuffleSet(absl::Span<const size_t> perm,
                                 absl::Span<internal::PTy> delta,
                                 size_t repeat) {
  const size_t batch_size = perm.size();
  const size_t full_size = delta.size();
  YACL_ENFORCE(batch_size * repeat == full_size);

  std::vector<uint128_t> seeds(2);
  ctx_->GetState<Prg>()->Fill(absl::MakeSpan(seeds));
  auto a = internal::op::Rand(seeds[0], full_size);
  auto b = internal::op::Rand(seeds[1], full_size);

  for (size_t offset = 0; offset < full_size; offset += batch_size) {
    for (size_t i = 0; i < batch_size; ++i) {
      delta[offset + i] = a[offset + perm[i]] + b[offset + i];
    }
  }
  // delta = - \Pi(a) - b
  internal::op::Neg(absl::MakeConstSpan(delta), absl::MakeSpan(delta));
}

void FakeCorrelation::ShuffleGet(absl::Span<internal::PTy> a,
                                 absl::Span<internal::PTy> b, size_t repeat) {
  const size_t full_size = a.size();
  const size_t batch_size = full_size / repeat;
  YACL_ENFORCE(full_size == b.size());
  YACL_ENFORCE(full_size == batch_size * repeat);

  std::vector<uint128_t> seeds(2);
  ctx_->GetState<Prg>()->Fill(absl::MakeSpan(seeds));
  auto a_buf = internal::op::Rand(seeds[0], full_size);
  auto b_buf = internal::op::Rand(seeds[1], full_size);

  memcpy(a.data(), a_buf.data(), full_size * sizeof(internal::PTy));
  memcpy(b.data(), b_buf.data(), full_size * sizeof(internal::PTy));
}

void FakeCorrelation::BatchShuffleSet(
    size_t batch_num, size_t per_size, size_t repeat,
    const std::vector<std::vector<size_t>>& perms,
    std::vector<std::vector<internal::PTy>>& vec_delta) {
  vec_delta.clear();

  for (size_t i = 0; i < batch_num; ++i) {
    auto& perm = perms[i];
    std::vector<internal::PTy> tmp_delta(per_size * repeat);
    ShuffleSet(absl::MakeConstSpan(perm), absl::MakeSpan(tmp_delta), repeat);

    vec_delta.emplace_back(std::move(tmp_delta));
  }
}

void FakeCorrelation::BatchShuffleGet(
    size_t batch_num, size_t per_size, size_t repeat,
    std::vector<std::vector<internal::PTy>>& vec_a,
    std::vector<std::vector<internal::PTy>>& vec_b) {
  vec_a.clear();
  vec_b.clear();

  for (size_t i = 0; i < batch_num; ++i) {
    std::vector<internal::PTy> tmp_a(per_size * repeat);
    std::vector<internal::PTy> tmp_b(per_size * repeat);
    ShuffleGet(absl::MakeSpan(tmp_a), absl::MakeSpan(tmp_b), repeat);

    vec_a.emplace_back(std::move(tmp_a));
    vec_b.emplace_back(std::move(tmp_b));
  }
}

void FakeCorrelation::_BatchShuffleSet(
    size_t batch_num, size_t per_size, size_t repeat,
    const std::vector<std::vector<size_t>>& perms,
    std::vector<std::vector<internal::PTy>>& vec_delta) {
  vec_delta.clear();

  for (size_t i = 0; i < batch_num; ++i) {
    auto& perm = perms[i];
    std::vector<internal::PTy> tmp_delta(per_size * repeat);
    ShuffleSet(absl::MakeConstSpan(perm), absl::MakeSpan(tmp_delta), repeat);

    vec_delta.emplace_back(std::move(tmp_delta));
  }
}

void FakeCorrelation::_BatchShuffleGet(
    size_t batch_num, size_t per_size, size_t repeat,
    std::vector<std::vector<internal::PTy>>& vec_a,
    std::vector<std::vector<internal::PTy>>& vec_b) {
  vec_a.clear();
  vec_b.clear();

  for (size_t i = 0; i < batch_num; ++i) {
    std::vector<internal::PTy> tmp_a(per_size * repeat);
    std::vector<internal::PTy> tmp_b(per_size * repeat);
    ShuffleGet(absl::MakeSpan(tmp_a), absl::MakeSpan(tmp_b), repeat);

    vec_a.emplace_back(std::move(tmp_a));
    vec_b.emplace_back(std::move(tmp_b));
  }
}

std::vector<size_t> FakeCorrelation::ASTSet(absl::Span<internal::ATy> a,
                                            absl::Span<internal::ATy> b) {
  const size_t size = a.size();
  YACL_ENFORCE(size == b.size());
  std::vector<uint128_t> seeds(1);
  ctx_->GetState<Prg>()->Fill(absl::MakeSpan(seeds));
  std::vector<size_t> perm = GenPerm(seeds[0], size);
  RandomAuth(b);
  for (size_t i = 0; i < size; ++i) {
    b[perm[i]] = a[i];
  }
  return perm;
}

void FakeCorrelation::ASTGet_2k(size_t T, absl::Span<internal::ATy> a,
                                absl::Span<internal::ATy> b) {
  const size_t size = a.size();
  YACL_ENFORCE(size == b.size());

  YACL_ENFORCE((size & (size - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);

  std::vector<uint128_t> seeds(1);
  ctx_->GetState<Prg>()->Fill(absl::MakeSpan(seeds));
  std::vector<size_t> perm = GenPerm(seeds[0], size);
  RandomAuth(b);
  for (size_t i = 0; i < size; ++i) {
    b[perm[i]] = a[i];
  }
}

std::vector<size_t> FakeCorrelation::ASTSet_2k(size_t T,
                                               absl::Span<internal::ATy> a,
                                               absl::Span<internal::ATy> b) {
  const size_t size = a.size();
  YACL_ENFORCE(size == b.size());

  YACL_ENFORCE((size & (size - 1)) == 0);
  YACL_ENFORCE((T & (T - 1)) == 0);

  std::vector<uint128_t> seeds(1);
  ctx_->GetState<Prg>()->Fill(absl::MakeSpan(seeds));
  std::vector<size_t> perm = GenPerm(seeds[0], size);
  RandomAuth(b);
  for (size_t i = 0; i < size; ++i) {
    b[perm[i]] = a[i];
  }
  return perm;
}

void FakeCorrelation::ASTGet(absl::Span<internal::ATy> a,
                             absl::Span<internal::ATy> b) {
  const size_t size = a.size();
  YACL_ENFORCE(size == b.size());
  std::vector<uint128_t> seeds(1);
  ctx_->GetState<Prg>()->Fill(absl::MakeSpan(seeds));
  std::vector<size_t> perm = GenPerm(seeds[0], size);
  RandomAuth(b);
  for (size_t i = 0; i < size; ++i) {
    b[perm[i]] = a[i];
  }
}

internal::ATy FakeCorrelation::NMul(absl::Span<internal::ATy> r) {
  const size_t size = r.size();
  auto rand0 = internal::op::Rand(*ctx_->GetState<Prg>(), size);
  auto rand1 = internal::op::Rand(*ctx_->GetState<Prg>(), size);

  auto rand = internal::op::Add(absl::MakeSpan(rand0), absl::MakeSpan(rand1));
  auto mac = internal::op::ScalarMul(key_, absl::MakeSpan(rand));

  internal::PTy mul_val = std::reduce(rand.begin(), rand.end(),
                                      internal::PTy(1), internal::PTy::Mul);
  internal::PTy mul_inv_val = internal::PTy::Inv(mul_val);
  internal::PTy mul_inv_mac = mul_inv_val * key_;

  internal::ATy ret;
  if (ctx_->GetRank() == 0) {
    internal::Pack(absl::MakeConstSpan(rand0), absl::MakeConstSpan(mac), r);
    ret = {mul_inv_val, mul_inv_mac};
  } else {
    internal::Pack(absl::MakeConstSpan(rand1), absl::MakeConstSpan(mac), r);
    ret = {0, mul_inv_mac};
  }
  return ret;
}

}  // namespace mosac
