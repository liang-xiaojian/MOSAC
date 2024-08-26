#include "mosac/utils/vec_op.h"

#include "field.h"
#include "yacl/math/mpint/mp_int.h"
#include "yacl/utils/parallel.h"

namespace mosac {

// -------------------
//     Fp 64-bit
// -------------------

void op64::Add(absl::Span<const kFp64> lhs, absl::Span<const kFp64> rhs,
               absl::Span<kFp64> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());

  std::transform(lhs.begin(), lhs.end(), rhs.begin(), out.begin(), std::plus());
}

void op64::Sub(absl::Span<const kFp64> lhs, absl::Span<const kFp64> rhs,
               absl::Span<kFp64> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());

  std::transform(lhs.begin(), lhs.end(), rhs.begin(), out.begin(),
                 std::minus());
}

void op64::Mul(absl::Span<const kFp64> lhs, absl::Span<const kFp64> rhs,
               absl::Span<kFp64> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());

  std::transform(lhs.begin(), lhs.end(), rhs.begin(), out.begin(),
                 std::multiplies());
}

void op64::ScalarMul(const kFp64 scalar, absl::Span<const kFp64> in,
                     absl::Span<kFp64> out) {
  YACL_ENFORCE(out.size() == in.size());
  std::transform(in.begin(), in.end(), out.begin(),
                 [&scalar](kFp64 val) { return scalar * val; });
}

void op64::Div(absl::Span<const kFp64> lhs, absl::Span<const kFp64> rhs,
               absl::Span<kFp64> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());
  Inv(rhs, out);
  Mul(lhs, out, out);
}

void op64::Neg(absl::Span<const kFp64> in, absl::Span<kFp64> out) {
  YACL_ENFORCE(in.size() == out.size());
  std::transform(in.begin(), in.end(), out.begin(), kFp64::Neg);
}

void op64::Sqrt(absl::Span<const kFp64> in, absl::Span<kFp64> out) {
  YACL_ENFORCE(in.size() == out.size());
  auto mod = yacl::math::MPInt(Prime64);
  auto power = yacl::math::MPInt((Prime64 + 1) >> 2);

  yacl::parallel_for(0, in.size(), [&](uint64_t bg, uint64_t ed) {
    std::transform(in.cbegin() + bg, in.cbegin() + ed, out.begin() + bg,
                   [&](const kFp64 &val) {
                     auto tmp = yacl::math::MPInt(0);
                     yacl::math::MPInt::PowMod(yacl::math::MPInt(val.GetVal()),
                                               power, mod, &tmp);
                     return kFp64(tmp.Get<uint64_t>());
                   });
  });

  // std::transform(in.cbegin(), in.cend(), out.begin(), [&](const kFp128 &val)
  // {
  //   auto tmp = yacl::math::MPInt(0);
  //   yacl::math::MPInt::PowMod(yacl::math::MPInt(val.GetVal()), power, mod,
  //                             &tmp);
  //   return kFp128(tmp.Get<uint128_t>());
  // });
}

template <size_t N>
kFp64 BatchInv64(absl::Span<const kFp64> in, absl::Span<kFp64> out,
                 kFp64 total = kFp64::One()) {
  auto inv = BatchInv64<N - 1>(in, out, total * in[N - 1]);
  out[N - 1] = inv * total;
  return inv * in[N - 1];
}

template <>
kFp64 BatchInv64<1>(absl::Span<const kFp64> in, absl::Span<kFp64> out,
                    kFp64 total) {
  auto inv = kFp64::Inv(total * in[0]);
  out[0] = total * inv;
  return inv * in[0];
}

