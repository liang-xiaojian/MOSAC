// Copyright 2026 Ant International, Ant Group Co., Ltd.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Author (Xiaojian Liang)


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
  const size_t num = 1 << 8;

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
  const size_t num = 1 << 8;
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

TEST(CrTest, DoubleASTWork) {
  auto context = TestParam::GetContext();
  const size_t num = 1 << 8;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [perm, a, b, aa, bb] = cr->DoubleASTSet(num);
    return std::make_tuple(perm, a, b, aa, bb);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [a, b, aa, bb] = cr->DoubleASTGet(num);
    return std::make_tuple(a, b, aa, bb);
  });

  auto [perm, a0, b0, aa0, bb0] = rank0.get();
  auto [a1, b1, aa1, bb1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);

  auto aa0_val = internal::ExtractVal(aa0);
  auto aa1_val = internal::ExtractVal(aa1);
  auto bb0_val = internal::ExtractVal(bb0);
  auto bb1_val = internal::ExtractVal(bb1);

  auto a_val =
      internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b_val =
      internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));

  auto aa_val =
      internal::op::Add(absl::MakeSpan(aa0_val), absl::MakeSpan(aa1_val));
  auto bb_val =
      internal::op::Add(absl::MakeSpan(bb0_val), absl::MakeSpan(bb1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a_val[i], b_val[perm[i]]);
    EXPECT_EQ(aa_val[i], bb_val[perm[i]]);
  }

  auto a0_mac = internal::ExtractMac(a0);
  auto a1_mac = internal::ExtractMac(a1);
  auto b0_mac = internal::ExtractMac(b0);
  auto b1_mac = internal::ExtractMac(b1);

  auto aa0_mac = internal::ExtractMac(aa0);
  auto aa1_mac = internal::ExtractMac(aa1);
  auto bb0_mac = internal::ExtractMac(bb0);
  auto bb1_mac = internal::ExtractMac(bb1);

  auto a_mac =
      internal::op::Add(absl::MakeSpan(a0_mac), absl::MakeSpan(a1_mac));
  auto b_mac =
      internal::op::Add(absl::MakeSpan(b0_mac), absl::MakeSpan(b1_mac));

  auto aa_mac =
      internal::op::Add(absl::MakeSpan(aa0_mac), absl::MakeSpan(aa1_mac));
  auto bb_mac =
      internal::op::Add(absl::MakeSpan(bb0_mac), absl::MakeSpan(bb1_mac));

  auto key = context[0]->GetState<Correlation>()->GetKey() +
             context[1]->GetState<Correlation>()->GetKey();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(key * a_val[i], a_mac[i]);
    EXPECT_EQ(key * b_val[i], b_mac[i]);

    EXPECT_EQ(key * aa_val[i], aa_mac[i]);
    EXPECT_EQ(key * bb_val[i], bb_mac[i]);
  }

  sort(perm.begin(), perm.end());
  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(perm[i], i);
  }
}

TEST(CrTest, DoubleASTWork_2k) {
  auto context = TestParam::GetContext();
  const size_t num = 1 << 8;
  const size_t T = 1 << 3;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [perm, a, b, aa, bb] = cr->DoubleASTSet_2k(T, num);
    return std::make_tuple(perm, a, b, aa, bb);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [a, b, aa, bb] = cr->DoubleASTGet_2k(T, num);
    return std::make_tuple(a, b, aa, bb);
  });

  auto [perm, a0, b0, aa0, bb0] = rank0.get();
  auto [a1, b1, aa1, bb1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);

  auto aa0_val = internal::ExtractVal(aa0);
  auto aa1_val = internal::ExtractVal(aa1);
  auto bb0_val = internal::ExtractVal(bb0);
  auto bb1_val = internal::ExtractVal(bb1);

  auto a_val =
      internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b_val =
      internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));

  auto aa_val =
      internal::op::Add(absl::MakeSpan(aa0_val), absl::MakeSpan(aa1_val));
  auto bb_val =
      internal::op::Add(absl::MakeSpan(bb0_val), absl::MakeSpan(bb1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a_val[i], b_val[perm[i]]);
    EXPECT_EQ(aa_val[i], bb_val[perm[i]]);
  }

  auto a0_mac = internal::ExtractMac(a0);
  auto a1_mac = internal::ExtractMac(a1);
  auto b0_mac = internal::ExtractMac(b0);
  auto b1_mac = internal::ExtractMac(b1);

  auto aa0_mac = internal::ExtractMac(aa0);
  auto aa1_mac = internal::ExtractMac(aa1);
  auto bb0_mac = internal::ExtractMac(bb0);
  auto bb1_mac = internal::ExtractMac(bb1);

  auto a_mac =
      internal::op::Add(absl::MakeSpan(a0_mac), absl::MakeSpan(a1_mac));
  auto b_mac =
      internal::op::Add(absl::MakeSpan(b0_mac), absl::MakeSpan(b1_mac));

  auto aa_mac =
      internal::op::Add(absl::MakeSpan(aa0_mac), absl::MakeSpan(aa1_mac));
  auto bb_mac =
      internal::op::Add(absl::MakeSpan(bb0_mac), absl::MakeSpan(bb1_mac));

  auto key = context[0]->GetState<Correlation>()->GetKey() +
             context[1]->GetState<Correlation>()->GetKey();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(key * a_val[i], a_mac[i]);
    EXPECT_EQ(key * b_val[i], b_mac[i]);

    EXPECT_EQ(key * aa_val[i], aa_mac[i]);
    EXPECT_EQ(key * bb_val[i], bb_mac[i]);
  }

  sort(perm.begin(), perm.end());
  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(perm[i], i);
  }
}

