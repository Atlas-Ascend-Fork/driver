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
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"

#include "soft_fault_define.h"
#include "devdrv_user_common.h"
#include "pbl_mem_alloc_interface.h"
#include "pbl_davinci_api.h"
#include "pbl_runenv_config.h"
#include "pbl_uda.h"
#include "dms_sensor_interface.h"
#include "ascend_dev_num.h"
#include "drv_kernel_soft.h"

#define SOFT_PARSE_HANDLE(node_type, sensor_idx, sensor_type, handle) \
    do {                                                              \
        node_type = (handle) >> SF_OFFSET_32BIT;                      \
        sensor_idx = ((handle) & SF_MASK_32BIT) >> SF_OFFSET_16BIT;   \
        sensor_type = (handle) & SF_MASK_16BIT;                       \
    } while (0)

#define SOFT_PRINT_REGISTER_LOG(client, user_node, cfg, handle, user_num, flag)                                       \
    soft_drv_event("soft node register success. (dev_id=%u; name=%s; pid=%d; user_id=%u; node_id=%u; node_type=0x%x;" \
                   " sensor_type=0x%x; assert_event_mask=0x%x; deassert_event_mask=0x%x; user_num=%u; node_num=%u;"   \
                   " handle=0x%llx; sensor_obj_num=%u; first_register=%d)\n",                                         \
                   (user_node)->dev_id, (cfg)->name, (client)->pid, (client)->user_id, (user_node)->node_id,          \
                   (cfg)->node_type, (cfg)->sensor_type, (cfg)->assert_event_mask, (cfg)->deassert_event_mask,        \
                   (user_num), (client)->node_num, *(handle), (user_node)->sensor_obj_num, (flag))

STATIC DMS_SENSOR_TYPE_T g_sensor_type_list[] = {DMS_SEN_TYPE_HEARTBEAT, DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT,
                                                 DMS_SEN_TYPE_SAFETY_SENSOR};

const struct soft_fault_acc g_soft_fault_whitelist[] = {
#include "fms/soft_fault_whitelist.inc"
};

const struct soft_fault_acc g_soft_fault_whitelist_legacy[] = {
#include "fms/soft_fault_whitelist_legacy.inc"
};

STATIC void soft_fault_get_whitelist(u32 devid, const struct soft_fault_acc **whitelist, u32 *num)
{
    u32 chip_type = uda_get_chip_type(devid);
    if ((chip_type == HISI_CLOUD_V4) || (chip_type == HISI_CLOUD_V5)) {
        *whitelist = g_soft_fault_whitelist;
        *num = sizeof(g_soft_fault_whitelist) / sizeof(g_soft_fault_whitelist[0]);
    } else {
        *whitelist = g_soft_fault_whitelist_legacy;
        *num = sizeof(g_soft_fault_whitelist_legacy) / sizeof(g_soft_fault_whitelist_legacy[0]);
    }
    soft_drv_event("Get fault whitelist. (devid=%u; chip_type=0x%x; num=0x%x)\n", devid, chip_type, *num);
}

STATIC int soft_fault_get_fault_index(uint64_t handle, int val, const struct soft_fault_acc *whitelist, u32 white_num,
                                      u32 *fault_index)
{
    u32 i;
    u32 node_type, sensor_idx, sensor_type;

    SOFT_PARSE_HANDLE(node_type, sensor_idx, sensor_type, handle);
    for (i = 0; i < white_num; i++) {
        if ((whitelist[i].node_type == node_type) && (whitelist[i].sensor_type == sensor_type) &&
            (whitelist[i].err_type == (u32)val)) {
            *fault_index = i;
            return 0;
        }
    }

    soft_drv_debug("Not support node_type sensor_type err_type. (node_type=0x%x; sensor_type=0x%x; err_type=0x%x)\n",
                   node_type, sensor_type, val);
    return -EINVAL;
}

STATIC u32 soft_fault_get_cur_user_role(void)
{
    u32 acc;
    const ka_cred_t *cred = ka_current_cred();

    /* check user role */
    if ((cred != NULL) && (cred->euid.val == 0)) {
        acc = SOFT_FAULT_ACC_ROOT;
    } else {
        if ((cred != NULL) && davinci_intf_confirm_user()) {
            if (davinci_intf_get_manage_group() == cred->egid.val) {
                acc = SOFT_FAULT_ACC_DM_USER;
            } else {
                acc = SOFT_FAULT_ACC_OPERATE;
            }
        } else {
            acc = SOFT_FAULT_ACC_USER;
        }
    }
    return acc;
}

STATIC bool soft_fault_check_user_role(u32 user_role_cur, u32 user_role_white)
{
    if ((user_role_cur & user_role_white) == 0) {
        return false;
    }
    return true;
}

