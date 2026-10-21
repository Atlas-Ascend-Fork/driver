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

#ifndef TRS_PUB_DEF_H
#define TRS_PUB_DEF_H

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"
#include "ka_system_pub.h"

#include "ascend_kernel_hal.h"
#include "pbl_ka_memory.h"
#include "pbl_ka_mem_query.h"
#include "ascend_hal_define.h"

#ifndef EMU_ST
#include "dmc_kernel_interface.h"
#else
#include "ut_log.h"
#endif

#ifndef EMU_ST
#define STATIC static
#else
#define STATIC
#endif

#define TRS_DEV_MAX_NUM 1124
#define TRS_TS_MAX_NUM 2
#define TRS_TS_INST_MAX_NUM (TRS_DEV_MAX_NUM * TRS_TS_MAX_NUM)

#define SHR_ID_PID_SERVER_ID_MAX_NUM 48
#define SHR_ID_PID_CHIP_ID_MAX_NUM 8
#define SHR_ID_PID_DIE_ID_MAX_NUM 2

#define TRS_VF_MAX_NUM 16

#define TRS_INVALID_PHYS_ADDR (~(phys_addr_t)0)

enum {
    TRS_INST_STATUS_UNINIT = 0,
    TRS_INST_STATUS_NORMAL,
    TRS_INST_STATUS_ABNORMAL,
    TRS_INST_STATUS_MAX
};

enum trsSqSendMode {
    TRS_MODE_TYPE_SQ_SEND_HIGH_SECURITY = 0x0, /* sq use uik mode */
    TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE,    /* sq use uio mode */
    TRS_MODE_TYPE_SQ_SEND_MAX
};

typedef enum tagTrsModeType {
    TRS_MODE_TYPE_SQ_SEND = 0,
    TRS_MODE_TYPE_MAX
} trsModeType_t;

struct trsModeInfo {
    uint32_t devId;
    uint32_t tsId;
    trsModeType_t mode_type;
    int mode;
};

