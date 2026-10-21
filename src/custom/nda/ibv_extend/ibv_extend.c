/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 *
 * File Name     : ibv_extend.c
 * Description   : The implementation of ibverbs extended Function extension interface
 */

#define _GNU_SOURCE

#include <infiniband/verbs.h>
#include <infiniband/driver.h>
#include <ccan/list.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <securec.h>
#include "config.h"

#include "ibv_extend.h"

#define OUTPFX "ibv_extend: "

const size_t driver_name_max_len = 128;
const size_t trunc_first = 1;
const size_t trunc_second = 2;
const size_t trunc_last = 3;
const size_t trunc_reserve_len = 4;
const size_t warning_msg_buf_len = 512;

#define API_EXPORT __attribute__((visibility("default")))

struct ibv_extend_driver {
    struct list_node entry;
    const struct verbs_device_extend_ops *ops; // 指向设备操作结构的指针，用于实现特定设备的操作
};

struct ibv_extend_driver_name {
    struct list_node entry;
    char *ext_name;
};

// 链表用于挂载注册的扩展驱动
static LIST_HEAD(extend_driver_list);

// 链表用于挂载查询到的扩展驱动的名称
static LIST_HEAD(extend_driver_name_list);

/**
 * @brief Warning日志封装函数
 * @param format 格式化字符串
 * @param ... 可变参数
 *
 * 通过检查环境变量IBV_EXTEND_SHOW_WARNINGS来决定是否输出Warning日志
 */
static void ibv_extend_warning(const char *format, ...)
{
    static volatile int show_warning = -1;
    char msg_buf[warning_msg_buf_len]; // 适中的缓冲区大小
    va_list args;
    int len;

    // 参数校验
    if (format == NULL) {
        (void)fprintf(stderr, OUTPFX "Warning: (null format)\n");
        return;
    }

    // 环境变量检查
    if (show_warning == -1) {
        int env_result = (getenv("IBV_EXTEND_SHOW_WARNINGS") != NULL) ? 1 : 0;
        show_warning = env_result;
    }

    // 如果环境变量存在,则输出Warning日志
    if (show_warning) {
        // 先格式化用户消息
        va_start(args, format);
        len = vsnprintf_s(msg_buf, sizeof(msg_buf), sizeof(msg_buf) - 1, format, args);
        va_end(args);

        // 处理格式化错误
        if (len < 0) {
            (void)fprintf(stderr, OUTPFX "Warning: (format error)\n");
            return;
        }

        // 如果消息被截断,添加省略号指示
        if ((size_t)len >= sizeof(msg_buf) - 1) {
            // 保留最后3个字符用于省略号
            size_t truncate_pos = sizeof(msg_buf) - trunc_reserve_len;
            msg_buf[truncate_pos] = '.';
            msg_buf[truncate_pos + trunc_first] = '.';
            msg_buf[truncate_pos + trunc_second] = '.';
            msg_buf[truncate_pos + trunc_last] = '\0';
        }

        (void)fprintf(stderr, OUTPFX "Warning: %s\n", msg_buf);
    }
}

/**
 * @brief 注册扩展驱动
 * @param ops 驱动操作结构体指针
 */
API_EXPORT void verbs_register_driver_extend(const struct verbs_device_extend_ops *ops)
{
    struct ibv_extend_driver *driver;

    if (!ops) {
        (void)fprintf(stderr, OUTPFX "Error: couldn't register driver for NULL ops\n");
        return;
    }

    if (!ops->name) {
        (void)fprintf(stderr, OUTPFX "Error: couldn't register driver for NULL name\n");
        return;
    }

    // 内存常驻在进程声明周期中，进程退出回收
    driver = malloc(sizeof(struct ibv_extend_driver));
    if (!driver) {
        (void)fprintf(stderr, OUTPFX "Error: couldn't allocate extend driver for %s\n", ops->name);
        return;
    }

    driver->ops = ops;

    list_add_tail(&extend_driver_list, &driver->entry);
}

/**
 * 尝试查找与指定设备匹配的扩展驱动程序。
 * @param device 指定的IB设备。
 * @return 如果找到匹配的驱动程序，返回其扩展操作结构体指针，否则返回NULL。
 */
static const struct verbs_device_extend_ops *try_extend_driver(struct ibv_device *device)
{
    const struct verbs_device_extend_ops *ext_ops = NULL; // 初始化扩展操作指针为NULL
    struct verbs_device *vdev;
    struct ibv_extend_driver *driver;

    if (!device) {
        return NULL;
    }

    vdev = verbs_get_device(device); // 获取设备的verbs_device结构体指针
    // 遍历扩展驱动程序列表，查找匹配的驱动程序
    list_for_each(&extend_driver_list, driver, entry)
    {
        if (strcmp(vdev->ops->name, driver->ops->name) == 0) {
            ext_ops = driver->ops; // 如果找到匹配的驱动程序，更新扩展操作指针
            break;
        }
    }
    return ext_ops; // 返回找到的扩展操作指针，如果没有找到则返回NULL
}

