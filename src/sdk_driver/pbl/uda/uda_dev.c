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
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "uda_dev_adapt.h"

#include "uda_notifier.h"
#include "pbl_mem_alloc_interface.h"
#include "ascend_dev_num.h"
#include "uda_dev.h"

static ka_mutex_t uda_dev_mutex;
static ka_rwlock_t uda_dev_lock;
static ka_mutex_t uda_reorder_mutex;
static struct uda_dev_inst *dev_insts[UDA_UDEV_MAX_NUM];
struct uda_reorder_insts reorder_insts;
struct udevid_reorder_para *reorder_para = NULL;
void __ka_mm_iomem *g_reorder_para_addr[UDA_UDEV_MAX_NUM] = {NULL};
static bool uda_udevid_reorder_flag = false;
STATIC unsigned int g_addid_array[UDA_UDEV_MAX_NUM] = {0};
STATIC unsigned int g_udevid_to_addid[UDA_UDEV_MAX_NUM] = {0};

void uda_type_to_string(struct uda_dev_type *type, char buf[], u32 buf_len)
{
    int ret = 0;
    static const char *uda_hw_name[UDA_HW_MAX] = {
        [UDA_DAVINCI] = "davinci",
        [UDA_KUNPENG] = "kunpeng",
    };

    static const char *uda_obj_name[UDA_OBJECT_MAX] = {
        [UDA_ENTITY] = "entity",
        [UDA_AGENT] = "agent",
    };

    static const char *uda_loc_name[UDA_LOCATION_MAX] = {
        [UDA_LOCAL] = "local",
        [UDA_NEAR] = "near",
        [UDA_REMOTE] = "remote",
    };

    static const char *uda_prop_name[UDA_PROP_MAX] = {
        [UDA_REAL] = "real",
        [UDA_VIRTUAL] = "virtual",
        [UDA_REAL_SEC_EH] = "real_sec_eh",
    };

    ret = sprintf_s(buf, buf_len, "%s %s %s %s", uda_hw_name[type->hw], uda_obj_name[type->object],
                    uda_loc_name[type->location], uda_prop_name[type->prop]);
    if (ret <= 0) {
        uda_err("Invoke sprintf_s error.\n");
    }
}

u32 uda_get_udev_max_num(void)
{
    return UDA_UDEV_MAX_NUM;
}
KA_EXPORT_SYMBOL(uda_get_udev_max_num);

u32 uda_get_remote_udev_max_num(void)
{
    return UDA_REMOTE_UDEV_MAX_NUM;
}
KA_EXPORT_SYMBOL(uda_get_remote_udev_max_num);

bool hal_kernel_uda_is_phy_dev(u32 udevid)
{
    return uda_is_phy_dev(udevid);
}
KA_EXPORT_SYMBOL(hal_kernel_uda_is_phy_dev);

bool uda_is_phy_dev(u32 udevid)
{
    return (udevid < UDA_MAX_PHY_DEV_NUM);
}
KA_EXPORT_SYMBOL(uda_is_phy_dev);

void uda_set_udevid_reorder_flag(bool flag)
{
    uda_udevid_reorder_flag = flag;
}
KA_EXPORT_SYMBOL(uda_set_udevid_reorder_flag);

bool uda_udevid_is_reorder(void)
{
    return uda_udevid_reorder_flag;
}
KA_EXPORT_SYMBOL(uda_udevid_is_reorder);

void uda_set_add_id(unsigned int add_id)
{
    unsigned int i;
    static unsigned int num = 0;

    if (num >= UDA_UDEV_MAX_NUM) {
        uda_err("Invalid num. (num=%u, add_id=%u)\n", num, add_id);
        return;
    }

    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        if (g_addid_array[i] == add_id) {
            return;
        }
    }
    g_addid_array[num++] = add_id;
}
KA_EXPORT_SYMBOL(uda_set_add_id);

bool uda_is_pf_dev(u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    bool is_pf = true;

    if (udevid >= UDA_MAX_PHY_DEV_NUM) {
        return false;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return false;
    }

    is_pf = dev_inst->para.pf_flag == 1 ? true : false;
    uda_dev_inst_put(dev_inst);
    return is_pf;
}
KA_EXPORT_SYMBOL(uda_is_pf_dev);

int uda_udevid_to_logic_id(u32 udevid, u32 *logic_id)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    *logic_id = dev_inst->para.logic_id;
    uda_dev_inst_put(dev_inst);
    return 0;
}
KA_EXPORT_SYMBOL(uda_udevid_to_logic_id);

static int uda_alloc_udevid(u32 start, u32 end, u32 *udevid) // [start, end)
{
    u32 i;

    for (i = start; i < end; i++) {
        if (dev_insts[i] == NULL) {
            *udevid = i;
            return 0;
        }
    }

    return -ENOSPC;
}

u32 uda_make_udevid(struct uda_mia_dev_para *mia_para)
{
    return mia_para->phy_devid * UDA_SUB_DEV_MAX_NUM + mia_para->sub_devid + UDA_MIA_UDEV_OFFSET;
}
KA_EXPORT_SYMBOL(uda_make_udevid);

static struct uda_dev_inst *uda_create_dev_inst(struct uda_dev_type *type, struct uda_dev_para *para)
{
    static u32 inst_id = 0;
    struct uda_dev_inst *dev_inst = dbl_kzalloc(sizeof(struct uda_dev_inst), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dev_inst == NULL) {
        uda_err("Out of memory.\n");
        return NULL;
    }

    dev_inst->type = *type;
    dev_inst->para = *para;
    dev_inst->agent_flag = 0;
    dev_inst->inst_id = inst_id++;
    ka_base_kref_init(&dev_inst->ref);

    return dev_inst;
}

static void uda_destroy_dev_inst(struct uda_dev_inst *dev_inst)
{
    dbl_kfree(dev_inst);
}

struct uda_dev_inst *uda_dev_inst_get(u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    unsigned long flags;

    if (udevid >= UDA_UDEV_MAX_NUM) {
        return NULL;
    }

    // support hrtimer callback
    ka_task_read_lock_irqsave(&uda_dev_lock, flags);
    dev_inst = dev_insts[udevid];
    if (dev_inst != NULL) {
        ka_base_kref_get(&dev_inst->ref);
    }
    ka_task_read_unlock_irqrestore(&uda_dev_lock, flags);

    return dev_inst;
}

struct uda_dev_inst *uda_dev_inst_get_ex(u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;

    if (udevid >= UDA_UDEV_MAX_NUM) {
        return NULL;
    }

    ka_task_read_lock_irq(&uda_dev_lock);
    dev_inst = dev_insts[udevid];
    if (dev_inst != NULL) {
        ka_base_kref_get(&dev_inst->ref);
    }
    ka_task_read_unlock_irq(&uda_dev_lock);

    return dev_inst;
}

