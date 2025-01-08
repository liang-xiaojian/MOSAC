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

example: offline_shuffle offline_shuffle_opt online_shuffle offline_AST2k online_AST2k

offline_shuffle:
	bazel run -c opt --distdir=./thirdparty //mosac/example:NDSS_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1 --opt=0

offline_shuffle_opt:
	bazel run -c opt --distdir=./thirdparty //mosac/example:NDSS_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1 --opt=1

online_shuffle:
	bazel run -c opt --distdir=./thirdparty //mosac/example:NDSS_online_example -- --alone=1 --small_power=4 --big_power=12 --CR=0 --cache=1

offline_AST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:AST2k_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1

online_AST2k:
	bazel run -c opt --distdir=./thirdparty //mosac/example:socket_example -- --alone=1 --num=4096 --CR=0 --cache=1

clean:
	bazel clean --expunge
	rm -rf bazel-*

