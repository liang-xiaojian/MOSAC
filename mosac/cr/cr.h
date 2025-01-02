#pragma once
#include <memory>
#include <vector>

#include "mosac/context/context.h"
#include "mosac/context/state.h"
#include "mosac/ss/type.h"

namespace mosac {

// Beaver Triple Type
struct BeaverTy {
  std::vector<internal::ATy> a;
  std::vector<internal::ATy> b;
  std::vector<internal::ATy> c;
  BeaverTy() { ; }
  BeaverTy(std::vector<internal::ATy>&& aa, std::vector<internal::ATy>&& bb,
           std::vector<internal::ATy>&& cc) {
    a = std::move(aa);
    b = std::move(bb);
    c = std::move(cc);
  }
  BeaverTy(uint32_t n) {
    a.resize(n);
    b.resize(n);
    c.resize(n);
  }
};

// Authorication Type
struct AuthTy {
  std::vector<internal::ATy> data;
  AuthTy() { ; }
  AuthTy(uint32_t n) { data.resize(n); }
  AuthTy(std::vector<internal::ATy>&& in) { data = std::move(in); }
};

// Shuffle Set Type
struct ShuffleSTy {
  std::vector<internal::PTy> delta;
  std::vector<size_t> perm;
  ShuffleSTy() { ; }
  ShuffleSTy(uint32_t n, uint32_t repeat = 1) {
    delta.resize(n * repeat);
    perm.resize(n);
  }
  ShuffleSTy(std::vector<internal::PTy>&& _delta, std::vector<size_t>&& _perm) {
    delta = std::move(_delta);
    perm = std::move(_perm);
  }
};

// Shuffle Get Type
struct ShuffleGTy {
  std::vector<internal::PTy> a;
  std::vector<internal::PTy> b;
  ShuffleGTy() { ; }
  ShuffleGTy(uint32_t n, uint32_t repeat = 1) {
    a.resize(n * repeat);
    b.resize(n * repeat);
  }
  ShuffleGTy(std::vector<internal::PTy>&& _a, std::vector<internal::PTy>&& _b) {
    a = std::move(_a);
    b = std::move(_b);
  }
};

// short for AST Set Type
struct ASTSTy {
  std::vector<size_t> perm;
  std::vector<internal::ATy> a;
  std::vector<internal::ATy> b;

  ASTSTy() { ; }
  ASTSTy(uint32_t n) {
    perm.resize(n);
    a.resize(n);
    b.resize(n);
  }
  ASTSTy(std::vector<size_t>&& _perm, std::vector<internal::ATy>&& _a,
         std::vector<internal::ATy>&& _b) {
    perm = std::move(_perm);
    a = std::move(_a);
    b = std::move(_b);
  }
};

// short for AST Get Type
struct ASTGTy {
  std::vector<internal::ATy> a;
  std::vector<internal::ATy> b;

  ASTGTy() { ; }
  ASTGTy(uint32_t n) {
    a.resize(n);
    b.resize(n);
  }
  ASTGTy(std::vector<internal::ATy>&& _a, std::vector<internal::ATy>&& _b) {
    a = std::move(_a);
    b = std::move(_b);
  }
};

// n-mul structure
struct NMulTy {
  std::vector<internal::ATy> r;
  internal::ATy mul_inv;

  NMulTy() { ; }
  NMulTy(uint32_t n) { r.resize(n); }
  NMulTy(std::vector<internal::ATy>&& _r, internal::ATy _mul_inv) {
    r = std::move(_r);
    mul_inv = _mul_inv;
  }
};

struct CorrelationCache;

class Correlation : public State {
 protected:
  std::shared_ptr<Context> ctx_;
  internal::PTy key_;

 public:
  static const std::string id;

  Correlation(std::shared_ptr<Context> ctx) : ctx_(ctx) {}

  virtual ~Correlation() {}

  virtual internal::PTy GetKey() const { return key_; }

  virtual void SetKey(internal::PTy key) { key_ = key; }

  virtual void OneTimeSetup() = 0;

