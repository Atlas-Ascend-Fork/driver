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

#ifndef CASM_KERNEL_H
#define CASM_KERNEL_H

#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "casm_def.h"

struct casm_src_ex {
    u64 updated_va; /* in ub case, we update va to uba id and offset, and stored here; in pcie case, this is 0 */
    int owner_tgid;
    struct casm_src_prop src_prop;
};

/* register by ub mem adapt */
struct svm_casm_src_ops {
    int (*add_src)(u32 udevid, struct svm_global_va *src_va, struct casm_src_ex *src_ex);
    void (*del_src)(u32 udevid, int tgid, struct svm_global_va *src_va,
                    struct casm_src_ex *src_ex); /* may not in task ctx */
};
void svm_casm_register_src_ops(const struct svm_casm_src_ops *ops);

/* register by cross server adapt who create key */
#define CASM_KEY_UDEVID_OFFSET 32
#define CASM_KEY_UDEVID_MASK 0xffff
static inline u64 casm_make_key(u32 udevid, u32 id)
{
    return ((((u64)udevid) << CASM_KEY_UDEVID_OFFSET) | (u64)(id));
}
static inline u32 casm_key_to_udevid(u64 key)
{
    return (((u32)((key) >> CASM_KEY_UDEVID_OFFSET)) & CASM_KEY_UDEVID_MASK);
}
static inline u32 casm_key_to_id(u64 key)
{
    return ((u32)(key));
}
struct svm_casm_key_ops {
    int (*update_key)(u64 *key);
    int (*parse_key)(u64 key, u32 *udevid, u32 *id);
    bool (*is_local_key)(u64 key);
};
void svm_casm_register_key_ops(const struct svm_casm_key_ops *ops);
int casm_get_src_va(u64 key, struct svm_global_va *src_va, struct casm_src_ex *src_ex);

/* register by cross server adapt who open key */
struct svm_casm_dst_ops {
    int (*src_info_query)(u32 udevid, u64 key, struct svm_global_va *src_va,
                          struct casm_src_prop *src_prop);                                   /* get src cmd */
    int (*src_info_get)(u32 udevid, u64 key, struct svm_global_va *src_va, int *owner_tgid); /* pin cmd */
    void (*src_info_put)(u32 udevid, u64 key, struct svm_global_va *src_va, int owner_tgid); /* unpin cmd */
};
void svm_casm_register_dst_ops(const struct svm_casm_dst_ops *ops);

/* called by smm um handle to update src info by dst va */
int svm_casm_get_src_info(u32 udevid, u64 va, u64 size, struct svm_global_va *src_info);

static inline void casm_try_to_update_src_va(struct svm_global_va *src_va, u64 updated_va)
{
    if (updated_va != 0) {
        src_va->va = updated_va;
    }
}

void svm_casm_register_get_src_va_ex_info_handle(int (*handle)(u32 udevid, struct svm_global_va *src_va,
                                                               struct casm_src_ex *src_ex, u64 *ex_info));

#endif
