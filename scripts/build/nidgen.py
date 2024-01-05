#!/usr/bin/env python3

# Copyright 2024 STMicroelectronics
# SPDX-License-Identifier: Apache-2.0

"""
Generates NIDs in import tables of LLEXT files and export tables of Zephyr executable files

NIDs correspond to the first [pointer size] bytes of the SHA-256 hash of the export's name,
taken as a big-endian integer. They occupy less space than strings while allowing faster
linking (via dichotomic search) and offering more confidentiality.
"""


from elftools.elf.elffile import ELFFile
from elftools.elf.sections import Section, SymbolTableSection
from Crypto.Hash import SHA256  #Requires pycryptodome (new dependency :s)
from enum import Enum

import argparse
import logging
import pathlib
import struct
import sys

#Name of sections holding information about the symbols
#exported for use by LLEXTs in a Zephyr executable
ZEPHEXEC_EXPORTS_SYMTBL_SECTION_NAME = "llext_const_symbol_area"
ZEPHEXEC_EXPORTS_STRTBL_SECTION_NAME = "llext_exports_strtab"

#Name of section holding information about the symbols
#exported for use by Zephyr applications in an LLEXT
LLEXT_EXPORTS_SYMTBL_SECTION_NAME = ".exported_sym"

#Part of SHF_MASKOS, reserved for the operating system.
SHF_ZEPHYR_NIDGEN_PERFORMED = 0x08000000

def _print_NID(name, nid, elfclass, log):
    if elfclass == 32:
        log.info(f"0x{nid:08X} -> {name}")
    else:
        log.info(f"0x{nid:016X} -> {name}")

def _generate_NID(name, nid_size):
    """Generates a Numeric Identifier for 'name' and returns it as an integer."""
    digest = SHA256.new(name.encode('ascii')).digest()[0:nid_size]
    return int.from_bytes(digest, byteorder='big', signed=False)

def _make_elf_sym_st_value_writer(fd, symtab_offset):
    elf = ELFFile(fd)

    byteorder = "little" if elf.little_endian else "big"

    if (elf.elfclass == 32):
        ELF_SYM_SIZE = 0x10    #sizeof(Elf32_Sym)
        ST_VALUE_OFFSET = 0x4  #offsetof(Elf32_Sym, st_value)
        ST_VALUE_SIZE = 0x4    #sizeof(Elf32_Sym.st_value)
    else:
        ELF_SYM_SIZE = 0x18
        ST_VALUE_OFFSET = 0x8
        ST_VALUE_SIZE = 0x8

    def _do_write(sym_idx, val):
        st_value = int.to_bytes(val, ST_VALUE_SIZE, byteorder)
        fd.seek(symtab_offset + sym_idx * ELF_SYM_SIZE + ST_VALUE_OFFSET)
        fd.write(st_value)

    return _do_write

