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

# README entry: build locally, then stage libraries under output/lib.
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"
FRAMEWORK_BUILD=""
RDMA_PREFIX=""
FULL_INSTALL=false
SANITIZE=false
usage() {
    cat <<'EOF'
Usage: bash build.sh [OPTIONS]
  No arguments: use installed RDMA packages; build the matching framework too.
  -e=PATH            Reuse an existing ibv_extend build (Provider only)
  --rdma-prefix PATH Installed/extracted RDMA prefix (e.g. /usr)
  -o=PATH            Build directory (default: ./build relative to this script)
  --output-dir PATH  Staging directory (default: ./output relative to this script)
  --full-install     Also stage headers and configuration (default: lib/)
  --sanitize        Enable Provider ASan and UBSan
  -h, --help         Show help
Environment: NDA_SECUREC_INCLUDE_DIR, NDA_SECUREC_LIBRARY, CMAKE_BUILD_PARALLEL_LEVEL
             NDA_RDMA_UAPI_INCLUDE_DIR, NDA_RDMA_SOURCE_DIR (extracted rdma-core tree)
             NDA_CCAN_INCLUDE_DIR (optional CCAN override; defaults to source tree)
No system installation or third-party download is performed.
EOF
}
fail() { echo "$*" >&2; exit 2; }
while (($#)); do
    case "$1" in
        -b|-b=*|-s|-s=*)
            fail "RDMA source/build options were removed; use --rdma-prefix for installed packages" ;;
        -e=*|-o=*)
            value="${1#*=}"
            [[ -n "$value" ]] || fail "Missing value for ${1%%=*}"
            case "$1" in
                -e=*) FRAMEWORK_BUILD="$value" ;;
                -o=*) BUILD_DIR="$value" ;;
            esac
            shift ;;
        --rdma-prefix|--output-dir)
            (($# >= 2)) && [[ -n "$2" && "$2" != -* ]] || fail "Missing value for $1"
            case "$1" in
                --rdma-prefix) RDMA_PREFIX="$2" ;;
                --output-dir) OUTPUT_DIR="$2" ;;
            esac
            shift 2 ;;
        --full-install) FULL_INSTALL=true; shift ;;
        --sanitize) SANITIZE=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; fail "Unknown option: $1" ;;
    esac
done

args=(--build-dir "$BUILD_DIR" --prefix "$OUTPUT_DIR")
[[ -z "$RDMA_PREFIX" ]] || args+=(--rdma-prefix "$RDMA_PREFIX")
[[ -z "$FRAMEWORK_BUILD" ]] || args+=(--framework-build-dir "$FRAMEWORK_BUILD")
$FULL_INSTALL || args+=(--runtime-only)
$SANITIZE && args+=(--sanitize)
exec bash "$SCRIPT_DIR/tools/build_installed.sh" "${args[@]}"
