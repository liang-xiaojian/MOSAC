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
