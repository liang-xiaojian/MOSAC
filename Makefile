#!/usr/bin/env bash
# Copyright 2026 Ant International
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

default: test

fetch:
	bazel sync --repository_cache="./thirdparty"

release:
	bazel build -c opt --distdir=./thirdparty //...

debug:
	bazel build --distdir=./thirdparty //...

test_all: test example

test:
	bazel test -c opt --distdir=./thirdparty //...

example: offline_shuffle offline_shuffle_opt online_shuffle offline_AST2k online_AST2k offline_DoubleAST2k offline_opt_DoubleAST2k online_DoubleAST2k

offline_shuffle:
	bazel run -c opt --distdir=./thirdparty //mosac/example:NDSS_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1 --opt=0

offline_shuffle_opt:
	bazel run -c opt --distdir=./thirdparty //mosac/example:NDSS_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1 --opt=1

online_shuffle:
	bazel run -c opt --distdir=./thirdparty //mosac/example:NDSS_online_example -- --alone=1 --small_power=4 --big_power=12 --CR=0 --cache=1

offline_AST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:AST2k_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1

offline_DoubleAST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:DoubleAST2k_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1

offline_opt_DoubleAST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:opt_DoubleAST2k_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1

online_AST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:socket_example -- --alone=1 --num=4096 --CR=0 --cache=1

online_DoubleAST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:opt_socket_example -- --alone=1 --num=4096 --CR=0 --cache=1

clean:
	bazel clean --expunge
	rm -rf bazel-*

