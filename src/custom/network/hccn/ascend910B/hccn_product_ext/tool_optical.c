/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include "tool_recovery.h"
#include "tool_ext.h"
#include "cmd.h"
#include "dsmi_common_interface.h"
#include "weak_passwd_dict.h"
#include "tls.h"
#include "tool.h"
#include "hccn_dfx_struct.h"
#include "log.h"
#include "tool_ops.h"
#include "tool_roce.h"

#define BIT_0 0
#define BIT_1 1
#define BIT_2 2
#define BIT_3 3
#define BIT_4 4
#define BIT_5 5
#define BIT_6 6
#define BIT_7 7
#define VALUE_WIDTH 12
#define DR4_FLAG 0x42
#define MW_TO_DBM_TEN 10
#define RX_FLAG 0
#define TX_FLAG 1
#define VOLTAGE_UNIT_CONVERT 10000
#define BIAS_UNIT_CONVERT 1000
#define NEGATIVE_UNIT_CONVERT (-1)
#define LOW_16_BITS 0xFFFF
#define SHIFT_8 8
#define MAX_LANE_NUM 4
#define NUM_2 2

#define MW_TO_DBM(value) (MW_TO_DBM_TEN * log10f(value))
#define GET_BIT(value, n) (((value) >> (n)) & 0x1U)
#define SET_BIT(value, n) ((value) = ((value) & (~(0x1U << (n)))) | (0x1U << (n)))

#define OPTICAL_SNR_QUERY_SWITCH "optical_snr_query_switch"

struct power_info {
    union {
        struct {
            float power0;
            float power1;
            float power2;
            float power3;
            float high_alarm;
            float high_warn;
            float low_alarm;
            float low_warn;
        };
        float array[SHIFT_8];
    };
};

struct tx_bias_info {
    union {
        struct {
            float bias0;
            float bias1;
            float bias2;
            float bias3;
            float high_alarm;
            float high_warn;
            float low_alarm;
            float low_warn;
        };
        float array[SHIFT_8];
    };
};

STATIC bool tool_optical_info_not_support_query(struct ds_optical_info *info)
{
    return ((info->present != TOOL_XSFP_PRESENT) || (info->temp == TEMP_INVALID) ||
            (info->additional_info.voltage == 0));
}

STATIC void tool_show_state_flag_dr4(struct ds_optical_info *info, unsigned int bit_info, char *print_info, int sflag)
{
    if (sflag == 0) {
        TOOL_PRINT_INFO("%s     %-10s| %-10s| %-10s| %-10s", print_info,
                        (GET_BIT(bit_info, BIT_0)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_1)) ? "Abnormal " : "Normal   ", "-", "-");
    } else {
        TOOL_PRINT_INFO("%s     %-10s| %-10s| %-10s| %-10s", print_info,
                        (GET_BIT(bit_info, BIT_2)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_3)) ? "Abnormal " : "Normal   ", "-", "-");
    }
}

STATIC void tool_show_state_flag_not_dr4(struct ds_optical_info *info, unsigned int bit_info, char *print_info,
                                         int sflag)
{
    if (sflag == 0) {
        TOOL_PRINT_INFO("%s     %-10s| %-10s| %-10s| %-10s", print_info,
                        (GET_BIT(bit_info, BIT_0)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_1)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_2)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_3)) ? "Abnormal " : "Normal   ");
    } else {
        TOOL_PRINT_INFO("%s     %-10s| %-10s| %-10s| %-10s", print_info,
                        (GET_BIT(bit_info, BIT_4)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_5)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_6)) ? "Abnormal " : "Normal   ",
                        (GET_BIT(bit_info, BIT_7)) ? "Abnormal " : "Normal   ");
    }
}

STATIC void tool_show_state_flag_print_v2(struct ds_optical_info *info, int bit_info, char *print_info, int sflag)
{
    if (tool_optical_info_not_support_query(info)) {
        TOOL_PRINT_INFO("%s     %-10s| %-10s| %-10s| %-10s", print_info, "-", "-", "-", "-");
        return;
    }

    if (strcmp(print_info, "TxLos Flag  ") == 0 || strcmp(print_info, "TxLol Flag  ") == 0) {
        tool_show_state_flag_not_dr4(info, (unsigned int)bit_info, print_info, sflag);
        return;
    }

    if (info->dr4_flag == DR4_FLAG) {
        tool_show_state_flag_dr4(info, (unsigned int)bit_info, print_info, sflag);
    } else {
        tool_show_state_flag_not_dr4(info, (unsigned int)bit_info, print_info, sflag);
    }
}

