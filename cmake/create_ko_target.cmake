# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

function(add_kernel_conftest)
    cmake_parse_arguments(CONFTEST "" "CONFTEST_SCRIPT;KERNEL_PATH;ARCH_TYPE;KERNEL_COMPILER_PREFIX;KERNEL_DEPENDS;TARGET_NAME" "" ${ARGN})

    if(NOT EXISTS ${CONFTEST_CONFTEST_SCRIPT})
        message(STATUS "CONFTEST_SCRIPT=${CONFTEST_CONFTEST_SCRIPT} not found, skip conftest")
        return()
    endif()

    if(NOT DEFINED CONFTEST_TARGET_NAME)
        set(CONFTEST_TARGET_NAME kernel_conftest)
    endif()

    get_filename_component(CONFTEST_PATH ${CONFTEST_CONFTEST_SCRIPT} DIRECTORY)
    include(${CONFTEST_PATH}/conftest.cmake)
    set(CONFTEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/kernel_adapt)
    set(CONFTEST_OUTPUT ${CONFTEST_OUTPUT_DIR}/conftest.h)

    file(GLOB_RECURSE CONFTEST_PROBE_DEPENDS CONFIGURE_DEPENDS
        ${CONFTEST_PATH}/probes/*.c)
    set(CONFTEST_INPUT_DEPENDS
        ${CONFTEST_CONFTEST_SCRIPT}
        ${CONFTEST_PATH}/conftest.cmake
        ${CONFTEST_PATH}/check_functions/check_tables.csv
        ${CONFTEST_PROBE_DEPENDS})
    foreach(KERNEL_INPUT Makefile .config include/config/auto.conf include/generated/autoconf.h)
        if(EXISTS ${CONFTEST_KERNEL_PATH}/${KERNEL_INPUT})
            list(APPEND CONFTEST_INPUT_DEPENDS ${CONFTEST_KERNEL_PATH}/${KERNEL_INPUT})
        endif()
    endforeach()

    conftest_make_command(
        OUT_CMD_VAR    CONFTEST_COMMAND
        SCRIPT         ${CONFTEST_CONFTEST_SCRIPT}
        KERNEL_PATH    ${CONFTEST_KERNEL_PATH}
        ARCH           ${CONFTEST_ARCH_TYPE}
        CROSS_COMPILE  ${CONFTEST_KERNEL_COMPILER_PREFIX}
        OUTPUT_PATH    ${CONFTEST_OUTPUT})

    if(NOT TARGET ${CONFTEST_TARGET_NAME})
        add_custom_command(
            OUTPUT ${CONFTEST_OUTPUT}
            BYPRODUCTS ${CONFTEST_OUTPUT}.log
            COMMAND ${CONFTEST_COMMAND}
            DEPENDS ${CONFTEST_INPUT_DEPENDS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM
        )
        add_custom_target(${CONFTEST_TARGET_NAME} DEPENDS ${CONFTEST_OUTPUT})
    endif()

    if(CONFTEST_KERNEL_DEPENDS)
        add_dependencies(${CONFTEST_TARGET_NAME} ${CONFTEST_KERNEL_DEPENDS})
    endif()
    set_target_properties(${CONFTEST_TARGET_NAME} PROPERTIES
        CONFTEST_OUTPUT ${CONFTEST_OUTPUT}
        CONFTEST_INCLUDE_DIR ${CONFTEST_OUTPUT_DIR})
endfunction()

function(add_host_ko)
    cmake_parse_arguments(HOST "" "LOCAL_MODULE;KO_SRC_FOLDER;RENAME;OUTPUT_NAME" "TARGETE_DPENDS;MAKE_ARGS" ${ARGN})
    set(host_os ${HOST_LINUX_DISTRIBUTOR_ID})
    set(KO_ARCH_TYPE ${CMAKE_HOST_SYSTEM_PROCESSOR})
    if( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
        set(SYS_ARCH arm64)
    elseif( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL x86_64)
        set(SYS_ARCH x86)
    else()
        set(SYS_ARCH ${KO_ARCH_TYPE})
    endif()
    set(HOST_KERNEL_WORKDIR ${CMAKE_BINARY_DIR}/linux_kernel)
    #打桩ko需要
    if(DEFINED HOST_RENAME)
        set(LOCAL_MODULE ${HOST_RENAME})
    else()
        set(LOCAL_MODULE ${HOST_LOCAL_MODULE})
    endif()
    if(NOT DEFINED HOST_OUTPUT_NAME)
        set(HOST_OUTPUT_NAME ${LOCAL_MODULE})
    endif()
    #生成ko存放路径
    get_filename_component(HOST_OUT_INTERMEDIATE "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)
    set(HOST_OUT_INTERMEDIATES ${HOST_OUT_INTERMEDIATE}/${PRODUCT}/${HOST_LOCAL_MODULE})
    set(MODULE_KO_DIR ${LOCAL_MODULE}_ko)
    # used for make option KBUILD_EXTRA_SYMBOLS
    list(REMOVE_DUPLICATES HOST_TARGETE_DPENDS)
    foreach(depends ${HOST_TARGETE_DPENDS})
        list(APPEND DEPENDS_SYMBOLS ${CMAKE_INSTALL_PREFIX}/${depends}_ko/Module.symvers)
    endforeach()
    set(HOST_KERNEL_COMPILER_PREFIX "")
    FILE(GLOB_RECURSE srclist "${HOST_KO_SRC_FOLDER}/*.*")
    list(APPEND srclist "${HOST_KO_SRC_FOLDER}/Makefile")

    if (HOST_LINUX_DISTRIBUTOR_ID STREQUAL "ubuntu")
        # EulerOS内核态热补丁制作工具kpatch-build不支持extra_flag选项中包含空格
        list(APPEND HOST_CFLAGS_MODULE "-isystem" "\\$$\\(shell \\$$\\(CC\\) -print-file-name=include\\)")
        list(JOIN HOST_CFLAGS_MODULE " " CFLAGS_MODULE)
        list(APPEND HOST_MAKE_ARGS
            CFLAGS_MODULE=${CFLAGS_MODULE}
        )
    endif()

    list(APPEND HOST_MAKE_ARGS
        FEATURE_MK_PATH=${FEATURE_MK_PATH}
    )

    string(TOUPPER ${HOST_LINUX_DISTRIBUTOR_ID}_${HOST_LINUX_DISTRIBUTOR_RELEASE}_${CMAKE_HOST_SYSTEM_PROCESSOR} KERNEL_ENV_INFO)

    # 使用环境上的kernel
    set(KERNEL_SYMVERS ${HOST_KERNEL_PATH}/Module.symvers)

    add_kernel_conftest(
        CONFTEST_SCRIPT ${PROJECT_SOURCE_DIR}/src/sdk_driver/kernel_adapt/conftest/conftest.sh
        KERNEL_PATH ${HOST_KERNEL_PATH}
        ARCH_TYPE ${SYS_ARCH}
        KERNEL_COMPILER_PREFIX ${HOST_KERNEL_COMPILER_PREFIX}
        TARGET_NAME host_kernel_conftest
    )
    get_target_property(HOST_CONFTEST_OUTPUT host_kernel_conftest CONFTEST_OUTPUT)
    get_target_property(HOST_CONFTEST_INCLUDE_DIR host_kernel_conftest CONFTEST_INCLUDE_DIR)

    add_custom_command(
        OUTPUT ${HOST_OUT_INTERMEDIATES}/src/${LOCAL_MODULE}.ko ${HOST_OUT_INTERMEDIATES}/src/Module.symvers
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HOST_OUT_INTERMEDIATES}/src ${CMAKE_INSTALL_PREFIX}/${MODULE_KO_DIR}
        COMMAND cp -lrf ${HOST_KO_SRC_FOLDER}/* ${HOST_OUT_INTERMEDIATES}/src/
        COMMAND ${MAKE_CMD} -C ${HOST_KERNEL_PATH} M=${HOST_OUT_INTERMEDIATES}/src CROSS_COMPILE=${HOST_KERNEL_COMPILER_PREFIX} ARCH=${SYS_ARCH}
                KCFLAGS+=-I${HOST_CONFTEST_INCLUDE_DIR}
                KBUILD_MODPOST_FAIL_ON_WARNINGS=1 KBUILD_EXTRA_SYMBOLS="${DEPENDS_SYMBOLS}" TOP_DIR=${TOP_DIR} ${HOST_MAKE_ARGS} PRODUCT=${PRODUCT}
                ENABLE_OPEN_SRC=${ENABLE_OPEN_SRC} ASCEND910_93_EX=${ASCEND910_93_EX} ENABLE_BUILD_PRODUCT=${ENABLE_BUILD_PRODUCT}
                ENABLE_UBE=${ENABLE_UBE} modules
        COMMAND ${HOST_KERNEL_COMPILER_PREFIX}strip -S --remove-section=.note.gnu.build-id ${HOST_OUT_INTERMEDIATES}/src/${LOCAL_MODULE}.ko
        COMMAND ${CMAKE_COMMAND} -E copy ${HOST_OUT_INTERMEDIATES}/src/Module.symvers ${CMAKE_INSTALL_PREFIX}/${MODULE_KO_DIR}
        DEPENDS ${srclist} ${HOST_CONFTEST_OUTPUT}
        COMMAND_EXPAND_LISTS
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target(${HOST_LOCAL_MODULE} ALL DEPENDS ${HOST_OUT_INTERMEDIATES}/src/${LOCAL_MODULE}.ko ${HOST_OUT_INTERMEDIATES}/src/Module.symvers)

    if(TARGET host_kernel_conftest)
        add_dependencies(${HOST_LOCAL_MODULE} host_kernel_conftest)
    endif()

    foreach(depends ${HOST_TARGETE_DPENDS})
        add_dependencies(${HOST_LOCAL_MODULE} ${depends})
    endforeach()

    INSTALL(FILES ${HOST_OUT_INTERMEDIATES}/src/Module.symvers
            DESTINATION ${CMAKE_INSTALL_PREFIX}/${MODULE_KO_DIR} OPTIONAL)
    INSTALL(FILES ${HOST_OUT_INTERMEDIATES}/src/${LOCAL_MODULE}.ko
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
            RENAME ${HOST_OUTPUT_NAME}.ko OPTIONAL)
endfunction(add_host_ko)
