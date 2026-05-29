#!/usr/bin/env python3
"""Add PE base relocations to a UEFI PE32+ binary for ARM64.

Objcopy for aarch64-elf doesn't convert ELF R_AARCH64_RELATIVE to
PE IMAGE_REL_BASED_DIR64. This script parses the ELF kernel's rela.dyn
section and injects the corresponding .reloc section into the PE file.
"""

import struct
import os
import sys


def read_elf_rela(elf_path):
    """Extract R_AARCH64_RELATIVE relocation offsets from ELF file.
    Returns list of RVAs (offsets from image base 0).
    """
    with open(elf_path, 'rb') as f:
        data = f.read()

    if data[:4] != b'\x7fELF':
        print(f"  Not an ELF file: {elf_path}")
        return []
    if data[4] != 2:  # 64-bit
        print(f"  Not 64-bit ELF")
        return []

    # Parse ELF header
    e_shoff = struct.unpack('<Q', data[0x28:0x30])[0]
    e_shentsize = struct.unpack('<H', data[0x3A:0x3C])[0]
    e_shnum = struct.unpack('<H', data[0x3C:0x3E])[0]
    e_shstrndx = struct.unpack('<H', data[0x3E:0x40])[0]

    # Find .rela.dyn section
    shstrtab_off = e_shoff + e_shstrndx * e_shentsize
    shstrtab_addr = struct.unpack('<Q', data[shstrtab_off+0x18:shstrtab_off+0x20])[0]
    shstrtab_size = struct.unpack('<Q', data[shstrtab_off+0x20:shstrtab_off+0x28])[0]
    strtab = data[shstrtab_addr:shstrtab_addr+shstrtab_size]

    relocs = []
    for i in range(e_shnum):
        sh_off = e_shoff + i * e_shentsize
        sh_name = struct.unpack('<I', data[sh_off+0x00:sh_off+0x04])[0]
        sh_type = struct.unpack('<I', data[sh_off+0x04:sh_off+0x08])[0]
        sh_addr = struct.unpack('<Q', data[sh_off+0x10:sh_off+0x18])[0]
        sh_offset = struct.unpack('<Q', data[sh_off+0x18:sh_off+0x20])[0]
        sh_size = struct.unpack('<Q', data[sh_off+0x20:sh_off+0x28])[0]

        name = strtab[sh_name:strtab.index(b'\x00', sh_name)].decode()
        if name == '.rela.dyn' and sh_type == 4:  # SHT_RELA
            for j in range(0, sh_size, 24):
                r_offset = struct.unpack('<Q', data[sh_offset+j:sh_offset+j+8])[0]
                r_info = struct.unpack('<Q', data[sh_offset+j+8:sh_offset+j+16])[0]
                r_type = r_info & 0xFFFFFFFF
                if r_type == 0x403:  # R_AARCH64_RELATIVE
                    relocs.append(r_offset)
            break

    return relocs


