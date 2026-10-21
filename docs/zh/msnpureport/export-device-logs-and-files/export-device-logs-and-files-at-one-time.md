# 单次导出Device侧系统类日志和其他维测信息

## 产品支持情况

<!-- npu="950" id1 -->

- Ascend 950PR&950DT系列产品：支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas训练系列产品：支持
<!-- end id6 -->

## 功能说明

msnpureport工具支持导出Device侧系统类日志，主要包括：

- Control CPU上的系统类日志，包括内核态日志和系统进程运行产生的用户态日志，主要反映AI处理器的整体运行情况。
- 非Control CPU（例如低功耗）上的系统类日志，主要反映低功耗、Task Scheduler、ISP等组件的运行情况。

除此之外，msnpureport还支持导出其他维测信息，例如：黑匣子日志、Device进程coredump时的调用栈信息、Host侧驱动内核日志等。

## 注意事项

- 该命令仅支持root用户执行。且须确保执行该命令的当前路径对普通用户无访问权限，否则普通用户也能操作导出的日志文件，存在恶意删除日志文件或泄露系统信息等安全风险。
- 该命令支持多进程运行，建议并发进程数不超过4个，否则可能由于资源不足导致执行失败；不建议在同路径同时刻并发使用以免造成落盘时间戳目录名冲突。
- 容器场景下，仅支持导出Host侧驱动内核日志，其他日志不支持使用该命令导出（特权容器场景除外，无此限制）。
- 容器场景下，命令执行时需要添加--docker参数，且需要配置msnpureport环境变量，示例如下：

    ```sh
    export PATH=/usr/local/Ascend/driver/tools:$PATH
    ```

## 命令格式

方式一（推荐）：

```sh
msnpureport report [options]
```

方式二：

```sh
msnpureport [options]
```

## 参数说明

### -d <deviceId>或--device <deviceId>

可选，指定Device ID（逻辑ID），默认导出所有Device日志。部分产品可能存在SMP工作模式，即多个Device共用一个OS组成一组，此时指定一个OS内的任意Device ID时，会导出该OS下的所有Device日志。该参数仅支持通过方式一传入，不支持通过方式二传入。

### 不指定任何参数

可选，导出以下信息：

- Device侧系统类日志：
  - Control CPU上的系统类日志
  - 非Control CPU上的系统类日志
- 其他维测信息：
  - Device侧进程coredump时的调用栈信息
  - 黑匣子日志
  - 事件调度模块的维测信息
  - Host侧驱动内核日志
  - Device侧OS系统日志

### -a 或--all

可选，导出以下信息：

- Device侧系统类日志：
  - Control CPU上的系统类日志
  - 非Control CPU上的系统类日志
- 其他维测信息：
  - Device侧进程coredump时的调用栈信息
  - 黑匣子日志、黑匣子设备事件信息
  - 事件调度模块的维测信息
  - Host侧驱动内核日志
  - Device侧OS系统日志

