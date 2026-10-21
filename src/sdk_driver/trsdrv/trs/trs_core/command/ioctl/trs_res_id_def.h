/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef TRS_RES_ID_DEF_H
#define TRS_RES_ID_DEF_H

#define TRS_HW_TYPE_TSCPU 0
#define TRS_HW_TYPE_STARS 1

enum trs_id_type {
    TRS_STREAM = 0,
    TRS_EVENT,
    TRS_NOTIFY,
    TRS_MODEL,
    TRS_CMO,
    TRS_CNT_NOTIFY,
    TRS_HW_SQ = 6,
    TRS_HW_CQ,
    TRS_SW_SQ = 8,
    TRS_SW_CQ,
    TRS_CB_SQ,
    TRS_CB_CQ,
    TRS_LOGIC_CQ = 12,
    TRS_MAINT_SQ,
    TRS_MAINT_CQ = 14,
    TRS_CDQ,
    TRS_STARS_H2D_JETTY,
    TRS_STARS_D2D_IN_BOARD_JETTY,
    TRS_STARS_D2D_CROSS_BOARD_JETTY,
    TRS_MAX_ID_TYPE,
};

#define TRS_CORE_MAX_ID_TYPE TRS_MAX_ID_TYPE
#endif
