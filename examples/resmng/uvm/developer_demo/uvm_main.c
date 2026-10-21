/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <ascend_hal.h>
#include <ascend_hal_define.h>

#include "utils.h"

// common method
void data_init_in_host(uint8_t *data, uint64_t start, uint64_t size)
{
    uint64_t i;
    for (i = 0; i < size; ++i) {
        data[i + start] = (uint8_t)((i + start) % 255 + 1);
    }
}

uint64_t check_uint8_data(uint8_t *dst, uint64_t dst_begin, uint8_t *src, uint64_t src_begin, uint64_t len)
{
    uint64_t err_count = 0;
    uint64_t i;

    for (i = 0; i < len; ++i) {
        if (dst[i + dst_begin] != src[i + src_begin]) {
            err_count++;
        }
    }

    return err_count;
}

static inline int check_u8_value_data(uint8_t *result, uint8_t z, uint64_t len)
{
    uint32_t i;
    for (i = 0; i < len; ++i) {
        if (result[i] != z) {
            return -1;
        }
    }

    return 0;
}

typedef uint32_t (*FUNC_TDT_OPEN)(uint32_t, uint32_t);
typedef uint32_t (*FUNC_TDT_CLOSE)(uint32_t);
int hlt_devmm_tsd_pull_cp_process(DVdevice device_id)
{
    void *handle = dlopen("libtsdclient.so", RTLD_LAZY);
    if (!handle) {
        char *dlError = dlerror();
        LOG_ERR("open libtsdclient.so failed, dlerror() = %s\n", dlError);
        return EOF;
    }
    FUNC_TDT_OPEN func = (FUNC_TDT_OPEN)dlsym(handle, "TsdOpen");
    if (!func) {
        LOG_ERR("TsdOpen is null \n");
        dlclose(handle);
        return EOF;
    }
    uint32_t tdtStatus = func(device_id, 0);
    if (tdtStatus != 0) {
        LOG_ERR("TsdOpen is null,tdtStatus:%d, device_id:%d \n", tdtStatus, device_id);
        dlclose(handle);
        return EOF;
    }

    dlclose(handle);

    int ret = drvMemDeviceOpen(device_id, 0);
    if (ret != 0) {
        LOG_ERR("devmm_drvMemDeviceOpen fail, devid = %u, ret = %d\n", device_id, ret);
        return ret;
    }

    return 0;
}

static int st_svm_open_cp(uint32_t cp_mode, uint32_t devid, HDC_SESSION *session)
{
    return hlt_devmm_tsd_pull_cp_process(devid);
}

int hlt_devmm_tsd_close_cp_process(DVdevice device_id)
{
    int ret = drvMemDeviceClose(device_id);
    if (ret != 0) {
        LOG_ERR("drvMemDeviceClose fail, devid = %u, ret = %d\n", device_id, ret);
        return ret;
    }

    void *handle = dlopen("libtsdclient.so", RTLD_LAZY);
    if (!handle) {
        char *dlError = dlerror();
        LOG_ERR("open libtsdclient.so failed, dlerror() = %s\n", dlError);
        return -1;
    }

    FUNC_TDT_CLOSE func = (FUNC_TDT_CLOSE)dlsym(handle, "TsdClose");
    if (!func) {
        LOG_ERR("TsdClose is null \n");
        dlclose(handle);
        return -1;
    }
    uint32_t tdtStatus = func(device_id);
    if (tdtStatus != 0) {
        LOG_ERR("TsdOpen is null,tdtStatus:%d \n", tdtStatus);
        dlclose(handle);
        return -1;
    }
    dlclose(handle);

    return 0;
}

void st_svm_close_cp(uint32_t cp_mode, uint32_t devid)
{
    hlt_devmm_tsd_close_cp_process(devid);
}

