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

#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "dms_bbox_dump.h"
#include "pbl/pbl_feature_loader.h"
#include "dms_template.h"
#include "dms/dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "urd_acc_ctrl.h"
#include "devdrv_manager.h"
#include "pbl_uda.h"
#include "devdrv_pcie.h"
#include "pbl/pbl_soc_res.h"
#include "comm_pcie.h"
#ifdef CFG_FEATURE_UB
#include "dms_bbox_type_mng.h"
#endif
#include "dms_bbox_h2d_msg.h"
#include "dms_bbox_dump_feature_check.h"

#define VMCORE_FILE_MAX_SIZE 0x100000U // 1M
#define RETRY_INTERVAL_MS 5000         // 5s

#ifndef DEVDRV_DUMP_SINGLE_LEN
#define DEVDRV_DUMP_SINGLE_LEN 0x200000UL
#endif

struct ub_dump_select {
    enum bbox_data_type data_type;
    struct dev_mem_region *region;
    enum devdrv_rao_client_type client_type;
    u32 offset;
};

struct pcie_dump_select {
    enum bbox_data_type data_type;
    enum log_slog_mem_type log_type;
    u32 mem_offset;
    int (*pcie_dump_func)(struct bbox_dma_dump *dma_dump);
};

struct reg_work_item {
    ka_delayed_work_t reg_work;
    u32 dev_id;
};

