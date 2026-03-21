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


#include "field.h"

#include "gtest/gtest.h"
namespace mosac {

TEST(kFp64Test, AddWork) {
  auto lhs = kFp64::Rand();
  auto rhs = kFp64::Rand();

  auto ret = kFp64::Add(lhs, rhs);
  auto ret2 = kFp64::Add(rhs, lhs);

  EXPECT_EQ(ret, ret2);
}

TEST(kFp64Test, SubWork) {
  auto lhs = kFp64::Rand();
  auto rhs = kFp64::Rand();

  auto ret = kFp64::Sub(lhs, rhs);
  auto ret2 = kFp64::Sub(rhs, lhs);

  auto ret3 = kFp64::Add(ret, ret2);

  EXPECT_EQ(ret3, kFp64(0));
}

TEST(kFp64Test, MulWork) {
  auto lhs = kFp64::Rand();
  auto rhs = kFp64::Rand();

  auto ret = kFp64::Mul(lhs, rhs);
  auto ret2 = kFp64::Mul(rhs, lhs);

  EXPECT_EQ(ret, ret2);
}

TEST(kFp64Test, DivWork) {
  auto lhs = kFp64::Rand();
  auto rhs = kFp64::Rand();

  auto ret = kFp64::Div(lhs, rhs);
  auto ret2 = kFp64::Div(rhs, lhs);

  auto ret3 = kFp64::Mul(ret, ret2);

  EXPECT_EQ(ret3, kFp64(1));
}

TEST(kFp64Test, InvWork) {
  auto lhs = kFp64::Rand();
  auto inv = kFp64::Inv(lhs);
  auto ret3 = kFp64::Mul(inv, lhs);

  EXPECT_EQ(ret3, kFp64(1));
}

TEST(kFp128Test, AddWork) {
  auto lhs = kFp128::Rand();
  auto rhs = kFp128::Rand();

  auto ret = kFp128::Add(lhs, rhs);
  auto ret2 = kFp128::Add(rhs, lhs);

  EXPECT_EQ(ret, ret2);
}

TEST(kFp128Test, SubWork) {
  auto lhs = kFp128::Rand();
  auto rhs = kFp128::Rand();

  auto ret = kFp128::Sub(lhs, rhs);
  auto ret2 = kFp128::Sub(rhs, lhs);

  auto ret3 = kFp128::Add(ret, ret2);

  EXPECT_EQ(ret3, kFp128(0));
}

TEST(kFp128Test, MulWork) {
  auto lhs = kFp128::Rand();
  auto rhs = kFp128::Rand();

  auto ret = kFp128::Mul(lhs, rhs);
  auto ret2 = kFp128::Mul(rhs, lhs);

  EXPECT_EQ(ret, ret2);
}

TEST(kFp128Test, DivWork) {
  auto lhs = kFp128::Rand();
  auto rhs = kFp128::Rand();

  auto ret = kFp128::Div(lhs, rhs);
  auto ret2 = kFp128::Div(rhs, lhs);

  auto ret3 = kFp128::Mul(ret, ret2);

  EXPECT_EQ(ret3, kFp128(1));
}

TEST(kFp128Test, InvWork) {
  auto lhs = kFp128::Rand();
  auto inv = kFp128::Inv(lhs);
  auto ret3 = kFp128::Mul(inv, lhs);

  EXPECT_EQ(ret3, kFp128(1));
}

}  // namespace mosac
