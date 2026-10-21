#!/bin/bash
# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
### BEGIN INIT INFO
# chkconfig: 123456 80 90
# Provides:        host_sys_init
# Required-Start:    $local_fs  $remote_fs
# Required-Stop:    $local_fs  $remote_fs
# Default-Start:    2 3 4 5
# Default-Stop:        0 1 6
# Short-Description:    Set up davinci servers
# Description:
### END INIT INFO
ASCEND_SECLOG="/var/log/ascend_seclog"
log_name="${ASCEND_SECLOG}/ascend_run_servers.log"
file_info_name="${ASCEND_SECLOG}/ascend_run_file.info"
installInfo="/etc/ascend_install.info"
targetdir=/usr/local/Ascend/driver
[ ! -d "${ASCEND_SECLOG}" ] && mkdir -m 750 -p "${ASCEND_SECLOG}"
touch $log_name $file_info_name

getInstallParam() {
    local _key="$1"
    local _file="$2"
    local _param
    # update "Driver_Install_Type" "UserName" "UserGroup" "Driver_Install_Path_Param" "Driver_Install_For_All"
    install_info_key_array=("Driver_Install_Type" "UserName" "UserGroup" "Driver_Install_Path_Param" "Driver_Install_Mode" "Driver_Install_For_All")
    if [ ! -f "${_file}" ];then
        exit 1
    fi
    for key_param in "${install_info_key_array[@]}"; do
        if [ ${key_param} == ${_key} ]; then
            _param=`grep -r "${_key}=" "${_file}" | cut -d"=" -f2-`
            break;
        fi
    done
    echo "${_param}"
}
UserName=$(getInstallParam "UserName" "${installInfo}")
UserGroup=$(getInstallParam "UserGroup" "${installInfo}")
Driver_Install_For_All=$(getInstallParam "Driver_Install_For_All" "${installInfo}")
# read Driver_Install_Path_Param from installInfo
Driver_Install_Path_Param=$(getInstallParam "Driver_Install_Path_Param" "${installInfo}")
# read Driver_Install_Mode Param from installInfo
Driver_Install_Mode=$(getInstallParam "Driver_Install_Mode" "${installInfo}")

targetdir="${Driver_Install_Path_Param}"
targetdir="${targetdir%*/}"
username="$UserName"
usergroup="$UserGroup"
if [ "$username" = "" ]; then
    username=HwHiAiUser
    usergroup=HwHiAiUser
fi
davinci_wait_times=300
connect_type=pcie

setFileChmod() {
    local option="$1"
    local mod="$2"
    local file_path="$3"

    if [ "${Driver_Install_For_All}" == "yes" ]; then
        local mod=${mod:0:-1}${mod: -2:1}
    fi

    chmod ${option} "${mod}" "${file_path}" >& /dev/null
    if [ $? -ne 0 ]; then
        echo "Error: ${mod} $file_path chmod failed!"
        exit 1
    fi
}

startMode(){
    if [ -d "${Driver_Install_Path_Param}" ]; then
        setFileChmod -f 640 $log_name
    fi
}

endMode(){
    if [ -d "${Driver_Install_Path_Param}" ]; then
        setFileChmod -f 440 $log_name
        setFileChmod -f 640 $file_info_name
    fi
}

servers_stop () {
    return 0
}

hdc_file_check() {
    hdc_wait_times=300
    for i in $(seq 1 $hdc_wait_times)
    do
        if [ -e /dev/hisi_hdc ];then
            setFileChmod -f 660 /dev/hisi_hdc
            chown -f $username:$usergroup /dev/hisi_hdc
            break
        fi
        echo "hdcdrv is not ready, wait ($i/$hdc_wait_times)..." >> $log_name
        sleep 1
    done
}

get_pci_info() {
    local bdf="" i=0

    g_dev_linked_nums=0
    g_bdfs=()

    for bdf in $(ls /sys/bus/pci/devices/)
    do
        cat /sys/bus/pci/devices/${bdf}/vendor 2>/dev/null | grep -Ewq "${vendor_bdf}" || continue
        cat /sys/bus/pci/devices/${bdf}/device 2>/dev/null | grep -Ewq "${device_bdf}" || continue
        [ -e /sys/bus/pci/devices/${bdf}/physfn ] && continue
        # 0x0110: it means that the chip is recognized as 1 on the OS PCI bus but 2 on the device management driver.
        cat /sys/bus/pci/devices/${bdf}/subsystem_device 2>/dev/null | grep -wq "0x0110" && g_dev_linked_nums=$(expr ${g_dev_linked_nums} + 1)
        g_dev_linked_nums=$(expr ${g_dev_linked_nums} + 1)
        # to get all bdfs
        g_bdfs[${i}]="${bdf}"
        i=$(expr ${i} + 1)
    done

    return 0
}

wait_pcie_device_init() {
    source "${targetdir}"/driver/script/specific_func.inc
    get_pci_info
    for i in $(seq 1 400)
    do
        local dev_num=$(ls /dev/davinci* 2>/dev/null | grep -cE "/dev/davinci[0-9]{1,2}$")
        if [ $dev_num -eq $g_dev_linked_nums ]; then
            break
        fi
        sleep 1
    done

    "${targetdir}"/driver/tools/upgrade-tool --mini_devices >> $log_name
    if [ $? -ne 0 ]; then
        echo "upgrade-tool get device count failed" >> $log_name
    fi
}

wait_device_init() {
    if [ "$connect_type" == "pcie" ]; then
        wait_pcie_device_init
    fi
}

