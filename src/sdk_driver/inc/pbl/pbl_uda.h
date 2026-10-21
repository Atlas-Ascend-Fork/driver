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

#ifndef PBL_UDA_H
#define PBL_UDA_H

#include "ka_type.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "pbl_dev_identity.h"
#include "ascend_kernel_hal.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"

/*
    UDA: unified device access
    phy_devid: Indicates the device that is added by uda_add_dev, it is a complete device,
               excluding devices split by mdev or sriov.
    udevid: unified device id, include davinci physical device, mia device and kunpeng device,
            phy_devid is equal to udevid
    devid: logical device id from the user view
*/

/* Method of the device prober */

static const char *uda_dev_hw_name[UDA_HW_MAX] = {"Davinci", "Kunpeng"};

static inline u32 uda_get_dev_hw_type_by_name(const char *name)
{
    u32 dev_hw_type;

    for (dev_hw_type = UDA_DAVINCI; dev_hw_type < UDA_HW_MAX; dev_hw_type++) {
        if (ka_base_strcmp(uda_dev_hw_name[dev_hw_type], name) == 0) {
            break;
        }
    }

    return dev_hw_type;
}

#define UDA_INVALID_UDEVID 0xffffffffU

struct uda_dev_para {
    u32 udevid; /* 0xffffffff, uda distribute devid, else use this devid if not conflict;
                  UDA_AGENT set existing devid; */
    u32 remote_udevid;
    u32 chip_type;
    ka_device_t *dev;
    int pf_flag; /* 1: pf; 0: vf */
    u32 master_id;
    u32 add_id;    /* ID added to the input during udevid reordering */
    u32 char_flag; /* 1: use logic id numbering */
    u32 logic_id;
};

struct uda_mia_dev_para {
    u32 phy_devid;
    u32 sub_devid;
};

#define GUID_NUM 4
#define DEV_NUM_IN_GROUP 4
#define UB_PORT_NUM 36
#define UDEVID_REORDER_RESV_NUM 3
struct udevid_reorder_para {
    u32 guid[GUID_NUM];  /* GUID of the current device */
    u32 guid1[GUID_NUM]; /* GUID of other devices1 in the same group */
    u32 guid2[GUID_NUM]; /* GUID of other devices2 in the same group */
    u32 guid3[GUID_NUM]; /* GUID of other devices3 in the same group */
    u32 ub_link_status;
    u8 ub_port_status[UB_PORT_NUM]; /* port num 36 */
    u32 group_dev_num;              /* 1p,2p,4p */
    u32 module_id;
    int slot_id;
    u32 resv[UDEVID_REORDER_RESV_NUM]; /* resv */
};

static inline void uda_dev_type_pack(struct uda_dev_type *type, enum uda_dev_hw hw, enum uda_dev_object object,
                                     enum uda_dev_location location, enum uda_dev_prop prop)
{
    type->hw = hw;
    type->object = object;
    type->location = location;
    type->prop = prop;
}

static inline void uda_davinci_local_real_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_LOCAL, UDA_REAL);
}

static inline void uda_davinci_local_virtual_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_LOCAL, UDA_VIRTUAL);
}

static inline void uda_davinci_near_real_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL);
}

static inline void uda_davinci_near_virtual_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_VIRTUAL);
}

static inline void uda_davinci_near_virtual_agent_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_AGENT, UDA_NEAR, UDA_VIRTUAL);
}

static inline void uda_davinci_local_virtual_agent_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_AGENT, UDA_LOCAL, UDA_VIRTUAL);
}

static inline void uda_davinci_local_real_agent_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_AGENT, UDA_LOCAL, UDA_REAL);
}

static inline void uda_davinci_remote_real_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_REMOTE, UDA_REAL);
}

static inline void uda_dev_para_pack(struct uda_dev_para *para, u32 udevid, u32 remote_udevid, u32 chip_type,
                                     ka_device_t *dev)
{
    para->udevid = udevid;
    para->remote_udevid = remote_udevid;
    para->chip_type = chip_type;
    para->dev = dev;
    para->pf_flag = 1; /* default pf dev */
    para->add_id = udevid;
}

static inline void uda_mia_dev_para_pack(struct uda_mia_dev_para *mia_para, u32 phy_devid, u32 sub_devid)
{
    mia_para->phy_devid = phy_devid;
    mia_para->sub_devid = sub_devid;
}

int uda_add_dev(struct uda_dev_type *type, struct uda_dev_para *para, u32 *udevid); /* uda_dev_prop must be real */
int uda_add_mia_dev(struct uda_dev_type *type, struct uda_dev_para *para, struct uda_mia_dev_para *mia_para,
                    u32 *udevid);
int uda_remove_dev(struct uda_dev_type *type, u32 udevid);

enum uda_dev_ctrl_cmd {
    UDA_CTRL_SUSPEND,
    UDA_CTRL_RESUME,
    UDA_CTRL_TO_MIA,         /* sia dev mode to mia mode */
    UDA_CTRL_TO_SIA,         /* mia dev mode to sia mode */
    UDA_CTRL_MIA_CHANGE = 4, /* The computing power changes. */
    UDA_CTRL_REMOVE,         /* sriov dev */
    UDA_CTRL_HOTRESET = 6,
    UDA_CTRL_HOTRESET_CANCEL,
    UDA_CTRL_SHUTDOWN,
    UDA_CTRL_PRE_HOTRESET,
    UDA_CTRL_PRE_HOTRESET_CANCEL = 10,
    UDA_CTRL_ADD_GROUP,
    UDA_CTRL_DEL_GROUP,
    UDA_CTRL_UPDATE_P2P_ADDR,
    UDA_CTRL_COMMUNICATION_LOST,
    UDA_CTRL_AIC_ATU_CFG,
    UDA_CTRL_MAX
};

