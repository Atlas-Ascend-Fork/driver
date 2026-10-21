/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "msnpureport_report.h"
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include "mmpa_api.h"
#include "adx_api.h"
#include "adcore_api.h"
#include "adump_device_pub.h"
#include "ascend_hal.h"
#include "dsmi_common_interface.h"
#include "msnpureport_print.h"
#include "msnpureport_utils.h"
#include "msnpureport_filedump.h"
#include "msnpureport_config.h"
#include "msnpureport_file_mgr.h"
#include "log_common.h"
#include "bbox_dump_lib.h"
#include "log_communication.h"
#include "extra_config.h"
#include "ide_daemon_api.h"

#define SLOG_PATH "slog"
#define STACKCORE_PATH "stackcore"
#define MESSAGE_PATH "message"
#define EVENT_PATH "event_sched"
#define SYSTEM_INFO_PATH "system_info"
#define DRV_DSMI_LIB "libdrvdsmi_host.so"
#define TIME_SIZE 128
#define DEFAULT_JOINABLE_THREAD_ATTR {0, 0, 0, 0, 0, 1, 128 * 1024} // Default ThreadSize(128KB), joinable
#define BLOCK_RETURN_CODE 4                                         // device check hdc-client is in docker
#define TMP_STR_SIZE 16
#define FAILED_LABELS_SIZE 128
#define FAILED_LABEL_SEPARATOR_LEN 2
#define DEFAULT_TIMEOUT 10000U // 10000ms
#define MAX_DIR_NUM 4
#define ACK_LEN 64
#define RECV_BUFF_SIZE (1024 * 1024 + sizeof(LogReportMsg))
#define MAX_RETRY_TIME 3
#define RETRY_WAIT_TIME 10000
#define MSN_ACK_SEND_TIMEOUT_MS 3000 // 3s, align with typical hdc kernel session default send timeout
#define FAULT_EVENT_RETRY_TIMES 2U
#define SLOGD_WAIT_TIME 9 // 9s
#define MAX_RECV_TIMEOUT_COUNT 5
#define INVALID_DEV_ID (-1) // BboxStartDump input this id means all device
#define PERMANENT_TIMEOUT 1000U
#define WAIT_ALL_DEVICE_UPDATE_TIME 10
#define INVALID_MASTER_ID (-1)
#define DEFAULT_BBOX_SIZE (10 * 1024 * 1024)                     // 10MB = 10 * 1024 * 1024 字节
#define DISK_SPACE_WARNING_THRESHOLD (20ULL * 1024ULL * 1024ULL) // 20MB

typedef int32_t (*DsmiSubscribeFaultEvent)(int, struct dsmi_event_filter, fault_event_callback);

typedef struct {
    int32_t logType;
    uint32_t logicId;
    int64_t devOsId;
    uint32_t phyId;
    bool isThreadExit;
} ThreadArgInfo;

typedef struct {
    mmUserBlock_t block;
    mmThread tid;
} ThreadInfo;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t retCode;
    uint8_t reserve[116]; // reserve 124 bytes
    char retMsg[128];     // msg length 128 bytes
} MsnServerResultInfo;

typedef struct {
    uint32_t devId; // user input logic id
    uint32_t phyId;
    int64_t masterId;
    uint32_t devNum;
    bool isSMPEnv;
    uint32_t devIds[MAX_DEV_NUM];   // all device logic id
    uint32_t phyIds[MAX_DEV_NUM];   // map logic id to physical id
    int64_t masterIds[MAX_DEV_NUM]; // map logic id to master id
} MsnDeviceInfo;

STATIC char g_logPath[MMPA_MAX_PATH + 1] = {0};
STATIC ThreadInfo g_slogdThread[MAX_DEV_NUM];
STATIC ThreadInfo g_logDaemonThread[MAX_DEV_NUM];
static ThreadArgInfo g_threadArgInfo[MAX_DEV_NUM];
static void *g_drvDsmiLibHandle = NULL;
static MsnDeviceInfo g_msnDeviceInfo = {0};

static const char *GetScriptDumpHostPath(const char *label)
{
    size_t itemNum = sizeof(MSNPUREPORT_FILE_DUMP_INFO) / sizeof(MSNPUREPORT_FILE_DUMP_INFO[0]);
    for (size_t i = 0; i < itemNum; ++i) {
        if (strcmp(label, MSNPUREPORT_FILE_DUMP_INFO[i].label) == 0) {
            return MSNPUREPORT_FILE_DUMP_INFO[i].hostFilePath;
        }
    }
    return NULL;
}

/**
 * @brief MsnReportInitDevInfo: get and save all device info
 * @param [in]devId: user input logic device id
 * @return EN_OK: success EN_ERROR: failed
 */
STATIC int32_t MsnReportInitDevInfo(uint32_t devId)
{
    int32_t ret = 0;
    g_msnDeviceInfo.devId = devId;
    if (devId == MAX_DEV_NUM) {
        g_msnDeviceInfo.masterId = MAX_DEV_NUM;
    } else {
        ret = MsnGetDevMasterId(devId, &g_msnDeviceInfo.phyId, &g_msnDeviceInfo.masterId);
        ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Get master id failed, devId:%u, ret:%d", devId, ret);
    }

    ret = MsnGetDevIDs(&g_msnDeviceInfo.devNum, g_msnDeviceInfo.devIds, MAX_DEV_NUM);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Get all device id failed, ret:%d", ret);

    int32_t masterIdArr[MAX_DEV_NUM] = {0};
    int64_t masterId = 0;
    uint32_t phyId = 0;
    g_msnDeviceInfo.isSMPEnv = false;
    for (uint32_t i = 0; i < g_msnDeviceInfo.devNum; i++) {
        uint32_t logicId = g_msnDeviceInfo.devIds[i];
        ret = MsnGetDevMasterId(logicId, &phyId, &masterId);
        if (ret != EN_OK) {
            g_msnDeviceInfo.masterIds[logicId] = INVALID_MASTER_ID;
            continue;
        }
        g_msnDeviceInfo.masterIds[logicId] = masterId;
        g_msnDeviceInfo.phyIds[logicId] = phyId;

        if (masterIdArr[masterId] == 1) {
            g_msnDeviceInfo.isSMPEnv = true;
        } else {
            masterIdArr[masterId] = 1;
        }
    }

    return EN_OK;
}

/**
 * @brief MsnRewriteFirmwareLogPath: AdxGetSpecifiedFile 拉回的 firmware log
 *        路径里 device-<phyId> 是 device 侧 SlogdFirmwareLogGet 拼好的 phy id,
 *        USE_LOGIC_ID 启用时把目录 device-<phyId> 重命名为 device-<logicId>,
 *        目录内 device-<phyId>_*.log 文件名也同步改为 device-<logicId>_*.log
 * @param [in]fullPath: dev-os 目录全路径(<g_logPath>/slog/dev-os-<logicId>)
 * @param [in]phyId: device 侧用的 phy id
 * @param [in]logicId: host 侧逻辑 id
 */
#ifdef USE_LOGIC_ID
STATIC void MsnRewriteFirmwareLogPath(const char *fullPath, uint32_t phyId, uint32_t logicId)
{
    if (phyId == logicId) {
        return;
    }
    char debugDir[MMPA_MAX_PATH + 1] = {0};
    int32_t ret = snprintf_s(debugDir, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/debug", fullPath);
    ONE_ACT_WARN_LOG(ret == -1, return, "snprintf debugDir failed.");
    char oldDir[MMPA_MAX_PATH + 1] = {0};
    char newDir[MMPA_MAX_PATH + 1] = {0};
    ret = snprintf_s(oldDir, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/device-%u", debugDir, phyId);
    ONE_ACT_WARN_LOG(ret == -1, return, "snprintf oldDir failed.");
    ret = snprintf_s(newDir, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/device-%u", debugDir, logicId);
    ONE_ACT_WARN_LOG(ret == -1, return, "snprintf newDir failed.");
    if (mmAccess(oldDir) != EN_OK) {
        return; // 该 device 没 firmware log 目录
    }

    char oldFilePrefix[TMP_STR_SIZE] = {0};
    char newFilePrefix[TMP_STR_SIZE] = {0};
    ret = snprintf_s(oldFilePrefix, TMP_STR_SIZE, TMP_STR_SIZE - 1, "device-%u_", phyId);
    ONE_ACT_WARN_LOG(ret == -1, return, "snprintf oldFilePrefix failed.");
    ret = snprintf_s(newFilePrefix, TMP_STR_SIZE, TMP_STR_SIZE - 1, "device-%u_", logicId);
    ONE_ACT_WARN_LOG(ret == -1, return, "snprintf newFilePrefix failed.");
    size_t oldPrefixLen = strlen(oldFilePrefix);

    DIR *dir = opendir(oldDir);
    ONE_ACT_WARN_LOG(dir == NULL, return, "open dir %s failed.", oldDir);
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0)) {
            continue;
        }
        if (strncmp(ent->d_name, oldFilePrefix, oldPrefixLen) != 0) {
            continue; // 不匹配 device-<phyId>_ 前缀, 不动
        }
        char oldFile[MMPA_MAX_PATH + 1] = {0};
        char newFile[MMPA_MAX_PATH + 1] = {0};
        ret = snprintf_s(oldFile, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s", oldDir, ent->d_name);
        ONE_ACT_WARN_LOG(ret == -1, continue, "snprintf oldFile failed.");
        ret = snprintf_s(newFile, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s%s", oldDir, newFilePrefix,
                         ent->d_name + oldPrefixLen);
        ONE_ACT_WARN_LOG(ret == -1, continue, "snprintf newFile failed.");
        if (rename(oldFile, newFile) != 0) {
            SELF_LOG_WARN("rename %s -> %s failed, errno=%d", oldFile, newFile, errno);
        }
    }
    closedir(dir);

    if (rename(oldDir, newDir) != 0) {
        SELF_LOG_WARN("rename dir %s -> %s failed, errno=%d", oldDir, newDir, errno);
    }
}
#endif

static int32_t MsnBuildSpecificLogPath(const ThreadArgInfo *threadArgInfo, const char *logPath, char *fullPath,
                                       const char **hostFilePath)
{
    int32_t ret = 0;
    char subDir[TMP_STR_SIZE] = {0};
    *hostFilePath = logPath;
    const char *scriptHostPath = GetScriptDumpHostPath(logPath);
    if (scriptHostPath != NULL) {
        *hostFilePath = scriptHostPath;
    } else if (strcmp(logPath, SINGLE_EXPORT_LOG) == 0) {
        *hostFilePath = SLOG_PATH;
    } else if (strcmp(logPath, EVENT_PATH) == 0) {
        *hostFilePath = SYSTEM_INFO_PATH;
        ret = snprintf_s(subDir, TMP_STR_SIZE, TMP_STR_SIZE - 1, "/%s", EVENT_PATH);
        ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "snprintf_s failed for %s, strerr=%s.", EVENT_PATH,
                        strerror(mmGetErrorCode()));
    }
#ifdef USE_LOGIC_ID
    ret = snprintf_s(fullPath, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s/dev-os-%u%s", g_logPath, *hostFilePath,
                     threadArgInfo->logicId, subDir);
#else
    ret = snprintf_s(fullPath, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s/dev-os-%ld%s", g_logPath, *hostFilePath,
                     threadArgInfo->devOsId, subDir);
#endif
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "copy path failed, device os %ld, strerr=%s.", threadArgInfo->devOsId,
                    strerror(mmGetErrorCode()));
    return MsnMkdirMulti(fullPath);
}

