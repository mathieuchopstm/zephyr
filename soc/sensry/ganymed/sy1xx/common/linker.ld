/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 * Copyright (c) 2016-2017 Jean-Paul Etienne <fractalclone@gmail.com>
 * Copyright (c) 2018 Foundries.io Ltd
 * Copyright (c) 2024 sensry.io
 *
 * This file is based on:
 *
 * - include/arch/arm/cortex_m/scripts/linker.ld
 * - include/arch/riscv/common/linker.ld
 * - include/arch/riscv/pulpino/linker.ld
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>

#include <zephyr/linker/sections.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>

#define MPU_ALIGN(region_size) . = ALIGN(4)

/*
 * Extra efforts would need to be taken to ensure the IRQ handlers are within
 * jumping distance of the vector table in non-XIP builds, so avoid them.
 */
#define ROMABLE_REGION              ROM
#define RAMABLE_REGION              RAM

## ROM AREA ##
#define ROM_BASE        0x1C010100
#define ROM_SIZE        0x5Fa00

## RAM AREA ##
#define RAM_BASE        0x1C070000
#define RAM_SIZE        0x200000

MEMORY
    {
        L2_START  (rx)          : ORIGIN = 0x1c010000, LENGTH = 0x00000080
        L2_VALIDITY (rx)        : ORIGIN = 0x1c010080, LENGTH = 0x00000080

        ROM (rx)                : ORIGIN = ROM_BASE,    LENGTH = ROM_SIZE       /*  392kb */
        RAM (rwx)               : ORIGIN = RAM_BASE,    LENGTH = RAM_SIZE       /* 2097kb */

        L2_PRIV_CH0             : ORIGIN = 0x1c004100, LENGTH = 0x2000          /* uDMA access */

        /*
         * Special section, not included in the final binary, used
         * to generate interrupt tables. See include/linker/intlist.ld.
         */
        IDT_LIST                : ORIGIN = 0xFFFFF7FF, LENGTH = 2K
    }

ENTRY(CONFIG_KERNEL_ENTRY)

SECTIONS
    {

        .pre_start MAX(0x1c010000,ALIGN(0x80)) :
        {
            KEEP(*(.pre_start))
        } > L2_START

        .validity_marker MAX(0x1c010080,ALIGN(0x80)) :
        {
            KEEP(*(.validity_marker))
        } > L2_VALIDITY

        /* uninitialized space for uDMA access */
        .udma_access (NOLOAD): {
            . = ALIGN(4);
            _udma_space_start = .;
            *(.udma_access)
            _udma_space_end = .;
        } > L2_PRIV_CH0

        #include <zephyr/linker/rel-sections.ld>

        #ifdef CONFIG_LLEXT
        #include <zephyr/linker/llext-sections.ld>
        #endif

        SECTION_PROLOGUE(.plt,,)
        {
            *(.plt)
        }


        SECTION_PROLOGUE(.iplt,,)
        {
            *(.iplt)
        }


        GROUP_START(ROM)
        __rom_region_start = ROM_BASE;

            SECTION_PROLOGUE(_TEXT_SECTION_NAME,,)
            {

                /* Located in generated directory. This file is populated by calling
                 * zephyr_linker_sources(ROM_START ...). This typically contains the vector
                 * table and debug information.
                 */
                #include <snippets-rom-start.ld>

                __text_region_start = .;

                *(.text .text.*)
                *(.gnu.linkonce.t.*)
                *(.eh_frame)

            } GROUP_LINK_IN(ROM)

            __text_region_end = .;


            __rodata_region_start = .;

                #include <zephyr/linker/common-rom.ld>
                #include <zephyr/linker/thread-local-storage.ld>

                SECTION_PROLOGUE(_RODATA_SECTION_NAME,,)
                {
                    . = ALIGN(4);
                    *(.srodata)
                    *(".srodata.*")
                    *(.rodata)
                    *(.rodata.*)
                    *(.gnu.linkonce.r.*)

                    /* Located in generated directory. This file is populated by the
                     * zephyr_linker_sources() Cmake function.
                     */
                    #include <snippets-rodata.ld>

                } GROUP_LINK_IN(ROMABLE_REGION)

                #include <zephyr/linker/cplusplus-rom.ld>

            __rodata_region_end = .;

    __rom_region_end = .;
    GROUP_END(ROM)

    GROUP_START(RAM)

        SECTION_DATA_PROLOGUE(_DATA_SECTION_NAME,,)
        {
        . = ALIGN(4);
        _image_ram_start = .;
        __data_region_start = .;
        __data_start = .;

        *(.data)
        *(.data.*)
        *(.gnu.linkonce.s.*)

        /* https://groups.google.com/a/groups.riscv.org/d/msg/sw-dev/60IdaZj27dY/TKT3hbNlAgAJ */
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)

        /* Located in generated directory. This file is populated by the
         * zephyr_linker_sources() Cmake function.
         */
        #include <snippets-rwdata.ld>

	    __data_end = .;

        } GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
	    __data_size = __data_end - __data_start;


	    __data_load_start = LOADADDR(_DATA_SECTION_NAME);

            #include <zephyr/linker/common-ram.ld>
            #include <zephyr/linker/cplusplus-ram.ld>

            /* Located in generated directory. This file is populated by the
             * zephyr_linker_sources() Cmake function.
             */
            #include <snippets-data-sections.ld>

        __data_region_end = .;
        __data_region_load_start = LOADADDR(_DATA_SECTION_NAME);

        SECTION_DATA_PROLOGUE(_BSS_SECTION_NAME,(NOLOAD),)
        {
        /*
         * For performance, BSS section is assumed to be 4 byte aligned and
         * a multiple of 4 bytes, so it can be cleared in words.
         */
         . = ALIGN(4);
         __bss_start = .;

         *(.bss .bss.*)
         *(.sbss .sbss.*)
         COMMON_SYMBOLS

         /* Ensure 4 byte alignment for the entire section. */
         . = ALIGN(4);
          __bss_end = .;
        } GROUP_DATA_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)

        SECTION_PROLOGUE(_NOINIT_SECTION_NAME,(NOLOAD),)
        {
        /*
         * This section is used for non-initialized objects that
         * will not be cleared during the boot process.
         */
         *(.noinit .noinit.*)

        /* Located in generated directory. This file is populated by the
         * zephyr_linker_sources() Cmake function.
         */
        #include <snippets-noinit.ld>

        } GROUP_LINK_IN(RAMABLE_REGION)

        /* Located in generated directory. This file is populated by the
         * zephyr_linker_sources() Cmake function.
         */
        #include <snippets-ram-sections.ld>

        /* Located in generated directory. This file is populated by the
         * zephyr_linker_sources() Cmake function.
         */
        #include <snippets-sections.ld>

        #include <zephyr/linker/ram-end.ld>

        GROUP_END(RAMABLE_REGION)

        #ifdef CONFIG_GEN_ISR_TABLES
        /* Bogus section, post-processed during the build to initialize interrupts. */
        #include <zephyr/linker/intlist.ld>
        #endif

        #include <zephyr/linker/debug-sections.ld>

	/DISCARD/ : { *(.note.GNU-stack) }

        SECTION_PROLOGUE(.riscv.attributes, 0,)
        {
            KEEP(*(.riscv.attributes))
            KEEP(*(.gnu.attributes))
        }

        /*
         * Pulpino toolchains emit these sections; we don't care about them,
         * but need to avoid build system warnings about orphaned sections.
         */
        SECTION_PROLOGUE(.Pulp_Chip.Info,,)
        {
            *(.Pulp_Chip.*)
        }

    }
