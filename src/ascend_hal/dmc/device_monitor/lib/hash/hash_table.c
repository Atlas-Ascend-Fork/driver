/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hash_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ascend_inpackage_hal.h"
#include "securec.h"
#include "dev_mon_log.h"

#define TABLE_SIZE 1024

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

/* element of the hash table's chain list */
struct kv {
    struct kv *next;
    char *tag;
    void *value;
    void (*free_value)(void *);
};

/* hash_table */
struct hash_table {
    struct kv **table;
};

/* constructor of struct kv */
static void init_kv(struct kv *kv)
{
    kv->next = NULL;
    kv->tag = NULL;
    kv->value = NULL;
    kv->free_value = NULL;
}
/* destructor of struct kv */
static void free_kv(struct kv *kv)
{
    if (kv != NULL) {
        if (kv->free_value != NULL) {
            kv->free_value(kv->value);
        }

        free(kv->tag);
        kv->tag = NULL;
        free(kv);
        kv = NULL;
    }
}
/* the classic Times33 hash function */
STATIC unsigned int hash_33(const char *tag)
{
    const char *tag_temp = tag;
    unsigned int hash = 0;

    while (*tag_temp != 0) {
        hash = (hash << 5) + (unsigned char)hash + (unsigned char)*tag_temp++; // 5 hash left offset
    }

    return hash;
}

/* new a hash_table instance */
hash_table *hash_table_new(void)
{
    int ret;
    hash_table *ht = malloc(sizeof(hash_table));

    if (ht == NULL) {
        return NULL;
    }

    ret = memset_s((void *)ht, sizeof(hash_table), 0, sizeof(hash_table));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail!\n");
        free(ht);
        ht = NULL;
        return ht;
    }

    ht->table = malloc(sizeof(struct kv *) * TABLE_SIZE);

    if (ht->table == NULL) {
        free(ht);
        ht = NULL;
        return NULL;
    }

    ret = memset_s(ht->table, sizeof(struct kv *) * TABLE_SIZE, 0, sizeof(struct kv *) * TABLE_SIZE);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, free(ht->table); ht->table = NULL; free(ht); ht = NULL;
                                DEV_MON_ERR("memset_s error\n"));
    return ht;
}
/* delete a hash_table instance */
void hash_table_delete(hash_table *ht)
{
    if (ht == NULL) {
        return;
    }

    if (ht->table != NULL) {
        int i = 0;
        /*lint -e409*/
        for (i = 0; i < TABLE_SIZE; i++) {
            struct kv *p = ht->table[i];
            struct kv *q = NULL;

            while (p != NULL) {
                q = p->next;
                free_kv(p);
                p = q;
            }
        }

        free(ht->table);
        ht->table = NULL;
    }

    free(ht);
    ht = NULL;
}

STATIC void hash_table_put2_tag_stored(struct kv **p, struct kv **prep, const char *tag, void *value,
                                       void (*free_value)(void *))
{
    while ((*p) != NULL) { /* if tag is already stored, update its value */
        if (strcmp((*p)->tag, tag) == 0) {
            if ((*p)->free_value != NULL) {
                (*p)->free_value((*p)->value);
            }

            (*p)->value = (void *)value;
            (*p)->free_value = free_value;
            break;
        }

        (*prep) = (*p);
        (*p) = (*p)->next;
    }
}

/* if tag has not been stored, then add it */
STATIC int hash_table_put2_tag_not_stored(hash_table *ht, struct kv **prep, const char *tag, void *value,
                                          void (*free_value)(void *))
{
    char *kstr = NULL;
    struct kv *kv = NULL;
    int ret, i;

    i = hash_33(tag) % TABLE_SIZE;
    kstr = malloc(strlen(tag) + 1);
    if (kstr == NULL) {
        DEV_MON_ERR("malloc fail!\n");
        return -1;
    }

    ret = memset_s((void *)kstr, strlen(tag) + 1, 0, strlen(tag) + 1);
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail!\n");
        goto out_2;
    }

    kv = malloc(sizeof(struct kv));
    if (kv == NULL) {
        ret = errno;
        DEV_MON_ERR("malloc fail!\n");
        goto out_2;
    }

    ret = memset_s((void *)kv, sizeof(struct kv), 0, sizeof(struct kv));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail!\n");
        goto out_1;
    }

    init_kv(kv);
    kv->next = NULL;
    ret = strcpy_s(kstr, strlen(tag) + 1, tag);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(kstr); kstr = NULL; free(kv); kv = NULL;
                                DEV_MON_ERR("strcpy_s error\n"));
    kv->tag = kstr;
    kv->value = (void *)value;
    kv->free_value = free_value;

    if ((*prep) == NULL) {
        ht->table[i] = kv;
    } else {
        (*prep)->next = kv;
    }
    return 0;
