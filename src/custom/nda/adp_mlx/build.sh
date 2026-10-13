#!/bin/bash
# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
# build.sh - 编译安装adp_mlx驱动库
# 用法: bash build.sh [-b=libibverbs构建目录] [-e=ibv_extend构建目录]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"

LIBIBVERBS_BUILD_DIR=""
IBV_EXTEND_BUILD_DIR=""

while [ $# -gt 0 ]; do
    case $1 in
        -b=*) LIBIBVERBS_BUILD_DIR="${1#*=}" ;;
        -e=*) IBV_EXTEND_BUILD_DIR="${1#*=}" ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}"

CMAKE_ARGS="-DCMAKE_INSTALL_PREFIX=${OUTPUT_DIR}"
[ -n "${LIBIBVERBS_BUILD_DIR}" ] && CMAKE_ARGS="${CMAKE_ARGS} -DLIBIBVERBS_BUILD_DIR=${LIBIBVERBS_BUILD_DIR}"
[ -n "${IBV_EXTEND_BUILD_DIR}" ] && CMAKE_ARGS="${CMAKE_ARGS} -DIBV_EXTEND_BUILD_DIR=${IBV_EXTEND_BUILD_DIR}"

cmake .. ${CMAKE_ARGS}
make -j$(nproc 2>/dev/null || echo 4) && make install