STATIC int soft_fault_get_cur_run_env(u32 *run_env_cur)
{
    if (run_in_normal_docker() == true) {
        *run_env_cur = SOFT_FAULT_ENV_DOCKER;
    } else if (run_in_admin_docker() == true) {
        *run_env_cur = SOFT_FAULT_ENV_ADMIN_DOCKER;
    } else if (run_in_virtual_mach() == true) {
        *run_env_cur = SOFT_FAULT_ENV_VIRTUAL;
    } else {
        *run_env_cur = SOFT_FAULT_ENV_PHYSICAL;
    }

    return 0;
}

STATIC bool soft_fault_check_run_env(u32 run_env_cur, u32 run_env_white)
{
    if ((run_env_cur & run_env_white) == 0) {
        return false;
    }

    return true;
}

STATIC int soft_fault_get_cur_process_name(char **proc_name_cur)
{
#ifndef DMS_UT
    struct mm_struct *active_mm = current->active_mm;

    if ((active_mm == NULL) || (active_mm->exe_file == NULL) || (active_mm->exe_file->f_path.dentry == NULL) ||
        (active_mm->exe_file->f_path.dentry->d_name.name == NULL)) {
        soft_drv_err("The active_mm parameter is invalid.\n");
        return -EINVAL;
    }

    *proc_name_cur = (char *)active_mm->exe_file->f_path.dentry->d_name.name;
#else
    *proc_name_cur = "";
#endif
    return 0;
}

STATIC bool soft_fault_check_proc_name(char *proc_name_cur, const char *proc_name_white)
{
    if (proc_name_white[0] == '\0') {
        return true;
    }

    if (strlen(proc_name_cur) == strlen(proc_name_white)) {
        if (memcmp(proc_name_cur, proc_name_white, strlen(proc_name_cur)) == 0) {
            return true;
        }
    }

    return false;
}

STATIC bool soft_fault_whitelist_check(u32 devid, uint64_t handle, int val)
{
    int ret;
    u32 fault_index = 0;
    u32 user_role_cur = 0;
    u32 run_env_cur = 0;
    char *proc_name_cur = NULL;
    const struct soft_fault_acc *whitelist = NULL;
    u32 white_num = 0;

    soft_fault_get_whitelist(devid, &whitelist, &white_num);
    if (soft_fault_get_fault_index(handle, val, whitelist, white_num, &fault_index) != 0) {
        soft_drv_debug("Not support handle val. (handle=0x%llx; val=%d)\n", handle, val);
        return false;
    }

    user_role_cur = soft_fault_get_cur_user_role();
    if (!soft_fault_check_user_role(user_role_cur, whitelist[fault_index].user_acc)) {
        soft_drv_err("Check user role fail. (fault_index=0x%x; user_role_cur=0x%x; user_role_white=0x%x)\n",
                     fault_index, user_role_cur, whitelist[fault_index].user_acc);
        return false;
    }

    ret = soft_fault_get_cur_run_env(&run_env_cur);
    if (ret != 0) {
        return false;
    }
    if (!soft_fault_check_run_env(run_env_cur, whitelist[fault_index].run_env)) {
        soft_drv_err("Check run env fail. (fault_index=0x%x; run_env_cur=0x%x, run_env_white=0x%x)\n", fault_index,
                     run_env_cur, whitelist[fault_index].run_env);
        return false;
    }

    ret = soft_fault_get_cur_process_name(&proc_name_cur);
    if (ret != 0) {
        return false;
    }

    if (!soft_fault_check_proc_name(proc_name_cur, whitelist[fault_index].proc_name)) {
        soft_drv_err("Check process name fail. (fault_index=0x%x; proc_name_cur=%s; proc_name_white=%s)\n", fault_index,
                     proc_name_cur, whitelist[fault_index].proc_name);
        return false;
    }

    return true;
}

