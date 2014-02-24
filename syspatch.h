/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>

#include "mapio.h"

#ifdef __cplusplus
extern "C" {
#endif

int syspatch(FILE *source_file, DontCareMap* source_map,
             unsigned char *patch_data, size_t patch_len,
             FILE *target_file, DontCareMap* target_map);

#ifdef __cplusplus
}
#endif
