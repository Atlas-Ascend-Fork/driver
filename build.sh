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

set -e

dotted_line="----------------------------------------------------------------"
BASE_PATH=$(cd "$(dirname $0)"; pwd)
export BUILD_PATH="${BASE_PATH}/build"
export BUILD_OUT_PATH="${BASE_PATH}/build_out"

# print usage message
usage()
{
  echo "Usage:"
  echo "    bash build.sh [-j[n]] [-h | --help] [-k] [-v]"
  echo ""
  echo "Options:"
  echo "    -h | --help             Print usage"
  echo "    -j[n]                  Set the number of threads used for building npu_driver, default is 32"
  echo "                           Examples: bash build.sh -j16 --pkg --soc=ascend910b"
  echo "    -v                     Display build command"
  echo "    -k <path>              Set configured kernel build tree, default \"/lib/modules/\$(uname -r)/build\""
  echo "    --soc=soc_version      Compile for specified Ascend SoC, soc_version is ascend910b or ascend910_93"
  echo "    --pkg                  Build run package"
  echo "    --demo                 Build demo package"
  echo "    --ube                  Enable UB engine"
  echo "    --cann_3rd_lib_path=<PATH>"
  echo "                           Set ascend third_party package install path, default ./third_party"
  echo "    --make_clean           Clean build artifacts"
  echo $dotted_line
  echo "Examples:"
  echo "    bash build.sh --pkg --soc=ascend910b"
  echo "    bash build.sh --pkg --soc=ascend910_93"
  echo "    bash build.sh --pkg --soc=ascend950"
  echo "    bash build.sh --pkg --soc=ascend950 --ube"
}

get_product()
{
  local _COMPUTE_UNIT="${COMPUTE_UNIT,,}"

  case "${_COMPUTE_UNIT}" in
    ascend910b)
      PRODUCT=ascend910B
      ;;
    ascend910_93)
      PRODUCT=ascend910B
      ASCEND910_93_EX=TRUE
      ;;
    ascend950)
      PRODUCT=ascend950
      ;;
    *)
      echo "Unknown COMPUTE_UNIT: ${COMPUTE_UNIT}"
      usage
      exit 1
  esac
}