/**
 * 处理driver配置项
 * @param driver_name 驱动名称
 * @return 0-成功，-1-失败
 *
 * 该函数负责处理配置文件中的driver配置项
 */
static int handle_driver_config(const char *driver_name)
{
    struct ibv_extend_driver_name *ext_driver_name;

    // 分配驱动名称结构体内存
    ext_driver_name = malloc(sizeof(struct ibv_extend_driver_name));
    if (!ext_driver_name) {
        (void)fprintf(stderr, OUTPFX "Error: couldn't allocate extend driver name '%s'.\n", driver_name);
        return -1;
    }

    // 复制驱动名称字符串
    ext_driver_name->ext_name = strdup(driver_name);
    if (!ext_driver_name->ext_name) {
        (void)fprintf(stderr, OUTPFX "Error: couldn't allocate extend driver name '%s'.\n", driver_name);
        free(ext_driver_name);
        return -1;
    }

    // 将驱动名称添加到全局链表
    list_add(&extend_driver_name_list, &ext_driver_name->entry);
    return 0;
}

/**
 * 检查是否为空行或注释行
 * @param line 配置行内容
 * @return 1-是空行或注释行，0-不是
 */
static bool is_empty_or_comment_line(const char *line)
{
    if (line == NULL) {
        return 1;
    }
    return line[0] == '\n' || line[0] == '#' || line[0] == '\0';
}

/**
 * 校验配置驱动名称的合法性
 * @param driver_name 驱动名称
 * @param path 配置文件路径
 * @return 1-合法，0-不合法
 */
static bool validate_config_driver_name(const char *driver_name, const char *path)
{
    // 检查驱动名称有效性
    if (driver_name == NULL || driver_name[0] == '\0') {
        ibv_extend_warning("missing driver name in file '%s'", path);
        return 0;
    }

    // 限制驱动名称长度
    if (strlen(driver_name) > driver_name_max_len) {
        ibv_extend_warning("driver name too long in file '%s'", path);
        return 0;
    }

    // 校验驱动名称合法性（只允许字母、数字、下划线、连字符）
    for (const char *p = driver_name; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            ibv_extend_warning("invalid character in driver name '%s' in file '%s'", driver_name, path);
            return 0;
        }
    }

    return 1;
}

/**
 * 处理driver配置项
 * @param ext_config 配置内容指针
 * @param path 配置文件路径
 */
static void process_driver_config(char *ext_config, const char *path)
{
    char *field;

    // 跳过字段间的空白字符，提取驱动名称
    ext_config += strspn(ext_config, "\t ");
    field = strsep(&ext_config, "\n\t ");
    // 校验驱动名称
    if (!validate_config_driver_name(field, path)) {
        return;
    }

    (void)handle_driver_config(field);
}

/**
 * 解析单行配置
 * @param line 配置行内容
 * @param path 配置文件路径（用于错误提示）
 *
 * 该函数负责解析配置文件的单行内容
 */
static void parse_config_line(char *line, const char *path)
{
    char *ext_config;
    char *field;

    // 参数校验
    if (line == NULL || path == NULL) {
        return;
    }

    // 跳过行首空白字符
    ext_config = line + strspn(line, "\t ");
    // 跳过空行和注释行
    if (is_empty_or_comment_line(ext_config)) {
        return;
    }

    // 提取第一个字段（配置项名称）
    field = strsep(&ext_config, "\n\t ");
    // 检查第一个字段有效性
    if (field == NULL || field[0] == '\0') {
        ibv_extend_warning("invalid config line in file '%s'", path);
        return;
    }

    // 处理driver配置项
    if (strcmp(field, "driver") == 0 && ext_config != NULL) {
        process_driver_config(ext_config, path);
    } else {
        // 未知的配置项，输出警告
        ibv_extend_warning("ignoring bad config directive '%s' in file '%s'", field, path);
    }
}

/**
 * 读取扩展配置文件
 * @param path 配置文件路径
 *
 * 此函数打开并读取指定路径的扩展配置文件
 */
static void read_extend_config_file(const char *path)
{
    FILE *ext_conf;
    char *line = NULL;
    size_t buflen = 0;
    ssize_t len;

    // 打开配置文件
    ext_conf = fopen(path, "r" STREAM_CLOEXEC);
    if (!ext_conf) {
        ibv_extend_warning("couldn't read extend config file %s", path);
        return;
    }

    // 逐行读取并解析配置文件
    while ((len = getline(&line, &buflen, ext_conf)) != -1) {
        parse_config_line(line, path);
    }

    // 释放缓冲区内存
    if (line) {
        free(line);
    }

    // 关闭配置文件
    (void)fclose(ext_conf);
}

