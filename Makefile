default: test

release:
	bazel build -c opt //...

debug:
	bazel build //...

test_all: test offline_shuffle offline_shuffle_opt online_shuffle offline_AST2k online_AST2k

test:
	bazel test -c opt //...

offline_shuffle:
	bazel run -c opt //mosac/example:NDSS_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1 --opt=0

offline_shuffle_opt:
	bazel run -c opt //mosac/example:NDSS_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1 --opt=1

online_shuffle:
	bazel run -c opt //mosac/example:NDSS_online_example -- --alone=1 --small_power=4 --big_power=12 --CR=0 --cache=1

offline_AST2k:
	bazel run -c opt //mosac/example:AST2k_offline_example -- --alone=1 --small_power=4 --big_power=12 --CR=1

online_AST2k:
	bazel run -c opt //mosac/example:socket_example -- --alone=1 --num=4096 --CR=0 --cache=1

clean:
	bazel clean --expunge
	rm -rf bazel-*

