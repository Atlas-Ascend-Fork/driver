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
#include "trs_host_accelerator_util.h"
#ifdef CFG_FEATURE_HOST_ACCELERATOR_UTIL
#include "comm_kernel_interface.h"
#include "udis.h"
#include "pbl_uda.h"
#ifdef CFG_FEATURE_TRS_SIA_ADAPT
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SEC_EH_ADAPT)
#include "trs_sec_eh_auto_init.h"
#endif
#include "trs_mailbox_def.h"
#include "trs_pub_def.h"

enum accelerator_core_type {
    TRS_AICUBE = 0,
    TRS_AIV,
    TRS_AIC,
    TRS_CORE_TYPE_MAX,
};

/* shared memory distribution: 0:statue 1-25:AIC 26-75:AIV */
#define TRS_AI_CORE_OFFSET 1U
#define TRS_AI_VECTOR_OFFSET 26U
#define TRS_AI_CORE_MAX_NUM 25U
#define TRS_AI_VECTOR_MAX_NUM 50U

#define TRS_UTIL_SLEEP_TIME 50U
#define TRS_CORE_UTILSTORE_SIZE 76U

#define TRS_CORE_UTIL_UNUSE_STATUS 0xED  /* core does not belong to this device. */
#define TRS_CORE_UTIL_DAMAGE_STATUS 0xEE /* core damage */
#define TRS_CORE_UTIL_INVALID_STATUS 0xEF

#define TRS_CORE_UTIL_IPC_CONFILCT_RET 10U /* conflict witth ipc */

static ka_mutex_t g_util_mutex[TRS_DEV_MAX_NUM];

static int trs_notice_tsfw_get_util(struct trs_id_inst *inst, enum accelerator_core_type type, u32 cmd_type)
{
    struct trs_accelerator_util_mailbox msg = {0};
    int ret;

    trs_mbox_init_header(&msg.header, cmd_type);
    msg.core_type = type;

    ret = trs_mbox_send(inst, 0, (void *)&msg, sizeof(msg), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);

    return (ret != 0) ? ret : msg.header.result;
}

static void trs_get_core_info(u8 *base, enum accelerator_core_type type, u32 *core_num, u8 **core_base)
{
    switch (type) {
        case TRS_AICUBE:
            *core_num = TRS_AI_CORE_MAX_NUM;
            *core_base = base + TRS_AI_CORE_OFFSET;
            break;
        case TRS_AIV:
            *core_num = TRS_AI_VECTOR_MAX_NUM;
            *core_base = base + TRS_AI_VECTOR_OFFSET;
            break;
        case TRS_AIC:
            *core_num = 1; // The tsfw will calculate correctly, just need to take final value.
            *core_base = base + TRS_AI_CORE_OFFSET;
            break;
        default:
            *core_num = 0; // The value 0 is returned, and no operation is performed.
            *core_base = base;
            break;
    }
}

static inline bool trs_util_status_is_valid(u8 *status)
{
    return (*status == 0);
}

static int trs_calculate_util(u8 *base, enum accelerator_core_type type, u32 *utilization)
{
    u32 core_num, unuse_num = 0;
    u8 *core_base = NULL;
    u32 core_ans = 0;
    int i;

    if (trs_util_status_is_valid(base) == false) {
        trs_warn("Status is invalid. (type=%d; status=%u)\n", type, *base);
        return -EINVAL;
    }

    trs_get_core_info(base, type, &core_num, &core_base);

    for (i = 0; i < core_num; i++) {
        trs_debug("Core util info. (index=%d; core_util=%u; core_ans=%u)\n", i, core_base[i], core_ans);
        if ((core_base[i] == TRS_CORE_UTIL_DAMAGE_STATUS) || (core_base[i] == TRS_CORE_UTIL_UNUSE_STATUS)) {
            unuse_num++;
        } else if (core_base[i] == TRS_CORE_UTIL_INVALID_STATUS) {
            *utilization = TRS_CORE_UTIL_INVALID_STATUS;
            return -EFAULT;
        } else {
            core_ans += core_base[i];
        }
    }

    *utilization = (unuse_num == core_num) ? 0 : (core_ans / (core_num - unuse_num));

    trs_debug("Core util info. (util=%u; core_num=%u; unuse_num=%u; type=%d)\n", *utilization, core_num, unuse_num,
              type);

    return 0;
}

static void trs_util_result_fill(struct udis_dev_info *core_res, u32 utilization)
{
    core_res->update_type = UPDATE_PERIOD_LEVEL_1;
    core_res->acc_ctrl = -1;
    memcpy_s(core_res->data, UDIS_MAX_DATA_LEN, &utilization, sizeof(u32));
    core_res->data_len = sizeof(u32);
    core_res->last_update_time = ktime_get_raw_ns() / KA_NSEC_PER_MSEC;
}

static inline void trs_init_util_status(u8 *status)
{
    *status = TRS_CORE_UTIL_INVALID_STATUS;
}

static inline bool trs_util_status_is_conflict(u8 *status)
{
    return (*status == TRS_CORE_UTIL_IPC_CONFILCT_RET);
}

