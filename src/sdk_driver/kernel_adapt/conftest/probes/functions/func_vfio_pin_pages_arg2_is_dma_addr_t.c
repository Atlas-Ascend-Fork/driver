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

#include <linux/vfio.h>

void conftest_func_vfio_pin_pages_arg2_is_dma_addr_t(void)
{
    struct vfio_device *device = NULL;
    struct page **pages = NULL;
    dma_addr_t iova = 1;

    (void)vfio_pin_pages(device, iova, 0, 0, pages);
}
