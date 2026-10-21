/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef TRS_UK_MSG_H_
#define TRS_UK_MSG_H_

#include "drv_type.h"

#define DRV_SUBEVENT_TRS_ALLOC_SQCQ_WITH_URMA_MSG 39 /* CCPU */
#define DRV_SUBEVENT_TRS_FREE_SQCQ_WITH_URMA_MSG 40  /* CCPU */
#define DRV_SUBEVENT_TRS_SQCQ_QUERY_MSG 41           /* CCPU */
#define DRV_SUBEVENT_TRS_SQCQ_CONFIG_MSG 42          /* CCPU */
#define DRV_SUBEVENT_TRS_CREATE_ASYNC_JETTY_MSG 43   /* CCPU */
#define DRV_SUBEVENT_TRS_DESTROY_ASYNC_JETTY_MSG 44  /* CCPU */
#define DRV_SUBEVENT_TRS_IMPORT_ASYNC_JETTY_MSG 45   /* CCPU */
#define DRV_SUBEVENT_TRS_INIT_BATCH_2D_JETTY_MSG 46  /* CCPU */
#define DRV_SUBEVENT_TRS_INIT_H2D2H_JETTY_MSG 47     /* CCPU */
#define DRV_SUBEVENT_TRS_ALLOC_JETTY_MSG 48          /* CCPU */
#define DRV_SUBEVENT_TRS_FREE_JETTY_MSG 49           /* CCPU */
#define DRV_SUBEVENT_TRS_FILL_WQE_MSG 50             /* CCPU */

#define TRS_ASYNC_SEND_SIDE 0
#define TRS_ASYNC_RECV_SIDE 1

struct trs_d2d_send_info {
    uint32_t flag;
    uint32_t pos;
    uint32_t sq_id; // send side need
    bool is_batch_2d;
    uint32_t d2d_index;
    uint32_t rsv[2];
};

struct trs_import_jetty_info {
    uint8_t jfr_raw[16];
    uint32_t jfr_uasid;
    uint32_t jfr_id;
    uint32_t token_val;
    uint32_t recv_dev_id; // send side need
    uint32_t d2d_index;
    uint32_t rsv[3];
};

#define TRS_CB_EVENT_GRP_ID 11
#define TRS_CB_GROUP_NUM 1024

#define TRS_CB_HW_SUBEVENTID 0x0             /* stars */
#define TRS_CB_SW_SUBEVENTID 0x1             /* trs drv send */
#define TRS_CB_HW_TIMEOUT_SUBEVENTID 0x0FEEU // 12bit for subtopic_id

struct trs_cb_cqe {
    unsigned short phase : 1;
    unsigned short SOP : 1;    /* start of packet, indicates this is the first 32bit return payload */
    unsigned short MOP : 1;    /* middle of packet, indicates the payload is a continuation of previous
        task return payload */
    unsigned short EOP : 1;    /* end of packet, indicates this is the last 32bit return payload.
                        SOP & EOP can appear in the same packet, MOP & EOP can also appear on the same packet. */
    unsigned short cq_id : 12; /* logic cq id */
    unsigned short stream_id;
    unsigned short task_id;
    unsigned short sq_id;       /* physical sq id */
    unsigned short sq_head;     /* physical sq head */
    unsigned short sequence_id; /* for match */
    unsigned char is_block;
    unsigned char reserved1;
    unsigned short event_id;
    unsigned long long host_func_cb_ptr;
    unsigned long long fn_data_ptr;
};

struct trs_cb_stars_event {
    unsigned int cqid : 16;
    unsigned int cb_groupid : 16;
    unsigned int devid : 16;
    unsigned int stream_id : 16;
    unsigned int event_id : 16;
    unsigned int is_block : 16;
    unsigned int task_id : 16;
    unsigned int res1 : 16;
    unsigned int host_func_low;
    unsigned int host_func_high;
    unsigned int fn_data_low;
    unsigned int fn_data_high;
};

static inline void trs_get_cb_group_num(unsigned int *group_num)
{
    *group_num = TRS_CB_GROUP_NUM;
}
#endif