static void uda_dev_inst_release(ka_kref_t *kref)
{
    struct uda_dev_inst *dev_inst = ka_container_of(kref, struct uda_dev_inst, ref);

    uda_info("Remove dev success. (udevid=%u; agent_flag=%u)\n", dev_inst->udevid, dev_inst->agent_flag);
    uda_destroy_dev_inst(dev_inst);
}

void uda_dev_inst_put(struct uda_dev_inst *dev_inst)
{
    ka_base_kref_put(&dev_inst->ref, uda_dev_inst_release);
}

static int _uda_add_dev_ex(struct uda_dev_type *type, struct uda_dev_para *para, struct uda_mia_dev_para *mia_para,
                           u32 *udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if (type->object == UDA_ENTITY) {
        if ((para->udevid != UDA_INVALID_UDEVID) || (mia_para != NULL)) {
            *udevid = (mia_para == NULL) ? para->udevid : uda_make_udevid(mia_para);
            if (dev_insts[*udevid] != NULL) {
                uda_warn("has added entity dev. (udevid=%u)\n", *udevid);
                dev_insts[*udevid]->para.dev = para->dev;
                dev_insts[*udevid]->status = 0;
                return 0;
            }
        } else {
            ret = uda_alloc_udevid(0, UDA_MIA_UDEV_OFFSET, udevid);
            if (ret != 0) {
                uda_err("No udevid res\n");
                return ret;
            }
        }
    } else { /* UDA_AGENT */
        *udevid = para->udevid;
        if (dev_insts[para->udevid] == NULL) {
            uda_err("No entity dev. (udevid=%u)\n", para->udevid);
            return -EINVAL;
        }

        if (dev_insts[para->udevid]->agent_dev != NULL) {
            uda_warn("has added agent dev. (udevid=%u)\n", para->udevid);
            ((struct uda_dev_inst *)dev_insts[para->udevid]->agent_dev)->para.dev = para->dev;
            ((struct uda_dev_inst *)dev_insts[para->udevid]->agent_dev)->status = 0;
            return 0;
        }
    }

    dev_inst = uda_create_dev_inst(type, para);
    if (dev_inst == NULL) {
        return -ENOMEM;
    }

    dev_inst->udevid = *udevid;
    if (mia_para != NULL) {
        dev_inst->mia_para = *mia_para;
    }

    if (type->object == UDA_ENTITY) {
        if (dev_inst->para.char_flag == UDA_ENABLE_CHAR_NUMBERING) {
            ret = uda_access_add_dev(dev_inst->udevid, dev_inst->para.logic_id, &dev_inst->access);
        } else {
            ret = uda_access_add_dev(dev_inst->udevid, UDA_INVALID_UDEVID, &dev_inst->access);
        }
        if (ret != 0) {
            uda_destroy_dev_inst(dev_inst);
            return ret;
        }
        dev_insts[*udevid] = dev_inst;
    } else {
        dev_inst->agent_flag = 1;
        dev_insts[para->udevid]->agent_dev = dev_inst;
        if (dev_inst->para.remote_udevid != UDA_INVALID_UDEVID) {
            dev_insts[para->udevid]->para.remote_udevid = dev_inst->para.remote_udevid;
        }
    }

    uda_info("Add dev success. (udevid=%u; agent_flag=%u)\n", *udevid, dev_inst->agent_flag);
    uda_show_type("Add", type);
    return 0;
}

STATIC void uda_slot_id_sort(void)
{
    struct uda_dev_para tmp_para;
    unsigned int slot_id1 = 0;
    unsigned int slot_id2 = 0;
    unsigned int i, j;

    for (i = 0; i < reorder_insts.dev_add_num - 1; i++) {
        for (j = 0; j < reorder_insts.dev_add_num - 1 - i; j++) {
            slot_id1 = (unsigned int)reorder_para[reorder_insts.dev_para[j].add_id].slot_id;
            slot_id2 = (unsigned int)reorder_para[reorder_insts.dev_para[j + 1].add_id].slot_id;
            if (slot_id1 > slot_id2) {
                tmp_para = reorder_insts.dev_para[j];
                reorder_insts.dev_para[j] = reorder_insts.dev_para[j + 1];
                reorder_insts.dev_para[j + 1] = tmp_para;
            }
        }
    }
}

int uda_guid_compare(u32 *guid, u32 *other_guid)
{
    int i;

    for (i = 0; i < UDA_GUID_LEN; i++) {
        if (guid[UDA_GUID_LEN - i - 1] != other_guid[UDA_GUID_LEN - i - 1]) {
            if (guid[UDA_GUID_LEN - i - 1] > other_guid[UDA_GUID_LEN - i - 1]) {
                return 1;
            } else {
                return -1;
            }
        }
    }
    return 0;
}

void uda_module_id_sort(unsigned int start, unsigned int end)
{
    struct uda_dev_para tmp_para;
    unsigned int module_id1 = 0;
    unsigned int module_id2 = 0;
    unsigned int i, j;

    if (end > reorder_insts.dev_add_num) {
        return;
    }

    for (i = start; i < end - 1; i++) {
        for (j = start; j < end - 1 - i + start; j++) {
            module_id1 = reorder_para[reorder_insts.dev_para[j].add_id].module_id;
            module_id2 = reorder_para[reorder_insts.dev_para[j + 1].add_id].module_id;
            if (module_id1 > module_id2) {
                tmp_para = reorder_insts.dev_para[j];
                reorder_insts.dev_para[j] = reorder_insts.dev_para[j + 1];
                reorder_insts.dev_para[j + 1] = tmp_para;
            }
        }
    }
}

