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

#include "pbl/pbl_feature_loader.h"

#include "dms_init.h"
#include "ka_kernel_def_pub.h"
#include "dms_module.h"

static struct sub_module_ops g_sub_table[] = {
    {dms_init, dms_exit},
};

STATIC int KA_MODULE_INIT dms_module_init(void)
{
    int index, ret;
    int table_size = sizeof(g_sub_table) / sizeof(struct sub_module_ops);

    for (index = 0; index < table_size; index++) {
        ret = g_sub_table[index].init();
        if (ret != 0) {
            goto out;
        }
    }

    return 0;
out:
    for (; index > 0; index--) {
        g_sub_table[index - 1].uninit();
    }
    return ret;
}

STATIC void KA_MODULE_EXIT dms_module_exit(void)
{
    int index;
    int table_size = sizeof(g_sub_table) / sizeof(struct sub_module_ops);

    for (index = table_size; index > 0; index--) {
        g_sub_table[index - 1].uninit();
    }
}

ka_module_init(dms_module_init);
ka_module_exit(dms_module_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");
