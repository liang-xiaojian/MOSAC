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


#pragma once
#include <vector>

#include "mosac/context/context.h"
#include "mosac/ss/type.h"
#include "mosac/utils/field.h"

namespace mosac::internal {

std::vector<PTy> AddPP(std::shared_ptr<Context>& ctx, absl::Span<const PTy> lhs,
                       absl::Span<const PTy> rhs);
std::vector<PTy> AddPP_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const PTy> rhs);

std::vector<PTy> SubPP(std::shared_ptr<Context>& ctx, absl::Span<const PTy> lhs,
                       absl::Span<const PTy> rhs);
std::vector<PTy> SubPP_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const PTy> rhs);

std::vector<PTy> MulPP(std::shared_ptr<Context>& ctx, absl::Span<const PTy> lhs,
                       absl::Span<const PTy> rhs);
std::vector<PTy> MulPP_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const PTy> rhs);

std::vector<PTy> DivPP(std::shared_ptr<Context>& ctx, absl::Span<const PTy> lhs,
                       absl::Span<const PTy> rhs);
std::vector<PTy> DivPP_cache(std::shared_ptr<Context>& ctx,
                             absl::Span<const PTy> lhs,
                             absl::Span<const PTy> rhs);

std::vector<PTy> NegP(std::shared_ptr<Context>& ctx, absl::Span<const PTy> in);
std::vector<PTy> NegP_cache(std::shared_ptr<Context>& ctx,
                            absl::Span<const PTy> in);

std::vector<PTy> InvP(std::shared_ptr<Context>& ctx, absl::Span<const PTy> in);
std::vector<PTy> InvP_cache(std::shared_ptr<Context>& ctx,
                            absl::Span<const PTy> in);

std::vector<PTy> OnesP(std::shared_ptr<Context>& ctx, size_t num);
std::vector<PTy> OnesP_cache(std::shared_ptr<Context>& ctx, size_t num);

std::vector<PTy> ZerosP(std::shared_ptr<Context>& ctx, size_t num);
std::vector<PTy> ZerosP_cache(std::shared_ptr<Context>& ctx, size_t num);

std::vector<PTy> RandP(std::shared_ptr<Context>& ctx, size_t num);
std::vector<PTy> RandP_cache(std::shared_ptr<Context>& ctx, size_t num);

std::vector<PTy> ScalarMulPP(std::shared_ptr<Context>& ctx, const PTy& scalar,
                             absl::Span<const PTy> in);
std::vector<PTy> ScalarMulPP_cache(std::shared_ptr<Context>& ctx,
                                   const PTy& scalar, absl::Span<const PTy> in);

}  // namespace mosac::internal
