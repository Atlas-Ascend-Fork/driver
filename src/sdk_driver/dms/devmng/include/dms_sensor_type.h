/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DMS_SENSOR_TYPE_H__
#define __DMS_SENSOR_TYPE_H__

#define DMS_MAX_SENSOR_TYPE_NAME_SIZE 20

/* As agreed with the product, the sensor type 0xFE is reserved
 * for the product to customize the fault event ID
 */
#define DMS_SENSOR_RESERVED_FOR_PRODUCT (0xFE)

/* sensor type definition */
typedef enum {
    DMS_SEN_TYPE_TEMPERATURE = 0x01, // temperature
    DMS_SEN_TYPE_VOLTAGE,            // voltage
    DMS_SEN_TYPE_CURRENT,            // electric current
    DMS_SEN_TYPE_FAN,                // fan
    DMS_SEN_TYPE_PHYSICAL_SECURITY,  // physical security event
    DMS_SEN_TYPE_PLATFORM_VIOLATION, // platform security event
    DMS_SEN_TYPE_PROCESSOR,          // processor
    DMS_SEN_TYPE_POWER_SUPPLY,       // power aupply/power conversion
    DMS_SEN_TYPE_POWER_UNIT,         // power supply unit
    DMS_SEN_TYPE_COOLING_DEVICE,     // cooling equipment
    DMS_SEN_TYPE_OTHER_UNITS_BASED_SENSOR,
    DMS_SEN_TYPE_MEMORY,             // memory
    DMS_SEN_TYPE_DRIVE_SLOT,         // disk/drive
    DMS_SEN_TYPE_POST_MEMORY_RESIZE, // memory size adjustment
    DMS_SEN_TYPE_SYSTEM_FW_PROGRESS, // system software error (including BIOS)
    DMS_SEN_TYPE_EVENT_LOGGING_DISABLED,
    DMS_SEN_TYPE_RESERVED1,
    DMS_SEN_TYPE_SYSTEM_EVENT,
    DMS_SEN_TYPE_CRITICAL_INTERRUPT, // emergency interruption
    DMS_SEN_TYPE_BUTTON,             // button/switch
    DMS_SEN_TYPE_MODULE_BOARD,
    DMS_SEN_TYPE_MICROCONTROLLER_COPROCESSOR,
    DMS_SEN_TYPE_ADDIN_CARD,
    DMS_SEN_TYPE_CHASSIS,
    DMS_SEN_TYPE_CHIP_SET,
    DMS_SEN_TYPE_OTHER_FRU,
    DMS_SEN_TYPE_CABLE_INTERCONNECT,
    DMS_SEN_TYPE_TERMINATOR,
    DMS_SEN_TYPE_SYSTEM_BOOT_INITIATED,
    DMS_SEN_TYPE_BOOT_ERROR, // startup error
    DMS_SEN_TYPE_OS_BOOT,
    DMS_SEN_TYPE_OS_CRITICAL_STOP, // operating system termination/Shutdown
    DMS_SEN_TYPE_SLOT_CONNECTOR,
    DMS_SEN_TYPE_SYSTEM_ACPI_POWER_STATE, // ACPI power status
    DMS_SEN_TYPE_WATCHDOG2,               // watchdog
    DMS_SEN_TYPE_PLATFORM_ALERT,
    DMS_SEN_TYPE_ENTITY_PRESENCE,
    DMS_SEN_TYPE_MONITOR_ASIC_IC,
    DMS_SEN_TYPE_HEARTBEAT,
    DMS_SEN_TYPE_MANAGEMENT_SUBSYSTEM_HEALTH,
    DMS_SEN_TYPE_BATTERY = 0x29,
    DMS_SEN_TYPE_SESSION_AUDIT,
    DMS_SEN_TYPE_VERSION_CHANGE = 0x2B,
    DMS_SEN_TYPE_MAX_STANDARD,
    /* above are the sensor types defined by the standard, below are the custom sensor types */
    DMS_SEN_TYPE_RAS_SENSOR = 0xC0, // AS sensor, chip hardware direct error detection
    DMS_SEN_TYPE_EXPIRED_SENSOR,
    DMS_SEN_TYPE_MEMORY_ERR_RECORD,
    DMS_SEN_TYPE_SAFETY_SENSOR = 0xC3,  // SOC EVENT1: SOC system exception
    DMS_SEN_TYPE_EXTEND_SENSOR = 0xC4,  // SOC EVENT2: SOC system exception - hardware error detection assisted by
                                        // software
    DMS_SEN_TYPE_BUS_SENSOR = 0xC5,     // SOC EVENT3: SOC system exception - external Bus exception
    DMS_SEN_TYPE_CHECK_SENSOR = 0xC6,   // SOC EVENT4: SOC system exception - verification exception
    DMS_SEN_TYPE_CRYPTO_SENSOR = 0xC7,  // SOC EVENT5: SOC system exception - security module exception
    DMS_SEN_TYPE_MODULE_SENSOR1 = 0xC8, // SOC EVENT6: SOC system exception - public IP module exception
    DMS_SEN_TYPE_MODULE_SENSOR2 = 0xC9, // SOC EVENT7: SOC system exception - public IP module exception
    DMS_SEN_TYPE_DISP_SENSOR = 0xCA,    // Dispatch module exception
    DMS_SEN_TYPE_SMMU_SENSOR = 0xCB,    // SMMU module exception
    DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT = 0xD0, // GENERAL SOFTWARE FALUT: common software exception
    DMS_SEN_TYPE_CHIP_HARDWARE = 0xD1,          // CHIP HARDWARE: chip hardware exception
    DMS_SEN_TYPE_UB_SENSOR = 0xD5,
    /* The 0xFE sensor type is reserved, used for product-defined event IDs. */
    DMS_SEN_TYPE_MAX_CUSTOM,
} DMS_SENSOR_TYPE_T;

