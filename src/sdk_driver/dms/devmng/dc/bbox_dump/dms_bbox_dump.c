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

#include "dms_bbox_dump.h"
#include "pbl/pbl_feature_loader.h"
#include "dms_template.h"
#include "dms/dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "urd_acc_ctrl.h"
#include "comm_kernel_interface.h"
#include "pbl/pbl_soc_res.h"
#ifdef CFG_FEATURE_UB
#include "dms_bbox_type_mng.h"
#endif

STATIC enum bbox_data_type g_bbox_dump_type_pcie[] = {MEM_TYPE_TS_LOG, MEM_TYPE_IMP_LOG, MEM_TYPE_IMU_LOG,
                                                      MEM_TYPE_HSM_LOG};
STATIC enum bbox_data_type g_bbox_dump_type_ub[] = {MEM_TYPE_VMCORE_FILE, MEM_TYPE_TS_LOG, MEM_TYPE_IMP_LOG,
                                                    MEM_TYPE_IMU_LOG, MEM_TYPE_HSM_LOG};

STATIC struct bbox_dump_type_info g_bbox_dump_info[] = {
    [MEM_TYPE_TS_LOG] = {BBOX_DUMP_TS_LOG_ADDRESS, BBOX_DUMP_TS_LOG_SIZE},
    [MEM_TYPE_IMP_LOG] = {BBOX_DUMP_IMP_LOG_ADDRESS, BBOX_DUMP_IMP_LOG_SIZE},
    [MEM_TYPE_IMU_LOG] = {BBOX_DUMP_IMU_LOG_ADDRESS, BBOX_DUMP_IMU_LOG_SIZE},
    [MEM_TYPE_HSM_LOG] = {BBOX_DUMP_HSM_LOG_ADDRESS, BBOX_DUMP_HSM_LOG_SIZE},
};

enum bbox_data_type *get_bbox_dump_type(u32 dev_id)
{
    if (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_UB) {
        return g_bbox_dump_type_ub;
    }
    return g_bbox_dump_type_pcie;
}

int get_bbox_dump_type_size(u32 dev_id)
{
    if (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_UB) {
        return sizeof(g_bbox_dump_type_ub) / sizeof(enum bbox_data_type);
    }
    return sizeof(g_bbox_dump_type_pcie) / sizeof(enum bbox_data_type);
}

struct bbox_dump_type_info *get_bbox_dump_info(void)
{
    return g_bbox_dump_info;
}

int get_bbox_dump_info_size(void)
{
    return sizeof(g_bbox_dump_info) / sizeof(struct bbox_dump_type_info);
}

struct region_type_info {
    unsigned int base_addr;
    unsigned int size;
    void *(*alloc_func)(unsigned long addr, unsigned int size);
    void (*free_func)(struct dev_mem_region *region);
};

STATIC const struct region_type_info g_region_info_tbl[] = {
    [DEVDRV_RAO_CLIENT_BBOX_DDR] =
        {
            .base_addr = DDR_BASE_ADDR,
            .size = DDR_TOTAL_SIZE,
            .alloc_func = bbox_alloc_region_mem,
            .free_func = bbox_free_region_mem,
        },
    [DEVDRV_RAO_CLIENT_BBOX_SRAM] =
        {
            .base_addr = SRAM_BASE_ADDR,
            .size = SRAM_TOTAL_SIZE,
            .alloc_func = bbox_alloc_region_mem,
            .free_func = bbox_free_region_mem,
        },
    [DEVDRV_RAO_CLIENT_BBOX_KLOG] =
        {
            .base_addr = KLOG_BASE_ADDR,
            .size = KLOG_TOTAL_SIZE,
            .alloc_func = bbox_alloc_klog_mem,
            .free_func = bbox_free_klog_mem,
        },
};

STATIC struct mem_mng dev_ctx[ASCEND_DEV_MAX_NUM] = {0};

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BBOX_DUMP)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_BBOX_DUMP, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_BBOX_DATA, NULL, NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_get_bbox_data)

ADD_FEATURE_COMMAND(DMS_MODULE_BBOX_DUMP, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_SET_BBOX_DATA, NULL, NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_set_bbox_data)

END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

struct mem_mng *bbox_mem_mng_get(unsigned int dev_id)
{
    return &dev_ctx[dev_id];
}

