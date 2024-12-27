#pragma once

namespace mosac::param {

// https://eprint.iacr.org/2016/505.pdf
// MASCOT
[[maybe_unused]] constexpr size_t kBeaverExtFactor = 3;

[[maybe_unused]] constexpr uint64_t kBatchOtSize = (1 << 24);

[[maybe_unused]] constexpr size_t kBatchShuffle = 2048;

[[maybe_unused]] constexpr size_t kBatchAST = 2048;

}  // namespace mosac::param