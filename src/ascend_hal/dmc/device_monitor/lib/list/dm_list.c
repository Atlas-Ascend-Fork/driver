/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <errno.h>
#include "dm_list.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

/*****************************************************************************
 function    : __default_cmp
 description : Default comparison function
 input        const void *data1, const void *data2
 return      : 0 : equal to; 1 : greater than; -1 : less than
 called func : list_create()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC int __default_cmp(const void *data1, const void *data2)
{
    if (data1 == data2) {
        return 0;
    } else if (data1 > data2) {
        return 1;
    } else {
        return -1;
    }
}

/*****************************************************************************
 function    : __list_lock_init
 description : Creation of mutex for the list and initialization of its attributes: PTHREAD_MUTEX_RECURSIVE
 input       : mmMutex_t *mutex
 return      : int
 called func : list_create()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC int __list_lock_init(mmMutex_t *mutex)
{
    pthread_mutexattr_t mtx_attr = {{0}};
    int ret;
    ret = pthread_mutexattr_init(&mtx_attr);
    if (!ret) {
        /* setting mutex type attribute: allowing multiple locks before unlocking */
        ret = pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE);
        if (!ret) {
            ret = pthread_mutex_init(mutex, &mtx_attr);
        }
    }

    return ret;
}

/*
 *  Destroy the created mutex signal
 */
STATIC int __list_lock_destroy(mmMutex_t *mutex)
{
    return mmMutexDestroy(mutex);
}
/*
 *  Lock the mutex signal
 */
// lint -e454
STATIC int __list_lock(mmMutex_t *mutex)
{
    return mmMutexLock(mutex);
}
// lint +e454
/*
 *  Unlock the mutex signal
 */
// lint -e455
STATIC int __list_unlock(mmMutex_t *mutex)
{
    return mmMutexUnLock(mutex);
}
// lint +e455
/*****************************************************************************
 function    : list_create
 description : create a doubly linked list, and initialize the relevant variables
 input       : LIST_T **list, LIST_CMP_T cmp, LIST_DEL_T del
 return      : int; 0 : success; >0 : fail
 calling func: __list_lock_init()
 called func : ipmi_init()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
int list_create(LIST_T **list, LIST_CMP_T cmp, LIST_DEL_T del)
{
    LIST_T *ls = NULL;
    int ret;

    if (list == NULL) {
        return EINVAL;
    }

    ls = malloc(sizeof(LIST_T));
    if (ls == NULL) {
        return ENOMEM;
    }

    ret = memset_s((void *)ls, sizeof(LIST_T), 0, sizeof(LIST_T));
    if (ret != 0) {
        free(ls);
        ls = NULL;
        return ret;
    }

    /* initialize the mutex and its type attributes, allowing multiple locks before unlocking */
    ret = __list_lock_init(&ls->mutex);
    if (ret != 0) {
        free(ls);
        *list = NULL;
        return ret;
    }

    ls->lock = __list_lock;
    ls->unlock = __list_unlock;
    ls->size = 0; /* the list is currently empty, with a size of 0 */
    ls->head.next = &ls->head;
    ls->head.prev = &ls->head;
    ls->head.list = (void *)ls;
    ls->cmp = (cmp != NULL) ? cmp : __default_cmp;
    ls->del = del;
    *list = ls;
    return 0;
}

STATIC void __node_remove(LIST_NODE_T *node)
{
    LIST_T *list = (LIST_T *)node->list;
    node->next->prev = node->prev;
    node->prev->next = node->next;

    list->size--;
}

STATIC void __node_item_free(LIST_NODE_T *node)
{
    LIST_T *list = (LIST_T *)node->list;
    LIST_DEL_T del = list->del; /* the del method of list (function handle) */

    if (del != NULL) {
        del(node->item);
        node->item = NULL;
    }
}

STATIC void __node_free(LIST_NODE_T *node)
{
    if (node == NULL) {
        return;
    }

    free(node);
    node = NULL;
}

