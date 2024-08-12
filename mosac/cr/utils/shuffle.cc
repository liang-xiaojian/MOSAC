#include "mosac/cr/utils/shuffle.h"

#include "yacl/base/dynamic_bitset.h"
#include "yacl/crypto/base/aes/aes_intrinsics.h"
#include "yacl/crypto/base/aes/aes_opt.h"
#include "yacl/crypto/primitives/ot/gywz_ote.h"
#include "yacl/crypto/utils/rand.h"
#include "yacl/math/gadget.h"

namespace mosac::shuffle {

namespace yc = yacl::crypto;
namespace ym = yacl::math;

// fixed key
namespace {
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

void ShuffleSend(std::shared_ptr<Connection>& conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<const size_t> perm, absl::Span<internal::PTy> delta,
                 size_t repeat) {
  YACL_ENFORCE(ot_ptr->IsSender() == false);
  const size_t batch_size = perm.size();
  const size_t full_size = delta.size();
  YACL_ENFORCE(batch_size * repeat == full_size);

  YACL_ENFORCE(repeat < kPrfKey.size());

  std::vector<internal::PTy> a(full_size, internal::PTy(0));
  std::vector<internal::PTy> b(full_size, internal::PTy(0));
  // gywz ote buff
  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> punctured_msgs(batch_size);
  // for consistency check
  std::vector<uint128_t> check_a(batch_size, 0);
  std::vector<uint128_t> check_b(batch_size, 0);

  const size_t ot_num = ym::Log2Ceil(batch_size);
  const size_t required_ot = batch_size * ot_num;
  std::vector<uint128_t> ot_buff(required_ot);
  yacl::dynamic_bitset<uint128_t> choices(required_ot);

  ot_ptr->recv_rcot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);
  for (size_t i = 0; i < batch_size; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    yc::GywzOtExtRecv(conn, ot_recv, batch_size, perm[i],
                      absl::MakeSpan(punctured_msgs));
    // break correlation
    auto extend = SeedExtend(absl::MakeSpan(punctured_msgs), repeat + 1);
    // set punctured point to be zero
    for (size_t _ = 0; _ < repeat; ++_) {
      extend[_ * batch_size + perm[i]] = 0;
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
      const size_t offset = _ * batch_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + batch_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }

  for (size_t _ = 0; _ < repeat; ++_) {
    const size_t offset = _ * batch_size;
    for (size_t i = 0; i < batch_size; ++i) {
      delta[offset + i] = a[offset + perm[i]] - b[offset + i];
    }
  }

  // ---- consistency check ----
  auto buf = conn->Recv(conn->NextRank(), "shuffle: consistency check");
  YACL_ENFORCE(static_cast<uint64_t>(buf.size()) ==
               batch_size * sizeof(uint128_t));
  auto tmp_span = absl::MakeSpan(buf.data<uint128_t>(), batch_size);
  std::transform(tmp_span.cbegin(), tmp_span.cend(), check_a.cbegin(),
                 check_a.begin(), std::bit_xor<uint128_t>());
  for (size_t i = 0; i < batch_size; ++i) {
    check_b[i] = check_b[i] ^ check_a[perm[i]];
  }
  auto hash_value = yacl::crypto::Sm3(yacl::ByteContainerView(
      check_b.data(), check_b.size() * sizeof(uint128_t)));
  auto remote_hash_value =
      conn->ExchangeWithCommit(yacl::ByteContainerView(hash_value));

  YACL_ENFORCE(yacl::ByteContainerView(hash_value) ==
               yacl::ByteContainerView(remote_hash_value));
  // ---- consistency check ----
}

void ShuffleRecv(std::shared_ptr<Connection> conn,
                 std::shared_ptr<ot::OtAdapter>& ot_ptr,
                 absl::Span<internal::PTy> a, absl::Span<internal::PTy> b,
                 size_t repeat) {
  YACL_ENFORCE(ot_ptr->IsSender() == true);
  const size_t full_size = a.size();
  const size_t batch_size = full_size / repeat;
  YACL_ENFORCE(full_size == b.size());
  YACL_ENFORCE(batch_size * repeat == full_size);

  YACL_ENFORCE(repeat < kPrfKey.size());

  internal::op::Zeros(a);

  std::vector<internal::PTy> opv(full_size);
  std::vector<uint128_t> all_msgs(batch_size);
  // for consistency check
  std::vector<uint128_t> check_a(batch_size, 0);
  std::vector<uint128_t> check_b(batch_size, 0);

  const size_t ot_num = ym::Log2Ceil(batch_size);
  const size_t required_ot = batch_size * ot_num;
  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->send_rcot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeCompactOtSendStore(ot_buff, ot_ptr->GetDelta());

  for (size_t i = 0; i < batch_size; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    yc::GywzOtExtSend(conn, ot_send, batch_size, absl::MakeSpan(all_msgs));
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
      const size_t offset = _ * batch_size;
      b[offset + i] =
          std::reduce(opv.begin() + offset, opv.begin() + offset + batch_size,
                      internal::PTy(0), std::plus<internal::PTy>());
    }
  }
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
}

void ASTSend(std::shared_ptr<Connection>& conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const size_t> perm, absl::Span<const internal::ATy> r,
             absl::Span<internal::ATy> lhs, absl::Span<internal::ATy> rhs) {
  YACL_ENFORCE(ot_ptr->IsSender() == false);

  const size_t num = perm.size();
  YACL_ENFORCE(r.size() == num);
  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);

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

  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;
  std::vector<uint128_t> ot_buff(required_ot);
  yacl::dynamic_bitset<uint128_t> choices(required_ot);

