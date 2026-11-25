"""Disassemble a small ARM64 IL2CPP RVA range from an Android ELF.

This is a read-only reverse-engineering helper. Addresses accepted on the
command line are RVAs as printed by Il2CppDumper (for example, 0x3397400).
"""

from __future__ import annotations

import argparse
from pathlib import Path

from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs
from elftools.elf.elffile import ELFFile


def rva_to_file_offset(elf: ELFFile, rva: int) -> int:
    for segment in elf.iter_segments():
        if segment["p_type"] != "PT_LOAD":
            continue
        start = int(segment["p_vaddr"])
        end = start + int(segment["p_filesz"])
        if start <= rva < end:
            return int(segment["p_offset"]) + (rva - start)
    raise ValueError(f"RVA 0x{rva:x} is outside the ELF load segments")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("rva", type=lambda value: int(value, 0), nargs="+")
    parser.add_argument("--bytes", type=lambda value: int(value, 0), default=0x100)
    parser.add_argument(
        "--xrefs",
        action="store_true",
        help="scan executable segments for direct branch references to each RVA",
    )
    args = parser.parse_args()

    disassembler = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        if args.xrefs:
            targets = set(args.rva)
            references: dict[int, list[int]] = {target: [] for target in targets}
            for segment in elf.iter_segments():
                if segment["p_type"] != "PT_LOAD" or not int(segment["p_flags"]) & 1:
                    continue
                start = int(segment["p_vaddr"])
                code = segment.data()
                for instruction in disassembler.disasm(code, start):
                    if instruction.mnemonic not in {"b", "bl"}:
                        continue
                    try:
                        target = int(instruction.op_str.lstrip("#"), 0)
                    except ValueError:
                        continue
                    if target in references:
                        references[target].append(instruction.address)
            for target in args.rva:
                callers = ", ".join(f"0x{caller:X}" for caller in references[target])
                print(f"XREF 0x{target:X}: {callers or '(none)'}")

        for rva in args.rva:
            offset = rva_to_file_offset(elf, rva)
            stream.seek(offset)
            code = stream.read(args.bytes)
            print(f"\nRVA 0x{rva:X} (file offset 0x{offset:X})")
            for instruction in disassembler.disasm(code, rva):
                print(
                    f"0x{instruction.address:08X}: "
                    f"{instruction.mnemonic:<8} {instruction.op_str}"
                )


if __name__ == "__main__":
    main()
