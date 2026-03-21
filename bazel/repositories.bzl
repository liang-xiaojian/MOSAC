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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

SECRETFLOW_GIT = "https://github.com/secretflow"

YACL_COMMIT_ID = "0154c40a46c0ea64eeb5bcda42d19d5fe5105fc1"

RULES_BOOST_COMMIT_ID = "1b6711875be9d90140e3c8e558667723bd4bed93"

SKYLIB_VERSION = "1.3.0"

def _yacl():
    maybe(
        git_repository,
        name = "yacl",
        commit = YACL_COMMIT_ID,
        remote = "https://github.com/liang-xiaojian/yacl.git",
        patch_args = ["-p1"],
        patches = ["//bazel:patches/yacl.patch"],
    )

def _gmp():
    maybe(
        http_archive,
        name = "gmp",
        build_file = "//bazel:gmp.BUILD",
        # sha256 = "fd4829912cddd12f84181c3451cc752be224643e87fac497b69edddadc49b4f2",
        strip_prefix = "gmp-6.3.0",
        urls = ["https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz"],
    )

def _boost():
    maybe(
        git_repository,
        name = "com_github_nelhage_rules_boost",
        commit = RULES_BOOST_COMMIT_ID,
        remote = "https://github.com/nelhage/rules_boost",
        shallow_since = "1580416893 -0800",
    )

def _bazel_skylib():
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/{version}/bazel-skylib-{version}.tar.gz".format(version = SKYLIB_VERSION),
            "https://github.com/bazelbuild/bazel-skylib/releases/download/{version}/bazel-skylib-{version}.tar.gz".format(version = SKYLIB_VERSION),
        ],
    )

def _rules_python():
    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "c68bdc4fbec25de5b5493b8819cfc877c4ea299c0dcb15c244c5a00208cde311",
        strip_prefix = "rules_python-0.31.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.31.0/rules_python-0.31.0.tar.gz",
    )

def _rules_pkg():
    maybe(
        http_archive,
        name = "rules_pkg",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
        ],
        sha256 = "8f9ee2dc10c1ae514ee599a8b42ed99fa262b757058f65ad3c384289ff70c4b8",
    )

def mosac_deps():
    _yacl()
    _bazel_skylib()
    _rules_python()
    _rules_pkg()
    _boost()
