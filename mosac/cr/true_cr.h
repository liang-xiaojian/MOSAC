#pragma once
#include <memory>
#include <vector>

#include "mosac/context/context.h"
#include "mosac/context/state.h"
#include "mosac/cr/cr.h"
#include "mosac/cr/utils/ot_adapter.h"
#include "mosac/cr/utils/vole_adapter.h"
#include "mosac/ss/type.h"

namespace mosac {

class TrueCorrelation : public Correlation {
 private:
  bool setup_ot_{false};
  bool setup_vole_{false};  // useless

 public:
  // OT adapter
  std::shared_ptr<ot::OtAdapter> ot_sender_;
  std::shared_ptr<ot::OtAdapter> ot_receiver_;
  // Vole adapter
  std::shared_ptr<vole::VoleAdapter> vole_sender_;
  std::shared_ptr<vole::VoleAdapter> vole_receiver_;

  // delay check
  std::vector<internal::PTy> delay_check_buff_;
  std::vector<std::vector<internal::PTy>> val_delay_check_buff_;
  std::vector<std::vector<internal::PTy>> mac_delay_check_buff_;

  TrueCorrelation(std::shared_ptr<Context> ctx) : Correlation(ctx) {}

  ~TrueCorrelation() {}

  void InitOtAdapter() {
    if (setup_ot_ == true) return;

    auto conn = ctx_->GetConnection();
    if (ctx_->GetRank() == 0) {
      ot_sender_ = std::make_shared<ot::YaclSsOtAdapter>(conn->Spawn(), true);
      // ot_sender_ =
      //     std::make_shared<ot::YaclFerretOtAdapter>(conn->Spawn(), true);
      ot_sender_->OneTimeSetup();

      ot_receiver_ =
          std::make_shared<ot::YaclSsOtAdapter>(conn->Spawn(), false);
      // ot_receiver_ =
      //     std::make_shared<ot::YaclFerretOtAdapter>(conn->Spawn(), false);
      ot_receiver_->OneTimeSetup();
    } else {
      ot_receiver_ =
          std::make_shared<ot::YaclSsOtAdapter>(conn->Spawn(), false);
      // ot_receiver_ =
      //     std::make_shared<ot::YaclFerretOtAdapter>(conn->Spawn(), false);
      ot_receiver_->OneTimeSetup();

      ot_sender_ = std::make_shared<ot::YaclSsOtAdapter>(conn->Spawn(), true);
      // ot_sender_ =
      //     std::make_shared<ot::YaclFerretOtAdapter>(conn->Spawn(), true);
      ot_sender_->OneTimeSetup();
    }

    setup_ot_ = true;
  }

  void InitVoleAdapter() {
    YACL_ENFORCE(setup_vole_ == false);
    if (setup_ot_ == false) InitOtAdapter();

    auto conn = ctx_->GetConnection();
    if (ctx_->GetRank() == 0) {
      vole_sender_ =
          std::make_shared<vole::WolverineVoleAdapter>(conn, ot_sender_, key_);
      vole_sender_->OneTimeSetup();

      vole_receiver_ =
          std::make_shared<vole::WolverineVoleAdapter>(conn, ot_receiver_);
      vole_receiver_->OneTimeSetup();
    } else {
      vole_receiver_ =
          std::make_shared<vole::WolverineVoleAdapter>(conn, ot_receiver_);
      vole_receiver_->OneTimeSetup();

      vole_sender_ =
          std::make_shared<vole::WolverineVoleAdapter>(conn, ot_sender_, key_);
      vole_sender_->OneTimeSetup();
    }
    setup_vole_ = true;
  }

  void OneTimeSetup() override {
    if (setup_ot_ == true) return;
    InitOtAdapter();
    if (setup_vole_ == true) return;
    InitVoleAdapter();
  }

  internal::PTy GetKey() const override { return key_; }

  void SetKey(internal::PTy key) override {
    key_ = key;

    setup_vole_ = false;  // set it as false
    InitVoleAdapter();
    YACL_ENFORCE(setup_vole_ == true);
  }

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

