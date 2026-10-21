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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif

#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include "ka_errno_pub.h"
#include "ka_fs_pub.h"
#include "ka_barrier_pub.h"
#include "ka_driver_pub.h"
#include "ka_common_pub.h"
#include "devdrv_device_load.h"
#include "devdrv_ctrl.h"
#include "devdrv_pci.h"
#include "devdrv_dma.h"
#include "devdrv_util.h"
#include "devdrv_mem_alloc.h"

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

STATIC int g_file_check_count[HISI_CHIP_NUM] = {0, 0, 0, 0, 0};
STATIC char g_line_buf[DEVDRV_STR_MAX_LEN] = {0};
STATIC char g_devdrv_sdk_path[DEVDRV_STR_MAX_LEN] = {0};
STATIC char *g_davinci_config_file = "/lib/davinci.conf";

struct devdrv_load_file g_load_file[HISI_CHIP_NUM][DEVDRV_BLOCKS_NUM];

void devdrv_set_device_boot_status(struct devdrv_pci_ctrl *pci_ctrl, u32 status);

#ifndef ka_mm_writeq
static inline void ka_mm_writeq(u64 value, volatile void *addr)
{
    *(volatile u64 *)addr = value;
}
#endif

#ifndef ka_mm_readq
static inline u64 ka_mm_readq(void __ka_mm_iomem *addr)
{
    return ka_mm_readl(addr) + ((u64)ka_mm_readl(addr + 4) << 32);
}
#endif

STATIC void *devdrv_load_mem_alloc(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    int connect_protocol = devdrv_get_connect_protocol_by_dev(dev);
    void *addr = NULL;

    if (connect_protocol == CONNECT_PROTOCOL_PCIE) {
        addr = devdrv_ka_dma_alloc_coherent(dev, size, dma_addr, gfp);
    } else {
        addr = devdrv_kzalloc(size, gfp);
        if (addr == NULL) {
            return NULL;
        }
        *dma_addr = ka_mm_dma_map_single(dev, addr, size, KA_DMA_BIDIRECTIONAL);
        if (ka_mm_dma_mapping_error(dev, *dma_addr) != 0) {
            devdrv_kfree(addr);
            return NULL;
        }
    }

    return addr;
}

STATIC void devdrv_load_mem_free(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr)
{
    int connect_protocol = devdrv_get_connect_protocol_by_dev(dev);
    if (connect_protocol == CONNECT_PROTOCOL_PCIE) {
        devdrv_ka_dma_free_coherent(dev, size, addr, dma_addr);
    } else {
        ka_mm_dma_unmap_single(dev, dma_addr, size, KA_DMA_BIDIRECTIONAL);
        devdrv_kfree(addr);
    }
}

STATIC void devdrv_load_free_one(ka_device_t *dev, struct devdrv_load_addr_pair *addr_pair, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        devdrv_load_mem_free(dev, addr_pair[i].size, addr_pair[i].addr, addr_pair[i].dma_addr);
        addr_pair[i].addr = NULL;
    }

    return;
}

STATIC int devdrv_load_dma_alloc(ka_device_t *dev, struct devdrv_load_addr_pair *addr_pair, size_t size, int depth)
{
    struct devdrv_load_addr_pair *p_addr = addr_pair;
    size_t part1_size;
    size_t part2_size;
    int block_num;
    int num;

    depth--;
    p_addr->addr = devdrv_load_mem_alloc(dev, size, &p_addr->dma_addr, KA_GFP_KERNEL);
    if (p_addr->addr == NULL) {
        if (depth <= 0) {
            devdrv_err("Alloc dma coherent failed. (driver_name=\"%s\"; size=%llu)\n", ka_driver_dev_driver_string(dev),
                       (u64)size);
            goto direct_out;
        }
        /* size should be align with cacheline,
         * otherwise bios will be wrong when data copy */
        part1_size = size >> 1;
        part1_size = DEVDRV_ALIGN(part1_size, DEVDRV_ADDR_ALIGN);
        if (part1_size >= size) {
            devdrv_err("Dma memory not enough. (driver_name=\"%s\")\n", ka_driver_dev_driver_string(dev));
            goto direct_out;
        }
        num = devdrv_load_dma_alloc(dev, p_addr, part1_size, depth);
        if (num < 0) {
            goto direct_out;
        }
        block_num = num;
        p_addr += num;

        part2_size = size - part1_size;
        num = devdrv_load_dma_alloc(dev, p_addr, part2_size, depth);
        if (num < 0) {
            goto free_out;
        }
        block_num += num;
    } else {
        p_addr->size = (u64)size;
        block_num = 1;
    }

    return block_num;
free_out:
    devdrv_load_free_one(dev, addr_pair, block_num);
direct_out:
    return -ENOMEM;
}

