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

llvm::cl::opt<uint32_t> cl_num("num", llvm::cl::init(4),
                               llvm::cl::desc("Numbers for NMul"));

auto AShare(const std::shared_ptr<yacl::link::Context> &lctx, size_t num,
            bool CR_mode) {
  auto rank = lctx->Rank();

  SPDLOG_INFO("[P{}] && num {}, working mode: {} ", rank, num,
              (CR_mode ? std::string("Real Correlated Randomness")
                       : std::string("Fake Correlated Randomness")));

  auto context = std::make_shared<Context>(lctx);
  SetupContext(context, CR_mode /* fake CR model or real CR model */);
  auto prot = context->GetState<Protocol>();
  auto cr = context->GetState<Correlation>();
  auto r = internal::op::Rand(num);
  auto ret = std::vector<internal::ATy>(num);

  // offline
  TIMER_N_COMM_START(AShare_offline);
  {
    if (rank) {
      prot->SetA(r, true);
      prot->GetA(num, true);
    } else {
      prot->GetA(num, true);
      prot->SetA(r, true);
    }

    cr->force_cache();
  }
  TIMER_N_COMM_END_PRINT(AShare_offline);

  // online
  TIMER_N_COMM_START(AShare_online);

  if (rank) {
    auto tmp1 = prot->SetA(r);
    auto tmp2 = prot->GetA(num);
    ret = prot->Add(tmp1, tmp2);
  } else {
    auto tmp1 = prot->GetA(num);
    auto tmp2 = prot->SetA(r);
    ret = prot->Add(tmp1, tmp2);
  }
  TIMER_N_COMM_END_PRINT(AShare_online);

  return ret;
}

struct ArgPack {
  uint32_t num;
  uint32_t CR_mode;

  bool operator==(const ArgPack &other) const {
    return (num == other.num) && (CR_mode == other.CR_mode);
  }
  bool operator!=(const ArgPack &other) const { return !(*this == other); }
};

bool SyncTask(const std::shared_ptr<yacl::link::Context> &lctx, uint32_t num,
              uint32_t CR_mode) {
  ArgPack tmp = {num, CR_mode};
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
  uint32_t num = cl_num.getValue();
  bool CR_mode = cl_CR.getValue();

  YACL_ENFORCE(2 <= num);

  // lambda
  auto run_party = [&](uint32_t rank) {
    auto lctx = MakeLink(cl_parties.getValue(), rank);
    SyncTask(lctx, num, CR_mode);

    SPDLOG_INFO("PROTOCOL START");
    auto ret = AShare(lctx, num, CR_mode);
    SPDLOG_INFO("PROTOCOL END");
    return ret;
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