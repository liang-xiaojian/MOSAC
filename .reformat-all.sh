#!/bin/bash

if ! [ -x "$(command -v clang-format)" ]; then
  echo 'Error: clang-format is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v buildifier)" ]; then
  echo 'Error: buildifier is not installed.' >&2
  exit 1
fi

# c++
echo 'Formatting c++ code'
find . -type f -name '*.h' -o -name '*.cc' -o -name '*.cu' | xargs clang-format -i

# bazel
echo 'Formatting bazel code'
buildifier -r .
find . -type f -name '*.BUILD' | xargs buildifier 