/* for DMS_SEN_TYPE_RAS_SENSOR(0xC0) sensor */
enum ras_error_type {
    RAS_ERROR_TYPE_ERROR = 0x00,
    RAS_ERROR_TYPE_ERROR_NF = 0x01,
    RAS_ERROR_TYPE_INPUT_ERR = 0x02,
    RAS_ERROR_TYPE_IN_CFG_ERR = 0x03,
    RAS_ERROR_TYPE_CFG_ERR = 0x04,
    RAS_ERROR_TYPE_PARITY = 0x05,
    RAS_ERROR_TYPE_SBECCOverThold = 0x06,
    RAS_ERROR_TYPE_SBECC_NC = 0x07,
    RAS_ERROR_TYPE_MBECC = 0x08,
    RAS_ERROR_TYPE_BUS_ERR = 0x09,
    RAS_ERROR_TYPE_TIMEOUT_ERR = 0x0A,
    RAS_ERROR_TYPE_HB_TIMEOUT = 0x0B,
    RAS_ERROR_TYPE_KO_INS_FAIL = 0x0C,
    RAS_ERROR_TYPE_LINK_ERROR = 0x0D,
    RAS_ERROR_TYPE_INIT_ABNORMAL = 0x0D,
    RAS_ERROR_TYPE_IN_CFG_ERR_MINOR = 0x0E,
    RAS_ERROR_TYPE_CFG_ERR_MINIOR = 0x0F,
    RAS_ERROR_TYPE_DATA_POISON = 0x10,
    RAS_ERROR_TYPE_ERR_TYPE_MAX
};

/* for DMS_SEN_TYPE_MEMORY(0xC) sensor */
enum memory_error_type {
    MEMORY_ERROR_TYPE_CEECC = 0x00,
    MEMORY_ERROR_TYPE_UCEECC = 0x01,
    MEMORY_ERROR_TYPE_PARITY = 0x02,
    MEMORY_ERROR_TYPE_SCRUB_FAILED = 0x03,
    MEMORY_ERROR_TYPE_DEVICE_DISABLED = 0x04,
    MEMORY_ERROR_TYPE_CECC_OVER_THOLD = 0x05,
    MEMORY_ERROR_TYPE_PRESENCE_DETECTED = 0x06,
    MEMORY_ERROR_TYPE_CONFIG_ERROR = 0x07,
    MEMORY_ERROR_TYPE_SPARE = 0x08,
    MEMORY_ERROR_TYPE_SCRUB_UNCORRECT = 0x09,
    MEMORY_ERROR_TYPE_MIRROR_SUB_ERROR = 0x0D,
    MEMORY_ERROR_TYPE_ERR_TYPE_MAX
};

/* for DMS_SEN_TYPE_MEMORY_ERR_RECORD(0xC2) sensor */
enum memory_error_record_type {
    MEMORY_ERROR_RECORD_TYPE_MULTI_BITS_ECC = 0,
    MEMORY_ERROR_RECORD_TYPE_NOTICE_ECC_COUNT,
    MEMORY_ERROR_RECORD_TYPE_UPPER_LIMITS_ECC_COUNT,
    MEMORY_ERROR_RECORD_TYPE_SINGLE_ECC_THRESHOLD,
    MEMORY_ERROR_RECORD_TYPE_UCE_INT_SHIELD,
    MEMORY_ERROR_RECORD_TYPE_HBM_SUB_HEALTH,
    MEMORY_ERROR_RECORD_TYPE_ERROR_RECORD_MAX
};