STATIC int devdrv_load_contiguous_alloc(ka_device_t *dev, struct devdrv_load_addr_pair *load_addr, int len, size_t size)
{
    struct devdrv_load_addr_pair *addr_pair = NULL;
    size_t remain_size;
    size_t alloc_size;
    int pairs_num;
    int num;
    int i;

    addr_pair = devdrv_kzalloc(sizeof(*addr_pair) * DEVDRV_DMA_CACHE_NUM, KA_GFP_KERNEL);
    if (addr_pair == NULL) {
        devdrv_err("addr_pair devdrv_kzalloc failed.\n");
        goto direct_out;
    }

    remain_size = size;
    pairs_num = 0;

    while (remain_size > 0) {
        alloc_size = (remain_size > DEVDRV_BLOCKS_SIZE) ? DEVDRV_BLOCKS_SIZE : remain_size;
        remain_size -= alloc_size;

        num = devdrv_load_dma_alloc(dev, addr_pair, alloc_size, DEVDRV_DMA_ALLOC_DEPTH);
        if (num <= 0) {
            devdrv_err("Call devdrv_load_dma_alloc failed.\n");
            goto dma_alloc_err;
        }

        if (pairs_num + num >= len) {
            devdrv_err("Dma buffer is not enough.\n");
            goto need_more_buf_err;
        }
        for (i = 0; i < num; i++, pairs_num++) {
            load_addr[pairs_num].addr = addr_pair[i].addr;
            load_addr[pairs_num].dma_addr = addr_pair[i].dma_addr;
            load_addr[pairs_num].size = addr_pair[i].size;
        }
    }

    devdrv_kfree(addr_pair);
    addr_pair = NULL;

    return pairs_num;
need_more_buf_err:
    devdrv_load_free_one(dev, addr_pair, num);
dma_alloc_err:
    devdrv_load_free_one(dev, load_addr, pairs_num);
    devdrv_kfree(addr_pair);
    addr_pair = NULL;
direct_out:
    return -ENOMEM;
}

STATIC loff_t devdrv_get_i_size_read(ka_file_t *p_file)
{
    return ka_fs_i_size_read(ka_fs_file_inode(p_file));
}

STATIC int devdrv_load_write_sram(u32 dev_id, u64 write_value, u64 *read_value, void __ka_mm_iomem *sram_reg,
                                  bool retry_flag)
{
    u32 retry_times = 0;
    int ret = -EINVAL;
    bool need_retry = true;

    do {
        ka_mm_writeq(write_value, sram_reg);
        ka_mb();
        *read_value = ka_mm_readq(sram_reg);
        if (*read_value == write_value) {
            ret = 0;
            break;
        }
        if ((retry_flag == true) && (retry_times < DEVDRV_WRITE_BLOCK_INFO_RETRY_TIMES)) {
            retry_times++;
            devdrv_warn("sram_reg read back. (dev_id=%u; write_value=0x%llx; read_value=0x%llx; retry_times=%u)\n",
                        dev_id, write_value, *read_value, retry_times);
            ka_system_msleep(DEVDRV_WRITE_BLOCK_INFO_WAIT_TIME);
            continue;
        }
        need_retry = false;
    } while (need_retry);

    return ret;
}

/*
 * read & write register with little endian.
 */
STATIC void devdrv_load_notice(struct devdrv_agent_load *loader, struct devdrv_load_blocks *blocks, u64 flag)
{
    void __ka_mm_iomem *sram_complet_addr = NULL;
    void __ka_mm_iomem *sram_reg = NULL;
    int ret = -EINVAL;
    u64 read_value = 0;
    u64 j;

    sram_complet_addr = loader->mem_sram_base;

    /* skip the complete flag */
    sram_reg = sram_complet_addr + sizeof(u64);
    if (blocks->blocks_num == 0) {
        devdrv_err("blocks_num is zero. (dev_id=%u)\n", loader->dev_id);
        return;
    }

    ret = devdrv_load_write_sram(loader->dev_id, blocks->blocks_valid_num, &read_value, sram_reg, true);
    if (ret != 0) {
        devdrv_err("block_num read back error. (dev_id=%u; block_num=0x%llx; read_value=0x%llx)\n", loader->dev_id,
                   blocks->blocks_valid_num, read_value);
        return;
    }
    sram_reg = sram_reg + sizeof(u64);

    for (j = 0; j < blocks->blocks_valid_num; j++) {
        ret = devdrv_load_write_sram(loader->dev_id, blocks->blocks_addr[j].dma_addr, &read_value, sram_reg, true);
        if (ret != 0) {
            devdrv_err("dma_addr read back error. (dev_id=%u; dma_addr=0x%llx; read_value=0x%llx)\n", loader->dev_id,
                       blocks->blocks_addr[j].dma_addr, read_value);
            return;
        }
        sram_reg = sram_reg + sizeof(u64);

        ret = devdrv_load_write_sram(loader->dev_id, blocks->blocks_addr[j].data_size, &read_value, sram_reg, true);
        if (ret != 0) {
            devdrv_err("data_size read back error. (dev_id=%u; data_size=0x%llx; read_value=0x%llx)\n", loader->dev_id,
                       blocks->blocks_addr[j].data_size, read_value);
            return;
        }
        sram_reg = sram_reg + sizeof(u64);
    }

    ka_wmb();

    /* notice agent to copy, no retry to avoid duplicate trigger */
    ret = devdrv_load_write_sram(loader->dev_id, flag, &read_value, sram_complet_addr, false);
    if (ret != 0) {
        devdrv_warn("flag read back. (dev_id=%u; flag=0x%llx; read_value=0x%llx)\n", loader->dev_id, flag, read_value);
        return;
    }

    return;
}

STATIC void devdrv_get_data_size(struct devdrv_load_blocks *blocks, u64 file_size, u64 cnt)
{
    if (cnt >= DEVDRV_BLOCKS_ADDR_PAIR_NUM) {
        devdrv_err("Input parameter is invalid. (cnt=%lld)", cnt);
        return;
    }
    if (file_size > blocks->blocks_addr[cnt].size) {
        blocks->blocks_addr[cnt].data_size = blocks->blocks_addr[cnt].size;
    } else {
        blocks->blocks_addr[cnt].data_size = file_size;
    }
}

STATIC u64 devdrv_get_wr_flag(u64 remain_size, u64 trans_size)
{
    u64 flag_w;

    if (remain_size == trans_size) {
        flag_w = DEVDRV_SEND_FINISH;
    } else {
        flag_w = DEVDRV_SEND_PATT_FINISH;
    }

    return flag_w;
}

