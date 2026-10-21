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
#ifndef PBL_SPOD_INFO_H
#define PBL_SPOD_INFO_H

#define DBL_SPOD_MAX_SERVER_NUM (48)
#define DBL_SPOD_MAX_CHIP_NUM (8)
#define DBL_SPOD_MAX_DIE_NUM (2)
#define DBL_SPOD_MAX_UDEVID_NUM (DBL_SPOD_MAX_CHIP_NUM * DBL_SPOD_MAX_DIE_NUM)
#define DBL_SPOD_MAX_NUM (DBL_SPOD_MAX_SERVER_NUM * DBL_SPOD_MAX_UDEVID_NUM)

#define DBL_SPOD_CHECK_SDID(sdid_parse_info, ret)                                           \
    do {                                                                                    \
        if (((sdid_parse_info).server_id >= DBL_SPOD_MAX_SERVER_NUM) ||                     \
            ((sdid_parse_info).chip_id >= DBL_SPOD_MAX_CHIP_NUM) ||                         \
            ((sdid_parse_info).die_id >= DBL_SPOD_MAX_DIE_NUM) ||                           \
            ((sdid_parse_info).udevid >= DBL_SPOD_MAX_UDEVID_NUM) ||                        \
            ((sdid_parse_info).chip_id * DBL_SPOD_MAX_DIE_NUM + (sdid_parse_info).die_id != \
             (sdid_parse_info).udevid)) {                                                   \
            (ret) = -EINVAL;                                                                \
        }                                                                                   \
    } while (0)

struct spod_info {
    unsigned int sdid;
    unsigned int scale_type;
    unsigned int super_pod_id;
    unsigned int server_id;
    unsigned int chassis_id;
    unsigned int super_pod_type;
    unsigned int super_pod_intercon_type;
    unsigned short scale_up_group_id;
    unsigned short scale_out_group_id;
    unsigned int reserve[4];
};

struct sdid_parse_info {
    unsigned int server_id; /* obp: server id, david pod: chass id, david other: server_id */
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int udevid;
    unsigned int reserve[8];
};

int dbl_get_spod_info(unsigned int udevid, struct spod_info *s);
int dbl_set_spod_info(unsigned int udevid, struct spod_info *s);
int dbl_parse_sdid(unsigned int sdid, struct sdid_parse_info *s);
int dbl_make_sdid(struct sdid_parse_info *s, unsigned int *sdid);
int dbl_set_spod_node_status(unsigned int local_udevid, unsigned int index, unsigned int status);
int dbl_spod_node_status_init(unsigned int udevid);
void dbl_spod_node_status_uninit(unsigned int udevid);

#endif /* PBL_SPOD_INFO_H */
