/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DEV_NUM_CHIP_H
#define DEV_NUM_CHIP_H

#if defined(CFG_HOST_ENV) || defined(DRV_HOST)
/* Host side */

/* Maximum number of devices */
#define ASCEND_DEV_MAX_NUM 1124
/* Maximum number of physical devices */
#define ASCEND_PDEV_MAX_NUM 64
/* maximum number of host physical devices */
#define ASCEND_HOST_PDEV_MAX_NUM ASCEND_PDEV_MAX_NUM
/* Maximum number of virtual devices */
#define ASCEND_VDEV_MAX_NUM 1024
#define ASCEND_VDEV_ID_START 100

#else
/* Device side */

/* Maximum number of devices */
#define ASCEND_DEV_MAX_NUM 64
/* Maximum number of physical devices */
#define ASCEND_PDEV_MAX_NUM 2
/* Maximum number of virtual devices */
#define ASCEND_VDEV_MAX_NUM 32
#define ASCEND_VDEV_ID_START 32

/* maximum number of host physical devices */
#define ASCEND_HOST_PDEV_MAX_NUM 64

#endif

#endif /* DEV_NUM_CHIP_H */
