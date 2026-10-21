/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdc_ub_drv.h"

static pthread_rwlock_t g_hdc_notify_rwlock[HDC_SERVICE_TYPE_MAX];

void hdc_ub_notiy_init(struct hdcConfig *hdc_config)
{
    int i;

    for (i = 0; i < HDC_SERVICE_TYPE_MAX; i++) {
        hdc_config->notify_list[i].valid = HDC_UB_INVALID;
        hdc_config->notify_list[i].notify.connect_notify = NULL;
        hdc_config->notify_list[i].notify.close_notify = NULL;
        hdc_config->notify_list[i].notify.data_in_notify = NULL;
        (void)pthread_rwlock_init(&g_hdc_notify_rwlock[i], NULL);
    }

    return;
}

hdcError_t hdc_ub_notify_register(int service_type, struct HdcSessionNotify *notify)
{
    if (!hdc_service_type_vaild(service_type)) {
        HDC_LOG_ERR("Invalid service_type, register failed.(type=%d)\n", service_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (notify == NULL) {
        HDC_LOG_ERR("notify is null, register failed.(type=%d)\n", service_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_wrlock(&g_hdc_notify_rwlock[service_type]);
    if (g_hdcConfig.notify_list[service_type].valid == HDC_UB_VALID) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        HDC_LOG_ERR("Service notify has been registered.(type=%d)\n", service_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    g_hdcConfig.notify_list[service_type].notify.connect_notify = notify->connect_notify;
    g_hdcConfig.notify_list[service_type].notify.close_notify = notify->close_notify;
    g_hdcConfig.notify_list[service_type].notify.data_in_notify = notify->data_in_notify;
    wmb();
    g_hdcConfig.notify_list[service_type].valid = HDC_UB_VALID;
    (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);

    return 0;
}

void hdc_ub_notify_unregister(int service_type)
{
    if (!hdc_service_type_vaild(service_type)) {
        HDC_LOG_ERR("Invalid service_type, unregister failed.(type=%d)\n", service_type);
        return;
    }

    (void)pthread_rwlock_wrlock(&g_hdc_notify_rwlock[service_type]);
    g_hdcConfig.notify_list[service_type].valid = HDC_UB_INVALID;
    wmb();
    g_hdcConfig.notify_list[service_type].notify.connect_notify = NULL;
    g_hdcConfig.notify_list[service_type].notify.close_notify = NULL;
    g_hdcConfig.notify_list[service_type].notify.data_in_notify = NULL;
    (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);

    return;
}

void hdc_touch_connect_notify(int dev_id, unsigned long long peer_pid, int service_type)
{
    int ret;

    if (!hdc_service_type_vaild(service_type)) {
        HDC_LOG_ERR("Invalid service_type, ConnectNotify failed.(type=%d; dev_id=%d)\n", service_type, dev_id);
        return;
    }

    (void)pthread_rwlock_rdlock(&g_hdc_notify_rwlock[service_type]);
    // notify not register, need to notify
    if (g_hdcConfig.notify_list[service_type].valid == HDC_UB_INVALID) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        return;
    }

    // notify register, but no need to notify connect
    if (g_hdcConfig.notify_list[service_type].notify.connect_notify == NULL) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        return;
    }

    ret = g_hdcConfig.notify_list[service_type].notify.connect_notify(dev_id, 0, (int)peer_pid, getpid());
    if (ret != 0) {
        HDC_LOG_ERR("Notify call failed.(dev_id=%d; service_type=%d; ret=%d)\n", dev_id, service_type, ret);
    }

    (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
    return;
}

void hdc_touch_close_notify(int dev_id, unsigned long long peer_pid, int service_type)
{
    int ret;

    if (!hdc_service_type_vaild(service_type)) {
        HDC_LOG_ERR("Invalid service_type, CloseNotify failed.(type=%d; dev_id=%d)\n", service_type, dev_id);
        return;
    }

    (void)pthread_rwlock_rdlock(&g_hdc_notify_rwlock[service_type]);
    // notify not register, need to notify
    if (g_hdcConfig.notify_list[service_type].valid == HDC_UB_INVALID) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        return;
    }

    // notify register, but no need to notify connect
    if (g_hdcConfig.notify_list[service_type].notify.close_notify == NULL) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        return;
    }

    ret = g_hdcConfig.notify_list[service_type].notify.close_notify(dev_id, 0, (int)peer_pid, getpid());
    if (ret != 0) {
        HDC_LOG_ERR("Notify call failed.(dev_id=%d; service_type=%d; ret=%d)\n", dev_id, service_type, ret);
    }

    (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);

    return;
}

void hdc_touch_data_in_notify(int dev_id, int service_type)
{
    int ret;

    if (!hdc_service_type_vaild(service_type)) {
        HDC_LOG_ERR("Invalid service_type, CloseNotify failed.(type=%d; dev_id=%d)\n", service_type, dev_id);
        return;
    }

    (void)pthread_rwlock_rdlock(&g_hdc_notify_rwlock[service_type]);
    // notify not register, need to notify
    if (g_hdcConfig.notify_list[service_type].valid == HDC_UB_INVALID) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        return;
    }

    // notify register, but no need to notify connect
    if (g_hdcConfig.notify_list[service_type].notify.data_in_notify == NULL) {
        (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);
        return;
    }

    ret = g_hdcConfig.notify_list[service_type].notify.data_in_notify(dev_id, 0, getpid());
    if (ret != 0) {
        HDC_LOG_ERR("Notify call failed.(dev_id=%d; service_type=%d; ret=%d)\n", dev_id, service_type, ret);
    }

    (void)pthread_rwlock_unlock(&g_hdc_notify_rwlock[service_type]);

    return;
}