/**
 * 读取扩展配置文件
 * 该函数用于读取指定目录下的扩展配置文件。
 */
static void read_extend_config()
{
    // 定义目录指针和目录项结构体指针
    DIR *ext_conf_dir;
    struct dirent *ext_dent;
    char *tmp_path;

    // 打开扩展配置文件目录
    ext_conf_dir = opendir(EXTEND_IBV_CONFIG_DIR);
    if (!ext_conf_dir) {
        // silent return
        return;
    }

    // 遍历目录中的每个文件
    while ((ext_dent = readdir(ext_conf_dir))) {
        struct stat buf;

        // 跳过 "." 和 ".." 目录，防止路径遍历
        if (strcmp(ext_dent->d_name, ".") == 0 || strcmp(ext_dent->d_name, "..") == 0) {
            continue;
        }

        // 检查文件名是否包含路径遍历字符
        if (strstr(ext_dent->d_name, "..") != NULL || strchr(ext_dent->d_name, '/') != NULL) {
            continue;
        }

        // 构造文件的完整路径
        if (asprintf(&tmp_path, "%s/%s", EXTEND_IBV_CONFIG_DIR, ext_dent->d_name) < 0) {
            ibv_extend_warning("couldn't read extend config file %s/%s", EXTEND_IBV_CONFIG_DIR, ext_dent->d_name);
            goto out;
        }

        // 获取文件状态信息（使用 lstat 避免跟随符号链接）
        if (lstat(tmp_path, &buf)) {
            // silent
            goto next;
        }

        // 检查文件是否为普通文件（排除符号链接、目录等）
        if (!S_ISREG(buf.st_mode)) {
            goto next;
        }

        // 检查文件权限，确保文件属于当前用户或root
        if (buf.st_uid != 0 && buf.st_uid != getuid()) {
            goto next;
        }

        // 读取扩展配置文件
        read_extend_config_file(tmp_path);
    next:
        // 释放
        free(tmp_path);
    }

out:
    // 关闭目录
    closedir(ext_conf_dir);
}

/**
 * 校验驱动名称的基本有效性
 * @param name 驱动程序名称
 * @return 0-有效，-1-无效
 */
static int validate_driver_name(const char *name)
{
    // 检查驱动程序名称是否为空
    if (!name) {
        ibv_extend_warning("couldn't load NULL extend file");
        return -1;
    }

    // 检查驱动程序名称是否为空字符串
    if (name[0] == '\0') {
        ibv_extend_warning("couldn't load empty extend file");
        return -1;
    }

    // 限制驱动名称长度
    if (strlen(name) > driver_name_max_len) {
        ibv_extend_warning("driver name too long '%s'", name);
        return -1;
    }

    return 0;
}

/**
 * 校验驱动名称字符的合法性（非绝对路径时）
 * @param name 驱动程序名称
 * @return 0-有效，-1-无效
 */
static int validate_driver_name_chars(const char *name)
{
    for (const char *p = name; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            ibv_extend_warning("invalid character in driver name '%s'", name);
            return -1;
        }
    }
    return 0;
}

/**
 * 尝试加载指定路径的驱动库
 * @param so_name 驱动库完整路径
 * @return 0-加载成功，-1-加载失败
 */
static int try_load_driver(const char *so_name)
{
    void *dlhandle = dlopen(so_name, RTLD_NOW);
    char *error;

    if (dlhandle) {
        // silent return
        return 0;
    }

    // 获取错误信息并处理NULL情况
    error = dlerror();
    if (error) {
        ibv_extend_warning("dlopen '%s' failed: %s", so_name, error);
    } else {
        ibv_extend_warning("dlopen '%s' failed: unknown error", so_name);
    }

    return -1;
}

/**
 * 加载绝对路径的驱动程序
 * @param name 驱动程序绝对路径
 * @return 0-成功，-1-失败
 */
static int load_absolute_path_driver(const char *name)
{
    char *so_name = NULL;
    int ret = -1;

    // 校验绝对路径的合法性（防止路径遍历）
    if (strstr(name, "..") != NULL) {
        ibv_extend_warning("invalid path '%s' (path traversal detected)", name);
        return -1;
    }

    if (asprintf(&so_name, "%s", name) < 0) {
        ibv_extend_warning("couldn't load extend driver '%s'", name);
        return -1;
    }

    if (try_load_driver(so_name) == 0) {
        ret = 0;
    }

    free(so_name);
    return ret;
}

