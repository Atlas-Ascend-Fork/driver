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

#include "ka_fs_pub.h"
#include "ka_errno_pub.h"
#include "ka_barrier_pub.h"
#include "ka_driver_pub.h"
#include "pbl/pbl_soc_res.h"
#include "asdrv_ub_path.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_pair_dev_info.h"
#include "asdrv_ub_load.h"

STATIC char g_devdrv_sdk_path[UBDRV_STR_MAX_LEN] = {0};
STATIC char *g_davinci_config_file = "/lib/davinci.conf";

/*
 * IMAGE               ID      normal name                 name in run package
 * BSBC                0x1     Bsbc.bin                    NA
 * Hboot1-a            0x2     hboot1_a.bin                ascend_xxxx_hboot1_a.bin
 * Hboot1-b            0x3     hboot1_b.bin                ascend_xxxx_hboot1_b.bin
 * Hilink32            0x4     HiLink32*.bin               ascend_xxxx_hilink32.bin
 * Hilink60            0x5     HiLink60*.bin               ascend_xxxx_hilink60.bin
 * Hilink112           0x6     HiLink112*.bin              ascend_xxxx_hilink112.bin
 * Hilink224           0x7     HiLink224*.bin              ascend_xxxx_hilink224.bin
 * Ddr                 0x8     Ddr.bin                     ascend_xxxx_ddr.bin
 * Hbm                 0x9     hbm_img.bin                 ascend_xxxx_hbm.bin
 * Imu                 0xA     IMU_task.bin                ascend_xxxx_imu.bin
 * LPM                 0xB     microwatt.bin               ascend_xxxx_lp.bin
 * Hsm                 0xC     hisserika.bin               ascend_xxxx_hsm.bin
 * Elastic cfg4sys     0xD     sysBaseConfig.bin           ascend_xxxx_sys_config.bin
 * Elastic Cfg4user    0xE     userBaseConfig.bin          ascend_xxxx_user_config.bin
 * Hboot2              0xF     *_HBOOT2_UEFI_*.fd          ascend_xxxx_uefi.fd
 * ATF                 0x10    atf.bin                     ascend_xxxx_atf.bin
 * HRT                 0x11    lpddr_mcu.bin(hiss+lpm)     ascend_xxxx_hrt.bin
 * TEE                 0x12    itrustee.img                ascend_xxxx_tee.bin
 * Kernel dtb          0x13    dt.img                      ascend_xxxx_device_config.bin
 * Kernel              0x14    device_sw.img               ascend_xxxx_device_sw.img
 * Initrd              0x15    device_sw.bin               ascend_xxxx_device_sw.bin
 * Kernel-sec          0x16    CCA Image                   ascend_xxxx_cca_image.image
 * Initrd-sec          0x17    CCA filesystem-le.cpio.gz   ascend_xxxx_cca_initrd_.cpio.gz
 * Lhash               0x18    hash                        ascend_xxxx_lhash.bin
 * crl                 0x19    crl                         ascend_xxxx_.crl
 * ubcfg               0x1A    ub_port_cfg.bin             ascend_xxxx_ubcfg.bin
 * imp                 0x1B    imp_david_asic.bin          ascend_xxxx_imp.bin
 */

struct ubdrv_load_file_cfg *g_load_file = NULL;

// clang-format off
struct ubdrv_load_file_cfg g_cloud_v4_load_file[UBDRV_LOAD_FILE_NUM] = {
    { .id = 0x0, .file_name = "", }, { .id = 0x1, .file_name = "", }, { .id = 0x2, .file_name = "", },
    { .id = 0x3, .file_name = "", }, { .id = 0x4, .file_name = "", }, { .id = 0x5, .file_name = "", },
    { .id = 0x6, .file_name = "", }, { .id = 0x7, .file_name = "", }, { .id = 0x8, .file_name = "", },
    { .id = 0x9, .file_name = "", },
    { .id = 0xA, .file_name = "/driver/device/ascend_950_imu.bin",            .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0xB, .file_name = "/driver/device/ascend_950_lp.bin",             .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0xC, .file_name = "", }, { .id = 0xD, .file_name = "", }, { .id = 0xE, .file_name = "", },
    { .id = 0xF, .file_name = "", }, { .id = 0x10, .file_name = "", }, { .id = 0x11, .file_name = "", },
    { .id = 0x12, .file_name = "/driver/device/ascend_950_tee.bin",           .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x13, .file_name = "/driver/device/ascend_950_device_config.bin", .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x14, .file_name = "/driver/device/ascend_950_device_sw.img",     .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x15, .file_name = "/driver/device/ascend_950_device_sw.bin",     .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x16, .file_name = "", }, { .id = 0x17, .file_name = "", }, { .id = 0x18, .file_name = "", },
    { .id = 0x19, .file_name = "/driver/device/ascend_cloud_v4.crl",          .file_type = UBDRV_NON_CRITICAL_FILE },
    { .id = 0x1A, .file_name = "/driver/device/ascend_950_ubcfg.bin",         .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x1B, .file_name = "/driver/device/ascend_950_imp.bin",           .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x1C, .file_name = "", }, { .id = 0x1D, .file_name = "", }, { .id = 0x1E, .file_name = "", },
    { .id = 0x1F, .file_name = "", },
};

struct ubdrv_load_file_cfg g_cloud_v5_load_file[UBDRV_LOAD_FILE_NUM] = {
    { .id = 0x0, .file_name = "", }, { .id = 0x1, .file_name = "", }, { .id = 0x2, .file_name = "", },
    { .id = 0x3, .file_name = "", }, { .id = 0x4, .file_name = "", }, { .id = 0x5, .file_name = "", },
    { .id = 0x6, .file_name = "", }, { .id = 0x7, .file_name = "", }, { .id = 0x8, .file_name = "", },
    { .id = 0x9, .file_name = "", },
    { .id = 0xA, .file_name = "/driver/device/ascend960_imu.bin",         .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0xB, .file_name = "/driver/device/ascend960_lp.bin",          .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0xC, .file_name = "", }, { .id = 0xD, .file_name = "", }, { .id = 0xE, .file_name = "", },
    { .id = 0xF, .file_name = "", }, { .id = 0x10, .file_name = "", }, { .id = 0x11, .file_name = "", },
    { .id = 0x12, .file_name = "/driver/device/ascend960_tee.bin",        .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x13, .file_name = "/driver/device/ascend960_dt.img",         .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x14, .file_name = "/driver/device/ascend960.image",          .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x15, .file_name = "/driver/device/ascend960.cpio.gz",        .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x16, .file_name = "", }, { .id = 0x17, .file_name = "", }, { .id = 0x18, .file_name = "", },
    { .id = 0x19, .file_name = "", },
    { .id = 0x1A, .file_name = "/driver/device/ascend960_ubcfg.bin",      .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x1B, .file_name = "/driver/device/ascend960_imp.bin",        .file_type = UBDRV_CRITICAL_FILE },
    { .id = 0x1C, .file_name = "", }, { .id = 0x1D, .file_name = "", }, { .id = 0x1E, .file_name = "", },
    { .id = 0x1F, .file_name = "", },
};

