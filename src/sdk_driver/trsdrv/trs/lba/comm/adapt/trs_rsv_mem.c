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
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"

#include "pbl_kref_safe.h"

#include "securec.h"
#include "trs_rsv_mem.h"

#define MAX_RCV_MEM_TYPE 4U

static KA_TASK_DEFINE_RWLOCK(rsv_mem_lock);

struct trs_rsv_mem {
    struct trs_id_inst inst;
    struct trs_rsv_mem_attr attr;

    int type;
    ka_gen_pool_t *pool;
    struct kref_safe ref;
};

static struct trs_rsv_mem *g_rsv_mem[TRS_TS_INST_MAX_NUM][MAX_RCV_MEM_TYPE];

static struct trs_rsv_mem *trs_rsv_mem_create(struct trs_id_inst *inst, int type, struct trs_rsv_mem_attr *attr)
{
    struct trs_rsv_mem *rsv_mem = NULL;
    ka_gen_pool_t *pool = NULL;
    int ret;

    rsv_mem = trs_kzalloc(sizeof(struct trs_rsv_mem), KA_GFP_KERNEL);
    if (rsv_mem == NULL) {
        trs_err("Malloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid, sizeof(struct trs_rsv_mem));
        return NULL;
    }

    pool = ka_base_gen_pool_create(KA_MM_PAGE_SHIFT, KA_NUMA_NO_NODE);
    if (pool == NULL) {
        trs_kfree(rsv_mem);
        trs_err("Gen pool create fail. (devid=%u; tsid=%u; type=%d)\n", inst->devid, inst->tsid, type);
        return NULL;
    }

    ret = ka_base_gen_pool_add_virt(pool, (unsigned long)attr->vaddr, attr->paddr, attr->total_size, KA_NUMA_NO_NODE);
    if (ret != 0) {
        ka_base_gen_pool_destroy(pool);
        trs_kfree(rsv_mem);
        trs_err("Get pool add virt fail. (devid=%u; tsid=%u; type=%d; total_size=0x%lx)\n", inst->devid, inst->tsid,
                type, attr->total_size);
        return NULL;
    }

    rsv_mem->inst = *inst;
    rsv_mem->attr = *attr;
    rsv_mem->type = type;
    rsv_mem->pool = pool;
    kref_safe_init(&rsv_mem->ref);

    return rsv_mem;
}

static void trs_rsv_mem_destroy(struct trs_rsv_mem *rsv_mem)
{
    size_t avail, total;

    avail = ka_base_gen_pool_avail(rsv_mem->pool);
    total = ka_base_gen_pool_size(rsv_mem->pool);
    if (avail != total) {
        trs_warn("leak rsv mem. (avail_size=0x%lx; total_size=0x%lx)\n", avail, total);
    } else {
        ka_base_gen_pool_destroy(rsv_mem->pool);
    }
    trs_kfree(rsv_mem);
}

static int trs_rsv_mem_add(struct trs_id_inst *inst, int type, struct trs_rsv_mem *rsv_mem)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);

    ka_task_write_lock_bh(&rsv_mem_lock);
    if (g_rsv_mem[ts_inst][type] != NULL) {
        ka_task_write_unlock_bh(&rsv_mem_lock);
        return -ENODEV;
    }
    g_rsv_mem[ts_inst][type] = rsv_mem;
    ka_task_write_unlock_bh(&rsv_mem_lock);
    return 0;
}

static void trs_rsv_mem_release(struct kref_safe *kref)
{
    struct trs_rsv_mem *rsv_mem = ka_container_of(kref, struct trs_rsv_mem, ref);
    u32 ts_inst = trs_id_inst_to_ts_inst(&rsv_mem->inst);

    ka_task_write_lock_bh(&rsv_mem_lock);
    g_rsv_mem[ts_inst][rsv_mem->type] = NULL;
    ka_task_write_unlock_bh(&rsv_mem_lock);

    trs_info("Trs rsv mem uninit. (devid=%u; tsid=%u; type=%d; total_size=0x%lx)\n", rsv_mem->inst.devid,
             rsv_mem->inst.tsid, rsv_mem->type, rsv_mem->attr.total_size);
    ka_mm_iounmap(rsv_mem->attr.vaddr);
    trs_rsv_mem_destroy(rsv_mem);
}

static struct trs_rsv_mem *trs_rsv_mem_get(struct trs_id_inst *inst, int type)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_rsv_mem *rsv_mem = NULL;

    if (type >= (int)MAX_RCV_MEM_TYPE) {
        trs_err("Invalid rsv mem type. (type=%d)\n", type);
        return NULL;
    }
    ka_task_read_lock_bh(&rsv_mem_lock);
    rsv_mem = g_rsv_mem[ts_inst][type];
    if (rsv_mem != NULL) {
        if (kref_safe_get_unless_zero(&rsv_mem->ref) == 0) {
            ka_task_read_unlock_bh(&rsv_mem_lock);
            trs_err("rsv mem ref is zero. (type=%d)\n", type);
            return NULL;
        };
    }
    ka_task_read_unlock_bh(&rsv_mem_lock);
    return rsv_mem;
}

static void trs_rsv_mem_put(struct trs_rsv_mem *rsv_mem)
{
    kref_safe_put(&rsv_mem->ref, trs_rsv_mem_release);
}