 private:
  void AuthSet(absl::Span<const internal::PTy> in,
               absl::Span<internal::ATy> out);

  void AuthGet(absl::Span<internal::ATy> out);

  std::vector<internal::PTy> OpenAndCheck(absl::Span<const internal::ATy> in);
  std::vector<internal::PTy> OpenAndDelayCheck(
      absl::Span<const internal::ATy> in);

  std::vector<internal::ATy> Mul(absl::Span<const internal::PTy> lhs,
                                 absl::Span<const internal::ATy> rhs);
  std::vector<internal::ATy> Mul(absl::Span<const internal::ATy> lhs,
                                 absl::Span<const internal::PTy> rhs);
  std::vector<internal::ATy> Mul(absl::Span<const internal::ATy> lhs,
                                 absl::Span<const internal::ATy> rhs);
  internal::ATy _NMul(absl::Span<const internal::ATy> in);
  internal::ATy _Func(absl::Span<const internal::ATy> in,
                      const internal::ATy& x);
  std::vector<internal::ATy> Inv(absl::Span<const internal::ATy> in);

  // BeaverTriple With Chosen B
  void BeaverTripleWithChosenB(absl::Span<internal::ATy> a,
                               absl::Span<const internal::ATy> b,
                               absl::Span<internal::ATy> c);

  std::vector<size_t> ASTSet_basic_2k(absl::Span<internal::ATy> a,
                                      absl::Span<internal::ATy> b);

  void ASTGet_basic_2k(absl::Span<internal::ATy> a,
                       absl::Span<internal::ATy> b);

  std::vector<std::vector<size_t>> ASTSet_batch_basic_2k(
      size_t num, size_t T, std::vector<std::vector<internal::ATy>>& vec_a,
      std::vector<std::vector<internal::ATy>>& vec_b);

  void ASTGet_batch_basic_2k(size_t num, size_t T,
                             std::vector<std::vector<internal::ATy>>& vec_a,
                             std::vector<std::vector<internal::ATy>>& vec_b);

  void compose_vec_2k(size_t i,
                      const std::vector<std::vector<internal::ATy>>& vecs,
                      absl::Span<internal::ATy> out);

  void compose_perm_2k(size_t i, const std::vector<std::vector<size_t>>& perms,
                       absl::Span<size_t> out);

  void ASTSet_merge(size_t in_num, size_t out_num,
                    std::vector<std::vector<size_t>>& in_perms,
                    std::vector<std::vector<internal::ATy>>& in_vec_a,
                    std::vector<std::vector<internal::ATy>>& in_vec_b,
                    std::vector<std::vector<size_t>>& out_perms,
                    std::vector<std::vector<internal::ATy>>& out_vec_a,
                    std::vector<std::vector<internal::ATy>>& out_vec_b);

  void ASTGet_merge(size_t in_num, size_t out_num,
                    std::vector<std::vector<internal::ATy>>& in_vec_a,
                    std::vector<std::vector<internal::ATy>>& in_vec_b,
                    std::vector<std::vector<internal::ATy>>& out_vec_a,
                    std::vector<std::vector<internal::ATy>>& out_vec_b);

  bool DelayCheck() override;

  void AShareLineCombineDelayCheck();
  void AShareLineCombineDelayCheck(absl::Span<const internal::ATy> in);
  // TODO:
  // internal::PTy SingleOpenAndCheck(const internal::ATy& in);
 private:
  uint64_t rand_set_num{0};
  uint64_t rand_get_num{0};
  uint64_t auth_set_num{0};
  uint64_t auth_get_num{0};

 public:
  uint64_t GetRandSetNum() const { return rand_set_num; }
  uint64_t GetRandGetNum() const { return rand_get_num; }
  uint64_t GetAuthSetNum() const { return auth_set_num; }
  uint64_t GetAuthGetNum() const { return auth_get_num; }
};

}  // namespace mosac