struct ubdrv_load_file_cfg g_mini_v4_load_file[UBDRV_LOAD_FILE_NUM] = {
    { .id = 0x0, .file_name = "", }, { .id = 0x1, .file_name = "", }, { .id = 0x2, .file_name = "", },
    { .id = 0x3, .file_name = "", }, { .id = 0x4, .file_name = "", }, { .id = 0x5, .file_name = "", },
    { .id = 0x6, .file_name = "", }, { .id = 0x7, .file_name = "", }, { .id = 0x8, .file_name = "", },
    { .id = 0x9, .file_name = "", }, { .id = 0xA, .file_name = "", }, { .id = 0xB, .file_name = "", },
    { .id = 0xC, .file_name = "", }, { .id = 0xD, .file_name = "", }, { .id = 0xE, .file_name = "", },
    { .id = 0xF, .file_name = "", }, { .id = 0x10, .file_name = "", }, { .id = 0x11, .file_name = "", },
    { .id = 0x12, .file_name = "", }, { .id = 0x13, .file_name = "", }, { .id = 0x14, .file_name = "", },
    { .id = 0x15, .file_name = "", }, { .id = 0x16, .file_name = "", }, { .id = 0x17, .file_name = "", },
    { .id = 0x18, .file_name = "", }, { .id = 0x19, .file_name = "", }, { .id = 0x1A, .file_name = "", },
    { .id = 0x1B, .file_name = "", }, { .id = 0x1C, .file_name = "", }, { .id = 0x1D, .file_name = "", },
    { .id = 0x1E, .file_name = "", }, { .id = 0x1F, .file_name = "", },
};

// clang-format on

struct ubdrv_load_file_cfg *ubdrv_get_load_file_cfg(void)
{
    if (g_load_file != NULL) {
        return g_load_file;
    }
    return NULL;
}

void ubdrv_set_load_abort(struct ubdrv_loader *loader)
{
    loader->load_abort = true;
}

bool ubdrv_get_load_abort(struct ubdrv_loader *loader)
{
    return loader->load_abort;
}

static inline void ubdrv_set_load_flag(struct ubdrv_loader *loader, u32 flag)
{
    struct ubdrv_load_sram_info *sram = (struct ubdrv_load_sram_info *)loader->mem_sram_base;
    sram->flag = flag;
    ka_wmb();
}

// use bar mem with bios exchange flag and addr info
static inline void ubdrv_set_sram_mem_base(struct ubdrv_loader *loader)
{
    loader->mem_sram_base = loader->udev->res.mem_bar.va;
}

// fill segment info registered in host, for bios to load file context
static inline void ubdrv_fill_seg_to_sram(struct ubdrv_loader *loader)
{
    struct ubdrv_load_sram_info *sram = (struct ubdrv_load_sram_info *)loader->mem_sram_base;
    struct ub_entity *ubus_dev;

    ubus_dev = (struct ub_entity *)loader->udev->ubus_dev;
    sram->eid = ubus_dev->bi->info.eid;
    sram->upi = ubus_dev->bi->info.upi;
    sram->cna = ubus_dev->ubc->uent->cna;
    sram->tid = loader->tid;
    ka_wmb();
    ubdrv_info("Set load param to sram. (dev_id=%u;upi=%u;cna=%u;tid=%u)", loader->udev->dev_id, ubus_dev->bi->info.upi,
               ubus_dev->ubc->uent->cna, loader->tid);
}

STATIC void ubdrv_get_data_size(struct ubdrv_load_blocks *blocks, u64 file_size, u64 cnt)
{
    if (cnt >= UBDRV_BLOCKS_ADDR_PAIR_NUM) {
        ubdrv_err("Input parameter is invalid. (cnt=%lld)", cnt);
        return;
    }
    if (file_size > blocks->blocks_addr[cnt].size) {
        blocks->blocks_addr[cnt].data_size = blocks->blocks_addr[cnt].size;
    } else {
        blocks->blocks_addr[cnt].data_size = file_size;
    }
}

STATIC u64 ubdrv_get_load_block_size(u64 remain_size, struct ubdrv_load_blocks *blocks)
{
    u64 translated_size;
    u64 file_size;
    u64 i;

    if (remain_size >= UBDRV_MAX_FILE_SIZE) {
        blocks->blocks_valid_num = blocks->blocks_num;
        translated_size = UBDRV_MAX_FILE_SIZE;
        for (i = 0; i < blocks->blocks_valid_num; i++) {
            blocks->blocks_addr[i].data_size = blocks->blocks_addr[i].size;
        }
    } else {
        translated_size = remain_size;
        file_size = remain_size;
        i = 0;
        while ((file_size > 0) && (i < UBDRV_BLOCKS_ADDR_PAIR_NUM)) {
            ubdrv_get_data_size(blocks, file_size, i);
            file_size = file_size - blocks->blocks_addr[i].data_size;
            i++;
        }
        blocks->blocks_valid_num = i;
    }
    return translated_size;
}

STATIC loff_t ubdrv_get_i_size_read(ka_file_t *p_file)
{
    return ka_fs_i_size_read(ka_fs_file_inode(p_file));
}

