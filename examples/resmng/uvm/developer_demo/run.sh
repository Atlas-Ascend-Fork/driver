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
DEP_INFO_FILE="/etc/ascend_install.info"
if [ -f "${DEP_INFO_FILE}" ]; then
    . ${DEP_INFO_FILE}
    DRV_INSTALL_PATH=""${Driver_Install_Path_Param%*/}"/driver"
    DRV_LIB64_COMMON_LDPATH=""${Driver_Install_Path_Param%*/}"/driver/lib64/common"
    DRV_LIB64_DRV_LDPATH=""${Driver_Install_Path_Param%*/}"/driver/lib64/driver"
    DRV_LIB64_LDPATH=""${Driver_Install_Path_Param%*/}"/driver/lib64"
else
    echo "[WARNING] Driver install path not found, please check the driver."
    exit 1
fi

source $DRV_INSTALL_PATH/bin/setenv.bash

BASE_DIR=$(cd "$(dirname $0)/../../../.."; pwd)

rm -rf build
mkdir -p build
cmake -B build \
    -DASCEND_COMMON_LIB_PATH=${DRV_LIB64_COMMON_LDPATH} \
    -DASCEND_DRV_LIB_PATH=${DRV_LIB64_DRV_LDPATH} \
    -DBASE_DIR=${BASE_DIR}
cmake --build build -j
cmake --install build

file_path=output_msg.txt
./build/main | tee $file_path

exit 0