  // implementation
  virtual void BeaverTriple(absl::Span<internal::ATy> a,
                            absl::Span<internal::ATy> b,
                            absl::Span<internal::ATy> c) = 0;
  virtual void RandomSet(absl::Span<internal::ATy> out) = 0;
  virtual void RandomGet(absl::Span<internal::ATy> out) = 0;
  virtual void RandomAuth(absl::Span<internal::ATy> out) = 0;
  virtual void ShuffleSet(absl::Span<const size_t> perm,
                          absl::Span<internal::PTy> delta,
                          size_t repeat = 1) = 0;
  virtual void ShuffleGet(absl::Span<internal::PTy> a,
                          absl::Span<internal::PTy> b, size_t repeat = 1) = 0;
  virtual std::vector<size_t> ASTSet(absl::Span<internal::ATy> a,
                                     absl::Span<internal::ATy> b) = 0;
  virtual void ASTGet(absl::Span<internal::ATy> a,
                      absl::Span<internal::ATy> b) = 0;
  virtual internal::ATy NMul(absl::Span<internal::ATy> r) = 0;

  // solve the case for ndss shuffle (offline benchmark currently)
  virtual void BatchShuffleSet(
      size_t batch_num, size_t per_size, size_t repeat,
      const std::vector<std::vector<size_t>>& perms,
      std::vector<std::vector<internal::PTy>>& vec_delta) = 0;
  virtual void BatchShuffleGet(
      size_t batch_num, size_t per_size, size_t repeat,
      std::vector<std::vector<internal::PTy>>& vec_a,
      std::vector<std::vector<internal::PTy>>& vec_b) = 0;
  virtual void _BatchShuffleSet(
      size_t batch_num, size_t per_size, size_t repeat,
      const std::vector<std::vector<size_t>>& perms,
      std::vector<std::vector<internal::PTy>>& vec_delta) = 0;
  virtual void _BatchShuffleGet(
      size_t batch_num, size_t per_size, size_t repeat,
      std::vector<std::vector<internal::PTy>>& vec_a,
      std::vector<std::vector<internal::PTy>>& vec_b) = 0;
  // solve the case for 2^k (offline benchmark currently)
  virtual std::vector<size_t> ASTSet_2k(size_t T, absl::Span<internal::ATy> a,
                                        absl::Span<internal::ATy> b) = 0;
  virtual void ASTGet_2k(size_t T, absl::Span<internal::ATy> a,
                         absl::Span<internal::ATy> b) = 0;

  virtual bool DelayCheck() = 0;

  // interface
  BeaverTy BeaverTriple(size_t num);
  AuthTy RandomSet(size_t num);
  AuthTy RandomGet(size_t num);
  AuthTy RandomAuth(size_t num);
  ShuffleSTy ShuffleSet(size_t num, size_t repeat = 1);
  ShuffleGTy ShuffleGet(size_t num, size_t repeat = 1);
  ASTSTy ASTSet(size_t num);
  ASTGTy ASTGet(size_t num);
  NMulTy NMul(size_t num);

  // solve the case for ndss shuffle (offline benchmark currently)
  std::vector<ShuffleSTy> BatchShuffleSet(size_t batch_num, size_t per_size,
                                          size_t repeat = 1);
  std::vector<ShuffleGTy> BatchShuffleGet(size_t batch_num, size_t per_size,
                                          size_t repeat = 1);
  std::vector<ShuffleSTy> _BatchShuffleSet(size_t batch_num, size_t per_size,
                                           size_t repeat = 1);
  std::vector<ShuffleGTy> _BatchShuffleGet(size_t batch_num, size_t per_size,
                                           size_t repeat = 1);
  // solve the case for 2^k (offline benchmark currently)
  ASTSTy ASTSet_2k(size_t T, size_t num);
  ASTGTy ASTGet_2k(size_t T, size_t num);

  // ------------ cache -------------
 private:
  size_t b_num_{0};
  size_t r_s_num_{0};
  size_t r_g_num_{0};
  std::vector<uint64_t> s_s_shape_;
  std::vector<uint64_t> s_g_shape_;
  std::vector<uint64_t> ast_s_shape_;
  std::vector<uint64_t> ast_g_shape_;
  std::vector<uint64_t> n_mul_shape_;

  std::unique_ptr<CorrelationCache> cache_;

