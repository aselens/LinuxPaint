#!/usr/bin/env bash
# Сборка одной командой. Полезно на свежей машине: сначала проверяет,
# что есть всё необходимое, и подсказывает пакет, если чего-то не хватает.
set -euo pipefail

cd "$(dirname "$0")"

missing=()
command -v cmake >/dev/null 2>&1 || missing+=("cmake")
command -v c++   >/dev/null 2>&1 || missing+=("g++")

if [ ${#missing[@]} -ne 0 ]; then
    echo "Не найдено: ${missing[*]}" >&2
    echo "Debian/Ubuntu: sudo apt install build-essential cmake qt6-base-dev" >&2
    echo "Fedora:        sudo dnf install gcc-c++ cmake qt6-qtbase-devel" >&2
    echo "Arch:          sudo pacman -S base-devel cmake qt6-base" >&2
    exit 1
fi

jobs="$(nproc 2>/dev/null || echo 4)"

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$jobs"

echo
echo "Готово: ./build/linux-paint"
echo "Установить в систему: sudo cmake --install build"
