/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"

#include "dvpp_cmdlist_proc.h"
#include "dvpp_cmdlist_context.h"
#include "dvpp_sqe_update.h"
#include "dvpp_cmdlist.h"
#include "vpc_cmdlist.h"
#include "jpegd_cmdlist.h"
#include "jpege_cmdlist.h"
#include "dvpp_tlv_decoder.h"
#include "dvpp_vpu_decoder.h"
#include "dvpp_mem_allocater.h"
#include "dvpp_cmdlist_ioctl.h"
#include "dvpp_vmng.h"
#include "dvpp_mem_allocater_comm.h"
#include "ka_compiler_pub.h"

#ifdef SUPPORT_VIR_MACH
#include "dvpp_share_mem_ctrl.h"
#endif

#include "dvpp_vpc_pipe.h"

static int32_t __ka_init dvpp_cmdlist_init(void)
{
#ifdef SUPPORT_VIR_MACH
    uint32_t devid;
#endif
    int32_t ret = 0;

    // 注册三个模块的cmdlist引擎
    jpege_cmdlist_engine_register();
    jpegd_cmdlist_engine_register();
    vpc_cmdlist_engine_register();

    // 注册两种解码器用于解析args
    dvpp_tlv_decoder_register();
    dvpp_vpu_decoder_register();

#ifdef SUPPORT_VPC_PIPE
    // 注册vpc pipe用户配置vpu_config
    dvpp_vpc_pipe_init_export_func();
#endif

    dvpp_get_version_init();
    dvpp_del_chn_hlist_init();
    dvpp_sqe_update_init();
    dvpp_cmdlist_context_init();

#ifdef SUPPORT_VIR_MACH
    // 注册虚拟机上线通知，rc形态 不支持虚拟机
    ret = dvpp_vmng_init();
    if (ret != 0) {
        goto FAIL0;
    }

    for (devid = 0; devid < DVPP_VMNG_DEVICE_NUM_MAX; devid++) {
        ka_task_spin_lock_init(&g_share_mem_pool_lock[devid]);
    }
#endif

    ret = dvpp_cmdlist_dev_init();
    if (ret != 0) {
        goto FAIL1;
    }
    return 0;

FAIL1:
#ifdef SUPPORT_VIR_MACH
    (void)dvpp_vmng_uninit();
#endif
FAIL0:
    dvpp_cmdlist_context_deinit();
    dvpp_sqe_update_uninit();
#ifdef SUPPORT_VPC_PIPE
    dvpp_vpc_pipe_uninit_export_func();
#endif
    dvpp_vpu_decoder_unregister();
    dvpp_tlv_decoder_unregister();
    vpc_cmdlist_engine_unregister();
    jpegd_cmdlist_engine_unregister();
    jpege_cmdlist_engine_unregister();
    dvpp_deinit_mem_allocater();

    return -1;
}

static void __ka_exit dvpp_cmdlist_exit(void)
{
    dvpp_cmdlist_dev_exit();
#ifdef SUPPORT_VIR_MACH
    (void)dvpp_vmng_uninit();
#endif
    dvpp_cmdlist_context_deinit();
    dvpp_sqe_update_uninit();
#ifdef SUPPORT_VPC_PIPE
    dvpp_vpc_pipe_uninit_export_func();
#endif
    dvpp_vpu_decoder_unregister();
    dvpp_tlv_decoder_unregister();
    vpc_cmdlist_engine_unregister();
    jpegd_cmdlist_engine_unregister();
    jpege_cmdlist_engine_unregister();
    dvpp_deinit_mem_allocater();
    return;
}

ka_module_init(dvpp_cmdlist_init);
ka_module_exit(dvpp_cmdlist_exit);

KA_MODULE_LICENSE("GPL");
