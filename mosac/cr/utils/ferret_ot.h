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

#include "yacl/base/dynamic_bitset.h"
#include "yacl/crypto/primitives/ot/base_ot.h"
#include "yacl/crypto/primitives/ot/ot_store.h"
#include "yacl/crypto/utils/rand.h"

namespace mosac::ot {

namespace yc = yacl::crypto;

uint64_t FerretCotHelper(const yc::LpnParam& lpn_param, uint64_t ot_num,
                         bool mal = false);

// malicious Ferret Ote
void FerretOtExtSend_mal(const std::shared_ptr<yacl::link::Context>& ctx,
                         const yc::OtSendStore& base_cot,
                         const yc::LpnParam& lpn_param, uint64_t ot_num,
                         absl::Span<uint128_t> out);

void FerretOtExtRecv_mal(const std::shared_ptr<yacl::link::Context>& ctx,
                         const yc::OtRecvStore& base_cot,
                         const yc::LpnParam& lpn_param, uint64_t ot_num,
                         absl::Span<uint128_t> out);

}  // namespace mosac::ot
