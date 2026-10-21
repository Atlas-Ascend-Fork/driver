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
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"

#include "securec.h"

#include "pbl/pbl_soc_res.h"
#include "soc_resmng.h"
#include "soc_resmng_log.h"
#include "pbl/pbl_soc_res_sync.h"

struct res_sync_info {
    u32 udevid;
    char *buf;
    u32 buf_len;
    u32 out_len;
    enum soc_res_sync_dir dir;
    soc_res_addr_encode func;
};

struct res_addr_sync {
    char name[SOC_RESMNG_MAX_NAME_LEN];
    u64 encode_addr;
    u64 len;
};

struct res_key_value_sync {
    char name[SOC_RESMNG_MAX_NAME_LEN];
    u64 value;
};

struct res_attr_sync {
    char name[SOC_RESMNG_MAX_NAME_LEN];
    u32 size;
    char attr[0];
};

struct res_mia_sync {
    int type;
    struct soc_mia_res_info_ex info;
};

struct res_dev_misc_sync {
    u32 vfg_num;
    u32 vf_num;
    u32 vfgid;
    u32 vfid;
    u32 ts_num;
};

#define HOST_IRQ_NAME_PRIFIX "host_irq_"
#define HOST_ADDR_NAME_PRIFIX "HOST_"
#define HOST_VF_ADDR_NAME_PRIFIX "VF_"

static bool soc_is_pura_key(char *key)
{
    if (ka_base_strstr(key, HOST_IRQ_NAME_PRIFIX) != NULL) {
        return false;
    }

    return true;
}

static bool soc_is_vf_addr(char *name)
{
    return (ka_base_strstr(name, HOST_VF_ADDR_NAME_PRIFIX) != NULL);
}

static bool soc_is_host_addr(char *name)
{
    return (ka_base_strstr(name, HOST_ADDR_NAME_PRIFIX) != NULL);
}

static char *soc_remove_addr_name_prifix(char *name)
{
    return soc_is_host_addr(name) ? (name + ka_base_strlen(HOST_ADDR_NAME_PRIFIX)) : name;
}

static void soc_pack_sync_info(struct res_sync_info *info, u32 udevid, char *buf, u32 len, soc_res_addr_encode func)
{
    info->udevid = udevid;
    info->buf = buf;
    info->buf_len = len;
    info->out_len = 0;
    info->func = func;
}

static int soc_key_value_extract(char *key, u64 value, void *priv)
{
    struct res_sync_info *info = (struct res_sync_info *)priv;
    struct res_key_value_sync *sync = (struct res_key_value_sync *)(info->buf + info->out_len);

    if (!soc_is_pura_key(key) && (info->dir == SOC_DIR_D2H)) {
        return 0;
    }

    if ((info->out_len + sizeof(*sync)) > info->buf_len) {
        soc_err("Buf is small. (buf_len=%u; key=%s)\n", info->buf_len, key);
        return -ENOMEM;
    }

    soc_res_name_copy(sync->name, key);

    sync->value = value;
    info->out_len += sizeof(*sync);

    return 0;
}

static int soc_attr_value_extract(const char *name, void *attr, u32 size, void *priv)
{
    struct res_sync_info *info = (struct res_sync_info *)priv;
    struct res_attr_sync *sync = NULL;
    int ret;

    if ((info->out_len + sizeof(*sync)) > info->buf_len) {
        soc_err("Buf is small. (buf_len=%u; name=%s)\n", info->buf_len, name);
        return -ENOMEM;
    }
    sync = (struct res_attr_sync *)(info->buf + info->out_len);

    soc_res_name_copy(sync->name, name);
    sync->size = size;

    if ((info->out_len + sizeof(*sync) + sync->size) > info->buf_len) {
        soc_err("Buf is small. (buf_len=%u; name=%s)\n", info->buf_len, name);
        return -ENOMEM;
    }

    ret = memcpy_s(sync->attr, sync->size, attr, size);
    if (ret != 0) {
        soc_err("Memcpy attr fail. (name=%s, size=0x%x)\n", sync->name, size);
        return ret;
    }

    info->out_len += sizeof(*sync) + size;

    return 0;
}

