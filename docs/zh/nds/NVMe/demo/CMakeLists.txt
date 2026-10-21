# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

# Build the NDS demo executable.
# The demo links:
#   - npu_file         : NDS ioctl wrapper (libc + kernel only)
#   - libascend_hal.so : HAL user-space API (halMemAlloc/Free, drvMemsetD8, drvMemcpy, drvMemDeviceOpen/Close)
#   - libc_sec.so      : secure C library
#   - -ldl             : dlopen for TsdOpen/TsdClose

add_executable(npu_nds_demo ${CMAKE_CURRENT_SOURCE_DIR}/../../demo/demo_main.c)

target_include_directories(npu_nds_demo PRIVATE
    ${BASE_DIR}/include
    ${BASE_DIR}/include/depends/libc_sec
    ${BASE_DIR}/examples
)

target_link_libraries(npu_nds_demo PRIVATE
    npu_file
    libascend_hal.so
    libc_sec.so
    -ldl
)
