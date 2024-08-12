#include "mosac/utils/vec_op.h"

#include "field.h"
#include "gtest/gtest.h"
namespace mosac {

TEST(kFp64Test, AddWork) {
  size_t num = 10000;
  auto lhs = op64::Rand(num);
  auto rhs = op64::Rand(num);

  auto ret = op64::Add(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op64::Add(absl::MakeSpan(rhs), absl::MakeSpan(lhs));
  op64::AddInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], ret2[i]);
    EXPECT_EQ(ret[i], lhs[i]);
  }
}

TEST(kFp64Test, SubWork) {
  size_t num = 10000;
  auto lhs = op64::Rand(num);
  auto rhs = op64::Rand(num);

  auto ret = op64::Sub(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op64::Sub(absl::MakeSpan(rhs), absl::MakeSpan(lhs));

  auto ret3 = op64::Add(absl::MakeSpan(ret), absl::MakeSpan(ret2));
  op64::SubInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  op64::AddInplace(absl::MakeSpan(lhs), absl::MakeSpan(ret2));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret3[i], kFp64(0));
    EXPECT_EQ(lhs[i], kFp64(0));
  }
}

TEST(kFp64Test, MulWork) {
  size_t num = 10000;
  auto lhs = op64::Rand(num);
  auto rhs = op64::Rand(num);

  auto ret = op64::Mul(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op64::Mul(absl::MakeSpan(rhs), absl::MakeSpan(lhs));
  op64::MulInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], ret2[i]);
    EXPECT_EQ(ret[i], lhs[i]);
  }
}

TEST(kFp64Test, DivWork) {
  size_t num = 10000;
  auto lhs = op64::Rand(num);
  auto rhs = op64::Rand(num);

  auto ret = op64::Div(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op64::Div(absl::MakeSpan(rhs), absl::MakeSpan(lhs));

  auto ret3 = op64::Mul(absl::MakeSpan(ret), absl::MakeSpan(ret2));
  op64::DivInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  op64::MulInplace(absl::MakeSpan(lhs), absl::MakeSpan(ret2));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret3[i], kFp64(1));
    EXPECT_EQ(lhs[i], kFp64(1));
  }
}

TEST(kFp64Test, InvWork) {
  size_t num = 10000;
  auto lhs = op64::Rand(num);
  auto inv = op64::Inv(absl::MakeSpan(lhs));
  auto ret3 = op64::Mul(absl::MakeSpan(inv), absl::MakeSpan(lhs));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret3[i], kFp64(1));
  }
}

TEST(kFp64Test, PrgWork) {
  size_t num = 10000;
  uint128_t seed = yacl::crypto::SecureRandU128();

  yacl::crypto::Prg<uint8_t> prg0(seed);
  yacl::crypto::Prg<uint8_t> prg1(seed);

  auto lhs = op64::Rand(prg0, num);
  auto rhs = op64::Rand(prg1, num);

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(lhs[i], rhs[i]);
  }
}

TEST(kFp64Test, OneZeroWork) {
  size_t num = 10000;

  auto lhs = op64::Ones(num);
  auto rhs = op64::Ones(num);

  auto ret = op64::Sub(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto zeros = op64::Zeros(num);

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], zeros[i]);
  }
}

TEST(kFp128Test, AddWork) {
  size_t num = 10000;
  auto lhs = op128::Rand(num);
  auto rhs = op128::Rand(num);

  auto ret = op128::Add(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op128::Add(absl::MakeSpan(rhs), absl::MakeSpan(lhs));
  op128::AddInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], ret2[i]);
    EXPECT_EQ(ret[i], lhs[i]);
  }
}

TEST(kFp128Test, SubWork) {
  size_t num = 10000;
  auto lhs = op128::Rand(num);
  auto rhs = op128::Rand(num);

  auto ret = op128::Sub(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op128::Sub(absl::MakeSpan(rhs), absl::MakeSpan(lhs));

  auto ret3 = op128::Add(absl::MakeSpan(ret), absl::MakeSpan(ret2));
  op128::SubInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  op128::AddInplace(absl::MakeSpan(lhs), absl::MakeSpan(ret2));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret3[i], kFp128(0));
    EXPECT_EQ(lhs[i], kFp128(0));
  }
}

TEST(kFp128Test, MulWork) {
  size_t num = 10000;
  auto lhs = op128::Rand(num);
  auto rhs = op128::Rand(num);

  auto ret = op128::Mul(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op128::Mul(absl::MakeSpan(rhs), absl::MakeSpan(lhs));
  op128::MulInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], ret2[i]);
    EXPECT_EQ(ret[i], lhs[i]);
  }
}

TEST(kFp128Test, DivWork) {
  size_t num = 10000;
  auto lhs = op128::Rand(num);
  auto rhs = op128::Rand(num);

  auto ret = op128::Div(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto ret2 = op128::Div(absl::MakeSpan(rhs), absl::MakeSpan(lhs));

  auto ret3 = op128::Mul(absl::MakeSpan(ret), absl::MakeSpan(ret2));
  op128::DivInplace(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  op128::MulInplace(absl::MakeSpan(lhs), absl::MakeSpan(ret2));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret3[i], kFp128(1));
    EXPECT_EQ(lhs[i], kFp128(1));
  }
}

TEST(kFp128Test, InvWork) {
  size_t num = 10000;
  auto lhs = op128::Rand(num);
  auto inv = op128::Inv(absl::MakeSpan(lhs));
  auto ret3 = op128::Mul(absl::MakeSpan(inv), absl::MakeSpan(lhs));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret3[i], kFp128(1));
  }
}

TEST(kFp128Test, PrgWork) {
  size_t num = 10000;
  uint128_t seed = yacl::crypto::SecureRandU128();

  yacl::crypto::Prg<uint8_t> prg0(seed);
  yacl::crypto::Prg<uint8_t> prg1(seed);

  auto lhs = op128::Rand(prg0, num);
  auto rhs = op128::Rand(prg1, num);

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(lhs[i], rhs[i]);
  }
}

TEST(kFp128Test, SqrtWork) {
  size_t num = 10000;
  auto in = op128::Rand(num);
  // square
  auto res = op128::Mul(absl::MakeSpan(in), absl::MakeSpan(in));
  // find root
  auto root = op128::Sqrt(absl::MakeSpan(res));
  auto ret = op128::Mul(absl::MakeSpan(root), absl::MakeSpan(root));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], res[i]);
  }
}

TEST(kFp128Test, OneZeroWork) {
  size_t num = 10000;

  auto lhs = op128::Ones(num);
  auto rhs = op128::Ones(num);

  auto ret = op128::Sub(absl::MakeSpan(lhs), absl::MakeSpan(rhs));
  auto zeros = op128::Zeros(num);

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(ret[i], zeros[i]);
  }
}

TEST(Test, ShuffleWork) {
  size_t num = 10000;

  auto p0 = GenPerm(num);
  auto p1 = GenPerm(num);

  std::sort(p0.begin(), p0.end());
  std::sort(p1.begin(), p1.end());

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(p0[i], i);
    EXPECT_EQ(p0[i], p1[i]);
  }
}

TEST(Test, SeedShuffleWork) {
  size_t num = 10000;

  uint128_t seed = yacl::crypto::SecureRandU128();

  auto p0 = GenPerm(seed, num);
  auto p1 = GenPerm(seed, num);

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(p0[i], p1[i]);
  }

  std::sort(p0.begin(), p0.end());
  std::sort(p1.begin(), p1.end());

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(p0[i], i);
    EXPECT_EQ(p0[i], p1[i]);
  }
}

}  // namespace mosac
