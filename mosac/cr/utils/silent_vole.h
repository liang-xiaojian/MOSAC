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

#include "mosac/context/state.h"
#include "mosac/cr/utils/code_interface.h"
#include "mosac/cr/utils/ea_code.h"
#include "mosac/cr/utils/ot_adapter.h"
#include "mosac/cr/utils/ot_helper.h"
#include "yacl/base/exception.h"
#include "yacl/base/int128.h"
#include "yacl/link/context.h"

namespace mosac::vole {

// Silent Vector OLE Implementation
//
// Silent VOLE is a "framework" to generate Vector OLE correlation. First of
// all, it would generate a special correlation c' = a' * delta + b', where a'
// is a random t-weight vector. Then, apply dual-LPN to make vectors a', b' and
// c' to be "Uniformly Random", satisfying c = a * delta + b. For more details,
// see https://eprint.iacr.org/2019/1159.pdf, figure 3.
//
//   +-------+      +-----------+     +-----------+      +----------+
//   |  COT  |  =>  | Base-VOLE |  => |  Mp-VOLE  |  =>  |   VOLE   |
//   +-------+      +-----------+     +-----------+      +----------+
//    num = m*         num = t           num = m         num = n (>256)
//   len = kappa      len = kappa       len = kappa       len = kappa
//
//  > kappa: computation security parameter (128 for example)
//
// Security assumptions:
//  > OT extension functionality, for more details about its implementation, see
//  `yacl/kernel/algorithms/softspoken_ote.h`
//  > base VOLE and multi-point VOLE functionalities, for more details about its
//  implementation, see `yacl/kernel/algorithms/mp_vole.h`
//  > Dual LPN problem, for more details, please see the original papers
//    1) Silver (https://eprint.iacr.org/2021/1150.pdf) Most
//    efficiency, but not recommended to use due to its security flaw.
//    2) Expand Accumulate Code (https://eprint.iacr.org/2022/1014.pdf)
//    3) Expand Convolute Code (https://eprint.iacr.org/2023/882.pdf)
//
// Note that:
// > Silent Vole Receiver would get vector a and vector b; Silent Vole Sender
// would get delta and vector c, such that c = a * delta + b
// > Silent Vole aims to generate large amount of VOLE correlation, thus the
// length of a,b,c should be greater than 256 at least.
// > When small amount of VOLE correlation is needed (less than 256), use
// `GilboaVoleSend/GilboaVoleRecv` instead.

// dual-LPN code type
enum class CodeType { ExAcc7, ExAcc11, ExAcc21, ExAcc40 };

class SilentVoleSender {
 public:
  explicit SilentVoleSender(CodeType code,
                            std::shared_ptr<ot::OtAdapter> ot_ptr,
                            internal::PTy delta) {
    codetype_ = code;
    ot_ptr_ = ot_ptr;
    delta_ = delta;
  }

  void OneTimeSetup(std::shared_ptr<Connection>& conn);

  // c = a * delta + b
  void Send(std::shared_ptr<Connection>& conn, absl::Span<internal::PTy> c);

  internal::PTy GetDelta() const { return delta_; }

  CodeType GetCodeType() const { return codetype_; }

 private:
  bool is_inited_{false};
  CodeType codetype_;
  internal::PTy delta_{0};
  std::shared_ptr<ot::OtAdapter> ot_ptr_{nullptr};

  std::vector<internal::PTy> pre_c_;
};

class SilentVoleReceiver {
 public:
  explicit SilentVoleReceiver(CodeType code,
                              std::shared_ptr<ot::OtAdapter> ot_ptr) {
    codetype_ = code;
    ot_ptr_ = ot_ptr;
  }

  void OneTimeSetup(std::shared_ptr<Connection>& conn);

  // c = a * delta + b
  void Recv(std::shared_ptr<Connection>& conn, absl::Span<internal::PTy> a,
            absl::Span<internal::PTy> b);

  CodeType GetCodeType() const { return codetype_; }

 private:
  bool is_inited_{false};
  CodeType codetype_;
  std::shared_ptr<ot::OtAdapter> ot_ptr_{nullptr};

  std::vector<internal::PTy> pre_a_;
  std::vector<internal::PTy> pre_b_;
};

}  // namespace mosac::vole