STATIC void tool_show_txbias_info_print_v2(struct ds_optical_info *info, struct tx_bias_info *tx_bias)
{
    int lane_id;

    if (tool_optical_info_not_support_query(info)) {
        for (lane_id = 0; lane_id < MAX_LANE_NUM; lane_id++) {
            TOOL_PRINT_INFO("Bias lane%d(mA)      %-12s%-12s%-12s%-12s%-12s%-12s", lane_id, "-", "-", "-", "-", "-",
                            "-");
        }
        return;
    }

    TOOL_PRINT_INFO("Bias lane0(mA)      %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s", tx_bias->bias0, tx_bias->high_alarm,
                    tx_bias->high_warn, tx_bias->low_alarm, tx_bias->low_warn, "-");
    TOOL_PRINT_INFO("Bias lane1(mA)      %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s", tx_bias->bias1, tx_bias->high_alarm,
                    tx_bias->high_warn, tx_bias->low_alarm, tx_bias->low_warn, "-");
    if (info->dr4_flag == DR4_FLAG) {
        TOOL_PRINT_INFO("Bias lane2(mA)      %-12s%-12s%-12s%-12s%-12s%-12s", "-", "-", "-", "-", "-", "-");
        TOOL_PRINT_INFO("Bias lane3(mA)      %-12s%-12s%-12s%-12s%-12s%-12s", "-", "-", "-", "-", "-", "-");
    } else {
        TOOL_PRINT_INFO("Bias lane2(mA)      %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s", tx_bias->bias2,
                        tx_bias->high_alarm, tx_bias->high_warn, tx_bias->low_alarm, tx_bias->low_warn, "-");
        TOOL_PRINT_INFO("Bias lane3(mA)      %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s", tx_bias->bias3,
                        tx_bias->high_alarm, tx_bias->high_warn, tx_bias->low_alarm, tx_bias->low_warn, "-");
    }
}

STATIC void tool_get_tx_bias(struct ds_optical_info *info, struct tx_bias_info *tx_bias, const int sflag)
{
    int lane_id;
    float *tx_bias_array = (float *)tx_bias->array;

    if (info->dr4_flag == DR4_FLAG) {
        if (sflag == 0) {
            tx_bias->bias0 = (float)((info->additional_info.tx_bias[CHANNEL_0_H] << SHIFT_8) +
                                     (unsigned char)info->additional_info.tx_bias[CHANNEL_0_L]) /
                             OPTICAL_BIAS_DEVISOR;
            tx_bias->bias1 = (float)((info->additional_info.tx_bias[CHANNEL_1_H] << SHIFT_8) +
                                     (unsigned char)info->additional_info.tx_bias[CHANNEL_1_L]) /
                             OPTICAL_BIAS_DEVISOR;
        } else {
            tx_bias->bias0 = (float)((info->additional_info.tx_bias[CHANNEL_2_H] << SHIFT_8) +
                                     (unsigned char)info->additional_info.tx_bias[CHANNEL_2_L]) /
                             OPTICAL_BIAS_DEVISOR;
            tx_bias->bias1 = (float)((info->additional_info.tx_bias[CHANNEL_3_H] << SHIFT_8) +
                                     (unsigned char)info->additional_info.tx_bias[CHANNEL_3_L]) /
                             OPTICAL_BIAS_DEVISOR;
        }
    } else {
        // 由CHANNEL_0_H = 0, CHANNEL_0_L = 1, CHANNEL_1_H = 2, CHANNEL_1_L = 3，...
        // 可以推出：CHANNEL_[lane_id]_H = 2 * lane_id, CHANNEL_[lane_id]_L = 2 * lane_id + 1
        for (lane_id = 0; lane_id < MAX_LANE_NUM; lane_id++) {
            tx_bias_array[lane_id] = (float)((info->additional_info.tx_bias[lane_id * NUM_2 + sflag] << SHIFT_8) +
                                             (unsigned char)
                                                 info->additional_info.tx_bias[lane_id * NUM_2 + 1 + sflag]) /
                                     OPTICAL_BIAS_DEVISOR;
        }
    }

    tx_bias->high_alarm = ((unsigned int)info->additional_info.tx_bias_high_alarm_threshold & LOW_16_BITS) /
                          OPTICAL_BIAS_DEVISOR;
    tx_bias->high_warn = ((unsigned int)info->additional_info.tx_bias_high_threshold & LOW_16_BITS) /
                         OPTICAL_BIAS_DEVISOR;
    tx_bias->low_alarm = ((unsigned int)info->additional_info.tx_bias_low_alarm_threshold & LOW_16_BITS) /
                         OPTICAL_BIAS_DEVISOR;
    tx_bias->low_warn = ((unsigned int)info->additional_info.tx_bias_low_threshold & LOW_16_BITS) /
                        OPTICAL_BIAS_DEVISOR;
}

