/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <dirent.h>
#include <sys/stat.h>
#include "msnpureport_file_mgr.h"
#include <regex.h>
#include "mmpa_api.h"
#include "msnpureport_print.h"
#include "msnpureport_utils.h"
#include "ascend_hal.h"

#define MAX_SCAN_FILES 14400
#define TIME_SEC_LEN 15

#define MAX_SLOG_TYPE (FIRM_LOG_TYPE + 1)

#define UNIT_US_TO_MS 1000
#define TIME_STR_SIZE 32
#define MSN_M_TO_BITE 1048576
#define NOT_AGING_FILE_NUM 2
#define NO_SPACE_LOG_INTERVAL (15 * 60) // 15 minute
// file mode
#define MSN_FILE_RDWR_MODE 0600
#define MSN_FILE_ARCHIVE_MODE 0400

typedef struct {
    uint16_t flag;
    const char *path;
    const char *type;
} MsnSlogTable;

static const MsnSlogTable MSNPUREPORT_SLOG_INFO[] = {{DEBUG_SYS_LOG_TYPE, "debug/device-os", "device-os"},
                                                     {SEC_SYS_LOG_TYPE, "security/device-os", "device-os"},
                                                     {RUN_SYS_LOG_TYPE, "run/device-os", "device-os"},
                                                     {EVENT_LOG_TYPE, "run/event", "event"},
                                                     {FIRM_LOG_TYPE, "debug/device-", "device-"}};

enum LogDaemonFileType {
    STACKCORE_UDF,
    STACKCORE,
    SLOGDLOG,
    DEVICE_APP_RUN_LOG,
    DEVICE_APP_DEBUG_LOG,
    FAULT_EVENT,
    BBOX_EVENT,
    LOG_DAEMON_FILE_TYPE_NUM
};

static const char *g_notAgingFile[NOT_AGING_FILE_NUM] = {
    "^slog/dev-os-[0-9]+/slogd/slogdlog$",
    "^hisi_logs/device-[0-9]+/history.log$",
};

static const char *g_logDaemonFileNameRegex[LOG_DAEMON_FILE_TYPE_NUM] = {
    [STACKCORE_UDF] = "^stackcore/dev-os-[0-9]+/udf/stackcore.*",
    [STACKCORE] = "^stackcore/dev-os-[0-9]+/(./)?(stackcore|coretrace).*",
    [SLOGDLOG] = "^slog/dev-os-[0-9]+/slogd/slogdlog_[0-9]+",
    [DEVICE_APP_RUN_LOG] = "^slog/dev-os-[0-9]+/run/device-app-[0-9]+",
    [DEVICE_APP_DEBUG_LOG] = "^slog/dev-os-[0-9]+/debug/device-app-[0-9]+",
    [FAULT_EVENT] = "^system_info/device-[0-9A-F]+/fault_event_0x[0-9A-F]+_[0-9]+",
    [BBOX_EVENT] = "^hisi_logs/device-[0-9]+/[0-9]+-[0-9]+",
};
static const char *g_slogFileNameRegex[MAX_SLOG_TYPE] = {
    [DEBUG_SYS_LOG_TYPE] = "^slog/dev-os-[0-9]+/debug/device-os/device-os_.*",
    [SEC_SYS_LOG_TYPE] = "^slog/dev-os-[0-9]+/security/device-os/device-os_.*",
    [RUN_SYS_LOG_TYPE] = "^slog/dev-os-[0-9]+/run/device-os/device-os_.*",
    [EVENT_LOG_TYPE] = "^slog/dev-os-[0-9]+/run/event/event_.*",
    [FIRM_LOG_TYPE] = "^slog/dev-os-[0-9]+/debug/device-[0-9]+/device-[0-9]+.*"};

typedef struct {
    uint32_t maxFileNum;
    uint32_t curFileIndex;
    char **fileName; // list of files of this type
} MsnFileList;

typedef struct {
    char rootPath[MMPA_MAX_PATH];
    struct {
        uint32_t maxFileNum;
        uint32_t maxFileSize;
        regex_t fileRegex;
    } slogFileParam[MAX_SLOG_TYPE];
    struct {
        uint32_t maxFileNum;
        regex_t fileRegex;
    } logDaemonFileParam[LOG_DAEMON_FILE_TYPE_NUM];
    regex_t noAgingFileRegex[NOT_AGING_FILE_NUM];
    MsnFileList slogFileList[DEVICE_MAX_DEV_NUM][MAX_SLOG_TYPE];
    MsnFileList logDaemonFileList[DEVICE_MAX_DEV_NUM][LOG_DAEMON_FILE_TYPE_NUM];
} MsnFileMgr;

STATIC MsnFileMgr g_fileMgr;
typedef struct {
    char fullPath[MMPA_MAX_PATH];
    char fileName[MMPA_MAX_PATH];
    time_t mtime;
} FileEntry;
FileEntry entries[MAX_SCAN_FILES];
static bool g_validFile[MAX_SCAN_FILES] = {false};
static bool g_deleteFile[MAX_SCAN_FILES] = {false};
static int FileEntryCmp(const void *a, const void *b)
{
    time_t ta = ((const FileEntry *)a)->mtime;
    time_t tb = ((const FileEntry *)b)->mtime;
    return (ta < tb) ? -1 : (ta > tb);
}

/**
 * @brief          : initialize the file manager module, record related data
 * @param [in]     : param         aging specifications of each log type
 * @param [in]     : rootPath      root path for files
 * @return         : EN_OK: succeed; EN_ERROR: failed
 */
int32_t MsnFileMgrInit(const FileAgeingParam *param, const char *rootPath)
{
    if (param == NULL || rootPath == NULL) {
        SELF_LOG_ERROR("msnpureport file manager init failed, null input.");
        return EN_ERROR;
    }
    int32_t ret = strcpy_s(g_fileMgr.rootPath, MMPA_MAX_PATH, rootPath);
    if (ret != 0) {
        SELF_LOG_ERROR("msnpureport file manager init failed, init rootPath failed.");
        return EN_ERROR;
    }

    g_fileMgr.slogFileParam[DEBUG_SYS_LOG_TYPE].maxFileNum = param->sysLogFileNum;
    g_fileMgr.slogFileParam[DEBUG_SYS_LOG_TYPE].maxFileSize = param->sysLogFileSize * MSN_M_TO_BITE;
    g_fileMgr.slogFileParam[RUN_SYS_LOG_TYPE].maxFileNum = param->sysLogFileNum;
    g_fileMgr.slogFileParam[RUN_SYS_LOG_TYPE].maxFileSize = param->sysLogFileSize * MSN_M_TO_BITE;
    g_fileMgr.slogFileParam[SEC_SYS_LOG_TYPE].maxFileNum = param->securityLogFileNum;
    g_fileMgr.slogFileParam[SEC_SYS_LOG_TYPE].maxFileSize = param->securityLogFileSize * MSN_M_TO_BITE;
    g_fileMgr.slogFileParam[EVENT_LOG_TYPE].maxFileNum = param->eventLogFileNum;
    g_fileMgr.slogFileParam[EVENT_LOG_TYPE].maxFileSize = param->eventLogFileSize * MSN_M_TO_BITE;
    g_fileMgr.slogFileParam[FIRM_LOG_TYPE].maxFileNum = param->firmwareLogFileNum;
    g_fileMgr.slogFileParam[FIRM_LOG_TYPE].maxFileSize = param->firmwareLogFileSize * MSN_M_TO_BITE;

    g_fileMgr.logDaemonFileParam[STACKCORE].maxFileNum = param->stackcoreFileNum;
    g_fileMgr.logDaemonFileParam[STACKCORE_UDF].maxFileNum = param->stackcoreFileNum;
    g_fileMgr.logDaemonFileParam[SLOGDLOG].maxFileNum = param->slogdLogFileNum - 1; // slogdlog not aging
    g_fileMgr.logDaemonFileParam[DEVICE_APP_RUN_LOG].maxFileNum = param->deviceAppDirNum;
    g_fileMgr.logDaemonFileParam[DEVICE_APP_DEBUG_LOG].maxFileNum = param->deviceAppDirNum;
    g_fileMgr.logDaemonFileParam[FAULT_EVENT].maxFileNum = param->faultEventDirNum;
    g_fileMgr.logDaemonFileParam[BBOX_EVENT].maxFileNum = param->bboxDirNum;

    for (int32_t i = 0; i < (int32_t)LOG_DAEMON_FILE_TYPE_NUM; i++) {
        ret = regcomp(&g_fileMgr.logDaemonFileParam[i].fileRegex, g_logDaemonFileNameRegex[i], REG_EXTENDED);
        ONE_ACT_ERR_LOG(ret != 0, return EN_ERROR, "Compile regex failed, file type:%d", i);
    }

    for (int32_t i = 0; i < NOT_AGING_FILE_NUM; i++) {
        ret = regcomp(&g_fileMgr.noAgingFileRegex[i], g_notAgingFile[i], REG_EXTENDED);
        ONE_ACT_ERR_LOG(ret != 0, return EN_ERROR, "Compile regex failed, file type:%d", i);
    }
    for (int32_t i = 0; i < (int32_t)MAX_SLOG_TYPE; i++) {
        ret = regcomp(&g_fileMgr.slogFileParam[i].fileRegex, g_slogFileNameRegex[i], REG_EXTENDED);
        ONE_ACT_ERR_LOG(ret != 0, return EN_ERROR, "Compile regex failed, file type:%d", i);
    }
    return EN_OK;
}

static int32_t MsnFileMgrFileListInit(MsnFileList *fileList, uint32_t maxNum)
{
    fileList->maxFileNum = maxNum;
    fileList->curFileIndex = 0;
    if (fileList->maxFileNum == 0) {
        return EN_OK;
    }
    fileList->fileName = (char **)MsnMalloc(sizeof(char *) * fileList->maxFileNum);
    if (fileList->fileName == NULL) {
        SELF_LOG_ERROR("malloc file name array failed, strerr: %s.", strerror(ToolGetErrorCode()));
        MsnFileMgrExit();
        return EN_ERROR;
    }
    fileList->fileName[0] = (char *)MsnMalloc(MAX_FILEPATH_LEN * fileList->maxFileNum);
    if (fileList->fileName[0] == NULL) {
        SELF_LOG_ERROR("malloc file name failed, strerr: %s.", strerror(ToolGetErrorCode()));
        MsnFileMgrExit();
        return EN_ERROR;
    }

    for (uint32_t j = 0; j < fileList->maxFileNum; j++) {
        fileList->fileName[j] = fileList->fileName[0] + MAX_FILEPATH_LEN * j;
    }
    return EN_OK;
}

/**
 * @brief          : initialize the name storage memory of specified device
 * @param [in]     : devId         device id
 * @return         : EN_OK: succeed; EN_ERROR: failed
 */
