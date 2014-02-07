/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "xdelta3.c"
#include "xdelta3.h"
#include "xz.h"

// XZ_DICT_SIZE represents the size of the dictionary used for XZ decompression.
// Higher values improve compression, but consume more memory.
#define XZ_DICT_SIZE (1U<<26)
// XZ_OUTPUT_SIZE tunes the amount of data that XZ buffers before handing it off
// to the patcher. Settings lower than TARGET_WINDOW_SIZE or higher than
// SOURCE_WINDOW_SIZE probably don't make any sense, but this is untested. The
// primary tradeoff made here is between memory and extra calls to the decoder.
#define XZ_OUTPUT_SIZE (1U<<23)
// BLOCK_SIZE is the base I/O size for xdelta. See the variables below for how
// this impacts performance.
#define BLOCK_SIZE (1U<<23)
// TARGET_WINDOW_SIZE determines how large writes to the target file are. There
// isn't a lot of science behind this yet, excepting the notes about the write
// queue below.
#define TARGET_WINDOW_SIZE (1 * BLOCK_SIZE)
// SOURCE_WINDOW_SIZE is one of the primary tunables. Higher values increase
// compression ratios and improve decoder speed, but consume more memory.
#define SOURCE_WINDOW_SIZE (8 * BLOCK_SIZE)
// WRITE_QUEUE_LENGTH * TARGET_WINDOW_SIZE must be >= SOURCE_WINDOW_SIZE / 2.
// This ensures that writes will always happen behind an area where a read
// might occur. This is enforced by the READ_FRONTIER check.
#define WRITE_QUEUE_LENGTH (4)
// READ_CACHE_LENGTH is defined based on the acceptability of a time/memory
// tradeoff. A value of at least 6 is necessary to not completely suck, and
// 8 or higher is recommended. Higher than 16 will probably not yield
// reasonable returns for realistic patches.
#define READ_CACHE_LENGTH (8)

static uint8_t in[XZ_OUTPUT_SIZE];
static uint8_t out[XZ_OUTPUT_SIZE];

typedef struct TargetWrite TargetWrite;
struct TargetWrite {
    size_t start;
    size_t length;
    uint8_t data[TARGET_WINDOW_SIZE];
};

typedef struct SourceRead SourceRead;
struct SourceRead {
    size_t blkno;
    size_t length;
    uint8_t data[BLOCK_SIZE];
};

static SourceRead* READ_CACHE[READ_CACHE_LENGTH];
static size_t SOURCE_WINDOWS_CACHED;

typedef struct XZContext XZContext;
struct XZContext {
    struct xz_buf b;
    struct xz_dec *s;
    enum xz_ret ret;
    const char *msg;
};

static TargetWrite* WRITE_QUEUE[WRITE_QUEUE_LENGTH];
static size_t TARGET_WINDOWS_WRITTEN;
static size_t READ_FRONTIER;

//============================================================================//
//                                 Source I/O                                 //
//============================================================================//

static SourceRead *shuffle_cache(SourceRead *tmp, size_t index) {
    SourceRead *current = READ_CACHE[index];
    READ_CACHE[index] = tmp;
    return current;
}

static int add_to_read_cache(SourceRead *source_read) {
    int i = 0;
    SourceRead *tmp = NULL;
    for (i; i < READ_CACHE_LENGTH; i++) {
        if (tmp == source_read)
            break;
        tmp = shuffle_cache(tmp, i);
    }
    if (tmp != source_read)
        free(tmp);
    READ_CACHE[0] = source_read;
    return 0;
}

static size_t read_source_file(uint8_t *data, FILE *file) {
    size_t size;
    fflush(file);
    size = fread((void*)data, 1, BLOCK_SIZE, file);
    fflush(file);
    return size;
}

static int check_read(size_t pos, size_t frontier) {
    if (pos < frontier) {
        fprintf(stderr, "Read past frontier: %zu > %zu\n", pos, frontier);
        return -1;
    }
    return 0;
}

static SourceRead *get_source_window_from_file(xd3_source *source) {
    size_t read_position = source->getblkno * source->blksize;
    SourceRead *source_read = malloc(sizeof(SourceRead));
    memset(source_read, 0, sizeof(SourceRead));
    if (check_read(read_position, READ_FRONTIER) < 0)
        return NULL;
    if (fseek(source->ioh, read_position, SEEK_SET) != 0) {
        fprintf(stderr, "Couldn't seek to %zu\n", read_position);
        return NULL;
    }
    source_read->blkno = source->getblkno;
    source_read->length = read_source_file(source_read->data, source->ioh);
    return source_read;
}

static SourceRead *get_source_window_from_cache(size_t blkno) {
    int i = 0;
    for (i; i < READ_CACHE_LENGTH; i++) {
        if (READ_CACHE[i]->blkno == blkno) {
            return READ_CACHE[i];
        }
    }
    return NULL;
}

static int read_to_source(SourceRead *source_read, xd3_source *source) {
    source->onblk = source_read->length;
    source->curblkno = source->getblkno;
    source->curblk = source_read->data;
    return 0;
}

