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
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_fs_pub.h"

#include "pbl_ubmm.h"
#include "framework_vma.h"
#include "svm_gup.h"
#include "svm_slab.h"
#include "svm_kern_log.h"
#include "svm_pub.h"
#include "svm_gfp.h"
#include "pmq.h"
#include "ubmm_ctx.h"
#include "ubmm_core.h"

static inline u32 ubmm_get_bkt(u64 va)
{
    return ka_hash_min((u32)(va >> 10), UBMM_NODE_HASH_BIT); /*  ignore lower 10 bit of va */
}

static inline void ubmm_node_add(struct ubmm_ctx *ctx, struct ubmm_node *node)
{
    u32 bkt = ubmm_get_bkt(node->va);
    ka_hash_add(ctx->htable, &node->link, bkt);
}

static inline void ubmm_node_del(struct ubmm_node *node)
{
    ka_hash_del(&node->link);
}

static inline struct ubmm_node *ubmm_node_find(struct ubmm_ctx *ctx, u64 va, u64 size)
{
    u32 bkt = ubmm_get_bkt(va);
    struct ubmm_node *node = NULL;

    ka_hash_for_each_possible(ctx->htable, node, link, bkt)
    {
        if ((node->va == va) && (node->size == size)) {
            return node;
        }
    }
    return NULL;
}

static int ubmm_map_host_pa_get(struct ubmm_node *node, int tgid, u64 va, u64 size, struct svm_pa_seg pa_seg[],
                                u64 *seg_num)
{
    u64 page_num = size / KA_MM_PAGE_SIZE;
    ka_page_t **pages = NULL;
    bool is_remap_addr;
    u64 i;
    int ret;

    pages = (ka_page_t **)svm_kvmalloc(page_num * sizeof(ka_page_t *), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pages == NULL) {
        svm_err("Alloc pages failed. (pages_num=%llu)\n", page_num);
        return -ENOMEM;
    }

    ret = svm_get_user_pages(tgid, va, page_num, (void **)pages, &is_remap_addr);
    if (ret != 0) {
        svm_kvfree(pages);
        svm_err("Get user pages failed. (ret=%d; va=0x%llx; size=%llu)\n", ret, va, size);
        return ret;
    }

    for (i = 0; i < page_num; i++) {
        pa_seg[i].pa = ka_mm_page_to_phys(pages[i]);
        pa_seg[i].size = KA_MM_PAGE_SIZE;
    }

    *seg_num = svm_make_pa_continues(pa_seg, page_num);
    node->pages = pages;
    node->page_num = page_num;
    node->is_remap_addr = is_remap_addr;
    return 0;
}

static void ubmm_map_host_pa_put(struct ubmm_node *node)
{
    if (node->pages != NULL) {
        svm_put_user_pages((void **)node->pages, node->page_num, node->is_remap_addr);
        svm_kvfree(node->pages);
        node->pages = NULL;
    }
}

static int ubmm_map_pa_get(struct ubmm_node *node, int tgid, u64 va, u64 size, struct svm_pa_seg pa_seg[], u64 *seg_num)
{
    return (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) ?
               ubmm_map_host_pa_get(node, tgid, va, size, pa_seg, seg_num) :
               svm_pmq_pa_query(tgid, va, size, pa_seg, seg_num);
}

static void ubmm_map_pa_put(struct ubmm_node *node)
{
    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        ubmm_map_host_pa_put(node);
    }
}

void ubmm_node_recycle(struct ubmm_ctx *ctx)
{
    struct ubmm_node *node = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt, num = 0;

    ka_task_mutex_lock(&ctx->mutex);
    ka_hash_for_each_safe(ctx->htable, bkt, hnode, node, link)
    {
        ubmm_node_del(node);
        (void)ubmm_unmap(ctx->udevid, node->uba, node->size);
        ubmm_map_pa_put(node);
        (void)ubmm_free_uba(node->raw_uba, node->raw_uba_size);
        svm_kvfree(node);
        num++;
    }
    ka_task_mutex_unlock(&ctx->mutex);

    if (num > 0) {
        svm_warn("Recycle uba. (udevid=%u; tgid=%d; num=%u)\n", ctx->udevid, ctx->tgid, num);
    }
}

