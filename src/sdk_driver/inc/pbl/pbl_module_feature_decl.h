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

#ifndef PBL_MODULE_FEATURE_DECL_H
#define PBL_MODULE_FEATURE_DECL_H

#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "securec.h"

#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_common.h"
#include "pbl/pbl_module_feature_msg.h"
#include "pbl/pbl_urd_common.h"
#include "pbl/pbl_dev_identity.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_urd.h"
#include "ascend_dev_num.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

struct module_chip_features {
    const u64 *bitmap;
    u32 bitmap_words;
};

struct module_feature_descriptor {
    const char *module_name;
    u32 bitmap_len; /* BITS_TO_LONGS(max_fid) */
    const struct module_chip_features *const *chip_table;
    u32 chip_table_size;
    u32 *dev_chip_map; /* g_<module>_dev_chip_map[ASCEND_DEV_MAX_NUM] */
    feature_handler handler;
};

const struct module_chip_features *module_get_chip_features(const struct module_chip_features *const *table,
                                                            u32 table_size, u32 chip_type);
u32 module_get_feature_bitmap(const struct module_feature_descriptor *desc, u32 dev_id, u64 *out, u32 out_words);
u32 module_get_feature_bitmap_batch(const struct module_feature_descriptor *desc, u32 dev_num, u64 *out,
                                    u32 out_words_per_dev);
bool module_has_feature(const struct module_feature_descriptor *desc, u32 dev_id, u32 feature_id);

/*
 * Example:
 *   BEGIN_MODULE_FEATURE_DECLARATION(pbl, PBL_FEATURE_MAX)
 *       MODULE_FEATURE_MAP(HISI_CLOUD_V4, pbl_features_cloud_v4)
 *       MODULE_FEATURE_MAP(HISI_CLOUD_V5, pbl_features_cloud_v5)
 *   MODULE_FEATURE_TABLE_END(pbl)
 *   MODULE_FEATURE_DESCRIPTOR(pbl, PBL_FEATURE_MAX)
 *   MODULE_FEATURE_HAS_FEATURE_DECL(pbl)
 *   MODULE_FEATURE_HAS_FEATURE_DEF(pbl)
 *   MODULE_FEATURE_INIT_BY_DEVID(pbl)
 *   MODULE_FEATURE_URD_HANDLER(pbl)
 *   MODULE_FEATURE_NOTIFIER(pbl)
 *   MODULE_FEATURE_CHECK_INIT(pbl)
 *   MODULE_FEATURE_CHECK_UNINIT(pbl)
 *
 *   DECLAER_FEATURE_AUTO_INIT(pbl_feature_check_init, FEATURE_LOADER_STAGE_5);
 *   DECLAER_FEATURE_AUTO_UNINIT(pbl_feature_check_uninit, FEATURE_LOADER_STAGE_5);
 *
 *   bool is_support = MODULE_FEATURE_HAS_FEATURE_CALL(pbl, 0, PBL_FEATURE_xxx);
 */

#define BEGIN_MODULE_FEATURE_DECLARATION(module, max_fid)     \
    STATIC u32 g_##module##_dev_chip_map[ASCEND_DEV_MAX_NUM]; \
    STATIC const struct module_chip_features *const g_##module##_chip_feature_table[HISI_CHIP_UNKNOWN] = {
#define MODULE_FEATURE_MAP(chip, feats)                            \
    [chip] = &(const struct module_chip_features){                 \
        .bitmap = (const u64 *)(feats),                            \
        .bitmap_words = (u32)(sizeof(feats) / sizeof((feats)[0])), \
    },

#define MODULE_FEATURE_TABLE_END(module)                      \
    }                                                         \
    ;                                                         \
    STATIC void module##_dev_chip_map_init(void)              \
    {                                                         \
        u32 i;                                                \
        for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {            \
            g_##module##_dev_chip_map[i] = HISI_CHIP_UNKNOWN; \
        }                                                     \
    }

