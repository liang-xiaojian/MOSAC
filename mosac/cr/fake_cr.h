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

  // entry
  std::vector<size_t> ASTSet(absl::Span<internal::ATy> a,
                             absl::Span<internal::ATy> b) override;
  void ASTGet(absl::Span<internal::ATy> a,
              absl::Span<internal::ATy> b) override;

  // entry
  internal::ATy NMul(absl::Span<internal::ATy> r) override;
};

}  // namespace mosac