void ubmm_node_show(struct ubmm_ctx *ctx, ka_seq_file_t *seq)
{
    struct ubmm_node *node = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt;
    int i = 0;

    ka_task_mutex_lock(&ctx->mutex);
    ka_hash_for_each_safe(ctx->htable, bkt, hnode, node, link)
    {
        if (i == 0) {
            ka_fs_seq_printf(seq, "ubmm:   index     ref     va    size   uba\n");
        }
        ka_fs_seq_printf(seq, "   %d   %d     %llx     %llx     %llx \n", i++, node->ref, node->va, node->size,
                         node->uba);
        if (i >= 128) { /* show max 128 item */
            break;
        }
    }
    ka_task_mutex_unlock(&ctx->mutex);
}

static struct ubmm_node *ubmm_create_node(struct ubmm_ctx *ctx, u64 va, u64 size)
{
    struct ubmm_node *node = NULL;

    node = (struct ubmm_node *)svm_kvzalloc(sizeof(*node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d; va=0x%llx)\n", ctx->udevid, ctx->tgid, va);
        return NULL;
    }

    node->ref = 1;
    node->va = va;
    node->size = size;
    ubmm_node_add(ctx, node);

    return node;
}

static void ubmm_destroy_node(struct ubmm_node *node)
{
    ubmm_node_del(node);
    svm_kvfree(node);
}

static int ubmm_svm_map(u32 udevid, u64 uba, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    struct ubmm_pa_seg *ubmm_pa_seg = NULL;
    int i = 0;
    int ret;

    ubmm_pa_seg = svm_kvmalloc(sizeof(struct ubmm_pa_seg) * seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ubmm_pa_seg == NULL) {
        svm_err("Map kvmalloc ubmm_pa_seg failed. (alloc_seg_num=%llu)\n", seg_num);
        return -ENOMEM;
    }
    for (i = 0; i < seg_num; i++) {
        ubmm_pa_seg[i].pa = pa_seg[i].pa;
        ubmm_pa_seg[i].size = pa_seg[i].size;
    }

    ret = ubmm_map(udevid, uba, size, ubmm_pa_seg, seg_num);
    svm_kvfree(ubmm_pa_seg);

    return ret;
}

static int ubmm_map_uba(struct ubmm_ctx *ctx, u64 va, u64 size, u64 *uba)
{
    u64 aligned_va = svm_align_down(va, KA_MM_PAGE_SIZE);
    u64 aligned_size = svm_align_up(size, KA_MM_PAGE_SIZE);
    struct ubmm_node *node = NULL;
    struct svm_pa_seg *pa_seg = NULL;
    u64 prefer_align = KA_HPAGE_SIZE;
    u64 page_size, prefer_align_offset;
    u64 seg_num = 0;
    int ret;

    node = ubmm_node_find(ctx, aligned_va, aligned_size);
    if (node != NULL) {
        *uba = node->uba;
        node->ref += 1;
        return 0;
    }

    seg_num = svm_get_align_up_num(va, size, KA_MM_PAGE_SIZE);
    pa_seg = svm_kvmalloc(sizeof(struct svm_pa_seg) * seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        svm_err("Map kvmalloc pa_seg failed. (alloc_seg_num=%llu)\n", seg_num);
        return -ENOMEM;
    }

    node = ubmm_create_node(ctx, aligned_va, aligned_size);
    if (node == NULL) {
        svm_kvfree(pa_seg);
        return -ENOMEM;
    }

    ret = ubmm_map_pa_get(node, ctx->tgid, aligned_va, aligned_size, pa_seg, &seg_num);
    if ((ret != 0) || (aligned_size != svm_get_pa_size(pa_seg, seg_num))) {
        if (ret == 0) {
            ubmm_map_pa_put(node);
        }
        ubmm_destroy_node(node);
        svm_kvfree(pa_seg);
        svm_err("Pa query failed. (tgid=%d; va=0x%llx; size=0x%llx)\n", ctx->tgid, aligned_va, aligned_size);
        return -EINVAL;
    }

    /* Host side we need hpage_aligned uba preferably to dev hpage_table for performance of va not aligned scene.
       Dev side we use real page_table, consider the memory floor noiser caused by the size of uba. */
    if (dbl_get_deployment_mode() != DBL_HOST_DEPLOYMENT) {
        page_size = svm_get_max_page_size_by_pa_seg(pa_seg, seg_num);
        prefer_align = (page_size <= KA_MM_PAGE_SIZE) ?
                           KA_MM_PAGE_SIZE :
                           KA_HPAGE_SIZE; /* only support normal,huge page size align for uba alloc */
    }

    prefer_align_offset = aligned_va - svm_align_down(aligned_va, prefer_align);
    node->raw_uba_size = aligned_size + prefer_align_offset;
    ret = ubmm_alloc_uba(node->raw_uba_size, prefer_align, &node->raw_uba);
    if (ret != 0) {
        ubmm_map_pa_put(node);
        ubmm_destroy_node(node);
        svm_kvfree(pa_seg);
        svm_err("Alloc uba failed. (ret=%d; tgid=%d; va=0x%llx; size=0x%llx)\n", ret, ctx->tgid, va, size);
        return ret;
    }

    *uba = (SVM_IS_ALIGNED(node->raw_uba, prefer_align)) ? (node->raw_uba + prefer_align_offset) : node->raw_uba;

    ret = ubmm_svm_map(ctx->udevid, *uba, aligned_size, pa_seg, seg_num);
    svm_kvfree(pa_seg);
    if (ret != 0) {
        ubmm_free_uba(node->raw_uba, node->raw_uba_size);
        ubmm_map_pa_put(node);
        ubmm_destroy_node(node);
        svm_err("Map uba failed. (tgid=%d; va=0x%llx; size=0x%llx)\n", ctx->tgid, va, size);
        return ret;
    }

    node->uba = *uba;
    return 0;
}