 public:
  // cache interface
  void BeaverTriple_cache(size_t num) { b_num_ += num; }
  void RandomSet_cache(size_t num) { r_s_num_ += num; }
  void RandomGet_cache(size_t num) { r_g_num_ += num; }
  void RandomAuth_cache(size_t num) {
    RandomGet_cache(num);
    RandomSet_cache(num);
  }
  void ShuffleSet_cache(size_t num, size_t repeat = 1) {
    s_s_shape_.emplace_back((num << 8) | repeat);
  }
  void ShuffleGet_cache(size_t num, size_t repeat = 1) {
    s_g_shape_.emplace_back((num << 8) | repeat);
  }
  void ASTSet_cache(size_t num) { ast_s_shape_.emplace_back(num); }
  void ASTGet_cache(size_t num) { ast_g_shape_.emplace_back(num); }
  void NMul_cache(size_t num) { n_mul_shape_.emplace_back(num); }

  // force cache
  void force_cache() {
    SPDLOG_INFO(
        "[P{}] FORCE CACHE!!! beaver num : {} , random set num : {} , random "
        "get num: {} , shuffle set num: {} , shuffle get num: {} , AST set "
        "num: {} , AST get num: {} , N mul num: {}",
        ctx_->GetRank(), b_num_, r_s_num_, r_g_num_, s_s_shape_.size(),
        s_g_shape_.size(), ast_s_shape_.size(), ast_g_shape_.size(),
        n_mul_shape_.size());
    force_cache(b_num_, r_s_num_, r_g_num_, s_s_shape_, s_g_shape_,
                ast_s_shape_, ast_g_shape_, n_mul_shape_);
  }

  void force_cache(size_t beaver_num, size_t rand_set_num, size_t rand_get_num,
                   const std::vector<uint64_t>& shuffle_set_shape = {},
                   const std::vector<uint64_t>& shuffle_get_shape = {},
                   const std::vector<uint64_t>& ast_set_shape = {},
                   const std::vector<uint64_t>& ast_get_shape = {},
                   const std::vector<uint64_t>& n_mul_shape = {});

  inline std::vector<uint64_t> GetASTSetShape() const { return ast_s_shape_; }
  inline std::vector<uint64_t> GetASTGetShape() const { return ast_g_shape_; }
  inline std::vector<uint64_t> GetShuffleSetShape() const { return s_s_shape_; }
  inline std::vector<uint64_t> GetShuffleGetShape() const { return s_g_shape_; }
};

struct CorrelationCache {
  BeaverTy beaver_cache;
  AuthTy random_set_cache;
  AuthTy random_get_cache;
  std::unordered_map<uint64_t, std::vector<ShuffleSTy>> shuffle_set_cache;
  std::unordered_map<uint64_t, std::vector<ShuffleGTy>> shuffle_get_cache;
  std::unordered_map<uint64_t, std::vector<ASTSTy>> AST_set_cache;
  std::unordered_map<uint64_t, std::vector<ASTGTy>> AST_get_cache;
  std::unordered_map<uint64_t, std::vector<NMulTy>> NMul_cache;

  size_t BeaverCacheSize() { return beaver_cache.a.size(); }
  size_t RandomSetSize() { return random_set_cache.data.size(); }
  size_t RandomGetSize() { return random_get_cache.data.size(); }

  size_t ShuffleSetCount(size_t num, size_t repeat = 1) {
    uint64_t idx = ((num << 8) | repeat);
    return shuffle_set_cache.count(idx) ? shuffle_set_cache[idx].size() : 0;
  }
  size_t ShuffleGetCount(size_t num, size_t repeat = 1) {
    uint64_t idx = ((num << 8) | repeat);
    return shuffle_get_cache.count(idx) ? shuffle_get_cache[idx].size() : 0;
  }

  size_t ASTSetCount(size_t num) {
    return AST_set_cache.count(num) ? AST_set_cache[num].size() : 0;
  }
  size_t ASTGetCount(size_t num) {
    return AST_get_cache.count(num) ? AST_get_cache[num].size() : 0;
  }
  size_t NMulCount(size_t num) {
    return NMul_cache.count(num) ? NMul_cache[num].size() : 0;
  }

  BeaverTy BeaverTriple(size_t num);
  AuthTy RandomSet(size_t num);
  AuthTy RandomGet(size_t num);
  ShuffleSTy ShuffleSet(size_t num, size_t repeat = 1);
  ShuffleGTy ShuffleGet(size_t num, size_t repeat = 1);
  ASTSTy ASTSet(size_t num);
  ASTGTy ASTGet(size_t num);
  NMulTy NMul(size_t num);
};

}  // namespace mosac
