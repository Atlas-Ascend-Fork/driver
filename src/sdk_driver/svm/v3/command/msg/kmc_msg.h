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

#ifndef KMC_MSG_H
#define KMC_MSG_H

#define SVM_KMC_P2P_MAX_EXTEND_NUM 32ULL /* pcie p2p msg_len is limit. */

enum svm_kmc_msg_id {
    SVM_KMC_MSG_QUERY_PA = 0U,
    SVM_KMC_MSG_QUERY_HOST_BAR,
    SVM_KMC_MSG_QUERY_DBI,
    SVM_KMC_MSG_UPDATE_DBI,
    SVM_KMC_MSG_INTI_PA_HANDLE,
    SVM_KMC_MSG_DMA_ADDR_GET,
    SVM_KMC_MSG_DMA_ADDR_PUT,
    SVM_KMC_MSG_UBMEM_MAP,
    SVM_KMC_MSG_UBMEM_UNMAP,
    SVM_KMC_MSG_MWL_OP,
    SVM_KMC_MSG_DMA_ADDR_QUERY,
    SVM_KMC_MSG_ID_MAX,
};

struct svm_kmc_msg_head {
    enum svm_kmc_msg_id msg_id;
    u64 extend_num;
    int ret;
};

#endif
