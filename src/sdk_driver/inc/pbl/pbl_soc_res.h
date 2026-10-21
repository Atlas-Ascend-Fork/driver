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

#ifndef PBL_SOC_RES_H
#define PBL_SOC_RES_H

#include "ka_base_pub.h"
#include "ka_common_pub.h"

#define SOC_RESMNG_MAX_NAME_LEN 40
#define SOC_MAX_MIA_GROUP_NUM 8
#define SOC_RESMNG_MAX_ATTR_SIZE 256

#define SOC_HOST_PHY_MACH_FLAG 0x5a6b7c8d /* host physical mathine flag */
#define SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM 16

#define SOC_TOPOLOGY_HCCS 0
#define SOC_TOPOLOGY_PIX 1
#define SOC_TOPOLOGY_PIB 2
#define SOC_TOPOLOGY_PHB 3
#define SOC_TOPOLOGY_SYS 4
#define SOC_TOPOLOGY_SIO 5
#define SOC_TOPOLOGY_HCCS_SW 6
#define SOC_TOPOLOGY_UB 7

struct soc_pcie_info {
    u32 hccs_status;
    u32 hccs_group_id[SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM];
    u32 host_flag;
    ka_pci_dev_t *pdev;
};

enum soc_mia_res_type {
    MIA_AC_AIV = 0,
    MIA_AC_AIC,
    MIA_AC_C_CORE,
    MIA_AC_DSA,
    MIA_AC_FFTS,
    MIA_AC_SDMA,
    MIA_AC_PCIE_DMA,

    /* ts sub sys start. */
    MIA_STARS_STREAM,
    MIA_STARS_EVENT,
    MIA_STARS_NOTIFY,
    MIA_STARS_MODEL,
    MIA_STARS_CMO,
    MIA_STARS_CDQ,
    MIA_STARS_ACSQ,
    MIA_STARS_RTSQ,
    MIA_STARS_RTCQ,
    MIA_STARS_SWSQ,
    MIA_STARS_SWCQ,
    MIA_STARS_UBDMA,
    MIA_STARS_FUSION,
    MIA_MAINT_SQ,
    MIA_MAINT_CQ,
    MIA_TASK_SCHED_CQ,
    MIA_STARS_TOPIC_ACPU_SLOT,
    MIA_STARS_TOPIC_CCPU_SLOT,
    MIA_STARS_CNT_NOTIFY,
    MIA_STARS_H2D_JETTY,
    MIA_STARS_D2D_IN_BOARD_JETTY,
    MIA_STARS_D2D_CROSS_BOARD_JETTY,
    /* ts sub sys end. */

    MIA_MEM_NUMA,
    MIA_SYS_MEM,

    MIA_CPU_HOST_CCPU,
    MIA_CPU_HOST_ACPU,
    MIA_CPU_DEV_ACPU,

    MIA_DVPP_JPEGD,
    MIA_DVPP_JPEGE,
    MIA_DVPP_VPC,
    MIA_DVPP_VDEC,
    MIA_DVPP_PNGD,
    MIA_DVPP_VENC,
    MIA_DVPP,

    MIA_MEM_HBM,
    MIA_MEM_SIO,
    MIA_MEM_D2B_SIO,
    MIA_MEM_D2U_SIO,
    MIA_MEM_D2D_SIO,

    MIA_SYS_HCCS,
    MIA_SYS_MATA,
    MIA_SYS_l2,
    MIA_SYS_GPU,
    MIA_SYS_ISP,
    MIA_SYS_DMC,

    MIA_CPU_DEV_CCPU,
    MIA_CPU_DEV_DCPU,
    MIA_CPU_DEV_CPU,
    MIA_CPU_DEV_COMCPU,

    MIA_SYS_STARS,
    MIA_SYS_L3D,
    MIA_SYS_HVCV,
    MIA_MAX_RES_TYPE
};

