#pragma once

namespace mosac::param {

// https://eprint.iacr.org/2016/505.pdf
// MASCOT
[[maybe_unused]] constexpr size_t kBeaverExtFactor = 3;

// Magic Number for OT
[[maybe_unused]] constexpr uint64_t kBatchOtSize = (1 << 23);
// Magic Number for Shuffle
[[maybe_unused]] constexpr size_t kBatchShuffle = 8192;
// Magic Number for Shuffle
[[maybe_unused]] constexpr size_t kSgrrBatchShuffle = 8192;
// Magic Number for AST
[[maybe_unused]] constexpr size_t kBatchAST = 8192;

}  // namespace mosac::param