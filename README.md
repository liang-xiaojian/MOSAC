# MOSAC

Malicious Shuffle Protocol

The project depends on [YACL](https://github.com/secretflow/yacl), which provide several cryptographic interface (e.g. prg, ot, network).


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
```

performance mode
```sh
bazel build -c opt //... # compile all files (with -O2)
bazel test -c opt //... # run all test (with -O2)
```

examples
```sh
bazel run -c opt //mosac/example:memory_example.cc # secure shuffle in memory model
bazel run -c opt //mosac/example:socket_example.cc -- --rank=0/1 --num=shuffle_size --CR=0/1 --cache=0/1 # secure shuffle in socket model
bazel run -c opt //mosac/example:AST2k_example.cc -- --rank=0/1 --small_power=small_two_power --big_power=big_two_power --CR=0/1 # secure shuffle in memory model
```

command line flags
```sh
--rank 0/1                  --> 0 for party0, while 1 for party1 (memory mode would ignore this flag)
--num shuffle_size          --> size of shuffle elements
--CR 0/1                    --> 0 for fake correlation randomness (use PRG to simulate offline randomness), while 1 for true correlation randomness (use OT and VOLE to generate offline randomness)
--cache 0/1                 --> 0 for NO offline/online separating, generating CR when online is needed, while 1 for generating offline randomness before executing the online protocol.
--small_power               --> T = 2^{small_power}, used in AST2k 
--big_power                 --> size of shuffle elements = 2^{big_power}, used in AST2k
```