// Batch Invert Optimize
void op64::Inv(absl::Span<const kFp64> in, absl::Span<kFp64> out) {
  const size_t size = out.size();
  YACL_ENFORCE(in.size() == size);
  size_t batch = size / 16;
  size_t bound = batch * 16;
  size_t remain = size - bound;
  for (size_t i = 0; i < batch; ++i) {
    BatchInv64<16>(in.subspan(i * 16, 16), out.subspan(i * 16, 16));
  }
  switch (remain) {
#define KASE64(T)                                               \
  case T:                                                       \
    BatchInv64<T>(in.subspan(bound, T), out.subspan(bound, T)); \
    break;
    KASE64(15);
    KASE64(14);
    KASE64(13);
    KASE64(12);
    KASE64(11);
    KASE64(10);
    KASE64(9);
    KASE64(8);
    KASE64(7);
    KASE64(6);
    KASE64(5);
    KASE64(4);
    KASE64(3);
    KASE64(2);
    KASE64(1);
#undef KASE64
    case 0:
      break;
    default:
      YACL_ENFORCE(false, "Inv Error");
  }
}

void op64::Ones(absl::Span<kFp64> out) {
  auto tmp = absl::Span(reinterpret_cast<uint64_t *>(out.data()), out.size());
  std::for_each(tmp.begin(), tmp.end(), [](uint64_t &val) { val = 1; });
}

void op64::Zeros(absl::Span<kFp64> out) {
  auto tmp = absl::Span(reinterpret_cast<uint64_t *>(out.data()), out.size());
  std::for_each(tmp.begin(), tmp.end(), [](uint64_t &val) { val = 0; });
}

void op64::Rand(absl::Span<kFp64> out) {
  const uint64_t prime = kFp64::GetPrime();

  auto out64 =
      absl::MakeSpan(reinterpret_cast<uint64_t *>(out.data()), out.size());

  auto prg = yacl::crypto::Prg<uint8_t>(yacl::crypto::SecureRandU128());
  prg.Fill(out64);

  for (auto &e : out64) {
    e %= prime;
  }
}

void op64::Rand(yacl::crypto::Prg<uint8_t> &prg, absl::Span<kFp64> out) {
  const uint64_t prime = kFp64::GetPrime();

  auto out64 =
      absl::MakeSpan(reinterpret_cast<uint64_t *>(out.data()), out.size());
  prg.Fill(out64);

  for (auto &e : out64) {
    e %= prime;
  }
}

// -------------------
//     Fp 128-bit
// -------------------

void op128::Add(absl::Span<const kFp128> lhs, absl::Span<const kFp128> rhs,
                absl::Span<kFp128> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());

  yacl::parallel_for(0, size, [&](uint64_t bg, uint64_t ed) {
    std::transform(lhs.begin() + bg, lhs.begin() + ed, rhs.begin() + bg,
                   out.begin() + bg, std::plus());
  });
  // std::transform(lhs.begin(), lhs.end(), rhs.begin(), out.begin(),
  // std::plus());
}

void op128::Sub(absl::Span<const kFp128> lhs, absl::Span<const kFp128> rhs,
                absl::Span<kFp128> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());
  yacl::parallel_for(0, size, [&](uint64_t bg, uint64_t ed) {
    std::transform(lhs.begin() + bg, lhs.begin() + ed, rhs.begin() + bg,
                   out.begin() + bg, std::minus());
  });
  // std::transform(lhs.begin(), lhs.end(), rhs.begin(), out.begin(),
  //                std::minus());
}

void op128::Mul(absl::Span<const kFp128> lhs, absl::Span<const kFp128> rhs,
                absl::Span<kFp128> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());
  yacl::parallel_for(0, size, [&](uint64_t bg, uint64_t ed) {
    std::transform(lhs.begin() + bg, lhs.begin() + ed, rhs.begin() + bg,
                   out.begin() + bg, std::multiplies());
  });

  // std::transform(lhs.begin(), lhs.end(), rhs.begin(), out.begin(),
  //                std::multiplies());
}

