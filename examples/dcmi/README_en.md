# Example Usage Guide

The dcmi directory provides a series of DCMI interface examples, including configuration, query, reset, and so on, for developer reference to help developers quickly get started.

## Example List

| Example Directory  | Subdirectory                                             | Feature Introduction                                                     |
| --------- | -------------------------------------------------- | ------------------------------------------------------------ |
| configure | [0_configure_manager](./dcmi/0_configure_manager/) | This example demonstrates DCMI initialization to user configuration, device sharing status configuration, and so on.         |
| query     | [1_query_npuinfo](./dcmi/1_query_npuinfo/)         | This example demonstrates DCMI querying PCIE information, Board information, Flash information, and so on.       |
| reset     | [2_chip_reset](./dcmi/2_chip_reset/)               | This example demonstrates DCMI reset: resetting Ascend AI processor through PCIE standard hot reset process, including inband and outband reset modes. |

## Environment Preparation

Get the dcmi_interface_api.h file from the "/usr/local/dcmi/" directory of the environment where driver and npu-smi tools are installed.

Before compiling and running examples, obtain firmware, driver, and CANN software packages for installation. For detailed steps, refer to the [CANN Software Installation Guide](https://hiascend.com/document/redirect/CannCommunityInstSoftware). After selecting the installation scenario, see the "Installing NPU Driver Firmware" section.

## Compilation and Running

1. Set environment variables.

```bash
# Specify dcmi library path in driver and npu-smi tool installation environment
export LD_LIBRARY_PATH="/usr/local/dcmi${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

2. Download Driver repository code and upload it to the environment where npu-smi tool software is installed. Switch to the example directory.

```bash
# Using 0_configure_manager(user) 0_set_user_config example as an example
# Directory and module correspondence
# 0_configure_manager -- user
# --1)0_set_user_config Set user configuration
# --2)1_set_device_share Set device sharing status
# 1_query_npuinfo     -- query
# --1)0_get_pcie_info Get specified device PCIe information
# --2)1_get_board_info Get specified device board information
# --3)2_get_flash_info Get flash information
# 2_chip_reset        -- reset
# --1)0_internal_reset Reset chip
# --2)1_external_reset Outband reset (only supported in standard PCIe card environment)
cd ${git_clone_path}/examples/dcmi/dcmi/
```

3. Execute the following command to run the example.

```bash
# Run method is bash run.sh corresponding module example subdirectory number
bash run.sh user 0
```