static void trs_rsv_mem_clear(struct trs_rsv_mem *rsv_mem, void *vaddr, size_t size)
{
    if ((rsv_mem->attr.flag & TRS_RSV_MEM_FLAG_DEVICE) != 0) {
        ka_mm_memset_io(vaddr, 0, size);
    } else {
        (void)memset_s(vaddr, size, 0, size);
    }
}

static void trs_rsv_mem_del(struct trs_id_inst *inst, int type)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_rsv_mem *rsv_mem = NULL;

    ka_task_write_lock_bh(&rsv_mem_lock);
    rsv_mem = g_rsv_mem[ts_inst][type];
    ka_task_write_unlock_bh(&rsv_mem_lock);

    if (rsv_mem != NULL) {
        u32 ref = kref_safe_read(&rsv_mem->ref);
        if (ref > 1) {
            trs_warn("Rsv mem is still in use. (type=%d; ref=%u)", type, ref);
        }
        kref_safe_put(&rsv_mem->ref, trs_rsv_mem_release);
    }
}

static int _trs_rsv_mem_v2p(struct trs_rsv_mem *rsv_mem, void *vaddr, phys_addr_t *phy_addr)
{
    *phy_addr = ka_base_gen_pool_virt_to_phys(rsv_mem->pool, (uintptr_t)vaddr);
    return (KA_MM_PAGE_ALIGNED(*phy_addr)) ? 0 : -EBADR;
}

int trs_rsv_mem_v2p(struct trs_id_inst *inst, int type, void *vaddr, phys_addr_t *phy_addr)
{
    struct trs_rsv_mem *rsv_mem = trs_rsv_mem_get(inst, type);
    int ret = -ENODEV;

    if (rsv_mem != NULL) {
        ret = _trs_rsv_mem_v2p(rsv_mem, vaddr, phy_addr);
        trs_rsv_mem_put(rsv_mem);
    }

    return ret;
}

void *trs_rsv_mem_alloc(struct trs_id_inst *inst, int type, size_t size, u32 flag)
{
    struct trs_rsv_mem *rsv_mem = trs_rsv_mem_get(inst, type);
    void *vaddr = NULL;

    if (rsv_mem != NULL) {
        vaddr = (void *)ka_base_gen_pool_alloc(rsv_mem->pool, size);
        if (vaddr == NULL) {
#ifndef EMU_ST
            trs_debug("No rsv mem resource. (devid=%u; tsid=%u; type=%d; total=0x%lx; avail=0x%lx)\n", inst->devid,
                      inst->tsid, type, rsv_mem->attr.total_size, ka_base_gen_pool_avail(rsv_mem->pool));
#endif
            trs_rsv_mem_put(rsv_mem);
            return NULL;
        }

        if ((flag & TRS_RSV_MEM_OP_ZERO) != 0) {
            trs_rsv_mem_clear(rsv_mem, vaddr, size);
        }
    }

    return vaddr;
}

int trs_rsv_mem_get_meminfo(struct trs_id_inst *inst, int type, size_t *alloc_size, size_t *total_size)
{
    struct trs_rsv_mem *rsv_mem = trs_rsv_mem_get(inst, type);

    if (rsv_mem == NULL) {
        return -ENODEV;
    }
    *total_size = rsv_mem->attr.total_size;
    *alloc_size = rsv_mem->attr.total_size - ka_base_gen_pool_avail(rsv_mem->pool);
    trs_rsv_mem_put(rsv_mem);
    return 0;
}

void trs_rsv_mem_free(struct trs_id_inst *inst, int type, void *vaddr, size_t size)
{
    struct trs_rsv_mem *rsv_mem = trs_rsv_mem_get(inst, type);
    if (rsv_mem != NULL) {
        ka_base_gen_pool_free(rsv_mem->pool, (uintptr_t)vaddr, size);
        trs_rsv_mem_put(rsv_mem);
        trs_rsv_mem_put(rsv_mem); /* match rsv_mem_alloc */
    }
}

bool trs_rsv_mem_is_in_range(struct trs_id_inst *inst, int type, void *vaddr)
{
    struct trs_rsv_mem *rsv_mem = trs_rsv_mem_get(inst, type);
    bool is_in_range = false;

    if (rsv_mem != NULL) {
        if ((vaddr >= rsv_mem->attr.vaddr) && (vaddr < (rsv_mem->attr.vaddr + rsv_mem->attr.total_size))) {
            is_in_range = true;
        }
        trs_rsv_mem_put(rsv_mem);
    }

    return is_in_range;
}

int trs_rsv_mem_init(struct trs_id_inst *inst, int type, struct trs_rsv_mem_attr *attr)
{
    struct trs_rsv_mem *rsv_mem = NULL;
    int ret;

    rsv_mem = trs_rsv_mem_create(inst, type, attr);
    if (rsv_mem == NULL) {
        return -ENOMEM;
    }

    ret = trs_rsv_mem_add(inst, type, rsv_mem);
    if (ret != 0) {
        trs_rsv_mem_destroy(rsv_mem);
        return -ENODEV;
    }

    trs_debug("Trs rsv mem init. (devid=%u; tsid=%u; type=%d; total_size=0x%lx)\n", inst->devid, inst->tsid, type,
              attr->total_size);
    return 0;
}

void trs_rsv_mem_uninit(struct trs_id_inst *inst, int type)
{
    trs_rsv_mem_del(inst, type);
}
