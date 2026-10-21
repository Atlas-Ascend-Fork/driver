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

#ifndef ESCHED_DRV_H
#define ESCHED_DRV_H

#include "ascend_hal_define.h"
#include "topic_sched_drv.h"
#include "comm_kernel_interface.h"
#include "trs_chan.h"
#include "esched_kernel_interface.h"
#include "esched_drv_interface.h"
#include "esched_drv_dc_adapt.h"

#define SCHED_DRV_REPORT_NONE 0
#define SCHED_DRV_REPORT_ACK 1
#define SCHED_DRV_REPORT_GET_EVENT 2

enum esched_topic_sched_version {
    TOPIC_SCHED_VERSION_UNKNOWN = 0,
    TOPIC_SCHED_VERSION_V1 = 1,
    TOPIC_SCHED_VERSION_V2 = 2,
    TOPIC_SCHED_VERSION_MAX
};

#define TOPIC_FINISH_REPORT_ABNORMAL 1

#define HOST_SIDE_SET_PID 0
#define DEVICE_SIDE_SET_PID 1

#define HOST_STD_PROC 0
#define HOST_USER_PROC 1
#define DEVICE_STD_PROC 2
#define DEVICE_USER_PROC 3
#define PID_TYPE_MAX 4

#define TOPIC_FINISH_STATUS_NORMAL 0x1
#define TOPIC_FINISH_STATUS_DEBUG 0x2
#define TOPIC_FINISH_STATUS_EXCEPTION 0x4
#define TOPIC_FINISH_STATUS_TRAP 0x8
#define TOPIC_FINISH_STATUS_WARNING 0x10

#define STATUS_REPORT_VAL(status, error_code) (((error_code) << 4) | (status))

#define TOPIC_SCHED_ESCHED_PID_HIGH_OFFSET 16
#define TOPIC_SCHED_USER_DATA_PAYLOAD_LEN 40

#define TOPIC_SCHED_MAX_PRIORITY_NUM 8U
#define TOPIC_SCHED_MAX_PRIORITY (TOPIC_SCHED_MAX_PRIORITY_NUM / TOPIC_SCHED_QOS_COMPRESS_RATE)

#define TOPIC_SCHED_SLIENT_FAULT 0x7U /* error code defined by aicpu */

#define TOPIC_SCHED_MAX_RTSQ_NUM_PER_CLASS 8U

#define TOPIC_SCHED_MB_STATUS_IDLE 0
#define TOPIC_SCHED_MB_STATUS_BUSY 1

#define TOPIC_SCHED_MAX_SUBEVENT_ID (1UL << 12) /* same as struct topic_sched_sqe */

#define TOPIC_SCHED_HOST_POOL_ID 0

#define TOPIC_SCHED_CHAN_AICPU_TYPE 0
#define TOPIC_SCHED_CHAN_COMCPU_TYPE 1

#define STARS_TOPIC_FIRST_COM_CPU_INDEX 2
#define STARS_TOPIC_SECOND_COM_CPU_INDEX 3
#define STARS_TOPIC_MAX_COM_CPU_NUM 2

#define STARS_WORK_MODE_MAILBOX 0
#define STARS_WORK_MODE_MSGQ 1

#define TOPIC_SCHED_STATUS_CHECK_TIME 2U
#define TOPIC_SCHED_WAIT_CPU_IDLE_CNT_MAX 5000U
#define TOPIC_SCHED_WAIT_CPU_IDLE_MIN_INTERVAL 20U
#define TOPIC_SCHED_WAIT_CPU_IDLE_MAX_INTERVAL 40U

/* Used by a single thread of the split operator. The number of CPUs must be greater
   than or equal to twice the number of CPUs. */
#define TOPIC_SCHED_CPU_PORT_DEPTH 32
struct topic_sched_cpu_port {
    ka_task_spinlock_t lock; /* Callers may submit tasks in software interrupts.  */
    u32 port_id;
    u32 status;
    void *sq_base;
    u16 tail;
    u16 depth;
};

#define SCHED_CPU_PORT_CLEAR_ERR_SET_PAUSE 1
#define SCHED_CPU_PORT_CLEAR_ERR_CLEAR_TAIL 2
#define SCHED_CPU_PORT_CLEAR_ERR_CLEAR_MB 3
#define SCHED_CPU_PORT_CLEAR_ERR_CLEAR_TAIL_AND_MB 4
struct sched_cpu_port_clear_info {
    u32 position;
    u32 port_id;
    u32 status;
    u32 head;
    u32 tail;
};

