#include "mosac/cr/cr.h"

#include <future>

#include "gtest/gtest.h"
#include "mosac/context/register.h"
#include "mosac/utils/test_util.h"
#include "mosac/utils/vec_op.h"

namespace mosac {

class TestParam {
 public:
  static std::vector<std::shared_ptr<Context>> ctx;

  // Getter
  static std::vector<std::shared_ptr<Context>>& GetContext() {
    if (ctx.empty()) {
      ctx = Setup();
    }
    return ctx;
  }

  static std::vector<std::shared_ptr<Context>> Setup() {
    auto ctx = MockContext(2);
    MockSetupContext(ctx);
    return ctx;
  }
};

std::vector<std::shared_ptr<Context>> TestParam::ctx =
    std::vector<std::shared_ptr<Context>>();

TEST(Setup, InitializeWork) {
  auto context = TestParam::GetContext();
  EXPECT_EQ(context.size(), 2);
}

TEST(CrTest, AuthBeaverWork) {
  auto context = TestParam::GetContext();
  const size_t num = 1000;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    std::vector<internal::ATy> a(num, {0, 0});
    std::vector<internal::ATy> b(num, {0, 0});
    std::vector<internal::ATy> c(num, {0, 0});
    cr->BeaverTriple(absl::MakeSpan(a), absl::MakeSpan(b), absl::MakeSpan(c));
    return std::make_tuple(a, b, c);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    std::vector<internal::ATy> a(num, {0, 0});
    std::vector<internal::ATy> b(num, {0, 0});
    std::vector<internal::ATy> c(num, {0, 0});
    cr->BeaverTriple(absl::MakeSpan(a), absl::MakeSpan(b), absl::MakeSpan(c));
    return std::make_tuple(a, b, c);
  });

  auto [a0, b0, c0] = rank0.get();
  auto [a1, b1, c1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);
  auto c0_val = internal::ExtractVal(c0);
  auto c1_val = internal::ExtractVal(c1);

  auto a = internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b = internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));
  auto c = internal::op::Add(absl::MakeSpan(c0_val), absl::MakeSpan(c1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a[i] * b[i], c[i]);
  }
}

TEST(CrTest, AuthBeaverCacheWork) {
  auto context = TestParam::GetContext();
  const size_t num = 1000;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    cr->force_cache(num, 0, 0);
    auto [a, b, c] = cr->BeaverTriple(num);
    return std::make_tuple(a, b, c);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    cr->force_cache(num, 0, 0);
    auto [a, b, c] = cr->BeaverTriple(num);
    return std::make_tuple(a, b, c);
  });

  auto [a0, b0, c0] = rank0.get();
  auto [a1, b1, c1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);
  auto c0_val = internal::ExtractVal(c0);
  auto c1_val = internal::ExtractVal(c1);

  auto a = internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b = internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));
  auto c = internal::op::Add(absl::MakeSpan(c0_val), absl::MakeSpan(c1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a[i] * b[i], c[i]);
  }
}

TEST(CrTest, ShuffleWork) {
  auto context = TestParam::GetContext();
  const size_t num = 1000;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [delta, perm] = cr->ShuffleSet(num);
    return std::make_tuple(delta, perm);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [a, b] = cr->ShuffleGet(num);
    return std::make_tuple(a, b);
  });

  auto [delta, perm] = rank0.get();
  auto [a, b] = rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(internal::PTy(0), a[perm[i]] + b[i] + delta[i]);
  }

  sort(perm.begin(), perm.end());
  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(perm[i], i);
  }
}

TEST(CrTest, BatchShuffleWork) {
  auto context = TestParam::GetContext();
  const size_t batch_num = 1 << 8;
  const size_t per_size = 1 << 4;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto vec_SS = cr->BatchShuffleSet(batch_num, per_size);
    return vec_SS;
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto vec_SG = cr->BatchShuffleGet(batch_num, per_size);
    return vec_SG;
  });

  auto vec_SS = rank0.get();
  auto vec_SG = rank1.get();

  for (size_t k = 0; k < batch_num; ++k) {
    auto& delta = vec_SS[k].delta;
    auto& perm = vec_SS[k].perm;

    auto& a = vec_SG[k].a;
    auto& b = vec_SG[k].b;

    for (size_t i = 0; i < per_size; ++i) {
      EXPECT_EQ(internal::PTy(0), a[perm[i]] + b[i] + delta[i]);
    }

    sort(perm.begin(), perm.end());
    for (size_t i = 0; i < per_size; ++i) {
      EXPECT_EQ(perm[i], i);
    }
  }
}

