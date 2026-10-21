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
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"
#include "ka_driver_pub.h"
#include "ka_fs_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_pci_pub.h"

#include "securec.h"
#include "soc_resmng_log.h"
#include "soc_subsys_ts.h"
#include "soc_resmng_ver.h"
#include "soc_resmng.h"
#include "soc_resmng_ioctl.h"
#if defined(CFG_BUILD_DEBUG)
#include "soc_proc_fs.h"
#endif

#include "pbl/pbl_uda.h"
#include "pbl/pbl_davinci_api.h"

void soc_res_show(u32 udevid, ka_seq_file_t *seq);

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define SOC_MAX_TS_NUM 2
#define SOC_MAX_DIE_NUM 4

static const u32 g_soc_dev_ver = SOC_DRIVER_DEV_VERSION;
static const u32 g_soc_host_ver = SOC_DRIVER_HOST_VERSION;

struct soc_mia_spec_info {
    u32 vfg_num;
    u32 vf_num;
};

struct soc_dev_resmng {
    u32 devid;
    u32 vfgid; /* ref the mia device belong to which vfg */
    u32 vfid;  /* valid rang:0 ~ 15 */
    struct soc_mia_grp_info grp_info[SOC_MAX_MIA_GROUP_NUM];

    ka_mutex_t mutex;

    ka_list_head_t io_bases_head;
    ka_list_head_t rsv_mems_head;
    ka_list_head_t key_value_head;
    ka_list_head_t attr_head;
    struct soc_irq_info irq_infos[DEV_IRQ_TYPE_MAX];
    struct soc_pcie_info pcie_info;

    struct soc_mia_spec_info spec_info; /* set in pf */
    struct soc_mia_res_info res_info[MIA_MAX_RES_TYPE];
    struct soc_mia_res_info_ex res_info_ex[MIA_MAX_RES_TYPE];
    struct soc_mia_res_info_ex *res_die_info[SOC_MAX_DIE_NUM];
    /* The subsys is directly placed here for unified management.
     *  The content of the c file is not exposed to other modules and does not generate coupling.
     */
    struct soc_resmng_ts ts_resmng[SOC_MAX_TS_NUM];

    u32 subsys_num[MAX_SOC_SUBSYS_TYPE];
    u32 ts_enable_ids[SOC_MAX_TS_NUM];
    struct soc_vnic_info vnic_info;
};

#define SOC_MAX_DAVINCI_NUM 2048
struct soc_dev_resmng *soc_resmng[SOC_MAX_DAVINCI_NUM];
ka_mutex_t soc_resmng_mutex;

static int subtype_maxid_trans[MAX_SOC_SUBSYS_TYPE] = {[TS_SUBSYS] = SOC_MAX_TS_NUM};

struct soc_rsv_mem *rsv_mem_node_find(const char *name, ka_list_head_t *rsv_mems_head)
{
    struct soc_rsv_mem *mem = NULL;
    struct soc_rsv_mem *n = NULL;

    ka_list_for_each_entry_safe(mem, n, rsv_mems_head, list_node)
    {
        if (ka_base_strcmp(mem->name, name) == 0) {
            return mem;
        }
    }

    return NULL;
}

struct soc_reg_base *io_bases_node_find(const char *name, ka_list_head_t *io_bases_head)
{
    struct soc_reg_base *reg = NULL;
    struct soc_reg_base *n = NULL;

    ka_list_for_each_entry_safe(reg, n, io_bases_head, list_node)
    {
        if (ka_base_strcmp(reg->name, name) == 0) {
            return reg;
        }
    }

    return NULL;
}

static struct soc_key_data *key_data_node_find(const char *name, ka_list_head_t *key_value_head)
{
    struct soc_key_data *info = NULL;

    ka_list_for_each_entry(info, key_value_head, list_node)
    {
        if (ka_base_strcmp(info->name, name) == 0) {
            return info;
        }
    }
    return NULL;
}

static struct soc_attr_data *attr_node_find(const char *name, ka_list_head_t *attr_head)
{
    struct soc_attr_data *info = NULL;

    ka_list_for_each_entry(info, attr_head, list_node)
    {
        if (ka_base_strcmp(info->name, name) == 0) {
            return info;
        }
    }
    return NULL;
}

int find_irq_index(struct soc_irq_info *info, u32 irq)
{
    u32 index;

    for (index = 0; index < info->irq_num; index++) {
        if (info->irqs[index].irq == irq) {
            return index;
        }
    }
    return info->irq_num;
}

int resmng_irqs_create(struct soc_irq_info *info, u32 irq_num)
{
    u32 i;

    info->irqs = (struct irq_info *)ka_mm_kzalloc(sizeof(struct irq_info) * irq_num, KA_GFP_KERNEL);
    if (info->irqs == NULL) {
        soc_err("Kzalloc failed.\n");
        return -ENOMEM;
    }

    for (i = 0; i < irq_num; i++) {
        info->irqs[i].irq = SOC_IRQ_INVALID_VALUE;
        info->irqs[i].hwirq = SOC_IRQ_INVALID_VALUE;
        info->irqs[i].tscpu_to_taishan_irq = SOC_IRQ_INVALID_VALUE;
    }

    return 0;
}

void resmng_irqs_destroy(struct soc_irq_info *info)
{
    if (info->irqs != NULL) {
        ka_mm_kfree(info->irqs);
        info->irqs = NULL;
    }
}

static void ts_resmng_create(struct soc_dev_resmng *resmng)
{
    u32 tsid;

    for (tsid = 0; tsid < SOC_MAX_TS_NUM; tsid++) {
        ka_task_mutex_init(&resmng->ts_resmng[tsid].mutex);
        KA_INIT_LIST_HEAD(&resmng->ts_resmng[tsid].io_bases_head);
        KA_INIT_LIST_HEAD(&resmng->ts_resmng[tsid].rsv_mems_head);
        KA_INIT_LIST_HEAD(&resmng->ts_resmng[tsid].key_value_head);
    }
}

static void rsv_mem_node_free(ka_list_head_t *rsv_mems_head)
{
    struct soc_rsv_mem *mem = NULL;
    struct soc_rsv_mem *n = NULL;

    ka_list_for_each_entry_safe(mem, n, rsv_mems_head, list_node)
    {
        ka_list_del(&mem->list_node);
        ka_mm_kfree(mem);
    }
}

static void io_base_node_free(ka_list_head_t *io_bases_head)
{
    struct soc_reg_base *reg = NULL;
    struct soc_reg_base *n = NULL;

    ka_list_for_each_entry_safe(reg, n, io_bases_head, list_node)
    {
        ka_list_del(&reg->list_node);
        ka_mm_kfree(reg);
    }
}

static void key_value_node_free(ka_list_head_t *key_value_head)
{
    struct soc_key_data *data = NULL;
    struct soc_key_data *n = NULL;

    ka_list_for_each_entry_safe(data, n, key_value_head, list_node)
    {
        ka_list_del(&data->list_node);
        ka_mm_kfree(data);
    }
}

