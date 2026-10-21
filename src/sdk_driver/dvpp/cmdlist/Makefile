# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
ifeq ($(ENABLE_OPEN_SRC), y)
    LOCAL_PATH := $(DRIVER_KERNEL_DIR)/dvpp/cmdlist

    obj-m += drv_dvpp_cmdlist.o

    drv_dvpp_cmdlist-objs := dvpp_cmdlist_init.o \
                            decoder/dvpp_decoder.o \
                            decoder/tlv/dvpp_tlv_decoder.o \
                            decoder/vpu/dvpp_vpu_decoder.o \
                            cmdlist/dvpp_cmdlist.o \
                            cmdlist/vpc/init/init_mod.o \
                            cmdlist/vpc/register/vpc_cmdlist.o \
                            cmdlist/vpc/architecture/arch_v1xx/drv_arch_comm.o \
                            reg/vpc/arch_v1xx/vpc_reg.o \
                            cmdlist/jpegd/drv/milan/drv_jpegd_comm.o \
                            cmdlist/jpegd/jpegd_cmdlist.o \
                            cmdlist/jpegd/drv/drv_jpegd.o \
                            proc/dvpp_cmdlist_proc.o \
                            context/dvpp_cmdlist_context.o \
                            base/dvpp_cmdlist_util.o \
                            cmdlist/jpegd/drv/drv_jpegd.o \
                            cmdlist/jpege/drv/drv_jpege_comm.o \
                            cmdlist/jpege/jpege_cmdlist.o \
                            cmdlist/jpege/drv/milan/drv_jpege.o \
                            main/comm/dvpp_sqe_update_comm.o \
                            main/comm/dvpp_sqe_para_check.o \
                            memory/args_mem_pool.o \
                            memory/mem_allocater.o \
                            memory/dvpp_mem_allocater_comm.o \
                            memory/dvpp_mem_allocater_args.o \
                            command/ioctl/dvpp_cmdlist_ioctl.o \
                            cmdlist/vpc/init/init.o

    EXTRA_CFLAGS += -DRUN_MODE_ONLINE
    drv_dvpp_cmdlist-objs += main/host/dvpp_sqe_update_host.o
    drv_dvpp_cmdlist-objs += main/host/dvpp_sqe_update_phy.o
    drv_dvpp_cmdlist-objs += main/host/mem_ctrl/dvpp_dma_mem_ctrl.o
    drv_dvpp_cmdlist-objs += main/host/mem_ctrl/dvpp_svm_mem_ctrl.o
    drv_dvpp_cmdlist-objs += dvpp_pcie_init.o
    drv_dvpp_cmdlist-objs += memory/dvpp_mem_allocater_comm_host.o
    drv_dvpp_cmdlist-objs += memory/dvpp_mem_allocater_dma.o
    drv_dvpp_cmdlist-objs += memory/dma_mem_pool.o

    EXTRA_CFLAGS += -I$(LOCAL_PATH)/main/host
    EXTRA_CFLAGS += -I$(LOCAL_PATH)/main/host/mem_ctrl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/common
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/command/ioctl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/vascend_drv_stub
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/host
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include

    EXTRA_CFLAGS += -I${DRIVER_HAL_INC_DIR}/depends/libc_sec \
                    -I$(DRIVER_KERNEL_DIR)/ts_agent/inc \
                    -I$(DRIVER_HAL_INC_DIR) \
                    -I$(DRIVER_KERNEL_DIR)/dvpp/include/cmdlist \
                    -I$(LOCAL_PATH)/reg/vpc/arch_v1xx \
                    -I$(LOCAL_PATH)/reg/jpegd/milan \
                    -I$(LOCAL_PATH)/reg/jpege/milan \
                    -I$(LOCAL_PATH)/cmdlist/vpc/init \
                    -I$(LOCAL_PATH)/main/comm \
                    -I$(LOCAL_PATH)/main/inc \
                    -I$(LOCAL_PATH)/decoder \
                    -I$(LOCAL_PATH)/decoder/tlv \
                    -I$(LOCAL_PATH)/decoder/vpu \
                    -I$(LOCAL_PATH)/decoder/type \
                    -I$(LOCAL_PATH)/base \
                    -I$(LOCAL_PATH)/cmdlist \
                    -I$(LOCAL_PATH)/cmdlist/vpc/register \
                    -I$(LOCAL_PATH)/cmdlist/vpc/architecture/arch_v1xx \
                    -I$(LOCAL_PATH)/cmdlist/vpc/architecture/arch_v1xx/v100 \
                    -I$(LOCAL_PATH)/cmdlist/jpegd \
                    -I$(LOCAL_PATH)/cmdlist/jpegd/drv \
                    -I$(LOCAL_PATH)/cmdlist/jpegd/drv/milan \
                    -I$(LOCAL_PATH)/cmdlist/jpege \
                    -I$(LOCAL_PATH)/cmdlist/jpege/drv \
                    -I$(LOCAL_PATH)/command/ioctl \
                    -I$(LOCAL_PATH)/proc \
                    -I$(LOCAL_PATH)/context \
                    -I$(LOCAL_PATH)/memory \
                    -I$(LOCAL_PATH)/vmng \
                    -I${C_SEC_INCLUDE}

    EXTRA_CFLAGS += -Iinclude/linux

    EXTRA_CFLAGS += -DSUPPORT_VIR_MACH
    drv_dvpp_cmdlist-objs += cmdlist/vpc/architecture/arch_v1xx/v100/drv_vpc_mlv1r1.o
    drv_dvpp_cmdlist-objs += vmng/dvpp_vcm_host.o
    drv_dvpp_cmdlist-objs += vmng/dvpp_vmng.o
    drv_dvpp_cmdlist-objs += main/host/dvpp_sqe_update_vir.o
    drv_dvpp_cmdlist-objs += main/host/mem_ctrl/dvpp_share_mem_ctrl.o