static struct res_addr_sync *soc_res_addr_find_sync(char *buf, u32 buf_len, char *name)
{
    u32 pos = 0;

    while (pos < buf_len) {
        struct res_addr_sync *sync = (struct res_addr_sync *)(buf + pos);
        if (ka_base_strcmp(sync->name, name) == 0) {
            return sync;
        }

        pos += sizeof(*sync);
    }

    return NULL;
}

static int soc_res_addr_extract(char *name, u64 addr, u64 len, void *priv)
{
    struct res_sync_info *info = (struct res_sync_info *)priv;
    struct res_addr_sync *sync = NULL;
    u64 encode_addr = addr;
    char *extract_name = NULL;

    bool addr_match = (((soc_is_vf_addr(name)) && (info->dir == SOC_DIR_P2V)) ||
                       ((!soc_is_vf_addr(name)) && (info->dir != SOC_DIR_P2V)));
    if (!addr_match) {
        return 0;
    }

    if (info->func != NULL) {
        if (info->func(info->udevid, addr, len, &encode_addr) != 0) {
            /* no need extract. not bar addr */
            return 0;
        }
        soc_info("Addr encoder. (name=%s; addr=%llx; encode_addr=%llx)\n", name, addr, encode_addr);
    }

    extract_name = (soc_is_vf_addr(name)) ? (name + ka_base_strlen(HOST_VF_ADDR_NAME_PRIFIX)) :
                                            soc_remove_addr_name_prifix(name);

    /* The reserved memory addresses of the host and device may be different.
       For example:
            TS_STARS_TOPIC_RSV_MEM 0xa
            HOST_TS_STARS_TOPIC_RSV_MEM 0xb
       the driver code in host and device use TS_STARS_TOPIC_RSV_MEM to get addr, we should sync
       HOST_TS_STARS_TOPIC_RSV_MEM to host and rename it to TS_STARS_TOPIC_RSV_MEM
       but all of the addresse is configed to bar, so can also be encoded,
       if TS_STARS_TOPIC_RSV_MEM is sync first, we refresh it; otherwise drop it */
    sync = soc_res_addr_find_sync(info->buf, info->out_len, extract_name);
    if (sync == NULL) {
        if ((info->out_len + sizeof(*sync)) > info->buf_len) {
            soc_err("Buf is small. (buf_len=%u; name=%s)\n", info->buf_len, name);
            return -ENOMEM;
        }

        sync = (struct res_addr_sync *)(info->buf + info->out_len);
        info->out_len += sizeof(*sync);
    } else {
        if (!soc_is_host_addr(name)) {
            soc_info("Device only addr, not sync. (name=%s; addr=%llx)\n", name, addr);
            return 0;
        }

        soc_info("Addr refresh. (name=%s; addr=%llx)\n", name, addr);
    }

    soc_res_name_copy(sync->name, (const char *)extract_name);
    sync->encode_addr = encode_addr;
    sync->len = len;

    return 0;
}

static int soc_res_irq_extract(enum soc_res_sync_scope scope, char *key, u64 value, void *priv)
{
    struct res_sync_info *info = (struct res_sync_info *)priv;
    struct res_sync_irq *sync = (struct res_sync_irq *)(info->buf + info->out_len);

    if (ka_base_strstr(key, HOST_IRQ_NAME_PRIFIX) == NULL) {
        return 0;
    }

    if ((info->out_len + sizeof(*sync)) > info->buf_len) {
        soc_err("Buf is small. (buf_len=%u; key=%s)\n", info->buf_len, key);
        return -ENOMEM;
    }

    sync->irq_type = (scope == SOC_DEV) ?
                         soc_resmng_get_dev_irq_type_by_name(key + ka_base_strlen(HOST_IRQ_NAME_PRIFIX)) :
                         soc_resmng_get_ts_irq_type_by_name(key + ka_base_strlen(HOST_IRQ_NAME_PRIFIX));
    sync->num = (u32)value;
    info->out_len += sizeof(*sync);

    return 0;
}