static int32_t MsnFetchSpecificLogs(const ThreadArgInfo *threadArgInfo, const char *fullPath, const char *logPath,
                                    uint32_t timeout)
{
    if (strcmp(logPath, SINGLE_EXPORT_LOG) != 0) {
        return AdxGetDeviceFileTimeout((uint16_t)(threadArgInfo->phyId), fullPath, logPath, timeout);
    }
#ifdef HDC_NEW_CHANNEL
    return AdxGetSpecifiedFile((uint16_t)(threadArgInfo->phyId), fullPath, logPath, (int32_t)HDC_SERVICE_TYPE_PROFILING,
                               (int32_t)COMPONENT_SYS_GET);
#else
    return AdxGetSpecifiedFile((uint16_t)(threadArgInfo->phyId), fullPath, logPath, (int32_t)HDC_SERVICE_TYPE_LOG,
                               (int32_t)COMPONENT_SYS_GET);
#endif
}

/**
 * @brief GetSpecificLogs: get specific logs
 * @param [in]threadArgInfo: thread arg info
 * @param [in]fullPath: splice g_logPath and logPath eg: 2021-12-30-03-19-54/message
 * @param [in]logicId: logicId
 * @param [in]logType: logType
 * @param [in]logPath: logPath
 * @return EN_OK/EN_ERROR
 */
static int GetSpecificLogs(const ThreadArgInfo *threadArgInfo, int logType, const char *logPath, uint32_t timeout)
{
    if ((threadArgInfo->logType != (int32_t)ALL_LOG) && (threadArgInfo->logType != (int32_t)logType)) {
        return EN_OK;
    }

    char fullPath[MMPA_MAX_PATH + 1] = {0};
    const char *hostFilePath = NULL;
    if (MsnBuildSpecificLogPath(threadArgInfo, logPath, fullPath, &hostFilePath) != EN_OK) {
        return EN_ERROR;
    }
    int32_t ret = MsnFetchSpecificLogs(threadArgInfo, fullPath, logPath, timeout);
    if (ret != 0) {
        if (ret == BLOCK_RETURN_CODE) {
            SELF_LOG_ERROR("export files failed, can not export files in docker.");
        } else if (ret == (int32_t)IDE_DAEMON_HDC_TIMEOUT) {
#ifdef USE_LOGIC_ID
            MSNPU_WAR("Export %s timed out. Retrying later may help. (device_id=%u)", logPath, threadArgInfo->logicId);
#else
            MSNPU_WAR("Export %s timed out. Retrying later may help. (device_id=%ld)", logPath, threadArgInfo->devOsId);
#endif
            return ret;
        } else {
#ifdef USE_LOGIC_ID
            SELF_LOG_ERROR("get device files failed by adx, device os %u.", threadArgInfo->logicId);
#else
            SELF_LOG_ERROR("get device files failed by adx, device os %ld.", threadArgInfo->devOsId);
#endif
        }
        return EN_ERROR;
    }
#ifdef USE_LOGIC_ID
    // device 侧 SlogdFirmwareLogGet 用 phyId 拼 debug/device-<phyId>/device-<phyId>_*.log,
    // host 侧改用 logicId 命名, 与外层 dev-os-<logicId> 保持一致
    if (strcmp(logPath, SINGLE_EXPORT_LOG) == 0) {
        MsnRewriteFirmwareLogPath(fullPath, threadArgInfo->phyId, threadArgInfo->logicId);
    }
    SELF_LOG_INFO("Export logs to dir succeed: %s/dev-os-%u", hostFilePath, threadArgInfo->logicId);
#else
    SELF_LOG_INFO("Export logs to dir succeed: %s/dev-os-%ld", hostFilePath, threadArgInfo->devOsId);
#endif
    return EN_OK;
}

static int32_t GetSystemInfoLogs(const ThreadArgInfo *threadArgInfo, char *failedLabels, size_t failedLabelsSize)
{
    size_t itemNum = sizeof(MSNPUREPORT_FILE_DUMP_INFO) / sizeof(MSNPUREPORT_FILE_DUMP_INFO[0]);
    uint32_t mask = 1U << threadArgInfo->logType;
    int32_t result = EN_OK;
    for (size_t i = 0; i < itemNum; ++i) {
        if ((MSNPUREPORT_FILE_DUMP_INFO[i].type & mask) == 0) {
            continue;
        }
        int32_t ret = GetSpecificLogs(threadArgInfo, threadArgInfo->logType, MSNPUREPORT_FILE_DUMP_INFO[i].label,
                                      MSNPUREPORT_FILE_DUMP_INFO[i].timeout);
        if (ret != EN_OK) {
            if (failedLabels != NULL) {
                int32_t appendRet = strcat_s(failedLabels, failedLabelsSize, MSNPUREPORT_FILE_DUMP_INFO[i].label);
                if (appendRet == EOK) {
                    appendRet = strcat_s(failedLabels, failedLabelsSize, ", ");
                }
                if (appendRet != EOK) {
                    SELF_LOG_ERROR("Failed to append export label. (label=%s, ret=%d)\n",
                                   MSNPUREPORT_FILE_DUMP_INFO[i].label, appendRet);
                }
            }
            if (ret != (int32_t)IDE_DAEMON_HDC_TIMEOUT) {
                result = EN_ERROR;
            } else if (result == EN_OK) {
                result = (int32_t)IDE_DAEMON_HDC_TIMEOUT;
            }
        }
    }
    return result;
}

static bool MsnExportSpecificLogs(const ThreadArgInfo *threadArgInfo, int32_t logType, const char *logPath,
                                  const char *logName, uint64_t devOsIdForPrint)
{
    int32_t ret = GetSpecificLogs(threadArgInfo, logType, logPath, DEFAULT_TIMEOUT);
    if (ret == EN_OK || ret == (int32_t)IDE_DAEMON_HDC_TIMEOUT) {
        return true;
    }
    SELF_LOG_ERROR("export %s failed, device os %lu", logName, devOsIdForPrint);
    return false;
}

static bool MsnExportSystemInfoLogs(const ThreadArgInfo *threadArgInfo, uint64_t devOsIdForPrint)
{
    char failedLabels[FAILED_LABELS_SIZE] = {0};
    int32_t ret = GetSystemInfoLogs(threadArgInfo, failedLabels, sizeof(failedLabels));
    if (ret == EN_OK || ret == (int32_t)IDE_DAEMON_HDC_TIMEOUT) {
        return true;
    }
    size_t len = strlen(failedLabels);
    if (len > FAILED_LABEL_SEPARATOR_LEN) {
        failedLabels[len - FAILED_LABEL_SEPARATOR_LEN] = '\0';
    }
    SELF_LOG_ERROR("export [%s] failed, device os %lu", failedLabels, devOsIdForPrint);
    return false;
}

STATIC void *SlogSyncThread(void *arg)
{
    ONE_ACT_ERR_LOG(arg == NULL, return NULL, "arg of SlogSyncThread is null.");
    ThreadArgInfo *threadArgInfo = (ThreadArgInfo *)arg;
    char threadName[TMP_STR_SIZE] = {0};
    int32_t ret = 0;
    ret = sprintf_s(threadName, TMP_STR_SIZE, "MsnLogRecv%ld", threadArgInfo->devOsId);
    ONE_ACT_ERR_LOG(ret == -1, return NULL, "Call sprintf_s failed for thread name %ld.", threadArgInfo->devOsId);
    ret = ToolSetThreadName(threadName);
    NO_ACT_WARN_LOG(ret != SYS_OK, "can not set thread name %s.", threadName);

    bool successFlag = true;
#ifdef USE_LOGIC_ID
    uint64_t devOsIdForPrint = (uint64_t)threadArgInfo->logicId;
#else
    uint64_t devOsIdForPrint = (uint64_t)threadArgInfo->devOsId;
#endif
    if (!MsnIsPoolEnv()) {
        successFlag &= MsnExportSpecificLogs(threadArgInfo, (int32_t)SLOG_LOG, SLOG_PATH, "device app",
                                             devOsIdForPrint);
        successFlag &= MsnExportSpecificLogs(threadArgInfo, (int32_t)SLOG_LOG, SINGLE_EXPORT_LOG, "slog",
                                             devOsIdForPrint);
    }
    successFlag &= MsnExportSpecificLogs(threadArgInfo, (int32_t)STACKCORE_LOG, STACKCORE_PATH, "stackcore",
                                         devOsIdForPrint);
    successFlag &= MsnExportSpecificLogs(threadArgInfo, (int32_t)SLOG_LOG, MESSAGE_PATH, "message", devOsIdForPrint);
    successFlag &= MsnExportSystemInfoLogs(threadArgInfo, devOsIdForPrint);
    successFlag &= MsnExportSpecificLogs(threadArgInfo, (int32_t)SLOG_LOG, EVENT_PATH, "event_sched", devOsIdForPrint);

    if (!successFlag) {
        MSNPU_FPRINTF("ERROR: export device os %lu files failed, check syslog for more information.\n",
                      devOsIdForPrint);
    }
    return NULL;
}

