#include "mosac/cr/utils/shuffle.h"

#include "mosac/cr/param.h"
#include "yacl/base/dynamic_bitset.h"
#include "yacl/crypto/base/aes/aes_intrinsics.h"
#include "yacl/crypto/base/aes/aes_opt.h"
#include "yacl/crypto/base/hash/hash_utils.h"
#include "yacl/crypto/primitives/ot/gywz_ote.h"
#include "yacl/crypto/primitives/ot/sgrr_ote.h"
#include "yacl/crypto/utils/rand.h"
#include "yacl/math/gadget.h"

namespace mosac::shuffle {

namespace yc = yacl::crypto;
namespace ym = yacl::math;

// fixed key
namespace {
// constexpr size_t param::kBatchAST = 2048;
// constexpr size_t kStep = 16;
const std::array<yc::AES_KEY, 12> kPrfKey = {
    yc::AES_set_encrypt_key(0),  yc::AES_set_encrypt_key(1),
    yc::AES_set_encrypt_key(2),  yc::AES_set_encrypt_key(3),
    yc::AES_set_encrypt_key(4),  yc::AES_set_encrypt_key(5),
    yc::AES_set_encrypt_key(6),  yc::AES_set_encrypt_key(7),
    yc::AES_set_encrypt_key(8),  yc::AES_set_encrypt_key(9),
    yc::AES_set_encrypt_key(10), yc::AES_set_encrypt_key(11)};

std::vector<uint128_t> SeedExtend(absl::Span<const uint128_t> seed,
                                  size_t repeat = 1) {
  const size_t seed_size = seed.size();
  std::vector<uint128_t> ret(seed_size * repeat);
  for (size_t i = 0; i < repeat; ++i) {
    std::transform(seed.cbegin(), seed.cend(), ret.data() + i * seed_size,
                   [](const uint128_t& val) { return val; });
  }

  switch (repeat) {
#define SWITCH_CASE(N)                                     \
  case N:                                                  \
    yc::ParaEnc<N>(ret.data(), kPrfKey.data(), seed_size); \
    break;
    SWITCH_CASE(12);
    SWITCH_CASE(11);
    SWITCH_CASE(10);
    SWITCH_CASE(9);
    SWITCH_CASE(8);
    SWITCH_CASE(7);
    SWITCH_CASE(6);
    SWITCH_CASE(5);
    SWITCH_CASE(4);
    SWITCH_CASE(3);
    SWITCH_CASE(2);
    SWITCH_CASE(1);
#undef SWITCH_CASE
    default:
      YACL_ENFORCE(false,
                   "SeedExtend Error, repeat should be in range (0,12].");
  }

  for (size_t i = 0; i < repeat; ++i) {
    std::transform(seed.cbegin(), seed.cend(), ret.data() + i * seed_size,
                   ret.data() + i * seed_size, std::bit_xor<uint128_t>());
  }
  return ret;
}
}  // namespace

// Shuffle implementation
std::pair<std::vector<uint128_t>, std::vector<uint128_t>> ShuffleSend_internal(
    std::shared_ptr<Connection>& conn, yc::OtRecvStore& ot_store,
    size_t per_size, size_t repeat, absl::Span<const size_t> perm,
    absl::Span<internal::PTy> delta) {
  const size_t full_size = per_size * repeat;
  YACL_ENFORCE(perm.size() == per_size);
  YACL_ENFORCE(delta.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    yc::GywzOtExtRecv_fixed_index(conn, ot_recv, per_size,
                                  absl::MakeSpan(punctured_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * per_size + perm[i]] = 0;
    }

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  for (size_t _ = 0; _ < repeat; ++_) {
    const size_t offset = _ * per_size;
    for (size_t i = 0; i < per_size; ++i) {
      delta[offset + i] = a[offset + perm[i]] - b[offset + i];
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>> ShuffleRecv_internal(
    std::shared_ptr<Connection>& conn, yc::OtSendStore& ot_store,
    size_t per_size, size_t repeat, absl::Span<internal::PTy> a,
    absl::Span<internal::PTy> b) {
  const size_t full_size = per_size * repeat;

  YACL_ENFORCE(a.size() == full_size);
  YACL_ENFORCE(b.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  internal::op::Zeros(a);

  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    yc::GywzOtExtSend_fixed_index(conn, ot_send, per_size,
                                  absl::MakeSpan(all_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(all_msgs), repeat + 1);

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::Sub(absl::MakeConstSpan(a), absl::MakeConstSpan(opv),
                      absl::MakeSpan(a));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>> _ShuffleSend_internal(
    yc::OtRecvStore& ot_store, size_t per_size, size_t repeat,
    absl::Span<const size_t> perm, absl::Span<internal::PTy> delta,
    absl::Span<uint128_t> recv_msgs) {
  const size_t full_size = per_size * repeat;
  YACL_ENFORCE(perm.size() == per_size);
  YACL_ENFORCE(delta.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);
  YACL_ENFORCE(ot_store.Size() == recv_msgs.size());

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    auto sub_msgs = recv_msgs.subspan(i * ot_num, ot_num);
    yc::GywzOtExtRecv_fixed_index(ot_recv, per_size,
                                  absl::MakeSpan(punctured_msgs), sub_msgs);
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * per_size + perm[i]] = 0;
    }

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  for (size_t _ = 0; _ < repeat; ++_) {
    const size_t offset = _ * per_size;
    for (size_t i = 0; i < per_size; ++i) {
      delta[offset + i] = a[offset + perm[i]] - b[offset + i];
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>> _ShuffleRecv_internal(
    yc::OtSendStore& ot_store, size_t per_size, size_t repeat,
    absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
    absl::Span<uint128_t> send_msgs) {
  const size_t full_size = per_size * repeat;

  YACL_ENFORCE(a.size() == full_size);
  YACL_ENFORCE(b.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  internal::op::Zeros(a);

  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);
  YACL_ENFORCE(ot_store.Size() == send_msgs.size());

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    auto sub_msgs = send_msgs.subspan(i * ot_num, ot_num);
    yc::GywzOtExtSend_fixed_index(ot_send, per_size, absl::MakeSpan(all_msgs),
                                  sub_msgs);
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(all_msgs), repeat + 1);

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::Sub(absl::MakeConstSpan(a), absl::MakeConstSpan(opv),
                      absl::MakeSpan(a));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

bool ShuffleSend_check(std::shared_ptr<Connection>& conn,
                       absl::Span<const size_t> perm,
                       std::vector<uint128_t>& check_a,
                       std::vector<uint128_t>& check_b) {
  const size_t num = check_a.size();
  // ---- consistency check ----
  auto buf = conn->Recv(conn->NextRank(), "shuffle: consistency check");
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) == num * sizeof(uint128_t));
  auto tmp_span = absl::MakeSpan(buf.data<uint128_t>(), num);
  std::transform(tmp_span.cbegin(), tmp_span.cend(), check_a.cbegin(),
                 check_a.begin(), std::bit_xor<uint128_t>());
  for (size_t i = 0; i < num; ++i) {
    check_b[i] = check_b[i] ^ check_a[perm[i]];
  }
  auto hash_value = yacl::crypto::Sm3(yacl::ByteContainerView(
      check_b.data(), check_b.size() * sizeof(uint128_t)));
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

bool ShuffleRecv_check(std::shared_ptr<Connection>& conn,
                       const std::vector<uint128_t>& check_a,
                       const std::vector<uint128_t>& check_b) {
  // ---- consistency check ----
  conn->SendAsync(conn->NextRank(),
                  yacl::ByteContainerView(check_a.data(),
                                          check_a.size() * sizeof(uint128_t)),
                  "shuffle: consistency check");

  auto hash_value = yacl::crypto::Sm3(yacl::ByteContainerView(
      check_b.data(), check_b.size() * sizeof(uint128_t)));
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

void ShuffleSend(std::shared_ptr<Connection>& conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<const size_t> perm, absl::Span<internal::PTy> delta,
                 size_t repeat) {
  YACL_ENFORCE(ot_ptr->IsSender() == false);
  const size_t per_size = perm.size();
  const size_t full_size = delta.size();
  YACL_ENFORCE(per_size * repeat == full_size);

  YACL_ENFORCE(repeat < kPrfKey.size());

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  yacl::dynamic_bitset<uint128_t> choices;
  for (size_t i = 0; i < per_size; ++i) {
    yacl::dynamic_bitset<uint128_t> tmp_choices;
    tmp_choices.append(perm[i]);
    tmp_choices.resize(ot_num);
    choices.append(tmp_choices);
  }

  YACL_ENFORCE(choices.size() == required_ot);

  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->recv_cot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);

  auto [check_a, check_b] =
      ShuffleSend_internal(conn, ot_store, per_size, repeat, perm, delta);

  // ---- consistency check ----
  YACL_ENFORCE(ShuffleSend_check(conn, perm, check_a, check_b));
  // ---- consistency check ----
}

void ShuffleRecv(std::shared_ptr<Connection>& conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                 size_t repeat) {
  YACL_ENFORCE(ot_ptr->IsSender() == true);
  const size_t full_size = a.size();
  const size_t per_size = full_size / repeat;
  YACL_ENFORCE(full_size == b.size());
  YACL_ENFORCE(per_size * repeat == full_size);
  YACL_ENFORCE(repeat < kPrfKey.size());

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;
  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->send_cot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeCompactOtSendStore(ot_buff, ot_ptr->GetDelta());

  auto [check_a, check_b] =
      ShuffleRecv_internal(conn, ot_store, per_size, repeat, a, b);

  // ---- consistency check ----
  YACL_ENFORCE(ShuffleRecv_check(conn, check_a, check_b));
  // ---- consistency check ----
}

bool BatchShuffleSend_check(std::shared_ptr<Connection>& conn,
                            const std::vector<std::vector<size_t>>& perm,
                            std::vector<std::vector<uint128_t>>& check_a,
                            std::vector<std::vector<uint128_t>>& check_b) {
  const size_t num = check_a.size();
  YACL_ENFORCE(num == check_b.size());
  const size_t size = check_a[0].size();

  // ---- consistency check ----
  auto buf = conn->Recv(conn->NextRank(), "shuffle: consistency check");
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) ==
               num * size * sizeof(uint128_t));

  auto ext_tmp_span = absl::MakeSpan(buf.data<uint128_t>(), num * size);
  auto hasher = yc::SslHash(yc::HashAlgorithm::SM3);

  for (size_t i = 0; i < num; ++i) {
    auto tmp_span = ext_tmp_span.subspan(i * size, size);
    std::transform(tmp_span.cbegin(), tmp_span.cend(), check_a[i].cbegin(),
                   check_a[i].begin(), std::bit_xor<uint128_t>());

    auto& _check_bi = check_b[i];
    auto& _check_ai = check_a[i];
    auto& _perm_i = perm[i];

    for (size_t j = 0; j < size; ++j) {
      _check_bi[j] = _check_bi[j] ^ _check_ai[_perm_i[j]];
    }

    hasher.Update(yacl::ByteContainerView(
        check_b[i].data(), check_b[i].size() * sizeof(uint128_t)));
  }

  auto hash_value = hasher.CumulativeHash();
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

bool BatchShuffleRecv_check(std::shared_ptr<Connection>& conn,
                            std::vector<std::vector<uint128_t>>& check_a,
                            std::vector<std::vector<uint128_t>>& check_b) {
  const auto num = check_a.size();
  YACL_ENFORCE(num == check_b.size());
  const auto size = check_a[0].size();

  std::vector<uint128_t> check_ext_a(num * size);
  auto hasher = yc::SslHash(yc::HashAlgorithm::SM3);

  for (size_t i = 0; i < num; ++i) {
    std::copy(check_a[i].begin(), check_a[i].end(),
              check_ext_a.begin() + i * size);
    hasher.Update(yacl::ByteContainerView(
        check_b[i].data(), check_b[i].size() * sizeof(uint128_t)));
  }

  // ---- consistency check ----
  conn->SendAsync(
      conn->NextRank(),
      yacl::ByteContainerView(check_ext_a.data(),
                              check_ext_a.size() * sizeof(uint128_t)),
      "shuffle: consistency check");

  auto hash_value = hasher.CumulativeHash();
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

void BatchShuffleSend(std::shared_ptr<Connection>& conn,
                      std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                      size_t per_size, size_t repeat,
                      const std::vector<std::vector<size_t>>& perms,
                      std::vector<std::vector<internal::PTy>>& vec_delta) {
  vec_delta.clear();

  YACL_ENFORCE(perms.size() == total_num);
  YACL_ENFORCE(perms[0].size() == per_size);

  yacl::dynamic_bitset<uint128_t> choices;

  const auto ot_num = ym::Log2Ceil(per_size);
  const auto required_ot = per_size * ot_num;
  for (size_t i = 0; i < total_num; ++i) {
    auto& perm = perms[i];
    for (size_t j = 0; j < per_size; ++j) {
      yacl::dynamic_bitset<uint128_t> tmp_choices;
      tmp_choices.append(perm[j]);
      tmp_choices.resize(ot_num);
      choices.append(tmp_choices);
    }
  }

  YACL_ENFORCE(choices.size() == total_num * required_ot);

  std::vector<uint128_t> ot_buff(required_ot * total_num);
  ot_ptr->recv_cot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);

  std::vector<std::vector<uint128_t>> check_vec_a;
  std::vector<std::vector<uint128_t>> check_vec_b;

  uint32_t delay_round = ym::DivCeil(param::kSgrrBatchShuffle, required_ot);
  // SPDLOG_INFO("magic delay round {}", delay_round);
  uint64_t delay_message_size = delay_round * required_ot * sizeof(uint128_t);

  yacl::Buffer recv_buf;
  absl::Span<uint128_t> recv_msgs;

  for (size_t i = 0; i < total_num; ++i) {
    auto& perm = perms[i];
    std::vector<internal::PTy> tmp_delta(per_size * repeat);
    auto ot_sub_store = ot_store.NextSlice(required_ot);

    if (i % delay_round == 0) {
      recv_buf = conn->Recv(conn->NextRank(), "BatchShuffle");
      YACL_ENFORCE((uint64_t)recv_buf.size() == (uint64_t)delay_message_size);
      recv_msgs =
          absl::MakeSpan(recv_buf.data<uint128_t>(), delay_round * required_ot);
    }

    auto cur_msgs =
        recv_msgs.subspan((i % delay_round) * required_ot, required_ot);

    auto [check_a, check_b] = _ShuffleSend_internal(
        ot_sub_store, per_size, repeat, perm, absl::MakeSpan(tmp_delta),
        absl::MakeSpan(cur_msgs));

    vec_delta.push_back(std::move(tmp_delta));
    check_vec_a.push_back(std::move(check_a));
    check_vec_b.push_back(std::move(check_b));
  }
  auto flag = BatchShuffleSend_check(conn, perms, check_vec_a, check_vec_b);
  YACL_ENFORCE(flag == true);
}

void BatchShuffleRecv(std::shared_ptr<Connection>& conn,
                      std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                      size_t per_size, size_t repeat,
                      std::vector<std::vector<internal::PTy>>& vec_a,
                      std::vector<std::vector<internal::PTy>>& vec_b) {
  SPDLOG_INFO("Enter Batch Shuffle Recv");
  vec_a.clear();
  vec_b.clear();

  const auto required_ot = per_size * ym::Log2Ceil(per_size);

  std::vector<uint128_t> ot_buff(required_ot * total_num);
  ot_ptr->send_cot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeCompactOtSendStore(ot_buff, ot_ptr->GetDelta());

  std::vector<std::vector<uint128_t>> check_vec_a;
  std::vector<std::vector<uint128_t>> check_vec_b;

  uint32_t delay_round = ym::DivCeil(param::kSgrrBatchShuffle, required_ot);
  // SPDLOG_INFO("magic delay round {}", delay_round);
  uint64_t delay_message_size = delay_round * required_ot * sizeof(uint128_t);

  std::vector<uint128_t> send_msgs(delay_round * required_ot, 0);

  for (size_t i = 0; i < total_num; ++i) {
    std::vector<internal::PTy> tmp_a(per_size * repeat);
    std::vector<internal::PTy> tmp_b(per_size * repeat);
    auto ot_sub_store = ot_store.NextSlice(required_ot);

    auto cur_msgs = absl::MakeSpan(send_msgs).subspan(
        (i % delay_round) * required_ot, required_ot);

    auto [check_a, check_b] = _ShuffleRecv_internal(
        ot_sub_store, per_size, repeat, absl::MakeSpan(tmp_a),
        absl::MakeSpan(tmp_b), absl::MakeSpan(cur_msgs));

    if ((i + 1) % delay_round == 0) {
      conn->SendAsync(
          conn->NextRank(),
          yacl::ByteContainerView(send_msgs.data(), delay_message_size),
          "BatchShuffle");
      send_msgs = std::vector<uint128_t>(delay_round * required_ot, 0);
    }

    vec_a.push_back(std::move(tmp_a));
    vec_b.push_back(std::move(tmp_b));
    check_vec_a.push_back(std::move(check_a));
    check_vec_b.push_back(std::move(check_b));
  }
  if (total_num % delay_round != 0) {
    conn->SendAsync(conn->NextRank(),
                    yacl::ByteContainerView(
                        send_msgs.data(), send_msgs.size() * sizeof(uint128_t)),
                    "BatchShuffle");
  }
  auto flag = BatchShuffleRecv_check(conn, check_vec_a, check_vec_b);
  YACL_ENFORCE(flag == true);
}

// Sgrr Shuffle implementation
std::pair<std::vector<uint128_t>, std::vector<uint128_t>>
SgrrShuffleSend_internal(std::shared_ptr<Connection>& conn,
                         yc::OtRecvStore& ot_store, size_t per_size,
                         size_t repeat, absl::Span<const size_t> perm,
                         absl::Span<internal::PTy> delta) {
  const size_t full_size = per_size * repeat;
  YACL_ENFORCE(perm.size() == per_size);
  YACL_ENFORCE(delta.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    yc::SgrrOtExtRecv_fixed_index(conn, ot_recv, per_size,
                                  absl::MakeSpan(punctured_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * per_size + perm[i]] = 0;
    }

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  for (size_t _ = 0; _ < repeat; ++_) {
    const size_t offset = _ * per_size;
    for (size_t i = 0; i < per_size; ++i) {
      delta[offset + i] = a[offset + perm[i]] - b[offset + i];
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>>
SgrrShuffleRecv_internal(std::shared_ptr<Connection>& conn,
                         yc::OtSendStore& ot_store, size_t per_size,
                         size_t repeat, absl::Span<internal::PTy> a,
                         absl::Span<internal::PTy> b) {
  const size_t full_size = per_size * repeat;

  YACL_ENFORCE(a.size() == full_size);
  YACL_ENFORCE(b.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  internal::op::Zeros(a);

  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    yc::SgrrOtExtSend_fixed_index(conn, ot_send, per_size,
                                  absl::MakeSpan(all_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(all_msgs), repeat + 1);

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::Sub(absl::MakeConstSpan(a), absl::MakeConstSpan(opv),
                      absl::MakeSpan(a));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>>
_SgrrShuffleSend_internal(yc::OtRecvStore& ot_store, size_t per_size,
                          size_t repeat, absl::Span<const size_t> perm,
                          absl::Span<internal::PTy> delta,
                          absl::Span<uint8_t> recv_msgs) {
  const size_t full_size = per_size * repeat;
  YACL_ENFORCE(perm.size() == per_size);
  YACL_ENFORCE(delta.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);
  const size_t sub_msg_size = yc::SgrrOtExtHelper(per_size);
  YACL_ENFORCE(sub_msg_size * per_size == recv_msgs.size());

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    auto sub_msgs = recv_msgs.subspan(i * sub_msg_size, sub_msg_size);
    yc::SgrrOtExtRecv_fixed_index(ot_recv, per_size,
                                  absl::MakeSpan(punctured_msgs), sub_msgs);
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * per_size + perm[i]] = 0;
    }

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  for (size_t _ = 0; _ < repeat; ++_) {
    const size_t offset = _ * per_size;
    for (size_t i = 0; i < per_size; ++i) {
      delta[offset + i] = a[offset + perm[i]] - b[offset + i];
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>>
_SgrrShuffleRecv_internal(yc::OtSendStore& ot_store, size_t per_size,
                          size_t repeat, absl::Span<internal::PTy> a,
                          absl::Span<internal::PTy> b,
                          absl::Span<uint8_t> send_msgs) {
  const size_t full_size = per_size * repeat;

  YACL_ENFORCE(a.size() == full_size);
  YACL_ENFORCE(b.size() == full_size);
  YACL_ENFORCE(repeat <= kPrfKey.size());

  internal::op::Zeros(a);

  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(per_size);
  // for consistency check
  std::vector<uint128_t> check_a(per_size, 0);
  std::vector<uint128_t> check_b(per_size, 0);

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  YACL_ENFORCE(ot_store.Size() == required_ot);
  const size_t sub_msg_size = yc::SgrrOtExtHelper(per_size);
  YACL_ENFORCE(sub_msg_size * per_size == send_msgs.size());

  for (size_t i = 0; i < per_size; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    auto sub_msgs = send_msgs.subspan(i * sub_msg_size, sub_msg_size);
    yc::SgrrOtExtSend_fixed_index(ot_send, per_size, absl::MakeSpan(all_msgs),
                                  sub_msgs);
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(all_msgs), repeat + 1);

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::Sub(absl::MakeConstSpan(a), absl::MakeConstSpan(opv),
                      absl::MakeSpan(a));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * per_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + per_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  return std::make_pair(std::move(check_a), std::move(check_b));
}

void SgrrShuffleSend(std::shared_ptr<Connection>& conn,
                     std::shared_ptr<ot::OtAdapter>& ot_ptr,
                     absl::Span<const size_t> perm,
                     absl::Span<internal::PTy> delta, size_t repeat) {
  YACL_ENFORCE(ot_ptr->IsSender() == false);
  const size_t per_size = perm.size();
  const size_t full_size = delta.size();
  YACL_ENFORCE(per_size * repeat == full_size);

  YACL_ENFORCE(repeat < kPrfKey.size());

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;

  yacl::dynamic_bitset<uint128_t> choices;
  for (size_t i = 0; i < per_size; ++i) {
    yacl::dynamic_bitset<uint128_t> tmp_choices;
    tmp_choices.append(perm[i]);
    tmp_choices.resize(ot_num);
    choices.append(tmp_choices);
  }

  YACL_ENFORCE(choices.size() == required_ot);

  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->recv_rot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);

  auto [check_a, check_b] =
      SgrrShuffleSend_internal(conn, ot_store, per_size, repeat, perm, delta);

  // ---- consistency check ----
  YACL_ENFORCE(ShuffleSend_check(conn, perm, check_a, check_b));
  // ---- consistency check ----
}

void SgrrShuffleRecv(std::shared_ptr<Connection>& conn,
                     std::shared_ptr<ot::OtAdapter>& ot_ptr,
                     absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                     size_t repeat) {
  YACL_ENFORCE(ot_ptr->IsSender() == true);
  const size_t full_size = a.size();
  const size_t per_size = full_size / repeat;
  YACL_ENFORCE(full_size == b.size());
  YACL_ENFORCE(per_size * repeat == full_size);
  YACL_ENFORCE(repeat < kPrfKey.size());

  const size_t ot_num = ym::Log2Ceil(per_size);
  const size_t required_ot = per_size * ot_num;
  std::vector<std::array<uint128_t, 2>> ot_buff(required_ot);
  ot_ptr->send_rot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeOtSendStore(ot_buff);

  auto [check_a, check_b] =
      SgrrShuffleRecv_internal(conn, ot_store, per_size, repeat, a, b);

  // ---- consistency check ----
  YACL_ENFORCE(ShuffleRecv_check(conn, check_a, check_b));
  // ---- consistency check ----
}

void SgrrBatchShuffleSend(std::shared_ptr<Connection>& conn,
                          std::shared_ptr<ot::OtAdapter>& ot_ptr,
                          size_t total_num, size_t per_size, size_t repeat,
                          const std::vector<std::vector<size_t>>& perms,
                          std::vector<std::vector<internal::PTy>>& vec_delta) {
  SPDLOG_INFO("Enter Batch SgrrShuffle Send (num : {}, per size {})", total_num,
              per_size);

  vec_delta.clear();

  YACL_ENFORCE(perms.size() == total_num);
  YACL_ENFORCE(perms[0].size() == per_size);

  yacl::dynamic_bitset<uint128_t> choices;

  const auto ot_num = ym::Log2Ceil(per_size);
  const auto required_ot = per_size * ot_num;
  for (size_t i = 0; i < total_num; ++i) {
    auto& perm = perms[i];
    for (size_t j = 0; j < per_size; ++j) {
      yacl::dynamic_bitset<uint128_t> tmp_choices;
      tmp_choices.append(perm[j]);
      tmp_choices.resize(ot_num);
      choices.append(tmp_choices);
    }
  }

  YACL_ENFORCE(choices.size() == total_num * required_ot);

  std::vector<uint128_t> ot_buff(required_ot * total_num, 0);
  ot_ptr->recv_rot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);

  std::vector<std::vector<uint128_t>> check_vec_a;
  std::vector<std::vector<uint128_t>> check_vec_b;

  uint32_t delay_round = ym::DivCeil(param::kBatchShuffle, required_ot);
  const size_t required_size = per_size * yc::SgrrOtExtHelper(per_size);
  // SPDLOG_INFO("magic delay round {}", delay_round);
  uint64_t delay_message_size = delay_round * required_size;

  yacl::Buffer recv_buf;
  absl::Span<uint8_t> recv_msgs;

  for (size_t i = 0; i < total_num; ++i) {
    auto& perm = perms[i];
    std::vector<internal::PTy> tmp_delta(per_size * repeat);
    auto ot_sub_store = ot_store.NextSlice(required_ot);

    if (i % delay_round == 0) {
      recv_buf = conn->Recv(conn->NextRank(), "BatchShuffle");
      YACL_ENFORCE((uint64_t)recv_buf.size() == (uint64_t)delay_message_size);
      recv_msgs =
          absl::MakeSpan(recv_buf.data<uint8_t>(), delay_round * required_size);
    }

    auto cur_msgs =
        recv_msgs.subspan((i % delay_round) * required_size, required_size);

    auto [check_a, check_b] = _SgrrShuffleSend_internal(
        ot_sub_store, per_size, repeat, perm, absl::MakeSpan(tmp_delta),
        absl::MakeSpan(cur_msgs));

    vec_delta.push_back(std::move(tmp_delta));
    check_vec_a.push_back(std::move(check_a));
    check_vec_b.push_back(std::move(check_b));
  }
  auto flag = BatchShuffleSend_check(conn, perms, check_vec_a, check_vec_b);
  YACL_ENFORCE(flag == true);
}

void SgrrBatchShuffleRecv(std::shared_ptr<Connection>& conn,
                          std::shared_ptr<ot::OtAdapter>& ot_ptr,
                          size_t total_num, size_t per_size, size_t repeat,
                          std::vector<std::vector<internal::PTy>>& vec_a,
                          std::vector<std::vector<internal::PTy>>& vec_b) {
  SPDLOG_INFO("Enter Batch SgrrShuffle Recv (num {}, per size {})", total_num,
              per_size);
  vec_a.clear();
  vec_b.clear();
  const auto ot_num = ym::Log2Ceil(per_size);
  const auto required_ot = per_size * ot_num;

  std::vector<std::array<uint128_t, 2>> ot_buff(required_ot * total_num);
  ot_ptr->send_rot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeOtSendStore(ot_buff);

  std::vector<std::vector<uint128_t>> check_vec_a;
  std::vector<std::vector<uint128_t>> check_vec_b;

  uint32_t delay_round = ym::DivCeil(param::kBatchShuffle, required_ot);
  const size_t required_size = per_size * yc::SgrrOtExtHelper(per_size);
  // SPDLOG_INFO("magic delay round {}", delay_round);
  uint64_t delay_message_size = delay_round * required_size;

  std::vector<uint8_t> send_msgs(delay_round * required_size, 0);

  for (size_t i = 0; i < total_num; ++i) {
    std::vector<internal::PTy> tmp_a(per_size * repeat);
    std::vector<internal::PTy> tmp_b(per_size * repeat);
    auto ot_sub_store = ot_store.NextSlice(required_ot);

    auto cur_msgs = absl::MakeSpan(send_msgs).subspan(
        (i % delay_round) * required_size, required_size);

    auto [check_a, check_b] = _SgrrShuffleRecv_internal(
        ot_sub_store, per_size, repeat, absl::MakeSpan(tmp_a),
        absl::MakeSpan(tmp_b), absl::MakeSpan(cur_msgs));

    if ((i + 1) % delay_round == 0) {
      conn->SendAsync(
          conn->NextRank(),
          yacl::ByteContainerView(send_msgs.data(), delay_message_size),
          "BatchShuffle");
      send_msgs = std::vector<uint8_t>(delay_round * required_size, 0);
    }

    vec_a.push_back(std::move(tmp_a));
    vec_b.push_back(std::move(tmp_b));
    check_vec_a.push_back(std::move(check_a));
    check_vec_b.push_back(std::move(check_b));
  }
  if (total_num % delay_round != 0) {
    conn->SendAsync(
        conn->NextRank(),
        yacl::ByteContainerView(send_msgs.data(), delay_message_size),
        "BatchShuffle");
  }
  auto flag = BatchShuffleRecv_check(conn, check_vec_a, check_vec_b);
  YACL_ENFORCE(flag == true);
}
// AST implementation
std::pair<std::vector<uint128_t>, std::vector<uint128_t>> ASTSend_internal(
    std::shared_ptr<Connection>& conn, yc::OtRecvStore& ot_store,
    absl::Span<const size_t> perm, absl::Span<const internal::ATy> r,
    absl::Span<internal::ATy> lhs, absl::Span<internal::ATy> rhs) {
  const size_t num = perm.size();
  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;

  YACL_ENFORCE(r.size() == num);
  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);
  YACL_ENFORCE(ot_store.Size() == required_ot);

  const size_t repeat = 2;
  const size_t full_size = num * repeat;

  YACL_ENFORCE(repeat < kPrfKey.size());

  auto [r_val, r_mac] = internal::Unpack(r);

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(num);
  // for consistency check
  std::vector<uint128_t> check_a(num, 0);
  std::vector<uint128_t> check_b(num, 0);

  for (size_t i = 0; i < num; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    yc::GywzOtExtRecv_fixed_index(conn, ot_recv, num,
                                  absl::MakeSpan(punctured_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * num + perm[i]] = 0;
    }

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * num;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + num,
                      internal::PTy(0), internal::PTy::Add);
    }
  }

  auto recv_buff = conn->Recv(conn->NextRank(), "shuffle: r_sub_b");
  YACL_ENFORCE(recv_buff.size() ==
               static_cast<int>(full_size * sizeof(internal::PTy)));
  auto r_sub_b = absl::MakeSpan(recv_buff.data<internal::PTy>(), full_size);

  internal::op::AddInplace(r_sub_b.subspan(0, num), absl::MakeConstSpan(r_val));
  internal::op::AddInplace(r_sub_b.subspan(num, num),
                           absl::MakeConstSpan(r_mac));

  for (size_t i = 0; i < num; ++i) {
    a[perm[i]] = r_sub_b[i] + b[i] - a[perm[i]];
    a[perm[i] + num] = r_sub_b[i + num] + b[i + num] - a[perm[i] + num];
  }

  internal::Pack(absl::MakeConstSpan(r_sub_b).subspan(0, num),
                 absl::MakeConstSpan(r_sub_b).subspan(num, num), lhs);
  internal::Pack(absl::MakeConstSpan(a).subspan(0, num),
                 absl::MakeConstSpan(a).subspan(num, num), rhs);

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>> _ASTSend_internal(
    yc::OtRecvStore& ot_store, absl::Span<const size_t> perm,
    absl::Span<const internal::ATy> r, absl::Span<internal::ATy> lhs,
    absl::Span<internal::ATy> rhs, absl::Span<uint128_t> recv_msgs,
    absl::Span<internal::PTy> recorret_msgs) {
  const size_t num = perm.size();
  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;
  const size_t repeat = 2;
  const size_t full_size = num * repeat;

  YACL_ENFORCE(r.size() == num);
  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);
  YACL_ENFORCE(ot_store.Size() == required_ot);
  YACL_ENFORCE(recv_msgs.size() == required_ot);
  YACL_ENFORCE(recorret_msgs.size() == num * repeat);

  YACL_ENFORCE(repeat < kPrfKey.size());

  auto [r_val, r_mac] = internal::Unpack(r);

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(num);
  // for consistency check
  std::vector<uint128_t> check_a(num, 0);
  std::vector<uint128_t> check_b(num, 0);

  for (size_t i = 0; i < num; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    auto sub_msgs = recv_msgs.subspan(i * ot_num, ot_num);
    yc::GywzOtExtRecv_fixed_index(ot_recv, num, absl::MakeSpan(punctured_msgs),
                                  sub_msgs);
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * num + perm[i]] = 0;
    }

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * num;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + num,
                      internal::PTy(0), internal::PTy::Add);
    }
  }

  // auto recv_buff = conn->Recv(conn->NextRank(), "shuffle: r_sub_b");
  // YACL_ENFORCE(recv_buff.size() ==
  //              static_cast<int>(full_size * sizeof(internal::PTy)));
  // auto r_sub_b = absl::MakeSpan(recv_buff.data<internal::PTy>(), full_size);
  auto r_sub_b = recorret_msgs;

  internal::op::AddInplace(r_sub_b.subspan(0, num), absl::MakeConstSpan(r_val));
  internal::op::AddInplace(r_sub_b.subspan(num, num),
                           absl::MakeConstSpan(r_mac));

  for (size_t i = 0; i < num; ++i) {
    a[perm[i]] = r_sub_b[i] + b[i] - a[perm[i]];
    a[perm[i] + num] = r_sub_b[i + num] + b[i + num] - a[perm[i] + num];
  }

  internal::Pack(absl::MakeConstSpan(r_sub_b).subspan(0, num),
                 absl::MakeConstSpan(r_sub_b).subspan(num, num), lhs);
  internal::Pack(absl::MakeConstSpan(a).subspan(0, num),
                 absl::MakeConstSpan(a).subspan(num, num), rhs);

  return std::make_pair(std::move(check_a), std::move(check_b));
}

bool ASTSend_check(std::shared_ptr<Connection>& conn,
                   absl::Span<const size_t> perm,
                   std::vector<uint128_t>& check_a,
                   std::vector<uint128_t>& check_b) {
  const size_t num = check_a.size();
  // ---- consistency check ----
  auto buf = conn->Recv(conn->NextRank(), "shuffle: consistency check");
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) == num * sizeof(uint128_t));
  auto tmp_span = absl::MakeSpan(buf.data<uint128_t>(), num);
  std::transform(tmp_span.cbegin(), tmp_span.cend(), check_a.cbegin(),
                 check_a.begin(), std::bit_xor<uint128_t>());
  for (size_t i = 0; i < num; ++i) {
    check_b[i] = check_b[i] ^ check_a[perm[i]];
  }
  auto hash_value = yacl::crypto::Sm3(yacl::ByteContainerView(
      check_b.data(), check_b.size() * sizeof(uint128_t)));
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

bool BatchASTSend_check(std::shared_ptr<Connection>& conn,
                        const std::vector<std::vector<size_t>>& perm,
                        std::vector<std::vector<uint128_t>>& check_a,
                        std::vector<std::vector<uint128_t>>& check_b) {
  const size_t num = check_a.size();
  YACL_ENFORCE(num == check_b.size());
  const size_t size = check_a[0].size();

  // ---- consistency check ----
  auto buf = conn->Recv(conn->NextRank(), "shuffle: consistency check");
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) ==
               num * size * sizeof(uint128_t));

  auto ext_tmp_span = absl::MakeSpan(buf.data<uint128_t>(), num * size);
  auto hasher = yc::SslHash(yc::HashAlgorithm::SM3);

  for (size_t i = 0; i < num; ++i) {
    auto tmp_span = ext_tmp_span.subspan(i * size, size);
    std::transform(tmp_span.cbegin(), tmp_span.cend(), check_a[i].cbegin(),
                   check_a[i].begin(), std::bit_xor<uint128_t>());

    auto& _check_bi = check_b[i];
    auto& _check_ai = check_a[i];
    auto& _perm_i = perm[i];

    for (size_t j = 0; j < size; ++j) {
      _check_bi[j] = _check_bi[j] ^ _check_ai[_perm_i[j]];
    }

    hasher.Update(yacl::ByteContainerView(
        check_b[i].data(), check_b[i].size() * sizeof(uint128_t)));
  }

  auto hash_value = hasher.CumulativeHash();
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

void ASTSend(std::shared_ptr<Connection>& conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const size_t> perm, absl::Span<const internal::ATy> r,
             absl::Span<internal::ATy> lhs, absl::Span<internal::ATy> rhs) {
  YACL_ENFORCE(ot_ptr->IsSender() == false);

  const size_t num = perm.size();
  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;

  YACL_ENFORCE(r.size() == num);
  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);

  yacl::dynamic_bitset<uint128_t> choices;
  for (size_t i = 0; i < num; ++i) {
    yacl::dynamic_bitset<uint128_t> tmp_choices;
    tmp_choices.append(perm[i]);
    tmp_choices.resize(ot_num);
    choices.append(tmp_choices);
  }

  YACL_ENFORCE(choices.size() == required_ot);

  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->recv_cot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);

  auto [check_a, check_b] = ASTSend_internal(conn, ot_store, perm, r, lhs, rhs);
  // auto flag = ASTSend_check(conn, perm, check_a, check_b);
  std::vector<std::vector<uint128_t>> check_aa(1, check_a);
  std::vector<std::vector<uint128_t>> check_bb(1, check_b);
  std::vector<std::vector<size_t>> permm(1, std::vector<size_t>(num, 0));
  std::copy(perm.begin(), perm.end(), permm[0].begin());
  auto flag = BatchASTSend_check(conn, permm, check_aa, check_bb);
  YACL_ENFORCE(flag == true);
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>> ASTRecv_internal(
    std::shared_ptr<Connection> conn, yc::OtSendStore& ot_store,
    absl::Span<const internal::ATy> r, absl::Span<internal::ATy> lhs,
    absl::Span<internal::ATy> rhs) {
  const size_t num = r.size();
  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;

  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);
  YACL_ENFORCE(ot_store.Size() == required_ot);

  const size_t repeat = 2;
  const size_t full_size = num * repeat;

  YACL_ENFORCE(repeat < kPrfKey.size());

  auto [r_val, r_mac] = internal::Unpack(r);

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(num);
  // for consistency check
  std::vector<uint128_t> check_a(num, 0);
  std::vector<uint128_t> check_b(num, 0);

  std::vector<uint128_t> ot_buff(required_ot);

  for (size_t i = 0; i < num; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    yc::GywzOtExtSend_fixed_index(conn, ot_send, num, absl::MakeSpan(all_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(all_msgs), repeat + 1);

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * num;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + num,
                      internal::PTy(0), internal::PTy::Add);
    }
  }

  std::vector<internal::PTy> r_sub_b(full_size, internal::PTy(0));

  internal::op::Sub(absl::MakeConstSpan(r_val),
                    absl::MakeConstSpan(b).subspan(0, num),
                    absl::MakeSpan(r_sub_b).subspan(0, num));
  internal::op::Sub(absl::MakeConstSpan(r_mac),
                    absl::MakeConstSpan(b).subspan(num, num),
                    absl::MakeSpan(r_sub_b).subspan(num, num));
  conn->SendAsync(conn->NextRank(),
                  yacl::ByteContainerView(
                      r_sub_b.data(), r_sub_b.size() * sizeof(internal::PTy)),
                  "shuffle: r_sub_b");

  internal::Pack(absl::MakeConstSpan(b).subspan(0, num),
                 absl::MakeConstSpan(b).subspan(num, num), absl::MakeSpan(lhs));
  internal::Pack(absl::MakeConstSpan(a).subspan(0, num),
                 absl::MakeConstSpan(a).subspan(num, num), absl::MakeSpan(rhs));

  return std::make_pair(std::move(check_a), std::move(check_b));
}

std::pair<std::vector<uint128_t>, std::vector<uint128_t>> _ASTRecv_internal(
    yc::OtSendStore& ot_store, absl::Span<const internal::ATy> r,
    absl::Span<internal::ATy> lhs, absl::Span<internal::ATy> rhs,
    absl::Span<uint128_t> send_msgs, absl::Span<internal::PTy> recorret_msgs) {
  const size_t num = r.size();
  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;
  const size_t repeat = 2;
  const size_t full_size = num * repeat;

  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);
  YACL_ENFORCE(ot_store.Size() == required_ot);
  YACL_ENFORCE(send_msgs.size() == required_ot);
  YACL_ENFORCE(recorret_msgs.size() == num * repeat);

