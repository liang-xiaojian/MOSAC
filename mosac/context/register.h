#pragma once

#include "mosac/context/context.h"
#include "mosac/context/state.h"
#include "mosac/cr/cr.h"
#include "mosac/cr/fake_cr.h"
#include "mosac/cr/true_cr.h"
#include "mosac/ss/protocol.h"

namespace mosac {

void inline SetupContext(std::shared_ptr<Context> ctx, bool CR_mode = true) {
  // Generate a same seed
  uint128_t seed = ctx->GetState<Connection>()->SyncSeed();
  // Shared Prg, all parities own a same Prg (with same seed)
  ctx->AddState<Prg>(seed);
  // Create Basic Protocol
  ctx->AddState<Protocol>(ctx);
  // Get SPDZ key
  auto key = ctx->GetState<Protocol>()->GetKey();
  // Create Correlated Randomness Generator
  std::shared_ptr<Correlation> cr = nullptr;
  if (CR_mode) {
    SPDLOG_INFO("True CR");
    auto true_cr = std::make_shared<TrueCorrelation>(ctx);
    cr = std::static_pointer_cast<Correlation>(true_cr);
  } else {
    auto fake_cr = std::make_shared<FakeCorrelation>(ctx);
    cr = std::static_pointer_cast<Correlation>(fake_cr);
  }
  ctx->AddState<Correlation>(cr);
  // FIX ME: maybe?
  // 1. ctx->AddState<Correlation>(ctx,key);
  // 2. ctx->GetState<Correlation>()->OneTimeSetup();
  // Set SPDZ key
  ctx->GetState<Correlation>()->SetKey(key);
}

void inline MockSetupContext(std::vector<std::shared_ptr<Context>>& ctxs) {
  YACL_ENFORCE(ctxs.size() == 2);
  auto task0 = std::async([&] { SetupContext(ctxs[0]); });
  auto task1 = std::async([&] { SetupContext(ctxs[1]); });
  task0.get();
  task1.get();
}

}  // namespace mosac
