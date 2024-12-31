#include <chrono>
#include <future>
#include <random>
#include <unordered_set>

#include "llvm/Support/CommandLine.h"
#include "mosac/context/register.h"
#include "mosac/example/macro.h"
#include "mosac/ss/protocol.h"
#include "mosac/utils/test_util.h"

using namespace mosac;

// ---------- CL -----------

llvm::cl::opt<std::string> cl_parties(
    "parties", llvm::cl::init("127.0.0.1:39530,127.0.0.1:39531"),
    llvm::cl::desc("server list, format: host1:port1[,host2:port2, ...]"));

llvm::cl::opt<uint32_t> cl_alone(
    "alone", llvm::cl::init(0),
    llvm::cl::desc(
        "0 for running in two terminal, 1 for running in one terminal"));

llvm::cl::opt<uint32_t> cl_rank("rank", llvm::cl::init(0),
                                llvm::cl::desc("self rank (0 or 1)"));

llvm::cl::opt<uint32_t> cl_CR(
    "CR", llvm::cl::init(0),
    llvm::cl::desc("0 for prg-based correlated randomness, 1 for real "
                   "correlated randomness"));

llvm::cl::opt<uint32_t> cl_small("small_power", llvm::cl::init(3),
                                 llvm::cl::desc("T=2^small_power"));

llvm::cl::opt<uint32_t> cl_big("big_power", llvm::cl::init(4),
                               llvm::cl::desc("num=2^big_power"));
llvm::cl::opt<uint32_t> cl_cache(
    "cache", llvm::cl::init(1),
    llvm::cl::desc(
        "0 for no cache, 1 for cache (pre-compute offline randomness)"));

auto NDSS_shuffle2k(const std::shared_ptr<yacl::link::Context> &lctx, size_t T,
                    size_t num, bool CR_mode, bool cache) {
  auto rank = lctx->Rank();

  SPDLOG_INFO("[P{}] T {} && num {}, working mode: {} && {} ", rank, T, num,
              (CR_mode ? std::string("Real Correlated Randomness")
                       : std::string("Fake Correlated Randomness")),
              (cache ? std::string("Cache") : std::string("No Cache")));

  auto context = std::make_shared<Context>(lctx);
  SetupContext(context, CR_mode /* fake CR model or real CR model */);

  auto prot = context->GetState<Protocol>();

  if (cache) {
    auto shares = prot->RandA(num, true);
    auto shuffle = prot->ShuffleA_2k(T, shares, true);
    auto result = prot->A2P(shuffle, true);

    context->GetState<Correlation>()->force_cache();
  }

  auto shares = prot->RandA(num);
  TIMER_N_COMM_START(NDSS_shuffle_online);
  auto shuffle = prot->ShuffleA_2k(T, shares);
  YACL_ENFORCE(prot->NdssDelayCheck());
  TIMER_N_COMM_END_PRINT(NDSS_shuffle_online);

  auto result = prot->A2P(shuffle);
  auto plaintext = prot->A2P(shares);
  return std::make_pair(result, plaintext);
}

struct ArgPack {
  uint32_t T;
  uint32_t num;
  uint32_t CR_mode;
  uint32_t cache;

  bool operator==(const ArgPack &other) const {
    return (T == other.T) && (num == other.num) && (CR_mode == other.CR_mode) &&
           (cache == other.cache);
  }
  bool operator!=(const ArgPack &other) const { return !(*this == other); }
};

bool SyncTask(const std::shared_ptr<yacl::link::Context> &lctx, uint32_t T,
              uint32_t num, uint32_t CR_mode, uint32_t cache) {
  ArgPack tmp = {T, num, CR_mode, cache};
  auto bv = yacl::ByteContainerView(&tmp, sizeof(tmp));

  ArgPack remote;
  if (lctx->Rank()) {
    lctx->SendAsync(lctx->NextRank(), bv, "Sync0");
    auto buf = lctx->Recv(lctx->NextRank(), "Sync1");
    YACL_ENFORCE(buf.size() == sizeof(ArgPack));
    memcpy(&remote, buf.data(), buf.size());

  } else {
    auto buf = lctx->Recv(lctx->NextRank(), "Sync0");
    lctx->SendAsync(lctx->NextRank(), bv, "Sync1");
    YACL_ENFORCE(buf.size() == sizeof(ArgPack));
    memcpy(&remote, buf.data(), buf.size());
  }

  YACL_ENFORCE(remote == tmp);
  return true;
}

std::shared_ptr<yacl::link::Context> MakeLink(const std::string &parties,
                                              size_t rank) {
  yacl::link::ContextDesc lctx_desc;
  std::vector<std::string> hosts = absl::StrSplit(parties, ',');
  for (size_t rank = 0; rank < hosts.size(); rank++) {
    const auto id = fmt::format("party{}", rank);
    lctx_desc.parties.emplace_back(id, hosts[rank]);
  }
  lctx_desc.throttle_window_size = 0;
  auto lctx = yacl::link::FactoryBrpc().CreateContext(lctx_desc, rank);
  lctx->ConnectToMesh();
  return lctx;
}

int main(int argc, char **argv) {
  yacl::set_num_threads(1);  // force single thread per party

  // extract command line args
  llvm::cl::ParseCommandLineOptions(argc, argv);

  bool alone = cl_alone.getValue();
  uint32_t small_power = cl_small.getValue();
  uint32_t big_power = cl_big.getValue();
  bool CR_mode = cl_CR.getValue();
  bool cache = cl_cache.getValue();

  YACL_ENFORCE(0 < small_power && small_power <= big_power);
  uint32_t T = 1 << small_power;
  uint32_t num = 1 << big_power;

  // lambda
  auto run_party = [&](uint32_t rank) {
    auto lctx = MakeLink(cl_parties.getValue(), rank);
    SyncTask(lctx, T, num, CR_mode, cache);

    SPDLOG_INFO("PROTOCOL START");
    auto [shuffle, plaintext] = NDSS_shuffle2k(lctx, T, num, CR_mode, cache);
    SPDLOG_INFO("PROTOCOL END");

    typedef decltype(std::declval<internal::PTy>().GetVal()) INTEGER;

    auto shuffle_span = absl::MakeSpan(
        reinterpret_cast<INTEGER *>(shuffle.data()), shuffle.size());
    auto plaintext_span = absl::MakeSpan(
        reinterpret_cast<INTEGER *>(plaintext.data()), plaintext.size());

    std::sort(shuffle_span.begin(), shuffle_span.end());
    std::sort(plaintext_span.begin(), plaintext_span.end());

    for (size_t i = 0; i < num; ++i) {
      YACL_ENFORCE(shuffle_span[i] == plaintext_span[i]);
    }

    SPDLOG_INFO("Double Check Done");
  };

  if (alone == true) {
    auto task0 = std::async(run_party, 0);
    auto task1 = std::async(run_party, 1);

    task0.get();
    task1.get();
  } else {
    run_party(cl_rank.getValue());
  }
  return 0;
}