STATIC void tool_show_txbias_info_v2(struct ds_optical_info *info, const int sflag)
{
    struct tx_bias_info tx_bias = {0};

    tool_get_tx_bias(info, &tx_bias, sflag);
    tool_show_txbias_info_print_v2(info, &tx_bias);
}

STATIC void tool_show_power_info_print_default_v2(int power_flag)
{
    int lane_id;

    for (lane_id = 0; lane_id < MAX_LANE_NUM; lane_id++) {
        TOOL_PRINT_INFO("%2sPower Lane%d(dBm)  %-12.2s%-12.2s%-12.2s%-12.2s%-12.2s%-12s",
                        power_flag == RX_FLAG ? "Rx" : "Tx", lane_id, "-", "-", "-", "-", "-", "-");
    }
}

STATIC void tool_show_power_info_print_v2(struct ds_optical_info *info, int power_flag, struct power_info *power)
{
    if (tool_optical_info_not_support_query(info)) {
        tool_show_power_info_print_default_v2(power_flag);
        return;
    }
    TOOL_PRINT_INFO("%sPower Lane0(dBm)  %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s", power_flag == RX_FLAG ? "Rx" : "Tx",
                    power->power0, power->high_alarm, power->high_warn, power->low_alarm, power->low_warn, "-");
    TOOL_PRINT_INFO("%2sPower Lane1(dBm)  %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s",
                    power_flag == RX_FLAG ? "Rx" : "Tx", power->power1, power->high_alarm, power->high_warn,
                    power->low_alarm, power->low_warn, "-");
    if (info->dr4_flag == DR4_FLAG) {
        TOOL_PRINT_INFO("%2sPower Lane2(dBm)  %-12.2s%-12.2s%-12.2s%-12.2s%-12.2s%-12s",
                        power_flag == RX_FLAG ? "Rx" : "Tx", "-", "-", "-", "-", "-", "-");
        TOOL_PRINT_INFO("%2sPower Lane3(dBm)  %-12.2s%-12.2s%-12.2s%-12.2s%-12.2s%-12s",
                        power_flag == RX_FLAG ? "Rx" : "Tx", "-", "-", "-", "-", "-", "-");
    } else {
        TOOL_PRINT_INFO("%2sPower Lane2(dBm)  %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s",
                        power_flag == RX_FLAG ? "Rx" : "Tx", power->power2, power->high_alarm, power->high_warn,
                        power->low_alarm, power->low_warn, "-");
        TOOL_PRINT_INFO("%2sPower Lane3(dBm)  %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s",
                        power_flag == RX_FLAG ? "Rx" : "Tx", power->power3, power->high_alarm, power->high_warn,
                        power->low_alarm, power->low_warn, "-");
    }
}

