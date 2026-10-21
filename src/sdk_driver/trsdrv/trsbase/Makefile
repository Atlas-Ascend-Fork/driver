# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

obj-m += asdrv_trsbase.o
asdrv_trsbase-objs := trsbase_init.o

include ${DRIVER_KERNEL_DIR}/trsdrv/trsbase/chan/chan.mk
include ${DRIVER_KERNEL_DIR}/trsdrv/trsbase/id_allocator/id_allocator.mk
include ${DRIVER_KERNEL_DIR}/trsdrv/trsbase/feature/feature.mk
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Wdate-time -Werror -Wfloat-equal -Wformat -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla
