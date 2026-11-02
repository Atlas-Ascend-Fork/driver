# 样例使用指导

dcmi目录下提供了一系列DCMI接口样例，包括配置、查询、复位等，供开发者参考，帮助开发者快速入门。

## 样例列表

| 样例目录  | 子目录                                             | 功能介绍                                                     |
| --------- | -------------------------------------------------- | ------------------------------------------------------------ |
| configure | [0_configure_manager](./dcmi/0_configure_manager/) | 本样例展示了从DCMI初始化到用户配置，设备共享状态配置等。         |
| query     | [1_query_npuinfo](./dcmi/1_query_npuinfo/)         | 本样例展示了DCMI查询PCIE信息，Board信息，Flash信息等。       |
| reset     | [2_chip_reset](./dcmi/2_chip_reset/)               | 本样例展示了DCMI复位：通过PCIE标准热复位流程复位昇腾AI处理器，包含带内和带外复位模式。 |

## 环境准备

从已安装驱动和npu-smi工具环境的“/usr/local/dcmi/”目录下获取 dcmi_interface_api.h文件。

编译运行样例前，需获取固件、驱动及CANN软件包安装，详情步骤请参见《[CANN软件安装指南](https://hiascend.com/document/redirect/CannCommunityInstSoftware)》（选择安装场景后，参见“安装NPU驱动固件”章节）。

## 编译运行

1.设置环境变量。

```bash
# 安装驱动和npu-smi工具环境指定dcmi库路径
export LD_LIBRARY_PATH="/usr/local/dcmi${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

2.下载Driver仓代码并上传至安装npu-smi工具软件的环境，切换到样例目录。

```bash
# 此处以0_configure_manager(user)下的0_set_user_config样例为例
# 目录与模块对应关系
# 0_configure_manager -- user
# --1)0_set_user_config 设置用户配置
# --2)1_set_device_share 设置设备共享状态
# 1_query_npuinfo     -- query
# --1)0_get_pcie_info 获取指定设备PCIe 信息
# --2)1_get_board_info 获取指定设备的board信息
# --3)2_get_flash_info 获取flash信息
# 2_chip_reset        -- reset
# --1)0_internal_reset 复位芯片
# --2)1_external_reset 带外复位（仅支持标准PCIe卡环境）
cd ${git_clone_path}/examples/dcmi/dcmi/
```

3.执行以下命令运行样例。

```bash
# 运行方式为 bash run.sh 对应模块 样例子目录序号
bash run.sh user 0
```