void op128::ScalarMul(const kFp128 scalar, absl::Span<const kFp128> in,
                      absl::Span<kFp128> out) {
  YACL_ENFORCE(out.size() == in.size());

  yacl::parallel_for(0, in.size(), [&](uint64_t bg, uint64_t ed) {
    std::transform(in.begin() + bg, in.begin() + ed, out.begin() + bg,
                   [&scalar](kFp128 val) { return scalar * val; });
  });
  // std::transform(in.begin(), in.end(), out.begin(),
  //                [&scalar](kFp128 val) { return scalar * val; });
}

void op128::Div(absl::Span<const kFp128> lhs, absl::Span<const kFp128> rhs,
                absl::Span<kFp128> out) {
  const uint32_t size = out.size();
  YACL_ENFORCE(size == lhs.size());
  YACL_ENFORCE(size == rhs.size());
  Inv(rhs, out);
  Mul(lhs, out, out);
}

void op128::Neg(absl::Span<const kFp128> in, absl::Span<kFp128> out) {
  YACL_ENFORCE(in.size() == out.size());

  yacl::parallel_for(0, in.size(), [&](uint64_t bg, uint64_t ed) {
    std::transform(in.begin() + bg, in.begin() + ed, out.begin() + bg,
                   kFp128::Neg);
  });
  // std::transform(in.begin(), in.end(), out.begin(), kFp128::Neg);
}

void op128::Sqrt(absl::Span<const kFp128> in, absl::Span<kFp128> out) {
  YACL_ENFORCE(in.size() == out.size());
  auto mod = yacl::math::MPInt(Prime128);
  auto power = yacl::math::MPInt((Prime128 + 1) >> 2);

  yacl::parallel_for(0, in.size(), [&](uint64_t bg, uint64_t ed) {
    std::transform(in.cbegin() + bg, in.cbegin() + ed, out.begin() + bg,
                   [&](const kFp128 &val) {
                     auto tmp = yacl::math::MPInt(0);
                     yacl::math::MPInt::PowMod(yacl::math::MPInt(val.GetVal()),
                                               power, mod, &tmp);
                     return kFp128(tmp.Get<uint128_t>());
                   });
  });

  // std::transform(in.cbegin(), in.cend(), out.begin(), [&](const kFp128 &val)
  // {
  //   auto tmp = yacl::math::MPInt(0);
  //   yacl::math::MPInt::PowMod(yacl::math::MPInt(val.GetVal()), power, mod,
  //                             &tmp);
  //   return kFp128(tmp.Get<uint128_t>());
  // });
}

template <size_t N>
kFp128 BatchInv128(absl::Span<const kFp128> in, absl::Span<kFp128> out,
                   kFp128 total = kFp128::One()) {
  auto inv = BatchInv128<N - 1>(in, out, total * in[N - 1]);
  out[N - 1] = inv * total;
  return inv * in[N - 1];
}

template <>
kFp128 BatchInv128<1>(absl::Span<const kFp128> in, absl::Span<kFp128> out,
                      kFp128 total) {
  auto inv = kFp128::Inv(total * in[0]);
  out[0] = total * inv;
  return inv * in[0];
}

// Batch Invert Optimize
void InvImpl(absl::Span<const kFp128> in, absl::Span<kFp128> out) {
  const size_t size = out.size();
  YACL_ENFORCE(in.size() == size);
  size_t batch = size / 16;
  size_t bound = batch * 16;
  size_t remain = size - bound;
  for (size_t i = 0; i < batch; ++i) {
    BatchInv128<16>(in.subspan(i * 16, 16), out.subspan(i * 16, 16));
  }
  switch (remain) {
#define KASE128(T)                                               \
  case T:                                                        \
    BatchInv128<T>(in.subspan(bound, T), out.subspan(bound, T)); \
    break;
    KASE128(15);
    KASE128(14);
    KASE128(13);
    KASE128(12);
    KASE128(11);
    KASE128(10);
    KASE128(9);
    KASE128(8);
    KASE128(7);
    KASE128(6);
    KASE128(5);
    KASE128(4);
    KASE128(3);
    KASE128(2);
    KASE128(1);
#undef KASE128
    case 0:
      break;
    default:
      YACL_ENFORCE(false, "Inv Error");
  }
}

