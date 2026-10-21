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

#include "ka_kernel_def_pub.h"
#include "msg_chan_main.h"

int devdrv_get_connect_protocol_inner(u32 index_id)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get connect type fail.\n");
        return -ENODEV;
    }

    ret = dev_ops->ops.get_connect_type(index_id);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_connect_protocol_inner);

int devdrv_get_connect_protocol(u32 dev_id)
{
    u32 index_id;

    index_id = devdrv_get_index_id_by_devid(dev_id);
    return devdrv_get_connect_protocol_inner(index_id);
}
KA_EXPORT_SYMBOL(devdrv_get_connect_protocol);

int devdrv_get_pfvf_type_by_devid_inner(u32 index_id)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get pfvf type fail.\n");
        return -ENODEV;
    }
    ret = dev_ops->ops.get_pfvf_type_by_devid(index_id);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_pfvf_type_by_devid_inner);

int devdrv_get_pfvf_type_by_devid(u32 dev_id)
{
    u32 index_id;

    index_id = devdrv_get_index_id_by_devid(dev_id);
    return devdrv_get_pfvf_type_by_devid_inner(index_id);
}
KA_EXPORT_SYMBOL(devdrv_get_pfvf_type_by_devid);

int devdrv_get_pm_devid_by_vm_devid(u32 udev_id, u32 *pm_devid)
{
    u32 index_id;
    int ret = -EOPNOTSUPP;

    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.get_pm_devid_by_vm_devid != NULL) {
        index_id = devdrv_get_index_id_by_devid(udev_id);
        ret = dev_ops->ops.get_pm_devid_by_vm_devid(index_id, pm_devid);
    }

    if (ret == 0) {
        (void)uda_add_id_to_udevid(*pm_devid, pm_devid);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_pm_devid_by_vm_devid);

bool devdrv_is_mdev_vm_boot_mode_inner(u32 index_id)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get vm boot mode fail.\n");
        return false;
    }
    ret = dev_ops->ops.mdev_vm_boot_mode(index_id);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_is_mdev_vm_boot_mode_inner);

bool devdrv_is_mdev_vm_boot_mode(u32 dev_id)
{
    u32 index_id;

    index_id = devdrv_get_index_id_by_devid(dev_id);
    return devdrv_is_mdev_vm_boot_mode_inner(index_id);
}
KA_EXPORT_SYMBOL(devdrv_is_mdev_vm_boot_mode);

bool devdrv_is_sriov_support(u32 dev_id)
{
    int ret;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get sriov support fail.\n");
        return false;
    }
    index_id = devdrv_get_index_id_by_devid(dev_id);
    ret = dev_ops->ops.sriov_support(index_id);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_is_sriov_support);

int devdrv_sriov_enable(u32 dev_id, u32 boot_mode)
{
    int ret = 0;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get sriov enable fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.sriov_enable != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.sriov_enable(index_id, boot_mode);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_sriov_enable);

int devdrv_sriov_disable(u32 dev_id, u32 boot_mode)
{
    int ret = 0;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get sriov disable fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.sriov_disable != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.sriov_disable(index_id, boot_mode);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_sriov_disable);

int devdrv_get_ub_urma_info_by_udevid(u32 udevid, struct ascend_urma_dev_info *urma_info)
{
    int ret = 0;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get sriov disable fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.get_urma_info_by_eid != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        ret = dev_ops->ops.get_urma_info_by_eid(index_id, urma_info);
    } else {
        devdrv_err("Get_urma_info_by_eid is not registered.\n");
        devdrv_sub_ops_ref(dev_ops);
        return -EINVAL;
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_ub_urma_info_by_udevid);

int devdrv_get_ub_dev_info(u32 dev_id, struct devdrv_ub_dev_info *eid_info, int *num)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get ub dev info fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.get_ub_dev_info != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.get_ub_dev_info(index_id, eid_info, num);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_ub_dev_info);

int devdrv_addr_trans_p2p_peer_to_local(u32 udevid, u32 peer_udevid, struct devdrv_addr_desc *addr_desc,
                                        u64 *trans_addr)
{
    int ret = -EOPNOTSUPP;
    u32 index_id, peer_index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("trans p2p peer to local fail.(devid=%d)\n", udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.addr_trans_p2p != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        peer_index_id = devdrv_get_index_id_by_devid(peer_udevid);
        ret = dev_ops->ops.addr_trans_p2p(index_id, peer_index_id, addr_desc, trans_addr);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_addr_trans_p2p_peer_to_local);

int devdrv_addr_trans_cs_p2p_peer_to_local(u32 udevid, u32 peer_sdid, struct devdrv_addr_desc *addr_desc,
                                           u64 *trans_addr)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("trans cs p2p peer to local fail.(devid=%d)\n", udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.addr_trans_cs_p2p != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        ret = dev_ops->ops.addr_trans_cs_p2p(index_id, peer_sdid, addr_desc, trans_addr);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_addr_trans_cs_p2p_peer_to_local);

int devdrv_get_token_val(u32 dev_id, u32 *token_val)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_get_token_val_ops();
    if (dev_ops == NULL) {
        devdrv_err("Get ub token_val fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.get_token_val != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.get_token_val(index_id, token_val);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_token_val);

int devdrv_process_pasid_add(u32 dev_id, u64 pasid)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get ub dev info fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.add_pasid != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.add_pasid(index_id, pasid);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_process_pasid_add);

int devdrv_process_pasid_del(u32 dev_id, u64 pasid)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get ub dev info fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.del_pasid != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.del_pasid(index_id, pasid);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_process_pasid_del);

int devdrv_get_d2d_eid(u32 local_udevid, u32 d2d_eid_type, struct devdrv_pair_info_eid *eid, u32 *eid_index)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("Get ub d2d eid fail.(devid=%u)\n", local_udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.get_d2d_eid != NULL) {
        index_id = devdrv_get_index_id_by_devid(local_udevid);
        ret = dev_ops->ops.get_d2d_eid(index_id, d2d_eid_type, eid, eid_index);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_d2d_eid);

int devdrv_get_bus_instance_eid(u32 udevid, struct devdrv_pair_info_eid *eid)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("Get ub bus instance eid fail.(devid=%d)\n", udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.get_bus_instance_eid != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        ret = dev_ops->ops.get_bus_instance_eid(index_id, eid);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_bus_instance_eid);

int devdrv_urma_copy(u32 dev_id, enum devdrv_urma_chan_type type, enum devdrv_urma_copy_dir dir,
                     struct devdrv_urma_copy *local, struct devdrv_urma_copy *peer)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get devdrv_urma_copy fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.urma_copy != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.urma_copy(index_id, type, dir, local, peer);
    } else {
        devdrv_err("devdrv_urma_copy is null. (devid=%d)\n", dev_id);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_urma_copy);

