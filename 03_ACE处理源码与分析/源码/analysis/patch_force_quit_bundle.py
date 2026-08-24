import argparse
import hashlib
import io
import os
import struct
from dataclasses import dataclass
from pathlib import Path

import lz4.block


OLD_SEQUENCE = bytes.fromhex("7B 93 29 01 04 17 33 50 28 87 24 02 06")
NEW_SEQUENCE = bytes.fromhex("7B 93 29 01 04 26 2B 4F 28 87 24 02 06")
# Unity 2022.3.62f3 / current preternatural cache build.
CURRENT_OLD_SEQUENCE = bytes.fromhex("7B 42 14 01 04 17 33 50 02 28 39 6A 00 06")
CURRENT_NEW_SEQUENCE = bytes.fromhex("7B 42 14 01 04 26 2B 4F 02 28 39 6A 00 06")


@dataclass
class Block:
    uncompressed_size: int
    compressed_size: int
    flags: int


@dataclass
class Node:
    offset: int
    size: int
    flags: int
    path: str


@dataclass
class Bundle:
    signature: str
    version: int
    player_version: str
    engine_version: str
    flags: int
    info_hash: bytes
    blocks: list[Block]
    nodes: list[Node]
    data: bytearray


def read_cstring(stream: io.BufferedIOBase) -> str:
    value = bytearray()
    while True:
        current = stream.read(1)
        if not current:
            raise EOFError("unterminated string")
        if current == b"\0":
            return value.decode("utf-8")
        value.extend(current)


def write_cstring(stream: io.BufferedIOBase, value: str) -> None:
    stream.write(value.encode("utf-8") + b"\0")


def read_be(stream: io.BufferedIOBase, fmt: str):
    size = struct.calcsize(">" + fmt)
    value = stream.read(size)
    if len(value) != size:
        raise EOFError(f"expected {size} bytes, got {len(value)}")
    return struct.unpack(">" + fmt, value)[0]


def write_be(stream: io.BufferedIOBase, fmt: str, value) -> None:
    stream.write(struct.pack(">" + fmt, value))


def align_read(stream: io.BufferedIOBase, alignment: int) -> None:
    stream.seek((-stream.tell()) % alignment, os.SEEK_CUR)


def align_write(stream: io.BufferedIOBase, alignment: int) -> None:
    stream.write(b"\0" * ((-stream.tell()) % alignment))


def decompress(data: bytes, compression: int, expected_size: int) -> bytes:
    if compression == 0:
        result = data
    elif compression in (2, 3):
        result = lz4.block.decompress(data, uncompressed_size=expected_size)
    else:
        raise ValueError(f"unsupported UnityFS compression type {compression}")
    if len(result) != expected_size:
        raise ValueError(f"decompressed size mismatch: {len(result)} != {expected_size}")
    return result


def compress(data: bytes, compression: int) -> bytes:
    if compression == 0:
        return data
    if compression == 2:
        return lz4.block.compress(data, mode="fast", store_size=False)
    if compression == 3:
        return lz4.block.compress(
            data,
            mode="high_compression",
            compression=12,
            store_size=False,
        )
    raise ValueError(f"unsupported UnityFS compression type {compression}")


def parse_bundle(path: Path) -> Bundle:
    raw = path.read_bytes()
    source = io.BytesIO(raw)
    signature = read_cstring(source)
    if signature != "UnityFS":
        raise ValueError(f"unexpected signature {signature!r}")
    version = read_be(source, "I")
    player_version = read_cstring(source)
    engine_version = read_cstring(source)
    bundle_size = read_be(source, "Q")
    compressed_info_size = read_be(source, "I")
    uncompressed_info_size = read_be(source, "I")
    flags = read_be(source, "I")
    header_end = source.tell()
    if bundle_size != len(raw):
        raise ValueError(f"bundle size mismatch: {bundle_size} != {len(raw)}")

    info_at_end = bool(flags & 0x80)
    if info_at_end:
        source.seek(bundle_size - compressed_info_size)
    else:
        if flags & 0x200:
            align_read(source, 16)
    compressed_info = source.read(compressed_info_size)
    info = io.BytesIO(decompress(compressed_info, flags & 0x3F, uncompressed_info_size))

    info_hash = info.read(16)
    blocks = [
        Block(read_be(info, "I"), read_be(info, "I"), read_be(info, "H"))
        for _ in range(read_be(info, "I"))
    ]
    nodes = [
        Node(read_be(info, "Q"), read_be(info, "Q"), read_be(info, "I"), read_cstring(info))
        for _ in range(read_be(info, "I"))
    ]

    if info_at_end:
        source.seek(header_end)
        if flags & 0x200:
            align_read(source, 16)
    elif flags & 0x200:
        align_read(source, 16)

    data = bytearray()
    for index, block in enumerate(blocks):
        encoded = source.read(block.compressed_size)
        if len(encoded) != block.compressed_size:
            raise EOFError(f"truncated data block {index}")
        data.extend(decompress(encoded, block.flags & 0x3F, block.uncompressed_size))

    for node in nodes:
        if node.offset + node.size > len(data):
            raise ValueError(f"node {node.path!r} exceeds bundle data")

    return Bundle(
        signature,
        version,
        player_version,
        engine_version,
        flags,
        info_hash,
        blocks,
        nodes,
        data,
    )


