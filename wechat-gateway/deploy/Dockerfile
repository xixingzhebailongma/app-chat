# syntax=docker/dockerfile:1
#
# wechat-gateway 生产镜像（P0：修复 deploy/Dockerfile 空文件）。
# 构建上下文 = 仓库根（wechat-gateway/）：
#   docker build -f deploy/Dockerfile -t wechat-gateway:latest .
#
# 构建需要网络：FetchContent 拉取 nlohmann/json，git clone Drogon。
# Drogon/trantor 编译为静态库（BUILD_ORM=OFF），直接进二进制；网关的
# PostgreSQL 能力来自 find_package(PostgreSQL) 直连 libpq（见 CMakeLists.txt）。

# ===== 构建阶段：编译 Drogon（静态）+ 网关 =====
FROM debian:bookworm-slim AS build
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake g++ git ca-certificates \
        libjsoncpp-dev libc-ares-dev uuid-dev \
        libssl-dev zlib1g-dev libbrotli-dev \
        libhiredis-dev libpq-dev \
    && rm -rf /var/lib/apt/lists/*

# 拉取并构建 Drogon（flags 与 scripts/setup-deps.sh 一致），安装到 /opt/drogon。
# BUILD_ORM=OFF：网关不用 Drogon ORM，libpq 由 CMake find_package(PostgreSQL) 直连。
RUN git clone --recursive https://github.com/drogonframework/drogon.git /opt/drogon-src \
    && cmake -S /opt/drogon-src -B /opt/drogon-src/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/drogon \
        -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_CTL=OFF -DBUILD_ORM=OFF \
    && cmake --build /opt/drogon-src/build -j"$(nproc)" \
    && cmake --install /opt/drogon-src/build \
    && rm -rf /opt/drogon-src

WORKDIR /src
COPY CMakeLists.txt ./
COPY src/ src/
COPY tests/ tests/

RUN cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/drogon \
    && cmake --build build --target wechat-gateway -j"$(nproc)"

# ===== 运行阶段：只装运行时共享库 + 二进制 + 配置 =====
FROM debian:bookworm-slim AS runtime
ENV DEBIAN_FRONTEND=noninteractive

# 运行时依赖（与 `ldd build/wechat-gateway` 对齐；libpq5 会自动带出
# krb5/ldap/sasl/gnutls 等传递依赖）。
RUN apt-get update && apt-get install -y --no-install-recommends \
        libjsoncpp25 libhiredis0.14 libpq5 \
        libssl3 zlib1g libbrotli1 libuuid1 \
        libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/wechat-gateway /app/wechat-gateway
COPY config/ /app/config/

# 运行期写 operation_logs.jsonl 到 cwd=/app；凭证/DB/Redis 全部走环境变量。
EXPOSE 8080
ENTRYPOINT ["/app/wechat-gateway"]