def process_llext_file(elf_fd, elffile, args, log):
    ELF_SYMTBL_SECTION_NAMES = [
        ".symtab", ".dynsym"
    ]

    #Find symbol table
    symtab = None
    for section_name in ELF_SYMTBL_SECTION_NAMES:
        symtab = elffile.get_section_by_name(section_name)
        if isinstance(symtab, SymbolTableSection):
            log.info(f"processing '{section_name}' symbol table...")
            log.debug(f"(symbol table is at file offset 0x{symtab['sh_offset']:X})")
            break
        else:
            log.debug(f"section {section_name} not found.")

    if symtab == None:
        log.error("no symbol table found in file")
        return 1

    #Find all imports in symbol table
    i = 0
    imports = []
    for sym in symtab.iter_symbols():
        #Check if symbol is an imported symbol
        if sym.entry['st_info']['type'] == 'STT_NOTYPE' and \
            sym.entry['st_info']['bind'] == 'STB_GLOBAL' and \
            sym.entry['st_shndx'] == 'SHN_UNDEF':

            log.debug(f"found imported symbol '{sym.name}' at index {i}")
            imports.append((i, sym))

        i += 1

    _write_st_value = _make_elf_sym_st_value_writer(elf_fd, symtab['sh_offset'])

    log.info(f"LLEXT has {len(imports)} import(s)")
    for (index, symbol) in imports:
        nid = _generate_NID(symbol.name, elffile.elfclass // 8)
        _write_st_value(index, nid)

        _print_NID(symbol.name, nid, elffile.elfclass, log)

    return 0

def _read_asciiz_string(fd, offset):
    s = b''
    fd.seek(offset)

    c = fd.read(1)
    while c != b'\0':
        s += c
        c = fd.read(1)

    return s.decode("ascii")

def _set_nidgen_shdr_flag(elf_fd, elffile, log):
    def _make_sh_flags_encdec(elf_class, little_endian):
        byteorder = 'little' if little_endian else 'big'
        size = 4 if elf_class == 32 else 8
        return (
            lambda intg : int.to_bytes(intg, 4 if elf_class == 32 else 8, byteorder),
            lambda fd : int.from_bytes(fd.read(size), byteorder)
        )

    (encode_sh_flags, read_and_dec_sh_flags) = _make_sh_flags_encdec(elffile.elfclass, elffile.little_endian)

    OFFSETOF_SH_FLAGS = 8 #Valid for both 32 and 64-bit ELF

    exp_tbl_shdr_index = elffile.get_section_index(ZEPHEXEC_EXPORTS_SYMTBL_SECTION_NAME)
    exp_tbl_file_off = elffile['e_shoff'] + exp_tbl_shdr_index * elffile['e_shentsize']

    log.debug(f"export table section header at file offset 0x{exp_tbl_file_off:X}")

    elf_fd.seek(exp_tbl_file_off + OFFSETOF_SH_FLAGS)
    sh_flags = read_and_dec_sh_flags(elf_fd)

    log.debug(f"old export table sh_flags: 0x{sh_flags:X}")
    sh_flags |= SHF_ZEPHYR_NIDGEN_PERFORMED
    log.debug(f"new export table sh_flags: 0x{sh_flags:X}")

    elf_fd.seek(exp_tbl_file_off + OFFSETOF_SH_FLAGS)
    elf_fd.write(encode_sh_flags(sh_flags))

class ExportSymbolTableManipulator():
    """Class used to manipulate the export table structure.

    This class is tightly coupled to the 'struct llext_const_symbol'
    structure found in include/llext/symbol.h - when this structure
    is update, edit this class to reflect changes and script should
    function again (hopefully).
    """
    def __init__(self, elf_fd, symtbl_file_offset, endianness, pointer_size):
        self.fd = elf_fd
        self.symtbl_file_offset = symtbl_file_offset

        #See struct llext_const_symbol.
        endian_prefix = "<" if endianness == "little" else ">"
        pointer = "I" if pointer_size == 4 else "Q"

        struct_spec = endian_prefix + 2 * pointer
        self._sym_ent_struct = struct.Struct(struct_spec)
        self.structure_size = self._sym_ent_struct.size

    def read_symbol_info(self, symbol_index):
        """Read symbol information from file. Returns (name offset in strtab, symbol address)"""
        offset = self.symtbl_file_offset + self._sym_ent_struct.size * symbol_index
        self.fd.seek(offset)

        return self._sym_ent_struct.unpack(self.fd.read(self._sym_ent_struct.size))

    def write_symbol_info(self, symbol_index, nid, symbol_address):
        """Write symbol information to file."""
        offset = self.symtbl_file_offset + self._sym_ent_struct.size * symbol_index
        self.fd.seek(offset)
        self.fd.write(self._sym_ent_struct.pack(nid, symbol_address))

class SectionWrp():
    """Wrapper class for pyelftools Section object"""
    def __init__(self, elffile, section_name):
        self.name = section_name
        self.section = elffile.get_section_by_name(section_name)
        if not isinstance(self.section, Section):
            raise KeyError(f"section {section_name} not found")

        self.size = self.section['sh_size']
        self.flags = self.section['sh_flags']
        self.offset = self.section['sh_offset']

def process_zephyr_exec_file(elf_fd, elffile, args, log):
    try:
        exp_tbl_section = SectionWrp(elffile, ZEPHEXEC_EXPORTS_SYMTBL_SECTION_NAME)
        strtab_section = SectionWrp(elffile, ZEPHEXEC_EXPORTS_STRTBL_SECTION_NAME)
    except KeyError as e:
        log.error(e.args[0])
        return 1

    symtab_manipulator = ExportSymbolTableManipulator(elf_fd, exp_tbl_section.offset,
                                                      args.endianness, args.pointer_size)

    if (exp_tbl_section.flags & SHF_ZEPHYR_NIDGEN_PERFORMED) != 0:
        log.warning(f"not processing ELF because export table section already has NIDGEN_PERFORMED flag")
        return 0

    if (exp_tbl_section.size % symtab_manipulator.structure_size) != 0:
        log.error(f"export table size (0x{exp_tbl_section.size:X}) not aligned to export symbol structure size (0x{symtab_manipulator.structure_size:X})")
        return 1

    num_exports = exp_tbl_section.size // symtab_manipulator.structure_size

    log.debug(f"exports table found at file offset 0x{exp_tbl_section.offset:X}")
    log.debug(f"exports string table found at file offset 0x{strtab_section.offset:X}")
    log.info(f"{num_exports} symbols are exported to LLEXTs")

    #Step 1: read all exports
    exports = []
    for i in range(num_exports):
        (name_strtab_offset, sym_addr) = symtab_manipulator.read_symbol_info(i)
        if name_strtab_offset > strtab_section.size:
            log.error(f"Overflow: name_strtab_offset (0x{name_strtab_offset:X}) > export strtab section size (0x{strtab_section.size:X})")
            return 1

        name = _read_asciiz_string(elf_fd, strtab_section.offset + name_strtab_offset)
        exports.append((name, sym_addr))

    #Step 2: generate NIDs and save into a sorted table (ascending order)
    sorted_exp_tbl = dict()
    for export_name, export_addr in exports:
        nid = _generate_NID(export_name, args.pointer_size)

        collision = sorted_exp_tbl.get(nid)
        if collision != None:
            log.error(f"NID collision: exports {collision} and {export_name} have identical NID 0x{nid:X}!")
            return 1

        sorted_exp_tbl[nid] = (export_name, export_addr)

    sorted_exp_tbl = dict(sorted(sorted_exp_tbl.items()))

    #Step 3: write the new export table to file
    i = 0
    for nid, name_and_addr in sorted_exp_tbl.items():
        _print_NID(name_and_addr[0], nid, elffile.elfclass, log)
        symtab_manipulator.write_symbol_info(i, nid, name_and_addr[1])
        i += 1

    #Step 4: add NIDGEN_PERFORMED flag to section header
    _set_nidgen_shdr_flag(elf_fd, elffile, log)

    return 0

class ExecutableKind(Enum):
    Zephyr = 0
    LLEXT = 1
    Unknown = 2

def _autodetect_elf_type(elffile):
    """Attempt to automatically detect the kind of ELF we're processing."""
    llext_export_section = elffile.get_section_by_name(LLEXT_EXPORTS_SYMTBL_SECTION_NAME)
    if isinstance(llext_export_section, Section):
        return ExecutableKind.LLEXT

    zephyr_export_table_section = elffile.get_section_by_name(ZEPHEXEC_EXPORTS_SYMTBL_SECTION_NAME)
    if isinstance(zephyr_export_table_section, Section):
        return ExecutableKind.Zephyr

    return ExecutableKind.Unknown

def _parse_args(argv):
    """Parse the command line arguments."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)

    parser.add_argument("-f", "--elf-file", default=pathlib.Path("build", "zephyr", "zephyr.elf"),
                        help="ELF file to process")
    parser.add_argument("-v", "--verbose", action="count",
                        help=("enable verbose output, can be used multiple times "
                              "to increase verbosity level"))
    parser.add_argument("--always-succeed", action="store_true",
                        help="always exit with a return code of 0, used for testing")
    parser.add_argument("--exec-type", choices=['zephyr', 'llext'],
                        help=("type of ELF file to process (by default, attempt to ",
                              "automatically detect)"))
    parser.add_argument("--endianness", default='little', choices=['little', 'big'],
                        help=("endianness to use instead of deducing from ELF EI_DATA "
                              "(this option is ignored for LLEXT files)"))
    parser.add_argument("--pointer-size", type=int, choices=[32, 64],
                        help=("pointer size to use instead of deducing from ELF EI_CLASS "
                              "(this option is ignored for LLEXT files)"))

    return parser.parse_args(argv)

def _init_log(verbose):
    """Initialize a logger object."""
    log = logging.getLogger(__file__)

    console = logging.StreamHandler()
    console.setFormatter(logging.Formatter("%(levelname)s: %(message)s"))
    log.addHandler(console)

    if verbose and verbose > 1:
        log.setLevel(logging.DEBUG)
    elif verbose and verbose > 0:
        log.setLevel(logging.INFO)
    else:
        log.setLevel(logging.WARNING)

    return log

def main(argv=None):
    args = _parse_args(argv)

    log = _init_log(args.verbose)

    log.info(f"nidgen: processing {args.elf_file}")

    with open(args.elf_file, 'rb+') as fd:
        elf = ELFFile(fd)

        if not args.exec_type:
            log.info(f"attempting to detect executable type")
            exec_kind = _autodetect_elf_type(elf)
            if exec_kind == ExecutableKind.Zephyr:
                args.exec_type = "zephyr"
                log.info("detected Zephyr executable")
            elif exec_kind == ExecutableKind.LLEXT:
                args.exec_type = "llext"
                log.info("detected LLEXT executable")
            else:
                log.error("failed to detect executable type (please specify --exec-type)")
                return 1

        if not args.endianness:
            args.endianness = 'little' if elf.little_endian else 'big'

        if not args.pointer_size:
            #4 bytes for 32-bit, 8 bytes for 64-bit
            args.pointer_size = elf.elfclass // 8

        if args.exec_type == "zephyr":
            err = process_zephyr_exec_file(fd, elf, args, log)
        else:
            err = process_llext_file(fd, elf, args, log)

    if args.always_succeed:
        return 0

    return err

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
