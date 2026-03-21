// Copyright 2026 Ant International, Ant Group Co., Ltd.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Author (Xiaojian Liang)


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