STATIC int32_t MsnFileMgrDeviceInit(uint32_t devId)
{
    ONE_ACT_ERR_LOG(devId >= DEVICE_MAX_DEV_NUM, return EN_ERROR, "invalid device id: %u", devId);
    MsnFileList *fileList = NULL;
    int32_t ret = 0;
    for (int32_t i = 0; i < MAX_SLOG_TYPE; ++i) {
        fileList = &g_fileMgr.slogFileList[devId][i];
        if (fileList->fileName != NULL) {
            continue;
        }
        ret = MsnFileMgrFileListInit(fileList, g_fileMgr.slogFileParam[i].maxFileNum);
        ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Init slogd file list %d failed", i);
    }
    for (int32_t i = 0; i < LOG_DAEMON_FILE_TYPE_NUM; i++) {
        fileList = &g_fileMgr.logDaemonFileList[devId][i];
        if (fileList->fileName != NULL) {
            continue;
        }
        ret = MsnFileMgrFileListInit(fileList, g_fileMgr.logDaemonFileParam[i].maxFileNum);
        ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Init log daemon file list %d failed", i);
    }
    return EN_OK;
}

STATIC void MsnFileMgrDeviceExit(int32_t devId)
{
    for (int32_t i = 0; i < MAX_SLOG_TYPE; ++i) {
        if (g_fileMgr.slogFileList[devId][i].fileName == NULL) {
            continue;
        }
        XFREE(g_fileMgr.slogFileList[devId][i].fileName[0]);
        XFREE(g_fileMgr.slogFileList[devId][i].fileName);
    }

    for (int32_t i = 0; i < (int32_t)LOG_DAEMON_FILE_TYPE_NUM; ++i) {
        if (g_fileMgr.logDaemonFileList[devId][i].fileName == NULL) {
            continue;
        }
        XFREE(g_fileMgr.logDaemonFileList[devId][i].fileName[0]);
        XFREE(g_fileMgr.logDaemonFileList[devId][i].fileName);
    }
}

void MsnFileMgrExit(void)
{
    for (int32_t i = 0; i < DEVICE_MAX_DEV_NUM; ++i) {
        MsnFileMgrDeviceExit(i);
    }
    for (int32_t i = 0; i < MAX_SLOG_TYPE; i++) {
        regfree(&g_fileMgr.slogFileParam[i].fileRegex);
    }
    for (int32_t i = 0; i < (int32_t)LOG_DAEMON_FILE_TYPE_NUM; i++) {
        regfree(&g_fileMgr.logDaemonFileParam[i].fileRegex);
    }
    for (int32_t i = 0; i < NOT_AGING_FILE_NUM; i++) {
        regfree(&g_fileMgr.noAgingFileRegex[i]);
    }
}

/**
 * @brief           : get current time by interface
 * @param [out]     : curTimeval      current time
 * @return          : EN_OK: success; EN_ERROR: failure
 */
STATIC int32_t MsnFileMgrGetTime(struct timespec *curTimeval)
{
    ONE_ACT_ERR_LOG(curTimeval == NULL, return EN_ERROR, "input timeval is null.");
    ToolTimeval timeval = {0, 0};
    if (ToolGetTimeOfDay(&timeval, NULL) != SYS_OK) {
        SELF_LOG_ERROR("get time of day failed, strerr: %s.", strerror(ToolGetErrorCode()));
        return EN_ERROR;
    }
    curTimeval->tv_sec = timeval.tvSec;
    curTimeval->tv_nsec = timeval.tvUsec * UNIT_US_TO_MS;
    return EN_OK;
}

/**
 * @brief           : get current time and splice into string
 * @param [out]     : timeStr     string to storage current time
 * @param [in]      : len         length of string
 * @return          : EN_OK: success; EN_ERROR: failure
 */
STATIC uint32_t MsnFileMgrGetTimeStr(char *timeStr, uint32_t len)
{
    ONE_ACT_ERR_LOG(timeStr == NULL, return EN_ERROR, "input time buffer is null.");
    struct timespec curTimeval = {0, 0};
    ONE_ACT_ERR_LOG(MsnFileMgrGetTime(&curTimeval) != EN_OK, return EN_ERROR, "get current time failed.");
    struct tm timeInfo = {0};
    if (ToolLocalTimeR((&curTimeval.tv_sec), &timeInfo) != EN_OK) {
        SELF_LOG_ERROR("get local time failed, strerr: %s.", strerror(ToolGetErrorCode()));
        return EN_ERROR;
    }

    int32_t ret = snprintf_s(timeStr, len, len - 1U, "%04d%02d%02d%02d%02d%02d%03ld", timeInfo.tm_year, timeInfo.tm_mon,
                             timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec,
                             curTimeval.tv_nsec / (UNIT_US_TO_MS * UNIT_US_TO_MS));
    if (ret == -1) {
        SELF_LOG_ERROR("snprintf_s time buffer failed, result: %d, strerr: %s.", ret, strerror(ToolGetErrorCode()));
        return EN_ERROR;
    }

    return EN_OK;
}

/**
 * @brief          : remove log file with given file name
 * @param [in]     : filename        file name to be deleted
 * @return         : EN_OK: succeed; EN_ERROR: failed
 */
STATIC int32_t MsnFileMgrRemoveFile(const char *filename)
{
    ONE_ACT_ERR_LOG(filename == NULL, return EN_ERROR, "input file name is null.");

    int32_t ret = ToolChmod(filename, MSN_FILE_RDWR_MODE);
    // logFile may be deleted by user, then selflog will be ignored
    if ((ret != 0) && (ToolGetErrorCode() != ENOENT)) {
        SELF_LOG_WARN("can not chmod file, file: %s, strerr: %s.", filename, strerror(ToolGetErrorCode()));
    }

    ret = ToolUnlink(filename);
    if (ret == 0) {
        return EN_OK;
    } else {
        // logFile may be deleted by user, then selflog will be ignored
        if (ToolGetErrorCode() != ENOENT) {
            SELF_LOG_WARN("can not unlink file, file: %s, strerr: %s.", filename, strerror(ToolGetErrorCode()));
        }
        return EN_ERROR;
    }
}

