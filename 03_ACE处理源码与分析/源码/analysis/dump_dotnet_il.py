import argparse
from pathlib import Path

import dnfile
from dncil.cil.body.reader import read_method_body_from_bytes
from dncil.clr.token import StringToken, Token


TABLE_NAMES = {
    0x01: "TypeRef",
    0x02: "TypeDef",
    0x04: "Field",
    0x06: "MethodDef",
    0x0A: "MemberRef",
    0x11: "StandAloneSig",
    0x1B: "TypeSpec",
    0x2B: "MethodSpec",
}


def full_type_name(row) -> str:
    namespace = str(getattr(row, "TypeNamespace", ""))
    name = str(getattr(row, "TypeName", "?"))
    return f"{namespace}.{name}" if namespace else name


def build_owner_maps(pe):
    method_owners = {}
    field_owners = {}
    for typedef in pe.net.mdtables.TypeDef.rows:
        owner = full_type_name(typedef)
        for item in typedef.MethodList:
            method_owners[item.row_index] = owner
        for item in typedef.FieldList:
            field_owners[item.row_index] = owner
    return method_owners, field_owners


def format_index(index, method_owners, field_owners) -> str:
    if index is None or index.row is None:
        return "?"
    table_name = index.table.name
    row = index.row
    if table_name in ("TypeDef", "TypeRef"):
        return full_type_name(row)
    if table_name == "MethodDef":
        return f"{method_owners.get(index.row_index, '?')}::{row.Name}"
    if table_name == "Field":
        return f"{field_owners.get(index.row_index, '?')}::{row.Name}"
    if table_name == "MemberRef":
        parent = format_index(row.Class, method_owners, field_owners)
        return f"{parent}::{row.Name}"
    if table_name == "MethodSpec":
        return format_index(row.Method, method_owners, field_owners)
    return f"{table_name}[{index.row_index}]"


def resolve_token(pe, token: Token, method_owners, field_owners) -> str:
    if isinstance(token, StringToken) or token.table == 0x70:
        value = pe.net.user_strings.get(token.rid, errors="replace")
        return repr(str(value)) if value is not None else str(token)
    table_name = TABLE_NAMES.get(token.table)
    if table_name is None:
        return str(token)
    table = getattr(pe.net.mdtables, table_name, None)
    if table is None or token.rid < 1 or token.rid > len(table.rows):
        return str(token)
    row = table.rows[token.rid - 1]
    if table_name in ("TypeDef", "TypeRef"):
        return full_type_name(row)
    if table_name == "MethodDef":
        return f"{method_owners.get(token.rid, '?')}::{row.Name}"
    if table_name == "Field":
        return f"{field_owners.get(token.rid, '?')}::{row.Name}"
    if table_name == "MemberRef":
        return f"{format_index(row.Class, method_owners, field_owners)}::{row.Name}"
    if table_name == "MethodSpec":
        return format_index(row.Method, method_owners, field_owners)
    return f"{table_name}[{token.rid}]"


def dump_method(pe, owner: str, method_index, method_owners, field_owners) -> None:
    row = method_index.row
    token = 0x06000000 | method_index.row_index
    print(f"\n=== {owner}::{row.Name} token=0x{token:08X} RVA=0x{row.Rva:X} ===")
    if not row.Rva:
        print("<no method body>")
        return
    body = read_method_body_from_bytes(pe.get_data(row.Rva, 0x100000))
    print(
        f"header={body.header_size} code_size={body.code_size} "
        f"max_stack={body.max_stack} locals={body.local_var_sig_tok}"
    )
    for instruction in body.instructions:
        operand = instruction.operand
        if isinstance(operand, Token):
            operand_text = resolve_token(pe, operand, method_owners, field_owners)
        elif isinstance(operand, list):
            operand_text = ", ".join(f"IL_{value:04X}" for value in operand)
        elif operand is None:
            operand_text = ""
        elif isinstance(operand, int) and instruction.opcode.name.startswith(("br", "leave", "beq", "bne", "bge", "bgt", "ble", "blt")):
            operand_text = f"IL_{operand:04X}"
        else:
            operand_text = str(operand)
        raw = " ".join(f"{byte:02X}" for byte in instruction.get_bytes())
        print(f"IL_{instruction.offset:04X}  {raw:<24} {instruction.opcode.name:<14} {operand_text}")
    if body.exception_handlers:
        print("exception handlers:")
        for handler in body.exception_handlers:
            print(f"  {handler}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("assembly", type=Path)
    parser.add_argument("targets", nargs="+", help="Type or Type::Method filters")
    args = parser.parse_args()

    pe = dnfile.dnPE(str(args.assembly))
    method_owners, field_owners = build_owner_maps(pe)
    for target in args.targets:
        if "::" in target:
            type_name, method_name = target.split("::", 1)
        else:
            type_name, method_name = target, None
        matches = [
            row
            for row in pe.net.mdtables.TypeDef.rows
            if full_type_name(row) == type_name
        ]
        if not matches:
            print(f"\n=== missing type {type_name} ===")
            continue
        for typedef in matches:
            methods = [
                item
                for item in typedef.MethodList
                if method_name is None or str(item.row.Name) == method_name
            ]
            if not methods:
                print(f"\n=== missing method {target} ===")
            for method in methods:
                dump_method(pe, type_name, method, method_owners, field_owners)


if __name__ == "__main__":
    main()
