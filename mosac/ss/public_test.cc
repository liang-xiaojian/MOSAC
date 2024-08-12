#include <future>

#include "gtest/gtest.h"
#include "mosac/context/register.h"
#include "mosac/ss/protocol.h"
#include "mosac/utils/test_util.h"

namespace mosac {

namespace yc = yacl::crypto;

class TestParam {
 public:
  static std::vector<std::shared_ptr<Context>> ctx;

  // Getter
  static std::vector<std::shared_ptr<Context>>& GetContext() {
    if (ctx.empty()) {
      ctx = Setup();
    }
    return ctx;
  }

  static std::vector<std::shared_ptr<Context>> Setup() {
    auto ctx = MockContext(2);
    MockSetupContext(ctx);
    return ctx;
  }
};

std::vector<std::shared_ptr<Context>> TestParam::ctx =
    std::vector<std::shared_ptr<Context>>();

TEST(Setup, InitializeWork) {
  auto ctx = TestParam::GetContext();
  EXPECT_EQ(ctx.size(), 2);
}

#define CALCULATE(context, num, func)        \
  auto prot = context->GetState<Protocol>(); \
  auto lhs = prot->RandP(num);               \
  auto rhs = prot->RandP(num);               \
  return prot->func(lhs, rhs);

#define DECLARE_PP_TEST(func)                                           \
  TEST(ProtocolTest, func##Work) {                                      \
    size_t num = 10000;                                                 \
    auto context = TestParam::GetContext();                             \
    auto rank0 = std::async([&] { CALCULATE(context[0], num, func); }); \
    auto rank1 = std::async([&] { CALCULATE(context[1], num, func); }); \
    auto r_b = rank0.get();                                             \
    auto r_a = rank1.get();                                             \
    for (size_t i = 0; i < num; ++i) {                                  \
      EXPECT_EQ(r_a[i], r_b[i]);                                        \
    }                                                                   \
  };

DECLARE_PP_TEST(Add);
DECLARE_PP_TEST(Sub);
DECLARE_PP_TEST(Mul);
DECLARE_PP_TEST(Div);

};  // namespace mosac
