/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hdc_cmn.h"
#include "hdc_pcie_drv.h"
#include "hdc_core.h"
#include "hdc_adapt.h"
#include "hdc_file_common.h"
#include "ascend_inpackage_hal.h"
#include "hdcdrv_cmd_ioctl.h"

#ifdef CFG_SOC_PLATFORM_HELPER_V51
char g_ppc_dirs[PPC_PATH_MAX] = "/home/drv/hdc_ppc/";
#else
#ifdef CFG_SOC_PLATFORM_RC
char g_ppc_dirs[PPC_PATH_MAX] = "/home/user/hdc_ppc/";
#else
char g_ppc_dirs[PPC_PATH_MAX] = "/var/driver/";
#endif /* end of if CFG_SOC_PLATFORM_RC */
#endif /* end of if CFG_SOC_PLATFORM_HELPER_V51 */

void drv_hdc_trans_type_mutex_init(void)
{
    /* not support */
    return;
}

int hdc_get_chip_type(void)
{
    return -1;
}

hdcError_t hdc_set_init_info(void)
{
    /* not support */
    return DRV_ERROR_NONE;
}

void hdc_pcie_init_sleep(void)
{
    (void)mmSleep(HDC_SLEEP_TIME);
    return;
}

void hdc_phandle_get_sleep(void)
{
    (void)mmSleep(100);
    return;
}

hdcError_t drv_hdc_recv_msg_body_ret_check(signed int ret)
{
    (void)ret;
    return DRV_ERROR_RECV_MESG;
}

signed int drv_hdc_get_max_session_num_by_type(signed int serviceType)
{
    (void)serviceType;
    return HDCDRV_SUPPORT_MAX_SESSION;
}

#if !defined(HDC_UT) && !defined(DRV_UT)
hdcError_t halHdcRegisterMem(signed int devid, enum drvHdcMemType mem_type, void *va, unsigned int len,
                             unsigned int flag)
{
    (void)devid;
    (void)mem_type;
    (void)va;
    (void)len;
    (void)flag;
    return DRV_ERROR_NOT_SUPPORT;
}

hdcError_t halHdcUnregisterMem(enum drvHdcMemType mem_type, void *va)
{
    (void)mem_type;
    (void)va;
    return DRV_ERROR_NOT_SUPPORT;
}

hdcError_t halHdcGetTransType(enum halHdcTransType *transType)
{
    (void)transType;
    return DRV_ERROR_NOT_SUPPORT;
}

hdcError_t halHdcSetTransType(enum halHdcTransType transType)
{
    (void)transType;
    return DRV_ERROR_NOT_SUPPORT;
}

hdcError_t halHdcWaitMemRelease(HDC_SESSION session, int time_out, struct drvHdcFastRecvMsg *msg)
{
    (void)session;
    (void)time_out;
    (void)msg;
    return DRV_ERROR_NOT_SUPPORT;
}

hdcError_t halHdcWaitMemReleaseEx(HDC_SESSION session, struct drvHdcWaitMsgInput *input,
                                  struct drvHdcFastSendFinishMsg *msg)
{
    (void)session;
    (void)input;
    (void)msg;
    return DRV_ERROR_NOT_SUPPORT;
}
#endif
signed int hdc_pcie_client_destroy(mmProcess handle, signed int devId, signed int serviceType)
{
    (void)handle;
    (void)devId;
    (void)serviceType;
    /* not support */
    return DRV_ERROR_NONE;
}

#ifndef HDC_UT
hdcError_t halHdcClientWakeUp(HDC_CLIENT client)
{
    (void)client;
    /* not support */
    return DRV_ERROR_NOT_SUPPORT;
}
hdcError_t halHdcServerWakeUp(HDC_SERVER server)
{
    (void)server;
    /* not support */
    return DRV_ERROR_NOT_SUPPORT;
}
#endif

#ifndef DRV_UT
hdcError_t drvHdcGetTrustedBasePath(signed int peer_node, signed int peer_devid, char *base_path, unsigned int path_len)
{
    return drvHdcGetTrustedBasePathEx(0, peer_node, peer_devid, base_path, path_len);
}

hdcError_t drvHdcSendFile(signed int peer_node, signed int peer_devid, const char *file, const char *dst_path,
                          void (*progress_notifier)(struct drvHdcProgInfo *))
{
    return drvHdcSendFileEx(0, peer_node, peer_devid, file, dst_path, progress_notifier);
}

hdcError_t drvHdcGetTrustedBasePathV2(signed int peer_node, signed int peer_devid, char *base_path,
                                      unsigned int path_len)
{
    return drvHdcGetTrustedBasePathEx(HDC_FILE_TRANS_MODE_CANN, peer_node, peer_devid, base_path, path_len);
}

hdcError_t drvHdcSendFileV2(signed int peer_node, signed int peer_devid, const char *file, const char *dst_path,
                            void (*progress_notifier)(struct drvHdcProgInfo *))
{
    return drvHdcSendFileEx(HDC_FILE_TRANS_MODE_CANN, peer_node, peer_devid, file, dst_path, progress_notifier);
}
#endif
