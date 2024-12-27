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

auto AST2k(const std::shared_ptr<yacl::link::Context> &lctx, size_t T,
           size_t num, bool CR_mode) {
  auto rank = lctx->Rank();

  SPDLOG_INFO("[P{}] T {} && num {}, working mode: {} ", rank, T, num,
              (CR_mode ? std::string("Real Correlated Randomness")
                       : std::string("Fake Correlated Randomness")));

  auto context = std::make_shared<Context>(lctx);
  SetupContext(context, CR_mode /* fake CR model or real CR model */);

  auto cr = context->GetState<Correlation>();

  TIMER_N_COMM_START(AST2k_2_side);
  if (rank == 0) {
    TIMER_N_COMM_START(_AST2k);
    auto [perm, a, b] = cr->ASTSet_2k(T, num);
    auto [remote_a, remote_b] = cr->ASTGet_2k(T, num);
    TIMER_N_COMM_END_PRINT(_AST2k);

    TIMER_N_COMM_START(_NMUL);
    cr->NMul(absl::MakeSpan(a));
    cr->NMul(absl::MakeSpan(b));
    cr->NMul(absl::MakeSpan(remote_a));
    cr->NMul(absl::MakeSpan(remote_b));
    TIMER_N_COMM_END_PRINT(_NMUL);

    TIMER_N_COMM_START(_Beaver);
    cr->BeaverTriple(num * 2 + 2);
    TIMER_N_COMM_END_PRINT(_Beaver);
  } else {
    TIMER_N_COMM_START(_AST2k);
    auto [remote_a, remote_b] = cr->ASTGet_2k(T, num);
    auto [perm, a, b] = cr->ASTSet_2k(T, num);
    TIMER_N_COMM_END_PRINT(_AST2k);

    TIMER_N_COMM_START(_NMUL);
    cr->NMul(absl::MakeSpan(remote_a));
    cr->NMul(absl::MakeSpan(remote_b));
    cr->NMul(absl::MakeSpan(a));
    cr->NMul(absl::MakeSpan(b));
    TIMER_N_COMM_END_PRINT(_NMUL);

    TIMER_N_COMM_START(_Beaver);
    cr->BeaverTriple(num * 2 + 2);
    TIMER_N_COMM_END_PRINT(_Beaver);
  }
  cr->DelayCheck();
  TIMER_N_COMM_END_PRINT(AST2k_2_side);

  return true;
}

struct ArgPack {
  uint32_t T;
  uint32_t num;
  uint32_t CR_mode;

  bool operator==(const ArgPack &other) const {
    return (T == other.T) && (num == other.num) && (CR_mode == other.CR_mode);
  }
  bool operator!=(const ArgPack &other) const { return !(*this == other); }
};

bool SyncTask(const std::shared_ptr<yacl::link::Context> &lctx, uint32_t T,
              uint32_t num, uint32_t CR_mode) {
  ArgPack tmp = {T, num, CR_mode};
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
  lctx_desc.http_timeout_ms = 120 * 1000;  // 1 min
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

  YACL_ENFORCE(0 < small_power && small_power <= big_power);
  uint32_t T = 1 << small_power;
  uint32_t num = 1 << big_power;

  // lambda
  auto run_party = [&](uint32_t rank) {
    auto lctx = MakeLink(cl_parties.getValue(), rank);
    SyncTask(lctx, T, num, CR_mode);

    SPDLOG_INFO("PROTOCOL START");
    AST2k(lctx, T, num, CR_mode);
    SPDLOG_INFO("PROTOCOL END");
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