STATIC void MsnFileMgrDeleteLogDaemonFile(const char *fileName, int32_t fileType)
{
    char absolutePath[MMPA_MAX_PATH] = {0};
    int32_t ret = sprintf_s(absolutePath, MMPA_MAX_PATH, "%s/%s", g_fileMgr.rootPath, fileName);
    ONE_ACT_ERR_LOG(ret == -1, return, "Call sprintf_s failed for absolutePath.");
    SELF_LOG_INFO("Will delete %s.", absolutePath);
    if ((fileType >= STACKCORE_UDF) && (fileType <= SLOGDLOG)) {
        ret = MsnFileMgrRemoveFile(absolutePath);
        ONE_ACT_WARN_LOG(ret != EN_OK, return, "Delete file %s failed,", absolutePath);
    } else {
        ret = mmRmdir(absolutePath);
        ONE_ACT_WARN_LOG(ret != EN_OK, return, "Delete directory %s failed, strerr: %s.", absolutePath,
                         strerror(ToolGetErrorCode()));
    }
}

/**
 * @brief          : get size of specific file
 * @param [in]     : fileName        destination file name
 * @param [out]    : filesize        destination file size
 * @return         : EN_OK: succeed; EN_ERROR: failed
 */
STATIC int32_t MsnFileMgrGetFileSize(const char *fileName, off_t *filesize)
{
    ToolStat statBuff = {0};
    FILE *fp = NULL;

    char path[TOOL_MAX_PATH] = {0};

    // get file length
    if (ToolRealPath(fileName, path, TOOL_MAX_PATH) == SYS_OK) {
        if (ToolStatGet(path, &statBuff) == SYS_OK) {
            *filesize = statBuff.st_size;
        }
    } else {
        *filesize = 0;
    }

    LOG_CLOSE_FILE(fp);
    return EN_OK;
}

/**
 * @brief          : create new file name by timestamp and log type
 * @param [out]    : fileName        new file name
 * @param [in]     : len             length of file name
 * @param [in]     : reportMsg       struct of message from device
 * @return         : EN_OK: succeed; EN_ERROR: failed
 */
STATIC int32_t MsgFileMgrCreateNewFile(char *fileName, uint32_t len, const LogReportMsg *reportMsg)
{
    char timeBuff[TIME_STR_SIZE + 1] = {0};
    if (MsnFileMgrGetTimeStr(timeBuff, TIME_STR_SIZE) != EN_OK) {
        return EN_ERROR;
    }
    int32_t ret = EN_ERROR;
    if (reportMsg->logType == FIRM_LOG_TYPE) {
        ret = sprintf_s(fileName, len, "%s%u_%s.log", MSNPUREPORT_SLOG_INFO[reportMsg->logType].type, reportMsg->devId,
                        timeBuff);
        ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "sprintf firm failed, strerr: %s", strerror(ToolGetErrorCode()));
    } else {
        ret = sprintf_s(fileName, len, "%s_%s.log", MSNPUREPORT_SLOG_INFO[reportMsg->logType].type, timeBuff);
        ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "sprintf new failed, strerr: %s", strerror(ToolGetErrorCode()));
    }
    return EN_OK;
}

STATIC int32_t MsnFileMgrFileProcess(MsnFileList *fileList, uint32_t maxSize, const LogReportMsg *msg, char *path)
{
    off_t filesize = 0;
    int32_t ret = 0;
    // create new one if not exists
    if (strlen(fileList->fileName[fileList->curFileIndex]) == 0) {
        return MsgFileMgrCreateNewFile(fileList->fileName[fileList->curFileIndex], MAX_FILEPATH_LEN, msg);
    }
    char fullPath[MMPA_MAX_PATH] = {0};
    ret = sprintf_s(fullPath, MMPA_MAX_PATH, "%s/%s", path, fileList->fileName[fileList->curFileIndex]);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "sprintf write file failed, strerr: %s", strerror(ToolGetErrorCode()));
    ret = MsnFileMgrGetFileSize(fullPath, &filesize);
    if ((ret != EN_OK) || (filesize < 0)) {
        SELF_LOG_ERROR("get file %s size failed, ret: %d, size: %ld", fullPath, ret, filesize);
        return EN_ERROR;
    }

    // record in new file
    if (filesize + msg->bufLen > maxSize) {
        ret = ToolChmod((const char *)fullPath, MSN_FILE_ARCHIVE_MODE);
        NO_ACT_WARN_LOG((ret != 0) && (ToolGetErrorCode() != ENOENT), "can not chmod file, file: %s, strerr: %s",
                        fullPath, strerror(ToolGetErrorCode()));
        fileList->curFileIndex = (fileList->curFileIndex + 1U) % fileList->maxFileNum;
        // need to aging
        if (strlen(fileList->fileName[fileList->curFileIndex]) != 0) {
            char oldFileName[MMPA_MAX_PATH] = {0};
            ret = sprintf_s(oldFileName, MMPA_MAX_PATH, "%s/%s", path, fileList->fileName[fileList->curFileIndex]);
            if (ret == -1) {
                SELF_LOG_WARN("can not sprintf write file, strerr: %s", strerror(ToolGetErrorCode()));
            }
            ret = MsnFileMgrRemoveFile(oldFileName);
            if (ret != EN_OK) {
                SELF_LOG_WARN("can not remove file %s, ret: %d", oldFileName, ret);
            }
            (void)memset_s(fileList->fileName[fileList->curFileIndex], MAX_FILEPATH_LEN, 0, MAX_FILEPATH_LEN);
        }
        return MsgFileMgrCreateNewFile(fileList->fileName[fileList->curFileIndex], MAX_FILEPATH_LEN, msg);
    }
    return EN_OK;
}