<br>说明：执行`-a`导出黑匣子相关信息时，如果存在卡住或导出慢的情况，建议执行`npu-smi set -t reset -i id -c chip_id`复位芯片后重新导出，详细信息请参考[《 npu-smi 命令参考》](https://support.huawei.com/enterprise/zh/ascend-computing/ascend-hdk-pid-252764743?category=reference-guides)。

### -f 或--force

可选，导出以下信息：

- Device侧系统类日志：
  - Control CPU上的系统类日志
  - 非Control CPU上的系统类日志
- 其他维测信息：
  - 进程coredump时的调用栈信息
  - 黑匣子日志、黑匣子设备事件信息、黑匣子存储空间中的历史维测信息
  - 事件调度模块的维测信息
  - Host侧驱动内核日志
  - Device侧OS系统日志

<br>说明：执行`-f`导出黑匣子相关信息时，如果存在卡住或导出慢的情况，建议执行`npu-smi set -t reset`复位芯片后重新导出，详细信息请参考[《 npu-smi 命令参考》](https://support.huawei.com/enterprise/zh/ascend-computing/ascend-hdk-pid-252764743?category=reference-guides)。

### -t <type_id>或--type <type_id>

可选，指定导出的日志类型，取值为：

- 0：导出以下信息：
  - Device侧系统类日志：包括Control CPU上的系统类日志和非Control CPU上的系统类日志
  - 其他维测信息：包括Device侧进程coredump时的调用栈信息、黑匣子日志、黑匣子设备事件信息、事件调度模块的维测信息、Host侧驱动内核日志和Device侧OS系统日志

- 1：导出以下信息：
  - Device侧系统类日志：包括Control CPU上的系统类日志和非Control CPU上的系统类日志

  - 其他维测信息：包括事件调度模块的维测信息、Device侧OS系统日志和其他模块日志，如DVPP、TEE日志、驱动proc日志等，不同产品型号支持导出的日志类型略有差异，请以实际为准

- 2：黑匣子日志、黑匣子设备事件信息、黑匣子存储空间中的历史维测信息。`-t 2`不支持多进程并发执行。<!-- npu="A3,910b" id7 --><br>对于以下产品，还支持导出当前硬件寄存器信息，导出后建议执行`npu-smi set -t reset -i id -c chip_id`复位芯片，详细信息请参考[《 npu-smi 命令参考》](https://support.huawei.com/enterprise/zh/ascend-computing/ascend-hdk-pid-252764743?category=reference-guides)。<!-- end id7 -->
  <!-- npu="910b" id8 -->
  - Atlas A2系列产品
  <!-- end id8 -->
  <!-- npu="A3" id9 -->
  - Atlas A3系列产品
  <!-- end id9 -->
  <!-- npu="950" id41 -->
  - Ascend 950PR&950DT系列产品
  <!-- end id41 -->
- 3：Device侧进程coredump时的调用栈信息
<!-- npu="950,A3,910b" id10 -->
- 4：vmcore文件。对于以下产品，当Device OS心跳丢失时，会同时生成vmcore文件和黑匣子日志，其他产品不会生成vmcore文件。`-t 4`不支持多进程并发执行。
  <!-- npu="910b" id11 -->
  - Atlas A2系列产品
   <!-- end id11 -->
  <!-- npu="A3" id12 -->
   - Atlas A3系列产品
  <!-- end id12 -->

  <!-- npu="950" id13 -->
   - Ascend 950PR&950DT系列产品
  <!-- end id13 -->

 <!-- end id10 -->
- 5：其他模块日志，如DVPP、TEE日志、驱动proc日志等，不同产品型号支持导出的日志类型略有差异，请以实际为准
- 6：Unified Bus统一总线的维测信息。
  <!-- npu="310b" id14 -->
  - Atlas 200I/500 A2推理产品：不支持
  <!-- end id14 -->
  <!-- npu="310p" id15 -->
  - Atlas推理系列产品：不支持
  <!-- end id15 -->
  <!-- npu="910" id47 -->
  - Atlas训练系列产品：不支持
  <!-- end id47 -->
  <!-- npu="910b" id16 -->
  - Atlas A2系列产品：不支持
  <!-- end id16 -->
  <!-- npu="A3" id17 -->
  - Atlas A3系列产品：不支持
  <!-- end id17 -->
  <!-- npu="950" id18 -->
  - Ascend 950PR&950DT系列产品：支持
  <!-- end id18 -->
- 7：AO（Always Online）区的日志信息。
  <!-- npu="310b" id26 -->
  - Atlas 200I/500 A2推理产品：不支持
  <!-- end id26 -->
  <!-- npu="310p" id27 -->
  - Atlas推理系列产品：不支持
  <!-- end id27 -->
  <!-- npu="910" id48 -->
  - Atlas训练系列产品：不支持
  <!-- end id48 -->
  <!-- npu="910b" id28 -->
  - Atlas A2系列产品：不支持
  <!-- end id28 -->
  <!-- npu="A3" id29 -->
  - Atlas A3系列产品：不支持
  <!-- end id29 -->
  <!-- npu="950" id30 -->
  - Ascend 950PR&950DT系列产品：支持
  <!-- end id30 -->
- 8：AO计数。
  <!-- npu="310b" id31 -->
  - Atlas 200I/500 A2推理产品：不支持
  <!-- end id31 -->
  <!-- npu="310p" id32 -->
  - Atlas推理系列产品：不支持
  <!-- end id32 -->
  <!-- npu="910" id49 -->
  - Atlas训练系列产品：不支持
  <!-- end id49 -->
  <!-- npu="910b" id33 -->
  - Atlas A2系列产品：不支持
  <!-- end id33 -->
  <!-- npu="A3" id34 -->
  - Atlas A3系列产品：不支持
  <!-- end id34 -->
  <!-- npu="950" id35 -->
  - Ascend 950PR&950DT系列产品：支持
  <!-- end id35 -->
- 9：串口录音。
  <!-- npu="310b" id36 -->
  - Atlas 200I/500 A2推理产品：不支持
  <!-- end id36 -->
  <!-- npu="310p" id37 -->
  - Atlas推理系列产品：不支持
  <!-- end id37 -->
  <!-- npu="910" id50 -->
  - Atlas训练系列产品：不支持
  <!-- end id50 -->
  <!-- npu="910b" id38 -->
  - Atlas A2系列产品：不支持
  <!-- end id38 -->
  <!-- npu="A3" id39 -->
  - Atlas A3系列产品：不支持
  <!-- end id39 -->
  <!-- npu="950" id40 -->
  - Ascend 950PR&950DT系列产品：支持
  <!-- end id40 -->
- 10：CCU日志。
  <!-- npu="310b" id42 -->
  - Atlas 200I/500 A2推理产品：不支持
  <!-- end id42 -->
  <!-- npu="310p" id43 -->
  - Atlas推理系列产品：不支持
  <!-- end id43 -->
  <!-- npu="910" id51 -->
  - Atlas训练系列产品：不支持
  <!-- end id51 -->
  <!-- npu="910b" id44 -->
  - Atlas A2系列产品：不支持
  <!-- end id44 -->
  <!-- npu="A3" id45 -->
  - Atlas A3系列产品：不支持
  <!-- end id45 -->
  <!-- npu="950" id46 -->
  - Ascend 950PR&950DT系列产品：支持
  <!-- end id46 -->
- 11：network日志。
  <!-- npu="310b" id53 -->
  - Atlas 200I/500 A2推理产品：不支持
  <!-- end id53 -->
  <!-- npu="310p" id54 -->
  - Atlas推理系列产品：不支持
  <!-- end id54 -->
  <!-- npu="910" id55 -->
  - Atlas训练系列产品：不支持
  <!-- end id55 -->
  <!-- npu="910b" id57 -->
  - Atlas A2系列产品：不支持
  <!-- end id57 -->
  <!-- npu="A3" id58 -->
  - Atlas A3系列产品：不支持
  <!-- end id58 -->
  <!-- npu="950" id52 -->
  - Ascend 950PR&950DT系列产品：支持
  <!-- end id52 -->

## 使用示例

1. 以root用户登录Host侧服务器。
2. 创建一个目录用来存放导出的文件（如/var/log/npu/report），确保当前用户有读、写、执行权限。
3. 进入以上目录，执行导出命令。

    ```sh
    msnpureport report
    ```

## 输出说明

Device侧的相关日志和文件被导出到Host侧，并存储到当前目录（如/var/log/npu/report）下以时间戳命名的文件夹中，具体如下：

```sh
├── hisi_logs
│   └── device-id   //对应设备的异常信息
│       ├── 时间戳  //以时间戳命名的文件夹，记录黑匣子日志、黑匣子的设备事件信息和黑匣子存储空间中的历史维测信息等
│       │   ├── bin
│       │   │   └── vmcore //vmcore文件，仅在Device OS心跳丢失时产生
│       ├── ub      //Unified Bus统一总线的维测信息，当前该目录软链接ub_info/mami，后续将废弃，相关信息请参考ub_info/mami。
│       │    ...
│       └── history.log  //记录各文件夹生成的异常场景信息
│   └── device_info.txt  //记录设备信息
├── slog
│   └── dev-os-id
│       ├── debug
│       │   ├── device-id
│       │   │   └── device-id_*.log  //Device侧非Control CPU上的系统类日志
│       │   ├── device-app-pid
│       │   │   └── device-app-pid.log  //Device侧应用进程产生的调试日志，仅在回传到Host失败时生成
│       │   └── device-os
│       │       └──device-os_*.log  //Device侧系统进程产生的调试日志
│       ├── run
│       │   ├── device-app-pid
│       │   │   └── device-app-pid.log  //Device侧应用进程产生的运行日志，仅在回传到Host失败时生成
│       │   ├── device-os
│       │   │   └── device-os_*.log  //Device侧系统进程产生的运行日志
│       │   └── event
│       │       └── event_*.log  //Device侧系统进程产生的EVENT日志
│       ├── security
│       │   ├── device-app-pid
│       │   │   └── device-app-pid.log  //Device侧应用进程产生的安全日志，仅在回传到Host失败时生成
│       │   └── device-os
│       │       └──device-os_*.log  //Device侧系统进程产生的安全日志
│       └── slogd  //日志工具自身产生的维测日志
│   └── host
│       └──host_kernel.log  //Host驱动内核日志
├── stackcore   // 进程coredump时的调用栈信息
│   └── dev-os-id
│       │    ... //不同产品型号产生的文件名可能不一样（例如stackcore.slogd.xx或coretrace.slogd.xx），以实际为准，使用asys工具可解析该文件
├── message  //Device侧OS系统日志
└── system_info
│   └── dev-os-id
│       │      ├── event_sched  //事件调度模块的维测信息
│       │      ├── hal  //驱动proc日志
│       │      ├── start_dmesg  //驱动proc日志
│       │      └── tee  //tee模块日志
│       │      └── ...
├── ub_info // ub领域的维测信息,使用asys工具可解析该文件。
│   └── dev-os-{id}
│       ├── ubctl //ubctl相关信息
│       ├── ...
│       └── mami  //mami领域的维测信息
│           ├── ubnl_dfx_statistic.bin
│           ├── ubnl_dfx_ssu_schedule.bin
│           ├── ubnl_dfx_config_item.bin
│           ├── ubmem_daw.bin
│           ├── ubtpl_acl.bin
│           ├── sl_to_vl.bin
│           └── ...
├── ao_info // AO领域的维测信息。
│   └── dev-os-{id}
│       ├── ao_self //AO区日志
│       ├── ao_cnt //AO计数
│       ├── ao_uart //串口录音
├── ccu_info // CCU领域的维测信息。
│   └── dev-os-{id}
│       └── ...
├── network_info // network领域的维测信息。
│   └── dev-os-{id}
│       └── ...
```

说明：使用asys工具解析ub领域维测信息请参考[《故障处理》](https://hiascend.com/document/redirect/CannCommunityasys)中的“UB文件解析”章节。

上述日志中_id_和_pid_分别代表Device ID和进程ID，请以实际为准；日志文件中的“\*”表示该日志文件创建时的时间戳。

<!-- npu="950" id19 -->
Device ID在如下场景，表示逻辑ID。

Ascend 950PR&950DT系列产品
<!-- end id19 -->
<!-- npu="A3,910b,910,310p,310b" id20 -->
Device ID在如下场景，表示物理ID。
<!-- end id20 -->
<!-- npu="310b" id21 -->
- Atlas 200I/500 A2推理产品
<!-- end id21 -->
<!-- npu="310p" id22 -->
- Atlas推理系列产品
<!-- end id22 -->
<!-- npu="910" id23 -->
- Atlas训练系列产品
<!-- end id23 -->
<!-- npu="910b" id24 -->
- Atlas A2系列产品
<!-- end id24 -->
<!-- npu="A3" id25 -->
- Atlas A3系列产品
<!-- end id25 -->