#define module_trs "trs_drv"
#ifndef EMU_ST
#define trs_err(fmt, ...)                                                                                 \
    do {                                                                                                  \
        drv_err(module_trs, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
        share_log_err(TSDRV_SHARE_LOG_START, fmt, ##__VA_ARGS__);                                         \
    } while (0)
#define trs_warn(fmt, ...)                                                                                 \
    do {                                                                                                   \
        drv_warn(module_trs, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)
#else
#define trs_err(fmt, ...)
#define trs_warn(fmt, ...)
#endif
#define trs_info(fmt, ...)                                                                                 \
    do {                                                                                                   \
        drv_info(module_trs, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)
#define trs_debug(fmt, ...)                                                                                    \
    do {                                                                                                       \
        drv_pr_debug(module_trs, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                     ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define trs_err_hex_dump(buf, len, fmt) drv_err_hex_dump(module_trs, buf, len, fmt)

#define trs_info_ratelimited(fmt, ...)                                                                                 \
    do {                                                                                                               \
        drv_info_ratelimited(module_trs, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                             ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define trs_warn_ratelimited(fmt, ...)                                                                                 \
    do {                                                                                                               \
        drv_warn_ratelimited(module_trs, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                             ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define PCI_VENDOR_ID_HUAWEI 0x19e5

#define TRS_KA_MODULE_ID_TYPE_0 ka_get_module_id(HAL_MODULE_TYPE_TS_DRIVER, KA_SUB_MODULE_TYPE_0)
#define TRS_KA_MODULE_ID_TYPE_1 ka_get_module_id(HAL_MODULE_TYPE_TS_DRIVER, KA_SUB_MODULE_TYPE_1)
#define trs_mem_debug(fmt, ...)

#define trs_kmalloc(size, flags)                      \
    ka_kmalloc(size, flags, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_kmalloc. (size=%u)\n", size)
#define trs_kzalloc(size, flags)                      \
    ka_kzalloc(size, flags, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_kzalloc. (size=%lu)\n", size)
#define trs_kfree(addr)                      \
    ka_kfree(addr, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_kfree.\n")

#define trs_vmalloc(size, gfp_mask, prot)                        \
    __ka_vmalloc(size, gfp_mask, prot, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_vmalloc. (size=%u)\n", size)
#define trs_vzalloc(size)                      \
    ka_vzalloc(size, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_vzalloc. (size=%lu)\n", size)
#define trs_vfree(addr)                      \
    ka_vfree(addr, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_vfree.\n")

#define trs_alloc_pages(gfp_mask, order)                      \
    ka_alloc_pages(gfp_mask, order, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_alloc_pages. (order=%u)\n", order)
#define trs_free_pages(addr, order)                        \
    __ka_free_pages(addr, order, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_free_pages. (order=%u)\n", order)

#define trs_kvzalloc(size, flags)                      \
    ka_kvzalloc(size, flags, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_kvzalloc. (size=%u)\n", size)
#define trs_kvfree(addr)                      \
    ka_kvfree(addr, TRS_KA_MODULE_ID_TYPE_0); \
    trs_mem_debug("trs_kvfree.\n")

#define trs_alloc_pages_node(nid, gfp_mask, order)                      \
    ka_alloc_pages_node(nid, gfp_mask, order, TRS_KA_MODULE_ID_TYPE_1); \
    trs_mem_debug("trs_alloc_pages_node. (nid=%d, order=%u)\n", nid, order)
#define trs_free_pages_node(addr, order)                   \
    __ka_free_pages(addr, order, TRS_KA_MODULE_ID_TYPE_1); \
    trs_mem_debug("trs_free_pages_node.\n")

enum {
    TRS_STREAM_ID = 0,
    TRS_EVENT_ID,
    TRS_NOTIFY_ID,
    TRS_MODEL_ID,
    TRS_CMO_ID,
    TRS_HW_SQ_ID,
    TRS_HW_CQ_ID,
    TRS_SW_SQ_ID,
    TRS_SW_CQ_ID,
    TRS_RSV_HW_SQ_ID,
    TRS_RSV_HW_CQ_ID,
    TRS_TASK_SCHED_CQ_ID,
    TRS_MAINT_SQ_ID,
    TRS_MAINT_CQ_ID,
    TRS_LOGIC_CQ_ID,
    TRS_CB_SQ_ID,
    TRS_CB_CQ_ID,
    TRS_CDQM_ID,
    TRS_CNT_NOTIFY_ID,
    TRS_STARS_H2D_JETTY_ID,
    TRS_STARS_D2D_IN_BOARD_JETTY_ID,
    TRS_STARS_D2D_CROSS_BOARD_JETTY_ID,
    TRS_ID_TYPE_MAX
};

static const char *trs_id_type_name[TRS_ID_TYPE_MAX] = {
    [TRS_STREAM_ID] = "StreamId",
    [TRS_EVENT_ID] = "EventId",
    [TRS_NOTIFY_ID] = "NotifyId",
    [TRS_MODEL_ID] = "ModelId",
    [TRS_CMO_ID] = "CmoId",
    [TRS_HW_SQ_ID] = "HwSqId",
    [TRS_HW_CQ_ID] = "HwCqId",
    [TRS_SW_SQ_ID] = "SwSqId",
    [TRS_SW_CQ_ID] = "SwCqId",
    [TRS_RSV_HW_SQ_ID] = "RsvHwSqId",
    [TRS_RSV_HW_CQ_ID] = "RsvHwCqId",
    [TRS_TASK_SCHED_CQ_ID] = "TaskSchedCqId",
    [TRS_MAINT_SQ_ID] = "MaintSqId",
    [TRS_MAINT_CQ_ID] = "MaintCqId",
    [TRS_LOGIC_CQ_ID] = "LogicCqId",
    [TRS_CB_SQ_ID] = "CbSqId",
    [TRS_CB_CQ_ID] = "CbCqId",
    [TRS_CDQM_ID] = "CdqmId",
    [TRS_CNT_NOTIFY_ID] = "CntNotify",
    [TRS_STARS_H2D_JETTY_ID] = "StarsH2DJetty",
    [TRS_STARS_D2D_IN_BOARD_JETTY_ID] = "StarsD2DInBoardJetty",
    [TRS_STARS_D2D_CROSS_BOARD_JETTY_ID] = "StarsD2DCrossBoardJetty",
};

enum {
    TRS_ID_NODE_DEFAULT_LEVEL = 0,
    TRS_ID_NODE_SECOND_LEVEL,
    TRS_ID_NODE_LEVEL_MAX
};

static inline bool trs_id_is_local_type(int type)
{
    return ((type == TRS_LOGIC_CQ_ID) || (type == TRS_CB_SQ_ID) || (type == TRS_CB_CQ_ID));
}

static inline bool trs_id_need_divide_type(int type)
{
    return !((type == TRS_MAINT_SQ_ID) || (type == TRS_MAINT_CQ_ID));
}

static inline bool trs_id_is_jetty_type(int type)
{
    return ((type == TRS_STARS_H2D_JETTY_ID) || (type == TRS_STARS_D2D_IN_BOARD_JETTY_ID) ||
            (type == TRS_STARS_D2D_CROSS_BOARD_JETTY_ID));
}

static inline bool trs_id_is_hw_divide_type(int type)
{
    return ((type == TRS_EVENT_ID) || (type == TRS_NOTIFY_ID) || (type == TRS_CNT_NOTIFY_ID) || (type == TRS_CMO_ID) ||
            (type == TRS_HW_SQ_ID) || (type == TRS_HW_CQ_ID) || (type == TRS_RSV_HW_SQ_ID) ||
            (type == TRS_RSV_HW_CQ_ID) || (type == TRS_CDQM_ID) || trs_id_is_jetty_type(type));
}

static inline int trs_id_type_replace(int type)
{
    if (type == TRS_HW_SQ_ID) {
        return TRS_RSV_HW_SQ_ID;
    } else if (type == TRS_HW_CQ_ID) {
        return TRS_RSV_HW_CQ_ID;
    } else {
        /* do nothing */
    }

    return type;
}

#define TRS_ID_CACHE_BATCH_NUM 64

static inline const char *trs_id_type_to_name(int type)
{
    if ((type >= TRS_STREAM_ID) && (type < TRS_ID_TYPE_MAX)) {
        return trs_id_type_name[type];
    }
    return "UnknownId";
}

static inline void trs_id_inst_pack(struct trs_id_inst *inst, u32 devid, u32 tsid)
{
    inst->devid = devid;
    inst->tsid = tsid;
}

static inline u32 trs_id_inst_to_ts_inst(struct trs_id_inst *inst)
{
    return inst->devid * TRS_TS_MAX_NUM + inst->tsid;
}

static inline void trs_ts_inst_to_id_inst(u32 ts_inst_id, struct trs_id_inst *inst)
{
    inst->devid = ts_inst_id / TRS_TS_MAX_NUM;
    inst->tsid = ts_inst_id % TRS_TS_MAX_NUM;
}

static inline int trs_id_inst_check(struct trs_id_inst *inst)
{
    if (inst == NULL) {
        trs_err("Null ptr\n");
        return -EINVAL;
    }

    if ((inst->devid >= TRS_DEV_MAX_NUM) || (inst->tsid >= TRS_TS_MAX_NUM)) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    return 0;
}

static inline bool trs_bitmap_bit_is_vaild(u32 bitmap, u32 bit)
{
    return (bitmap & (1ul << bit));
}

static inline void trs_try_resched(unsigned long *cur_jiffies, unsigned long timeout_ms)
{
    unsigned long interval = ka_system_jiffies_to_msecs(ka_jiffies - *cur_jiffies);
    if ((interval > timeout_ms) && !ka_base_in_atomic()) {
        ka_task_cond_resched();
        *cur_jiffies = ka_jiffies;
    }
}

#ifndef EMU_ST

bool trs_is_reboot_active(void);

#define TRS_INIT_REBOOT_NOTIFY                                                                          \
    static bool g_trs_active_reboot = false;                                                            \
    static int trs_reboot_notify_handle(ka_notifier_block_t *notifier, unsigned long event, void *data) \
    {                                                                                                   \
        if (event != SYS_RESTART && event != SYS_HALT && event != SYS_POWER_OFF) {                      \
            return KA_NOTIFY_DONE;                                                                      \
        }                                                                                               \
                                                                                                        \
        g_trs_active_reboot = true;                                                                     \
        return KA_NOTIFY_OK;                                                                            \
    }                                                                                                   \
                                                                                                        \
    static ka_notifier_block_t trs_reboot_notifier = {                                                  \
        .notifier_call = trs_reboot_notify_handle,                                                      \
        .priority = 1,                                                                                  \
    };                                                                                                  \
                                                                                                        \
    bool trs_is_reboot_active(void)                                                                     \
    {                                                                                                   \
        return (g_trs_active_reboot == true);                                                           \
    }

#define TRS_REGISTER_REBOOT_NOTIFY (void)ka_dfx_register_reboot_notifier(&trs_reboot_notifier)
#define TRS_UNREGISTER_REBOOT_NOTIFY (void)ka_dfx_unregister_reboot_notifier(&trs_reboot_notifier)

#define TRS_IS_REBOOT_ACTIVE trs_is_reboot_active()

#else
#define TRS_INIT_REBOOT_NOTIFY
#define TRS_REGISTER_REBOOT_NOTIFY
#define TRS_UNREGISTER_REBOOT_NOTIFY
#define TRS_IS_REBOOT_ACTIVE false
#endif

struct trs_sq_mem_handle {
    struct ka_vmap_pfn_handle *vmap_pfn_handle; /* opaque handle for all SVM mapping */
    u32 pa_num;                                 /* number of segments */
    struct ka_pa_wraper *pa_wraper_list;
    struct ka_mem_attr mem_attr; /* mem_attr.addr = aligned base va */
};

#endif /* TRS_PUB_DEF_H */
