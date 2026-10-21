# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(urd_obj OBJECT)

target_sources(urd_obj PRIVATE
    ${MODULE_SRC_DIR}/urd_user.c
    urd_user_adapt.c
)
target_include_directories(urd_obj PRIVATE
    ${DRIVER_HAL_INC_DIR}/
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/mmpa_sdk
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl
)

target_link_libraries(urd_obj PRIVATE
    $<BUILD_INTERFACE:c_sec_headers>
    $<BUILD_INTERFACE:intf_pub>
    -ldl
    -Wl,--Bsymbolic
)
target_compile_definitions(urd_obj PRIVATE
    DRV_HOST
    $<IF:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,OS_TYPE=WIN,OS_TYPE=0>
    CFG_FEATURE_MODULE_CHECK
)

target_compile_options(urd_obj PRIVATE
    -fno-common
    -funsigned-char
    -Werror
    -Wformat=2
    $<$<VERSION_GREATER:${CMAKE_C_COMPILER_VERSION},4.8.5>: -Wdate-time>
    -Wfloat-equal
    -Wswitch-default
    -Wshadow
    -Wframe-larger-than=2048
    -Wvla
    -Wunused
    -Wundef
    -Wextra
    -fno-common
    -Wstrict-prototypes
    -Wcast-align
    -Wcast-qual
    -Wno-strict-prototypes
    -Wno-cast-align
    -Wno-cast-qual
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-missing-braces
)