STATIC void devdrv_check_bar_space_cfg(struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 bar_offset_h = 0;
    u32 bar_offset_l = 0;
    u64 mem_bar_offset;
    u64 io_bar_offset;
    u64 rsv_mem_bar_offset;
    u32 cfg_cmdsts = 0;

    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_CFG_CMDSTS_REG, &cfg_cmdsts);
    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_BAR0_CFG_REG, &bar_offset_l);
    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_BAR1_CFG_REG, &bar_offset_h);
    mem_bar_offset = ((u64)bar_offset_h << DEVDRV_BAR_CFG_OFFSET) | bar_offset_l;

    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_BAR2_CFG_REG, &bar_offset_l);
    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_BAR3_CFG_REG, &bar_offset_h);
    io_bar_offset = ((u64)bar_offset_h << DEVDRV_BAR_CFG_OFFSET) | bar_offset_l;

    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_BAR4_CFG_REG, &bar_offset_l);
    ka_pci_read_config_dword(pci_ctrl->pdev, DEVDRV_PCIE_BAR5_CFG_REG, &bar_offset_h);
    rsv_mem_bar_offset = ((u64)bar_offset_h << DEVDRV_BAR_CFG_OFFSET) | bar_offset_l;

    devdrv_info("Get bar info. (dev_id=%u;bar_cfg=0x%x;offset_bar0=0x%llx;val=0x%x;"
                "bar2=0x%llx;val=0x%x;bar4=0x%llx)\n",
                pci_ctrl->dev_id, cfg_cmdsts, mem_bar_offset, ka_mm_readl((volatile unsigned char *)pci_ctrl->io_base),
                io_bar_offset, ka_mm_readl((volatile unsigned char *)pci_ctrl->msi_base), rsv_mem_bar_offset);
}

STATIC void devdrv_check_bar_space_access(struct devdrv_pci_ctrl *pci_ctrl)
{
    /* 1:read other bar contents; check bar space base addr in cfg space */
    devdrv_check_bar_space_cfg(pci_ctrl);
}

STATIC int devdrv_get_boot_mode_flag(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_agent_load *loader = NULL;
    u64 *sram_complet_addr = NULL;
    u64 flag_r = 0;
    int count = 0;
    enum devdrv_load_wait_mode load_wait_mode;

    loader = pci_ctrl->agent_loader;
    sram_complet_addr = loader->mem_sram_base;

    load_wait_mode = pci_ctrl->ops.get_load_wait_mode(pci_ctrl);
    if (load_wait_mode != DEVDRV_LOAD_WAIT_UNKNOWN) {
        loader->load_wait_mode = (int)load_wait_mode;
        return 0;
    }

    while (1) {
        flag_r = ka_mm_readq(sram_complet_addr);
        if ((flag_r == DEVDRV_ABNORMAL_BOOT_MODE) || (flag_r == DEVDRV_NORMAL_BOOT_MODE) ||
            (flag_r == DEVDRV_SLOW_BOOT_MODE)) {
            break;
        }
        count++;
        if (count % DEVDRV_GET_FLAG_COUNT == 0) {
            devdrv_info("Wait boot mode from bios. (dev_id=%u;flag_r=0x%llx)\n", loader->dev_id, flag_r);
        }
        if (count >= DEVDRV_GET_FLAG_COUNT) {
            break;
        }
        ka_system_msleep(DEVDRV_DELAY_TIME);
    }
    devdrv_info("Get boot mode from bios. (dev_id=%u;flag_r=0x%llx)\n", loader->dev_id, flag_r);

    if (flag_r == DEVDRV_SLOW_BOOT_MODE) {
        loader->load_wait_mode = DEVDRV_LOAD_WAIT_FOREVER;
    } else if (flag_r == DEVDRV_NORMAL_BOOT_MODE) {
        loader->load_wait_mode = DEVDRV_LOAD_WAIT_INTERVAL;
    } else {
        devdrv_err("Get boot mode from bios error. (dev_id=%u; flag_r=0x%llx)\n", loader->dev_id, flag_r);
        if (flag_r == DEVDRV_ABNORMAL_BOOT_MODE) {
            /* add dfx */
            devdrv_check_bar_space_access(pci_ctrl);
        }
        return -EINVAL;
    }

    return 0;
}

STATIC void devdrv_check_load_file(u32 devid, u64 flag)
{
    if (flag == DEVDRV_TEE_CHECK_FAIL) {
        devdrv_err("Load file check tee failed. (dev_id=%u)\n", devid);
    } else if (flag == DEVDRV_IMAGE_CHECK_FAIL) {
        devdrv_err("Load file check image failed. (dev_id=%u)\n", devid);
    } else if (flag == DEVDRV_FILESYSTEM_CHECK_FAIL) {
        devdrv_err("Load file check filesystem failed. (dev_id=%u)\n", devid);
    } else {
        return;
    }
    return;
}