/*
 *  Remove a node from the list
 */
STATIC void __node_del(LIST_NODE_T *node)
{
    __node_remove(node);

    __node_item_free(node);

    __node_free(node);
}

/*****************************************************************************
 function    : list_destroy
 description : Destroy a doubly linked list
 input       : LIST_T *list
 return      : NULL
 calling func: __list_lock_destroy()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
void list_destroy(LIST_T *list)
{
    LIST_NODE_T *node = NULL;
    LIST_NODE_T *next = NULL;

    if ((list == NULL) || (list->lock == NULL) || (list->unlock == NULL)) {
        return;
    }

    /* release by level: first, the item of the node, then the node itself, and finally the list */
    (void)list->lock(&list->mutex);
    node = list->head.next;

    while (node != &list->head) { /* not the last node (the last node points to the head node) */
        next = node->next;
        __node_del(node);
        node = next;
    }

    (void)list->unlock(&list->mutex);

    /* destroying mutex mutex */
    (void)__list_lock_destroy(&list->mutex);
    free(list);
    list = NULL;
    return;
}

/*
 *  list size, i.e., the number of nodes
 */
int list_size(LIST_T *list)
{
    if (list == NULL) {
        return 0;
    }

    return list->size;
}

/*
 *  Determine if the list is empty
 *  A linked list is empty if the next node of the head node points to the head node
 */
bool list_is_empty(LIST_T *list)
{
    if ((list == NULL) || (list->head.next == &list->head)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 *  Return the item content of the node (e.g., entry in a poller list)
 */
void *list_to_item(LIST_NODE_T *node)
{
    if (node != NULL) {
        return node->item;
    } else {
        return NULL;
    }
}

/*****************************************************************************
 function    : list_first_item
 description : Return the item of the first node in the linked list
 input       : LIST_T *list
 return      : void *
 calling func: __list_lock_destroy()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
void *list_first_item(LIST_T *list)
{
    LIST_NODE_T *node = NULL;

    /* pointer validity check */
    if (list == NULL) {
        return NULL;
    }

    /* check whether the list is empty */
    if (list_is_empty(list)) {
        return NULL;
    } else {
        node = list->head.next;
        return node->item;
    }
}

/*****************************************************************************
 function    : list_last_item
 description : Return the item of the last node in the linked list
 input       : LIST_T *list
 return      : void *
 calling func: list_is_empty()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
void *list_last_item(LIST_T *list)
{
    LIST_NODE_T *node = NULL;

    if (list == NULL) {
        return NULL;
    }

    if (list_is_empty(list)) {
        return NULL;
    } else {
        node = list->head.prev;
        return node->item;
    }
}

/*****************************************************************************
 function    : __list_node_init
 description : Initialize the newly created node and add the item that the user wants to mount to the node
 input       : LIST_T *list, LIST_NODE_T **new_node, void *item
 output      : the newly created node is returned to the caller via the LIST_NODE_T **new_node pointer
 return      : 0 : success, >0 : fail

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC int __list_node_init(LIST_T *list, LIST_NODE_T **new_node, void *item)
{
    int ret;
    *new_node = (LIST_NODE_T *)malloc(sizeof(LIST_NODE_T));

    if (*new_node == NULL) {
        /* alloc memory failed */
        return ENOMEM;
    }

    ret = memset_s((void *)*new_node, sizeof(LIST_NODE_T), 0, sizeof(LIST_NODE_T));
    if (ret != 0) {
        free(*new_node);
        *new_node = NULL;
        return ret;
    }

    (*new_node)->list = (void *)list;
    (*new_node)->item = (void *)item;
    (*new_node)->prev = NULL;
    (*new_node)->next = NULL;
    (*new_node)->removed = 0;
    (*new_node)->ref_cnt = 1; /* the newly created node is referenced for the first time */
    return 0;
}

/*
 *  Function to add a node to a doubly linked list (with prev and next pointers added between nodes)
 */