static int trs_notice_tsfw_get_util_start(struct trs_id_inst *inst, enum accelerator_core_type type, u8 *base)
{
    int ret;

    ret = trs_notice_tsfw_get_util(inst, type, TRS_MBOX_QUERY_CORE_RATE_START);
    if ((ret == 0) && (trs_util_status_is_conflict(base) == true)) {
        trs_debug("Start not success. (type=%d; ret=%d; status=%u)\n", type, ret, *base);
        ret = -ENODATA;
    }

    return ret;
}

static int _trs_get_accelerator_util(u32 devid, enum accelerator_core_type type, u8 *base,
                                     struct udis_dev_info *udis_info)
{
    struct trs_id_inst inst = {.devid = devid, .tsid = 0};
    u32 utilization;
    int ret;

    trs_init_util_status(base);

    ret = trs_notice_tsfw_get_util_start(&inst, type, base);
    if (ret != 0) {
        trs_debug("Start not success. (devid=%u; type=%d; ret=%d)\n", devid, type, ret);
        return ret;
    }

    ka_system_msleep(TRS_UTIL_SLEEP_TIME);

    ret = trs_notice_tsfw_get_util(&inst, type, TRS_MBOX_QUERY_CORE_RATE_END);
    if (ret != 0) {
        trs_warn("End not success. (devid=%u; type=%d; ret=%d)\n", devid, type, ret);
        return ret;
    }

    ret = trs_calculate_util(base, type, &utilization);
    if (ret != 0) {
        trs_warn("Calculate util not success. (devid=%u; type=%d; ret=%d)\n", devid, type, ret);
        return ret;
    }

    trs_util_result_fill(udis_info, utilization);

    ret = hal_kernel_set_udis_info(devid, UDIS_MODULE_TS, udis_info);
    if (ret != 0) {
        trs_warn("Set_udis info not success. (devid=%u; type=%d; ret=%d)\n", devid, type, ret);
    }

    return ret;
}

static int trs_get_accelerator_util(u32 devid, enum accelerator_core_type type, struct udis_dev_info *udis_info)
{
    void __ka_mm_iomem *base = NULL;
    size_t size;
    u64 addr;
    int ret;

    if ((devid >= TRS_DEV_MAX_NUM) || (udis_info == NULL)) {
        trs_err("Invalid value. (devid=%u; type=%d)\n", devid, type);
        return -EINVAL;
    }

    ret = devdrv_get_addr_info(devid, DEVDRV_ADDR_TSDRV_RESV_BASE, 0, &addr, &size);
    if ((ret != 0) || (addr == 0) || (size < TRS_CORE_UTILSTORE_SIZE)) {
        trs_warn("Get base address not success. (devid=%u; type=%d; ret=%d; size=%lx)\n", devid, type, ret, size);
        return -ENODATA;
    }

    base = ka_mm_ioremap(addr, size);
    if (base == NULL) {
        trs_warn("Ioremap not success. (devid=%u; type=%d)\n", devid, type);
        return -ENODATA;
    }

    ka_task_mutex_lock(&g_util_mutex[devid]);
    ret = _trs_get_accelerator_util(devid, type, (u8 *)base, udis_info);
    if (ret != 0) {
        trs_warn("Get accelerator util not success. (devid=%u; type=%d)\n", devid, type);
    }
    ka_task_mutex_unlock(&g_util_mutex[devid]);

    ka_mm_iounmap(base);

    return (ret != 0) ? -ENODATA : 0;
}

static int trs_get_vector_util(u32 devid, struct udis_dev_info *udis_info)
{
    return trs_get_accelerator_util(devid, TRS_AIV, udis_info);
}

static int trs_get_aicube_util(u32 devid, struct udis_dev_info *udis_info)
{
    return trs_get_accelerator_util(devid, TRS_AICUBE, udis_info);
}

static int trs_get_aicore_util(u32 devid, struct udis_dev_info *udis_info)
{
    return trs_get_accelerator_util(devid, TRS_AIC, udis_info);
}

static void trs_util_mutex_init(void)
{
    u32 devid;

    for (devid = 0; devid < TRS_DEV_MAX_NUM; devid++) {
        ka_task_mutex_init(&g_util_mutex[devid]);
    }
}

static void trs_util_mutex_uninit(void)
{
    u32 devid;

    for (devid = 0; devid < TRS_DEV_MAX_NUM; devid++) {
        ka_task_mutex_destroy(&g_util_mutex[devid]);
    }
}

void trs_accelerator_util_init(void)
{
    trs_util_mutex_init();
    (void)hal_kernel_register_udis_func(UDIS_MODULE_TS, "vector_util", trs_get_vector_util);
    (void)hal_kernel_register_udis_func(UDIS_MODULE_TS, "aicube_util", trs_get_aicube_util);
    (void)hal_kernel_register_udis_func(UDIS_MODULE_TS, "aicore_util", trs_get_aicore_util);
    trs_info("Trs accelerator util init succeed.\n");
}

void trs_accelerator_util_uninit(void)
{
    (void)hal_kernel_unregister_udis_func(UDIS_MODULE_TS, "aicore_util");
    (void)hal_kernel_unregister_udis_func(UDIS_MODULE_TS, "aicube_util");
    (void)hal_kernel_unregister_udis_func(UDIS_MODULE_TS, "vector_util");
    trs_util_mutex_uninit();
    trs_info("Trs accelerator util uninit succeed.\n");
}
#else
void trs_accelerator_util_init(void)
{}

void trs_accelerator_util_uninit(void)
{}
#endif