int ubdrv_wait_for_flag_change(struct ubdrv_loader *loader, u32 flag_set, u32 flag_expect)
{
    struct ubdrv_load_sram_info *sram = (struct ubdrv_load_sram_info *)loader->mem_sram_base;
    const u32 wait_time = UBDRV_LOAD_FILE_CHECK_TIME; // 20ms
    int cnt = UBDRV_LOAD_WAIT_CNT;

    while ((sram->flag == flag_set) && (cnt > 0)) {
        if (ubdrv_get_load_abort(loader)) {
            ubdrv_info("Device load abort, not wait bios ack. (dev_id=%u)\n", loader->udev->dev_id);
            return -ECANCELED;
        }
        ka_system_msleep(wait_time); // wait 20ms each time to bios change
        cnt--;
        if (sram->flag == UBDRV_BIOS_LOAD_FILE_SUCCESS) {
            break;
        }
    }
    if (sram->flag == UBDRV_BIOS_LOAD_FILE_SUCCESS) {
        ubdrv_info("File load success.(dev_id=%u;flag_set=0x%x;flag_sram=0x%x;flag_expect=0x%x)\n",
                   loader->udev->dev_id, flag_set, sram->flag, flag_expect);
        return 0;
    }
    if ((cnt <= 0) || (sram->flag != flag_expect)) {
        ubdrv_err("Wait for bios signal process timeout. (dev_id=%u;flag_set=0x%x;flag_sram=0x%x;flag_expect=0x%x)\n",
                  loader->udev->dev_id, flag_set, sram->flag, flag_expect);
        return -ETIMEDOUT;
    }

    return 0;
}

int ubdrv_load_file_fill_blocks(struct ubdrv_loader *loader, u32 dev_id, const char *file_name, ka_file_t *p_file,
                                loff_t *offset)
{
    struct ubdrv_load_sram_info *sram = (struct ubdrv_load_sram_info *)loader->mem_sram_base;
    struct ubdrv_load_blocks *blocks = loader->blocks;
    u64 translated_size, remain_size;
    u64 *sram_dma_addr = NULL;
    ssize_t len;
    int ret = 0;
    u64 i;

    remain_size = (u64)ubdrv_get_i_size_read(p_file);
    while (remain_size > 0) {
        translated_size = ubdrv_get_load_block_size(remain_size, blocks);
        sram->block_num = blocks->blocks_valid_num;
        sram_dma_addr = (u64 *)((char *)sram + sizeof(struct ubdrv_load_sram_info));

        if (ubdrv_get_load_abort(loader)) {
            ubdrv_info("Device load abort, not read file. (dev_id=%u)\n", dev_id);
            goto fill_exit;
        }
        for (i = 0; i < blocks->blocks_valid_num; i++) {
            len = ka_fs_kernel_read(p_file, blocks->blocks_addr[i].addr, (size_t)blocks->blocks_addr[i].data_size,
                                    offset);
            if ((len < 0) || (len != (ssize_t)blocks->blocks_addr[i].data_size)) {
                ubdrv_err("Read file failed. (dev_id=%u;file_name=%s;len=%llu;data_size=%llu)\n", dev_id, file_name,
                          (u64)len, blocks->blocks_addr[i].data_size);
                ret = -EIO;
                goto fill_exit;
            }

            ka_mm_writeq(blocks->blocks_addr[i].dma_addr, sram_dma_addr++);
            ka_mm_writeq(blocks->blocks_addr[i].data_size, sram_dma_addr++);
        }
        ka_wmb();
        remain_size -= translated_size;
        if (remain_size > 0) {
            ubdrv_set_load_flag(loader, UBDRV_LOAD_FILE_PART_READY);
            ret = ubdrv_wait_for_flag_change(loader, UBDRV_LOAD_FILE_PART_READY, UBDRV_BIOS_LOAD_FILE_FINISH);
            if (ret != 0) {
                ubdrv_err("Bios recv part file timeout. (dev_id=%u;file_name=%s;sram_flag=%u)\n", dev_id, file_name,
                          sram->flag);
                goto fill_exit;
            }
        } else {
            ubdrv_set_load_flag(loader, UBDRV_LOAD_FILE_READY);
        }
    }
fill_exit:
    return ret;
}

STATIC void ubdrv_file_read_ret_print(u32 dev_id, const char *file_name, u8 file_type)
{
    if (file_type == UBDRV_CRITICAL_FILE) {
        ubdrv_err("Critical file open fail. (dev_id=%u; file_name=%s)\n", dev_id, file_name);
    } else {
        ubdrv_info("Non-critical file open was unsuccessful. (dev_id=%u; file_name=%s)\n", dev_id, file_name);
    }
    return;
}