def add_pe_reloc_section(pe_path, rvas):
    """Add a .reloc section to a PE32+ file with given RVA list."""
    if not rvas:
        print("  No relocations needed")
        return True

    with open(pe_path, 'rb') as f:
        data = bytearray(f.read())

    e_lfanew = struct.unpack('<I', data[0x3C:0x40])[0]
    pe_sig = e_lfanew  # e_lfanew points directly to "PE\0\0"
    if bytes(data[pe_sig:pe_sig+4]) != b'PE\x00\x00':
        print(f"  Not a PE file (sig={bytes(data[pe_sig:pe_sig+4])})")
        return False

    # COFF header
    coff = pe_sig + 4
    num_sections = struct.unpack('<H', data[coff+2:coff+4])[0]
    opt_hdr_size = struct.unpack('<H', data[coff+16:coff+18])[0]

    # Fix COFF characteristics: objcopy sets IMAGE_FILE_RELOCS_STRIPPED (0x0001)
    # but we're adding relocations back. The UEFI loader checks this flag:
    # if set, it refuses to load the image (returns Unsupported).
    chars_off = coff + 18
    characteristics = struct.unpack('<H', data[chars_off:chars_off+2])[0]
    characteristics &= ~0x0001          # Clear RELOCS_STRIPPED
    characteristics |= 0x0020           # Set LARGE_ADDRESS_AWARE (required for ARM64)
    struct.pack_into('<H', data, chars_off, characteristics)

    # Optional header (PE32+)
    opt = coff + 20
    magic = struct.unpack('<H', data[opt:opt+2])[0]
    if magic != 0x20B:
        print(f"  Not PE32+ (magic=0x{magic:04X})")
        return False

    sect_align = struct.unpack('<I', data[opt+32:opt+36])[0]
    file_align = struct.unpack('<I', data[opt+36:opt+40])[0]
    image_size_off = opt + 56
    image_size = struct.unpack('<I', data[image_size_off:image_size_off+4])[0]
    headers_size = struct.unpack('<I', data[opt+60:opt+64])[0]
    num_rva = struct.unpack('<I', data[opt+108:opt+112])[0]

    # Build .reloc section data
    # Group RVAs by 4KB page
    pages = {}
    for rva in rvas:
        page = (rva // 0x1000) * 0x1000
        offset = rva - page
        entry = (0xA << 12) | offset  # IMAGE_REL_BASED_DIR64
        pages.setdefault(page, []).append(entry)

    reloc_data = bytearray()
    for page in sorted(pages):
        entries = pages[page]
        block_size = 8 + len(entries) * 2
        reloc_data += struct.pack('<II', page, block_size)
        for entry in entries:
            reloc_data += struct.pack('<H', entry)

    # Align reloc data to file_align
    reloc_size = len(reloc_data)
    padded_size = (reloc_size + file_align - 1) // file_align * file_align
    reloc_data += b'\x00' * (padded_size - reloc_size)

    # Section header starts after the last existing section header
    last_sect_off = opt + 112 + num_rva * 8 + num_sections * 40

    # Compute virtual address for new section
    # Find last section's VA and size
    max_va = 0
    max_end = 0
    s = opt + 112 + num_rva * 8
    for i in range(num_sections):
        vsize = struct.unpack('<I', data[s+8:s+12])[0]
        vaddr = struct.unpack('<I', data[s+12:s+16])[0]
        end = vaddr + vsize
        if end > max_end:
            max_end = end
            max_va = vaddr
        s += 40
    new_va = (max_end + sect_align - 1) // sect_align * sect_align

    # Current file size (end of data)
    current_size = len(data)

    # Align to file_align for the new section data
    raw_offset = (current_size + file_align - 1) // file_align * file_align
    padding_needed = raw_offset - current_size

    # Pad file to raw_offset
    data += b'\x00' * padding_needed

    # Write new section data
    data += reloc_data

    # Add section header
    new_sect = bytearray(40)
    name = b'.reloc\x00\x00'
    new_sect[0:8] = name
    struct.pack_into('<I', new_sect, 8, reloc_size)  # VirtualSize
    struct.pack_into('<I', new_sect, 12, new_va)     # VirtualAddress
    struct.pack_into('<I', new_sect, 16, reloc_size) # SizeOfRawData
    struct.pack_into('<I', new_sect, 20, raw_offset) # PointerToRawData
    struct.pack_into('<I', new_sect, 36, 0x42000040)

    # Insert section header before padding/gap, or at the end
    data[last_sect_off:last_sect_off] = new_sect

    # Update number of sections
    struct.pack_into('<H', data, coff+2, num_sections + 1)

    # Update SizeOfImage
    new_image_size = new_va + (reloc_size + sect_align - 1) // sect_align * sect_align
    struct.pack_into('<I', data, image_size_off, new_image_size)

    # Set Base Relocation Table data directory entry
    dd_reloc_off = opt + 112 + 5 * 8  # Reloc is index 5
    struct.pack_into('<I', data, dd_reloc_off, new_va)
    struct.pack_into('<I', data, dd_reloc_off + 4, reloc_size)

    with open(pe_path, 'wb') as f:
        f.write(data)

    print(f"  Added .reloc section: {len(rvas)} entries at RVA 0x{new_va:X}")
    return True


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    build_dir = os.path.join(project_dir, 'build')
    pe_path = os.path.join(build_dir, 'BOOTAA64.EFI')
    elf_path = os.path.join(build_dir, 'kernel.so')

    if not os.path.exists(pe_path):
        print(f"ERROR: {pe_path} not found")
        sys.exit(1)
    if not os.path.exists(elf_path):
        print(f"ERROR: {elf_path} not found")
        sys.exit(1)

    print("  Adding PE base relocations...")
    rvas = read_elf_rela(elf_path)
    if rvas:
        print(f"  Found {len(rvas)} R_AARCH64_RELATIVE relocations: {[hex(r) for r in rvas]}")
    if add_pe_reloc_section(pe_path, rvas):
        print("  Relocation section added successfully")


if __name__ == '__main__':
    main()
