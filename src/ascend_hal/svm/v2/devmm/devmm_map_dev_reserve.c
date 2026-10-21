/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include "ascend_hal_define.h"
#include "devmm_svm_init.h"
#include "devmm_svm.h"
#include "svm_ioctl.h"
#include "devmm_map_dev_reserve.h"

struct devmm_map_dev_reserve dev_reserve_addr;

void devmm_init_dev_reserve_addr(uint32_t devid)
{
    if (dev_reserve_addr.inited == 1) {
        return;
    }
    (void)pthread_mutex_init(&dev_reserve_addr.map_dev_lock[devid], NULL);
    dev_reserve_addr.inited = 1;
}

STATIC bool devmm_dev_reserve_addr_is_inited(void)
{
    return dev_reserve_addr.inited == 1;
}

void devmm_get_dev_reserve_addr(uint32_t devid, uint32_t addr_type, uint64_t *addr, uint64_t *size)
{
    *addr = dev_reserve_addr.mapped_dev_reserve_addr[devid][addr_type];
    *size = dev_reserve_addr.mapped_dev_reserve_size[devid][addr_type];
}

STATIC void devmm_set_dev_reserve_addr(uint32_t devid, uint32_t addr_type, uint64_t addr, uint64_t size)
{
    dev_reserve_addr.mapped_dev_reserve_addr[devid][addr_type] = addr;
    dev_reserve_addr.mapped_dev_reserve_size[devid][addr_type] = size;
}

STATIC bool devmm_dev_reserve_addr_is_valid(uint32_t devid, uint32_t addr_type, uint64_t addr, uint64_t size)
{
    return (addr != 0) && (addr == dev_reserve_addr.mapped_dev_reserve_addr[devid][addr_type]) &&
           (size == dev_reserve_addr.mapped_dev_reserve_size[devid][addr_type]);
}

STATIC bool devmm_dev_reserve_addr_is_mapped(uint32_t devid, uint32_t addr_type)
{
    return (dev_reserve_addr.mapped_dev_reserve_addr[devid][addr_type] != 0) &&
           (dev_reserve_addr.mapped_dev_reserve_size[devid][addr_type] > 0);
}

void devmm_reset_dev_reserve_addr(uint32_t devid)
{
    uint32_t i;
    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        return;
    }
    for (i = 0; i < ADDR_MAP_TYPE_MAX; i++) {
        devmm_set_dev_reserve_addr(devid, i, 0, 0);
    }
}

STATIC bool devmm_addr_size_para_is_valid(uint32_t devid, uint32_t addr_type)
{
    return (devid < DEVMM_MAX_PHY_DEVICE_NUM) && (addr_type < ADDR_MAP_TYPE_MAX);
}

