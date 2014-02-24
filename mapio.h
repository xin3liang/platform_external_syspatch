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

#ifndef _SYSPATCH_MAPIO_H
#define _SYSPATCH_MAPIO_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _DontCareMap {
    size_t block_size;
    int region_count;      // always even
    int* regions;          // even-numbered entries (starting with [0]) are counts of blocks we care about
                           // odd-numbered entries are counts of blocks we don't care about
} DontCareMap;

typedef struct _MapState {
    DontCareMap* map;
    int cr;              // current region; always even
    size_t so_far;       // how far into the current care region we are
    FILE* f;             // file we're writing
} MapState;

ssize_t write_with_map(const unsigned char* data, size_t data_length, MapState* s);

size_t read_with_map(unsigned char* data, size_t data_length, MapState* state);

int seek_with_map(off_t offset, MapState* state);

#ifdef __cplusplus
}
#endif

#endif // _SYSPATCH_MAPIO_H