static void _uda_udevid_reorder(void)
{
    struct uda_dev_para tmp_para;
    unsigned int ub_link_count = 0;
    unsigned int group_dev_num = 1; /* 1p */
    unsigned int *guid = NULL;
    unsigned int tmp_index = 0;
    unsigned int i, j;

    for (i = 0; i < reorder_insts.dev_add_num;) {
        tmp_index = i + 1;
        ub_link_count = 0;
        if ((reorder_para[reorder_insts.dev_para[i].add_id].group_dev_num <= UDA_REORDER_GROUP_DEV_MAX_NUM) &&
            (reorder_para[reorder_insts.dev_para[i].add_id].group_dev_num != 0)) {
            group_dev_num = reorder_para[reorder_insts.dev_para[i].add_id].group_dev_num;
        } else {
            uda_warn("The set group device number is invalid. (set_group_dev_num=%u)\n",
                     reorder_para[reorder_insts.dev_para[i].add_id].group_dev_num);
            group_dev_num = 1; /* 1p */
        }
        if (group_dev_num == 1) {
            i++;
            continue;
        }
        /* 1:Establish links on all ports */
        if (reorder_para[reorder_insts.dev_para[i].add_id].ub_link_status == 1) {
            ub_link_count++;
        }
        /* Devices within a group are placed together. */
        for (j = tmp_index; j < reorder_insts.dev_add_num; j++) {
            guid = reorder_para[reorder_insts.dev_para[j].add_id].guid;
            if ((uda_guid_compare(guid, reorder_para[reorder_insts.dev_para[i].add_id].guid1) == 0) ||
                (uda_guid_compare(guid, reorder_para[reorder_insts.dev_para[i].add_id].guid2) == 0) ||
                (uda_guid_compare(guid, reorder_para[reorder_insts.dev_para[i].add_id].guid3) == 0)) {
                tmp_para = reorder_insts.dev_para[tmp_index];
                reorder_insts.dev_para[tmp_index] = reorder_insts.dev_para[j];
                reorder_insts.dev_para[j] = tmp_para;
                /* 1:Establish links on all ports */
                if (reorder_para[reorder_insts.dev_para[tmp_index].add_id].ub_link_status == 1) {
                    ub_link_count++;
                }
                tmp_index++;
            }
            if ((tmp_index - i) == group_dev_num) {
                break;
            }
        }

        if (ub_link_count == group_dev_num) {
            uda_module_id_sort(i, i + group_dev_num);
            for (j = i; (j - i) < group_dev_num; j++) {
                reorder_insts.dev_para[j].udevid = reorder_insts.udevid;
                reorder_insts.dev_para[j].logic_id = reorder_insts.logic_id;
                reorder_insts.udevid++;
                reorder_insts.logic_id++;
            }
        }
        i = tmp_index;
    }

    for (i = 0; i < reorder_insts.dev_add_num; i++) {
        if (reorder_insts.dev_para[i].udevid == UDA_INVALID_UDEVID) {
            reorder_insts.dev_para[i].udevid = reorder_insts.udevid;
            reorder_insts.dev_para[i].logic_id = reorder_insts.logic_id;
            reorder_insts.udevid++;
            reorder_insts.logic_id++;
        }
    }
}

static void _uda_master_id_reorder(void)
{
    unsigned int i, j;

    for (i = 0; i < reorder_insts.dev_add_num; i++) {
        for (j = 0; j < reorder_insts.dev_add_num; j++) {
            if (reorder_insts.dev_para[i].master_id == reorder_insts.dev_para[j].add_id) {
                reorder_insts.dev_para[i].master_id = reorder_insts.dev_para[j].udevid;
                break;
            }
        }
    }
}

STATIC void uda_allocate_logic_id(void)
{
    struct uda_dev_para tmp_para;
    unsigned int i, j;

    for (i = 0; i < reorder_insts.dev_add_num - 1; i++) {
        for (j = 0; j < reorder_insts.dev_add_num - 1 - i; j++) {
            if (reorder_insts.dev_para[j].add_id > reorder_insts.dev_para[j + 1].add_id) {
                tmp_para = reorder_insts.dev_para[j];
                reorder_insts.dev_para[j] = reorder_insts.dev_para[j + 1];
                reorder_insts.dev_para[j + 1] = tmp_para;
            }
        }
    }

    for (i = 0; i < reorder_insts.dev_add_num; i++) {
        reorder_insts.dev_para[i].logic_id = i;
        reorder_insts.dev_para[i].udevid = reorder_insts.dev_para[i].add_id;
    }
    reorder_insts.logic_id = i;
}

#ifdef EMU_ST
#define UDA_ALL_DEV_UP_WAIT_CNT 1
#else
#define UDA_ALL_DEV_UP_WAIT_CNT 300
#endif
#define UDA_ALL_DEV_UP_WAIT_TIME 1000
#define UDA_UB_PORT_ALL_LINK 1
#define UDA_UB_PORT_NOT_LINK 3

static bool uda_all_dev_is_up(void)
{
    unsigned int link_all_count = 0;
    unsigned int dev_add_num = 0;
    unsigned int add_id;
    unsigned int i;

    if (uda_udevid_reorder_flag) {
        /* 1:links on all ports; 3:No need to establish a link on the port. */
        for (i = 0; i < reorder_insts.dev_add_num; i++) {
            add_id = reorder_insts.dev_para[i].add_id;
            if (g_reorder_para_addr[add_id] == NULL) {
                continue;
            }
            ka_mm_memcpy_fromio(&reorder_para[add_id], g_reorder_para_addr[add_id], sizeof(struct udevid_reorder_para));
            if (reorder_para[add_id].group_dev_num == 1) {
                dev_add_num++;
            } else if ((reorder_para[add_id].ub_link_status == UDA_UB_PORT_ALL_LINK) ||
                       (reorder_para[add_id].ub_link_status == UDA_UB_PORT_NOT_LINK)) {
                link_all_count++;
            }
        }
        if ((link_all_count == uda_get_detected_phy_dev_num()) || (dev_add_num == uda_get_detected_phy_dev_num())) {
            return true;
        }
    } else {
        if (reorder_insts.dev_add_num == uda_get_detected_phy_dev_num()) {
            return true;
        }
    }
    return false;
}

STATIC void uda_refresh_udevid_to_addid_map(void)
{
    unsigned int dev_num = uda_get_detected_phy_dev_num();
    unsigned int udevid = reorder_insts.udevid;
    unsigned int i, j;

    if (!uda_udevid_reorder_flag) {
        return;
    }

    for (i = 0; i < dev_num; i++) {
        for (j = 0; j < reorder_insts.dev_add_num; j++) {
            if (g_addid_array[i] == reorder_insts.dev_para[j].add_id) {
                g_udevid_to_addid[reorder_insts.dev_para[j].udevid] = reorder_insts.dev_para[j].add_id;
                break;
            }
        }
        if (j == reorder_insts.dev_add_num) {
            g_udevid_to_addid[udevid] = g_addid_array[i];
            udevid++;
        }
    }
}

