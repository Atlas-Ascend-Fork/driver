/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_DSMI_COMMON_H
#define SRC_DRIVER_NETWORK_DSMI_COMMON_H

#define DSMI_COMMON_SET_SEND_DATA(_trans_data, _cmd, _inbuf, _size_in, _outbuf, _size_out) \
    do {                                                                                   \
        (_trans_data).cmd = (_cmd);                                                        \
        (_trans_data).inbuf = (char *)(_inbuf);                                            \
        (_trans_data).size_in = (_size_in);                                                \
        (_trans_data).outbuf = (char *)(_outbuf);                                          \
        (_trans_data).size_out = (_size_out);                                              \
        (_trans_data).result = (-1);                                                       \
        (_trans_data).pid = ((_trans_data).size_out > MAX_CMD_PAYLOAD_LEN) ? getpid() : 0; \
    } while (0)

int dsmi_network_common_send(int logic_id, int port_id, struct ds_trans_data *data_info);

#endif // SRC_DRIVER_NETWORK_DSMI_COMMON_H