struct sched_cpu_port_task_info {
    u32 port_id;
    u32 mb_id;
    u32 tail;
};

struct sqe_submit_chan_res {
    bool need_destroy;
    int chan_id;
};

struct rtsq_sched_res {
    struct sqe_submit_chan_res sqe_submit[TOPIC_SCHED_MAX_RTSQ_NUM_PER_CLASS];
    u32 rtsq_num;
    u32 init_rtsq_index;
    ka_atomic_t cur_rtsq_index;
};

struct rtsq_non_sched_res {
    int chan_id;
};

struct sched_rtsq_res {
    struct rtsq_sched_res sched_rtsq[TOPIC_SCHED_RTSQ_CLASS_NUM];
    struct rtsq_non_sched_res non_sched_rtsq;
};

struct sched_trs_chan_param {
    struct trs_id_inst id_inst;
    struct trs_chan_para chan_param;
    struct sched_trs_chan_ext_msg ext_msg;
};

struct topic_data_chan_sched_record {
    u64 schedule_sn;
    u64 schedule_sn_last;
    u32 no_schedule_cnt;
};

struct topic_data_chan {
    struct sched_hard_res *hard_res;
    struct topic_sched_mailbox *wait_mb;
    struct topic_sched_mailbox *get_mb;
    struct topic_sched_cpu_port *cpu_port;
    ka_tasklet_struct_t sched_task;
    struct topic_data_chan_sched_record sched_record;
    int valid;
    int sched_mode; /* 0: hw sched, 1: hw+soft sched */
    u32 chan_id;
    u32 mb_id;
    u32 mb_type;
    int wait_mb_status;
    int irq;
    u32 report_flag;
    u64 serial_no;
    int topic_chan_type; /* 0: aicpu topic chan, 1: com cpu topic chan */
    struct sched_cpu_ctx *cpu_ctx;
    struct sched_event *event;
};

struct sched_thread_spec {
    int chan_id[TOPIC_SCHED_MAX_CHAN_NUM];
    u32 pool_id;
    int (*get_chan_func)(u32 devid, struct sched_published_event_info *event_info, int *chan_id);
};

struct sched_hard_res {
    ka_mutex_t mutex;
    void __ka_mm_iomem *io_base;
    void __ka_mm_iomem *com_io_base;
    void __ka_mm_iomem *int_io_base;
    void __ka_mm_iomem *report_addr;
    struct sched_rtsq_res rtsq;
    u32 dev_id;
    u32 topic_sched_version;
    int sched_mode; /* 0: hw sched, 1: hw+soft sched */
    u32 irq_num;
    int *irq;
    int irq_reg_flag;
    u64 rsv_mem_pa;
    void *rsv_mem_va;
    ka_delayed_work_t init;
    u32 init_flag;
    u32 intr_config_flag;
    u32 report_fault_flag;
    u32 retry_times;
    u32 sub_dev_num; /* Number of subdevices created based on the device, like vf dev */
    struct topic_data_chan *topic_chan[TOPIC_SCHED_MAX_CHAN_NUM];
    void *priv;                    /* for host */
    u32 topic_sched_chan_num;      /* The number of topic channels' resource be created for SCHED. */
    u32 topic_sched_chan_start_id; /* The start id of topic channels' resource be created for SCHED. */
    u32 comcpu_chan_num;           /* comcpu num. only hccl use these cpu */
    u32 aicpu_chan_num;
    u32 aicpu_chan_start_id;
    u32 aicpu_start_cpuid;
    u32 ccpu_chan_id;
    u32 delay_work_enable;
    u32 die_id;
    u32 cpu_work_mode; /* 0: mailbox, 1: msgq */
    int msgq_res_map_pid;
    ka_mutex_t msgq_map_mutex;
    int sentry_mode;
    u32 topic_chan_to_cpuid[TOPIC_SCHED_MAX_CHAN_NUM];
    struct sched_thread_spec thread_spec;
};

struct topic_sched_rts_task_info {
    u64 task_so_name_ptr; /* kernelSo */
    u64 para_ptr;         /* paramBase */
    u64 task_name_ptr;    /* kernelName */
    u64 l2_struct_ptr;    /* l2Ctrl */
    u64 extra_field_ptr;
};

#define TOPIC_SCHED_MB_SIZE sizeof(struct topic_sched_mailbox)
#define TOPIC_SCHED_SQE_SIZE sizeof(struct topic_sched_sqe)

