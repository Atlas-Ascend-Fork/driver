# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
target_sources(queue_obj PRIVATE
    queue_feature.c
)

if (${PRODUCT} IN_LIST support_ub)
target_sources(queue_obj PRIVATE
    que_uma.c
    que_urma.c
    que_jetty.c
    que_comm_agent.c
    que_comm_ctx.c
    que_comm_event.c
    que_comm_chan.c
    que_comm_thread.c
    que_ini_proc.c
    que_tgt_proc.c
    que_inter_dev_ini.c
    que_inter_dev_tgt.c
    que_mem_merge.c
)
endif ()
