/*
 * Copyright (c) 2014, Eric Fritz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdio.h>
#include "semant.h"

#define MAGIC_BYTE 0x43484E55
#define CHINNU_VERSION "0.0.1"

Expression *program;
char *filename;

typedef enum {
    WARNING_SHADOW,
    WARNING_UNREACHABLE
} WarningType;

#define NUM_WARNING_TYPES (WARNING_UNREACHABLE + 1)

int warning_flags[NUM_WARNING_TYPES];

void fatal(const char *fmt, ...);
void warning(SourcePos pos, const char *fmt, ...);
void error(SourcePos pos, const char *fmt, ...);
void message(SourcePos pos, const char *fmt, ...);

void vwarning(SourcePos pos, const char *fmt, va_list args);
void verror(SourcePos pos, const char *fmt, va_list args);
void vmessage(SourcePos pos, const char *fmt, va_list args);
