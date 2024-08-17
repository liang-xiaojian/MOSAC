#include <future>
#include <unordered_set>

#include "mosac/context/register.h"
#include "mosac/ss/protocol.h"
#include "mosac/utils/test_util.h"

using namespace mosac;

auto OSS(const std::shared_ptr<yacl::link::Context> &lctx,
         absl::Span<PTy> set0) {
  auto rank = lctx->Rank();

  auto context = std::make_shared<Context>(lctx);
  SetupContext(context, true);
  auto prot = context->GetState<Protocol>();

  // cache
  if (rank == 0) {
    auto shares = prot->SetA(set0, true);
    auto shuffle = prot->SShuffleA(shares, true);
    auto plaintext = prot->A2P(shuffle, true);
  } else {
    auto shares = prot->GetA(set0.size(), true);
    auto shuffle = prot->SShuffleA(shares, true);
    auto plaintext = prot->A2P(shuffle, true);
  }
  context->GetState<Correlation>()->force_cache();

  std::vector<ATy> shares;
  if (rank == 0) {
    shares = prot->SetA(set0);
  } else {
    shares = prot->GetA(set0.size());
  }

  auto shuffle = prot->SShuffleA(shares);
  return prot->A2P(shuffle);
}

int main() {
  size_t num = 100;
  auto val = OP::Rand(num);

  auto lctxs = SetupWorld(2);
  SPDLOG_INFO("START");
  auto task0 = std::async([&] { return OSS(lctxs[0], absl::MakeSpan(val)); });
  auto task1 = std::async([&] { return OSS(lctxs[1], absl::MakeSpan(val)); });

  auto ret0 = task0.get();
  auto ret1 = task1.get();

  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;

  auto val_span =
      absl::MakeSpan(reinterpret_cast<INTEGER *>(val.data()), val.size());
  auto ret0_span =
      absl::MakeSpan(reinterpret_cast<INTEGER *>(ret0.data()), ret0.size());
  auto ret1_span =
      absl::MakeSpan(reinterpret_cast<INTEGER *>(ret1.data()), ret1.size());

  std::sort(val_span.begin(), val_span.end());
  std::sort(ret0_span.begin(), ret0_span.end());
  std::sort(ret1_span.begin(), ret1_span.end());

  for (size_t i = 0; i < num; ++i) {
    YACL_ENFORCE(val[i] == ret0[i]);
    YACL_ENFORCE(val[i] == ret1[i]);
  }
  SPDLOG_INFO("END");
  return 0;
}