STATIC void devdrv_get_rd_flag(struct devdrv_agent_load *loader, u64 *addr)
{
    u64 flag_r = 0;
    int count = 0;
    int ret;
    int count_f = 0; /* Use for tee.bin get rdy.
When device boot, the rdy would change. If 0xA20000 is change from 66 77 to
others, then consider device has boot, so it's ready. */

    while (1) {
        if (loader->load_abort == DEVDRV_LOAD_ABORT) {
            devdrv_info("Load abort. (dev_id=%u)\n", loader->dev_id);
            return;
        }
        if (ka_base_atomic_read(&loader->load_flag) == DEVDRV_LOAD_SUCCESS) {
            devdrv_info("Device load success. (dev_id=%u)\n", loader->dev_id);
            return;
        }
        flag_r = ka_mm_readq(addr);
        ret = (flag_r == DEVDRV_RECV_FINISH) || (flag_r == DEVDRV_TEE_CHECK_FAIL) ||
              (flag_r == DEVDRV_IMAGE_CHECK_FAIL) || (flag_r == DEVDRV_FILESYSTEM_CHECK_FAIL);
        if (ret != 0) {
            devdrv_info("Receive BIOS flag. (dev_id=%u;flag=0x%llx;count=%d)\n", loader->dev_id, flag_r, count);
            break;
        }

        /* Check flag change to others, after 2 second. */
        count_f++;
        if ((count_f >= DEVDRV_GET_FLAG_COUNT) &&
            ((flag_r != DEVDRV_SEND_FINISH) && (flag_r != DEVDRV_SEND_PATT_FINISH))) {
            devdrv_info("Check flag change, after 2 second. (dev_id=%u;flag=0x%llx)\n", loader->dev_id, flag_r);
            break;
        }

        if (loader->load_wait_mode == DEVDRV_LOAD_WAIT_INTERVAL) {
            count++;
            if (count >= DEVDRV_LOAD_FILE_CHECK_CNT) {
                devdrv_err("Load timeout. (dev_id=%u; flag_r=0x%llx)\n", loader->dev_id, flag_r);
                devdrv_notify_blackbox_err(loader->dev_id, DEVDRV_SYSTEM_START_FAIL);
                break;
            }
        }
        ka_system_msleep(DEVDRV_LOAD_FILE_CHECK_TIME);
    }
    devdrv_check_load_file(loader->dev_id, flag_r);
}

STATIC u64 devdrv_get_load_block_size(u64 remain_size, struct devdrv_load_blocks *blocks)
{
    u64 translated_size;
    u64 file_size;
    u64 i;

    if (remain_size >= DEVDRV_BLOCKS_STATIC_SIZE) {
        blocks->blocks_valid_num = blocks->blocks_num;
        translated_size = DEVDRV_BLOCKS_STATIC_SIZE;
        for (i = 0; i < blocks->blocks_valid_num; i++) {
            blocks->blocks_addr[i].data_size = blocks->blocks_addr[i].size;
        }
    } else {
        translated_size = remain_size;
        file_size = remain_size;
        i = 0;
        while ((file_size > 0) && (i < DEVDRV_BLOCKS_ADDR_PAIR_NUM)) {
            devdrv_get_data_size(blocks, file_size, i);
            file_size = file_size - blocks->blocks_addr[i].data_size;
            i++;
        }
        blocks->blocks_valid_num = i;
    }
    return translated_size;
}

STATIC bool devdrv_chk_load_abort(int status)
{
    return (status == DEVDRV_LOAD_ABORT);
}

STATIC int devdrv_load_file_copy(struct devdrv_agent_load *loader, const char *name, struct devdrv_load_blocks *blocks)
{
    u64 *sram_complet_addr = NULL;
    ka_file_t *p_file = NULL;
    u64 translated_size, remain_size;
    loff_t offset, size;
    ssize_t len;
    u64 flag_w;
    int ret;
    u64 i;
    static int file_exist_flag = 0;
    int count = DEVDRV_WAIT_LOAD_FILE_TIME;
    int wait_file_cnt = 0;

    if (devdrv_chk_load_abort(loader->load_abort)) {
        devdrv_info("Load abort. (dev_id=%u)\n", loader->dev_id);
        return 0;
    }
    /* file operation */
    sram_complet_addr = loader->mem_sram_base;

retry:
    p_file = ka_fs_filp_open(name, KA_O_RDONLY | KA_O_LARGEFILE, 0);
    if (KA_IS_ERR_OR_NULL(p_file)) {
        if ((wait_file_cnt < count) && (file_exist_flag == 0)) {
            wait_file_cnt++;
            ka_system_ssleep(1);
            goto retry;
        }
        ret = KA_PTR_ERR(p_file);
        goto direct_out;
    }
    file_exist_flag = 1;
    size = devdrv_get_i_size_read(p_file);
    devdrv_info("Get file size. (dev_id=%u;file_name=\"%s\";size=%lld)\n", loader->dev_id, name, size);
    if (size <= 0) {
        ret = -EIO;
        devdrv_err("Get file size error. (dev_id=%u; file_name=\"%s\"; size=%lld)\n", loader->dev_id, name, size);
        goto close_out;
    }

    remain_size = (u64)size;

    /* copy file data to dma memory */
    offset = 0;
    while (remain_size > 0) {
        devdrv_get_rd_flag(loader, sram_complet_addr);
        /* kernel 3.10, can not read file when reboot. */
        if (devdrv_chk_load_abort(loader->load_abort)) {
            devdrv_info("Device load abort, not read file. (dev_id=%u)\n", loader->dev_id);
            ret = 0;
            goto free_out;
        }

        translated_size = devdrv_get_load_block_size(remain_size, blocks);
        for (i = 0; i < blocks->blocks_valid_num; i++) {
            len = ka_fs_read_file(p_file, &offset, blocks->blocks_addr[i].addr,
                                  (size_t)blocks->blocks_addr[i].data_size);
            if (len < 0) {
                ret = -EIO;
                devdrv_err("File read error. (dev_id=%u; len=%ld)\n", loader->dev_id, (long)len);
                goto free_out;
            }

            if ((u64)len != blocks->blocks_addr[i].data_size) {
                ret = -EIO;
                devdrv_err("File read error. (dev_id=%u; len=%ld; size=%llu)\n", loader->dev_id, (long)len,
                           blocks->blocks_addr[i].size);
                goto free_out;
            }
        }
        flag_w = devdrv_get_wr_flag(remain_size, translated_size);

        devdrv_load_notice(loader, blocks, flag_w);
        devdrv_info("Notice BIOS to Load file. (dev_id=%u;file_name=\"%s\";size=%llu;flag=0x%llx)\n", loader->dev_id,
                    name, blocks->blocks_valid_num, flag_w);

        remain_size -= translated_size;
    }

    /* check */
    if (offset != size) {
        ret = -EIO;
        devdrv_err("File read error. (dev_id=%u; offset=%lld; size=%lld)\n", loader->dev_id, offset, size);
        goto free_out;
    }

    ka_fs_filp_close(p_file, NULL);
    p_file = NULL;

    return 0;

free_out:
close_out:
    ka_fs_filp_close(p_file, NULL);
    p_file = NULL;
direct_out:
    return ret;
}

