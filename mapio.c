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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "syspatch.h"

//============================================================================//
//                          I/O with don't-care maps                          //
//============================================================================//

size_t read_with_map(unsigned char* data, size_t data_length, MapState* s) {
    size_t this_read = 0;

    while (this_read < data_length && s->cr < s->map->region_count) {
        size_t in_region =
            (s->map->regions[s->cr] * s->map->block_size) - s->so_far;
        if (in_region > 0) {
            if (in_region > data_length - this_read) {
                // the current region is big enough to satisfy the
                // remainder of the request; read from it and return.
                if (fread(data + this_read, data_length - this_read,
                          1, s->f) != 1) {
                    printf("  short read: %s\n", strerror(errno));
                }
                s->so_far += (data_length - this_read);
                this_read += (data_length - this_read);
                return this_read;
            }
            fread(data + this_read, in_region, 1, s->f);
            this_read += in_region;
        }

        fseek(s->f, s->map->regions[s->cr+1] * s->map->block_size, SEEK_CUR);
        s->cr += 2;
        s->so_far = 0;
    }

    return this_read;
}

int seek_with_map(off_t offset, MapState* state) {
    off_t real_offset = 0;

    int i;
    for (i = 0; i < state->map->region_count; i += 2) {
        if (offset < (off_t)(state->map->regions[i] * state->map->block_size)) {
            state->so_far = offset;
            state->cr = i;
            real_offset += offset;
            return fseek(state->f, real_offset, SEEK_SET);
        }
        offset -= state->map->regions[i] * state->map->block_size;
        real_offset += (state->map->regions[i] + state->map->regions[i+1]) *
            state->map->block_size;
    }
    return -1;
}

ssize_t write_with_map(const unsigned char* data, size_t data_length,
                       MapState* s) {
    // s->so_far is how far we've written into the current care
    // region.  It should never exceed regions[rc] * block_size.  (But
    // it can equal it, if we've filled the current care region but
    // haven't advanced cr.)

    // We start by checking if the current region is full because the
    // map can begin with a zero-length care region.

    if (s->cr >= s->map->region_count) {
        printf("attempted to write off end of map!\n");
        return -1;
    }

    while (s->so_far >= s->map->regions[s->cr] * s->map->block_size) {
        fseek(s->f, s->map->regions[s->cr+1] * s->map->block_size, SEEK_CUR);

        s->so_far = 0;
        s->cr += 2;
    }

    ssize_t written = 0;
    while (data_length > 0) {
        if (s->cr >= s->map->region_count) {
            printf("attempted to write off end of map!\n");
            return -1;
        }

        // in_region is how much more we need to write in the current
        // care region.
        size_t in_region =
            s->map->regions[s->cr] * s->map->block_size - s->so_far;

        // if we have less data than that, then all the data goes in
        // the current region and we don't need to do anything else
        // (apart from advance so_far).
        if (data_length < in_region) {
            fwrite(data, data_length, 1, s->f);
            s->so_far += data_length;
            written += data_length;
            return written;
        }

        // The data we have fills the current region (and possibly
        // more).  Write out the rest of the current region...
        fwrite(data, in_region, 1, s->f);
        data += in_region;
        data_length -= in_region;
        s->so_far += in_region;
        written += in_region;

        // ... then skip over the next don't-care region ...
        fseek(s->f, s->map->regions[s->cr+1] * s->map->block_size, SEEK_CUR);

        // ... then advance to the next region.
        s->so_far = 0;
        s->cr += 2;
    }

    return written;
}
