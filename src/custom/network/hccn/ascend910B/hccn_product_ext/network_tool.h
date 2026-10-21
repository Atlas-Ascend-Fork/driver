/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _NETWORK_TOOL_H_
#define _NETWORK_TOOL_H_

#ifndef _NT_CHANNEL_COMMON_DEFINITION_ // 该宏没有实际含义，仅方便vscode折叠用，以下简称 =FOLD=

struct nt_channel_ops {
    int cmd;
    int (*execute)(unsigned int device_id, void *data, unsigned int data_len);
};

// 建议把network_tool.h放在最后引用，以避免其他likely宏未做重入防护
#ifndef likely
#define likely(x) __builtin_expect((x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect((x), 0)
#endif

/*
CMD是一个unsigned int有32位，其中后24位是自定义的，前8位与通道相关，这里针对前8位进行说明：
00XX XXXX -> HOST
01XX XXXX -> DEVICE
10XX XXXX -> KERNEL
110X XXXX -> IMP(同步)
111X XXXX -> IMP(异步) // 注意NTC下的异步命令同时只能有一个正在处理，不像比如说scdr异步和
*/

#define IS_HOST_CMD(cmd) (((cmd) & 0xC0000000) == 0x00000000)
#define IS_DEVICE_CMD(cmd) (((cmd) & 0xC0000000) == 0x40000000)
#define IS_KERNEL_CMD(cmd) (((cmd) & 0xC0000000) == 0x80000000)
#define IS_IMP_CMD(cmd) (((cmd) & 0xC0000000) == 0xC0000000)
#define IS_IMP_SYNC_CMD(cmd) (((cmd) & 0xE0000000) == 0xC0000000)
#define IS_IMP_ASYNC_CMD(cmd) (((cmd) & 0xE0000000) == 0xE0000000)

#define NT_HOST_CMD(cmd) (((cmd) & 0xFFFFFF) | 0x00000000)
#define NT_DEVICE_CMD(cmd) (((cmd) & 0xFFFFFF) | 0x40000000)
#define NT_KERNEL_CMD(cmd) (((cmd) & 0xFFFFFF) | 0x80000000)
#define NT_IMP_SYNC_CMD(cmd) (((cmd) & 0xFFFFFF) | 0xC0000000)
#define NT_IMP_ASYNC_CMD(cmd) (((cmd) & 0xFFFFFF) | 0xE0000000)

#define GET_NT_CMD(cmd) ((cmd) & 0xFFFFFF)

#define KERNEL_IMP_BD_SIZE 24  // K->I通道上，一个BD数据大小为24
#define KERNEL_IMP_EXT_SIZE 12 // NTC中，K->I要额外带12B信息(dev_id cmd data_len)

#endif

#ifndef _NT_CHANNEL_HOST_LAYER_ // HOST层专属 =FOLD=
// 不想把 权限校验 和 环境校验 加在NTC中，感觉不够泛用，请在进入NTC前自行用这两个宏完成校验吧
#define NT_CHANNEL_ROOT_CHECK_RETURN        \
    do {                                    \
        if (hccn_check_usr_identify()) {    \
            return UDA_TOOL_SYS_NOT_ACCESS; \
        }                                   \
    } while (0)

#define NT_CHANNEL_PHY_CHECK_RETURN             \
    do {                                        \
        if (tool_is_virtual_machine()) {        \
            return UDA_DSMI_NOT_SUPPORT_VM_ERR; \
        }                                       \
    } while (0)

// 还差一个容器检查后面再补充
#endif

#ifndef _NT_CHANNEL_CUSTOM_FUNCTION_ // 支持自定义的功能配置 =FOLD=

#define NTC_HUYANG_FMEA_STUB // 例如后续计划打桩QILIAN的FMEA，支持测试验证他们设计的测试用例，相关功能重要但不进主线，就以宏隔离实现

#endif

// 在此统一注册NTC命令字
enum NT_CHANNEL_CMD {
    HUYANG_REG_OP = 1,
    HUYANG_FMEA_STUB = 2,
};

#ifndef _NT_CHANNEL_DATA_STRUCT_ // 数据结构一步到位 =FOLD=
// 我是觉得在这里定义数据结构到处引用，可以避免改了这头忘了那头的问题
// 没必要把头文件分那么明白吧，当然你也可以在通道两端自己定义数据结构，无需担心转换的cleancode问题因为通道全程void *
struct huyang_reg_op_t {
    unsigned char rw; // 0 read 1 write
    unsigned int reg;
    unsigned int val;
};

#endif

#endif