static int _ubmm_remap_uba(struct ubmm_ctx *ctx, struct ubmm_node *node)
{
    struct svm_pa_seg *pa_seg = NULL;
    u64 seg_num;
    int ret;

    seg_num = svm_get_align_up_num(node->va, node->size, KA_MM_PAGE_SIZE);
    pa_seg = svm_kvmalloc(sizeof(struct svm_pa_seg) * seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        svm_err("Map kvmalloc pa_seg failed. (alloc_seg_num=%llu)\n", seg_num);
        return -ENOMEM;
    }

    ret = ubmm_map_pa_get(node, ctx->tgid, node->va, node->size, pa_seg, &seg_num);
    if ((ret != 0) || (node->size != svm_get_pa_size(pa_seg, seg_num))) {
        if (ret == 0) {
            ubmm_map_pa_put(node);
        }
        svm_kvfree(pa_seg);
        svm_err("Pmq pa query failed. (tgid=%d; va=0x%llx; size=0x%llx)\n", ctx->tgid, node->va, node->size);
        return -EINVAL;
    }

    ret = ubmm_svm_map(ctx->udevid, node->uba, node->size, pa_seg, seg_num);
    svm_kvfree(pa_seg);
    if (ret != 0) {
        ubmm_map_pa_put(node);
        svm_err("Map uba failed. (tgid=%d; va=0x%llx; size=0x%llx)\n", ctx->tgid, node->va, node->size);
        return ret;
    }

    return 0;
}

