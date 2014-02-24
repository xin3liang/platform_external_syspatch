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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>

#include <syspatch.h>

static int usage(char *progname) {
    return fprintf(stderr, "%s: <source> <patch> <target>\n", progname);
}

static int parse_arguments(
        int argc,
        char **argv,
        FILE **source_file,
        unsigned char **patch_data,
        size_t *patch_len,
        FILE **target_file) {

    if (argc < 4) {
        usage(argv[0]);
        return -1;
    }

    *source_file = fopen(argv[1], "r");
    if (*source_file == NULL) {
        fprintf(stderr, "Error opening source file: %s\n", strerror(errno));
        return -1;
    }

    int fd = open(argv[2], O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening patch file: %s\n", strerror(errno));
        return -1;
    }
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        fprintf(stderr, "Error stat'ing patch file: %s\n", strerror(errno));
        return -1;
    }
    *patch_data = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (patch_data == MAP_FAILED) {
        fprintf(stderr, "Error mmap'ing patch file: %s\n", strerror(errno));
        return -1;
    }
    *patch_len = sb.st_size;
    close(fd);

    *target_file = fopen(argv[3], "r+");
    if (*target_file == NULL) {
        fprintf(stderr, "Error opening target file: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int retval;
    FILE *source_file;
    unsigned char *patch_data;
    size_t patch_len;
    FILE *target_file;

    if (parse_arguments(argc, argv, &source_file, &patch_data, &patch_len, &target_file))
        return 1;

    DontCareMap source_map;
    int source_regions[2] = { INT_MAX, 0 };
    source_map.block_size = 4096;
    source_map.region_count = 2;
    source_map.regions = source_regions;

    DontCareMap target_map;
    int target_regions[2] = { INT_MAX, 0 };
    target_map.block_size = 4096;
    target_map.region_count = 2;
    target_map.regions = target_regions;

    retval = syspatch(source_file, &source_map, patch_data, patch_len, target_file, &target_map);

    fclose(source_file);
    fclose(target_file);
    munmap(patch_data, patch_len);

    return (retval != 0);
}
