#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>

#include "yacl/crypto/base/hash/hash_utils.h"
#include "yacl/crypto/tools/prg.h"
#include "yacl/crypto/utils/rand.h"
#include "yacl/link/link.h"

namespace mosac {

// class State is just a kind of interface
class State {
 public:
  virtual ~State() = default;
};

class StateContainer final {
 private:
  std::map<std::string, std::shared_ptr<State>> map_;

 public:
  StateContainer() : map_() {}

  template <typename StateTy>
  void AddState(std::shared_ptr<StateTy> state) {
    map_.emplace(StateTy::id, state);
  }

  template <typename StateTy, typename... Args>
  void AddState(Args&&... args) {
    map_.emplace(StateTy::id,
                 std::make_shared<StateTy>(std::forward<Args>(args)...));
  }

  template <typename StateTy>
  std::shared_ptr<StateTy> GetState() {
    auto iter = map_.find(StateTy::id);
    YACL_ENFORCE(iter != map_.end(), "State id: {} NOT found !!!", StateTy::id);
    return std::dynamic_pointer_cast<StateTy>(iter->second);
  }
};

// Prg  (yacl::crypto::Prg)
class Prg : public State, public yacl::crypto::Prg<uint8_t> {
 public:
  static const std::string id;

  template <typename... Args>
  Prg(Args&&... args)
      : yacl::crypto::Prg<uint8_t>(std::forward<Args>(args)...) {}
};

// Connection (impl yacl::link::Context)
class Connection : public State, public yacl::link::Context {
 public:
  static const std::string id;

  template <typename... Args>
  Connection(Args&&... args)
      : yacl::link::Context(std::forward<Args>(args)...) {}

  uint128_t SyncSeed() {
    auto seed = yacl::crypto::SecureRandU128();
    return seed ^ ExchangeWithCommit(seed);
  }

  uint128_t Exchange(uint128_t val);

  uint64_t Exchange(uint64_t val);

  uint128_t ExchangeWithCommit(uint128_t val);

  uint64_t ExchangeWithCommit(uint64_t val);

  yacl::Buffer Exchange(yacl::ByteContainerView bv);

  yacl::Buffer ExchangeWithCommit(yacl::ByteContainerView bv);

 private:
  template <typename T>
  T _ExchangeWithCommit_T(T val);

  template <typename T>
  T _Exchange_T(T val);

  yacl::Buffer _Exchange_Buffer(yacl::ByteContainerView bv);

  yacl::Buffer _ExchangeWithCommit_Buffer(yacl::ByteContainerView bv);
};

}  // namespace mosac