/**
 * 加载相对路径的驱动程序
 * @param name 驱动程序名称
 * @return 0-成功，-1-失败
 */
static int load_relative_path_driver(const char *name)
{
    char *so_name = NULL;

    // 尝试在EXTEND_VERBS_PROVIDER_DIR目录下查找并加载驱动程序
    if (sizeof(EXTEND_VERBS_PROVIDER_DIR) > 1) {
        if (asprintf(&so_name, EXTEND_VERBS_PROVIDER_DIR "/lib%s.so", name) < 0) {
            ibv_extend_warning("couldn't load extend driver '%s'", name);
            return -1;
        }
        if (try_load_driver(so_name) == 0) {
            free(so_name);
            return 0;
        }
        free(so_name);
    }

    // 尝试在当前目录下加载驱动程序
    if (asprintf(&so_name, "lib%s.so", name) < 0) {
        ibv_extend_warning("couldn't load extend driver '%s'", name);
        return -1;
    }
    if (try_load_driver(so_name) == 0) {
        free(so_name);
        return 0;
    }

    free(so_name);
    return -1;
}

/**
 * 加载扩展驱动程序。
 * @param name 驱动程序名称，不能为空。
 *
 * 该函数尝试加载指定的扩展驱动程序
 * 1. 如果name是绝对路径，则直接加载
 * 2. 如果定义EXTEND_VERBS_PROVIDER_DIR,则尝试在此目录下查找并加载驱动程序
 * 3. 最后尝试在当前目录下加载驱动程序
 */
static void load_extend_driver(const char *name)
{
    // 校验驱动名称基本有效性
    if (validate_driver_name(name) != 0) {
        return;
    }

    // 如果name是绝对路径，则直接加载
    if (name[0] == '/') {
        (void)load_absolute_path_driver(name);
        return;
    }

    // 校验驱动名称字符合法性
    if (validate_driver_name_chars(name) != 0) {
        return;
    }

    // 加载相对路径的驱动程序
    (void)load_relative_path_driver(name);
}

/**
 * 从环境变量加载驱动程序
 * @param env 环境变量字符串
 */
static void load_drivers_from_env(const char *env)
{
    char *list, *split, *env_name;

    // 在堆上分配内存，避免环境变量配置过长
    list = strdup(env);
    if (!list) {
        (void)fprintf(stderr, OUTPFX "Error: failed to allocate memory for IBV_EXTEND_DRIVERS.\n");
        return;
    }

    split = list;
    while ((env_name = strsep(&split, ":;"))) {
        load_extend_driver(env_name);
    }
    free(list);
}

/**
 * 加载配置文件中的驱动程序
 */
static void load_drivers_from_config(void)
{
    struct ibv_extend_driver_name *name, *next_name;

    // 遍历驱动程序名称列表，加载驱动程序并释放内存
    list_for_each_safe(&extend_driver_name_list, name, next_name, entry)
    {
        load_extend_driver(name->ext_name);
        list_del(&name->entry);
        free(name->ext_name);
        free(name);
    }
}

/**
 * @brief 加载扩展驱动程序
 *
 * 此函数用于加载扩展驱动程序。
 */
static void load_extend_drivers()
{
    const char *env;

    // 读取扩展配置
    read_extend_config();

    // 优先从环境变量中加载驱动程序
    if (getuid() == geteuid() && (env = getenv("IBV_EXTEND_DRIVERS"))) {
        load_drivers_from_env(env);
    }

    // 加载配置文件中的驱动程序
    load_drivers_from_config();
}

/**
 * @brief 检查驱动版本有效性
 * @param ext_context 扩展上下文
 * @param ext_ops 扩展操作结构体
 * @return 0-版本有效，-1-版本无效
 */
static int validate_driver_version(struct ibv_context_extend *ext_context)
{
    if (!ext_context || !ext_context->ops) {
        return 0; // 无ops则跳过版本检查
    }

    /* 检查版本是否未设置(UNUSED) */
    if (ext_context->ops->version == IBV_EXTEND_DRIVER_VERSION_UNUSED) {
        (void)fprintf(stderr, OUTPFX "Error: Driver version is not set (version=0). "
                                     "Driver must set a valid version number.\n");
        return -1;
    }

    /* 检查版本上限：驱动版本不能超过库支持的最新版本 */
    if (ext_context->ops->version > IBV_EXTEND_DRIVER_VERSION_MAX) {
        (void)fprintf(stderr,
                      OUTPFX "Error: Driver version %d exceeds ibv_extend(%s) max version %d. "
                             "Please upgrade the ibv_extend to support this driver.\n",
                      ext_context->ops->version, IBV_EXTEND_VERSION_STRING, IBV_EXTEND_DRIVER_VERSION_MAX);
        return -1;
    }

    return 0;
}