static int32_t MsnReportCreateThreadForAllDevice(ThreadInfo *threadInfo, uint32_t threadNum, bool isUseMasterId)
{
    (void)threadNum;
    mmThreadAttr threadAttr = DEFAULT_JOINABLE_THREAD_ATTR;

    bool devFlag[MAX_DEV_NUM] = {false};
    int64_t devOsId = 0;
    uint32_t i;
    for (i = 0; i < g_msnDeviceInfo.devNum; i++) {
        uint32_t logicId = g_msnDeviceInfo.devIds[i];
        if (isUseMasterId) {
            if (g_msnDeviceInfo.masterIds[logicId] == INVALID_MASTER_ID) {
                continue;
            }
            devOsId = g_msnDeviceInfo.masterIds[logicId];
        } else {
            if (logicId >= threadNum) {
                continue;
            }
            devOsId = (int64_t)logicId;
        }
        if (devFlag[devOsId] == true) {
            continue;
        }
        int32_t ret = mmCreateTaskWithThreadAttr(&threadInfo[logicId].tid, &threadInfo[logicId].block, &threadAttr);
        if (ret != EN_OK) {
            SELF_LOG_ERROR("create task(SlogSyncThread) failed, dev_id=%u, strerr=%s.", g_msnDeviceInfo.devIds[i],
                           strerror(mmGetErrorCode()));
            continue;
        }
        devFlag[devOsId] = true;
    }
    // all threads failed to be created, return ERROR
    for (i = 0; i < MAX_DEV_NUM; i++) {
        if (devFlag[i]) {
            return EN_OK;
        }
    }
    SELF_LOG_ERROR("All device thread create failed");
    return EN_ERROR;
}

static uint32_t MsnGetAllLogicIdByMatserId(int64_t masterId, uint32_t *logicIds, uint32_t size)
{
    uint32_t idNum = 0;
    for (uint32_t i = 0; i < g_msnDeviceInfo.devNum; i++) {
        if ((g_msnDeviceInfo.masterIds[g_msnDeviceInfo.devIds[i]] == masterId) && (idNum < size)) {
            logicIds[idNum] = g_msnDeviceInfo.devIds[i];
            idNum++;
        }
    }
    return idNum;
}

static void MsnSetThreadInfo(ThreadInfo *threadInfo, ThreadArgInfo *threadArg, void *(*func)(void *), int32_t logType)
{
    for (uint32_t i = 0; i < g_msnDeviceInfo.devNum; i++) {
        uint32_t logicId = g_msnDeviceInfo.devIds[i];
        threadArg[logicId].devOsId = g_msnDeviceInfo.masterIds[logicId];
        threadArg[logicId].phyId = g_msnDeviceInfo.phyIds[logicId];
        threadArg[logicId].logicId = logicId;
        threadArg[logicId].isThreadExit = false;
        threadArg[logicId].logType = logType;
        threadInfo[logicId].block.procFunc = func;
        threadInfo[logicId].block.pulArg = (void *)&threadArg[logicId];
    }
}

STATIC int32_t MsnReportCreateThreadSingle(uint32_t devId, ThreadInfo *threadInfo)
{
    mmThreadAttr threadAttr = DEFAULT_JOINABLE_THREAD_ATTR;
    int32_t ret = mmCreateTaskWithThreadAttr(&threadInfo[devId].tid, &threadInfo[devId].block, &threadAttr);
    if (ret != EN_OK) {
        SELF_LOG_ERROR("Create Report Thread failed, dev_id=%u, strerr=%s.", devId, strerror(mmGetErrorCode()));
        return EN_ERROR;
    }
    return EN_OK;
}

STATIC int32_t MsnReportCreateThreadSingleOs(uint32_t devId, ThreadInfo *threadInfo, bool isUseMasterId)
{
    int64_t devOsId = g_msnDeviceInfo.masterIds[devId];
    if (devOsId == INVALID_MASTER_ID) {
        return EN_ERROR;
    }

    uint32_t logicIds[MAX_DEV_NUM] = {0};
    uint32_t logicNum = MsnGetAllLogicIdByMatserId(devOsId, logicIds, MAX_DEV_NUM);
    int32_t ret = EN_ERROR;
    for (uint32_t i = 0; i < logicNum; i++) {
        if (MsnReportCreateThreadSingle(logicIds[i], threadInfo) != EN_OK) {
            continue;
        }
        // all threads failed to be created, return ERROR
        ret = EN_OK;
        if (isUseMasterId) {
            return EN_OK;
        }
    }
    return ret;
}

/**
 * @brief SlogStartSyncFile: start thread to sync device log files
 * @param [in]logType: log type
 * @return EN_OK/EN_ERROR
 */
static int SlogStartSyncFile(uint32_t logicId, int32_t logType)
{
    if (MsnReportInitDevInfo(logicId) != EN_OK) {
        return EN_ERROR;
    }
    errno_t err = memset_s(g_slogdThread, MAX_DEV_NUM * sizeof(ThreadInfo), 0, MAX_DEV_NUM * sizeof(ThreadInfo));
    if (err != EOK) {
        SELF_LOG_ERROR("Memset for g_slogdThread failed, ret:%d", err);
        return EN_ERROR;
    }
    MsnSetThreadInfo(g_slogdThread, g_threadArgInfo, SlogSyncThread, logType);
    if (logicId == MAX_DEV_NUM) {
        return MsnReportCreateThreadForAllDevice(g_slogdThread, MAX_DEV_NUM, true);
    } else {
        return MsnReportCreateThreadSingleOs(logicId, g_slogdThread, true);
    }
}

static void MsnJoinThreads(ThreadInfo *threadInfo, uint32_t threadNum)
{
    int32_t ret = 0;
    for (uint32_t i = 0; i < threadNum; i++) {
        if (threadInfo[i].tid == 0) {
            continue;
        }
        ret = mmJoinTask(&threadInfo[i].tid);
        if (ret != EN_OK) {
            SELF_LOG_ERROR("pthread join failed, dev_id=%u, strerr=%s.", i, strerror(mmGetErrorCode()));
        }
        threadInfo[i].tid = 0;
    }
}

static void *ExportRegisterThread(void *arg)
{
    ONE_ACT_ERR_LOG(arg == NULL, return NULL, "arg is null.");
    ThreadArgInfo *threadArgInfo = (ThreadArgInfo *)arg;
    ArgInfo argInfo = {REPORT, 0, threadArgInfo->logicId, 0, -1, 0, 0, 0, 0, {0}};
    if (MsnConfig(&argInfo) == EN_OK) {
        MSNPU_WAR("Need reboot after export register.");
    }
    return NULL;
}

static void ExportDeviceRegister(uint32_t logicId)
{
    ThreadInfo threadInfo[MAX_DEV_NUM];
    ThreadArgInfo threadArgInfo[MAX_DEV_NUM];
    (void)memset_s(threadInfo, MAX_DEV_NUM * sizeof(ThreadInfo), 0, MAX_DEV_NUM * sizeof(ThreadInfo));
    (void)memset_s(threadArgInfo, MAX_DEV_NUM * sizeof(ThreadArgInfo), 0, MAX_DEV_NUM * sizeof(ThreadArgInfo));
    MsnSetThreadInfo(threadInfo, threadArgInfo, ExportRegisterThread, 0);
    int32_t ret = 0;
    if (logicId == MAX_DEV_NUM) {
        ret = MsnReportCreateThreadForAllDevice(threadInfo, MAX_DEV_NUM, false);
    } else {
        ret = MsnReportCreateThreadSingleOs(logicId, threadInfo, false);
    }
    if (ret != EN_OK) {
        return;
    }
    MsnJoinThreads(threadInfo, MAX_DEV_NUM);
}

/**
 * @brief BboxStartSyncFile: start to sync bbox files
 * @param [in]opt: bbox dump opt
 * @return EN_OK/EN_ERROR
 */
static int BboxStartSyncFile(uint32_t logicId, const struct BboxDumpOpt *opt, int32_t logType)
{
    if (logType == (int32_t)HISILOGS_LOG) {
        ExportDeviceRegister(logicId);
    }
    int32_t ret = 0;
    if (logicId == MAX_DEV_NUM) {
        ret = BboxStartDump(INVALID_DEV_ID, g_logPath, (int32_t)strlen(g_logPath), opt);
        ONE_ACT_ERR_LOG(ret != 0, return EN_ERROR, "start bbox dump failed.");
        // wait bbox sync thread stop
        BboxStopDump();
    } else {
        uint32_t logicIds[MAX_DEV_NUM] = {0};
        uint32_t logicNum = MsnGetAllLogicIdByMatserId(g_msnDeviceInfo.masterId, logicIds, MAX_DEV_NUM);
        for (uint32_t i = 0; i < logicNum; i++) {
            // the bbox interface does not support multiple device concurrency
            ret = BboxStartDump((int32_t)logicIds[i], g_logPath, (int32_t)strlen(g_logPath), opt);
            ONE_ACT_ERR_LOG(ret != BBOX_SUCCESS, return EN_ERROR, "Start bbox dump failed.");
            BboxStopDump();
        }
    }
    return EN_OK;
}

