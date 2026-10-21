# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

# CMake lowest version requirement
cmake_minimum_required(VERSION 3.16.0)

# Project information
project(Devmng_Sample)

include_directories(${ASCEND_DRV_PACKAGE_PATH}/include 
                    ${BASE_DIR}/pkg_inc
                    ${BASE_DIR}/examples)

link_directories(${ASCEND_DRV_LIB_PATH})

add_executable(main main.c)

target_link_libraries(main ascend_hal)