  YACL_ENFORCE(repeat < kPrfKey.size());

  auto [r_val, r_mac] = internal::Unpack(r);

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(num);
  // for consistency check
  std::vector<uint128_t> check_a(num, 0);
  std::vector<uint128_t> check_b(num, 0);

  std::vector<uint128_t> ot_buff(required_ot);

  for (size_t i = 0; i < num; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    auto sub_msgs = send_msgs.subspan(i * ot_num, ot_num);
    yc::GywzOtExtSend_fixed_index(ot_send, num, absl::MakeSpan(all_msgs),
                                  sub_msgs);
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(all_msgs), repeat + 1);

    std::transform(extend.cbegin(), extend.cbegin() + full_size, opv.begin(),
                   [](const uint128_t& val) { return internal::PTy(val); });

    // ---- consistency check ----
    std::transform(extend.cbegin() + full_size, extend.cend(), check_a.begin(),
                   check_a.begin(), std::bit_xor<uint128_t>());
    check_b[i] = std::reduce(extend.cbegin() + full_size, extend.cend(),
                             uint128_t(0), std::bit_xor<uint128_t>());
    // ---- consistency check ----

    internal::op::AddInplace(absl::MakeSpan(a), absl::MakeConstSpan(opv));
    for (size_t _ = 0; _ < repeat; ++_) {
      const size_t offset = _ * num;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + num,
                      internal::PTy(0), internal::PTy::Add);
    }
  }

  // std::vector<internal::PTy> r_sub_b(full_size, internal::PTy(0));
  auto& r_sub_b = recorret_msgs;
  std::fill(r_sub_b.begin(), r_sub_b.end(), internal::PTy(0));

  internal::op::Sub(absl::MakeConstSpan(r_val),
                    absl::MakeConstSpan(b).subspan(0, num),
                    absl::MakeSpan(r_sub_b).subspan(0, num));
  internal::op::Sub(absl::MakeConstSpan(r_mac),
                    absl::MakeConstSpan(b).subspan(num, num),
                    absl::MakeSpan(r_sub_b).subspan(num, num));
  // conn->SendAsync(conn->NextRank(),
  //                 yacl::ByteContainerView(
  //                     r_sub_b.data(), r_sub_b.size() *
  //                     sizeof(internal::PTy)),
  //                 "shuffle: r_sub_b");

  internal::Pack(absl::MakeConstSpan(b).subspan(0, num),
                 absl::MakeConstSpan(b).subspan(num, num), absl::MakeSpan(lhs));
  internal::Pack(absl::MakeConstSpan(a).subspan(0, num),
                 absl::MakeConstSpan(a).subspan(num, num), absl::MakeSpan(rhs));

  return std::make_pair(std::move(check_a), std::move(check_b));
}