STATIC void _list_add(LIST_NODE_T *node, LIST_NODE_T *prev, LIST_NODE_T *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
    return;
}

/*****************************************************************************
 function    : __list_nth
 description : Find the Nth node and return it
 input       : LIST_T *list, INT32 n
 return      : LIST_NODE_T *
 called func : list_insert_pos()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC LIST_NODE_T *__list_nth(LIST_T *list, int n)
{
    int n_temp = n;
    LIST_NODE_T *node = NULL;
    node = list->head.next; /* the first node in the linked list */

    while ((n_temp-- > 0) && (node != &list->head)) {
        node = node->next;
    }

    if (node == &list->head) {
        return NULL;
    } else {
        return node; /* find the Nth node and return it */
    }
}

/*****************************************************************************
 function    : list_insert_after
 description : Add a new node after the node in the list table (initialize the new node and attach the user's item)
 input       : LIST_T *list, LIST_NODE_T *node, void *item
 return      : int
 calling func: __list_node_init()/_list_add()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC int list_insert_after(LIST_T *list, LIST_NODE_T *node, void *item)
{
    LIST_NODE_T *new_node = NULL;
    int ret;

    if ((list == NULL) || (node == NULL) || (item == NULL)) {
        /* invalid list or node */
        return EINVAL;
    }

    /* initialize the newly created node and add the item that the user wants to mount to the node */
    ret = __list_node_init(list, &new_node, item);
    if (ret != 0) {
        return ret;
    }

    /* lock before operating the list */
    (void)list->lock(&list->mutex);
    /* add new_node after node */
    _list_add(new_node, node, node->next);
    list->size++;
    (void)list->unlock(&list->mutex);
    return 0;
}

/*****************************************************************************
 function    : list_insert_before
 description : Add a new node before the node in the list table (initialize the new node and attach the user's item)
 input       : LIST_T *list, LIST_NODE_T *node, void *item
 return      : int
 calling func: __list_node_init()/_list_add()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC int list_insert_before(LIST_T *list, LIST_NODE_T *node, void *item)
{
    LIST_NODE_T *new_node = NULL;
    int ret;

    if ((list == NULL) || (node == NULL) || (item == NULL)) {
        /* invalid list or node */
        return EINVAL;
    }

    /* initialize the newly created node and add the item that the user wants to mount to the node */
    ret = __list_node_init(list, &new_node, item);
    if (ret != 0) {
        return ret;
    }

    (void)list->lock(&list->mutex);
    /* before loading new_node onto node (where node is the next node of the new node) */
    _list_add(new_node, node->prev, node);
    list->size++;
    (void)list->unlock(&list->mutex);
    return 0;
}

/*
 *  Node added at the end
 */
int list_append(LIST_T *list, void *item)
{
    return list_insert_before(list, &list->head, item);
}

/*
 *  When list_iter_next() traverses and processes node operations, the reference count is incremented by 1
 */
STATIC void __node_get(LIST_NODE_T *node)
{
    if (!node->removed) {
        node->ref_cnt++;
    }
    return;
}

/*
 *  Add a new node before the node in the list table, increment the reference count by 1
 *  The caller needs to manually decrement the reference count
 */
STATIC int list_insert_before_get_node(LIST_T *list, LIST_NODE_T *node, void *item, LIST_NODE_T **new_node_out)
{
    LIST_NODE_T *new_node = NULL;
    int ret;

    if ((list == NULL) || (node == NULL) || (item == NULL) || (new_node_out == NULL)) {
        /* invalid list or node */
        return EINVAL;
    }

    /* initialize the newly created node and add the item that the user wants to mount to the node */
    ret = __list_node_init(list, &new_node, item);
    if (ret != 0) {
        return ret;
    }

    (void)list->lock(&list->mutex);
    __node_get(new_node);
    /* before loading new_node onto node (where node is the next node of the new node) */
    _list_add(new_node, node->prev, node);
    list->size++;
    (void)list->unlock(&list->mutex);
    *new_node_out = new_node;
    return 0;
}

