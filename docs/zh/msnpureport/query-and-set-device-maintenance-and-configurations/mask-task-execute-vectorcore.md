# 屏蔽指定Vector Core上的任务执行

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
- Atlas 200I/500 A2推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas训练系列产品：不支持
<!-- end id6 -->

## 命令功能

屏蔽指定Vector Core上的任务执行，以便排查哪个Vector Core故障。

建议单算子调用场景下使用该命令排查问题。

<!-- npu="950,A3,910b" id7 -->
对于AI Core分离架构的产品来说，此处的AI Core特指AIC核。分离架构是将AI Core拆成矩阵计算（AI Cube，AIC）和向量计算（AI Vector，AIV）两个独立的核，从而实现矩阵计算与向量计算的解耦。典型的分离架构产品包括：
<!-- end id7 -->

<!-- npu="910b" id8 -->

- Atlas A2系列产品
<!-- end id8 -->
<!-- npu="A3" id9 -->
- Atlas A3系列产品
<!-- end id9 -->
<!-- npu="950" id10 -->
- Ascend 950PR&950DT系列产品
<!-- end id10 -->

## 注意事项

- 该命令仅支持root用户执行。
- 容器场景下，命令执行时需要添加--docker参数，且需要配置msnpureport环境变量，示例如下：

    ```sh
    export PATH=/usr/local/Ascend/driver/tools:$PATH
    ```

- 昇腾AI应用进程运行过程中不建议执行该命令，否则可能会导致应用进程运行异常，建议应用进程退出后执行该命令。
- 对于涉及核间同步的算子，算子必须一次拿到所需要的核数才执行，因此使用该命令屏蔽指定核后，若屏蔽后的实际核数小于算子需要的核数，会导致算子执行失败。

    屏蔽后的实际核数可以使用[查询Device维测配置信息](../query-and-set-device-maintenance-and-configurations/query-device-maintenance.md)中命令查询。算子需要的核数可以从Host侧应用程序的调试日志中查看，调试日志默认路径为$HOME/ascend/log/debug/plog/plog-_pid_\_\*.log，可以搜索关键core\_num查看算子需要的核数。

## 命令格式

```sh
msnpureport config --set [--device <deviceId>] --aiv_switch <switchflag> --coreid <coreid>
```

## 参数说明

|参数|可选/必选|说明|
|--|--|--|
|-d <deviceId>或--device <deviceId>|可选|指定Device ID（逻辑ID），默认为0。<br>部分产品可能存在SMP工作模式，即多个Device共用一个OS组成一组，此时指定一个OS内的任意Device ID时，会导出该OS下的所有Device日志。|
|--aiv_switch <switchflag>|必选|是否屏蔽Vector Core，取值说明如下：<br>- 0：屏蔽Vector Core。设置为该值后，若重启运行环境，默认不屏蔽Vector Core。<br>- 1：不屏蔽Vector Core，默认该选项。|
|--coreid \<coreid>|必选|指定core id，取值范围为[0,255]。多个core id之间以英文逗号分隔，最多可同时指定4个Vector Core，只要有一个core id无效，则该命令会返回报错。|

若想恢复初始值（即所有core id都不屏蔽），可将--aic_switch设置为1，将--coreid设置为0xFFFF（0xFFFF不支持多个core id组合使用）。

## 使用示例

1. 登录Host侧服务器。
2. 执行以下命令。

    ```sh
    msnpureport config --set --device 0 --aiv_switch 0 --coreid 5,6
    ```