STATIC struct dev_mem_region *bbox_get_mem_region(struct mem_mng *ctx, enum devdrv_rao_client_type type)
{
    switch (type) {
        case DEVDRV_RAO_CLIENT_BBOX_DDR:
            return &ctx->ddr;
        case DEVDRV_RAO_CLIENT_BBOX_SRAM:
            return &ctx->sram;
        case DEVDRV_RAO_CLIENT_BBOX_KLOG:
            return &ctx->klog;
        default:
            return NULL;
    }
}

int bbox_set_mem(u32 udevid, u32 log_type, void *kva, u32 size)
{
    int ret = 0;
    ret = bbox_device_set_mem_platform(udevid, log_type, kva, size);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(bbox_set_mem);

void bbox_persistent_unregister(u32 udevid, u32 log_type)
{
    bbox_device_persistent_unregister(udevid, log_type);
}
KA_EXPORT_SYMBOL_GPL(bbox_persistent_unregister);

STATIC void bbox_free_and_unregister(u32 udevid, enum devdrv_rao_client_type type, struct mem_mng *ctx)
{
    struct dev_mem_region *mem_region = NULL;
    int ret;

    mem_region = bbox_get_mem_region(ctx, type);
    if (mem_region == NULL) {
        dms_err("Failed to get mem region. (dev_id=%u; type=%d)\n", udevid, type);
        return;
    }

    if (mem_region->reg_flag == false) {
        dms_warn("This type is not registered. (dev_id=%u; type=%d)\n", udevid, type);
        return;
    }

    ret = devdrv_unregister_rao_client(udevid, type);
    if (ret != 0) {
        dms_err("Unregister rao failed. (dev_id=%u; type=%d; ret=%d)\n", udevid, type, ret);
    }
    mem_region->free_func(mem_region);
    mem_region->reg_flag = false;
}

STATIC int bbox_uninit_instance(u32 udevid)
{
    struct mem_mng *ctx = bbox_mem_mng_get(udevid);
    enum devdrv_rao_client_type type;
    int ret = 0, i, type_size;
    enum bbox_data_type *data_type;

    if (devdrv_get_connect_protocol(udevid) != CONNECT_PROTOCOL_UB) {
        bbox_dma_addrs_uninit(udevid);
        return 0;
    }
    data_type = get_bbox_dump_type(udevid);
    type_size = get_bbox_dump_type_size(udevid);
    for (i = 0; i < type_size; i++) {
        bbox_dump_ctx_uninit(udevid, (u32)data_type[i]);
    }
    ka_task_down_write(&ctx->sem);
    for (type = DEVDRV_RAO_CLIENT_BBOX_DDR; type <= DEVDRV_RAO_CLIENT_BBOX_KLOG; type++) {
        bbox_free_and_unregister(udevid, type, ctx);
    }
    ka_task_up_write(&ctx->sem);
    ret |= bbox_device_unregister_persistent_export(udevid, ctx);

    dms_info("Bbox uninit instance successfully. (dev_id=%u)\n", udevid);
    return ret;
}

STATIC void bbox_alloc_and_register(u32 udevid, enum devdrv_rao_client_type type, struct mem_mng *ctx)
{
    const struct region_type_info *info = NULL;
    struct dev_mem_region *mem_region = NULL;
    unsigned int chip_type;
    int ret;

    if (type >= KA_BASE_ARRAY_SIZE(g_region_info_tbl)) {
        dms_err("Invalid region type. (dev_id=%u; type=%d)\n", udevid, type);
        return;
    }
    info = &g_region_info_tbl[type];

    mem_region = bbox_get_mem_region(ctx, type);
    if (mem_region == NULL) {
        dms_err("Failed to get mem region. (dev_id=%u; type=%d)\n", udevid, type);
        return;
    }

    chip_type = uda_get_chip_type(udevid);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        dms_err("Unknown chip type. (dev_id=%u)\n", udevid);
        return;
    }

    if ((chip_type == HISI_CLOUD_V5) && (type == DEVDRV_RAO_CLIENT_BBOX_SRAM)) {
        mem_region->base_addr = SRAM_BASE_ADDR_V5;
    } else {
        mem_region->base_addr = info->base_addr;
    }
    mem_region->size = info->size;
    mem_region->alloc_func = info->alloc_func;
    mem_region->free_func = info->free_func;
    mem_region->va = mem_region->alloc_func(mem_region->base_addr, mem_region->size);
    if (mem_region->va == NULL) {
        dms_err("Alloc mem failed. (dev_id=%u; type=%d; size=%u)\n", udevid, type, mem_region->size);
        return;
    }

    ret = devdrv_register_rao_client(udevid, type, (u64)(uintptr_t)mem_region->va, mem_region->size,
                                     DEVDRV_RAO_PERM_RMT_READ);
    if (ret != 0) {
        mem_region->free_func(mem_region);
        dms_err("Register rao failed. (dev_id=%d; type=%d; ret=%d)\n", udevid, type, ret);
        return;
    }

    mem_region->reg_flag = true;
    return;
}

