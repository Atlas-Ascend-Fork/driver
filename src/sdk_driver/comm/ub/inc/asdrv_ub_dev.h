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
#ifndef ASDRV_UB_DEV_H
#define ASDRV_UB_DEV_H

#include <ub/ubus/ubus.h>

#include "ascend_dev_num.h"
#include "asdrv_ub_load.h"
#include "asdrv_ub_jetty.h"
#include "asdrv_ub_msg.h"

#define ASCEND_UDMA_DEV_MAX_NUM 154U
#define ASCEND_PFE_IDX 0U
#define ASCEND_UDMA_MAX_FE_NUM 9U // 8 virtual fe + 1 physical fe
#define USER_CTRL_DEFAULT_DEV_ID 0U
#define UBDRV_DEVNUM_PER_SLOT 8u

#define ASCEND_UB_DEV_MAX_NUM ASCEND_DEV_MAX_NUM
#define ASCEND_UB_PF_DEV_MAX_NUM ASCEND_PDEV_MAX_NUM
#define ASCEND_UB_VF_DEV_NUM_BASE ASCEND_VDEV_ID_START
#define ASCEND_INVALID 0
#define ASCEND_VALID 1
#define UBDRV_IO_LOAD_SRAM_OFFSET 0
#define UBDRV_CLOUD_V4_SHR_PARA_SRAM_OFFSET 0x400
#define UBDRV_CLOUD_V4_HW_INFO_SRAM_OFFSET 0x5800

#define ASCEND_UB_IO_RESOURCE 2
#define ASCEND_UB_MEM_RESOURCE 1
#define ASCEND_UB_DEV_NUM_PER_FE 8U
/*
 * device start status:
 * UNPROBED:no probe
 * PROBED:probed, alloc devid ok
 * TOP_HALF_OK:bar map and init interrupt ok
 * BOTTOM_HALF_START: host recv device start notice
 * BOTTOM_HALF_OK: msg chan ok
 */
enum ubdrv_dev_startup_flag_type {
    UBDRV_DEV_STARTUP_UNPROBED = 0,
    UBDRV_DEV_STARTUP_PROBED,
    UBDRV_DEV_STARTUP_TOP_HALF_OK,
    UBDRV_DEV_STARTUP_BOTTOM_HALF_START,
    UBDRV_DEV_STARTUP_BOTTOM_HALF_OK
};

enum ubdrv_idev_status {
    UBDRV_UDMA_UNPROBE = 0,
    UBDRV_UDMA_PROBE,
    UBDRV_UDMA_STATUS_MAX
};

struct ub_idev {
    u32 idev_id;
    u32 ue_idx;
    u32 eid_index;
    u32 valid; // 0: invalid, 1: valid for server/client jetty
    ka_atomic_t ref_cnt;
    u32 dev_id[ASCEND_UB_DEV_NUM_PER_FE];
    ka_rw_semaphore_t rw_sem;
    struct ubcore_device *ubc_dev;
    enum ubdrv_idev_status status;
    struct ascend_ub_admin_chan link_chan;
};

struct ubdev_resource_space {
    void __ka_mm_iomem *va;
    size_t size;
};

enum ubdrv_device_chip_type {
    UBDRV_DEV_DEVICE_A5_1DIE = 0,   // A5 1die
    UBDRV_DEV_DEVICE_A5_2DIE,       // A5 2die
    UBDRV_DEV_UNS,                  // A5 un(ion)s
    UBDRV_DEV_DEVICE_CHIP_TYPE_RESV // resv
};

typedef struct ubdrv_shr_para {
    int load_flag;      /* D2H: device bios notice host to load device os via pcie. 0: no, 1 yes */
    int chip_id;        /* D2H: device bios notice host: cloud ai server, index in one node(4P 0-3); others 0 */
    int node_id;        /* D2H: device bios notice host: cloud ai server has total 8P, one node has 4p, which node */
    int slot_id;        /* D2H: device bios notice host: slot_id (0-7) */
    int board_type;     /* D2H: device bios notice host: cloud pcie card, ai server, evb */
    int chip_type;      /* D2H: mini cloud */
    int driver_version; /* H2D: host notice device bios: driver version */
    int dev_num;        /* D2H: device bios notice host: only cloud v2 set, other chip device driver set */
} ubdrv_shr_para_t;

#define UBDRV_HW_INFO_ASCEND_DRV_RESV_BYTES 32
typedef struct ubdrv_hw_info {
    u8 chip_id; // module id
    u8 multi_chip;
    u8 multi_die; // num of Ddie
    u8 mainboard_id;
    u16 addr_mode; // unify addr mode, fixed to 0
    u16 board_id;

    u8 version;           // version of this struct, A3/A4's version is 3, A5's version is 4
    u8 connect_type;      // V4 resv
    u16 hccs_hpcs_bitmap; // resv

    u16 server_id;    // resv
    u16 scale_type;   // resv
    u32 super_pod_id; // resv
    u32 resv;         // resv

    u16 chassis_id; // V4 add
    u16 super_pod_type;
    u8 board_type;
    u8 slot_id;
    u8 boot_type;
    u8 asd_recv[UBDRV_HW_INFO_ASCEND_DRV_RESV_BYTES];
} ubdrv_hw_info_t;

