# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
occupy_space=300
vendor_bdf="0x19e5"
device_bdf="0xd806"
mode_normal="ts_agent|asdrv_queue|asdrv_buff|asdrv_trs|asdrv_svm|drv_pcie_hdc_host|asdrv_esched|asdrv_dms|asdrv_fms|asdrv_vmng|asdrv_vnic|asdrv_dpa|drv_pcie_host|asdrv_vpc|asdrv_ub|ascend_adapter|asdrv_trsbase|asdrv_pbl|drv_vascend|asdrv_vascend_adapt|ascend_kernel_open_adapt|drv_seclib_host"
mode_vnpu_host="${mode_normal}"
mode_vnpu_docker_host="${mode_normal}"
mode_vnpu_guest="asdrv_queue|asdrv_buff|asdrv_trs|asdrv_svm|drv_pcie_hdc_host|asdrv_esched|asdrv_dms|asdrv_fms|asdrv_vvmng|asdrv_vnic|asdrv_dpa|drv_pcie_host|asdrv_vvpc|asdrv_ub|ascend_adapter|asdrv_trsbase|asdrv_pbl|asdrv_vascend_adapt|ascend_kernel_open_adapt|drv_seclib_host"
src_names=(device_sw.bin device_sw.img itrustee.img ub_port_cfg.bin IMU_task.bin device_config.bin microwatt.bin imp_david_asic.bin)
dst_names=(ascend_950_device_sw.bin ascend_950_device_sw.img ascend_950_tee.bin ascend_950_ubcfg.bin ascend_950_imu.bin ascend_950_device_config.bin ascend_950_lp.bin ascend_950_imp.bin)
crltool_bin="ascend_check.bin"
crlFile="ascend_cloud_v4.crl"
curcrlFile="cur_ascend_cloud_v4.crl"
source_tee="device_sw.img"
pcie_tee="ascend_950_device_sw.img"
image_bin="device_sw.bin device_sw.img itrustee.img device_config.bin"
lib_put_path="common"
basic_ko="drv_pcie_hdc_host.ko"
vnpu_guest_ko="drv_pcie_hdc_host.ko"
reset_dev_nums="8|16|32|64"
character_device="davinci_manager asdrv_svm dvpp_cmdlist hisi_hdc"
