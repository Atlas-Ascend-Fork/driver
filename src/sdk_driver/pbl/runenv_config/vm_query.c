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
#include "ka_driver_pub.h"
#if defined(CONFIG_X86_64) && defined(CONFIG_ARCH_HAS_CC_PLATFORM)
#include "cc_platform.h"
#endif

#include "runenv_config_module.h"
#include "vm_query.h"

#define DMI_VIRTUAL_SYSVENDOR "QEMU"
#define DMI_VIRTUAL_PRODUCT_NAME "KVM Virtual Machine"

bool run_in_virtual_mach(void)
{
    if (ka_driver_dmi_match(DMI_SYS_VENDOR, DMI_VIRTUAL_SYSVENDOR)) {
        recfg_debug("In virtual_mach.\n");
        return true;
    }
    return false;
}
KA_EXPORT_SYMBOL_GPL(run_in_virtual_mach);

bool run_in_confidential_virtual_mach(void)
{
#if defined(CONFIG_X86_64) && defined(CONFIG_ARCH_HAS_CC_PLATFORM)
    return cc_platform_has(CC_ATTR_GUEST_MEM_ENCRYPT);
#else
    return false;
#endif
}
KA_EXPORT_SYMBOL_GPL(run_in_confidential_virtual_mach);