STATIC void GetHostDrvLog(void)
{
    char *fullPath = (char *)MsnMalloc(MMPA_MAX_PATH + 1);
    ONE_ACT_ERR_LOG(fullPath == NULL, return, "malloc failed for host drv, strerr=%s.", strerror(mmGetErrorCode()));
    int32_t ret = 0;
    do {
        ret = snprintf_s(fullPath, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s/host", g_logPath, SLOG_PATH);
        ONE_ACT_ERR_LOG(ret == -1, break, "copy path failed, strerr=%s.", strerror(mmGetErrorCode()));

        if (MsnMkdirMulti(fullPath) != EN_OK) {
            break;
        }

        int32_t len = (int32_t)strlen(fullPath);
        drvError_t err = halGetDeviceInfoByBuff(0, MODULE_TYPE_LOG, INFO_TYPE_HOST_KERN_LOG, (void *)fullPath, &len);
        if (err != DRV_ERROR_NONE) {
            if (err == DRV_ERROR_NOT_SUPPORT) {
                SELF_LOG_WARN("Current environment not support export host driver log.");
            } else if (err == DRV_ERROR_OUT_OF_MEMORY) {
                MSNPU_ERR(MEMORY_WARNING_MSG);
                SELF_LOG_ERROR("get host driver log failed due to insufficient memory, return: %d", (int32_t)err);
            } else {
                SELF_LOG_ERROR("get host driver log failed, return: %d", (int32_t)err);
            }
        }
    } while (0);
    free(fullPath);
}

/**
 * @brief MsnReportStop: wait all thread stop
 * @return void
 */
static void MsnReportStop(void)
{
    int ret;
    // wait slog sync thread stop
    unsigned int i;
    for (i = 0; i < MAX_DEV_NUM; i++) {
        if (g_slogdThread[i].tid == 0) {
            continue;
        }
        ret = mmJoinTask(&g_slogdThread[i].tid);
        if (ret != EN_OK) {
            SELF_LOG_ERROR("pthread join failed, dev_id=%u, strerr=%s.", i, strerror(mmGetErrorCode()));
        }
        g_slogdThread[i].tid = 0;
    }
}

#ifdef UB_SUPPORT
STATIC int32_t MsnWriteUBInfoData(const char *writeFile, const char *buffer, uint32_t bufferSize)
{
    uint32_t fileMode = 0600;
    int32_t fd = ToolOpenWithMode(writeFile, (uint32_t)O_CREAT | (uint32_t)O_WRONLY | (uint32_t)O_APPEND, fileMode);
    if (fd < 0) {
        SELF_LOG_ERROR("open file failed with mode, file: %s, strerr: %s.", writeFile, strerror(ToolGetErrorCode()));
        return EN_ERROR;
    }
    int32_t ret = ToolWrite(fd, buffer, bufferSize);
    if ((ret < 0) || ((uint32_t)ret != bufferSize)) {
        LOG_CLOSE_FD(fd);
        SELF_LOG_ERROR("write log to file:%s failed, data length: %u bytes, strerr: %s.", writeFile, bufferSize,
                       strerror(ToolGetErrorCode()));
        return EN_ERROR;
    }

    ret = ToolFChownPath(fd);
    if (ret != EN_OK) {
        SELF_LOG_ERROR("change file:%s owner failed, ret: %d, strerr: %s.", writeFile, ret,
                       strerror(ToolGetErrorCode()));
    }
    LOG_CLOSE_FD(fd);
    return EN_OK;
}

STATIC const char *GetUBInfoFileName(unsigned int ubDataType)
{
    const char *ub_file_name[] = {"port_status.bin",
                                  "port_perf_test.bin",
                                  "dlphy_get_config.bin",
                                  "ubnl_dfx_statistic.bin",
                                  "ubnl_dfx_ssu_schedule.bin",
                                  "ubnl_dfx_config_item.bin",
                                  "ubmem_daw.bin",
                                  "ubtpl_acl_src.bin",
                                  "sl_to_vl.bin",
                                  "invalid.bin"};

    if (ubDataType >= UB_INFO_MAX) {
        SELF_LOG_WARN("Invalid ub datatype, dataType=%u.", ubDataType);
        return "invalid.bin";
    }
    return ub_file_name[ubDataType];
}

STATIC void MsnInitUbnlDfxInput(struct DsmiUbDfxInput *ubDfxInput, uint32_t minor)
{
    ubDfxInput->major = UB_MAMI_CMD_UBNL;
    ubDfxInput->minor = minor;
    ubDfxInput->buf_size = sizeof(struct UbMamiStatsKey);
    struct UbMamiStatsKey *statsKey = (struct UbMamiStatsKey *)(ubDfxInput->buf);
    statsKey->idType = UB_INFO_UBNL_DFX_PARAM_ID_TYPE;
    statsKey->planeId = UB_INFO_UBNL_DFX_PARAM_PLANE_ID;
}

STATIC void MsnInitUbTplAclInput(struct DsmiUbDfxInput *ubDfxInput)
{
    ubDfxInput->major = UB_MAMI_CMD_UBTPL;
    ubDfxInput->minor = UB_INFO_UBTPL_ACL_PARAM_MINOR;
    ubDfxInput->buf_size = sizeof(struct UbMamiTplAclQry);
    struct UbMamiTplAclQry *aclQuery = (struct UbMamiTplAclQry *)(ubDfxInput->buf);
    aclQuery->fieldMask = UB_INFO_UBTPL_ACL_PARMA_MASK;
    aclQuery->planeId = 0;
}

STATIC void MsnInitSlToVlInput(struct DsmiUbDfxInput *ubDfxInput, uint32_t *bufferSizePtr)
{
    ubDfxInput->major = UB_MAMI_CMD_UBQOS;
    ubDfxInput->minor = UB_INFO_SL_TO_VL_PARAM_MINOR;
    ubDfxInput->buf_size = sizeof(struct UbQosSl2VlCmd);
    struct UbQosSl2VlCmd *slToVlCmd = (struct UbQosSl2VlCmd *)(ubDfxInput->buf);
    slToVlCmd->plane_id = 0;
    slToVlCmd->num = UB_INFO_UBQOS_MAX_SL_NUM;
    *bufferSizePtr = sizeof(struct UbQosSl2VlCmd);
}

STATIC int32_t getDrvUBInfo(uint32_t devId, uint32_t ubDataType, void *buffer, uint32_t *bufferSizePtr)
{
    (void)memset_s(buffer, MAX_UB_INFO_SIZE, 0, MAX_UB_INFO_SIZE);
    *bufferSizePtr = MAX_UB_INFO_SIZE;
    struct DsmiUbDfxInput *ubDfxInput = (struct DsmiUbDfxInput *)buffer;
    switch (ubDataType) {
        case UB_INFO_UBNL_DFX_STATISTIC:
            MsnInitUbnlDfxInput(ubDfxInput, UB_INFO_UBNL_DFX_STATISTIC_PARAM_MINOR);
            break;
        case UB_INFO_UBNL_DFX_SSU_SCHEDULE:
            MsnInitUbnlDfxInput(ubDfxInput, UB_INFO_UBNL_DFX_SSU_SCHEDULE_PARAM_MINOR);
            break;
        case UB_INFO_UBNL_DFX_CONFIG_ITEM:
            MsnInitUbnlDfxInput(ubDfxInput, UB_INFO_UBNL_DFX_CONFIG_ITEM_PARAM_MINOR);
            break;
        case UB_INFO_UBMEM_DAW:
            ubDfxInput->major = UB_MAMI_CMD_UBMEM;
            ubDfxInput->minor = UB_INFO_UBMEM_DAW_PARAM_MINOR;
            ubDfxInput->buf_size = 0;
            break;
        case UB_INFO_UBTPL_ACL:
            MsnInitUbTplAclInput(ubDfxInput);
            break;
        case UB_INFO_SL_TO_VL:
            MsnInitSlToVlInput(ubDfxInput, bufferSizePtr);
            break;
        default:
            SELF_LOG_WARN("UNSUPPORTED UB INFO TYPE:%u", ubDataType);
            break;
    }
    drvError_t err = dsmi_get_device_info(devId, DSMI_MAIN_CMD_UB, UB_SUB_CMD_UB_DFX_INFO, buffer, bufferSizePtr);
    return err;
}

STATIC int32_t MsnSyncDeviceUBInfo(uint32_t devId, uint32_t ubDataType, const char *fullPath, void *buffer,
                                   uint32_t *bufferSizePtr)
{
    int ret = 0;
    ret = getDrvUBInfo(devId, ubDataType, buffer, bufferSizePtr);
    ONE_ACT_WARN_LOG(ret != DRV_ERROR_NONE, return EN_ERROR,
                     "Can not get device ub info, "
                     "device id:%u ub info:%u, err=%d.",
                     devId, ubDataType, ret);
    const char *fileName = GetUBInfoFileName(ubDataType);
    char fullFileName[MMPA_MAX_PATH + 1] = {0};
    ret = snprintf_s(fullFileName, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s", fullPath, fileName);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "copy full file name failed, device id:%u ub info:%u", devId,
                    ubDataType);
    ret = MsnWriteUBInfoData(fullFileName, buffer, *bufferSizePtr);
    SELF_LOG_INFO("Export logs to dir succeed: %s/%s", fullPath, fileName);
    return ret;
}

STATIC int32_t MsnCreateUBInfoRootDir(uint32_t logicId, const char *hostPath, char *fullPath)
{
    int32_t ret = snprintf_s(fullPath, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s/dev-os-%u/%s", g_logPath, hostPath,
                             logicId, UB_INFO_MAMI_DIR);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "copy path failed");
    ret = MsnMkdirMulti(fullPath);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "mkdir path %s failed, strerr: %s, ret: %d.", fullPath,
                    strerror(ToolGetErrorCode()), ret);
    return EN_OK;
}