STATIC int32_t MsnFileMgrFileGetName(MsnFileList *fileList, uint32_t maxSize, const LogReportMsg *msg, char *path)
{
    int32_t ret = MsnFileMgrFileProcess(fileList, maxSize, msg, path);
    if (ret != EN_OK) {
        return ret;
    }
    ret = sprintf_s(path, MMPA_MAX_PATH, "%s/%s", path, fileList->fileName[fileList->curFileIndex]);
    ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "sprintf fullpath failed, strerr: %s", strerror(ToolGetErrorCode()));
    return EN_OK;
}

STATIC int32_t MsnFileMgrWriteData(const char *writeFile, const LogReportMsg *reportMsg)
{
    static struct timespec timeVal = {0, 0}; // last time to print error message

    int32_t fd = ToolOpenWithMode(writeFile, (uint32_t)O_CREAT | (uint32_t)O_WRONLY | (uint32_t)O_APPEND,
                                  MSN_FILE_RDWR_MODE);
    if (fd < 0) {
        SELF_LOG_ERROR("open file failed with mode, file: %s, strerr: %s.", writeFile, strerror(ToolGetErrorCode()));
        return EN_ERROR;
    }

    int32_t ret = ToolWrite(fd, reportMsg->buf, reportMsg->bufLen);
    if ((ret < 0) || ((uint32_t)ret != reportMsg->bufLen)) {
        int savedErrno = ToolGetErrorCode(); // save errno before LOG_CLOSE_FD may change it
        LOG_CLOSE_FD(fd);
        if (savedErrno == ENOSPC) { // disk full: print screen error and abort
            SELF_LOG_ERROR("Write log file failed due to insufficient disk space, strerr: %s.", strerror(savedErrno));
            return EN_ERROR;
        }
        if (timeVal.tv_sec == 0) { // print error log at the first time.
            SELF_LOG_ERROR("write log to file failed, data length: %u bytes, strerr: %s.", reportMsg->bufLen,
                           strerror(savedErrno));
            ONE_ACT_ERR_LOG(MsnFileMgrGetTime(&timeVal) != EN_OK, return EN_ERROR, "get current time failed.");
        } else { // print error log every 15 minute
            struct timespec curTimeval = {0, 0};
            ONE_ACT_ERR_LOG(MsnFileMgrGetTime(&curTimeval) != EN_OK, return EN_ERROR, "get current time failed.");
            if (curTimeval.tv_sec - timeVal.tv_sec > NO_SPACE_LOG_INTERVAL) {
                SELF_LOG_ERROR("Write log to file failed, strerr: %s.", strerror(savedErrno));
                timeVal = curTimeval;
            }
        }
        return EN_ERROR;
    }
    timeVal.tv_sec = 0;

    ret = ToolFChownPath(fd);
    if (ret != EN_OK) {
        SELF_LOG_ERROR("change file owner failed, file: %s, ret: %d, strerr: %s.", writeFile, ret,
                       strerror(ToolGetErrorCode()));
    }
    LOG_CLOSE_FD(fd);
    return EN_OK;
}

/**
 * @brief          : storage log from device according to specific device and log type
 * @param [in]     : msg         message of struct LogReportMsg
 * @param [in]     : len         length of message
 * @param [in]     : devId       master id
 * @return         : EN_OK: succeed; EN_ERROR: failed
 */
int32_t MsnFileMgrWriteDeviceSlog(void *msg, uint32_t len, uint32_t deviceId)
{
    ONE_ACT_ERR_LOG(msg == NULL, return EN_ERROR, "invalid input, null message");
    ONE_ACT_ERR_LOG(len < sizeof(LogReportMsg), return EN_ERROR, "invalid message length: %u bytes", len);
    ONE_ACT_ERR_LOG(deviceId > (uint32_t)DEVICE_MAX_DEV_NUM, return EN_ERROR, "invalid device id: %u", deviceId);

    LogReportMsg *reportMsg = (LogReportMsg *)msg;
    ONE_ACT_ERR_LOG(reportMsg->magic != LOG_REPORT_MAGIC, return EN_ERROR, "invalid magic num: %u", reportMsg->magic);
    ONE_ACT_ERR_LOG(reportMsg->logType >= MAX_SLOG_TYPE, return EN_ERROR, "invalid log type: %u", reportMsg->logType);
    uint16_t type = reportMsg->logType;
#ifdef USE_LOGIC_ID
    // FIRM_LOG_TYPE 路径里 device-<X>/device-<X>_*.log 中的 X 也用 logicId, 不用 device 侧上来的 phy id
    if (type == FIRM_LOG_TYPE) {
        reportMsg->devId = (uint8_t)deviceId;
    }
#endif
    uint32_t devId = (type == FIRM_LOG_TYPE) ? reportMsg->devId : deviceId;
    ONE_ACT_ERR_LOG(MsnFileMgrDeviceInit(devId) != EN_OK, return EN_ERROR, "init device %u file list failed", devId);

    uint32_t maxFileSize = g_fileMgr.slogFileParam[type].maxFileSize;
    // path check
    char path[MMPA_MAX_PATH] = {0};
    int32_t ret = 0;
    if (type == FIRM_LOG_TYPE) {
        ret = sprintf_s(path, MMPA_MAX_PATH, "%s/slog/dev-os-%u/%s%u", g_fileMgr.rootPath, deviceId,
                        MSNPUREPORT_SLOG_INFO[type].path, devId);
        ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "sprintf firm failed, strerr: %s", strerror(ToolGetErrorCode()));
    } else {
        ret = sprintf_s(path, MMPA_MAX_PATH, "%s/slog/dev-os-%u/%s", g_fileMgr.rootPath, deviceId,
                        MSNPUREPORT_SLOG_INFO[type].path);
        ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "sprintf path failed, strerr: %s", strerror(ToolGetErrorCode()));
    }
    ret = MsnMkdirMulti(path);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "mkdir path %s failed, strerr: %s, ret: %d.", path,
                    strerror(ToolGetErrorCode()), ret);

    // judge save in new or old file, get name
    MsnFileList *curFileList = &(g_fileMgr.slogFileList[devId][type]);
    ret = MsnFileMgrFileGetName(curFileList, maxFileSize, reportMsg, path);
    ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_ERROR, "get type %u file failed", type);

    return MsnFileMgrWriteData(path, reportMsg);
}

