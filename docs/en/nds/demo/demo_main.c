/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * NPU Direct Storage (NDS) demo program — shared by both zero-copy paths.
 *
 * The NDS client library (libnpu_file.so) abstracts the difference between the
 * two kernel zero-copy implementations:
 *
 *   - Path A (VFS shadow buffer):  the library mmaps the shadow buffer and
 *     drives the VFS O_DIRECT path; the kernel resolves the file layout.
 *   - Path B (NVMe passthrough):   the library queries FS_IOC_FIEMAP for the
 *     on-disk layout and hands the extents to the kernel.
 *
 * From the demo's point of view the `npu_file_*` API is identical on both
 * paths, so a single demo_main.c can exercise either one — link against the
 * appropriate `libnpu_file.so` to select the path.
 *
 * Mirrors cufile-samples/cufile_rw.cc but for Ascend NPU HBM:
 *
 *   1. TsdOpen + drvMemDeviceOpen — open NPU device context (CUDA init)
 *   2. halMemAlloc          — allocate NPU HBM        (cudaMalloc)
 *   3. posix_memalign       — allocate host buffer     (cudaMallocHost)
 *   4. drvMemsetD8          — fill NPU with 0xAB        (cudaMemset)
 *   5. open(O_CREAT|O_RDWR|O_DIRECT) + fallocate
 *   6. npu_file_open + buf_register — pin NPU mgroup    (cuFileHandleRegister)
 *   7. npu_file_write        — NPU HBM -> file  (zero-copy)
 *   8. drvMemsetD8          — clear NPU HBM            (cudaMemset 0)
 *   9. npu_file_read         — file -> NPU HBM  (zero-copy)
 *  10. drvMemcpy D2H + verify — NPU -> host, compare pattern (cudaMemcpy D2H)
 *
 * drvMemsetD8 / drvMemcpy are NEVER in the data path (steps 7, 9).  They are
 * only used for fill (4), clear (8), and verify (10).
 *
 * Usage: npu_nds_demo [path] [size]
 *   path  : test file path        (default: /tmp/npu_nds_demo.data)
 *   size  : transfer size in bytes (default: 1 MiB, aligned up to 4096)
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <dlfcn.h>

#include "ascend_hal.h"
#include "ascend_hal_define.h"
#include "ascend_hal_error.h"
#include "utils.h"
#include "npu_file.h"

#define NDS_BLOCK_SIZE 4096
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

#define PATTERN 0xAB

#define NPU_CHECK(call)                           \
    do {                                          \
        int _r = (int)(call);                     \
        if (_r) {                                 \
            LOG_ERR("NPU call failed: %d\n", _r); \
            goto fail;                            \
        }                                         \
    } while (0)

#define NDS_CHECK(call)                           \
    do {                                          \
        int _r = (call);                          \
        if (_r) {                                 \
            LOG_ERR("NDS call failed: %d\n", _r); \
            goto fail;                            \
        }                                         \
    } while (0)

typedef uint32_t (*func_tsd_open_t)(uint32_t, uint32_t);
typedef uint32_t (*func_tsd_close_t)(uint32_t);

static int open_npu_device(uint32_t devid)
{
    void *handle = dlopen("libtsdclient.so", RTLD_LAZY);
    func_tsd_open_t func;

    if (!handle) {
        LOG_ERR("dlopen libtsdclient.so failed: %s\n", dlerror());
        return -1;
    }
    func = (func_tsd_open_t)dlsym(handle, "TsdOpen");
    if (!func || func(devid, 0) != 0) {
        LOG_ERR("TsdOpen failed\n");
        dlclose(handle);
        return -1;
    }
    dlclose(handle);

    return drvMemDeviceOpen(devid, 0);
}

static void close_npu_device(uint32_t devid)
{
    void *handle = dlopen("libtsdclient.so", RTLD_LAZY);
    func_tsd_close_t func;

    (void)drvMemDeviceClose(devid);
    if (handle) {
        func = (func_tsd_close_t)dlsym(handle, "TsdClose");
        if (func)
            func(devid);
        dlclose(handle);
    }
}

/*
 * Make sure the test file has a contiguous (or at least FIEMAP-mappable)
 * on-disk allocation for the requested size.  We fallocate the range so the
 * filesystem yields the physical blocks that Path B (NVMe passthrough) needs
 * to build its SGL; Path A (VFS shadow buffer) simply ignores the explicit
 * layout and relies on the kernel O_DIRECT path.
 */
static int prepare_file(int fd, size_t size)
{
    if (fallocate(fd, 0, 0, (off_t)size) < 0) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        LOG_ERR("fallocate failed: %s\n", errbuf);
        return -errno;
    }
    return 0;
}