STATIC int32_t MsnCreateUBSymlink(const char *targetDir, uint32_t logicId, const char *hostPath)
{
    char linkDir[MMPA_MAX_PATH + 1] = {0};
    int32_t ret = snprintf_s(linkDir, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s/device-%u/%s", g_logPath,
                             UB_INFO_HISI_LOGS_DIR, logicId, UB_INFO_DIR);
    ONE_ACT_WARN_LOG(ret == -1, return EN_OK, "copy symlink dir path failed");
    if (ToolAccess(linkDir) == SYS_OK) {
        return EN_OK;
    }
    // mkdir <g_logPath>/hisi_logs/device-<logicId>
    char linkParent[MMPA_MAX_PATH + 1] = {0};
    ret = snprintf_s(linkParent, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s/%s/device-%u", g_logPath, UB_INFO_HISI_LOGS_DIR,
                     logicId);
    ONE_ACT_WARN_LOG(ret == -1, return EN_OK, "copy symlink parent path failed");
    ret = MsnMkdirMulti(linkParent);
    ONE_ACT_WARN_LOG(ret != EN_OK, return EN_OK, "mkdir %s failed", linkParent);

    // calculate relative path from linkDir's parent to targetDir
    // linkDir:    <g_logPath>/hisi_logs/device-<logicId>/ub
    // targetDir:  <g_logPath>/<hostPath>/dev-os-<logicId>/mami
    // relative:   ../../<hostPath>/dev-os-<logicId>/mami
    char relTarget[MMPA_MAX_PATH + 1] = {0};
    if (hostPath != NULL) {
        ret = snprintf_s(relTarget, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "../../%s/dev-os-%u/%s", hostPath, logicId,
                         UB_INFO_MAMI_DIR);
    }
    if ((hostPath == NULL) || (ret == -1)) {
        SELF_LOG_WARN("build relative symlink path failed, fallback to absolute");
        ret = snprintf_s(relTarget, MMPA_MAX_PATH + 1, MMPA_MAX_PATH, "%s", targetDir);
        if (ret == -1) {
            SELF_LOG_ERROR("fallback to absolute symlink path also failed");
            return EN_OK;
        }
    }
    if (symlink(relTarget, linkDir) != 0) {
        SELF_LOG_WARN("symlink %s -> %s failed, err=%d", linkDir, relTarget, errno);
    } else {
        SELF_LOG_INFO("symlink %s -> %s created", linkDir, relTarget);
    }
    return EN_OK;
}

STATIC int32_t MsnSyncDeviceAllUBInfo(uint32_t logicId)
{
    uint32_t ubTypeArr[] = {UB_INFO_UBNL_DFX_STATISTIC,
                            UB_INFO_UBNL_DFX_SSU_SCHEDULE,
                            UB_INFO_UBNL_DFX_CONFIG_ITEM,
                            UB_INFO_UBMEM_DAW,
                            UB_INFO_UBTPL_ACL,
                            UB_INFO_SL_TO_VL};
    uint32_t ubTypeArrSize = sizeof(ubTypeArr) / sizeof(ubTypeArr[0]);
    uint32_t bufferSize = MAX_UB_INFO_SIZE;
    void *buffer = MsnMalloc(bufferSize);
    ONE_ACT_ERR_LOG(buffer == NULL, return EN_ERROR, "malloc sync ub info failed, size=%d", MAX_UB_INFO_SIZE);
    const char *hostPath = GetScriptDumpHostPath("ub");
    ONE_ACT_ERR_LOG(hostPath == NULL, XFREE(buffer); return EN_ERROR, "get ub host path failed");
    char fullPath[MMPA_MAX_PATH + 1] = {0};
    int32_t ret = MsnCreateUBInfoRootDir(logicId, hostPath, fullPath);
    ONE_ACT_ERR_LOG(ret == -1, XFREE(buffer); return EN_ERROR, "create ub info dir failed");

    for (uint32_t i = 0; i < ubTypeArrSize; i++) {
        uint32_t ubDataType = ubTypeArr[i];
        ret = MsnSyncDeviceUBInfo(logicId, ubDataType, fullPath, buffer, &bufferSize);
        if (ret != EN_OK) {
            SELF_LOG_WARN("MsnSyncDeviceUBInfo fail, devId=%u, dataType:%u.", logicId, ubDataType);
        }
        (void)ToolSleep(UB_INFO_WAIT_TIME);
    }
    XFREE(buffer);
    MsnCreateUBSymlink(fullPath, logicId, hostPath);
    return EN_OK;
}
#endif

/**
 * @brief MsnSyncDeviceLog: start thread to receive files from device
 * @param [in]opt: bbox dump opt
 * @param [in]logType: log type
 * @return EN_OK/EN_ERROR
 */
static int32_t MsnSyncDeviceLog(uint32_t logicId, const struct BboxDumpOpt *opt, int logType)
{
    int32_t ret = SlogStartSyncFile(logicId, logType);
    if (ret != EN_OK) {
        if (MsnIsDockerEnv()) {
            SELF_LOG_ERROR("export files failed, can not export files in docker.");
        } else {
            SELF_LOG_ERROR("export files failed, please check error message.");
        }
    }

    int32_t err = 0;
    if ((logType == (int32_t)ALL_LOG) || (logType == (int32_t)HISILOGS_LOG) || (logType == (int32_t)VMCORE_FILE)) {
        // check disk free space
        mmDiskSize diskSize;
        (void)memset_s(&diskSize, sizeof(diskSize), 0, sizeof(diskSize));
        err = mmGetDiskFreeSpace(g_logPath, &diskSize);
        if (err == EN_OK && diskSize.freeSize >= DEFAULT_BBOX_SIZE) {
            err = BboxStartSyncFile(logicId, opt, logType);
            if (err != EN_OK) {
                SELF_LOG_ERROR("start to sync bbox dump failed.");
                goto stop_report;
            }
        } else {
            unsigned long long freeSize = diskSize.freeSize / 1024 / 1024;
            SELF_LOG_ERROR("No space left on device.");
            SELF_LOG_ERROR("Available space (%lluM) less than 10M. Bbox data will not be collected.", freeSize);
        }
    }
#ifdef UB_SUPPORT
    if (logType == (int32_t)UB_LOG) {
        for (uint32_t i = 0; i < g_msnDeviceInfo.devNum; i++) {
            // if -d/--device is specified, only sync ub log of deviceID param
            if ((logicId != MAX_DEV_NUM) && (i != logicId)) {
                continue;
            }
            err = MsnSyncDeviceAllUBInfo(i);
            if (err != EN_OK) {
                SELF_LOG_WARN("start to sync ub log failed.");
            }
        }
    }
#endif
stop_report:
    MsnReportStop();
    return ((ret == EN_OK) && (err == EN_OK)) ? EN_OK : EN_ERROR;
}

/**
 * @brief GetLocalTimeForPath: get current timestamp
 * @param [in]bufLen: time buffer size
 * @param [in/out]timeBuffer: timestamp buffer
 * @return EN_OK/EN_ERROR
 */
static int GetLocalTimeForPath(unsigned int bufLen, char *timeBuffer)
{
    mmTimeval currentTimeval = {0};
    struct tm timeInfo = {0};

    int32_t ret = mmGetTimeOfDay(&currentTimeval, NULL);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "get times failed.");

    const time_t sec = currentTimeval.tv_sec;
    ret = mmLocalTimeR(&sec, &timeInfo);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "get timestamp failed.");

    ret = snprintf_s(timeBuffer, bufLen, bufLen - 1, "%04d-%02d-%02d-%02d-%02d-%02d", timeInfo.tm_year, timeInfo.tm_mon,
                     timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "copy time buffer failed, strerr=%s.", strerror(mmGetErrorCode()));

    return EN_OK;
}

/**
 * @brief CreateLogRootPath: mkdir slog root path for device log syncing
 * @return EN_OK/EN_ERROR
 */
STATIC int32_t CreateLogRootPath(void)
{
    char timer[TIME_SIZE + 1] = {0};
    if (GetLocalTimeForPath(TIME_SIZE, timer) != EN_OK) {
        return EN_ERROR;
    }
    char *currentPath = (char *)MsnMalloc(MMPA_MAX_PATH + 1);
    ONE_ACT_ERR_LOG(currentPath == NULL, return EN_ERROR, "malloc failed, strerr=%s.", strerror(mmGetErrorCode()));

    bool flag = false;
    do {
        if (mmGetCwd(currentPath, MMPA_MAX_PATH) != EN_OK) {
            SELF_LOG_ERROR("get current path failed, strerr=%s.", strerror(mmGetErrorCode()));
            break;
        }

        if (sprintf_s(g_logPath, MMPA_MAX_PATH + 1, "%s/%s", currentPath, timer) == -1) {
            SELF_LOG_ERROR("sprintf_s for root path failed, strerr=%s.", strerror(mmGetErrorCode()));
            break;
        }
        if (MsnMkdirMulti(g_logPath) != EN_OK) {
            SELF_LOG_ERROR("mkdir failed, strerr=%s.", strerror(mmGetErrorCode()));
            break;
        }

        MSNPU_FPRINTF("Start exporting logs and files to path: %s\n", g_logPath);
        flag = true;
    } while (0);

    free(currentPath);
    return flag == true ? EN_OK : EN_ERROR;
}