static const char *mia_res_name[MIA_MAX_RES_TYPE] = {
    [MIA_AC_AIV] = "mia_ac_aiv",
    [MIA_AC_AIC] = "mia_ac_aic",
    [MIA_AC_C_CORE] = "mia_ac_c_core",
    [MIA_AC_DSA] = "mia_ac_da",
    [MIA_AC_FFTS] = "mia_ac_ffts",
    [MIA_AC_SDMA] = "mia_ac_sdma",
    [MIA_AC_PCIE_DMA] = "mia_ac_pcie_dma",
    [MIA_STARS_STREAM] = "mia_stars_stream",
    [MIA_STARS_EVENT] = "mia_stars_event",
    [MIA_STARS_NOTIFY] = "mia_stars_notify",
    [MIA_STARS_MODEL] = "mia_stars_model",
    [MIA_STARS_CMO] = "mia_stars_cmo",
    [MIA_STARS_CDQ] = "mia_stars_cdq",
    [MIA_STARS_ACSQ] = "mia_stars_acsq",
    [MIA_STARS_RTSQ] = "mia_stars_rtsq",
    [MIA_STARS_RTCQ] = "mia_stars_rtcq",
    [MIA_STARS_SWSQ] = "mia_stars_swsq",
    [MIA_STARS_SWCQ] = "mia_stars_swcq",
    [MIA_STARS_UBDMA] = "mia_stars_ubdma",
    [MIA_STARS_FUSION] = "mia_stars_fusion",
    [MIA_MAINT_SQ] = "mia_maint_sq",
    [MIA_MAINT_CQ] = "mia_maint_cq",
    [MIA_TASK_SCHED_CQ] = "mia_task_sched_cq",
    [MIA_STARS_TOPIC_ACPU_SLOT] = "mia_stars_topic_acpu_slot",
    [MIA_STARS_TOPIC_CCPU_SLOT] = "mia_stars_topic_ccpu_slot",
    [MIA_STARS_CNT_NOTIFY] = "mia_stars_cnt_notify",
    [MIA_STARS_H2D_JETTY] = "mia_stars_h2d_jetty",
    [MIA_STARS_D2D_IN_BOARD_JETTY] = "mia_stars_d2d_in_board_jetty",
    [MIA_STARS_D2D_CROSS_BOARD_JETTY] = "mia_stars_d2d_cross_board_jetty",
    [MIA_MEM_NUMA] = "mia_mem_numa",
    [MIA_SYS_MEM] = "mia_sys_mem",
    [MIA_CPU_HOST_CCPU] = "mia_cpu_host_ccpu",
    [MIA_CPU_HOST_ACPU] = "mia_cpu_host_acpu",
    [MIA_CPU_DEV_ACPU] = "mia_cpu_dev_acpu",
    [MIA_DVPP_JPEGD] = "mia_dvpp_jpegd",
    [MIA_DVPP_JPEGE] = "mia_dvpp_jpege",
    [MIA_DVPP_VPC] = "mia_dvpp_vpc",
    [MIA_DVPP_VDEC] = "mia_dvpp_vdec",
    [MIA_DVPP_PNGD] = "mia_dvpp_pngd",
    [MIA_DVPP_VENC] = "mia_dvpp_venc",
    [MIA_DVPP] = "mia_dvpp",
    [MIA_MEM_HBM] = "mia_mem_hbm",
    [MIA_MEM_SIO] = "mia_mem_sio",
    [MIA_MEM_D2B_SIO] = "mia_mem_d2b_sio",
    [MIA_MEM_D2U_SIO] = "mia_mem_d2u_sio",
    [MIA_MEM_D2D_SIO] = "mia_mem_d2d_sio",
    [MIA_SYS_HCCS] = "mia_sys_hccs",
    [MIA_SYS_MATA] = "mia_sys_mata",
    [MIA_SYS_l2] = "mia_sys_l2",
    [MIA_SYS_GPU] = "mia_sys_gpu",
    [MIA_SYS_ISP] = "mia_sys_isp",
    [MIA_SYS_DMC] = "mia_sys_dmc",
    [MIA_CPU_DEV_CCPU] = "mia_cpu_dev_ccpu",
    [MIA_CPU_DEV_DCPU] = "mia_cpu_dev_dcpu",
    [MIA_CPU_DEV_CPU] = "mia_cpu_dev_cpu",
    [MIA_CPU_DEV_COMCPU] = "mia_cpu_dev_comcpu",
    [MIA_SYS_STARS] = "mia_sys_stars",
    [MIA_SYS_L3D] = "mia_sys_l3d",
    [MIA_SYS_HVCV] = "mia_sys_hvcv",
};

