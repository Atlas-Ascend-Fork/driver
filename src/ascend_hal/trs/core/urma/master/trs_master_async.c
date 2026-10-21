/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdbool.h>
#include <limits.h>
#include "ascend_inpackage_hal.h"
#include "securec.h"
#include "urma_api.h"
#include "trs_ioctl.h"
#include "drv_user_common.h"
#include "esched_user_interface.h"
#include "svm_user_interface.h"
#include "hash_table.h"
#include "trs_urma.h"
#include "trs_user_pub_def.h"
#include "trs_uk_msg.h"
#include "trs_dev_drv.h"
#include "trs_master_event.h"
#include "trs_master_urma.h"
#include "trs_master_async.h"
#include "trs_urma_async.h"
#include "trs_res.h"

struct udma_sqe_ctl_tmp {
    uint32_t sqe_bb_idx : 16;
    uint32_t odr : 3;
    uint32_t fence : 1;
    uint32_t se : 1;
    uint32_t cqe : 1;
    uint32_t inline_en : 1;
    uint32_t udf_flag : 1;
    uint32_t rsv : 3;
    uint32_t nf : 1;
    uint32_t token_en : 1;
    uint32_t rmt_jetty_type : 2;
    uint32_t owner : 1;

    uint32_t target_hint : 8;
    uint32_t opcode : 8;
    uint32_t rsv1 : 6;
    uint32_t inline_msg_len : 10;

    uint32_t tpn : 24;
    uint32_t sge_num : 8;

    uint32_t rmt_obj_id : 20;
    uint32_t rsv2 : 12;

    uint32_t rmt_eid[4];
    uint32_t rmt_token_value;
    uint32_t udf;
    uint32_t rmt_addr_l_or_token_id;
    uint32_t rmt_addr_h_or_token_value;
};

struct udma_wqe_sge_tmp {
    uint32_t length;
    uint32_t token_id : 20;
    uint32_t rsv3 : 12;
    uint64_t va;
};

struct trs_urma_dwqe_fill_para {
    uint64_t send_addr;
    uint64_t recv_addr;
    uint64_t len;
    uint32_t cqe_report;
    uint32_t op_code;
    uint32_t sqe_bb_idx;
    urma_eid_t *rmt_eid;
    uint32_t tpn;
    urma_seg_t seg;
    urma_token_t token;
};

struct trs_async_jetty_node {
    struct trs_async_jetty_info jetty_info;
    struct halAsyncJettyHandle jettyHandle;
    bool is_free_implicitly;
};

struct trs_async_jetty_mng_ctx {
    hash_table *hash_table;
    pthread_rwlock_t rw_mutex;
    uint32_t node_num;
};

static struct trs_async_jetty_mng_ctx g_jetty_mng_ctx[TRS_DEV_NUM];

extern int halMemGetSeg(uint32_t dev_id, uint64_t va, uint64_t size, urma_seg_t *seg, urma_token_t *token);

/* Pre-segmented address range for async DMA copy. */
struct trs_async_seg {
    uint64_t src;
    uint64_t dst;
    uint64_t len;
    uint32_t dir;
    struct trs_async_devid async_devid;
};

/* Segment array buffer: holds pre-segmented segs and tracks capacity/usage. */
struct trs_async_seg_info {
    struct trs_async_seg *segs;
    uint32_t seg_cnt;
    uint32_t max_segs;
};

/* Maximum number of segments for a single copy element/row. */
#define TRS_ASYNC_MAX_SEGS 256U

/* Compute the max segment array size based on actual input dimensions. */
static uint32_t trs_async_get_max_segs(struct trs_async_dma_input_para *in)
{
    if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) {
        return TRS_ASYNC_MAX_SEGS * (uint32_t)in->async_batch_in.count;
    }
    if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) {
        return TRS_ASYNC_MAX_SEGS * (uint32_t)in->async_2d_in.height;
    }
    return TRS_ASYNC_MAX_SEGS;
}

static int trs_get_segment(struct trs_urma_ctx *urma_ctx, uint64_t va, uint64_t size,
                           struct trs_async_dma_wqe_info *wqe_info, urma_seg_t *seg, urma_token_t *token)
{
    int ret = 0;

    ret = halMemGetSeg(urma_ctx->devid, va, (uint64_t)size, seg, token);
    if (ret != 0) {
        trs_err("Get segment failed. (devid=%u; va=0x%llx; size=%llu; ret=%d)\n", urma_ctx->devid, va, size, ret);
        return ret;
    }

    wqe_info->flag = 0;
    trs_debug("Get token_id from svm. (devid=%u)\n", urma_ctx->devid);
    return 0;
}

static bool trs_mem_type_is_host(uint32_t mem_type)
{
    return (mem_type == DV_MEM_LOCK_HOST) || (mem_type == DV_MEM_LOCK_HOST_AGENT) || (mem_type == DV_MEM_SVM_HOST) ||
           (mem_type == DV_MEM_USER_MALLOC) || (mem_type == DV_MEM_USER_REGISTER);
}

static int trs_get_mem_attribute(DVdeviceptr vptr, struct DVattribute *attr)
{
    int ret = drvMemGetAttribute(vptr, attr);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Failed to get va attribute. (ret=%d; va=0x%llx)\n", ret, vptr);
        return ret;
    }

    if (attr->memType == DV_MEM_USER_MALLOC) {
        trs_err("User malloc mem not support, need register. (mem_type=%u; va=0x%llx)\n", attr->memType, vptr);
        return DRV_ERROR_UNINIT;
    }

    if (trs_mem_type_is_host(attr->memType)) {
        uint32_t host_id = 0;
        ret = halGetHostID(&host_id);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Failed to get host id. (ret=%d; va=0x%llx)\n", ret, vptr);
            return ret;
        }
        /* drvMemGetAttribute does not guarantee devId for host memory. */
        attr->devId = host_id;
    }

    return 0;
}

void trs_swap_endian(uint8_t dst[], uint8_t src[], uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        dst[i] = src[size - 1 - i];
    }
}

static inline void trs_get_async_send_recv_addr(struct trs_async_devid *async_devid, uint64_t src, uint64_t dst,
                                                uint64_t *send_addr, uint64_t *recv_addr)
{
    if (async_devid->local_devid == async_devid->dst_devid) {
        *send_addr = dst;
        *recv_addr = src;
    } else {
        *send_addr = src;
        *recv_addr = dst;
    }
}

/* Pre-segment a copy range by calling halMemGetAddrSeg on the host address.
 * For H2D, src is host address; for D2H, dst is host address; for D2D, fall back to 256M splitting. */
