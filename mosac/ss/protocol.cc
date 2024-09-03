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

std::vector<PTy> Protocol::A2P_delay(absl::Span<const ATy> in, bool cache) {
  if (cache) {
    return internal::A2P_delay_cache(ctx_, in);
  }
  // Special
  return internal::A2P_delay(ctx_, in);
}

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

// NDSS shuffle
std::vector<ATy> Protocol::ShuffleA_2k(size_t T, absl::Span<const ATy> in,
                                       bool cache) {
  if (cache) {
    return internal::ShuffleA_2k_cache(ctx_, T, in);
  }
  return internal::ShuffleA_2k(ctx_, T, in);
}

std::vector<ATy> Protocol::ShuffleASet_2k(size_t T, absl::Span<const ATy> in,
                                          bool cache) {
  if (cache) {
    return internal::ShuffleASet_2k_cache(ctx_, T, in);
  }
  return internal::ShuffleASet_2k(ctx_, T, in);
}
std::vector<ATy> Protocol::ShuffleAGet_2k(size_t T, absl::Span<const ATy> in,
                                          bool cache) {
  if (cache) {
    return internal::ShuffleAGet_2k_cache(ctx_, T, in);
  }
  return internal::ShuffleAGet_2k(ctx_, T, in);
}

// secure shuffle
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

void Protocol::NdssBufferAppend(absl::Span<const ATy> in) {
  auto [in_val, in_mac] = Unpack(in);
  ndss_val_buff_.emplace_back(std::move(in_val));
  ndss_mac_buff_.emplace_back(std::move(in_mac));

  const size_t DelayMaxSize = 1 << 24;

  size_t ndss_size = 0;
  for (const auto& sub_buff : ndss_val_buff_) {
    ndss_size = ndss_size + sub_buff.size();
  }

  if (ndss_size >= DelayMaxSize) {
    SPDLOG_DEBUG("Ndss Buffer size is {}, greater than {}", ndss_size,
                 DelayMaxSize);
    auto flag = NdssDelayCheck();
    YACL_ENFORCE(flag == true);
  }
}

void Protocol::NdssBufferAppend(const ATy& in) {
  std::vector<ATy> in_vec(1, in);
  NdssBufferAppend(absl::MakeConstSpan(in_vec));
}

bool Protocol::NdssDelayCheck() {
  YACL_ENFORCE(ndss_val_buff_.size() == ndss_mac_buff_.size());
  if (ndss_val_buff_.size() == 0) {
    return true;
  }
  const size_t seed_len = ndss_val_buff_.size();

  auto conn = ctx_->GetConnection();
  auto sync_seed = conn->SyncSeed();

  auto prg = yacl::crypto::Prg<uint128_t>(sync_seed);
  std::vector<uint128_t> ext_seed(seed_len);
  prg.Fill(absl::MakeSpan(ext_seed));

  auto r = RandA(1);
  auto& r_val = r[0].val;
  auto& r_mac = r[0].mac;

  for (size_t i = 0; i < seed_len; ++i) {
    YACL_ENFORCE(ndss_val_buff_[i].size() == ndss_mac_buff_[i].size());

    auto cur_len = ndss_val_buff_[i].size();
    auto coef = internal::op::Rand(ext_seed[i], cur_len);
    auto val_affine = internal::op::InPro(absl::MakeSpan(coef),
                                          absl::MakeSpan(ndss_val_buff_[i]));
    auto mac_affine = internal::op::InPro(absl::MakeSpan(coef),
                                          absl::MakeSpan(ndss_mac_buff_[i]));
    r_val = r_val + val_affine;
    r_mac = r_mac + mac_affine;
  }

  auto remote_r_val = conn->Exchange(r_val.GetVal());
  auto real_val = r_val + PTy(remote_r_val);
  auto zero_mac = r_mac - real_val * key_;

  if (ctx_->GetRank() == 0) {
    zero_mac = PTy::Neg(zero_mac);
  }

  auto bv = yacl::ByteContainerView(&zero_mac, sizeof(PTy));
  auto remote_bv = conn->ExchangeWithCommit(bv);
  bool flag = (bv == yacl::ByteContainerView(remote_bv));
  SPDLOG_INFO("NdssDelayCheck is {}", flag);

  ndss_val_buff_.clear();
  ndss_mac_buff_.clear();

  return flag;
}

void Protocol::CheckBufferAppend(absl::Span<const PTy> in) {
  std::copy(in.begin(), in.end(), std::back_inserter(check_buff_));
}

void Protocol::CheckBufferAppend(const PTy& in) {
  check_buff_.emplace_back(in);
}

bool Protocol::DelayCheck() {
  if (check_buff_.size() == 0) {
    return true;
  }
  auto conn = ctx_->GetConnection();

  auto hash_val = yacl::crypto::Sm3(yacl::ByteContainerView(
      check_buff_.data(), check_buff_.size() * sizeof(internal::PTy)));
  check_buff_.clear();

  auto remote_hash_val = conn->ExchangeWithCommit(
      yacl::ByteContainerView(hash_val.data(), hash_val.size()));
  auto flag = (yacl::ByteContainerView(hash_val.data(), hash_val.size()) ==
               yacl::ByteContainerView(remote_hash_val));
  SPDLOG_INFO("delay check result: {} ", flag);
  return flag;
}

}  // namespace mosac