bool BatchASTRecv_check(std::shared_ptr<Connection>& conn,
                        std::vector<std::vector<uint128_t>>& check_a,
                        std::vector<std::vector<uint128_t>>& check_b) {
  const auto num = check_a.size();
  YACL_ENFORCE(num == check_b.size());
  const auto size = check_a[0].size();

  std::vector<uint128_t> check_ext_a(num * size);
  auto hasher = yc::SslHash(yc::HashAlgorithm::SM3);

  for (size_t i = 0; i < num; ++i) {
    std::copy(check_a[i].begin(), check_a[i].end(),
              check_ext_a.begin() + i * size);
    hasher.Update(yacl::ByteContainerView(
        check_b[i].data(), check_b[i].size() * sizeof(uint128_t)));
  }

  // ---- consistency check ----
  conn->SendAsync(
      conn->NextRank(),
      yacl::ByteContainerView(check_ext_a.data(),
                              check_ext_a.size() * sizeof(uint128_t)),
      "shuffle: consistency check");

  auto hash_value = hasher.CumulativeHash();
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

bool ASTRecv_check(std::shared_ptr<Connection>& conn,
                   const std::vector<uint128_t>& check_a,
                   const std::vector<uint128_t>& check_b) {
  // ---- consistency check ----
  conn->SendAsync(conn->NextRank(),
                  yacl::ByteContainerView(check_a.data(),
                                          check_a.size() * sizeof(uint128_t)),
                  "shuffle: consistency check");

  auto hash_value = yacl::crypto::Sm3(yacl::ByteContainerView(
      check_b.data(), check_b.size() * sizeof(uint128_t)));
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
  return true;
}

void ASTRecv(std::shared_ptr<Connection>& conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const internal::ATy> r, absl::Span<internal::ATy> lhs,
             absl::Span<internal::ATy> rhs) {
  YACL_ENFORCE(ot_ptr->IsSender() == true);

  const size_t num = r.size();
  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;

  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);

  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->send_cot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeCompactOtSendStore(ot_buff, ot_ptr->GetDelta());

  auto [check_a, check_b] = ASTRecv_internal(conn, ot_store, r, lhs, rhs);
  std::vector<std::vector<uint128_t>> check_aa(1, check_a);
  std::vector<std::vector<uint128_t>> check_bb(1, check_b);
  auto flag = BatchASTRecv_check(conn, check_aa, check_bb);
  YACL_ENFORCE(flag == true);
}