/*
 *  Node added at the end
 */
int list_append_get_node(LIST_T *list, void *item, LIST_NODE_T **new_node)
{
    return list_insert_before_get_node(list, &list->head, item, new_node);
}

/*
 *  Node added at the beginning
 */
int list_prepend(LIST_T *list, void *item)
{
    return list_insert_after(list, &list->head, item);
}

/*****************************************************************************
 function    : list_insert_pos
 description : Add a new node to a specified position in the list (initialize the new node and attach the user's item)
               Note: If the position exceeds the current number of nodes in the list, add it to the end
 input       : LIST_T *list, void *item, int pos
 return      : int
 calling func: __list_node_init()/_list_add()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
int list_insert_pos(LIST_T *list, void *item, int pos)
{
    LIST_NODE_T *node = NULL;
    LIST_NODE_T *new_node = NULL;
    int ret;

    if ((list == NULL) || (item == NULL)) {
        return EINVAL;
    }

    ret = __list_node_init(list, &new_node, item);
    if (ret != 0) {
        return ret;
    }

    /* lock before operating the list */
    (void)list->lock(&list->mutex);
    /* node that obtains the POS position */
    node = __list_nth(list, pos);
    if (node != NULL) {
        /* new node become the nth node */
        _list_add(new_node, node->prev, node);
    } else {
        /* append new node to list */
        _list_add(new_node, list->head.prev, &list->head);
    }

    list->size++;
    (void)list->unlock(&list->mutex);
    return 0;
}

/*****************************************************************************
 function    : list_insert_descend
 description : The items in the list are sorted in ascending order according to the custom_cmp rule.
               (Used for sorting the poller timer timeout nodes)
 input       : LIST_T *list, void *item, LIST_CMP_T custom_cmp
 return      : int
 calling func: __list_node_init()/_list_add()
 called func : poller_timer_add()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
int list_insert_ascend(LIST_T *list, void *item, LIST_CMP_T custom_cmp)
{
    LIST_NODE_T *node = NULL;
    LIST_NODE_T *new_node = NULL;
    LIST_CMP_T cmp = NULL;
    int ret;

    if ((list == NULL) || (item == NULL) || (list->lock == NULL) || (list->unlock == NULL)) {
        return EINVAL;
    }

    ret = __list_node_init(list, &new_node, item);
    if (ret != 0) {
        return ret;
    }

    cmp = custom_cmp ? custom_cmp : list->cmp;
    (void)list->lock(&list->mutex);
    node = list->head.prev; /* last node */

    while ((node != &list->head) && (cmp(node->item, item) > 0)) {
        node = node->prev; /* insert a larger node before a smaller node */
    }

    _list_add(new_node, node, node->next);
    list->size++;
    (void)list->unlock(&list->mutex);
    return 0;
}

/*****************************************************************************
 function    : list_insert_descend
 description : The items in the list are sorted in descending order according to the custom_cmp rule.
               (Used for sorting the poller timer timeout nodes)
 input       : LIST_T *list, void *item, LIST_CMP_T custom_cmp
 return      : int
 calling func: __list_node_init()/_list_add()
 called func : poller_timer_add()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
int list_insert_descend(LIST_T *list, void *item, LIST_CMP_T custom_cmp)
{
    LIST_NODE_T *node = NULL;
    LIST_NODE_T *new_node = NULL;
    LIST_CMP_T cmp;
    int ret;

    if ((list == NULL) || (item == NULL)) {
        return EINVAL;
    }

    ret = __list_node_init(list, &new_node, item);
    if (ret != 0) {
        return ret;
    }

    cmp = custom_cmp ? custom_cmp : list->cmp;
    (void)list->lock(&list->mutex);
    node = list->head.prev; /* last node */

    while ((node != &list->head) && (cmp(node->item, item) < 0)) {
        node = node->prev; /* insert a smaller node before a larger node */
    }

    _list_add(new_node, node, node->next);
    list->size++;
    (void)list->unlock(&list->mutex);
    return 0;
}

