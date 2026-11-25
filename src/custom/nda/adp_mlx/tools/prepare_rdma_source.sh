#!/usr/bin/env bash
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

# Print only the selected source directory to stdout for the build entry point.
set -euo pipefail
fail() { printf 'RDMA source preparation failed: %s\n' "$*" >&2; exit 1; }
need_tool() { command -v "$1" >/dev/null 2>&1 || fail "Missing tool: $1"; }
verify_download_package() {
    local actual
    actual="$(sha256sum -- "$1")" || fail "Cannot checksum downloaded source RPM: $1"
    [[ "${actual%% *}" == "$download_sha256" ]] || \
        fail "Downloaded source RPM SHA256 mismatch: expected $download_sha256; got ${actual%% *}"
}
check_source() {
    local source_dir="$1" file
    for file in libibverbs/driver.h providers/mlx5/mlx5.h util/util.h ccan/list.h \
                buildlib/config.h.in buildlib/make_abi_structs.py kernel-headers/rdma/ib_user_verbs.h; do
        [[ -f "$source_dir/$file" ]] || fail "Missing $source_dir/$file; provide a complete rdma-core-2410mlnx54 source tree"
    done
}
if [[ -n "${NDA_RDMA_SOURCE_DIR:-}" ]]; then
    [[ -d "$NDA_RDMA_SOURCE_DIR" ]] || fail "NDA_RDMA_SOURCE_DIR is not a directory: $NDA_RDMA_SOURCE_DIR"
    source_dir="$(cd -- "$NDA_RDMA_SOURCE_DIR" && pwd)"
    check_source "$source_dir"
    printf '%s\n' "$source_dir"
    exit 0
fi

[[ $# == 1 && -n "$1" ]] || fail "Usage: bash $0 CACHE_DIRECTORY"
source_name=rdma-core-2410mlnx54
package_name=rdma-core-2410mlnx54-1.2410068.src.rpm
package_url="https://linux.mellanox.com/public/repo/mlnx_ofed/24.10-5.1.6.1/SRPMS/$package_name"
# Pin the reviewed source RPM before extracting build-time scripts.
download_sha256=adbaba621af3b5d9e0a13f6af5dd903daf06fc0872b3cef6ed67d0f02abe302a
package_path=""
cache_key="download-$download_sha256"
need_tool sha256sum
if [[ -n "${NDA_RDMA_SOURCE_RPM:-}" ]]; then
    [[ -f "$NDA_RDMA_SOURCE_RPM" ]] || fail "NDA_RDMA_SOURCE_RPM is not a file: $NDA_RDMA_SOURCE_RPM"
    package_path="$(cd -- "$(dirname -- "$NDA_RDMA_SOURCE_RPM")" && pwd)/$(basename -- "$NDA_RDMA_SOURCE_RPM")"
    package_hash="$(sha256sum -- "$package_path")"
    cache_key="local-${package_hash%% *}"
fi
mkdir -p -- "$1"
cache_dir="$(cd -- "$1" && pwd)"
cached="$cache_dir/$cache_key"
if [[ -d "$cached" ]]; then
    [[ -n "$package_path" ]] || verify_download_package "$cached/$package_name"
    check_source "$cached/$source_name"
    printf '%s\n' "$cached/$source_name"
    exit 0
fi

for tool in flock rpm2cpio cpio tar; do need_tool "$tool"; done
[[ -n "$package_path" ]] || need_tool curl
exec 9>"$cache_dir/.prepare.lock"
flock 9
# Another build may have published the cache while this process waited.
if [[ -d "$cached" ]]; then
    [[ -n "$package_path" ]] || verify_download_package "$cached/$package_name"
    check_source "$cached/$source_name"
    printf '%s\n' "$cached/$source_name"
    exit 0
fi
temporary="$(mktemp -d "$cache_dir/.prepare.XXXXXX")"
trap 'rm -rf -- "$temporary"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
if [[ -z "$package_path" ]]; then
    package_path="$temporary/$package_name"
    printf '[NDA] Downloading %s\n' "$package_url" >&2
    curl --fail --location --proto '=https' --proto-redir '=https' \
        --connect-timeout 15 --max-time 180 --retry 2 --retry-max-time 180 \
        --output "$package_path" "$package_url" || \
        fail "Download failed; set NDA_RDMA_SOURCE_RPM to a local source RPM for an offline build"
    verify_download_package "$package_path"
fi
printf '[NDA] Extracting %s\n' "$package_path" >&2
(
    cd -- "$temporary" || exit 1
    rpm2cpio "$package_path" | cpio -idm --quiet --no-absolute-filenames --no-preserve-owner \
        "$source_name.tar.gz" "./$source_name.tar.gz"
) || fail "Cannot extract source archive from $package_path"
[[ -f "$temporary/$source_name.tar.gz" && ! -L "$temporary/$source_name.tar.gz" ]] || \
    fail "Source RPM must contain $source_name.tar.gz"
tar -xzf "$temporary/$source_name.tar.gz" -C "$temporary" \
    --no-same-owner --no-same-permissions "$source_name" || fail "Cannot unpack $source_name.tar.gz"
check_source "$temporary/$source_name"
if [[ -n "${NDA_RDMA_SOURCE_RPM:-}" ]]; then
    [[ "$(sha256sum -- "$package_path")" == "$package_hash" ]] || fail "Source RPM changed during extraction"
fi
rm -f -- "$temporary/$source_name.tar.gz"
# Publish only after both extraction steps and source checks have succeeded.
mv -T -- "$temporary" "$cached"
printf '%s\n' "$cached/$source_name"