else
    ifeq ($(DAVINCI_HIAI_DKMS), y)
        LOCAL_PATH := $(HIAI_DKMS_DIR)/dvpp_cmdlist
        include $(LOCAL_PATH)/Makefile_dkms
    else
        LOCAL_PATH := $(DRIVER_KERNEL_DIR)/dvpp/cmdlist

        obj-m   += drv_dvpp_cmdlist.o

        drv_dvpp_cmdlist-objs := dvpp_cmdlist_init.o \
                                decoder/dvpp_decoder.o \
                                decoder/tlv/dvpp_tlv_decoder.o \
                                decoder/vpu/dvpp_vpu_decoder.o \
                                cmdlist/dvpp_cmdlist.o \
                                cmdlist/vpc/init/init_mod.o \
                                cmdlist/vpc/register/vpc_cmdlist.o \
                                cmdlist/vpc/architecture/arch_v1xx/drv_arch_comm.o \
                                reg/vpc/arch_v1xx/vpc_reg.o \
                                cmdlist/jpegd/drv/milan/drv_jpegd_comm.o \
                                cmdlist/jpegd/jpegd_cmdlist.o \
                                cmdlist/jpegd/drv/drv_jpegd.o \
                                proc/dvpp_cmdlist_proc.o \
                                context/dvpp_cmdlist_context.o \
                                base/dvpp_cmdlist_util.o \
                                cmdlist/jpegd/drv/drv_jpegd.o \
                                cmdlist/jpege/drv/drv_jpege_comm.o \
                                cmdlist/jpege/jpege_cmdlist.o \
                                cmdlist/jpege/drv/milan/drv_jpege.o \
                                main/comm/dvpp_sqe_update_comm.o \
                                main/comm/dvpp_sqe_para_check.o \
                                memory/args_mem_pool.o \
                                memory/mem_allocater.o \
                                memory/dvpp_mem_allocater_comm.o \
                                memory/dvpp_mem_allocater_args.o \
                                command/ioctl/dvpp_cmdlist_ioctl.o \
                                cmdlist/vpc/init/init.o

        EXTRA_CFLAGS += -DRUN_MODE_ONLINE
        drv_dvpp_cmdlist-objs += main/host/dvpp_sqe_update_host.o
        drv_dvpp_cmdlist-objs += main/host/dvpp_sqe_update_phy.o
        drv_dvpp_cmdlist-objs += main/host/mem_ctrl/dvpp_dma_mem_ctrl.o
        drv_dvpp_cmdlist-objs += main/host/mem_ctrl/dvpp_svm_mem_ctrl.o
        drv_dvpp_cmdlist-objs += dvpp_pcie_init.o
        drv_dvpp_cmdlist-objs += memory/dvpp_mem_allocater_comm_host.o
        drv_dvpp_cmdlist-objs += memory/dvpp_mem_allocater_dma.o
        drv_dvpp_cmdlist-objs += memory/dma_mem_pool.o

        EXTRA_CFLAGS += -I$(LOCAL_PATH)/main/host
        EXTRA_CFLAGS += -I$(LOCAL_PATH)/main/host/mem_ctrl
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/common
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/vascend_drv_stub
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/host
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include

        EXTRA_CFLAGS += -I${DRIVER_HAL_INC_DIR}/depends/libc_sec \
                        -I$(DRIVER_KERNEL_DIR)/ts_agent/inc \
                        -I$(DRIVER_HAL_INC_DIR) \
                        -I$(DRIVER_KERNEL_DIR)/dvpp/include/cmdlist \

        EXTRA_CFLAGS += -I$(LOCAL_PATH)/reg/vpc/arch_v1xx \
                        -I$(LOCAL_PATH)/reg/jpegd/milan \
                        -I$(LOCAL_PATH)/reg/jpege/milan \
                        -I$(LOCAL_PATH)/cmdlist/vpc/init \
                        -I$(LOCAL_PATH)/main/comm \
                        -I$(LOCAL_PATH)/main/inc \
                        -I$(LOCAL_PATH)/decoder \
                        -I$(LOCAL_PATH)/decoder/tlv \
                        -I$(LOCAL_PATH)/decoder/vpu \
                        -I$(LOCAL_PATH)/decoder/type \
                        -I$(LOCAL_PATH)/base \
                        -I$(LOCAL_PATH)/cmdlist \
                        -I$(LOCAL_PATH)/cmdlist/vpc/register \
                        -I$(LOCAL_PATH)/cmdlist/vpc/architecture/arch_v1xx \
                        -I$(LOCAL_PATH)/cmdlist/jpegd \
                        -I$(LOCAL_PATH)/cmdlist/jpegd/drv \
                        -I$(LOCAL_PATH)/cmdlist/jpegd/drv/milan \
                        -I$(LOCAL_PATH)/cmdlist/jpege \
                        -I$(LOCAL_PATH)/cmdlist/jpege/drv \
                        -I$(LOCAL_PATH)/command/ioctl \
                        -I$(LOCAL_PATH)/proc \
                        -I$(LOCAL_PATH)/context \
                        -I$(LOCAL_PATH)/memory \
                        -I$(LOCAL_PATH)/vmng

        ifneq ($(findstring $(PRODUCT), ascend910B),)
            EXTRA_CFLAGS += -DSUPPORT_VIR_MACH
            drv_dvpp_cmdlist-objs += cmdlist/vpc/architecture/arch_v1xx/v100/drv_vpc_mlv1r1.o
            drv_dvpp_cmdlist-objs += vmng/dvpp_vcm_host.o
            drv_dvpp_cmdlist-objs += vmng/dvpp_vmng.o
            drv_dvpp_cmdlist-objs += main/host/dvpp_sqe_update_vir.o
            drv_dvpp_cmdlist-objs += main/host/mem_ctrl/dvpp_share_mem_ctrl.o
            EXTRA_CFLAGS += -I$(LOCAL_PATH)/cmdlist/vpc/architecture/arch_v1xx/v100
        endif
    endif
endif