STATIC void devdrv_load_blocks_free(struct devdrv_agent_load *loader)
{
    struct devdrv_load_blocks *blocks = loader->blocks;
    ka_device_t *dev = loader->dev;

    if (loader->blocks == NULL) {
        return;
    }

    if (blocks->blocks_num == 0) {
        return;
    }

    devdrv_load_free_one(dev, blocks->blocks_addr, (int)blocks->blocks_num);

    blocks->blocks_num = 0;

    devdrv_kfree(blocks);
    blocks = NULL;
    loader->blocks = NULL;

    return;
}

STATIC int devdrv_load_blocks_alloc(struct devdrv_agent_load *loader)
{
    struct devdrv_load_blocks *blocks = NULL;
    ka_device_t *dev = loader->dev;
    int ret;

    blocks = devdrv_kzalloc(sizeof(*blocks), KA_GFP_KERNEL);
    if (blocks == NULL) {
        devdrv_err("Blocks devdrv_kzalloc failed. (dev_id=%u)\n", loader->dev_id);
        return -ENOMEM;
    }

    ret = devdrv_load_contiguous_alloc(dev, blocks->blocks_addr, DEVDRV_BLOCKS_ADDR_PAIR_NUM,
                                       (size_t)DEVDRV_BLOCKS_STATIC_SIZE);
    devdrv_info("Get block size. (dev_id=%u;block_num=%d;size=0x%x)\n", loader->dev_id, ret,
                (u32)DEVDRV_BLOCKS_STATIC_SIZE);
    if (ret <= 0) {
        devdrv_err("Load file comm dma alloc failed. (dev_id=%u; ret=%d)\n", loader->dev_id, ret);
        devdrv_kfree(blocks);
        blocks = NULL;
        return -ENOMEM;
    }
    loader->blocks = blocks;
    blocks->blocks_num = (u64)ret;

    return 0;
}

void devdrv_set_load_abort(struct devdrv_agent_load *agent_loader)
{
    agent_loader->load_abort = DEVDRV_LOAD_ABORT;
}

STATIC int devdrv_load_file_trans(struct devdrv_agent_load *loader)
{
    struct devdrv_load_blocks *blocks = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = loader->load_work.ctrl;
    u32 chip_type = pci_ctrl->chip_type;
    int i, ret;

    ret = devdrv_load_blocks_alloc(loader);
    if (ret != 0) {
        devdrv_err("Blocks alloc failed. (dev_id=%u)\n", loader->dev_id);
        return ret;
    }
    blocks = loader->blocks;
    devdrv_info("Alloc block memory. (dev_id=%d;blocks_num=%llu;chip_type=%u)\n", loader->dev_id, blocks->blocks_num,
                chip_type);
    ret = devdrv_get_boot_mode_flag(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Get boot mode flag failed, stop probe. (dev_id=%u)\n", loader->dev_id);
        devdrv_load_blocks_free(loader);
        return -ENOMEM;
    }

    /* set device boot status:boot bios */
    devdrv_set_device_boot_status(pci_ctrl, DSMI_BOOT_STATUS_BIOS);
    ka_mm_writeq(DEVDRV_RECV_FINISH, loader->mem_sram_base); // lint !e144
    for (i = 0; i < DEVDRV_BLOCKS_NUM; i++) {
        if (ka_base_strcmp(g_load_file[chip_type][i].file_name, "") == 0) {
            continue;
        }
        ret = devdrv_load_file_copy(loader, g_load_file[chip_type][i].file_name, blocks);
        if (ret < 0) {
            if (g_load_file[chip_type][i].file_type == DEVDRV_CRITICAL_FILE) {
                devdrv_err("File copy error. (dev_id=%u; file=%d; name=\"%s\"; %d)\n", loader->dev_id, i,
                           g_load_file[chip_type][i].file_name, ret);
                devdrv_load_blocks_free(loader);
                return ret;
            }
            /* notice BIOS to skip non-critical files */
            if (g_load_file[chip_type][i].fail_mode == DEVDRV_NOTICE_BIOS) {
                devdrv_get_rd_flag(loader, (u64 *)loader->mem_sram_base);
                blocks->blocks_valid_num = 0;
                devdrv_load_notice(loader, blocks, DEVDRV_SEND_FINISH);
                devdrv_info("Skip file. (dev_id=%u;file=%d;name=\"%s\")\n", loader->dev_id, i,
                            g_load_file[chip_type][i].file_name);
            }
        }
    }

    /* set device boot status:boot bios */
    devdrv_set_device_boot_status(pci_ctrl, DSMI_BOOT_STATUS_OS);

    return 0;
}

STATIC void devdrv_load_finish(ka_work_struct_t *p_work)
{
    struct devdrv_load_work *load_work = ka_container_of(p_work, struct devdrv_load_work, work);
    struct devdrv_pci_ctrl *pci_ctrl = load_work->ctrl;
    struct devdrv_agent_load *loader = pci_ctrl->agent_loader;

    if (loader->load_timer.function != NULL) {
        ka_system_del_timer_sync(&loader->load_timer);
    } else {
        devdrv_warn("Irq func callback before timer build. (dev_id=%u)\n", pci_ctrl->dev_id);
    }

    loader->timer_expires = 0;

    /* free memory used  load files */
    devdrv_load_blocks_free(loader);

    devdrv_load_half_probe(pci_ctrl);
}