/*
 *  Handle node operations. Parameter remove is used for deletion; delete the node and decrease the reference count by 1
 */
STATIC void __node_put(LIST_NODE_T *node, int remove)
{
    if (remove != 0) {
        node->removed = remove;
    }

    node->ref_cnt--;
    if (node->ref_cnt <= 0) {
        __node_del(node);
    }
    return;
}

/*
 *  Handle node operation, reference count decreased by 1
 */
void list_node_put(LIST_T *list, LIST_NODE_T *node)
{
    if ((list == NULL) || (list->lock == NULL) || (list->unlock == NULL)) {
        return;
    }
    (void)list->lock(&list->mutex);
    __node_put(node, 0);
    (void)list->unlock(&list->mutex);
}

/*****************************************************************************
 function    : __internal_search
 description : According to the comparison criteria provided by the user, find the node through tag
 input       : LIST_T *list, void *tag, LIST_CMP_T custom_cmp
 return      : LIST_NODE_T *
 called func : list_remove_by_tag

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
STATIC LIST_NODE_T *__internal_search(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp)
{
    LIST_NODE_T *node = NULL;
    LIST_CMP_T cmp = NULL;
    cmp = (custom_cmp) ? custom_cmp : list->cmp;
    node = list->head.next;

    while (node != &list->head) {
        if (!node->removed) {
            /* find the node by comparing items through tags */
            if (cmp(node->item, tag) == 0) {
                return node;
            }
        }

        node = node->next;
    }

    return NULL;
}

/*
 *  Delete the node from the list
 */
int list_remove(LIST_T *list, LIST_NODE_T *node)
{
    if ((list == NULL) || (list->lock == NULL) || (list->unlock == NULL)) {
        return EINVAL;
    }

    if (node != NULL) {
        (void)list->lock(&list->mutex);
        __node_put(node, 1);
        (void)list->unlock(&list->mutex);
    }

    return 0;
}

/*****************************************************************************
 function    : list_remove_by_tag
 description : Delete nodes on a list based on keyword tags
 input       : LIST_T *list, void *tag, LIST_CMP_T custom_cmp
 return      : int
 calling func: __internal_search()/__node_put()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
int list_remove_by_tag(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp)
{
    LIST_NODE_T *node = NULL;

    if ((list == NULL) || (list->lock == NULL) || (list->unlock == NULL)) {
        return EINVAL;
    }

    (void)list->lock(&list->mutex);
    /* based on tag, find out node */
    node = __internal_search(list, tag, custom_cmp);
    if (node != NULL) {
        __node_put(node, 1);
    }

    (void)list->unlock(&list->mutex);
    return 0;
}

/*****************************************************************************
 function    : list_del_by_tag
 description : Remove and release nodes on a list based on keyword tags,
               but do not release the item resources within the nodes (in contrast to the list_remove_by_tag function)
 input       : LIST_T *list, void *tag, LIST_CMP_T custom_cmp
 return      : int
 calling func: NULL

 history     :
  1.date       : January 13, 2025
    author     : huawei
    content    : function creation
 *****************************************************************************/
int list_del_by_tag(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp)
{
    LIST_NODE_T *node = NULL;

    if ((list == NULL) || (list->lock == NULL) || (list->unlock == NULL)) {
        return EINVAL;
    }

    (void)list->lock(&list->mutex);
    /* based on tag, find out node */
    node = __internal_search(list, tag, custom_cmp);
    if (node != NULL) {
        node->removed = 1;
        node->ref_cnt--;
        if (node->ref_cnt <= 0) {
            __node_remove(node);
            __node_free(node);
        }
    }

    (void)list->unlock(&list->mutex);
    return 0;
}