TEST(CrTest, SgrrBatchShuffleWork) {
  auto context = TestParam::GetContext();
  const size_t batch_num = 1 << 8;
  const size_t per_size = 1 << 4;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto vec_SS = cr->_BatchShuffleSet(batch_num, per_size);
    return vec_SS;
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto vec_SG = cr->_BatchShuffleGet(batch_num, per_size);
    return vec_SG;
  });

  auto vec_SS = rank0.get();
  auto vec_SG = rank1.get();

  for (size_t k = 0; k < batch_num; ++k) {
    auto& delta = vec_SS[k].delta;
    auto& perm = vec_SS[k].perm;

    auto& a = vec_SG[k].a;
    auto& b = vec_SG[k].b;

    for (size_t i = 0; i < per_size; ++i) {
      EXPECT_EQ(internal::PTy(0), a[perm[i]] + b[i] + delta[i]);
    }

    sort(perm.begin(), perm.end());
    for (size_t i = 0; i < per_size; ++i) {
      EXPECT_EQ(perm[i], i);
    }
  }
}

TEST(CrTest, NMulTest) {
  auto context = TestParam::GetContext();
  const size_t num = 1000;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [r, mul_inv] = cr->NMul(num);
    return std::make_tuple(r, mul_inv);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [r, mul_inv] = cr->NMul(num);
    return std::make_tuple(r, mul_inv);
  });

  auto [r0, mul_inv0] = rank0.get();
  auto [r1, mul_inv1] = rank1.get();

  auto r0_val = internal::ExtractVal(r0);
  auto r1_val = internal::ExtractVal(r1);
  auto r = internal::op::Add(absl::MakeConstSpan(r0_val),
                             absl::MakeConstSpan(r1_val));

  auto mul =
      std::reduce(r.begin(), r.end(), internal::PTy(1), internal::PTy::Mul);
  auto mul_inv = internal::PTy::Add(mul_inv0.val, mul_inv1.val);

  EXPECT_EQ(internal::PTy(1), internal::PTy::Mul(mul, mul_inv));

  auto r0_mac = internal::ExtractMac(r0);
  auto r1_mac = internal::ExtractMac(r1);
  auto r_mac = internal::op::Add(absl::MakeConstSpan(r0_mac),
                                 absl::MakeConstSpan(r1_mac));
  auto key = context[0]->GetState<Correlation>()->GetKey() +
             context[1]->GetState<Correlation>()->GetKey();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(key * r[i], r_mac[i]);
  }
}

TEST(CrTest, ASTWork) {
  auto context = TestParam::GetContext();
  const size_t num = 1 << 4;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [perm, a, b] = cr->ASTSet(num);
    return std::make_tuple(perm, a, b);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [a, b] = cr->ASTGet(num);
    return std::make_tuple(a, b);
  });

  auto [perm, a0, b0] = rank0.get();
  auto [a1, b1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);

  auto a_val =
      internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b_val =
      internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a_val[i], b_val[perm[i]]);
  }

  auto a0_mac = internal::ExtractMac(a0);
  auto a1_mac = internal::ExtractMac(a1);
  auto b0_mac = internal::ExtractMac(b0);
  auto b1_mac = internal::ExtractMac(b1);

  auto a_mac =
      internal::op::Add(absl::MakeSpan(a0_mac), absl::MakeSpan(a1_mac));
  auto b_mac =
      internal::op::Add(absl::MakeSpan(b0_mac), absl::MakeSpan(b1_mac));

  auto key = context[0]->GetState<Correlation>()->GetKey() +
             context[1]->GetState<Correlation>()->GetKey();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(key * a_val[i], a_mac[i]);
    EXPECT_EQ(key * b_val[i], b_mac[i]);
  }

  sort(perm.begin(), perm.end());
  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(perm[i], i);
  }
}

TEST(CrTest, ASTWork_2k) {
  auto context = TestParam::GetContext();
  const size_t num = 1 << 4;
  const size_t T = 1 << 3;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [perm, a, b] = cr->ASTSet_2k(T, num);
    return std::make_tuple(perm, a, b);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [a, b] = cr->ASTGet_2k(T, num);
    return std::make_tuple(a, b);
  });

  auto [perm, a0, b0] = rank0.get();
  auto [a1, b1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);

  auto a_val =
      internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b_val =
      internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a_val[i], b_val[perm[i]]);
  }

  auto a0_mac = internal::ExtractMac(a0);
  auto a1_mac = internal::ExtractMac(a1);
  auto b0_mac = internal::ExtractMac(b0);
  auto b1_mac = internal::ExtractMac(b1);

  auto a_mac =
      internal::op::Add(absl::MakeSpan(a0_mac), absl::MakeSpan(a1_mac));
  auto b_mac =
      internal::op::Add(absl::MakeSpan(b0_mac), absl::MakeSpan(b1_mac));

  auto key = context[0]->GetState<Correlation>()->GetKey() +
             context[1]->GetState<Correlation>()->GetKey();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(key * a_val[i], a_mac[i]);
    EXPECT_EQ(key * b_val[i], b_mac[i]);
  }

  sort(perm.begin(), perm.end());
  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(perm[i], i);
  }
}

}  // namespace mosac