void devdrv_notify_blackbox_err(u32 devid, u32 code)
{
    ka_timespec_t stamp;
    u32 udevid;

    stamp = ka_system_current_kernel_time();

    if (g_black_box.callback != NULL) {
        (void)uda_add_id_to_udevid(devid, &udevid);
        devdrv_info("Get blackbox code. (dev_id=%u;blackbox_code=%u)\n", udevid, code);
        g_black_box.callback(udevid, code, stamp);
    }
}

STATIC void devdrv_timer_task(ka_timer_list_t *t)
{
    struct devdrv_agent_load *loader = ka_system_from_timer(loader, t, load_timer);

    u32 device_id = loader->dev_id;

    /* device os have being uped. */
    if (ka_base_atomic_read(&loader->load_flag) == DEVDRV_LOAD_SUCCESS) {
        devdrv_info("Device os load success. (dev_id=%u)\n", loader->dev_id);
        return;
    }

    /* when loader->timer_remain <= 0, means os load failed. */
    if (loader->timer_remain-- > 0) {
        loader->load_timer.expires = ka_jiffies + loader->timer_expires;
        ka_system_add_timer(&loader->load_timer);
    } else {
        devdrv_err("Device os load failed. (loader_devid=%u; dev_id=%u)\n", loader->dev_id, device_id);
        /* reset device */
        devdrv_notify_blackbox_err(device_id, DEVDRV_SYSTEM_START_FAIL);
    }

    return;
}

STATIC void devdrv_load_timer(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_agent_load *loader = pci_ctrl->agent_loader;

    loader->timer_remain = DEVDRV_TIMER_SCHEDULE_TIMES;
    loader->timer_expires = DEVDRV_TIMER_EXPIRES;
    ka_system_timer_setup(&loader->load_timer, devdrv_timer_task, 0);
    loader->load_timer.expires = ka_jiffies + loader->timer_expires;

    ka_system_add_timer(&loader->load_timer);
    devdrv_info("Load timer add. (dev_id=%u)\n", loader->dev_id);
    return;
}

ka_irqreturn_t devdrv_load_irq(int irq, void *data)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)data;
    struct devdrv_agent_load *loader = pci_ctrl->agent_loader;
    u32 status = 0;
    int ret;

    ret = devdrv_get_device_boot_status_inner(pci_ctrl->dev_id, &status);
    if ((ret != 0) || (status != DSMI_BOOT_STATUS_OS)) {
        devdrv_warn("Recv irq before load files finish. (dev_id=%u; status=%u)\n", pci_ctrl->dev_id, status);
        return KA_IRQ_HANDLED;
    }

    pci_ctrl->load_status_flag = DEVDRV_LOAD_SUCCESS_STATUS;
    /* notice device addr info for addr trans */
    devdrv_notify_dev_init_status(pci_ctrl);
    /* set load success flag */
    if (ka_base_atomic_read(&loader->load_flag) == DEVDRV_LOAD_SUCCESS) {
        return KA_IRQ_HANDLED;
    } else {
        ka_base_atomic_set(&loader->load_flag, (int)DEVDRV_LOAD_SUCCESS);
    }

    /* start work queue */
    ka_task_schedule_work(&loader->load_work.work);

    return KA_IRQ_HANDLED;
}

STATIC char *devdrv_get_one_line(char *file_buf, u32 file_buf_len, char *line_buf_tmp, u32 buf_len)
{
    u32 i;
    u32 offset = 0;

    *line_buf_tmp = '\0';

    if (*file_buf == '\0') {
        return NULL;
    }
    while ((*file_buf == ' ') || (*file_buf == '\t')) {
        file_buf++;
        offset++;
    }

    for (i = 0; i < buf_len; i++) {
        if (*file_buf == '\n') {
            line_buf_tmp[i] = '\0';
            file_buf = file_buf + 1;
            offset += 1;
            goto exit;
        }
        if (*file_buf == '\0') {
            line_buf_tmp[i] = '\0';
            goto exit;
        }
        line_buf_tmp[i] = *file_buf;
        file_buf++;
        offset++;
    }
    line_buf_tmp[buf_len - 1] = '\0';
    file_buf = file_buf + 1;
    offset += 1;

exit:
    if (offset >= file_buf_len) {
        devdrv_err("File buf out of bound. (offset=%d; file_buf_len=%d)\n", offset, file_buf_len);
        return NULL;
    }
    return file_buf;
}

STATIC void devdrv_get_val(const char *env_buf, u32 buf_len, char *env_val, u32 val_len)
{
    const char *buf = env_buf;
    u32 i;
    *env_val = '\0';
    for (i = 0; (i < val_len) && (i < buf_len); i++) {
        if ((*buf == ' ') || (*buf == '\t') || (*buf == '\r') || (*buf == '\n') || (*buf == '\0')) {
            env_val[i] = '\0';
            break;
        }
        env_val[i] = *buf;
        buf++;
    }
    env_val[val_len - 1] = '\0';
}