/**
 * @brief 分配并验证扩展ibv_context_extend
 * @param context ibv_context结构体指针
 * @param ext_ops 扩展操作结构体
 * @return 扩展上下文指针，失败返回NULL
 */
static struct ibv_context_extend *alloc_and_validate_context(struct ibv_context *context,
                                                             const struct verbs_device_extend_ops *ext_ops)
{
    struct ibv_context_extend *ext_context;

    if (!ext_ops->alloc_context) {
        ibv_extend_warning("alloc_context is NULL for ops %s", ext_ops->name);
        return NULL;
    }

    ext_context = ext_ops->alloc_context(context);
    if (!ext_context) {
        ibv_extend_warning("driver %s alloc ext context failed", ext_ops->name);
        return NULL;
    }

    /* 版本有效性检查 */
    if (validate_driver_version(ext_context) != 0) {
        /* 版本检查失败，释放已分配的上下文 */
        if (ext_ops->free_context) {
            ext_ops->free_context(ext_context);
        }
        return NULL;
    }

    return ext_context;
}

/**
 * @brief 初始化verbs_extend上下文
 *
 * @param context ibv_context结构体指针
 * @return 返回扩展的上下文，如果失败则返回NULL
 */
API_EXPORT struct ibv_context_extend *ibv_open_extend(struct ibv_context *context)
{
    struct ibv_device *device;
    const struct verbs_device_extend_ops *ext_ops = NULL;
    struct ibv_context_extend *ext_context = NULL;

    /* 检查上下文是否为空 */
    if (context == NULL) {
        ibv_extend_warning("couldn't open extend context for NULL ibv_context");
        return NULL;
    }

    /* 获取ibv设备 */
    device = context->device;

    /* 第一次尝试：查找已注册的驱动程序 */
    ext_ops = try_extend_driver(device);
    if (ext_ops) {
        ext_context = alloc_and_validate_context(context, ext_ops);
        /* 如果找到驱动,无论分配成功或失败都直接返回 */
        return ext_context;
    }

    /* 第二次尝试：加载驱动程序后再查找 */
    load_extend_drivers();

    ext_ops = try_extend_driver(device);
    if (ext_ops) {
        ext_context = alloc_and_validate_context(context, ext_ops);
        return ext_context;
    }

    /* 未找到可用的扩展操作 */
    ibv_extend_warning("no available ops for open extend context");
    return NULL;
}

/**
 * @brief 关闭扩展上下文
 * @param context 扩展上下文，由ibv_open_extend分配
 * @return 0-成功，其他-失败
 */
API_EXPORT int ibv_close_extend(struct ibv_context_extend *context)
{
    struct ibv_context *ctx;
    struct ibv_device *device;
    const struct verbs_device_extend_ops *ext_ops;

    if (context == NULL) {
        return -EINVAL;
    }

    ctx = context->context;
    if (!ctx) {
        return -EINVAL;
    }

    // 获取ibv设备
    device = ctx->device;

    // 查找匹配的驱动程序
    ext_ops = try_extend_driver(device);
    if (!ext_ops) {
        return -ENOENT;
    }

    // 释放扩展上下文
    if (ext_ops->free_context) {
        ext_ops->free_context(context);
    }
    return 0;
}

/**
 * @brief 查询设备支持的扩展能力
 * @param context 扩展上下文
 * @param ext_dev_attr 扩展设备属性输出参数
 * @return 0-成功，其他值-失败
 */
API_EXPORT int ibv_query_device_extend(struct ibv_context_extend *context, struct ibv_device_attr_extend *ext_dev_attr)
{
    if (context == NULL || ext_dev_attr == NULL) {
        ibv_extend_warning("invalid parameter for query device extend");
        return -EINVAL;
    }

    if (context->ops == NULL || context->ops->query_device == NULL) {
        ibv_extend_warning("no available ops for query device extend");
        return -EOPNOTSUPP;
    }

    ext_dev_attr->ext_cap = 0;

    // 调用底层驱动查询设备扩展属性
    return context->ops->query_device(context->context, ext_dev_attr);
}

/**
 * @brief 创建扩展QP
 * @param context 扩展上下文
 * @param qp_init_attr QP初始化属性
 * @return QP扩展结构体指针，失败返回NULL
 */
API_EXPORT struct ibv_qp_extend *ibv_create_qp_extend(struct ibv_context_extend *context,
                                                      struct ibv_qp_init_attr_extend *qp_init_attr)
{
    if (context == NULL || qp_init_attr == NULL) {
        ibv_extend_warning("invalid parameter for create qp extend");
        return NULL;
    }

    if (context->ops == NULL || context->ops->create_qp == NULL) {
        ibv_extend_warning("no available ops for create qp extend");
        return NULL;
    }

