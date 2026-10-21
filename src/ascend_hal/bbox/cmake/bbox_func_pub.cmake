# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
#### translate chip type to product name ####
macro(get_product_name prod_name)
    if(NOT PRODUCT)
        message(FATAL_ERROR "PRODUCT is not set.")
    elseif(${PRODUCT} STREQUAL "ascend910B")
        set(${prod_name} "milan")
    elseif(${PRODUCT} STREQUAL "ascend950" OR ${PRODUCT} STREQUAL "ascend950esl" OR ${PRODUCT} STREQUAL "ascend910_55" OR ${PRODUCT} STREQUAL "ascend910_55esl" OR ${PRODUCT} STREQUAL "ascend960" OR ${PRODUCT} STREQUAL "ascend960esl")
        set(${prod_name} "david")
    elseif(${PRODUCT} STREQUAL "ascend310B" OR ${PRODUCT} STREQUAL "ascend310Besl" OR ${PRODUCT} STREQUAL "ascend310Bemu")
        set(${prod_name} "milanr3")
    elseif(${PRODUCT} STREQUAL "ascend310Brc" OR ${PRODUCT} STREQUAL "ascend310Brcesl" OR ${PRODUCT} STREQUAL "ascend310Brcemu")
        set(${prod_name} "milanr3")
    else()
        message(FATAL_ERROR "Unknown PRODUCT ${PRODUCT}.")
    endif()
endmacro()
