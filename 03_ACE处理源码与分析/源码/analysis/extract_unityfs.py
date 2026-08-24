import argparse
import io
import os
import struct
from pathlib import Path

import lz4.block


def read_cstring(stream: io.BufferedIOBase) -> str:
    data = bytearray()
    while True:
        byte = stream.read(1)
        if not byte or byte == b"\0":
            return data.decode("utf-8", errors="replace")
        data.extend(byte)


def read_be(stream: io.BufferedIOBase, fmt: str):
    size = struct.calcsize(">" + fmt)
    data = stream.read(size)
    if len(data) != size:
        raise EOFError(f"expected {size} bytes, got {len(data)}")
    return struct.unpack(">" + fmt, data)[0]


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


def align(stream: io.BufferedIOBase, alignment: int) -> None:
    padding = (-stream.tell()) % alignment
    if padding:
        stream.seek(padding, os.SEEK_CUR)


def extract(bundle_path: Path, output_dir: Path) -> None:
    with bundle_path.open("rb") as source:
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

        info_at_end = bool(flags & 0x80)
        if info_at_end:
            source.seek(bundle_size - compressed_info_size)
        else:
            if flags & 0x200:
                align(source, 16)
            compressed_info = source.read(compressed_info_size)
            if len(compressed_info) != compressed_info_size:
                raise EOFError("truncated UnityFS block info")

        if info_at_end:
            compressed_info = source.read(compressed_info_size)

        block_info_data = decompress(
            compressed_info,
            flags & 0x3F,
            uncompressed_info_size,
        )
        block_info = io.BytesIO(block_info_data)
        block_info.read(16)  # content hash
        block_count = read_be(block_info, "I")
        blocks = []
        for _ in range(block_count):
            blocks.append(
                (
                    read_be(block_info, "I"),
                    read_be(block_info, "I"),
                    read_be(block_info, "H"),
                )
            )

        node_count = read_be(block_info, "I")
        nodes = []
        for _ in range(node_count):
            nodes.append(
                (
                    read_be(block_info, "Q"),
                    read_be(block_info, "Q"),
                    read_be(block_info, "I"),
                    read_cstring(block_info),
                )
            )

        if info_at_end:
            source.seek(header_end)
            if flags & 0x200:
                align(source, 16)
        else:
            if flags & 0x200:
                align(source, 16)

        data = bytearray()
        for index, (uncompressed_size, compressed_size, block_flags) in enumerate(blocks):
            compressed_block = source.read(compressed_size)
            if len(compressed_block) != compressed_size:
                raise EOFError(f"truncated data block {index}")
            data.extend(decompress(compressed_block, block_flags & 0x3F, uncompressed_size))

    output_dir.mkdir(parents=True, exist_ok=True)
    print(
        f"UnityFS v{version}, player={player_version}, engine={engine_version}, "
        f"blocks={block_count}, nodes={node_count}, data={len(data)}"
    )
    for offset, size, node_flags, path in nodes:
        if offset + size > len(data):
            raise ValueError(f"node {path!r} exceeds decompressed data")
        safe_path = Path(*[part for part in Path(path).parts if part not in ("..", ".")])
        destination = (output_dir / safe_path).resolve()
        if output_dir.resolve() not in destination.parents and destination != output_dir.resolve():
            raise ValueError(f"unsafe node path {path!r}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data[offset : offset + size])
        print(f"{path}\t{size}\tflags=0x{node_flags:X}\t{destination}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    extract(args.bundle, args.output)


if __name__ == "__main__":
    main()
