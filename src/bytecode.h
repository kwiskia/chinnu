/*
 * Copyright (C) 2012, Eric Fritz
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

#define SIZE_O 6
#define SIZE_A 8
#define SIZE_B 9
#define SIZE_C 9

#define POS_O (0)
#define POS_A (SIZE_O)
#define POS_B (SIZE_O + SIZE_A)
#define POS_C (SIZE_O + SIZE_A + SIZE_B)

#define MAX_O ((1 << SIZE_O) - 1)
#define MAX_A ((1 << SIZE_A) - 1)
#define MAX_B ((1 << SIZE_B) - 1)
#define MAX_C ((1 << SIZE_C) - 1)

#define GET_O(i) ((i >> POS_O) & MAX_O)
#define GET_A(i) ((i >> POS_A) & MAX_A)
#define GET_B(i) ((i >> POS_B) & MAX_B)
#define GET_C(i) ((i >> POS_C) & MAX_C)

#define CREATE(op, a, b, c) ((op << POS_O) | (a << POS_A) | (b << POS_B) | (c << POS_C))

typedef enum {
    OP_MOVE,            // R(A) := RK(B)
    OP_GETUPVAL,        // R(A) := UpValue[B]
    OP_SETUPVAL,        // UpValue[B] := R(A)

    OP_ADD,             // R(A) := RK(B) + RK(C)
    OP_SUB,             // R(A) := RK(B) - RK(C)
    OP_MUL,             // R(A) := RK(B) * RK(C)
    OP_DIV,             // R(A) := RK(B) / RK(C)
    OP_UNM,             // R(A) := -RK(B)
    OP_NOT,             // R(A) := ~RK(B)

    OP_EQ,              // R(A) := RK(B) == RK(C)
    OP_LT,              // R(A) := RK(B) <  RK(C)
    OP_LE,              // R(A) := RK(B) <= RK(C)

    OP_CLOSURE,         // R(A) := Proto[B]
    OP_CALL,            // [???]
    OP_RETURN,          // return RK(B)

    OP_JUMP,            // PC := PC + (R(C) ? -B : B)
    OP_JUMP_TRUE,       // PC := PC + (R(C) ? -B : B) : if R(A) == true
    OP_JUMP_FALSE,      // PC := PC + (R(C) ? -B : B) : if R(A) == false

    NUM_OPCODES
} OpCode;

const char *const opcode_names[NUM_OPCODES];