void op128::Inv(absl::Span<const kFp128> in, absl::Span<kFp128> out) {
  const size_t size = out.size();
  YACL_ENFORCE(in.size() == size);
  yacl::parallel_for(0, size, 16, [&](uint64_t bg, uint64_t ed) {
    InvImpl(in.subspan(bg, ed - bg), out.subspan(bg, ed - bg));
  });
}

void op128::Ones(absl::Span<kFp128> out) {
  const size_t size = out.size();
  auto tmp = absl::Span(reinterpret_cast<uint128_t *>(out.data()), size);
  yacl::parallel_for(0, size, 4096, [&](uint64_t bg, uint64_t ed) {
    std::for_each(tmp.begin() + bg, tmp.begin() + ed,
                  [](uint128_t &val) { val = 1; });
  });
  // std::for_each(tmp.begin(), tmp.end(), [](uint128_t &val) { val = 0; });
}

void op128::Zeros(absl::Span<kFp128> out) {
  const size_t size = out.size();
  auto tmp = absl::Span(reinterpret_cast<uint128_t *>(out.data()), size);
  yacl::parallel_for(0, size, 4096, [&](uint64_t bg, uint64_t ed) {
    std::for_each(tmp.begin() + bg, tmp.begin() + ed,
                  [](uint128_t &val) { val = 0; });
  });
  // std::for_each(tmp.begin(), tmp.end(), [](uint128_t &val) { val = 0; });
}

void op128::Rand(absl::Span<kFp128> out) {
  const uint128_t prime = kFp128::GetPrime();
  const size_t size = out.size();

  auto out128 = absl::MakeSpan(reinterpret_cast<uint128_t *>(out.data()), size);
  auto prg = yacl::crypto::Prg<uint8_t>(yacl::crypto::SecureRandU128());
  prg.Fill(out128);

  yacl::parallel_for(0, size, 4096, [&](uint64_t bg, uint64_t ed) {
    std::for_each(out128.begin() + bg, out128.begin() + ed,
                  [&prime](uint128_t &val) { val %= prime; });
  });

  // for (auto &e : out128) {
  //   e %= prime;
  // }
}

void op128::Rand(yacl::crypto::Prg<uint8_t> &prg, absl::Span<kFp128> out) {
  const uint128_t prime = kFp128::GetPrime();
  const size_t size = out.size();

  auto out128 = absl::MakeSpan(reinterpret_cast<uint128_t *>(out.data()), size);
  prg.Fill(out128);

  yacl::parallel_for(0, size, 4096, [&](uint64_t bg, uint64_t ed) {
    std::for_each(out128.begin() + bg, out128.begin() + ed,
                  [&prime](uint128_t &val) { val %= prime; });
  });

  // for (auto &e : out128) {
  //   e %= prime;
  // }
}

std::vector<size_t> GenPerm(uint32_t num) {
  std::vector<size_t> perm(num);
  for (size_t i = 0; i < num; ++i) {
    perm[i] = i;
  }
  // std::shuffle
  std::random_device rd;
  std::shuffle(perm.begin(), perm.end(), std::mt19937(rd()));
  return perm;
}

std::vector<size_t> GenPerm(yacl::crypto::Prg<uint8_t> &prg, uint32_t num) {
  std::vector<size_t> perm(num);
  for (size_t i = 0; i < num; ++i) {
    perm[i] = i;
  }
  // shuffle
  std::vector<uint32_t> rand(num);
  prg.Fill(absl::MakeSpan(rand));

  for (size_t i = 0; i < num; ++i) {
    std::swap(perm[i], perm[rand[i] % num]);
  }
  return perm;
}

}  // namespace mosac
