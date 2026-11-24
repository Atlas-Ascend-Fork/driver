/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ADP_MLX_INTERNAL_H
#define ADP_MLX_INTERNAL_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <infiniband/mlx5dv.h>
#include "ibv_extend.h"

/* Internal queue layout and metadata tags, not an installed application API. */
#define ADP_MLX_NDA_ABI 1U
#define ADP_MLX_META_MAGIC UINT64_C(0x4d4c58354e444131)
#define ADP_MLX_WQEBB_SIZE 64U
#define ADP_MLX_CQE_SIZE 64U
/* Minimum / single-SGE stride only; actual SRQ stride is exported per queue. */
#define ADP_MLX_SRQ_STRIDE 32U

/* Validated NPU/Host page granule. NDA V1 has no NPU page-size callback. */
#define ADP_MLX_NPU_PAGE_SIZE 4096U

struct mlx_resource;
/* Fixed provider implementation, not a dynamically negotiated feature table.
 * Every callback is provided. Public inputs are checked by mlx_create_* / query. */
struct mlx_backend {
    bool (*supported)(struct ibv_context *);
    int (*query_device)(struct ibv_context *, struct ibv_device_attr_extend *);
    struct ibv_pd *(*alloc_pd)(struct ibv_context *);
    struct ibv_td *(*alloc_td)(struct ibv_context *);
    struct ibv_pd *(*alloc_parent)(struct mlx_resource *, struct ibv_pd *);
    int (*dealloc_pd)(struct ibv_pd *);
    int (*dealloc_td)(struct ibv_td *);
    struct ibv_cq *(*create_cq)(struct mlx_resource *, struct ibv_cq_init_attr_ex *);
    struct ibv_qp *(*create_qp)(struct mlx_resource *, struct ibv_qp_init_attr *);
    struct ibv_srq *(*create_srq)(struct ibv_pd *, struct ibv_srq_init_attr *);
    int (*export_srq)(struct ibv_srq *, struct mlx5dv_srq *);
    int (*destroy_srq)(struct ibv_srq *);
    int (*export_cq)(struct ibv_cq *, struct mlx5dv_cq *);
    int (*export_qp)(struct ibv_qp *, struct mlx5dv_qp *);
    int (*destroy_cq)(struct ibv_cq *);
    int (*destroy_qp)(struct ibv_qp *);
};

struct mlx_allocation {
    struct mlx_allocation *next;
    void *raw;
    uintptr_t addr;
    size_t size;
    uint64_t type;
};

enum mlx_kind {
    MLX_CQ,
    MLX_QP,
    MLX_SRQ
};
struct mlx_resource {
    struct mlx_resource *next;
    enum mlx_kind kind;
    struct ibv_context *context; /* 借用的 verbs context，须存活到回收结束。 */
    const struct mlx_backend *backend;
    struct ibv_extend_ops ops;          /* 应用回调表的副本，回滚和重试仍会调用。 */
    struct mlx_allocation *allocations; /* 此资源尚未释放的队列及 DBR 内存。 */
    uint32_t srq_entries; /* SRQ 实际槽数；为队列长度对齐增加的槽位也加入 WQE 链。 */
    uint32_t srq_stride; /* 按请求 SGE 容量取整后的 WQE 字节数；分配前设置，供 H2D 建链使用。 */
    struct ibv_pd *parent;
    struct ibv_pd *base_pd; /* CQ 未传入 PD 时由 Provider 创建并负责释放。 */
    struct ibv_td *td;
    struct ibv_pd *app_pd; /* 借用的应用 PD，用于检查 QP 与 SRQ 是否同属一个 PD。 */

    /* QP 创建时取得的依赖引用；存在硬件 QP 时须在其销毁成功后才释放。 */
    struct mlx_resource *shared_rq;
    struct mlx_resource *send_cq;
    struct mlx_resource *recv_cq;
    /* 该 CQ/SRQ 被多少个 QP 引用；同一 QP 的收发共用一个 CQ 时只计一次。 */
    unsigned int refs;
    bool creating; /* 创建开始即登记，close 可观察尚未返回公共句柄的资源。 */
    bool busy;     /* 创建或 destroy 正在执行，禁止重入销毁。 */
    bool closing;  /* 已进入清理，禁止新 QP 绑定该 CQ/SRQ；清理失败时仍置位。 */
    bool orphan;   /* 创建失败且回滚未完成，调用方没有句柄，由 reap 按 context 重试。 */

    void *db_mapping; /* db_mmap 返回的原始基址，与 db_desc 配对用于 db_unmap。 */
    struct doorbell_map_desc db_desc;
    /* 创建成功后返回相应成员的地址；destroy 据此查找本记录。 */
    union {
        struct ibv_qp_extend qp;
        struct ibv_cq_extend cq;
        struct ibv_srq_extend srq;
    } out;
};

extern const struct mlx_backend mlx_real_backend;
/* Retry failed-create rollback while context/callbacks are alive. Preserved
 * exported symbol; applications normally use destroy/close for recovery. */
__attribute__((visibility("default"))) int adp_mlx_reap_resources(struct ibv_context *context);
int mlx_close_resources(struct ibv_context *);
struct ibv_cq *mlx_create_cq_nda(struct mlx_resource *, struct ibv_cq_init_attr_ex *);
struct ibv_srq *mlx_create_srq_nda(struct ibv_pd *, struct ibv_srq_init_attr *);
int mlx_destroy_srq_nda(struct ibv_srq *);
int mlx_error(int error);
void *mlx_alloc(struct ibv_pd *, void *, size_t, size_t, uint64_t);
void mlx_free(struct ibv_pd *, void *, void *, uint64_t);
bool mlx_contains(const struct mlx_resource *, const void *, size_t, uint64_t);
void mlx_release_memory(struct mlx_resource *);
struct ibv_qp_extend *mlx_create_qp(struct ibv_context *, struct ibv_qp_init_attr_extend *);
struct ibv_cq_extend *mlx_create_cq(struct ibv_context *, struct ibv_cq_init_attr_extend *);
struct ibv_srq_extend *mlx_create_srq(struct ibv_context *, struct ibv_srq_init_attr_extend *);
int mlx_destroy_srq(struct ibv_srq_extend *);
int mlx_destroy_qp(struct ibv_qp_extend *);
int mlx_destroy_cq(struct ibv_cq_extend *);
int mlx_query_device(struct ibv_context *, struct ibv_device_attr_extend *);
#endif
