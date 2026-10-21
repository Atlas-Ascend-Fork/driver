# 查询Device维测配置信息

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

查询Device配置信息，查询出的内容包括Device日志级别、TaskSchedule是否自动复位加速器、AI Core上任务串联或并行执行模式等。

## 注意事项

容器场景下，命令执行时需要添加--docker参数，且需要配置msnpureport环境变量，示例如下：

```sh
export PATH=/usr/local/Ascend/driver/tools:$PATH
```

## 命令格式

方式一（推荐）：

```sh
msnpureport config --get [--device <deviceId>]
```

方式二（仅支持查询日志级别）：

```sh
msnpureport -r [--device <deviceId>]
```

## 参数说明

|参数|可选/必选|说明|
|--|--|--|
|-d \<deviceId>或--device \<deviceId>|可选|指定Device ID（逻辑ID），默认为0。<br>部分产品可能存在SMP工作模式，即多个Device共用一个OS组成一组，此时指定一个OS内的任意Device ID时，会导出该OS下的所有Device日志。|
|-r或--request|必选|查询Device侧slog系统类日志的级别，包括全局级、模块级和是否开启Event日志。如果不指定Device ID，默认查询Device 0的日志级别。该参数仅支持方式二传入。|

## 使用示例

1. 登录Host侧服务器。
2. 执行以下命令。

    ```sh
    msnpureport config --get
    ```

## 输出说明

命令执行完成后，输出如下信息。

```sh
+--------------------------------+--------------------------------+
| Device ID: 0                   | Current Configuration          |
+================================+================================+
| Icache check Range             | 1000                           |
| Accelerator Recover            | Enable                         |
| Aic Coremask                   | 0x1ff1cff                      |
| Aiv Coremask                   | 0x3ffff03f0ffff                |
| Aic Singlecommit               | Enable                         |
+-------- Log Level -------------+--------------------------------+
| Global Level                   | ERROR                          |
| Event Level                    | ENABLE                         |
| Module [IMP]                   | ERROR                          |
| Module [IMU]                   | ERROR                          |
| Module [LP]                    | ERROR                          |
| Module [TSDUMP]                | ERROR                          |
| Module [TS]                    | ERROR                          |
...
```

Aic Coremask和Aiv Coremask展示的字段为bitmap结构。
