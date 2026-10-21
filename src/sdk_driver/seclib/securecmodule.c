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

#include <linux/module.h>
#include <linux/kernel.h>

int __init SecureCModuleInit(void)
{
    return 0;
}

void __exit SecureCModuleExit(void)
{
    return;
}

module_init(SecureCModuleInit);
module_exit(SecureCModuleExit);

MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("secure c library for ko");
MODULE_ALIAS("host-kernel-seclib");
MODULE_LICENSE("GPL");