  ot_ptr->recv_rcot(absl::MakeSpan(ot_buff), choices);
  auto ot_store = yc::MakeOtRecvStore(choices, ot_buff);
  for (size_t i = 0; i < num; ++i) {
    auto ot_recv = ot_store.NextSlice(ot_num);
    yc::GywzOtExtRecv(conn, ot_recv, num, perm[i],
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

  // for (size_t _ = 0; _ < repeat; ++_) {
  //   const size_t offset = _ * num;
  //   for (size_t i = 0; i < num; ++i) {
  //     delta[offset + i] = a[offset + perm[i]] - b[offset + i];
  //   }
  // }

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

  // internal::op::AddInplace(r_sub_b, absl::MakeConstSpan(b));

  internal::Pack(absl::MakeConstSpan(r_sub_b).subspan(0, num),
                 absl::MakeConstSpan(r_sub_b).subspan(num, num), lhs);
  internal::Pack(absl::MakeConstSpan(a).subspan(0, num),
                 absl::MakeConstSpan(a).subspan(num, num), rhs);
}

void ASTRecv(std::shared_ptr<Connection> conn,
             std::shared_ptr<ot::OtAdapter>& ot_ptr,
             absl::Span<const internal::ATy> r, absl::Span<internal::ATy> lhs,
             absl::Span<internal::ATy> rhs) {
  YACL_ENFORCE(ot_ptr->IsSender() == true);

  const size_t num = r.size();
  YACL_ENFORCE(lhs.size() == num);
  YACL_ENFORCE(rhs.size() == num);

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

  const size_t ot_num = ym::Log2Ceil(num);
  const size_t required_ot = num * ot_num;
  std::vector<uint128_t> ot_buff(required_ot);
  ot_ptr->send_rcot(absl::MakeSpan(ot_buff));
  auto ot_store = yc::MakeCompactOtSendStore(ot_buff, ot_ptr->GetDelta());

  for (size_t i = 0; i < num; ++i) {
    auto ot_send = ot_store.NextSlice(ot_num);
    yc::GywzOtExtSend(conn, ot_send, num, absl::MakeSpan(all_msgs));
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
}

}  // namespace mosac::shuffle