STATIC int bbox_init_instance(u32 udevid)
{
    struct mem_mng *ctx = bbox_mem_mng_get(udevid);
    enum devdrv_rao_client_type type;
    int ret = 0, i, type_size;
    enum bbox_data_type *data_type;

    if (devdrv_get_connect_protocol(udevid) != CONNECT_PROTOCOL_UB) {
        return bbox_dma_addrs_init(udevid);
    }
    ka_task_down_write(&ctx->sem);
    for (type = DEVDRV_RAO_CLIENT_BBOX_DDR; type <= DEVDRV_RAO_CLIENT_BBOX_KLOG; type++) {
        bbox_alloc_and_register(udevid, type, ctx);
    }
    ka_task_up_write(&ctx->sem);
    ret = bbox_device_register_persistent_export(udevid, ctx);
    if (ret != 0) {
        return ret;
    }
    data_type = get_bbox_dump_type(udevid);
    type_size = get_bbox_dump_type_size(udevid);
    for (i = 0; i < type_size; i++) {
        ret = bbox_dump_ctx_init(udevid, (u32)data_type[i]);
        if (ret != 0) {
            dms_err("Failed to init dump ctx. (dev_id=%u; type=%u; ret=%d)\n", udevid, (u32)data_type[i], ret);
        }
    }
    dms_info("Bbox init instance successfully. (dev_id=%u)\n", udevid);
    return 0;
}

STATIC int bbox_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid para. (dev_id=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = bbox_init_instance(udevid);
    } else if (action == UDA_UNINIT) {
        ret = bbox_uninit_instance(udevid);
    }

    dms_info("Bbox notifier action. (dev_id=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

void dms_bbox_sem_init(void)
{
    int i;
    struct mem_mng *ctx = NULL;

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        ctx = bbox_mem_mng_get(i);
        ka_task_init_rwsem(&ctx->sem);
        ctx->uda_init_flag = BBOX_UNDONE;
    }
}

int dms_bbox_dump_init(void)
{
    struct uda_dev_type type;
    int ret;

    CALL_INIT_MODULE(DMS_MODULE_BBOX_DUMP);

    dms_bbox_sem_init();
#ifdef CFG_FEATURE_UB
    bbox_ub_dump_ctx_init();
#endif
    bbox_uda_davinci_type_pack(&type, true);
    ret = uda_notifier_register(DMS_MODULE_BBOX_DUMP, &type, UDA_PRI2, bbox_notifier_func);
    if (ret != 0) {
        CALL_EXIT_MODULE(DMS_MODULE_BBOX_DUMP);
        dms_err("Register notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = bbox_client_init();
    if (ret != 0) {
        (void)uda_notifier_unregister(DMS_MODULE_BBOX_DUMP, &type);
        CALL_EXIT_MODULE(DMS_MODULE_BBOX_DUMP);
        dms_err("Failed to init client. (ret=%d)\n", ret);
        return ret;
    }

    dms_info("BBox module initialized.\n");
    return ret;
}
DECLAER_FEATURE_AUTO_INIT(dms_bbox_dump_init, FEATURE_LOADER_STAGE_5);

void dms_bbox_dump_uninit(void)
{
    struct uda_dev_type type;

    bbox_client_uninit();
    bbox_uda_davinci_type_pack(&type, false);
    (void)uda_notifier_unregister(DMS_MODULE_BBOX_DUMP, &type);
    CALL_EXIT_MODULE(DMS_MODULE_BBOX_DUMP);

    dms_info("BBox module deinitialize.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_bbox_dump_uninit, FEATURE_LOADER_STAGE_5);