static void attr_node_free(ka_list_head_t *attr_head)
{
    struct soc_attr_data *data = NULL;
    struct soc_attr_data *n = NULL;

    ka_list_for_each_entry_safe(data, n, attr_head, list_node)
    {
        ka_list_del(&data->list_node);
        if (data->attr != NULL) {
            ka_mm_kfree(data->attr);
        }
        ka_mm_kfree(data);
    }
    return;
}

static void ts_resmng_destroy(struct soc_dev_resmng *resmng)
{
    u32 tsid;
    u32 irq_type;

    for (tsid = 0; tsid < SOC_MAX_TS_NUM; tsid++) {
        for (irq_type = 0; irq_type < TS_IRQ_TYPE_MAX; irq_type++) {
            resmng_irqs_destroy(&resmng->ts_resmng[tsid].irq_infos[irq_type]);
        }
        rsv_mem_node_free(&resmng->ts_resmng[tsid].rsv_mems_head);
        io_base_node_free(&resmng->ts_resmng[tsid].io_bases_head);
        key_value_node_free(&resmng->ts_resmng[tsid].key_value_head);
        ka_task_mutex_destroy(&resmng->ts_resmng[tsid].mutex);
    }
}

static struct soc_dev_resmng *resmng_create(u32 devid)
{
    struct soc_dev_resmng *resmng = NULL;

    resmng = (struct soc_dev_resmng *)ka_mm_kzalloc(sizeof(struct soc_dev_resmng), KA_GFP_KERNEL);
    if (resmng == NULL) {
        soc_err("Kmalloc failed. (devid=%u)\n", devid);
        return NULL;
    }

    ka_task_mutex_init(&resmng->mutex);
    KA_INIT_LIST_HEAD(&resmng->io_bases_head);
    KA_INIT_LIST_HEAD(&resmng->rsv_mems_head);
    KA_INIT_LIST_HEAD(&resmng->key_value_head);
    KA_INIT_LIST_HEAD(&resmng->attr_head);
    resmng->devid = devid;

    ts_resmng_create(resmng);

    return resmng;
}

static void resmng_destroy(struct soc_dev_resmng *resmng)
{
    ts_resmng_destroy(resmng);

    rsv_mem_node_free(&resmng->rsv_mems_head);
    io_base_node_free(&resmng->io_bases_head);
    key_value_node_free(&resmng->key_value_head);
    attr_node_free(&resmng->attr_head);
    ka_task_mutex_destroy(&resmng->mutex);

    ka_mm_kfree(resmng);
}

static struct soc_dev_resmng *get_resmng(u32 devid)
{
    struct soc_dev_resmng *resmng = NULL;

    ka_task_mutex_lock(&soc_resmng_mutex);
    if (soc_resmng[devid] == NULL) {
        resmng = resmng_create(devid);
        if (resmng == NULL) {
            ka_task_mutex_unlock(&soc_resmng_mutex);
            return NULL;
        }
        soc_resmng[devid] = resmng;
    }
    ka_task_mutex_unlock(&soc_resmng_mutex);

    return soc_resmng[devid];
}

static int inst_param_check(struct res_inst_info *inst)
{
    if (inst == NULL) {
        soc_err("Param is NULL.\n");
        return -EINVAL;
    }
    if ((inst->devid >= SOC_MAX_DAVINCI_NUM) || (inst->sub_type >= MAX_SOC_SUBSYS_TYPE)) {
        soc_err("Param is illegal. (devid=%u; subtype=%d)\n", inst->devid, (int)inst->sub_type);
        return -EINVAL;
    }
    if (inst->subid >= (u32)subtype_maxid_trans[inst->sub_type]) {
        soc_err("Param is illegal. (devid=%u; subtype=%d; subid=%u; max=%d)\n", inst->devid, (int)inst->sub_type,
                inst->subid, subtype_maxid_trans[inst->sub_type]);
        return -EINVAL;
    }

    return 0;
}

static int soc_for_each_res_addr(ka_list_head_t *reg_head, ka_list_head_t *rsv_mem_head, u32 type,
                                 int (*func)(char *name, u64 addr, u64 len, void *priv), void *priv)
{
    int ret = 0;

    if (type == 0) {
        struct soc_reg_base *reg = NULL;

        ka_list_for_each_entry(reg, reg_head, list_node)
        {
            ret = func(reg->name, (u64)reg->info.io_base, (u64)reg->info.io_base_size, priv);
            if (ret != 0) {
                break;
            }
        }
    } else {
        struct soc_rsv_mem *mem = NULL;

        ka_list_for_each_entry(mem, rsv_mem_head, list_node)
        {
            ret = func(mem->name, (u64)mem->info.rsv_mem, (u64)mem->info.rsv_mem_size, priv);
            if (ret != 0) {
                break;
            }
        }
    }

    return ret;
}

static int subsys_ts_for_each_res_addr(struct soc_resmng_ts *ts_resmng, u32 type,
                                       int (*func)(char *name, u64 addr, u64 len, void *priv), void *priv)
{
    int ret = 0;

    ka_task_mutex_lock(&ts_resmng->mutex);
    ret = soc_for_each_res_addr(&ts_resmng->io_bases_head, &ts_resmng->rsv_mems_head, type, func, priv);
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return ret;
}

int soc_resmng_for_each_res_addr(struct res_inst_info *inst, u32 type,
                                 int (*func)(char *name, u64 addr, u64 len, void *priv), void *priv)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = -EINVAL;
    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_for_each_res_addr(&resmng->ts_resmng[inst->subid], type, func, priv);
    }

    return ret;
}

int soc_resmng_dev_for_each_res_addr(u32 devid, u32 type, int (*func)(char *name, u64 addr, u64 len, void *priv),
                                     void *priv)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is invalid when handling each resource addr. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = soc_for_each_res_addr(&resmng->io_bases_head, &resmng->rsv_mems_head, type, func, priv);
    ka_task_mutex_unlock(&resmng->mutex);

    return ret;
}

static int soc_for_each_key_value(ka_list_head_t *head, int (*func)(char *key, u64 value, void *priv), void *priv)
{
    struct soc_key_data *info = NULL;
    int ret = 0;

    ka_list_for_each_entry(info, head, list_node)
    {
        ret = func(info->name, info->value, priv);
        if (ret != 0) {
            break;
        }
    }

    return ret;
}

static int soc_for_each_attr_value(ka_list_head_t *head,
                                   int (*func)(const char *name, void *attr, u32 size, void *priv), void *priv)
{
    struct soc_attr_data *attr_node = NULL;
    int ret = 0;

    ka_list_for_each_entry(attr_node, head, list_node)
    {
        ret = func(attr_node->name, attr_node->attr, attr_node->size, priv);
        if (ret != 0) {
            break;
        }
    }

    return ret;
}

int soc_resmng_for_each_key_value(struct res_inst_info *inst, int (*func)(char *key, u64 value, void *priv), void *priv)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = -EINVAL;
    if (inst->sub_type == TS_SUBSYS) {
        ret = soc_for_each_key_value(&resmng->ts_resmng[inst->subid].key_value_head, func, priv);
    }
    ka_task_mutex_unlock(&resmng->mutex);

    return ret;
}

