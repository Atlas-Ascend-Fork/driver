#
# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------


#
# The code snippet comes from Ascend project
#
# Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
cmake_minimum_required(VERSION 3.14.1)

set(MMPA_DIR ${CMAKE_CURRENT_LIST_DIR})

add_library(mmpa_headers INTERFACE)
target_include_directories(mmpa_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../inc>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../inc/mmpa>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../inc/mmpa/sub_inc>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/mmpa>
    $<INSTALL_INTERFACE:include/mmpa/sub_inc>
)

add_library(mmpa SHARED
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_thread.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_file.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_time.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_atom.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_dlfcn.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_memory.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_message.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_option.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_env.c>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_thread.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_file.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_time.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_atom.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_dlfcn.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_memory.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_message.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_option.cpp>
)
target_include_directories(mmpa PRIVATE
    ${TOP_DIR}/inc
)

target_link_options(mmpa PRIVATE
        $<$<CONFIG:Release>:-Wl,--build-id=none>
        $<$<CONFIG:Release>:-s>
        $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:-Wl,-z,relro,-z,now>
)

target_link_libraries(mmpa
    PRIVATE
        $<BUILD_INTERFACE:c_sec_headers>
        c_sec
        "$<$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>:dl;rt;pthread>"
    PUBLIC
        mmpa_headers
)
target_compile_options(mmpa PRIVATE
    -fPIC -pipe -Wall
    -fstack-protector-strong
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>:-fvisibility=hidden>
)
target_compile_definitions(mmpa PRIVATE
    $<IF:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,OS_TYPE=WIN,OS_TYPE=0>
    LINUX=0
    FUNC_VISIBILITY
    $<$<STREQUAL:${PRODUCT},ascend035>:STRERROR_R_RETURN_INT>
    $<$<STREQUAL:${PRODUCT},ascend031>:STRERROR_R_RETURN_INT>
)
if (WIN32)
    set_target_properties(mmpa PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
        OUTPUT_NAME libmmpa
    )
endif()


add_library(static_mmpa  STATIC
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_thread.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_file.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_time.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_atom.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_dlfcn.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_memory.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_message.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_option.c>
    $<$<OR:$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>,$<STREQUAL:${TARGET_SYSTEM_NAME},Android>>:src/mmpa_linux_env.c>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_thread.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_file.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_time.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_atom.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_dlfcn.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_memory.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_message.cpp>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:src/mmpa_win_option.cpp>
)
target_include_directories(static_mmpa PRIVATE
    ${TOP_DIR}/inc
)
target_compile_definitions(static_mmpa PRIVATE
    LINUX=0
    $<IF:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,OS_TYPE=WIN,OS_TYPE=0>
    $<$<STREQUAL:${PRODUCT},ascend035>:STRERROR_R_RETURN_INT>
    $<$<STREQUAL:${PRODUCT},ascend031>:STRERROR_R_RETURN_INT>
)
target_compile_options(static_mmpa PRIVATE
    -fPIC -pipe -Wall
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>:-fvisibility=hidden>
    $<$<AND:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,$<CONFIG:Debug>>:/MTd>
    $<$<AND:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,$<CONFIG:Release>>:/MT>
)
target_link_libraries(static_mmpa
    PRIVATE
        $<BUILD_INTERFACE:c_sec_headers>
    PUBLIC
        mmpa_headers
)
set_target_properties(static_mmpa PROPERTIES
    ARCHIVE_OUTPUT_NAME $<IF:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,libstatic_mmpa,mmpa>
)

install(TARGETS mmpa static_mmpa
    DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)
