/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This very simple hello world C code can be used as a test case for building
 * probably the simplest loadable extension. It requires a single symbol be
 * linked, section relocation support, and the ability to export and call out to
 * a function.
 */

#include <stdint.h>
#include <zephyr/llext/symbol.h>

extern void printk(char *fmt, ...);

__attribute__((used))
static const uint32_t static_const = 1; /* .text, file-local linkage */

__attribute__((used))
static uint32_t static_var = 2;         /* .data, file-local linkage */

__attribute__((used))
static uint32_t static_bss; /* = 3 */          /* .bss,  file-local linkage */

const uint32_t global_const = 4;        /* .text, global linkage */
uint32_t global_var = 5;                /* .data, global linkage */
uint32_t global_bss; /* = 6 */          /* .bss,  global linkage */

extern void hello_world(void)
{
	printk("hello world\n");
	printk("A number is %lu\n", global_var);
}
LL_EXTENSION_SYMBOL(hello_world);
