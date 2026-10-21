# 连续导出Device侧系统类日志和其他维测信息

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

msnpureport提供常驻进程模式持续导出Device侧系统日志。在模型推理/训练任务运行时，执行msnpureport命令导出Device侧日志文件时，可能会出现日志导出不全的情况，比如：Device出现异常导致无法导出Device侧日志、任务执行时间过长导致Device侧日志被清理掉。此时可以在任务执行之前，在Host侧运行该命令连续导出Device侧的日志和文件，确保能够获取到Device异常前的所有日志和文件，便于定位问题。

## 注意事项

- 该命令仅支持root用户执行。且须确保执行该命令的当前路径对普通用户无访问权限，否则普通用户也能操作导出的日志文件，存在恶意删除日志文件或泄露系统信息等安全风险。
- 不建议在同路径同时刻并发使用以免造成落盘时间戳目录名冲突；一个Device不支持并发使用。
- 容器场景不支持使用该命令（特权容器场景除外，无此限制）。
- 在异常场景下，可能会出现连续导出Device侧日志和文件失败的情况。
- 如果用户想要终止导出Device侧日志和文件或者模型推理/训练完成，需要使用“Ctrl+C”或`kill -15 pid`命令结束进程。

    其中pid代表连续导出日志的脚本进程ID，可以通过**ps -elf | grep msnpureport**命令进行查询。

## 命令格式

```sh
msnpureport report --permanent [options]
```

## 参数说明

**表1**  options参数说明

|参数|可选/必选|说明|
|--|--|--|
|-d <deviceId>或--device <deviceId>|可选|指定Device ID（逻辑ID），默认导出所有Device日志。<br>部分产品可能存在SMP工作模式，即多个Device共用一个OS组成一组，此时指定一个OS内的任意Device ID时，会导出该OS下的所有Device日志。<br>SMP工作模式下，一个OS内不支持多个导出进程并发执行，例如：device 0和device 1共用一个OS时，不能同时执行**msnpureport report --permanent -d 0**和**msnpureport report --permanent -d 1**|
|--sys_log_num <sys_log_num>|可选|设置Device侧系统进程产生的调试日志或运行日志的文件数量。<br>取值范围：[1,1000]，默认值为10。注意该参数控制的是调试日志目录或运行日志目录下各自的最大文件数量。当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--sys_log_size <sys_log_size>|可选|设置Device侧系统进程产生的调试日志和运行日志的单个文件大小。取值范围：[1,100]，单位M，默认值为10。<br>当文件大小大于该值时，日志将生成在新的文件中。|
|--event_log_num <event_log_num>|可选|设置Device侧系统进程产生的EVENT日志的文件数量。<br>取值范围：[1,1000]，默认值为10。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--event_log_size <event_log_size>|可选|设置Device侧系统进程产生的EVENT日志的单个文件大小。<br>取值范围：[1,100]，单位M，默认值为10。<br>当文件大小大于该值时，日志将生成在新的文件中。|
|--sec_log_num <sec_log_num>|可选|设置Device侧系统进程产生的安全日志的文件数量。<br>取值范围：[1,1000]，默认值为10。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--sec_log_size <sec_log_size>|可选|设置Device侧系统进程产生的安全日志的单个文件大小。<br>取值范围：[1,100]，单位M，默认值为10。<br>当文件大小大于该值时，日志将生成在新的文件中。|
|--fw_log_num <fw_log_num>|可选|设置非Control CPU上的系统类日志的文件数量。<br>取值范围：[1,1000]，默认值为10。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--fw_log_size <fw_log_size>|可选|设置非Control CPU上的系统类日志的单个文件大小。<br>取值范围：[1,100]，单位M，默认值为10。<br>当文件大小大于该值时，日志将生成在新的文件中。|
|--stackcore_num <stackcore_num>|可选|设置stackcore目录下的文件数量，具体指以下文件数量：<br>- 当前目录/\*/stackcore/*dev-os-id*/stackcore.slogd.xxx*<br>- 当前目录/\*/stackcore/*dev-os-id*/udf/stackcore.slogd.xxx <br>取值范围：[1,100]，默认值为50。该参数控制的是上述两个目录下各自的最大文件数量。不同产品型号产生的文件名可能不一样，部分型号下文件名为coretrace.slogd.xx。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--bbox_num <bbox_num>|可选|设置黑匣子日志文件夹数量。<br>取值范围：[1,10000]，默认值为1000。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--fault_event_num <fault_event_num>|可选|设置Proc文件和Device侧OS系统日志的文件夹（即fault_event_{故障码}_*）数量，当Device故障触发时生成。<br>取值范围：[1,100]，默认值为10。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--slogd_log_num <slogd_log_num>|可选|设置日志工具自身产生的维测日志的文件数量。<br>取值范围：[1,100]，默认值为2。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|--app_dir_num <app_dir_num>|可选|Device产生的应用类日志会自动回传到Host侧，但部分场景会出现未来得及回传的情况。通过该参数可以配置Device侧未来得及回传的剩余日志文件数量。<br>取值范围：[1,100]，默认值为8。<br>当日志文件数目大于该数目时，新日志覆盖最早的日志。|
|-o <path>或--output <path>|可选|指定导出文件路径。要求配置为相对路径，且该路径必须已存在。不建议配置到单次导出任务已存在的时间戳目录下。<br>配置示例：--output mypath|