static inline u32 soc_resmng_get_mia_res_type_by_name(const char *name)
{
    u32 res_type;

    for (res_type = MIA_AC_AIV; res_type < MIA_MAX_RES_TYPE; res_type++) {
        if (ka_base_strcmp(mia_res_name[res_type], name) == 0) {
            break;
        }
    }

    return res_type;
}

/* global irq type. u should use prefix to distinguish dev/ts irq. */
enum soc_dev_irq_type {
    DEV_IRQ_TYPE_MAX = 0
};

enum soc_ts_irq_type {
    TS_MAILBOX_ACK_IRQ = 0,
    TS_FUNC_CQ_IRQ,
    TS_DISP_NFE_IRQ,
    TS_CQ_UPDATE_IRQ,
    TS_CQ_UPDATE_BIND_THREAD_IRQ,
    TS_STARS_CDQM_IRQ,
    TS_STARS_TOPIC_IRQ,
    TS_SQ_SEND_TRIGGER_IRQ,
    TS_PROF_AICORE_IRQ,
    TS_PROF_HWTS_LOG_IRQ,
    TS_TSCPU_STARS_IRQ,
    TS_TSCPU_FFTS_IRQ,
    TS_STARS_SRAM_IRQ,
    TS_TSCPU_HB_ACK_IRQ,
    TS_STARS_AST_IRQ,
    TS_IRQ_TYPE_MAX
};

static const char *ts_irq_name[TS_IRQ_TYPE_MAX] = {
    [TS_MAILBOX_ACK_IRQ] = "mbox_ack_irq",
    [TS_FUNC_CQ_IRQ] = "func_cq_irq",
    [TS_DISP_NFE_IRQ] = "disp_nfe_irq",
    [TS_CQ_UPDATE_IRQ] = "cq_update_irq",
    [TS_CQ_UPDATE_BIND_THREAD_IRQ] = "cq_update_bind_thread_irq",
    [TS_STARS_CDQM_IRQ] = "stars_cdqm_irq",
    [TS_STARS_TOPIC_IRQ] = "stars_topic_irq",
    [TS_SQ_SEND_TRIGGER_IRQ] = "sq_send_trigger_irq",
    [TS_PROF_AICORE_IRQ] = "prof_aicore_irq",
    [TS_PROF_HWTS_LOG_IRQ] = "prof_hwts_log_irq",
    [TS_TSCPU_STARS_IRQ] = "tscpu_stars_irq",
    [TS_TSCPU_FFTS_IRQ] = "tscpu_ffts_irq",
    [TS_STARS_SRAM_IRQ] = "stars_sram_irq",
    [TS_TSCPU_HB_ACK_IRQ] = "tscpu_hb_ack_irq",
    [TS_STARS_AST_IRQ] = "stars_ast_irq",
};

static inline u32 soc_resmng_get_dev_irq_type_by_name(const char *name)
{
    return DEV_IRQ_TYPE_MAX;
}

static inline u32 soc_resmng_get_ts_irq_type_by_name(const char *name)
{
    u32 ts_irq_type;

    for (ts_irq_type = 0; ts_irq_type < TS_IRQ_TYPE_MAX; ts_irq_type++) {
        if (ka_base_strcmp(ts_irq_name[ts_irq_type], name) == 0) {
            break;
        }
    }

    return ts_irq_type;
}

static inline const char *soc_resmng_get_ts_irq_name_by_type(enum soc_ts_irq_type irq_type)
{
    if ((irq_type >= TS_IRQ_TYPE_MAX) || (irq_type < 0)) {
        return NULL;
    }
    return ts_irq_name[irq_type];
}

struct soc_mia_res_info {
    u64 bitmap;
    u32 unit_per_bit;
};

