/*
 * Simple trace backend
 *
 * Copyright IBM, Corp. 2010
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#ifndef TRACE_SIMPLE_H
#define TRACE_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef uint64_t TraceEventID;

typedef struct {
    const char *tp_name;
    bool state;
} TraceEvent;

void st_print_trace_file_status(FILE *stream, fprintf_function stream_printf);
void st_set_trace_file_enabled(bool enable);
bool st_set_trace_file(const char *file);
void st_flush_trace_buffer(void);

typedef struct {
    unsigned int tbuf_idx;
    unsigned int next_tbuf_idx;
    unsigned int rec_off;
} TraceBufferRecord;

/**
 * Initialize a trace record and claim space for it in the buffer
 *
 * @arglen  number of bytes required for arguments
 */
int trace_record_start(TraceBufferRecord *rec, TraceEventID id, size_t arglen);

/**
 * Append a 64-bit argument to a trace record
 */
void trace_record_write_u64(TraceBufferRecord *rec, uint64_t val);

/**
 * Append a string argument to a trace record
 */
void trace_record_write_str(TraceBufferRecord *rec, const char *s);

/**
 * Mark a trace record completed
 *
 * Don't append any more arguments to the trace record after calling this.
 */
void trace_record_finish(TraceBufferRecord *rec);

#endif /* TRACE_SIMPLE_H */
