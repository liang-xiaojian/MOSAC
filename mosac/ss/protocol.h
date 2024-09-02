#pragma once
#include <vector>

#include "mosac/context/context.h"
#include "mosac/context/state.h"
#include "mosac/ss/ashare.h"
#include "mosac/ss/public.h"
#include "mosac/ss/type.h"
#include "mosac/utils/config.h"
#include "yacl/crypto/utils/rand.h"

namespace mosac {

namespace ym = yacl::math;
namespace yc = yacl::crypto;

using PTy = internal::PTy;
using ATy = internal::ATy;
// using GTy = internal::GTy;
// using MTy = internal::MTy;
using OP = internal::op;

class Protocol : public State {
 private:
  std::shared_ptr<Context> ctx_;
  // SPDZ key
  PTy key_;
  // check buffer
  std::vector<PTy> check_buff_;
  std::vector<std::vector<PTy>> ndss_val_buff_;
  std::vector<std::vector<PTy>> ndss_mac_buff_;

 public:
  static const std::string id;

  Protocol(std::shared_ptr<Context> ctx) : ctx_(ctx) {
    // SPDZ key setup
    key_ = PTy(yacl::crypto::SecureRandU64());
  }
  // SPDZ key
  PTy GetKey() const { return key_; }

  // PP evaluation
  std::vector<PTy> Add(absl::Span<const PTy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);
  std::vector<PTy> Sub(absl::Span<const PTy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);
  std::vector<PTy> Mul(absl::Span<const PTy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);
  std::vector<PTy> Div(absl::Span<const PTy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);

  // AA evalutaion
  std::vector<ATy> Add(absl::Span<const ATy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);
  std::vector<ATy> Sub(absl::Span<const ATy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);
  std::vector<ATy> Mul(absl::Span<const ATy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);
  std::vector<ATy> Div(absl::Span<const ATy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);

  // AP evalutaion
  std::vector<ATy> Add(absl::Span<const ATy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);
  std::vector<ATy> Sub(absl::Span<const ATy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);
  std::vector<ATy> Mul(absl::Span<const ATy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);
  std::vector<ATy> Div(absl::Span<const ATy> lhs, absl::Span<const PTy> rhs,
                       bool cache = false);

  // PA evalutaion
  std::vector<ATy> Add(absl::Span<const PTy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);
  std::vector<ATy> Sub(absl::Span<const PTy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);
  std::vector<ATy> Mul(absl::Span<const PTy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);
  std::vector<ATy> Div(absl::Span<const PTy> lhs, absl::Span<const ATy> rhs,
                       bool cache = false);

  // convert
  std::vector<PTy> A2P_delay(absl::Span<const ATy> in, bool cache = false);
  std::vector<PTy> A2P(absl::Span<const ATy> in, bool cache = false);
  std::vector<ATy> P2A(absl::Span<const PTy> in, bool cache = false);

  // others
  std::vector<PTy> Inv(absl::Span<const PTy> in, bool cache = false);
  std::vector<PTy> Neg(absl::Span<const PTy> in, bool cache = false);
  std::vector<ATy> Inv(absl::Span<const ATy> in, bool cache = false);
  std::vector<ATy> Neg(absl::Span<const ATy> in, bool cache = false);

  // special
  std::vector<PTy> ZerosP(size_t num, bool cache = false);
  std::vector<PTy> RandP(size_t num, bool cache = false);
  std::vector<ATy> ZerosA(size_t num, bool cache = false);
  std::vector<ATy> RandA(size_t num, bool cache = false);
  std::vector<ATy> SetA(absl::Span<const PTy> in, bool cache = false);
  std::vector<ATy> GetA(size_t num, bool cache = false);
  // Circuit Operation
  std::vector<ATy> SumA(absl::Span<const ATy> in, bool cache = false);
  // Filter
  std::vector<ATy> FilterA(absl::Span<const ATy> in,
                           absl::Span<const size_t> indexes,
                           bool cache = false);

  // shuffle entry
  std::vector<ATy> ShuffleA(absl::Span<const ATy> in, bool cache = false);
  std::vector<ATy> ShuffleASet(absl::Span<const ATy> in, bool cache = false);
  std::vector<ATy> ShuffleAGet(absl::Span<const ATy> in, bool cache = false);

  // NDSS shuffle entry
  std::vector<ATy> ShuffleA_2k(size_t T, absl::Span<const ATy> in,
                               bool cache = false);
  std::vector<ATy> ShuffleASet_2k(size_t T, absl::Span<const ATy> in,
                                  bool cache = false);
  std::vector<ATy> ShuffleAGet_2k(size_t T, absl::Span<const ATy> in,
                                  bool cache = false);

  // secure shuffle entry
  std::vector<ATy> SShuffleA(absl::Span<const ATy> in, bool cache = false);
  std::vector<ATy> SShuffleASet(absl::Span<const ATy> in, bool cache = false);
  std::vector<ATy> SShuffleAGet(absl::Span<const ATy> in, bool cache = false);

  std::vector<ATy> NMulA(absl::Span<const ATy> in, bool cache = false);

  std::vector<ATy> ZeroOneA(size_t num, bool cache = false);

  std::pair<std::vector<ATy>, std::vector<ATy>> RandFairA(size_t num,
                                                          bool cache = false);

  std::vector<PTy> FairA2P(absl::Span<const ATy> in, absl::Span<const ATy> bits,
                           bool cache = false);

  std::vector<ATy> ScalarMulPA(const PTy& scalar, absl::Span<const ATy> in,
                               bool cache = false);

  std::vector<ATy> ScalarMulAP(const ATy& scalar, absl::Span<const PTy> in,
                               bool cache = false);
  std::vector<PTy> ScalarMulPP(const PTy& scalar, absl::Span<const PTy> in,
                               bool cache = false);

  // check buffer
  void NdssBufferAppend(absl::Span<const ATy> in);
  void NdssBufferAppend(const ATy& in);
  bool NdssDelayCheck();

  // check buffer
  void CheckBufferAppend(absl::Span<const PTy> in);
  void CheckBufferAppend(const PTy& in);
  bool DelayCheck();
};

}  // namespace mosac