STATIC void tool_get_tx_rx_power(struct ds_optical_info *info, int power_flag, struct power_info *power,
                                 const int sflag)
{
    unsigned char *tx_rx_power = NULL;
    int lane_id;
    float *power_array = (float *)power->array;

    if (power_flag == TX_FLAG) {
        tx_rx_power = info->additional_info.tx_power;
        power->high_alarm = MW_TO_DBM((float)info->additional_info.tx_power_high_alarm_threshold /
                                      OPTICAL_POWER_DEVISOR);
        power->high_warn = MW_TO_DBM((float)info->additional_info.tx_power_high_threshold / OPTICAL_POWER_DEVISOR);
        power->low_alarm = MW_TO_DBM((float)info->additional_info.tx_power_low_alarm_threshold / OPTICAL_POWER_DEVISOR);
        power->low_warn = MW_TO_DBM((float)info->additional_info.tx_power_low_threshold / OPTICAL_POWER_DEVISOR);
    } else {
        tx_rx_power = info->additional_info.rx_power;
        power->high_alarm = MW_TO_DBM((float)info->additional_info.rx_power_high_alarm_threshold /
                                      OPTICAL_POWER_DEVISOR);
        power->high_warn = MW_TO_DBM((float)info->additional_info.rx_power_high_threshold / OPTICAL_POWER_DEVISOR);
        power->low_alarm = MW_TO_DBM((float)info->additional_info.rx_power_low_alarm_threshold / OPTICAL_POWER_DEVISOR);
        power->low_warn = MW_TO_DBM((float)info->additional_info.rx_power_low_threshold / OPTICAL_POWER_DEVISOR);
    }

    if (info->dr4_flag == DR4_FLAG) {
        if (sflag == 0) {
            power->power0 = MW_TO_DBM(
                (float)((tx_rx_power[CHANNEL_0_H] << SHIFT_8) + (unsigned char)tx_rx_power[CHANNEL_0_L]) /
                OPTICAL_POWER_DEVISOR);
            power->power1 = MW_TO_DBM(
                (float)((tx_rx_power[CHANNEL_1_H] << SHIFT_8) + (unsigned char)tx_rx_power[CHANNEL_1_L]) /
                OPTICAL_POWER_DEVISOR);
        } else {
            power->power0 = MW_TO_DBM(
                (float)((tx_rx_power[CHANNEL_2_H] << SHIFT_8) + (unsigned char)tx_rx_power[CHANNEL_2_L]) /
                OPTICAL_POWER_DEVISOR);
            power->power1 = MW_TO_DBM(
                (float)((tx_rx_power[CHANNEL_3_H] << SHIFT_8) + (unsigned char)tx_rx_power[CHANNEL_3_L]) /
                OPTICAL_POWER_DEVISOR);
        }
    } else {
        // 由CHANNEL_0_H = 0, CHANNEL_0_L = 1, CHANNEL_1_H = 2, CHANNEL_1_L = 3，...
        // 可以推出：CHANNEL_[lane_id]_H = 2 * lane_id, CHANNEL_[lane_id]_L = 2 * lane_id + 1
        for (lane_id = 0; lane_id < MAX_LANE_NUM; lane_id++) {
            power_array[lane_id] = MW_TO_DBM((float)((tx_rx_power[lane_id * NUM_2 + sflag] << SHIFT_8) +
                                                     (unsigned char)tx_rx_power[lane_id * NUM_2 + 1 + sflag]) /
                                             OPTICAL_POWER_DEVISOR);
        }
    }
}

STATIC void tool_show_power_info_v2(struct ds_optical_info *info, const int sflag)
{
    struct power_info power = {0};

    // RX
    tool_get_tx_rx_power(info, RX_FLAG, &power, sflag);
    tool_show_power_info_print_v2(info, RX_FLAG, &power);

    // TX
    tool_get_tx_rx_power(info, TX_FLAG, &power, sflag);
    tool_show_power_info_print_v2(info, TX_FLAG, &power);
}