int uda_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd);
int uda_dev_ctrl_ex(u32 udevid, enum uda_dev_ctrl_cmd cmd, u32 val);
int uda_agent_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd);

/* Method of the device notifier */

typedef int (*uda_notify)(u32 udevid, enum uda_notified_action action);
int uda_notifier_register(const char *notifier, struct uda_dev_type *type, enum uda_priority pri, uda_notify func);
int uda_notifier_unregister(const char *notifier, struct uda_dev_type *type);
int uda_get_action_para(u32 udevid, enum uda_notified_action action, u32 *val);

ka_device_t *uda_get_device(u32 udevid);
ka_device_t *uda_get_agent_device(u32 udevid);

/* Some modules do not separate the drivers of real devices and virtual devices.
   Therefore, simples methods are provided here. */
static inline int uda_real_virtual_notifier_register(const char *notifier, struct uda_dev_type *type,
                                                     enum uda_priority pri, uda_notify func)
{
    int ret = uda_notifier_register(notifier, type, pri, func);
    if (ret == 0) {
        type->prop = UDA_VIRTUAL;
        ret = uda_notifier_register(notifier, type, pri, func);
        if (ret != 0) {
            type->prop = UDA_REAL;
            (void)uda_notifier_unregister(notifier, type);
        }
    }

    return ret;
}

static inline int uda_real_virtual_notifier_unregister(const char *notifier, struct uda_dev_type *type)
{
    int ret = uda_notifier_unregister(notifier, type);
    if (ret == 0) {
        type->prop = UDA_VIRTUAL;
        ret = uda_notifier_unregister(notifier, type);
    }

    return ret;
}

/* Method of the device probe or notifier */
bool uda_is_support_udev_mng(void); /* obp not support, milan support */
bool uda_is_phy_dev(u32 udevid);    /* Check whether the device is a physical device, not mia device. */
/* Check whether the device is a pf device , not vf device, applicable to physical env, virtual env */
bool uda_is_pf_dev(u32 udevid);
bool uda_is_udevid_exist(u32 udevid);

int uda_dev_set_remote_udevid(u32 udevid, u32 remote_udevid);
int uda_dev_get_remote_udevid(u32 udevid, u32 *remote_udevid);
int uda_remote_udevid_to_udevid(u32 remote_udevid, u32 *udevid);

/* mia dev trans */
int uda_udevid_to_mia_devid(u32 udevid, struct uda_mia_dev_para *mia_para);
int uda_mia_devid_to_udevid(struct uda_mia_dev_para *mia_para, u32 *udevid);
u32 uda_make_udevid(struct uda_mia_dev_para *mia_para);

int uda_devid_to_udevid(u32 devid, u32 *udevid);
int uda_devid_to_phy_devid(u32 devid, u32 *phy_devid, u32 *vfid);
int uda_udevid_to_phy_devid(u32 udevid, u32 *phy_devid, u32 *vfid);
/* Host ID conversion is supported. */
int uda_devid_to_udevid_ex(u32 devid, u32 *udevid);

/* recommend to use uda_devid_to_udevid instead of this,
    when outband hotreset, device offline, can only use this interface */
int uda_ns_node_devid_to_udevid(u32 devid, u32 *udevid);

bool uda_can_access_udevid(u32 udevid);
bool uda_proc_can_access_udevid(int tgid, u32 udevid);
bool uda_task_can_access_udevid_inherit(ka_task_struct_t *task, u32 udevid);

/* detected dev num, it is not related to whether the device is online */
int uda_set_detected_phy_dev_num(u32 dev_num);
u32 uda_get_detected_phy_dev_num(void);

u32 uda_get_udev_max_num(void);        /* System udeivce specifications */
u32 uda_get_remote_udev_max_num(void); /* System udeivce specifications */

u32 uda_get_dev_num(void);     /* online phy dev num */
u32 uda_get_mia_dev_num(void); /* online mia dev num */

u32 uda_get_cur_ns_dev_num(void);
int uda_get_cur_ns_udevids(u32 udevids[], u32 num);

/* set/get udev ns id */
#define UDA_NS_NUM 128U
int uda_set_dev_ns_identify(u32 udevid, u64 identify); /* should call in notifier call ctx */
int uda_get_dev_ns_identify(u32 udevid, u64 *identify);
int uda_get_cur_ns_id(u32 *ns_id);
int uda_get_container_docker_id(u32 *docker_id);

int uda_set_dev_share(u32 udevid, u8 share_flag);
int uda_get_dev_share(u32 udevid, u8 *share_flag);

void uda_release_idle_ns_by_vdev_id(unsigned int phy_id, unsigned int vdev_id);

/* chip_type define in pbl_dev_identity.h */
/* only support phy device which udevid < 100 */
int uda_set_chip_type(u32 udevid, u32 chip_type);
u32 uda_get_chip_type(u32 udevid);
u32 uda_get_master_id(u32 udevid);

u32 uda_get_host_id(void);

void uda_set_udevid_reorder_flag(bool flag);
bool uda_udevid_is_reorder(void);
void uda_set_add_id(unsigned int add_id);
int uda_set_udevid_reorder_para(u32 add_id, struct udevid_reorder_para *para);
int uda_get_udevid_reorder_para(u32 udevid, struct udevid_reorder_para *para);
int uda_udevid_to_add_id(u32 udevid, u32 *add_id);
int uda_add_id_to_udevid(u32 add_id, u32 *udevid);
int uda_udevid_to_logic_id(u32 udevid, u32 *logic_id);
#endif
