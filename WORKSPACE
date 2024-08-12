workspace(name = "test")

load("//bazel:repositories.bzl", "mosac_deps")

mosac_deps()

load("@yacl//bazel:repositories.bzl", "yacl_deps")

yacl_deps()

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()

load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
rules_pkg_dependencies()

load(
    "@rules_foreign_cc//foreign_cc:repositories.bzl",
    "rules_foreign_cc_dependencies",
)

rules_foreign_cc_dependencies(
    register_built_tools = False,
    register_default_tools = False,
    register_preinstalled_tools = True,
)

# ref: https://github.com/google/llvm-bazel/blob/http-archive-demo/http-archive-demo/WORKSPACE
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

LLVM_COMMIT = "499bce3abab8a362b9b4197944bd40b826c736c4"

LLVM_BAZEL_TAG = "llvm-project-%s" % (LLVM_COMMIT,)

LLVM_BAZEL_SHA256 = "a05a83300b6b4d8b45c9ba48296c06217f3ea27ed06b7e698896b5a3b2ed498d"

http_archive(
    name = "llvm-bazel",
    sha256 = LLVM_BAZEL_SHA256,
    strip_prefix = "llvm-bazel-{tag}/llvm-bazel".format(tag = LLVM_BAZEL_TAG),
    url = "https://github.com/google/llvm-bazel/archive/{tag}.tar.gz".format(tag = LLVM_BAZEL_TAG),
)

LLVM_SHA256 = "a154965dfeb2b5963acc2193bc334ce90b314acbe48430ba310d8a7c7a20de8b"

LLVM_URLS = [
    "https://storage.googleapis.com/mirror.tensorflow.org/github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT),
    "https://github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT),
]

http_archive(
    name = "llvm-project-raw",
    build_file_content = "#empty",
    sha256 = LLVM_SHA256,
    strip_prefix = "llvm-project-" + LLVM_COMMIT,
    urls = LLVM_URLS,
)

load("@llvm-bazel//:terminfo.bzl", "llvm_terminfo_disable")

llvm_terminfo_disable(
    name = "llvm_terminfo",
)

load("@llvm-bazel//:zlib.bzl", "llvm_zlib_disable")

llvm_zlib_disable(
    name = "llvm_zlib",
)

load("@llvm-bazel//:configure.bzl", "llvm_configure")

llvm_configure(
    name = "llvm-project",
    src_path = ".",
    src_workspace = "@llvm-project-raw//:WORKSPACE",
)