STATIC int32_t SetLogRootPath(const char *outputPath)
{
    char *currentPath = (char *)MsnMalloc(MMPA_MAX_PATH + 1);
    ONE_ACT_ERR_LOG(currentPath == NULL, return EN_ERROR, "malloc failed, strerr=%s.", strerror(mmGetErrorCode()));

    bool flag = false;
    do {
        if (mmGetCwd(currentPath, MMPA_MAX_PATH) != EN_OK) {
            SELF_LOG_ERROR("get current path failed, strerr=%s.", strerror(mmGetErrorCode()));
            break;
        }
        if (strlen(outputPath) >= MMPA_MAX_PATH) {
            SELF_LOG_ERROR("Output path too long.");
            break;
        }

        if (sprintf_s(g_logPath, MMPA_MAX_PATH + 1, "%s/%s", currentPath, outputPath) == -1) {
            SELF_LOG_ERROR("strcpy_s failed for output path.");
            break;
        }
        if (mmAccess(g_logPath) != EN_OK) {
            SELF_LOG_ERROR("%s is not access.", g_logPath);
            break;
        }

        MSNPU_FPRINTF("Start exporting logs to specified path: %s\n", g_logPath);
        flag = true;
    } while (0);

    free(currentPath);
    return flag == true ? EN_OK : EN_ERROR;
}

STATIC bool IsHaveExecPermission(void)
{
#if (OS_TYPE == LINUX)
    return geteuid() == 0;
#else
    return false;
#endif
}

/**
 * @brief CheckDiskSpaceWarning: check disk available space at path, warn if below 10MB
 * @param [in]path: filesystem path to check
 * @return void
 */
static void CheckDiskSpaceWarning(const char *path)
{
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) {
        SELF_LOG_WARN("Failed to check disk space for path: %s, strerr: %s.", path, strerror(errno));
        return;
    }
    uint64_t freeBytes = (uint64_t)stat.f_bavail * (uint64_t)stat.f_bsize;
    if (freeBytes < DISK_SPACE_WARNING_THRESHOLD) {
        MSNPU_WAR("Insufficient disk space (available: %llu MB), only partial logs may be exported.",
                  (unsigned long long)(freeBytes / (1024ULL * 1024ULL)));
    }
}

/**
 * @brief export device log
 * @param [in]opt: bboxdump info
 * @param [in]logType: log type
 * @return EN_OK: succ EN_ERROR: failed
 */
STATIC int32_t SyncDeviceLog(uint32_t logicId, const struct BboxDumpOpt *opt, int32_t logType)
{
    if (!IsHaveExecPermission()) {
        SELF_LOG_ERROR("Not have permission to export files.");
        return EN_ERROR;
    }

    if (CreateLogRootPath() != EN_OK) {
        return EN_ERROR;
    }

    CheckDiskSpaceWarning(g_logPath);

    if (logType == (int32_t)ALL_LOG) {
        GetHostDrvLog();
    }

    // run thread to connect device for log
    int32_t ret = MsnSyncDeviceLog(logicId, opt, logType);
    if (ret == EN_OK) {
        MSNPU_FPRINTF("Export finished.\n");
    }
    return ret;
}

static int32_t MsnReportCreateLongLink(uint32_t devId, uint32_t timeout, void **handle, int32_t serverType,
                                       ComponentType compType)
{
    AdxCommHandle adxHandle = AdxCreateCommHandle(serverType, (int32_t)devId, compType);
    int32_t ret = AdxIsCommHandleValid(adxHandle);
    if (ret != IDE_DAEMON_OK) {
        SELF_LOG_ERROR("Adx create handle is invalid, devId=%u, ret=%d.", devId, ret);
        return EN_ERROR;
    }

    // receive ack
    char *buffer = (char *)MsnMalloc(ACK_LEN);
    ONE_ACT_ERR_LOG(buffer == NULL, return EN_ERROR, "Malloc for ack buffer failed, devId=%u, strerr=%s.", devId,
                    strerror(ToolGetErrorCode()));
    uint32_t bufLen = ACK_LEN;
    do {
        ret = AdxRecvMsg(adxHandle, &buffer, &bufLen, timeout);
        if ((ret != IDE_DAEMON_OK) || (bufLen == 0U)) {
            SELF_LOG_ERROR("AdxRecvMsg failed, ret=%d, bufLen=%u bytes.", ret, bufLen);
            ret = EN_ERROR;
            break;
        }
        if (strcmp(buffer, HDC_END_MSG) == 0) {
            ret = EN_OK;
            break;
        }
        ret = EN_ERROR;
        if (strcmp(buffer, CONNECT_OCCUPIED_MESSAGE) == 0) {
            MSNPU_FPRINTF("Another msnpureport process is exporting logs of device os %u.\n", devId);
        } else if (strcmp(buffer, CONTAINER_NO_SUPPORT_MESSAGE) == 0) {
            MSNPU_FPRINTF("Not support export device log in container environment.\n");
        } else {
            SELF_LOG_ERROR("Get start messages failed, devId=%u, serverType=%d.", devId, serverType);
        }
    } while (0);

    XFREE(buffer);
    if (ret != EN_OK) {
        AdxDestroyCommHandle(adxHandle);
        return EN_ERROR;
    }

    *handle = (void *)adxHandle;
    return EN_OK;
}

static int32_t MsnReportServerLongLink(uint32_t devId, uint32_t timeout, void **handle, int32_t serverType,
                                       ComponentType compType)
{
    AdxCommHandle adxHandle = AdxCreateCommHandle(serverType, (int32_t)devId, compType);
    int32_t ret = AdxIsCommHandleValid(adxHandle);
    if (ret != IDE_DAEMON_OK) {
        SELF_LOG_ERROR("Adx create handle is invalid, devId=%u, ret=%d.", devId, ret);
        return EN_ERROR;
    }

    // receive ack
    MsnServerResultInfo *buffer = (MsnServerResultInfo *)MsnMalloc(sizeof(MsnServerResultInfo));
    ONE_ACT_ERR_LOG(buffer == NULL, return EN_ERROR, "Malloc for ack buffer failed, devId=%u, strerr=%s.", devId,
                    strerror(ToolGetErrorCode()));
    uint32_t bufLen = sizeof(MsnServerResultInfo);
    do {
        ret = AdxRecvMsg(adxHandle, (char **)&buffer, &bufLen, timeout);
        if ((ret != IDE_DAEMON_OK) || (bufLen == 0U)) {
            SELF_LOG_ERROR("AdxRecvMsg failed, ret=%d, length=%u bytes.", ret, bufLen);
            ret = EN_ERROR;
            break;
        }
        if (buffer->retCode != 0) {
            SELF_LOG_ERROR("Get error message, devId=%u, ret=%d, errmsg=%s.", devId, buffer->retCode, buffer->retMsg);
            ret = EN_ERROR;
            break;
        }
        ret = EN_OK;
    } while (0);

    XFREE(buffer);
    if (ret != EN_OK) {
        AdxDestroyCommHandle(adxHandle);
        return EN_ERROR;
    }

    *handle = (void *)adxHandle;
    return EN_OK;
}

static void MsnReportReleaseHandle(void **handle)
{
    AdxDestroyCommHandle((AdxCommHandle)(*handle));
    *handle = NULL;
}

static void MsnReportSaveSlog(uint32_t devId, uint32_t timeout, void **handle, ThreadArgInfo *threadArgInfo)
{
    void *buffer = MsnMalloc(RECV_BUFF_SIZE);
    ONE_ACT_ERR_LOG(buffer == NULL, return, "Malloc slog receive buffer failed, devId=%u, strerr=%s", devId,
                    strerror(ToolGetErrorCode()));

    uint32_t bufLen = 0;
    int32_t retryTime = 0;
    int32_t ret = 0;
    while ((retryTime < MAX_RETRY_TIME) && !threadArgInfo->isThreadExit) {
        if (AdxIsCommHandleValid(*handle) != IDE_DAEMON_OK) {
            retryTime++;
#ifdef HDC_NEW_CHANNEL
            // Using the new channel to trans data, For compatibility
            ret = MsnReportCreateLongLink(devId, timeout, handle, HDC_SERVICE_TYPE_PROFILING, COMPONENT_SYS_REPORT);
#else
            ret = MsnReportCreateLongLink(devId, timeout, handle, HDC_SERVICE_TYPE_LOG, COMPONENT_SYS_REPORT);
#endif
            if (ret == EN_OK) {
                retryTime = 0;
                continue;
            }
            sleep(SLOGD_WAIT_TIME);
            continue;
        }
        bufLen = RECV_BUFF_SIZE;
        ret = AdxRecvMsg((AdxCommHandle)(*handle), (char **)&buffer, &bufLen, timeout);
        if ((ret != IDE_DAEMON_OK) || (bufLen == 0U)) {
            if (ret == IDE_DAEMON_SOCK_CLOSE) { // 对端不存在, slogd 进程退出, 等待9s slogd重新被拉起
                SELF_LOG_WARN("Can not receive data, slogd process exit, try to connect again.");
                MsnReportReleaseHandle(handle);
                sleep(SLOGD_WAIT_TIME);
                continue;
            }
            usleep(RETRY_WAIT_TIME);
            continue;
        }

        const int32_t ackRet = AdxSendMsgTimeout((AdxCommHandle)(*handle), HDC_END_MSG, strlen(HDC_END_MSG),
                                                 MSN_ACK_SEND_TIMEOUT_MS);
        (void)MsnFileMgrWriteDeviceSlog(buffer, bufLen, (uint32_t)threadArgInfo->devOsId);
        (void)memset_s(buffer, bufLen, 0, bufLen);
        if (ackRet != EN_OK) {
            // hdc send timeout means the peer queue is full or abnormal; release the session so the
            // next loop iteration recreates the long link, which rebuilds the hdc channel.
            SELF_LOG_WARN("Can not send response message, release session and recreate. (devId=%u)", devId);
            MsnReportReleaseHandle(handle);
        }
    }
    XFREE(buffer);
}