STATIC void tool_show_media_snr_info_v2(struct ds_optical_info *info, const int sflag)
{
    int offset = sflag / 2;
    int i;

    bool media_support = (info->additional_info.snr_support >> TOOL_SNR_SUPPORT_BIT_MEDIA) & 0x1;
    if (media_support) {
        if (info->dr4_flag == DR4_FLAG) {
            TOOL_PRINT_INFO("MediaSNR Lane0(dB)  %-12.2f%-12s%-12s%-12s%-12s%-12s",
                            ((offset == 0) ? (info->additional_info.media_snr[MEDIA_LANE0]) :
                                             info->additional_info.media_snr[MEDIA_LANE2]),
                            "-", "-", "-", "-", "-");
            TOOL_PRINT_INFO("MediaSNR Lane1(dB)  %-12.2f%-12s%-12s%-12s%-12s%-12s",
                            ((offset == 0) ? (info->additional_info.media_snr[MEDIA_LANE1]) :
                                             info->additional_info.media_snr[MEDIA_LANE3]),
                            "-", "-", "-", "-", "-");
            TOOL_PRINT_INFO("MediaSNR Lane2(dB)  %-12s%-12s%-12s%-12s%-12s%-12s", "-", "-", "-", "-", "-", "-");
            TOOL_PRINT_INFO("MediaSNR Lane3(dB)  %-12s%-12s%-12s%-12s%-12s%-12s", "-", "-", "-", "-", "-", "-");
        } else {
            for (i = 0; i < XSFP_SHOW_SNR_LEN; i++) {
                TOOL_PRINT_INFO("MediaSNR Lane%d(dB)  %-12.2f%-12s%-12s%-12s%-12s%-12s", i,
                                info->additional_info.media_snr[i + offset], "-", "-", "-", "-", "-");
            }
        }
    } else {
        for (i = 0; i < XSFP_SHOW_SNR_LEN; i++) {
            TOOL_PRINT_INFO("MediaSNR Lane%d(dB)  %-12s%-12s%-12s%-12s%-12s%-12s", i, "-", "-", "-", "-", "-", "-");
        }
    }
}

STATIC void tool_show_host_snr_info_v2(struct ds_optical_info *info, const int sflag)
{
    int offset = sflag / 2;
    int i;

    bool host_support = (info->additional_info.snr_support >> TOOL_SNR_SUPPORT_BIT_HOST) & 0x1;
    if (host_support) {
        for (i = 0; i < XSFP_SHOW_SNR_LEN; i++) {
            TOOL_PRINT_INFO("HostSNR Lane%d(dB)   %-12.2f%-12s%-12s%-12s%-12s%-12s", i,
                            info->additional_info.host_snr[i + offset], "-", "-", "-", "-", "-");
        }
    } else {
        for (i = 0; i < XSFP_SHOW_SNR_LEN; i++) {
            TOOL_PRINT_INFO("HostSNR Lane%d(dB)   %-12s%-12s%-12s%-12s%-12s%-12s", i, "-", "-", "-", "-", "-", "-");
        }
    }
}

STATIC void tool_show_temp_voltage_info_v2(struct ds_optical_info *info)
{
    char temp_value[VALUE_WIDTH + 1];

    if (info->temp == TEMP_INVALID || info->temp == TEMP_FAULT || info->present != TOOL_XSFP_PRESENT) {
        TOOL_PRINT_INFO("Temperature(C)      %-12.2s%-12.2s%-12.2s%-12.2s%-12.2s%-12s", "-", "-", "-", "-", "-", "-");
    } else {
        sprintf_s(temp_value, VALUE_WIDTH + 1, "%-12.2f", (float)info->temp);
        TOOL_PRINT_INFO("Temperature(C)      %-12s%-12.2f%-12.2f%-12.2f%-12.2f%-12s", temp_value,
                        (float)info->additional_info.temp_high_alarm_threshold,
                        (float)info->additional_info.temp_high_threshold,
                        (float)info->additional_info.temp_low_alarm_threshold,
                        (float)(int)(int8_t)info->additional_info.temp_low_threshold, "-");
    }

    if (tool_optical_info_not_support_query(info)) {
        TOOL_PRINT_INFO("Voltage(V)          %-12.2s%-12.2s%-12.2s%-12.2s%-12.2s%-12s", "-", "-", "-", "-", "-", "-");
    } else {
        TOOL_PRINT_INFO("Voltage(V)          %-12.2f%-12.2f%-12.2f%-12.2f%-12.2f%-12s",
                        (float)info->additional_info.voltage / VOLTAGE_UNIT_CONVERT,
                        (float)info->additional_info.vcc_high_alarm_threshold / VOLTAGE_UNIT_CONVERT,
                        (float)info->additional_info.vcc_high_threshold / VOLTAGE_UNIT_CONVERT,
                        (float)info->additional_info.vcc_low_alarm_threshold / VOLTAGE_UNIT_CONVERT,
                        (float)info->additional_info.vcc_low_threshold / VOLTAGE_UNIT_CONVERT, "-");
    }
}

