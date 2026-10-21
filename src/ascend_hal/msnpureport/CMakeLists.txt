# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_subdirectory(adcore)
message(STATUS "msnpureport start")
get_filename_component(BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(reportSrcFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/msnpureport.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/msnpureport_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/msnpureport_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/config/msnpureport_level.c
    ${CMAKE_CURRENT_SOURCE_DIR}/config/msn_operate_log_level.c
    ${CMAKE_CURRENT_SOURCE_DIR}/config/msnpureport_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/report/msnpureport_report.c
    ${CMAKE_CURRENT_SOURCE_DIR}/report/msnpureport_file_mgr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/options/msnpureport_options.c
    ${CMAKE_CURRENT_SOURCE_DIR}/options/msnpureport_options_old.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/log_system_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/log_common.c
)

set(reportHeaderDir
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/adump
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/libc_sec
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/mmpa
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/slog
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/adump
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/bbox
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/options
    ${CMAKE_CURRENT_SOURCE_DIR}/report
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/msnpureport
)

add_executable(msnpureport ${reportSrcFiles})

target_include_directories(msnpureport PRIVATE ${reportHeaderDir})

target_compile_definitions(msnpureport PRIVATE
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:OS_TYPE_DEF=0>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:OS_TYPE_DEF=1>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:SECUREC_USING_STD_SECURE_LIB=0>
    # ascend950
    $<$<STREQUAL:${PRODUCT},ascend950>:UB_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:UB_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950>:AO_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:AO_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950>:CCU_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:CCU_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950>:NETWORK_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:NETWORK_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend950>:HDC_NEW_CHANNEL>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:HDC_NEW_CHANNEL>
    $<$<STREQUAL:${PRODUCT},ascend950>:USE_LOGIC_ID>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:USE_LOGIC_ID>
    # ascend960
    $<$<STREQUAL:${PRODUCT},ascend960>:HDC_NEW_CHANNEL>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:HDC_NEW_CHANNEL>
    $<$<STREQUAL:${PRODUCT},ascend960>:UB_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:UB_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960>:AO_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:AO_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960>:CCU_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:CCU_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960>:NETWORK_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:NETWORK_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend960>:USE_LOGIC_ID>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:USE_LOGIC_ID>
    # ascend350
    $<$<STREQUAL:${PRODUCT},ascend350>:UB_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend350>:HDC_NEW_CHANNEL>
    $<$<STREQUAL:${PRODUCT},ascend350>:USE_LOGIC_ID>
    $<$<STREQUAL:${PRODUCT},ascend350>:AO_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend350>:CCU_SUPPORT>
    $<$<STREQUAL:${PRODUCT},ascend350>:NETWORK_SUPPORT>
)

target_compile_options(msnpureport PRIVATE
    # Build type specific options
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-Os -D_FORTIFY_SOURCE=2>

    # Platform-specific options (non-Windows)
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Werror>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wfloat-equal -Wextra>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wunused>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wshadow>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wundef>

    # Common options
    -std=gnu11
    -fPIE
    -fstack-protector-strong
    -fvisibility=hidden
    -fdata-sections
    -ffunction-sections
    -Werror
    -Wall
)

target_link_options(msnpureport PRIVATE
    $<$<CONFIG:Release>:-Wl,--build-id=none>
    $<$<CONFIG:Release>:-s>
)

# target_link_options(msnpureport PRIVATE
#     $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wl,-z,relro,-z,now,-z,noexecstack>
#     -Wl,--gc-sections
# )

# Link libraries
target_link_libraries(msnpureport PRIVATE
    # Build interface libraries
    # $<BUILD_INTERFACE:intf_pub>
    # $<BUILD_INTERFACE:mmpa_headers>
    # $<BUILD_INTERFACE:slog_headers>

    # Core libraries
    mmpa
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${PRODUCT_SIDE},host>>:drvdsmi_host>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950esl>,$<STREQUAL:${PRODUCT_SIDE},host>>:drvdsmi_host>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960>,$<STREQUAL:${PRODUCT_SIDE},host>>:drvdsmi_host>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960esl>,$<STREQUAL:${PRODUCT_SIDE},host>>:drvdsmi_host>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend350>,$<STREQUAL:${PRODUCT_SIDE},host>>:drvdsmi_host>
    c_sec
    msn_adcore
    bbox_dump

    ascend_hal 
    # Platform-specific libraries
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>:dl>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>:rt>
    -pthread
)

### INTERNAL START ###
# Conditionally link URMA libraries for specific configurations
target_link_libraries(msnpureport PRIVATE
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910_55>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910_55>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend910_55esl>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910_55esl>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend950esl>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950esl>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend960>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend960esl>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960esl>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend350>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend350>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:urma_common>
)

target_link_directories(msnpureport PRIVATE
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910_55>,$<STREQUAL:${PRODUCT_SIDE},host>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910_55esl>,$<STREQUAL:${PRODUCT_SIDE},host>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${PRODUCT_SIDE},host>,$<STREQUAL:${ENABLE_UBE},true>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950esl>,$<STREQUAL:${PRODUCT_SIDE},host>,$<STREQUAL:${ENABLE_UBE},true>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960>,$<STREQUAL:${PRODUCT_SIDE},host>,$<STREQUAL:${ENABLE_UBE},true>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960esl>,$<STREQUAL:${PRODUCT_SIDE},host>,$<STREQUAL:${ENABLE_UBE},true>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend350>,$<STREQUAL:${PRODUCT_SIDE},host>,$<STREQUAL:${ENABLE_UBE},true>>:${TOP_DIR}/build/bin/ccec_for_milan/lib/urma>
)
### INTERNAL END ###

set_target_properties(msnpureport
    PROPERTIES
    OUTPUT_NAME msnpureport
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
)

set(INSTALL_BASE_DIR "")
set(INSTALL_INCLUDE_DIR include)
set(INSTALL_LIBRARY_DIR lib)
set(INSTALL_RUNTIME_DIR bin)
set(INSTALL_CONFIG_DIR lib/cmake)

include(CMakePackageConfigHelpers)

install(TARGETS msnpureport
    RUNTIME DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)

set(msnpureport_auto_export
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/msnpureport_auto_export.sh
)

install(PROGRAMS ${msnpureport_auto_export}
    DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)
