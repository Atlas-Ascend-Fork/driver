# 连续导出日志和文件（脚本方式）

在模型推理/训练任务运行时，执行msnpureport命令导出Device侧日志文件或者Host驱动日志时，可能会出现日志导出不全的情况，比如：Device出现异常导致无法导出Device侧日志、任务执行时间过长导致Device侧日志被清理掉。此时可以在任务执行之前，在Host侧运行msnpureport\_auto\_export.sh脚本连续导出Device侧的日志和文件，确保能够获取到Device异常前的所有日志文件，便于定位问题。更多说明如下：

- 更推荐使用命令行方式连续导出日志和文件，具体请参考[连续导出Device侧系统类日志和其他维测信息](../export-device-logs-and-files/consecutively-export-device-logs-and-files.md)。
- 该操作仅支持root用户执行。且须确保<logAbsolutePath\>指定的存储路径对普通用户无访问权限，否则普通用户也能操作导出的日志文件，存在恶意删除日志文件或泄露系统信息等安全风险。
- msnpureport\_auto\_export.sh脚本不支持容器场景，部署容器时，禁止将msnpureport\_auto\_export.sh脚本映射到容器内。
- msnpureport\_auto\_export.sh脚本会持续采集所有Device侧日志，由于并发执行会导致采集内容重复，浪费系统资源，因此不建议并发执行。
- 在异常场景下，可能会出现连续导出Device侧日志和文件失败的情况。
- 如果用户想要终止导出Device侧日志和文件或者模型推理/训练完成，需要使用“Ctrl+C”或`kill -15 pid`命令结束进程。

  其中pid代表连续导出日志和文件的脚本进程ID，可以通过**ps -elf | grep msnpureport\_auto\_export.sh**命令进行查询。

## 操作步骤

1. 以root用户登录Host侧服务器。
2. 获取msnpureport\_auto\_export.sh脚本。

    msnpureport\_auto\_export.sh脚本在驱动Driver的安装目录下，路径为“_Driver安装目录_/driver/tools/msnpureport\_auto\_export.sh”。

3. 在某个有执行权限的目录（如“/home/work”）下执行如下命令，运行脚本。

    ```sh
    Driver安装目录/driver/tools/msnpureport_auto_export.sh <timeInterval> <logAbsolutePathCapacity> <logAbsolutePath>
    ```

    命令示例：

    ```sh
    /usr/local/Ascend/driver/tools/msnpureport_auto_export.sh 2 10 /home/log/
    ```

    在加锁的目录下（使用**lsattr**命令查看目录属性，有“i”选项的为加锁目录），用户没有权限运行msnpureport\_auto\_export.sh脚本。如果用户想在该目录下运行该脚本，可以通过`chattr -i <加锁的目录\>`命令，将目录的“i”选项撤销。脚本运行完后建议通过`chattr +i <加锁的目录\>`命令，将目录的“i”选项加上。为了安全起见，不建议在加锁目录中运行脚本。

    其中各参数解释如表1所示：

**表1**  参数说明

|参数|说明|
|--|--|
|\<timeInterval >|导出日志和文件的间隔时间。取值为大于0的整数，单位是s，如：2s。|
|\<logAbsolutePathCapacity>|导出日志和文件的存储目录容量。取值为大于等于2的整数，单位是G，如：10G。|
|\<logAbsolutePath>|导出日志和文件的存储路径，仅支持配置为绝对路径。如：/home/log/。|
|\<logClearFlag>|脚本启动时是否清空路径下的日志。取值：1-是，0-否，默认为1。|

 脚本运行成功后，日志和文件将存储在运行脚本时指定的存储路径下（如“/home/log/”），如果不存在该目录，会自动进行创建。该目录下会自动创建如下子目录：

**表2**  主要目录及文件说明

|目录|说明|
|--|--|
|msnpureport_log_new|导出日志和文件的存储目录。该目录下包含如下子目录。<br>- hisi_logs<br>-message <br>-system_info<br>-slog<br>-stackcore<br>其中hisi_logs目录下的history.log文件和message目录下的messages/messages.0文件是在Device侧的同名文件中进行老化的，所以每次导出之后需要将导出内容追加到同一目录下的history_new.log和message_new.log文件中并进行去重，来获取所有日志文件。<br>其他目录的日志文件在Device侧是以时间戳命名的，是通过删除较早时间戳的日志文件进行老化的，所以每次导出之后只需将导出内容拷贝覆盖就可以获取所有日志文件。|
|msnpureport_log_old|老化的日志和文件的存储目录。<br>如果msnpureport_log_new目录存储的日志容量超过运行脚本时指定的存储目录容量的一半（如10/2=5G），将自动清空msnpureport_log_old目录下的日志和文件，再将msnpureport_log_new目录下存储的日志和文件全部移动到msnpureport_log_old目录下。|