// sflag is offset for choose between first 4 lanes and last 4 lanes
void tool_show_optical_info_v2(struct ds_optical_info *info, const int sflag)
{
    TOOL_PRINT_INFO("============================================================================================");
    TOOL_PRINT_INFO("State-flag Diagnostic Monitoring:");
    TOOL_PRINT_INFO("--------------------------------------------------------------------------------------------");
    TOOL_PRINT_INFO("Items            Status");
    TOOL_PRINT_INFO("--------------------------------------------------------------------------------------------");
    tool_show_state_flag_print_v2(info, info->additional_info.tx_fault, "TxFault Flag", sflag);
    tool_show_state_flag_print_v2(info, info->additional_info.rx_los, "RxLos Flag  ", sflag);
    tool_show_state_flag_print_v2(info, info->additional_info.tx_los, "TxLos Flag  ", sflag);
    tool_show_state_flag_print_v2(info, info->additional_info.tx_lol, "TxLol Flag  ", sflag);
    tool_show_state_flag_print_v2(info, info->additional_info.rx_lol, "RxLol Flag  ", sflag);
    TOOL_PRINT_INFO("High power enable reg: %s",
                    (info->high_power_enable == TOOL_XSFP_HIGH_POWER_ENABLED) ? "enabled" : "not enabled");
    TOOL_PRINT_INFO("--------------------------------------------------------------------------------------------");
    TOOL_PRINT_INFO("============================================================================================");
    TOOL_PRINT_INFO("Digital Diagnostic Monitoring:");
    TOOL_PRINT_INFO("--------------------------------------------------------------------------------------------");
    TOOL_PRINT_INFO("Items               Value       HighAlarm   HighWarn    LowAlarm    LowWarn     Status");

    tool_show_temp_voltage_info_v2(info);
    tool_show_txbias_info_v2(info, sflag);
    tool_show_power_info_v2(info, sflag);
    info->additional_info.snr_support <<= (sflag == 0 ? TOOL_SNR_SUPPORT_BIT_FRONT_OFFSET :
                                                        TOOL_SNR_SUPPORT_BIT_REAR_OFFSET);
    tool_show_media_snr_info_v2(info, sflag);
    tool_show_host_snr_info_v2(info, sflag);
}