STATIC void uda_wait_all_dev_up_work(ka_work_struct_t *work)
{
    unsigned int udevid = 0;
    unsigned int i;
    int wait_cnt = 0;
    int ret;

    while (wait_cnt < UDA_ALL_DEV_UP_WAIT_CNT) {
        if (uda_all_dev_is_up()) {
            break;
        }

        ka_system_msleep(UDA_ALL_DEV_UP_WAIT_TIME);
        wait_cnt++;
    }
    uda_info("Reorder wait end. (dev_add_num=%u; detected_phy_dev_num=%u)\n", reorder_insts.dev_add_num,
             uda_get_detected_phy_dev_num());

    ka_task_mutex_lock(&reorder_insts.work_mutex);
    if (uda_udevid_reorder_flag) {
        uda_slot_id_sort();
        _uda_udevid_reorder();
        _uda_master_id_reorder();
    } else {
        uda_allocate_logic_id();
    }

    for (i = 0; i < reorder_insts.dev_add_num; i++) {
        ka_task_mutex_lock(&uda_dev_mutex);
        ret = _uda_add_dev_ex(&reorder_insts.dev_type, &reorder_insts.dev_para[i], NULL, &udevid);
        ka_task_mutex_unlock(&uda_dev_mutex);
        if (ret == 0) {
            (void)uda_notifier_call(udevid, &reorder_insts.dev_type, UDA_INIT);
        }
    }
    uda_info("Device added. (reorder=%d; dev_add_num=%u)\n", uda_udevid_reorder_flag, reorder_insts.dev_add_num);
    reorder_insts.wait_all_dev_work_state = UDA_ALL_DEV_UP_WORK_FINISH;
    uda_refresh_udevid_to_addid_map();
    ka_task_mutex_unlock(&reorder_insts.work_mutex);
    uda_info("Reorder task end.\n");
}

static int uda_dev_add_after_wait_work(struct uda_dev_para *para)
{
    unsigned int udevid = 0;
    int ret;

    if (uda_udevid_reorder_flag) {
        reorder_insts.dev_para[reorder_insts.dev_add_num].master_id = reorder_insts.udevid;
        reorder_insts.dev_para[reorder_insts.dev_add_num].udevid = reorder_insts.udevid;
        reorder_insts.udevid++;
    } else {
        reorder_insts.dev_para[reorder_insts.dev_add_num].master_id = para->udevid;
        reorder_insts.dev_para[reorder_insts.dev_add_num].udevid = para->udevid;
    }
    reorder_insts.dev_para[reorder_insts.dev_add_num].logic_id = reorder_insts.logic_id;
    reorder_insts.logic_id++;

    ka_task_mutex_lock(&uda_dev_mutex);
    ret = _uda_add_dev_ex(&reorder_insts.dev_type, &reorder_insts.dev_para[reorder_insts.dev_add_num], NULL, &udevid);
    ka_task_mutex_unlock(&uda_dev_mutex);
    if (ret == 0) {
        (void)uda_notifier_call(udevid, &reorder_insts.dev_type, UDA_INIT);
    }
    reorder_insts.dev_add_num++;
    uda_refresh_udevid_to_addid_map();
    return ret;
}

STATIC int uda_wait_all_dev_up(struct uda_dev_type *type, struct uda_dev_para *para)
{
    unsigned int all_dev_num = uda_get_detected_phy_dev_num();
    unsigned int udevid = 0;
    unsigned int set_add_id = para->udevid;
    int ret;
    unsigned int i;

    reorder_insts.dev_type = *type;
    for (i = 0; i < reorder_insts.dev_add_num; i++) {
        if (reorder_insts.dev_para[i].add_id == set_add_id) {
            reorder_insts.dev_para[i].dev = para->dev;
            ka_task_mutex_lock(&uda_dev_mutex);
            ret = _uda_add_dev_ex(&reorder_insts.dev_type, &reorder_insts.dev_para[i], NULL, &udevid);
            ka_task_mutex_unlock(&uda_dev_mutex);
            if (ret == 0) {
                (void)uda_notifier_call(udevid, &reorder_insts.dev_type, UDA_INIT);
            }
            return ret;
        }
    }

    ka_task_mutex_lock(&reorder_insts.work_mutex);
    if ((reorder_insts.dev_add_num >= all_dev_num) || (reorder_insts.dev_add_num >= ASCEND_PDEV_MAX_NUM)) {
        ka_task_mutex_unlock(&reorder_insts.work_mutex);
        uda_err("Reorder insts is full. (dev_add_num=%u; all_dev_num=%u)\n", reorder_insts.dev_add_num, all_dev_num);
        return -EINVAL;
    }
    reorder_insts.dev_para[reorder_insts.dev_add_num] = *para;
    reorder_insts.dev_para[reorder_insts.dev_add_num].add_id = set_add_id;
    reorder_insts.dev_para[reorder_insts.dev_add_num].udevid = UDA_INVALID_UDEVID;
    reorder_insts.dev_para[reorder_insts.dev_add_num].logic_id = UDA_INVALID_UDEVID;
    reorder_insts.dev_para[reorder_insts.dev_add_num].char_flag = UDA_ENABLE_CHAR_NUMBERING;
    if (reorder_insts.wait_all_dev_work_state == UDA_ALL_DEV_UP_WORK_FINISH) {
        ret = uda_dev_add_after_wait_work(para);
        if (ret != 0) {
            ka_task_mutex_unlock(&reorder_insts.work_mutex);
            return ret;
        }
    } else {
        reorder_insts.dev_add_num++;
    }

    /* 1:Add the first device to start work */
    if (reorder_insts.dev_add_num == 1) {
        ka_task_mutex_unlock(&reorder_insts.work_mutex);
        ka_task_schedule_work(&reorder_insts.wait_all_dev_work);
    } else {
        ka_task_mutex_unlock(&reorder_insts.work_mutex);
    }
    uda_info("Reorder add success. (dev_add_num=%u; add_id=%u)\n", reorder_insts.dev_add_num, para->udevid);

    return 0;
}

static int uda_add_dev_ex(struct uda_dev_type *type, struct uda_dev_para *para, struct uda_mia_dev_para *mia_para,
                          u32 *udevid)
{
    int ret;

    if ((type == NULL) || (para == NULL) || (udevid == NULL)) {
        uda_err("Null ptr.\n");
        return -EINVAL;
    }

    ret = uda_dev_type_valid_check(type);
    if (ret != 0) {
        uda_err("Invalid type.\n");
        return ret;
    }

    if (type->object == UDA_ENTITY) {
        if (((para->udevid != UDA_INVALID_UDEVID) && !uda_is_phy_dev(para->udevid))) {
            uda_err("Invalid para. (udevid=%u)\n", para->udevid);
            return -EINVAL;
        }
    } else {
        if (para->udevid >= UDA_UDEV_MAX_NUM) {
            uda_err("Invalid para. (udevid=%u)\n", para->udevid);
            return -EINVAL;
        }
    }

    if (UDA_WAIT_ALL_DEV_UP_FLAG && para->pf_flag == 1) {
        return uda_wait_all_dev_up(type, para);
    } else if (para->pf_flag == 0 && mia_para != NULL) {
        para->add_id = uda_make_udevid(mia_para);
    } else {
        para->add_id = para->udevid;
    }

    ka_task_mutex_lock(&uda_dev_mutex);
    ret = _uda_add_dev_ex(type, para, mia_para, udevid);
    ka_task_mutex_unlock(&uda_dev_mutex);

    if (ret == 0) {
        (void)uda_notifier_call(*udevid, type, UDA_INIT);
    }

    return ret;
}

