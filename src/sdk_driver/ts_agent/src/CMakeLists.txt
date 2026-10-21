# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
set(PRODUCTS_LIST "ascend950;ascend950esl;ascend960;ascend960esl")
if(NOT PRODUCT IN_LIST PRODUCTS_LIST)
    set(ts_agent_vm_depends asdrv_dms drv_seclib_host asdrv_vvmng asdrv_svm asdrv_vtrs asdrv_pbl ascend_kernel_open_adapt)
    add_host_ko(LOCAL_MODULE ts_agent_vm
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${ts_agent_vm_depends}")
endif()