struct soc_mia_res_info_ex {
    u64 bitmap;
    u32 unit_per_bit;
    u32 start;
    u32 total_num;
    u32 freq;
    u32 min_num;
};

struct soc_reg_base_info {
    phys_addr_t io_base;
    size_t io_base_size;
};

struct soc_rsv_mem_info {
    phys_addr_t rsv_mem;
    size_t rsv_mem_size;
};

// interface
enum soc_sub_type {
    TS_SUBSYS = 0,
    MAX_SOC_SUBSYS_TYPE,
};

struct res_inst_info {
    u32 devid;
    enum soc_sub_type sub_type;
    u32 subid;
};

#define SOC_MAX_MIA_GROUP_NUM 8

#define MIA_GROUP_VALID 1
#define MIA_GROUP_INVALID 0
#define MAX_DIE_NUM_PER_DEV 2
struct soc_mia_grp_info {
    u8 valid;
    u32 vfid;
    u32 pool_id;
    u32 poolid_max;

    struct soc_mia_res_info_ex aic_info[MAX_DIE_NUM_PER_DEV];
    struct soc_mia_res_info_ex aiv_info[MAX_DIE_NUM_PER_DEV];
    struct soc_mia_res_info_ex rtsq_info;
    struct soc_mia_res_info_ex notify_info;
};

struct soc_vnic_info {
    u8 valid;
    u32 vnic_ip;
};

static inline enum soc_sub_type soc_resmng_subsys_type(enum soc_mia_res_type res_type)
{
    if (res_type >= MIA_STARS_STREAM && res_type <= MIA_STARS_CNT_NOTIFY) {
        return TS_SUBSYS;
    }
    return MAX_SOC_SUBSYS_TYPE;
}

static inline void soc_resmng_inst_pack(struct res_inst_info *inst, u32 devid, enum soc_sub_type sub_type, u32 subid)
{
    inst->devid = devid;
    inst->sub_type = sub_type;
    inst->subid = subid;
}

/* for subsys */
int soc_resmng_set_rsv_mem(struct res_inst_info *inst, const char *name, struct soc_rsv_mem_info *rsv_mem);
int soc_resmng_get_rsv_mem(struct res_inst_info *inst, const char *name, struct soc_rsv_mem_info *rsv_mem);

int soc_resmng_set_reg_base(struct res_inst_info *inst, const char *name, struct soc_reg_base_info *io_base);
int soc_resmng_get_reg_base(struct res_inst_info *inst, const char *name, struct soc_reg_base_info *io_base);

int soc_resmng_set_irq_num(struct res_inst_info *inst, u32 irq_type, u32 irq_num);
int soc_resmng_get_irq_num(struct res_inst_info *inst, u32 irq_type, u32 *irq_num);
int soc_resmng_set_irq_by_index(struct res_inst_info *inst, u32 irq_type, u32 index, u32 irq);
int soc_resmng_get_irq_by_index(struct res_inst_info *inst, u32 irq_type, u32 index, u32 *irq);
int soc_resmng_set_hwirq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 hwirq);
int soc_resmng_get_hwirq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 *hwirq);
int soc_resmng_set_tscpu_to_taishan_irq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 tscpu_to_taishan_irq);
int soc_resmng_get_tscpu_to_taishan_irq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 *tscpu_to_taishan_irq);

int soc_resmng_set_key_value(struct res_inst_info *inst, const char *name, u64 value);
int soc_resmng_get_key_value(struct res_inst_info *inst, const char *name, u64 *value);

int soc_resmng_set_ts_status(struct res_inst_info *inst, u32 status);
int soc_resmng_get_ts_status(struct res_inst_info *inst, u32 *status);

int soc_resmng_set_hccs_link_status_and_group_id(u32 devid, u32 hccs_status, u32 *hccs_group_id, u32 group_id_num);
int soc_resmng_get_hccs_link_status_and_group_id(u32 devid, u32 *hccs_status, u32 *hccs_group_id, u32 group_id_num);

int soc_resmng_set_host_phy_mach_flag(u32 devid, u32 host_flag);
int soc_resmng_get_host_phy_mach_flag(u32 devid, u32 *host_flag);