LIST_NODE_T *list_search(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp)
{
    LIST_NODE_T *node = NULL;
    LIST_CMP_T cmp;
    cmp = (custom_cmp) ? custom_cmp : list->cmp;
    node = list->head.next;

    while (node != &list->head) {
        if (!node->removed) {
            /* find the node by comparing items through tags */
            if (cmp(node->item, tag) == 0) {
                return node;
            }
        }

        node = node->next;
    }

    return NULL;
}

/*
 *  Initialize traversal structure, i_cur is null
 */
STATIC void list_iter_init_node(LIST_T *list, LIST_ITER_T *iter, LIST_NODE_T *node)
{
    iter->i_list = list;
    iter->i_cur = node;

    if (node != NULL) {
        node->ref_cnt++; /* the number of times the node is referenced is incremented by 1 */
    }
}

/*
 *  Initialization before traversal, set as node reference counter,
 *  and used in conjunction with list_iter_destroy()
 */
void list_iter_init(LIST_T *list, LIST_ITER_T *iter)
{
    if ((list == NULL) || (iter == NULL)) {
        return;
    }

    list_iter_init_node(list, iter, NULL);
}

/*****************************************************************************
 function    : list_iter_next
 description : Traverse the list and return valid nodes. Note: When using nodes, handle reference counting.
 input       : LIST_ITER_T *iter
 return      : LIST_NODE_T *
 calling func: __node_put() / __node_get()
 called func : functions such as __poller_load_fds()

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
LIST_NODE_T *list_iter_next(LIST_ITER_T *iter)
{
    LIST_T *list = NULL;
    LIST_NODE_T *last = NULL;
    LIST_NODE_T *next = NULL;

    if ((iter == NULL) || (iter->i_list == NULL)) {
        return NULL;
    }

    list = iter->i_list;
    last = iter->i_cur;

    if ((list->lock == NULL) || (list->unlock == NULL)) {
        return NULL;
    }

    (void)list->lock(&list->mutex);

    if (last != NULL) {
        next = last->next;
        __node_put(last, 0); /* the last node is no longer operated on; reference count -1 */
    } else {
        next = list->head.next;
    }

    iter->i_cur = NULL;

    while (next != &list->head) {
        if (!next->removed) {
            __node_get(next); /* the next node needs to be operated; reference count +1 */
            iter->i_cur = next;
            break; /* if a valid node is obtained, return this node */
        }

        next = next->next;
    }

    (void)list->unlock(&list->mutex);
    return iter->i_cur;
}

/*
 *  Used in conjunction with list_iter_init()
 */
void list_iter_destroy(LIST_ITER_T *iter)
{
    LIST_T *list = NULL;

    if ((iter == NULL) || (iter->i_list == NULL)) {
        return;
    }

    list = iter->i_list;
    if ((list->lock == NULL) || (list->unlock == NULL)) {
        return;
    }

    (void)list->lock(&list->mutex);

    if (iter->i_cur != NULL) {
        __node_put(iter->i_cur, 0);
    }

    iter->i_cur = NULL;
    (void)list->unlock(&list->mutex);
    return;
}

/*****************************************************************************
 function    : list_iterate
 description : Traverse the linked list, operate on its elements with user-defined function
 input       : LIST_T *list, LIST_HNDL_T hndl, void *user_data, INT32 data_len
 return      : NULL
 calling func: list_iter_next()/ list_to_item()/ hndl - user-provided handle to the process
 called func : ipmi_cmd_register() /ipmi_add_evt_handler() and their corresponding unregistration functions

 history     :
  1.date       : January 10, 2013
    author     : huawei
    content    : function creation
 *****************************************************************************/
void list_iterate(LIST_T *list, LIST_HNDL_T hndl, void *user_data, int data_len)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    list_iter_init(list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        /* traverse through iter to find the item of the node, and process each one individually using hndl */
        hndl(list_to_item(node), user_data, data_len);
    }

    list_iter_destroy(&iter);
    return;
}
