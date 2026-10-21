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

#ifndef _KA_COMPILER_H
#define _KA_COMPILER_H

#ifdef __ka_user
#undef __ka_user
#endif
#define __ka_user __user
#define __ka_init __init
#define __ka_exit __exit
#define __ka_must_check __must_check
#ifdef __BIG_ENDIAN
#define __KA_BIG_ENDIAN __BIG_ENDIAN
#endif
#ifdef __LITTLE_ENDIAN
#define __KA_LITTLE_ENDIAN __LITTLE_ENDIAN
#endif

#define ka_likely(x) likely(x)
#define ka_unlikely(x) unlikely(x)
#define KA_READ_ONCE(x) READ_ONCE(x)
#define KA_WRITE_ONCE(x, val) WRITE_ONCE(x, val)

#endif