static int process_source_data(xd3_source *source) {
    SourceRead *source_read = get_source_window_from_cache(source->getblkno);
    if (source_read == NULL) {
        source_read = get_source_window_from_file(source);
    }
    if (source_read == NULL) {
        return -1;
    }
    add_to_read_cache(source_read);
    return read_to_source(source_read, source);
}

//============================================================================//
//                                Patch I/O                                   //
//============================================================================//

static int read_compressed_input(XZContext *context, FILE *infile) {
    if (context->b.in_pos == context->b.in_size) {
        context->b.in_size = fread(in, 1, XZ_OUTPUT_SIZE, infile);
        context->b.in_pos = 0;
    }
    return 0;
}

//============================================================================//
//                                 Target I/O                                 //
//============================================================================//

static int write_target(TargetWrite *tgt, FILE *target_file) {
    if (tgt->length > 0) {
        if (fseek(target_file, tgt->start, SEEK_SET) != 0)
            return -1;
        if (fwrite(tgt->data, 1, tgt->length, target_file) != tgt->length)
            return -1;
        if (fflush(target_file) != 0)
            return -1;
    }
    READ_FRONTIER = ftell(target_file);
    return 0;
}

static int stream_to_target_write(xd3_stream *stream, TargetWrite *tgt) {
    if (stream->avail_out) {
        tgt->start = TARGET_WINDOWS_WRITTEN * TARGET_WINDOW_SIZE;
        tgt->length = stream->avail_out;
        memcpy(tgt->data, stream->next_out, tgt->length);
        xd3_consume_output(stream);
    }
    return 0;
}

static int advance_target_buffer(xd3_stream *stream, FILE *target_file) {
    TargetWrite *tgt = WRITE_QUEUE[TARGET_WINDOWS_WRITTEN % WRITE_QUEUE_LENGTH];
    if (write_target(tgt, target_file) != 0)
        return -1;
    if (stream_to_target_write(stream, tgt) != 0)
        return -1;
    TARGET_WINDOWS_WRITTEN += 1;
    return 0;
}

static int process_target_data(xd3_stream *stream, FILE *target, int force) {
    size_t iters = (force == 0) + (force * WRITE_QUEUE_LENGTH);
    while (iters) {
        if (advance_target_buffer(stream, target) < 0)
            return -1;
        iters -= 1;
    }
    return 0;
}

//============================================================================//
//                          Read Cache Setup and Teardown                     //
//============================================================================//

static int setup_read_cache(FILE *source_file) {
    int i = 0;
    size_t length;
    for (i; i < READ_CACHE_LENGTH; i++) {
        READ_CACHE[i] = malloc(sizeof(SourceRead));
        if (READ_CACHE[i] == NULL)
            return -1;
        READ_CACHE[i]->blkno = i;
        length = read_source_file(READ_CACHE[i]->data, source_file);
        READ_CACHE[i]->length = length;
    }
    SOURCE_WINDOWS_CACHED = READ_CACHE_LENGTH;
    return 0;
}

static int teardown_read_cache() {
    int i = 0;
    for (i; i < READ_CACHE_LENGTH; i++) {
        free(READ_CACHE[i]);
    }
    return 0;
}

//============================================================================//
//                        Write Queue Setup and Teardown                      //
//============================================================================//

static int setup_write_queue() {
    TARGET_WINDOWS_WRITTEN = 0;
    int i = 0;
    for (i; i < WRITE_QUEUE_LENGTH; i++) {
        WRITE_QUEUE[i] = malloc(sizeof(TargetWrite));
        if (WRITE_QUEUE[i] == NULL)
            return -1;
        WRITE_QUEUE[i]->start = 0;
        WRITE_QUEUE[i]->length = 0;
        memset(WRITE_QUEUE[i]->data, 0, TARGET_WINDOW_SIZE);
    }
    return 0;
}

static int teardown_write_queue() {
    int i = 0;
    for (i; i < WRITE_QUEUE_LENGTH; i++) {
        free(WRITE_QUEUE[i]);
    }
    return 0;
}

//============================================================================//
//                        Decompressor Setup and Teardown                     //
//============================================================================//
static int setup_xz_buf(struct xz_buf *b) {
    b->in = in;
    b->in_pos = 0;
    b->in_size = 0;
    b->out = out;
    b->out_pos = 0;
    b->out_size = XZ_OUTPUT_SIZE;
    return 0;
}

static int setup_xz_dec(struct xz_dec **s) {
    *s = xz_dec_init(XZ_DYNALLOC, XZ_DICT_SIZE);
    if (*s == NULL)
        return -1;
    return 0;
}

static int setup_xz_context(XZContext *context) {
    xz_crc32_init();
    setup_xz_dec(&context->s);
    if (setup_xz_buf(&context->b) < 0)
        return -1;
    return 0;
}

//============================================================================//
//                          Patcher Setup and Teardown                        //
//============================================================================//