STATIC void *MsnReportRecvSlogd(void *args)
{
    ONE_ACT_ERR_LOG(args == NULL, return NULL, "args of MsnReportRecvSlogd is null, thead exit.");
    ThreadArgInfo *threadArgInfo = (ThreadArgInfo *)args;

    char threadName[TMP_STR_SIZE] = {0};
    int32_t ret = 0;
    ret = sprintf_s(threadName, TMP_STR_SIZE, "MsnRecvSlog%ld", threadArgInfo->devOsId);
    ONE_ACT_ERR_LOG(ret == -1, return NULL, "Call sprintf_s failed for slogd thread name %ld.", threadArgInfo->devOsId);
    ret = ToolSetThreadName(threadName);
    NO_ACT_WARN_LOG(ret != SYS_OK, "can not set thread name %s.", threadName);

    uint32_t logicId = threadArgInfo->logicId;
    void *handle = NULL;
    uint32_t timeout = 1000;
#ifdef HDC_NEW_CHANNEL
    // Using the new channel to trans data, For compatibility
    ret = MsnReportCreateLongLink(logicId, timeout, &handle, HDC_SERVICE_TYPE_PROFILING, COMPONENT_SYS_REPORT);
#else
    ret = MsnReportCreateLongLink(logicId, timeout, &handle, HDC_SERVICE_TYPE_LOG, COMPONENT_SYS_REPORT);
#endif
    if (ret != EN_OK) {
        threadArgInfo->isThreadExit = true;
        SELF_LOG_ERROR("Create long link to slogd failed, dev os %ld.", threadArgInfo->devOsId);
        return NULL;
    }

    SELF_LOG_INFO("start export log, dev os %ld", threadArgInfo->devOsId);
    MsnReportSaveSlog(logicId, timeout, &handle, threadArgInfo);
    // destory connect
    MsnReportReleaseHandle(&handle);
    threadArgInfo->isThreadExit = true;
    SELF_LOG_WARN("Receive slogd thread exited, dev os %ld, logic id %u.", threadArgInfo->devOsId, logicId);
    return NULL;
}

static int32_t MsnReportConnectToLogDaemon(void **handle, uint32_t timeout, int32_t devMasterId, uint32_t logicId)
{
    int32_t ret = 0;
    char threadName[TMP_STR_SIZE] = {0};
    ret = sprintf_s(threadName, TMP_STR_SIZE, "MsnRecvFile%d", devMasterId);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s failed for log daemon thread name %d.", devMasterId);
    ret = ToolSetThreadName(threadName);
    NO_ACT_WARN_LOG(ret != SYS_OK, "Can not set thread name %s.", threadName);

    ret = MsnReportServerLongLink(logicId, timeout, handle, HDC_SERVICE_TYPE_IDE_FILE_TRANS, COMPONENT_FILE_REPORT);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Create long link to log deamon failed, devId=%u.", logicId);

    // send device os id
    char devOsId[ACK_LEN] = {0};
    ret = sprintf_s(devOsId, ACK_LEN, "dev-os-%d", devMasterId);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s failed for devOsId.");
    ret = AdxSendMsgTimeout(*handle, devOsId, (uint32_t)strlen(devOsId), MSN_ACK_SEND_TIMEOUT_MS);
    if (ret != IDE_DAEMON_OK) {
        SELF_LOG_ERROR("Send dev os id failed.");
        MsnReportReleaseHandle(handle);
        return EN_ERROR;
    }
    return EN_OK;
}

/**
 * @brief MsnReportBboxHBL: export bbox when device heart beat lost
 * @param [in]threadArgs: thread args
 */
static void MsnReportBboxHBL(uint32_t logicId, ThreadArgInfo *threadArgs)
{
    struct BboxDumpOpt opt = {false, false, false, true, MsnGetLogPrintMode(), MsnPrintGetLogLevel()};
    int32_t ret = 0;
    if (!g_msnDeviceInfo.isSMPEnv) { // not SMP env
        ret = BboxStartDump((int32_t)logicId, g_logPath, (int32_t)strlen(g_logPath), &opt);
        ONE_ACT_ERR_LOG(ret != BBOX_SUCCESS, return, "Start bbox dump device %u failed.", logicId);
        BboxStopDump();
        return;
    }

    // SMP env
    sleep(WAIT_ALL_DEVICE_UPDATE_TIME); // wait 10s for all device update status
    for (uint32_t i = 0; i < g_msnDeviceInfo.devNum; i++) {
        if (g_msnDeviceInfo.masterIds[g_msnDeviceInfo.devIds[i]] == threadArgs->devOsId) {
            ret = BboxStartDump((int32_t)g_msnDeviceInfo.devIds[i], g_logPath, (int32_t)strlen(g_logPath), &opt);
            ONE_ACT_ERR_LOG(ret != BBOX_SUCCESS, continue, "Start bbox dump device %u failed.",
                            g_msnDeviceInfo.devIds[i]);
            BboxStopDump();
        }
    }
}

STATIC bool MsnReportCheckDeviceAlive(uint32_t logicId)
{
    drvStatus_t deviceStatus = DRV_STATUS_INITING;
    drvError_t err = drvDeviceStatus(logicId, &deviceStatus);
    if (err != DRV_ERROR_NONE) {
        SELF_LOG_ERROR("Get device %u status failed.", logicId);
        return false;
    }

    if (deviceStatus == DRV_STATUS_COMMUNICATION_LOST) {
        SELF_LOG_WARN("Device %u heart beat lost.", logicId);
        return false;
    }
    return true;
}

STATIC int32_t MsnReportFaultEventGetDeviceFileWithRetry(uint16_t phyId, const char *absolutePath, const char *label,
                                                         uint32_t timeout)
{
    int32_t ret = EN_ERROR;
    for (uint32_t i = 0; i < FAULT_EVENT_RETRY_TIMES; i++) {
        if (i > 0) {
            SELF_LOG_WARN("Retry to get %s after waiting for server process.", label);
            sleep(SLOGD_WAIT_TIME);
        }
        ret = AdxGetDeviceFileTimeout(phyId, absolutePath, label, timeout);
        if (ret == IDE_DAEMON_OK) {
            break;
        }
    }
    return ret;
}

STATIC int32_t MsnReportBuildFaultEventPath(const struct dsmi_event *event, uint32_t devId, uint32_t phyId,
                                            char *relativePath)
{
    int32_t ret = 0;
#ifdef USE_LOGIC_ID
    (void)phyId;
    ret = sprintf_s(relativePath, MMPA_MAX_PATH, "%s/device-%u/fault_event_0x%X_", SYSTEM_INFO_PATH, devId,
                    event->event_t.dms_event.event_id);
#else
    (void)devId;
    ret = sprintf_s(relativePath, MMPA_MAX_PATH, "%s/device-%u/fault_event_0x%X_", SYSTEM_INFO_PATH, phyId,
                    event->event_t.dms_event.event_id);
#endif
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s failed.");
    ret = MsnGetTimeStr(relativePath + (int32_t)strlen(relativePath), MMPA_MAX_PATH - (int32_t)strlen(relativePath));
    ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_ERROR, "Get time stamp failed.");
    (void)MsnFileMgrSaveFile(relativePath, MMPA_MAX_PATH, devId);
    return EN_OK;
}

STATIC void MsnReportGetFaultSystemInfoFiles(uint16_t phyId, const char *absolutePath)
{
    size_t itemNum = sizeof(MSNPUREPORT_FILE_DUMP_INFO) / sizeof(MsnpureportFileDumpTable);
    for (uint32_t i = 0; i < (uint32_t)itemNum; i++) {
        if (strcmp(MSNPUREPORT_FILE_DUMP_INFO[i].hostFilePath, SYSTEM_INFO_PATH) != 0) {
            continue;
        }
        int32_t ret = MsnReportFaultEventGetDeviceFileWithRetry(
            phyId, absolutePath, MSNPUREPORT_FILE_DUMP_INFO[i].label, MSNPUREPORT_FILE_DUMP_INFO[i].timeout);
        NO_ACT_WARN_LOG(ret != IDE_DAEMON_OK, "Get %s failed.", MSNPUREPORT_FILE_DUMP_INFO[i].label);
    }
}

STATIC int32_t MsnReportGetFaultEventFiles(uint16_t phyId, const char *relativePath)
{
    char absolutePath[MMPA_MAX_PATH] = {0};
    int32_t ret = sprintf_s(absolutePath, MMPA_MAX_PATH, "%s/%s", g_logPath, relativePath);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s failed.");
    MsnReportGetFaultSystemInfoFiles(phyId, absolutePath);

    (void)memset_s(absolutePath, MMPA_MAX_PATH, 0, MMPA_MAX_PATH);
    ret = sprintf_s(absolutePath, MMPA_MAX_PATH, "%s/%s/%s", g_logPath, relativePath, MESSAGE_PATH);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s for message failed.");
    ret = MsnReportFaultEventGetDeviceFileWithRetry(phyId, absolutePath, MESSAGE_PATH, DEFAULT_TIMEOUT);
    NO_ACT_WARN_LOG(ret != IDE_DAEMON_OK, "Get message failed.");

    (void)memset_s(absolutePath, MMPA_MAX_PATH, 0, MMPA_MAX_PATH);
    ret = sprintf_s(absolutePath, MMPA_MAX_PATH, "%s/%s/%s", g_logPath, relativePath, EVENT_PATH);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s for event_sched failed.");
    ret = MsnReportFaultEventGetDeviceFileWithRetry(phyId, absolutePath, EVENT_PATH, DEFAULT_TIMEOUT);
    NO_ACT_WARN_LOG(ret != IDE_DAEMON_OK, "Get event_sched failed.");
    return EN_OK;
}