static int soc_res_ts_irq_extract(char *key, u64 value, void *priv)
{
    return soc_res_irq_extract(SOC_TS_SUBSYS, key, value, priv);
}

static int soc_res_dev_irq_extract(char *key, u64 value, void *priv)
{
    return soc_res_irq_extract(SOC_DEV, key, value, priv);
}

static bool soc_res_mia_skip_sync_to_host(enum soc_mia_res_type type)
{
    return ((type == MIA_STARS_H2D_JETTY) || (type == MIA_STARS_D2D_IN_BOARD_JETTY) ||
            (type == MIA_STARS_D2D_CROSS_BOARD_JETTY));
}

static int soc_res_mia_extract(enum soc_mia_res_type type, struct soc_mia_res_info_ex *mia_res, void *priv)
{
    struct res_sync_info *info = (struct res_sync_info *)priv;
    struct res_mia_sync *sync = (struct res_mia_sync *)(info->buf + info->out_len);

    if (mia_res->bitmap == 0) {
        return 0;
    }

    if (soc_res_mia_skip_sync_to_host(type)) {
        return 0;
    }

    if ((info->out_len + sizeof(*sync)) > info->buf_len) {
        soc_err("Buf is small. (buf_len=%u)\n", info->buf_len);
        return -ENOMEM;
    }

    sync->type = type;
    sync->info = *mia_res;
    info->out_len += sizeof(*sync);

    return 0;
}

