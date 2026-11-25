# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# Shared by the framework and Provider. No network/package-manager side effects.
# Keep the reviewed Linux x86_64 configuration local to header generation.
# The Provider's own package version and install directories are independent.
function(nda_generate_rdma_config source_dir output)
    set(nda_template "${source_dir}/buildlib/config.h.in")
    if(NOT EXISTS "${nda_template}")
        message(FATAL_ERROR "RDMA config template missing: ${nda_template}. Set NDA_RDMA_SOURCE_DIR to the complete extracted rdma-core-2410mlnx54 source tree.")
    endif()
    set(PACKAGE_VERSION "54.0")
    set(IBVERBS_PABI_VERSION 34)
    set(IBVERBS_PROVIDER_SUFFIX "-rdmav34.so")
    set(SIZEOF_LONG_CODE "#define SIZEOF_LONG 8")
    set(IOCTL_MODE_NUM 3)
    set(HAVE_FUNC_ATTRIBUTE_ALWAYS_INLINE 1)
    set(HAVE_FUNC_ATTRIBUTE_IFUNC 1)
    set(HAVE_FUNC_ATTRIBUTE_SYMVER 1)
    set(HAVE_WORKING_IF_H 0)
    set(HAVE_FULL_SYMBOL_VERSIONS 1)
    set(HAVE_LIMITED_SYMBOL_VERSIONS 0)
    configure_file("${nda_template}" "${output}" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${output}")
endfunction()

# Expose the selected external headers under the include names used by NDA.
function(nda_prepare_private_headers source_dir include_dir)
    set(nda_private_headers
        "libibverbs/cmd_ioctl.h|infiniband/cmd_ioctl.h"
        "libibverbs/driver.h|infiniband/driver.h"
        "libibverbs/kern-abi.h|infiniband/kern-abi.h"
        "providers/mlx5/mlx5-abi.h|mlx5-abi.h"
        "providers/mlx5/mlx5.h|mlx5.h"
        "util/bitmap.h|util/bitmap.h"
        "util/cl_qmap.h|util/cl_qmap.h"
        "util/compiler.h|util/compiler.h"
        "util/udma_barrier.h|util/udma_barrier.h"
        "util/util.h|util/util.h")
    file(GLOB_RECURSE nda_unexpected_headers RELATIVE "${include_dir}" "${include_dir}/*.h")
    foreach(nda_entry IN LISTS nda_private_headers)
        string(REPLACE "|" ";" nda_paths "${nda_entry}")
        list(GET nda_paths 0 nda_source_name)
        list(GET nda_paths 1 nda_include_name)
        if(NOT EXISTS "${source_dir}/${nda_source_name}")
            message(FATAL_ERROR "RDMA private header missing: ${source_dir}/${nda_source_name}")
        endif()
        get_filename_component(nda_source "${source_dir}/${nda_source_name}" REALPATH)
        set(nda_link "${include_dir}/${nda_include_name}")
        get_filename_component(nda_link_dir "${nda_link}" DIRECTORY)
        file(MAKE_DIRECTORY "${nda_link_dir}")
        set(nda_link_target "")
        if(IS_SYMLINK "${nda_link}")
            file(READ_SYMLINK "${nda_link}" nda_link_target)
        endif()
        if(NOT nda_link_target STREQUAL nda_source)
            file(CREATE_LINK "${nda_source}" "${nda_link}" SYMBOLIC)
        endif()
        list(REMOVE_ITEM nda_unexpected_headers "${nda_include_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${nda_source}" "${nda_link}")
    endforeach()
    if(nda_unexpected_headers)
        message(FATAL_ERROR "Unexpected private headers in ${include_dir}: ${nda_unexpected_headers}. Use a fresh build directory.")
    endif()
endfunction()

# Generate with rdma-core's script without replacing valid output on failure.
function(nda_generate_abi_header generator input output)
    if(NOT EXISTS "${generator}")
        message(FATAL_ERROR "RDMA ABI generator missing: ${generator}")
    endif()
    get_filename_component(nda_output_dir "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${nda_output_dir}")
    set(nda_temporary "${output}.tmp")
    file(REMOVE "${nda_temporary}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${generator}" "${input}" "${nda_temporary}"
        RESULT_VARIABLE nda_generate_result OUTPUT_VARIABLE nda_generate_output ERROR_VARIABLE nda_generate_error)
    if(NOT nda_generate_result STREQUAL "0")
        file(REMOVE "${nda_temporary}")
        message(FATAL_ERROR "External RDMA ABI generator failed (exit ${nda_generate_result}): ${nda_generate_error}${nda_generate_output}")
    endif()
    if(NOT EXISTS "${nda_temporary}")
        message(FATAL_ERROR "RDMA ABI generator produced no output: ${input}")
    endif()
    file(READ "${nda_temporary}" nda_content)
    string(STRIP "${nda_content}" nda_content)
    if(nda_content STREQUAL "" OR nda_content MATCHES [[\\$]])
        file(REMOVE "${nda_temporary}")
        message(FATAL_ERROR "Incomplete or empty UAPI structures: ${input}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${nda_temporary}" "${output}"
        RESULT_VARIABLE nda_copy_result ERROR_VARIABLE nda_copy_error)
    file(REMOVE "${nda_temporary}")
    if(NOT nda_copy_result STREQUAL "0")
        message(FATAL_ERROR "Cannot publish RDMA ABI header ${output}: ${nda_copy_error}")
    endif()
endfunction()

macro(nda_use_installed_rdma)
    # Accept existing -DNDA_DEPENDENCY_MODE=INSTALLED commands, but reject old
    # source-build caches/options instead of silently selecting system libraries.
    set(NDA_DEPENDENCY_MODE "INSTALLED" CACHE STRING "Installed RDMA packages only")
    set_property(CACHE NDA_DEPENDENCY_MODE PROPERTY STRINGS INSTALLED)
    if(NOT NDA_DEPENDENCY_MODE STREQUAL "INSTALLED")
        message(FATAL_ERROR "Only NDA_DEPENDENCY_MODE=INSTALLED is supported. Use a fresh build directory and NDA_RDMA_PREFIX for installed/extracted packages.")
    endif()
    foreach(nda_source_option LIBIBVERBS_BUILD_DIR LIBIBVERBS_SOURCE_DIR ADP_MLX_RDMA_SOURCE_DIR LIBBOUNDSCHECK_SOURCE_DIR)
        if(DEFINED ${nda_source_option} AND NOT "${${nda_source_option}}" STREQUAL "")
            message(FATAL_ERROR "${nda_source_option} is no longer supported. Use a fresh build directory, NDA_RDMA_PREFIX and NDA_SECUREC_INCLUDE_DIR/NDA_SECUREC_LIBRARY.")
        endif()
    endforeach()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
       OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        message(FATAL_ERROR "The 2410 RDMA dependencies currently support Linux x86_64 only")
    endif()
    find_package(Python3 3.6 COMPONENTS Interpreter REQUIRED)
    set(NDA_RDMA_PREFIX "" CACHE PATH "Installed RDMA prefix or extracted RPM usr directory")
    if(NDA_RDMA_PREFIX)
        find_path(NDA_RDMA_INCLUDE_DIR infiniband/verbs.h PATHS "${NDA_RDMA_PREFIX}/include" NO_DEFAULT_PATH REQUIRED)
        find_library(NDA_IBVERBS_LIBRARY ibverbs PATHS "${NDA_RDMA_PREFIX}/lib64" "${NDA_RDMA_PREFIX}/lib"
            "${NDA_RDMA_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}" NO_DEFAULT_PATH REQUIRED)
        find_library(NDA_MLX5_LIBRARY mlx5 PATHS "${NDA_RDMA_PREFIX}/lib64" "${NDA_RDMA_PREFIX}/lib"
            "${NDA_RDMA_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}" NO_DEFAULT_PATH REQUIRED)
    else()
        find_path(NDA_RDMA_INCLUDE_DIR infiniband/verbs.h REQUIRED)
        find_library(NDA_IBVERBS_LIBRARY ibverbs REQUIRED)
        find_library(NDA_MLX5_LIBRARY mlx5 REQUIRED)
    endif()
    # Source headers supplement installed libraries; no rdma-core build is run.
    set(NDA_RDMA_SOURCE_DIR "$ENV{NDA_RDMA_SOURCE_DIR}" CACHE PATH
        "Extracted rdma-core-2410mlnx54 source directory for private headers")
    if(NOT NDA_RDMA_SOURCE_DIR)
        message(FATAL_ERROR "RDMA private source headers missing. Set NDA_RDMA_SOURCE_DIR to the extracted rdma-core-2410mlnx54 directory containing libibverbs/, providers/mlx5/, util/ and ccan/.")
    endif()
    message(STATUS "RDMA private source headers: ${NDA_RDMA_SOURCE_DIR}")
    set(NDA_PRIVATE_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/rdma-private/include")
    nda_prepare_private_headers("${NDA_RDMA_SOURCE_DIR}" "${NDA_PRIVATE_INCLUDE}")
    # The external source tree supplies CCAN headers; generated config.h stays first.
    find_path(NDA_CCAN_INCLUDE_DIR NAMES ccan/list.h
        HINTS ENV NDA_CCAN_INCLUDE_DIR "${NDA_RDMA_SOURCE_DIR}"
        DOC "Directory containing ccan/*.h from rdma-core-2410mlnx54")
    if(NOT NDA_CCAN_INCLUDE_DIR)
        message(FATAL_ERROR "CCAN headers missing. Extract rdma-core-2410mlnx54-1.2410068.src.rpm and its source tarball, then set NDA_CCAN_INCLUDE_DIR to the directory containing ccan/list.h (not the ccan subdirectory).")
    endif()
    message(STATUS "CCAN headers: ${NDA_CCAN_INCLUDE_DIR}")
    # Use exported RDMA UAPI headers; build.sh defaults to the selected source tree.
    find_path(NDA_RDMA_UAPI_INCLUDE_DIR NAMES rdma/ib_user_verbs.h
        HINTS "${NDA_RDMA_INCLUDE_DIR}"
        DOC "Directory containing exported userspace rdma/ UAPI headers")
    foreach(nda_uapi_header ib_user_verbs.h ib_user_ioctl_verbs.h mlx5-abi.h
                            mlx5_user_ioctl_verbs.h rdma_user_ioctl_cmds.h)
        if(NOT EXISTS "${NDA_RDMA_UAPI_INCLUDE_DIR}/rdma/${nda_uapi_header}")
            message(FATAL_ERROR "RDMA UAPI header missing: rdma/${nda_uapi_header}. Install compatible userspace kernel-headers, or set NDA_RDMA_UAPI_INCLUDE_DIR to their include directory. rdma-core-devel alone does not supply these headers.")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${NDA_RDMA_UAPI_INCLUDE_DIR}/rdma/${nda_uapi_header}")
    endforeach()
    message(STATUS "RDMA UAPI headers: ${NDA_RDMA_UAPI_INCLUDE_DIR}")

    set(NDA_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
    nda_generate_rdma_config("${NDA_RDMA_SOURCE_DIR}" "${NDA_GENERATED_INCLUDE}/config.h")
    set(NDA_RDMA_ABI_GENERATOR "${NDA_RDMA_SOURCE_DIR}/buildlib/make_abi_structs.py")
    foreach(nda_abi_header ib_user_verbs.h mlx5-abi.h)
        nda_generate_abi_header("${NDA_RDMA_ABI_GENERATOR}"
            "${NDA_RDMA_UAPI_INCLUDE_DIR}/rdma/${nda_abi_header}"
            "${NDA_GENERATED_INCLUDE}/kernel-abi/${nda_abi_header}")
        # Reconfigure on input changes or deletion/modification of a generated file.
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${NDA_GENERATED_INCLUDE}/kernel-abi/${nda_abi_header}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${NDA_RDMA_ABI_GENERATOR}")
    set(RDMA_INCLUDE "${NDA_RDMA_INCLUDE_DIR}")
    set(IBVERBS_LIB "${NDA_IBVERBS_LIBRARY}")
    set(MLX5_LIB "${NDA_MLX5_LIBRARY}")
    set(NDA_PRIVATE_INCLUDES "${NDA_GENERATED_INCLUDE}" "${NDA_PRIVATE_INCLUDE}" "${RDMA_INCLUDE}/infiniband"
        "${NDA_RDMA_UAPI_INCLUDE_DIR}" "${NDA_CCAN_INCLUDE_DIR}")
endmacro()

# The original framework loads this module through CMAKE_PROJECT_ibv_extend_INCLUDE.
if(PROJECT_NAME STREQUAL "ibv_extend")
    nda_use_installed_rdma()
    find_package(Threads REQUIRED)

    set(nda_securec_include_hints "$ENV{ASCEND_HOME_PATH}/include" "$ENV{ASCEND_TOOLKIT_HOME}/include")
    set(nda_securec_library_hints "$ENV{ASCEND_HOME_PATH}/lib64" "$ENV{ASCEND_TOOLKIT_HOME}/lib64")
    if(LIBBOUNDSCHECK_BUILD_DIR)
        list(PREPEND nda_securec_include_hints "${LIBBOUNDSCHECK_BUILD_DIR}/include")
        list(PREPEND nda_securec_library_hints "${LIBBOUNDSCHECK_BUILD_DIR}/lib")
    endif()
    find_path(NDA_SECUREC_INCLUDE_DIR securec.h HINTS ${nda_securec_include_hints})
    find_library(NDA_SECUREC_LIBRARY NAMES c_sec boundscheck HINTS ${nda_securec_library_hints})
    if(NOT EXISTS "${NDA_SECUREC_INCLUDE_DIR}/securec.h"
       OR NOT EXISTS "${NDA_SECUREC_LIBRARY}" OR IS_DIRECTORY "${NDA_SECUREC_LIBRARY}")
        message(FATAL_ERROR "Installed securec dependency missing. Set NDA_SECUREC_INCLUDE_DIR and NDA_SECUREC_LIBRARY (full .so path), or LIBBOUNDSCHECK_BUILD_DIR to an existing build.")
    endif()

    # Adapt installed include/lib64 layouts to the original CMake's include/lib inputs.
    # These normal variables are intentionally not persisted as source-build cache options.
    set(LIBIBVERBS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/dependencies/rdma")
    set(LIBBOUNDSCHECK_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/dependencies/securec")
    file(MAKE_DIRECTORY "${LIBIBVERBS_BUILD_DIR}/lib" "${LIBBOUNDSCHECK_BUILD_DIR}/lib")
    file(CREATE_LINK "${RDMA_INCLUDE}" "${LIBIBVERBS_BUILD_DIR}/include" SYMBOLIC)
    file(CREATE_LINK "${IBVERBS_LIB}" "${LIBIBVERBS_BUILD_DIR}/lib/libibverbs.so" SYMBOLIC)
    file(CREATE_LINK "${NDA_SECUREC_INCLUDE_DIR}" "${LIBBOUNDSCHECK_BUILD_DIR}/include" SYMBOLIC)
    file(CREATE_LINK "${NDA_SECUREC_LIBRARY}" "${LIBBOUNDSCHECK_BUILD_DIR}/lib/libc_sec.so" SYMBOLIC)
    include_directories(SYSTEM ${NDA_PRIVATE_INCLUDES})
    link_libraries(Threads::Threads ${CMAKE_DL_LIBS})
    get_filename_component(nda_ibverbs_library_dir "${IBVERBS_LIB}" DIRECTORY)
    get_filename_component(nda_securec_library_dir "${NDA_SECUREC_LIBRARY}" DIRECTORY)
    list(APPEND CMAKE_BUILD_RPATH "${nda_ibverbs_library_dir}" "${nda_securec_library_dir}")

    # Both projects use CONFIG_H_IN and VERBS_PROVIDER_SUFFIX. Load the RDMA
    # feature macros first, then retain the framework's own provider lookup settings.
    set(nda_framework_config "${CMAKE_CURRENT_BINARY_DIR}/generated/framework_config.h")
    file(CONFIGURE OUTPUT "${nda_framework_config}" CONTENT
    "#include \"${NDA_GENERATED_INCLUDE}/config.h\"
    #undef CONFIG_H_IN
    #undef VERBS_PROVIDER_SUFFIX
    #include \"${CMAKE_CURRENT_BINARY_DIR}/include/config.h\"
    " @ONLY)
    add_compile_options(-include "${nda_framework_config}")
endif()
