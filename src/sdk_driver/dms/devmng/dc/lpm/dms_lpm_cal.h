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

#ifndef DMS_LPM_CAL_H
#define DMS_LPM_CAL_H
#define FIT_POWER_COEFF_MIN 900
#define FIT_POWER_COEFF_STEP 980
#define FIT_POWER_COEFF_MAX 1000
#define FIT_POWER_CAL 1000
void dms_cal_dev_fitted_power(unsigned int *reportPwr, unsigned int dev_num, unsigned int *fittedPwr);

#endif /* DMS_LPM_CAL_H */