STATIC struct bbox_dump_type_info g_host_dma_infos[ASCEND_DEV_MAX_NUM][MEM_CTRL_TYPE_MAX] = {0};
STATIC struct reg_work_item g_reg_work[ASCEND_DEV_MAX_NUM];
STATIC int dms_get_bbox_data_para_check(char *in, unsigned int in_len)
{
    bbox_data_info_t *bbox_info = NULL;

    if ((in == NULL) || (in_len != sizeof(bbox_data_info_t))) {
        dms_err("Invalid input buffer. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    bbox_info = (bbox_data_info_t *)in;
    if ((bbox_info->dev_id >= DEVDRV_PF_DEV_MAX_NUM) || (bbox_info->type >= MEM_CTRL_TYPE_MAX)) {
        dms_err("Invalid devid or type. (dev_id=%u; type=%u)\n", bbox_info->dev_id, bbox_info->type);
        return -EINVAL;
    }

    if ((bbox_info->bbox_data_buf == NULL) || (bbox_info->len == 0)) {
        dms_err("Invalid paras. (dev_id=%u; len=%u)\n", bbox_info->dev_id, bbox_info->len);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_get_bbox_addr_info(u32 dev_id, enum bbox_data_type type, u64 *addr, size_t *size)
{
    int ret = 0;
    switch (type) {
        case MEM_TYPE_PCIE_SRAM:
            ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_LOAD_RAM, 0, addr, size);
            break;
        case MEM_TYPE_PCIE_DDR:
        case MEM_TYPE_BBOX_HDR:
        case MEM_TYPE_CHIP_LOG_PCIE_BAR:
        case MEM_TYPE_BBOX_PCIE_BAR:
        case MEM_TYPE_KDUMP_MAGIC:
        case MEM_TYPE_VMCORE_STAT:
            ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_BBOX_BASE, 0, addr, size);
            break;
        case MEM_TYPE_HBOOT_SRAM:
            ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_HBOOT_SRAM_MEM, 0, addr, size);
            break;
        default:
            dms_err("Parameter type is invalid. (type=%d)\n", (int)type);
            ret = -EINVAL;
            break;
    }

    return ret;
}

int dms_bar_bbox_dump(struct bbox_dma_dump *dma_dump)
{
    int ret;
    u64 phy_addr;
    size_t phy_addr_size;
    void *tmp_buf = NULL;
    void __ka_mm_iomem *vir_addr = NULL;
    enum bbox_data_type type = (enum bbox_data_type)dma_dump->data_type;

    ret = dms_get_bbox_addr_info(dma_dump->dev_id, type, &phy_addr, &phy_addr_size);
    if (ret != 0) {
        dms_err("Get bar log addr failed. (dev_id=%u; type=%d; ret=%d)\n", dma_dump->dev_id, type, ret);
        return ret;
    }

    if (((u64)dma_dump->offset > KA_U64_MAX - phy_addr) || ((u64)dma_dump->offset >= (u64)phy_addr_size) ||
        ((u64)dma_dump->len > (u64)phy_addr_size - (u64)dma_dump->offset)) {
        dms_err("Para offset len check failed. (dev_id=%u; log_type=%d; offset=0x%x; len=0x%x; max_offset=0x%lx)\n",
                dma_dump->dev_id, type, dma_dump->offset, dma_dump->len, phy_addr_size);
        return -EINVAL;
    }

    tmp_buf = ka_mm_kvzalloc(dma_dump->len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (tmp_buf == NULL) {
        dms_err("Failed to allocate buffer. (dev_id=%u; len=%u)\n", dma_dump->dev_id, dma_dump->len);
        return -ENOMEM;
    }

    vir_addr = ka_mm_ioremap(phy_addr + dma_dump->offset, dma_dump->len);
    if (vir_addr == NULL) {
        dms_err("Failed to invoke the ioremap. (dev_id=%u; len=%u)\n", dma_dump->dev_id, dma_dump->len);
        ka_mm_kvfree(tmp_buf);
        return -ENOMEM;
    }

    ka_mm_memcpy_fromio(tmp_buf, vir_addr, dma_dump->len);
    ret = copy_to_user_safe(dma_dump->dst_buf, tmp_buf, dma_dump->len);
    if (ret != 0) {
        dms_err("Copy to user failed. (dev_id=%u; len=%u; ret=%d)\n", dma_dump->dev_id, dma_dump->len, ret);
    }

    ka_mm_iounmap(vir_addr);
    vir_addr = NULL;
    ka_mm_kvfree(tmp_buf);
    return ret;
}

STATIC int dms_bbox_region_get(struct mem_mng *ctx, unsigned int type, unsigned int *offset,
                               const struct dev_mem_region **region, enum devdrv_rao_client_type *client_type)
{
    int i;
    const struct ub_dump_select region_map[] = {
        {MEM_TYPE_PCIE_SRAM, &ctx->sram, DEVDRV_RAO_CLIENT_BBOX_SRAM, *offset},
        {MEM_TYPE_PCIE_DDR, &ctx->klog, DEVDRV_RAO_CLIENT_BBOX_KLOG, *offset},
        {MEM_TYPE_BBOX_DDR, &ctx->ddr, DEVDRV_RAO_CLIENT_BBOX_DDR, DDR_BASE_OFFSET + *offset},
        {MEM_TYPE_BBOX_HDR, &ctx->ddr, DEVDRV_RAO_CLIENT_BBOX_DDR, DDR_HDR_OFFSET},
        {MEM_TYPE_REG_DDR, &ctx->ddr, DEVDRV_RAO_CLIENT_BBOX_DDR, DDR_REG_OFFSET + *offset},
        {MEM_TYPE_DEBUG_OS_LOG, &ctx->persistent.booked, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS, *offset},
        {MEM_TYPE_SEC_LOG, &ctx->persistent.sec_log, DEVDRV_RAO_CLIENT_BBOX_SEC_OS, *offset},
        {MEM_TYPE_RUN_OS_LOG, &ctx->persistent.booked, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS, *offset},
        {MEM_TYPE_RUN_EVENT_LOG, &ctx->persistent.booked, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS, *offset},
        {MEM_TYPE_DEBUG_DEV_LOG, &ctx->persistent.debug_dev_log, DEVDRV_RAO_CLIENT_BBOX_DEBUG_DEV, *offset},
        {MEM_TYPE_KDUMP_MAGIC, &ctx->ddr, DEVDRV_RAO_CLIENT_BBOX_DDR, KDUMP_MAGIC_OFFSET},
        {MEM_TYPE_VMCORE_STAT, &ctx->ddr, DEVDRV_RAO_CLIENT_BBOX_DDR, VMCORE_STAT_OFFSET},
    };

    for (i = 0; i < KA_BASE_ARRAY_SIZE(region_map); i++) {
        if (region_map[i].data_type == type) {
            *region = region_map[i].region;
            *client_type = region_map[i].client_type;
            *offset = region_map[i].offset;
            return 0;
        }
    }

    return -EOPNOTSUPP;
}

STATIC int dms_get_bbox_data_ub(unsigned int dev_id, unsigned int type, unsigned int offset, unsigned int len,
                                void *buf)
{
    const struct dev_mem_region *region = NULL;
    enum devdrv_rao_client_type client_type;
    int ret = 0;
    struct mem_mng *ctx = bbox_mem_mng_get(dev_id);

#ifdef CFG_FEATURE_UB
    enum bbox_data_type *data_type;
    int i, type_size;
    data_type = get_bbox_dump_type(dev_id);
    type_size = get_bbox_dump_type_size(dev_id);
    for (i = 0; i < type_size; i++) {
        if (type == data_type[i]) {
            return bbox_ub_get_data(dev_id, type, offset, len, buf);
        }
    }
#endif

    ka_task_down_read(&ctx->sem);
    ret = dms_bbox_region_get(ctx, type, &offset, &region, &client_type);
    if (ret != 0) {
        dms_warn("Invalid type. (dev_id=%u; type=%d)\n", dev_id, type);
        goto fail_out;
    }

    if ((region == NULL) || (region->va == NULL) || (region->reg_flag == false)) {
        dms_warn("Invalid type or unmapped region. (dev_id=%u; type=%d; region_is_null=%d)\n", dev_id, type,
                 (int)(region == NULL));
        ret = -EOPNOTSUPP;
        goto fail_out;
    }

    if ((offset >= region->size) || (len > region->size - offset)) {
        dms_err("Para offset is invalid. (dev_id=%u; type=%d; offset=0x%x; len=0x%x)\n", dev_id, type, offset, len);
        ret = -EINVAL;
        goto fail_out;
    }

    ret = devdrv_rao_read(dev_id, client_type, offset, len);
    if (ret != 0) {
        dms_err("Read rao data fail. (dev_id=%u; type=%d; offset=0x%x; len=0x%x; ret=%d)\n", dev_id, type, offset, len,
                ret);
        goto fail_out;
    }

    if (copy_to_user_safe(buf, (u8 *)region->va + offset, len) != 0) {
        dms_err("Copy to user failed. (type=%d; len=0x%x)\n", type, len);
        ret = -EFAULT;
        goto fail_out;
    }

fail_out:
    ka_task_up_read(&ctx->sem);
    return ret;
}

STATIC int dma_bbox_dump_para_check(unsigned int dev_id, void *value)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid device id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (!devdrv_manager_is_pf_device(dev_id)) {
        return -EOPNOTSUPP;
    }
    if (value == NULL) {
        dms_err("Invalid buffer. (dev_id=%u)\n", dev_id);
        return -EFAULT;
    }
    return 0;
}
STATIC int bbox_dma_copy_info(struct bbox_dma_dump *dma_dump, struct devdrv_info *dev_info, u64 dump_dma_addr,
                              ka_dma_addr_t host_addr_dma, void *buffer)
{
    int count, i, ret;
    void *dst_buf;
    u64 dma_addr, copy_size;
    u32 dev_id;

    dma_addr = dump_dma_addr;
    dev_id = dma_dump->dev_id;
    count = KA_BASE_DIV_ROUND_UP(dma_dump->len, DEVDRV_DUMP_SINGLE_LEN);
    dst_buf = dma_dump->dst_buf;
    for (i = 0; i < count; i++) {
        copy_size = DEVDRV_DUMP_SINGLE_LEN;
        if (i == (count - 1)) {
            copy_size = dma_dump->len - (count - 1) * DEVDRV_DUMP_SINGLE_LEN;
        }
        ret = hal_kernel_devdrv_dma_sync_copy(dev_info->pci_dev_id, DEVDRV_DMA_DATA_COMMON, (u64)dma_addr,
                                              (u64)host_addr_dma, copy_size, DEVDRV_DMA_DEVICE_TO_HOST);
        if (ret != 0) {
            dms_err("Dma sync copy failed. (ret=%d, devid=%u, len=%u, copy_size=%llu, idx=%d)\n", ret, dev_id,
                    dma_dump->len, copy_size, i);
            return ret;
        }
        ret = copy_to_user_safe(dst_buf, buffer, copy_size);
        if (ret != 0) {
            dms_err("copy_to_user_safe fail. (ret=%d, dev_id=%u, len=%u, copy_size=%llu, idx=%d)\n", ret, dev_id,
                    dma_dump->len, copy_size, i);
            return ret;
        }
        dma_addr += copy_size;
        dst_buf += copy_size;
    }
    return 0;
}
STATIC int bbox_check_dev_info(struct devdrv_info *dev_info)
{
    if (dev_info == NULL) {
        dms_err("No device.\n");
        return -ENODEV;
    }
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        dms_warn("Device has been reset. (dev_id=%u)\n", (u32)dev_info->dev_id);
        return -EINVAL;
    }
    return 0;
}
STATIC int bbox_check_dma_info(struct bbox_dma_dump *dma_dump, u64 dump_size, u64 dump_dma_addr)
{
    if (dump_size <= 0 || dump_dma_addr <= 0) {
        dms_err("Bbox dma info is invalid. (type=%u, addr_size=%llu)\n", (u32)dma_dump->data_type, dump_size);
        return -EINVAL;
    }
    if ((dma_dump->len <= 0) || (dma_dump->offset >= dump_size) || (dma_dump->len > dump_size - dma_dump->offset)) {
        dms_err("Invalid size. (len=%u, offset=%u, mem_size=%llu)\n", dma_dump->len, dma_dump->offset, dump_size);
        return -EINVAL;
    }
    return 0;
}
STATIC int bbox_dma_dump_pcie(struct bbox_dma_dump *dma_dump)
{
    int ret = 0;
    struct devdrv_info *dev_info = NULL;
    ka_dma_addr_t host_addr_dma = 0;
    void *buffer;
    u64 dump_dma_addr, dump_size;
    u32 dev_id = dma_dump->dev_id;
    ret = dma_bbox_dump_para_check(dev_id, dma_dump->dst_buf);
    if (ret != 0) {
        dms_err("Check black bbox parameter fail. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }
    dump_size = g_host_dma_infos[dev_id][dma_dump->data_type].size;
    dump_dma_addr = g_host_dma_infos[dev_id][dma_dump->data_type].address;
    ret = bbox_check_dma_info(dma_dump, dump_size, dump_dma_addr);
    if (ret != 0) {
        dms_err("Failed to check dma info. (ret=%d)\n", ret);
        return ret;
    }
    dev_info = devdrv_get_devdrv_info_array(dev_id);
    ret = bbox_check_dev_info(dev_info);
    if (ret != 0) {
        dms_err("Failed to check dev_info. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }
    dump_dma_addr += dma_dump->offset;
    ka_base_atomic_inc(&dev_info->occupy_ref);
    buffer = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, DEVDRV_DUMP_SINGLE_LEN, &host_addr_dma,
                                                  KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        dms_err("dma_alloc_coherent fail. (dev_id=%u, size=%u)\n", dev_id, dma_dump->len);
        ka_base_atomic_dec(&dev_info->occupy_ref);
        return -ENOMEM;
    }
    ret = bbox_dma_copy_info(dma_dump, dev_info, dump_dma_addr, host_addr_dma, buffer);
    if (ret != 0) {
        dms_err("Failed to copy info. (ret=%d)\n", ret);
    }
    ka_base_atomic_dec(&dev_info->occupy_ref);
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, DEVDRV_DUMP_SINGLE_LEN, buffer, host_addr_dma);
    buffer = NULL;
    return ret;
}

STATIC int dms_get_bbox_data_pcie(unsigned int dev_id, unsigned int type, unsigned int offset, unsigned int len,
                                  void *buf)
{
    int i;
    struct bbox_dma_dump dma_dump;
    const struct pcie_dump_select select[] = {
        {MEM_TYPE_PCIE_SRAM, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_PCIE_DDR, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_BBOX_HDR, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_BBOX_DDR, LOG_SLOG_BBOX_DDR, offset, devdrv_dma_bbox_dump},
        {MEM_TYPE_REG_DDR, LOG_SLOG_REG_DDR, offset, devdrv_dma_bbox_dump},
        {MEM_TYPE_HBOOT_SRAM, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_DEBUG_OS_LOG, LOG_SLOG_DEBUG_OS, 0, devdrv_dma_bbox_dump},
        {MEM_TYPE_SEC_LOG, LOG_SLOG_SEC_OS, 0, devdrv_dma_bbox_dump},
        {MEM_TYPE_RUN_OS_LOG, LOG_SLOG_RUN_OS, 0, devdrv_dma_bbox_dump},
        {MEM_TYPE_RUN_EVENT_LOG, LOG_SLOG_RUN_EVENT, 0, devdrv_dma_bbox_dump},
        {MEM_TYPE_DEBUG_DEV_LOG, LOG_SLOG_DEBUG_DEV, 0, devdrv_dma_bbox_dump},
        {MEM_TYPE_CHIP_LOG_PCIE_BAR, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_BBOX_PCIE_BAR, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_VMCORE_STAT, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_dump},
        {MEM_TYPE_VMCORE_FILE, LOG_VMCORE_FILE_DDR, offset, devdrv_dma_bbox_dump},
        {MEM_TYPE_TS_LOG, 0, offset, bbox_dma_dump_pcie},
        {MEM_TYPE_IMP_LOG, 0, offset, bbox_dma_dump_pcie},
        {MEM_TYPE_IMU_LOG, 0, offset, bbox_dma_dump_pcie},
        {MEM_TYPE_HSM_LOG, 0, offset, bbox_dma_dump_pcie},
    };

    dma_dump.dev_id = dev_id;
    dma_dump.dst_buf = buf;
    dma_dump.len = len;
    dma_dump.data_type = type;
    for (i = 0; i < KA_BASE_ARRAY_SIZE(select); i++) {
        if (select[i].data_type != type) {
            continue;
        }
        dma_dump.offset = select[i].mem_offset;
        dma_dump.log_type = select[i].log_type;
        return select[i].pcie_dump_func(&dma_dump);
    }

    return -EOPNOTSUPP;
}

int dms_get_bbox_data(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    bbox_data_info_t *bbox_info = NULL;
    int ret;

    ret = dms_get_bbox_data_para_check(in, in_len);
    if (ret != 0) {
        return ret;
    }

    bbox_info = (bbox_data_info_t *)in;
    if (devdrv_get_connect_protocol(bbox_info->dev_id) == CONNECT_PROTOCOL_UB) {
        return dms_get_bbox_data_ub(bbox_info->dev_id, bbox_info->type, bbox_info->offset, bbox_info->len,
                                    bbox_info->bbox_data_buf);
    }

    return dms_get_bbox_data_pcie(bbox_info->dev_id, bbox_info->type, bbox_info->offset, bbox_info->len,
                                  bbox_info->bbox_data_buf);
}

STATIC int dms_set_bbox_data_ub(unsigned int dev_id, unsigned int type, unsigned int offset, unsigned int len,
                                void *buf)
{
    const struct dev_mem_region *region = NULL;
    enum devdrv_rao_client_type client_type;
    int ret = 0;
    struct mem_mng *ctx = bbox_mem_mng_get(dev_id);

    ka_task_down_read(&ctx->sem);
    ret = dms_bbox_region_get(ctx, type, &offset, &region, &client_type);
    if (ret != 0) {
        dms_err("Invalid type. (dev_id=%u; type=%d)\n", dev_id, type);
        goto fail_out;
    }

    if ((region == NULL) || (region->va == NULL)) {
        dms_err("Invalid type or unmapped region. (dev_id=%u; type=%d; region_is_null=%d)\n", dev_id, type,
                (int)(region == NULL));
        ret = -EINVAL;
        goto fail_out;
    }

    if ((offset >= region->size) || (len > region->size - offset)) {
        dms_err("Para offset is invalid. (dev_id=%u; type=%d; offset=0x%x; len=0x%x)\n", dev_id, type, offset, len);
        ret = -EINVAL;
        goto fail_out;
    }

    if (copy_from_user_safe((u8 *)region->va + offset, buf, len) != 0) {
        dms_err("Copy from user failed. (dev_id=%u; type=%d; len=0x%x)\n", dev_id, type, len);
        ret = -EFAULT;
        goto fail_out;
    }

    ret = devdrv_rao_write(dev_id, client_type, offset, len);
    if (ret != 0) {
        dms_err("Write rao data fail. (dev_id=%u; offset=0x%x; len=0x%x; ret=%d)\n", dev_id, offset, len, ret);
        goto fail_out;
    }

fail_out:
    ka_task_up_read(&ctx->sem);
    return ret;
}

int dms_bar_bbox_store(struct bbox_dma_dump *dma_dump)
{
    int ret;
    u64 phy_addr;
    size_t phy_addr_size;
    void __ka_mm_iomem *vir_addr = NULL;
    enum bbox_data_type type = (enum bbox_data_type)dma_dump->data_type;

    ret = dms_get_bbox_addr_info(dma_dump->dev_id, type, &phy_addr, &phy_addr_size);
    if (ret != 0) {
        dms_err("Get bar log addr failed. (dev_id=%u; type=%d; ret=%d)\n", dma_dump->dev_id, type, ret);
        return ret;
    }

    if (((u64)dma_dump->offset >= (u64)phy_addr_size) ||
        ((u64)dma_dump->len > (u64)phy_addr_size - (u64)dma_dump->offset)) {
        dms_err("Para offset len check failed. (dev_id=%u; log_type=%d; offset=0x%x; len=0x%x; max_offset=0x%lx)\n",
                dma_dump->dev_id, type, dma_dump->offset, dma_dump->len, phy_addr_size);
        return -EINVAL;
    }

    vir_addr = ka_mm_ioremap(phy_addr + dma_dump->offset, dma_dump->len);
    if (vir_addr == NULL) {
        dms_err("Failed to invoke the ioremap. (dev_id=%u; len=%u)\n", dma_dump->dev_id, dma_dump->len);
        return -ENOMEM;
    }

    ret = copy_from_user_safe(vir_addr, dma_dump->dst_buf, dma_dump->len);
    if (ret != 0) {
        dms_err("Copy from user failed. (dev_id=%u; len=%u; ret=%d)\n", dma_dump->dev_id, dma_dump->len, ret);
    }

    ka_mm_iounmap(vir_addr);
    vir_addr = NULL;
    return ret;
}

STATIC int dms_set_bbox_data_pcie(unsigned int dev_id, unsigned int type, unsigned int offset, unsigned int len,
                                  void *buf)
{
    int i;
    struct bbox_dma_dump dma_dump;
    const struct pcie_dump_select select[] = {{MEM_TYPE_KDUMP_MAGIC, LOG_SLOG_MEM_MAX, offset, dms_bar_bbox_store}};

    dma_dump.dev_id = dev_id;
    dma_dump.dst_buf = buf;
    dma_dump.len = len;
    dma_dump.data_type = type;
    for (i = 0; i < KA_BASE_ARRAY_SIZE(select); i++) {
        if (select[i].data_type != type) {
            continue;
        }
        dma_dump.offset = select[i].mem_offset;
        dma_dump.log_type = select[i].log_type;
        return select[i].pcie_dump_func(&dma_dump);
    }

    return -EOPNOTSUPP;
}

int dms_set_bbox_data(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    bbox_data_info_t *bbox_info = NULL;
    int ret;

    ret = dms_get_bbox_data_para_check(in, in_len);
    if (ret != 0) {
        return ret;
    }

    bbox_info = (bbox_data_info_t *)in;
    if (devdrv_get_connect_protocol(bbox_info->dev_id) == CONNECT_PROTOCOL_UB) {
        return dms_set_bbox_data_ub(bbox_info->dev_id, bbox_info->type, bbox_info->offset, bbox_info->len,
                                    bbox_info->bbox_data_buf);
    }

    return dms_set_bbox_data_pcie(bbox_info->dev_id, bbox_info->type, bbox_info->offset, bbox_info->len,
                                  bbox_info->bbox_data_buf);
}

void *bbox_alloc_klog_mem(unsigned long addr, unsigned int size)
{
    return ka_mm_vzalloc(size);
}

void bbox_free_klog_mem(struct dev_mem_region *region)
{
    if (region->va != NULL) {
        ka_mm_vfree(region->va);
        region->va = NULL;
    }
}

void *bbox_alloc_region_mem(unsigned long addr, unsigned int size)
{
    return ka_mm_vzalloc(size);
}

void bbox_free_region_mem(struct dev_mem_region *region)
{
    if (region->va != NULL) {
        ka_mm_vfree(region->va);
        region->va = NULL;
    }
}

void bbox_uda_davinci_type_pack(struct uda_dev_type *type, bool is_init)
{
    uda_davinci_near_real_entity_type_pack(type);
}

static int bbox_persistent_export_alloc(struct persistent_export_region *persistent)
{
    unsigned int offset = 0;
    unsigned int rsv_mem_size = MEM_TYPE_DEBUG_OS_LOG_LEN + MEM_TYPE_RUN_OS_LOG_LEN + MEM_TYPE_RUN_EVENT_LOG_LEN;
    unsigned int alloc_size = MEM_TYPE_DEBUG_OS_LOG_LEN + MEM_TYPE_SEC_LOG_LEN + MEM_TYPE_RUN_OS_LOG_LEN +
                              MEM_TYPE_RUN_EVENT_LOG_LEN + MEM_TYPE_DEBUG_DEV_LOG_LEN;

    persistent->base_addr = bbox_alloc_region_mem(0, alloc_size);
    if (persistent->base_addr == NULL) {
        dms_err("Alloc mem failed. (alloc_size=%u)\n", alloc_size);
        return -ENOMEM;
    }

    persistent->booked.va = persistent->base_addr;
    persistent->booked.size = rsv_mem_size;
    offset += rsv_mem_size;

    persistent->sec_log.va = persistent->base_addr + offset;
    persistent->sec_log.size = MEM_TYPE_SEC_LOG_LEN;
    offset += MEM_TYPE_SEC_LOG_LEN;

    persistent->debug_dev_log.va = persistent->base_addr + offset;
    persistent->debug_dev_log.size = MEM_TYPE_DEBUG_DEV_LOG_LEN;

    return 0;
}

static void bbox_persistent_export_free(struct mem_mng *ctx)
{
    struct dev_mem_region region;

    ctx->persistent.debug_dev_log.va = NULL;
    ctx->persistent.sec_log.va = NULL;
    ctx->persistent.booked.va = NULL;

    region.va = ctx->persistent.base_addr;
    bbox_free_region_mem(&region);
}

STATIC int bbox_persistent_register_rao_client(struct persistent_export_region *persistent, u32 udevid)
{
    int ret;

    ret = devdrv_register_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS, (u64)(uintptr_t)persistent->booked.va,
                                     persistent->booked.size, DEVDRV_RAO_PERM_RMT_READ);
    if (ret != 0) {
        return ret;
    }
    persistent->booked.reg_flag = true;

    ret = devdrv_register_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_SEC_OS, (u64)(uintptr_t)persistent->sec_log.va,
                                     persistent->sec_log.size, DEVDRV_RAO_PERM_RMT_READ);
    if (ret != 0) {
        devdrv_unregister_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS);
        persistent->booked.reg_flag = false;
        return ret;
    }
    persistent->sec_log.reg_flag = true;

    ret = devdrv_register_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_DEBUG_DEV,
                                     (u64)(uintptr_t)persistent->debug_dev_log.va, persistent->debug_dev_log.size,
                                     DEVDRV_RAO_PERM_RMT_READ);
    if (ret != 0) {
        devdrv_unregister_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_SEC_OS);
        persistent->sec_log.reg_flag = false;
        devdrv_unregister_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS);
        persistent->booked.reg_flag = false;
        return ret;
    }
    persistent->debug_dev_log.reg_flag = true;

    return 0;
}