STATIC u32 devdrv_get_env_value(char *env_buf, u32 buf_len, const char *env_name, char *env_val, u32 val_len)
{
    const char *buf = env_buf;
    u32 len;
    u32 offset = 0;

    if (*buf == '#') {
        return DEVDRV_CONFIG_FAIL;
    }

    len = (u32)ka_base_strlen(env_name);
    if (ka_base_strncmp(buf, env_name, len) != 0) {
        return DEVDRV_CONFIG_FAIL;
    }

    buf += len;
    offset += len;

    if ((*buf != ' ') && (*buf != '\t') && (*buf != '=')) {
        return DEVDRV_CONFIG_FAIL;
    }

    while ((*buf == ' ') || (*buf == '\t')) {
        buf++;
        offset++;
    }

    if (*buf != '=') {
        devdrv_err("Get value error.\n");
        return DEVDRV_CONFIG_FAIL;
    }

    buf++;
    offset++;

    while ((*buf == ' ') || (*buf == '\t')) {
        buf++;
        offset++;
    }

    if (offset >= buf_len) {
        devdrv_err("Buffer out of bound. (offset=%d; buf_len=%d)\n", offset, buf_len);
        return DEVDRV_CONFIG_FAIL;
    }
    devdrv_get_val(buf, buf_len - offset, env_val, val_len);
    return DEVDRV_CONFIG_OK;
}

STATIC int devdrv_read_cfg_file(char *file, char *buf, u32 buf_size)
{
    ka_file_t *fp = NULL;
    u32 len;
    ssize_t len_integer;
    int filesize;
    loff_t pos = 0;
    static int wait_cnt = 0;

retry:
    fp = ka_fs_filp_open(file, KA_O_RDONLY, 0);
    if (KA_IS_ERR(fp) || (fp == NULL)) {
        if (wait_cnt < DEVDRV_OPEN_CFG_FILE_COUNT) {
            wait_cnt++;
            ka_system_msleep(DEVDRV_OPEN_CFG_FILE_TIME_MS);
            goto retry;
        }

        devdrv_info("Can not open file. (file=\"%s\")\n", file);
        return -EIO;
    }

    filesize = (int)devdrv_get_i_size_read(fp);
    if (filesize >= (int)buf_size) {
        devdrv_err("File is too large. (file=\"%s\"; size=%lld; max=%lld)\n", file, (long long)filesize,
                   (long long)buf_size);
        ka_fs_filp_close(fp, NULL);
        fp = NULL;
        return -EIO;
    }

    len_integer = ka_fs_read_file(fp, &pos, buf, (size_t)filesize);
    if (len_integer != (ssize_t)(filesize)) {
        devdrv_err("Read file fail. (file_size=%d)\n", filesize);
        ka_fs_filp_close(fp, NULL);
        fp = NULL;
        return -EIO;
    }

    len = (u32)len_integer;
    buf[len] = '\0';

    ka_fs_filp_close(fp, NULL);
    fp = NULL;

    return filesize;
}

STATIC int devdrv_get_env_value_from_buf(char *buf, u32 filesize, const char *env_name, char *env_val, u32 env_val_len)
{
    u32 ret, len;
    char *tmp_buf = NULL;
    char tmp_val[DEVDRV_STR_MAX_LEN];
    int ret_val;
    u32 match_flag = 0;

    tmp_buf = buf;
    while (1) {
        tmp_buf = devdrv_get_one_line(tmp_buf, filesize + 1, g_line_buf, (u32)sizeof(g_line_buf));
        if (tmp_buf == NULL) {
            break;
        }

        ret = devdrv_get_env_value(g_line_buf, DEVDRV_STR_MAX_LEN, env_name, tmp_val, (u32)sizeof(tmp_val));
        if (ret != 0) {
            continue;
        }
        len = (u32)ka_base_strlen(tmp_val);
        if (env_val_len < (len + 1)) {
            devdrv_err("Parameter env_val_len failed.\n");
            return -EINVAL;
        }
        ret_val = strcpy_s(env_val, env_val_len, tmp_val);
        if (ret_val != 0) {
            devdrv_err("strcpy_s failed. (ret=%d)\n", ret_val);
            return -EINVAL;
        }
        env_val[env_val_len - 1] = '\0';
        match_flag = 1;
        break;
    }

    return (match_flag == 1) ? 0 : -EINVAL;
}

u32 devdrv_get_env_value_from_file(char *file, const char *env_name, char *env_val, u32 env_val_len)
{
    u32 ret;
    int filesize, ret_val;
    char *buf = NULL;

    buf = (char *)devdrv_vzalloc(DEVDRV_MAX_FILE_SIZE);
    if (buf == NULL) {
        devdrv_err("Vzalloc failed\n");
        return DEVDRV_CONFIG_FAIL;
    }

    filesize = devdrv_read_cfg_file(file, buf, DEVDRV_MAX_FILE_SIZE);
    if (filesize < 0) {
        ret = DEVDRV_CONFIG_FAIL;
        goto out;
    }

    ret_val = devdrv_get_env_value_from_buf(buf, (u32)filesize, env_name, env_val, env_val_len);
    if (ret_val != 0) {
        ret = DEVDRV_CONFIG_NO_MATCH;
        goto out;
    }

    ret = DEVDRV_CONFIG_OK;
out:
    devdrv_vfree(buf);
    buf = NULL;
    return ret;
}

char *devdrv_get_config_file(void)
{
    return g_davinci_config_file;
}

STATIC u32 devdrv_get_sdk_path(char *sdk_path, u32 path_buf_len)
{
    return devdrv_get_env_value_from_file(g_davinci_config_file, "DAVINCI_HOME_PATH", sdk_path, path_buf_len);
}

