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

#include <syspatch.h>

static int usage(char *progname) {
    return fprintf(stderr, "%s: <source> <patch> <target>", progname);
}

static int parse_arguments(
        int argc,
        char **argv,
        FILE **source_file,
        FILE **patch_file,
        FILE **target_file) {

    if (argc < 4) {
        usage(argv[1]);
        return -1;
    }

    *source_file = fopen(argv[1], "r");
    if (*source_file == NULL) {
        fprintf(stderr, "Error opening source file: %s\n", strerror(errno));
        return -1;
    }

    *patch_file = fopen(argv[2], "r");
    if (*patch_file == NULL) {
        fprintf(stderr, "Error opening patch file: %s\n", strerror(errno));
        return -1;
    }

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
    FILE *patch_file;
    FILE *target_file;

    if (parse_arguments(argc, argv, &source_file, &patch_file, &target_file))
        return 1;

    retval = syspatch(source_file, patch_file, target_file);

    fclose(source_file);
    fclose(patch_file);
    fclose(target_file);

    return (retval != 0);
}