STATIC int bbox_host_msg_send(u32 dev_id, BBOX_MSG_TYPE cmd_type, void *msg, u32 msg_len)
{
    u32 out_len;
    int ret;
    struct bbox_msg_head *head = (struct bbox_msg_head *)msg;
    head->cmd_type = cmd_type;
    head->error_code = 0;
    head->valid = BBOX_MSG_MAGIC;
    head->rsv = 0;

    ret = devdrv_common_msg_send(dev_id, (void *)msg, msg_len, msg_len, &out_len, DEVDRV_COMMON_MSG_BBOX);
    if ((ret != 0) || (head->error_code != 0)) {
        return ret != 0 ? ret : head->error_code;
    }
    return 0;
}
#ifdef CFG_FEATURE_UB
STATIC int bbox_host_seg_msg_send(u32 dev_id, u32 type, u32 *token_value, struct ubcore_seg *seg)
{
    int ret;
    struct bbox_msg_data_for_ub msg = {0};
    msg.mem_type = type;
    ret = bbox_host_msg_send(dev_id, BBOX_QUERY_SEG_MSG, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        dms_err("Failed to send message to device. (dev_id=%u; type=%u; error_code=%d; ret=%d)\n", dev_id, type,
                msg.head.error_code, ret);
        return ret;
    }

    *token_value = msg.token_value;
    *seg = msg.seg;
    return 0;
}
#endif

