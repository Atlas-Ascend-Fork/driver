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

#include "ka_memory_pub.h"
#include "hdcdrv_core.h"
#include "hdcdrv_core_com.h"

#ifndef CFG_FEATURE_VHDC_ADAPT
u32 hdcdrv_gen_unique_value(void)
{
    u32 value;
    value = (u32)ka_base_atomic_inc_return(&hdc_ctrl->unique_val);
    value = HDCDRV_SESSION_UNIQUE_VALUE_HOST_FLAG | (value & HDCDRV_SESSION_UNIQUE_VALUE_MASK);
    return value;
}

bool hdcdrv_ctrl_msg_connect_get_permission(const struct hdcdrv_ctrl_msg *msg, u32 devid)
{
    return true;
}

int hdcdrv_get_connect_fid(int service_type, u32 fid)
{
    (void)service_type;
    return fid;
}

long hdcdrv_convert_id_from_vir_to_phy(u32 drv_cmd, union hdcdrv_cmd *cmd_data, u32 *vfid)
{
    long ret = HDCDRV_OK;
    int *p_devid = NULL;
    int local_devid = 0;
    switch (drv_cmd) {
        case HDCDRV_CMD_GET_PEER_DEV_ID:
        case HDCDRV_CMD_CLIENT_DESTROY:
        case HDCDRV_CMD_SERVER_CREATE:
        case HDCDRV_CMD_SERVER_DESTROY:
        case HDCDRV_CMD_ACCEPT:
        case HDCDRV_CMD_CONNECT:
        case HDCDRV_CMD_ALLOC_MEM:
        case HDCDRV_CMD_FREE_MEM:
        case HDCDRV_CMD_FAST_SEND:
        case HDCDRV_CMD_FAST_RECV:
        case HDCDRV_CMD_DMA_MAP:
        case HDCDRV_CMD_DMA_UNMAP:
        case HDCDRV_CMD_DMA_REMAP:
        case HDCDRV_CMD_REGISTER_MEM:
        case HDCDRV_CMD_UNREGISTER_MEM:
        case HDCDRV_CMD_WAIT_MEM:
            p_devid = &cmd_data->cmd_com.dev_id;
            break;
        case HDCDRV_CMD_EPOLL_ALLOC_FD:
            p_devid = &local_devid;
            break;
        case HDCDRV_CMD_EPOLL_CTL:
            p_devid = hdcdrv_epoll_get_dev_id_ptr(cmd_data);
            if (p_devid == NULL) {
                return HDCDRV_OK;
            }
            break;
        default:
            return ret;
    }
    ret = hdcdrv_container_vir_to_phs_devid((u32)(*p_devid), (u32 *)p_devid, vfid);
    return ret;
}

void hdcdrv_init_register(void)
{
    devdrv_pci_suspend_check_register(hdcdrv_session_free_check);
    devdrv_peer_fault_notifier_register(hdcdrv_peer_fault_notify);
}

void hdcdrv_uninit_unregister(void)
{
    devdrv_pci_suspend_check_unregister();
    devdrv_peer_fault_notifier_unregister();
}

void hdcdrv_get_mempool_size(u32 *small_packet_num, u32 *huge_packet_num)
{
    u32 mempool_level = hdcdrv_get_hdc_mempool_level();
    if (mempool_level >= HDC_MEM_POOL_LEVEL_INVALID) {
        *small_packet_num = HDCDRV_SMALL_PACKET_NUM;
        *huge_packet_num = HDCDRV_HUGE_PACKET_NUM;
    } else {
        *small_packet_num = HDCDRV_SMALL_PACKET_NUM >> mempool_level;
        *huge_packet_num = HDCDRV_HUGE_PACKET_NUM >> mempool_level;
    }
}

void hdcdrv_set_session_run_env(u32 dev_id, u32 fid, int *run_env)
{
    *run_env = hdcdrv_get_session_run_env(dev_id, fid);
}
#endif

ka_page_t *hdcdrv_alloc_pages_node_inner(u32 dev_id, ka_gfp_t gfp_mask, u32 order)
{
    return hdcdrv_alloc_pages(gfp_mask, order, KA_SUB_MODULE_TYPE_2);
}

void *hdcdrv_kzalloc_mem_node_inner(u32 dev_id, ka_gfp_t gfp_mask, u32 size, u32 level)
{
    return hdcdrv_kzalloc(size, gfp_mask, level);
}

ka_gfp_t hdcdrv_init_mem_pool_get_gfp(void)
{
    /* declare __GFP_RECLAIM (GFP_KERNEL) to allow blocking, alloc cma firstly */
    return KA_GFP_KERNEL;
}

u64 hdcdrv_get_hash_fid(u32 fid)
{
    return (u64)fid;
}