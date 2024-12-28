#pragma once
#include "mosac/utils/field.h"
#include "mosac/utils/vec_op.h"
#include "yacl/math/mpint/mp_int.h"

namespace mosac::internal {

namespace ym = yacl::math;
namespace yc = yacl::crypto;

// using PTy = kFp64;
// using op = op64;
// (DY-PRF) Group Type
// using GTy = ym::MPInt;

// Field 128 bits
using PTy = kFp128;
using op = op128;

// Field 64 bits
// using PTy = kFp64;
// using op = op64;

#pragma pack(8)
// Distribute PTy with Mac (additive share)
struct ATy {
  PTy val;
  PTy mac;
};
#pragma pack()

void inline Pack(absl::Span<const PTy> val, absl::Span<const PTy> mac,
                 absl::Span<ATy> ret) {
  const size_t size = ret.size();
  YACL_ENFORCE(size == val.size());
  YACL_ENFORCE(size == mac.size());
  auto ret_span = absl::MakeSpan(reinterpret_cast<PTy*>(ret.data()), size * 2);
  for (size_t i = 0; i < size; ++i) {
    ret_span[2 * i] = val[i];
    ret_span[2 * i + 1] = mac[i];
  }
}

std::vector<ATy> inline Pack(absl::Span<const PTy> val,
                             absl::Span<const PTy> mac) {
  const size_t size = val.size();
  YACL_ENFORCE(size == mac.size());
  std::vector<ATy> ret(size);
  Pack(val, mac, absl::MakeSpan(ret));
  return ret;
}

void inline Unpack(absl::Span<const ATy> in, absl::Span<PTy> val,
                   absl::Span<PTy> mac) {
  const size_t size = in.size();
  auto in_span =
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(in.data()), size * 2);
  for (size_t i = 0; i < size; ++i) {
    val[i] = in_span[i * 2];
    mac[i] = in_span[i * 2 + 1];
  }
}

std::pair<std::vector<PTy>, std::vector<PTy>> inline Unpack(
    absl::Span<const ATy> in) {
  const size_t size = in.size();
  std::vector<PTy> val(size);
  std::vector<PTy> mac(size);
  Unpack(in, absl::MakeSpan(val), absl::MakeSpan(mac));
  return std::make_pair(val, mac);
}

std::vector<PTy> inline ExtractVal(absl::Span<const ATy> in) {
  const size_t size = in.size();
  std::vector<PTy> val(size);
  auto in_span =
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(in.data()), size * 2);
  for (size_t i = 0; i < size; ++i) {
    val[i] = in_span[i * 2];
  }
  return val;
}

std::vector<PTy> inline ExtractMac(absl::Span<const ATy> in) {
  const size_t size = in.size();
  std::vector<PTy> mac(size);
  auto in_span =
      absl::MakeConstSpan(reinterpret_cast<const PTy*>(in.data()), size * 2);
  for (size_t i = 0; i < size; ++i) {
    mac[i] = in_span[i * 2 + 1];
  }
  return mac;
}

}  // namespace mosac::internal