int soc_resmng_dev_for_each_key_value(u32 devid, int (*func)(char *key, u64 value, void *priv), void *priv)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = soc_for_each_key_value(&resmng->key_value_head, func, priv);
    ka_task_mutex_unlock(&resmng->mutex);

    return ret;
}

int soc_resmng_dev_for_each_attr(u32 devid, int (*func)(const char *name, void *attr, u32 size, void *priv), void *priv)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = soc_for_each_attr_value(&resmng->attr_head, func, priv);
    ka_task_mutex_unlock(&resmng->mutex);

    return ret;
}

int soc_resmng_set_rsv_mem(struct res_inst_info *inst, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is null when setting rsv mem. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when setting rsv mem. (devid=%u; name_len=%u; max_len=%u)\n", inst->devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_rsv_mem(&resmng->ts_resmng[inst->subid], name, rsv_mem);
        if (ret != 0) {
            soc_err("Param is illegal. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_rsv_mem);

int soc_resmng_get_rsv_mem(struct res_inst_info *inst, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when getting rsv mem. (devid=%u; name_len=%u; max_len=%u)\n", inst->devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_rsv_mem(&resmng->ts_resmng[inst->subid], name, rsv_mem);
        if (ret != 0) {
            soc_debug("Info. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_rsv_mem);

int soc_resmng_set_reg_base(struct res_inst_info *inst, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when setting reg base. (devid=%u; name_len=%u; max_len=%u)\n", inst->devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_reg_base(&resmng->ts_resmng[inst->subid], name, io_base);
        if (ret != 0) {
            soc_err("Param is illegal. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_reg_base);

int soc_resmng_get_reg_base(struct res_inst_info *inst, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when getting reg base. (devid=%u; name_len=%u; max_len=%u)\n", inst->devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_reg_base(&resmng->ts_resmng[inst->subid], name, io_base);
        if (ret != 0) {
            soc_debug("Info. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_reg_base);

#define SOC_IRQ_NUM_MAX 64
int soc_resmng_set_irq_num(struct res_inst_info *inst, u32 irq_type, u32 irq_num)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq_num >= SOC_IRQ_NUM_MAX) {
        soc_err("Param is illegal. (devid=%u; type=%u; num=%u)\n", inst->devid, irq_type, irq_num);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_irq_num(&resmng->ts_resmng[inst->subid], irq_type, irq_num);
        if (ret != 0) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; num=%u; ret=%d)\n", inst->devid, inst->subid, irq_type,
                    irq_num, ret);
            return ret;
        }
    }
    soc_info("Set success. (irq_num=%u; devid=%u; tsid=%u; type=%u)\n", irq_num, inst->devid, inst->subid, irq_type);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_irq_num);

int soc_resmng_get_irq_num(struct res_inst_info *inst, u32 irq_type, u32 *irq_num)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq_num == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_irq_num(&resmng->ts_resmng[inst->subid], irq_type, irq_num);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; ret=%d)\n", inst->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_irq_num);

int soc_resmng_set_irq_by_index(struct res_inst_info *inst, u32 irq_type, u32 index, u32 irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_irq_by_index(&resmng->ts_resmng[inst->subid], irq_type, index, irq);
        if (ret != 0) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; index=%u; ret=%d)\n", resmng->devid, inst->subid,
                    irq_type, index, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_irq_by_index);

int soc_resmng_get_irq_by_index(struct res_inst_info *inst, u32 irq_type, u32 index, u32 *irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq == NULL) {
        soc_err("Param is NULL. (devid=%u, irq_type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_irq_by_index(&resmng->ts_resmng[inst->subid], irq_type, index, irq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; index=%u; ret=%d)\n", resmng->devid, inst->subid,
                    irq_type, index, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_irq_by_index);

int soc_resmng_set_irq(struct res_inst_info *inst, u32 irq_type, u32 irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_irq(&resmng->ts_resmng[inst->subid], irq_type, irq);
        if (ret != 0) {
            soc_err("Need set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type,
                    ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_irq);

int soc_resmng_get_irq(struct res_inst_info *inst, u32 irq_type, u32 *irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_irq(&resmng->ts_resmng[inst->subid], irq_type, irq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_irq);

int soc_resmng_set_hwirq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 hwirq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_hwirq(&resmng->ts_resmng[inst->subid], irq_type, irq, hwirq);
        if (ret != 0) {
            soc_err("Set failed. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_hwirq);

int soc_resmng_get_hwirq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 *hwirq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (hwirq == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_hwirq(&resmng->ts_resmng[inst->subid], irq_type, irq, hwirq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_warn("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_hwirq);

int soc_resmng_set_tscpu_to_taishan_irq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 tscpu_to_taishan_irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_tscpu_to_taishan_irq(&resmng->ts_resmng[inst->subid], irq_type, irq, tscpu_to_taishan_irq);
        if (ret != 0) {
            soc_err("Set failed. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_tscpu_to_taishan_irq);

int soc_resmng_get_tscpu_to_taishan_irq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 *tscpu_to_taishan_irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (tscpu_to_taishan_irq == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_tscpu_to_taishan_irq(&resmng->ts_resmng[inst->subid], irq_type, irq, tscpu_to_taishan_irq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_tscpu_to_taishan_irq);

int soc_resmng_set_key_value(struct res_inst_info *inst, const char *name, u64 value)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 len;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    if (name == NULL) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Param is invalid. (devid=%u; len=%u)\n", inst->devid, len);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_key_value(&resmng->ts_resmng[inst->subid], name, value);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_key_value);

int soc_resmng_get_key_value(struct res_inst_info *inst, const char *name, u64 *value)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 len;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((value == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Param is invalid. (devid=%u; len=%u)\n", inst->devid, len);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_key_value(&resmng->ts_resmng[inst->subid], name, value);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_key_value);

static int dev_set_rsv_mem(struct soc_dev_resmng *resmng, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_rsv_mem *rsv_mem_inst = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    rsv_mem_inst = rsv_mem_node_find(name, &resmng->rsv_mems_head);
    if (rsv_mem_inst != NULL) {
        soc_res_name_copy(rsv_mem_inst->name, name);
        rsv_mem_inst->info = *rsv_mem;
        ka_task_mutex_unlock(&resmng->mutex);
        soc_info("Rsv mem set again. (devid=%u)\n", resmng->devid);
        return 0;
    }

    rsv_mem_inst = ka_mm_kzalloc(sizeof(*rsv_mem_inst), KA_GFP_KERNEL);
    if (rsv_mem_inst == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(rsv_mem_inst->name, name);
    rsv_mem_inst->info = *rsv_mem;

    ka_list_add(&rsv_mem_inst->list_node, &resmng->rsv_mems_head);
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_set_rsv_mem(u32 devid, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when setting rsv mem. (devid=%u; name_len=%u; max_len=%u)\n", devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_set_rsv_mem(resmng, name, rsv_mem);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_rsv_mem);

static int dev_get_rsv_mem(struct soc_dev_resmng *resmng, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_rsv_mem *rsv_mem_inst = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    rsv_mem_inst = rsv_mem_node_find(name, &resmng->rsv_mems_head);
    if (rsv_mem_inst == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOENT;
    }

    *rsv_mem = rsv_mem_inst->info;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_get_rsv_mem(u32 devid, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when getting rsv mem. (devid=%u; name_len=%u; max_len=%u)\n", devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_get_rsv_mem(resmng, name, rsv_mem);
    if (ret != 0) {
        soc_debug("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_rsv_mem);

int dev_set_key_value(ka_list_head_t *head, const char *name, u64 value)
{
    struct soc_key_data *key_value_list = NULL;

    key_value_list = key_data_node_find(name, head);
    if (key_value_list != NULL) {
        key_value_list->value = value;
        return 0;
    }

    key_value_list = ka_mm_kzalloc(sizeof(*key_value_list), KA_GFP_KERNEL);
    if (key_value_list == NULL) {
        return -ENOSPC;
    }

    soc_res_name_copy(key_value_list->name, name);
    key_value_list->value = value;

    ka_list_add(&key_value_list->list_node, head);

    return 0;
}

int soc_resmng_dev_set_key_value(u32 devid, const char *name, u64 value)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 len;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (name == NULL) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Param is invalid. (devid=%u; len=%u)\n", devid, len);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = dev_set_key_value(&resmng->key_value_head, name, value);
    ka_task_mutex_unlock(&resmng->mutex);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_key_value);

int dev_get_key_value(ka_list_head_t *head, const char *name, u64 *value)
{
    struct soc_key_data *key_value_list = NULL;

    key_value_list = key_data_node_find(name, head);
    if (key_value_list == NULL) {
        return -ENOENT;
    }

    *value = key_value_list->value;

    return 0;
}

int soc_resmng_dev_get_key_value(u32 devid, const char *name, u64 *value)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 len;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((value == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Param is invalid. (devid=%u; len=%u)\n", devid, len);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = dev_get_key_value(&resmng->key_value_head, name, value);
    ka_task_mutex_unlock(&resmng->mutex);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_key_value);

static int soc_resmng_name_check(const char *name)
{
    u32 len;

    if (name == NULL) {
        soc_err("Para name is NULL\n");
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Para name is invalid. (len=%u)\n", len);
        return -EINVAL;
    }
    return 0;
}

static int dev_repeat_set_attr(struct soc_attr_data *attr_node, const void *attr, u32 size)
{
    int ret;

    if (attr_node->size != size) {
        soc_err("Not support repeat set attr with different size. (name=%s; old_attr_size=0x%x, new_attr_size=0x%x)\n",
                attr_node->name, attr_node->size, size);
        return -EEXIST;
    }

    ret = memcpy_s(attr_node->attr, size, attr, size);
    if (ret != 0) {
        soc_err("Memcpy attr fail. (name=%s, size=0x%x)\n", attr_node->name, size);
        return ret;
    }

    return 0;
}

static int dev_set_attr(ka_list_head_t *head, const char *name, const void *attr, u32 size)
{
    struct soc_attr_data *attr_node = NULL;
    int ret;

    attr_node = attr_node_find(name, head);
    if (attr_node != NULL) {
        ret = dev_repeat_set_attr(attr_node, attr, size);
        if (ret != 0) {
            soc_err("Repeat set attr fail. (name=%s; size=0x%x)\n", name, size);
        }
        return ret;
    }

    attr_node = ka_mm_kzalloc(sizeof(*attr_node), KA_GFP_KERNEL);
    if (attr_node == NULL) {
        soc_err("Alloc attr node fail. (name=%s)\n", name);
        return -ENOSPC;
    }

    attr_node->attr = ka_mm_kzalloc(size, KA_GFP_KERNEL);
    if (attr_node->attr == NULL) {
        soc_err("Alloc attr fail. (name=%s, size=0x%x)\n", name, size);
        ka_mm_kfree(attr_node);
        attr_node = NULL;
        return -ENOSPC;
    }

    soc_res_name_copy(attr_node->name, name);

    ret = memcpy_s(attr_node->attr, size, attr, size);
    if (ret != 0) {
        soc_err("Memcpy attr fail. (name=%s, size=0x%x)\n", name, size);
        ka_mm_kfree(attr_node->attr);
        attr_node->attr = NULL;
        ka_mm_kfree(attr_node);
        attr_node = NULL;
        return ret;
    }
    attr_node->size = size;

    ka_list_add(&attr_node->list_node, head);

    return 0;
}

int soc_resmng_dev_set_attr(u32 devid, const char *name, const void *attr, u32 size)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    ret = soc_resmng_name_check(name);
    if (ret != 0) {
        soc_err("Invalid name. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    if ((attr == NULL) || (size == 0) || (size > SOC_RESMNG_MAX_ATTR_SIZE)) {
        soc_err("Invalid attr. (devid=%u; size=0x%x, max_attr_size=0x%x)\n", devid, size, SOC_RESMNG_MAX_ATTR_SIZE);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = dev_set_attr(&resmng->attr_head, name, attr, size);
    ka_task_mutex_unlock(&resmng->mutex);
    if (ret != 0) {
        soc_err("Set attr failed. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
        return ret;
    }

    soc_info("Set attr ok. (devid=%u; name=%s)\n", resmng->devid, name);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_attr);

static int dev_get_attr(ka_list_head_t *head, const char *name, void *attr, u32 size)
{
    struct soc_attr_data *attr_node = NULL;
    int ret;

    attr_node = attr_node_find(name, head);
    if (attr_node == NULL) {
        soc_err("Attr does not exist. (name=%s)\n", name);
        return -ENOENT;
    }

    if (attr_node->size != size) {
        soc_err("Attr size invalid. (name=%s; attr_node size=0x%x, size=0x%x)\n", name, attr_node->size, size);
        return -EINVAL;
    }

    ret = memcpy_s(attr, size, attr_node->attr, attr_node->size);
    if (ret != 0) {
        soc_err("Memcpy attr fail. (name=%s, size=0x%x)\n", name, size);
        return ret;
    }

    return 0;
}

int soc_resmng_dev_get_attr(u32 devid, const char *name, void *attr, u32 size)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    ret = soc_resmng_name_check(name);
    if (ret != 0) {
        soc_err("Invalid name. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    if (attr == NULL) {
        soc_err("Param attr is NULL. (devid=%u; name=%s)\n", devid, name);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    ret = dev_get_attr(&resmng->attr_head, name, attr, size);
    ka_task_mutex_unlock(&resmng->mutex);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_attr);

static int dev_set_reg_base(struct soc_dev_resmng *resmng, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_reg_base *reg = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    reg = io_bases_node_find(name, &resmng->io_bases_head);
    if (reg != NULL) {
        soc_res_name_copy(reg->name, name);
        reg->info = *io_base;
        ka_task_mutex_unlock(&resmng->mutex);
        soc_info("Reg base set again. (devid=%u)\n", resmng->devid);
        return 0;
    }

    reg = ka_mm_kzalloc(sizeof(*reg), KA_GFP_KERNEL);
    if (reg == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(reg->name, name);
    reg->info = *io_base;

    ka_list_add(&reg->list_node, &resmng->io_bases_head);
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_set_reg_base(u32 devid, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when setting reg base. (devid=%u; name_len=%u; max_len=%u)\n", devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_set_reg_base(resmng, name, io_base);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_reg_base);

static int dev_get_reg_base(struct soc_dev_resmng *resmng, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_reg_base *reg = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    reg = io_bases_node_find(name, &resmng->io_bases_head);
    if (reg == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOENT;
    }

    *io_base = reg->info;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_get_reg_base(u32 devid, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid when getting reg base. (devid=%u; name_len=%u; max_len=%u)\n", devid,
                (u32)ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN), SOC_RESMNG_MAX_NAME_LEN);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_get_reg_base(resmng, name, io_base);
    if (ret != 0) {
        soc_debug("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_reg_base);

int soc_resmng_dev_set_irq_num(u32 devid, u32 irq_type, u32 irq_num)
{
    soc_err("Not implemented.\n");
    return -EINVAL;
}
int soc_resmng_dev_get_irq_num(u32 devid, u32 irq_type, u32 *irq_num)
{
    soc_err("Not implemented.\n");
    return -EINVAL;
}
int soc_resmng_dev_set_irq_by_index(u32 devid, u32 irq_type, u32 index, u32 irq)
{
    soc_err("Not implemented.\n");
    return -EINVAL;
}
int soc_resmng_dev_get_irq_by_index(u32 devid, u32 irq_type, u32 index, u32 *irq)
{
    soc_err("Not implemented.\n");
    return -EINVAL;
}

int soc_resmng_subsys_set_num(u32 devid, enum soc_sub_type type, u32 subnum)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (type >= MAX_SOC_SUBSYS_TYPE)) {
        soc_err("Param is illegal. (devid=%u; type=%d)\n", devid, (int)type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->subsys_num[type] = subnum;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_subsys_set_num);

int soc_resmng_subsys_get_num(u32 devid, enum soc_sub_type type, u32 *subnum)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (type >= MAX_SOC_SUBSYS_TYPE)) {
        soc_err("Param is illegal. (devid=%u; type=%d)\n", devid, (int)type);
        return -EINVAL;
    }

    if (subnum == NULL) {
        soc_err("Param is illegal. (devid=%u; type=%d)\n", devid, (int)type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *subnum = resmng->subsys_num[type];
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_subsys_get_num);

int soc_resmng_dev_set_mia_res_ex(u32 devid, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((info == NULL) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Invalid para or res type. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->res_info_ex[type] = *info;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_res_ex);

int soc_resmng_dev_set_mia_res(u32 devid, enum soc_mia_res_type type, u64 bitmap, u32 unit_per_bit)
{
    struct soc_mia_res_info_ex info;

    info.bitmap = bitmap;
    info.unit_per_bit = unit_per_bit;
    info.start = 0;
    info.total_num = ka_base_hweight64(bitmap) * unit_per_bit;

    return soc_resmng_dev_set_mia_res_ex(devid, type, &info);
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_res);

int soc_resmng_dev_get_mia_res_ex(u32 devid, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((info == NULL) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Invalid para or res type. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *info = resmng->res_info_ex[type];
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_res_ex);

int soc_resmng_dev_get_mia_res(u32 devid, enum soc_mia_res_type type, u64 *bitmap, u32 *unit_per_bit)
{
    struct soc_mia_res_info_ex info;
    int ret;

    if ((bitmap == NULL) || (unit_per_bit == NULL)) {
        soc_err("Invalid para. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    ret = soc_resmng_dev_get_mia_res_ex(devid, type, &info);
    if (ret == 0) {
        *bitmap = info.bitmap;
        *unit_per_bit = info.unit_per_bit;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_res);

int soc_resmng_dev_set_mia_spec(u32 devid, u32 vfg_num, u32 vf_num)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->spec_info.vfg_num = vfg_num;
    resmng->spec_info.vf_num = vf_num;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_spec);

int soc_resmng_dev_get_mia_spec(u32 devid, u32 *vfg_num, u32 *vf_num)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *vfg_num = resmng->spec_info.vfg_num;
    *vf_num = resmng->spec_info.vf_num;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_spec);

static void soc_copy_soc_mia_res_info_ex_by_die(struct soc_mia_res_info_ex *self, struct soc_mia_res_info_ex *from)
{
    u32 die_idx;
    for (die_idx = 0; die_idx < MAX_DIE_NUM_PER_DEV; die_idx++) {
        self[die_idx] = from[die_idx];
    }
}

int soc_resmng_dev_set_mia_grp_info(u32 devid, u32 grp_id, struct soc_mia_grp_info *grp_info)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (grp_id >= SOC_MAX_MIA_GROUP_NUM)) {
        soc_err("Param is illegal. (devid=%u; grp_id=%u)\n", devid, grp_id);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    soc_copy_soc_mia_res_info_ex_by_die(resmng->grp_info[grp_id].aic_info, grp_info->aic_info);
    soc_copy_soc_mia_res_info_ex_by_die(resmng->grp_info[grp_id].aiv_info, grp_info->aiv_info);
    resmng->grp_info[grp_id].rtsq_info = grp_info->rtsq_info;
    resmng->grp_info[grp_id].notify_info = grp_info->notify_info;

    resmng->grp_info[grp_id].vfid = grp_info->vfid;
    resmng->grp_info[grp_id].valid = grp_info->valid;
    resmng->grp_info[grp_id].pool_id = grp_info->pool_id;
    resmng->grp_info[grp_id].poolid_max = grp_info->poolid_max;
    ka_task_mutex_unlock(&resmng->mutex);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_grp_info);

int soc_resmng_dev_get_mia_grp_info(u32 devid, u32 grp_id, struct soc_mia_grp_info *grp_info)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (grp_id >= SOC_MAX_MIA_GROUP_NUM)) {
        soc_err("Param is illegal. (devid=%u; grp_id=%u)\n", devid, grp_id);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    soc_copy_soc_mia_res_info_ex_by_die(grp_info->aic_info, resmng->grp_info[grp_id].aic_info);
    soc_copy_soc_mia_res_info_ex_by_die(grp_info->aiv_info, resmng->grp_info[grp_id].aiv_info);
    grp_info->rtsq_info = resmng->grp_info[grp_id].rtsq_info;
    grp_info->notify_info = resmng->grp_info[grp_id].notify_info;

    grp_info->valid = resmng->grp_info[grp_id].valid;
    grp_info->vfid = resmng->grp_info[grp_id].vfid;
    grp_info->pool_id = resmng->grp_info[grp_id].pool_id;
    grp_info->poolid_max = resmng->grp_info[grp_id].poolid_max;
    ka_task_mutex_unlock(&resmng->mutex);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_grp_info);

int soc_resmng_dev_set_mia_base_info(u32 devid, u32 vfgid, u32 vfid)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->vfgid = vfgid;
    resmng->vfid = vfid;
    ka_task_mutex_unlock(&resmng->mutex);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_base_info);

int soc_resmng_dev_get_mia_base_info(u32 devid, u32 *vfgid, u32 *vfid)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *vfgid = resmng->vfgid;
    *vfid = resmng->vfid;
    ka_task_mutex_unlock(&resmng->mutex);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_base_info);

int soc_resmng_set_mia_res(struct res_inst_info *inst, enum soc_mia_res_type type, u64 bitmap, u32 unit_per_bit)
{
    struct soc_dev_resmng *resmng = NULL;
    struct soc_mia_res_info_ex info = {0};
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        info.bitmap = bitmap;
        info.unit_per_bit = unit_per_bit;
        info.start = 0;
        info.total_num = ka_base_hweight64(bitmap) * unit_per_bit;
        ret = subsys_ts_set_mia_res_ex(&resmng->ts_resmng[inst->subid], type, &info);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_mia_res);

int soc_resmng_get_mia_res(struct res_inst_info *inst, enum soc_mia_res_type type, u64 *bitmap, u32 *unit_per_bit)
{
    struct soc_dev_resmng *resmng = NULL;
    struct soc_mia_res_info_ex info = {0};
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((bitmap == NULL) || (unit_per_bit == NULL)) {
        soc_err("Para is NULL. (devid=%u; type=%u)\n", inst->devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_mia_res_ex(&resmng->ts_resmng[inst->subid], type, &info);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, type, ret);
        }
        *bitmap = info.bitmap;
        *unit_per_bit = info.unit_per_bit;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_mia_res);

int soc_resmng_set_mia_res_ex(struct res_inst_info *inst, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (info == NULL) {
        soc_err("Para is null. (devid=%u; type=%u)\n", inst->devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_mia_res_ex(&resmng->ts_resmng[inst->subid], type, info);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_mia_res_ex);

int soc_resmng_get_mia_res_ex(struct res_inst_info *inst, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (info == NULL) {
        soc_err("Para is null. (devid=%u; type=%u)\n", inst->devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_mia_res_ex(&resmng->ts_resmng[inst->subid], type, info);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_mia_res_ex);

int soc_resmng_set_ts_status(struct res_inst_info *inst, u32 status)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        subsys_ts_set_ts_status(&resmng->ts_resmng[inst->subid], status);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_ts_status);

int soc_resmng_get_ts_status(struct res_inst_info *inst, u32 *status)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    if (status == NULL) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        subsys_ts_get_ts_status(&resmng->ts_resmng[inst->subid], status);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_ts_status);

static int dev_die_res_info_alloc(struct soc_dev_resmng *resmng, u32 die_id)
{
    if (resmng->res_die_info[die_id] == NULL) {
        resmng->res_die_info[die_id] = (struct soc_mia_res_info_ex *)ka_mm_kzalloc(
            sizeof(struct soc_mia_res_info_ex) * MIA_MAX_RES_TYPE, KA_GFP_KERNEL);
        if (resmng->res_die_info[die_id] == NULL) {
            soc_err("Kzalloc failed.\n");
            return -ENOMEM;
        }
    }

    return 0;
}

static void dev_die_res_info_free(struct soc_dev_resmng *resmng)
{
    u32 i;

    if (resmng == NULL) {
        return;
    }

    for (i = 0; i < SOC_MAX_DIE_NUM; i++) {
        if (resmng->res_die_info[i] != NULL) {
            ka_mm_kfree(resmng->res_die_info[i]);
            resmng->res_die_info[i] = NULL;
        }
    }
}

int soc_resmng_set_hccs_link_status_and_group_id(u32 devid, u32 hccs_status, u32 *hccs_group_id, u32 group_id_num)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 i;

    if (group_id_num > SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM) {
        return -EINVAL;
    }

    if (hccs_group_id == NULL) {
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    resmng->pcie_info.hccs_status = hccs_status;
    for (i = 0; i < group_id_num; i++) {
        resmng->pcie_info.hccs_group_id[i] = hccs_group_id[i];
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_hccs_link_status_and_group_id);

int soc_resmng_get_hccs_link_status_and_group_id(u32 devid, u32 *hccs_status, u32 *hccs_group_id, u32 group_id_num)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 i;

    if (group_id_num > SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM) {
        return -EINVAL;
    }

    if ((hccs_group_id == NULL) || (hccs_status == NULL)) {
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    *hccs_status = resmng->pcie_info.hccs_status;
    for (i = 0; i < group_id_num; i++) {
        hccs_group_id[i] = resmng->pcie_info.hccs_group_id[i];
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_hccs_link_status_and_group_id);

int soc_resmng_set_host_phy_mach_flag(u32 devid, u32 host_flag)
{
    struct soc_dev_resmng *resmng = NULL;

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    resmng->pcie_info.host_flag = host_flag;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_host_phy_mach_flag);

int soc_resmng_get_host_phy_mach_flag(u32 devid, u32 *host_flag)
{
    struct soc_dev_resmng *resmng = NULL;

    if (host_flag == NULL) {
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    *host_flag = resmng->pcie_info.host_flag;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_host_phy_mach_flag);

int soc_resmng_set_pdev_by_devid(u32 devid, ka_pci_dev_t *pdev)
{
    struct soc_dev_resmng *resmng = NULL;

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    resmng->pcie_info.pdev = pdev;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_pdev_by_devid);

int soc_resmng_get_pdev_by_devid(u32 devid, ka_pci_dev_t **pdev)
{
    struct soc_dev_resmng *resmng = NULL;

    if (pdev == NULL) {
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    *pdev = resmng->pcie_info.pdev;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_pdev_by_devid);

int soc_resmng_get_topology_by_host_flag(u32 devid, u32 peer_devid, int *topo_type)
{
    u32 host_flag = 0;
    int ret;

    if (topo_type == NULL) {
        return -EINVAL;
    }

    ret = soc_resmng_get_host_phy_mach_flag(devid, &host_flag);
    if (ret != 0) {
        soc_err("Get phy mach flag failed. (devid=%u;ret=%d)\n", devid, ret);
        return ret;
    }
    if (host_flag != SOC_HOST_PHY_MACH_FLAG) {
        *topo_type = SOC_TOPOLOGY_SYS;
        return 0;
    }

    ret = soc_resmng_get_host_phy_mach_flag(peer_devid, &host_flag);
    if (ret != 0) {
        soc_err("Get phy mach flag failed. (peer_devid=%u;ret=%d)\n", peer_devid, ret);
        return ret;
    }
    if (host_flag != SOC_HOST_PHY_MACH_FLAG) {
        *topo_type = SOC_TOPOLOGY_SYS;
        return 0;
    }
    return 0;
}

int soc_resmng_get_dev_topology(u32 devid, u32 peer_devid, int *topo_type)
{
    ka_pci_dev_t *pdev_a = NULL;
    ka_pci_dev_t *pdev_b = NULL;
    ka_pci_dev_t *bridge_a = NULL;
    ka_pci_dev_t *bridge_b = NULL;
    int ret;

    if (topo_type == NULL) {
        return -EINVAL;
    }

    if (devid == peer_devid) {
        soc_err("Input devid and peer_devid are equal, invalid.(devid=%u;peer_devid=%u)\n", devid, peer_devid);
        return -EINVAL;
    }

    if ((!uda_is_pf_dev(devid)) || (!uda_is_pf_dev(peer_devid))) {
        *topo_type = SOC_TOPOLOGY_SYS;
        return 0;
    }

    ret = soc_resmng_get_topology_by_host_flag(devid, peer_devid, topo_type);
    if ((ret != 0) || (*topo_type == SOC_TOPOLOGY_SYS)) {
        return ret;
    }

    ret = soc_resmng_get_pdev_by_devid(devid, &pdev_a);
    if ((pdev_a == NULL) || (ret != 0)) {
        soc_err("Get pdev failed, pdev_a is NULL. (devid=%u;peer_devid=%u;ret=%d;)\n", devid, peer_devid, ret);
        return -EINVAL;
    }

    ret = soc_resmng_get_pdev_by_devid(peer_devid, &pdev_b);
    if ((pdev_b == NULL) || (ret != 0)) {
        soc_err("Get pdev failed, pdev_b is NULL. (devid=%u;peer_devid=%u;ret=%d;)\n", devid, peer_devid, ret);
        return -EINVAL;
    }

    bridge_a = ka_pci_upstream_bridge(pdev_a);
    bridge_b = ka_pci_upstream_bridge(pdev_b);
    if ((bridge_a != NULL) && (bridge_b != NULL)) {
        // If two devices in the same switch, return PIX
        if ((bridge_a->bus->self != NULL) && (bridge_a->bus->self == bridge_b->bus->self)) {
            *topo_type = SOC_TOPOLOGY_PIX;
            return 0;
        }
        // If two devices have no switch but in the same bus, return PHB
        if (bridge_a->bus == bridge_b->bus) {
            *topo_type = SOC_TOPOLOGY_PHB;
            return 0;
        }
    }

    // If two devices in the same NUMA, return PHB
    // In this way, PIB topology type is considered PHB topology type
    if ((ka_driver_dev_to_node(&pdev_a->dev) != KA_NUMA_NO_NODE) &&
        (ka_driver_dev_to_node(&pdev_a->dev) == ka_driver_dev_to_node(&pdev_b->dev))) {
        *topo_type = SOC_TOPOLOGY_PHB;
        return 0;
    }
    *topo_type = SOC_TOPOLOGY_SYS;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_dev_topology);

int soc_resmng_dev_die_set_res(u32 devid, u32 die_id, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (die_id >= SOC_MAX_DIE_NUM) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Param is illegal. (devid=%u; die_id=%u; type=%u)\n", devid, die_id, type);
        return -EINVAL;
    }

    if (info == NULL) {
        soc_err("Info is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    if (resmng->res_die_info[die_id] == NULL) {
        ret = dev_die_res_info_alloc(resmng, die_id);
        if (ret != 0) {
            ka_task_mutex_unlock(&resmng->mutex);
            return ret;
        }
    }

    resmng->res_die_info[die_id][type] = *info;
    ka_task_mutex_unlock(&resmng->mutex);

    soc_info("set res ok. (devid=%u; die_id=%u, type=%u; bitmap=0x%llx; unit=%u; start=%u; total_num=%u)\n", devid,
             die_id, type, info->bitmap, info->unit_per_bit, info->start, info->total_num);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_die_set_res);

int soc_resmng_dev_die_get_res(u32 devid, u32 die_id, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (die_id >= SOC_MAX_DIE_NUM) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Param is illegal. (devid=%u; die_id=%u; type=%u)\n", devid, die_id, type);
        return -EINVAL;
    }

    if (info == NULL) {
        soc_err("Info is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    if (resmng->res_die_info[die_id] == NULL) {
        soc_info("Res die info is NULL. (devid=%u; die_id=%u)\n", devid, die_id);
        info->bitmap = 0;
        info->unit_per_bit = 1;
        info->start = 0;
        info->total_num = 0;
        info->freq = 0;
        ka_task_mutex_unlock(&resmng->mutex);
        return 0;
    }

    *info = resmng->res_die_info[die_id][type];
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_die_get_res);

int soc_resmng_dev_set_vnic_ip(u32 devid, u32 vnic_ip)
{
    struct soc_dev_resmng *resmng = NULL;

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    resmng->vnic_info.valid = true;
    resmng->vnic_info.vnic_ip = vnic_ip;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_vnic_ip);

int soc_resmng_dev_get_vnic_ip(u32 devid, u32 *vnic_ip)
{
    struct soc_dev_resmng *resmng = NULL;

    if (vnic_ip == NULL) {
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (resmng->vnic_info.valid != true) {
        return -ENOENT;
    }

    *vnic_ip = resmng->vnic_info.vnic_ip;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_vnic_ip);

#if defined(CFG_BUILD_DEBUG)
static int soc_res_addr_show(char *name, u64 addr, u64 len, void *priv)
{
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    ka_fs_seq_printf(seq, "        name %s addr %llx len %llx\n", name, addr, len);
    return 0;
}

static int soc_res_key_value_show(char *key, u64 value, void *priv)
{
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    ka_fs_seq_printf(seq, "        key %s value %llx\n", key, value);
    return 0;
}

static int soc_res_attr_show(const char *name, void *attr, u32 size, void *priv)
{
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    ka_fs_seq_printf(seq, "        name %s size 0x%x\n", name, size);
    return 0;
}

static void soc_res_irq_show(ka_seq_file_t *seq, struct soc_irq_info *irqs, u32 num)
{
    u32 i, j;

    ka_fs_seq_printf(seq, "        type  irq_num  hw_irq irq\n");

    for (i = 0; i < num; i++) {
        struct soc_irq_info *irq = irqs + i;
        if (irq->irqs == NULL) {
            continue;
        }
        for (j = 0; j < irq->irq_num; j++) {
            ka_fs_seq_printf(seq, "        %d   %u   %u   %u\n", i, irq->irq_num, irq->irqs[j].hwirq, irq->irqs[j].irq);
        }
    }
}

static void soc_res_mia_show(ka_seq_file_t *seq, struct soc_mia_res_info_ex *mias, u32 num)
{
    u32 i;

    ka_fs_seq_printf(seq, "        type       bitmap    unit_per_bit    start   total_num       min_num     freq\n");

    for (i = 0; i < num; i++) {
        struct soc_mia_res_info_ex *mia = mias + i;
        if (mia->unit_per_bit != 0) {
            ka_fs_seq_printf(seq, "        %s       %llx      %u      %u      %u        %u      %u\n", mia_res_name[i],
                             mia->bitmap, mia->unit_per_bit, mia->start, mia->total_num, mia->min_num, mia->freq);
        }
    }
}

void soc_res_show(u32 udevid, ka_seq_file_t *seq)
{
    u32 i;
    struct soc_dev_resmng *resmng = (udevid < SOC_MAX_DAVINCI_NUM) ? soc_resmng[udevid] : NULL;
    if (resmng == NULL) {
        return;
    }

    ka_fs_seq_printf(seq, "udevid: %d\n", udevid);

    ka_fs_seq_printf(seq, "dev reg info:\n");
    (void)soc_resmng_dev_for_each_res_addr(udevid, 0, soc_res_addr_show, seq);

    ka_fs_seq_printf(seq, "dev rsv mem info:\n");
    (void)soc_resmng_dev_for_each_res_addr(udevid, 1, soc_res_addr_show, seq);

    ka_fs_seq_printf(seq, "dev key value info:\n");
    (void)soc_resmng_dev_for_each_key_value(udevid, soc_res_key_value_show, seq);

    ka_fs_seq_printf(seq, "dev attr info:\n");
    (void)soc_resmng_dev_for_each_attr(udevid, soc_res_attr_show, seq);

    ka_fs_seq_printf(seq, "dev irq info:\n");
    soc_res_irq_show(seq, resmng->irq_infos, DEV_IRQ_TYPE_MAX);

    ka_fs_seq_printf(seq, "dev mia info:\n");
    soc_res_mia_show(seq, resmng->res_info_ex, MIA_MAX_RES_TYPE);

    ka_fs_seq_printf(seq, "ts num: %d\n", resmng->subsys_num[TS_SUBSYS]);

    for (i = 0; i < resmng->subsys_num[TS_SUBSYS]; i++) {
        struct res_inst_info inst;

        soc_resmng_inst_pack(&inst, udevid, TS_SUBSYS, i);

        ka_fs_seq_printf(seq, "udevid: %d ts %u\n", udevid, i);

        ka_fs_seq_printf(seq, "ts subsys reg info:\n");
        (void)soc_resmng_for_each_res_addr(&inst, 0, soc_res_addr_show, seq);

        ka_fs_seq_printf(seq, "ts subsys rsv mem info:\n");
        (void)soc_resmng_for_each_res_addr(&inst, 1, soc_res_addr_show, seq);

        ka_fs_seq_printf(seq, "ts subsys key value info:\n");
        (void)soc_resmng_for_each_key_value(&inst, soc_res_key_value_show, seq);

        ka_fs_seq_printf(seq, "ts subsys irq info:\n");
        soc_res_irq_show(seq, resmng->ts_resmng[i].irq_infos, TS_IRQ_TYPE_MAX);

        ka_fs_seq_printf(seq, "ts subsys mia info:\n");
        soc_res_mia_show(seq, resmng->ts_resmng[i].res_info_ex, MIA_MAX_RES_TYPE);
    }

    for (i = 0; i < SOC_MAX_DIE_NUM; i++) {
        if (resmng->res_die_info[i] != NULL) {
            ka_fs_seq_printf(seq, "dev die %u mia info:\n", i);
            soc_res_mia_show(seq, resmng->res_die_info[i], MIA_MAX_RES_TYPE);
        }
    }

    ka_fs_seq_printf(seq, "\n");
}
#endif

int soc_resnmg_get_version(enum soc_ver_type type)
{
    switch (type) {
        case VER_TYPE_DEV:
            return g_soc_dev_ver;
        case VER_TYPE_HOST:
            return g_soc_host_ver;
        default:
            soc_warn("Invalid version type. (type=%d; valid range=0-%d)\n", type, VER_TYPE_MAX - 1);
            return -EINVAL;
    }
}
KA_EXPORT_SYMBOL_GPL(soc_resnmg_get_version);

static long soc_resnmg_get_version_ioctl(ka_file_t *filp, unsigned int cmd, unsigned long arg)
{
    u32 ver;

    if (_KA_IOC_TYPE(cmd) != SOC_RESMNG_GET_VER_MAGIC) {
        return -EINVAL;
    }

    if (_KA_IOC_NR(cmd) > 0) {
        return -EINVAL;
    }

    switch (cmd) {
        case SOC_RESMNG_GET_DEV_VER:
            ver = soc_resnmg_get_version(VER_TYPE_DEV);
            if (ka_base_copy_to_user((u32 __ka_user *)arg, &ver, sizeof(ver))) {
                return -EFAULT;
            }
            break;
        case SOC_RESMNG_GET_HOST_VER:
            ver = soc_resnmg_get_version(VER_TYPE_HOST);
            if (ka_base_copy_to_user((u32 __ka_user *)arg, &ver, sizeof(ver))) {
                return -EFAULT;
            }
            break;
        default:
            return -ENOTTY;
    }

    return 0;
}

static int get_version_ioctl_open(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

static int get_version_ioctl_release(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

STATIC ka_file_operations_t soc_resmng_fops = {
    .owner = KA_THIS_MODULE,
    .open = get_version_ioctl_open,
    .release = get_version_ioctl_release,
    .unlocked_ioctl = soc_resnmg_get_version_ioctl,
};

static int soc_resnmg_ioctl_init(void)
{
    int ret;

    ret = drv_davinci_register_sub_module(SOC_RESMNG_MODULE_NAME, &soc_resmng_fops);
    if (ret != 0) {
        soc_err("Register soc_resmng sub module failed. (ret=%d)\n", ret);
        return -ENODEV;
    }

    return 0;
}

static void soc_resnmg_ioctl_uninit(void)
{
    int ret;

    ret = drv_ascend_unregister_sub_module(SOC_RESMNG_MODULE_NAME);
    if (ret != 0) {
        soc_err("Unregister soc_resmng sub module failed. (ret=%d)\n", ret);
        return;
    }
    return;
}

static void resmng_init(void)
{
    u32 devid;

    for (devid = 0; devid < SOC_MAX_DAVINCI_NUM; devid++) {
        soc_resmng[devid] = NULL;
    }
}

static void resmng_uninit(void)
{
    u32 devid;

    for (devid = 0; devid < SOC_MAX_DAVINCI_NUM; devid++) {
        if (soc_resmng[devid] != NULL) {
            dev_die_res_info_free(soc_resmng[devid]);
            resmng_destroy(soc_resmng[devid]);
            soc_resmng[devid] = NULL;
        }
    }
}

int resmng_init_module(void)
{
    ka_task_mutex_init(&soc_resmng_mutex);
    resmng_init();
    if (soc_resnmg_ioctl_init() != 0) {
        soc_err("soc_resmng_ioctl_init failed\n");
        return -EFAULT;
    }
#if defined(CFG_BUILD_DEBUG)
    soc_proc_fs_init();
#endif
    soc_info("Init_module success\n");

    return 0;
}

void resmng_exit_module(void)
{
#if defined(CFG_BUILD_DEBUG)
    soc_proc_fs_uninit();
#endif
    resmng_uninit();
    soc_resnmg_ioctl_uninit();
    ka_task_mutex_destroy(&soc_resmng_mutex);
    soc_info("Exit_module success\n");
}