/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef PBL_MODULE_FEATURE_MSG_H
#define PBL_MODULE_FEATURE_MSG_H

/* Maximum number of supported features: 64 (bits) * 64 = 4096 */
#define PBL_QUERY_FEATURE_BITMAP_MAX_WORDS 64
#define PBL_QUERY_FEATURE_ALL_DEV 0xFFFFFFFFu

/*
 * ioctl message for module feature query.
 *
 * Passed as cmd_para.input to the URD handler. The handler copies this
 * struct from user space, computes the bitmap in a kernel temporary
 * buffer, then writes the result back to user space via copy_to_user.
 *
 * Two query modes selected by dev_id:
 *   - dev_id < ASCEND_DEV_MAX_NUM  : single-device query, bitmap is 1-D
 *   - dev_id == PBL_QUERY_FEATURE_ALL_DEV  : batch query all devices, bitmap is 2-D
 */
struct pbl_module_feature_arg {
    unsigned int dev_id;              /* in: device id, or PBL_QUERY_FEATURE_ALL_DEV for batch query */
    unsigned int max_dev_num;         /* in: actual device count (1..ASCEND_DEV_MAX_NUM) */
    unsigned long long *bitmap_buff;  /* in/out: caller-allocated bitmap buffer */
    unsigned int bitmap_buff_len;     /* in: total size of bitmap_buff in u64 words */
    unsigned int *per_dev_bitmap_len; /* out: actual u64 words filled per device */
};

#endif /* PBL_MODULE_FEATURE_MSG_H */
