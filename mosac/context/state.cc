#include "mosac/context/state.h"

namespace mosac {
// register string
const std::string Prg::id = std::string("Prg");
// register string
const std::string Connection::id = std::string("Connection");

yacl::Buffer Connection::_Exchange_Buffer(yacl::ByteContainerView bv) {
  yacl::Buffer ret;
  if (rank_ == 0) {
    SendAsync(NextRank(), bv, "Send:0");
    ret = Recv(NextRank(), "Send:1");
  } else {
    ret = Recv(NextRank(), "Send:0");
    SendAsync(NextRank(), bv, "Send:1");
  }
  return ret;
}

yacl::Buffer Connection::_ExchangeWithCommit_Buffer(
    yacl::ByteContainerView bv) {
  yacl::Buffer buff(bv.size() + sizeof(uint128_t));
  memcpy(buff.data(), bv.data(), bv.size());
  *reinterpret_cast<uint128_t*>((uint8_t*)buff.data() + bv.size()) =
      yacl::crypto::SecureRandU128();

  auto buff_bv = yacl::ByteContainerView(buff);

  // commitment = hash( val || randomness )
  auto commitment = yacl::crypto::Sm3(buff);
  auto commitment_bv = yacl::ByteContainerView(commitment);

  auto remote_commitment = _Exchange_Buffer(commitment_bv);
  auto remote_buff = _Exchange_Buffer(buff_bv);

  auto check_commitment = yacl::crypto::Sm3(remote_buff);
  auto check_bv = yacl::ByteContainerView(check_commitment);

  YACL_ENFORCE(yacl::ByteContainerView(remote_commitment) == check_bv);

  remote_buff.resize(bv.size());
  return remote_buff;
}

template <typename T>
T Connection::_Exchange_T(T val) {
  auto ret_buf = _Exchange_Buffer(yacl::ByteContainerView(&val, sizeof(val)));
  T ret = *reinterpret_cast<T*>(ret_buf.data());
  return ret;
}

template <typename T>
T Connection::_ExchangeWithCommit_T(T val) {
  auto ret_buf =
      _ExchangeWithCommit_Buffer(yacl::ByteContainerView(&val, sizeof(val)));
  T ret = *reinterpret_cast<T*>(ret_buf.data());
  return ret;
}

#define ExchangeWithCommitImpl(T) \
  T Connection::ExchangeWithCommit(T val) { return _ExchangeWithCommit_T(val); }

#define ExchangeImpl(T) \
  T Connection::Exchange(T val) { return _Exchange_T(val); }

ExchangeImpl(uint128_t);
ExchangeImpl(uint64_t);

yacl::Buffer Connection::Exchange(yacl::ByteContainerView bv) {
  return _Exchange_Buffer(bv);
}

ExchangeWithCommitImpl(uint128_t);
ExchangeWithCommitImpl(uint64_t);

yacl::Buffer Connection::ExchangeWithCommit(yacl::ByteContainerView bv) {
  return _ExchangeWithCommit_Buffer(bv);
}

}  // namespace mosac