STATIC bool soft_sensor_whitelist_check(u32 devid, unsigned short node_type, unsigned char sensor_type)
{
    int ret;
    u32 i;
    u32 user_role_cur = 0;
    u32 run_env_cur = 0;
    char *proc_name_cur = NULL;
    const struct soft_fault_acc *whitelist = NULL;
    u32 white_num = 0;
    bool find_flag = false;

    soft_fault_get_whitelist(devid, &whitelist, &white_num);
    user_role_cur = soft_fault_get_cur_user_role();

    ret = soft_fault_get_cur_run_env(&run_env_cur);
    if (ret != 0) {
        return false;
    }

    ret = soft_fault_get_cur_process_name(&proc_name_cur);
    if (ret != 0) {
        return false;
    }

    for (i = 0; i < white_num; i++) {
        if ((whitelist[i].node_type == node_type) && (whitelist[i].sensor_type == sensor_type)) {
            find_flag = true;
            if ((soft_fault_check_user_role(user_role_cur, whitelist[i].user_acc)) &&
                (soft_fault_check_run_env(run_env_cur, whitelist[i].run_env)) &&
                (soft_fault_check_proc_name(proc_name_cur, whitelist[i].proc_name))) {
                return true;
            } else {
                continue;
            }
        }
    }

    if (find_flag == false) {
        soft_drv_debug("Not support node_type sensor_type. (node_type=0x%x; sensor_type=0x%x)\n", node_type,
                       sensor_type);
    } else {
        soft_drv_err("Check sensor whitelist fail. (node_type=0x%x; sensor_type=0x%x; user_role_cur=0x%x; "
                     "run_env_cur=0x%x; proc_name_cur=%s)\n",
                     node_type, sensor_type, user_role_cur, run_env_cur, proc_name_cur);
    }

    return false;
}

STATIC int soft_sensor_para_check(struct dms_sensor_node_cfg cfg)
{
    int i;

    if ((cfg.node_type < HAL_DMS_DEV_TYPE_BASE_SERVCIE) || (cfg.node_type >= HAL_DMS_DEV_TYPE_MAX)) {
        return -EINVAL;
    }

    for (i = 0; i < sizeof(g_sensor_type_list) / sizeof(g_sensor_type_list[0]); i++) {
        if (g_sensor_type_list[i] == cfg.sensor_type) {
            return 0;
        }
    }

    return -EINVAL;
}

