# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

$(MODULE_NAME)-objs += trs_stars/comm/trs_stars_soc.o trs_stars/adapt/comm/trs_stars_func_com.o trs_stars/adapt/comm/src/stars_cnt_notify_tbl.o trs_stars/adapt/comm/src/stars_event_tbl_ns.o trs_stars/adapt/comm/src/stars_notify_tbl.o trs_stars/adapt/near/trs_stars_near_func.o
$(MODULE_NAME)-objs += trs_stars/adapt/soc/cloud_v2/stars_notify_tbl_def_cloud_v2.o trs_stars/adapt/soc/cloud_v4/stars_notify_tbl_def_cloud_v4.o trs_stars/adapt/soc/cloud_v4/stars_cnt_notify_tbl_def_cloud_v4.o
ifneq ($(filter $(PRODUCT), ascend950),)
    $(MODULE_NAME)-objs += trs_stars/adapt/near/stars_v2/trs_stars_v2_func_adapt.o
else
    $(MODULE_NAME)-objs += trs_stars/adapt/near/stars_v1/trs_stars_v1_func_adapt.o
endif

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/trs
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trsbase/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_stars/comm
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_stars/adapt/comm
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_stars/adapt/comm/src
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_stars/adapt/near
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_stars/adapt/soc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/drv_devmng/drv_devmng_inc/

EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_TRS_STARS