static int demo_verify(uint8_t *hbuf, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (hbuf[i] != (uint8_t)PATTERN) {
            LOG_ERR("verify failed @ %zu: got 0x%02x, expect 0x%02x\n", i, hbuf[i], (uint8_t)PATTERN);
            return 1;
        }
    }
    LOG_INFO("verify OK (%zu bytes, all 0x%02x)\n", size, PATTERN);
    return 0;
}

static void demo_cleanup(uint32_t devid, uint64_t npu_va, uint8_t *hbuf, int file_fd, const char *path,
                         npu_file_ctx_t *nds_ctx, npu_file_handle_t *handle, int rc)
{
    (void)npu_file_buf_deregister(nds_ctx, handle);
    npu_file_close(nds_ctx);
    if (file_fd >= 0) {
        close(file_fd);
        unlink(path);
    }
    if (npu_va)
        (void)halMemFree((void *)npu_va);
    free(hbuf);
    close_npu_device(devid);
    if (rc == 0)
        LOG_INFO("NPU Direct Storage demo SUCCESS.\n");
    else
        LOG_ERR("NPU Direct Storage demo FAILED.\n");
}

static int demo_io_roundtrip(npu_file_ctx_t *nds_ctx, npu_file_handle_t *handle, int file_fd, uint64_t npu_va,
                             uint8_t *hbuf, const char *path, size_t size)
{
    ssize_t n;
    int ret;

    n = npu_file_write(nds_ctx, handle, file_fd, 0, size, 0);
    if (n < 0) {
        LOG_ERR("npu_file_write failed: %zd\n", n);
        return 1;
    }
    LOG_INFO("npu_file_write: %zd bytes (NPU HBM -> %s)\n", n, path);

    ret = drvMemsetD8((DVdeviceptr)npu_va, size, 0, size);
    if (ret) {
        LOG_ERR("NPU call failed: %d\n", ret);
        return 1;
    }
    LOG_INFO("Cleared NPU HBM.\n");

    n = npu_file_read(nds_ctx, handle, file_fd, 0, size, 0);
    if (n < 0) {
        LOG_ERR("npu_file_read failed: %zd\n", n);
        return 1;
    }
    LOG_INFO("npu_file_read: %zd bytes (%s -> NPU HBM)\n", n, path);

    ret = drvMemcpy((DVdeviceptr)(uintptr_t)hbuf, size, (DVdeviceptr)npu_va, size);
    if (ret) {
        LOG_ERR("NPU call failed: %d\n", ret);
        return 1;
    }
    return demo_verify(hbuf, size);
}

static int demo_run(const char *path, size_t size, uint32_t devid)
{
    int rc = 0;
    uint64_t npu_va = 0;
    uint8_t *hbuf = NULL;
    int file_fd = -1;
    npu_file_ctx_t nds_ctx = {0};
    npu_file_handle_t handle = {0};

    LOG_INFO("NPU Direct Storage demo (zero-copy P2P) start.\n");
    LOG_INFO("path=%s size=%zu\n", path, size);

    rc = open_npu_device(devid);
    if (rc) {
        LOG_ERR("open_npu_device failed: %d\n", rc);
        return rc;
    }

    NPU_CHECK(halMemAlloc((void **)&npu_va, size, MEM_DEV | devid | MEM_TYPE_HBM | MEM_PAGE_HUGE));
    LOG_INFO("Allocated NPU HBM: va=0x%llx size=%zu\n", (unsigned long long)npu_va, size);

    if (posix_memalign((void **)&hbuf, NDS_BLOCK_SIZE, size)) {
        LOG_ERR("posix_memalign failed\n");
        rc = 1;
        goto fail;
    }

    NPU_CHECK(drvMemsetD8((DVdeviceptr)npu_va, size, PATTERN, size));
    LOG_INFO("Filled NPU HBM with 0x%02x pattern.\n", PATTERN);

    file_fd = open(path, O_CREAT | O_RDWR | O_DIRECT, 0664);
    if (file_fd < 0) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        LOG_ERR("open(%s) failed: %s\n", path, errbuf);
        rc = 1;
        goto fail;
    }
    rc = prepare_file(file_fd, size);
    if (rc)
        goto fail;

    NDS_CHECK(npu_file_open(&nds_ctx));
    NDS_CHECK(npu_file_buf_register(&nds_ctx, npu_va, size, &handle));

    rc = demo_io_roundtrip(&nds_ctx, &handle, file_fd, npu_va, hbuf, path, size);

fail:
    demo_cleanup(devid, npu_va, hbuf, file_fd, path, &nds_ctx, &handle, rc);
    return rc;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/tmp/npu_nds_demo.data";
    size_t size = (argc > 2) ? (size_t)strtoull(argv[2], NULL, 0) : (1UL << 20);
    size = ALIGN_UP(size, NDS_BLOCK_SIZE);
    return demo_run(path, size, 0);
}
