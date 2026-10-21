# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

function(conftest_make_command)
    cmake_parse_arguments(CONFTEST
        "" "OUT_CMD_VAR;SCRIPT;KERNEL_PATH;ARCH;CROSS_COMPILE;OUTPUT_PATH" "" ${ARGN})

    set(_args
        --kernel-src "${CONFTEST_KERNEL_PATH}"
        --arch       "${CONFTEST_ARCH}"
        --output     "${CONFTEST_OUTPUT_PATH}")

    if(DEFINED CONFTEST_CROSS_COMPILE AND NOT CONFTEST_CROSS_COMPILE STREQUAL "")
        list(APPEND _args --cross-compile "${CONFTEST_CROSS_COMPILE}")
    endif()

    set(${CONFTEST_OUT_CMD_VAR} bash "${CONFTEST_SCRIPT}" ${_args} PARENT_SCOPE)
endfunction()