STATIC void bbox_register_work(ka_work_struct_t *work)
{
    struct reg_work_item *work_item = ka_container_of(ka_task_to_delayed_work(work), struct reg_work_item, reg_work);
    struct mem_mng *ctx = bbox_mem_mng_get(work_item->dev_id);
    int ret;
#ifdef CFG_FEATURE_UB
    struct bbox_msg_data_for_ub msg = {0};

    ret = bbox_host_msg_send(work_item->dev_id, BBOX_QUERY_REG_MSG, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        dms_warn("Device not registered, need retry. (dev_id=%u)\n", work_item->dev_id);
        ka_task_schedule_delayed_work(&g_reg_work[work_item->dev_id].reg_work,
                                      ka_system_msecs_to_jiffies(RETRY_INTERVAL_MS));
        return;
    }
#endif

    ka_task_down_write(&ctx->sem);
    ret = bbox_persistent_register_rao_client(&ctx->persistent, work_item->dev_id);
    ka_task_up_write(&ctx->sem);
    if (ret != 0) {
        dms_err("Failed to register persistent rao. (dev_id=%u)\n", work_item->dev_id);
        return;
    }

    dms_info("Register persistent rao success. (dev_id=%u)\n", work_item->dev_id);
}

int bbox_register_persistent_export(u32 udevid, struct mem_mng *ctx)
{
    int ret;

    ka_task_down_write(&ctx->sem);
    ret = bbox_persistent_export_alloc(&ctx->persistent);
    ka_task_up_write(&ctx->sem);
    if (ret != 0) {
        return ret;
    }

    g_reg_work[udevid].dev_id = udevid;
    KA_TASK_INIT_DELAYED_WORK(&g_reg_work[udevid].reg_work, bbox_register_work);
    ka_task_schedule_delayed_work(&g_reg_work[udevid].reg_work, 0);
    return 0;
}