TEST(CrTest, _DoubleASTWork_2k) {
  auto context = TestParam::GetContext();
  const size_t num = 1 << 8;
  const size_t T = 1 << 3;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [perm, a, b, aa, bb] = cr->_DoubleASTSet_2k(T, num);
    YACL_ENFORCE(cr->DelayCheck());
    return std::make_tuple(perm, a, b, aa, bb);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [a, b, aa, bb] = cr->_DoubleASTGet_2k(T, num);
    YACL_ENFORCE(cr->DelayCheck());
    return std::make_tuple(a, b, aa, bb);
  });

  auto [perm, a0, b0, aa0, bb0] = rank0.get();
  auto [a1, b1, aa1, bb1] = rank1.get();

  auto a0_val = internal::ExtractVal(a0);
  auto a1_val = internal::ExtractVal(a1);
  auto b0_val = internal::ExtractVal(b0);
  auto b1_val = internal::ExtractVal(b1);

  auto aa0_val = internal::ExtractVal(aa0);
  auto aa1_val = internal::ExtractVal(aa1);
  auto bb0_val = internal::ExtractVal(bb0);
  auto bb1_val = internal::ExtractVal(bb1);

  auto a_val =
      internal::op::Add(absl::MakeSpan(a0_val), absl::MakeSpan(a1_val));
  auto b_val =
      internal::op::Add(absl::MakeSpan(b0_val), absl::MakeSpan(b1_val));

  auto aa_val =
      internal::op::Add(absl::MakeSpan(aa0_val), absl::MakeSpan(aa1_val));
  auto bb_val =
      internal::op::Add(absl::MakeSpan(bb0_val), absl::MakeSpan(bb1_val));

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(a_val[i], b_val[perm[i]]);
    EXPECT_EQ(aa_val[i], bb_val[perm[i]]);
  }

  auto a0_mac = internal::ExtractMac(a0);
  auto a1_mac = internal::ExtractMac(a1);
  auto b0_mac = internal::ExtractMac(b0);
  auto b1_mac = internal::ExtractMac(b1);

  auto aa0_mac = internal::ExtractMac(aa0);
  auto aa1_mac = internal::ExtractMac(aa1);
  auto bb0_mac = internal::ExtractMac(bb0);
  auto bb1_mac = internal::ExtractMac(bb1);

  auto a_mac =
      internal::op::Add(absl::MakeSpan(a0_mac), absl::MakeSpan(a1_mac));
  auto b_mac =
      internal::op::Add(absl::MakeSpan(b0_mac), absl::MakeSpan(b1_mac));

  auto aa_mac =
      internal::op::Add(absl::MakeSpan(aa0_mac), absl::MakeSpan(aa1_mac));
  auto bb_mac =
      internal::op::Add(absl::MakeSpan(bb0_mac), absl::MakeSpan(bb1_mac));

  auto key = context[0]->GetState<Correlation>()->GetKey() +
             context[1]->GetState<Correlation>()->GetKey();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(key * a_val[i], a_mac[i]);
    EXPECT_EQ(key * b_val[i], b_mac[i]);

    EXPECT_EQ(key * aa_val[i], aa_mac[i]);
    EXPECT_EQ(key * bb_val[i], bb_mac[i]);
  }

  sort(perm.begin(), perm.end());
  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ(perm[i], i);
  }
}

TEST(CrTest, VoleTest) {
  auto context = TestParam::GetContext();
  const size_t num = 1 << 4;

  auto rank0 = std::async([&] {
    auto cr = context[0]->GetState<Correlation>();
    auto [a, b] = cr->RandomVoleSet(num);
    return std::make_tuple(a, b);
  });
  auto rank1 = std::async([&] {
    auto cr = context[1]->GetState<Correlation>();
    auto [c] = cr->RandomVoleGet(num);
    auto key = cr->GetVoleKey();
    return std::make_tuple(key, c);
  });

  auto [a, b] = rank0.get();
  auto [key, c] = rank1.get();

  for (size_t i = 0; i < num; ++i) {
    EXPECT_EQ((a[i] * key + b[i]), c[i]);
  }
}

}  // namespace mosac
