# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
#
# File Name     : CMakeLists.txt
# Description   : Build configuration for adp_mlx driver library

cmake_minimum_required(VERSION 3.18)
project(adp_mlx VERSION 1.0.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)
option(ADP_MLX_SANITIZERS "Enable address/undefined behavior sanitizers" OFF)
set(IBV_EXTEND_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ibv_extend")
set(ADP_MLX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include(cmake/RdmaDependencies.cmake)
nda_use_installed_rdma()

# mlx5.h includes the environment's Valgrind headers even with requests disabled.
find_path(NDA_VALGRIND_INCLUDE_DIR NAMES valgrind/memcheck.h
    DOC "Directory containing installed Valgrind development headers")
if(NOT EXISTS "${NDA_VALGRIND_INCLUDE_DIR}/valgrind/memcheck.h"
   OR NOT EXISTS "${NDA_VALGRIND_INCLUDE_DIR}/valgrind/valgrind.h")
    message(FATAL_ERROR "Valgrind development headers missing. Install valgrind-devel (TencentOS/RHEL), or set NDA_VALGRIND_INCLUDE_DIR to the directory containing valgrind/memcheck.h and valgrind/valgrind.h.")
endif()

# Compile the actual provider headers, including their command-layout assertions.
# Presence alone is insufficient for older system UAPI packages. Recheck on each
# configure so an updated header package cannot reuse a stale successful result.
include(CheckCSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${NDA_PRIVATE_INCLUDES} "${RDMA_INCLUDE}" "${NDA_VALGRIND_INCLUDE_DIR}")
set(CMAKE_REQUIRED_DEFINITIONS -DNVALGRIND)
unset(NDA_RDMA_UAPI_COMPATIBLE CACHE)
check_c_source_compiles([=[
#include <mlx5.h>
#include <infiniband/cmd_ioctl.h>
/* The command wrappers use unions: their padding assertions alone cannot
 * detect an environment structure that grows beyond the pinned payload. */
#define CHECK_UAPI(type) _Static_assert(sizeof(struct type) == sizeof(_STRUCT_##type) && _Alignof(struct type) == _Alignof(_STRUCT_##type), "RDMA UAPI layout mismatch: " #type)
CHECK_UAPI(ib_uverbs_ex_create_cq);
CHECK_UAPI(ib_uverbs_create_cq_resp);
CHECK_UAPI(ib_uverbs_ex_create_cq_resp);
CHECK_UAPI(ib_uverbs_create_srq);
CHECK_UAPI(ib_uverbs_create_srq_resp);
CHECK_UAPI(mlx5_ib_create_cq);
CHECK_UAPI(mlx5_ib_create_cq_resp);
CHECK_UAPI(mlx5_ib_create_srq);
CHECK_UAPI(mlx5_ib_create_srq_resp);
int main(void) { return 0; }
]=] NDA_RDMA_UAPI_COMPATIBLE)
cmake_pop_check_state()
if(NOT NDA_RDMA_UAPI_COMPATIBLE)
    message(FATAL_ERROR "Selected RDMA UAPI headers are incompatible with the 2410 Provider headers. See CMakeFiles/CMakeConfigureLog.yaml (or CMakeError.log) for missing definitions/layout assertions. Use compatible userspace kernel-headers, optionally selected with NDA_RDMA_UAPI_INCLUDE_DIR; do not use the kernel build's internal headers.")
endif()

set(ADP_MLX_BACKEND_SOURCES adp_mlx_backend.c adp_mlx_memory.c adp_mlx_cq_42.c adp_mlx_srq_42.c)
# Register one mlx5 provider for NDA resources and V2 UDP source port control.
set(ADP_MLX_SOURCES adp_mlx.c adp_mlx_nda.c ${ADP_MLX_BACKEND_SOURCES})
list(TRANSFORM ADP_MLX_SOURCES PREPEND "${ADP_MLX_SOURCE_DIR}/")
function(configure_adp_target target)
    target_include_directories(${target} PRIVATE "${IBV_EXTEND_SRC_DIR}" "${ADP_MLX_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories(${target} SYSTEM PRIVATE ${NDA_PRIVATE_INCLUDES} "${RDMA_INCLUDE}"
        "${NDA_VALGRIND_INCLUDE_DIR}")
    target_compile_definitions(${target} PRIVATE NVALGRIND)
    target_link_libraries(${target} PRIVATE "${IBVERBS_LIB}" "${MLX5_LIB}" Threads::Threads)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fvisibility=hidden)
        if(ADP_MLX_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(adp_mlx SHARED ${ADP_MLX_SOURCES})
configure_adp_target(adp_mlx)
set_target_properties(adp_mlx PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
if(IBV_EXTEND_BUILD_DIR)
    unset(IBV_EXTEND_LIB CACHE)
    find_library(IBV_EXTEND_LIB ibv_extend PATHS "${IBV_EXTEND_BUILD_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(adp_mlx PRIVATE "${IBV_EXTEND_LIB}")
endif()

install(TARGETS adp_mlx LIBRARY DESTINATION lib COMPONENT Runtime)
install(FILES adp_mlx.conf DESTINATION etc/libibverbs_extend.d COMPONENT Configuration)