    // 调用底层驱动创建QP
    return context->ops->create_qp(context->context, qp_init_attr);
}

/**
 * @brief 创建扩展CQ
 * @param context 扩展上下文
 * @param cq_init_attr CQ初始化属性
 * @return CQ扩展结构体指针，失败返回NULL
 */
API_EXPORT struct ibv_cq_extend *ibv_create_cq_extend(struct ibv_context_extend *context,
                                                      struct ibv_cq_init_attr_extend *cq_init_attr)
{
    if (context == NULL || cq_init_attr == NULL) {
        ibv_extend_warning("invalid parameter for create cq extend");
        return NULL;
    }

    if (context->ops == NULL || context->ops->create_cq == NULL) {
        ibv_extend_warning("no available ops for create cq extend");
        return NULL;
    }

    // 调用底层驱动创建CQ
    return context->ops->create_cq(context->context, cq_init_attr);
}

/**
 * @brief 创建扩展SRQ
 * @param context 扩展上下文
 * @param srq_init_attr SRQ初始化属性
 * @return SRQ扩展结构体指针，失败返回NULL
 */
API_EXPORT struct ibv_srq_extend *ibv_create_srq_extend(struct ibv_context_extend *context,
                                                        struct ibv_srq_init_attr_extend *srq_init_attr)
{
    if (context == NULL || srq_init_attr == NULL) {
        ibv_extend_warning("invalid parameter for create srq extend");
        return NULL;
    }

    if (context->ops == NULL || context->ops->create_srq == NULL) {
        ibv_extend_warning("no available ops for create srq extend");
        return NULL;
    }

    // 调用底层驱动创建SRQ
    return context->ops->create_srq(context->context, srq_init_attr);
}

/**
 * @brief 销毁扩展QP
 * @param context 扩展上下文
 * @param qp_extend QP扩展结构体
 * @return 0-成功，其他-失败
 */
API_EXPORT int ibv_destroy_qp_extend(struct ibv_context_extend *context, struct ibv_qp_extend *qp_extend)
{
    if (context == NULL || qp_extend == NULL) {
        ibv_extend_warning("invalid parameter for destroy qp extend");
        return -EINVAL;
    }

    if (context->ops == NULL || context->ops->destroy_qp == NULL) {
        ibv_extend_warning("no available ops for destroy qp extend");
        return -EOPNOTSUPP;
    }

    // 调用底层驱动销毁QP
    return context->ops->destroy_qp(qp_extend);
}

/**
 * @brief 销毁扩展CQ
 * @param context 扩展上下文
 * @param cq_extend CQ扩展结构体
 * @return 0-成功，其他-失败
 */
API_EXPORT int ibv_destroy_cq_extend(struct ibv_context_extend *context, struct ibv_cq_extend *cq_extend)
{
    if (context == NULL || cq_extend == NULL) {
        ibv_extend_warning("invalid parameter for destroy cq extend");
        return -EINVAL;
    }

    if (context->ops == NULL || context->ops->destroy_cq == NULL) {
        ibv_extend_warning("no available ops for destroy cq extend");
        return -EOPNOTSUPP;
    }

    // 调用底层驱动销毁CQ
    return context->ops->destroy_cq(cq_extend);
}

/**
 * @brief 销毁扩展SRQ
 * @param context 扩展上下文
 * @param srq_extend SRQ扩展结构体
 * @return 0-成功，其他-失败
 */
API_EXPORT int ibv_destroy_srq_extend(struct ibv_context_extend *context, struct ibv_srq_extend *srq_extend)
{
    if (context == NULL || srq_extend == NULL) {
        ibv_extend_warning("invalid parameter for destroy srq extend");
        return -EINVAL;
    }

    if (context->ops == NULL || context->ops->destroy_srq == NULL) {
        ibv_extend_warning("no available ops for destroy srq extend");
        return -EOPNOTSUPP;
    }

    // 调用底层驱动销毁SRQ
    return context->ops->destroy_srq(srq_extend);
}

/**
 * @brief 修改QP扩展属性
 * @param context 扩展上下文指针
 * @param attr QP扩展属性结构体
 * @param attr_mask 属性掩码，指定需要修改的属性
 * @return 0-成功，其他值-失败
 */
