#pragma once

#include "mosac/context/state.h"
#include "mosac/cr/utils/ot_adapter.h"
#include "mosac/ss/type.h"

namespace mosac::ot {

class OtHelper {
 public:
  OtHelper(const std::shared_ptr<OtAdapter>& ot_sender,
           const std::shared_ptr<OtAdapter>& ot_receiver) {
    ot_sender_ = ot_sender;
    ot_receiver_ = ot_receiver;
  }

  void MulPPSend(std::shared_ptr<Connection>& conn, absl::Span<internal::PTy> b,
                 absl::Span<internal::PTy> c);

  void MulPPRecv(std::shared_ptr<Connection>& conn, absl::Span<internal::PTy> a,
                 absl::Span<internal::PTy> c);

  void BeaverTriple(std::shared_ptr<Connection>& conn,
                    absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                    absl::Span<internal::PTy> c);

  // a * b = c && A * b = C
  void MulPPExtendSend(std::shared_ptr<Connection>& conn,
                       absl::Span<internal::PTy> b, absl::Span<internal::PTy> c,
                       absl::Span<internal::PTy> C);

  // a * b = c && A * b = C
  void MulPPExtendRecv(std::shared_ptr<Connection>& conn,
                       absl::Span<internal::PTy> a, absl::Span<internal::PTy> c,
                       absl::Span<internal::PTy> A,
                       absl::Span<internal::PTy> C);

  // a * b = c && A * b = C
  void BeaverTripleExtend(std::shared_ptr<Connection>& conn,
                          absl::Span<internal::PTy> a,
                          absl::Span<internal::PTy> b,
                          absl::Span<internal::PTy> c,
                          absl::Span<internal::PTy> A,
                          absl::Span<internal::PTy> C);

  // a * b = c && A * b = C (with given b)
  void MulPPExtendSendWithChosenB(std::shared_ptr<Connection>& conn,
                                  absl::Span<const internal::PTy> b,
                                  absl::Span<internal::PTy> c,
                                  absl::Span<internal::PTy> C);

  // a * b = c && A * b = C (with given b)
  void MulPPExtendRecvWithChosenB(std::shared_ptr<Connection>& conn,
                                  absl::Span<internal::PTy> a,
                                  absl::Span<internal::PTy> c,
                                  absl::Span<internal::PTy> A,
                                  absl::Span<internal::PTy> C);

  // a * b = c && A * b = C (with given b)
  void BeaverTripleExtendWithChosenB(std::shared_ptr<Connection>& conn,
                                     absl::Span<internal::PTy> a,
                                     absl::Span<const internal::PTy> b,
                                     absl::Span<internal::PTy> c,
                                     absl::Span<internal::PTy> A,
                                     absl::Span<internal::PTy> C);

  void BaseVoleSend(std::shared_ptr<Connection>& conn, internal::PTy delta,
                    absl::Span<internal::PTy> c);

  void BaseVoleRecv(std::shared_ptr<Connection>& conn,
                    absl::Span<internal::PTy> a, absl::Span<internal::PTy> b);

  void ShuffleSend(std::shared_ptr<Connection>& conn,
                   absl::Span<const size_t> perm,
                   absl::Span<internal::PTy> delta, size_t repeat = 1);

  void ShuffleRecv(std::shared_ptr<Connection>& conn,
                   absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                   size_t repeat = 1);

  void BatchShuffleSend(std::shared_ptr<Connection>& conn, size_t total_num,
                        size_t per_size, size_t repeat,
                        const std::vector<std::vector<size_t>>& perms,
                        std::vector<std::vector<internal::PTy>>& vec_delta);

  void BatchShuffleRecv(std::shared_ptr<Connection>& conn, size_t total_num,
                        size_t per_size, size_t repeat,
                        std::vector<std::vector<internal::PTy>>& vec_a,
                        std::vector<std::vector<internal::PTy>>& vec_b);

  void SgrrShuffleSend(std::shared_ptr<Connection>& conn,
                       absl::Span<const size_t> perm,
                       absl::Span<internal::PTy> delta, size_t repeat = 1);

  void SgrrShuffleRecv(std::shared_ptr<Connection>& conn,
                       absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                       size_t repeat = 1);

  void SgrrBatchShuffleSend(std::shared_ptr<Connection>& conn, size_t total_num,
                            size_t per_size, size_t repeat,
                            const std::vector<std::vector<size_t>>& perms,
                            std::vector<std::vector<internal::PTy>>& vec_delta);

  void SgrrBatchShuffleRecv(std::shared_ptr<Connection>& conn, size_t total_num,
                            size_t per_size, size_t repeat,
                            std::vector<std::vector<internal::PTy>>& vec_a,
                            std::vector<std::vector<internal::PTy>>& vec_b);

  void ASTSend(std::shared_ptr<Connection>& conn, absl::Span<const size_t> perm,
               absl::Span<const internal::ATy> r, absl::Span<internal::ATy> a,
               absl::Span<internal::ATy> b);

  void ASTRecv(std::shared_ptr<Connection>& conn,
               absl::Span<const internal::ATy> r, absl::Span<internal::ATy> a,
               absl::Span<internal::ATy> b);

  void BatchASTSend(std::shared_ptr<Connection>& conn, size_t total_num,
                    size_t per_size, absl::Span<const internal::ATy> r,
                    const std::vector<std::vector<size_t>>& perms,
                    std::vector<std::vector<internal::ATy>>& lhs,
                    std::vector<std::vector<internal::ATy>>& rhs);

  void BatchASTRecv(std::shared_ptr<Connection>& conn, size_t total_num,
                    size_t per_size, absl::Span<const internal::ATy> r,
                    std::vector<std::vector<internal::ATy>>& lhs,
                    std::vector<std::vector<internal::ATy>>& rhs);

 private:
  std::shared_ptr<OtAdapter> ot_sender_{nullptr};
  std::shared_ptr<OtAdapter> ot_receiver_{nullptr};
};

}  // namespace mosac::ot