STATIC int soft_trans_and_check_id(u32 logic_id, u32 *phy_id, u32 *vfid)
{
    int ret;

    if (logic_id >= ASCEND_DEV_MAX_NUM) {
        soft_drv_err("Wrong logic id. (logic_id=%u)\n", logic_id);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(logic_id, phy_id, vfid);
    if (ret != 0) {
        soft_drv_err("Transfer logic id to phy id failed. (logic_id=%u; ret=%d)\n", logic_id, ret);
        return ret;
    }

    if (*phy_id >= ASCEND_DEV_MAX_NUM) {
        soft_drv_err("wrong phy id. (logic_id=%u; phy_id=%u)\n", logic_id, *phy_id);
        return -EINVAL;
    }

    return 0;
}

STATIC void soft_get_default_event_mask(unsigned int sensor_type, unsigned int *assert_event_mask,
                                        unsigned int *deassert_event_mask)
{
    if (sensor_type == DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT) {
        *assert_event_mask = 0x3FF;   /* 0x3FF: 0-9 */
        *deassert_event_mask = 0x3FF; /* 0x3FF: 0-9 */
    } else if (sensor_type == DMS_SEN_TYPE_SAFETY_SENSOR) {
        *assert_event_mask = 0x3FF;
        *deassert_event_mask = DST_MASK;
    } else {
        *assert_event_mask = AST_MASK;
        *deassert_event_mask = DST_MASK;
    }
    return;
}

STATIC int user_sensor_register(struct soft_dev *user_node, struct dms_sensor_node_cfg *cfg, unsigned int user_id,
                                unsigned int sub_id)
{
    int ret;
    int pid = ka_task_get_current_tgid();
    unsigned int assert_event_mask = cfg->assert_event_mask;
    unsigned int deassert_event_mask = cfg->deassert_event_mask;
    struct dms_sensor_object_cfg *obj = &user_node->sensor_obj_table[sub_id];
    struct dms_sensor_object_cfg s_cfg = SOFT_SENSOR_DEF(cfg->sensor_type, "hb", 0UL, user_id, SF_SUB_ID0,
                                                         assert_event_mask, deassert_event_mask, SF_SENSOR_SCAN_TIME,
                                                         soft_fault_event_scan, pid);

    /* This is compatibility processing, when the caller not uses the extension field,
     * set default event mask */
    if ((assert_event_mask == 0) && (deassert_event_mask == 0)) {
        soft_get_default_event_mask(cfg->sensor_type, &assert_event_mask, &deassert_event_mask);
        s_cfg.assert_event_mask = assert_event_mask;
        s_cfg.deassert_event_mask = deassert_event_mask;
    }

    *obj = s_cfg;
    obj->private_data = soft_combine_private_data(user_node->dev_id, user_id, cfg->node_type, user_node->node_id,
                                                  sub_id);
    ret = snprintf_s(obj->sensor_name, DMS_SENSOR_DESCRIPT_LENGTH, DMS_SENSOR_DESCRIPT_LENGTH, "%s", cfg->name);
    if (ret <= 0) {
        soft_drv_err("snprintf_s sensor_name failed. (name=%s; ret=%d)\n", cfg->name, ret);
        return ret;
    }

    ret = dms_sensor_register_for_userspace(&user_node->dev_node, obj);
    if (ret != 0) {
        soft_drv_err("Register sensor failed. (dev_id=%u; user_name=%s; node_type=0x%x; sensor_type=0x%x; ret=%d)\n",
                     user_node->dev_id, ka_task_get_current_comm(), cfg->node_type, cfg->sensor_type, ret);
        return ret;
    }

    return 0;
}

STATIC int user_dev_node_register(unsigned int dev_id, unsigned int user_id, struct soft_dev *user_node,
                                  struct dms_sensor_node_cfg *cfg, uint64_t *handle)
{
    int ret;
    unsigned int sensor_idx;
    struct dms_node_operations *soft_ops = soft_get_ops();
    struct dms_node s_node = SOFT_NODE_DEF(cfg->node_type, "user", dev_id, user_node->node_id, soft_ops);

    if (user_node->sensor_obj_num >= SF_SUB_ID_MAX) {
        soft_drv_warn("exceed max sensor num. (dev_id=%u; sensor_obj_num=%u)\n", dev_id, user_node->sensor_obj_num);
        return -EBUSY;
    }

    if (user_node->registered == 0) { /* dev_node register once only */
        user_node->dev_node = s_node;
        ret = snprintf_s(user_node->dev_node.node_name, DMS_MAX_DEV_NAME_LEN, DMS_MAX_DEV_NAME_LEN,
                         "%02u_%02d_%03x_%.5s", dev_id, user_node->node_id, cfg->node_type, cfg->name);
        if (ret <= 0) {
            soft_drv_err("snprintf_s failed. (dev_id=%u; node_id=%u; ret=%d)\n", dev_id, user_node->node_id, ret);
            return ret;
        }

        ret = dms_register_dev_node(&user_node->dev_node);
        if (ret != 0) {
            soft_drv_err("register dev_node failed. (dev_id=%u; user_name=%s; node_type=%u; ret=%d)\n", dev_id,
                         ka_task_get_current_comm(), cfg->node_type, ret);
            return ret;
        }
    }

    for (sensor_idx = 0; sensor_idx < SF_SUB_ID_MAX; sensor_idx++) {
        if ((user_node->sensor_obj_registered[sensor_idx]) &&
            (user_node->sensor_obj_table[sensor_idx].sensor_type == cfg->sensor_type) &&
            (ka_base_strcmp(user_node->sensor_obj_table[sensor_idx].sensor_name, cfg->name) == 0)) {
            soft_drv_warn("sensor already registered. (dev_id=%u; sensor_type=0x%x; name=%s)\n", dev_id,
                          cfg->sensor_type, cfg->name);
            goto OUT;
        }
    }

    /* get the first free sensor table index */
    for (sensor_idx = 0; sensor_idx < SF_SUB_ID_MAX; sensor_idx++) {
        if (user_node->sensor_obj_registered[sensor_idx] == 0) {
            break;
        }
    }

    if (sensor_idx >= SF_SUB_ID_MAX) {
        soft_drv_warn("exceed max sensor num. (dev_id=%u; sensor_idx=%u)\n", dev_id, sensor_idx);
        return -EBUSY;
    }

    ret = user_sensor_register(user_node, cfg, user_id, sensor_idx); /* register a new sensor obj , index i */
    if (ret != 0) {
        soft_drv_err("user sensor register failed. "
                     "(dev_id=%u, node_type=0x%x, sensor_type=0x%x, sensor_id=%u; ret=%d)\n",
                     dev_id, cfg->node_type, cfg->sensor_type, sensor_idx, ret);
        goto ERROR;
    }

    user_node->sensor_obj_registered[sensor_idx] = 1;
    user_node->sensor_obj_num++;
    user_node->registered = 1;
OUT:
    *handle = ((uint64_t)cfg->node_type << SF_OFFSET_32BIT) | (sensor_idx << SF_OFFSET_16BIT) | cfg->sensor_type;
    return 0;
ERROR:
    if (user_node->registered == 0) {
        (void)dms_unregister_dev_node(&user_node->dev_node);
    }
    return ret;
}

STATIC struct soft_dev *soft_dev_node_get(unsigned int dev_id, struct soft_dev_client *client,
                                          struct dms_sensor_node_cfg *cfg, unsigned int user_id, int *first_register)
{
    struct soft_dev *pos = NULL;
    struct soft_dev *n = NULL;
    struct soft_dev *s_dev = NULL;

    *first_register = 1;
    ka_list_for_each_entry_safe(pos, n, &client->head, list)
    {
        if ((pos->registered == 1) && (pos->dev_node.node_type == cfg->node_type)) {
            *first_register = 0;
            break;
        }
    }

    if (*first_register) {
        s_dev = dbl_kzalloc(sizeof(struct soft_dev), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (s_dev == NULL) {
            soft_drv_err("ka_mm_kzalloc soft_dev failed. (user_name=%s)\n", ka_task_get_current_comm());
            return NULL;
        }
        soft_one_dev_init(s_dev);
        s_dev->dev_id = dev_id;
        s_dev->node_id = user_id;
    } else {
        s_dev = pos;
    }

    return s_dev;
}

STATIC void soft_dev_client_add(struct soft_dev_client *client, struct soft_dev *user_node, uint32_t user_id,
                                uint32_t *user_num, int first_register)
{
    if (client->registered == 0) {
        client->pid = ka_task_get_current_tgid();
        client->user_id = user_id;
        client->registered = 1;
        (*user_num)++;
    }

    if (first_register != 0) {
        ka_list_add(&user_node->list, &client->head);
        client->node_num++; /* first register, node_num add 1 */
    }

    return;
}

STATIC struct soft_dev_client *dms_soft_get_client(u32 dev_id, struct drv_soft_ctrl *soft_ctrl, u32 *user_id)
{
    int i;

    for (i = SF_SENSOR_USER; i < SF_USER_MAX; i++) {
        if (soft_ctrl->s_dev_t[dev_id][i]->pid == ka_task_get_current_tgid()) {
            *user_id = i;
            return soft_ctrl->s_dev_t[dev_id][i]; /* find pid, the user already registered */
        }
    }

    for (i = SF_SENSOR_USER; i < SF_USER_MAX; i++) {
        if (soft_ctrl->s_dev_t[dev_id][i]->registered == 0) {
            *user_id = i;
            return soft_ctrl->s_dev_t[dev_id][i]; /* a new client, get a free element for it */
        }
    }

    return NULL;
}

STATIC int dms_soft_node_register(unsigned int dev_id, struct dms_sensor_node_cfg *cfg, uint64_t *handle)
{
    int ret;
    int first_register = 0;
    unsigned int user_id = 0;
    struct soft_dev_client *client = NULL;
    struct soft_dev *user_node = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    ka_task_mutex_lock(&soft_ctrl->mutex[dev_id]);

    client = dms_soft_get_client(dev_id, soft_ctrl, &user_id);
    if (client == NULL) {
        soft_drv_warn("exceed max user num. (dev_id=%u; user_num=%u)\n", dev_id, soft_ctrl->user_num[dev_id]);
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return -EBUSY;
    }

    if (client->node_num >= SF_USER_NODE_MAX) {
        soft_drv_warn("Exceed max user node num. (dev_id=%u; node_num=%u)\n", dev_id, client->node_num);
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return -EBUSY;
    }

    user_node = soft_dev_node_get(dev_id, client, cfg, user_id, &first_register);
    if (user_node == NULL) {
        soft_drv_err("get new user soft_dev failed. (user_name=%s)\n", ka_task_get_current_comm());
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return -ENOMEM;
    }

    ret = user_dev_node_register(dev_id, user_id, user_node, cfg, handle);
    if (ret != 0) {
        soft_drv_err("register user dev_node failed. (dev_id=%u; user_name=%s; node_id=%u; ret=%d)\n", dev_id,
                     ka_task_get_current_comm(), user_node->node_id, ret);
        if (first_register != 0) {
            dbl_kfree(user_node);
            user_node = NULL;
        }
        goto out;
    }

    soft_dev_client_add(client, user_node, user_id, &soft_ctrl->user_num[dev_id], first_register);
    SOFT_PRINT_REGISTER_LOG(client, user_node, cfg, handle, soft_ctrl->user_num[dev_id], first_register);

out:
    ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
    return ret;
}

STATIC int user_sensor_object_unregister(struct soft_dev *pos, unsigned int sensor_idx)
{
    struct dms_sensor_object_cfg *obj = NULL;
    int ret;

    obj = &pos->sensor_obj_table[sensor_idx];
    ret = dms_sensor_object_unregister(&pos->dev_node, obj);
    if (ret != 0) {
        soft_drv_err("User sensor object unregister failed. (sensor_index=%u)\n", sensor_idx);
        return ret;
    }

    pos->sensor_obj_registered[sensor_idx] = 0;
    soft_fault_event_free(&pos->sensor_event_queue[sensor_idx]);
    pos->sensor_obj_num--;
    return DRV_ERROR_NONE;
}

STATIC struct soft_dev *dms_soft_dev_node_get(struct soft_dev_client *client, unsigned int node_type)
{
    struct soft_dev *pos = NULL;
    struct soft_dev *pos_temp = NULL;
    struct soft_dev *n = NULL;

    ka_list_for_each_entry_safe(pos_temp, n, &client->head, list)
    {
        if ((pos_temp->registered == 1) && (pos_temp->dev_node.node_type == node_type)) {
            pos = pos_temp;
            break;
        }
    }
    return pos;
}

STATIC int dms_soft_node_unregister(unsigned int dev_id, uint64_t handle)
{
    int i;
    int ret;
    unsigned int node_type, sensor_idx, sensor_type;
    struct soft_dev_client *client = NULL;
    struct soft_dev *pos = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    ka_task_mutex_lock(&soft_ctrl->mutex[dev_id]);
    for (i = SF_SENSOR_USER; i < SF_USER_MAX; i++) {
        if ((soft_ctrl->s_dev_t[dev_id][i]->pid == ka_task_get_current_tgid()) &&
            (soft_ctrl->s_dev_t[dev_id][i]->registered == 1)) {
            client = soft_ctrl->s_dev_t[dev_id][i];
            break;
        }
    }

    SOFT_PARSE_HANDLE(node_type, sensor_idx, sensor_type, handle);
    if ((client == NULL) || (sensor_idx >= SF_SUB_ID_MAX)) {
        soft_drv_err("Invalid para. (dev_id=%u; handle=0x%llx)\n", dev_id, handle);
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return -EINVAL;
    }

    pos = dms_soft_dev_node_get(client, node_type);
    if (pos == NULL) {
        soft_drv_err("Invalid para. (node_type=%u; sensor_index=%u)\n", node_type, sensor_idx);
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = user_sensor_object_unregister(pos, sensor_idx);
    if (ret != 0) {
        soft_drv_err("user sensor object unregister failed."
                     " (dev_id=%u; pid=%d; user_id=%u; node_type=%u; sensor_idx=%u; sensor_type=%u)\n",
                     dev_id, client->pid, client->user_id, node_type, sensor_idx, sensor_type);
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return ret;
    }

    if (pos->sensor_obj_num == 0) {
        soft_free_one_node(client, node_type);
    }
    soft_ctrl->user_num[dev_id] -= (client->node_num == 0) ? (1) : (0);
    soft_drv_event("soft node unregister success. (dev_id=%u; pid=%d; user_id=%u; node_type=0x%llx;"
                   " sensor_type=0x%x; user_num=%u; handle=0x%llx)\n",
                   dev_id, client->pid, client->user_id, handle >> SF_OFFSET_32BIT, sensor_type,
                   soft_ctrl->user_num[dev_id], handle);

    ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
    return DRV_ERROR_NONE;
}

static inline int sensor_event_state_convert_assertion(struct dms_sensor_object_cfg *obj_cfg, int event_data)
{
    if (event_data < 0 || event_data >= SF_OFFSET_32BIT) {
        return GENERAL_EVENT_TYPE_OCCUR;
    }

    if ((obj_cfg->assert_event_mask & (1U << event_data)) && !(obj_cfg->deassert_event_mask & (1U << event_data))) {
        return GENERAL_EVENT_TYPE_ONE_TIME;
    }

    return GENERAL_EVENT_TYPE_OCCUR;
}

STATIC int dms_update_sensor_state(unsigned int dev_id, uint64_t handle, int val, int assertion)
{
    int i, ret, event_type;
    unsigned int node_type, sensor_idx, sensor_type;
    struct soft_fault event = {0};
    struct dms_sensor_object_cfg *p_cfg = NULL;
    struct soft_dev_client *client = NULL;
    struct soft_dev *pos = NULL;
    struct soft_dev *n = NULL;
    struct soft_dev *user_node = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    ka_task_mutex_lock(&soft_ctrl->mutex[dev_id]);
    for (i = SF_SENSOR_USER; i < SF_USER_MAX; i++) {
        if ((soft_ctrl->s_dev_t[dev_id][i]->pid == ka_task_get_current_tgid()) &&
            (soft_ctrl->s_dev_t[dev_id][i]->registered == 1)) {
            client = soft_ctrl->s_dev_t[dev_id][i];
            break;
        }
    }

    SOFT_PARSE_HANDLE(node_type, sensor_idx, sensor_type, handle);
    if ((client == NULL) || (sensor_idx >= SF_SUB_ID_MAX)) {
        soft_drv_err("Invalid para. (dev_id=%u; handle=0x%llx; client=%u)\n", dev_id, handle, (client != NULL));
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return -EINVAL;
    }

    ka_list_for_each_entry_safe(pos, n, &client->head, list)
    {
        if ((pos->dev_node.node_type == node_type) && (pos->registered == 1)) {
            user_node = pos;
            break;
        }
    }

    if (user_node == NULL) {
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        soft_drv_err("Can not find a matching node. (handle=%llu; node_type=0x%x; sensor_type=0x%x; sensor_idx=%u)\n",
                     handle, node_type, sensor_type, sensor_idx);
        return -EINVAL;
    }

    for (i = 0; i < SF_SUB_ID_MAX; i++) {
        if (!pos->sensor_obj_registered[i]) {
            continue;
        }

        p_cfg = &user_node->sensor_obj_table[i];
        if ((p_cfg->sensor_type != sensor_type) || (i != sensor_idx)) {
            continue;
        }

        event_type = sensor_event_state_convert_assertion(p_cfg, val);
        /* if not resume and event_type mismatched, it will return fail here. */
        if ((assertion != GENERAL_EVENT_TYPE_RESUME) && (event_type != assertion)) {
            ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
            soft_drv_err(
                "The assertion type is mismatched. "
                "(devid=%u; handle=%llu; node_type=0x%x; sensor_type=0x%x; sensor_idx=%u; type0=0x%x; type1=0x%x)\n",
                dev_id, handle, node_type, sensor_type, sensor_idx, event_type, assertion);
            return -EINVAL;
        }

        event.sensor_type = p_cfg->sensor_type;
        event.dev_id = dev_id;
        event.user_id = client->user_id;
        event.node_type = node_type;
        event.node_id = user_node->node_id;
        event.sub_id = i;
        event.err_type = val;
        event.assertion = (unsigned int)assertion;
        ret = soft_fault_event_handler(&event);
        if (ret == 0) {
            soft_drv_event("soft node update state success. (dev_id=%u; pid=%d; sensor_name=%s; user_id=%u;"
                           " node_id=%u; node_type=0x%x; sensor_type=0x%x; val=%d; assertion=%d; handle=0x%llx)\n",
                           dev_id, client->pid, p_cfg->sensor_name, client->user_id, user_node->node_id, node_type,
                           sensor_type, val, assertion, handle);
        }
        break;
    }

    ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
    return 0;
}

int soft_node_register(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    size_t name_len = 0;
    uint64_t handle = 0;
    u32 dev_id, phy_id, vfid;
    struct dms_sensor_user *arg = NULL;
    struct dms_sensor_node_cfg cfg;

    if ((in == NULL) || (in_len != sizeof(struct dms_sensor_user)) || (out == NULL) || (out_len != sizeof(uint64_t))) {
        soft_drv_err("Invalid para. (in_len=%u; out_len=%u)\n", in_len, out_len);
        return -EINVAL;
    }

    arg = (struct dms_sensor_user *)in;
    dev_id = arg->dev_id;
    cfg = arg->cfg;

    name_len = ka_base_strnlen(cfg.name, CFG_NAME_MAX_LENGTH);
    if (name_len >= CFG_NAME_MAX_LENGTH) {
        soft_drv_err("Cfg name is invalid, length of name should be less than 20. (len=%zu)\n", name_len);
        return -EINVAL;
    }

    ret = soft_sensor_para_check(cfg);
    if (ret != 0) {
        soft_drv_debug("Not support Sensor cfg. (dev_id=%u; node_type=%d; sensor_type=%d; ret=%d;)\n", dev_id,
                       cfg.node_type, cfg.sensor_type, ret);
        return ret;
    }

    ret = soft_trans_and_check_id(dev_id, &phy_id, &vfid);
    if (ret != 0) {
        soft_drv_err("can't transform dev_id. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (!soft_sensor_whitelist_check(phy_id, cfg.node_type, cfg.sensor_type)) {
        return -EINVAL;
    }

    ret = dms_soft_node_register(phy_id, &cfg, &handle);
    if (ret != 0) {
        soft_drv_err("register soft node failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    *(uint64_t *)out = handle;

    return 0;
}

int soft_node_unregister(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    uint64_t handle;
    unsigned int phy_id, vfid;
    struct dms_sensor_user *arg = NULL;
    unsigned int node_type, sensor_idx, sensor_type;

    if ((in == NULL) || (in_len != sizeof(struct dms_sensor_user))) {
        soft_drv_err("Invalid para. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    arg = (struct dms_sensor_user *)in;
    handle = arg->handle;

    ret = soft_trans_and_check_id(arg->dev_id, &phy_id, &vfid);
    if (ret != 0) {
        soft_drv_err("can't transform dev_id. (dev_id=%u; ret=%d)\n", arg->dev_id, ret);
        return ret;
    }

    SOFT_PARSE_HANDLE(node_type, sensor_idx, sensor_type, handle);
    if (!soft_sensor_whitelist_check(phy_id, node_type, sensor_type)) {
        soft_drv_err("whitelist check fail. (dev_id=%u; phy_id=%u; node_type=0x%x; sensor_type=0x%x)\n", arg->dev_id,
                     phy_id, node_type, sensor_type);
        return -EINVAL;
    }

    return dms_soft_node_unregister(phy_id, handle);
}

int soft_node_update_state(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret, val, assertion;
    uint64_t handle;
    unsigned int phy_id, vfid;
    struct dms_sensor_user *arg = NULL;

    if ((in == NULL) || (in_len != sizeof(struct dms_sensor_user))) {
        soft_drv_err("Invalid para. (in_is_null=%d; in_len=%u; valid_in_len=%zd)\n", (in == NULL), in_len,
                     sizeof(struct dms_sensor_user));
        return -EINVAL;
    }

    arg = (struct dms_sensor_user *)in;
    val = arg->value;
    assertion = arg->assertion;
    handle = arg->handle;

    if (assertion >= GENERAL_EVENT_TYPE_MAX) {
        soft_drv_debug("Not support assertion. (dev_id=%u; assertion=%d; max=%d)\n", arg->dev_id, assertion,
                       GENERAL_EVENT_TYPE_MAX - 1);
        return -EINVAL;
    }

    ret = soft_trans_and_check_id(arg->dev_id, &phy_id, &vfid);
    if (ret != 0) {
        soft_drv_err("can't transform dev_id. (dev_id=%u; ret=%d)\n", arg->dev_id, ret);
        return ret;
    }

    if (!soft_fault_whitelist_check(phy_id, handle, val)) {
        soft_drv_err("Fault whitelist check failed. (dev_id=%u; phy_id=%u; handle=0x%llx; val=%d)\n", arg->dev_id,
                     phy_id, handle, val);
        return -EINVAL;
    }

    return dms_update_sensor_state(phy_id, handle, val, assertion);
}

STATIC void soft_dev_free_registered_dev(struct soft_dev_client *client)
{
    int i;
    struct soft_dev *pos = NULL;
    struct soft_dev *n = NULL;
    ka_list_for_each_entry_safe(pos, n, &client->head, list)
    {
        if (pos->registered == 0) {
            continue;
        }
        soft_unregister_one_node(pos);
        for (i = 0; i < SF_SUB_ID_MAX; i++) {
            if (pos->sensor_obj_registered[i] == 1) {
                soft_fault_event_free(&pos->sensor_event_queue[i]);
            }
        }
        soft_one_dev_exit(pos);
        ka_list_del(&pos->list);
        dbl_kfree(pos);
        pos = NULL;
    }
}

void soft_dev_exit(void)
{
    int i, j;
    struct soft_dev_client *client = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        ka_task_mutex_lock(&soft_ctrl->mutex[i]);
        for (j = SF_SENSOR_USER; j < SF_USER_MAX; j++) {
            client = soft_ctrl->s_dev_t[i][j];
            soft_dev_free_registered_dev(client);
            client->registered = 0;
            client->node_num = 0;
            client->pid = -1;
        }

        soft_ctrl->user_num[i] = 0;
        ka_task_mutex_unlock(&soft_ctrl->mutex[i]);
    }

    return;
}

void soft_client_release(int owner_pid)
{
    int i, j;
    unsigned int user_num;
    struct soft_dev_client *client = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        ka_task_mutex_lock(&soft_ctrl->mutex[i]);
        user_num = soft_ctrl->user_num[i];
        if (user_num == 0) {
            ka_task_mutex_unlock(&soft_ctrl->mutex[i]);
            continue;
        }
        for (j = SF_SENSOR_USER; j < SF_USER_MAX; j++) {
            client = soft_ctrl->s_dev_t[i][j];
            if ((client->registered == 0) || (client->pid != owner_pid)) {
                continue;
            }
            soft_dev_free_registered_dev(client);
            soft_drv_event("release one process success. (dev_id=%d; user_id=%d; owner_pid=%d)\n", i, j, owner_pid);
            client->registered = 0;
            client->node_num = 0;
            client->pid = -1;
            soft_ctrl->user_num[i]--;
            break;
        }
        ka_task_mutex_unlock(&soft_ctrl->mutex[i]);
    }

    return;
}

BEGIN_DMS_MODULE_DECLARATION(soft_fault)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_SOFT_FAULT, DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_REGISTER, NULL, NULL,
                    DMS_SUPPORT_ALL, soft_node_register)
ADD_FEATURE_COMMAND(DMS_MODULE_SOFT_FAULT, DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_UNREGISTER, NULL, NULL,
                    DMS_SUPPORT_ALL, soft_node_unregister)
ADD_FEATURE_COMMAND(DMS_MODULE_SOFT_FAULT, DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_UPDATE_VAL, NULL, NULL,
                    DMS_SUPPORT_ALL, soft_node_update_state)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()