def build_block_info(bundle: Bundle, compressed_blocks: list[bytes]) -> bytes:
    info = io.BytesIO()
    info.write(bundle.info_hash)
    write_be(info, "I", len(bundle.blocks))
    for block, compressed in zip(bundle.blocks, compressed_blocks):
        write_be(info, "I", block.uncompressed_size)
        write_be(info, "I", len(compressed))
        write_be(info, "H", block.flags)
    write_be(info, "I", len(bundle.nodes))
    for node in bundle.nodes:
        write_be(info, "Q", node.offset)
        write_be(info, "Q", node.size)
        write_be(info, "I", node.flags)
        write_cstring(info, node.path)
    return info.getvalue()


def write_bundle(bundle: Bundle, destination: Path) -> None:
    compressed_blocks = []
    cursor = 0
    for block in bundle.blocks:
        block_data = bytes(bundle.data[cursor : cursor + block.uncompressed_size])
        cursor += block.uncompressed_size
        compressed_blocks.append(compress(block_data, block.flags & 0x3F))
    if cursor != len(bundle.data):
        raise ValueError(f"block sizes cover {cursor} bytes, data has {len(bundle.data)}")

    raw_info = build_block_info(bundle, compressed_blocks)
    compressed_info = compress(raw_info, bundle.flags & 0x3F)
    output = io.BytesIO()
    write_cstring(output, bundle.signature)
    write_be(output, "I", bundle.version)
    write_cstring(output, bundle.player_version)
    write_cstring(output, bundle.engine_version)
    size_position = output.tell()
    write_be(output, "Q", 0)
    write_be(output, "I", len(compressed_info))
    write_be(output, "I", len(raw_info))
    write_be(output, "I", bundle.flags)

    info_at_end = bool(bundle.flags & 0x80)
    if bundle.flags & 0x200:
        align_write(output, 16)
    if info_at_end:
        for block in compressed_blocks:
            output.write(block)
        output.write(compressed_info)
    else:
        output.write(compressed_info)
        if bundle.flags & 0x200:
            align_write(output, 16)
        for block in compressed_blocks:
            output.write(block)

    final_size = output.tell()
    output.seek(size_position)
    write_be(output, "Q", final_size)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output.getvalue())


def patch_bundle(bundle: Bundle) -> list[tuple[str, int]]:
    patched = []
    for node in bundle.nodes:
        node_data = bytes(bundle.data[node.offset : node.offset + node.size])
        matches = [(OLD_SEQUENCE, NEW_SEQUENCE), (CURRENT_OLD_SEQUENCE, CURRENT_NEW_SEQUENCE)]
        found = [(old, new, node_data.count(old)) for old, new in matches if node_data.count(old)]
        if not found:
            continue
        if len(found) != 1 or found[0][2] != 1:
            raise ValueError(f"node {node.path!r} has ambiguous patch anchors: {[(len(a), c) for a, _, c in found]}")
        old, new, _ = found[0]
        relative_offset = node_data.index(old)
        absolute_offset = node.offset + relative_offset
        bundle.data[absolute_offset : absolute_offset + len(old)] = new
        patched.append((node.path, relative_offset + 5))
    if len(patched) != 1 or not patched[0][0].startswith("CAB-"):
        raise ValueError(f"expected one patched CAB node, got {patched}")
    return patched


def verify_bundle(path: Path, expected: list[tuple[str, int]]) -> None:
    bundle = parse_bundle(path)
    actual = []
    for node in bundle.nodes:
        node_data = bytes(bundle.data[node.offset : node.offset + node.size])
        if OLD_SEQUENCE in node_data or CURRENT_OLD_SEQUENCE in node_data:
            raise ValueError(f"old sequence remains in {node.path!r}")
        cursor = 0
        for new in (NEW_SEQUENCE, CURRENT_NEW_SEQUENCE):
            cursor = 0
            while True:
                cursor = node_data.find(new, cursor)
                if cursor < 0:
                    break
                actual.append((node.path, cursor + 5))
                cursor += 1
    if sorted(actual) != sorted(expected):
        raise ValueError(f"verification mismatch: expected {expected}, got {actual}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    source_hash = hashlib.sha256(args.source.read_bytes()).hexdigest().upper()
    bundle = parse_bundle(args.source)
    patched = patch_bundle(bundle)
    write_bundle(bundle, args.destination)
    verify_bundle(args.destination, patched)
    destination_hash = hashlib.sha256(args.destination.read_bytes()).hexdigest().upper()

    print(f"source_sha256={source_hash}")
    print(f"destination_sha256={destination_hash}")
    for path, offset in patched:
        print(f"patched={path} file_offset=0x{offset:X} old=17-33-50 new=26-2B-4F")
    print(f"output={args.destination} size={args.destination.stat().st_size}")


if __name__ == "__main__":
    main()
