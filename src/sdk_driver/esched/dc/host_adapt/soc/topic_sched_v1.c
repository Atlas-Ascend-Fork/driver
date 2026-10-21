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
#ifndef EVENT_SCHED_UT

#include "tsdrv_interface.h"
#include "comm_kernel_interface.h"

#include "ascend_hal_define.h"
#include "esched_kernel_interface.h"

#include "esched_adapt_platform.h"

#include "esched.h"
#include "esched_host_msg.h"
#include "esched_drv_adapt.h"
#include "topic_sched_common.h"
#include "topic_sched_v1.h"

/* HOST reg offset */
#define SCHED_HOST_TOPIC_ADDR_OFFSET (0x400000)

/* wait/get status and report reg */
#define STARS_TOPIC_VF_HOST_AICPU_STATUS_REPORT_NS(cpu_id, vf_id) (0x000000 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_AICPU_WAIT_TOPIC_NS(cpu_id, vf_id) (0x000004 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_AICPU_INT_EN_NS(cpu_id, vf_id) (0x000008 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_AICPU_ERRCODE_REPORT_NS(cpu_id, vf_id) (0x00000C + (cpu_id) * 0x20 + (vf_id) * 0x10000)

#define STARS_TOPIC_VF_HOST_CTRLCPU_STATUS_REPORT_NS(cpu_id, vf_id) (0x000010 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_CTRLCPU_ERRCODE_REPORT_NS(cpu_id, vf_id) (0x00001C + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_CTRLCPU_WAIT_TOPIC_NS(cpu_id, vf_id) (0x000014 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_CTRLCPU_INT_EN_NS(cpu_id, vf_id) (0x000018 + (cpu_id) * 0x20 + (vf_id) * 0x10000)

#define STARS_TOPIC_HOST_AICPU_INT_STS0_NS(index, vf_id) (0x000A00 + (vf_id) * 0x10000 + (index) * 0x20)
#define STARS_TOPIC_HOST_AICPU_INT_CLR0_NS(index, vf_id) (0x000A08 + (vf_id) * 0x10000 + (index) * 0x20)
#define STARS_TOPIC_HOST_AICPU_INT_MASK0_NS(index, vf_id) (0x000A0C + (vf_id) * 0x10000 + (index) * 0x20)
#define STARS_TOPIC_HOST_AICPU_INT_STS_NS(vf_id) (0x000A40 + (vf_id) * 0x10000)
#define STARS_TOPIC_HOST_AICPU_INT_CLR_NS(vf_id) (0x000A48 + (vf_id) * 0x10000)

#define STARS_TOPIC_HOST_CTRLCPU_INT_STS_NS(vf_id) (0x000A60 + (vf_id) * 0x10000)
#define STARS_TOPIC_HOST_CTRLCPU_INT_CLR_NS(vf_id) (0x000A68 + (vf_id) * 0x10000)
#define STARS_TOPIC_HOST_CTRLCPU_INT_MASK_NS(vf_id) (0x000A6C + (vf_id) * 0x10000)

/* vf_id: range from 0 to 15 */
STATIC void topic_sched_host_ccpu_status_report(void __ka_mm_iomem *io_base, u32 mb_id, u32 vf_id, u32 status)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_STATUS_REPORT_NS(mb_id, vf_id_tmp), status);
}

STATIC void topic_sched_host_ccpu_errcode_report(void __ka_mm_iomem *io_base, u32 mb_id, u32 vf_id, u32 error_code)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_ERRCODE_REPORT_NS(mb_id, vf_id_tmp), error_code);
}

void topic_sched_host_aicpu_intr_mask_set_v1(void __ka_mm_iomem *io_base, u32 mask_index, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_AICPU_INT_MASK0_NS(mask_index, vf_id_tmp), val);
}

void topic_sched_host_ctrlcpu_intr_mask_set_v1(void __ka_mm_iomem *io_base, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_CTRLCPU_INT_MASK_NS(vf_id_tmp), val);
}

