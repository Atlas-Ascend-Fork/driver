#!/bin/bash
# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
#
# Build & run the NPU Direct Storage (NDS) demo:
#   1. build the npu_nds.ko kernel module
#   2. insmod it -> /dev/npu-nds
#   3. build libnpu_file.so + npu_nds_demo
#   4. run the demo (open device -> register -> read -> verify -> write -> verify -> deregister -> close)
#   5. rmmod on exit
#
# Must be executed as root (insmod/rmmod + pinning NPU memory).

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")"; pwd)
DEMO_DIR="${SCRIPT_DIR}"
KERN_SRC="${DEMO_DIR}/kernel"

DEP_INFO_FILE="/etc/ascend_install.info"
if [ -f "${DEP_INFO_FILE}" ]; then
    . "${DEP_INFO_FILE}"
    DRV_INSTALL_PATH="${Driver_Install_Path_Param%*/}/driver"
    DRV_LIB64_COMMON_LDPATH="${Driver_Install_Path_Param%*/}/driver/lib64/common"
    DRV_LIB64_DRV_LDPATH="${Driver_Install_Path_Param%*/}/driver/lib64/driver"
    DRV_LIB64_LDPATH="${Driver_Install_Path_Param%*/}/driver/lib64"
else
    echo "[ERROR] Driver install path not found, please check the driver."
    exit 1
fi

# the driver setenv.bash exports the kernel build inc dirs used by the Makefile
source "${DRV_INSTALL_PATH}/bin/setenv.bash"

BASE_DIR=$(cd "${DEMO_DIR}/../.."; pwd)

KDIR="${KDIR:-/lib/modules/$(uname -r)/build}"
echo "[INFO] KDIR=${KDIR}"
echo "[INFO] DRIVER_HAL_INC_DIR=${DRIVER_HAL_INC_DIR:-<unset>}"
echo "[INFO] DRIVER_KERNEL_DIR=${DRIVER_KERNEL_DIR:-<unset>}"

# 1. build the kernel module
echo "======================================"
echo "[1/4] Building npu_nds.ko"
echo "======================================"
rm -rf "${KERN_SRC}/build"
mkdir -p "${KERN_SRC}/build"
make -C "${KDIR}" \
     M="${KERN_SRC}" \
     NDS_SRC_DIR="${KERN_SRC}" \
     src="${KERN_SRC}" \
     modules

# 2. load the module
echo "======================================"
echo "[2/4] Loading npu_nds.ko"
echo "======================================"
if lsmod | grep -q "^npu_nds "; then
    echo "[INFO] npu_nds already loaded, reloading"
    rmmod npu_nds || true
fi
# the module depends on the NPU SVM symbols (hal_kernel_p2p_get_pages)
insmod "${KERN_SRC}/npu_nds.ko" || {
    echo "[ERROR] insmod failed; check dmesg | tail"
    exit 1
}
sleep 1
if [ -c /dev/npu-nds ]; then
    echo "[INFO] /dev/npu-nds ready"
else
    echo "[ERROR] /dev/npu-nds not created"
    rmmod npu_nds 2>/dev/null || true
    exit 1
fi

cleanup() {
    echo "======================================"
    echo "[cleanup] rmmod npu_nds"
    echo "======================================"
    rmmod npu_nds 2>/dev/null || true
}
trap cleanup EXIT

# 3. build the client library + demo
echo "======================================"
echo "[3/4] Building libnpu_file + npu_nds_demo"
echo "======================================"
rm -rf "${DEMO_DIR}/build"
cmake -B "${DEMO_DIR}/build" \
      -S "${DEMO_DIR}" \
      -DASCEND_COMMON_LIB_PATH="${DRV_LIB64_COMMON_LDPATH}" \
      -DASCEND_DRV_LIB_PATH="${DRV_LIB64_DRV_LDPATH}" \
      -DBASE_DIR="${BASE_DIR}"
cmake --build "${DEMO_DIR}/build" -j
cmake --install "${DEMO_DIR}/build"

# 4. run
echo "======================================"
echo "[4/4] Running NPU NDS demo"
echo "======================================"
file_path=output_msg.txt
export LD_LIBRARY_PATH="${DEMO_DIR}/build/lib:${LD_LIBRARY_PATH}"
"${DEMO_DIR}/build/demo/npu_nds_demo" | tee "${file_path}"
main_ret=${PIPESTATUS[0]}

if [ "${main_ret}" -ne 0 ]; then
    echo "[ERROR] NDS demo failed: ret=${main_ret}"
    exit 1
fi

echo "[INFO] NDS demo finished successfully."
exit 0