changeDavinciMode() {
    for i in $(seq 1 $davinci_wait_times)
    do
        if [ -e /dev/davinci_manager ]; then
            setFileChmod -f 660 /dev/davinci_manager
            chown -f $username:$usergroup /dev/davinci_manager
            break
        fi
        sleep 1
    done

    wait_device_init
    for i in $(ls /dev/davinci*)
    do
        setFileChmod -f 660 $i
        chown -f $username:$usergroup $i
    done
}

clean_servers_log() {
    echo >$log_name
}

servers_log() {
    dmesg >>$log_name
    lsmod >>$log_name
    ps -ef >>$log_name
}

run_file_info() {
    ls -lR "$1" >$file_info_name
}

set_sock_option() {
    SLOGD_SIZE_2M=2097152
    SLOGD_DGRAM_QLEN=51200
    sysctl -w net.core.wmem_max=$SLOGD_SIZE_2M >/dev/null 2>&1
    sysctl -w net.unix.max_dgram_qlen=$SLOGD_DGRAM_QLEN >/dev/null 2>&1
}

get_connect_type() {
    config=$(cat "$Driver_Install_Path_Param"/driver/script/feature.conf | grep FEATURE_HD_CONNECT_IS_UB)
    if [ "$config"x = "FEATURE_HD_CONNECT_IS_UB=y"x ]; then
        connect_type="ub"
    else
        connect_type="pcie"
    fi
}

ub_insmod_asdrv() {
    if [ "$connect_type" = "pcie" ]; then
        return
    fi

    for i in $(seq 1 300)
    do
        local ubcore_ko_num=$(lsmod | grep -cE ^ubcore)
        if [ $ubcore_ko_num -ne 0 ]; then
            break
        fi
        sleep 1
    done

    local ubdrv_ko_num=$(lsmod | grep -cE ^asdrv_ub)
    if [ $ubdrv_ko_num -ne 0 ]; then
        return
    fi

    modprobe drv_seclib_host
    modprobe ascend_kernel_open_adapt
    modprobe asdrv_vascend_adapt
    modprobe drv_vascend
    modprobe asdrv_pbl
    modprobe asdrv_trsbase
    modprobe ascend_adapter
    modprobe asdrv_ub
    modprobe asdrv_dpa
    modprobe asdrv_vpc
    modprobe drv_pcie_host
    modprobe asdrv_vnic
    modprobe asdrv_vmng
    modprobe asdrv_fms
    modprobe asdrv_dms
    modprobe asdrv_esched
    modprobe drv_pcie_hdc_host
    modprobe asdrv_svm
    modprobe asdrv_trs
    modprobe asdrv_buff
    modprobe asdrv_queue
    modprobe ts_agent
}

init_signature_configuration() {

    if [ ! -d "/var/asdrv" ]; then
        mkdir -p /var/asdrv
        chmod 755 /var/asdrv
    fi

    devids=({0..64} {100..1139})
    for devid in "${devids[@]}"; do
        if [ ! -f "/var/asdrv/dev${devid}/device-sw-plugin/verify/flag" ];then
            mkdir -p /var/asdrv/dev${devid}
            chmod 755 /var/asdrv/dev${devid}

            mkdir -p /var/asdrv/dev${devid}/device-sw-plugin
            chmod 755 /var/asdrv/dev${devid}/device-sw-plugin

            mkdir -p /var/asdrv/dev${devid}/device-sw-plugin/verify
            chmod 755 /var/asdrv/dev${devid}/device-sw-plugin/verify

            echo "verify_flag=5" > "/var/asdrv/dev${devid}/device-sw-plugin/verify/flag";
            touch "/var/asdrv/dev${devid}/device-sw-plugin/verify/user-certificate";
            chmod 644 "/var/asdrv/dev${devid}/device-sw-plugin/verify/flag";
            chmod 644 "/var/asdrv/dev${devid}/device-sw-plugin/verify/user-certificate";
        fi

        if [ ! -f "/var/asdrv/dev${devid}/device-sw-plugin/verify/user-certificate" ];then
            touch "/var/asdrv/dev${devid}/device-sw-plugin/verify/user-certificate";
            chmod 644 "/var/asdrv/dev${devid}/device-sw-plugin/verify/user-certificate";
        fi
    done
}

servers_start() {
    get_connect_type
    ub_insmod_asdrv
    clean_servers_log
    if [ -d "$targetdir"/driver/tools ];then
        hdc_file_check
        changeDavinciMode

        # toolchain servers
        set_sock_option
    fi

    run_file_info "$targetdir"
    servers_log
    is_lab=0
    # check whether LogAnalysis server exist
    if [ `which Get_Server_Info.py 2>/dev/null | wc -l` -ne 0 ] && [ -f "/dlyupg/Contral_Plane_IpAddress.xls" ];then
        ip_exist1=`Get_Server_Info.py Log IP 2>/dev/null`
        ip_exist2=`Get_Server_Info.py LogAnalysis IP 2>/dev/null`
        if [ ! -z "$ip_exist1" ] || [ ! -z "$ip_exist2" ];then
            is_lab=1
        fi
    fi

    if [ -f "/sys/kernel/mm/transparent_hugepage/shmem_enabled" ];then
        mount -o remount,huge=advise /dev/shm/
        echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
        echo advise > /sys/kernel/mm/transparent_hugepage/shmem_enabled
    fi

    init_signature_configuration
}

case "$1" in
    start)
    startMode
    servers_start
    ;;

    restart)
    startMode
    servers_stop
    servers_start
    ;;

    *)
    # no-operational
    ;;
esac
endMode

exit 0
