/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _DRV_USER_COMMON_H_
#define _DRV_USER_COMMON_H_

#include <stddef.h>

#ifndef TSDRV_UT
/* user list definition */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) {&(name), &(name)}

/*lint -e773 -e773*/
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
/*lint +e773 +e773*/

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

#ifndef container_of
#define container_of(ptr, type, member)                       \
    ({                                                        \
        const typeof(((type *)0)->member)(*__mptr) = (ptr);   \
        (type *)((uintptr_t)__mptr - offsetof(type, member)); \
    })
#endif

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

int drv_user_list_empty(const struct list_head *head);
void drv_user_list_add_tail(struct list_head *new_node, struct list_head *head);
void drv_user_list_add_head(struct list_head *new_node, struct list_head *head);
void drv_user_list_del(struct list_head *entry);

typedef void (*drv_list_handle_func)(struct list_head *node);

void drv_user_list_for_each(struct list_head *head, drv_list_handle_func func);
#endif

int drvGetRuntimeApiVer(void);

#endif