API_EXPORT int ibv_modify_qp_extend(struct ibv_context_extend *context, struct ibv_qp_attr_extend *attr, int attr_mask)
{
    if (context == NULL || attr == NULL) {
        ibv_extend_warning("invalid parameter for modify qp extend");
        return -EINVAL;
    }

    if (context->ops == NULL) {
        ibv_extend_warning("no available ops for modify qp extend");
        return -EOPNOTSUPP;
    }

    /* 版本检查：modify_qp 是 V2 新增功能，驱动版本必须 >= V2 */
    if (context->ops->version < IBV_EXTEND_DRIVER_VERSION_V2) {
        ibv_extend_warning("driver version %d does not support modify_qp (requires V2)", context->ops->version);
        return -EOPNOTSUPP;
    }

    if (attr_mask & ~(IBV_QP_ATTR_EXTEND_MAX - 1)) {
        ibv_extend_warning("attr_mask 0x%x is not supported", attr_mask);
        return -EOPNOTSUPP;
    }

    if (context->ops->modify_qp == NULL) {
        ibv_extend_warning("modify_qp is not supported");
        return -EOPNOTSUPP;
    }

    // 调用底层驱动修改QP属性
    return context->ops->modify_qp(context->context, attr, attr_mask);
}

/**
 * @brief 查询QP扩展属性
 * @param context 扩展上下文指针
 * @param attr QP扩展属性结构体，用于返回查询结果
 * @param attr_mask 属性掩码，指定需要查询的属性
 * @return 0-成功，其他值-失败
 */
API_EXPORT int ibv_query_qp_extend(struct ibv_context_extend *context, struct ibv_qp_attr_extend *attr, int attr_mask)
{
    if (context == NULL || attr == NULL) {
        ibv_extend_warning("invalid parameter for query qp extend");
        return -EINVAL;
    }

    if (context->ops == NULL) {
        ibv_extend_warning("no available ops for query qp extend");
        return -EOPNOTSUPP;
    }

    /* 版本检查：query_qp 是 V2 新增功能，驱动版本必须 >= V2 */
    if (context->ops->version < IBV_EXTEND_DRIVER_VERSION_V2) {
        ibv_extend_warning("driver version %d does not support query_qp (requires V2)", context->ops->version);
        return -EOPNOTSUPP;
    }

    if (attr_mask & ~(IBV_QP_ATTR_EXTEND_MAX - 1)) {
        ibv_extend_warning("attr_mask 0x%x is not supported", attr_mask);
        return -EOPNOTSUPP;
    }

    if (context->ops->query_qp == NULL) {
        ibv_extend_warning("query_qp is not supported");
        return -EOPNOTSUPP;
    }

    // 调用底层驱动查询QP属性
    return context->ops->query_qp(context->context, attr, attr_mask);
}

/**
 * @brief 查询网卡支持的高阶RoCE特性
 * @param context 扩展上下文指针
 * @param qp QP指针
 * @param sl 服务等级
 * @param tc 流量控制
 * @param feature 返回支持的高阶RoCE特性
 * @return 0-成功，其他值-失败
 */
API_EXPORT int ibv_query_qp_supported_hyroce_feature(struct ibv_context_extend *context, struct ibv_qp *qp, uint32_t sl,
                                                     uint32_t tc, struct ibv_hyroce_feature *feature)
{
    if (context == NULL || qp == NULL || feature == NULL) {
        ibv_extend_warning("invalid parameter for query qp supported hyroce feature");
        return -EINVAL;
    }

    if (context->ops == NULL) {
        ibv_extend_warning("no available ops for query qp supported hyroce feature");
        return -EOPNOTSUPP;
    }

    /* 版本检查：高阶RoCE的qp_hyroce_feature 是 V3 新增功能，驱动版本必须 >= V3 */
    if (context->ops->version < IBV_EXTEND_DRIVER_VERSION_V3) {
        ibv_extend_warning("driver version %d does not support query qp hyroce feature (requires V3)",
                           context->ops->version);
        return -EOPNOTSUPP;
    }

    if (context->ops->query_qp_supported_hyroce_feature == NULL) {
        ibv_extend_warning("query_qp_supported_hyroce_feature is not supported");
        return -EOPNOTSUPP;
    }

    return context->ops->query_qp_supported_hyroce_feature(context->context, qp, sl, tc, feature);
}

/**
 * @brief 网卡协商高阶RoCE特性
 * @param context 扩展上下文指针
 * @param qp QP指针
 * @param input 输入的高阶RoCE特性
 * @param output 协商完成后高阶RoCE特性
 * @param need_more_nego 是否需要再次协商
 * @return 0-成功，其他值-失败
 */
