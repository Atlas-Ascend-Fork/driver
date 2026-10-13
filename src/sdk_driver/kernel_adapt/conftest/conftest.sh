#!/bin/bash
# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

PATH="${PATH}:/bin:/sbin:/usr/bin"

# make sure we are in the directory containing this script
SCRIPTDIR=`dirname $0`
cd $SCRIPTDIR

SOURCEDIR="$3"
OUTPUT=$SOURCEDIR/kernel/dev_inc/inc/conftest
DEBUG=0

if [ "$4" = "--debug" ]; then
    DEBUG=1
fi

log_debug(){
    if [ "$DEBUG" = "1" ]; then
        echo "$1" >> $OUTPUT/debug.log
    fi
}

get_os_info(){
    ARCH=$(uname -m)
    [ "$ARCH" = "aarch64" ] && ARCH="arm64"

    KERNEL_VERSION=$(uname -r)

    SOURCES="/usr/src/kernels/$KERNEL_VERSION"
    HEADERS=$SOURCES/include

    log_debug "ARCH=\"$ARCH\""
    log_debug "SOURCES=\"$SOURCES\""
    log_debug "OUTPUT=\"$OUTPUT\""
}

build_cflags(){
    ISYSTEM=`gcc -print-file-name=include 2> /dev/null`
    BASE_CFLAGS="-O2 -D__KERNEL__ \
-DKBUILD_BASENAME=\"#conftest$$\" -DKBUILD_MODNAME=\"#conftest$$\" \
-nostdinc -isystem $ISYSTEM \
-Wno-implicit-function-declaration -Wno-strict-prototypes"

    if [ -f "$HEADERS/generated/autoconf.h" ]; then
        AUTOCONF_FILE="$HEADERS/generated/autoconf.h"
    elif [ -f "$HEADERS/linux/autoconf.h" ]; then
        AUTOCONF_FILE="$HEADERS/linux/autoconf.h"
    elif [ -f "/usr/src/linux-headers-$KERNEL_VERSION/include/generated/autoconf.h" ]; then
        AUTOCONF_FILE="/usr/src/linux-headers-$KERNEL_VERSION/include/generated/autoconf.h"
    else
        AUTOCONF_FILE=""
    fi

    KERNEL_ARCH="$ARCH"
    if [ "$ARCH" = "i386" -o "$ARCH" = "x86_64" ]; then
        if [ -d "$SOURCES/arch/x86" ]; then
            KERNEL_ARCH="x86"
        fi
    fi

    SOURCE_HEADERS="$HEADERS"
    SOURCE_ARCH_HEADERS="$SOURCES/arch/$KERNEL_ARCH/include"

    for _mach_dir in `ls -1d $SOURCES/arch/$KERNEL_ARCH/mach-* 2>/dev/null`; do
        _mach=`echo $_mach_dir | \
            sed -e "s,$SOURCES/arch/$KERNEL_ARCH/mach-,," | \
            tr 'a-z' 'A-Z'`
        if grep -q "CONFIG_ARCH_$_mach 1" $AUTOCONF_FILE; then
            ARCH_CFLAGS="$ARCH_CFLAGS -I$_mach_dir/include"
        fi
    done

    if [ "$ARCH" = "arm" ]; then
        ARCH_CFLAGS="$ARCH_CFLAGS -D__LINUX_ARM_ARCH__=7"
    fi

    ARCH_CFLAGS="$ARCH_CFLAGS -I$SOURCE_HEADERS/asm-$KERNEL_ARCH/mach-default"
    ARCH_CFLAGS="$ARCH_CFLAGS -I$SOURCE_ARCH_HEADERS/asm/mach-default"

    CFLAGS="$BASE_CFLAGS $ARCH_CFLAGS -include $AUTOCONF_FILE"
    CFLAGS="$CFLAGS -I$SOURCE_HEADERS"
    CFLAGS="$CFLAGS -I$SOURCE_HEADERS/uapi"
    CFLAGS="$CFLAGS -I$SOURCE_HEADERS/xen"
    CFLAGS="$CFLAGS -I$SOURCE_HEADERS/generated/uapi"
    CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS"
    CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS/uapi"
    CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS/generated"
    CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS/generated/uapi"
    log_debug "BASE_CFLAGS=\"$BASE_CFLAGS\""
    log_debug "ARCH_CFLAGS=\"$ARCH_CFLAGS\""
    log_debug "AUTOCONF_FILE=\"$AUTOCONF_FILE\""
    log_debug "CFLAGS=\"$CFLAGS\""

    if [ -n "$BUILD_PARAMS" ]; then
        CFLAGS="$CFLAGS -D$BUILD_PARAMS"
    fi

    GCC_GOTO_SH="$SOURCES/build/gcc-goto.sh"

    if [ -f "$GCC_GOTO_SH" ]; then
        if [ `/bin/sh "$GCC_GOTO_SH" "gcc"` = "y" ]; then
            CFLAGS="$CFLAGS -DCC_HAVE_ASM_GOTO"
        fi
    fi
    if [ -n "$AUTOCONF_FILE" ]; then
        setup_fentry_flags
    fi
}