int bbox_unregister_persistent_export(u32 udevid, struct mem_mng *ctx)
{
    struct persistent_export_region *persistent = NULL;
    int ret = 0;

    ka_task_cancel_delayed_work_sync(&g_reg_work[udevid].reg_work);
    ka_task_down_write(&ctx->sem);
    persistent = &ctx->persistent;
    if (persistent->booked.reg_flag == true) {
        ret = devdrv_unregister_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS);
        if (ret != 0) {
            dms_warn("Unregister debug dev memory rao unsuccessfully. (dev_id=%d; ret=%d)\n", udevid, ret);
        } else {
            persistent->booked.reg_flag = false;
        }
    }

    if (persistent->sec_log.reg_flag == true) {
        ret = devdrv_unregister_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_SEC_OS);
        if (ret != 0) {
            dms_warn("Unregister sec memory rao unsuccessfully. (dev_id=%d; ret=%d)\n", udevid, ret);
        } else {
            persistent->sec_log.reg_flag = false;
        }
    }

    if (persistent->debug_dev_log.reg_flag == true) {
        ret = devdrv_unregister_rao_client(udevid, DEVDRV_RAO_CLIENT_BBOX_DEBUG_DEV);
        if (ret != 0) {
            dms_warn("Unregister debug dev memory rao unsuccessfully. (dev_id=%d; ret=%d)\n", udevid, ret);
        } else {
            persistent->debug_dev_log.reg_flag = false;
        }
    }

    bbox_persistent_export_free(ctx);
    ka_task_up_write(&ctx->sem);

    return ret;
}