static int32_t MsnFileMgrGetFileType(char *filename, uint32_t len, int32_t *fileType)
{
    regmatch_t match[1];
    int32_t ret = 0;

    for (int32_t i = 0; i < NOT_AGING_FILE_NUM; i++) {
        ret = regexec(&g_fileMgr.noAgingFileRegex[i], filename, 0, NULL, 0);
        if (ret == 0) {
            return EN_ERROR; // return error not save file name
        }
    }

    for (int32_t i = 0; i < LOG_DAEMON_FILE_TYPE_NUM; i++) {
        ret = regexec(&g_fileMgr.logDaemonFileParam[i].fileRegex, filename, 1, match, 0);
        if (ret == 0) { // match
            *fileType = i;
            if ((uint32_t)match[0].rm_eo < len) {
                filename[match[0].rm_eo] = '\0'; // for bbox, system_info only save dir name
            }
            return EN_OK;
        }
    }
    SELF_LOG_WARN("Unexpected log daemon file name:%s", filename);
    return EN_ERROR;
}

static int32_t MsnFileMgrFilterDuplicates(const MsnFileList *fileList, const char *filename)
{
    if (strcmp(fileList->fileName[fileList->curFileIndex], filename) == 0) { // BBOX multiple file
        return EN_OK;
    }

    for (uint32_t i = 0; i < fileList->maxFileNum; ++i) {
        if (strcmp(fileList->fileName[i], filename) == 0) {
            return EN_OK;
        }
    }
    return EN_ERROR;
}