bool topic_sched_host_ccpu_is_mb_valid_v1(const void __ka_mm_iomem *io_base, u32 mb_id, u32 vf_id)
{
    u32 val;

    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_WAIT_TOPIC_NS(mb_id, vf_id_tmp), &val);

    return ((val & 0x1) == 1);
}

STATIC void topic_sched_host_aicpu_status_report(void __ka_mm_iomem *io_base, u32 mb_id, u32 vf_id, u32 status)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_AICPU_STATUS_REPORT_NS(mb_id, vf_id_tmp), status);
}

STATIC void topic_sched_host_aicpu_errcode_report(void __ka_mm_iomem *io_base, u32 mb_id, u32 vf_id, u32 error_code)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_AICPU_ERRCODE_REPORT_NS(mb_id, vf_id_tmp), error_code);
}

bool topic_sched_host_aicpu_is_mb_valid_v1(const void __ka_mm_iomem *io_base, u32 mb_id, u32 vf_id)
{
    u32 val;

    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_VF_HOST_AICPU_WAIT_TOPIC_NS(mb_id, vf_id_tmp), &val);

    return ((val & 0x1) == 1);
}

void topic_sched_host_aicpu_intr_clr_v1(void __ka_mm_iomem *io_base, u32 intr_index, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_AICPU_INT_CLR0_NS(intr_index, vf_id_tmp), val);
}

void topic_sched_host_ccpu_intr_clr_v1(void __ka_mm_iomem *io_base, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_CTRLCPU_INT_CLR_NS(vf_id_tmp), val);
}

void topic_sched_host_aicpu_intr_enable_v1(void __ka_mm_iomem *io_base, u32 cpu_index, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_AICPU_INT_EN_NS(cpu_index, vf_id_tmp), 0x1);
}

void topic_sched_host_ctrlcpu_intr_enable_v1(void __ka_mm_iomem *io_base, u32 cpu_index, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_INT_EN_NS(cpu_index, vf_id_tmp), 0x1);
}

void topic_sched_host_aicpu_int_all_status_v1(const void __ka_mm_iomem *io_base, u32 *val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_HOST_AICPU_INT_STS_NS(vf_id_tmp), val);
}

void topic_sched_host_aicpu_intr_all_clr_v1(void __ka_mm_iomem *io_base, u32 val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_AICPU_INT_CLR_NS(vf_id_tmp), val);
}

void topic_sched_host_aicpu_int_status_v1(const void __ka_mm_iomem *io_base, u32 intr_index, u32 *val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_HOST_AICPU_INT_STS0_NS(intr_index, vf_id_tmp), val);
}

void topic_sched_host_ccpu_int_status_v1(const void __ka_mm_iomem *io_base, u32 *val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_HOST_CTRLCPU_INT_STS_NS(vf_id_tmp), val);
}

static void esched_drv_status_report(struct topic_data_chan *topic_chan, u32 vf_id, u32 status)
{
#ifdef CFG_PLATFORM_ESL
    status = 1; /* ESL not surport abnormal report sched, The ESL will suspend. */
#endif

    /* For the CDQM event, the topic requires that no exception be replied. */
    if (topic_chan->wait_mb->topic_id == EVENT_CDQ_MSG) {
        status = 1;
    }

    if (topic_chan->mb_type == ACPU_HOST) {
        topic_sched_host_aicpu_status_report(topic_chan->hard_res->io_base, topic_chan->mb_id, vf_id, status);
    } else {
        topic_sched_host_ccpu_status_report(topic_chan->hard_res->io_base, topic_chan->mb_id, vf_id, status);
    }
}

