#include "mosac/ss/protocol.h"

#include "protocol.h"

namespace mosac {

// register string
const std::string Protocol::id = std::string("Protocol");

#define RegPP(name)                                                        \
  std::vector<PTy> Protocol::name(absl::Span<const PTy> lhs,               \
                                  absl::Span<const PTy> rhs, bool cache) { \
    if (cache) {                                                           \
      return internal::name##PP_cache(ctx_, lhs, rhs);                     \
    }                                                                      \
    return internal::name##PP(ctx_, lhs, rhs);                             \
  }

#define RegAP(name)                                                        \
  std::vector<ATy> Protocol::name(absl::Span<const ATy> lhs,               \
                                  absl::Span<const PTy> rhs, bool cache) { \
    if (cache) {                                                           \
      return internal::name##AP_cache(ctx_, lhs, rhs);                     \
    }                                                                      \
    return internal::name##AP(ctx_, lhs, rhs);                             \
  }

#define RegPA(name)                                                        \
  std::vector<ATy> Protocol::name(absl::Span<const PTy> lhs,               \
                                  absl::Span<const ATy> rhs, bool cache) { \
    if (cache) {                                                           \
      return internal::name##PA_cache(ctx_, lhs, rhs);                     \
    }                                                                      \
    return internal::name##PA(ctx_, lhs, rhs);                             \
  }

#define RegAA(name)                                                        \
  std::vector<ATy> Protocol::name(absl::Span<const ATy> lhs,               \
                                  absl::Span<const ATy> rhs, bool cache) { \
    if (cache) {                                                           \
      return internal::name##AA_cache(ctx_, lhs, rhs);                     \
    }                                                                      \
    return internal::name##AA(ctx_, lhs, rhs);                             \
  }

#define RegBi(name) \
  RegAA(name);      \
  RegAP(name);      \
  RegPA(name);      \
  RegPP(name)

RegBi(Add);
RegBi(Sub);
RegBi(Mul);
RegBi(Div);

#define RegP(name)                                                        \
  std::vector<PTy> Protocol::name(absl::Span<const PTy> in, bool cache) { \
    if (cache) {                                                          \
      return internal::name##P_cache(ctx_, in);                           \
    }                                                                     \
    return internal::name##P(ctx_, in);                                   \
  }

#define RegA(name)                                                        \
  std::vector<ATy> Protocol::name(absl::Span<const ATy> in, bool cache) { \
    if (cache) {                                                          \
      return internal::name##A_cache(ctx_, in);                           \
    }                                                                     \
    return internal::name##A(ctx_, in);                                   \
  }

#define RegSi(name) \
  RegP(name);       \
  RegA(name)

RegSi(Neg);
RegSi(Inv);

#define RegConvert(FROM, TO)                                               \
  std::vector<TO##Ty> Protocol::FROM##2##TO(absl::Span<const FROM##Ty> in, \
                                            bool cache) {                  \
    if (cache) {                                                           \
      return internal::FROM##2##TO##_cache(ctx_, in);                      \
    }                                                                      \
    return internal::FROM##2##TO(ctx_, in);                                \
  }

RegConvert(A, P);
RegConvert(P, A);

std::vector<PTy> Protocol::ZerosP(size_t num, bool cache) {
  if (cache) {
    return internal::ZerosP_cache(ctx_, num);
  }
  return internal::ZerosP(ctx_, num);
}

std::vector<PTy> Protocol::RandP(size_t num, bool cache) {
  if (cache) {
    return internal::RandP_cache(ctx_, num);
  }
  return internal::RandP(ctx_, num);
}

std::vector<ATy> Protocol::ZerosA(size_t num, bool cache) {
  if (cache) {
    return internal::ZerosA_cache(ctx_, num);
  }
  return internal::ZerosA(ctx_, num);
}

std::vector<ATy> Protocol::RandA(size_t num, bool cache) {
  if (cache) {
    return internal::RandA_cache(ctx_, num);
  }
  return internal::RandA(ctx_, num);
}

std::vector<ATy> Protocol::SetA(absl::Span<const PTy> in, bool cache) {
  if (cache) {
    return internal::SetA_cache(ctx_, in);
  }
  return internal::SetA(ctx_, in);
}

std::vector<ATy> Protocol::GetA(size_t num, bool cache) {
  if (cache) {
    return internal::GetA_cache(ctx_, num);
  }
  return internal::GetA(ctx_, num);
}

