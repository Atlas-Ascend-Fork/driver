# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

message("Build third party library rdma-core")
set(RDMA_CORE_NAME "rdma-core")
set(RDMA_CORE_BUILD_PATH "${CMAKE_BINARY_DIR}")
set(RDMA_CORE_SEARCH_PATHS "${CANN_3RD_LIB_PATH}/open_source/${RDMA_CORE_NAME}")
set(RDMA_CORE_INSTALL_PATHS "${CMAKE_CURRENT_BINARY_DIR}/${RDMA_CORE_NAME}/build")
set(RDMA_CORE_SRC_DIR ${RDMA_CORE_BUILD_PATH}/rdma-core)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

find_package(Python3 REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())"
    OUTPUT_VARIABLE GET_PYTHON_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import distutils.sysconfig as sysconfig; print(sysconfig.get_config_var('LDLIBRARY'))"
    OUTPUT_VARIABLE GET_PYTHON_LIBRARY
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message("Python include directory. ${GET_PYTHON_INCLUDE_DIR}")
message("Python library. ${GET_PYTHON_LIBRARY}")

if(EXISTS ${RDMA_CORE_SEARCH_PATHS})
    file(COPY ${RDMA_CORE_SEARCH_PATHS} DESTINATION ${RDMA_CORE_BUILD_PATH})
    message(STATUS "Successfully copied ${RDMA_CORE_SEARCH_PATHS} to ${RDMA_CORE_BUILD_PATH}.")
else()
    if (EXISTS "${CANN_3RD_LIB_PATH}/rdma-core-42.7.tar.gz")
        set(RDMA_CORE_URL "file://${CANN_3RD_LIB_PATH}/rdma-core-42.7.tar.gz")
    else()
        set(RDMA_CORE_URL "https://cann-3rd.obs.cn-north-4.myhuaweicloud.com/rdma-core/rdma-core-42.7.tar.gz")
    endif()

    if (EXISTS "${CANN_3RD_LIB_PATH}/rdma-core-42.7.patch")
        set(RDMA_CORE_PATCH_URL "file://${CANN_3RD_LIB_PATH}/rdma-core-42.7.patch")
    else()
        set(RDMA_CORE_PATCH_URL "https://gitcode.com/cann-src-third-party/rdma-core/releases/download/\
v42.7-h3/rdma-core-42.7.patch")
    endif()

    file(DOWNLOAD ${RDMA_CORE_PATCH_URL} ${EXTERN_DEPEND_DOWNLOAD_DIR}/rdma-core-42.7.patch
        EXPECTED_HASH SHA256=7ed858a480fbf7828d15a2b4c9aedb49cee0569eb05044651951f8ef49e2ed79)
    set(RDMA_CORE_DOWNLOAD_COMMAND
        URL ${RDMA_CORE_URL}
        URL_HASH SHA256=aa935de1fcd07c42f7237b0284b5697b1ace2a64f2bcfca3893185bc91b8c74d
        DOWNLOAD_DIR ${RDMA_CORE_SRC_DIR}
        PATCH_COMMAND patch -p1 < ${EXTERN_DEPEND_DOWNLOAD_DIR}/rdma-core-42.7.patch
    )

    message(STATUS "downloading ${RDMA_CORE_URL} to ${RDMA_CORE_SRC_DIR}")

endif()

set(SECURITY_COMPILE_OPT "-Wl,-z,relro,-z,now -fstack-protector-all -O2")
set(EXT_COMPILE_OPT "-Wno-switch -Wno-enum-compare -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-nested-externs -Wno-maybe-uninitialized -Wno-missing-prototypes")
set(COMPILE_OPT "${SECURITY_COMPILE_OPT} ${EXT_COMPILE_OPT}")
ExternalProject_Add(${RDMA_CORE_NAME}
    ${RDMA_CORE_DOWNLOAD_COMMAND}
    SOURCE_DIR ${RDMA_CORE_SRC_DIR}
    CONFIGURE_COMMAND ${CMAKE_COMMAND}
                    -DNO_MAN_PAGES=1
                    -DENABLE_RESOLVE_NEIGH=0
                    -DCMAKE_C_FLAGS=${COMPILE_OPT}
                    -DCMAKE_INSTALL_LIBDIR=lib
                    -DCMAKE_SKIP_RPATH=True
                    -DPYTHON_INCLUDE_DIR=${GET_PYTHON_INCLUDE_DIR}
                    -DPYTHON_LIBRARY=${GET_PYTHON_LIBRARY}
                    -DNO_PYVERBS=1
                    <SOURCE_DIR>
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND mkdir -p ${RDMA_CORE_INSTALL_PATHS}
                            COMMAND cp -rf include ${RDMA_CORE_INSTALL_PATHS}
                            COMMAND cp -rf lib ${RDMA_CORE_INSTALL_PATHS}
                        EXCLUDE_FROM_ALL True
)

ExternalProject_Add_Step(${RDMA_CORE_NAME} extra_install
    COMMAND cp -fr ${RDMA_CORE_INSTALL_PATHS}/lib/libibverbs.so.1.14.42.7 ${CMAKE_BINARY_DIR}/lib/libibverbs.so.1
    COMMAND cp -fr ${RDMA_CORE_INSTALL_PATHS}/lib/libibumad.so.3.2.42.7  ${CMAKE_BINARY_DIR}/lib/libibumad.so.3
    COMMAND cp -fr ${RDMA_CORE_INSTALL_PATHS}/lib/librdmacm.so.1.3.42.7  ${CMAKE_BINARY_DIR}/lib/librdmacm.so.1
    COMMAND cp -f ${RDMA_CORE_INSTALL_PATHS}/lib/libhns-rdmav34.so  ${CMAKE_BINARY_DIR}/lib/libhns-rdmav34.so
    COMMAND cp -f ${RDMA_CORE_INSTALL_PATHS}/lib/libhns-rdmav34.so  ${CMAKE_BINARY_DIR}/lib/libhns-rdmav25.so
    DEPENDEES install
)

ExternalProject_Add_Step(${RDMA_CORE_NAME} strip_libs
    COMMAND ${CMAKE_STRIP} ${RDMA_CORE_INSTALL_PATHS}/lib/libibverbs.so.1.14.42.7
    COMMAND ${CMAKE_STRIP} ${RDMA_CORE_INSTALL_PATHS}/lib/libibumad.so.3.2.42.7
    COMMAND ${CMAKE_STRIP} ${RDMA_CORE_INSTALL_PATHS}/lib/librdmacm.so.1.3.42.7
    COMMAND ${CMAKE_STRIP} ${RDMA_CORE_INSTALL_PATHS}/lib/libhns-rdmav34.so
    DEPENDEES install
    DEPENDERS extra_install
)

set(RDMA_CORE_INCLUDE
    ${RDMA_CORE_SRC_DIR}/build/include
)
