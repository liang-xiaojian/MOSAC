FROM ubuntu

WORKDIR /app

# install dependency
RUN apt-get update
RUN apt-get install -y gcc cmake nasm iproute2 npm ninja-build

# install bazelisk
RUN npm install -g @bazel/bazelisk
RUN alias bazel='bazelisk'

# copy source code
COPY . /app

# It would complie and run all unit-test
RUN bazel test -c opt //...