int32_t st_uvm_test_001(void *hdc_session, uint32_t devid, uint64_t arg[10])
{
    int ret = 0;
    uint32_t blockDim = 8;
    uint32_t inputByteSize = 2 << 20;  // 2 << 20 = 2M
    uint32_t outputByteSize = 2 << 20; // 2 << 20 = 2M
    void *x = NULL;
    void *y = NULL;
    uint8_t *x_Data = NULL;
    uint8_t *y_Data = NULL;
    uint64_t err_count = 0;
    int32_t num_attributes = 3;
    enum DEVMM_MEM_RANGE_ATTRIBUTE attributes[num_attributes];
    size_t size[num_attributes];
    struct drv_uvm_location location;
    uint8_t value = 0;

    // Allocate 2MB of UVM memory and return a virtual address(x) within the UVM region.
    CHECK_ERROR(halMemAlloc(&x, inputByteSize, MEM_UVM));
    CHECK_ERROR(halMemAlloc(&y, inputByteSize, MEM_UVM));

    x_Data = (uint8_t *)x;
    y_Data = (uint8_t *)y;

    // Host triggers a normal page fault (no mapping)
    data_init_in_host(x_Data, 0, inputByteSize);
    LOG_INFO("page fault success.\n");

    // Setting the read-mostly attribute on x_Data makes the host the primary copy.
    struct drv_uvm_location loc = {DRV_UVM_LOCATION_TYPE_HOST, 0};
    CHECK_ERROR(halMemManagedAdvise((DVdeviceptr)x_Data, inputByteSize, READ_MOSTLY, loc));

    // The host triggers a write fault on a read-mostly va (host mapped)
    data_init_in_host(x_Data, 0, inputByteSize);
    LOG_INFO("page fault readmostly success.\n");

    // Setting the preferredLocation and AccessedBy attribute on x_Data
    CHECK_ERROR(halMemManagedAdvise((DVdeviceptr)x_Data, inputByteSize, PREFER_LOCATION, loc));
    CHECK_ERROR(halMemManagedAdvise((DVdeviceptr)x_Data, inputByteSize, ACCESS_BY_LOCATION, loc));

    // memsets the value into x
    CHECK_ERROR(drvMemsetD8((DVdeviceptr)x, inputByteSize, value, inputByteSize));
    LOG_INFO("Init data success. (inputByteSize=%#xB, x_Data[0]=%u)\n", inputByteSize, x_Data[0]);

    // halMemManagedPrefetch prefetches x_Data (size inputByteSize) to the specified location (device 0)
    LOG_INFO("[S2] Start prefetch data from host to device 0.\n");
    location.type = DRV_UVM_LOCATION_TYPE_DEVICE;
    location.id = 0;
    CHECK_ERROR(halMemManagedPrefetch((DVdeviceptr)x_Data, inputByteSize, location, 0));
    LOG_INFO("Prefetch data to device 0 success.\n");

    // Query the MEM_RANGE_ATTR_LAST_PREFETCH_LOC, MEM_RANGE_ATTR_LAST_PREFETCH_LOC_TYPE, and
    // MEM_RANGE_ATTR_LAST_PREFETCH_LOC_ID attributes of x_Data.
    LOG_INFO("[S3] Start get data attribute.\n");
    attributes[0] = MEM_RANGE_ATTR_LAST_PREFETCH_LOC;
    attributes[1] = MEM_RANGE_ATTR_LAST_PREFETCH_LOC_TYPE;
    attributes[2] = MEM_RANGE_ATTR_LAST_PREFETCH_LOC_ID;

    for (int i = 0; i < num_attributes; i++) {
        size[i] = sizeof(int32_t);
    }
    int32_t attr_res[num_attributes][2];
    uint64_t attr_res_addr[num_attributes];
    for (int idx = 0; idx < num_attributes; idx++) {
        attr_res_addr[idx] = (uint64_t)attr_res[idx];
    }

    CHECK_ERROR(halMemManagedRangeGetAttributes((void **)attr_res_addr, size, attributes, num_attributes,
                                                (DVdeviceptr)x_Data, inputByteSize));
    LOG_INFO("MEM_RANGE_ATTR_LAST_PREFETCH_LOC=%d\n", attr_res[0][0]);
    LOG_INFO("MEM_RANGE_ATTR_LAST_PREFETCH_LOC_TYPE=%d, DRV_UVM_LOCATION_TYPE_DEVICE=%d\n", attr_res[1][0],
             DRV_UVM_LOCATION_TYPE_DEVICE);
    LOG_INFO("MEM_RANGE_ATTR_LAST_PREFETCH_LOC_ID=%d\n", attr_res[2][0]);
    if (attr_res[0][0] != 0 || attr_res[1][0] != DRV_UVM_LOCATION_TYPE_DEVICE || attr_res[2][0] != 0) {
        LOG_ERR("multi attrs query failed.");
        ret = -1;
        goto exit;
    }
    LOG_INFO("Get data attribute success.\n");

    // check the data on the host
    ret = check_u8_value_data(x_Data, value, inputByteSize);
    if (ret != 0) {
        LOG_ERR("Check x value in host failed.\n");
        ret = -1;
        goto exit;
    } else {
        LOG_INFO("Check x value in host success.\n");
    }

    // memcopy: copy x to y
    ret = drvMemcpy((DVdeviceptr)y, inputByteSize, (DVdeviceptr)x, inputByteSize);
    if (ret != 0) {
        LOG_ERR("drvMemcpy failed: ret=%d, src=0x%lx, dst=0x%lx, size=%u\n", ret, (unsigned long)x, (unsigned long)y,
                inputByteSize);
        goto exit;
    }
    // check if x and y match
    err_count = check_uint8_data(y_Data, 0, x_Data, 0, inputByteSize);
    if (err_count != 0) {
        LOG_ERR("copy_size=%lu;  err_count=%lu\n", (unsigned long)inputByteSize, (unsigned long)err_count);
        ret = -1;
    }
    LOG_INFO("memcopy success.\n");

exit:
    // Release resources
    if (x != NULL) {
        CHECK_ERROR(halMemFree(x));
    }
    if (y != NULL) {
        CHECK_ERROR(halMemFree(y));
    }

    return ret;
}

