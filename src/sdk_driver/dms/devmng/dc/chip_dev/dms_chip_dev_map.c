/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ascend_dev_num.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager.h"
#include "devdrv_manager_container.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "dms_chip_dev_map.h"

STATIC int dms_chip_dev_map_init(dms_chip_dev_map_t **map_info)
{
    if (map_info == NULL) {
        dms_err("Invalid parameter, map_info is null.\n");
        return -EINVAL;
    }

    *map_info = (dms_chip_dev_map_t *)ka_mm_kzalloc(sizeof(dms_chip_dev_map_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if ((*map_info) == NULL) {
        dms_err("Alloc memory for chip dev map structure fail.\n");
        return -ENOMEM;
    }
    KA_INIT_LIST_HEAD(&(*map_info)->chip_head);
    return 0;
}

STATIC int dms_get_random_number(dms_chip_dev_node_t *dev_node)
{
    int ret;

    ret = memset_s(dev_node->random_number, RANDOM_SIZE, 0, RANDOM_SIZE);
    if (ret != 0) {
        dms_err("Memset for random sign fail, ret=%d.\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_CHIP_DIE
    ret = devdrv_manager_get_random_from_dev_info(dev_node->dev_id, dev_node->random_number, RANDOM_SIZE);
    if (ret != 0) {
        dms_err("Get random from device info failed. (dev_id=%u; ret=%d)\n", dev_node->dev_id, ret);
        return ret;
    }
#else
    dev_node->random_number[0] = dev_node->dev_id / 2; /* 2 device assigned to 1 chip */
#endif

    return 0;
}

STATIC int dms_get_dev_num_and_list(unsigned int *dev_num, unsigned int *dev_id_list, unsigned int list_size)
{
    unsigned int i;

    *dev_num = devdrv_manager_get_devnum();
    if ((*dev_num > ASCEND_DEV_MAX_NUM) || (*dev_num == 0)) {
        dms_err("Invalid dev_num. (dev_num=%u; max=%u)\n", *dev_num, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }

    *dev_num = (list_size > *dev_num ? *dev_num : list_size);
    for (i = 0; i < *dev_num; i++) {
        dev_id_list[i] = i;
    }

    return 0;
}

STATIC int dms_get_all_dev_random_num(dms_chip_dev_node_t *dev_info, unsigned int *dev_id_list, unsigned int dev_num)
{
    unsigned int i;
    dms_chip_dev_node_t *dev_info_node = NULL;

    for (i = 0; i < dev_num; i++) {
        dev_info_node = dev_info + i;
        dev_info_node->dev_id = dev_id_list[i];

        if (dms_get_random_number(dev_info_node) != 0) {
            dms_err("Get random number fail.\n");
            return -EINVAL;
        }
    }

    return 0;
}

STATIC int dms_dev_list_init(dms_chip_dev_map_t *map_info)
{
    int ret;
    unsigned int i, j;
    dms_chip_dev_node_t *dev_node_cur = NULL;
    dms_chip_dev_node_t *dev_node_cmp = NULL;
    dms_chip_dev_node_t *dev_list_head = map_info->all_dev_info;

    map_info->chip_num = 0;
    for (i = 0; i < map_info->dev_num; i++) {
        dev_node_cur = dev_list_head + i;
        if (dev_node_cur->assign_to_chip_flag == ASSIGN_TO_CHIP) {
            continue;
        }

        for (j = i + 1; j < map_info->dev_num; j++) {
            dev_node_cmp = dev_list_head + j;
            ret = ka_base_memcmp(dev_node_cur->random_number, dev_node_cmp->random_number, RANDOM_SIZE);
            if (ret != 0) {
                continue;
            }

            /* if current node and compare node have the same random number, set the same chip_num */
            dev_node_cmp->assign_to_chip_flag = ASSIGN_TO_CHIP;
            dev_node_cmp->belong_to_chip = map_info->chip_num;
        }

        dev_node_cur->assign_to_chip_flag = ASSIGN_TO_CHIP;
        dev_node_cur->belong_to_chip = map_info->chip_num;
        map_info->chip_num++;

        if (map_info->chip_num > DEVDRV_MAX_CHIP_NUM) {
            dms_err("Chip node num invalid. (chip_num=%u; max=%u)\n", map_info->chip_num, DEVDRV_MAX_CHIP_NUM);
            return -EINVAL;
        }
    }

    /* dfx info */
    for (i = 0; i < map_info->dev_num; i++) {
        dev_node_cur = dev_list_head + i;
        dms_debug("dev[%u] chip_flag[%u] chip_id[%u].\n", dev_node_cur->dev_id, dev_node_cur->assign_to_chip_flag,
                  dev_node_cur->belong_to_chip);
    }

    return 0;
}

STATIC int dms_creat_chip_and_dev_linklist(dms_chip_dev_map_t *map_info)
{
    unsigned int i, j;
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n_dev = NULL;
    ka_list_head_t *pos_dev = NULL;
    dms_chip_node_t *chip_node = NULL;
    dms_chip_dev_node_t *dev_node = NULL;

    for (i = 0; i < map_info->chip_num; i++) {
        chip_node = (dms_chip_node_t *)ka_mm_kzalloc(sizeof(dms_chip_node_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (chip_node == NULL) {
            dms_err("Alloc memory for chip node fail.\n");
            return -ENOMEM;
        }

        ka_list_add_tail(&chip_node->chip_node_list, &map_info->chip_head);
        KA_INIT_LIST_HEAD(&chip_node->dev_head);
        chip_node->chip_id = i;

        /* devices has the same chip_num assign to the same chip */
        for (j = 0; j < map_info->dev_num; j++) {
            dev_node = map_info->all_dev_info + j;
            if ((dev_node->assign_to_chip_flag == ASSIGN_TO_CHIP) && (dev_node->belong_to_chip == i)) {
                ka_list_add_tail(&dev_node->dev_node_list, &chip_node->dev_head);
                chip_node->dev_num++;
            }
        }
    }

    /* dfx info */
    if (ka_list_empty_careful(&map_info->chip_head)) {
        dms_debug("Chip link list is empty.\n");
        return 0;
    }

    ka_list_for_each_safe(pos, n, &map_info->chip_head)
    {
        chip_node = ka_list_entry(pos, dms_chip_node_t, chip_node_list);
        if (ka_list_empty_careful(&chip_node->dev_head)) {
            dms_debug("Device link list is empty.\n");
            continue;
        }

        ka_list_for_each_safe(pos_dev, n_dev, &chip_node->dev_head)
        {
            dev_node = ka_list_entry(pos_dev, dms_chip_dev_node_t, dev_node_list);
            dms_debug("Dev[%u] belong to chip[%u].\n", dev_node->dev_id, dev_node->belong_to_chip);
        }
    }

    return 0;
}

void dms_chip_dev_map_resource_free(dms_chip_dev_map_t **map_info)
{
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    dms_chip_node_t *chip_node = NULL;
    dms_chip_dev_map_t *chip_dev_map = NULL;
    if ((map_info == NULL) || ((*map_info) == NULL)) {
        return;
    }
    chip_dev_map = *map_info;
    if (!ka_list_empty_careful(&chip_dev_map->chip_head)) {
        ka_list_for_each_safe(pos, n, &chip_dev_map->chip_head)
        {
            chip_node = ka_list_entry(pos, dms_chip_node_t, chip_node_list);
            ka_list_del(&chip_node->chip_node_list);
            ka_mm_kfree(chip_node);
            chip_node = NULL;
        }
    }

    if (chip_dev_map->all_dev_info != NULL) {
        ka_mm_kfree(chip_dev_map->all_dev_info);
        chip_dev_map->all_dev_info = NULL;
    }

    chip_dev_map->chip_num = 0;
    chip_dev_map->dev_num = 0;
    ka_mm_kfree(chip_dev_map);
    *map_info = NULL;
}

int dms_creat_chip_dev_map(dms_chip_dev_map_t *chip_dev_map)
{
    int ret;
    unsigned int dev_num;
    unsigned int *dev_id_list = NULL;
    dms_chip_dev_node_t *dev_node = NULL;

    /* 0 input parameter check */
    if (chip_dev_map == NULL) {
        dms_err("Input parameter chip_dev_map is NULL.\n");
        return -EINVAL;
    }

    /* 1 get device number and device id list */
    dev_id_list = (unsigned int *)ka_mm_kzalloc(sizeof(unsigned int) * ASCEND_DEV_MAX_NUM,
                                                KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (dev_id_list == NULL) {
        dms_err("Allocate memory for device list failed.\n");
        return -ENOMEM;
    }
    ret = dms_get_dev_num_and_list(&dev_num, dev_id_list, ASCEND_DEV_MAX_NUM);
    if (ret != 0) {
        ka_mm_kfree(dev_id_list);
        dev_id_list = NULL;
        dms_err("Get device list failed. (ret=%d)\n", ret);
        return ret;
    }

    /* 2 get random num of all device */
    dev_node = (dms_chip_dev_node_t *)ka_mm_kzalloc(dev_num * sizeof(dms_chip_dev_node_t),
                                                    KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (dev_node == NULL) {
        ka_mm_kfree(dev_id_list);
        dev_id_list = NULL;
        dms_err("Alloc memory for device fail.\n");
        return -ENOMEM;
    }

    chip_dev_map->dev_num = dev_num;
    chip_dev_map->all_dev_info = dev_node;

    ret = dms_get_all_dev_random_num(chip_dev_map->all_dev_info, dev_id_list, dev_num);
    if (ret != 0) {
        dms_err("devdrv_dev_random_map_init fail, ret=%d.\n", ret);
        goto FREE_EXIT;
    }

    /* 3 compute chip num for each device */
    ret = dms_dev_list_init(chip_dev_map);
    if (ret != 0) {
        dms_err("dms_dev_list_init fail. (ret=%d)\n", ret);
        goto FREE_EXIT;
    }

    /* 4 compute device list for each chip */
    ret = dms_creat_chip_and_dev_linklist(chip_dev_map);
    if (ret != 0) {
        dms_err("Failed to create chip and dev linklist. (ret=%d)\n", ret);
        goto FREE_EXIT;
    }

    ka_mm_kfree(dev_id_list);
    dev_id_list = NULL;
    return 0;

FREE_EXIT:
    ka_mm_kfree(dev_id_list);
    dev_id_list = NULL;
    return ret;
}

int dms_get_chip_count(int *count)
{
    int ret;
    dms_chip_dev_map_t *chip_dev_map = NULL;

    /* 1 malloc memory for chip_dev_map */
    ret = dms_chip_dev_map_init(&chip_dev_map);
    if (ret != 0) {
        dms_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }
    /* 2 build the relationship between device and chip */
    ret = dms_creat_chip_dev_map(chip_dev_map);
    if (ret != 0) {
        dms_err("Fail to create chip dev map. (ret=%d)\n", ret);
        dms_chip_dev_map_resource_free(&chip_dev_map);
        return ret;
    }

    /* 3 get chip count */
    *count = chip_dev_map->chip_num;
    dms_chip_dev_map_resource_free(&chip_dev_map);
    return 0;
}

int dms_get_chip_list(struct devdrv_chip_list *chip_info)
{
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    dms_chip_node_t *chip_node = NULL;
    dms_chip_dev_map_t *chip_dev_map = NULL;

    /* 1 malloc memory for chip_dev_map */
    if (dms_chip_dev_map_init(&chip_dev_map) != 0) {
        dms_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }

    /* 2 build the relationship between device and chip */
    if (dms_creat_chip_dev_map(chip_dev_map) != 0) {
        dms_err("Create chip device map failed.\n");
        dms_chip_dev_map_resource_free(&chip_dev_map);
        return -EINVAL;
    }

    if (!ka_list_empty_careful(&chip_dev_map->chip_head)) {
        int num = 0;
        ka_list_for_each_safe(pos, n, &chip_dev_map->chip_head)
        {
            chip_node = ka_list_entry(pos, dms_chip_node_t, chip_node_list);
            if (num >= VDAVINCI_VDEV_OFFSET) {
                dms_err("chip node num invalid. (num=%d; max=%d)\n", num, VDAVINCI_VDEV_OFFSET);
                dms_chip_dev_map_resource_free(&chip_dev_map);
                return -EINVAL;
            }

            chip_info->chip_list[num] = chip_node->chip_id;
            num++;
        }
    }

    chip_info->count = chip_dev_map->chip_num;
    dms_chip_dev_map_resource_free(&chip_dev_map);
    return 0;
}

int dms_get_device_from_chip(struct devdrv_chip_dev_list *chip_dev_list)
{
    int num = 0;
    ka_list_head_t *n_dev = NULL;
    ka_list_head_t *pos_dev = NULL;
    ka_list_head_t *n_chip = NULL;
    ka_list_head_t *pos_chip = NULL;
    dms_chip_node_t *chip_node = NULL;
    dms_chip_dev_node_t *dev_node = NULL;
    dms_chip_dev_map_t *chip_dev_map = NULL;

    /* 0 check input parameter */
    if (chip_dev_list->chip_id >= DEVDRV_MAX_CHIP_NUM || chip_dev_list->chip_id < 0) {
        dms_err("Chip id invalid. (chip_id=%d; valid range=[0,%d))\n", chip_dev_list->chip_id, DEVDRV_MAX_CHIP_NUM);
        return -EINVAL;
    }

    /* 1 malloc memory for chip_dev_map */
    if (dms_chip_dev_map_init(&chip_dev_map) != 0) {
        dms_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }

    /* 2 build the relationship between device and chip */
    if (dms_creat_chip_dev_map(chip_dev_map) != 0) {
        dms_err("Create chip device map failed.\n");
        goto ERROR_OUT;
    }

    if (ka_list_empty_careful(&chip_dev_map->chip_head)) {
        dms_err("Chip head list is empty.\n");
        goto ERROR_OUT;
    }
    ka_list_for_each_safe(pos_chip, n_chip, &chip_dev_map->chip_head)
    {
        chip_node = ka_list_entry(pos_chip, dms_chip_node_t, chip_node_list);
        if (chip_node->chip_id == chip_dev_list->chip_id) {
            if (ka_list_empty_careful(&chip_node->dev_head)) {
                dms_err("Device list is empty. (chip_id=%d)\n", chip_dev_list->chip_id);
                goto ERROR_OUT;
            }

            ka_list_for_each_safe(pos_dev, n_dev, &chip_node->dev_head)
            {
                if (num >= DEVDRV_MAX_DAVINCI_NUM) {
                    dms_err("chip node num invalid. (num=%d; max=%d)\n", num, DEVDRV_MAX_DAVINCI_NUM);
                    goto ERROR_OUT;
                }

                dev_node = ka_list_entry(pos_dev, dms_chip_dev_node_t, dev_node_list);
                chip_dev_list->dev_list[num] = dev_node->dev_id;
                num++;
            }
            chip_dev_list->count = num;
            dms_chip_dev_map_resource_free(&chip_dev_map);
            return 0;
        }
    }

ERROR_OUT:
    dms_chip_dev_map_resource_free(&chip_dev_map);
    return -EINVAL;
}

int dms_get_chip_from_device(struct devdrv_get_dev_chip_id *chip_from_dev)
{
    ka_list_head_t *n_dev = NULL;
    ka_list_head_t *pos_dev = NULL;
    ka_list_head_t *n_chip = NULL;
    ka_list_head_t *pos_chip = NULL;
    dms_chip_node_t *chip_node = NULL;
    dms_chip_dev_node_t *dev_node = NULL;
    dms_chip_dev_map_t *chip_dev_map = NULL;

    if (chip_from_dev->dev_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Dev_id invalid. (dev_id=%d; max=%u)\n", chip_from_dev->dev_id, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }

    /* 1 malloc memory for chip_dev_map */
    if (dms_chip_dev_map_init(&chip_dev_map) != 0) {
        dms_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }

    /* 2 build the relationship between device and chip */
    if (dms_creat_chip_dev_map(chip_dev_map) != 0) {
        dms_err("Create chip device map failed.\n");
        goto ERR_OUT;
    }

    /* 3 get chip_id by device_id */
    if (ka_list_empty_careful(&chip_dev_map->chip_head)) {
        dms_err("Chip head list is empty.\n");
        goto ERR_OUT;
    }
    ka_list_for_each_safe(pos_chip, n_chip, &chip_dev_map->chip_head)
    {
        chip_node = ka_list_entry(pos_chip, dms_chip_node_t, chip_node_list);
        if (ka_list_empty_careful(&chip_node->dev_head)) {
            dms_err("Device link list is empty.\n");
            goto ERR_OUT;
        }

        ka_list_for_each_safe(pos_dev, n_dev, &chip_node->dev_head)
        {
            dev_node = ka_list_entry(pos_dev, dms_chip_dev_node_t, dev_node_list);
            if (chip_from_dev->dev_id == dev_node->dev_id) {
                chip_from_dev->chip_id = chip_node->chip_id;
                dms_chip_dev_map_resource_free(&chip_dev_map);
                return 0;
            }
        }
    }

ERR_OUT:
    dms_chip_dev_map_resource_free(&chip_dev_map);
    return -EINVAL;
}