typedef union {
    struct {
        u8 master_slave_type : 1; // bit0   : 0: master-slave, 1: master-master
        u8 sub_machine_form : 4;  // bit1-4 : sub type of machine form
        u8 machine_form : 3;      // bit5-7 : EVB/SERVER/POD/CARD
    } bits;
    u8 id;
} ubdrv_hw_mainboard_id_t;

struct ubdev_resource {
    struct ubdev_resource_space io_bar;
    struct ubdev_resource_space mem_bar;
};

struct ascend_ub_dev {
    int dev_valid;
    u32 dev_id;
    struct ub_entity *ubus_dev;
    struct ascend_dev *asd_dev;
    struct ubdev_resource res;        // bar address
    struct ubdrv_load_work load_work; // per file load work
    struct ubdrv_loader loader;
    struct ubdrv_timer_work timer_work;
};

struct ubdrv_rao {
    struct ubdrv_non_trans_chan rao_msg_chan[DEVDRV_RAO_CLIENT_MAX];
};

struct ascend_ub_msg_dev {
    int dev_valid;
    u32 dev_id;
    u32 remote_id; // vf set 0
    ka_mutex_t mutex_lock;
    struct ubcore_device *ubc_dev;
    struct ub_idev *idev;
    struct ascend_dev *asd_dev;
    struct ascend_ub_admin_chan admin_msg_chan;
    u32 chan_cnt;
    struct ubdrv_non_trans_chan non_trans_chan[UBDRV_NON_TRANS_CHAN_CNT];
    struct ubdrv_urma_chan urma_chan[URMA_CHAN_MAX];
    struct ubdrv_rao rao;
};

enum ubdrv_local_device_status {
    UBDRV_LOCAL_DEVICE_OFFLINE = 0,
    UBDRV_LOCAL_DEVICE_ONLINE,
    UBDRV_LOCAL_DEVICE_MAX_STATUS,
};

#define PAIR_CHAN_MAX_NUM 8
struct dev_eid_info {
    struct ubcore_eid_info remote_eid[PAIR_CHAN_MAX_NUM];
    struct ubcore_eid_info local_eid[PAIR_CHAN_MAX_NUM];
    u32 num; // max is 8
    u32 min_idx;
};

struct ascend_token {
    u32 local_token;
    u32 token_valid;
};

struct ascend_dev {
    u32 dev_id;
    struct ascend_token token;
    struct dev_eid_info eid_info;
    ubdrv_shr_para_t shr_para;
    ubdrv_hw_info_t hw_info;
    ka_rw_semaphore_t rw_sem;
    struct ascend_ub_dev *ub_dev;      // ascend ub device
    struct ascend_ub_msg_dev *msg_dev; // abstract msg device manage struct
    struct ubdrv_loader loader;
    enum ubdrv_dev_startup_flag_type startup_flag;
    u32 device_boot_status;
    bool phy_flag;
    enum ubdrv_local_device_status local_status;
    u32 msg_retry_cnt;
    u32 id_info_valid;
    struct ubdrv_id_info id_info;
};

enum ubdrv_dev_status {
    UBDRV_DEVICE_UNINIT = 0,    //  uninit state, case1:insmod ko; case2:device clear all resource
    UBDRV_DEVICE_BEGIN_INIT,    //  begin to init resource and add device
    UBDRV_DEVICE_BEGIN_ONLINE,  //  set begin online between msg_dev init finish and uda call add davinci
    UBDRV_DEVICE_ONLINE,        //  uda call is finish, online success and davinci is running
    UBDRV_DEVICE_BEGIN_OFFLINE, //  set begin offline before call uda remove davinci
    UBDRV_DEVICE_OFFLINE,       //  set offline before clear all msg dev resource
    UBDRV_DEVICE_DEAD,          //  set dev is dead when heart_beat lost
    UBDRV_DEVICE_FE_RESET,      //  set when fe reset, then pre_reset api will choose process
    UBDRV_DEVICE_STATUS_MAX,
};

struct ascend_ub_dev_status {
    ka_rw_semaphore_t rw_sem;
    ka_atomic_t ref_cnt;
    enum ubdrv_dev_status device_status;
};

struct ascend_ub_link_res {
    struct ubcore_tjetty *link_tjetty; // peer link_chan jfr of send_jetty
    struct ascend_ub_sync_jetty *admin_jetty;
};

struct ascend_ub_ctrl {
    struct ub_idev idev[ASCEND_UDMA_DEV_MAX_NUM];
    struct ascend_dev asd_dev[ASCEND_UB_DEV_MAX_NUM];
    struct ascend_ub_dev_status dev_status[ASCEND_UB_DEV_MAX_NUM];
    ka_mutex_t mutex_lock;
    struct ascend_ub_link_res link_res[ASCEND_UB_DEV_MAX_NUM];
};

struct ub_idev *ubdrv_get_idev(u32 idev_id);
#endif