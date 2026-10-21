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
#include "ka_system_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"
#include "ka_feature_pub.h"

#include "securec.h"

#include "uda_notifier.h"
#include "uda_dev.h"
#include "uda_access.h"
#include "uda_proc_fs.h"
#include "uda_proc_fs_adapt.h"

#define PROC_FS_MODE 0400

static void uda_udev_single_show(ka_seq_file_t *seq, struct uda_dev_inst *dev_inst)
{
    char buf[TYPE_BUF_LEN];

    uda_type_to_string(&dev_inst->type, buf, TYPE_BUF_LEN);

    ka_fs_seq_printf(seq, "udevid %u type(%s) status '%s, %s' added_udevid %u chip_type %s\n", dev_inst->udevid, buf,
                     uda_is_suspend_status(dev_inst->status) ? "suspend" : "ready",
                     uda_is_mia_status(dev_inst->status) ? "mia" : "sia", dev_inst->para.udevid,
                     uda_get_chip_name(dev_inst->para.chip_type));

    if (dev_inst->para.remote_udevid != UDA_INVALID_UDEVID) {
        ka_fs_seq_printf(seq, "        remote udevid %u\n", dev_inst->para.remote_udevid);
    }

    if (dev_inst->para.add_id != UDA_INVALID_UDEVID) {
        ka_fs_seq_printf(seq, "        add_id %u\n", dev_inst->para.add_id);
    }

    if (dev_inst->para.dev != NULL) {
        ka_fs_seq_printf(seq, "        has os device\n");
    }

    if (uda_is_dev_shared(dev_inst->udevid)) {
        struct uda_access_share_node *share_node = NULL;
        ka_fs_seq_printf(seq, "        shared dev used by ns id:");
        ka_task_mutex_lock(&dev_inst->access.mutex);
        ka_list_for_each_entry(share_node, &dev_inst->access.share_head, node)
        {
            ka_fs_seq_printf(seq, " %u", share_node->ns_id);
        }
        ka_fs_seq_printf(seq, "\n");
        ka_task_mutex_unlock(&dev_inst->access.mutex);
    } else {
        if (dev_inst->access.ns != NULL) {
            ka_fs_seq_printf(seq, "        has used by container, ns id %u\n", dev_inst->access.ns_id);
        }
    }

    if (!uda_is_phy_dev(dev_inst->udevid)) {
        ka_fs_seq_printf(seq, "        mia para: phy_devid %u sub_devid %u\n", dev_inst->mia_para.phy_devid,
                         dev_inst->mia_para.sub_devid);
    }

    if (dev_inst->agent_dev != NULL) {
        ka_fs_seq_printf(seq, "        has agent device\n");
    }

    ka_fs_seq_printf(seq, "\n");
}

static int uda_udev_show(ka_seq_file_t *seq, void *offset)
{
    u32 i;

    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        struct uda_dev_inst *dev_inst = uda_dev_inst_get(i);
        if (dev_inst != NULL) {
            uda_udev_single_show(seq, dev_inst);
            uda_dev_inst_put(dev_inst);
        }
    }

    return 0;
}

int uda_udev_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, uda_udev_show, inode->i_private);
}

static void uda_ns_node_single_show(struct uda_ns_node *ns_node, void *priv)
{
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    u32 devid;

    ka_fs_seq_printf(seq, "ns_id %u identify %llx root_tgid %u dev_num %u namespace %pK%s, udev list:\n",
                     ns_node->ns_id, ns_node->identify, ns_node->root_tgid, ns_node->dev_num, ns_node->ns,
                     (ns_node->ns == uda_get_current_ns() ? "(#)" : ""));
    ka_fs_seq_printf(seq, "logic_devid udevid\n");
    for (devid = 0; (devid < ns_node->dev_num) && (devid < UDA_DEV_MAX_NUM); devid++) {
        ka_fs_seq_printf(seq, "%8u %8u\n", devid, ns_node->devid_to_udevid[devid]);
    }
    ka_fs_seq_printf(seq, "\n\n");
}

static int uda_ns_node_show(ka_seq_file_t *seq, void *offset)
{
    if (ka_feature_is_enable()) {
        uda_recycle_idle_ns_node_immediately();
    }
    uda_for_each_ns_node_safe(seq, uda_ns_node_single_show);
    return 0;
}

int uda_ns_node_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, uda_ns_node_show, inode->i_private);
}

static void _uda_notifier_show(struct uda_dev_type *type, struct uda_notifiers *notifiers, void *priv)
{
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    char buf[TYPE_BUF_LEN];
    int i;
    bool has_notifier = false;

    ka_task_down_read(&notifiers->sem);
    for (i = 0; i < UDA_PRI_MAX; i++) {
        if (!ka_list_empty(&notifiers->pri_head[i])) {
            has_notifier = true;
            break;
        }
    }

    if (has_notifier) {
        uda_type_to_string(type, buf, TYPE_BUF_LEN);
        ka_fs_seq_printf(seq, "notifier type: %s\n", buf);
    }

    for (i = 0; i < UDA_PRI_MAX; i++) {
        ka_list_head_t *nf_head = &notifiers->pri_head[i];
        struct uda_notifier_node *nf = NULL;

        if (!ka_list_empty(nf_head)) {
            ka_fs_seq_printf(seq, "    pri %d\n", i);

            ka_list_for_each_entry(nf, nf_head, node)
            {
                ka_fs_seq_printf(seq, "        notifier: %s %u %u\n", nf->notifier, nf->call_count, nf->call_finish);
            }
        }
    }
    ka_task_up_read(&notifiers->sem);
    if (has_notifier) {
        ka_fs_seq_printf(seq, "\n");
    }
}

static int uda_notifier_show(ka_seq_file_t *seq, void *offset)
{
    uda_for_each_notifiers(seq, _uda_notifier_show);
    return 0;
}

int uda_notifier_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, uda_notifier_show, inode->i_private);
}

static int uda_detected_device_show(ka_seq_file_t *seq, void *offset)
{
    ka_fs_seq_printf(seq, "detected_dev_num: %u\n", uda_get_detected_phy_dev_num());
    return 0;
}

int uda_detected_device_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, uda_detected_device_show, inode->i_private);
}

static STATIC_PROCFS_FILE_FUNC_OPS_OPEN(udevice_ops, uda_udev_open);
static STATIC_PROCFS_FILE_FUNC_OPS_OPEN(namespace_node_ops, uda_ns_node_open);
static STATIC_PROCFS_FILE_FUNC_OPS_OPEN(notifier_ops, uda_notifier_open);
static STATIC_PROCFS_FILE_FUNC_OPS_OPEN(detected_device_ops, uda_detected_device_open);

void uda_proc_fs_init(void)
{
    ka_proc_dir_entry_t *top_entry = ka_fs_proc_mkdir("uda", NULL);
    if (top_entry != NULL) {
        (void)ka_fs_proc_create_data("udevice", PROC_FS_MODE, top_entry, &udevice_ops, NULL);
        (void)ka_fs_proc_create_data("namespace_node", PROC_FS_MODE, top_entry, &namespace_node_ops, NULL);
        (void)ka_fs_proc_create_data("notifiers", PROC_FS_MODE, top_entry, &notifier_ops, NULL);
        (void)ka_fs_proc_create_data("detected_device", PROC_FS_MODE, top_entry, &detected_device_ops, NULL);
    }
}

void uda_proc_fs_uninit(void)
{
    (void)ka_fs_remove_proc_subtree("uda", NULL);
}
