#pragma once

namespace mosac::param {

// https://eprint.iacr.org/2016/505.pdf
// MASCOT
[[maybe_unused]] constexpr size_t kBeaverExtFactor = 3;

// Magic Number for OT
[[maybe_unused]] constexpr uint64_t kBatchOtSize = (1 << 24);
// Magic Number for Shuffle
[[maybe_unused]] constexpr size_t kBatchShuffle = 2048;
// Magic Number for AST
[[maybe_unused]] constexpr size_t kBatchAST = 1024;

}  // namespace mosac::param