STATIC int tool_get_optical_info_v2(int logic_id, const int sflag)
{
    int ret;
    struct ds_optical_info info = {0};

    ret = dsmi_get_optical_info(logic_id, 0, &info);
    if (ret != 0) {
        roce_err("Dsmi get optical info fail! ret[%d] logic_id[%d]", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_show_optical_info_v2(&info, sflag);

    return 0;
}

void tool_show_optical_invalid_info_dfx()
{
    TOOL_PRINT_INFO("optical dfx info:");
    TOOL_PRINT_INFO("loopback status    : NA");
    TOOL_PRINT_INFO("tx disable status  : NA");
}

void tool_show_optical_info_dfx(struct ds_optical_info *info)
{
    int i;
    const char *loopback_mode_str[OPTICAL_LOOPBACK_MODE_MAX + 1] = {
        "no loopback mode",                // 无环回模式
        "host side input loopback mode",   // host侧input环回, 另称为电侧内环
        "media side output loopback mode", // media侧output环回, 另称为光侧内环
        "host side output loopback mode",  // host侧output环回, 另称为电侧外环
        "media side input loopback mode"   // media侧input环回, 另称为光侧外环
    };

    if (info->present != TOOL_XSFP_PRESENT || info->temp == TEMP_INVALID || info->additional_info.voltage == 0) {
        tool_show_optical_invalid_info_dfx();
        return;
    }

    TOOL_PRINT_INFO("optical dfx info:");

    for (i = 0; i <= OPTICAL_LOOPBACK_MODE_MAX; ++i) {
        if (i == info->additional_info.loopback_mode) {
            TOOL_PRINT_INFO("loopback status    : %s", loopback_mode_str[i]);
            break;
        }
    }
    TOOL_PRINT_INFO("tx disable status  : 0x%x", info->additional_info.tx_disable_status);
}

int tool_get_optical_info_dfx(int logic_id)
{
    int ret;
    struct ds_optical_info info = {0};

    ret = dsmi_get_optical_info(logic_id, 0, &info);
    if (ret != 0) {
        roce_err("Dsmi get optical info fail! ret[%d] logic_id[%d]", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_show_optical_info_dfx(&info);

    return 0;
}

STATIC int tool_get_optical_info(int logic_id, const int sflag)
{
    int ret;
    unsigned int id_map_idx;
    struct ds_optical_info info = {0};

    ret = dsmi_get_optical_info(logic_id, 0, &info);
    if (ret != 0) {
        roce_err("Dsmi get optical info fail! ret[%d] logic_id[%d]", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_get_xsfp_id_map_index(&info, &id_map_idx);
    if (ret != 0) {
        roce_warn("Tool_get_xsfp_id_map_index fail. logic_id[%d] xsfp_identifier [0x%x]", logic_id,
                  info.base_info.xsfp_identifier);
    }

    tool_show_optical_info(&info, id_map_idx, sflag);

    return 0;
}

int tool_get_optical_info_handle(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id = param->logic_id;
    int sflag = 0;
    int logic_id_convert = logic_id;
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    ret = tool_set_sflag(param, &sflag, &logic_id_convert);
    if (ret != 0) {
        roce_err("Func tool_set_sflag fail. (ret=%d)", ret);
        return ret;
    }
#endif
    if (argc == 0) {
        ret = tool_get_optical_info(logic_id_convert, sflag);
        if (ret) {
            roce_err("tool_get_optical_info fail. (ret=%d; logic_id=%d; logic_id_convert=%d)", ret, logic_id,
                     logic_id_convert);
            return ret;
        }
#ifdef HCCN_TOOL_SHOW_EXT
    } else if (argc == ARGC_NUM_1) {
        if (!strncmp(argv[0], "v2", strlen(argv[0]) + 1)) {
            ret = tool_get_optical_info_v2(logic_id_convert, sflag);
            if (ret) {
                roce_err("tool_get_optical_info_v2 fail. (ret=%d; logic_id=%d; logic_id_convert=%d)", ret, logic_id,
                         logic_id_convert);
                return ret;
            }
        } else if (strncmp(argv[0], "dfx_cfg", strlen(argv[0]) + 1) == 0) {
            ret = tool_get_optical_info_dfx(logic_id_convert);
            if (ret) {
                roce_err("tool_get_optical_dfx_info fail. (ret=%d; logic_id=%d; logic_id_convert=%d)", ret, logic_id,
                         logic_id_convert);
            }
        } else {
            roce_err("tool_get_optical_info_handle invalid operation.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
#endif
    } else {
        roce_err("tool_get_optical_info_handle err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    return 0;
}

void tool_show_optical_snr_query_info(struct ds_optical_snr_query_info *info)
{
    bool host_support = (info->snr_support >> TOOL_SNR_SUPPORT_BIT_HOST) & 0x1;
    bool media_support = (info->snr_support >> TOOL_SNR_SUPPORT_BIT_MEDIA) & 0x1;

    TOOL_PRINT_INFO("Query switch        : %s", (info->switch_status ? "enable" : "disable"));

    if (info->snr_support == XSFP_SNR_SUPPORT_INVALID) {
        TOOL_PRINT_INFO("Host side ability   : NA");
        TOOL_PRINT_INFO("Media side ability  : NA");
        return;
    }

    TOOL_PRINT_INFO("Host side ability   : %s", (host_support ? "support" : "not support"));
    TOOL_PRINT_INFO("Media side ability  : %s", (media_support ? "support" : "not support"));
}

int tool_get_optical_snr_query_info(int argc, char **argv, struct tool_param *param)
{
    int ret, convert_logic_id;
    struct ds_optical_snr_query_info info = {0};

    if (argc >= ARGC_NUM_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_convert_logic_id_and_master_flag(param, &convert_logic_id, &info.master_flag);
    if (ret != 0) {
        return ret;
    }

    ret = dsmi_get_optical_snr_query_info(convert_logic_id, &info);
    if (ret != 0) {
        roce_err("Dsmi to get optical snr query info fail. (ret=%u; logic_id=%d; convert_logic_id=%d)", ret,
                 param->logic_id, convert_logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_show_optical_snr_query_info(&info);

    return UDA_EXE_SUCCESS;
}

int tool_parse_optical_snr_query_info_parm(int argc, char **argv, struct ds_optical_snr_query_info *info)
{
    char *p_tmp = NULL;

    if (strncmp(argv[0], "status", strlen("status") + 1) == 0) {
        if (strncmp(argv[1], "enable", strlen("enable") + 1) == 0) {
            info->switch_status = TOOL_OPTICAL_SNR_QUERY_ENABLE;
        } else if (strncmp(argv[1], "disable", strlen("disable") + 1) == 0) {
            info->switch_status = TOOL_OPTICAL_SNR_QUERY_DISABLE;
        } else {
            roce_err("The value of status %s is wrong. [enable or disable]", argv[1]);
            return UDA_PARAM_INVALID_ERR;
        }
    } else {
        roce_err("Invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return UDA_EXE_SUCCESS;
}

int tool_set_optical_snr_query_info(int argc, char **argv, struct tool_param *param)
{
    int ret, convert_logic_id;
    struct ds_optical_snr_query_info info = {0};

    if (argc != ARGC_NUM_2) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_parse_optical_snr_query_info_parm(argc, argv, &info);
    if (ret != 0) {
        roce_err("Parse optical snr query info parm return error. (ret=%d)\n", ret);
        return ret;
    }

    ret = tool_get_convert_logic_id_and_master_flag(param, &convert_logic_id, &info.master_flag);
    if (ret != 0) {
        return ret;
    }

    ret = dsmi_set_optical_snr_query_info(convert_logic_id, &info);
    if (ret != 0) {
        roce_err("Dsmi to set optical snr query info fail. (ret=%u; logic_id=%d; convert_logic_id=%d)", ret,
                 param->logic_id, convert_logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (!g_recovering) {
        ret = cfg_write_conf(param->phy_id, OPTICAL_SNR_QUERY_SWITCH, argv[1]);
        if (ret != 0) {
            roce_err("Tool save write optical snr query info error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    return UDA_EXE_SUCCESS;
}

int tool_cfg_optical_snr_query_recovery(struct tool_param *param)
{
    char *argv_tmp[PARAM_COUNT_TWO] = {NULL};
    char switch_status[PARAM_COUNT_NINE] = {0};
    int ret;

    ret = tool_is_optical_supported_board_type(param);
    if (ret != 0) {
        roce_info("Optical snr query not need recover.");
        return 0;
    }

    ret = cfg_read_conf(param->phy_id, OPTICAL_SNR_QUERY_SWITCH, &switch_status, PARAM_COUNT_NINE);
    if (ret != 0) {
        return tool_cfg_recovery_err_proc(ret, param->phy_id, OPTICAL_SNR_QUERY_SWITCH);
    }

    argv_tmp[0] = "status";
    argv_tmp[1] = &switch_status;

    ret = tool_set_optical_snr_query_info(PARAM_COUNT_TWO, argv_tmp, param);
    if (ret != UDA_EXE_SUCCESS) {
        roce_err("Cfg optical snr query info failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
    }

    return ret;
}

int tool_optical_snr_query_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret, board_type;

    if (argc < ARGC_NUM_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_is_optical_supported_board_type(param);
    if (ret != 0) {
        return ret;
    }

    if (strncmp(argv[0], "-g", strlen("-g") + 1) == 0) {
        param->get_cmd_flag = true;
        ret = tool_get_optical_snr_query_info(argc - 1, argv + 1, param);
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("Get the optical snr query info fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    } else if (strncmp(argv[0], "-s", strlen("-s") + 1) == 0) {
        ret = tool_set_optical_snr_query_info(argc - 1, argv + 1, param);
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("Set the optical snr query info fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return UDA_EXE_SUCCESS;
}
