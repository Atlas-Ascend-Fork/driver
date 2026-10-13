#!/bin/bash
# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

# build.sh - 编译安装ibv_extend库
# 将库文件安装到当前目录的output路径下

set -e  # 遇到错误立即退出

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"
COVERAGE_DIR="${SCRIPT_DIR}/coverage_report"

# 可配置变量
LIBIBVERBS_BUILD_DIR=""
LIBIBVERBS_SOURCE_DIR=""
LIBBOUNDSCHECK_BUILD_DIR=""
LIBBOUNDSCHECK_SOURCE_DIR=""
BUILD_TYPE="Release"
ENABLE_COVERAGE=false
RUN_UT=false

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印信息
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# 显示帮助信息
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -b, --libibverbs-build-dir=PATH        Specify libibverbs build directory (pre-built)"
    echo "  -s, --libibverbs-source-dir=PATH       Specify libibverbs source directory"
    echo "  -x, --libboundscheck-build-dir=PATH    Specify libboundscheck build directory (pre-built)"
    echo "  -e, --libboundscheck-source-dir=PATH   Specify libboundscheck source directory"
    echo "  -t, --type=TYPE                        Build type: release or debug (default: release)"
    echo "  -c, --coverage                         Run unit tests after build and enable code coverage"
    echo "  -u, --run-ut                           Run unit tests after build"
    echo "  -h, --help                             Show this help message"
    echo ""
    echo "Notes:"
    echo "  -b and -s are mutually exclusive"
    echo "  If neither -b nor -s is specified, rdma-core will be auto-downloaded"
    echo "  If -e is specified, libboundscheck will be auto-downloaded"
    echo "  --coverage-report will automatically enable coverage and run tests"
    echo ""
    echo "Examples:"
    echo "  $0                                     # Build with auto-download (release mode)"
    echo "  $0 -t=debug                            # Build in debug mode"
    echo "  $0 -t=debug -c                         # Build with coverage support"
    echo "  $0 -t=debug -u                         # Build and run unit tests without coverage support"
    echo "  $0 -b=/path/to/rdma-core/build         # Use custom built libibverbs"
    echo "  $0 -s=/path/to/rdma-core               # Use custom rdma-core source"
    echo "  $0 -e=/path/to/libboundscheck          # Use custom libboundscheck source"
    exit 0
}

# 解析参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -b=*)
                LIBIBVERBS_BUILD_DIR="${1#*=}"
                shift
                ;;
            -b)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBIBVERBS_BUILD_DIR="$2"
                    shift 2
                else
                    error "Option -b requires an argument"
                fi
                ;;
            --libibverbs-build-dir=*)
                LIBIBVERBS_BUILD_DIR="${1#*=}"
                shift
                ;;
            --libibverbs-build-dir)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBIBVERBS_BUILD_DIR="$2"
                    shift 2
                else
                    error "Option --libibverbs-build-dir requires an argument"
                fi
                ;;
            -s=*)
                LIBIBVERBS_SOURCE_DIR="${1#*=}"
                shift
                ;;
            -s)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBIBVERBS_SOURCE_DIR="$2"
                    shift 2
                else
                    error "Option -s requires an argument"
                fi
                ;;
            --libibverbs-source-dir=*)
                LIBIBVERBS_SOURCE_DIR="${1#*=}"
                shift
                ;;
            --libibverbs-source-dir)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBIBVERBS_SOURCE_DIR="$2"
                    shift 2
                else
                    error "Option --libibverbs-source-dir requires an argument"
                fi
                ;;
            -x=*)
                LIBBOUNDSCHECK_BUILD_DIR="${1#*=}"
                shift
                ;;
            -x)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBBOUNDSCHECK_BUILD_DIR="$2"
                    shift 2
                else
                    error "Option -x requires an argument"
                fi
                ;;
            --libboundscheck-build-dir=*)
                LIBBOUNDSCHECK_BUILD_DIR="${1#*=}"
                shift
                ;;
            --libboundscheck-build-dir)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBBOUNDSCHECK_BUILD_DIR="$2"
                    shift 2
                else
                    error "Option --libboundscheck-build-dir requires an argument"
                fi
                ;;
            -e=*)
                LIBBOUNDSCHECK_SOURCE_DIR="${1#*=}"
                shift
                ;;
            -e)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBBOUNDSCHECK_SOURCE_DIR="$2"
                    shift 2
                else
                    error "Option -e requires an argument"
                fi
                ;;
            --libboundscheck-source-dir=*)
                LIBBOUNDSCHECK_SOURCE_DIR="${1#*=}"
                shift
                ;;
            --libboundscheck-source-dir)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    LIBBOUNDSCHECK_SOURCE_DIR="$2"
                    shift 2
                else
                    error "Option --libboundscheck-source-dir requires an argument"
                fi
                ;;
            -t=*)
                BUILD_TYPE="${1#*=}"
                shift
                ;;
            -t)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    BUILD_TYPE="$2"
                    shift 2
                else
                    error "Option -t requires an argument"
                fi
                ;;
            --type=*)
                BUILD_TYPE="${1#*=}"
                shift
                ;;
            --type)
                if [ -n "$2" ] && [[ "$2" != -* ]]; then
                    BUILD_TYPE="$2"
                    shift 2
                else
                    error "Option --type requires an argument"
                fi
                ;;
            -c|--coverage)
                ENABLE_COVERAGE=true
                RUN_UT=true
                shift
                ;;
            -u|--run-ut)
                RUN_UT=true
                shift
                ;;
            -h|--help)
                show_help
                ;;
            *)
                error "Unknown option: $1\nUse -h or --help for usage information."
                ;;
        esac
    done
    # 验证 BUILD_TYPE
    BUILD_TYPE_LOWER=$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
    case "$BUILD_TYPE_LOWER" in
        release)
            BUILD_TYPE="Release"
            ;;
        debug)
            BUILD_TYPE="Debug"
            ;;
        *)
            error "Invalid build type: $BUILD_TYPE\nValid options: release, debug"
            ;;
    esac
    # 验证 -b 和 -s 互斥
    if [ -n "${LIBIBVERBS_BUILD_DIR}" ] && [ -n "${LIBIBVERBS_SOURCE_DIR}" ]; then
        error "Options -b and -s are mutually exclusive.\nUse -b for pre-built libibverbs or -s for source directory."
    fi
    # 验证 -x 和 -e 互斥
    if [ -n "${LIBBOUNDSCHECK_BUILD_DIR}" ] && [ -n "${LIBBOUNDSCHECK_SOURCE_DIR}" ]; then
        error "Options -x and -e are mutually exclusive.\nUse -x for pre-built libboundscheck or -e for source directory."
    fi
}