## 使用示例

1. 以root用户登录Host侧服务器。
2. 创建一个目录用来存放导出的文件（如/var/log/npu/report/），确保当前用户有读、写、执行权限。
3. 进入以上目录，执行导出命令。

    **msnpureport** **report --permanent**

4. 如果用户想要终止导出Device侧日志和文件或者模型推理/训练完成，需要使用“Ctrl+C”或`kill -15 pid`命令结束进程。

    其中pid代表连续导出日志的脚本进程ID，可以通过**ps -elf | grep msnpureport**命令进行查询。

## 输出说明

Device侧的相关日志和文件被导出到Host侧，并存储到当前目录（如/var/log/npu/report）下以时间戳命名的文件夹中，具体如下：

```sh
├── hisi_logs  //仅在监测到device侧log-daemon进程心跳异常时，做一次临终异常上报
│   └── device-id
│       ├── 时间戳  //以时间戳命名的文件夹，记录黑匣子日志、黑匣子的设备事件信息和黑匣子存储空间中的历史维测信息等
│       └── history.log  //记录各文件夹生成的异常场景信息，如Device心跳丢失等
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
├── stackcore   // 进程coredump时的调用栈信息
│   └── dev-os-id
│       │    ... //不同产品型号产生的文件名可能不一样（例如stackcore.slogd.xx或coretrace.slogd.xx），请以实际为准，如需解析相关文件，可以使用asys工具
└── system_info
    └── device-id
    │   └── fault_event_{故障码}_*  //Proc文件和Device侧OS系统日志，Device故障触发时生成
    │      ├── event_sched // 事件调度模块的维测信息
    │      ├── hal // 驱动proc日志
    │      ├── message // Device侧OS系统日志
    │      └── ...
```

上述日志中id和pid分别代表Device ID（物理ID）和进程ID，请以实际为准；日志文件中的“\*”表示该日志文件创建时的时间戳。asys工具详细信息请参考[《故障处理》](https://hiascend.com/document/redirect/CannCommunityasys)。

<!-- npu="950" id7 -->
Device ID在如下场景，表示逻辑ID。

Ascend 950PR&950DT系列产品
<!-- end id7 -->

<!-- npu="A3,910b,910,310p,310b" id8 -->
Device ID在如下场景，表示物理ID。
<!-- end id8 -->
<!-- npu="310b" id9 -->
Atlas 200I/500 A2推理产品
<!-- end id9 -->
<!-- npu="310p" id10 -->
Atlas推理系列产品
<!-- end id10 -->
<!-- npu="910" id11 -->
Atlas训练系列产品
<!-- end id11 -->
<!-- npu="910b" id12 -->
Atlas A2系列产品
<!-- end id12 -->

<!-- npu="A3" id13 -->Atlas A3系列产品<!-- end id13 -->