/* for DMS_SEN_TYPE_SAFETY_SENSOR(0xC3) sensor */
enum soc_safety_error_type {
    SOC_SAFETY_LOCKSTEP_ERR = 0x00,
    SOC_SAFETY_REGOVERFLOW_ERR = 0x01,
    SOC_SAFETY_TX_TIMEOUT = 0x02,
    SOC_SAFETY_LINK_STATE_CHG = 0x03,
    SOC_SAFETY_ACCESS_DENY = 0x04,
    SOC_SAFETY_REG_VAL_UNEXPECT = 0x05,
    SOC_SAFETY_LANE_DROP = 0x05,
    SOC_SAFETY_ACCESS_TIMEOUT = 0x06,
    SOC_SAFETY_REPORT_TIMEOUT_MINOR = 0x07,
    SOC_SAFETY_SLAVE_PORT_LINK_STATE_CHG = 0x07,
    SOC_SAFETY_REPORT_TIMEOUT_MAJOR = 0x08,
    SOC_SAFETY_OP_RETRY_SUCCESS = 0x09,
    SOC_SAFETY_USE_BACKUP_LINK = 0x0A,
    SOC_SAFETY_OP_RETRY_FAIL = 0x0B,
    SOC_SAFETY_UNAUTHORIZED_ACCESS_ERR = 0x0f,
    SOC_SAFETY_READ_DATA_POISON = 0x0D,
    SOC_SAFETY_CPU_WAKEUP_NOTIF = 0x0E,
    SOC_SAFETY_UN_ACCESS_NOTIF = 0x0F,
    SOC_SAFETY_ERR_TYPE_MAX
};

/* for DMS_SEN_TYPE_BUS_SENSOR(0xC5) sensor */
enum soc_bus_error_type {
    SOC_BUS_BUS_TIMESTAMP_ABNORMAL = 0x00,
    SOC_BUS_TXRX_EVENTS_LOST = 0x01,
    SOC_BUS_BUS_OVERLOAD = 0x02,
    SOC_BUS_BUS_OFF_STATE = 0x03,
    SOC_BUS_SPMI_BUS_ERROR = 0x04,
    SOC_BUS_BUFFER_OVERFLOW = 0x05,
    SOC_BUS_INVALID_PACKET = 0x06,
    SOC_BUS_ERR_TYPE_MAX
};

/* for DMS_SEN_TYPE_CHECK_SENSOR(0xC6) sensor */
enum soc_check_error_type {
    SOC_CHECK_CHECK_FAIL = 0x00,
    SOC_CHECK_CRC_CHECK_FAIL = 0x01,
    SOC_CHECK_RTSQ_CHECK_FAIL = 0x02,
    SOC_CHECK_ACSQ_CHECK_FAIL = 0x03,
    SOC_CHECK_PCIE_DMA_CHECK_FAIL = 0x04,
    SOC_CHECK_STRESS_CHECK_FAIL = 0x05,
    SOC_CHECK_STRESS_CHECK_ABNORMAL = 0x06,
    SOC_CHECK_ERR_TYPE_MAX
};

/* for DMS_SEN_TYPE_UB_SENSOR(0xD5) sensor */
enum ub_senor_error_type {
    UB_ERR_REMOTE_DATA_EXCP_OCCUR = 0x00,
    UB_ERR_PORT_EXCP_OCCUR = 0x01,
    UB_ERR_NETWORK_TIMEOUT_OCCUR = 0x02,
    UB_ERR_REMOTE_CFG_ERROR_OCCUR = 0x03,
    UB_ERR_LOCAL_CFG_ERROR_OCCUR = 0x04,
    UB_ERR_ABNORMAL_REMOTE_RESOURCE_SPACE = 0x05,
    UB_ERR_NETWORK_EXCP_OCCUR = 0x06,
    UB_SENSOR_ERR_TYPE_MAX
};

enum temp_error_type {
    TEMP_ERROR_TYPE_L1L_OCCUR = 0x00,
    TEMP_ERROR_TYPE_L1L_RESUME = 0x01,
    TEMP_ERROR_TYPE_L2L_OCCUR = 0x02,
    TEMP_ERROR_TYPE_L2L_RESUME = 0x03,
    TEMP_ERROR_TYPE_L3L_OCCUR = 0x04,
    TEMP_ERROR_TYPE_L3L_RESUME = 0x05,
    TEMP_ERROR_TYPE_L1H_RESUME = 0x06,
    TEMP_ERROR_TYPE_L1H_OCCUR = 0x07,
    TEMP_ERROR_TYPE_L2H_RESUME = 0x08,
    TEMP_ERROR_TYPE_L2H_OCCUR = 0x09,
    TEMP_ERROR_TYPE_L3H_RESUME = 0x0A,
    TEMP_ERROR_TYPE_L3H_OCCUR = 0x0B,
    TEMP_ERROR_TYPE_ERR_TYPE_MAX
};

