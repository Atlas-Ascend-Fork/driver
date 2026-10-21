/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DMS_DEVDRV_INFO_H
#define DMS_DEVDRV_INFO_H

struct dmanage_pcie_id_info {
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int deviceid;
    unsigned int subdeviceid;
    unsigned int bus;
    unsigned int device;
    unsigned int fn;
    unsigned int davinci_id;
    unsigned int domain;
};

#endif