static int trs_async_pre_segment(uint32_t dev_id, const struct trs_async_seg *in_seg, struct trs_async_seg *segs,
                                 uint32_t *seg_cnt, uint32_t max_cnt)
{
    struct addr_range addr_in = {0};
    struct addr_range *out = NULL;
    bool segment_src = (in_seg->dir == TRS_ASYNC_DEVICE_TO_HOST) ? false : true;
    uint32_t cnt, valid_cnt, i;
    int ret;

    *seg_cnt = 0;

    cnt = (uint32_t)((in_seg->len + TRS_URMA_WR_CPY_MAX_SIZE - 1) / TRS_URMA_WR_CPY_MAX_SIZE * 2);
    if ((cnt == 0) || (cnt > TRS_ASYNC_MAX_SEGS)) {
        return DRV_ERROR_INVALID_VALUE;
    }
    out = (struct addr_range *)malloc(sizeof(struct addr_range) * cnt);
    if (out == NULL) {
        trs_err("Malloc addr_range array failed. (size=%zu)\n", sizeof(struct addr_range) * cnt);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    addr_in.addr = segment_src ? in_seg->src : in_seg->dst;
    addr_in.size = in_seg->len;
    ret = halMemGetAddrSeg(dev_id, addr_in, out, &cnt, 0);
    if (ret != DRV_ERROR_NONE) {
        trs_err("HalMemGetAddrSeg failed. (dev_id=%u; addr=0x%llx; len=%llu; ret=%d)\n", dev_id, addr_in.addr,
                in_seg->len, ret);
        free(out);
        return ret;
    }
    valid_cnt = (cnt > max_cnt) ? max_cnt : cnt;

    for (i = 0; i < valid_cnt; i++) {
        if (segment_src) {
            segs[i].src = out[i].addr;
            segs[i].dst = in_seg->dst + (out[i].addr - in_seg->src);
        } else {
            segs[i].dst = out[i].addr;
            segs[i].src = in_seg->src + (out[i].addr - in_seg->dst);
        }
        segs[i].len = out[i].size;
        segs[i].dir = in_seg->dir;
    }
    *seg_cnt = cnt; /* should be the cnt of halMemGetAddrSeg */

    free(out);
    return DRV_ERROR_NONE;
}

static struct trs_async_ctx *trs_get_async_ctx(struct trs_urma_ctx *urma_ctx, uint32_t dir,
                                               enum trs_async_dma_type async_dma_type, trs_d2d_index_t d2d_index)
{
    struct trs_async_ctx *async_ctx = NULL;

    if (dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        if ((async_dma_type == TRS_ASYNC_DMA_TYPE_2D) || (async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH)) {
            async_ctx = &urma_ctx->d2d_batch_2d_async_ctx[d2d_index];
        } else {
            async_ctx = &urma_ctx->d2d_async_ctx[d2d_index];
        }
    } else {
        if ((async_dma_type == TRS_ASYNC_DMA_TYPE_2D) || (async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH)) {
            async_ctx = &urma_ctx->batch_2d_async_ctx;
        } else {
            async_ctx = &urma_ctx->async_ctx;
        }
    }

    return (async_ctx->init_flag) ? async_ctx : NULL;
}

static void trs_get_async_input_src_dst_len_array(struct trs_async_dma_input_para *in, unsigned long long **src_array,
                                                  unsigned long long **dst_array, unsigned long long **src_len_array,
                                                  unsigned long long **dst_len_array)
{
    enum trs_async_dma_type async_type = in->async_dma_type;

    if (async_type == TRS_ASYNC_DMA_TYPE_2D) {
        *src_array = (unsigned long long *)&in->async_2d_in.src;
        *dst_array = (unsigned long long *)&in->async_2d_in.dst;
    } else {
        *src_array = (async_type == TRS_ASYNC_DMA_TYPE_BATCH) ? in->async_batch_in.src :
                                                                (unsigned long long *)&in->async_normal_in.src;
        *dst_array = (async_type == TRS_ASYNC_DMA_TYPE_BATCH) ? in->async_batch_in.dst :
                                                                (unsigned long long *)&in->async_normal_in.dst;
    }

    if (async_type == TRS_ASYNC_DMA_TYPE_2D) {
        *src_len_array = &in->async_2d_in.width;
        *dst_len_array = &in->async_2d_in.width;
    } else {
        *src_len_array = (async_type == TRS_ASYNC_DMA_TYPE_BATCH) ? in->async_batch_in.len :
                                                                    (unsigned long long *)&in->async_normal_in.len;
        *dst_len_array = *src_len_array;
    }
}

static drvError_t trs_check_d2d_async_devid(uint32_t dev_id, uint32_t src_dev_id, uint32_t dst_dev_id)
{
    uint32_t host_dev_id;

    if ((dev_id != src_dev_id) && (dev_id != dst_dev_id)) {
        trs_err("Invalid dev_id. (dev_id=%u; src_dev_id=%u; dst_dev_id=%u)\n", dev_id, src_dev_id, dst_dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)halGetHostID(&host_dev_id);
    if ((src_dev_id == host_dev_id) || (dst_dev_id == host_dev_id)) {
        trs_err("Invalid dev_id. (host_dev_id=%u; src_dev_id=%u; dst_dev_id=%u)\n", host_dev_id, src_dev_id,
                dst_dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (src_dev_id == dst_dev_id) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

static int trs_check_async_devid_dir(struct trs_async_dma_input_para *in, uint32_t local_devid, uint32_t src_devid,
                                     uint32_t dst_devid, uint32_t dir)
{
    enum trs_async_dma_type async_type = in->async_dma_type;
    int ret = 0;

    if ((async_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE) && (dir != TRS_ASYNC_HOST_TO_DEVICE)) {
        trs_err("Invalid async_type or dir. (async_type=%d; dir=%u)\n", async_type, dir);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dir == TRS_ASYNC_HOST_TO_DEVICE) {
        if (dst_devid != local_devid) {
            trs_err("Invalid devid. (dst_devid=%u; local_devid=%u)\n", dst_devid, local_devid);
            return DRV_ERROR_PARA_ERROR;
        }
    } else if (dir == TRS_ASYNC_DEVICE_TO_HOST) {
        if (src_devid != local_devid) {
            trs_err("Invalid devid. (src_devid=%u; local_devid=%u)\n", src_devid, local_devid);
            return DRV_ERROR_PARA_ERROR;
        }
    } else if (dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        ret = trs_check_d2d_async_devid(local_devid, src_devid, dst_devid);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int trs_get_d2d_index_dir(uint32_t src_devid, uint32_t dst_devid, trs_d2d_index_t *d2d_idx, uint32_t *dir)
{
    int64_t board_location = LOCATION_IN_BOARD;
    int ret = halGetPairDevicesInfo(src_devid, dst_devid, DEVS_INFO_TYPE_BOARD_LOCATION, &board_location);
    if (ret != 0) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            trs_err("Failed to get board location. (ret=%d; src_devid=%u; dst_devid=%u)\n", ret, src_devid, dst_devid);
            return ret;
        }
        /* DRV_ERROR_NOT_SUPPORT: Default to in-board */
    }
    *d2d_idx = (board_location == LOCATION_IN_BOARD) ? TRS_D2D_IN_BOARD_INDEX : TRS_D2D_CROSS_BOARD_INDEX;
    *dir = TRS_ASYNC_DEVICE_TO_DEVICE;
    return 0;
}

static int trs_get_check_element_devid_dir(uint32_t dev_id, struct trs_async_dma_input_para *in,
                                           struct trs_async_addr_pack *elem, struct trs_async_devid *async_devid,
                                           uint32_t *dir)
{
    enum trs_async_dma_type async_type = in->async_dma_type;
    struct DVattribute srcAttr = {0}, dstAttr = {0};
    uint64_t src = elem->src;
    uint64_t dst = elem->dst;
    trs_d2d_index_t d2d_idx = TRS_MAX_D2D_INDEX;
    uint32_t host_id;
    int ret;

    ret = trs_get_mem_attribute((DVdeviceptr)src, &srcAttr);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Failed to get src attribute. (ret=%d; src=0x%llx)\n", ret, src);
        return ret;
    }

    if (async_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE) {
        dstAttr.devId = dev_id;
    } else {
        ret = trs_get_mem_attribute((DVdeviceptr)dst, &dstAttr);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Failed to get dst attribute. (ret=%d; dst=0x%llx)\n", ret, dst);
            return ret;
        }
    }

    (void)halGetHostID(&host_id);
    if ((srcAttr.devId == host_id) && (dstAttr.devId != host_id)) {
        *dir = TRS_ASYNC_HOST_TO_DEVICE;
    } else if ((dstAttr.devId == host_id) && (srcAttr.devId != host_id)) {
        *dir = TRS_ASYNC_DEVICE_TO_HOST;
    } else if ((dstAttr.devId != host_id) && (srcAttr.devId != host_id)) {
        ret = trs_get_d2d_index_dir(srcAttr.devId, dstAttr.devId, &d2d_idx, dir);
        if (ret != 0) {
            return ret;
        }
    } else {
        trs_err("Invalid va. (src=0x%llx; srcMemType=%u; dst=0x%llx; dstMemType=%u; asyncType=%d)\n", src,
                srcAttr.memType, dst, dstAttr.memType, async_type);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = trs_check_async_devid_dir(in, dev_id, srcAttr.devId, dstAttr.devId, *dir);
    if (ret != 0) {
        return ret;
    }

    trs_pack_async_devid(dev_id, srcAttr.devId, dstAttr.devId, d2d_idx, async_devid);
    trs_debug("(src=0x%llx; src_devid=%u; srcType=%u; dst=0x%llx; dst_devid=%u; dstType=%u; asyncType=%d; devid=%u; "
              "d2d_index=%u)\n",
              src, async_devid->src_devid, srcAttr.memType, dst, async_devid->dst_devid, dstAttr.memType, async_type,
              dev_id, d2d_idx);

    return 0;
}

static int trs_get_check_async_devid_dir(uint32_t dev_id, struct trs_async_dma_input_para *in,
                                         struct trs_async_devid *async_devid, uint32_t *dir)
{
    unsigned long long *src_array, *dst_array;
    unsigned long long *src_len_array, *dst_len_array;
    struct trs_async_addr_pack elem = {0};

    trs_get_async_input_src_dst_len_array(in, &src_array, &dst_array, &src_len_array, &dst_len_array);
    elem.src = src_array[0];
    elem.dst = dst_array[0];

    return trs_get_check_element_devid_dir(dev_id, in, &elem, async_devid, dir);
}

static urma_opcode_t trs_get_async_urma_opcode(uint32_t dev_id, uint32_t dst_devid, uint32_t dir)
{
    if (dir == TRS_ASYNC_HOST_TO_DEVICE) {
        return URMA_OPC_READ;
    } else if (dir == TRS_ASYNC_DEVICE_TO_HOST) {
        return URMA_OPC_WRITE;
    } else if (dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        return (dev_id == dst_devid) ? URMA_OPC_READ : URMA_OPC_WRITE;
    } else {
        return URMA_OPC_LAST;
    }
}

static int trs_fill_remote_wr_info(struct trs_dma_wr_addr *wr_addr, uint32_t devid, urma_opcode_t urma_opcode,
                                   uint32_t dir, struct trs_async_addr_pack *elem)
{
    urma_token_t src_token, dst_token;
    urma_seg_t src_seg = {0};
    urma_seg_t dst_seg = {0};
    int ret;

    ret = halMemGetSeg(devid, elem->src, elem->len, &src_seg, &src_token);
    if (ret != 0) {
        trs_err("Failed to get src segment. (vaddr=0x%p; dir=%u; ret=%d)\n", elem->src, dir, ret);
        return ret;
    }

    ret = halMemGetSeg(devid, elem->dst, elem->len, &dst_seg, &dst_token);
    if (ret != 0) {
        trs_err("Failed to get dst segment. (vaddr=0x%p; dir=%u; ret=%d)\n", elem->dst, dir, ret);
        return ret;
    }

    /* svm return the base seg va, need set to real va */
    src_seg.ubva.va = elem->src;
    dst_seg.ubva.va = elem->dst;

    wr_addr->len = elem->len;
    wr_addr->src_ubva = elem->src;
    wr_addr->dst_ubva = elem->dst;
    wr_addr->recv_seg = (urma_opcode == URMA_OPC_READ) ? src_seg : dst_seg;
    wr_addr->send_seg = (urma_opcode == URMA_OPC_READ) ? dst_seg : src_seg;
    wr_addr->recv_token = (urma_opcode == URMA_OPC_READ) ? src_token : dst_token;
    wr_addr->urma_opcode = (uint32_t)urma_opcode;
    wr_addr->dir = dir;

    return 0;
}

static int trs_fill_remote_wr_batch_info(struct trs_urma_ctx *urma_ctx, struct trs_async_devid *async_devid,
                                         struct trs_async_seg *segs, uint32_t seg_cnt,
                                         struct trs_remote_fill_wqe_info *wqe_fill_info)
{
    urma_opcode_t urma_opcode;
    uint32_t i;
    int ret = 0;

    for (i = 0; i < seg_cnt; i++) {
        struct trs_async_addr_pack elem = {0};
        elem.src = segs[i].src;
        elem.dst = segs[i].dst;
        elem.len = segs[i].len;
        urma_opcode = trs_get_async_urma_opcode(async_devid->local_devid, async_devid->dst_devid, segs[i].dir);
        ret = trs_fill_remote_wr_info(&wqe_fill_info->wr_addr[i], urma_ctx->devid, urma_opcode, segs[i].dir, &elem);
        if (ret != 0) {
            return ret;
        }
    }

    return ret;
}

static int trs_remote_fill_async_dma_wqe(struct trs_async_devid *async_devid, struct trs_urma_ctx *urma_ctx,
                                         struct trs_async_seg *segs, uint32_t seg_cnt,
                                         struct trs_async_dma_input_para *in)
{
    struct trs_remote_fill_wqe_info *wqe_fill_info = NULL;
    unsigned long long wqe_fill_info_size = 0;
    struct event_reply reply;
    uint32_t dev_id = async_devid->local_devid;
    int result, ret;

    wqe_fill_info_size = (sizeof(struct trs_remote_fill_wqe_info) + sizeof(struct trs_dma_wr_addr) * seg_cnt);
    wqe_fill_info = (struct trs_remote_fill_wqe_info *)malloc(wqe_fill_info_size);
    if (wqe_fill_info == NULL) {
        trs_err("Malloc wqe fill info failed. (size=%llu)\n", wqe_fill_info_size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    wqe_fill_info->sq_id = in->sqId;
    wqe_fill_info->recv_dev_id = async_devid->remote_devid;
    wqe_fill_info->d2d_index = async_devid->d2d_index;
    wqe_fill_info->async_dma_type = in->async_dma_type;
    wqe_fill_info->batch_num = seg_cnt;
    ret = trs_fill_remote_wr_batch_info(urma_ctx, async_devid, segs, seg_cnt, wqe_fill_info);
    if (ret != 0) {
        free(wqe_fill_info);
        return ret;
    }

    reply.buf_len = sizeof(result);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, wqe_fill_info, wqe_fill_info_size, DRV_SUBEVENT_TRS_FILL_WQE_MSG, &reply);
    if (ret != DRV_ERROR_NONE) {
        free(wqe_fill_info);
        trs_err("Remote fill wqe failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

    free(wqe_fill_info);
    return (drvError_t)result;
}

static uint32_t trs_get_async_dwqe_opcode(uint32_t dev_id, struct trs_async_devid *async_devid, uint32_t dir)
{
    if (dir == TRS_ASYNC_HOST_TO_DEVICE) {
        return TRS_URMA_OPCODE_READ;
    } else if (dir == TRS_ASYNC_DEVICE_TO_HOST) {
        return TRS_URMA_OPCODE_WRITE;
    } else if (dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        return (dev_id == async_devid->dst_devid) ? TRS_URMA_OPCODE_READ : TRS_URMA_OPCODE_WRITE;
    } else {
        return 0xFFFF; // 0xFFFF: invalid opcode
    }
}

static void _trs_fill_async_dma_direct_wqe(struct trs_urma_dwqe_fill_para *fill_para, uint8_t *wqe)
{
    struct udma_sqe_ctl_tmp *sqe = (struct udma_sqe_ctl_tmp *)wqe;
    struct udma_wqe_sge_tmp *sge = (struct udma_wqe_sge_tmp *)(wqe + sizeof(struct udma_sqe_ctl_tmp));

    sqe->sqe_bb_idx = (fill_para->sqe_bb_idx & 0xFFFFu);
    sqe->odr = 0x6; // send:0x4; write:0x6
    sqe->fence = 1;
    sqe->se = 1;
    sqe->cqe = (fill_para->cqe_report & 0x1);
    sqe->inline_en = 0;
    sqe->udf_flag = 0;
    sqe->nf = 0;
    sqe->token_en = 1;
    sqe->rmt_jetty_type = 0;
    sqe->owner = 0;
    sqe->target_hint = 0;
    sqe->inline_msg_len = 0;
    sqe->sge_num = 1;

    sqe->tpn = (fill_para->tpn & 0xFFFFFF);
    if (fill_para->rmt_eid != NULL) {
        trs_swap_endian((uint8_t *)sqe->rmt_eid, fill_para->rmt_eid->raw, URMA_EID_SIZE);
    }

    sqe->opcode = (fill_para->op_code & 0xFF);

    sqe->rmt_token_value = fill_para->token.token;
    sqe->rmt_obj_id = (fill_para->seg.token_id >> 8) & 0xFFFFFu; // 8: token_id offset

    /* read/write: JFR segment */
    sqe->rmt_addr_l_or_token_id = ((uint32_t)fill_para->recv_addr) & (0xFFFFFFFF);
    sqe->rmt_addr_h_or_token_value = (uint32_t)(fill_para->recv_addr >> 32) & (0xFFFFFFFF); // 32: right bits

    /* read/write: JFS segment */
    sge->length = (uint32_t)fill_para->len;
    sge->token_id = 0;
    sge->va = fill_para->send_addr;
}

static int trs_fill_async_dma_direct_wqe(struct trs_async_devid *async_devid, struct trs_urma_ctx *urma_ctx,
                                         struct trs_async_ctx *async_ctx, struct trs_async_dma_wqe_info *wqe_info,
                                         struct trs_async_dma_input_para *in)
{
    struct trs_urma_dwqe_fill_para fill_para = {0};
    int ret = 0;

    trs_get_async_send_recv_addr(async_devid, (uint64_t)(uintptr_t)in->async_normal_in.src,
                                 (uint64_t)(uintptr_t)in->async_normal_in.dst, &fill_para.send_addr,
                                 &fill_para.recv_addr);
    fill_para.len = in->async_normal_in.len;
    fill_para.op_code = trs_get_async_dwqe_opcode(async_devid->local_devid, async_devid, in->dir);
    fill_para.sqe_bb_idx = async_ctx->pi;
    fill_para.cqe_report = 1;

    if (in->dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        struct trs_d2d_dev_ctx *d2d_dev_ctx = NULL;
        trs_dev_ctx_mutex_lock(wqe_info->remote_dev_id);
        d2d_dev_ctx = (struct trs_d2d_dev_ctx *)trs_get_d2d_dev_ctx(wqe_info->remote_dev_id, async_devid->d2d_index);
        if (d2d_dev_ctx == NULL) {
            trs_dev_ctx_mutex_un_lock(wqe_info->remote_dev_id);
            return DRV_ERROR_UNINIT;
        }

        fill_para.tpn = d2d_dev_ctx->tpn[async_devid->local_devid] & 0xFFFFFF;
        fill_para.rmt_eid = &(d2d_dev_ctx->dst_jfr_id.eid);
        trs_dev_ctx_mutex_un_lock(wqe_info->remote_dev_id);
    } else {
        fill_para.tpn = async_ctx->tpn & 0xFFFFFF;
        fill_para.rmt_eid = &(urma_ctx->trs_jfr.jfr->urma_ctx->eid);
    }

    ret = trs_get_segment(urma_ctx, fill_para.recv_addr, fill_para.len, wqe_info, &fill_para.seg, &fill_para.token);
    if (ret != 0) {
        trs_err("Failed to get segment. (vaddr=0x%llx; dir=%u; ret=%d)\n", fill_para.recv_addr, in->dir, ret);
        return ret;
    }

    _trs_fill_async_dma_direct_wqe(&fill_para, wqe_info->wqe);
    return 0;
}

static inline uint32_t trs_get_async_jetty_credit(struct trs_async_ctx *async_ctx, bool is_direct_wqe)
{
    if (is_direct_wqe == true) { /* dwqe not post wr, just pack */
        return trs_jetty_get_credit(async_ctx->pi, async_ctx->ci, TRS_UB_PI_CI_DEPTH);
    } else {
        return trs_jetty_get_credit(async_ctx->pi % TRS_STARS_NORMAL_JETTY_DEPTH,
                                    async_ctx->ci % TRS_STARS_NORMAL_JETTY_DEPTH, TRS_STARS_NORMAL_JETTY_DEPTH);
    }
}

/* fixedVal: means fixedCnt for batch/nop; fixedSIze for others */
static void trs_get_async_post_wr_num(struct trs_async_dma_input_para *in, uint32_t credit, bool is_normal_use_fixedVal,
                                      uint32_t *wr_num, uint64_t *fixedVal, uint64_t *partial_fixed_size)
{
    *wr_num = 0;

    if ((in->async_dma_type == TRS_ASYNC_DMA_TYPE_NORMAL) || (in->async_dma_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE)) {
        *wr_num = (uint32_t)(trs_align_up_power2(in->async_normal_in.len, TRS_URMA_WR_CPY_MAX_SIZE) /
                             TRS_URMA_WR_CPY_MAX_SIZE);
        if (is_normal_use_fixedVal == true) {
            *fixedVal = (*wr_num <= credit) ? in->async_normal_in.len : (credit * TRS_URMA_WR_CPY_MAX_SIZE);
            *wr_num = (uint32_t)((*wr_num <= credit) ? *wr_num : credit);
        }
    } else if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) { /* batch : one element is atomic async cpy */
        uint64_t i;

        *fixedVal = 0;
        for (i = 0; i < in->async_batch_in.count; i++) {
            uint32_t tmp = (uint32_t)(trs_align_up_power2(in->async_batch_in.len[i], TRS_URMA_WR_CPY_MAX_SIZE) /
                                      TRS_URMA_WR_CPY_MAX_SIZE);
            if ((*wr_num + tmp) <= credit) {
                (*wr_num) += tmp;
                (*fixedVal)++;
                continue;
            }

            if (partial_fixed_size != NULL) {
                *partial_fixed_size = (credit - *wr_num) * TRS_URMA_WR_CPY_MAX_SIZE;
                *wr_num = credit;
            }
            return;
        }
    } else if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) { /* batch : one byte is atomic async cpy */
        u64 remain_height = in->async_2d_in.height - (in->async_2d_in.fixedSize / in->async_2d_in.width);
        u64 width_offset = (in->async_2d_in.fixedSize % in->async_2d_in.width);
        u64 align_fixed_width = trs_align_up_power2(in->async_2d_in.width - width_offset, TRS_URMA_WR_CPY_MAX_SIZE);
        u32 align_fixed_width_wr_num = (uint32_t)(align_fixed_width / TRS_URMA_WR_CPY_MAX_SIZE);
        u32 align_width_wr_num = (uint32_t)(trs_align_up_power2(in->async_2d_in.width, TRS_URMA_WR_CPY_MAX_SIZE) /
                                            TRS_URMA_WR_CPY_MAX_SIZE);

        *fixedVal = in->async_2d_in.fixedSize;
        if (align_fixed_width_wr_num > credit) { /* credit not meet the the-fixedSize-row */
            *wr_num = credit;
            (*fixedVal) += (credit * TRS_URMA_WR_CPY_MAX_SIZE);
        } else {
            if ((remain_height - 1) * align_width_wr_num >
                (credit - align_fixed_width_wr_num)) { /* credit not meet the whole 2d size */
                *wr_num = credit;
                (*fixedVal) += ((in->async_2d_in.width - width_offset) +
                                (((credit - align_fixed_width_wr_num) / align_width_wr_num) * in->async_2d_in.width) +
                                (((credit - align_fixed_width_wr_num) % align_width_wr_num) *
                                 TRS_URMA_WR_CPY_MAX_SIZE));
            } else { /* credit meet the whole 2d size */
                (*wr_num) = (uint32_t)(align_fixed_width_wr_num + (remain_height - 1) * align_width_wr_num);
                *fixedVal = in->async_2d_in.height * in->async_2d_in.width;
            }
        }
    } else if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_NOP) {
        *wr_num = (credit >= in->async_nop_in.nopCnt) ? (uint32_t)in->async_nop_in.nopCnt : credit;
        *fixedVal = *wr_num;
    }
}

/* Build segs array for BATCH type. Iterate elements, pre-segment, accumulate into segs array.
 * If partial_fixed_size is NULL: stop at the first element that exceeds max_segs (no partial).
 * If partial_fixed_size is not NULL: take partial segs up to max_segs limit.
 * *fixedVal: fully completed element count.
 * *partial_fixed_size: bytes in partial element (0 if none). */
static int trs_async_batch_build_segs(struct trs_async_devid *async_devid, struct trs_async_dma_input_para *in,
                                      struct trs_async_seg_info *seg_info, uint64_t *fixedVal,
                                      uint64_t *partial_fixed_size)
{
    uint32_t elem_dir = in->dir, i, cnt = 0;
    int ret = 0;

    *fixedVal = 0;
    if (partial_fixed_size != NULL) {
        *partial_fixed_size = 0;
    }

    for (i = 0; i < in->async_batch_in.count; i++) {
        struct trs_async_seg in_seg = {0};
        uint32_t tmp_cnt = 0, j;

        in_seg.src = in->async_batch_in.src[i];
        in_seg.dst = in->async_batch_in.dst[i];
        in_seg.len = in->async_batch_in.len[i];
        in_seg.dir = elem_dir;
        if (i != 0) {
            struct trs_async_addr_pack elem = {.src = in_seg.src, .dst = in_seg.dst, .len = in_seg.len};
            ret = trs_get_check_element_devid_dir(async_devid->local_devid, in, &elem, async_devid, &elem_dir);
            if (ret != 0) {
                return ret;
            }
            in_seg.dir = elem_dir;
        }

        ret = trs_async_pre_segment(async_devid->local_devid, &in_seg, &seg_info->segs[cnt], &tmp_cnt,
                                    seg_info->max_segs - cnt);
        if (ret != 0) {
            return ret;
        }

        if (cnt + tmp_cnt > seg_info->max_segs) {
            if (partial_fixed_size != NULL) {
                tmp_cnt = seg_info->max_segs - cnt;
                for (j = 0; j < tmp_cnt; j++) {
                    *partial_fixed_size += seg_info->segs[cnt + j].len;
                    seg_info->segs[cnt + j].async_devid = *async_devid;
                }
                cnt += tmp_cnt;
            }
            break;
        }

        for (j = 0; j < tmp_cnt; j++) {
            seg_info->segs[cnt + j].async_devid = *async_devid;
        }
        cnt += tmp_cnt;
        (*fixedVal)++;

        if (cnt >= seg_info->max_segs) {
            break;
        }
    }

    seg_info->seg_cnt = cnt;
    return 0;
}

/* Build segs array for 2D type. Iterate rows, pre-segment, accumulate into segs array.
 * Takes partial segs up to max_segs limit.
 * *fixedVal: total copied bytes. */
static int trs_async_2d_build_segs(struct trs_async_devid *async_devid, struct trs_async_dma_input_para *in,
                                   struct trs_async_seg_info *seg_info, uint64_t *fixedVal)
{
    uint64_t copied = in->async_2d_in.fixedSize;
    uint32_t start_row = (uint32_t)(copied / in->async_2d_in.width);
    uint64_t row_offset = copied % in->async_2d_in.width;
    uint32_t cnt = 0;
    uint32_t i;
    int ret = 0;

    for (i = start_row; i < in->async_2d_in.height; i++) {
        uint64_t row_start = (i == start_row) ? row_offset : 0;
        uint64_t row_len = in->async_2d_in.width - row_start;
        struct trs_async_seg in_seg = {0};
        uint32_t tmp_cnt = 0;
        uint32_t j;

        in_seg.src = (uint64_t)(uintptr_t)in->async_2d_in.src + i * in->async_2d_in.spitch + row_start;
        in_seg.dst = (uint64_t)(uintptr_t)in->async_2d_in.dst + i * in->async_2d_in.dpitch + row_start;
        in_seg.len = row_len;
        in_seg.dir = in->dir;

        ret = trs_async_pre_segment(async_devid->local_devid, &in_seg, &seg_info->segs[cnt], &tmp_cnt,
                                    seg_info->max_segs - cnt);
        if (ret != 0) {
            return ret;
        }

        if (cnt + tmp_cnt > seg_info->max_segs) {
            tmp_cnt = seg_info->max_segs - cnt;
        }
        for (j = cnt; j < cnt + tmp_cnt; j++) {
            copied += seg_info->segs[j].len;
            seg_info->segs[j].async_devid = *async_devid;
        }
        cnt += tmp_cnt;

        if (cnt >= seg_info->max_segs) {
            break;
        }
    }

    *fixedVal = copied;
    seg_info->seg_cnt = cnt;
    return 0;
}

/* Build pre-segmented array from input for online (non-direct-wqe) path.
 * Each segment is <= 256MB with consistent token_id.
 * Truncates to seg_info->max_segs limit. Computes fixedVal for output. */
static int trs_async_build_segs(struct trs_async_devid *async_devid, struct trs_async_dma_input_para *in,
                                struct trs_async_seg_info *seg_info, uint64_t *fixedVal)
{
    int ret = 0;

    *fixedVal = 0;

    if ((in->async_dma_type == TRS_ASYNC_DMA_TYPE_NORMAL) || (in->async_dma_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE)) {
        struct trs_async_seg in_seg = {0};
        in_seg.src = (uint64_t)(uintptr_t)in->async_normal_in.src;
        in_seg.dst = (uint64_t)(uintptr_t)in->async_normal_in.dst;
        in_seg.len = in->async_normal_in.len;
        in_seg.dir = in->dir;
        ret = trs_async_pre_segment(async_devid->local_devid, &in_seg, seg_info->segs, &seg_info->seg_cnt,
                                    seg_info->max_segs);
        if (ret != 0) {
            return ret;
        }
    } else if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) {
        ret = trs_async_batch_build_segs(async_devid, in, seg_info, fixedVal, NULL);
        if (ret != 0) {
            return ret;
        }
    } else if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) {
        ret = trs_async_2d_build_segs(async_devid, in, seg_info, fixedVal);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/* Non-direct WQE path: pre-segment addresses and fill remote WQEs.
 * NOP type skips pre-segmentation; other types are pre-segmented for token_id consistency. */
static int trs_fill_async_dma_nondirect_wqe(struct trs_async_devid *async_devid, struct trs_urma_ctx *urma_ctx,
                                            struct trs_async_ctx *async_ctx, struct trs_async_dma_input_para *in,
                                            uint32_t credit, uint32_t *wr_num, uint64_t *fixedVal)
{
    int ret = 0;

    if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_NOP) {
        /* NOP does not involve address copy; use original logic without pre-segmentation. */
        trs_get_async_post_wr_num(in, credit, false, wr_num, fixedVal, NULL);
        if (*wr_num == 0) {
            return 0;
        }
        async_ctx->pi = ((async_ctx->pi + *wr_num) % TRS_UB_PI_CI_DEPTH);
        ret = trs_remote_fill_async_dma_wqe(async_devid, urma_ctx, NULL, 0, in);
    } else {
        /* Pre-segment host addresses for token_id consistency. */
        struct trs_async_seg_info seg_info = {0};
        uint32_t max_segs = trs_async_get_max_segs(in);

        seg_info.max_segs = (max_segs < credit) ? max_segs : credit;
        seg_info.segs = (struct trs_async_seg *)malloc(sizeof(struct trs_async_seg) * seg_info.max_segs);
        if (seg_info.segs == NULL) {
            trs_err("Malloc segs array failed. (size=%zu)\n", sizeof(struct trs_async_seg) * seg_info.max_segs);
            return DRV_ERROR_OUT_OF_MEMORY;
        }

        ret = trs_async_build_segs(async_devid, in, &seg_info, fixedVal);
        if (ret != 0) {
            free(seg_info.segs);
            return ret;
        }
        *wr_num = seg_info.seg_cnt;
        if (*wr_num == 0) {
            free(seg_info.segs);
            return 0;
        }
        async_ctx->pi = ((async_ctx->pi + *wr_num) % TRS_UB_PI_CI_DEPTH);
        ret = trs_remote_fill_async_dma_wqe(async_devid, urma_ctx, seg_info.segs, seg_info.seg_cnt, in);
        free(seg_info.segs);
    }

    return ret;
}

static int trs_fill_async_dma_wqe(struct trs_async_devid *async_devid, struct sqcq_usr_info *sq_info,
                                  struct trs_async_dma_wqe_info *wqe_info, struct trs_async_dma_input_para *in,
                                  struct halAsyncDmaOutputPara *out)
{
    struct trs_urma_ctx *urma_ctx = sq_info->urma_ctx;
    bool is_direct_wqe = trs_is_async_direct_wqe(sq_info->flag, in->async_dma_type);
    bool is_wqe_sink = trs_is_async_jetty_wqe_sink(sq_info->flag);
    struct trs_async_ctx *async_ctx = trs_get_async_ctx(urma_ctx, in->dir, in->async_dma_type, async_devid->d2d_index);
    uint32_t credit = trs_get_async_jetty_credit(async_ctx, is_direct_wqe);
    uint64_t fixedVal = 0;
    uint32_t wr_num = 0;
    uint32_t last_pi = async_ctx->pi;
    int ret = 0;

    if (is_direct_wqe == true) {
        /* Direct WQE path: use original arithmetic splitting. */
        trs_get_async_post_wr_num(in, credit, false, &wr_num, &fixedVal, NULL);
        if (credit < wr_num) {
            trs_err("Jetty Credit not enough. (pi=%u; ci=%u; credit=%u; wr_num=%u)\n", async_ctx->pi, async_ctx->ci,
                    credit, wr_num);
            return DRV_ERROR_NO_RESOURCES;
        }
        if (wr_num == 0) {
            goto exit;
        }
        async_ctx->pi = ((async_ctx->pi + wr_num) % TRS_UB_PI_CI_DEPTH);
        ret = trs_fill_async_dma_direct_wqe(async_devid, urma_ctx, async_ctx, wqe_info, in);
    } else {
        /* Non-direct WQE path: pre-segment addresses and fill remote WQEs. */
        ret = trs_fill_async_dma_nondirect_wqe(async_devid, urma_ctx, async_ctx, in, credit, &wr_num, &fixedVal);
    }

    if (ret != 0) {
        async_ctx->pi = last_pi;
        return ret;
    }

exit:
    out->jettyId = async_ctx->src_jetty_id.id;
    out->pi = (is_wqe_sink == false) ? async_ctx->pi : wr_num; /* absolute in single task, relative in task sinking */
    out->functionId = async_ctx->func_id;
    out->dieId = async_ctx->die_id;
    out->fixedSize = fixedVal;
    return 0;
}

static inline uint64_t trs_get_sq_bind_remote_que_addr(uint32_t dev_id, uint32_t ts_id, struct sqcq_usr_info *sq_info)
{
    if (((struct trs_urma_ctx *)sq_info->urma_ctx)->remote_sq_que_addr != 0) {
        return ((struct trs_urma_ctx *)sq_info->urma_ctx)->remote_sq_que_addr;
    }

    if (sq_info->switch_stream_flag) {
        struct res_id_usr_info *stream_usr_info = trs_get_res_id_info(dev_id, ts_id, DRV_STREAM_ID, sq_info->stream_id);
        if ((stream_usr_info != NULL) && (stream_usr_info->valid != 0)) {
            return stream_usr_info->res_addr;
        }
    }

    return (uintptr_t)NULL;
}

static int trs_fill_h2d_async_dma_wqe(struct trs_async_devid *async_devid, struct sqcq_usr_info *sq_info,
                                      struct trs_async_dma_wqe_info *wqe_info, struct trs_async_dma_input_para *in,
                                      struct halAsyncDmaOutputPara *out)
{
    uint32_t sqid = in->async_normal_in.info.sq_id;
    uint32_t sqe_pos = 0;
    int ret;

    if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE) {
        struct sqcq_usr_info *update_sq_info = NULL;
        uint64_t remote_sq_que_addr = 0;

        update_sq_info = trs_get_sq_info(async_devid->local_devid, in->tsId, in->type, sqid);
        if ((update_sq_info == NULL) || (update_sq_info->urma_ctx == NULL)) {
            trs_err("Invalid para. (devid=%u; tsId=%u; type=%d; sqid=%u)\n", async_devid->local_devid, in->tsId,
                    in->type, sqid);
            return DRV_ERROR_INVALID_VALUE;
        }

        sqe_pos = in->async_normal_in.info.sqe_pos;
        if ((sqe_pos >= update_sq_info->depth) ||
            (in->async_normal_in.len > (update_sq_info->depth - sqe_pos) * update_sq_info->e_size)) {
            trs_err("Invalid para. (sqid=%u; sqe_pos=%u; len=%u; depth=%u, sqe_size=%u)\n", sqid, sqe_pos,
                    in->async_normal_in.len, update_sq_info->depth, update_sq_info->e_size);
            return DRV_ERROR_PARA_ERROR;
        }

        remote_sq_que_addr = trs_get_sq_bind_remote_que_addr(async_devid->local_devid, 0, update_sq_info);
        if (remote_sq_que_addr == 0) {
            trs_err("Sq queue not inited. (devid=%u; type=%d; sqid=%u)\n", async_devid->local_devid, in->type, sqid);
            return DRV_ERROR_PARA_ERROR;
        }

        in->async_normal_in.dst = (u8 *)(uintptr_t)remote_sq_que_addr + sqe_pos * update_sq_info->e_size;
        trs_debug("(devid=%u; sqid=%u; sqe_pos=%u; sqe_size=%u; len=%u; remote_sq_que_addr=0x%llx; dst=0x%llx)\n",
                  async_devid->local_devid, sqid, sqe_pos, update_sq_info->e_size, in->async_normal_in.len,
                  remote_sq_que_addr, (u64)(uintptr_t)in->async_normal_in.dst);
    }

    ret = trs_fill_async_dma_wqe(async_devid, sq_info, wqe_info, in, out);
    if (in->async_dma_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE) {
        in->async_normal_in.info.sq_id = sqid;
        in->async_normal_in.info.sqe_pos = sqe_pos;
    }

    return ret;
}

static int trs_fill_d2h_async_dma_wqe(struct trs_async_devid *async_devid, struct sqcq_usr_info *sq_info,
                                      struct trs_async_dma_wqe_info *wqe_info, struct trs_async_dma_input_para *in,
                                      struct halAsyncDmaOutputPara *out)
{
    return trs_fill_async_dma_wqe(async_devid, sq_info, wqe_info, in, out);
}

static int trs_alloc_device_jetty(uint32_t dev_id, struct trs_d2d_send_info *alloc_info,
                                  struct trs_d2d_sync_info *sync_msg)
{
    struct event_reply reply;
    int ret, result;

    reply.buf_len = sizeof(struct trs_d2d_sync_info) + sizeof(int);
    reply.buf = (char *)malloc(reply.buf_len);
    if (reply.buf == NULL) {
        trs_err("Malloc reply buffer failed. (size=%u)\n", reply.buf_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = trs_local_mem_event_sync(dev_id, alloc_info, sizeof(struct trs_d2d_send_info),
                                   DRV_SUBEVENT_TRS_CREATE_ASYNC_JETTY_MSG, &reply);
    result = DRV_EVENT_REPLY_BUFFER_RET(reply.buf);
    if ((ret != DRV_ERROR_NONE) || (result != 0)) {
        trs_err("Failed to sync event. (dev_id=%u; ret=%d; result=%d)\n", dev_id, ret, result);
        free(reply.buf);
        return (ret != DRV_ERROR_NONE) ? ret : result;
    }
    (void)memcpy_s(sync_msg, sizeof(struct trs_d2d_sync_info), DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply.buf),
                   sizeof(struct trs_d2d_sync_info));

    free(reply.buf);
    return 0;
}

static int trs_free_device_jetty(uint32_t dev_id, struct trs_d2d_send_info *free_info)
{
    struct event_reply reply;
    int result, ret;

    reply.buf_len = sizeof(result);
    reply.buf = (char *)&result;
    ret = trs_local_mem_event_sync(dev_id, free_info, sizeof(struct trs_d2d_send_info),
                                   DRV_SUBEVENT_TRS_DESTROY_ASYNC_JETTY_MSG, &reply);
    if ((ret != 0) || (result != 0)) {
        trs_err("Failed to sync destroy async jetty. (ret=%d; result=%d; dev_id=%u)\n", ret, result, dev_id);
        return (ret != 0) ? ret : result;
    }

    return 0;
}

static int trs_import_remote_d2d_jetty(uint32_t dev_id, uint32_t remote_dev_id, trs_d2d_index_t d2d_index,
                                       struct trs_d2d_dev_ctx *d2d_dev_ctx)
{
    struct trs_d2d_sync_info sync_msg = {0};
    struct trs_import_jetty_info info = {0};
    struct event_reply reply;
    int result, ret;

    (void)memcpy_s(info.jfr_raw, URMA_EID_SIZE, d2d_dev_ctx->dst_jfr_id.eid.raw, URMA_EID_SIZE);
    info.jfr_uasid = d2d_dev_ctx->dst_jfr_id.uasid;
    info.jfr_id = d2d_dev_ctx->dst_jfr_id.id;
    info.token_val = d2d_dev_ctx->token_value;
    info.recv_dev_id = remote_dev_id;
    info.d2d_index = d2d_index;
    reply.buf_len = sizeof(struct trs_d2d_sync_info) + sizeof(int);
    reply.buf = (char *)malloc(reply.buf_len);
    if (reply.buf == NULL) {
        trs_err("Malloc reply buffer failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = trs_local_mem_event_sync(dev_id, &info, sizeof(struct trs_import_jetty_info),
                                   DRV_SUBEVENT_TRS_IMPORT_ASYNC_JETTY_MSG, &reply);
    result = DRV_EVENT_REPLY_BUFFER_RET(reply.buf);
    if ((ret != 0) || (result != 0)) {
        free(reply.buf);
        trs_err("Failed to sync import remote jetty. (ret=%d; result=%d; dev_id=%u)\n", ret, result, dev_id);
        return (ret != 0) ? ret : result;
    }

    (void)memcpy_s(&sync_msg, sizeof(struct trs_d2d_sync_info), DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply.buf),
                   sizeof(struct trs_d2d_sync_info));
    d2d_dev_ctx->tpn[dev_id] = sync_msg.tpn;
    d2d_dev_ctx->tpn_valid[dev_id] = true;

    free(reply.buf);
    return 0;
}

static int trs_create_local_d2d_jetty(struct trs_async_devid *d2d_async_devid, uint32_t sq_id, uint32_t flag,
                                      bool is_batch_2d, struct trs_async_ctx *async_ctx)
{
    struct trs_d2d_sync_info sync_msg;
    struct trs_d2d_send_info alloc_info = {0};
    int ret;
    alloc_info.flag = flag;
    alloc_info.pos = TRS_ASYNC_SEND_SIDE;
    alloc_info.sq_id = sq_id;
    alloc_info.is_batch_2d = is_batch_2d;
    alloc_info.d2d_index = d2d_async_devid->d2d_index;
    ret = trs_alloc_device_jetty(d2d_async_devid->local_devid, &alloc_info, &sync_msg);
    if (ret != 0) {
        trs_err("Failed to alloc device jetty. (local_devid=%u; sq_id=%u)\n", d2d_async_devid->local_devid, sq_id);
        return ret;
    }
    async_ctx->src_jetty_id = sync_msg.jetty_id;
    async_ctx->func_id = sync_msg.func_id;
    async_ctx->die_id = sync_msg.die_id;
    return 0;
}

static int trs_destroy_local_d2d_jetty(uint32_t local_devid, trs_d2d_index_t d2d_idx, uint32_t sq_id, bool is_batch_2d)
{
    struct trs_d2d_send_info free_info = {0};
    free_info.pos = TRS_ASYNC_SEND_SIDE;
    free_info.sq_id = sq_id;
    free_info.is_batch_2d = is_batch_2d;
    free_info.d2d_index = d2d_idx;
    return trs_free_device_jetty(local_devid, &free_info);
}

static int trs_create_remote_d2d_jetty(uint32_t remote_devid, trs_d2d_index_t d2d_index, uint32_t flag,
                                       struct trs_d2d_dev_ctx *d2d_dev_ctx)
{
    struct trs_d2d_sync_info sync_msg;
    struct trs_d2d_send_info alloc_info = {0};
    int ret;

    alloc_info.flag = flag;
    alloc_info.pos = TRS_ASYNC_RECV_SIDE;
    alloc_info.d2d_index = d2d_index;
    ret = trs_alloc_device_jetty(remote_devid, &alloc_info, &sync_msg);
    if (ret != 0) {
        trs_err("Failed to alloc remote jetty. (ret=%d; remote_devid=%u; flag=0x%x)\n", ret, remote_devid, flag);
        return ret;
    }

    d2d_dev_ctx->dst_jfr_id = sync_msg.jfr_id;
    d2d_dev_ctx->token_value = sync_msg.token_value;

    trs_debug("Create jetty success. (remote_devid=%u; jfr_id=%u; flag=0x%x)\n", remote_devid,
              d2d_dev_ctx->dst_jfr_id.id, flag);
    return 0;
}

int trs_destroy_remote_d2d_jetty(uint32_t dev_id)
{
    struct trs_d2d_dev_ctx *d2d_dev_ctx = NULL;
    trs_d2d_index_t idx;
    uint32_t i;

    trs_dev_ctx_mutex_lock(dev_id);
    for (idx = TRS_D2D_IN_BOARD_INDEX; idx < TRS_MAX_D2D_INDEX; idx++) {
        d2d_dev_ctx = (struct trs_d2d_dev_ctx *)trs_get_d2d_dev_ctx(dev_id, idx);
        if (d2d_dev_ctx != NULL) {
            /* remote jetty use d2d proc_ctx->jfr, would destroy by device close */
            trs_set_d2d_dev_ctx(dev_id, idx, NULL);
            free(d2d_dev_ctx);
        }
    }
    trs_dev_ctx_mutex_un_lock(dev_id);

    for (i = 0; i < TRS_DEV_NUM; i++) {
        if (trs_is_dev_open(i) == false) {
            continue;
        }
        trs_dev_ctx_mutex_lock(i);
        for (idx = TRS_D2D_IN_BOARD_INDEX; idx < TRS_MAX_D2D_INDEX; idx++) {
            d2d_dev_ctx = (struct trs_d2d_dev_ctx *)trs_get_d2d_dev_ctx(i, idx);
            if (d2d_dev_ctx != NULL) {
                d2d_dev_ctx->tpn_valid[dev_id] = false;
            }
        }
        trs_dev_ctx_mutex_un_lock(i);
    }

    return 0;
}

static int trs_d2d_dev_ctx_check_init(struct trs_async_devid *d2d_async_devid, uint32_t flag)
{
    struct trs_d2d_dev_ctx *d2d_dev_ctx = NULL;
    uint32_t remote_devid = d2d_async_devid->remote_devid;
    uint32_t local_devid = d2d_async_devid->local_devid;
    trs_d2d_index_t d2d_idx = d2d_async_devid->d2d_index;
    int ret = 0;

    trs_dev_ctx_mutex_lock(remote_devid);

    d2d_dev_ctx = (struct trs_d2d_dev_ctx *)trs_get_d2d_dev_ctx(remote_devid, d2d_idx);
    if (d2d_dev_ctx == NULL) {
        d2d_dev_ctx = (struct trs_d2d_dev_ctx *)calloc(1, sizeof(struct trs_d2d_dev_ctx));
        if (d2d_dev_ctx == NULL) {
            trs_err("Alloc d2d_dev_ctx failed. (remote_devid=%u; d2d_idx=%u)\n", remote_devid, d2d_idx);
            ret = DRV_ERROR_OUT_OF_MEMORY;
            goto exit;
        }

        ret = trs_create_remote_d2d_jetty(remote_devid, d2d_async_devid->d2d_index, flag, d2d_dev_ctx);
        if (ret != 0) {
            trs_err("Failed to create remote jetty. (remote_devid=%u; d2d_idx=%u; ret=%d; flag=0x%x)\n", remote_devid,
                    d2d_idx, ret, flag);
            free(d2d_dev_ctx);
            goto exit;
        }

        trs_set_d2d_dev_ctx(remote_devid, d2d_idx, (void *)d2d_dev_ctx);
        trs_debug("Create remote jetty success. (remote_devid=%u; d2d_idx=%u; flag=0x%x; jetty_id=%u)\n", remote_devid,
                  d2d_idx, flag, d2d_dev_ctx->dst_jfr_id.id);
    }

    if (d2d_dev_ctx->tpn_valid[local_devid] == false) {
        ret = trs_import_remote_d2d_jetty(local_devid, remote_devid, d2d_async_devid->d2d_index, d2d_dev_ctx);
        if (ret != 0) {
            goto exit; /* destroy remote jetty when close device */
        }
    }

exit:
    (void)trs_dev_ctx_mutex_un_lock(remote_devid);
    return ret;
}

static int trs_d2d_async_ctx_create(struct trs_urma_ctx *urma_ctx, struct trs_async_devid *d2d_async_devid,
                                    struct trs_async_dma_input_para *in, uint32_t flag)
{
    struct trs_async_ctx *async_ctx = NULL;
    uint32_t local_devid = d2d_async_devid->local_devid;
    trs_d2d_index_t d2d_idx = d2d_async_devid->d2d_index;
    int ret, create_local_flag = 0;
    bool is_batch_2d = false;

    if ((in->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) || (in->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH)) {
        async_ctx = &urma_ctx->d2d_batch_2d_async_ctx[d2d_idx];
        is_batch_2d = true;
    } else {
        async_ctx = &urma_ctx->d2d_async_ctx[d2d_idx];
        is_batch_2d = false;
    }

    (void)pthread_mutex_lock(&urma_ctx->ctx_mutex);
    if (!async_ctx->init_flag) {
        ret = trs_create_local_d2d_jetty(d2d_async_devid, in->sqId, flag, is_batch_2d, async_ctx);
        if (ret != 0) {
            (void)pthread_mutex_unlock(&urma_ctx->ctx_mutex);
            trs_err("Failed to create local jetty. (local_devid=%u; d2d_idx=%u; ret=%d; flag=0x%x)\n", local_devid,
                    d2d_idx, ret, flag);
            return ret;
        }
        async_ctx->init_flag = true;
        create_local_flag = 1;
        trs_debug("Create local jetty success. (devid=%u; d2d_idx=%u; flag=0x%x; sq_id=%u; jetty_id=%u; fun_id=%u; "
                  "die_id=%u)\n",
                  local_devid, d2d_idx, flag, in->sqId, async_ctx->src_jetty_id.id, async_ctx->func_id,
                  async_ctx->die_id);
    }
    (void)pthread_mutex_unlock(&urma_ctx->ctx_mutex);

    ret = trs_d2d_dev_ctx_check_init(d2d_async_devid, flag);
    if (ret != 0) {
        if (create_local_flag == 1) {
            (void)pthread_mutex_lock(&urma_ctx->ctx_mutex);
            (void)trs_destroy_local_d2d_jetty(local_devid, d2d_idx, in->sqId, is_batch_2d);
            async_ctx->init_flag = false;
            (void)pthread_mutex_unlock(&urma_ctx->ctx_mutex);
        }
        return ret;
    }

    return 0;
}

static int _trs_d2d_async_ctx_destroy(uint32_t dev_id, uint32_t sq_id, struct trs_urma_ctx *urma_ctx, bool is_batch_2d)
{
    struct trs_async_ctx *async_ctx = NULL;
    trs_d2d_index_t idx;
    int ret;

    for (idx = TRS_D2D_IN_BOARD_INDEX; idx < TRS_MAX_D2D_INDEX; idx++) {
        async_ctx = is_batch_2d ? (&urma_ctx->d2d_batch_2d_async_ctx[idx]) : (&urma_ctx->d2d_async_ctx[idx]);
        if (async_ctx->init_flag) {
            ret = trs_destroy_local_d2d_jetty(dev_id, idx, sq_id, is_batch_2d);
            if (ret != 0) {
                trs_err("Failed to destroy local jetty. (dev_id=%u; d2d_idx=%u; sq_id=%u; is_batch_2d=%d; ret=%d)\n",
                        dev_id, idx, sq_id, is_batch_2d, ret);
                return ret;
            }
            trs_debug("Destroy d2d jetty success. (dev_id=%u; d2d_idx=%u; sq_id=%u; is_batch_2d=%d)\n", dev_id, idx,
                      sq_id, is_batch_2d);
            async_ctx->init_flag = false;
        }
    }
    return 0;
}

static int trs_d2d_async_ctx_destroy(uint32_t dev_id, uint32_t sq_id, struct trs_urma_ctx *urma_ctx)
{
    int ret;

    (void)pthread_mutex_lock(&urma_ctx->ctx_mutex);
    ret = _trs_d2d_async_ctx_destroy(dev_id, sq_id, urma_ctx, false);
    if (ret != 0) {
        (void)pthread_mutex_unlock(&urma_ctx->ctx_mutex);
        return ret;
    }
    ret = _trs_d2d_async_ctx_destroy(dev_id, sq_id, urma_ctx, true);
    (void)pthread_mutex_unlock(&urma_ctx->ctx_mutex);
    return ret;
}

static int trs_fill_d2d_async_dma_wqe(struct trs_async_devid *async_devid, struct sqcq_usr_info *sq_info,
                                      struct trs_async_dma_wqe_info *wqe_info, struct trs_async_dma_input_para *in,
                                      struct halAsyncDmaOutputPara *out)
{
    wqe_info->remote_dev_id = async_devid->remote_devid;
    return trs_fill_async_dma_wqe(async_devid, sq_info, wqe_info, in, out);
}

static int (*const trs_async_dma_wqe_fill_handles[TRS_ASYNC_MAX_DIR])(struct trs_async_devid *async_devid,
                                                                      struct sqcq_usr_info *sq_info,
                                                                      struct trs_async_dma_wqe_info *wqe_info,
                                                                      struct trs_async_dma_input_para *in,
                                                                      struct halAsyncDmaOutputPara *out) = {
    [TRS_ASYNC_HOST_TO_DEVICE] = trs_fill_h2d_async_dma_wqe,
    [TRS_ASYNC_DEVICE_TO_HOST] = trs_fill_d2h_async_dma_wqe,
    [TRS_ASYNC_DEVICE_TO_DEVICE] = trs_fill_d2d_async_dma_wqe,
};

static int trs_h2d2h_async_ctx_init(uint32_t dev_id, struct trs_urma_ctx *urma_ctx, struct trs_async_dma_input_para *in)
{
    struct trs_async_ctx *async_ctx = NULL;
    uint32_t subevent_id;
    struct trs_async_jetty_init_para jetty_para = {0};
    uint32_t sq_id = in->sqId;
    struct event_reply reply;
    int ret, result;

    if ((in->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) || (in->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH)) {
        async_ctx = &urma_ctx->batch_2d_async_ctx;
        subevent_id = DRV_SUBEVENT_TRS_INIT_BATCH_2D_JETTY_MSG;
    } else {
        async_ctx = &urma_ctx->async_ctx;
        subevent_id = DRV_SUBEVENT_TRS_INIT_H2D2H_JETTY_MSG;
    }

    if (async_ctx->init_flag == true) {
        return 0;
    }

    reply.buf_len = sizeof(struct trs_async_jetty_init_para) + sizeof(int);
    reply.buf = (char *)malloc(reply.buf_len);
    if (reply.buf == NULL) {
        trs_err("Malloc reply buffer failed. (size=%u)\n", reply.buf_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = trs_local_mem_event_sync(dev_id, &sq_id, sizeof(uint32_t), subevent_id, &reply);
    result = DRV_EVENT_REPLY_BUFFER_RET(reply.buf);

    if ((ret != DRV_ERROR_NONE) || (result != 0)) {
        trs_err("Failed to sync event. (dev_id=%u; subevent_id=%u; ret=%d; result=%d)\n", dev_id, subevent_id, ret,
                result);
        free(reply.buf);
        return (ret != DRV_ERROR_NONE) ? ret : result;
    }

    (void)memcpy_s(&jetty_para, sizeof(struct trs_async_jetty_init_para), DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply.buf),
                   sizeof(struct trs_async_jetty_init_para));
    free(reply.buf);

    async_ctx->src_jetty_id = jetty_para.jetty_id;
    async_ctx->tpn = jetty_para.tpn;
    async_ctx->die_id = urma_ctx->die_id;
    async_ctx->func_id = urma_ctx->func_id;
    async_ctx->init_flag = true;
    return 0;
}

static void trs_h2d2h_async_ctx_uninit(struct trs_urma_ctx *urma_ctx)
{
    /* async jetty destroyed in sqcq remote free */
    urma_ctx->batch_2d_async_ctx.init_flag = false;
    urma_ctx->async_ctx.init_flag = false;
}

static int trs_async_init_async_ctx(struct trs_async_devid *async_devid, struct sqcq_usr_info *sq_info,
                                    struct trs_async_dma_input_para *in)
{
    struct trs_urma_ctx *urma_ctx = sq_info->urma_ctx;
    int ret = 0;

    if (in->dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        ret = trs_d2d_async_ctx_create(urma_ctx, async_devid, in, sq_info->flag);
        if (ret != 0) {
            return ret;
        }
    } else if ((in->dir == TRS_ASYNC_HOST_TO_DEVICE) || (in->dir == TRS_ASYNC_DEVICE_TO_HOST)) {
        ret = trs_h2d2h_async_ctx_init(async_devid->local_devid, urma_ctx, in);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/* all async ctx would be destroyed in sq free */
int trs_async_uninit_async_ctx(uint32_t dev_id, uint32_t sq_id, void *master_ctx)
{
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)master_ctx;

    trs_h2d2h_async_ctx_uninit(urma_ctx);

    return trs_d2d_async_ctx_destroy(dev_id, sq_id, urma_ctx);
}

drvError_t trs_async_dma_wqe_create(uint32_t dev_id, struct trs_async_dma_input_para *in,
                                    struct halAsyncDmaOutputPara *out)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct trs_async_dma_wqe_info *wqe_info = NULL;
    struct trs_async_devid async_devid = {0};
    int ret;

    ret = trs_get_check_async_devid_dir(dev_id, in, &async_devid, &in->dir);
    if (ret != 0) {
        return ret;
    }

    if ((in->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) && (in->dir == TRS_ASYNC_DEVICE_TO_DEVICE)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    sq_info = trs_get_sq_info(dev_id, in->tsId, in->type, in->sqId);
    if ((sq_info == NULL) || (sq_info->urma_ctx == NULL)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, in->tsId, in->type, in->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* d2d and 2d/batch async cpy need init when first submit */
    ret = trs_async_init_async_ctx(&async_devid, sq_info, in);
    if (ret != 0) {
        return ret;
    }

    wqe_info = (struct trs_async_dma_wqe_info *)calloc(1, sizeof(struct trs_async_dma_wqe_info));
    if (wqe_info == NULL) {
        trs_err("WqeInfo malloc failed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = trs_async_dma_wqe_fill_handles[in->dir](&async_devid, sq_info, wqe_info, in, out);
    if (ret != 0) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            trs_err("Failed to fill async dma wqe. (ret=%d; dev_id=%u; sq_id=%u)\n", ret, dev_id, in->sqId);
        }
        free(wqe_info);
        return ret;
    }

    /* normal dma dwqe/2d d2d cpy need to store wqe_info when destroy */
    if ((trs_is_async_direct_wqe(sq_info->flag, in->async_dma_type) == true) ||
        ((in->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) && (in->dir == TRS_ASYNC_DEVICE_TO_DEVICE))) {
        wqe_info->dir = (uint8_t)in->dir;
        wqe_info->d2d_index = async_devid.d2d_index;
        out->size = STARS_ASYNC_DMA_WQE_SIZE;
        out->wqe = wqe_info->wqe;
    } else {
        out->size = 0;
        out->wqe = NULL;
        free(wqe_info);
    }

    trs_debug("Create async dma wqe. (dev_id=%u; dir=%u; sq_id=%u; dieid=%u; func_id=%u; jetty_id=%u)\n", dev_id,
              in->dir, in->sqId, out->dieId, out->functionId, out->jettyId);
    return DRV_ERROR_NONE;
}

static drvError_t trs_async_dma_wqe_normal_destory(uint32_t dev_id, struct trs_urma_ctx *urma_ctx,
                                                   struct halAsyncDmaDestoryPara *para)
{
    struct trs_async_dma_wqe_info *wqe_info = NULL;
    struct trs_async_ctx *async_ctx = NULL;

    if ((para->wqe == NULL) && (para->size == 0)) {
        return DRV_ERROR_NONE;
    }

    if ((para->wqe == NULL) || (para->size != STARS_ASYNC_DMA_WQE_SIZE)) {
        trs_err("Invalid para. (dev_id=%u; wqe=0x%llx; size=%u)\n", dev_id, (uint64_t)(uintptr_t)para->wqe, para->size);
        return DRV_ERROR_INVALID_VALUE;
    }

    wqe_info = (struct trs_async_dma_wqe_info *)(void *)para->wqe;
    if ((wqe_info->flag == 1) && (wqe_info->async_tseg != NULL)) {
        (void)urma_unregister_seg(wqe_info->async_tseg);
        trs_debug("Unregister seg. (dev_id=%u)\n", dev_id);
    }
    async_ctx = trs_get_async_ctx(urma_ctx, wqe_info->dir, TRS_ASYNC_DMA_TYPE_NORMAL, wqe_info->d2d_index);
    if (async_ctx == NULL) {
        trs_err("Not create. (dev_id=%u; sq_id=%u)\n", dev_id, para->sqId);
        return DRV_ERROR_UNINIT;
    }

    async_ctx->ci = (async_ctx->ci + 1) % trs_get_async_pi_ci_max(false);
    trs_debug("Async dma wqe destroy. (dev_id=%u; sq_id=%u; ci=%u; dir=%u)\n", dev_id, para->sqId, async_ctx->ci,
              wqe_info->dir);
    free(wqe_info);
    return 0;
}

static drvError_t trs_async_dma_wqe_batch_2d_destory(uint32_t dev_id, struct trs_urma_ctx *urma_ctx,
                                                     struct trs_async_dma_destroy_para *para)
{
    struct trs_async_ctx *async_ctx = NULL;
    drvAsyncDmaDir_t asyncDir;
    trs_d2d_index_t d2d_index = TRS_MAX_D2D_INDEX;

    if (para->async_batch_para->ci >= TRS_UB_PI_CI_DEPTH) {
        trs_err("The ci exceeds the max. (dev_id=%u; sq_id=%u; ci=%u; valid range=[0, %u))\n", dev_id, para->sqId,
                para->async_batch_para->ci, TRS_UB_PI_CI_DEPTH);
        return DRV_ERROR_PARA_ERROR;
    }

    if (para->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) {
        struct trs_async_dma_wqe_info *wqe_info = (struct trs_async_dma_wqe_info *)(void *)para->async_2d_para->wqe;
        if (wqe_info == NULL) {
            asyncDir = TRS_ASYNC_HOST_TO_DEVICE; /* NULL wqe default for h2d/d2h, for compatibility */
        } else {
            asyncDir = wqe_info->dir;
            d2d_index = wqe_info->d2d_index;
            free(wqe_info);
        }
    } else {
        asyncDir = TRS_ASYNC_HOST_TO_DEVICE;
    }
    /* batch/2d use same async_ctx */
    async_ctx = trs_get_async_ctx(urma_ctx, asyncDir, TRS_ASYNC_DMA_TYPE_BATCH, d2d_index);
    if (async_ctx == NULL) {
        trs_err("Uninit. (dev_id=%u; sq_id=%u; type=%d; dir=%d; d2d_index=%u)\n", dev_id, para->sqId,
                para->async_dma_type, asyncDir, d2d_index);
        return DRV_ERROR_UNINIT;
    }

    async_ctx->ci = para->async_batch_para->ci;
    trs_debug("Async dma wqe destroy. (dev_id=%u; sq_id=%u; ci=%u; dir=%u; d2d_index=%u)\n", dev_id, para->sqId,
              async_ctx->ci, asyncDir, d2d_index);

    return 0;
}

drvError_t trs_async_dma_wqe_destory(uint32_t dev_id, struct trs_async_dma_destroy_para *para)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct trs_urma_ctx *urma_ctx = NULL;
    int ret = 0;

    sq_info = trs_get_sq_info(dev_id, para->tsId, DRV_NORMAL_TYPE, para->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; sq_id=%u)\n", dev_id, para->tsId, para->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    if (para->async_dma_type == TRS_ASYNC_DMA_TYPE_NORMAL) {
        ret = trs_async_dma_wqe_normal_destory(dev_id, urma_ctx, para->normal_para);
    } else if ((para->async_dma_type == TRS_ASYNC_DMA_TYPE_2D) || (para->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH)) {
        ret = trs_async_dma_wqe_batch_2d_destory(dev_id, urma_ctx, para);
    } else {
        ret = DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

drvError_t trs_sq_jetty_info_query(uint32_t dev_id, struct halSqCqQueryInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct trs_async_ctx *async_ctx = NULL;
    struct trs_urma_ctx *urma_ctx = NULL;
    trs_d2d_index_t d2d_index;
    uint32_t dir;

    sq_info = trs_get_sq_info(dev_id, info->tsId, info->type, info->sqId);
    if ((sq_info == NULL) || (sq_info->urma_ctx == NULL)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, info->tsId, info->type, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((sq_info->flag & TSDRV_FLAG_TASK_SINK_SQ) == 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;

    if (info->prop == DRV_SQCQ_PROP_D2D_ASYNC_JETTY_INFO) {
        dir = TRS_ASYNC_DEVICE_TO_DEVICE;
        d2d_index = TRS_D2D_IN_BOARD_INDEX;
    } else if (info->prop == DRV_SQCQ_PROP_D2D_CROSS_BOARD_ASYNC_JETTY_INFO) {
        dir = TRS_ASYNC_DEVICE_TO_DEVICE;
        d2d_index = TRS_D2D_CROSS_BOARD_INDEX;
    } else {
        dir = TRS_ASYNC_HOST_TO_DEVICE;
        d2d_index = TRS_MAX_D2D_INDEX;
    }
    async_ctx = trs_get_async_ctx(urma_ctx, dir, TRS_ASYNC_DMA_TYPE_NORMAL, d2d_index);
    if (async_ctx == NULL) {
        trs_err("Failed to get async ctx. (dev_id=%u; sq_id=%u; prop=%d)\n", dev_id, info->sqId, info->prop);
        return DRV_ERROR_UNINIT;
    }

    info->value[0] = trs_get_async_pi_ci_max(true) - async_ctx->pi; /* 0: NOP number */
    info->value[1] = async_ctx->src_jetty_id.id;                    /* 1: jetty id */
    info->value[2] = async_ctx->func_id;                            /* 2: func id */
    info->value[3] = async_ctx->die_id;                             /* 3: die id*/
    trs_debug("Query jetty info. (devid=%u; sqid=%u; prop=%d; nop_num=%u; jetty_id=%u; func_id=%u; die_id=%u)\n",
              dev_id, info->sqId, info->prop, info->value[0], info->value[1], info->value[2], info->value[3]);

    return DRV_ERROR_NONE;
}

static int trs_async_dma_jetty_remote_alloc(uint32_t dev_id, struct halAsyncDmaJettyCreateIn *in,
                                            struct trs_async_jetty_info *jetty_info)
{
    struct trs_urma_proc_ctx *urma_proc_ctx = NULL;
    struct trs_remote_alloc_jetty_para jetty_para = {0};
    struct event_reply reply;
    drvError_t ret = 0;
    int result;

    reply.buf_len = sizeof(struct trs_async_jetty_info) + sizeof(int);
    reply.buf = (char *)malloc(reply.buf_len);
    if (reply.buf == NULL) {
        trs_err("Malloc reply buffer failed. (size=%u)\n", reply.buf_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    urma_proc_ctx = trs_get_urma_proc_ctx(dev_id);
    jetty_para.in = *in;
    jetty_para.jfr_id = urma_proc_ctx->trs_jfr.jfr->jfr_id;
    jetty_para.token = urma_proc_ctx->token;
    ret = trs_svm_mem_event_sync(dev_id, &jetty_para, sizeof(struct trs_remote_alloc_jetty_para),
                                 DRV_SUBEVENT_TRS_ALLOC_JETTY_MSG, &reply);
    result = DRV_EVENT_REPLY_BUFFER_RET(reply.buf);

    if ((ret != DRV_ERROR_NONE) || (result != 0)) {
        trs_err("Failed to sync event. (dev_id=%u; ret=%d; result=%d)\n", dev_id, ret, result);
        free(reply.buf);
        return (ret != DRV_ERROR_NONE) ? (int)ret : result;
    }

    (void)memcpy_s(jetty_info, sizeof(struct trs_async_jetty_info), DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply.buf),
                   sizeof(struct trs_async_jetty_info));
    free(reply.buf);

    return 0;
}

static int trs_async_dma_jetty_remote_free(uint32_t dev_id, struct trs_urma_stars_jetty *slave_jetty_addr)
{
    struct event_reply reply;
    drvError_t ret = 0;
    int result;

    reply.buf_len = sizeof(result);
    reply.buf = (char *)&result;

    ret = trs_local_mem_event_sync(dev_id, &slave_jetty_addr, sizeof(struct trs_urma_stars_jetty *),
                                   DRV_SUBEVENT_TRS_FREE_JETTY_MSG, &reply);
    if ((ret != DRV_ERROR_NONE) || (result != 0)) {
        trs_err("Failed to sync event. (dev_id=%u; ret=%d; result=%d)\n", dev_id, ret, result);
        return (ret != DRV_ERROR_NONE) ? (int)ret : result;
    }

    return 0;
}

static inline void trs_async_update_h2d2h_tpn(uint32_t dev_id, drvAsyncDmaJettyDir_t dir, uint32_t tpn)
{
    if (dir == TRS_ASYNC_JETTY_HOST_DEVICE) { /* get h2d d2h tjetty tpn */
        struct trs_urma_proc_ctx *urma_proc_ctx = trs_get_urma_proc_ctx(dev_id);
        if (urma_proc_ctx->tpn_valid == false) {
            urma_proc_ctx->tpn = tpn;
            urma_proc_ctx->tpn_valid = true;
        }
    }
}

static inline void trs_async_jetty_node_free(void *jetty_node)
{
    if (((struct trs_async_jetty_node *)jetty_node)->is_free_implicitly == true) {
        free(jetty_node);
    }
}

drvError_t trs_async_dma_jetty_create(uint32_t dev_id, struct halAsyncDmaJettyCreateIn *in,
                                      struct halAsyncDmaJettyCreateOut *out)
{
    struct trs_async_jetty_node *jetty_node = NULL;
    struct trs_async_jetty_info *jetty_info = NULL;
    int ret = 0;

    if (g_jetty_mng_ctx[dev_id].hash_table == NULL) {
        return DRV_ERROR_UNINIT;
    }

    jetty_node = (struct trs_async_jetty_node *)calloc(1, sizeof(struct trs_async_jetty_node));
    if (jetty_node == NULL) {
        trs_err("Failed to alloc jetty node. (dev_id=%u; size=%ld)\n", dev_id, sizeof(struct trs_async_jetty_node));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    jetty_info = &jetty_node->jetty_info;
    ret = trs_async_dma_jetty_remote_alloc(dev_id, in, jetty_info);
    if (ret != 0) {
        free(jetty_node);
        return ret;
    }

    ret = snprintf_s(jetty_node->jettyHandle.handle, TRS_ASYNC_DMA_JETTY_HANDLE_LEN, TRS_ASYNC_DMA_JETTY_HANDLE_LEN - 1,
                     "%08x%08x%08x", jetty_info->die_id, jetty_info->func_id, jetty_info->jetty_id);
    if (ret < 0) {
        trs_err("Failed to format jetty handle. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = DRV_ERROR_INNER_ERR;
        goto remote_free;
    }

    ret = trs_register_reg(dev_id, jetty_info->jetty_queue_base, jetty_info->jetty_queue_len);
    if (ret != 0) {
        goto remote_free;
    }

    (void)pthread_rwlock_wrlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
    ret = halHashTablePut2(g_jetty_mng_ctx[dev_id].hash_table, jetty_node->jettyHandle.handle, jetty_node,
                           trs_async_jetty_node_free);
    if (ret != 0) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to insert jetty node. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = DRV_ERROR_INNER_ERR;
        goto unregister_seg;
    }

    g_jetty_mng_ctx[dev_id].node_num++;
    out->jettyHandle = &jetty_node->jettyHandle;
    trs_async_update_h2d2h_tpn(dev_id, in->dir, jetty_info->tpn);
    (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
    return 0;

unregister_seg:
    trs_unregister_reg(dev_id, jetty_info->jetty_queue_base, jetty_info->jetty_queue_len);
remote_free:
    (void)trs_async_dma_jetty_remote_free(dev_id, jetty_info->slave_jetty_addr);
    free(jetty_node);
    return ret;
}

drvError_t trs_async_dma_jetty_destroy(uint32_t dev_id, struct halAsyncJettyDestroyPara *para)
{
    struct trs_async_jetty_node *jetty_node = NULL;
    drvError_t ret = 0;

    if (g_jetty_mng_ctx[dev_id].hash_table == NULL) {
        return DRV_ERROR_UNINIT;
    }

    (void)pthread_rwlock_wrlock(&g_jetty_mng_ctx[dev_id].rw_mutex);

    jetty_node = halHashTableGet(g_jetty_mng_ctx[dev_id].hash_table, para->jettyHandle->handle);
    if (jetty_node == NULL) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to find jetty node. (dev_id=%u; handle=%s)\n", dev_id, para->jettyHandle->handle);
        return DRV_ERROR_UNINIT;
    }

    g_jetty_mng_ctx[dev_id].node_num--;
    halHashTableRm(g_jetty_mng_ctx[dev_id].hash_table, para->jettyHandle->handle);

    (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);

    trs_unregister_reg(dev_id, jetty_node->jetty_info.jetty_queue_base, jetty_node->jetty_info.jetty_queue_len);
    ret = trs_async_dma_jetty_remote_free(dev_id, jetty_node->jetty_info.slave_jetty_addr);
    if (ret != 0) {
        trs_register_reg(dev_id, jetty_node->jetty_info.jetty_queue_base, jetty_node->jetty_info.jetty_queue_len);
        (void)pthread_rwlock_wrlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        (void)halHashTablePut2(g_jetty_mng_ctx[dev_id].hash_table, jetty_node->jettyHandle.handle, jetty_node,
                               trs_async_jetty_node_free);
        g_jetty_mng_ctx[dev_id].node_num++;
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        return ret;
    }

    free(jetty_node);
    return 0;
}

drvError_t trs_async_dma_jetty_query(uint32_t dev_id, struct halAsyncDmaJettyQueryIn *in,
                                     struct halAsyncDmaJettyQueryOut *out)
{
    struct trs_async_jetty_node *jetty_node = NULL;

    if (g_jetty_mng_ctx[dev_id].hash_table == NULL) {
        return DRV_ERROR_UNINIT;
    }

    (void)pthread_rwlock_rdlock(&g_jetty_mng_ctx[dev_id].rw_mutex);

    jetty_node = halHashTableGet(g_jetty_mng_ctx[dev_id].hash_table, in->jettyHandle->handle);
    if (jetty_node == NULL) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to find jetty node. (dev_id=%u; handle=%s)\n", dev_id, in->jettyHandle->handle);
        return DRV_ERROR_UNINIT;
    }

    out->dieId = jetty_node->jetty_info.die_id;
    out->funcId = jetty_node->jetty_info.func_id;
    out->jettyId = jetty_node->jetty_info.jetty_id;
    (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);

    return 0;
}

static int trs_async_dma_nop_wqe_convert(struct trs_async_dma_input_para *input, uint8_t *wqe_buffer,
                                         uint64_t wqe_bufferLen, struct halAsyncDmaWqeOutputPara *out)
{
    struct trs_urma_dwqe_fill_para fill_para = {0};
    uint32_t credit = (uint32_t)(wqe_bufferLen / TRS_URMA_DWQE_LEN);
    uint32_t i = 0;

    if (input->async_nop_in.nopCnt <= credit) {
        out->wqeCnt = (unsigned int)(input->async_nop_in.nopCnt);
    } else {
        out->wqeCnt = credit;
    }

    out->fixedSize = 0;
    out->fixedCnt = out->wqeCnt;

    fill_para.sqe_bb_idx = 1;
    fill_para.op_code = TRS_URMA_OPCODE_NOP;
    fill_para.rmt_eid = NULL;
    for (i = 0; i < out->wqeCnt; i++) {
        fill_para.cqe_report = (i == (out->wqeCnt - 1)) ? 1 : 0;
        _trs_fill_async_dma_direct_wqe(&fill_para, wqe_buffer + i * STARS_ASYNC_DMA_WQE_SIZE);
    }

    return 0;
}

static int _trs_async_dma_copy_wqe_convert(struct trs_async_devid *async_devid, uint32_t dir,
                                           struct trs_async_addr_pack *addr_pack, uint8_t *wqe_buffer,
                                           bool is_last_batch, uint32_t *wqe_num)
{
    struct trs_urma_dwqe_fill_para fill_para = {0};
    uint32_t devid = async_devid->local_devid;
    uint64_t send_addr, recv_addr;
    uint32_t i = 0;
    int ret = 0;

    trs_get_async_send_recv_addr(async_devid, addr_pack->src, addr_pack->dst, &send_addr, &recv_addr);

    fill_para.op_code = trs_get_async_dwqe_opcode(devid, async_devid, dir);
    fill_para.sqe_bb_idx = 1; /* in acl graph mode, sqe_bb_idx not used */

    if (dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        struct trs_d2d_dev_ctx *d2d_dev_ctx = NULL;
        trs_dev_ctx_mutex_lock(async_devid->remote_devid);
        d2d_dev_ctx = (struct trs_d2d_dev_ctx *)trs_get_d2d_dev_ctx(async_devid->remote_devid, async_devid->d2d_index);
        if (d2d_dev_ctx == NULL) {
            trs_dev_ctx_mutex_un_lock(async_devid->remote_devid);
            return DRV_ERROR_UNINIT;
        }

        fill_para.tpn = d2d_dev_ctx->tpn[devid];
        fill_para.rmt_eid = &(d2d_dev_ctx->dst_jfr_id.eid);
        trs_dev_ctx_mutex_un_lock(async_devid->remote_devid);
    } else {
        struct trs_urma_proc_ctx *urma_proc_ctx = trs_get_urma_proc_ctx(devid);
        fill_para.tpn = urma_proc_ctx->tpn;
        fill_para.rmt_eid = &(urma_proc_ctx->urma_ctx->eid);
    }

    ret = halMemGetSeg(devid, recv_addr, addr_pack->len, &fill_para.seg, &fill_para.token);
    if (ret != 0) {
        trs_err("Get token_id from svm failed. (devid=%u; va=0x%llx; len=0x%llx; ret=%d)\n", devid, recv_addr,
                addr_pack->len, ret);
        return ret;
    }

    *wqe_num = (uint32_t)(trs_align_up_power2(addr_pack->len, TRS_URMA_WR_CPY_MAX_SIZE) / TRS_URMA_WR_CPY_MAX_SIZE);
    for (i = 0; i < *wqe_num; i++) {
        fill_para.send_addr = send_addr + i * TRS_URMA_WR_CPY_MAX_SIZE;
        fill_para.recv_addr = recv_addr + i * TRS_URMA_WR_CPY_MAX_SIZE;
        fill_para.len = (i == (*wqe_num - 1)) ? (addr_pack->len - i * TRS_URMA_WR_CPY_MAX_SIZE) :
                                                TRS_URMA_WR_CPY_MAX_SIZE;
        fill_para.cqe_report = ((i == (*wqe_num - 1)) && is_last_batch) ? 1 : 0;
        _trs_fill_async_dma_direct_wqe(&fill_para, wqe_buffer + i * STARS_ASYNC_DMA_WQE_SIZE);
    }

    return 0;
}

static inline bool trs_is_async_wqe_convert_complete(struct trs_async_dma_input_para *input, uint64_t fixedVal)
{
    switch (input->async_dma_type) {
        case TRS_ASYNC_DMA_TYPE_NORMAL:
            return (fixedVal == input->async_normal_in.len);
        case TRS_ASYNC_DMA_TYPE_2D:
            return (fixedVal == (input->async_2d_in.width * input->async_2d_in.height));
        case TRS_ASYNC_DMA_TYPE_NOP:
            return (fixedVal == input->async_nop_in.nopCnt);
        default:
            return false;
    }
}

/* NORMAL type: user has already done token_id segmentation and 256MB splitting.
 * Only fill one WQE per call here. */
static int trs_async_wqe_convert_normal(struct trs_async_devid *async_devid, struct trs_async_dma_input_para *input,
                                        uint32_t credit, uint8_t *dst_wqe_addr, uint32_t *wr_num, uint64_t *fixedVal)
{
    struct trs_async_addr_pack addr_pack = {0};
    int ret;

    addr_pack.src = (uint64_t)(uintptr_t)input->async_normal_in.src;
    addr_pack.dst = (uint64_t)(uintptr_t)input->async_normal_in.dst;
    addr_pack.len = input->async_normal_in.len;

    /* Cap length to credit * 256MB to avoid WQE buffer overflow. */
    if (addr_pack.len > (uint64_t)credit * TRS_URMA_WR_CPY_MAX_SIZE) {
        addr_pack.len = (uint64_t)credit * TRS_URMA_WR_CPY_MAX_SIZE;
    }

    ret = _trs_async_dma_copy_wqe_convert(async_devid, input->dir, &addr_pack, dst_wqe_addr, false, wr_num);
    if (ret != 0) {
        return ret;
    }
    *fixedVal = addr_pack.len;
    return 0;
}

/* BATCH and 2D type: pre-segment addresses then fill WQEs up to credit limit. */
static int trs_async_wqe_convert_batch_2d(struct trs_async_devid *async_devid, struct trs_async_dma_input_para *input,
                                          uint32_t credit, uint8_t *dst_wqe_addr, uint32_t *wr_num, uint64_t *fixedVal,
                                          uint64_t *partial_fixed_size)
{
    struct trs_async_seg_info seg_info = {0};
    uint32_t max_segs = trs_async_get_max_segs(input);
    uint32_t j;
    int ret;

    seg_info.max_segs = (max_segs < credit) ? max_segs : credit;
    seg_info.segs = (struct trs_async_seg *)malloc(sizeof(struct trs_async_seg) * seg_info.max_segs);
    if (seg_info.segs == NULL) {
        trs_err("Malloc segs array failed. (size=%zu)\n", sizeof(struct trs_async_seg) * seg_info.max_segs);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (input->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) {
        ret = trs_async_batch_build_segs(async_devid, input, &seg_info, fixedVal, partial_fixed_size);
    } else {
        ret = trs_async_2d_build_segs(async_devid, input, &seg_info, fixedVal);
    }
    if (ret != 0) {
        free(seg_info.segs);
        return ret;
    }

    for (j = 0; j < seg_info.seg_cnt; j++) {
        struct trs_async_addr_pack addr_pack = {0};
        uint32_t seg_wqe_num = 0;

        addr_pack.src = seg_info.segs[j].src;
        addr_pack.dst = seg_info.segs[j].dst;
        addr_pack.len = seg_info.segs[j].len;

        ret = _trs_async_dma_copy_wqe_convert(&seg_info.segs[j].async_devid, seg_info.segs[j].dir, &addr_pack,
                                              dst_wqe_addr, (j == seg_info.seg_cnt - 1), &seg_wqe_num);
        if (ret != 0) {
            free(seg_info.segs);
            return ret;
        }
        dst_wqe_addr += seg_wqe_num * STARS_ASYNC_DMA_WQE_SIZE;
        *wr_num += seg_wqe_num;
    }
    free(seg_info.segs);
    return 0;
}

static int trs_async_dma_copy_wqe_convert(struct trs_async_devid *async_devid, struct trs_async_dma_input_para *input,
                                          uint8_t *wqe_buffer, uint64_t wqe_bufferLen,
                                          struct halAsyncDmaWqeOutputPara *out)
{
    uint8_t *dst_wqe_addr = wqe_buffer;
    uint32_t credit = (uint32_t)(wqe_bufferLen / TRS_URMA_DWQE_LEN);
    uint32_t wr_num = 0;
    uint64_t fixedVal = 0;
    uint64_t partial_fixed_size = 0;
    int ret = 0;

    if (input->async_dma_type == TRS_ASYNC_DMA_TYPE_NORMAL) {
        ret = trs_async_wqe_convert_normal(async_devid, input, credit, dst_wqe_addr, &wr_num, &fixedVal);
        if (ret != 0) {
            return ret;
        }
    } else if ((input->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) ||
               (input->async_dma_type == TRS_ASYNC_DMA_TYPE_2D)) {
        ret = trs_async_wqe_convert_batch_2d(async_devid, input, credit, dst_wqe_addr, &wr_num, &fixedVal,
                                             &partial_fixed_size);
        if (ret != 0) {
            return ret;
        }
    }

    /* Set CQE on the last WQE to signal completion of this batch. */
    if (wr_num > 0) {
        struct udma_sqe_ctl_tmp *last_sqe = (struct udma_sqe_ctl_tmp *)(wqe_buffer +
                                                                        (wr_num - 1) * STARS_ASYNC_DMA_WQE_SIZE);
        last_sqe->cqe = 1;
    }

    out->wqeCnt = wr_num;
    if (input->async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) {
        out->fixedCnt = fixedVal;
        out->fixedSize = partial_fixed_size;
    } else {
        out->fixedCnt = trs_is_async_wqe_convert_complete(input, fixedVal) ? 1 : 0;
        out->fixedSize = (out->fixedCnt == 1) ? 0 : fixedVal;
    }
    trs_debug("Convert success. (async_dma_type=%u; wqeCnt=%u; fixedCnt=%llu; fixedSize=%llu)\n", input->async_dma_type,
              out->wqeCnt, out->fixedCnt, out->fixedSize);
    return 0;
}

drvError_t trs_async_dma_wqe_convert(uint32_t dev_id, struct halAsyncDmaWqeInputPara *in,
                                     struct halAsyncDmaWqeOutputPara *out)
{
    struct trs_async_dma_input_para input = {0};
    struct trs_async_devid async_devid = {0};
    int ret = 0;

    input.async_dma_type = (enum trs_async_dma_type)in->wqeType;
    if (input.async_dma_type == TRS_ASYNC_DMA_TYPE_NORMAL) {
        input.async_normal_in = in->normal;
    } else if (input.async_dma_type == TRS_ASYNC_DMA_TYPE_2D) {
        input.async_2d_in = in->matrix2d;
    } else if (input.async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) {
        input.async_batch_in = in->batch;
    } else if (input.async_dma_type == TRS_ASYNC_DMA_TYPE_NOP) {
        input.async_nop_in = in->nop;
    }

    if (input.async_dma_type == TRS_ASYNC_DMA_TYPE_NOP) {
        return trs_async_dma_nop_wqe_convert(&input, in->wqeBuffer, in->wqeBufferLen, out);
    }

    ret = trs_get_check_async_devid_dir(dev_id, &input, &async_devid, &input.dir);
    if (ret != 0) {
        return ret;
    }

    if (input.dir == TRS_ASYNC_DEVICE_TO_DEVICE) {
        ret = trs_d2d_dev_ctx_check_init(&async_devid, 0);
        if (ret != 0) {
            return ret;
        }
    }

    return trs_async_dma_copy_wqe_convert(&async_devid, &input, in->wqeBuffer, in->wqeBufferLen, out);
}

drvError_t trs_async_dma_jetty_fill(uint32_t dev_id, struct halAsyncDmaJettyFillInfo *para)
{
    struct trs_async_jetty_node *jetty_node = NULL;
    drvError_t ret = 0;

    if (g_jetty_mng_ctx[dev_id].hash_table == NULL) {
        return DRV_ERROR_UNINIT;
    }

    (void)pthread_rwlock_rdlock(&g_jetty_mng_ctx[dev_id].rw_mutex);

    jetty_node = halHashTableGet(g_jetty_mng_ctx[dev_id].hash_table, para->jettyHandle->handle);
    if (jetty_node == NULL) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to find jetty node. (dev_id=%u; handle=%s)\n", dev_id, para->jettyHandle->handle);
        return DRV_ERROR_UNINIT;
    }

    if ((para->offset + para->size) > jetty_node->jetty_info.jetty_queue_len) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to fill jetty queue. (dev_id=%u; offset=0x%llx; size=0x%llx; len=0x%x; ret=%d)\n", dev_id,
                para->offset, para->size, jetty_node->jetty_info.jetty_queue_len, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halSvmAccess(dev_id, jetty_node->jetty_info.jetty_queue_base + para->offset,
                       (uint64_t)(uintptr_t)para->srcWqe, para->size, SVM_MEM_ACCESS_WRITE);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to fill jetty queue. (dev_id=%u; addr=0x%llx; offset=0x%llx; src=%px; len=0x%llx; ret=%d)\n",
                dev_id, jetty_node->jetty_info.jetty_queue_base, para->offset, para->srcWqe, para->size, ret);
        return ret;
    }

    (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
    return 0;
}

int trs_async_urma_init(uint32_t dev_id)
{
    int ret;

    if (g_jetty_mng_ctx[dev_id].hash_table != NULL) {
        return DRV_ERROR_REPEATED_INIT;
    }

    ret = pthread_rwlock_init(&g_jetty_mng_ctx[dev_id].rw_mutex, NULL);
    if (ret != 0) {
        trs_err("Failed to init jetty mng rwlock. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    g_jetty_mng_ctx[dev_id].hash_table = halHashTableNew();
    if (g_jetty_mng_ctx[dev_id].hash_table == NULL) {
        (void)pthread_rwlock_destroy(&g_jetty_mng_ctx[dev_id].rw_mutex);
        return DRV_ERROR_INNER_ERR;
    }

    g_jetty_mng_ctx[dev_id].node_num = 0;
    return 0;
}

void trs_async_urma_uninit(uint32_t dev_id)
{
    struct trs_async_jetty_node **node_buff = NULL;
    uint32_t node_num;
    uint32_t i;

    if (g_jetty_mng_ctx[dev_id].hash_table == NULL) {
        return;
    }

    (void)pthread_rwlock_wrlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
    node_num = g_jetty_mng_ctx[dev_id].node_num;
    if (node_num == 0) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        goto out;
    }

    node_buff = (struct trs_async_jetty_node **)malloc(sizeof(void *) * node_num);
    if (node_buff == NULL) {
        (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);
        trs_err("Failed to alloc node buffer. (dev_id=%u; node_num=%u)\n", dev_id, node_num);
        goto out;
    }

    node_num = get_hash_table_value(g_jetty_mng_ctx[dev_id].hash_table, (void **)node_buff, (unsigned short)node_num);
    for (i = 0; i < node_num; i++) {
        struct trs_async_jetty_node *jetty_node = node_buff[i];
        trs_unregister_reg(dev_id, jetty_node->jetty_info.jetty_queue_base, jetty_node->jetty_info.jetty_queue_len);
        (void)trs_async_dma_jetty_remote_free(dev_id, jetty_node->jetty_info.slave_jetty_addr);
        jetty_node->is_free_implicitly = true;
        halHashTableRm(g_jetty_mng_ctx[dev_id].hash_table, jetty_node->jettyHandle.handle);
    }

    g_jetty_mng_ctx[dev_id].node_num = 0;
    free(node_buff);
    (void)pthread_rwlock_unlock(&g_jetty_mng_ctx[dev_id].rw_mutex);

out:
    halHashTableDelete(g_jetty_mng_ctx[dev_id].hash_table);
    g_jetty_mng_ctx[dev_id].hash_table = NULL;
    (void)pthread_rwlock_destroy(&g_jetty_mng_ctx[dev_id].rw_mutex);
}