STATIC void devdrv_load_file(ka_work_struct_t *p_work)
{
    struct devdrv_load_work *load_work = ka_container_of(p_work, struct devdrv_load_work, work);
    struct devdrv_pci_ctrl *pci_ctrl = load_work->ctrl;
    struct devdrv_agent_load *loader = pci_ctrl->agent_loader;
    int ret;

    devdrv_info("Enter trans file work queue. (dev_id=%u)\n", pci_ctrl->dev_id);
    ret = devdrv_load_file_trans(loader);
    if (ret != 0) {
        devdrv_err("Trans file to agent bios failed. (dev_id=%u)\n", pci_ctrl->dev_id);
        return;
    }

    devdrv_load_timer(pci_ctrl);
    devdrv_info("Leave trans file work queue. (dev_id=%u)\n", pci_ctrl->dev_id);
    devdrv_get_rd_flag(loader, loader->mem_sram_base);
    ka_mm_writeq(0x0, loader->mem_sram_base); // lint !e144
}

STATIC int devdrv_sdk_path_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int block_num = pci_ctrl->res.load_file.load_file_num;
    struct devdrv_load_file_cfg *load_file_cfg = pci_ctrl->res.load_file.load_file_cfg;
    struct devdrv_load_file_cfg *file_cfg = NULL;
    int i, ret;
    u32 chip_type = pci_ctrl->chip_type;

    ret = (int)devdrv_get_sdk_path(g_devdrv_sdk_path, (u32)sizeof(g_devdrv_sdk_path));
    if (ret != 0) {
        ret = strcpy_s(g_devdrv_sdk_path, DEVDRV_STR_MAX_LEN, DEVDRV_HOST_FILE_PATH);
        if (ret != 0) {
            devdrv_err("strcpy base path failed.\n");
            return ret;
        }
    }

    devdrv_info("Device load file path init begin. (chip_type=%d)\n", chip_type);

    /* init */
    for (i = 0; i < DEVDRV_BLOCKS_NUM; i++) {
        g_load_file[chip_type][i].file_name[0] = '\0';
    }

    for (i = 0; i < block_num; i++) {
        ret = strcpy_s(g_load_file[chip_type][i].file_name, DEVDRV_STR_MAX_LEN, g_devdrv_sdk_path);
        if (ret != 0) {
            devdrv_err("strcpy base path to file name failed.\n");
            return ret;
        }

        file_cfg = &load_file_cfg[i];

        if (ka_base_strlen(g_devdrv_sdk_path) + ka_base_strlen(file_cfg->file_name) >= DEVDRV_STR_MAX_LEN) {
            devdrv_err("String len error. (file_len=%ld; base_len=%ld)\n", ka_base_strlen(file_cfg->file_name),
                       ka_base_strlen(g_devdrv_sdk_path));
            return -EFAULT;
        }

        ret = strcat_s(g_load_file[chip_type][i].file_name, DEVDRV_STR_MAX_LEN, file_cfg->file_name);
        if (ret != 0) {
            devdrv_err("strcat failed. (file_len=%ld; base_len=%ld)\n", ka_base_strlen(file_cfg->file_name),
                       ka_base_strlen(g_devdrv_sdk_path));
            return ret;
        }
        g_load_file[chip_type][i].file_type = file_cfg->file_type;
        g_load_file[chip_type][i].fail_mode = file_cfg->fail_mode;
    }

    return 0;
}

int devdrv_load_device(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_agent_load *loader = NULL;
    ka_device_t *dev = ka_pci_get_dev(pci_ctrl->pdev);
    int ret;
    u32 chip_type = pci_ctrl->chip_type;

    devdrv_info("Device os load start. (dev_id=%u)\n", pci_ctrl->dev_id);

    if (g_file_check_count[chip_type] == 0) {
        ret = devdrv_sdk_path_init(pci_ctrl);
        if (ret != 0) {
            devdrv_err("Device sdk_path init failed. (dev_id=%u)\n", pci_ctrl->dev_id);
            return ret;
        }
        g_file_check_count[chip_type]++;
    }

    loader = devdrv_kzalloc(sizeof(*loader), KA_GFP_KERNEL);
    if (loader == NULL) {
        devdrv_err("agent_loader devdrv_kzalloc failed. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -ENOMEM;
    }
    pci_ctrl->agent_loader = loader;

    loader->dev_id = pci_ctrl->dev_id;
    loader->mem_sram_base = pci_ctrl->res.load_sram_base;
    ka_base_atomic_set(&loader->load_flag, 0);
    loader->dev = dev;

    loader->load_work.ctrl = pci_ctrl;
    KA_TASK_INIT_WORK(&loader->load_work.work, devdrv_load_finish);

    /* add file trans work queue */
    pci_ctrl->load_work.ctrl = pci_ctrl;
    KA_TASK_INIT_WORK(&pci_ctrl->load_work.work, devdrv_load_file);

    /* start work queue */
    ka_task_schedule_work(&pci_ctrl->load_work.work);
    devdrv_info("Load write dma addr finish, wait device os start. (dev_id=%u)\n", pci_ctrl->dev_id);

    return 0;
}

void devdrv_load_exit(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_agent_load *loader = NULL;

    if (pci_ctrl->agent_loader == NULL) {
        return;
    }

    loader = pci_ctrl->agent_loader;
    if ((loader != NULL) && (pci_ctrl->load_work.work.func != NULL)) {
        devdrv_set_load_abort(loader);
        (void)ka_task_cancel_work_sync(&pci_ctrl->load_work.work);
    }

    if ((loader != NULL) && (pci_ctrl->agent_loader->load_work.work.func != NULL)) {
        (void)ka_task_cancel_work_sync(&pci_ctrl->agent_loader->load_work.work);
    }

    devdrv_load_blocks_free(loader);

    if (loader->timer_expires != 0) {
        ka_system_del_timer_sync(&loader->load_timer);
        loader->timer_expires = 0;
    }

    devdrv_kfree(loader);
    loader = NULL;
    devdrv_info("devdrv_load_exit finish. (dev_id=%u)\n", pci_ctrl->dev_id);
}