setup_fentry_flags(){
    if grep -q "CONFIG_HAVE_FENTRY 1" $AUTOCONF_FILE; then
        echo "" > conftest$$.c

        gcc -mfentry -c -x c conftest$$.c > /dev/null 2>&1
        rm -f conftest$$.c

        if [ -f conftest$$.o ]; then
            rm -f conftest$$.o
            CFLAGS="$CFLAGS -mfentry -DCC_USING_FENTRY"
        fi
    fi
}

create_preamble(){
    CONFTEST_PREAMBLE="
    #include <linux/kconfig.h>
    #if defined(CONFIG_KASAN) && defined(CONFIG_ARM64)
        #ifdef CONFIG_KASAN_SW_TAGS
            #define KASAN_SHADOW_SCALE_SHIFT 4
        #else
            #define KASAN_SHADOW_SCALE_SHIFT 3
        #endif
    #endif"

    if [ -n "$AUTOCONF_FILE" ] && [ -f "$AUTOCONF_FILE" ]; then
        CONFTEST_PREAMBLE="${CONFTEST_PREAMBLE}
            #include <generated/autoconf.h>"
    else
        CONFTEST_PREAMBLE="${CONFTEST_PREAMBLE}
            #include <linux/autoconf.h>"
    fi
}

safe_clean_directory(){
    local dir="$1"

    if [ -z "$dir" ]; then
        return 1
    fi

    case "$dir" in
        "/"|"/bin"|"/etc"|"/home"|"/lib"|"/opt"|"/root"|"/sbin"|"/sys"|"/usr"|"/var")
            return 1
            ;;
    esac

    if [[ ! "$dir" =~ /kernel/dev_inc/inc/conftest ]]; then
        return 1
    fi

    local depth=$(echo "$dir" | tr -cd '/' | wc -c)
    if [ "$depth" -lt 3 ]; then
        return 1
    fi

    rm -rf "$dir"
    mkdir -p "$dir"
}

initialize_conftest(){
    DEBUG_LOG="$OUTPUT/debug.log"
    if [ "$DEBUG" = "1" ]; then
        touch "$DEBUG_LOG"
    fi

    safe_clean_directory "$OUTPUT"

    get_os_info

    build_cflags

    create_preamble
}

append_conftest(){
    # Echo data from stdin: this is a transitional function to make it easier
    # to port conftests from drivers with parallel conftest generation to
    # older driver versions

    while read LINE; do
        echo ${LINE} >> "$OUTPUT/$1.h"
    done
}

test_header_presence(){
    # Determine if the given header file (which may or may not be
    # present) is provided by the target kernel.

    # Input:
    #     $1: dependent header files
    #     $2: macro name

    FILE="$1"
    FILE_DEFINE="$2"

    TEST_CFLAGS="-E -M $CFLAGS"

    CODE="#include <$FILE>"
    if echo "$CODE" | gcc $TEST_CFLAGS - > /dev/null 2>&1; then
        echo "#define $FILE_DEFINE" | append_conftest "headers"
    else
        # If preprocessing fails, it might be because the tested header file
        # is missing or exits but depends on other header files. Using the
        # -MG option for preprocessing again will ignore a missing header file,
        # but preprocessing will still fail if the header file exists.
        if echo "$CODE" | gcc $TEST_CFLAGS -MG - > /dev/null 2>&1; then
            echo "#undef $FILE_DEFINE" | append_conftest "headers"
        else
            echo "#define $FILE_DEFINE" | append_conftest "headers"
        fi
    fi
}

compile_check_conftest(){
    # Compile the current conftest C file and check+output the result
    CODE="$1"
    DEFINE="$2"
    VALUE="$3"
    CATEGORY="$4"

    echo "$CONFTEST_PREAMBLE
    $CODE" > conftest$$.c

    gcc $CFLAGS -c conftest$$.c >> $OUTPUT/make.log 2>&1
    rm -f conftest$$.c

    if [ -f conftest$$.o ]; then
        rm -f conftest$$.o
        if [ "${CATEGORY}" = "functions" ]; then
            echo "#undef ${DEFINE}" | append_conftest "${CATEGORY}"
        else
            echo "#define ${DEFINE} ${VALUE}" | append_conftest "${CATEGORY}"
        fi
        return
    else
        if [ "${CATEGORY}" = "functions" ]; then
            echo "#define ${DEFINE} ${VALUE}" | append_conftest "${CATEGORY}"
        else
            echo "#undef ${DEFINE}" | append_conftest "${CATEGORY}"
        fi
        return
    fi
}

