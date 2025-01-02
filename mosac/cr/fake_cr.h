#pragma once
#include <memory>
#include <vector>

#include "mosac/context/context.h"
#include "mosac/context/state.h"
#include "mosac/cr/cr.h"
#include "mosac/ss/type.h"

namespace mosac {

class FakeCorrelation : public Correlation {
 public:
  FakeCorrelation(std::shared_ptr<Context> ctx) : Correlation(ctx) {}

  ~FakeCorrelation() {}

  internal::PTy GetKey() const override { return key_; }

  void SetKey(internal::PTy key) override { key_ = key; }

  void OneTimeSetup() override { ; }

  // entry
  void BeaverTriple(absl::Span<internal::ATy> a, absl::Span<internal::ATy> b,
                    absl::Span<internal::ATy> c) override;

  // entry
  void RandomSet(absl::Span<internal::ATy> out) override;
  void RandomGet(absl::Span<internal::ATy> out) override;
  void RandomAuth(absl::Span<internal::ATy> out) override;

  // entry
  void ShuffleSet(absl::Span<const size_t> perm,
                  absl::Span<internal::PTy> delta, size_t repeat = 1) override;

  void ShuffleGet(absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                  size_t repeat = 1) override;

  void BatchShuffleSet(
      size_t batch_num, size_t per_size, size_t repeat,
      const std::vector<std::vector<size_t>>& perms,
      std::vector<std::vector<internal::PTy>>& vec_delta) override;
  void BatchShuffleGet(size_t batch_num, size_t per_size, size_t repeat,
                       std::vector<std::vector<internal::PTy>>& vec_a,
                       std::vector<std::vector<internal::PTy>>& vec_b) override;
  void _BatchShuffleSet(
      size_t batch_num, size_t per_size, size_t repeat,
      const std::vector<std::vector<size_t>>& perms,
      std::vector<std::vector<internal::PTy>>& vec_delta) override;
  void _BatchShuffleGet(
      size_t batch_num, size_t per_size, size_t repeat,
      std::vector<std::vector<internal::PTy>>& vec_a,
      std::vector<std::vector<internal::PTy>>& vec_b) override;
  // entry
  std::vector<size_t> ASTSet(absl::Span<internal::ATy> a,
                             absl::Span<internal::ATy> b) override;
  void ASTGet(absl::Span<internal::ATy> a,
              absl::Span<internal::ATy> b) override;

  std::vector<size_t> ASTSet_2k(size_t T, absl::Span<internal::ATy> a,
                                absl::Span<internal::ATy> b) override;
  void ASTGet_2k(size_t T, absl::Span<internal::ATy> a,
                 absl::Span<internal::ATy> b) override;

  // entry
  internal::ATy NMul(absl::Span<internal::ATy> r) override;

  bool DelayCheck() override { return true; }
};

}  // namespace mosac
