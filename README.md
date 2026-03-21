# MOSAC

Malicious Shuffle Protocol

The project depends on [YACL](https://github.com/secretflow/yacl), which provide several cryptographic interface (e.g. prg, ot, network).

<!--
Copyright 2026 Ant International, Ant Group Co., Ltd.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author (Xiaojian Liang)
-->

### File layout:
+ [context](mcpsi/context/): provide runtime environment
+ [cr](mcpsi/cr/): correlated-randomness (e.g. Beaver Triple, MAC generation) 
+ [ss](mcpsi/ss/): SPDZ-like protocol, supports several operators (e.g. Mul, Shuffle) between public value and arithmetic share.
+ [utils](mcpsi/utils/): basic tools (e.g. 64bit / 128bit field)

### Dependencies

#### Linux
```sh
Install gcc>=10.3, cmake, ninja, nasm
```

#### macOS
```sh
# Install Xcode
https://apps.apple.com/us/app/xcode/id497799835?mt=12

# Select Xcode toolchain version
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

# Install homebrew
https://brew.sh/

# Install dependencies
brew install bazel cmake ninja nasm automake libtool
```

### Build && Test

debug mode (only for developing)
```sh
bazel build //... # compile all files
bazel test //... # run all test

# OR
make release # compile all files
make test    # run all test
make test_all # run all examples and test
```

performance mode
```sh
bazel build -c opt //... # compile all files (with -O2)
bazel test -c opt //... # run all test (with -O2)
```

clean all
```sh
bazel clean --expunge # clean all bazel-*

# OR
make clean
```

examples
```sh
bazel run -c opt //mosac/example:memory_example # secure shuffle in memory model (PoC)
bazel run -c opt //mosac/example:socket_example -- --rank=0/1 --num=shuffle_size --CR=0/1 --cache=0/1 # secure shuffle in socket model
bazel run -c opt //mosac/example:AST2k_offline_example -- --rank=0/1 --small_power=small_two_power --big_power=big_two_power --CR=0/1 # secure shuffle (offline bench benchmark)
bazel run -c opt //mosac/example:NDSS_online_example -- --rank=0/1 --small_power=small_two_power --big_power=big_two_power --CR=0/1 --cache=0/1 --opt=0/1 # NDSS shuffle (online benchmark)
bazel run -c opt //mosac/example:NDSS_offline_example -- --rank=0/1 --small_power=small_two_power --big_power=big_two_power --CR=0/1 # NDSS shuffle (offline benchmark)
bazel run -c opt //mosac/example:NMul_offline_example -- --alone=0/1 --rank=0/1 --CR=0/1 --num=number_for_N # NMul Share benchmark
bazel run -c opt //mosac/example:AShare_example -- --alone=0/1 --rank=0/1 --CR=0/1 --num=number_of_ashare # A Share benchmark
```

command line flags
```sh
--alone 0/1                 --> 1 for single terminal to create two threads to run the protocol (default 0)
--rank 0/1                  --> 0 for party0, while 1 for party1 (memory mode would ignore this flag)
--num shuffle_size          --> size of shuffle elements
--CR 0/1                    --> 0 for fake correlation randomness (use PRG to simulate offline randomness), while 1 for true correlation randomness (use OT and VOLE to generate offline randomness)
--cache 0/1                 --> 0 for NO offline/online separating, generating CR when online is needed, while 1 for generating offline randomness before executing the online protocol.
--small_power               --> T = 2^{small_power}, used in AST2k 
--big_power                 --> size of shuffle elements = 2^{big_power}, used in AST2k
--opt 0/1                   --> 0 for sgrr-ote, 1 for gywz-ote
```

### About Dockerfile
```sh
# build docker image to solve dependency
docker build -t mosac:latest .   
# create container
docker run -it --name mosac-dev --cap-add=NET_ADMIN --privileged=true mosac:latest bash
# build all && unit test
bazel test -c opt //...
```

```sh
# re-enter container or stop it
docker start mosac-dev          # start 
docker exec -it mosac-dev bash  # launch the terminal
docker stop mosac-dev           # stop
```

### NOTICE
Fake CR (FakeCorrelation) in `mosac/cr` is provided only for benchmarking and performance evaluation. It uses simulated randomness and MUST NOT be used in production or for security-sensitive deployments.