enum voltage_error_type {
    VOLTAGE_ERROR_TYPE_LNC_LOW = 0x00,  /* Crossed Lower non-critical going low */
    VOLTAGE_ERROR_TYPE_LNC_HIGH = 0x01, /* Crossed Lower non-critical going high */
    VOLTAGE_ERROR_TYPE_LC_LOW = 0x02,   /* Crossed Lower critical going low */
    VOLTAGE_ERROR_TYPE_LC_HIGH = 0x03,  /* Crossed Lower critical going high */
    VOLTAGE_ERROR_TYPE_LNR_LOW = 0x04,  /* Crossed Lower non-recoverable going low */
    VOLTAGE_ERROR_TYPE_LNR_HIGH = 0x05, /* Crossed Lower non-recoverable going high */
    VOLTAGE_ERROR_TYPE_UNC_LOW = 0x06,  /* Crossed Upper non-critical going low */
    VOLTAGE_ERROR_TYPE_UNC_HIGH = 0x07, /* Crossed Upper non-critical going high */
    VOLTAGE_ERROR_TYPE_UC_LOW = 0x08,   /* Crossed Upper critical going low */
    VOLTAGE_ERROR_TYPE_UC_HIGH = 0x09,  /* Crossed Upper critical going high */
    VOLTAGE_ERROR_TYPE_UNR_LOW = 0x0A,  /* Crossed Upper non-recoverable going low */
    VOLTAGE_ERROR_TYPE_UNR_HIGH = 0x0B, /* Crossed Upper non-recoverable going high */
    VOLTAGE_ERROR_TYPE_ERR_TYPE_MAX
};

enum heartbeat_error_type {
    HEARTBEAT_ERROR_TYPE_HEARTBEAT_LOST = 0x00,
    HEARTBEAT_ERROR_TYPE_HEARTBEAT_LOST2 = 0x01,
    HEARTBEAT_ERROR_TYPE_ERR_TYPE_MAX
};

enum general_software_error_type {
    GENERAL_SOFTWARE_FAULT_PROCESS_START_FAILED_OR_EXIT = 0x00,
    GENERAL_SOFTWARE_FAULT_MEMORY_OVER_LIMIT = 0x01,
    HAL_GENERAL_SOFTWARE_FAULT_NORMAL_RESOURCE_RECYCLE_FAILED = 0x05,
    HAL_GENERAL_SOFTWARE_FAULT_CRITICAL_RESOURCE_RECYCLE_FAILED = 0x06,
    GENERAL_SOFTWARE_FAULT_ERR_TYPE_MAX
};

enum extend_error_type {
    EXTEND_ERROR_TYPE_MONITOR_TIMEOUT_MINIOR = 0x00,
    EXTEND_ERROR_TYPE_MONITOR_TIMEOUT_MAJOR = 0x01,
    EXTEND_ERROR_TYPE_REPORT_TIMEOUT_MINIOR = 0x02,
    EXTEND_ERROR_TYPE_REPORT_TIMEOUT_MAJOR = 0x03,
    EXTEND_ERROR_TYPE_REPORT_CHANNEL_ERR = 0x04,
    EXTEND_ERROR_TYPE_REPORT_CHANNEL_TO_SILS_ERR = 0x05,
    EXTEND_ERROR_TYPE_REG_ERR_READ_AFTRE_WRITER = 0x06,
    EXTEND_ERROR_TYPE_REG_ERR_PERIOD_READ = 0x07,
    EXTEND_ERROR_TYPE_REG_ERR_WITHOUT_SERVICE_AFFACTION = 0x08,
    EXTEND_ERROR_TYPE_STL_TEST_ERR = 0x09,
    EXTEND_ERROR_TYPE_STL_DIAGNOSTICS_ERR = 0x0A,
    EXTEND_ERROR_TYPE_CHANNEL_SELF_CHECK = 0x0B,
    EXTEND_ERROR_TYPE_INTERRUPTION_STORM_EXCEPTION = 0x0D,
    EXTEND_ERROR_TYPE_ERR_TYPE_MAX
};

enum ub_error_type {
    SOC_UB_ERROR_TYPE_MEM_UN_ACCESS = 0x06,
    SOC_UB_ERROR_TYPE_ERR_TYPE_MAX
};

struct dms_sensor_type_string {
    unsigned char offset;
    unsigned char severity;
    const char *eventstring;
};

/* String structure corresponding to sensor type and offset */
struct tag_dms_sensor_type {
    unsigned char sensor_type;
    const char *type_name;
    unsigned char sensor_event_count;
    struct dms_sensor_type_string *sensor_event;
};

int dms_get_sensor_type_count(void);
struct tag_dms_sensor_type *dms_get_sensor_type(void);

#endif