static int _ubmm_for_each_node(struct ubmm_ctx *ctx, int (*func)(struct ubmm_node *node, void *priv), void *priv)
{
    struct ubmm_node *node = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt;
    int ret;

    ka_hash_for_each_safe(ctx->htable, bkt, hnode, node, link)
    {
        ret = func(node, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static bool ubmm_range_is_overlap(u64 va, u64 size, const struct ubmm_node *node)
{
    return (va < (node->va + node->size)) && (node->va < (va + size));
}

static int ubmm_remap_node(struct ubmm_ctx *ctx, struct ubmm_node *node)
{
    int ret;

    ret = ubmm_unmap(ctx->udevid, node->uba, node->size);
    if (ret != 0) {
        svm_err("Unmap failed. (udevid=%u; tgid=%d; va=0x%llx; uba=0x%llx)\n", ctx->udevid, ctx->tgid, node->va,
                node->uba);
        return ret;
    }

    ubmm_map_pa_put(node);

    ret = _ubmm_remap_uba(ctx, node);
    if (ret != 0) {
        (void)ubmm_free_uba(node->raw_uba, node->raw_uba_size);
        ubmm_destroy_node(node);
    }

    return ret;
}

struct ubmm_remap_priv {
    struct ubmm_ctx *ctx;
    u64 va;
    u64 size;
    int ret;
    bool found;
};

static int ubmm_remap_range_node(struct ubmm_node *node, void *priv)
{
    struct ubmm_remap_priv *remap_priv = (struct ubmm_remap_priv *)priv;
    u64 node_va;
    u64 node_size;
    int ret;

    if (!ubmm_range_is_overlap(remap_priv->va, remap_priv->size, node)) {
        return 0;
    }

    remap_priv->found = true;
    node_va = node->va;
    node_size = node->size;
    ret = ubmm_remap_node(remap_priv->ctx, node);
    if (ret != 0) {
        svm_err("Remap uba failed. (ret=%d; udevid=%u; tgid=%d; repair_va=0x%llx; repair_size=0x%llx; "
                "node_va=0x%llx; node_size=0x%llx)\n",
                ret, remap_priv->ctx->udevid, remap_priv->ctx->tgid, remap_priv->va, remap_priv->size, node_va,
                node_size);
        if (remap_priv->ret == 0) {
            remap_priv->ret = ret;
        }
    }

    return 0;
}

static int ubmm_remap_uba(struct ubmm_ctx *ctx, u64 va, u64 size)
{
    struct ubmm_remap_priv priv = {.ctx = ctx, .va = va, .size = size, .ret = 0, .found = false};
    int ret;

    ret = _ubmm_for_each_node(ctx, ubmm_remap_range_node, &priv);
    if (ret != 0) {
        return ret;
    }

    return priv.found ? priv.ret : -ENOENT;
}

int ubmm_do_map(u32 udevid, int tgid, u64 va, u64 size, u64 *uba)
{
    int ret;
    struct ubmm_ctx *ctx = ubmm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        ret = ubmm_init_try(udevid, tgid);
        if (ret != 0) {
            svm_err_if((ret != -ENOTSUPP), "Ubmm init retry failed. (udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n",
                       udevid, tgid, va, size);
            return ret;
        }

        ctx = ubmm_ctx_get(udevid, tgid);
        if (ctx == NULL) {
            svm_err("Invalid para. (udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n", udevid, tgid, va, size);
            return -EINVAL;
        }
    }

    ka_task_mutex_lock(&ctx->mutex);
    ret = ubmm_map_uba(ctx, va, size, uba);
    ka_task_mutex_unlock(&ctx->mutex);
    ubmm_ctx_put(ctx);

    if (ret == 0) {
        *uba += va - svm_align_down(va, KA_MM_PAGE_SIZE);
    }

    return ret;
}

static int ubmm_unmap_uba(struct ubmm_ctx *ctx, u64 va, u64 size)
{
    u64 aligned_va = svm_align_down(va, KA_MM_PAGE_SIZE);
    u64 aligned_size = svm_align_up(size, KA_MM_PAGE_SIZE);
    struct ubmm_node *node = NULL;
    int ret;

    node = ubmm_node_find(ctx, aligned_va, aligned_size);
    if (node == NULL) {
        svm_err("Invalid para. (udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n", ctx->udevid, ctx->tgid, va, size);
        return -EINVAL;
    }

    node->ref -= 1;
    if (node->ref > 0) {
        return 0;
    }

    ret = ubmm_unmap(ctx->udevid, node->uba, node->size);
    if (ret != 0) {
        svm_err("Unmap failed. (udevid=%u; tgid=%d; va=0x%llx; uba=0x%llx)\n", ctx->udevid, ctx->tgid, va, node->uba);
        node->ref += 1;
        return ret;
    }

    ubmm_map_pa_put(node);
    (void)ubmm_free_uba(node->raw_uba, node->raw_uba_size);
    ubmm_destroy_node(node);

    return 0;
}

int ubmm_do_unmap(u32 udevid, int tgid, u64 va, u64 size)
{
    int ret;
    struct ubmm_ctx *ctx = ubmm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid para. (udevid=%u; tgid=%d; va=0x%llx)\n", udevid, tgid, va);
        return -EINVAL;
    }

    ka_task_mutex_lock(&ctx->mutex);
    ret = ubmm_unmap_uba(ctx, va, size);
    ka_task_mutex_unlock(&ctx->mutex);
    ubmm_ctx_put(ctx);

    return ret;
}

int ubmm_do_remap(u32 udevid, int tgid, u64 va, u64 size)
{
    int ret;
    struct ubmm_ctx *ctx = ubmm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_debug("Invalid para. (udevid=%u; tgid=%d; va=0x%llx)\n", udevid, tgid, va);
        return -ESRCH;
    }

    ka_task_mutex_lock(&ctx->mutex);
    ret = ubmm_remap_uba(ctx, va, size);
    ka_task_mutex_unlock(&ctx->mutex);
    ubmm_ctx_put(ctx);

    return ret;
}

int ubmm_query_uba_base(u32 udevid, u64 *uba_base)
{
    u64 total_size, avail_size;
    return ubmm_get_uba_pool(uba_base, &total_size, &avail_size);
}