int32_t MsnFileMgrSaveFile(char *filename, uint32_t len, uint32_t deviceId)
{
    ONE_ACT_ERR_LOG(filename == NULL, return EN_ERROR, "Receive filename is NULL.");
    ONE_ACT_ERR_LOG(deviceId >= DEVICE_MAX_DEV_NUM, return EN_ERROR, "invalid device id: %u", deviceId);
    SELF_LOG_DEBUG("Receive file %s.", filename);
    int32_t fileType = 0;
    int32_t ret = MsnFileMgrGetFileType(filename, len, &fileType);
    if (ret != EN_OK) {
        return EN_ERROR;
    }

    uint32_t devId = deviceId;
    if (fileType == (int32_t)BBOX_EVENT) {
        ret = sscanf_s(filename, "hisi_logs/device-%u/*", &devId);
        ONE_ACT_ERR_LOG(ret != 1, return EN_ERROR, "Not find device id, filename: %s", filename);
    }
    ret = MsnFileMgrDeviceInit(devId);
    ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Init file manage for device %u failed.", devId);

    MsnFileList *fileList = &g_fileMgr.logDaemonFileList[devId][fileType];
    if ((fileType == (int32_t)SLOGDLOG) && (fileList->maxFileNum == 0)) {
        char absolutePath[MMPA_MAX_PATH] = {0};
        ret = sprintf_s(absolutePath, MMPA_MAX_PATH, "%s/%s", g_fileMgr.rootPath, filename);
        ONE_ACT_ERR_LOG(ret == -1, return EN_ERROR, "Call sprintf_s failed for absolutePath.");
        (void)MsnFileMgrRemoveFile(absolutePath);
        return EN_OK;
    }

    if (MsnFileMgrFilterDuplicates(fileList, filename) == EN_OK) {
        return EN_OK;
    }

    uint32_t index = (fileList->curFileIndex + 1) % fileList->maxFileNum;
    if (strlen(fileList->fileName[index]) != 0) {
        MsnFileMgrDeleteLogDaemonFile(fileList->fileName[index], fileType);
        (void)memset_s(fileList->fileName[index], MAX_FILEPATH_LEN, 0, MAX_FILEPATH_LEN);
    }

    errno_t err = strcpy_s(fileList->fileName[index], MAX_FILEPATH_LEN, filename);
    if (err != EOK) {
        SELF_LOG_ERROR("Call strcpy_s to save filename failed, ret:%d.", err);
        return EN_ERROR;
    }
    fileList->curFileIndex = index;

    return EN_OK;
}
static int32_t RemoveEmptyDirs(const char *rootPath, const char *relativePath)
{
    char absPath[MMPA_MAX_PATH];
    int32_t ret = sprintf_s(absPath, sizeof(absPath), "%s/%s", rootPath, relativePath);
    ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_ERROR, "sprintf abspath failed.");
    DIR *dir = opendir(absPath);
    if (!dir)
        return EN_OK;
    struct dirent *ent;
    bool hasFiles = false;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        char relFile[MMPA_MAX_PATH], fullFile[MMPA_MAX_PATH];
        ret = sprintf_s(relFile, sizeof(relFile), "%s/%s", relativePath, ent->d_name); //  /rel/name
        ONE_ACT_WARN_LOG(ret == EN_ERROR, continue, "sprintf relFile failed.");
        ret = sprintf_s(fullFile, sizeof(fullFile), "%s/%s", rootPath, relFile); // root/rel/name
        ONE_ACT_WARN_LOG(ret == EN_ERROR, continue, "sprintf fullFile failed.");
        struct stat st;
        ret = stat(fullFile, &st);
        ONE_ACT_WARN_LOG(ret != EN_OK, continue, "get fullFile stat failed.");
        if (S_ISDIR(st.st_mode)) {
            RemoveEmptyDirs(rootPath, relFile);
        }
        if (!S_ISDIR(st.st_mode) || access(fullFile, F_OK) == 0) {
            hasFiles = true;
        }
    }
    closedir(dir);
    if (strcmp(relativePath, "") != 0 && strcmp(relativePath, "/") != 0 && !hasFiles) {
        char fullPath[MMPA_MAX_PATH];
        if (sprintf_s(fullPath, sizeof(fullPath), "%s/%s", rootPath, relativePath) != -1) {
            ret = rmdir(fullPath);
            ONE_ACT_WARN_LOG(ret != EN_OK, return EN_OK, "rmdir %s failed.", fullPath);
        }
    }
    return EN_OK;
}
static void PurgeInvalidDirs(uint32_t cnt)
{
    ONE_ACT_WARN_LOG(cnt == 0, return, "no file delete.");
    uint32_t deleted_count = 0;
    for (uint32_t i = 0; i < cnt; ++i) {
        if (g_validFile[i])
            continue;
        // delete invalid file
        char absolutePath[MMPA_MAX_PATH] = {0};
        int32_t ret = sprintf_s(absolutePath, MMPA_MAX_PATH, "%s/%s", g_fileMgr.rootPath, entries[i].fullPath);
        ONE_ACT_WARN_LOG(ret == EN_ERROR, continue, "can not sprintf write file, strerr: %d.", ret);
        ret = MsnFileMgrRemoveFile(absolutePath);
        ONE_ACT_WARN_LOG(ret != EN_OK, continue, "can not remove file %s, ret: %d", absolutePath, ret);
        deleted_count++;
    }
    ONE_ACT_INFO_LOG(deleted_count == EN_OK, return, "no file delete");
    // delete invalid dir
    RemoveEmptyDirs(g_fileMgr.rootPath, "");
}
static int32_t GetFilenameDeviceId(char *filename)
{
    int dev_os_id = -1;
    int dev_os_id_len = 7;
    const char *pos = strstr(filename, "dev-os-");
    if (pos != NULL) {
        pos += dev_os_id_len;
        if (sscanf_s(pos, "%d", &dev_os_id) != 1) {
            dev_os_id = -1;
        }
    }
    if (dev_os_id != -1 && dev_os_id < DEVICE_MAX_DEV_NUM) {
        return dev_os_id;
    }
    int device_id = -1;
    int device_id_len = 7;
    pos = strstr(filename, "device-");
    if (pos != NULL) {
        pos += device_id_len;
        if (sscanf_s(pos, "%d", &device_id) != 1) {
            device_id = -1;
        }
    }
    if (device_id != -1 && device_id < DEVICE_MAX_DEV_NUM) {
        return device_id;
    }
    return EN_ERROR;
}
static int32_t MsnFileMgrGetFileDevice(char *filename, uint32_t len, uint32_t deviceId, regex_t regex)
{
    int32_t dev = GetFilenameDeviceId(filename);
    if (dev != (int32_t)deviceId) {
        return EN_ERROR;
    }
    regmatch_t match[1];
    int32_t ret = 0;
    for (int32_t i = 0; i < NOT_AGING_FILE_NUM; i++) {
        ret = regexec(&g_fileMgr.noAgingFileRegex[i], filename, 0, NULL, 0);
        if (ret == 0) {
            return EN_ERROR; // return error not save file name
        }
    }
    ret = regexec(&regex, filename, 1, match, 0);
    if (ret == 0) {
        if ((uint32_t)match[0].rm_eo < len) {
            filename[match[0].rm_eo] = '\0'; // for bbox, system_info only save dir name
        }
        return EN_OK;
    }
    return EN_ERROR;
}
static void PrimeFileList(uint32_t cnt, MsnFileList *fileList, uint32_t deviceId, int32_t filetype, bool isDaemon)
{
    regex_t fileRegex = {0};
    if (isDaemon) {
        fileRegex = g_fileMgr.logDaemonFileParam[filetype].fileRegex;
    } else {
        fileRegex = g_fileMgr.slogFileParam[filetype].fileRegex;
    }
    uint32_t matchSize = 0;
    for (uint32_t i = 0; i < cnt && matchSize < cnt; ++i) {
        if (g_validFile[i] || g_deleteFile[i])
            continue;
        int32_t ret = MsnFileMgrGetFileDevice(entries[i].fullPath, MMPA_MAX_PATH, deviceId, fileRegex);
        if (ret == EN_ERROR) {
            continue;
        }
        if (MsnFileMgrFilterDuplicates(fileList, entries[i].fullPath) == EN_OK) {
            g_validFile[i] = true;
            matchSize++;
            continue;
        }
        uint32_t index = (fileList->curFileIndex + 1) % fileList->maxFileNum;
        if (strlen(fileList->fileName[index]) != 0) {
            // delete file
            if (isDaemon) {
                MsnFileMgrDeleteLogDaemonFile(fileList->fileName[index], filetype);
            } else {
                char *lastSlash = strrchr(entries[i].fullPath, '/');
                size_t dir_len = lastSlash - entries[i].fullPath;
                char path[MMPA_MAX_PATH] = {0};
                ret = memcpy_s(path, MMPA_MAX_PATH, entries[i].fullPath, dir_len);
                ONE_ACT_WARN_LOG(ret != EN_OK, continue, "memcpy failed, strerr: %s", strerror(ToolGetErrorCode()));
                char oldFileName[MMPA_MAX_PATH] = {0};
                ret = sprintf_s(oldFileName, MMPA_MAX_PATH, "%s/%s/%s", g_fileMgr.rootPath, path,
                                fileList->fileName[index]);
                ONE_ACT_WARN_LOG(ret == EN_ERROR, continue, "can not sprintf write file, strerr: %s",
                                 strerror(ToolGetErrorCode()));
                ret = MsnFileMgrRemoveFile(oldFileName);
                ONE_ACT_WARN_LOG(ret != EN_OK, continue, "can not remove file %s, ret: %d", oldFileName, ret);
            }
            g_deleteFile[i] = true;
            (void)memset_s(fileList->fileName[index], MAX_FILEPATH_LEN, 0, MAX_FILEPATH_LEN);
        }
        errno_t err = strcpy_s(fileList->fileName[index], MAX_FILEPATH_LEN, entries[i].fileName);
        ONE_ACT_WARN_LOG(ret != EN_OK, continue, "Call strcpy_s %s to save filename failed, ret:%d.",
                         entries[i].fileName, err);
        fileList->curFileIndex = index;
        g_validFile[i] = true;
        matchSize++;
    }
}
static int32_t MsnFileMgrScanDirectory(const char *rootPath, const char *relativePath, uint32_t *cnt, uint32_t maxCnt)
{
    char absPath[MMPA_MAX_PATH];
    int32_t ret = sprintf_s(absPath, sizeof(absPath), "%s/%s", rootPath, relativePath);
    ONE_ACT_ERR_LOG(ret == EN_ERROR, return EN_ERROR, "sprintf abspath failed.");
    DIR *dir = opendir(absPath);
    if (!dir)
        return EN_OK;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && *cnt < maxCnt) {
        if (ent->d_name[0] == '.')
            continue;
        char relFile[MMPA_MAX_PATH], fullFile[MMPA_MAX_PATH];
        ret = sprintf_s(relFile, sizeof(relFile), "%s/%s", relativePath, ent->d_name); //  /rel/name
        ONE_ACT_WARN_LOG(ret == EN_ERROR, continue, "sprintf relFile failed.");
        if (relFile[0] == '/') {
            memmove_s(relFile, strlen(relFile), relFile + 1, strlen(relFile));
        }
        ret = sprintf_s(fullFile, sizeof(fullFile), "%s/%s", rootPath, relFile); // root/rel/name
        ONE_ACT_ERR_LOG(ret == EN_ERROR, continue, "sprintf fullFile failed.");
        struct stat st;
        ret = stat(fullFile, &st);
        ONE_ACT_WARN_LOG(ret != EN_OK, continue, "get fullFile stat failed.");
        if (S_ISDIR(st.st_mode)) {
            MsnFileMgrScanDirectory(rootPath, relFile, cnt, maxCnt);
        } else {
            int32_t fileType = -1;
            MsnFileMgrGetFileType(relFile, MMPA_MAX_PATH, &fileType);
            FileEntry *e = &entries[*cnt];
            ret = strcpy_s(e->fullPath, sizeof(e->fullPath), relFile);
            ONE_ACT_WARN_LOG(ret != EN_OK, continue, "sprintf e->fullPath failed.");
            ret = strcpy_s(e->fileName, sizeof(e->fileName), (fileType != EN_ERROR) ? relFile : ent->d_name);
            ONE_ACT_WARN_LOG(ret != EN_OK, continue, "sprintf e->fileName failed.");
            e->mtime = st.st_mtime;
            (*cnt)++;
        }
    }
    closedir(dir);
    return EN_OK;
}
int32_t MsnFileMgrScanAndAge(const char *rootPath)
{
    int32_t ret = memset_s(g_validFile, sizeof(g_validFile), 0, sizeof(g_validFile));
    ONE_ACT_INFO_LOG(ret != EN_OK, return EN_ERROR, "memset_s g_validFile failed.");
    uint32_t cnt = 0;
    MsnFileMgrScanDirectory(rootPath, "", &cnt, MAX_SCAN_FILES);
    ONE_ACT_INFO_LOG(cnt == 0, return EN_OK, "Scan no file.");
    qsort(entries, cnt, sizeof(FileEntry), FileEntryCmp);
    for (uint32_t dev = 0; dev < DEVICE_MAX_DEV_NUM; ++dev) {
        ret = MsnFileMgrDeviceInit(dev);
        ONE_ACT_ERR_LOG(ret != EN_OK, return EN_ERROR, "Init file manage for device %u failed.", dev);
        for (int32_t t = 0; t < MAX_SLOG_TYPE; ++t) {
            if (g_fileMgr.slogFileParam[t].maxFileNum == 0)
                continue;
            MsnFileList *list = &g_fileMgr.slogFileList[dev][t];
            PrimeFileList(cnt, list, dev, t, false);
        }
        for (int32_t t = 0; t < LOG_DAEMON_FILE_TYPE_NUM; ++t) {
            if (g_fileMgr.logDaemonFileParam[t].maxFileNum == 0)
                continue;
            MsnFileList *list = &g_fileMgr.logDaemonFileList[dev][t];
            PrimeFileList(cnt, list, dev, t, true);
        }
    }
    SELF_LOG_INFO("Will delete files and empty dir in %s.", rootPath);
    PurgeInvalidDirs(cnt);
    return EN_OK;
}
