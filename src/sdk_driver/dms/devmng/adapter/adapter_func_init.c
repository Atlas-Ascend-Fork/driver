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
#include "pbl/pbl_feature_loader.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "adpater_def.h"
#include "dms_define.h"

static inline void *get_symbol_objective_func(const ka_kernel_symbol_t *sym, const char *synname)
{
    const char *module_sym_name = get_symbol_name(sym);
    if (ka_base_strcmp(module_sym_name, synname) != 0) {
        return NULL;
    }

    return get_symbol_fun(sym);
}

void init_module_func(const ka_module_t *mod, const struct symbol_list *fun_name_list, unsigned int count,
                      struct bus_adpater_stu *adapt)
{
    unsigned int i, j;
    void *fn = NULL;
    uintptr_t *addr = NULL;
    bool found = false;
    ka_task_down_write(&adapt->rw_lock);
    for (i = 0; i < count; i++) {
        found = false;
        for (j = 0; j < mod->num_syms; j++) {
            fn = get_symbol_objective_func(&mod->syms[j], fun_name_list[i].name);
            if (fn != NULL) {
                addr = (uintptr_t *)((uintptr_t)adapt + fun_name_list[i].offset);
                *addr = (uintptr_t)fn;
                found = true;
                break;
            }
        }
        if (!found) {
            dms_warn("Cannot find symbol objective func. (func_name=%s)\n", fun_name_list[i].name);
        }
    }
    ka_task_up_write(&adapt->rw_lock);
    return;
}
void uninit_module_func(const struct symbol_list *fun_name_list, unsigned int count, struct bus_adpater_stu *adapt)
{
    unsigned int i;
    uintptr_t *addr = NULL;
    ka_task_down_write(&adapt->rw_lock);
    for (i = 0; i < count; i++) {
        addr = (uintptr_t *)((uintptr_t)adapt + fun_name_list[i].offset);
        *addr = (uintptr_t)0;
    }
    ka_task_up_write(&adapt->rw_lock);
}
