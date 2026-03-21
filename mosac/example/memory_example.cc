// Copyright 2026 Ant International, Ant Group Co., Ltd.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Author (Xiaojian Liang)


#include <future>
#include <unordered_set>

#include "mosac/context/register.h"
#include "mosac/example/macro.h"
#include "mosac/ss/protocol.h"
#include "mosac/utils/test_util.h"

using namespace mosac;

auto OSS(const std::shared_ptr<yacl::link::Context>& lctx, size_t num) {
  auto rank = lctx->Rank();

  auto context = std::make_shared<Context>(lctx);
  SetupContext(context, false /* fake CR model or real CR model */);
  auto prot = context->GetState<Protocol>();

  // cache offline randomness
  {
    auto shares = prot->RandA(num, true);
    auto shuffle = prot->SShuffleA(shares, true);
    auto result = prot->A2P(shuffle, true);

    context->GetState<Correlation>()->force_cache();
    context->GetState<Correlation>()->cache_print();
  }

  // scheme with online
  auto shares = prot->RandA(num);

  TIMER_N_COMM_START(OSS_online);
  auto shuffle = prot->SShuffleA(shares);
  YACL_ENFORCE(prot->DelayCheck());
  TIMER_N_COMM_END_PRINT(OSS_online);

  auto result = prot->A2P(shuffle);
  auto plaintext = prot->A2P(shares);

  return std::make_pair(result, plaintext);
}

int main() {
  size_t num = 1 << 20;

  auto lctxs = SetupWorld(2);
  SPDLOG_INFO("PROTOCOL START");
  auto task0 = std::async([&] { return OSS(lctxs[0], num); });
  auto task1 = std::async([&] { return OSS(lctxs[1], num); });

  auto [shuffle0, plaintext0] = task0.get();
  auto [shuffle1, plaintext1] = task1.get();
  SPDLOG_INFO("PROTOCOL END");

  for (size_t i = 0; i < num; ++i) {
    YACL_ENFORCE(shuffle0[i] == shuffle1[i]);
    YACL_ENFORCE(plaintext0[i] == plaintext1[i]);
  }

  typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;

  auto shuffle_span = absl::MakeSpan(
      reinterpret_cast<INTEGER*>(shuffle0.data()), shuffle0.size());
  auto plaintext_span = absl::MakeSpan(
      reinterpret_cast<INTEGER*>(plaintext0.data()), plaintext0.size());

  std::sort(shuffle_span.begin(), shuffle_span.end());
  std::sort(plaintext_span.begin(), plaintext_span.end());

  for (size_t i = 0; i < num; ++i) {
    YACL_ENFORCE(shuffle_span[i] == plaintext_span[i]);
  }

  SPDLOG_INFO("Double Check Done");
  return 0;
}