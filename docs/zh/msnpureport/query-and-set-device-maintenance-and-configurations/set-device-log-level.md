# 设置Device侧系统类日志级别

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

## 命令功能

设置Device侧系统类日志的日志级别。

## 注意事项

- 该命令仅支持root用户执行。
- 容器场景下，命令执行时需要添加--docker参数，且需要配置msnpureport环境变量，示例如下：

    ```sh
    export PATH=/usr/local/Ascend/driver/tools:$PATH
    ```

## 命令格式

方式一（推荐）：

```sh
msnpureport config --set --log [options]
```

方式二：

```sh
msnpureport [options]
```

## 参数说明

**表1**  options参数说明

|参数|可选/必选|说明|
|--|--|--|
|-d <deviceId>或--device <deviceId>|可选|指定Device ID（逻辑ID），默认为0。<br>部分产品可能存在SMP工作模式，即多个Device共用一个OS组成一组，此时指定一个OS内的任意Device ID时，会导出该OS下的所有Device日志。|
|-g <level>或--global <level>|可选|设置全局级的日志级别。<br>- debug：表示DEBUG级别。<br>- info：表示INFO级别。<br>- warning：表示WARNING级别。<br>- error：表示ERROR级别。<br>- null：表示NULL级别，不输出日志。|
|-m \<module:level>或--module \<module:level>|可选|设置模块级的日志级别。<br>- module：模块名称。例如SLOG等。<br>- level：模块级别。取值为：debug：表示DEBUG级别；info：表示INFO级别；warning：表示WARNING级别；error：表示ERROR级别；null：表示NULL级别，不输出日志。|
|-e \<level>或--event \<level>|可选|设置是否开启Event日志。<br>- enable：开启Event日志。默认开启。<br>- disable：关闭Event日志。|

## 使用示例

1. 登录Host侧服务器。
2. 执行以下命令。

    设置Device侧全局日志级别，使用-g或--global参数，例如设置全局日志级别为info级别：

    ```sh
    msnpureport config --set --log -g info
    ```

    设置Device侧模块日志级别，使用-m或--module参数，例如设置CCE模块的日志级别为debug级别：

    ```sh
    msnpureport config --set --log -m CCE:debug
    ```

    设置是否开启Device侧event日志，使用-e或--event参数，例如：

    ```sh
    msnpureport config --set --log -e enable
    ```

    以上三种设置日志级别的方式，均可以使用-d或--device参数指定对应逻辑ID的设备，例如设置逻辑ID为1的设备全局日志级别为info：

    ```sh
    msnpureport config --set --log -g info -d 1
    ```

## 输出说明

无。

## SMP形态特殊说明

部分产品可能存在SMP工作模式，即多个Device共用一个OS组成一组，OS上部署Device共用的进程模块，各个Device小核上部署各自独有的模块，如TS、LP、IMP和IMU模块。此时通过-g、-d参数设置日志级别，表示修改OS上共用进程模块及-d指定Device小核上所有模块的日志级别。

关于物理ID与逻辑ID的说明如下：

- 使用npu-smi info命令查看到的设备上的NPU Device ID即为物理ID。假设物理ID的取值范围是0\~7，Device0\~Device3四个为一组，Device4\~Device7四个为一组。物理ID的分组情况请以设备实际情况为准。
- 查询到的物理ID会按照数字大小从小到大自上而下排列，那么对应的逻辑ID则从0开始按顺序为0\~n。假设查询到的物理ID为“0，1，4，5”，那么对应逻辑ID则为“0，1，2，3”。根据上一步的分组情况可判断物理ID“0，1”为一组，“4，5”为一组，那么可以推断出逻辑ID“0，1”为一组，“2，3”为一组。
