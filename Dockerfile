# syntax=docker/dockerfile:1

FROM openresty/openresty:noble AS base

ENV C_INCLUDE_PATH="/usr/include/luajit-2.1/"
ENV LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu"
ENV LUA_CPATH="/app/build/?.so;/usr/local/lib/lua/5.1/?.so;/usr/local/lib/x86_64-linux-gnu/?.so"
ENV LUA_PATH="/app/integration/tests/?.lua;;"
ENV PATH="$PATH:/opt/openresty-devel-utils"

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

# RUN apt-get -y install --no-install-recommends wget gnupg ca-certificates lsb-release \
#     wget -O - https://openresty.org/package/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/openresty.gpg \
#     echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/openresty.gpg] http://openresty.org/package/ubuntu $(lsb_release -sc) main" | sudo tee /etc/apt/sources.list.d/openresty.list > /dev/null \
#     apt-get update \
#     apt-get -y install openresty-dev

# wget -qO- https://github.com/openresty/openresty-devel-utils/archive/refs/heads/master.zip | busybox unzip -
# mv /opt/openresty-devel-utils-master /opt/openresty-devel-utils
# chmod +x /opt/openresty-devel-utils/ngx-*

COPY --from=mvdan/shfmt:v3-alpine /bin/shfmt /usr/bin/shfmt
COPY --from=johnnymorganz/stylua:2.0.2 /stylua /usr/bin/stylua
COPY vendor /app/vendor/

WORKDIR /app/vendor/cpp-driver/build
RUN <<EOF
cmake ..
make
make install
EOF

WORKDIR /app/vendor/nginx
RUN <<EOF
auto/configure
make
make install
EOF

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
