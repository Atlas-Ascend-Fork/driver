#!/usr/bin/env bash
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

# Build only NDA against installed dependencies. No downloads or package changes.
set -euo pipefail
ADP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PWD}/build/nda-installed"
PREFIX=""
RDMA_PREFIX=""
FRAMEWORK_BUILD=""
RUNTIME_ONLY=false
SANITIZE=OFF
BUILD_TYPE=Release
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
while (($#)); do
    case "$1" in
        --build-dir|--prefix|--rdma-prefix|--framework-build-dir)
            (($# >= 2)) && [[ -n "$2" && "$2" != -* ]] || { echo "Missing value for $1" >&2; exit 2; }
            case "$1" in
                --build-dir) BUILD_DIR="$2" ;;
                --prefix) PREFIX="$2" ;;
                --rdma-prefix) RDMA_PREFIX="$2" ;;
                --framework-build-dir) FRAMEWORK_BUILD="$2" ;;
            esac
            shift 2 ;;
        --runtime-only) RUNTIME_ONLY=true; shift ;;
        --sanitize) SANITIZE=ON; BUILD_TYPE=Debug; shift ;;
        --help)
            echo "Usage: bash $0 [--build-dir DIR] [--prefix DIR] [--rdma-prefix /usr-or-extracted-usr]"
            echo "Options: --framework-build-dir DIR (reuse, Provider only), --runtime-only (lib only), --sanitize"
            echo "Optional environment: NDA_SECUREC_INCLUDE_DIR, NDA_SECUREC_LIBRARY (full .so path), NDA_RDMA_UAPI_INCLUDE_DIR, NDA_RDMA_SOURCE_DIR, NDA_CCAN_INCLUDE_DIR, CMAKE_BUILD_PARALLEL_LEVEL"
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done
mkdir -p -- "$BUILD_DIR"
BUILD_DIR="$(cd -- "$BUILD_DIR" && pwd)"
PREFIX="${PREFIX:-$BUILD_DIR/install}"
mkdir -p -- "$PREFIX"
PREFIX="$(cd -- "$PREFIX" && pwd)"
COMMON=(-DNDA_DEPENDENCY_MODE=INSTALLED "-DCMAKE_BUILD_TYPE=$BUILD_TYPE")
[[ -z "$RDMA_PREFIX" ]] || COMMON+=("-DNDA_RDMA_PREFIX=$RDMA_PREFIX")
[[ -z "${NDA_RDMA_UAPI_INCLUDE_DIR:-}" ]] || COMMON+=("-DNDA_RDMA_UAPI_INCLUDE_DIR=$NDA_RDMA_UAPI_INCLUDE_DIR")
[[ -z "${NDA_CCAN_INCLUDE_DIR:-}" ]] || COMMON+=("-DNDA_CCAN_INCLUDE_DIR=$NDA_CCAN_INCLUDE_DIR")
[[ -z "${NDA_RDMA_SOURCE_DIR:-}" ]] || COMMON+=("-DNDA_RDMA_SOURCE_DIR=$NDA_RDMA_SOURCE_DIR")
SECUREC=()
[[ -z "${NDA_SECUREC_INCLUDE_DIR:-}" ]] || SECUREC+=("-DNDA_SECUREC_INCLUDE_DIR=$NDA_SECUREC_INCLUDE_DIR")
[[ -z "${NDA_SECUREC_LIBRARY:-}" ]] || SECUREC+=("-DNDA_SECUREC_LIBRARY=$NDA_SECUREC_LIBRARY")
BUILD_FRAMEWORK=true
if [[ -n "$FRAMEWORK_BUILD" ]]; then
    FRAMEWORK_BUILD="$(cd -- "$FRAMEWORK_BUILD" && pwd)"
    BUILD_FRAMEWORK=false
else
    FRAMEWORK_BUILD="$BUILD_DIR/framework-upstream"
    cmake -S "$ADP_DIR/../ibv_extend" -B "$FRAMEWORK_BUILD" \
        "${COMMON[@]}" "${SECUREC[@]}" \
        "-DCMAKE_PROJECT_ibv_extend_INCLUDE=$ADP_DIR/cmake/RdmaDependencies.cmake" \
        -DBUILD_TESTS=OFF -DBUILD_UNIT_TESTS=OFF
    cmake --build "$FRAMEWORK_BUILD" --parallel "$JOBS"
fi
cmake -S "$ADP_DIR" -B "$BUILD_DIR/provider" "${COMMON[@]}" \
    "-DADP_MLX_SANITIZERS=$SANITIZE" "-DIBV_EXTEND_BUILD_DIR=$FRAMEWORK_BUILD"

# Use the exact configured library directories for dependency inspection, including
# when the caller selected an extracted SDK rather than system /usr.
if [[ -r "$FRAMEWORK_BUILD/CMakeCache.txt" ]]; then
    while IFS='=' read -r key value; do
        if [[ "$key" == NDA_SECUREC_LIBRARY:FILEPATH ]]; then
            export LD_LIBRARY_PATH="$(dirname -- "$value")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        fi
    done < "$FRAMEWORK_BUILD/CMakeCache.txt"
fi
if [[ -n "${NDA_SECUREC_LIBRARY:-}" ]]; then
    export LD_LIBRARY_PATH="$(dirname -- "$NDA_SECUREC_LIBRARY")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
while IFS='=' read -r key value; do
    case "$key" in
        NDA_IBVERBS_LIBRARY:FILEPATH|NDA_MLX5_LIBRARY:FILEPATH)
            export LD_LIBRARY_PATH="$(dirname -- "$value")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ;;
    esac
done < "$BUILD_DIR/provider/CMakeCache.txt"
export LD_LIBRARY_PATH="$BUILD_DIR/provider:$FRAMEWORK_BUILD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake --build "$BUILD_DIR/provider" --parallel "$JOBS"
# Component installation changes staging only.
stage_build() {
    if $RUNTIME_ONLY; then
        cmake --install "$1" --prefix "$PREFIX" --component Runtime
    else
        cmake --install "$1" --prefix "$PREFIX"
    fi
}
if $BUILD_FRAMEWORK; then
    # The original framework installs without components. Stage its library
    # separately for runtime-only builds; full installs also retain its header.
    if $RUNTIME_ONLY; then
        cmake --install "$FRAMEWORK_BUILD" --prefix "$FRAMEWORK_BUILD/stage"
        mkdir -p -- "$PREFIX/lib"
        cp -a -- "$FRAMEWORK_BUILD/stage/lib/." "$PREFIX/lib/"
    else
        cmake --install "$FRAMEWORK_BUILD" --prefix "$PREFIX"
    fi
fi
stage_build "$BUILD_DIR/provider"
export LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH"
DEPENDENCIES="$(ldd "$PREFIX/lib/libadp_mlx.so")"
printf '%s\n' "$DEPENDENCIES"
[[ "$DEPENDENCIES" != *"not found"* ]] || { echo "Installed library dependency missing" >&2; exit 1; }
printf '\nStaged NDA in %s\n' "$PREFIX"
printf 'For applications: export IBV_EXTEND_DRIVERS="%s/lib/libadp_mlx.so"\n' "$PREFIX"
printf 'Add %s/lib and any non-system securec directory to LD_LIBRARY_PATH.\n' "$PREFIX"
if ! $BUILD_FRAMEWORK; then
    printf 'Reused framework (not copied): %s; deploy its matching libibv_extend.so* separately.\n' "$FRAMEWORK_BUILD"
fi