std::vector<ATy> Protocol::SumA(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::SumA_cache(ctx_, in);
  }
  return internal::SumA(ctx_, in);
}

std::vector<ATy> Protocol::FilterA(absl::Span<const ATy> in,
                                   absl::Span<const size_t> indexes,
                                   bool cache) {
  if (cache) {
    internal::FilterA_cache(ctx_, in, indexes);
  }
  return internal::FilterA(ctx_, in, indexes);
}

// shuffle
std::vector<ATy> Protocol::ShuffleA(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::ShuffleA_cache(ctx_, in);
  }
  return internal::ShuffleA(ctx_, in);
}

std::vector<ATy> Protocol::ShuffleASet(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::ShuffleASet_cache(ctx_, in);
  }
  return internal::ShuffleASet(ctx_, in);
}
std::vector<ATy> Protocol::ShuffleAGet(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::ShuffleAGet_cache(ctx_, in);
  }
  return internal::ShuffleAGet(ctx_, in);
}

std::array<std::vector<ATy>, 2> Protocol::ShuffleA(absl::Span<const ATy> in0,
                                                   absl::Span<const ATy> in1,
                                                   bool cache) {
  if (cache) {
    return internal::ShuffleA_cache(ctx_, in0, in1);
  }
  return internal::ShuffleA(ctx_, in0, in1);
}

std::array<std::vector<ATy>, 2> Protocol::ShuffleASet(absl::Span<const ATy> in0,
                                                      absl::Span<const ATy> in1,
                                                      bool cache) {
  if (cache) {
    return internal::ShuffleASet_cache(ctx_, in0, in1);
  }
  return internal::ShuffleASet(ctx_, in0, in1);
}

std::array<std::vector<ATy>, 2> Protocol::ShuffleAGet(absl::Span<const ATy> in0,
                                                      absl::Span<const ATy> in1,
                                                      bool cache) {
  if (cache) {
    return internal::ShuffleAGet_cache(ctx_, in0, in1);
  }
  return internal::ShuffleAGet(ctx_, in0, in1);
}

// sshufle
std::vector<ATy> Protocol::SShuffleA(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::SShuffleA_cache(ctx_, in);
  }
  return internal::SShuffleA(ctx_, in);
}

std::vector<ATy> Protocol::SShuffleASet(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::SShuffleASet_cache(ctx_, in);
  }
  return internal::SShuffleASet(ctx_, in);
}

std::vector<ATy> Protocol::SShuffleAGet(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::SShuffleAGet_cache(ctx_, in);
  }
  return internal::SShuffleAGet(ctx_, in);
}

std::vector<ATy> Protocol::NMulA(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::NMulA_cache(ctx_, in);
  }
  return internal::NMulA(ctx_, in);
}

std::vector<ATy> Protocol::ZeroOneA(size_t num, bool cache) {
  if (cache) {
    return internal::ZeroOneA_cache(ctx_, num);
  } else {
    return internal::ZeroOneA(ctx_, num);
  }
}

std::vector<ATy> Protocol::ScalarMulPA(const PTy& scalar,
                                       absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::ScalarMulPA_cache(ctx_, scalar, in);
  }
  return internal::ScalarMulPA(ctx_, scalar, in);
}

std::vector<ATy> Protocol::ScalarMulAP(const ATy& scalar,
                                       absl::Span<const PTy> in, bool cache) {
  if (cache) {
    return internal::ScalarMulAP_cache(ctx_, scalar, in);
  }
  return internal::ScalarMulAP(ctx_, scalar, in);
}

std::vector<PTy> Protocol::ScalarMulPP(const PTy& scalar,
                                       absl::Span<const PTy> in, bool cache) {
  if (cache) {
    return internal::ScalarMulPP_cache(ctx_, scalar, in);
  }
  return internal::ScalarMulPP(ctx_, scalar, in);
}

std::pair<std::vector<ATy>, std::vector<ATy>> Protocol::RandFairA(size_t num,
                                                                  bool cache) {
  if (cache) {
    return internal::RandFairA_cache(ctx_, num);
  }
  return internal::RandFairA(ctx_, num);
}

std::vector<PTy> Protocol::FairA2P(absl::Span<const ATy> in,
                                   absl::Span<const ATy> bits, bool cache) {
  if (cache) {
    return internal::FairA2P_cache(ctx_, in, bits);
  }
  return internal::FairA2P(ctx_, in, bits);
}

}  // namespace mosac