merge_headers_conftest(){

    local output_file="./$OUTPUT/conftests.h"
    cat > "$output_file" << EOF
/*
 * Auto-generated conftest header
 * This file includes all individual conftest headers
 */

#ifndef _MERGED_CONFTEST_HEADERS_H_
#define _MERGED_CONFTEST_HEADERS_H_

EOF

    local merged_count=0
    local header_files=("functions" "types" "macros" "generics" "symbols" "headers")

    for category in "${header_files[@]}"; do
        local header_file="./$OUTPUT/${category}.h"
        if [ -f "$header_file" ] && [ -s "$header_file" ]; then
            echo "/* Include ${category} conftest */" >> "$output_file"
            echo "#include \"${category}.h\"" >> "$output_file"
            echo "" >> "$output_file"
            ((merged_count++))
        fi
    done

    echo '#endif /* _MERGED_CONFTEST_HEADERS_H_ */' >> "$output_file"
}

compile_test(){
    case "$1" in
        func_pci_aer_clear_nonfatal_status_present)
            CODE="
            #include <linux/aer.h>
            void conftest_func_pci_aer_clear_nonfatal_status_present(void) {
                pci_aer_clear_nonfatal_status();
            }"
            compile_check_conftest "$CODE" "FUNC_PCI_AER_CLEAR_NONFATAL_STATUS_PRESENT" "" "functions"
        ;;

        func_init_iova_domain_change_dma_bit_mask_args)
            CODE="
            #include <linux/iova.h>
            void conftest_func_init_iova_domain_change_dma_bit_mask_args(void) {
                init_iova_domain();
            }"
            compile_check_conftest "$CODE" "FUNC_INIT_IOVA_DOMAIN_CHANGE_DMA_BIT_MASK_ARGS" "" "functions"
        ;;

        header_types)
            test_header_presence "linux/types.h" "HEADER_TYPES"
        ;;

        header_stdbool)
            test_header_presence "stdbool.h" "HEADER_STDBOOL"
        ;;

        macro_tes_sq_get_wq_flag)
            CODE="
            #include <linux/workqueue.h>
            unsigned int conftest_macro_tes_sq_get_wq_flag(void) {
                return __WQ_LEGACY;
            }"
            compile_check_conftest "$CODE" "MACRO_TRS_SQ_GET_WQ_FLAG" "" "macros"

        ;;

        macro_gfp_retry_or_repeat)
            CODE="
            #include <linux/gfp.h>
            unsigned int conftest_macro_gfp_retry_or_repeat(void) {
                return __GFP_RETRY_MAYFAIL;
            }"
            compile_check_conftest "$CODE" "MACRO_GFP_RETRY_OR_REPEAT" "" "macros"
        ;;

        func_get_pid_link_node)
            CODE="
            #include <linux/sched.h>
            uintptr_t conftest_func_get_pid_link_node(void) {
                struct task_struct *task;
                return (uintptr_t)(&task->pid_links[0]);
            }"
            compile_check_conftest "$CODE" "FUNC_GET_PID_LINK_NODE" "" "types"
        ;;

        header_rwlock)
            test_header_presence "linux/rwlock.h" "HEADER_RWLOCK"
        ;;

        header_rwlock_types)
            test_header_presence "linux/rwlock_types.h" "HEADER_RWLOCK_TYPES"
        ;;

        *)
            echo "Error: unknown conftest '$1' requested" >&2
            exit 1
        ;;
    esac
 }

case "$1" in
    compile_single_test)
        test_name="$2"

        initialize_conftest

        compile_test "$test_name"
        log_debug "=== All Tests Completed ==="
    ;;
    compile_multi_test)
        table_file="$2"

        initialize_conftest

        log_debug "=== Running All Tests ==="

        ASCEND_CONFTEST_FUNCTION_COMPILE_TESTS=$(tail -n +2 "$table_file" | cut -d',' -f1 | tr -d ' "')
        log_debug "Found functions: $ASCEND_CONFTEST_FUNCTION_COMPILE_TESTS"

        for test_name in $ASCEND_CONFTEST_FUNCTION_COMPILE_TESTS; do
            log_debug "Testing: $test_name"
            compile_test "$test_name"
            log_debug "---"
        done
        log_debug "=== All Tests Completed ==="
        ;;
    merge_conftest)
        merge_headers_conftest
    ;;
esac


# 单个函数检测
# bash conftest.sh compile_single_test header_types .
# 多个函数检测
# bash conftest.sh compile_multi_test ./check_tables.csv .

# 将生成的所有头文件合并成conftest.h
# bash conftest.sh merge_conftest
