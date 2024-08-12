#pragma once

#include "mosac/context/state.h"
#include "mosac/cr/utils/ot_adapter.h"
#include "mosac/ss/type.h"

namespace mosac::shuffle {

void ShuffleSend(std::shared_ptr<Connection>& conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<const size_t> perm, absl::Span<internal::PTy> delta,
                 size_t repeat = 1);

void ShuffleRecv(std::shared_ptr<Connection> conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                 size_t repeat = 1);

void ASTSend(std::shared_ptr<Connection>& conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const size_t> perm, absl::Span<const internal::ATy> r,
             absl::Span<internal::ATy> lhs, absl::Span<internal::ATy> rhs);

void ASTRecv(std::shared_ptr<Connection> conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const internal::ATy> r, absl::Span<internal::ATy> lhs,
             absl::Span<internal::ATy> rhs);
}  // namespace mosac::shuffle