STATIC DVresult devmm_check_map_addr_para(struct AddrMapInPara *in_para, size_t param_value_size,
                                          struct AddrMapOutPara *out_para)
{
    (void)param_value_size;
    if ((in_para == NULL) || (out_para == NULL)) {
        DEVMM_DRV_ERR("Parameter is invalid. (in_para=%p; out_para=%p)\n", in_para, out_para);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (!devmm_addr_size_para_is_valid(in_para->devid, in_para->addr_type)) {
        DEVMM_DRV_ERR("Devid or addr_type is invalid. (devid=%u; addr_type=%u)\n", in_para->devid, in_para->addr_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC DVresult devmm_check_unmap_addr_para(struct AddrUnmapInPara *in_para, size_t param_value_size)
{
    if (in_para == NULL || param_value_size != sizeof(struct AddrUnmapInPara)) {
        DEVMM_DRV_ERR("Parameter is invalid. (in_para=%p; param_value_size=%lu)\n", in_para, param_value_size);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (!devmm_addr_size_para_is_valid(in_para->devid, in_para->addr_type)) {
        DEVMM_DRV_ERR("Devid or addr_type is invalid. (devid=%u; addr_type=%u)\n", in_para->devid, in_para->addr_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_map_mem(struct AddrMapInPara *in_para, struct AddrMapOutPara *out_para)
{
    struct devmm_ioctl_arg arg;
    DVresult ret;

    /* device reserve addr just alloc once to improved performance, not free */
    if (devmm_dev_reserve_addr_is_mapped(in_para->devid, in_para->addr_type)) {
        devmm_get_dev_reserve_addr(in_para->devid, in_para->addr_type, (uint64_t *)&out_para->ptr,
                                   (uint64_t *)&out_para->len);
        DEVMM_DRV_DEBUG_ARG("Address has been mapped. (map_ptr=0x%llx; len=%lld)\n", out_para->ptr, out_para->len);
        return DRV_ERROR_NONE;
    }

    arg.head.devid = in_para->devid;
    arg.data.map_dev_reserve_para.addr_type = in_para->addr_type;
    arg.data.map_dev_reserve_para.va = 0;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MAP_DEV_RESERVE, &arg);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Map address ioctl error. (devid=%u; ret=%d)\n",
                         in_para->devid, ret);
        return ret;
#endif
    }

    out_para->ptr = arg.data.map_dev_reserve_para.va;
    out_para->len = arg.data.map_dev_reserve_para.len;

    devmm_set_dev_reserve_addr(in_para->devid, in_para->addr_type, out_para->ptr, out_para->len);

    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_map_addr_restore(struct AddrMapInPara *in_para)
{
    struct devmm_ioctl_arg arg;
    struct AddrMapOutPara mapped_addr;
    DVresult ret;

    if (!devmm_dev_reserve_addr_is_mapped(in_para->devid, in_para->addr_type)) {
        return DRV_ERROR_NONE;
    }

    devmm_get_dev_reserve_addr(in_para->devid, in_para->addr_type, (uint64_t *)&mapped_addr.ptr,
                               (uint64_t *)&mapped_addr.len);

    arg.head.devid = in_para->devid;
    arg.data.map_dev_reserve_para.addr_type = in_para->addr_type;
    arg.data.map_dev_reserve_para.va = mapped_addr.ptr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MAP_DEV_RESERVE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Map address ioctl error. (devid=%u; ret=%d)\n",
                         in_para->devid, ret);
        return ret;
    }
#ifdef EMU_ST
    devmm_set_dev_reserve_addr(in_para->devid, in_para->addr_type, arg.data.map_dev_reserve_para.va,
                               arg.data.map_dev_reserve_para.len);
#endif
    DEVMM_RUN_INFO("Restore ctrl_map_addr succ. (map_ptr=0x%llx; len=%lld; devid=%u; type=%u)\n",
                   arg.data.map_dev_reserve_para.va, arg.data.map_dev_reserve_para.len, in_para->devid,
                   in_para->addr_type);
    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_unmap_mem(struct AddrUnmapInPara *in_para)
{
    /* device reserve addr just alloc once to improved performance, not free */
    if (!devmm_dev_reserve_addr_is_valid(in_para->devid, in_para->addr_type, in_para->ptr, in_para->len)) {
        DEVMM_DRV_ERR("Device address or size error. (addr=0x%llx; len=%llu; addr_type=%u; devid=%u)\n", in_para->ptr,
                      in_para->len, in_para->addr_type, in_para->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static DVresult (*devmm_ctrl_map_handlers[ADDR_MAP_TYPE_MAX])(struct AddrMapInPara *in_para,
                                                              struct AddrMapOutPara *out_para) = {
    [ADDR_MAP_TYPE_L2_BUFF] = devmm_ctrl_map_mem,
    [ADDR_MAP_TYPE_REG_C2C_CTRL] = devmm_ctrl_map_mem,
    [ADDR_MAP_TYPE_REG_AIC_CTRL] = devmm_ctrl_map_mem,
    [ADDR_MAP_TYPE_REG_AIC_PMU_CTRL] = devmm_ctrl_map_mem,
    [ADDR_MAP_TYPE_REG_CNT_NOTIFY_CTRL] = devmm_ctrl_map_mem,
};

static DVresult (*devmm_ctrl_unmap_handlers[ADDR_MAP_TYPE_MAX])(struct AddrUnmapInPara *in_para) = {
    [ADDR_MAP_TYPE_L2_BUFF] = devmm_ctrl_unmap_mem,
    [ADDR_MAP_TYPE_REG_C2C_CTRL] = devmm_ctrl_unmap_mem,
    [ADDR_MAP_TYPE_REG_CNT_NOTIFY_CTRL] = devmm_ctrl_unmap_mem,
};

DVresult devmm_ctrl_map_addr(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    (void)out_size_ret;
    struct AddrMapInPara *in_para = (struct AddrMapInPara *)param_value;
    struct AddrMapOutPara *out_para = (struct AddrMapOutPara *)out_value;
    DVresult ret = DRV_ERROR_INVALID_VALUE;

    if (devmm_check_map_addr_para(in_para, param_value_size, out_para) != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Check map address parameter is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (((in_para->addr_type == ADDR_MAP_TYPE_REG_AIC_CTRL) ||
         (in_para->addr_type == ADDR_MAP_TYPE_REG_AIC_PMU_CTRL)) &&
        devmm_is_split_mode()) {
        DEVMM_RUN_INFO("Aic reg map not support in split mode.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (!devmm_dev_reserve_addr_is_inited()) {
        DEVMM_DRV_ERR("Dev_reserve_addr isn't inited, please call open device first. (devid=%u)\n", in_para->devid);
        return DRV_ERROR_INVALID_HANDLE;
    }
    (void)pthread_mutex_lock(&dev_reserve_addr.map_dev_lock[in_para->devid]);
    if (devmm_ctrl_map_handlers[in_para->addr_type] != NULL) {
        ret = devmm_ctrl_map_handlers[in_para->addr_type](in_para, out_para);
    }
    (void)pthread_mutex_unlock(&dev_reserve_addr.map_dev_lock[in_para->devid]);

    DEVMM_DRV_DEBUG_ARG("Argument. (ret=%d; map_ptr=0x%llx; len=%lld; devid=%u; addr_type=%u)\n", ret, out_para->ptr,
                        out_para->len, in_para->devid, in_para->addr_type);

    return ret;
}

DVresult devmm_ctrl_unmap_addr(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    (void)out_value;
    (void)out_size_ret;
    struct AddrUnmapInPara *in_para = (struct AddrUnmapInPara *)param_value;
    DVresult ret = DRV_ERROR_INVALID_VALUE;

    if (devmm_check_unmap_addr_para(in_para, param_value_size) != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Check unmap address parameter is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (!devmm_dev_reserve_addr_is_inited()) {
        DEVMM_DRV_ERR("Dev_reserve_addr isn't inited, please call open device first. (devid=%u)\n", in_para->devid);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (devmm_ctrl_unmap_handlers[in_para->addr_type] != NULL) {
        ret = devmm_ctrl_unmap_handlers[in_para->addr_type](in_para);
    }

    DEVMM_DRV_DEBUG_ARG("Argument. (unmap_ptr=0x%llx; len=%lld; devid=%u)\n", in_para->ptr, in_para->len,
                        in_para->devid);
    return ret;
}

static DVresult devmm_restore_ctrl_map_for_device(uint32_t devid)
{
    enum addrMapType addr_map_type;
    struct AddrMapInPara in_para;
    DVresult ret;

    in_para.devid = devid;

    (void)pthread_mutex_lock(&dev_reserve_addr.map_dev_lock[devid]);
    for (addr_map_type = ADDR_MAP_TYPE_L2_BUFF; addr_map_type < ADDR_MAP_TYPE_MAX; addr_map_type++) {
        in_para.addr_type = addr_map_type;
        ret = devmm_ctrl_map_addr_restore(&in_para);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&dev_reserve_addr.map_dev_lock[devid]);
            return ret;
        }
    }
    (void)pthread_mutex_unlock(&dev_reserve_addr.map_dev_lock[devid]);

    return DRV_ERROR_NONE;
}

DVresult devmm_ctrl_map_mem_restore(void)
{
    DVresult ret;

    if (!devmm_dev_reserve_addr_is_inited()) {
        DEVMM_DRV_ERR("Dev_reserve_addr isn't inited, please call open device first.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    for (uint32_t devid = 0; devid < DEVMM_MAX_PHY_DEVICE_NUM; devid++) {
        ret = devmm_restore_ctrl_map_for_device(devid);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}