int bbox_set_mem_and_register(u32 udevid, struct mem_mng *ctx, u32 log_type, void *kva, u32 size)
{
    return 0;
}

void bbox_persistent_unregister_by_type(u32 udevid, struct mem_mng *ctx, u32 log_type)
{}

STATIC int bbox_ctrl_msg_recv(u32 dev_id, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    return 0;
}

struct devdrv_common_msg_client bbox_host_msg_client = {
    .type = DEVDRV_COMMON_MSG_BBOX,
    .common_msg_recv = bbox_ctrl_msg_recv,
};

int bbox_client_init(void)
{
    int ret;

    ret = devdrv_register_common_msg_client(&bbox_host_msg_client);
    if (ret != 0) {
        dms_err("Failed to register client. (type=%d)\n", (int)bbox_host_msg_client.type);
        return ret;
    }

    dms_info("Succeed to register client. (type=%d)\n", (int)bbox_host_msg_client.type);
    return 0;
}

void bbox_client_uninit(void)
{
    (void)devdrv_unregister_common_msg_client(0, &bbox_host_msg_client);
}
#ifdef CFG_FEATURE_UB
int bbox_dump_ctx_va_init_host(struct bbox_ub_dump_ctx *ub_dump_ctx, u32 type)
{
    struct bbox_dump_type_info *dump_info = get_bbox_dump_info();
    u32 mem_size;

    if (type == MEM_TYPE_VMCORE_FILE) {
        mem_size = VMCORE_FILE_MAX_SIZE;
    } else {
        mem_size = dump_info[type].size;
    }
    ub_dump_ctx->size = mem_size;
    ub_dump_ctx->va = ka_mm_vzalloc(mem_size);
    return ub_dump_ctx->va == NULL ? -EFAULT : 0;
}
#endif