int devdrv_register_seg(u32 dev_id, struct devdrv_seg_info *info, void **tseg, size_t *out_len)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get devdrv_register_seg fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.register_seg != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.register_seg(index_id, info, tseg, out_len);
    } else {
        devdrv_err("devdrv_register_seg is null. (devid=%d)\n", dev_id);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_register_seg);

int devdrv_unregister_seg(u32 dev_id, void *tseg, size_t in_len)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get devdrv_unregister_seg fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dev_ops->ops.unregister_seg != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.unregister_seg(index_id, tseg, in_len);
    } else {
        devdrv_err("devdrv_unregister_seg is null. (devid=%d)\n", dev_id);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_unregister_seg);

void *devdrv_import_seg(u32 dev_id, u32 peer_token, void *peer_seg, size_t in_len, size_t *out_len)
{
    void *ret = NULL;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get devdrv_import_seg fail.(devid=%d)\n", dev_id);
        return NULL;
    }

    if (dev_ops->ops.import_seg != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.import_seg(index_id, peer_token, peer_seg, in_len, out_len);
    } else {
        devdrv_err("devdrv_import_seg is null. (devid=%d)\n", dev_id);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_import_seg);

int devdrv_unimport_seg(u32 dev_id, void *peer_tseg, size_t in_len)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();

    if (dev_ops == NULL) {
        devdrv_err("Get devdrv_unimport_seg fail.(devid=%d)\n", dev_id);
        return -ENODEV;
    }
    if (dev_ops->ops.unimport_seg != NULL) {
        index_id = devdrv_get_index_id_by_devid(dev_id);
        ret = dev_ops->ops.unimport_seg(index_id, peer_tseg, in_len);
    } else {
        devdrv_err("devdrv_unimport_seg is null. (devid=%d)\n", dev_id);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_unimport_seg);

int devdrv_get_dev_id_info(u32 udevid, struct devdrv_dev_id_info *info)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("Get ub dev id info fail.(udevid=%d)\n", udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.get_ub_dev_id_info != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        ret = dev_ops->ops.get_ub_dev_id_info(index_id, info);
    } else {
        devdrv_err("get_ub_dev_id_info is null. (udevid=%d)\n", udevid);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_dev_id_info);

int devdrv_addr_trans_peer_to_local(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("trans addr peer to local fail.(devid=%d)\n", udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.addr_trans_p2l != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        ret = dev_ops->ops.addr_trans_p2l(index_id, addr_desc, trans_addr);
    } else {
        devdrv_err("addr_trans_p2l is null. (udevid=%d)\n", udevid);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_addr_trans_peer_to_local);

int devdrv_addr_trans_local_to_peer(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr)
{
    int ret = -EOPNOTSUPP;
    u32 index_id;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_err("trans addr local to peer fail.(devid=%u)\n", udevid);
        return -ENODEV;
    }

    if (dev_ops->ops.addr_trans_l2p != NULL) {
        index_id = devdrv_get_index_id_by_devid(udevid);
        ret = dev_ops->ops.addr_trans_l2p(index_id, addr_desc, trans_addr);
    } else {
        devdrv_err("addr_trans_l2p is null. (udevid=%u)\n", udevid);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_addr_trans_local_to_peer);

bool devdrv_query_addr_trans_support(enum devdrv_addr_trans_cap_type cap_type)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    bool support = false;

    dev_ops = devdrv_add_ops_ref_by_type(DEVDRV_COMMNS_UB);
    if (dev_ops == NULL) {
        devdrv_warn("Ub comm ops not ready, addr trans cap not support. (cap_type=%u)\n", (u32)cap_type);
        return false;
    }

    if (dev_ops->ops.query_addr_trans_support != NULL) {
        support = dev_ops->ops.query_addr_trans_support(cap_type);
    } else {
        devdrv_warn("Query addr trans support ops is null, cap not support. (cap_type=%u)\n", (u32)cap_type);
    }
    devdrv_sub_ops_ref_by_type(dev_ops);
    return support;
}
KA_EXPORT_SYMBOL(devdrv_query_addr_trans_support);

int devdrv_force_linkdown(u32 udevid)
{
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    int ret = -EOPNOTSUPP;
    if (dev_ops == NULL) {
        devdrv_err("Get dev ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.force_linkdown != NULL) {
        ret = dev_ops->ops.force_linkdown(udevid);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_force_linkdown);

int devdrv_get_non_trans_chan_capacity(void *msg_chan, u32 *len)
{
    int ret = -EOPNOTSUPP;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get msg_chan capacity fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.get_non_trans_chan_capacity != NULL) {
        ret = dev_ops->ops.get_non_trans_chan_capacity(msg_chan, len);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_non_trans_chan_capacity);
