import argparse
from pathlib import Path

import dnfile
from dncil.cil.body.reader import read_method_body_from_bytes
from dncil.clr.token import Token


def full_type_name(row) -> str:
    namespace = str(getattr(row, "TypeNamespace", ""))
    name = str(getattr(row, "TypeName", "?"))
    return f"{namespace}.{name}" if namespace else name


def build_method_owners(pe):
    owners = {}
    for typedef in pe.net.mdtables.TypeDef.rows:
        owner = full_type_name(typedef)
        for method in typedef.MethodList:
            owners[method.row_index] = owner
    return owners


def operand_text(value) -> str:
    if isinstance(value, Token):
        return str(value)
    if isinstance(value, list):
        return ", ".join(f"IL_{item:04X}" for item in value)
    if value is None:
        return ""
    return str(value)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("assembly", type=Path)
    parser.add_argument("value", type=lambda item: int(item, 0))
    parser.add_argument("--context", type=int, default=8)
    args = parser.parse_args()

    pe = dnfile.dnPE(str(args.assembly))
    owners = build_method_owners(pe)
    raw = args.assembly.read_bytes()
    pattern = b"\x20" + args.value.to_bytes(4, "little", signed=True)
    raw_hits = []
    cursor = 0
    while True:
        cursor = raw.find(pattern, cursor)
        if cursor < 0:
            break
        raw_hits.append(cursor)
        cursor += 1

    methods = []
    for index, row in enumerate(pe.net.mdtables.MethodDef.rows, 1):
        if row.Rva:
            methods.append((pe.get_offset_from_rva(row.Rva), index, row))
    methods.sort()

    matches = 0
    for raw_hit in raw_hits:
        candidates = [item for item in methods if item[0] <= raw_hit]
        if not candidates:
            continue
        for method_file_offset, index, row in reversed(candidates[-8:]):
            try:
                body = read_method_body_from_bytes(pe.get_data(row.Rva, 0x100000))
            except Exception:
                continue
            hit_indexes = [
                position
                for position, instruction in enumerate(body.instructions)
                if instruction.opcode.name.startswith("ldc.i4")
                and instruction.operand == args.value
            ]

            for position in hit_indexes:
                matches += 1
                token = 0x06000000 | index
                instruction = body.instructions[position]
                instruction_file_offset = method_file_offset + instruction.offset
                instruction_file_with_header = (
                    method_file_offset + body.header_size + instruction.offset
                )
                print(
                    f"\n=== {owners.get(index, '?')}::{row.Name} "
                    f"token=0x{token:08X} RVA=0x{row.Rva:X} "
                    f"method_file=0x{method_file_offset:X} raw_hit=0x{raw_hit:X} "
                    f"instruction_file=0x{instruction_file_offset:X} "
                    f"instruction_file_plus_header=0x{instruction_file_with_header:X} ==="
                )
                start = max(0, position - args.context)
                end = min(len(body.instructions), position + args.context + 1)
                for current in body.instructions[start:end]:
                    marker = ">>" if current is instruction else "  "
                    encoded = " ".join(f"{byte:02X}" for byte in current.get_bytes())
                    print(
                        f"{marker} IL_{current.offset:04X}  {encoded:<24} "
                        f"{current.opcode.name:<14} {operand_text(current.operand)}"
                    )

    print(f"\nraw_hits={len(raw_hits)} matches={matches}")


if __name__ == "__main__":
    main()
