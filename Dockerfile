# syntax=docker/dockerfile:1

FROM openresty/openresty:noble AS base

ENV LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu"
ENV LUA_CPATH="/app/build/?.so;/usr/local/lib/lua/5.1/?.so;/usr/local/lib/x86_64-linux-gnu/?.so"
ENV LUA_PATH="/app/integration/tests/?.lua;;"

RUN <<EOF
DEBIAN_FRONTEND="noninteractive" apt-get update
DEBIAN_FRONTEND="noninteractive" apt-get install -y \
  git \
  libpcre3-dev \
  boxes \
  clang \
  clang-format \
  make \
  cmake \
  libssl-dev \
  libuv1-dev \
  zlib1g-dev \
  libluajit-5.1-dev \
  luajit \
  luarocks \
  pkg-config \
  valgrind \
  gdb \
  doxygen \
  graphviz
apt-get clean
git config --global url.https://.insteadOf git://
luarocks install busted
luarocks install luasocket
luarocks install uuid
luarocks install lua-cassandra
EOF

COPY --from=mvdan/shfmt:v3-alpine /bin/shfmt /usr/bin/shfmt
# COPY --from=ghcr.io/JohnnyMorganz/StyLua:stylua:0.17.0 /stylua /usr/bin/stylua

COPY ./vendor /app/vendor/
WORKDIR /app/vendor/cpp-driver/build
RUN <<EOF
cmake ..
make
make install
EOF
WORKDIR /app

FROM base AS build
COPY . /app/
WORKDIR /app/build
RUN <<EOF
cmake ..
cmake --build .
EOF
WORKDIR /app
RUN ./format.sh

FROM scratch AS artifacts
COPY --from=build /app/build/lucas.so* /
COPY --from=build /app/vendor/cpp-driver/build/libcassandra.so* /