int uda_add_dev(struct uda_dev_type *type, struct uda_dev_para *para, u32 *udevid)
{
    return uda_add_dev_ex(type, para, NULL, udevid);
}
KA_EXPORT_SYMBOL(uda_add_dev);

static int uda_mia_dev_check(struct uda_mia_dev_para *mia_para)
{
    if (!uda_is_phy_dev(mia_para->phy_devid) || (mia_para->sub_devid >= UDA_SUB_DEV_MAX_NUM) ||
        (uda_make_udevid(mia_para) >= UDA_UDEV_MAX_NUM)) {
        uda_err("Invalid para. (phy_devid=%u; sub_devid=%u)\n", mia_para->phy_devid, mia_para->sub_devid);
        return -EINVAL;
    }

    return 0;
}

int uda_add_mia_dev(struct uda_dev_type *type, struct uda_dev_para *para, struct uda_mia_dev_para *mia_para,
                    u32 *udevid)
{
    if ((para == NULL) || (mia_para == NULL)) {
        uda_err("Null ptr.\n");
        return -EINVAL;
    }

    if (para->udevid != UDA_INVALID_UDEVID) {
        uda_err("Mia udevid cannot be specified. (udevid=%u)\n", para->udevid);
        return -EINVAL;
    }

    if (uda_mia_dev_check(mia_para) != 0) {
        return -EINVAL;
    }

    para->pf_flag = 0;
    return uda_add_dev_ex(type, para, mia_para, udevid);
}
KA_EXPORT_SYMBOL(uda_add_mia_dev);

static int _uda_remove_dev(struct uda_dev_inst *dev_inst, struct uda_dev_type *type)
{
    unsigned long flags;

    if (uda_dev_type_is_match(type, &dev_inst->type)) {
        int ret = uda_access_remove_dev(dev_inst->udevid, &dev_inst->access);
        if (ret != 0) {
            return ret;
        }

        ka_task_write_lock_irqsave(&uda_dev_lock, flags);
        dev_insts[dev_inst->udevid] = NULL; /* set inst invalid, so other thread will not get it */
        ka_task_write_unlock_irqrestore(&uda_dev_lock, flags);
        if (dev_inst->agent_dev != NULL) {
            struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
            uda_warn("Not remove agent dev. (udevid=%d)\n", dev_inst->udevid);
            ka_base_kref_put(&agent_dev_inst->ref, uda_dev_inst_release);
        }
        ka_base_kref_put(&dev_inst->ref, uda_dev_inst_release);
    } else {
        struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
        dev_inst->agent_dev = NULL;
        ka_base_kref_put(&agent_dev_inst->ref, uda_dev_inst_release);
    }

    return 0;
}