out_1:
    free(kv);
    kv = NULL;
out_2:
    free(kstr);
    kstr = NULL;
    return ret;
}

/* insert or update a value indexed by tag */
int hash_table_put2(hash_table *ht, const char *tag, void *value, void (*free_value)(void *))
{
    int i = hash_33(tag) % TABLE_SIZE;
    struct kv *p = ht->table[i];
    struct kv *prep = p;
    int ret = 0;

    hash_table_put2_tag_stored(&p, &prep, tag, value, free_value);
    if (p == NULL) {
        ret = hash_table_put2_tag_not_stored(ht, &prep, tag, value, free_value);
        if (ret != 0) {
            DEV_MON_ERR("put tag fail %d\n", ret);
            return ret;
        }
    }
    return 0;
}

/* get a value indexed by tag */
void *hash_table_get(const hash_table *ht, const char *tag)
{
    int i = hash_33(tag) % TABLE_SIZE;
    struct kv *p = ht->table[i];

    while (p != NULL) {
        if (strcmp(tag, p->tag) == 0) {
            return p->value;
        }

        p = p->next;
    }

    return NULL;
}

/* remove a value indexed by tag */
void hash_table_rm(hash_table *ht, const char *tag)
{
    int i = hash_33(tag) % TABLE_SIZE;
    struct kv *p = ht->table[i];
    struct kv *prep = p;
    struct kv *tmp = NULL;

    while (p != NULL) {
        if (strcmp(tag, p->tag) == 0) {
            /* if prep is the head node */
            if (p == ht->table[i]) {
                /* if prep->next = NULL,means the node is the last node */
                if (p->next == NULL) {
                    ht->table[i] = NULL;
                } else {
                    ht->table[i] = p->next;
                    prep = ht->table[i];
                }
            } else {
                prep->next = p->next;
            }

            tmp = p;
            p = p->next;
            free_kv(tmp);
            tmp = NULL;
        } else {
            prep = p;
            p = p->next;
        }
    }
}

/* get a value indexed by tag */
void display_hash_table(const hash_table *ht)
{
    int i = 0;
    struct kv *p = NULL;

    for (i = 0; i < TABLE_SIZE; i++) {
        p = ht->table[i];

        while (p != NULL) {
            p = p->next;
        }
    }

    return;
}

/* count */
unsigned short count_hash_table(const hash_table *ht)
{
    int i = 0;
    unsigned short count = 0;
    struct kv *p = NULL;

    for (i = 0; i < TABLE_SIZE; i++) {
        p = ht->table[i];

        while (p != NULL) {
            count++;
            p = p->next;
        }
    }

    return count;
}

/* get all node value */
unsigned short get_hash_table_value(hash_table *ht, void **node_buff, unsigned short count)
{
    int i = 0;
    unsigned short count_tmp = 0;
    struct kv *p = NULL;

    for (i = 0; i < TABLE_SIZE; i++) {
        p = ht->table[i];
        /*lint +e409*/
        while ((p != NULL) && (count_tmp < count)) {
            node_buff[count_tmp++] = p->value;
            p = p->next;
        }
    }

    return count_tmp;
}

void *halHashTableNew(void)
{
    return (void *)hash_table_new();
}

void halHashTableDelete(void *ht)
{
    hash_table_delete((hash_table *)ht);
}

int halHashTablePut2(void *ht, const char *tag, void *value, void (*free_value)(void *))
{
    return hash_table_put2((hash_table *)ht, tag, value, free_value);
}

void *halHashTableGet(const void *ht, const char *tag)
{
    return hash_table_get((const hash_table *)ht, tag);
}

void halHashTableRm(void *ht, const char *tag)
{
    hash_table_rm((hash_table *)ht, tag);
}
