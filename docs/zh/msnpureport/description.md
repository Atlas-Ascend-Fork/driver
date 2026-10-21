# 简介

msnpureport工具主要用于导出Device侧系统类日志以及其他维测信息，并提供Device侧的维测信息的查询和设置等功能。

该工具部署在昇腾驱动软件包中，路径为“_Driver安装目录_/driver/tools/msnpureport”，驱动软件包部署完成后，用户可以在安装环境任意目录下执行msnpureport命令。

## 主要功能

该工具有以下用途：

- 导出Device侧系统类日志以及其他维测信息。

  包括Device侧Control CPU和非Control CPU的系统类日志以及黑匣子日志、Device进程coredump时的调用栈信息、Host侧驱动内核日志等维测信息。

- 设置Device配置信息。

  包括配置Device日志级别、设置TaskSchedule是否自动复位加速器、设置AI Core上任务串联或并行执行模式等。

- 查询Device配置信息。

查询出的内容包括Device日志级别、TaskSchedule是否自动复位加速器、AI Core上任务串联或并行执行模式等。

## 使用约束

- msnpureport工具仅适用于Ascend EP形态。
- msnpureport不支持在昇腾虚拟化实例场景下使用。
- 安全起见，不建议在加锁目录中执行命令。

    在加锁的目录下（使用**lsattr**命令查看目录属性，有“i”选项的为加锁目录），用户没有权限执行msnpureport命令。如果用户想在该目录下运行该脚本，可以通过**chattr** **-i** **<加锁的目录\>**命令，将目录的“i”选项撤销。命令执行完后建议通过**chattr** **+i** <加锁的目录\>命令，将目录的“i”选项加上。

- 如果非root用户需要使用msnpureport，安装驱动时需携带--install-for-all参数，确保非root用户拥有Device侧权限。