clean_build() {
  if [ -d "${BUILD_PATH}" ]; then
    rm -rf ${BUILD_PATH}/*
  fi
}

clean_build_out_all() {
  [ -d "${BUILD_OUT_PATH}" ] && rm -rf "${BUILD_OUT_PATH}"

  local default_dirs=("build" "scripts/package/common/py/__pycache__" "src/sdk_driver/kernel_adapt/conftest/kernel" "scripts/package/common/py/utils/__pycache__")
  local extra_dirs=("${default_dirs[@]}")
  for dir_name in "${extra_dirs[@]}";do
    [ -d "${dir_name}" ] && rm -rf "${dir_name}" 2>/dev/null || true
  done
}

check_param()
{
  if [ "x${COMPUTE_UNIT}" = "x" ]; then
    echo "Missing option: --soc"
    usage
    exit 1
  fi
}

# parse and set options
checkopts()
{
  VERBOSE=""
  THREAD_NUM=32
  CORE_NUMS=$(cat /proc/cpuinfo | grep "processor" | wc -l)
  ENABLE_GE_UT="n"
  ENABLE_GE_ST="n"
  ENABLE_GE_COV="n"
  PRODUCT=""
  COMPUTE_UNIT=""
  KERNEL_PATH=""
  ENABLE_PACKAGE=FALSE
  ENABLE_BUILD_PRODUCT=TRUE
  ENABLE_UBE=FALSE
  BUILD_COMPONENT="DRIVER"
  CANN_3RD_LIB_PATH=""
  ASCEND910_93_EX=FALSE

  # Process the options
  while getopts 'uschj:k:v-:' opt
  do
    case "${opt}" in
      h)
        usage
        exit 0
        ;;
      j)
        THREAD_NUM=$OPTARG
        ;;
      v)
        VERBOSE="VERBOSE=1"
        ;;
      k)
        KERNEL_PATH=$OPTARG
        ;;
      -)
        case $OPTARG in
          help)
            usage
            exit 0
            ;;
          pkg)
            ENABLE_PACKAGE="TRUE"
            ;;
          demo)
            ENABLE_BUILD_PRODUCT="FALSE"
            ;;
          ube)
            ENABLE_UBE="TRUE"
            ;;
          soc=*)
            COMPUTE_UNIT=${OPTARG#*=}
            get_product
            ;;
          driver_compat)
            BUILD_COMPONENT="DRIVER_COMPAT"
            ;;
          cann_3rd_lib_path=*)
            CANN_3RD_LIB_PATH="$(realpath ${OPTARG#*=})"
            ;;
          make_clean)
            clean_build
            clean_build_out_all
            exit 0
            ;;
          *)
            echo "Undefined option: ${OPTARG}"
            usage
            exit 1
            ;;
        esac ;;
      *)
        echo "Undefined option: ${opt}"
        usage
        exit 1
    esac
  done

  check_param
}
checkopts "$@"

mk_dir() {
  local create_dir="$1"  # the target to make

  mkdir -pv "${create_dir}"
  echo "created ${create_dir}"
}

prepare_src()
{
  if [ "${ENABLE_BUILD_PRODUCT}" != "TRUE" ]; then
    return
  fi
  echo "prepare source"

  pushd $BASE_PATH
  # rpepare source for dsmi
  cp -rf ./src/custom/dev_prod/user/dsmi_product_ext ./src/ascend_hal/dmc/dsmi/

  #prepare source for asdrv_dms.ko
  mv ./src/sdk_driver/dms/devmng/product/dms_product.c ./src/sdk_driver/dms/devmng/product/dms_product.c.org
  mv ./src/sdk_driver/dms/devmng/product/dms_product.h ./src/sdk_driver/dms/devmng/product/dms_product.h.org
  mv ./src/sdk_driver/dms/devmng/product/dms_product.mk ./src/sdk_driver/dms/devmng/product/dms_product.mk.org
  mv ./scripts/package/driver/ascend910_93/scripts/specific_func.inc ./scripts/package/driver/ascend910_93/scripts/specific_func.inc.org
  mv ./scripts/package/driver/ascend910B/scripts/specific_func.inc ./scripts/package/driver/ascend910B/scripts/specific_func.inc.org
  mv ./scripts/package/driver/ascend910B/scripts/sys_version/sys_version.conf scripts/package/driver/ascend910B/scripts/sys_version/sys_version.conf.org
  mv ./scripts/package/driver/ascend950/scripts/sys_version/sys_version.conf scripts/package/driver/ascend950/scripts/sys_version/sys_version.conf.org
  mv scripts/package/driver/common/conf/itf_ver.conf scripts/package/driver/common/conf/itf_ver.conf.org

  cp -rf ./src/custom/dev_prod/kernel/drv_devmng/* ./src/sdk_driver/dms/devmng/drv_devmng/drv_devmng_host/ascend910/
  cp -rf ./src/custom/dev_prod/kernel/dms/product/* ./src/sdk_driver/dms/devmng/product/
  cp -f ./src/custom/include/dms_product_ioctl.h ./src/sdk_driver/dms/devmng/product/
  cp -f ./scripts/package/custom/driver/common/conf/itf_ver.conf scripts/package/driver/common/conf/itf_ver.conf

  cp scripts/package/driver/ascend910B/driver.xml scripts/package/driver/ascend910B/driver.xml.org
  cp scripts/package/driver/ascend950/driver.xml scripts/package/driver/ascend950/driver.xml.org
  if [ "${PRODUCT}" = "ascend950" ]; then
    python3 ./scripts/package/custom/copy_xml.py scripts/package/driver/ascend950/driver.xml scripts/package/custom/driver/ascend950/driver.xml
    python3 ./scripts/package/custom/copy_xml.py scripts/package/driver/ascend950/driver.xml scripts/package/custom/driver/ascend950/driver_atlas.xml
    COMPATIBLE_VERSION=$(grep -rn "ascend950" scripts/package/custom/driver/common/compatible_version.conf | cut -d":" -f3)
    cp scripts/package/custom/driver/ascend950/scripts/sys_version/sys_version.conf scripts/package/driver/ascend950/scripts/sys_version/sys_version.conf
    PACKAGE_VERSION=$(cat scripts/package/driver/ascend950/scripts/sys_version/sys_version.conf)
  else
    if [ "${ASCEND910_93_EX}" = "TRUE" ]; then
      cp scripts/package/custom/driver/ascend910_93/scripts/specific_func.inc scripts/package/driver/ascend910_93/scripts/specific_func.inc
      python3 ./scripts/package/custom/copy_xml.py scripts/package/driver/ascend910B/driver.xml scripts/package/custom/driver/ascend910_93/driver.xml
      python3 ./scripts/package/custom/copy_xml.py scripts/package/driver/ascend910B/driver.xml scripts/package/custom/driver/ascend910B/driver_atlas.xml
      COMPATIBLE_VERSION=$(grep -rn "ascend910_93" scripts/package/custom/driver/common/compatible_version.conf | cut -d":" -f3)
    else
      cp scripts/package/custom/driver/ascend910B/scripts/specific_func.inc scripts/package/driver/ascend910B/scripts/specific_func.inc
      python3 ./scripts/package/custom/copy_xml.py scripts/package/driver/ascend910B/driver.xml scripts/package/custom/driver/ascend910B/driver.xml
      python3 ./scripts/package/custom/copy_xml.py scripts/package/driver/ascend910B/driver.xml scripts/package/custom/driver/ascend910B/driver_atlas.xml
      COMPATIBLE_VERSION=$(grep -rn "ascend910B" scripts/package/custom/driver/common/compatible_version.conf | cut -d":" -f3)
    fi

    cp scripts/package/custom/driver/ascend910B/scripts/sys_version/sys_version.conf scripts/package/driver/ascend910B/scripts/sys_version/sys_version.conf
    PACKAGE_VERSION=$(cat scripts/package/driver/ascend910B/scripts/sys_version/sys_version.conf)
  fi
  echo "version=${PACKAGE_VERSION}" >./scripts/package/custom/version.info
  sed -i "s/compatible_version=/compatible_version=${COMPATIBLE_VERSION}/g" scripts/package/driver/common/conf/itf_ver.conf
  sed -i "s/package_version=/package_version=${PACKAGE_VERSION}/g" scripts/package/driver/common/conf/itf_ver.conf
  popd
}

clean_src()
{
  if [ "${ENABLE_BUILD_PRODUCT}" != "TRUE" ]; then
    return
  fi
  echo "clean source"

  pushd $BASE_PATH
  rm -rf ./src/ascend_hal/dmc/dsmi/dsmi_product_ext
  rm -rf ./src/sdk_driver/dms/devmng/drv_devmng/drv_devmng_host/ascend910/devdrv_manager_dev_share.c
  rm -rf ./src/sdk_driver/dms/devmng/drv_devmng/drv_devmng_host/ascend910/devdrv_manager_dev_share.h
  rm -rf ./src/sdk_driver/dms/devmng/product/dms_product_host.c
  rm -rf ./src/sdk_driver/dms/devmng/product/dms_product_host.h
  rm -rf ./src/sdk_driver/dms/devmng/product/dms_product_ioctl.h
  mv ./src/sdk_driver/dms/devmng/product/dms_product.c.org ./src/sdk_driver/dms/devmng/product/dms_product.c
  mv ./src/sdk_driver/dms/devmng/product/dms_product.h.org ./src/sdk_driver/dms/devmng/product/dms_product.h
  mv ./src/sdk_driver/dms/devmng/product/dms_product.mk.org ./src/sdk_driver/dms/devmng/product/dms_product.mk
  mv ./scripts/package/driver/ascend910_93/scripts/specific_func.inc.org ./scripts/package/driver/ascend910_93/scripts/specific_func.inc
  mv ./scripts/package/driver/ascend910B/scripts/specific_func.inc.org ./scripts/package/driver/ascend910B/scripts/specific_func.inc
  mv scripts/package/driver/ascend910B/driver.xml.org scripts/package/driver/ascend910B/driver.xml
  mv scripts/package/driver/ascend950/driver.xml.org scripts/package/driver/ascend950/driver.xml
  mv scripts/package/driver/ascend910B/scripts/sys_version/sys_version.conf.org scripts/package/driver/ascend910B/scripts/sys_version/sys_version.conf
  mv scripts/package/driver/ascend950/scripts/sys_version/sys_version.conf.org scripts/package/driver/ascend950/scripts/sys_version/sys_version.conf
  mv scripts/package/driver/common/conf/itf_ver.conf.org scripts/package/driver/common/conf/itf_ver.conf
  rm -f scripts/package/custom/version.info
  popd
}

cleanup() {
    clean_src
}

# cleanup temporary source files before exit
trap cleanup EXIT

build_check_with_ube()
{
  local distributor_id=$(cat /etc/os-release | grep '^NAME' | awk -F '\"' '{print $2}')
  local arch=$(uname -m)

  if [ "${distributor_id}" != "openEuler" ]; then
    echo "Not supported build in the ${distributor_id}, please use openEuler building with ube."
    return 1
  fi

  if [ "${arch}" != "aarch64" ]; then
    echo "Not supported build in the ${arch}, please use aarch64 building with ube."
    return 1
  fi

  if [ "$(yum list installed umdk-urma-lib | grep 'Installed Packages')" != "Installed Packages" ]; then
    echo "The \"umdk-urma-lib\" is not installed. Please Use \"yum install -y umdk-urma-lib\" before building with ube."
    return 1
  fi

  if [ "$(yum list installed umdk-urma-devel | grep 'Installed Packages')" != "Installed Packages" ]; then
    echo "The \"umdk-urma-devel\" is not installed. Please Use \"yum install -y umdk-urma-devel\" before building with ube."
    return 1
  fi

  return 0
}

# create build path
build_npu_driver()
{
  echo "create build directory and build npu_driver"
  export PROJECT_VERSION=$(cat scripts/package/driver/${PRODUCT}/scripts/sys_version/sys_version.conf)
  mk_dir "${BUILD_PATH}"
  cd "${BUILD_PATH}"

  if [ "$THREAD_NUM" -gt "$CORE_NUMS" ]; then
    echo "compile thread num:$THREAD_NUM over core num:$CORE_NUMS, adjust to core num"
    THREAD_NUM=$CORE_NUMS
  fi
  echo "compile thread num:$THREAD_NUM"

  CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_OPEN_SRC=y -DPRODUCT_SIDE=host -DCMAKE_INSTALL_PREFIX=."
  if [[ "X$ENABLE_GE_COV" = "Xy" ]]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_GE_COV=y"
  fi

  if [[ "X$ENABLE_GE_UT" = "Xy" ]]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_GE_UT=y"
  fi

  if [[ "X$ENABLE_GE_ST" = "Xy" ]]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_GE_ST=y"
  fi

  if [[ "X$KERNEL_PATH" != "X" ]]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DCUSTOM_KERNEL_PATH=${KERNEL_PATH}"
  fi

  if [ "$ENABLE_PACKAGE" = "TRUE" ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_PACKAGE=TRUE"
  fi

  if [ "$ASCEND910_93_EX" = "TRUE" ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DASCEND910_93_EX=TRUE"
  fi

  if [ "$ENABLE_BUILD_PRODUCT" = "TRUE" ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_BUILD_PRODUCT=TRUE"
  fi

  if [ "${PRODUCT}" = "ascend950" ] && [ "${ENABLE_UBE}" = "TRUE" ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_UBE=true"
    build_check_with_ube
    if [ $? -ne 0 ]; then
      return 1
    fi
  fi

  CMAKE_ARGS="${CMAKE_ARGS} -DPRODUCT=${PRODUCT} -DBUILD_COMPONENT=${BUILD_COMPONENT} ${VERBOSE} -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH}"

  echo "${CMAKE_ARGS}"
  cmake ${CMAKE_ARGS} ..
  if [ $? -ne 0 ]
  then
    echo "execute command: cmake ${CMAKE_ARGS} .. failed."
    return 1
  fi

  if [ "$BUILD_COMPONENT" = "DRIVER" ]; then
    TARGET="driver"
  fi

  if [ "$BUILD_COMPONENT" = "DRIVER_COMPAT" ]; then
    TARGET="driver_compat"
  fi

  mkdir -p ${BUILD_PATH}/lib/tools/
  echo $(date "+%Y-%m-%d 00:00:00") > ${BUILD_PATH}/lib/tools/build.info
  make ${TARGET} -j${THREAD_NUM} && make install
  if [ $? -ne 0 ]
  then
    echo "execute command: make ${TARGET} -j${THREAD_NUM} && make install failed."
    return 1
  fi

  echo "npu_driver build success!"
}

# generate output package in tar form, including ut/st libraries/executables
generate_package()
{
  make package -j${THREAD_NUM}
  if [ $? -ne 0 ]; then
    echo "Failed to generate ${BUILD_COMPONENT} package."
    return 1
  fi
  find ${BUILD_OUT_PATH} -maxdepth 1 ! -name "Ascend-hdk-*" ! -path ${BUILD_OUT_PATH} -exec rm -rf {} \;
  echo "Generate package success."
}

# npu_driver build start
echo "---------------- npu_driver build start ------------------"
g++ -v

prepare_src

build_npu_driver || { echo "npu_driver build failed."; exit -1; }
echo "---------------- npu_driver build finished ----------------"

if [[ "X$ENABLE_GE_UT" = "Xn" && "$ENABLE_PACKAGE" = "TRUE" ]]; then
  echo "-------------- npu_driver package generate start ----------------"
  generate_package
  echo "-------------- npu_driver package generate finished -------------"
fi

exit 0
