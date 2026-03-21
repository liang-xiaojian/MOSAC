# Copyright 2026 Ant International, Ant Group Co., Ltd.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 
# Author (Xiaojian Liang)

_OPTS = [
    "-Werror",
    "-pedantic-errors",
    "-Weverything",
    "--system-header-prefix=gmp",
    "-fPIC",
]

_COPTS = _OPTS + ["-std=c17"]

_CXXOPTS = _OPTS + ["-std=c++17"]

cc_library(
    name = "gmp",
    srcs = ["libgmp.a"],
    hdrs = ["include/gmp.h"],
    copts = _COPTS,
    includes = ["include"],
    # Using an empty include_prefix causes Bazel to emit -I instead of -iquote
    # options for the include directory, so that #include <gmp.h> works.
    #include_prefix = "",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "gmpxx",
    srcs = ["libgmpxx.a"],
    hdrs = ["include/gmpxx.h"],
    copts = _CXXOPTS,
    includes = ["include"],
    # Using an empty include_prefix causes Bazel to emit -I instead of -iquote
    # options for the include directory, so that #include <gmpxx.h> works.
    #include_prefix = "",
    visibility = ["//visibility:public"],
)

genrule(
    name = "gmp-build",
    srcs = glob(
        ["**/*"],
        exclude = ["bazel-*"],
    ),
    outs = [
        "libgmp.a",
        "libgmpxx.a",
        "include/gmp.h",
        "include/gmpxx.h",
    ],
    cmd = """
        export CFLAGS="-fPIC"
        export CXXFLAGS="-fPIC"
        CONFIGURE_LOG=$$(mktemp)
        MAKE_LOG=$$(mktemp)
        GMP_ROOT=$$(dirname $(location configure))
        pushd $$GMP_ROOT > /dev/null
            mkdir -p /tmp/gmp
            if ! ./configure --prefix=/tmp/gmp --enable-cxx --disable-assembly > $$CONFIGURE_LOG; then
                cat $$CONFIGURE_LOG
            fi
            if ! make -j`nproc` > $$MAKE_LOG; then
                cat $$MAKE_LOG
            fi
            if ! make install > $$MAKE_LOG; then
                cat $$MAKE_LOG
            fi
        popd > /dev/null
        cp /tmp/gmp/lib/libgmp.a $(location libgmp.a)
        cp /tmp/gmp/lib/libgmpxx.a $(location libgmpxx.a)
        cp /tmp/gmp/include/gmp.h $(location include/gmp.h)
        cp /tmp/gmp/include/gmpxx.h $(location include/gmpxx.h)
    """,
)
