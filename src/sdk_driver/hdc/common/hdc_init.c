/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#include "ka_compiler_pub.h"
#include "comm_kernel_interface.h"
#include "hdc_init.h"
#include "hdcdrv_log.h"

int __attribute__((weak)) hdcdrv_ub_init_module(void)
{
    return 0;
}
void __attribute__((weak)) hdcdrv_ub_exit_module(void)
{}

STATIC int __ka_init init_hdc(void)
{
    int ret;

    ret = hdcdrv_pcie_init_module();
    if (ret != 0) {
        return ret;
    }

    ret = hdcdrv_ub_init_module();
    if (ret != 0) {
        hdcdrv_pcie_exit_module();
    }

    return ret;
}

STATIC void __ka_exit exit_hdc(void)
{
    hdcdrv_ub_exit_module();
    hdcdrv_pcie_exit_module();
}

ka_module_init(init_hdc);
ka_module_exit(exit_hdc);
KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");