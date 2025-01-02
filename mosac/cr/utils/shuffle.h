#pragma once

#include "mosac/context/state.h"
#include "mosac/cr/utils/ot_adapter.h"
#include "mosac/ss/type.h"

namespace mosac::shuffle {

// ------ gywz ote based shuffle ----------
void ShuffleSend(std::shared_ptr<Connection>& conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<const size_t> perm, absl::Span<internal::PTy> delta,
                 size_t repeat = 1);

void ShuffleRecv(std::shared_ptr<Connection>& conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                 size_t repeat = 1);

void BatchShuffleSend(std::shared_ptr<Connection>& conn,
                      std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                      size_t per_size, size_t repeat,
                      const std::vector<std::vector<size_t>>& perms,
                      std::vector<std::vector<internal::PTy>>& vec_delta);

void BatchShuffleRecv(std::shared_ptr<Connection>& conn,
                      std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                      size_t per_size, size_t repeat,
                      std::vector<std::vector<internal::PTy>>& vec_a,
                      std::vector<std::vector<internal::PTy>>& vec_b);

// ------ sgrr ote based shuffle ----------
void SgrrShuffleSend(std::shared_ptr<Connection>& conn,
                     std::shared_ptr<ot::OtAdapter>& ot_ptr,
                     absl::Span<const size_t> perm,
                     absl::Span<internal::PTy> delta, size_t repeat = 1);

void SgrrShuffleRecv(std::shared_ptr<Connection>& conn,
                     std::shared_ptr<ot::OtAdapter>& ot_ptr,
                     absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                     size_t repeat = 1);

void SgrrBatchShuffleSend(std::shared_ptr<Connection>& conn,
                          std::shared_ptr<ot::OtAdapter>& ot_ptr,
                          size_t total_num, size_t per_size, size_t repeat,
                          const std::vector<std::vector<size_t>>& perms,
                          std::vector<std::vector<internal::PTy>>& vec_delta);

void SgrrBatchShuffleRecv(std::shared_ptr<Connection>& conn,
                          std::shared_ptr<ot::OtAdapter>& ot_ptr,
                          size_t total_num, size_t per_size, size_t repeat,
                          std::vector<std::vector<internal::PTy>>& vec_a,
                          std::vector<std::vector<internal::PTy>>& vec_b);

// ------ gywz ote baesd AST ----------
void ASTSend(std::shared_ptr<Connection>& conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const size_t> perm, absl::Span<const internal::ATy> r,
             absl::Span<internal::ATy> lhs, absl::Span<internal::ATy> rhs);

void ASTRecv(std::shared_ptr<Connection>& conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const internal::ATy> r, absl::Span<internal::ATy> lhs,
             absl::Span<internal::ATy> rhs);

void BatchASTSend(std::shared_ptr<Connection>& conn,
                  std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                  size_t per_size, absl::Span<const internal::ATy> r,
                  const std::vector<std::vector<size_t>>& perms,
                  std::vector<std::vector<internal::ATy>>& lhs,
                  std::vector<std::vector<internal::ATy>>& rhs);

void BatchASTRecv(std::shared_ptr<Connection> conn,
                  std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                  size_t per_size, absl::Span<const internal::ATy> r,
                  std::vector<std::vector<internal::ATy>>& lhs,
                  std::vector<std::vector<internal::ATy>>& rhs);

}  // namespace mosac::shuffle
