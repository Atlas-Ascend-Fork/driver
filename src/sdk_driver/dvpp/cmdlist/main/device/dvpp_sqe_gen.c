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

#include "ka_type.h"
#include "dvpp_sqe_gen.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_vpu_decoder.h"
#include "dvpp_cmdlist.h"

static int32_t param_check(dvpp_cmdlist_param_t *param, void *sqe)
{
    uint32_t i = 0;
    if (sqe == NULL) {
        DVPP_CMDLIST_LOG_ERROR("sqe is null.\n");
        return -1;
    }

    if (param == NULL) {
        DVPP_CMDLIST_LOG_ERROR("input param is null.\n");
        return -1;
    }

    if ((param->cmdbuf_uva == 0) || (param->cmdbuf_kva == 0) || (param->cmdbuf_size == 0)) {
        DVPP_CMDLIST_LOG_ERROR("cmdbuf addr or size illegal.\n");
        return -1;
    }

    if ((param->vpu_config == NULL) || (param->node_cnt_of_batch == NULL)) {
        DVPP_CMDLIST_LOG_ERROR("vpu_config info is null.\n");
        return -1;
    }

    if (param->node_cnt > DVPP_MAX_NODE_NUM) {
        DVPP_CMDLIST_LOG_ERROR("node_cnt cannot exceed %u, node_cnt: %u\n", DVPP_MAX_NODE_NUM, param->node_cnt);
        return -1;
    }

    if ((param->batch_cnt < DVPP_MIN_BATCH_NUM) || (param->batch_cnt > DVPP_MAX_BATCH_NUM)) {
        DVPP_CMDLIST_LOG_ERROR("batch_cnt should be in [%u, %u], batch_cnt: %u\n", DVPP_MIN_BATCH_NUM,
                               DVPP_MAX_BATCH_NUM, param->batch_cnt);
        return -1;
    }

    for (i = 0; i < param->batch_cnt; i++) {
        if (param->node_cnt_of_batch[i] > DVPP_MAX_NODE_NUM) {
            DVPP_CMDLIST_LOG_ERROR("node_cnt_of_batch[%u] cannot exceed %u, node_cnt_of_batch: %u\n", i,
                                   DVPP_MAX_NODE_NUM, param->node_cnt_of_batch[i]);
            return -1;
        }
    }
    DVPP_CMDLIST_LOG_DEBUG("node_cnt:%u, batch_cnt:%u\n", param->node_cnt, param->batch_cnt);
    DVPP_CMDLIST_LOG_DEBUG("cmdbuf_kva:%pK, cmdbuf_uva:%pK, cmdbuf_size:%u\n", (void *)(param->cmdbuf_kva),
                           (void *)(param->cmdbuf_uva), param->cmdbuf_size);

    return 0;
}

int32_t dvpp_gen_vpc_sqe(dvpp_cmdlist_param_t *param, void *sqe)
{
    int32_t ret = 0;
    dvpp_decoder dec;
    dvpp_vpu_decoder vpu_dec;
    struct CmdBuf cmd_buf;

    ret = param_check(param, sqe);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("param check fail.\n");
        return -1;
    }
    vpu_dec.vpu_config = param->vpu_config;
    vpu_dec.node_cnt = param->node_cnt;
    vpu_dec.batch_cnt = param->batch_cnt;
    vpu_dec.node_cnt_of_batch = param->node_cnt_of_batch;
    vpu_dec.mod = DVPP_MOD_TYPE_VPC;

    dec.decoder = (void *)&vpu_dec;
    dec.protocol = DVPP_PROTOCOL_VPU;

    ret = dvpp_decode_args(&dec);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("decode vpu args fail.\n");
        return -1;
    }

    dvpp_init_cmdbuf(param->cmdbuf_kva, param->cmdbuf_uva, param->cmdbuf_size, &cmd_buf);
    ret = dvpp_gen_cmdlist(&dec, &cmd_buf);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp generate cmdlist fail.\n");
        return -1;
    }

    (void)dvpp_gen_sqe(&dec, &cmd_buf, (struct dvpp_sqe *)sqe);

#ifdef BUILD_DEBUG
    dvpp_dump_cmdbuf(&cmd_buf);
    dvpp_dump_sqe((struct dvpp_sqe *)sqe);
    dvpp_dump_sqe_list(&cmd_buf);
#endif

    return 0;
}