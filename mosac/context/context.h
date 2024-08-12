#pragma once

#include <memory>

#include "mosac/context/state.h"
#include "yacl/link/link.h"

namespace mosac {

class Context {
 public:
  std::unique_ptr<StateContainer> states_{nullptr};

 public:
  Context(std::shared_ptr<yacl::link::Context> lctx) : lctx_(lctx) {
    states_ = std::make_unique<StateContainer>();
    AddState<Connection>(*lctx_);
    rank_ = lctx_->Rank();
  }

  std::shared_ptr<Connection> GetConnection() { return GetState<Connection>(); }

  uint32_t NextRank() { return GetState<Connection>()->NextRank(); }

  uint32_t GetRank() { return GetState<Connection>()->Rank(); }

  template <typename StateTy>
  void AddState(std::shared_ptr<StateTy> state) {
    states_->template AddState<StateTy>(state);
  }

  template <typename StateTy, typename... Args>
  void AddState(Args&&... args) {
    states_->template AddState<StateTy>(std::forward<Args>(args)...);
  }

  template <typename StateTy>
  std::shared_ptr<StateTy> GetState() {
    return states_->template GetState<StateTy>();
  }

 private:
  std::shared_ptr<yacl::link::Context> lctx_{nullptr};
  uint32_t rank_;
};

};  // namespace mosac
