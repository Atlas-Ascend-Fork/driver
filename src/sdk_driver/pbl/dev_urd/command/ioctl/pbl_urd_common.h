/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef PBL_URD_COMMON_H
#define PBL_URD_COMMON_H

struct urd_cmd {
    unsigned int main_cmd;
    unsigned int sub_cmd;
    const char *filter;
    unsigned int filter_len;
};

struct urd_cmd_para {
    void *input;
    unsigned int input_len;
    void *output;
    unsigned int output_len;
};

/* acc */
#define DMS_ACC_MASK 0X100FU
/* allow running as root */
#define DMS_ACC_ROOT 0X0001U
/* allow running as HwHiAiUser's or HwBaseUser's group */
#define DMS_ACC_OPERATE 0X0002U
/* allow running as guest */
#define DMS_ACC_USER 0X0004U
/* allow running as HwDmUser */
#define DMS_ACC_DM_USER 0X0008U
/* allow running as restrict access guset, such as CustAiCpuUser */
#define DMS_ACC_LIMIT_USER 0X1000U

#define DMS_ACC_ALL (DMS_ACC_ROOT | DMS_ACC_DM_USER | DMS_ACC_OPERATE | DMS_ACC_USER | DMS_ACC_LIMIT_USER)
#define DMS_ACC_NOT_LIMIT_USER (DMS_ACC_ROOT | DMS_ACC_DM_USER | DMS_ACC_OPERATE | DMS_ACC_USER)
#define DMS_ACC_NOT_USER (DMS_ACC_ROOT | DMS_ACC_DM_USER | DMS_ACC_OPERATE)
#define DMS_ACC_MANAGE_USER (DMS_ACC_ROOT | DMS_ACC_DM_USER)
#define DMS_ACC_ROOT_ONLY (DMS_ACC_ROOT)

#define DMS_RUN_ENV_MASK 0X00F0U
/* Runtime environment */
/* Support physical */
#define DMS_ENV_PHYSICAL 0X0010U
/* Support virtual */
#define DMS_ENV_VIRTUAL 0X0020U
/* Support docker */
#define DMS_ENV_DOCKER 0X0040U
/* Support admin docker */
#define DMS_ENV_ADMIN_DOCKER 0X0080U

#define DMS_ENV_ALL (DMS_ENV_PHYSICAL | DMS_ENV_VIRTUAL | DMS_ENV_DOCKER | DMS_ENV_ADMIN_DOCKER)
#define DMS_ENV_NOT_VIRTUAL (DMS_ENV_PHYSICAL | DMS_ENV_DOCKER | DMS_ENV_ADMIN_DOCKER)
#define DMS_ENV_NOT_DOCKER (DMS_ENV_PHYSICAL | DMS_ENV_VIRTUAL)
#define DMS_ENV_NOT_PHYSICAL (DMS_ENV_VIRTUAL | DMS_ENV_DOCKER | DMS_ENV_ADMIN_DOCKER)
#define DMS_ENV_NOT_NORMAL_DOCKER (DMS_ENV_PHYSICAL | DMS_ENV_VIRTUAL | DMS_ENV_ADMIN_DOCKER)
#define DMS_PHYSICAL_ONLY (DMS_ENV_PHYSICAL)
#define DMS_VIRTUAL_ONLY (DMS_ENV_VIRTUAL)
#define DMS_DOCKER_ONLY (DMS_ENV_DOCKER)
#define DMS_ADMIN_DOCKER_ONLY (DMS_ENV_ADMIN_DOCKER)

#define DMS_VDEV_MASK 0X0700U

/* Runtime environment in vdevice */
/* Can also be used on physical machines when vdevice have been Used. */
#define DMS_VDEV_PHYSICAL 0X0100U
/* Support virtual */
#define DMS_VDEV_VIRTUAL 0X0200U
/* Support docker */
#define DMS_VDEV_DOCKER 0X0400U
/* Support admin docker */
#define DMS_VDEV_ADMIN_DOCKER 0X0800U

#define DMS_VDEV_ALL (DMS_VDEV_PHYSICAL | DMS_VDEV_VIRTUAL | DMS_VDEV_DOCKER)
#define DMS_VDEV_NOT_DOCKER (DMS_VDEV_PHYSICAL | DMS_VDEV_VIRTUAL)
#define DMS_VDEV_NOT_VIRTUAL (DMS_VDEV_PHYSICAL | DMS_VDEV_DOCKER)
#define DMS_VDEV_NOT_PHYSICAL (DMS_VDEV_VIRTUAL | DMS_VDEV_DOCKER)
#define DMS_VDEV_PHYSICAL_ONLY (DMS_VDEV_VIRTUAL | DMS_VDEV_DOCKER)
#define DMS_VDEV_VIRTUAL_ONLY (DMS_VDEV_VIRTUAL | DMS_VDEV_DOCKER)
#define DMS_VDEV_DOCKER_ONLY (DMS_VDEV_VIRTUAL | DMS_VDEV_DOCKER)
#define DMS_VDEV_NOTSUPPORT (0)

#define DMS_SUPPORT_ALL_USER (DMS_ACC_ALL | DMS_ENV_ALL | DMS_VDEV_ALL)
/* root, admin, normal user, NOT limit user */
#define DMS_SUPPORT_ALL (DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL | DMS_VDEV_ALL)

#define DMS_SUPPORT_ROOT_ONLY (DMS_ACC_ROOT_ONLY | DMS_ENV_ALL | DMS_VDEV_ALL)

#define DMS_SUPPORT_ROOT_PHY (DMS_ACC_ROOT_ONLY | DMS_PHYSICAL_ONLY)

#ifdef CFG_FEATURE_DMS_SUPPORT_ALL
/* CFG_FEATURE_DMS_SUPPORT_ALL will be used by lp */
#define DMS_SUPPORT_MANAGE_PHY DMS_SUPPORT_ALL
#else
#define DMS_SUPPORT_MANAGE_PHY (DMS_ACC_MANAGE_USER | DMS_PHYSICAL_ONLY)
#endif

#endif