int soc_resmng_set_pdev_by_devid(u32 devid, struct pci_dev *pdev);
int soc_resmng_get_pdev_by_devid(u32 devid, struct pci_dev **pdev);

int soc_resmng_get_dev_topology(u32 devid, u32 peer_devid, int *topo_type);
int soc_resmng_get_topology_by_host_flag(u32 devid, u32 peer_devid, int *topo_type);

int soc_resmng_set_mia_res(struct res_inst_info *inst, enum soc_mia_res_type type, u64 bitmap, u32 unit_per_bit);
int soc_resmng_get_mia_res(struct res_inst_info *inst, enum soc_mia_res_type type, u64 *bitmap, u32 *unit_per_bit);

int soc_resmng_set_mia_res_ex(struct res_inst_info *inst, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info);
int soc_resmng_get_mia_res_ex(struct res_inst_info *inst, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info);

int soc_resmng_subsys_set_num(u32 devid, enum soc_sub_type type, u32 subnum);
int soc_resmng_subsys_get_num(u32 devid, enum soc_sub_type type, u32 *subnum);

/* for dev */
int soc_resmng_dev_set_rsv_mem(u32 devid, const char *name, struct soc_rsv_mem_info *rsv_mem);
int soc_resmng_dev_get_rsv_mem(u32 devid, const char *name, struct soc_rsv_mem_info *rsv_mem);

int soc_resmng_dev_set_reg_base(u32 devid, const char *name, struct soc_reg_base_info *io_base);
int soc_resmng_dev_get_reg_base(u32 devid, const char *name, struct soc_reg_base_info *io_base);

int soc_resmng_dev_set_key_value(u32 devid, const char *name, u64 value);
int soc_resmng_dev_get_key_value(u32 devid, const char *name, u64 *value);

int soc_resmng_dev_set_attr(u32 devid, const char *name, const void *attr, u32 size);
int soc_resmng_dev_get_attr(u32 devid, const char *name, void *attr, u32 size);

int soc_resmng_dev_set_irq_num(u32 devid, u32 irq_type, u32 irq_num);              /* stub */
int soc_resmng_dev_get_irq_num(u32 devid, u32 irq_type, u32 *irq_num);             /* stub */
int soc_resmng_dev_set_irq_by_index(u32 devid, u32 irq_type, u32 index, u32 irq);  /* stub */
int soc_resmng_dev_get_irq_by_index(u32 devid, u32 irq_type, u32 index, u32 *irq); /* stub */

int soc_resmng_dev_set_mia_res(u32 devid, enum soc_mia_res_type type, u64 bitmap, u32 unit_per_bit);
int soc_resmng_dev_get_mia_res(u32 devid, enum soc_mia_res_type type, u64 *bitmap, u32 *unit_per_bit);

int soc_resmng_dev_set_mia_res_ex(u32 devid, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info);
int soc_resmng_dev_get_mia_res_ex(u32 devid, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info);

int soc_resmng_dev_die_set_res(u32 devid, u32 die_id, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info);
int soc_resmng_dev_die_get_res(u32 devid, u32 die_id, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info);

int soc_resmng_dev_set_mia_spec(u32 devid, u32 vfg_num, u32 vf_num);
int soc_resmng_dev_get_mia_spec(u32 devid, u32 *vfg_num, u32 *vf_num);

int soc_resmng_dev_set_mia_base_info(u32 devid, u32 vfgid, u32 vfid);
int soc_resmng_dev_get_mia_base_info(u32 devid, u32 *vfgid, u32 *vfid);

int soc_resmng_dev_set_mia_grp_info(u32 devid, u32 grp_id, struct soc_mia_grp_info *grp_info);
int soc_resmng_dev_get_mia_grp_info(u32 devid, u32 grp_id, struct soc_mia_grp_info *grp_info);

int soc_get_dev_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type);

int soc_resmng_dev_set_vnic_ip(u32 devid, u32 vnic_ip);
int soc_resmng_dev_get_vnic_ip(u32 devid, u32 *vnic_ip);

#endif /* PBL_SOC_RES_H */