static int soc_resmng_for_each_mia_res(struct res_inst_info *inst,
                                       int (*func)(enum soc_mia_res_type type, struct soc_mia_res_info_ex *mia_res,
                                                   void *priv),
                                       void *priv)
{
    enum soc_mia_res_type type;

    for (type = 0; type < MIA_MAX_RES_TYPE; type++) {
        struct soc_mia_res_info_ex mia_res;
        int ret = soc_resmng_get_mia_res_ex(inst, type, &mia_res);
        if (ret != 0) {
            soc_err("Get mia res failed. (type=%d; ret=%d)\n", type, ret);
            return ret;
        }

        ret = func(type, &mia_res, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int soc_resmng_dev_for_each_mia_res(
    u32 udevid, int (*func)(enum soc_mia_res_type type, struct soc_mia_res_info_ex *mia_res, void *priv), void *priv)
{
    enum soc_mia_res_type type;

    for (type = 0; type < MIA_MAX_RES_TYPE; type++) {
        struct soc_mia_res_info_ex mia_res;
        int ret = soc_resmng_dev_get_mia_res_ex(udevid, type, &mia_res);
        if (ret != 0) {
            soc_err("Get mia res failed. (type=%d; ret=%d)\n", type, ret);
            return ret;
        }

        ret = func(type, &mia_res, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int soc_resmng_dev_die_for_each_mia_res(u32 udevid, u32 die_id,
                                               int (*func)(enum soc_mia_res_type type,
                                                           struct soc_mia_res_info_ex *mia_res, void *priv),
                                               void *priv)
{
    enum soc_mia_res_type type;

    for (type = 0; type < MIA_MAX_RES_TYPE; type++) {
        struct soc_mia_res_info_ex mia_res;
        int ret = soc_resmng_dev_die_get_res(udevid, die_id, type, &mia_res);
        if (ret != 0) {
            soc_err("Get mia res failed. (type=%d; ret=%d)\n", type, ret);
            return ret;
        }

        ret = func(type, &mia_res, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int soc_subsys_res_extract(struct res_inst_info *inst, struct res_sync_target *target, char *buf, u32 *len,
                                  soc_res_addr_encode func)
{
    struct res_sync_info info;
    u32 buf_len = *len;
    u32 *out_len = len;
    int ret = 0;

    soc_pack_sync_info(&info, inst->devid, buf, buf_len, func);
    info.dir = target->dir;

    switch (target->type) {
        case SOC_KEY_VALUE_RES:
            ret = soc_resmng_for_each_key_value(inst, soc_key_value_extract, (void *)&info);
            break;
        case SOC_REG_ADDR:
        case SOC_RSV_MEM_ADDR: {
            u32 addr_type = (target->type == SOC_REG_ADDR) ? 0 : 1;
            ret = soc_resmng_for_each_res_addr(inst, addr_type, soc_res_addr_extract, (void *)&info);
            break;
        }
        case SOC_IRQ_RES:
            ret = soc_resmng_for_each_key_value(inst, soc_res_ts_irq_extract, (void *)&info);
            break;
        case SOC_MIA_RES:
            ret = soc_resmng_for_each_mia_res(inst, soc_res_mia_extract, (void *)&info);
            break;
        default:
            break;
    }

    if (ret == 0) {
        *out_len = info.out_len;
    }

    return ret;
}

static int soc_key_value_inject(struct res_inst_info *inst, char *buf, u32 buf_len)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_key_value_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_key_value_sync *sync = (struct res_key_value_sync *)(buf + pos);
        int ret = soc_resmng_set_key_value(inst, (const char *)sync->name, sync->value);
        if (ret != 0) {
            soc_err("Set failed. (name=%s; avalue=0x%llx; ret=%d)\n", sync->name, sync->value, ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_res_addr_inject(struct res_inst_info *inst, enum soc_res_sync_type type, char *buf, u32 buf_len,
                               soc_res_addr_decode func)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_addr_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_addr_sync *sync = (struct res_addr_sync *)(buf + pos);
        u64 decode_addr = sync->encode_addr;
        int ret;

        if (func != NULL) {
            ret = func(inst->devid, sync->encode_addr, sync->len, &decode_addr);
            if (ret != 0) {
                soc_err("Decode addr failed. (name=%s; encode_addr=0x%llx; len=0x%llx; ret=%d)\n", sync->name,
                        sync->encode_addr, sync->len, ret);
                return ret;
            }
        }

        if (type == SOC_REG_ADDR) {
            struct soc_reg_base_info io_base = {.io_base = (phys_addr_t)decode_addr, .io_base_size = (size_t)sync->len};
            ret = soc_resmng_set_reg_base(inst, (const char *)sync->name, &io_base);
        } else {
            struct soc_rsv_mem_info rsv_mem = {.rsv_mem = (phys_addr_t)decode_addr, .rsv_mem_size = (size_t)sync->len};
            ret = soc_resmng_set_rsv_mem(inst, (const char *)sync->name, &rsv_mem);
        }

        if (ret != 0) {
            soc_err("Set failed. (name=%s; addr=0x%llx; len=0x%llx; ret=%d)\n", sync->name, decode_addr, sync->len,
                    ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_res_mia_inject(struct res_inst_info *inst, char *buf, u32 buf_len)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_mia_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_mia_sync *sync = (struct res_mia_sync *)(buf + pos);
        int ret = soc_resmng_set_mia_res_ex(inst, sync->type, &sync->info);
        if (ret != 0) {
            soc_err("Set mia res failed. (type=%d; ret=%d)\n", sync->type, ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_subsys_res_inject(struct res_inst_info *inst, enum soc_res_sync_type type, char *buf, u32 buf_len,
                                 soc_res_addr_decode func)
{
    int ret;

    switch (type) {
        case SOC_KEY_VALUE_RES:
            ret = soc_key_value_inject(inst, buf, buf_len);
            break;
        case SOC_REG_ADDR:
        case SOC_RSV_MEM_ADDR:
            ret = soc_res_addr_inject(inst, type, buf, buf_len, func);
            break;
        case SOC_MIA_RES:
            ret = soc_res_mia_inject(inst, buf, buf_len);
            break;
        default:
            soc_err("Invalid para. (type=%d)\n", type);
            return -EINVAL;
    }

    return ret;
}

static int soc_dev_res_misc_extract(u32 udevid, char *buf, u32 buf_len, u32 *out_len)
{
    struct res_dev_misc_sync *sync = (struct res_dev_misc_sync *)buf;
    int ret;

    ret = soc_resmng_dev_get_mia_spec(udevid, &sync->vfg_num, &sync->vf_num);
    ret |= soc_resmng_dev_get_mia_base_info(udevid, &sync->vfgid, &sync->vfid);
    ret |= soc_resmng_subsys_get_num(udevid, TS_SUBSYS, &sync->ts_num);
    if (ret == 0) {
        *out_len = sizeof(*sync);
    }

    return ret;
}

static int soc_dev_res_extract(u32 udevid, struct res_sync_target *target, char *buf, u32 *len,
                               soc_res_addr_encode func)
{
    struct res_sync_info info;
    u32 buf_len = *len;
    u32 *out_len = len;
    int ret = 0;

    soc_pack_sync_info(&info, udevid, buf, buf_len, func);
    info.dir = target->dir;

    switch (target->type) {
        case SOC_MISC_RES:
            ret = soc_dev_res_misc_extract(udevid, buf, buf_len, out_len);
            info.out_len = *out_len;
            break;
        case SOC_KEY_VALUE_RES:
            ret = soc_resmng_dev_for_each_key_value(udevid, soc_key_value_extract, (void *)&info);
            break;
        case SOC_ATTR_RES:
            ret = soc_resmng_dev_for_each_attr(udevid, soc_attr_value_extract, (void *)&info);
            break;
        case SOC_REG_ADDR:
        case SOC_RSV_MEM_ADDR: {
            u32 addr_type = (target->type == SOC_REG_ADDR) ? 0 : 1;
            ret = soc_resmng_dev_for_each_res_addr(udevid, addr_type, soc_res_addr_extract, (void *)&info);
            break;
        }
        case SOC_IRQ_RES:
            ret = soc_resmng_dev_for_each_key_value(udevid, soc_res_dev_irq_extract, (void *)&info);
            break;
        case SOC_MIA_RES:
            ret = soc_resmng_dev_for_each_mia_res(udevid, soc_res_mia_extract, (void *)&info);
            break;
        default:
            break;
    }

    if (ret == 0) {
        *out_len = info.out_len;
    }

    return ret;
}

static int soc_dev_res_addr_inject(u32 udevid, enum soc_res_sync_type type, char *buf, u32 buf_len,
                                   soc_res_addr_decode func)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_addr_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_addr_sync *sync = (struct res_addr_sync *)(buf + pos);
        u64 decode_addr = sync->encode_addr;
        int ret;

        if (func != NULL) {
            ret = func(udevid, sync->encode_addr, sync->len, &decode_addr);
            if (ret != 0) {
                soc_err("Decode addr failed. (name=%s; encode_addr=0x%llx; len=0x%llx; ret=%d)\n", sync->name,
                        sync->encode_addr, sync->len, ret);
                return ret;
            }
        }

        if (type == SOC_REG_ADDR) {
            struct soc_reg_base_info io_base = {.io_base = (phys_addr_t)decode_addr, .io_base_size = (size_t)sync->len};
            ret = soc_resmng_dev_set_reg_base(udevid, (const char *)sync->name, &io_base);
        } else {
            struct soc_rsv_mem_info rsv_mem = {.rsv_mem = (phys_addr_t)decode_addr, .rsv_mem_size = (size_t)sync->len};
            ret = soc_resmng_dev_set_rsv_mem(udevid, (const char *)sync->name, &rsv_mem);
        }

        if (ret != 0) {
            soc_err("Set failed. (name=%s; addr=0x%llx; len=0x%llx; ret=%d)\n", sync->name, decode_addr, sync->len,
                    ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_dev_key_value_inject(u32 udevid, char *buf, u32 buf_len)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_key_value_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_key_value_sync *sync = (struct res_key_value_sync *)(buf + pos);
        int ret = soc_resmng_dev_set_key_value(udevid, (const char *)sync->name, sync->value);
        if (ret != 0) {
            soc_err("Set failed. (name=%s; avalue=0x%llx; ret=%d)\n", sync->name, sync->value, ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_dev_attr_inject(u32 udevid, char *buf, u32 buf_len)
{
    struct res_attr_sync *sync = NULL;
    u32 pos = 0;
    int ret;

    while (pos < buf_len) {
        if (buf_len - pos < sizeof(*sync)) {
            soc_err("Invalid len. (buf_len=0x%x; pos=0x%x; need_len=0x%lx)\n", buf_len, pos, sizeof(*sync));
            return -EINVAL;
        }
        sync = (struct res_attr_sync *)(buf + pos);

        if ((buf_len - pos - sizeof(*sync)) < sync->size) {
            soc_err("Invalid len. (buf_len=0x%x; pos=0x%x; need_len=0x%x)\n", buf_len, pos, sync->size);
            return -EINVAL;
        }
        ret = soc_resmng_dev_set_attr(udevid, (const char *)sync->name, (void *)sync->attr, sync->size);
        if (ret != 0) {
            soc_err("Set failed. (name=%s; size=0x%x; ret=%d)\n", sync->name, sync->size, ret);
            return ret;
        }

        pos += sizeof(*sync) + sync->size;
    }

    return 0;
}

static int soc_dev_res_mia_inject(u32 udevid, char *buf, u32 buf_len)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_mia_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_mia_sync *sync = (struct res_mia_sync *)(buf + pos);
        int ret = soc_resmng_dev_set_mia_res_ex(udevid, sync->type, &sync->info);
        if (ret != 0) {
            soc_err("Set mia res failed. (type=%d; ret=%d)\n", sync->type, ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_dev_res_misc_inject(u32 udevid, char *buf, u32 buf_len)
{
    struct res_dev_misc_sync *sync = (struct res_dev_misc_sync *)buf;
    int ret;

    if (buf_len != sizeof(*sync)) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    ret = soc_resmng_dev_set_mia_spec(udevid, sync->vfg_num, sync->vf_num);
    ret |= soc_resmng_dev_set_mia_base_info(udevid, sync->vfgid, sync->vfid);
    ret |= soc_resmng_subsys_set_num(udevid, TS_SUBSYS, sync->ts_num);
    if (ret != 0) {
        soc_err("Set failed. (devid=%u; ret=%d)\n", udevid, ret);
    }

    return ret;
}

static int soc_dev_res_inject(u32 udevid, enum soc_res_sync_type type, char *buf, u32 buf_len, soc_res_addr_decode func)
{
    int ret;

    switch (type) {
        case SOC_MISC_RES:
            ret = soc_dev_res_misc_inject(udevid, buf, buf_len);
            break;
        case SOC_KEY_VALUE_RES:
            ret = soc_dev_key_value_inject(udevid, buf, buf_len);
            break;
        case SOC_ATTR_RES:
            ret = soc_dev_attr_inject(udevid, buf, buf_len);
            break;
        case SOC_REG_ADDR:
        case SOC_RSV_MEM_ADDR:
            ret = soc_dev_res_addr_inject(udevid, type, buf, buf_len, func);
            break;
        case SOC_MIA_RES:
            ret = soc_dev_res_mia_inject(udevid, buf, buf_len);
            break;
        default:
            soc_err("Invalid para. (type=%d)\n", type);
            return -EINVAL;
    }

    return ret;
}

static int soc_dev_die_res_extract(u32 udevid, struct res_sync_target *target, char *buf, u32 *len)
{
    struct res_sync_info info;
    u32 buf_len = *len;
    u32 *out_len = len;
    int ret = 0;

    soc_pack_sync_info(&info, udevid, buf, buf_len, NULL);

    switch (target->type) {
        case SOC_MIA_RES:
            ret = soc_resmng_dev_die_for_each_mia_res(udevid, target->id, soc_res_mia_extract, (void *)&info);
            break;
        default:
            break;
    }

    if (ret == 0) {
        *out_len = info.out_len;
    }

    return ret;
}

static int soc_dev_die_res_mia_inject(u32 udevid, u32 die_id, char *buf, u32 buf_len)
{
    u32 pos = 0;

    if ((buf_len % sizeof(struct res_mia_sync)) != 0) {
        soc_err("Invalid para. (buf_len=%u)\n", buf_len);
        return -EINVAL;
    }

    while (pos < buf_len) {
        struct res_mia_sync *sync = (struct res_mia_sync *)(buf + pos);
        int ret = soc_resmng_dev_die_set_res(udevid, die_id, sync->type, &sync->info);
        if (ret != 0) {
            soc_err("Set mia res failed. (type=%d; ret=%d)\n", sync->type, ret);
            return ret;
        }

        pos += sizeof(*sync);
    }

    return 0;
}

static int soc_dev_die_res_inject(u32 udevid, u32 die_id, enum soc_res_sync_type type, char *buf, u32 buf_len)
{
    int ret;

    switch (type) {
        case SOC_MIA_RES:
            ret = soc_dev_die_res_mia_inject(udevid, die_id, buf, buf_len);
            break;
        default:
            soc_err("Invalid para. (type=%d)\n", type);
            return -EINVAL;
    }

    return ret;
}

int soc_res_extract(u32 udevid, struct res_sync_target *target, char *buf, u32 *len, soc_res_addr_encode func)
{
    int ret = 0;

    if ((target == NULL) || (buf == NULL) || (len == NULL)) {
        soc_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    soc_info("soc_res_extract. (udevid=%u; len=%u; type=%d; scope=%u)\n", udevid, *len, target->type, target->scope);

    if (target->scope == SOC_DEV) {
        ret = soc_dev_res_extract(udevid, target, buf, len, func);
    } else if (target->scope == SOC_TS_SUBSYS) {
        struct res_inst_info inst;
        soc_resmng_inst_pack(&inst, udevid, TS_SUBSYS, target->id);
        ret = soc_subsys_res_extract(&inst, target, buf, len, func);
    } else if (target->scope == SOC_DEV_DIE) {
        ret = soc_dev_die_res_extract(udevid, target, buf, len);
    } else {
        *len = 0;
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_res_extract);

int soc_res_inject(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len, soc_res_addr_decode func)
{
    int ret = -EINVAL;

    if ((target == NULL) || (buf == NULL)) {
        soc_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    soc_info("soc_res_inject. (udevid=%u; len=%u; type=%d; scope=%u)\n", udevid, buf_len, target->type, target->scope);

    if (target->scope == SOC_DEV) {
        ret = soc_dev_res_inject(udevid, target->type, buf, buf_len, func);
    } else if (target->scope == SOC_TS_SUBSYS) {
        struct res_inst_info inst;
        soc_resmng_inst_pack(&inst, udevid, TS_SUBSYS, target->id);
        ret = soc_subsys_res_inject(&inst, target->type, buf, buf_len, func);
    } else if (target->scope == SOC_DEV_DIE) {
        ret = soc_dev_die_res_inject(udevid, target->id, target->type, buf, buf_len);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_res_inject);

u32 soc_res_sync_get_sub_num(u32 udevid, enum soc_res_sync_scope scope)
{
    if (scope == SOC_DEV) {
        return 1;
    } else if (scope == SOC_TS_SUBSYS) {
        u32 sub_num;
        return (soc_resmng_subsys_get_num(udevid, TS_SUBSYS, &sub_num) == 0) ? sub_num : 0;
    } else if (scope == SOC_DEV_DIE) {
        u64 value;
        return (soc_resmng_dev_get_key_value(udevid, "soc_die_num", &value) == 0) ? (u32)value : 0;
    } else {
        return 0;
    }
}
KA_EXPORT_SYMBOL_GPL(soc_res_sync_get_sub_num);