static void esched_drv_errcode_report(struct topic_data_chan *topic_chan, u32 vf_id, u32 error_code)
{
    /* For the CDQM event, the topic requires that no exception be replied. */
    if (topic_chan->wait_mb->topic_id == EVENT_CDQ_MSG) {
        error_code = 0;
    }

    if (topic_chan->mb_type == ACPU_HOST) {
        topic_sched_host_aicpu_errcode_report(topic_chan->hard_res->io_base, topic_chan->mb_id, vf_id, error_code);
    } else {
        topic_sched_host_ccpu_errcode_report(topic_chan->hard_res->io_base, topic_chan->mb_id, vf_id, error_code);
    }
}

void esched_drv_cpu_report_v1(struct topic_data_chan *topic_chan, u32 vf_id, u32 error_code, u32 status)
{
    esched_drv_errcode_report(topic_chan, vf_id, error_code);
    esched_drv_status_report(topic_chan, vf_id, status);
    if (status == TOPIC_FINISH_STATUS_WARNING) { // topic constraint
#ifndef EMU_ST
        esched_drv_status_report(topic_chan, vf_id, TOPIC_FINISH_STATUS_NORMAL);
#endif
    }
}

int esched_drv_host_map_addr_v1(u32 dev_id, struct sched_hard_res *res)
{
    if (!esched_adapt_is_soc_cloud_v4(dev_id)) {
        int ret;
        u64 reg_base;
        size_t size;
        u64 rsv_mem_pa;

        /* topic sched base */
        ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_BASE, 0, &reg_base, &size);
        if (ret != 0) {
            sched_err("Failed to invoke the devdrv_get_addr_info. "
                      "(dev_id=%u; type=%d)\n",
                      dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_BASE);
            return ret;
        }

        /* rsv_mem_pa are different between PF and VF */
        if (uda_is_pf_dev(dev_id)) {
            reg_base += SCHED_HOST_TOPIC_ADDR_OFFSET;
        }

        res->io_base = ka_mm_ioremap(reg_base, size);
        if (res->io_base == NULL) {
            sched_err("Failed to invoke the ioremap. (dev_id=%u; size=0x%x)\n", res->dev_id, (u32)size);
            return -ENOMEM;
        }

        res->int_io_base = NULL;

        ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_RES_MEM_BASE, 0, &rsv_mem_pa, &size);
        if (ret != 0) {
            sched_err("Failed to invoke the devdrv_get_addr_info. (dev_id=%u; type=%d; size=0x%x; ret=%d)\n", dev_id,
                      DEVDRV_ADDR_STARS_TOPIC_SCHED_RES_MEM_BASE, (u32)size, ret);
            goto iounmap_io_base;
        }

        res->rsv_mem_pa = rsv_mem_pa;
        res->rsv_mem_va = ka_mm_ioremap(res->rsv_mem_pa, size);
        if (res->rsv_mem_va == NULL) {
            sched_err("Failed to invoke the ioremap. (dev_id=%u; size=0x%x)\n", res->dev_id, (u32)size);
            ret = -ENOMEM;
            goto iounmap_io_base;
        }

        sched_info("Show details. (dev_id=%u; io_base=%pK; int_io_base=%pK; rsv_mem_pa=0x%pK; size=0x%x)\n", dev_id,
                   res->io_base, res->int_io_base, (void *)res->rsv_mem_pa, (u32)size);

        return 0;

    iounmap_io_base:
        ka_mm_iounmap(res->io_base);
        res->io_base = NULL;
        return ret;
    } else {
        return 0;
    }
}

void esched_host_iounmap_v1(struct sched_hard_res *res)
{
    if (!esched_adapt_is_soc_cloud_v4(res->dev_id)) {
        if (res->io_base != NULL) {
            ka_mm_iounmap(res->io_base);
            res->io_base = NULL;
        }

        if (res->int_io_base != NULL) {
            ka_mm_iounmap(res->int_io_base);
            res->int_io_base = NULL;
        }

        if (res->rsv_mem_va != NULL) {
            ka_mm_iounmap(res->rsv_mem_va);
            res->rsv_mem_va = NULL;
        }
    }
}
#else
void esched_drv_host_topic_sched_v1(void)
{}
#endif