# 检查依赖
check_dependencies() {
    info "Checking build dependencies..."
    local missing_deps=()
    # 检查cmake
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    # 检查gcc
    if ! command -v gcc &> /dev/null; then
        missing_deps+=("gcc")
    fi
    # 检查make
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    if [ ${#missing_deps[@]} -ne 0 ]; then
        error "Missing dependencies: ${missing_deps[*]}\nPlease install them first."
    fi
    info "All dependencies are satisfied."
}

# 清理旧的构建文件
clean() {
    info "Cleaning old build files..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${OUTPUT_DIR}"
    info "Clean completed."
}

# 配置CMake
configure() {
    info "Configuring project with CMake..."
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    # 根据编译类型设置编译选项
    local c_flags=""
    if [ "${BUILD_TYPE}" = "Debug" ]; then
        c_flags="-g -O0 -Wall -Wextra"
    else
        c_flags="-O2 -Wall -Wextra"
    fi
    # 构建CMake参数
    local cmake_args=(
        "-DCMAKE_INSTALL_PREFIX=${OUTPUT_DIR}"
        "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
        "-DCMAKE_C_FLAGS=${c_flags}"
    )

    # 如果启用覆盖率,添加覆盖率选项
    if [ "${ENABLE_COVERAGE}" = true ]; then
        cmake_args+=("-DENABLE_COVERAGE=ON")
        info "Code coverage enabled"
    fi

    # 如果指定了libibverbs路径,添加到CMake参数
    if [ -n "${LIBIBVERBS_BUILD_DIR}" ]; then
        if [ -d "${LIBIBVERBS_BUILD_DIR}" ]; then
            cmake_args+=("-DLIBIBVERBS_BUILD_DIR=${LIBIBVERBS_BUILD_DIR}")
            info "Using libibverbs from: ${LIBIBVERBS_BUILD_DIR}"
        else
            error "libibverbs build directory not found: ${LIBIBVERBS_BUILD_DIR}"
        fi
    fi

    # 如果指定了libibverbs源码目录,添加到CMake参数
    if [ -n "${LIBIBVERBS_SOURCE_DIR}" ]; then
        if [ -d "${LIBIBVERBS_SOURCE_DIR}" ]; then
            cmake_args+=("-DLIBIBVERBS_SOURCE_DIR=${LIBIBVERBS_SOURCE_DIR}")
            info "Using libibverbs source from: ${LIBIBVERBS_SOURCE_DIR}"
        else
            error "libibverbs source directory not found: ${LIBIBVERBS_SOURCE_DIR}"
        fi
    fi

    # 如果指定了libboundscheck构建目录,添加到CMake参数
    if [ -n "${LIBBOUNDSCHECK_BUILD_DIR}" ]; then
        if [ -d "${LIBBOUNDSCHECK_BUILD_DIR}" ]; then
            cmake_args+=("-DLIBBOUNDSCHECK_BUILD_DIR=${LIBBOUNDSCHECK_BUILD_DIR}")
            info "Using libboundscheck from: ${LIBBOUNDSCHECK_BUILD_DIR}"
        else
            error "libboundscheck build directory not found: ${LIBBOUNDSCHECK_BUILD_DIR}"
        fi
    fi

    # 如果指定了libboundscheck源码目录,添加到CMake参数
    if [ -n "${LIBBOUNDSCHECK_SOURCE_DIR}" ]; then
        if [ -d "${LIBBOUNDSCHECK_SOURCE_DIR}" ]; then
            cmake_args+=("-DLIBBOUNDSCHECK_SOURCE_DIR=${LIBBOUNDSCHECK_SOURCE_DIR}")
            info "Using libboundscheck source from: ${LIBBOUNDSCHECK_SOURCE_DIR}"
        else
            error "libboundscheck source directory not found: ${LIBBOUNDSCHECK_SOURCE_DIR}"
        fi
    fi

    cmake .. "${cmake_args[@]}" || error "CMake configuration failed."

    info "Configuration completed (Build Type: ${BUILD_TYPE})."
}

# 运行单元测试
run_unit_tests() {
    info "Running unit tests..."
    local ut_binary="${BUILD_DIR}/ut/ibv_extend_ut"
    if [ ! -f "${ut_binary}" ]; then
        error "Unit test binary not found: ${ut_binary}\nPlease build the project first."
    fi
    cd "${BUILD_DIR}/ut"
    ./ibv_extend_ut || error "Unit tests failed."
    info "Unit tests completed successfully."
}

# 生成覆盖率报告
generate_coverage_report() {
    info "Generating coverage report..."

    # 检查是否安装了lcov
    if ! command -v lcov &> /dev/null; then
        warn "lcov not found, installing..."
        sudo yum install -y lcov || error "Failed to install lcov, lcov-1.16 is recommended"
    fi

    cd "${BUILD_DIR}/ut"

    # 生成覆盖率数据
    info "Capturing coverage data..."
    lcov --capture --directory . --output-file coverage_all.info || error "Failed to capture coverage data"

    # 过滤覆盖率数据,只保留ibv_extend.c文件
    info "Filtering coverage data to include only ibv_extend.c..."
    lcov --extract coverage_all.info "*/ibv_extend.c" --output-file coverage.info || error "Failed to filter coverage data"

    # 删除临时文件
    rm -f coverage_all.info

    # 生成HTML报告
    info "Generating HTML report..."
    genhtml coverage.info --output-directory "${COVERAGE_DIR}" || error "Failed to generate HTML report"

    info "Coverage report generated at: ${COVERAGE_DIR}/index.html"
}

# 打包覆盖率报告
package_coverage_report() {
    info "Packaging coverage report..."

    local timestamp=$(date +%Y%m%d_%H%M%S)
    local package_name="coverage_report_${timestamp}.tar.gz"

    cd "${SCRIPT_DIR}"
    tar -czf "${package_name}" -C "${SCRIPT_DIR}" coverage_report || error "Failed to package coverage report"

    info "Coverage report packaged: ${package_name}"
    info "Package size: $(du -h "${package_name}" | cut -f1)"
}

# 编译
build() {
    info "Building project..."
    cd "${BUILD_DIR}"
    local nproc=$(nproc 2>/dev/null || echo 4)
    make -j${nproc} || error "Build failed."
    info "Build completed successfully."
}

# 安装
install() {
    info "Installing to ${OUTPUT_DIR}..."
    cd "${BUILD_DIR}"

    local nproc=$(nproc 2>/dev/null || echo 4)
    make -j${nproc} install || error "Installation failed."

    # 确保 .so 文件有执行权限
    info "Setting executable permissions for shared libraries..."
    if [ -d "${OUTPUT_DIR}/lib" ]; then
        find "${OUTPUT_DIR}/lib" -name "*.so*" -type f -exec chmod 755 {} \;
        info "Shared library permissions set successfully."
    fi

    info "Installation completed."

    # 显示安装信息
    echo ""
    info "========================================="
    info "Installation Summary:"
    info "========================================="
    info "Build Type: ${BUILD_TYPE}"
    info "Install Prefix: ${OUTPUT_DIR}"
    info "Library: ${OUTPUT_DIR}/lib/"
    info "Headers: ${OUTPUT_DIR}/include/"
    info "========================================="
}

# 主函数
main() {
    parse_args "$@"

    check_dependencies
    clean
    configure
    build
    install
    # 如果需要运行单元测试
    if [ "${RUN_UT}" = true ]; then
        run_unit_tests
    fi
    # 如果需要生成覆盖率报告
    if [ "${ENABLE_COVERAGE}" = true ]; then
        generate_coverage_report
        package_coverage_report
    fi

    # 显示最终摘要
    echo ""
    info "========================================="
    info "Build Summary:"
    info "========================================="
    info "Build Type: ${BUILD_TYPE}"
    info "Install Prefix: ${OUTPUT_DIR}"
    info "Library: ${OUTPUT_DIR}/lib/"
    info "Headers: ${OUTPUT_DIR}/include/"
    if [ "${RUN_UT}" = true ]; then
        info "Unit Tests: Executed"
    fi
    if [ "${ENABLE_COVERAGE}" = true ]; then
        info "Coverage: Enabled"
        info "Coverage Report: ${COVERAGE_DIR}/index.html"
        info "Coverage Package: coverage_report_*.tar.gz"
    fi
    info "========================================="
}

# 运行主函数
main "$@"
