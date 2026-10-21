/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SVM_IOCTL_EX_H
#define SVM_IOCTL_EX_H

#include "svm_pub.h"
#include "casm_ioctl.h"
#include "smm_ioctl.h"
#include "mpl_ioctl.h"
#include "async_copy_ioctl.h"
#include "dma_desc_ioctl.h"
#include "dma_map_ioctl.h"
#include "dbi_ioctl.h"
#include "share_repair_ioctl.h"
#include "mms_def.h"
#include "soma_ioctl.h"

#define SVM_CHAR_DEV_NAME "svm"
#define SVM_MMAP_CHAR_DEV_NAME "svm_mmap"

/* casm cmd */
#define SVM_CASM_CREATE_KEY _IOWR('U', 0, struct svm_casm_create_key_para)
#define SVM_CASM_DESTROY_KEY _IOW('U', 1, struct svm_casm_destroy_key_para)
#define SVM_CASM_OP_TASK _IOW('U', 2, struct svm_casm_op_task_para)
#define SVM_CASM_GET_SRC_VA _IOWR('U', 3, struct svm_casm_get_src_va_para)
#define SVM_CASM_MEM_PIN _IOW('U', 4, struct svm_casm_mem_pin_para)
#define SVM_CASM_MEM_UNPIN _IOW('U', 5, struct svm_casm_mem_unpin_para)
#define SVM_CASM_CS_QUERY_SRC _IOWR('U', 6, struct svm_casm_cs_query_src_para)
#define SVM_CASM_CS_SET_SRC _IOW('U', 7, struct svm_casm_cs_set_src_para)
#define SVM_CASM_CS_CLR_SRC _IOW('U', 8, struct svm_casm_cs_clr_src_para)

/* async copy cmd */
#define SVM_ASYNC_COPY_SUBMIT _IOWR('U', 10, struct svm_async_copy_submit_para)
#define SVM_ASYNC_COPY_SUBMIT_2D _IOWR('U', 11, struct svm_async_copy_submit_para)
#define SVM_ASYNC_COPY_SUBMIT_BATCH _IOWR('U', 12, struct svm_async_copy_submit_batch_para)
#define SVM_ASYNC_COPY_WAIT _IOW('U', 13, struct svm_async_copy_wait_para)

/* dma map cmd */
#define SVM_DMA_MAP _IOW('U', 15, struct svm_dma_map_para)
#define SVM_DMA_UNMAP _IOW('U', 16, struct svm_dma_unmap_para)

/* dma desc cmd */
#define SVM_DMA_DESC_CONVERT _IOWR('U', 20, struct svm_dma_desc_convert_para)
#define SVM_DMA_DESC_CONVERT_2D _IOWR('U', 21, struct svm_dma_desc_convert_2d_para)
#define SVM_DMA_DESC_SUBMIT _IOW('U', 22, struct svm_dma_desc_submit_para)
#define SVM_DMA_DESC_WAIT _IOW('U', 23, struct svm_dma_desc_wait_para)
#define SVM_DMA_DESC_DESTROY _IOW('U', 24, struct svm_dma_desc_destroy_para)

/* smm cmd */
#define SVM_SMM_MAP _IOWR('U', 25, struct svm_smm_map_para)
#define SVM_SMM_UNMAP _IOW('U', 26, struct svm_smm_unmap_para)

/* mpl cmd */
#define SVM_MPL_POPULATE _IOW('U', 30, struct svm_mpl_populate_para)
#define SVM_MPL_DEPOPULATE _IOW('U', 31, struct svm_mpl_depopulate_para)

/* mem show */
#define SVM_MEM_SHOW_FEATURE_ACK _IO('U', 32)

/* dbi query */
#define SVM_DBI_QUERY _IOWR('U', 33, struct svm_dbi_query_para)

/* mem madvise */
#define SVM_MEM_MADVISE _IOW('U', 34, struct svm_madvise_para)

/* mms */
#define SVM_MMS_STATS_MEM_CFG _IOW('U', 35, struct mms_stats)

/* soma cmd */
#define SVM_SOMA_MEM_CFG _IOWR('U', 36, struct svm_soma_mem_cfg_para)

/* share repair */
#define SVM_SHARE_REPAIR _IOW('U', 37, struct svm_share_repair_para)

#define SVM_MAX_CMD 38

#endif
