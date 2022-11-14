FROM --platform=linux/amd64 ubuntu:22.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential cmake clang

COPY . /repo
WORKDIR /repo
RUN c++ -o quick-lint-js -std=gnu++17 -I src -I vendor/boost -I vendor/simdjson/include -I vendor/simdjson/src -D_LIBCPP_HIDE_FROM_ABI_PER_TU_BY_DEFAULT=1 -D_LIBCPP_INLINE_VISIBILITY="__attribute__((internal_linkage))" src/quick-lint-js/*.cpp src/quick-lint-js/*/*.cpp vendor/boost/libs/container/src/*.cpp vendor/boost/libs/container/src/alloc_lib.c vendor/simdjson/src/simdjson.cpp -pthread

WORKDIR /repo/build
RUN cmake ..
RUN make -j8
RUN clang++ /repo/mayhem/fuzz_checked_add.cpp -fsanitize=fuzzer,address -I /repo/src -std=c++17 -o /fuzz


FROM ubuntu:22.04 as package
COPY --from=builder /repo/quick-lint-js /
COPY --from=builder /fuzz /fuzz_checked_add
