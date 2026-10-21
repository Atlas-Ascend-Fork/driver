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

// ubus header find in ubengine/ssapi/kernelspace
#include "ka_driver_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_list_pub.h"
#include "ka_barrier_pub.h"

#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_uda.h"
#include "asdrv_ub_hotreset.h"
#include "dms/dms_interface.h"
#include "pbl/pbl_soc_res_sync.h"
#include "ascend_kernel_hal.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_common_msg.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_pair_dev_info.h"
#include "asdrv_ub_rao.h"
#include "asdrv_ub_urma_chan.h"
#include "asdrv_ub_mem_decoder.h"
#include "asdrv_ub_main.h"

ka_mutex_t g_ubdrv_p2p_mutex;
u32 g_ubdrv_chip_type = HISI_CHIP_UNKNOWN;

STATIC struct ubdrv_p2p_attr_info (*g_p2p_attr_info)[UBDRV_P2P_SUPPORT_MAX_DEVICE] = NULL;
#ifdef CFG_FEATURE_SLAVE_MODE
STATIC const char ascend_ub_driver_name[] = "asdrv_ub";
#define ASCEND_UDEV_VENDOR_ID_STUB 0xffffff
#define ASCEND_UDEV_VENDOR_ID_HW 0xe0fc
#define ASCEND_UDEV_DEVICE_ID_CLOUD_V4 0xd800

STATIC const struct ub_device_id ascend_ubus_tbl[] = {
    {UB_DEVICE(ASCEND_UDEV_VENDOR_ID_STUB, ASCEND_UDEV_DEVICE_ID_CLOUD_V4), HISI_CLOUD_V4, 0},
    {UB_DEVICE(ASCEND_UDEV_VENDOR_ID_HW, ASCEND_UDEV_DEVICE_ID_CLOUD_V4), HISI_CLOUD_V4, 0},
    /* required last entry */
    {0},
};

static void ubdrv_init_dev_num(void)
{
    struct ub_entity *udev = NULL;
    u32 dev_num = 0, dev_num_total = 0;
    int id_num = (int)(sizeof(ascend_ubus_tbl) / sizeof(struct ub_device_id));
    int i;

    for (i = 0; i < id_num; i++) {
        udev = NULL;
        dev_num = 0;

        do {
            udev = ub_get_device(ascend_ubus_tbl[i].vendor, ascend_ubus_tbl[i].device, udev);
            if (udev == NULL) {
                break;
            }
            dev_num += 1;
        } while (1);

        ubdrv_info("Find out device. (vendor=%x; device=%x; dev_num=%u)\n", ascend_ubus_tbl[i].vendor,
                   ascend_ubus_tbl[i].device, dev_num);
        dev_num_total += dev_num;
    }

    if (dev_num_total != 0) {
        (void)uda_set_detected_phy_dev_num(dev_num_total);
        ubdrv_info("Find out total device. (dev_num=%u)\n", dev_num_total);
    } else {
        ubdrv_info("No device found. (dev_num=%u)\n", dev_num_total);
    }
}