int ubdrv_load_get_file_size(u32 dev_id, const char *file_name, u32 file_id, ka_file_t **p_file, loff_t *file_size)
{
    int count = UBDRV_WAIT_LOAD_FILE_TIME, wait_file_cnt = 0;
    struct ubdrv_load_file_cfg *load_file = ubdrv_get_load_file_cfg();
    u8 file_type;
    static int file_exist_flag = 0;
    int ret = 0;

    if (load_file == NULL) {
        ubdrv_err("Load file cfg is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    file_type = load_file[file_id].file_type;
retry:
    /* wait file disk to mount for 10min */
    *p_file = ka_fs_filp_open(file_name, KA_O_RDONLY | KA_O_LARGEFILE, 0);
    if (KA_IS_ERR_OR_NULL(*p_file)) {
        if ((wait_file_cnt < count) && (file_exist_flag == 0)) {
            wait_file_cnt++;
            ka_system_ssleep(1);
            goto retry;
        }
        ubdrv_file_read_ret_print(dev_id, file_name, file_type);
        ret = -EIO;
        goto direct_out;
    }
    file_exist_flag = 1;

    *file_size = ubdrv_get_i_size_read(*p_file);
    ubdrv_info("Get file size. (dev_id=%u; file_name=\"%s\"; size=%lld)\n", dev_id, file_name, *file_size);
    if (*file_size < 0) {
        ubdrv_err("Read file too large. (dev_id=%u;file_name=%s;file_size=%lld;max_size=%lld)\n", dev_id, file_name,
                  *file_size, (long long)UBDRV_MAX_FILE_SIZE);
        ret = -EIO;
        goto close_file;
    }
    return 0;

close_file:
    ka_fs_filp_close(*p_file, NULL);
    *p_file = NULL;
direct_out:
    return ret;
}

STATIC int ubdrv_load_file_copy(struct ubdrv_loader *loader, u32 dev_id, const char *file_name, u32 file_id,
                                struct ubdrv_load_blocks *blocks)
{
    loff_t offset = 0, file_size;
    ka_file_t *p_file = NULL;
    int ret = 0;

    if (ubdrv_get_load_abort(loader)) {
        ubdrv_info("Load abort. (dev_id=%u)\n", dev_id);
        return 0;
    }

    ret = ubdrv_load_get_file_size(dev_id, file_name, file_id, &p_file, &file_size);
    if (ret != 0) {
        goto direct_out;
    }

    ret = ubdrv_load_file_fill_blocks(loader, dev_id, file_name, p_file, &offset);
    if (ret != 0) {
        ubdrv_err("Fill load blocks err. (dev_id=%u;file_name=%s;file_size=%lld; ret=%d)\n", dev_id, file_name,
                  file_size, ret);
        goto close_file;
    }

    if (offset != file_size) {
        ubdrv_err("Read file error. (dev_id=%u;file_name=%s;offset=%llu;file_size=%llu)\n", dev_id, file_name,
                  (u64)offset, (u64)file_size);
        ret = -EIO;
    }
close_file:
    ka_fs_filp_close(p_file, NULL);
    p_file = NULL;
direct_out:
    return ret;
}

/* alloc a kernel sva addr for BIOS CDMA */
STATIC int ubdrv_load_sva_alloc(u32 dev_id, struct ubdrv_loader *loader, u32 seg_size, u64 **buffer)
{
    struct ummu_seg_attr seg_attr = {0};
    struct ummu_param param = {0};
    int ret;

    // enable KSVA
    ret = iommu_dev_enable_feature(loader->ummu_tdev, IOMMU_DEV_FEAT_KSVA);
    if (ret != 0) {
        ubdrv_err("KSVA enable failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    // alloc iommu_sva
    param.mode = MAPT_MODE_TABLE; // table mode
    loader->sva = iommu_ksva_bind_device(loader->ummu_tdev, &param);
    if (IS_ERR_OR_NULL(loader->sva)) {
        ubdrv_err("KSVA bind device failed. (dev_id=%u)\n", dev_id);
        ret = -EINVAL;
        goto disable_ksva;
    }

    // get sva's tid
    ret = ummu_get_tid(loader->ummu_tdev, loader->sva, &loader->tid);
    if (ret != 0) {
        ubdrv_err("UMMU get tid failed. (dev_id=%u; tid=%u; ret=%d)\n", dev_id, loader->tid, ret);
        goto unbind_device;
    }

    *buffer = (u64 *)ubdrv_vmalloc(seg_size);
    if (*buffer == NULL) {
        ubdrv_err("Malloc block failed. (dev_id=%u)\n", dev_id);
        ret = -EINVAL;
        goto unbind_device;
    }
    (void)memset_s(*buffer, seg_size, 0, seg_size);

    seg_attr.token = NULL;
    seg_attr.e_bit = UMMU_EBIT_OFF;
    ret = iommu_sva_grant(loader->sva, *buffer, seg_size, UMMU_DEV_READ, &seg_attr);
    if (ret != 0) {
        ubdrv_err("UMMU sva_grant_range failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto free_buffer;
    }

    return 0;

free_buffer:
    ubdrv_vfree(*buffer);
    *buffer = 0;
unbind_device:
    iommu_ksva_unbind_device(loader->sva);
disable_ksva:
    (void)iommu_dev_disable_feature(loader->ummu_tdev, IOMMU_DEV_FEAT_KSVA);
    return ret;
}

int ubdrv_alloc_load_segment(u32 dev_id, struct ubdrv_loader *loader, u32 seg_size)
{
    struct ubdrv_load_blocks *blocks = NULL;
    struct tdev_attr attr = {0};
    u64 *buffer = NULL;
    int ret;

    if (loader->blocks != NULL) {
        return 0;
    }

    tdev_attr_init(&attr);
    /* UMMU_INVALID_TID means that the tid is not specified and will be assigned by the framework. */
    loader->tid = UMMU_INVALID_TID;
    loader->ummu_tdev = ummu_core_alloc_tdev(&attr, &loader->tid);
    if (KA_IS_ERR_OR_NULL(loader->ummu_tdev)) {
        ubdrv_err("Alloc ummu tdev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    blocks = ubdrv_kzalloc(sizeof(*blocks), KA_GFP_KERNEL);
    if (blocks == NULL) {
        ubdrv_err("Blocks ubdrv_kzalloc failed. (dev_id=%u)\n", dev_id);
        ret = -ENOMEM;
        goto free_tdev;
    }
    ret = ubdrv_load_sva_alloc(dev_id, loader, seg_size, &buffer);
    if (ret != 0) {
        ubdrv_err("SVA alloc failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto free_blocks;
    }

    blocks->blocks_addr[0].addr = (void *)buffer;
    blocks->blocks_addr[0].dma_addr = (ka_dma_addr_t)(uintptr_t)buffer;
    blocks->blocks_addr[0].size = seg_size;
    blocks->blocks_num = 1; // only one 48MB sva
    loader->blocks = blocks;
    ubdrv_info("Get block size. (dev_id=%u; tid=%u; size=0x%x)\n", dev_id, loader->tid, seg_size);

    return 0;

free_blocks:
    ubdrv_kfree(blocks);
    blocks = NULL;
free_tdev:
    (void)ummu_core_free_tdev(loader->ummu_tdev);
    loader->ummu_tdev = NULL;
    return ret;
}

STATIC void ubdrv_ksva_iotlb_flush(struct iommu_domain *domain, unsigned long addr, size_t size)
{
    ka_iommu_iotlb_gather_t gather;

    ka_iommu_iotlb_gather_init(&gather);
    ka_iommu_iotlb_gather_add_range(&gather, addr, PAGE_ALIGN(size));
    ka_iommu_iotlb_sync(domain, &gather);
}

void ubdrv_free_load_segment(u32 dev_id, struct ubdrv_loader *loader)
{
    struct ubdrv_load_blocks *blocks = loader->blocks;
    int ret;

    if (blocks == NULL) {
        return;
    }
    if (blocks->blocks_num == 0) {
        return;
    }
    ubdrv_info("Start free mem. (dev_id=%u; tid=%u)\n", dev_id, loader->tid);
    ret = iommu_sva_ungrant(loader->sva, blocks->blocks_addr[0].addr, blocks->blocks_addr[0].size, NULL);
    if (ret != 0) {
        ubdrv_err("UMMU sva ungrant failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }

    ubdrv_ksva_iotlb_flush(loader->sva->handle.domain, (unsigned long)blocks->blocks_addr[0].addr,
                           blocks->blocks_addr[0].size);

    ubdrv_vfree(blocks->blocks_addr[0].addr);
    blocks->blocks_addr[0].addr = NULL;
    iommu_ksva_unbind_device(loader->sva);
    ret = iommu_dev_disable_feature(loader->ummu_tdev, IOMMU_DEV_FEAT_KSVA);
    if (ret != 0) {
        ubdrv_err("UMMU disable ksva feature failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }

    blocks->blocks_num = 0;
    ubdrv_kfree(blocks);
    loader->blocks = NULL;
    ret = ummu_core_free_tdev(loader->ummu_tdev);
    if (ret != 0) {
        ubdrv_err("Free ummu tdev failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
    loader->ummu_tdev = NULL;
    ubdrv_info("Finish free mem. (dev_id=%u)\n", dev_id);
    return;
}

int ubdrv_single_file_path_init(char *file_path, u32 len, const u32 file_id)
{
    int ret;
    struct ubdrv_load_file_cfg *load_file = ubdrv_get_load_file_cfg();

    if (load_file == NULL) {
        ubdrv_err("Load file cfg is NULL.\n");
        return -EINVAL;
    }

    if (len != UBDRV_STR_MAX_LEN) {
        ubdrv_err("Check file len failed. (file_id=%u; len=%u)\n", file_id, len);
        return -EINVAL;
    }

    ret = strcpy_s(file_path, UBDRV_STR_MAX_LEN, g_devdrv_sdk_path);
    if (ret != 0) {
        ubdrv_err("Strcpy base path to file name failed. (ret=%d)\n", ret);
        return ret;
    }

    if (file_id >= UBDRV_LOAD_FILE_NUM) {
        ubdrv_err("Check file id failed. (file_id=%u)\n", file_id);
        return -EINVAL;
    }

    if (load_file[file_id].file_name == NULL) {
        ubdrv_err("Check file name is null. (file_id=%u)\n", file_id);
        return -EINVAL;
    }

    ret = strcat_s(file_path, UBDRV_STR_MAX_LEN, load_file[file_id].file_name);
    if (ret != 0) {
        ubdrv_err("Strcat failed. (file_len=%ld;base_len=%ld;ret=%d)\n", ka_base_strlen(load_file[file_id].file_name),
                  ka_base_strlen(file_path), ret);
        return ret;
    }
    return 0;
}

int ubdrv_load_single_file(struct ubdrv_loader *loader)
{
    struct ubdrv_load_sram_info *sram = (struct ubdrv_load_sram_info *)loader->mem_sram_base;
    char file_path[UBDRV_STR_MAX_LEN] = {0};
    u32 dev_id = loader->udev->dev_id;
    u32 file_id = sram->file_id;
    u32 flag;
    int ret;

    ret = ubdrv_single_file_path_init(file_path, UBDRV_STR_MAX_LEN, file_id);
    if (ret != 0) {
        ubdrv_err("ubdrv_single_file_path_init failed. (dev_id=%u;ret=%d)\n", loader->udev->dev_id, ret);
        return ret;
    }

    ret = ubdrv_load_file_copy(loader, dev_id, file_path, file_id, loader->blocks);
    if (ret != 0) {
        ubdrv_err("Load file to segment failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = ubdrv_wait_for_flag_change(loader, UBDRV_LOAD_FILE_READY, UBDRV_BIOS_REQUIRE_FILE);
    if (ret != 0) {
        ubdrv_err("Wait BIOS load file timeout.(dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    flag = sram->flag;
    if (flag == UBDRV_BIOS_REQUIRE_FILE) {
        return ubdrv_load_single_file(loader);
    } else if (flag == UBDRV_BIOS_LOAD_FILE_SUCCESS) {
        ubdrv_info("BIOS load file finish. (dev_id=%u)\n", dev_id);
        return 0;
    } else {
        ubdrv_err("BIOS load file failed. (dev_id=%u;final_flag=%#x)\n", dev_id, flag);
        return -EIO;
    }
}

int ubdrv_sdk_path_init(void)
{
    int ret;

    ret = ubdrv_get_env_value_from_file(g_davinci_config_file, "DAVINCI_HOME_PATH", g_devdrv_sdk_path,
                                        (u32)sizeof(g_devdrv_sdk_path));
    if (ret != 0) {
        ret = strcpy_s(g_devdrv_sdk_path, UBDRV_STR_MAX_LEN, DEVDRV_HOST_FILE_PATH);
        if (ret != 0) {
            ubdrv_err("Strcpy base path failed. (ret=%d)\n", ret);
            return ret;
        }
    }
    return 0;
}

int ubdrv_start_load_file(struct ubdrv_loader *loader)
{
    struct ubdrv_load_sram_info *sram = (struct ubdrv_load_sram_info *)loader->mem_sram_base;
    int ret;

    ubdrv_set_load_flag(loader, UBDRV_LOAD_HOST_READY);
    ret = ubdrv_wait_for_flag_change(loader, UBDRV_LOAD_HOST_READY, UBDRV_BIOS_REQUIRE_FILE);
    if (ret != 0) {
        ubdrv_err("Wait for BIOS request file timeout. (dev_id=%u;ret=%d;flag=%#x)\n", loader->udev->dev_id, ret,
                  sram->flag);
        return ret;
    }
    devdrv_ub_set_device_boot_status(loader->udev->dev_id, DSMI_BOOT_STATUS_BIOS);

    ret = ubdrv_load_single_file(loader);
    if (ret != 0) {
        ubdrv_err("Load file failed. (dev_id=%u;ret=%d)\n", loader->udev->dev_id, ret);
        return ret;
    }

    ubdrv_set_load_flag(loader, 0);
    devdrv_ub_set_device_boot_status(loader->udev->dev_id, DSMI_BOOT_STATUS_OS);
    return 0;
}

STATIC void ubdrv_timer_task(ka_timer_list_t *t)
{
    struct ubdrv_timer_work *timer_work = ka_container_of(t, struct ubdrv_timer_work, load_timer);
    enum ubdrv_dev_startup_flag_type startup_flag = ubdrv_get_startup_flag(timer_work->udev->dev_id);
    if ((startup_flag == UBDRV_DEV_STARTUP_BOTTOM_HALF_START) || (startup_flag == UBDRV_DEV_STARTUP_BOTTOM_HALF_OK)) {
        ubdrv_info("Device os load success. (dev_id=%u)\n", timer_work->udev->dev_id);
        return;
    }

    if (timer_work->timer_remain-- > 0) {
        timer_work->load_timer.expires = ka_jiffies + timer_work->timer_expires;
        ka_system_add_timer(&timer_work->load_timer);
    } else {
        ubdrv_err("Device os load failed. (dev_id=%u;startup_flag=%u)\n", timer_work->udev->dev_id, startup_flag);
    }

    return;
}

STATIC void ubdrv_prepare_load_timer(struct ascend_ub_dev *udev)
{
    struct ubdrv_timer_work *timer_work = &(udev->timer_work);

    timer_work->udev = udev;
    timer_work->timer_remain = UBDRV_TIMER_SCHEDULE_TIMES;
    timer_work->timer_expires = UBDRV_TIMER_EXPIRES;
    timer_setup(&timer_work->load_timer, ubdrv_timer_task, 0);
    timer_work->load_timer.expires = ka_jiffies + timer_work->timer_expires;
    ka_system_add_timer(&timer_work->load_timer);
    ubdrv_info("Load timer add. (dev_id=%u)\n", udev->dev_id);
}

STATIC void ubdrv_del_load_timer(struct ascend_ub_dev *udev)
{
    struct ubdrv_timer_work *timer_work = &(udev->timer_work);

    if (timer_work->timer_expires != 0) {
        ka_system_del_timer_sync(&timer_work->load_timer);
        timer_work->timer_expires = 0;
        ubdrv_info("Load timer exit. (dev_id=%u)\n", udev->dev_id);
    }
}

int ubdrv_load_file_process(ka_work_struct_t *p_work)
{
    struct ubdrv_load_work *load_work = ka_container_of(p_work, struct ubdrv_load_work, work);
    struct ascend_ub_dev *udev = load_work->udev;
    struct ubdrv_loader *loader = &udev->loader;
    int ret;

    ubdrv_set_sram_mem_base(loader);
    ret = ubdrv_alloc_load_segment(udev->dev_id, loader, UBDRV_MAX_FILE_SIZE);
    if (ret != 0) {
        ubdrv_err("Blocks alloc failed. (dev_id=%u;ret=%d)\n", udev->dev_id, ret);
        return ret;
    }
    ubdrv_fill_seg_to_sram(loader);
    ret = ubdrv_start_load_file(loader);
    if (ret != 0) {
        ubdrv_err("Load file failed. (dev_id=%u;ret=%d)\n", udev->dev_id, ret);
    } else {
        ubdrv_prepare_load_timer(udev);
    }

    ubdrv_free_load_segment(udev->dev_id, loader);
    return ret;
}

void ubdrv_load_file(ka_work_struct_t *p_work)
{
    int ret;
    struct ubdrv_load_work *load_work = ka_container_of(p_work, struct ubdrv_load_work, work);
    struct ascend_ub_dev *udev = load_work->udev;

    ubdrv_info("Enter trans file work queue. (dev_id=%u)\n", udev->dev_id);

    ret = ubdrv_sdk_path_init();
    if (ret != 0) {
        ubdrv_err("Device sdk_path init failed. (dev_id=%u; ret=%d)\n", udev->dev_id, ret);
        return;
    }

    ret = ubdrv_load_file_process(p_work);
    if (ret != 0) {
        ubdrv_err("Trans file to agent bios failed. (dev_id=%u;ret=%d)\n", udev->dev_id, ret);
        return;
    }
    ubdrv_info("Leave trans file work queue. (dev_id=%u)\n", udev->dev_id);

    return;
}

void ubdrv_load_exit(struct ascend_ub_dev *udev)
{
    ubdrv_set_load_abort(&udev->loader);
    ubdrv_del_load_timer(udev);
}

#ifdef CFG_FEATURE_CDMA_LOAD_IMAGE
STATIC int ubdrv_uvb_msg_check_para(const struct cis_message *msg, unsigned long expect_input_len,
                                    unsigned long expect_output_len)
{
    if (msg == NULL) {
        ubdrv_err("UVB msg is null.\n");
        return -EINVAL;
    }
    if (msg->input == NULL) {
        ubdrv_err("UVB msg input is null.\n");
        return -EINVAL;
    }
    if (msg->output == NULL) {
        ubdrv_err("UVB msg input is null.\n");
        return -EINVAL;
    }

    if (msg->p_output_size == NULL) {
        ubdrv_err("UVB msg p_output_size is null.\n");
        return -EINVAL;
    }

    if (msg->input_size < expect_input_len) {
        ubdrv_err("UVB msg input_size is invalid.(input_size=%u; expect_len=%lu)\n", msg->input_size, expect_input_len);
        return -EINVAL;
    }

    if (*msg->p_output_size < expect_output_len) {
        ubdrv_err("UVB msg output_size is invalid.(input_size=%u; expect_len=%lu)\n", *msg->p_output_size,
                  expect_output_len);
        return -EINVAL;
    }

    return 0;
}

STATIC size_t ubdrv_uvb_get_file_trans_size(size_t file_len, u32 offset)
{
    if ((file_len - offset) > UBDRV_MAX_FILE_SIZE) {
        return UBDRV_MAX_FILE_SIZE;
    }

    return (size_t)(file_len - offset);
}

STATIC int ubdrv_uvb_msg_process_part_load(struct ascend_dev *asd_dev, u32 offset, u32 file_id)
{
    struct ubdrv_load_blocks *blocks;
    loff_t pos = offset;
    ka_file_t *p_file;
    size_t file_len;
    ssize_t len;
    int ret;

    p_file = ka_fs_filp_open(asd_dev->loader.file_path, KA_O_RDONLY | KA_O_LARGEFILE, 0);
    if (KA_IS_ERR_OR_NULL(p_file)) {
        ubdrv_err("Open file failed. (dev_id=%u;file_name=%s)\n", asd_dev->dev_id, asd_dev->loader.file_path);
        return -EIO;
    }

    blocks = asd_dev->loader.blocks;
    file_len = ubdrv_uvb_get_file_trans_size(asd_dev->loader.remain_size, offset);
    len = ka_fs_kernel_read(p_file, blocks->blocks_addr[0].addr, file_len, &pos);
    if ((len < 0) || (len != (ssize_t)file_len)) {
        ubdrv_err("Read file failed. (dev_id=%u; file_id=%u; data_size=%llu; read len=%llu)\n", asd_dev->dev_id,
                  file_id, (u64)file_len, (u64)len);
        ret = -EIO;
    } else {
        ubdrv_info("Processed load file. (dev_id=%u; file_id=%u; data_size=%llu; total_file_len=%llu)\n",
                   asd_dev->dev_id, file_id, (u64)(offset + file_len), asd_dev->loader.remain_size);
        ret = 0;
    }

    ka_fs_filp_close(p_file, NULL);

    return ret;
}

STATIC int ubdrv_uvb_msg_get_final_state(struct cis_message *msg)
{
    struct ubdrv_uvb_get_final_state_input *load_input = NULL;
    struct ascend_dev *asd_dev = NULL;
    int ret = -EINVAL;
    u32 dev_id;

    if ((msg == NULL) || (msg->input == NULL)) {
        ubdrv_err("UVB msg or msg input is null.\n");
        return -EINVAL;
    }

    if (msg->input_size < sizeof(struct ubdrv_uvb_get_final_state_input)) {
        ubdrv_err("UVB msg input_size is invalid.(input_size=%u; expect_len=%lu)\n", msg->input_size,
                  sizeof(struct ubdrv_uvb_get_final_state_input));
        return -EINVAL;
    }

    load_input = (struct ubdrv_uvb_get_final_state_input *)msg->input;
    dev_id = load_input->module_id;
    if (dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) {
        ubdrv_err("Get devid by slotid failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }

    asd_dev = ubdrv_get_asd_dev_by_devid(dev_id);
    if (asd_dev == NULL) {
        ubdrv_err("Get asd_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (load_input->state != 0) {
        ubdrv_err("state has problem(status=%u; dev_id=%u)\n", load_input->state, dev_id);
        return -EINVAL;
    }

    ubdrv_free_load_segment(dev_id, &asd_dev->loader);
    devdrv_ub_set_device_boot_status(dev_id, DSMI_BOOT_STATUS_OS);
    return 0;
}

STATIC int ubdrv_uvb_msg_get_loading_state(struct cis_message *msg)
{
    struct ubdrv_uvb_get_loading_state_input *load_input = NULL;
    struct ascend_dev *asd_dev = NULL;
    int ret = -EINVAL;
    u32 dev_id;

    if ((msg == NULL) || (msg->input == NULL)) {
        ubdrv_err("UVB msg or msg input is null.\n");
        return -EINVAL;
    }

    if (msg->input_size < sizeof(struct ubdrv_uvb_get_loading_state_input)) {
        ubdrv_err("UVB msg input_size is invalid.(input_size=%u; expect_len=%lu)\n", msg->input_size,
                  sizeof(struct ubdrv_uvb_get_loading_state_input));
        return -EINVAL;
    }

    load_input = (struct ubdrv_uvb_get_loading_state_input *)msg->input;
    dev_id = load_input->module_id;
    if (dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) {
        ubdrv_err("Get devid by slotid failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }

    asd_dev = ubdrv_get_asd_dev_by_devid(dev_id);
    if (asd_dev == NULL) {
        ubdrv_err("Get asd_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (load_input->state == 1) {
        // continue trans file
        ret = ubdrv_uvb_msg_process_part_load(asd_dev, load_input->offset, load_input->file_id);
        if (ret != 0) {
            ubdrv_err("Load file failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        }
    } else if (load_input->state == 0) {
        ubdrv_info("File transform succ. (dev_id=%u; state=%u; file_id=%d)\n", dev_id, load_input->state,
                   load_input->file_id);
        ret = 0;
    } else {
        ubdrv_err("State error. (dev_id=%u; state=%u)\n", dev_id, load_input->state);
    }

    return ret;
}

STATIC u32 ubdrv_uvb_get_file_id_by_name(char *file_name)
{
    int ret;
    u32 val;

    ret = ka_base_kstrtou32(file_name, 10, &val); // 10 is Decimal
    if (ret < 0) {
        ubdrv_err("File name trans id failed.(name_id=\"%s\"; ret=%d)\n", file_name, ret);
        return UBDRV_UVB_INVALID_FILE_ID;
    }

    return val;
}

STATIC int ubdrv_uvb_msg_get_file_info(struct cis_message *msg)
{
    struct ubdrv_uvb_get_file_info_output *load_output = NULL;
    struct ubdrv_uvb_get_file_info_input *load_input = NULL;
    struct ascend_dev *asd_dev = NULL;
    u32 chip_type = HISI_CHIP_UNKNOWN;
    ka_file_t *p_file = NULL;
    char *file_path = NULL;
    u32 dev_id, file_id;
    int ret = -EINVAL;

    ret = ubdrv_uvb_msg_check_para(msg, sizeof(struct ubdrv_uvb_get_file_info_input),
                                   sizeof(struct ubdrv_uvb_get_file_info_output));
    if (ret != 0) {
        ubdrv_err("UVB msg callback para check failed.\n");
        return ret;
    }

    load_output = (struct ubdrv_uvb_get_file_info_output *)msg->output;
    load_input = (struct ubdrv_uvb_get_file_info_input *)msg->input;
    dev_id = load_input->module_id;
    ubdrv_info("UVB recv msg info. (dev_id=%u;type=%u;slot_id=%u;module_id=%u;server_eid=%u)\n", dev_id,
               load_input->type, load_input->slot_id, load_input->module_id, load_input->server_eid);

    chip_type = ubdrv_get_chip_type();
    if (chip_type == HISI_CHIP_UNKNOWN) {
        ubdrv_err("Get chip type fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = soc_resmng_dev_set_key_value(dev_id, "chip_type", chip_type);
    if (ret != 0) {
        ubdrv_err("Failed to set chip type to soc_res. (chip_type=%u;ret=%d)\n", chip_type, ret);
        return ret;
    }

    load_input->file_name[UBDRV_STR_MAX_LEN - 1] = '\0';
    file_id = ubdrv_uvb_get_file_id_by_name(load_input->file_name);
    if (file_id == UBDRV_UVB_INVALID_FILE_ID) {
        ubdrv_err("Find file_id by name failed.\n");
        return -EINVAL;
    }

    if (dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) {
        ubdrv_err("Get devid by slotid failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }

    asd_dev = ubdrv_get_asd_dev_by_devid(dev_id);
    if (asd_dev == NULL) {
        ubdrv_err("Get asd_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    ret = ubdrv_alloc_load_segment(dev_id, &asd_dev->loader, UBDRV_MAX_FILE_SIZE);
    if (ret != 0) {
        ubdrv_err("Blocks alloc failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    devdrv_ub_set_device_boot_status(dev_id, DSMI_BOOT_STATUS_BIOS);

    file_path = asd_dev->loader.file_path;
    (void)memset_s(file_path, UBDRV_STR_MAX_LEN, 0, UBDRV_STR_MAX_LEN);
    ret = ubdrv_single_file_path_init(file_path, UBDRV_STR_MAX_LEN, file_id);
    if (ret != 0) {
        ubdrv_err("Single file path init failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto exit;
    }

    ret = ubdrv_load_get_file_size(dev_id, file_path, file_id, &p_file, &asd_dev->loader.remain_size);
    if (ret != 0) {
        goto exit;
    }
    ka_fs_filp_close(p_file, NULL);

    ret = ubdrv_uvb_msg_process_part_load(asd_dev, 0, file_id);
    if (ret != 0) {
        ubdrv_err("File read err. (dev_id=%u; file_name=%s; ret=%d)\n", dev_id, file_path, ret);
        goto exit;
    }

    load_output->file_address = asd_dev->loader.blocks->blocks_addr[0].dma_addr;
    load_output->file_total_size = (u64)asd_dev->loader.remain_size;
    load_output->file_id = file_id;
    load_output->token_id = asd_dev->loader.tid;
    load_output->section_size = UBDRV_MAX_FILE_SIZE;

    return 0;

exit:
    ubdrv_free_load_segment(dev_id, &asd_dev->loader);
    return ret;
}

STATIC void ubdrv_set_load_file_cfg(u32 device_id)
{
    if (device_id == UBDRV_CLOUD_V4_DEVICE_ID) {
        g_load_file = g_cloud_v4_load_file;
    } else if (device_id == UBDRV_CLOUD_V5_DEVICE_ID) {
        g_load_file = g_cloud_v5_load_file;
    } else if (device_id == UBDRV_MINI_V4_DEVICE_ID) {
        g_load_file = g_mini_v4_load_file;
    } else {
        g_load_file = NULL;
        ubdrv_err("Set load file cfg error. (device_id=0x%x)\n", device_id);
    }
}

STATIC int ubdrv_uvb_msg_get_ability(struct cis_message *msg)
{
    struct ubdrv_uvb_get_ability_output *load_output = NULL;
    struct ubdrv_uvb_get_ability_input *load_input = NULL;
    u32 device_id = UBDRV_CLOUD_V4_DEVICE_ID;
    int ret = -EINVAL;

    // sizeof(u32) for compatibility with legacy versions
    ret = ubdrv_uvb_msg_check_para(msg, sizeof(u32), sizeof(struct ubdrv_uvb_get_ability_output));
    if (ret != 0) {
        ubdrv_err("UVB msg callback para check failed.\n");
        return ret;
    }

    load_output = (struct ubdrv_uvb_get_ability_output *)msg->output;

    if (msg->input_size >= sizeof(struct ubdrv_uvb_get_ability_input)) {
        load_input = (struct ubdrv_uvb_get_ability_input *)msg->input;
        device_id = load_input->device_id;
    }

    ret = ubdrv_set_chip_type_from_image_load(device_id);
    if (ret != 0) {
        ubdrv_err("ubdrv set chip type error. (ret=%d)\n", ret);
        return ret;
    }

    ubdrv_set_load_file_cfg(device_id);

    load_output->capability = 0;
    load_output->capability |= UBDRV_UB_MEMORY_MASK;
    *msg->p_output_size = sizeof(struct ubdrv_uvb_get_ability_output);
    return 0;
}

static const ubdrv_uvb_msg_ctrl_t g_ubdrv_uvb_func_list[] = {
    {UBIOS_CALL_ID_GET_ABILITY, UBIOS_USER_ID_UB_DEVICE, ubdrv_uvb_msg_get_ability},
    {UBIOS_CALL_ID_GET_FILE_INFO, UBIOS_USER_ID_UB_DEVICE, ubdrv_uvb_msg_get_file_info},
    {UBIOS_CALL_ID_GET_FILE_LOADING_STATE, UBIOS_USER_ID_UB_DEVICE, ubdrv_uvb_msg_get_loading_state},
    {UBIOS_CALL_ID_GET_LOADING_STATE, UBIOS_USER_ID_UB_DEVICE, ubdrv_uvb_msg_get_final_state},
};

STATIC int ubdrv_cis_func_register(void)
{
    int ret, ret_tmp, i, len;

    len = sizeof(g_ubdrv_uvb_func_list) / sizeof(ubdrv_uvb_msg_ctrl_t);
    for (i = 0; i < len; i++) {
        ret = register_local_cis_func(g_ubdrv_uvb_func_list[i].call_id, g_ubdrv_uvb_func_list[i].receiver_id,
                                      g_ubdrv_uvb_func_list[i].func);
        if (ret != 0) {
            ubdrv_err("Register uvb msg callback failed.(ret=%d; call_id=0x%x)\n", ret,
                      g_ubdrv_uvb_func_list[i].call_id);
            goto register_failed;
        }
    }

    return 0;

register_failed:
    for (i = i - 1; i >= 0; i--) {
        ret_tmp = unregister_local_cis_func(g_ubdrv_uvb_func_list[i].call_id, g_ubdrv_uvb_func_list[i].receiver_id);
        if (ret_tmp != 0) {
            ubdrv_err("Unregister uvb msg callback failed. (ret=%d; call_id=0x%x)\n", ret_tmp,
                      g_ubdrv_uvb_func_list[i].call_id);
        }
    }

    return ret;
}

STATIC void ubdrv_cis_func_unregister(void)
{
    int ret, i, len;

    len = sizeof(g_ubdrv_uvb_func_list) / sizeof(ubdrv_uvb_msg_ctrl_t);
    for (i = len - 1; i >= 0; i--) {
        ret = unregister_local_cis_func(g_ubdrv_uvb_func_list[i].call_id, g_ubdrv_uvb_func_list[i].receiver_id);
        if (ret != 0) {
            ubdrv_warn("Unregister uvb msg callback has problem.(ret=%d; call_id=0x%x)\n", ret,
                       g_ubdrv_uvb_func_list[i].call_id);
        }
    }

    return;
}
#endif

int ubdrv_load_device_init(void)
{
    int ret = 0;

    ret = ubdrv_sdk_path_init();
    if (ret != 0) {
        ubdrv_err("Device sdk_path init failed. (ret=%d)\n", ret);
        return ret;
    }
#ifdef CFG_FEATURE_CDMA_LOAD_IMAGE
    ret = ubdrv_cis_func_register();
    if (ret != 0) {
        ubdrv_err("Register uvb msg callback failed. (ret=%d)\n", ret);
    }
#endif
    return ret;
}

void ubdrv_load_device_uninit(void)
{
    struct ascend_dev *asd_dev = NULL;
    int i;

#ifdef CFG_FEATURE_CDMA_LOAD_IMAGE
    /* unregister image load UVB msg callback */
    ubdrv_cis_func_unregister();
#endif
    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        asd_dev = ubdrv_get_asd_dev_by_devid(i);
        ubdrv_free_load_segment(i, &asd_dev->loader);
    }
    return;
}