void BatchASTSend(std::shared_ptr<Connection>& conn,
                  std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                  size_t per_size, absl::Span<const internal::ATy> r,
                  const std::vector<std::vector<size_t>>& perms,
                  std::vector<std::vector<internal::ATy>>& lhs,
                  std::vector<std::vector<internal::ATy>>& rhs) {
  SPDLOG_INFO("Enter Batch AST Send");
  lhs.clear();
  rhs.clear();

  YACL_ENFORCE(perms.size() == total_num);
  YACL_ENFORCE(perms[0].size() == per_size);
  YACL_ENFORCE(r.size() == total_num * per_size);

  yacl::dynamic_bitset<uint128_t> choices;

  const auto ot_num = ym::Log2Ceil(per_size);
  const auto required_ot = per_size * ot_num;
  for (size_t i = 0; i < total_num; ++i) {
    auto& perm = perms[i];
    for (size_t j = 0; j < per_size; ++j) {
      yacl::dynamic_bitset<uint128_t> tmp_choices;
      tmp_choices.append(perm[j]);
      tmp_choices.resize(ot_num);
      choices.append(tmp_choices);
    }
  }

  YACL_ENFORCE(choices.size() == total_num * required_ot);

  std::vector<uint128_t> ot_buff(required_ot * total_num);
  ot_ptr->recv_cot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);

  yacl::Buffer recv_buf;
  std::vector<std::vector<uint128_t>> check_vec_a;
  std::vector<std::vector<uint128_t>> check_vec_b;

  uint32_t delay_round = ym::DivCeil(param::kBatchAST, required_ot);
  SPDLOG_INFO("magic delay round {}", delay_round);
  uint64_t delay_message_size =
      delay_round *
      (required_ot * sizeof(uint128_t) + sizeof(internal::PTy) * per_size * 2);
  absl::Span<uint128_t> delay_recv_msgs;
  absl::Span<internal::PTy> delay_recorrect_msgs;

  for (size_t i = 0; i < total_num; ++i) {
    auto& perm = perms[i];
    std::vector<internal::ATy> tmp_lhs(per_size);
    std::vector<internal::ATy> tmp_rhs(per_size);
    auto sub_r = r.subspan(i * per_size, per_size);
    auto ot_sub_store = ot_store.NextSlice(required_ot);

    if (i % delay_round == 0) {
      recv_buf = conn->Recv(conn->NextRank(), "BatchAST");
      YACL_ENFORCE((uint64_t)recv_buf.size() == (uint64_t)delay_message_size);
      delay_recv_msgs =
          absl::MakeSpan(recv_buf.data<uint128_t>(), delay_round * required_ot);
      delay_recorrect_msgs = absl::MakeSpan(
          reinterpret_cast<internal::PTy*>(recv_buf.data<uint128_t>() +
                                           delay_round * required_ot),
          delay_round * per_size * 2);
    }

    // auto buf_msgs = conn->Recv(conn->NextRank(), "BatchAST");
    // YACL_ENFORCE((uint64_t)buf_msgs.size() ==
    //              (sizeof(uint128_t) * required_ot +
    //               sizeof(internal::PTy) * per_size * 2));

    auto recv_msgs =
        delay_recv_msgs.subspan((i % delay_round) * required_ot, required_ot);
    auto recorret_msgs = delay_recorrect_msgs.subspan(
        (i % delay_round) * (per_size * 2), per_size * 2);

    // auto recv_msgs = absl::MakeSpan(
    //     reinterpret_cast<uint128_t*>(buf_msgs.data()), required_ot);
    // auto recorret_msgs = absl::MakeSpan(
    //     reinterpret_cast<internal::PTy*>(buf_msgs.data<uint8_t>() +
    //                                      sizeof(uint128_t) * required_ot),
    //     per_size * 2);

    auto [check_a, check_b] =
        _ASTSend_internal(ot_sub_store, perm, sub_r, absl::MakeSpan(tmp_lhs),
                          absl::MakeSpan(tmp_rhs), recv_msgs, recorret_msgs);

    lhs.push_back(std::move(tmp_lhs));
    rhs.push_back(std::move(tmp_rhs));
    check_vec_a.push_back(std::move(check_a));
    check_vec_b.push_back(std::move(check_b));
  }
  auto flag = BatchASTSend_check(conn, perms, check_vec_a, check_vec_b);
  YACL_ENFORCE(flag == true);
}