STATIC void *MsnReportRecvLogDaemon(void *args)
{
    ONE_ACT_ERR_LOG(args == NULL, return NULL, "args of MsnReportRecvLogDaemon is null, thead exit.");
    ThreadArgInfo *threadArgs = (ThreadArgInfo *)args;
    uint32_t logicId = threadArgs->logicId;

    void *handle = NULL;
    int32_t ret = MsnReportConnectToLogDaemon(&handle, PERMANENT_TIMEOUT, (int32_t)threadArgs->devOsId, logicId);
    TWO_ACT_ERR_LOG(ret != EN_OK, threadArgs->isThreadExit = true, return NULL, "Connect to log daemon failed");

    char filename[MMPA_MAX_PATH] = {0};
    int32_t retryTime = 0;
    int32_t timeoutCount = 0;
    while ((retryTime < MAX_RETRY_TIME) && !threadArgs->isThreadExit) {
        if (AdxIsCommHandleValid(handle) != IDE_DAEMON_OK) {
            retryTime++;
            ret = MsnReportConnectToLogDaemon(&handle, PERMANENT_TIMEOUT, (int32_t)threadArgs->devOsId, logicId);
            if (ret == EN_OK) {
                retryTime = 0;
                continue;
            }
            sleep(SLOGD_WAIT_TIME);
            continue;
        }
        (void)memset_s(filename, MMPA_MAX_PATH, 0, MMPA_MAX_PATH);
        ret = AdxRecvDevFileTimeout(handle, g_logPath, PERMANENT_TIMEOUT, filename, MMPA_MAX_PATH);
        if (ret == IDE_DAEMON_OK) {
            timeoutCount = 0;
            (void)MsnFileMgrSaveFile(filename, MMPA_MAX_PATH, logicId);
        } else if (ret == IDE_DAEMON_SOCK_CLOSE) { // 对端不存在, log daemon 进程退出, 等待9s log daemon重新被拉起
            SELF_LOG_WARN("Can not receive data, log daemon process exit, try to connect again.");
            MsnReportReleaseHandle(&handle);
            sleep(SLOGD_WAIT_TIME);
        } else if (ret == (int32_t)IDE_DAEMON_HDC_TIMEOUT) {
            usleep(RETRY_WAIT_TIME);
            if ((timeoutCount == MAX_RECV_TIMEOUT_COUNT - 1) && (!MsnReportCheckDeviceAlive(logicId))) {
                threadArgs->isThreadExit = true;
            }
            timeoutCount = (timeoutCount + 1) % MAX_RECV_TIMEOUT_COUNT;
        } else {
            usleep(RETRY_WAIT_TIME);
        }
    }
    MsnReportBboxHBL(logicId, threadArgs);
    threadArgs->isThreadExit = true;
    MsnReportReleaseHandle(&handle);
    SELF_LOG_WARN("Receive log daemon thread exited, device os %ld", threadArgs->devOsId);
    return NULL;
}

STATIC void MsnReportFaultEventHandle(struct dsmi_event *event)
{
    uint32_t devId = event->event_t.dms_event.deviceid;
    SELF_LOG_INFO("Receive fault event, event_id:0x%X, device id:%u, assertion:%u", event->event_t.dms_event.event_id,
                  devId, (uint8_t)event->event_t.dms_event.assertion);
    if (event->event_t.dms_event.assertion == 0) { // 故障恢复
        return;
    }

    // 筛选出要监控的 device
    if ((g_msnDeviceInfo.devId != MAX_DEV_NUM) && (g_msnDeviceInfo.masterIds[devId] != g_msnDeviceInfo.masterId)) {
        return;
    }

    uint32_t phyId = 0;
    drvError_t err = DRV_ERROR_NONE;
    err = drvDeviceGetPhyIdByIndex(devId, &phyId);
    ONE_ACT_ERR_LOG(err != DRV_ERROR_NONE, return, "Get physical id failed, logic id:%u", devId);

    char relativePath[MMPA_MAX_PATH] = {0};
    int32_t ret = MsnReportBuildFaultEventPath(event, devId, phyId, relativePath);
    if (ret != EN_OK) {
        return;
    }
    ret = MsnReportGetFaultEventFiles((uint16_t)phyId, relativePath);
    if (ret != EN_OK) {
        return;
    }
    SELF_LOG_INFO("Save file %s finish.", relativePath);
}

static int32_t MsnReportSubscribeFaultEvent(void)
{
    g_drvDsmiLibHandle = dlopen(DRV_DSMI_LIB, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    ONE_ACT_ERR_LOG(g_drvDsmiLibHandle == NULL, return EN_ERROR, "Dlopen api from %s failed, dlopen error: %s",
                    DRV_DSMI_LIB, dlerror())
    DsmiSubscribeFaultEvent func = (DsmiSubscribeFaultEvent)dlsym(g_drvDsmiLibHandle, "dsmi_subscribe_fault_event");
    ONE_ACT_ERR_LOG(func == NULL, return EN_ERROR, "Dlsym api dsmi_read_fault_event failed.");
    struct dsmi_event_filter filter = {0};
    int32_t ret = func(-1, filter, MsnReportFaultEventHandle);
    ONE_ACT_ERR_LOG(ret != 0, return EN_ERROR, "Call dsmi_subscribe_fault_event failed, ret:%d", ret);

    return EN_OK;
}

STATIC void MsnReportPermanentStop(void)
{
    int32_t ret = 0;
    for (int32_t i = 0; i < MAX_DEV_NUM; i++) {
        if (g_slogdThread[i].tid != 0) {
            ret = mmJoinTask(&g_slogdThread[i].tid);
            if (ret != EN_OK) {
                SELF_LOG_ERROR("Join slogd thread failed, dev_id=%d, strerr=%s.", i, strerror(mmGetErrorCode()));
            }
            g_slogdThread[i].tid = 0;
        }
        if (g_logDaemonThread[i].tid != 0) {
            ret = mmJoinTask(&g_logDaemonThread[i].tid);
            if (ret != EN_OK) {
                SELF_LOG_ERROR("Join log daemon thread failed, dev_id=%d, strerr=%s.", i, strerror(mmGetErrorCode()));
            }
            g_logDaemonThread[i].tid = 0;
        }
    }
}

STATIC int32_t MsnReportCreateThread(uint32_t devId)
{
    int32_t ret = 0;
    MsnSetThreadInfo(g_slogdThread, g_threadArgInfo, MsnReportRecvSlogd, ALL_LOG);
    MsnSetThreadInfo(g_logDaemonThread, g_threadArgInfo, MsnReportRecvLogDaemon, ALL_LOG);
    if (devId != MAX_DEV_NUM) {
        ret = MsnReportCreateThreadSingleOs(devId, g_slogdThread, true);
        ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_ERROR, "Create slogd Thread failed.");
        ret = MsnReportCreateThreadSingleOs(devId, g_logDaemonThread, true);
        // if failed, return EN_OK to ensure the slogd thread can be joined
        ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_OK, "Create log daemon Thread failed.");
    } else {
        ret = MsnReportCreateThreadForAllDevice(g_slogdThread, MAX_DEV_NUM, true);
        ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_ERROR, "Create slogd Thread failed.");
        ret = MsnReportCreateThreadForAllDevice(g_logDaemonThread, MAX_DEV_NUM, true);
        ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_OK, "Create log daemon Thread failed.");
    }

    return EN_OK;
}

STATIC int32_t MsnReportPermanent(uint32_t devId, FileAgeingParam *param)
{
    if (!IsHaveExecPermission()) {
        SELF_LOG_ERROR("Not have permission to export files.");
        return EN_ERROR;
    }
    bool setPathFlag = false;

    if (param->outputFileName[0] == '\0') {
        if (CreateLogRootPath() != EN_OK) {
            return EN_ERROR;
        }
    } else {
        if (SetLogRootPath(param->outputFileName) != EN_OK) {
            SELF_LOG_ERROR("SetLogRootPath failed");
            return EN_ERROR;
        }
        setPathFlag = true;
    }
    if (MsnReportInitDevInfo(devId) != EN_OK) {
        return EN_ERROR;
    }

    // init param
    if (MsnFileMgrInit(param, g_logPath) != EN_OK) {
        return EN_ERROR;
    }

    if (setPathFlag) {
        SELF_LOG_INFO("Scanning and aging existing files in %s", g_logPath);
        if (MsnFileMgrScanAndAge(g_logPath) != EN_OK) {
            SELF_LOG_WARN("Scan and age existing files failed, continue anyway.");
        }
    }

    // subscriber fault event
    int32_t ret = MsnReportSubscribeFaultEvent();
    if (ret == EN_OK) {
        // create thread
        ret = MsnReportCreateThread(devId);
        if (ret == EN_OK) {
            MsnReportPermanentStop();
        }
    }

    if (g_drvDsmiLibHandle != NULL) {
        dlclose(g_drvDsmiLibHandle);
    }
    MsnFileMgrExit();
    MSNPU_FPRINTF("ERROR: Continuous export failed, check syslog for more information.\n");
    return ret;
}

int32_t MsnReport(ArgInfo *argInfo)
{
    if (argInfo->cmdType == REPORT_PERMANENT) {
        return MsnReportPermanent(argInfo->deviceId, (FileAgeingParam *)argInfo->value);
    }

    struct BboxDumpOpt bboxDumpOpt = {false, false, false, false, argInfo->printMode, argInfo->selfLogLevel};
    if (argInfo->subCmd == REPORT_ALL) {
        bboxDumpOpt.all = true;
    } else if ((argInfo->subCmd == REPORT_FORCE) ||
               ((argInfo->subCmd == REPORT_TYPE) && (argInfo->reportType == HISILOGS_LOG))) {
        bboxDumpOpt.force = true;
    } else if (argInfo->reportType == VMCORE_FILE) {
        bboxDumpOpt.vmcore = true;
    }
    return SyncDeviceLog(argInfo->deviceId, &bboxDumpOpt, argInfo->reportType);
}
