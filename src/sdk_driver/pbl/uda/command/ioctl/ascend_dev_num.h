/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __ASCEND_DEV_NUM_H__
#define __ASCEND_DEV_NUM_H__

#ifdef CONFIG_PLATFORM_310P
#include "ascend_platform/ascend_dev_num_310P.h"
#endif
#ifdef CONFIG_PLATFORM_910
#include "ascend_platform/ascend_dev_num_910.h"
#endif
#ifdef CONFIG_PLATFORM_310B
#include "ascend_platform/ascend_dev_num_310B.h"
#endif
#ifdef CONFIG_PLATFORM_310M
#include "ascend_platform/ascend_dev_num_310B.h"
#endif
#ifdef CONFIG_PLATFORM_910B
#include "ascend_platform/ascend_dev_num_910B.h"
#endif
#ifdef CONFIG_PLATFORM_950
#include "ascend_platform/ascend_dev_num_950.h"
#endif
#ifdef CONFIG_PLATFORM_960
#include "ascend_platform/ascend_dev_num_950.h"
#endif
#ifdef CONFIG_PLATFORM_350
#include "ascend_platform/ascend_dev_num_350.h"
#endif
#ifdef CONFIG_PLATFORM_310N
#include "ascend_platform/ascend_dev_num_310N.h"
#endif
#ifdef CONFIG_PLATFORM_970
#include "ascend_platform/ascend_dev_num_950.h"
#endif
#endif /* __ASCEND_DEV_NUM_H__ */