int bbox_dump_ctx_init(u32 dev_id, u32 type)
{
#ifdef CFG_FEATURE_UB
    struct bbox_ub_dump_ctx *ub_dump_ctx = bbox_get_ub_dump_ctx(dev_id, type);
    struct ubcore_device *ubc_dev = NULL;
    u32 token_val, rmt_token_value;
    struct ubcore_seg seg = {0};
    int ret;

    ubc_dev = bbox_get_ubcore_dev(dev_id);
    if (ubc_dev == NULL) {
        dms_err("Failed to get dev. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_get_token_val(dev_id, &token_val);
    if (ret != 0) {
        dms_err("Failed to get token value. (dev_id=%u)\n", dev_id);
        return ret;
    }

    ka_task_down_write(&ub_dump_ctx->sem);
    ret = bbox_dump_ctx_va_init_host(ub_dump_ctx, type);
    if (ret != 0) {
        dms_err("Alloc mem failed. (dev_id=%d, type=%u)\n", dev_id, type);
        ret = -ENOMEM;
        goto alloc_fail;
    }

    ret = bbox_init_ub_segment(ubc_dev, dev_id, type, (u64)(uintptr_t)ub_dump_ctx->va, ub_dump_ctx->size, token_val);
    if (ret != 0) {
        goto init_fail;
    }

    ret = bbox_host_seg_msg_send(dev_id, type, &rmt_token_value, &seg);
    if (ret != 0) {
        goto send_fail;
    }

    ret = bbox_import_ub_segment(ubc_dev, ub_dump_ctx, &seg, rmt_token_value);
    if (ret != 0) {
        goto send_fail;
    }
    ka_task_up_write(&ub_dump_ctx->sem);
    dms_info("Bbox init ub dump ctx successfully. (dev_id=%u, type=%u)\n", dev_id, type);
#endif
    return 0;

#ifdef CFG_FEATURE_UB
send_fail:
    bbox_uninit_ub_segment(ub_dump_ctx);
init_fail:
    ka_mm_vfree(ub_dump_ctx->va);
    ub_dump_ctx->va = NULL;
alloc_fail:
    ka_task_up_write(&ub_dump_ctx->sem);
    return ret;
#endif
}

void bbox_dump_ctx_uninit(u32 dev_id, u32 type)
{
#ifdef CFG_FEATURE_UB
    struct bbox_ub_dump_ctx *ub_dump_ctx = bbox_get_ub_dump_ctx(dev_id, type);

    ka_task_down_write(&ub_dump_ctx->sem);
    bbox_unimport_ub_segment(ub_dump_ctx);
    bbox_uninit_ub_segment(ub_dump_ctx);
    if (ub_dump_ctx->va != NULL) {
        ka_mm_vfree(ub_dump_ctx->va);
        ub_dump_ctx->va = NULL;
    }
    ka_task_up_write(&ub_dump_ctx->sem);
#endif
}

int bbox_dma_addrs_init(u32 udevid)
{
    int ret, i, type_size;
    u32 type;
    enum bbox_data_type *data_type;
    struct bbox_msg_data_for_pcie msg = {0};
    ret = bbox_host_msg_send(udevid, BBOX_QUERY_DMA_MSG, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        dms_err("Failed to send message to device. (dev_id=%u, error_code=%d, ret=%d)\n", udevid,
                (int)msg.head.error_code, ret);
        return ret;
    }
    data_type = get_bbox_dump_type(udevid);
    type_size = get_bbox_dump_type_size(udevid);
    for (i = 0; i < type_size; i++) {
        type = data_type[i];
        g_host_dma_infos[udevid][type] = msg.bbox_dma_infos[type];
    }
    return 0;
}

void bbox_dma_addrs_uninit(u32 udevid)
{
    int i, type_size;
    u32 type;
    enum bbox_data_type *data_type;
    data_type = get_bbox_dump_type(udevid);
    type_size = get_bbox_dump_type_size(udevid);
    for (i = 0; i < type_size; i++) {
        type = data_type[i];
        g_host_dma_infos[udevid][type].address = 0;
        g_host_dma_infos[udevid][type].size = 0;
    }
}

int bbox_device_set_mem_platform(u32 udevid, u32 log_type, void *kva, u32 size)
{
    int ret = 0;
    struct mem_mng *ctx = NULL;
    if (bbox_has_feature(udevid, BBOX_FEATURE_NOT_SUPPORT_ALLOC) == 0) {
        if (udevid >= ASCEND_DEV_MAX_NUM || kva == NULL) {
            dms_err("Invalid para. (dev_id=%u)\n", udevid);
            return -EINVAL;
        }
        ctx = bbox_mem_mng_get(udevid);
        ka_task_down_write(&ctx->sem);
        ret = bbox_set_mem_and_register(udevid, ctx, log_type, kva, size);
        ka_task_up_write(&ctx->sem);
    }
    return ret;
}

void bbox_device_persistent_unregister(u32 udevid, u32 log_type)
{
    struct mem_mng *ctx = NULL;

    if (bbox_has_feature(udevid, BBOX_FEATURE_NOT_SUPPORT_ALLOC) == 0) {
        if (udevid >= ASCEND_DEV_MAX_NUM) {
            dms_err("Invalid para. (dev_id=%u, log_type=%u)\n", udevid, log_type);
            return;
        }
        ctx = bbox_mem_mng_get(udevid);
        ka_task_down_write(&ctx->sem);
        bbox_persistent_unregister_by_type(udevid, ctx, log_type);
        ka_task_up_write(&ctx->sem);
    }
}

int bbox_device_register_persistent_export(u32 udevid, struct mem_mng *ctx)
{
    if (bbox_has_feature(udevid, BBOX_FEATURE_NOT_SUPPORT_ALLOC) == 0) {
        return bbox_register_persistent_export(udevid, ctx);
    }
    return 0;
}

int bbox_device_unregister_persistent_export(u32 udevid, struct mem_mng *ctx)
{
    if (bbox_has_feature(udevid, BBOX_FEATURE_NOT_SUPPORT_ALLOC) == 0) {
        return bbox_unregister_persistent_export(udevid, ctx);
    }
    return 0;
}