int32_t st_uvm_test_002(void *hdc_session, uint32_t devid, uint64_t arg[10])
{
    int ret = 0;
    uint32_t blockDim = 8;
    uint32_t inputByteSize = 2 << 20; // 2 << 20 = 2M
    void *x = NULL;
    uint8_t *x_Data = NULL;
    uint8_t value = 0;
    struct drv_uvm_location location;

    // Allocate 2MB of UVM memory and return a virtual address(x) within the UVM region.
    CHECK_ERROR(halMemAlloc(&x, inputByteSize, MEM_UVM));

    x_Data = (uint8_t *)x;

    // Setting the read-mostly attribute on x_Data
    struct drv_uvm_location loc = {DRV_UVM_LOCATION_TYPE_HOST, 0};
    CHECK_ERROR(halMemManagedAdvise((DVdeviceptr)x_Data, inputByteSize, READ_MOSTLY, loc));

    // memsets the value into x
    CHECK_ERROR(drvMemsetD8((DVdeviceptr)x, inputByteSize, value, inputByteSize));
    LOG_INFO("Init data success. (inputByteSize=%#xB, x_Data[0]=%u)\n", inputByteSize, x_Data[0]);

    // halMemManagedPrefetch prefetches x_Data (size inputByteSize) to the device 0
    LOG_INFO("[S2] Start prefetch data from host to device 0.\n");
    location.type = DRV_UVM_LOCATION_TYPE_DEVICE;
    location.id = 0;
    CHECK_ERROR(halMemManagedPrefetch((DVdeviceptr)x_Data, inputByteSize, location, 0));
    LOG_INFO("Prefetch data to device 0 success.\n");

    // check the data on the host
    ret = check_u8_value_data(x_Data, value, inputByteSize);
    if (ret != 0) {
        LOG_ERR("Check x value in host failed.\n");
        ret = -1;
        goto exit;
    } else {
        LOG_INFO("Check x value in host success.\n");
    }

exit:
    // Release resources
    if (x != NULL) {
        CHECK_ERROR(halMemFree(x));
    }

    return ret;
}

int hlt_uvm_develop_st(int argc, char **arg)
{
    HDC_SESSION session = NULL;
    int ret;
    uint32_t devid = 0;
    uint64_t arg_list[10] = {0};

    ret = st_svm_open_cp(1, devid, &session);
    if (ret) {
        LOG_ERR("open_failed.\n");
        return ret;
    }

    ret = st_uvm_test_001(&session, devid, arg_list);
    if (ret) {
        LOG_ERR("st_uvm_test_001 failed.\n");
        return ret;
    }
    LOG_INFO("st_uvm_test_001 success.\n");

    ret = st_uvm_test_002(&session, devid, arg_list);
    if (ret) {
        LOG_ERR("st_uvm_test_002 failed.\n");
        return ret;
    }
    LOG_INFO("st_uvm_test_002 success.\n");

    st_svm_close_cp(1, devid);

    if (ret == 0) {
        LOG_INFO("testcase success.\n");
    } else {
        LOG_INFO("testcase failed.\n");
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    HDC_SESSION session = NULL;
    uint32_t devid = 0;
    uint64_t arg_list[10] = {0};

    LOG_INFO("In main.\n");
    ret = hlt_uvm_develop_st(argc, argv);

    return ret;
}