#include "mosac/ss/public.h"

#include <vector>

#include "mosac/utils/field.h"
#include "mosac/utils/vec_op.h"

namespace mosac::internal {

#define Reg_Bi(name)                                                        \
  std::vector<PTy> name##PP([[maybe_unused]] std::shared_ptr<Context>& ctx, \
                            absl::Span<const PTy> lhs,                      \
                            absl::Span<const PTy> rhs) {                    \
    return op::name(absl::MakeSpan(lhs), absl::MakeSpan(rhs));              \
  }

Reg_Bi(Add);
Reg_Bi(Sub);
Reg_Bi(Mul);
Reg_Bi(Div);

#define REG_Si(name)                                                       \
  std::vector<PTy> name##P([[maybe_unused]] std::shared_ptr<Context>& ctx, \
                           absl::Span<const PTy> in) {                     \
    return op::name(absl::MakeSpan(in));                                   \
  }

REG_Si(Neg);
REG_Si(Inv);

#define REG_Num(name)                                                      \
  std::vector<PTy> name##P([[maybe_unused]] std::shared_ptr<Context>& ctx, \
                           size_t num) {                                   \
    return op::name(num);                                                  \
  }

REG_Num(Ones);
REG_Num(Zeros);

std::vector<PTy> RandP(std::shared_ptr<Context>& ctx, size_t num) {
  return op::Rand(*ctx->GetState<Prg>(), num);
}

#define REG_Bi_Cache(name)                                    \
  std::vector<PTy> name##PP_cache(                            \
      [[maybe_unused]] std::shared_ptr<Context>& ctx,         \
      absl::Span<const PTy> lhs, absl::Span<const PTy> rhs) { \
    const auto num = lhs.size();                              \
    YACL_ENFORCE(num == rhs.size());                          \
    return op::Zeros(num);                                    \
  }

REG_Bi_Cache(Add);
REG_Bi_Cache(Sub);
REG_Bi_Cache(Mul);
REG_Bi_Cache(Div);

#define REG_Si_Cache(name)                            \
  std::vector<PTy> name##P_cache(                     \
      [[maybe_unused]] std::shared_ptr<Context>& ctx, \
      absl::Span<const PTy> in) {                     \
    const auto num = in.size();                       \
    return op::Zeros(num);                            \
  }

REG_Si_Cache(Neg);
REG_Si_Cache(Inv);

#define REG_Num_Cache(name)                                         \
  std::vector<PTy> name##P_cache(                                   \
      [[maybe_unused]] std::shared_ptr<Context>& ctx, size_t num) { \
    return op::Zeros(num);                                          \
  }

REG_Num_Cache(Ones);
REG_Num_Cache(Zeros);
REG_Num_Cache(Rand);

std::vector<PTy> ScalarMulPP([[maybe_unused]] std::shared_ptr<Context>& ctx,
                             [[maybe_unused]] const PTy& scalar,
                             absl::Span<const PTy> in) {
  return op::ScalarMul(scalar, in);
}

std::vector<PTy> ScalarMulPP_cache(
    [[maybe_unused]] std::shared_ptr<Context>& ctx,
    [[maybe_unused]] const PTy& scalar, absl::Span<const PTy> in) {
  const size_t num = in.size();
  return op::Zeros(num);
}
}  // namespace mosac::internal
