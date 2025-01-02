#include "mosac/cr/cr.h"

#include "mosac/cr/param.h"

namespace mosac {

// register string
const std::string Correlation::id = std::string("Correlation");

BeaverTy Correlation::BeaverTriple(size_t num) {
  if (cache_ != nullptr && cache_->BeaverCacheSize() >= num) {
    return cache_->BeaverTriple(num);
  }
  SPDLOG_DEBUG("BeaverTriple Uncached");
  std::vector<internal::ATy> a(num);
  std::vector<internal::ATy> b(num);
  std::vector<internal::ATy> c(num);
  BeaverTriple(absl::MakeSpan(a), absl::MakeSpan(b), absl::MakeSpan(c));
  YACL_ENFORCE(DelayCheck());
  return BeaverTy(std::move(a), std::move(b), std::move(c));
}

AuthTy Correlation::RandomSet(size_t num) {
  if (cache_ != nullptr && cache_->RandomSetSize() >= num) {
    return cache_->RandomSet(num);
  }
  SPDLOG_DEBUG("RandomSet Uncached");
  std::vector<internal::ATy> ret(num);
  RandomSet(absl::MakeSpan(ret));
  YACL_ENFORCE(DelayCheck());
  return AuthTy(std::move(ret));
}

AuthTy Correlation::RandomGet(size_t num) {
  if (cache_ != nullptr && cache_->RandomGetSize() >= num) {
    return cache_->RandomGet(num);
  }
  SPDLOG_DEBUG("RandomGet Uncached");
  std::vector<internal::ATy> ret(num);
  RandomGet(absl::MakeSpan(ret));
  YACL_ENFORCE(DelayCheck());
  return AuthTy(std::move(ret));
}

AuthTy Correlation::RandomAuth(size_t num) {
  if (cache_ != nullptr && cache_->RandomSetSize() >= num &&
      cache_->RandomGetSize() >= num) {
    AuthTy set = cache_->RandomSet(num);
    AuthTy get = cache_->RandomGet(num);
    std::vector<internal::ATy> out(num);
    internal::op::Add(
        absl::MakeConstSpan(
            reinterpret_cast<const internal::PTy*>(set.data.data()), 2 * num),
        absl::MakeConstSpan(
            reinterpret_cast<const internal::PTy*>(get.data.data()), 2 * num),
        absl::MakeSpan(reinterpret_cast<internal::PTy*>(out.data()), 2 * num));
    return AuthTy(std::move(out));
  }
  SPDLOG_DEBUG("RandomAuth Uncached");
  std::vector<internal::ATy> ret(num);
  RandomAuth(absl::MakeSpan(ret));
  YACL_ENFORCE(DelayCheck());
  return AuthTy(std::move(ret));
}

ShuffleSTy Correlation::ShuffleSet(size_t num, size_t repeat) {
  if (cache_ != nullptr && cache_->ShuffleSetCount(num, repeat)) {
    return cache_->ShuffleSet(num, repeat);
  }
  SPDLOG_DEBUG("Shuffle Set Uncached");
  std::vector<internal::PTy> delta(num * repeat);
  std::vector<size_t> perm = GenPerm(num);
  ShuffleSet(absl::MakeSpan(perm), absl::MakeSpan(delta), repeat);
  YACL_ENFORCE(DelayCheck());
  return ShuffleSTy(std::move(delta), std::move(perm));
}

ShuffleGTy Correlation::ShuffleGet(size_t num, size_t repeat) {
  if (cache_ != nullptr && cache_->ShuffleGetCount(num, repeat)) {
    return cache_->ShuffleGet(num, repeat);
  }
  SPDLOG_DEBUG("Shuffle Get Uncached");
  std::vector<internal::PTy> a(num * repeat);
  std::vector<internal::PTy> b(num * repeat);
  ShuffleGet(absl::MakeSpan(a), absl::MakeSpan(b), repeat);
  YACL_ENFORCE(DelayCheck());
  return ShuffleGTy(std::move(a), std::move(b));
}

ASTSTy Correlation::ASTSet(size_t num) {
  if (cache_ != nullptr && cache_->ASTSetCount(num)) {
    return cache_->ASTSet(num);
  }
  SPDLOG_DEBUG("AST Set Uncached");
  std::vector<internal::ATy> a(num);
  std::vector<internal::ATy> b(num);
  std::vector<size_t> perm = ASTSet(absl::MakeSpan(a), absl::MakeSpan(b));
  YACL_ENFORCE(DelayCheck());
  return ASTSTy(std::move(perm), std::move(a), std::move(b));
}

ASTGTy Correlation::ASTGet(size_t num) {
  if (cache_ != nullptr && cache_->ASTGetCount(num)) {
    return cache_->ASTGet(num);
  }
  SPDLOG_DEBUG("AST Get Uncached");
  std::vector<internal::ATy> a(num);
  std::vector<internal::ATy> b(num);
  ASTGet(absl::MakeSpan(a), absl::MakeSpan(b));
  YACL_ENFORCE(DelayCheck());
  return ASTGTy(std::move(a), std::move(b));
}

std::vector<ShuffleSTy> Correlation::BatchShuffleSet(size_t batch_num,
                                                     size_t per_size,
                                                     size_t repeat) {
  std::vector<ShuffleSTy> ret;

  const auto ot_per_shuffle = yacl::math::Log2Ceil(per_size) * per_size;
  const auto small_batch_size =
      yacl::math::DivCeil(param::kBatchOtSize, ot_per_shuffle);

  auto small_batch_num = yacl::math::DivCeil(batch_num, small_batch_size);

  // FIXME: Try to move this to true_cr
  for (size_t _ = 0; _ < small_batch_num; ++_) {
    auto remain = std::min(small_batch_size, batch_num - _ * small_batch_size);
    std::vector<std::vector<size_t>> perms;
    for (size_t i = 0; i < remain; ++i) {
      auto perm = GenPerm(per_size);
      perms.emplace_back(std::move(perm));
    }
    std::vector<std::vector<internal::PTy>> vec_delta;
    BatchShuffleSet(remain, per_size, repeat, perms, vec_delta);

    for (size_t i = 0; i < remain; ++i) {
      ret.emplace_back(std::move(vec_delta[i]), std::move(perms[i]));
    }
  }

  // std::vector<std::vector<size_t>> perms;
  // for (size_t i = 0; i < batch_num; ++i) {
  //   auto perm = GenPerm(per_size);
  //   perms.emplace_back(std::move(perm));
  // }
  // std::vector<std::vector<internal::PTy>> vec_delta;
  // BatchShuffleSet(batch_num, per_size, repeat, perms, vec_delta);

  // for (size_t i = 0; i < batch_num; ++i) {
  //   ret.emplace_back(std::move(vec_delta[i]), std::move(perms[i]));
  // }
  return ret;
}

std::vector<ShuffleGTy> Correlation::BatchShuffleGet(size_t batch_num,
                                                     size_t per_size,
                                                     size_t repeat) {
  std::vector<ShuffleGTy> ret;

  const auto ot_per_shuffle = yacl::math::Log2Ceil(per_size) * per_size;
  const auto small_batch_size =
      yacl::math::DivCeil(param::kBatchOtSize, ot_per_shuffle);
  auto small_batch_num = yacl::math::DivCeil(batch_num, small_batch_size);

  // FIXME: Try to move this to true_cr
  for (size_t _ = 0; _ < small_batch_num; ++_) {
    auto remain = std::min(small_batch_size, batch_num - _ * small_batch_size);

    std::vector<std::vector<internal::PTy>> vec_a;
    std::vector<std::vector<internal::PTy>> vec_b;
    BatchShuffleGet(remain, per_size, repeat, vec_a, vec_b);

    for (size_t i = 0; i < remain; ++i) {
      ret.emplace_back(std::move(vec_a[i]), std::move(vec_b[i]));
    }
  }
  // std::vector<std::vector<internal::PTy>> vec_a;
  // std::vector<std::vector<internal::PTy>> vec_b;
  // BatchShuffleGet(batch_num, per_size, repeat, vec_a, vec_b);
  // std::vector<ShuffleGTy> ret;
  // for (size_t i = 0; i < batch_num; ++i) {
  //   ret.emplace_back(std::move(vec_a[i]), std::move(vec_b[i]));
  // }
  return ret;
}

std::vector<ShuffleSTy> Correlation::_BatchShuffleSet(size_t batch_num,
                                                      size_t per_size,
                                                      size_t repeat) {
  std::vector<ShuffleSTy> ret;

  const auto ot_per_shuffle = yacl::math::Log2Ceil(per_size) * per_size;
  const auto small_batch_size =
      yacl::math::DivCeil(param::kBatchOtSize >> 3, ot_per_shuffle);

  auto small_batch_num = yacl::math::DivCeil(batch_num, small_batch_size);

  // FIXME: Try to move this to true_cr
  for (size_t _ = 0; _ < small_batch_num; ++_) {
    auto remain = std::min(small_batch_size, batch_num - _ * small_batch_size);
    std::vector<std::vector<size_t>> perms;
    for (size_t i = 0; i < remain; ++i) {
      auto perm = GenPerm(per_size);
      perms.emplace_back(std::move(perm));
    }
    std::vector<std::vector<internal::PTy>> vec_delta;
    _BatchShuffleSet(remain, per_size, repeat, perms, vec_delta);

    for (size_t i = 0; i < remain; ++i) {
      ret.emplace_back(std::move(vec_delta[i]), std::move(perms[i]));
    }
  }

  return ret;
}

std::vector<ShuffleGTy> Correlation::_BatchShuffleGet(size_t batch_num,
                                                      size_t per_size,
                                                      size_t repeat) {
  std::vector<ShuffleGTy> ret;

  const auto ot_per_shuffle = yacl::math::Log2Ceil(per_size) * per_size;
  const auto small_batch_size =
      yacl::math::DivCeil(param::kBatchOtSize >> 3, ot_per_shuffle);
  auto small_batch_num = yacl::math::DivCeil(batch_num, small_batch_size);

  // FIXME: Try to move this to true_cr
  for (size_t _ = 0; _ < small_batch_num; ++_) {
    auto remain = std::min(small_batch_size, batch_num - _ * small_batch_size);

    std::vector<std::vector<internal::PTy>> vec_a;
    std::vector<std::vector<internal::PTy>> vec_b;
    _BatchShuffleGet(remain, per_size, repeat, vec_a, vec_b);

    for (size_t i = 0; i < remain; ++i) {
      ret.emplace_back(std::move(vec_a[i]), std::move(vec_b[i]));
    }
  }

  return ret;
}

ASTSTy Correlation::ASTSet_2k(size_t T, size_t num) {
  std::vector<internal::ATy> a(num);
  std::vector<internal::ATy> b(num);
  std::vector<size_t> perm = ASTSet_2k(T, absl::MakeSpan(a), absl::MakeSpan(b));
  YACL_ENFORCE(DelayCheck());
  return ASTSTy(std::move(perm), std::move(a), std::move(b));
}

ASTGTy Correlation::ASTGet_2k(size_t T, size_t num) {
  std::vector<internal::ATy> a(num);
  std::vector<internal::ATy> b(num);
  ASTGet_2k(T, absl::MakeSpan(a), absl::MakeSpan(b));
  YACL_ENFORCE(DelayCheck());
  return ASTGTy(std::move(a), std::move(b));
}

NMulTy Correlation::NMul(size_t num) {
  if (cache_ != nullptr && cache_->NMulCount(num)) {
    return cache_->NMul(num);
  }
  SPDLOG_DEBUG("NMul Uncached");
  std::vector<internal::ATy> r(num);
  internal::ATy mul_inv = NMul(absl::MakeSpan(r));
  YACL_ENFORCE(DelayCheck());
  return NMulTy(std::move(r), mul_inv);
}

// cache
void Correlation::force_cache(size_t beaver_num, size_t rand_set_num,
                              size_t rand_get_num,
                              const std::vector<uint64_t>& shuffle_set_shape,
                              const std::vector<uint64_t>& shuffle_get_shape,
                              const std::vector<uint64_t>& ast_set_shape,
                              const std::vector<uint64_t>& ast_get_shape,
                              const std::vector<uint64_t>& n_mul_shape) {
  cache_ = std::make_unique<CorrelationCache>();
  // beaver
  if (beaver_num != 0) {
    cache_->beaver_cache = BeaverTy(beaver_num);
    auto& a = cache_->beaver_cache.a;
    auto& b = cache_->beaver_cache.b;
    auto& c = cache_->beaver_cache.c;
    BeaverTriple(absl::MakeSpan(a), absl::MakeSpan(b), absl::MakeSpan(c));
  }
  // random
  {
    cache_->random_set_cache = AuthTy(rand_set_num);
    cache_->random_get_cache = AuthTy(rand_get_num);
    auto& set_data = cache_->random_set_cache.data;
    auto& get_data = cache_->random_get_cache.data;
    if (ctx_->GetRank() == 0) {
      RandomSet(absl::MakeSpan(set_data));
      RandomGet(absl::MakeSpan(get_data));
    } else {
      RandomGet(absl::MakeSpan(get_data));
      RandomSet(absl::MakeSpan(set_data));
    }
  }
  // shuffle
  {
    auto& set_map = cache_->shuffle_set_cache;
    auto& get_map = cache_->shuffle_get_cache;
    if (ctx_->GetRank() == 0) {
      // set
      for (const auto index : shuffle_set_shape) {
        ShuffleSTy tmp(index >> 8, index & 0xFF);
        tmp.perm = GenPerm(index >> 8);
        ShuffleSet(absl::MakeSpan(tmp.perm), absl::MakeSpan(tmp.delta),
                   index & 0xFF);
        auto& vec = set_map[index];
        vec.emplace_back(tmp);
      }
      // get
      for (const auto index : shuffle_get_shape) {
        ShuffleGTy tmp(index >> 8, index & 0xFF);
        ShuffleGet(absl::MakeSpan(tmp.a), absl::MakeSpan(tmp.b), index & 0xFF);
        auto& vec = get_map[index];
        vec.emplace_back(tmp);
      }
    } else {
      // get
      for (const auto index : shuffle_get_shape) {
        ShuffleGTy tmp(index >> 8, index & 0xFF);
        ShuffleGet(absl::MakeSpan(tmp.a), absl::MakeSpan(tmp.b), index & 0xFF);
        auto& vec = get_map[index];
        vec.emplace_back(tmp);
      }
      // set
      for (const auto index : shuffle_set_shape) {
        ShuffleSTy tmp(index >> 8, index & 0xFF);
        tmp.perm = GenPerm(index >> 8);
        ShuffleSet(absl::MakeSpan(tmp.perm), absl::MakeSpan(tmp.delta),
                   index & 0xFF);
        auto& vec = set_map[index];
        vec.emplace_back(tmp);
      }
    }
  }
  // AST
  {
    auto& ast_set_map = cache_->AST_set_cache;
    auto& ast_get_map = cache_->AST_get_cache;
    if (ctx_->GetRank() == 0) {
      // set
      for (const auto num : ast_set_shape) {
        ASTSTy tmp(num);
        tmp.perm = ASTSet(absl::MakeSpan(tmp.a), absl::MakeSpan(tmp.b));
        auto& vec = ast_set_map[num];
        vec.emplace_back(tmp);
      }
      // get
      for (const auto num : ast_get_shape) {
        ASTGTy tmp(num);
        ASTGet(absl::MakeSpan(tmp.a), absl::MakeSpan(tmp.b));
        auto& vec = ast_get_map[num];
        vec.emplace_back(tmp);
      }
    } else {
      // get
      for (const auto num : ast_get_shape) {
        ASTGTy tmp(num);
        ASTGet(absl::MakeSpan(tmp.a), absl::MakeSpan(tmp.b));
        auto& vec = ast_get_map[num];
        vec.emplace_back(tmp);
      }
      // set
      for (const auto num : ast_set_shape) {
        ASTSTy tmp(num);
        tmp.perm = ASTSet(absl::MakeSpan(tmp.a), absl::MakeSpan(tmp.b));
        auto& vec = ast_set_map[num];
        vec.emplace_back(tmp);
      }
    }
  }
  // N Mul
  {
    auto& nmul_map = cache_->NMul_cache;
    for (const auto num : n_mul_shape) {
      auto& vec = nmul_map[num];
      NMulTy tmp(num);
      tmp.mul_inv = NMul(absl::MakeSpan(tmp.r));
      vec.emplace_back(tmp);
    }
  }
  YACL_ENFORCE(DelayCheck());
}

BeaverTy CorrelationCache::BeaverTriple(size_t num) {
  const size_t remain = BeaverCacheSize();
  YACL_ENFORCE(num <= remain);
  // copy
  std::vector<internal::ATy> a(beaver_cache.a.end() - num,
                               beaver_cache.a.end());
  std::vector<internal::ATy> b(beaver_cache.b.end() - num,
                               beaver_cache.b.end());
  std::vector<internal::ATy> c(beaver_cache.c.end() - num,
                               beaver_cache.c.end());
  // resize
  beaver_cache.a.resize(remain - num);
  beaver_cache.b.resize(remain - num);
  beaver_cache.c.resize(remain - num);

  return BeaverTy(std::move(a), std::move(b), std::move(c));
}

AuthTy CorrelationCache::RandomSet(size_t num) {
  const size_t remain = RandomSetSize();
  YACL_ENFORCE(num <= remain);
  // copy
  std::vector<internal::ATy> data(random_set_cache.data.end() - num,
                                  random_set_cache.data.end());
  // resize
  random_set_cache.data.resize(remain - num);

  return AuthTy(std::move(data));
}

AuthTy CorrelationCache::RandomGet(size_t num) {
  const size_t remain = RandomGetSize();
  YACL_ENFORCE(num <= remain);
  // copy
  std::vector<internal::ATy> data(random_get_cache.data.end() - num,
                                  random_get_cache.data.end());
  // resize
  random_get_cache.data.resize(remain - num);

  return AuthTy(std::move(data));
}

ShuffleSTy CorrelationCache::ShuffleSet(size_t num, size_t repeat) {
  YACL_ENFORCE(ShuffleSetCount(num, repeat) > 0);
  const uint64_t index = ((num << 8) | repeat);
  ShuffleSTy ret = std::move(shuffle_set_cache[index].back());
  shuffle_set_cache[index].pop_back();
  return ret;
}

ShuffleGTy CorrelationCache::ShuffleGet(size_t num, size_t repeat) {
  YACL_ENFORCE(ShuffleGetCount(num, repeat) > 0);
  const uint64_t index = ((num << 8) | repeat);
  ShuffleGTy ret = std::move(shuffle_get_cache[index].back());
  shuffle_get_cache[index].pop_back();
  return ret;
}

ASTSTy CorrelationCache::ASTSet(size_t num) {
  YACL_ENFORCE(num > 0);
  ASTSTy ret = std::move(AST_set_cache[num].back());
  AST_set_cache[num].pop_back();
  return ret;
}

ASTGTy CorrelationCache::ASTGet(size_t num) {
  YACL_ENFORCE(num > 0);
  ASTGTy ret = std::move(AST_get_cache[num].back());
  AST_get_cache[num].pop_back();
  return ret;
}

NMulTy CorrelationCache::NMul(size_t num) {
  YACL_ENFORCE(num > 0);
  NMulTy ret = std::move(NMul_cache[num].back());
  NMul_cache[num].pop_back();
  return ret;
}

}  // namespace mosac
