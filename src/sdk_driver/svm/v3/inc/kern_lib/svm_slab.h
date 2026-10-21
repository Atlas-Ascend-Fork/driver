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
#ifndef SVM_SLAB_H
#define SVM_SLAB_H

#include "ka_type.h"

#include "pbl_ka_memory.h"
#include "ascend_hal_define.h"
/*
 * KA_SUB_MODULE_TYPE_0 : ctrl memory
 * KA_SUB_MODULE_TYPE_1 : ai memory normal page (business use)
 * KA_SUB_MODULE_TYPE_2 : ai memory huge page (business use)
 * KA_SUB_MODULE_TYPE_3 : svm memory normal page (svm self-use)
 * KA_SUB_MODULE_TYPE_4 : svm memory huge page (svm self-use)
 */

/* KA_SUB_MODULE_TYPE_0 : ctrl memory */

#define svm_kmalloc(size, flags) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define svm_kzalloc(size, flags) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define svm_kcalloc(size, flags) ka_kcalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define svm_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define svm_vzalloc(size) ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define svm_vmalloc(size, gfp_mask, prot) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define svm_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define svm_get_free_pages(gfp_mask, order) \
    ka_get_free_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define svm__exact(size, gfp_mask) \
    ka__exact(size, gfp_mask, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define svm_free_pages_exact(virt, size) \
    ka_free_pages_exact(virt, size, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define svm_kvzalloc(size, flags) \
    ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define svm_kvmalloc(size, flags) \
    ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define svm_kvfree(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

/* KA_SUB_MODULE_TYPE_1 : ai memory normal page (business use) */
#define svm_alloc_pages_node_ex(nid, gfp_mask, order) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define svm_free_pages_ex(page, order) \
    __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define svm_kvmalloc_node(size, flags, node) \
    ka_kvmalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define svm_kvfree_node(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))

/* KA_SUB_MODULE_TYPE_2 : ai memory huge page (business use) */
#define svm_hugetlb_alloc_hugepage(nid, flag) \
    ka_hugetlb_alloc_hugepage(nid, flag, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_2))
#ifdef DAVINCI_DEVICE
static inline void svm_hugetlb_free_hugepage(ka_page_t *page)
{
    ka_hugetlb_free_hugepage(page, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_2));
}
#else
static inline void svm_hugetlb_free_hugepage(ka_page_t *page)
{
    ka_mm_put_page(page);
}
#endif

/* KA_SUB_MODULE_TYPE_3 : svm memory normal page (svm self-use) */
#define svm_alloc_pages_node_self(nid, gfp_mask, order) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_3))
#define svm_free_pages_self(page, order) \
    __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_3))
#define svm_kvmalloc_node_self(size, flags, node) \
    ka_kvmalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_3))
#define svm_kvfree_node_self(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_3))

/* KA_SUB_MODULE_TYPE_4 : svm memory huge page (svm self-use) */
#define svm_hugetlb_alloc_hugepage_self(nid, flag) \
    ka_hugetlb_alloc_hugepage(nid, flag, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_4))
#ifdef DAVINCI_DEVICE
static inline void svm_hugetlb_free_hugepage_self(ka_page_t *page)
{
    ka_hugetlb_free_hugepage(page, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_4));
}
#else
static inline void svm_hugetlb_free_hugepage_self(ka_page_t *page)
{
    ka_mm_put_page(page);
}
#endif
#endif
