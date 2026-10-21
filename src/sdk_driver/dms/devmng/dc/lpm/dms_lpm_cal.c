/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "dms_template.h"
#include "dms_lpm_cal.h"

void dms_cal_dev_fitted_power(unsigned int *reportPwr, unsigned int dev_num, unsigned int *fittedPwr)
{
    unsigned int i;
    unsigned int maxPwr = 0;
    unsigned int coeff = 0;

    if (dev_num == 0 || reportPwr == NULL || fittedPwr == NULL) {
        dms_err("dev_num is 0, no device to calculate fitted power.\n");
        dms_err("Invalid parameter. (dev_num=%d, reportPwr=%s, fittedPwr=%s)\n", dev_num,
                (reportPwr == NULL) ? "NULL" : "OK", (fittedPwr == NULL) ? "NULL" : "OK");
        return;
    }
    for (i = 0; i < dev_num; i++) {
        if (reportPwr[i] > maxPwr) {
            maxPwr = reportPwr[i];
        }
    }

    if (maxPwr == 0) {
        dms_err("maxPwr is 0.\n");
        return;
    }

    for (i = 0; i < dev_num; i++) {
        coeff = (unsigned int)(((unsigned long long)reportPwr[i] * FIT_POWER_CAL) / maxPwr);
        if (coeff < FIT_POWER_COEFF_MIN) {
            coeff = FIT_POWER_COEFF_MIN;
        } else if (coeff > FIT_POWER_COEFF_STEP) {
            coeff = FIT_POWER_COEFF_MAX;
        }
        fittedPwr[i] = reportPwr[i] == 0 ? 0 : coeff;
    }
}
