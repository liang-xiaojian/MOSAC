#pragma once

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "mosac/context/context.h"
#include "yacl/link/test_util.h"

namespace mosac {

namespace yl = yacl::link;

// Create yacl::link::Context in Memory
inline std::vector<std::shared_ptr<yl::Context>> SetupWorld(
    const std::string& id, size_t world_size) {
  return yl::test::SetupWorld(id, world_size);
}

inline std::vector<std::shared_ptr<yl::Context>> SetupWorld(size_t world_size) {
  return yl::test::SetupWorld(world_size);
}

// Create yacl::link::Context in localhost
inline std::vector<std::shared_ptr<yl::Context>> SetupBrpcWorld(
    const std::string& id, size_t world_size) {
  yl::ContextDesc ctx_desc;
  // ctx_desc.id = id;
  for (size_t rank = 0; rank < world_size; rank++) {
    const auto party_id = fmt::format("{}-{}", id, rank);
    const auto host = fmt::format("127.0.0.1:{}", 11111 + rank);
    ctx_desc.parties.push_back({party_id, host});
  }
  ctx_desc.throttle_window_size = 0;
  ctx_desc.http_timeout_ms = 60 * 1000;  // 1 min

  std::vector<std::shared_ptr<yl::Context>> contexts(world_size);
  for (size_t rank = 0; rank < world_size; rank++) {
    contexts[rank] = yl::FactoryBrpc().CreateContext(ctx_desc, rank);
  }

  auto proc = [&](size_t rank) { contexts[rank]->ConnectToMesh(); };
  std::vector<std::future<void>> jobs(world_size);
  for (size_t rank = 0; rank < world_size; rank++) {
    jobs[rank] = std::async(proc, rank);
  }

  for (size_t rank = 0; rank < world_size; rank++) {
    jobs[rank].get();
  }
  return contexts;
}

inline std::vector<std::shared_ptr<yl::Context>> SetupBrpcWorld(
    size_t world_size) {
  auto id = fmt::format("world_{}", world_size);
  return SetupBrpcWorld(id, world_size);
}

// Mock mosac Context
std::vector<std::shared_ptr<Context>> MockContext(size_t world_size,
                                                  bool memory_mode = true) {
  std::vector<std::shared_ptr<yl::Context>> lctxs;
  if (memory_mode) {
    lctxs = SetupWorld(fmt::format("sim.{}", world_size), world_size);
  } else {
    lctxs = SetupBrpcWorld(fmt::format("sim.{}", world_size), world_size);
  }
  std::vector<std::shared_ptr<Context>> result;
  for (uint32_t rank = 0; rank < world_size; ++rank) {
    result.emplace_back(std::make_shared<Context>(lctxs[rank]));
  }
  return result;
}

std::shared_ptr<Context> MakeContext(const std::string& parties, size_t rank) {
  yl::ContextDesc lctx_desc;
  std::vector<std::string> hosts = absl::StrSplit(parties, ',');
  for (size_t rank = 0; rank < hosts.size(); rank++) {
    const auto id = fmt::format("party{}", rank);
    lctx_desc.parties.emplace_back(id, hosts[rank]);
  }
  auto lctx = yl::FactoryBrpc().CreateContext(lctx_desc, rank);
  lctx->ConnectToMesh();
  return std::make_shared<Context>(lctx);
}

};  // namespace mosac