#define ESCHED_DRV_SCHED_MODE_HW 0
#define ESCHED_DRV_SCHED_MODE_HW_SOFT 1

static inline bool esched_drv_is_sched_mode_change_task(u32 topic_id, u32 subtopic_id)
{
    return ((topic_id == (u32)EVENT_DRV_MSG) && (subtopic_id == (u32)DRV_SUBEVENT_ESCHED_SCHED_MODE_CHANGE_MSG));
}

static inline bool esched_drv_cpu_is_hw_sched_mode(struct topic_data_chan *topic_chan)
{
    return (topic_chan->sched_mode == ESCHED_DRV_SCHED_MODE_HW);
}

static inline bool esched_drv_dev_is_hw_soft_sched_mode(struct sched_hard_res *hard_res)
{
    return (hard_res->sched_mode == ESCHED_DRV_SCHED_MODE_HW_SOFT);
}

static inline void esched_drv_dev_to_hw_soft_sched_mode(struct sched_hard_res *hard_res)
{
    if (hard_res->sched_mode != ESCHED_DRV_SCHED_MODE_HW_SOFT) {
        hard_res->sched_mode = ESCHED_DRV_SCHED_MODE_HW_SOFT;
    }
}

static inline bool esched_drv_must_report_normal(u32 topic_id)
{
    /* For the CDQM/SPLIT event, the topic requires that no exception be replied. */
    if ((topic_id == (u32)EVENT_CDQ_MSG) || (topic_id == (u32)EVENT_SPLIT_KERNEL)) {
        return true;
    }
    return false;
}
int esched_drv_get_dst_cpuid_in_node(u32 devid, struct sched_published_event_info *event_info, u32 *cpuid);
void esched_drv_init_topic_types(u32 chip_id);
u8 esched_drv_get_topic_type(unsigned int policy, unsigned int dst_engine);
struct sched_hard_res *esched_get_hard_res(u32 chip_id);
void esched_drv_init_topic_sched_version(u32 chip_id);
u32 esched_drv_get_topic_sched_version(u32 chip_id);
void esched_drv_set_chan_create_para(u32 chip_id, u32 pool_id, struct sched_trs_chan_param *param);
struct topic_data_chan *esched_drv_get_topic_chan(u32 dev_id, u32 chan_id);
struct topic_data_chan *esched_drv_create_one_topic_chan(u32 devid, u32 chan_id);
void esched_drv_destroy_one_topic_chan(u32 devid, u32 chan_id);
void esched_drv_destroy_topic_chans(u32 devid, u32 start_chan_id, u32 chan_num);
int esched_drv_create_topic_chans(u32 devid, u32 start_chan_id, u32 chan_num, u32 comcpu_chan_num);
int esched_publish_event_to_topic(u32 chip_id, u32 event_src, struct sched_published_event_info *event_info,
                                  struct sched_published_event_func *event_func);

int esched_drv_fill_sqe_qos(u32 chip_id, struct sched_published_event_info *event_info, struct topic_sched_sqe *sqe,
                            bool wait_thread_check);
int esched_drv_fill_task_msg(u32 chip_id, u32 event_src, void *task_msg_data,
                             struct sched_published_event_info *event_info);

void esched_ccpu_sched_task(unsigned long data);
void esched_aicpu_sched_task(unsigned long data);
int esched_drv_init_non_sched_task_submit_chan(u32 chip_id, u32 pool_id);
void esched_drv_uninit_non_sched_task_submit_chan(u32 chip_id);
int esched_drv_init_sched_task_submit_chan(u32 chip_id, u32 pool_id, u32 available_chan_num, u32 aicpu_chan_num);
void esched_drv_uninit_sched_task_submit_chan(u32 chip_id);
int esched_drv_abnormal_task_handle(struct trs_id_inst *inst, u32 sqid, void *sqe, void *info);
int esched_drv_convert_cpuid_to_topic_chan(u32 devid, u32 cpuid, u32 *topic_chan_id);
void sched_record_cpu_port_clear_log(struct sched_cpu_port_clear_info *clr_info);

/* Platform-specific: defined in esched_device_adapt.c (device) or esched_dc_host_adapt.c (host) */
struct sched_thread_ctx *sched_drv_sched_cpu_get_next_thread(struct sched_cpu_ctx *cpu_ctx);
void esched_drv_sched_cpu_to_idle(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx);
struct topic_data_chan *esched_drv_get_thread_topic_chan(u32 dev_id, struct sched_thread_ctx *thread_ctx);

#endif