int uda_remove_dev(struct uda_dev_type *type, u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if ((type == NULL) || (udevid >= UDA_UDEV_MAX_NUM)) {
        uda_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = uda_dev_type_valid_check(type);
    if (ret != 0) {
        uda_err("Invalid type.\n");
        return ret;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (!uda_dev_type_is_match(type, &dev_inst->type)) {
        struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
        if ((agent_dev_inst == NULL) || (!uda_dev_type_is_match(type, &agent_dev_inst->type))) {
            uda_err("Remove type mismatch. (udevid=%d)\n", udevid);
            uda_show_type("Remove type", type);
            uda_show_type("Dev type", &dev_inst->type);
            uda_dev_inst_put(dev_inst);
            return -ENODEV;
        }
    }

    (void)uda_notifier_call(udevid, type, UDA_UNINIT);

#ifndef EMU_ST
    if (!uda_is_phy_dev(udevid)) {
#else
    {
#endif
        ka_task_mutex_lock(&uda_dev_mutex);
        ret = _uda_remove_dev(dev_inst, type);
        ka_task_mutex_unlock(&uda_dev_mutex);
    }
    dev_inst->para.dev = NULL;
    if (dev_inst->para.add_id < UDA_UDEV_MAX_NUM) {
        g_reorder_para_addr[dev_inst->para.add_id] = NULL;
    }
    uda_update_status_by_action(&dev_inst->status, UDA_REMOVE);

    if (ret != 0) {
        (void)uda_notifier_call(udevid, type, UDA_INIT);
    }

    uda_dev_inst_put(dev_inst);

    return ret;
}
KA_EXPORT_SYMBOL(uda_remove_dev);

int uda_get_action_para(u32 udevid, enum uda_notified_action action, u32 *val)
{
    struct uda_dev_inst *dev_inst = NULL;

    if ((action < 0) || (action >= UDA_ACTION_MAX)) {
        uda_err("Invalid action. (udevid=%u; action=%d)\n", udevid, action);
        return -EINVAL;
    }

    if (val == NULL) {
        uda_err("Null ptr. (udevid=%u; action=%d)\n", udevid, action);
        return -EINVAL;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        return -EINVAL;
    }

    *val = dev_inst->action_para[action];
    uda_dev_inst_put(dev_inst);

    return 0;
}
KA_EXPORT_SYMBOL(uda_get_action_para);

static void uda_dev_ctrl_restore(struct uda_dev_inst *dev_inst, enum uda_dev_ctrl_cmd cmd, u32 val)
{
    static enum uda_notified_action ctrl_cmd_to_restore_action[UDA_CTRL_MAX] = {
        [UDA_CTRL_SUSPEND] = UDA_RESUME,
        [UDA_CTRL_RESUME] = UDA_SUSPEND,
        [UDA_CTRL_TO_MIA] = UDA_TO_SIA,
        [UDA_CTRL_TO_SIA] = UDA_TO_MIA,
        [UDA_CTRL_MIA_CHANGE] = UDA_ACTION_MAX,
        [UDA_CTRL_REMOVE] = UDA_ACTION_MAX,
        [UDA_CTRL_HOTRESET] = UDA_HOTRESET_CANCEL,
        [UDA_CTRL_HOTRESET_CANCEL] = UDA_ACTION_MAX,
        [UDA_CTRL_SHUTDOWN] = UDA_ACTION_MAX,
        [UDA_CTRL_PRE_HOTRESET] = UDA_PRE_HOTRESET_CANCEL,
        [UDA_CTRL_PRE_HOTRESET_CANCEL] = UDA_ACTION_MAX,
        [UDA_CTRL_ADD_GROUP] = UDA_DEL_GROUP,
        [UDA_CTRL_DEL_GROUP] = UDA_ACTION_MAX,
        [UDA_CTRL_UPDATE_P2P_ADDR] = UDA_ACTION_MAX,
        [UDA_CTRL_COMMUNICATION_LOST] = UDA_ACTION_MAX,
        [UDA_CTRL_AIC_ATU_CFG] = UDA_ACTION_MAX,
    };

    if (ctrl_cmd_to_restore_action[cmd] != UDA_ACTION_MAX) {
        dev_inst->action_para[ctrl_cmd_to_restore_action[cmd]] = val;
        (void)uda_notifier_call(dev_inst->udevid, &dev_inst->type, ctrl_cmd_to_restore_action[cmd]);
    }
}

static int _uda_dev_ctrl(struct uda_dev_inst *dev_inst, enum uda_dev_ctrl_cmd cmd, u32 val)
{
    int ret;
    static enum uda_notified_action ctrl_cmd_to_action[UDA_CTRL_MAX] = {
        [UDA_CTRL_SUSPEND] = UDA_SUSPEND,
        [UDA_CTRL_RESUME] = UDA_RESUME,
        [UDA_CTRL_TO_MIA] = UDA_TO_MIA,
        [UDA_CTRL_TO_SIA] = UDA_TO_SIA,
        [UDA_CTRL_MIA_CHANGE] = UDA_MIA_CHANGE,
        [UDA_CTRL_REMOVE] = UDA_REMOVE,
        [UDA_CTRL_HOTRESET] = UDA_HOTRESET,
        [UDA_CTRL_HOTRESET_CANCEL] = UDA_HOTRESET_CANCEL,
        [UDA_CTRL_SHUTDOWN] = UDA_SHUTDOWN,
        [UDA_CTRL_PRE_HOTRESET] = UDA_PRE_HOTRESET,
        [UDA_CTRL_PRE_HOTRESET_CANCEL] = UDA_PRE_HOTRESET_CANCEL,
        [UDA_CTRL_ADD_GROUP] = UDA_ADD_GROUP,
        [UDA_CTRL_DEL_GROUP] = UDA_DEL_GROUP,
        [UDA_CTRL_UPDATE_P2P_ADDR] = UDA_UPDATE_P2P_ADDR,
        [UDA_CTRL_COMMUNICATION_LOST] = UDA_COMMUNICATION_LOST,
        [UDA_CTRL_AIC_ATU_CFG] = UDA_AIC_ATU_CFG,
    };

    if (uda_is_action_conflict(dev_inst->status, ctrl_cmd_to_action[cmd])) {
        uda_err("Invalid cmd. (udevid=%u; cmd=%d; status=%x)\n", dev_inst->udevid, cmd, dev_inst->status);
        return -EINVAL;
    }

    dev_inst->action_para[ctrl_cmd_to_action[cmd]] = val;
    ret = uda_notifier_call(dev_inst->udevid, &dev_inst->type, ctrl_cmd_to_action[cmd]);
    if (ret != 0) {
        uda_dev_ctrl_restore(dev_inst, cmd, val);
        uda_debug("Notifier call failed. (udevid=%d; cmd=%d; ret=%d)\n", dev_inst->udevid, cmd, ret);
        return ret;
    }

    uda_update_status_by_action(&dev_inst->status, ctrl_cmd_to_action[cmd]);

    uda_info("Dev ctrl success. (udevid=%d; cmd=%d)\n", dev_inst->udevid, cmd);
    return 0;
}

int uda_dev_ctrl_ex(u32 udevid, enum uda_dev_ctrl_cmd cmd, u32 val)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if ((cmd < 0) || (cmd >= UDA_CTRL_MAX)) {
        uda_err("Invalid cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
        return -EINVAL;
    }

    if (uda_is_phy_dev(udevid)) {
        if ((cmd == UDA_CTRL_MIA_CHANGE) || (cmd == UDA_CTRL_REMOVE)) {
            uda_err("Phy dev does not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    } else {
        if ((cmd == UDA_CTRL_SUSPEND) || (cmd == UDA_CTRL_RESUME) || (cmd == UDA_CTRL_TO_MIA) ||
            (cmd == UDA_CTRL_TO_SIA) || (cmd == UDA_CTRL_SHUTDOWN)) {
            uda_err("Mia dev does not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        return -EINVAL;
    }

    ret = _uda_dev_ctrl(dev_inst, cmd, val);
    uda_dev_inst_put(dev_inst);

    return ret;
}
KA_EXPORT_SYMBOL(uda_dev_ctrl_ex);

int uda_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd)
{
    return uda_dev_ctrl_ex(udevid, cmd, 0);
}
KA_EXPORT_SYMBOL(uda_dev_ctrl);

int uda_agent_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if ((cmd < 0) || (cmd >= UDA_CTRL_MAX)) {
        uda_err("Invalid cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
        return -EINVAL;
    }

    if (uda_is_phy_dev(udevid)) {
        if ((cmd == UDA_CTRL_MIA_CHANGE) || (cmd == UDA_CTRL_REMOVE)) {
            uda_err("Phy dev does not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    } else {
        if ((cmd == UDA_CTRL_SUSPEND) || (cmd == UDA_CTRL_RESUME) || (cmd == UDA_CTRL_TO_MIA) ||
            (cmd == UDA_CTRL_TO_SIA)) {
            uda_err("Mia dev does not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("uda_dev_inst_get fail. (udevid=%u; cmd=%d)\n", udevid, cmd);
        return -EINVAL;
    }

    if (dev_inst->agent_dev == NULL) {
        uda_err("agent dev does not exist. (udevid=%u; cmd=%d)\n", udevid, cmd);
        uda_dev_inst_put(dev_inst);
        return -EINVAL;
    }

    ret = _uda_dev_ctrl((struct uda_dev_inst *)dev_inst->agent_dev, cmd, 0);
    uda_dev_inst_put(dev_inst);

    return ret;
}
KA_EXPORT_SYMBOL(uda_agent_dev_ctrl);

int uda_dev_set_remote_udevid(u32 udevid, u32 remote_udevid)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dev_inst->para.remote_udevid = remote_udevid;
    uda_dev_inst_put(dev_inst);

    return 0;
}
KA_EXPORT_SYMBOL(uda_dev_set_remote_udevid);

int uda_dev_get_remote_udevid(u32 udevid, u32 *remote_udevid)
{
    return uda_udevid_to_remote_udevid(udevid, remote_udevid);
}
KA_EXPORT_SYMBOL(uda_dev_get_remote_udevid);

bool uda_is_support_udev_mng(void)
{
#ifdef CFG_FEATURE_SURPORT_UDEV_MNG
    return true;
#else
    return false;
#endif
}
KA_EXPORT_SYMBOL(uda_is_support_udev_mng);

static ka_device_t *_uda_get_device(u32 udevid, bool is_agent)
{
    ka_device_t *dev = NULL;
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return NULL;
    }

    if (is_agent) {
        struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
        if (agent_dev_inst != NULL) {
            dev = agent_dev_inst->para.dev;
        }
    } else {
        dev = dev_inst->para.dev;
    }

    uda_dev_inst_put(dev_inst);

    return dev;
}

ka_device_t *hal_kernel_uda_get_device(u32 udevid)
{
    return uda_get_device(udevid);
}
KA_EXPORT_SYMBOL(hal_kernel_uda_get_device);

ka_device_t *uda_get_device(u32 udevid)
{
    return _uda_get_device(udevid, false);
}
KA_EXPORT_SYMBOL(uda_get_device);

ka_device_t *uda_get_agent_device(u32 udevid)
{
    return _uda_get_device(udevid, true);
}
KA_EXPORT_SYMBOL(uda_get_agent_device);

u32 uda_get_chip_type(u32 udevid)
{
    u32 chip_type = HISI_CHIP_UNKNOWN;
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return HISI_CHIP_UNKNOWN;
    }

    chip_type = dev_inst->para.chip_type;
    uda_dev_inst_put(dev_inst);

    return chip_type;
}
KA_EXPORT_SYMBOL(uda_get_chip_type);

u32 uda_get_master_id(u32 udevid)
{
    u32 master_id = UDA_INVALID_UDEVID;
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return UDA_INVALID_UDEVID;
    }

    master_id = dev_inst->para.master_id;
    uda_dev_inst_put(dev_inst);

    return master_id;
}
KA_EXPORT_SYMBOL(uda_get_master_id);

int uda_udevid_to_mia_devid(u32 udevid, struct uda_mia_dev_para *mia_para)
{
    struct uda_dev_inst *dev_inst = NULL;

    if (mia_para == NULL) {
        uda_err("Null ptr\n");
        return -EINVAL;
    }

    if (udevid < UDA_MIA_UDEV_OFFSET) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    *mia_para = dev_inst->mia_para;

    uda_dev_inst_put(dev_inst);
    return 0;
}
KA_EXPORT_SYMBOL(uda_udevid_to_mia_devid);

int uda_mia_devid_to_udevid(struct uda_mia_dev_para *mia_para, u32 *udevid)
{
    struct uda_dev_inst *dev_inst = NULL;

    if ((mia_para == NULL) || (udevid == NULL)) {
        uda_err("Null ptr\n");
        return -EINVAL;
    }

    if (uda_mia_dev_check(mia_para) != 0) {
        return -EINVAL;
    }

    *udevid = uda_make_udevid(mia_para);

    dev_inst = uda_dev_inst_get(*udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid para. (phy_devid=%u; sub_devid=%u)\n", mia_para->phy_devid, mia_para->sub_devid);
        return -EINVAL;
    }

    uda_dev_inst_put(dev_inst);
    return 0;
}
KA_EXPORT_SYMBOL(uda_mia_devid_to_udevid);

static int uda_agent_dev_status_check(u32 udevid, struct uda_dev_inst *dev_inst)
{
#if !defined(DRV_HOST) && defined(CFG_FEATURE_REMOTE_DEV_CHECK)
    if ((dev_inst->agent_dev == NULL) && (dev_inst->para.remote_udevid == UDA_INVALID_UDEVID)) {
        uda_debug("Remote udevid is invalid, agent dev is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }
#else
    (void)udevid;
    (void)dev_inst;
#endif
    return 0;
}

int uda_udevid_to_remote_udevid(u32 udevid, u32 *remote_udevid)
{
    int ret = 0;
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = uda_agent_dev_status_check(udevid, dev_inst);
    if (ret != 0) {
        uda_dev_inst_put(dev_inst);
        return ret;
    }

    /* current only support ep device, rc is itself */
    *remote_udevid = ((dev_inst->agent_dev != NULL) || (dev_inst->para.remote_udevid != UDA_INVALID_UDEVID)) ?
                         dev_inst->para.remote_udevid :
                         udevid;
    uda_dev_inst_put(dev_inst);

    if (*remote_udevid == UDA_INVALID_UDEVID) {
        uda_warn("Remote udevid not config. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    return 0;
}

int uda_remote_udevid_to_udevid(u32 remote_udevid, u32 *udevid)
{
    u32 i;

    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        struct uda_dev_inst *dev_inst = uda_dev_inst_get(i);
        if (dev_inst == NULL) {
            continue;
        }

        if (dev_inst->para.remote_udevid == remote_udevid) {
            *udevid = i;
            uda_dev_inst_put(dev_inst);
            return 0;
        }
        uda_dev_inst_put(dev_inst);
    }

    return -EINVAL;
}
KA_EXPORT_SYMBOL(uda_remote_udevid_to_udevid);

u32 uda_get_dev_num_all(void)
{
    u32 udevid, udev_num = 0;
    for (udevid = 0; udevid < UDA_MAX_PHY_DEV_NUM; udevid++) {
        if (dev_insts[udevid] != NULL) {
            udev_num++;
        }
    }
    return udev_num;
}

u32 uda_get_dev_num_local(void)
{
    u32 udevid, udev_num = 0;
    for (udevid = 0; udevid < UDA_MAX_PHY_DEV_NUM; udevid++) {
        if (dev_insts[udevid] != NULL && dev_insts[udevid]->type.location == UDA_LOCAL) {
            udev_num++;
        }
    }
    return udev_num;
}

u32 uda_get_dev_num(void)
{
    return uda_get_dev_num_adapt();
}
KA_EXPORT_SYMBOL(uda_get_dev_num);

u32 uda_get_mia_dev_num(void)
{
    u32 udevid, udev_num = 0;

    for (udevid = UDA_MIA_UDEV_OFFSET; udevid < UDA_UDEV_MAX_NUM; udevid++) {
        if (dev_insts[udevid] != NULL) {
            udev_num++;
        }
    }

    return udev_num;
}
KA_EXPORT_SYMBOL(uda_get_mia_dev_num);

u32 uda_get_host_id(void)
{
    return UDA_HOST_ID;
}
KA_EXPORT_SYMBOL(uda_get_host_id);

bool uda_is_udevid_exist(u32 udevid)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        return false;
    }
    uda_dev_inst_put(dev_inst);

    return true;
}
KA_EXPORT_SYMBOL(uda_is_udevid_exist);

int uda_set_udevid_reorder_para(u32 add_id, struct udevid_reorder_para *para)
{
    if (add_id >= UDA_UDEV_MAX_NUM || para == NULL) {
        uda_err("Invalid add_id or reorder_para is NULL. (add_id=%u; para_is_NULL=%d)\n", add_id, (para == NULL));
        return -EINVAL;
    }

    ka_task_mutex_lock(&uda_reorder_mutex);
    if (reorder_para == NULL) {
        reorder_para = dbl_kzalloc(sizeof(struct udevid_reorder_para) * UDA_UDEV_MAX_NUM,
                                   KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (reorder_para == NULL) {
            ka_task_mutex_unlock(&uda_reorder_mutex);
            uda_err("Out of memory.\n");
            return -ENOMEM;
        }
    }
    ka_mm_memcpy_fromio(&reorder_para[add_id], para, sizeof(struct udevid_reorder_para));
    g_reorder_para_addr[add_id] = (void *)para;
    ka_task_mutex_unlock(&uda_reorder_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(uda_set_udevid_reorder_para);

int uda_get_udevid_reorder_para(u32 udevid, struct udevid_reorder_para *para)
{
    struct uda_dev_inst *dev_inst = NULL;

    if (udevid >= UDA_UDEV_MAX_NUM || para == NULL) {
        uda_err("Invalid udevid or reorder_para is NULL. (udevid=%u; info_is_NULL=%d)\n", udevid, (para == NULL));
        return -EINVAL;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (g_reorder_para_addr[dev_inst->para.add_id] == NULL) {
        uda_dev_inst_put(dev_inst);
        uda_err("Reorder_para is NULL.\n");
        return -EINVAL;
    }
    ka_mm_memcpy_fromio(para, g_reorder_para_addr[dev_inst->para.add_id], sizeof(struct udevid_reorder_para));
    uda_dev_inst_put(dev_inst);
    return 0;
}
KA_EXPORT_SYMBOL(uda_get_udevid_reorder_para);

int uda_udevid_to_add_id(u32 udevid, u32 *add_id)
{
    struct uda_dev_inst *dev_inst = NULL;

    if (add_id == NULL) {
        uda_err("Null ptr. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dev_inst = uda_dev_inst_get_ex(udevid);
    if (dev_inst != NULL) {
        *add_id = dev_inst->para.add_id;
        uda_dev_inst_put(dev_inst);
        return 0;
    }

    if ((udevid >= UDA_UDEV_MAX_NUM) || (g_udevid_to_addid[udevid] == UDA_INVALID_UDEVID)) {
        *add_id = udevid;
    } else {
        *add_id = g_udevid_to_addid[udevid];
    }
    uda_debug("Invalid udevid. (udevid=%u)\n", udevid);
    return -EINVAL;
}
KA_EXPORT_SYMBOL(uda_udevid_to_add_id);

int uda_add_id_to_udevid(u32 add_id, u32 *udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    int i;

    if (udevid == NULL) {
        uda_err("Null ptr. (add_id=%u)\n", add_id);
        return -EINVAL;
    }

    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        dev_inst = uda_dev_inst_get_ex(i);
        if (dev_inst == NULL) {
            continue;
        }
        if (dev_inst->para.add_id == add_id) {
            *udevid = i;
            uda_dev_inst_put(dev_inst);
            return 0;
        }
        uda_dev_inst_put(dev_inst);
    }

    *udevid = add_id;
    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        if (g_udevid_to_addid[i] == add_id) {
            *udevid = i;
            break;
        }
    }
    uda_debug("Invalid add ID. (add_id=%u)\n", add_id);
    return -EINVAL;
}
KA_EXPORT_SYMBOL(uda_add_id_to_udevid);

STATIC int uda_alloc_dev_para(void)
{
    if (reorder_insts.dev_para == NULL) {
        reorder_insts.dev_para = dbl_kzalloc(sizeof(struct uda_dev_para) * ASCEND_PDEV_MAX_NUM,
                                             KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (reorder_insts.dev_para == NULL) {
            uda_err("Out of memory.\n");
            return -ENOMEM;
        }
        reorder_insts.udevid = 0;
        reorder_insts.logic_id = 0;
        reorder_insts.dev_add_num = 0;
        reorder_insts.wait_all_dev_work_state = 0;
        ka_task_mutex_init(&reorder_insts.work_mutex);
    }

    return 0;
}

int uda_dev_init(void)
{
    int ret = 0;

    (void)memset_s(dev_insts, sizeof(dev_insts), 0, sizeof(dev_insts));
    (void)memset_s(g_addid_array, sizeof(g_addid_array), 0xFF, sizeof(g_addid_array));
    (void)memset_s(g_udevid_to_addid, sizeof(g_udevid_to_addid), 0xFF, sizeof(g_udevid_to_addid));
    ka_task_mutex_init(&uda_dev_mutex);
    ka_task_mutex_init(&uda_reorder_mutex);
    ka_task_rwlock_init(&uda_dev_lock);
    if (UDA_WAIT_ALL_DEV_UP_FLAG) {
        ret = uda_alloc_dev_para();
        if (ret != 0) {
            return ret;
        }
    }
    KA_TASK_INIT_WORK(&reorder_insts.wait_all_dev_work, uda_wait_all_dev_up_work);

    return 0;
}

void uda_dev_uninit(void)
{
    u32 udevid;

    (void)ka_task_cancel_work_sync(&reorder_insts.wait_all_dev_work);
    if (reorder_insts.dev_para != NULL) {
        dbl_kfree(reorder_insts.dev_para);
        reorder_insts.dev_para = NULL;
    }

    if (reorder_para != NULL) {
        dbl_kfree(reorder_para);
        reorder_para = NULL;
    }

    for (udevid = 0; udevid < UDA_UDEV_MAX_NUM; udevid++) {
        if (dev_insts[udevid] != NULL) {
            if (dev_insts[udevid]->agent_dev != NULL) {
                uda_destroy_dev_inst(dev_insts[udevid]->agent_dev);
            }
            uda_destroy_dev_inst(dev_insts[udevid]);
        }
    }
}