#define MODULE_FEATURE_DESCRIPTOR(module, max_fid)                                                 \
    STATIC s32 module##_get_features(void *feature, char *in, u32 in_len, char *out, u32 out_len); \
    STATIC struct module_feature_descriptor g_##module##_descriptor = {                            \
        .module_name = #module,                                                                    \
        .bitmap_len = BITS_TO_LONGS(max_fid),                                                      \
        .chip_table = g_##module##_chip_feature_table,                                             \
        .chip_table_size = (u32)(sizeof(g_##module##_chip_feature_table) /                         \
                                 sizeof(g_##module##_chip_feature_table[0])),                      \
        .dev_chip_map = g_##module##_dev_chip_map,                                                 \
        .handler = module##_get_features,                                                          \
    };

#define MODULE_FEATURE_HAS_FEATURE_DECL(prefix) \
    bool prefix##_has_feature(unsigned int dev_id, enum prefix##_feature_id feature);

#define MODULE_FEATURE_HAS_FEATURE_DEF(prefix)                                       \
    bool prefix##_has_feature(unsigned int dev_id, enum prefix##_feature_id feature) \
    {                                                                                \
        return module_has_feature(&g_##prefix##_descriptor, dev_id, (u32)feature);   \
    }

#define MODULE_FEATURE_HAS_FEATURE_CALL(prefix, dev_id, feature_id) prefix##_has_feature((dev_id), (feature_id))

#define MODULE_FEATURE_INIT_BY_DEVID(module)                                                                         \
    STATIC int module##_feature_init_by_devid(u32 udevid)                                                            \
    {                                                                                                                \
        u32 chip_type;                                                                                               \
        const struct module_chip_features *feat = NULL;                                                              \
        if (udevid >= ASCEND_DEV_MAX_NUM) {                                                                          \
            return -EINVAL;                                                                                          \
        }                                                                                                            \
        chip_type = uda_get_chip_type(udevid);                                                                       \
        if (chip_type == HISI_CHIP_UNKNOWN) {                                                                        \
            return -ENOSYS;                                                                                          \
        }                                                                                                            \
        feat = module_get_chip_features(                                                                             \
            g_##module##_chip_feature_table,                                                                         \
            (u32)(sizeof(g_##module##_chip_feature_table) / sizeof(g_##module##_chip_feature_table[0])), chip_type); \
        if (feat == NULL) {                                                                                          \
            return -ENOSYS;                                                                                          \
        }                                                                                                            \
        g_##module##_dev_chip_map[udevid] = chip_type;                                                               \
        return 0;                                                                                                    \
    }

#define MODULE_FEATURE_URD_HANDLER(module)                                                                 \
    STATIC s32 module##_get_features(void *feature, char *in, u32 in_len, char *out, u32 out_len)          \
    {                                                                                                      \
        struct pbl_module_feature_arg *arg = (struct pbl_module_feature_arg *)in;                          \
        u64 *kern_bitmap = NULL;                                                                           \
        u32 per_dev_words, per_dev_fill, bitmap_bytes, dev_num;                                            \
        s32 ret = -EINVAL;                                                                                 \
                                                                                                           \
        (void)feature;                                                                                     \
        (void)out;                                                                                         \
        (void)out_len;                                                                                     \
                                                                                                           \
        if (in == NULL || in_len < sizeof(struct pbl_module_feature_arg)) {                                \
            return -EINVAL;                                                                                \
        }                                                                                                  \
                                                                                                           \
        if (arg->bitmap_buff == NULL || arg->per_dev_bitmap_len == NULL || arg->bitmap_buff_len == 0) {    \
            return -EINVAL;                                                                                \
        }                                                                                                  \
        if (arg->max_dev_num == 0 || arg->max_dev_num > ASCEND_DEV_MAX_NUM) {                              \
            return -EINVAL;                                                                                \
        }                                                                                                  \
                                                                                                           \
        if (arg->dev_id == PBL_QUERY_FEATURE_ALL_DEV) {                                                    \
            dev_num = arg->max_dev_num;                                                                    \
            per_dev_words = arg->bitmap_buff_len / dev_num;                                                \
            if (per_dev_words == 0 || per_dev_words > PBL_QUERY_FEATURE_BITMAP_MAX_WORDS) {                \
                return -EINVAL;                                                                            \
            }                                                                                              \
            bitmap_bytes = dev_num * per_dev_words * (u32)sizeof(u64);                                     \
            kern_bitmap = (u64 *)ka_mm_vzalloc(bitmap_bytes);                                              \
            if (kern_bitmap == NULL) {                                                                     \
                return -ENOMEM;                                                                            \
            }                                                                                              \
            per_dev_fill = module_get_feature_bitmap_batch(&g_##module##_descriptor, dev_num, kern_bitmap, \
                                                           per_dev_words);                                 \
        } else if (arg->dev_id < ASCEND_DEV_MAX_NUM) {                                                     \
            per_dev_words = arg->bitmap_buff_len;                                                          \
            if (per_dev_words == 0 || per_dev_words > PBL_QUERY_FEATURE_BITMAP_MAX_WORDS) {                \
                return -EINVAL;                                                                            \
            }                                                                                              \
            bitmap_bytes = per_dev_words * (u32)sizeof(u64);                                               \
            kern_bitmap = (u64 *)ka_mm_vzalloc(bitmap_bytes);                                              \
            if (kern_bitmap == NULL) {                                                                     \
                return -ENOMEM;                                                                            \
            }                                                                                              \
            per_dev_fill = module_get_feature_bitmap(&g_##module##_descriptor, arg->dev_id, kern_bitmap,   \
                                                     per_dev_words);                                       \
        } else {                                                                                           \
            return -EINVAL;                                                                                \
        }                                                                                                  \
                                                                                                           \
        if (ka_base_copy_to_user(arg->bitmap_buff, kern_bitmap, bitmap_bytes) != 0) {                      \
            ret = -EFAULT;                                                                                 \
            goto out;                                                                                      \
        }                                                                                                  \
        if (ka_base_copy_to_user(arg->per_dev_bitmap_len, &per_dev_fill, sizeof(u32)) != 0) {              \
            ret = -EFAULT;                                                                                 \
            goto out;                                                                                      \
        }                                                                                                  \
        ret = 0;                                                                                           \
    out:                                                                                                   \
        ka_mm_vfree(kern_bitmap);                                                                          \
        return ret;                                                                                        \
    }

#define MODULE_FEATURE_NOTIFIER(module)                                                   \
    STATIC int module##_common_notifier_func(u32 udevid, enum uda_notified_action action) \
    {                                                                                     \
        if (action == UDA_INIT) {                                                         \
            return module##_feature_init_by_devid(udevid);                                \
        }                                                                                 \
        return 0;                                                                         \
    }

#define MODULE_FEATURE_CHECK_INIT(module)                                                     \
    STATIC DMS_FEATURE_S module##_feature_cmd = {                                             \
        .owner_name = #module,                                                                \
        .main_cmd = DMS_MAIN_CMD_BASIC,                                                       \
        .sub_cmd = DMS_SUBCMD_GET_MODULE_FEATURE,                                             \
        .filter = #module,                                                                    \
        .privilege = DMS_SUPPORT_ALL_USER,                                                    \
        .handler_type = URD_HANDLER,                                                          \
        .handler = module##_get_features,                                                     \
    };                                                                                        \
    STATIC int module##_feature_check_init(void)                                              \
    {                                                                                         \
        int ret;                                                                              \
        struct uda_dev_type type;                                                             \
        module##_dev_chip_map_init();                                                         \
        uda_davinci_near_real_entity_type_pack(&type);                                        \
        ret = uda_notifier_register(#module, &type, UDA_PRI2, module##_common_notifier_func); \
        if (ret != 0) {                                                                       \
            return ret;                                                                       \
        }                                                                                     \
        return dms_feature_register(&module##_feature_cmd);                                   \
    }

#define MODULE_FEATURE_CHECK_UNINIT(module)                  \
    STATIC void module##_feature_check_uninit(void)          \
    {                                                        \
        struct uda_dev_type type;                            \
        (void)dms_feature_unregister(&module##_feature_cmd); \
        uda_davinci_near_real_entity_type_pack(&type);       \
        (void)uda_notifier_unregister(#module, &type);       \
        module##_dev_chip_map_init();                        \
    }

#endif /* PBL_MODULE_FEATURE_DECL_H */