API_EXPORT int ibv_nego_qp_hyroce_feature(struct ibv_context_extend *context, struct ibv_qp *qp,
                                          const struct ibv_hyroce_feature *input, struct ibv_hyroce_feature *output,
                                          uint32_t *need_more_nego)
{
    if (context == NULL || qp == NULL || input == NULL || output == NULL || need_more_nego == NULL) {
        ibv_extend_warning("invalid parameter for nego qp hyroce feature");
        return -EINVAL;
    }

    if (context->ops == NULL) {
        ibv_extend_warning("no available ops for nego qp hyroce feature");
        return -EOPNOTSUPP;
    }

    /* 版本检查：高阶RoCE的qp_hyroce_feature 是 V3 新增功能，驱动版本必须 >= V3 */
    if (context->ops->version < IBV_EXTEND_DRIVER_VERSION_V3) {
        ibv_extend_warning("driver version %d does not support nego qp hyroce feature (requires V3)",
                           context->ops->version);
        return -EOPNOTSUPP;
    }

    if (context->ops->nego_qp_hyroce_feature == NULL) {
        ibv_extend_warning("nego_qp_hyroce_feature is not supported");
        return -EOPNOTSUPP;
    }

    return context->ops->nego_qp_hyroce_feature(context->context, qp, input, output, need_more_nego);
}

/**
 * @brief 获取库版本号
 * @param major 主版本号输出参数（可为NULL）
 * @param minor 次版本号输出参数（可为NULL）
 * @param patch 修订号输出参数（可为NULL）
 * @return 版本字符串指针（静态字符串，无需释放）
 */
API_EXPORT const char *ibv_extend_get_version(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (major) {
        *major = IBV_EXTEND_VERSION_MAJOR;
    }
    if (minor) {
        *minor = IBV_EXTEND_VERSION_MINOR;
    }
    if (patch) {
        *patch = IBV_EXTEND_VERSION_PATCH;
    }

    return IBV_EXTEND_VERSION_STRING;
}

/**
 * @brief 检查版本兼容性
 * @param driver_major 驱动编译时的主版本号
 * @param driver_minor 驱动编译时的次版本号
 * @param driver_patch 驱动编译时的修订号
 * @return 0-兼容，-1-不兼容
 *
 * 此函数在运行时检查驱动编译时使用的头文件版本与当前库版本是否兼容。
 * 建议在驱动初始化时调用此函数进行版本检查。
 */
API_EXPORT int ibv_extend_check_version(uint32_t driver_major, uint32_t driver_minor, uint32_t driver_patch)
{
    /* 规则1: 主版本号必须完全一致 */
    if (driver_major != IBV_EXTEND_VERSION_MAJOR) {
        (void)fprintf(stderr,
                      OUTPFX "Error: Version check failed: major version mismatch "
                             "(driver: %u, library: %u)\n",
                      driver_major, IBV_EXTEND_VERSION_MAJOR);
        return -1;
    }

    /* 规则2: 驱动的次版本号必须 <= 库的次版本号 */
    if (driver_minor > IBV_EXTEND_VERSION_MINOR) {
        (void)fprintf(stderr,
                      OUTPFX "Error: Version check failed: driver minor version newer than library "
                             "(driver: %u.%u.%u, library: %u.%u.%u)\n",
                      driver_major, driver_minor, driver_patch, IBV_EXTEND_VERSION_MAJOR, IBV_EXTEND_VERSION_MINOR,
                      IBV_EXTEND_VERSION_PATCH);
        return -1;
    }

    /* 规则3: 如果次版本号相同，驱动的修订号必须 <= 库的修订号 */
    if (driver_minor == IBV_EXTEND_VERSION_MINOR && driver_patch > IBV_EXTEND_VERSION_PATCH) {
        (void)fprintf(stderr,
                      OUTPFX "Error: Version check failed: driver patch version newer than library "
                             "(driver: %u.%u.%u, library: %u.%u.%u)\n",
                      driver_major, driver_minor, driver_patch, IBV_EXTEND_VERSION_MAJOR, IBV_EXTEND_VERSION_MINOR,
                      IBV_EXTEND_VERSION_PATCH);
        return -1;
    }

    /* 版本兼容 */
    return 0;
}

/**
 * @brief 清理扩展驱动资源
 *
 * 此函数释放在运行过程中未释放的资源
 * 通过 __attribute__((destructor)) 实现自动清理，应用无需手动调用。
 */
static void ibv_extend_cleanup(void)
{
    struct ibv_extend_driver *driver, *next_driver;
    struct ibv_extend_driver_name *name, *next_name;

    /* 释放扩展驱动列表 */
    list_for_each_safe(&extend_driver_list, driver, next_driver, entry)
    {
        list_del(&driver->entry);
        free(driver);
    }

    /* 释放驱动名称列表（如果还有残留） */
    list_for_each_safe(&extend_driver_name_list, name, next_name, entry)
    {
        list_del(&name->entry);
        free(name->ext_name);
        free(name);
    }
}

/* 使用 destructor 属性在库卸载或程序退出时自动清理资源 */
__attribute__((destructor)) static void ibv_extend_destructor(void)
{
    ibv_extend_cleanup();
}