// shr_para info offset is 1KB in LOAD SRAM
// hardware info offset is 0x5800 in LOAD SRAM
STATIC int ubdrv_init_share_param_info(struct ascend_dev *asd_dev)
{
    ubdrv_shr_para_t __ka_mm_iomem *shr_para = NULL;
    ubdrv_hw_info_t __ka_mm_iomem *hw_info = NULL;
    int ret;
    u32 dev_id;

    // master-slave
    dev_id = asd_dev->ub_dev->dev_id;
    shr_para = asd_dev->ub_dev->res.mem_bar.va + UBDRV_IO_LOAD_SRAM_OFFSET + UBDRV_CLOUD_V4_SHR_PARA_SRAM_OFFSET;
    hw_info = asd_dev->ub_dev->res.mem_bar.va + UBDRV_IO_LOAD_SRAM_OFFSET + UBDRV_CLOUD_V4_HW_INFO_SRAM_OFFSET;

    ret = memcpy_s(&(asd_dev->shr_para), sizeof(ubdrv_shr_para_t), shr_para, sizeof(ubdrv_shr_para_t));
    if (ret != 0) {
        ubdrv_err("Copy share param failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    ubdrv_info("Get share param. (devid=%u; load flag=%u; chip_id=%u; node_id=%u; slot_id=%u; chip_type=%u)\n", dev_id,
               asd_dev->shr_para.load_flag, asd_dev->shr_para.chip_id, asd_dev->shr_para.node_id,
               asd_dev->shr_para.slot_id, asd_dev->shr_para.chip_type);

    ret = memcpy_s(&(asd_dev->hw_info), sizeof(ubdrv_hw_info_t), hw_info, sizeof(ubdrv_hw_info_t));
    if (ret != 0) {
        ubdrv_err("Copy hardware info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    ubdrv_info("Get hardware info. (devid=%u; chip_id=%u; multi_chip=%u; multi_die=%u; mainboard_id=0x%x)\n", dev_id,
               asd_dev->hw_info.chip_id, asd_dev->hw_info.multi_chip, asd_dev->hw_info.multi_die,
               asd_dev->hw_info.mainboard_id);

    return 0;
}

STATIC int ubdrv_resource_init(struct ascend_ub_dev *udev)
{
    int ret;
    u64 pa = 0;
    struct ub_entity *ubus_dev;

    ubus_dev = udev->ubus_dev;
    ub_fe_enable(ubus_dev, 1);
    ret = ka_mm_dma_set_mask_and_coherent(&ubus_dev->dev, KA_DMA_BIT_MASK(UBDEV_UBUS_DMA_BIT));
    if (ret != 0) {
        ubdrv_err("Can't set consistent UBUS DMA. (ret=%d)\n", ret);
        goto err_enable_device;
    }

    udev->res.io_bar.size = ub_resource_len(ubus_dev, ASCEND_UB_IO_RESOURCE);
    udev->res.io_bar.va = ub_iomap(ubus_dev, ASCEND_UB_IO_RESOURCE, 0);
    if (udev->res.io_bar.va == NULL) {
        ubdrv_err("Map io base failed.\n");
        goto err_clear_io;
    }

    pa = ub_resource_start(ubus_dev, ASCEND_UB_MEM_RESOURCE);
    udev->res.mem_bar.size = ub_resource_len(ubus_dev, ASCEND_UB_MEM_RESOURCE);
    if ((pa == 0) || (udev->res.mem_bar.size == 0)) {
        ubdrv_err("Ub get mem resource start failed.\n");
        udev->res.mem_bar.size = 0;
        goto err_unmap_io;
    }

    udev->res.mem_bar.va = ka_base_devm_ioremap_wc(&ubus_dev->dev, pa, udev->res.mem_bar.size);
    if (udev->res.mem_bar.va == NULL) {
        ubdrv_err("Map mem base failed.\n");
        goto err_unmap_io_base;
    }

    ret = ubdrv_init_share_param_info(udev->asd_dev);
    if (ret != 0) {
        ubdrv_err("Init share param info failed. (devid=%u; ret=%d)\n", udev->dev_id, ret);
        goto err_unmap_mem_base;
    }

    return 0;

err_unmap_mem_base:
    ka_base_devm_iounmap(&ubus_dev->dev, udev->res.mem_bar.va);
    udev->res.mem_bar.va = 0;
err_unmap_io_base:
    udev->res.mem_bar.size = 0;
err_unmap_io:
    ub_iounmap(udev->res.io_bar.va);
    udev->res.io_bar.va = 0;
err_clear_io:
    udev->res.io_bar.size = 0;
err_enable_device:
    ub_fe_enable(ubus_dev, 0);
    return -ENOMEM;
}

STATIC void ubdrv_resource_uninit(struct ascend_ub_dev *udev)
{
    struct ub_entity *ubus_dev;

    ubus_dev = udev->ubus_dev;
    ka_base_devm_iounmap(&ubus_dev->dev, udev->res.mem_bar.va);
    udev->res.mem_bar.va = 0;
    udev->res.mem_bar.size = 0;
    ub_iounmap(udev->res.io_bar.va);
    udev->res.io_bar.va = 0;
    udev->res.io_bar.size = 0;
    ub_fe_enable(ubus_dev, 0);
    ubdrv_debug("Ubdev region uninit success.\n");
    return;
}

STATIC int ubdrv_prepare_half_probe(struct ascend_ub_dev *udev)
{
    udev->load_work.udev = udev;
    udev->loader.udev = udev;

    KA_TASK_INIT_WORK(&udev->load_work.work, ubdrv_load_file);
    /* start load file work queue and timer */
    ka_task_schedule_work(&udev->load_work.work);
    ubdrv_info("Prepare half probe finish. (dev_id=%u)\n", udev->dev_id);

    return 0;
}

STATIC void ubdrv_prepare_half_free(struct ascend_ub_dev *udev)
{
    ubdrv_load_exit(udev);
    ka_task_cancel_work_sync(&udev->load_work.work);
}

STATIC int ubdrv_alloc_devid_inturn(u32 begin, u32 stride)
{
    int dev_id = -1;
    u32 i;

    for (i = begin; i < ASCEND_UB_DEV_MAX_NUM; i = i + stride) {
        if (ubdrv_get_startup_flag(i) == UBDRV_DEV_STARTUP_UNPROBED) {
            ubdrv_set_startup_flag(i, UBDRV_DEV_STARTUP_PROBED);
            dev_id = (int)i;
            break;
        }
    }

    return dev_id;
}

STATIC int ubdrv_alloc_devid(struct ascend_ub_dev *ub_dev)
{
    struct ascend_ub_ctrl *ub_ctrl;
    int dev_id = -1;

    ub_ctrl = get_global_ub_ctrl();
    ka_task_mutex_lock(&ub_ctrl->mutex_lock);
    dev_id = ubdrv_alloc_devid_inturn(0, 1);
    ka_task_mutex_unlock(&ub_ctrl->mutex_lock);

    return dev_id;
}

STATIC void ubdrv_ub_dev_init(struct ascend_ub_dev *ub_dev, struct ub_entity *ubus_dev, u32 dev_id)
{
    struct ascend_ub_ctrl *ub_ctrl;

    ub_dev->ubus_dev = ubus_dev;
    ub_dev->dev_id = dev_id;
    ub_ctrl = get_global_ub_ctrl();
    ub_ctrl->asd_dev[dev_id].ub_dev = ub_dev;
    ub_dev->asd_dev = &(ub_ctrl->asd_dev[dev_id]);
}

STATIC void ubdrv_ub_dev_uninit(struct ascend_ub_dev *ub_dev)
{
    struct ascend_ub_ctrl *ub_ctrl;

    ub_ctrl = get_global_ub_ctrl();
    ub_dev->ubus_dev = NULL;
    ub_ctrl->asd_dev[ub_dev->dev_id].ub_dev = NULL;
    ub_dev->dev_id = 0;
    ub_dev->asd_dev = NULL;
}

STATIC int ubdrv_register_devctrl(struct ascend_ub_dev *ub_dev, struct ub_entity *ubus_dev)
{
    int dev_id;

    ub_dev->ubus_dev = ubus_dev;

    dev_id = ubdrv_alloc_devid(ub_dev);
    if (dev_id < 0) {
        ubdrv_err("UB device register failed. (dev_id=%d)\n", dev_id);
        return -ENOSPC;
    } else {
        ubdrv_info("Alloc devid. (dev_id=%d)\n", dev_id);
        ubdrv_ub_dev_init(ub_dev, ubus_dev, dev_id);
        return 0;
    }
}

STATIC void ubdrv_unregister_devctrl(struct ascend_ub_dev *ub_dev)
{
    ubdrv_set_startup_flag(ub_dev->dev_id, UBDRV_DEV_STARTUP_UNPROBED);

    ubdrv_ub_dev_uninit(ub_dev);

    return;
}

STATIC int ubdrv_ubus_probe(struct ub_entity *ubus_dev, const struct ub_device_id *utbl_entry)
{
    int ret;
    struct ascend_ub_dev *ub_dev;

    if ((ubus_dev == NULL) || (utbl_entry == NULL)) {
        ubdrv_err("Ascend ubus probe check fail.\n");
        return -EINVAL;
    }

    ubdrv_info("Ascend ubus probe driver IN. (chip_type=%lu; ub dev number=%u)\n", utbl_entry->driver_data,
               ubus_dev->udev_num);
    ub_dev = ubdrv_kzalloc(sizeof(struct ascend_ub_dev), KA_GFP_KERNEL);
    if (ub_dev == NULL) {
        ubdrv_err("Alloc ub_dev mem failed. (ub dev number=%u)\n", ubus_dev->udev_num);
        return -ENOMEM;
    }

    /* alloc devid */
    ret = ubdrv_register_devctrl(ub_dev, ubus_dev);
    if (ret != 0) {
        ubdrv_err("Failed to init ubus. (dev_id=%u;ret=%d)\n", ub_dev->dev_id, ret);
        goto err_register_devctrl;
    }

    dev_set_drvdata(&ubus_dev->dev, ub_dev);
    ret = ubdrv_resource_init(ub_dev);
    if (ret != 0) {
        ubdrv_err("Failed to init ubus. (dev_id=%u;ret=%d)\n", ub_dev->dev_id, ret);
        goto err_free_set_drvdata;
    }

    ret = ubdrv_prepare_half_probe(ub_dev);
    if (ret != 0) {
        ubdrv_err("Prepare half probe fail. (dev_id=%u;ret=%d)\n", ub_dev->dev_id, ret);
        goto err_region_init;
    }

    ubdrv_set_startup_flag(ub_dev->dev_id, UBDRV_DEV_STARTUP_TOP_HALF_OK);
    ubdrv_info("Ascend ubus probe driver OUT. (chip_type=%lu; ub dev number=%u; dev_id=%u)\n", utbl_entry->driver_data,
               ubus_dev->udev_num, ub_dev->dev_id);
    return 0;

err_region_init:
    ubdrv_resource_uninit(ub_dev);
err_free_set_drvdata:
    dev_set_drvdata(&ubus_dev->dev, NULL);
    ubdrv_unregister_devctrl(ub_dev);
err_register_devctrl:
    ubdrv_kfree(ub_dev);
    return ret;
}

STATIC void ubdrv_ubus_remove(struct ub_entity *ubus_dev)
{
    struct ascend_ub_dev *ub_dev;
    u32 dev_id;

    if (ubus_dev == NULL) {
        ubdrv_err("Ascend ubus remove check fail.\n");
        return;
    }

    ub_dev = ka_driver_dev_get_drvdata(&ubus_dev->dev);
    if (ub_dev == NULL) {
        ubdrv_err("Ascend remove fail, ub_dev is null.\n");
        return;
    }
    dev_id = ub_dev->dev_id;
    if (dev_id >= ASCEND_UDMA_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return;
    }

    ubdrv_info("Ascend ubus remove driver start. (dev_id=%u; ub dev number=%u)\n", dev_id, ubus_dev->udev_num);

    /* load exit */
    ubdrv_prepare_half_free(ub_dev);
    ubdrv_remove_davinci_dev(dev_id, UDA_REAL);
    ubdrv_del_msg_device(dev_id, UBDRV_DEVICE_UNINIT);
    ubdrv_resource_uninit(ub_dev);
    dev_set_drvdata(&ubus_dev->dev, NULL);
    ubdrv_unregister_devctrl(ub_dev);
    ubdrv_kfree(ub_dev);

    ubdrv_info("Ascend ubus remove driver exit. (dev_id=%u; ub dev number=%u)\n", dev_id, ubus_dev->udev_num);
    return;
}

STATIC int ubdrv_ubus_virt_configure(struct ub_entity *ubus_dev, int num, bool is_en)
{
    return num;
}

STATIC void ubdrv_ubus_reset_prepare(struct ub_entity *ubus_dev)
{
    ubdrv_info("UBUS ELR start.\n");
}

STATIC void ubdrv_ubus_reset_done(struct ub_entity *ubus_dev)
{
    ubdrv_info("UBUS ELR done.\n");
}

STATIC const struct ub_error_handlers ascend_ubus_err_handler = {
    .reset_prepare = ubdrv_ubus_reset_prepare,
    .reset_done = ubdrv_ubus_reset_done,
};

// clang-format off
STATIC struct ub_driver ascend_ubus_driver = {
    ka_driver_init_drv_name(ascend_ub_driver_name)
    ka_driver_init_drv_id_table(ascend_ubus_tbl)
    ka_driver_init_drv_probe(ubdrv_ubus_probe)
    ka_driver_init_drv_remove(ubdrv_ubus_remove)
    ka_driver_init_drv_virt_configure(ubdrv_ubus_virt_configure)
    ka_driver_init_drv_err_handler(&ascend_ubus_err_handler)
};
// clang-format on
#endif

STATIC ka_device_t *ubdrv_ub_get_device(u32 devid, u32 vfid, u32 udevid)
{
    struct ubcore_eid_info remote_eid = {0};
    struct ubcore_eid_info local_eid = {0};
    struct ub_idev *idev = NULL;

    if (ubdrv_query_h2d_pair_chan_info(udevid, &local_eid, &remote_eid) != 0) {
        return NULL;
    }
    idev = ubdrv_get_idev_by_eid(&local_eid, UBDRV_INFO_LEVEL);
    if ((idev == NULL) || (idev->ubc_dev == NULL)) {
        ubdrv_err("Find idev fail. (dev_id=%u)\n", udevid);
        return NULL;
    }
    return &idev->ubc_dev->dev;
}

int ubdrv_ub_get_dev_topology(u32 devid, u32 peer_devid, int *topo_type)
{
    struct ascend_ub_msg_dev *msg_dev;
    struct ascend_ub_msg_dev *peer_msg_dev;

    if ((devid >= ASCEND_UB_DEV_MAX_NUM) || (peer_devid >= ASCEND_UB_DEV_MAX_NUM) || (devid == peer_devid)) {
        ubdrv_err("Invalid devid. (devid=%u;peer_id=%u)\n", devid, peer_devid);
        return -EINVAL;
    }
    if (topo_type == NULL) {
        ubdrv_err("Input topo_type is null.\n");
        return -EINVAL;
    }
    msg_dev = ubdrv_get_msg_dev_by_devid(devid);
    peer_msg_dev = ubdrv_get_msg_dev_by_devid(peer_devid);
    if ((msg_dev == NULL) || (peer_msg_dev == NULL)) {
        ubdrv_err("Get msg_dev failed. (%s=NULL;devid=%u;peer_devid=%u)\n", msg_dev == NULL ? "devid" : "peer_devid",
                  devid, peer_devid);
        return -EINVAL;
    }
    *topo_type = TOPOLOGY_UB;
    return 0;
}

STATIC int devdrv_get_ub_device_info(u32 devid, struct devdrv_base_device_info *dev_info)
{
    return -EOPNOTSUPP;
}

void ubdrv_free_attr_info(void)
{
    if (g_p2p_attr_info != NULL) {
        ubdrv_kfree(g_p2p_attr_info);
        g_p2p_attr_info = NULL;
    }
    return;
}

int ubdrv_alloc_attr_info(void)
{
    ka_task_mutex_init(&g_ubdrv_p2p_mutex);

    g_p2p_attr_info = ubdrv_kzalloc(
        UBDRV_P2P_SUPPORT_MAX_DEVICE * UBDRV_P2P_SUPPORT_MAX_DEVICE * sizeof(struct ubdrv_p2p_attr_info),
        KA_GFP_KERNEL);
    if (g_p2p_attr_info == NULL) {
        ubdrv_err("Alloc p2p_attr_info failed.\n");
        return -ENOMEM;
    }

    return 0;
}

STATIC int ubdrv_get_p2p_attr_proc_id(const struct ubdrv_p2p_attr_info *p2p_attr, int pid)
{
    int index, i;
    index = -1;

    for (i = 0; i < UBDRV_P2P_MAX_PROC_NUM; i++) {
        if ((p2p_attr->proc_ref[i] > 0) && (p2p_attr->pid[i] == pid)) {
            index = i;
            break;
        }
    }

    return index;
}

STATIC int ubdrv_get_idle_p2p_attr_proc_id(const struct ubdrv_p2p_attr_info *p2p_attr)
{
    int index, i;
    index = -1;

    for (i = 0; i < UBDRV_P2P_MAX_PROC_NUM; i++) {
        if (p2p_attr->proc_ref[i] == 0) {
            index = i;
            break;
        }
    }

    return index;
}

STATIC void ubdrv_device_p2p_del_ref(struct ubdrv_p2p_attr_info *p2p_attr, int index)
{
    p2p_attr->proc_ref[index]--;
    p2p_attr->ref--;

    if (p2p_attr->proc_ref[index] == 0) {
        p2p_attr->pid[index] = 0;
    }
}

STATIC struct ubdrv_p2p_attr_info *ubdrv_get_p2p_attr(u32 dev_id, u32 peer_dev_id)
{
    return &g_p2p_attr_info[dev_id][peer_dev_id];
}

int ubdrv_enable_p2p(int pid, u32 dev_id, u32 peer_dev_id)
{
    struct ubdrv_p2p_attr_info *p2p_attr = NULL;
    struct ubdrv_p2p_attr_info *p2p_peer_attr = NULL;
    int ret = 0;
    int index;

    ka_task_mutex_lock(&g_ubdrv_p2p_mutex);
    p2p_attr = ubdrv_get_p2p_attr(dev_id, peer_dev_id);
    index = ubdrv_get_p2p_attr_proc_id(p2p_attr, pid);
    if (index < 0) {
        index = ubdrv_get_idle_p2p_attr_proc_id(p2p_attr);
        if (index < 0) {
            ubdrv_err("dev_id used up. (pid=%d; dev_id=%u; peer_dev_id=%u)\n", pid, dev_id, peer_dev_id);
            ret = -ENOMEM;
            goto out;
        }
        p2p_peer_attr = ubdrv_get_p2p_attr(peer_dev_id, dev_id);
        /* First configuration, and peer all ready configuration */
        if ((p2p_attr->ref == 0) && (p2p_peer_attr->ref > 0)) {
            ubdrv_info("Enable p2p success. (pid=%d; dev_id=%u; peer_dev_id=%u)\n", pid, dev_id, peer_dev_id);
        }
        p2p_attr->pid[index] = pid;
    }
    p2p_attr->proc_ref[index]++;
    p2p_attr->ref++;
out:
    ka_task_mutex_unlock(&g_ubdrv_p2p_mutex);
    return ret;
}

int ubdrv_disable_p2p(int pid, u32 dev_id, u32 peer_dev_id)
{
    struct ubdrv_p2p_attr_info *p2p_attr = NULL;
    struct ubdrv_p2p_attr_info *p2p_peer_attr = NULL;
    int ret = 0;
    int index;

    ka_task_mutex_lock(&g_ubdrv_p2p_mutex);
    p2p_attr = ubdrv_get_p2p_attr(dev_id, peer_dev_id);
    index = ubdrv_get_p2p_attr_proc_id(p2p_attr, pid);
    if (index < 0) {
        ret = -ESRCH;
        goto out;
    }

    if ((p2p_attr->proc_ref[index] <= 0) || (p2p_attr->ref <= 0)) {
        ubdrv_err("p2p_attr is error. (dev_id=%u; peer_dev_id=%u; pid=%d; proc_ref=%d; total_ref=%d)\n", pid, dev_id,
                  peer_dev_id, p2p_attr->proc_ref[index], p2p_attr->ref);
        ret = -ESRCH;
        goto out;
    }

    ubdrv_device_p2p_del_ref(p2p_attr, index);
    if (p2p_attr->ref == 0) {
        /* peer not yet cancel configuration */
        p2p_peer_attr = ubdrv_get_p2p_attr(peer_dev_id, dev_id);
        if (p2p_peer_attr->ref > 0) {
            ubdrv_info("Disable p2p success. (pid=%d; dev_id=%u; peer_dev_id=%u)\n", pid, dev_id, peer_dev_id);
        }
    }
out:
    ka_task_mutex_unlock(&g_ubdrv_p2p_mutex);
    return ret;
}

bool ubdrv_is_p2p_enabled(u32 dev_id, u32 peer_dev_id)
{
    struct ubdrv_p2p_attr_info *p2p_attr = NULL;
    struct ubdrv_p2p_attr_info *p2p_peer_attr = NULL;
    bool ret;

    ka_task_mutex_lock(&g_ubdrv_p2p_mutex);
    p2p_attr = ubdrv_get_p2p_attr(dev_id, peer_dev_id);
    p2p_peer_attr = ubdrv_get_p2p_attr(peer_dev_id, dev_id);
    ret = (p2p_attr->ref > 0) && (p2p_peer_attr->ref > 0);
    ka_task_mutex_unlock(&g_ubdrv_p2p_mutex);

    return ret;
}

int ubdrv_get_p2p_access_status(u32 devid, u32 peer_devid, int *status)
{
    *status = UBDRV_P2P_ACCESS_ENABLE;
    return 0;
}

int ubdrv_get_p2p_capability(u32 dev_id, u64 *capability)
{
    if ((dev_id >= ASCEND_UB_DEV_MAX_NUM) || (capability == NULL)) {
        ubdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *capability = P2PCAPABILITY_CAPABILITY_ID | P2PCAPABILITY_NEXT_POINTER | P2PCAPABILITY_CAPABILITY_LENGTH |
                  P2PCAPABILITY_SIGNATURE_BITS | P2PCAPABILITY_VERSION | P2PCAPABILITY_GROUP_ID |
                  P2PCAPABILITY_RESERVED;

    return 0;
}

void ubdrv_flush_p2p(int pid)
{
    struct ubdrv_p2p_attr_info *p2p_attr = NULL;
    int num, index;
    u32 i, j;

    for (i = 0; i < UBDRV_P2P_SUPPORT_MAX_DEVICE; i++) {
        for (j = 0; j < UBDRV_P2P_SUPPORT_MAX_DEVICE; j++) {
            if (i == j) {
                continue;
            }

            /* get pid attr */
            p2p_attr = ubdrv_get_p2p_attr(i, j);
            index = ubdrv_get_p2p_attr_proc_id(p2p_attr, pid);
            if (index < 0) {
                continue;
            }
            /* pid has config p2p */
            num = 0;
            while (ubdrv_disable_p2p(pid, i, j) == 0) {
                num++;
            }

            if (num > 0) {
                ubdrv_info("Get ubdrv_disable_p2p dev_num. (dev_id=%d; peer_dev_id=%d; pid=%d; recycle=%d)\n", i, j,
                           pid, num);
            }
        }
    }
}
KA_EXPORT_SYMBOL(ubdrv_flush_p2p);

STATIC int ubdrv_p2p_attr_op(struct devdrv_base_comm_p2p_attr *p2p_attr)
{
    int ret = 0;

    if (p2p_attr == NULL) {
        ubdrv_err("p2p_attr is NULL\n");
        return -EINVAL;
    }

    if ((p2p_attr->devid >= ASCEND_UB_DEV_MAX_NUM) || (p2p_attr->peer_dev_id >= ASCEND_UB_DEV_MAX_NUM) ||
        (p2p_attr->devid == p2p_attr->peer_dev_id)) {
        ubdrv_err("Invalid id. (pid=%d; devid=%u; peer_dev_id=%u)\n", ka_task_get_current_tgid(), p2p_attr->devid,
                  p2p_attr->peer_dev_id);
        return -EINVAL;
    }
    if (p2p_attr->op != DEVDRV_BASE_P2P_CAPABILITY_QUERY) {
        if (uda_is_udevid_exist(p2p_attr->peer_dev_id) != true) {
            ubdrv_err("Trans failed. (peer_dev_id=%u)\n", p2p_attr->peer_dev_id);
            return p2p_attr->op == DEVDRV_BASE_P2P_ACCESS_STATUS_QUERY ? -ENXIO : -EINVAL;
        }
    }

    switch (p2p_attr->op) {
        case DEVDRV_BASE_P2P_ADD:
            ret = ubdrv_enable_p2p(ka_task_get_current_tgid(), p2p_attr->devid, p2p_attr->peer_dev_id);
            break;
        case DEVDRV_BASE_P2P_DEL:
            ret = ubdrv_disable_p2p(ka_task_get_current_tgid(), p2p_attr->devid, p2p_attr->peer_dev_id);
            break;
        case DEVDRV_BASE_P2P_QUERY:
            if (ubdrv_is_p2p_enabled(p2p_attr->devid, p2p_attr->peer_dev_id)) {
                *p2p_attr->status = UBDRV_ENABLE; // p2p is enable
            } else {
                *p2p_attr->status = UBDRV_DISABLE; // p2p is disable
            }
            break;
        case DEVDRV_BASE_P2P_ACCESS_STATUS_QUERY:
            ret = ubdrv_get_p2p_access_status(p2p_attr->devid, p2p_attr->peer_dev_id, p2p_attr->status);
            break;
        case DEVDRV_BASE_P2P_CAPABILITY_QUERY:
            ret = ubdrv_get_p2p_capability(p2p_attr->devid, p2p_attr->capability);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

int ubdrv_get_host_phy_flag(u32 devid, u32 *host_flag)
{
    struct ascend_dev *asd_dev = NULL;

    if (host_flag == NULL) {
        ubdrv_err("Check host flag is null. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    asd_dev = ubdrv_get_asd_dev_by_devid(devid);
    if (asd_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (asd_dev->phy_flag == true) {
        *host_flag = DEVDRV_HOST_PHY_MACH_FLAG;
    } else {
        *host_flag = DEVDRV_VIRT_PASS_THROUGH_MACH_FLAG;
    }
    return 0;
}

struct devdrv_comm_ops g_ubdrv_ops = {
    .comm_type = DEVDRV_COMMNS_UB,
    .alloc_non_trans = devdrv_ub_msg_alloc_non_trans_queue,
    .free_non_trans = devdrv_ub_msg_free_non_trans_queue,
    .sync_msg_send = devdrv_ub_sync_msg_send,
    .register_common_msg_client = devdrv_ub_register_common_msg_client,
    .unregister_common_msg_client = devdrv_ub_unregister_common_msg_client,
    .common_msg_send = devdrv_ub_common_msg_send,
    .get_boot_status = devdrv_ub_get_device_boot_status,
    .get_host_phy_mach_flag = ubdrv_get_host_phy_flag,
    .get_env_boot_type = devdrv_ub_get_env_boot_type,
    .set_msg_chan_priv = ubdrv_set_msg_chan_priv,
    .get_msg_chan_priv = ubdrv_get_msg_chan_priv,
    .get_msg_chan_devid = ubdrv_get_msg_chan_devid,
    .get_connect_type = devdrv_ub_get_connect_protocol,
    .get_pfvf_type_by_devid = devdrv_ub_get_pfvf_type_by_devid,
    .get_device_info = devdrv_get_ub_device_info,
    .mdev_vm_boot_mode = devdrv_ub_is_mdev_vm_boot_mode,
    .sriov_support = devdrv_ub_is_sriov_support,
    .sriov_enable = ubdrv_ub_enable_funcs,
    .sriov_disable = ubdrv_ub_disable_funcs,
    .get_device = ubdrv_ub_get_device,
    .get_dev_topology = ubdrv_ub_get_dev_topology,
    .hotreset_assemble = ubdrv_hot_reset_device,
    .prereset_assemble = ubdrv_device_pre_reset,
    .rescan_atomic = ubdrv_device_rescan,
    .register_rao_client = ubdrv_register_rao_client,
    .unregister_rao_client = ubdrv_unregister_rao_client,
    .rao_read = ubdrv_rao_read,
    .rao_write = ubdrv_rao_write,
    .get_all_device_count = ubdrv_get_all_device_count,
    .get_device_probe_list = ubdrv_get_device_probe_list,
    .p2p_attr_op = ubdrv_p2p_attr_op,
    .get_urma_info_by_eid = ubdrv_get_urma_info,
    .get_ub_dev_info = ubdrv_get_ub_dev_info,
    .get_token_val = ubdrv_get_token_val,
    .add_pasid = ubdrv_process_add_pasid,
    .del_pasid = ubdrv_process_del_pasid,
    .addr_trans_cs_p2p = NULL,
    .addr_trans_l2p = ubdrv_addr_trans_local_to_peer,
    .query_addr_trans_support = ubdrv_query_addr_trans_support,
    .urma_copy = ubdrv_urma_copy,
    .register_seg = ubdrv_register_seg,
    .unregister_seg = ubdrv_unregister_seg,
    .import_seg = ubdrv_import_seg,
    .unimport_seg = ubdrv_unimport_seg,
    .get_ub_dev_id_info = ubdrv_get_dev_id_info,
    .get_non_trans_chan_capacity = ubdrv_get_non_trans_chan_capacity,
};

struct devdrv_comm_ops *get_global_ubdrv_ops(void)
{
    return &g_ubdrv_ops;
}

STATIC int ubdrv_host_module_init(void)
{
    int ret = 0;

    ret = ubdrv_load_device_init();
    if (ret != 0) {
        return -EINVAL;
    }
    ret = ubdrv_alloc_attr_info();
    if (ret != 0) {
        ubdrv_err("Alloc attr info failed. (ret=%d)\n", ret);
        goto alloc_attr_info_failed;
    }
    ret = ubdrv_mem_host_cfg_ctrl_init();
    if (ret != 0) {
        goto mem_cfg_init_failed;
    }
    ret = ubdrv_mem_decoder_ctrl_init();
    if (ret != 0) {
        goto mem_decoder_init_failed;
    }
#ifdef CFG_FEATURE_SLAVE_MODE
    ret = ub_register_driver(&ascend_ubus_driver);
    if (ret != 0) {
        ubdrv_err("Register ub driver failed. (ret=%d)\n", ret);
        goto ub_register_driver_failed;
    }
    ubdrv_init_dev_num();
#endif
    return 0;

#ifdef CFG_FEATURE_SLAVE_MODE
ub_register_driver_failed:
    ubdrv_mem_decoder_ctrl_uninit();
#endif
mem_decoder_init_failed:
    ubdrv_mem_host_cfg_ctrl_uninit();
mem_cfg_init_failed:
    ubdrv_free_attr_info();
alloc_attr_info_failed:
    ubdrv_load_device_uninit();
    return ret;
}

STATIC void ubdrv_host_module_exit(void)
{
#ifdef CFG_FEATURE_SLAVE_MODE
    ub_unregister_driver(&ascend_ubus_driver);
#endif
    ubdrv_mem_decoder_ctrl_uninit();
    ubdrv_mem_host_cfg_ctrl_uninit();
    ubdrv_free_attr_info();
    ubdrv_load_device_uninit();
    return;
}

STATIC int ub_pack_master_id_to_uda(u32 devid, struct uda_dev_para *uda_para)
{
    struct ascend_dev *asd_dev = NULL;

    asd_dev = ubdrv_get_asd_dev_by_devid(devid);
    if (asd_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", devid);
        return -ENODEV;
    }

    /* master slave mode */
    if (asd_dev->ub_dev != NULL) {
        if (asd_dev->shr_para.load_flag == 1) {
            uda_para->master_id = devid;
        } else {
            uda_para->master_id = devid - 1;
        }
        ubdrv_info("Set master_devid. (devid=%u; master_id=%u)\n", devid, uda_para->master_id);
        return 0;
    }
    uda_para->master_id = devid;
    ubdrv_info("Set master_devid. (devid=%u; master_id=%u)\n", devid, uda_para->master_id);

    return 0;
}

STATIC int ubdrv_prepare_vfe_online_msg(u32 udevid, u32 phy_dev_id, u32 ue_idx, struct ub_idev *idev,
                                        struct ascend_ub_user_data *data, struct ubdrv_jetty_exchange_data *cmd)
{
    struct ascend_ub_ctrl *ub_ctrl;

    ub_ctrl = get_global_ub_ctrl();
    struct ascend_ub_link_res *link_res = &ub_ctrl->link_res[udevid];
    u32 len = sizeof(struct ubdrv_jetty_exchange_data);
    u32 token = 0;
    int ret;

    if (ubdrv_get_token_val(phy_dev_id, &token) != 0) {
        ubdrv_err("Get token val failed.(dev_id=%u;vf_dev_id=%u)\n", phy_dev_id, udevid);
        return -EINVAL;
    }
    ubdrv_set_local_token(udevid, token, ASCEND_VALID);
    ret = ubdrv_create_admin_jetty(idev, udevid, UBDRV_DEFAULT_JETTY_ID);
    if (ret != 0) {
        ubdrv_err("Create admin jetty failed.(idev_id=%u;fe_id=%u;ret=%d)\n", idev->idev_id, idev->ue_idx, ret);
        goto set_token_invalid;
    }

    ret = ubdrv_get_send_jetty_info(&link_res->admin_jetty->send_jetty, &cmd->admin_jetty_info);
    if (ret != 0) {
        ubdrv_err("Get send jetty info failed.(idev_id=%u;fe_id=%u;ret=%d)\n", idev->idev_id, idev->ue_idx, ret);
        goto del_vf_admin_jetty;
    }
    cmd->ue_idx = ue_idx;
    data->opcode = UBDRV_VFE_ONLINE;
    data->size = len;
    data->reply_size = len;
    data->cmd = cmd;
    data->reply = cmd;
    return 0;

del_vf_admin_jetty:
    ubdrv_delete_admin_jetty(udevid);
set_token_invalid:
    ubdrv_set_local_token(udevid, 0, ASCEND_INVALID);
    return ret;
}

STATIC void ubdrv_send_vfe_offline_msg(u32 phy_dev_id, u32 ue_idx)
{
    u32 len = sizeof(struct ubdrv_jetty_exchange_data);
    struct ascend_ub_user_data user_desc = {0};
    struct ubdrv_jetty_exchange_data cmd = {0};
    int ret;

    cmd.ue_idx = ue_idx;
    user_desc.opcode = UBDRV_VFE_OFFLINE;
    user_desc.size = len;
    user_desc.reply_size = len;
    user_desc.cmd = &cmd;
    user_desc.reply = &cmd;
    ret = ubdrv_admin_send_msg(phy_dev_id, &user_desc);
    if (ret != 0) {
        ubdrv_err("Send admin msg to device fail, vfe offline fail. (phy_dev_id=%u;ret=%d)\n", phy_dev_id, ret);
    }
    return;
}

static int ubdrv_sync_soc_res(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len)
{
    struct ascend_ub_user_data user_desc = {0};
    u32 out_len;
    int ret;

    if ((target->type == SOC_REG_ADDR) || (target->type == SOC_RSV_MEM_ADDR) || (target->type == SOC_IRQ_RES)) {
        return 0;
    }

    user_desc.opcode = UBDRV_SYNC_RES_INFO;
    user_desc.size = sizeof(*target);
    user_desc.reply_size = buf_len;
    user_desc.cmd = (void *)target;
    user_desc.reply = (void *)buf;

    ret = ubdrv_admin_send_msg(udevid, &user_desc);
    if (ret != 0) {
        ubdrv_err("Send failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    out_len = *(u32 *)buf;
    if (out_len > buf_len - sizeof(u32)) {
        ubdrv_err("Outlen error. (udevid=%u; out_len=%u)\n", udevid, out_len);
        return -EFAULT;
    }

    ubdrv_info("Sync. (udevid=%u; scope=%u; type=%u; out_len=%u)\n", udevid, target->scope, target->type, out_len);

    if (out_len == 0) {
        return 0;
    }

    return soc_res_inject(udevid, target, buf + sizeof(u32), out_len, NULL);
}

STATIC int ubdrv_get_vfe_phydevid_by_udevid(u32 udevid, u32 *phy_dev_id, u32 *ue_idx)
{
    struct uda_mia_dev_para mia_para;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if ((ret != 0) || (mia_para.sub_devid >= ASCEND_UDMA_MAX_FE_NUM - 1)) {
        ubdrv_err("Get devid and ue_idx failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -EINVAL;
    }
    *phy_dev_id = mia_para.phy_devid;
    *ue_idx = mia_para.sub_devid + 1;
    return 0;
}

int ubdrv_fe_init_instance(u32 udevid)
{
    struct ubdrv_jetty_exchange_data cmd = {0};
    struct ascend_ub_user_data user_desc = {0};
    struct ub_idev *idev;
    u32 ue_idx, dev_id;
    int ret = 0;

    if (uda_is_phy_dev(udevid)) {
        return soc_res_sync_d2h(udevid, ubdrv_sync_soc_res);
    }

    ret = ubdrv_get_vfe_phydevid_by_udevid(udevid, &dev_id, &ue_idx);
    if (ret != 0) {
        ubdrv_err("Get devid and ue_idx failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -EINVAL;
    }

    idev = ubdrv_find_idev_by_udevid(udevid);
    if (idev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u;ue_idx=%u)\n", dev_id, ue_idx);
        return -EINVAL;
    }
    ret = ubdrv_init_h2d_eid_index(udevid);
    if (ret != 0) {
        return ret;
    }
    ret = ubdrv_davinci_bind_fe(idev, udevid);
    if (ret != 0) {
        goto vf_uninit_eid_index;
    }
    ret = ubdrv_prepare_vfe_online_msg(udevid, dev_id, ue_idx, idev, &user_desc, &cmd);
    if (ret != 0) {
        ubdrv_err("Prepare admin jetty failed. (dev_id=%u;ue_idx=%u;ret=%d)\n", dev_id, ue_idx, ret);
        goto unbind_vfe;
    }

    ubdrv_print_exchange_data(&cmd);
    ret = ubdrv_admin_send_msg(dev_id, &user_desc);
    if (ret != 0) {
        ubdrv_err("Admin msg send failed. (dev_id=%u;ue_idx=%u;ret=%d)\n", dev_id, ue_idx, ret);
        goto fe_online_fail;
    }
    ubdrv_print_exchange_data(&cmd);

    ret = ubdrv_add_msg_device(udevid, 0, idev->idev_id, idev->ue_idx, &cmd.admin_jetty_info);
    if (ret != 0) {
        ubdrv_err("Add msg dev failed. (dev_id=%u;ue_idx=%u;ret=%d)\n", dev_id, ue_idx, ret);
        goto add_msg_fail;
    }

    ret = soc_res_sync_d2h(udevid, ubdrv_sync_soc_res);
    if (ret != 0) {
        ubdrv_err("Sync fail. (dev_id=%u)\n", udevid);
        goto sco_sync_fail;
    }
    devdrv_ub_set_device_boot_status(udevid, DSMI_BOOT_STATUS_FINISH);
    ubdrv_set_device_status(udevid, UBDRV_DEVICE_ONLINE);
    return 0;

sco_sync_fail:
    ubdrv_del_msg_device(udevid, UBDRV_DEVICE_UNINIT);
add_msg_fail:
    ubdrv_send_vfe_offline_msg(dev_id, ue_idx);
fe_online_fail:
    ubdrv_delete_admin_jetty(udevid);
    ubdrv_set_local_token(udevid, 0, ASCEND_INVALID);
unbind_vfe:
    ubdrv_davinci_unbind_fe(idev, udevid);
vf_uninit_eid_index:
    ubdrv_uninit_h2d_eid_index(udevid);
    return ret;
}

int ubdrv_fe_uninit_instance(u32 udevid)
{
    struct ascend_ub_msg_dev *msg_dev;
    u32 ue_idx, phy_dev_id;
    struct ub_idev *idev;
    int ret;

    if (uda_is_phy_dev(udevid)) {
        return 0;
    }
    devdrv_ub_set_device_boot_status(udevid, DSMI_BOOT_STATUS_UNINIT);
    ret = ubdrv_get_vfe_phydevid_by_udevid(udevid, &phy_dev_id, &ue_idx);
    if (ret != 0) {
        ubdrv_err("Get devid and ue_idx failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -EINVAL;
    }
    // send admin msg to device offline
    ubdrv_send_vfe_offline_msg(phy_dev_id, ue_idx);
    msg_dev = ubdrv_get_msg_dev_by_devid(udevid);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }
    idev = msg_dev->idev;
    ubdrv_set_device_status(udevid, UBDRV_DEVICE_DEAD);
    ubdrv_del_msg_device(udevid, UBDRV_DEVICE_UNINIT);
    ubdrv_delete_admin_jetty(udevid);
    ubdrv_set_local_token(udevid, 0, ASCEND_INVALID);
    ubdrv_davinci_unbind_fe(idev, udevid);
    ubdrv_uninit_h2d_eid_index(udevid);
    return 0;
}

void ubdrv_set_heart_beat_lost(u32 udevid)
{
    ubdrv_set_device_status(udevid, UBDRV_DEVICE_DEAD);
    ubdrv_err("Device heart beat lost, stop all msg send. (udevid=%u)\n", udevid);
}

STATIC int ubdrv_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= ASCEND_UB_DEV_MAX_NUM && udevid != uda_get_host_id()) {
        ubdrv_err("Invalid para. (udevid=%u;action=%d)\n", udevid, action);
        return -EINVAL;
    }

    if (action == UDA_UPDATE_P2P_ADDR) {
        ka_base_atomic_inc(&g_decoder_info.query_stats.uda_trigger_cnt);
        ret = ubdrv_query_host_mem_decoder_info(udevid, UBDRV_WARN_LEVEL); // D2H is enable, return 0
        (void)ubdrv_query_device_mem_decoder_info(udevid, UBDRV_WARN_LEVEL);
    }

    if (action == UDA_COMMUNICATION_LOST) {
        ubdrv_set_heart_beat_lost(udevid);
    }

    ubdrv_info("notifier action. (udevid=%u;action=%d;ret=%d)\n", udevid, action, ret);
    return ret;
}

STATIC int ubdrv_near_entity_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    if (udevid >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid para. (udevid=%u;action=%d)\n", udevid, action);
        return -EINVAL;
    }
    if (action == UDA_INIT) {
        ubdrv_set_msg_retry_cnt(udevid, ASCEND_MSG_NORMAL_RETRY_CNT);
    }
    ubdrv_info("Last notifier action. (udevid=%u;action=%d)\n", udevid, action);
    return 0;
}

STATIC int ubdrv_host_register_uda_notifier(void)
{
    struct uda_dev_type near_real_type;
    int ret;

    uda_davinci_near_real_entity_type_pack(&near_real_type);
    ret = uda_notifier_register(ASCEND_UB_MIA_NOTIFIER, &near_real_type, UDA_PRI0, ubdrv_mia_dev_notifier_func);
    if (ret != 0) {
        ubdrv_err("Register near virtual entity type uda failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = uda_notifier_register(ASCEND_UB_REAL_NOTIFIER, &near_real_type, UDA_PRI3, ubdrv_host_notifier_func);
    if (ret != 0) {
        ubdrv_err("Register local real entity type uda failed. (ret=%d)\n", ret);
        goto unregister_mia;
    }

    ret = uda_notifier_register(ASCEND_UB_DAVINCI_NOTIFIER, &near_real_type, UDA_PRI5,
                                ubdrv_near_entity_dev_notifier_func);
    if (ret != 0) {
        ubdrv_err("Register host last real entity type uda failed. (ret=%d)\n", ret);
        goto unregister_host_func;
    }
    return 0;

unregister_host_func:
    (void)uda_notifier_unregister(ASCEND_UB_REAL_NOTIFIER, &near_real_type);
unregister_mia:
    (void)uda_notifier_unregister(ASCEND_UB_MIA_NOTIFIER, &near_real_type);
    return ret;
}

int ubdrv_get_token_val(u32 devid, u32 *token_val)
{
    int ret;

    struct ascend_ub_user_data user_desc = {0};
    u32 len = sizeof(u32);
    u32 tmp_token_val;

    if (devid >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get msg_dev failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (token_val == NULL) {
        ubdrv_err("token_val is NULL. (dev_id=%d)\n", devid);
        return -EINVAL;
    }

    user_desc.opcode = UBDRV_GET_TOKEN_VAL;
    user_desc.size = len;
    user_desc.reply_size = len;
    user_desc.cmd = &tmp_token_val;
    user_desc.reply = &tmp_token_val;
    ret = ubdrv_admin_send_msg(devid, &user_desc);
    if (ret != 0) {
        ubdrv_err("Send admin msg to device fail, can not get token_val. (phy_dev_id=%u;ret=%d)\n", devid, ret);
        return ret;
    }

    *token_val = tmp_token_val;
    return 0;
}

void ubdrv_remove_davinci_dev_proc(u32 dev_id)
{
    return;
}

int ub_check_pack_master_id_to_uda(struct ascend_ub_msg_dev *msg_dev, struct uda_dev_para *uda_para, u32 dev_id)
{
    int ret;

    ret = ub_pack_master_id_to_uda(msg_dev->dev_id, uda_para);
    if (ret != 0) {
#ifndef EMU_ST
        ubdrv_err("Pack master id fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
#endif
    }
    return ret;
}

int ubdrv_wait_add_davinci_dev(void)
{
    return 0;
}

void uda_dev_type_pack_proc(struct uda_dev_type *uda_type, u32 dev_type)
{
    uda_dev_type_pack(uda_type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, dev_type);
}

int ubdrv_add_msg_device_proc(struct ascend_ub_msg_dev *msg_dev, u32 dev_id)
{
    int ret;

    ret = ubdrv_alloc_common_msg_queue(msg_dev);
    if (ret != 0) {
        ubdrv_err("ubdrv_alloc_common_msg_queue failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return ret;
    }
    ret = ubdrv_alloc_urma_chan(dev_id);
    if (ret != 0) {
        ubdrv_free_common_msg_queue(dev_id);
        return ret;
    }
    return 0;
}

void ubdrv_exit_release_msg_chan_proc(u32 dev_id)
{
    ubdrv_free_urma_chan(dev_id);
    (void)ubdrv_free_common_msg_queue(dev_id);
}

int ubdrv_register_uda_notifier(void)
{
    return ubdrv_host_register_uda_notifier();
}

void ubdrv_unregister_uda_notifier(void)
{
    struct uda_dev_type type;

    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(ASCEND_UB_DAVINCI_NOTIFIER, &type);
    (void)uda_notifier_unregister(ASCEND_UB_REAL_NOTIFIER, &type);
    (void)uda_notifier_unregister(ASCEND_UB_MIA_NOTIFIER, &type);
}

int ubdrv_get_ub_pcie_sel(void)
{
    return UBDRV_UB_SEL;
}

int ubdrv_module_init_for_pcie(void)
{
    ubdrv_info("Sel non-ub mode, insmod ub driver return.\n");
    return 0;
}

void devdrv_set_communication_ops_status_proc(u32 type, u32 status, u32 dev_id)
{
    return;
}

int ubdrv_module_init_proc(void)
{
    return ubdrv_host_module_init();
}

void ubdrv_module_uninit_for_pcie(void)
{
    return;
}

void ubdrv_module_exit_proc(void)
{
    ubdrv_host_module_exit();
}

int ubdrv_set_chip_type_from_image_load(u32 device_id)
{
    if (device_id == UBDRV_CLOUD_V4_DEVICE_ID) {
        g_ubdrv_chip_type = HISI_CLOUD_V4;
    } else if (device_id == UBDRV_CLOUD_V5_DEVICE_ID) {
        g_ubdrv_chip_type = HISI_CLOUD_V5;
    } else if (device_id == UBDRV_MINI_V4_DEVICE_ID) {
        g_ubdrv_chip_type = HISI_MINI_V4;
    } else {
        return -EINVAL;
    }

    return 0;
}

void ubdrv_set_chip_type_from_exchange_data(u32 chip_type)
{
    g_ubdrv_chip_type = chip_type;
}

u32 ubdrv_get_chip_type(void)
{
    return g_ubdrv_chip_type;
}

void ubdrv_admin_recv_prepare_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc, u32 seg_id)
{
    return;
}