void BatchASTRecv(std::shared_ptr<Connection> conn,
                  std::shared_ptr<ot::OtAdapter>& ot_ptr, size_t total_num,
                  size_t per_size, absl::Span<const internal::ATy> r,
                  std::vector<std::vector<internal::ATy>>& lhs,
                  std::vector<std::vector<internal::ATy>>& rhs) {
  SPDLOG_INFO("Enter Batch AST Recv");
  lhs.clear();
  rhs.clear();

  YACL_ENFORCE(r.size() == total_num * per_size);

  const auto required_ot = per_size * ym::Log2Ceil(per_size);

  std::vector<uint128_t> ot_buff(required_ot * total_num);
  ot_ptr->send_cot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeCompactOtSendStore(ot_buff, ot_ptr->GetDelta());

  std::vector<std::vector<uint128_t>> check_vec_a;
  std::vector<std::vector<uint128_t>> check_vec_b;

  yacl::Buffer buf_msgs(sizeof(uint128_t) * required_ot +
                        sizeof(internal::PTy) * per_size * 2);

  uint32_t delay_round = ym::DivCeil(param::kBatchAST, required_ot);
  // SPDLOG_INFO("magic delay round {}", delay_round);
  uint64_t delay_message_size =
      delay_round *
      (required_ot * sizeof(uint128_t) + per_size * 2 * sizeof(internal::PTy));

  yacl::Buffer delay_msgs(delay_message_size);
  memset(delay_msgs.data<uint8_t>(), 0, delay_message_size);

  auto delay_send_msgs =
      absl::MakeSpan(delay_msgs.data<uint128_t>(), delay_round * required_ot);
  auto delay_recorrect_msgs = absl::MakeSpan(
      reinterpret_cast<internal::PTy*>(delay_msgs.data<uint128_t>() +
                                       delay_round * required_ot),
      delay_round * per_size * 2);

  for (size_t i = 0; i < total_num; ++i) {
    std::vector<internal::ATy> tmp_lhs(per_size);
    std::vector<internal::ATy> tmp_rhs(per_size);
    auto ot_sub_store = ot_store.NextSlice(required_ot);
    auto sub_r = r.subspan(i * per_size, per_size);

    auto send_msgs =
        delay_send_msgs.subspan((i % delay_round) * required_ot, required_ot);
    auto recorret_msgs = delay_recorrect_msgs.subspan(
        (i % delay_round) * per_size * 2, per_size * 2);

    auto [check_a, check_b] =
        _ASTRecv_internal(ot_sub_store, sub_r, absl::MakeSpan(tmp_lhs),
                          absl::MakeSpan(tmp_rhs), send_msgs, recorret_msgs);

    // conn->SendAsync(conn->NextRank(), buf_msgs, "BatchAST");
    if ((i + 1) % delay_round == 0) {
      conn->SendAsync(conn->NextRank(), yacl::ByteContainerView(delay_msgs),
                      "BatchAST");
      memset(delay_msgs.data<uint8_t>(), 0, delay_message_size);
    }

    lhs.push_back(std::move(tmp_lhs));
    rhs.push_back(std::move(tmp_rhs));
    check_vec_a.push_back(std::move(check_a));
    check_vec_b.push_back(std::move(check_b));
  }

  if (total_num % delay_round != 0) {
    conn->SendAsync(conn->NextRank(), yacl::ByteContainerView(delay_msgs),
                    "BatchAST");
  }

  auto flag = BatchASTRecv_check(conn, check_vec_a, check_vec_b);
  YACL_ENFORCE(flag == true);
}

}  // namespace mosac::shuffle