static int setup_xdelta_config(xd3_config *config, xd3_stream *stream) {
    int ret;
    memset(config, 0, sizeof(*config));
    xd3_init_config(config, 0);
    config->winsize = TARGET_WINDOW_SIZE;
    config->getblk = NULL;
    ret = xd3_config_stream(stream, config);
    if (ret != 0) {
        fprintf(stderr, "xd3_config_stream error: %s\n", xd3_strerror(ret));
        return -1;
    }
    return 0;
}

static int setup_xdelta_source( xd3_source *source,
                                xd3_stream *stream,
                                FILE *source_file) {
    int ret;
    memset(source, 0, sizeof(*source));
    source->name = "source";
    source->ioh = source_file;
    source->blksize = BLOCK_SIZE;
    source->curblkno = 0;
    source->curblk = NULL;
    source->onblk = 0;
    READ_FRONTIER = 0;

    ret = xd3_set_source(stream, source);
    if (ret != 0) {
        fprintf(stderr, "xd3_set_source error: %s\n", xd3_strerror(ret));
        return -1;
    }
    return 0;
}

static void teardown_xdelta_stream(xd3_stream *stream) {
    xd3_close_stream(stream);
    xd3_free_stream(stream);
}

//============================================================================//
//                             Decompression Loop                             //
//============================================================================//

static int run_decompressor(XZContext *context, FILE *infile) {
    context->ret = xz_dec_run(context->s, &context->b);
    read_compressed_input(context, infile);
    return context->b.out_pos == XZ_OUTPUT_SIZE;
}

static int decompress_into_buffer(
        XZContext *context,
        FILE *infile,
        int *buffer_filled,
        int *done) {

    *buffer_filled = run_decompressor(context, infile);
    if (context->ret == XZ_OK)
        return 0;

    switch (context->ret) {
        case XZ_STREAM_END:
            *buffer_filled = 1;
            *done = 1;
            return 0;
        case XZ_MEM_ERROR:
        case XZ_MEMLIMIT_ERROR:
        case XZ_FORMAT_ERROR:
        case XZ_OPTIONS_ERROR:
        case XZ_DATA_ERROR:
        case XZ_BUF_ERROR:
            context->msg = "File is corrupt\n";
            return -1;
        default:
            context->msg = "Bug!\n";
            return -1;
    }
}

static int decompress(XZContext *context, FILE *patch_file) {
    int done = 0;
    int filled = 0;
    while (!filled) {
        if (decompress_into_buffer(context, patch_file, &filled, &done) < 0)
            return -1;
    }
    return done;
}

static int decompress_patch(XZContext *context,
                            FILE *patch_file,
                            xd3_stream *stream) {
    int done = decompress(context, patch_file);
    xd3_avail_input(stream, out, context->b.out_pos);
    context->b.out_pos = 0;
    return done;
}

//============================================================================//
//                                Patching Loop                               //
//============================================================================//

static int patch(
        XZContext *context,
        xd3_stream *stream,
        xd3_source *source,
        FILE *patch_file,
        FILE *target_file) {

    int ret = 0;
    int decompression_done;

    source->curblk = READ_CACHE[0]->data;
    source->onblk = READ_CACHE[0]->length;
    source->curblkno = 0;

    do {
        decompression_done = decompress_patch(context, patch_file, stream);
        if (decompression_done < 0)
            goto err;
        if (decompression_done)
            xd3_set_flags(stream, XD3_FLUSH);
process:
        ret = xd3_decode_input(stream);
        switch (ret) {
            case XD3_INPUT:
                continue;
            case XD3_OUTPUT:
                if (process_target_data(stream, target_file, 0) < 0)
                    goto err;
                goto process;
            case XD3_GETSRCBLK:
                if (process_source_data(source) < 0)
                    goto err;
                goto process;
            case XD3_GOTHEADER:
                goto process;
            case XD3_WINSTART:
                goto process;
            case XD3_WINFINISH:
                goto process;
            default:
                goto err;
        }
    } while (!decompression_done);

    process_target_data(stream, target_file, 1);

    ret = 0;
    goto out;

err:
    fprintf(stderr, "error: %s (%s)\n", xd3_strerror(ret), stream->msg);
    ret = -1;
out:
    xz_dec_end(context->s);
    return ret;
}

//============================================================================//
//                                 Main Logic                                 //
//============================================================================//
int syspatch(FILE *source_file, FILE *patch_file, FILE *target_file) {
    XZContext xz_context;
    xd3_stream stream;
    xd3_config config;
    xd3_source source;

    if (setup_read_cache(source_file) != 0)
        return -1;

    if (setup_write_queue() != 0)
        return -1;

    if (setup_xz_context(&xz_context) != 0)
        return -1;

    if (setup_xdelta_config(&config, &stream) != 0)
        return -1;

    if (setup_xdelta_source(&source, &stream, source_file) != 0)
        return -1;

    if (patch(&xz_context, &stream, &source, patch_file, target_file) != 0)
        return -1;

    teardown_read_cache();
    teardown_write_queue();
    teardown_xdelta_stream(&stream);
    return 0;
}
