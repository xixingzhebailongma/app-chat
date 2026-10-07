#!/usr/bin/env bash
set -euo pipefail

# wechat-gateway 的一次性依赖安装（第 4 节骨架）。
#   - 安装 Drogon 的系统依赖（需要 sudo）。
#   - 将 Drogon + trantor 构建到 third_party/install（不做系统级安装）。
#
# 无需 sudo 的替代方案：使用 vcpkg（`vcpkg install drogon nlohmann-json`）
# 并将 CMake 指向 vcpkg 工具链文件。

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="$ROOT/third_party/install"
SRC="$ROOT/third_party/src"

echo "[1/4] Installing system dependencies (needs sudo)..."
sudo apt-get update
sudo apt-get install -y \
    libjsoncpp-dev libc-ares-dev uuid-dev \
    libssl-dev zlib1g-dev libbrotli-dev \
    libhiredis-dev libpq-dev \
    cmake g++ git

echo "[2/4] Cloning Drogon + trantor..."
mkdir -p "$SRC"
if [ ! -d "$SRC/drogon" ]; then
    git clone --recursive https://github.com/drogonframework/drogon.git "$SRC/drogon"
fi

echo "[3/4] Building Drogon..."
# BUILD_ORM=OFF 跳过 libpq（PostgreSQL），这样暂时无需 libpq-dev。
cmake -S "$SRC/drogon" -B "$SRC/drogon/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_CTL=OFF -DBUILD_ORM=OFF
cmake --build "$SRC/drogon/build" -j"$(nproc)"
cmake --install "$SRC/drogon/build"

echo "[4/4] Done. Drogon installed to $PREFIX"
echo "Now build the gateway:"
echo "  cmake -S . -B build -DCMAKE_PREFIX_PATH=$PREFIX"
echo "  cmake --build build -j"
