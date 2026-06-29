#!/usr/bin/env python3

import os
import struct
import sys


def read_u32(f):
    data = f.read(4)
    if len(data) != 4:
        raise EOFError("Unexpected end of file")
    return struct.unpack("<I", data)[0]


def extract_pak(pak_path, output_dir):
    with open(pak_path, "rb") as f:
        # Read entry count
        entry_count = read_u32(f)
        print(f"Entries: {entry_count}")

        entries = []

        # Read directory
        for i in range(entry_count):
            name_len = read_u32(f)

            if name_len < 1:
                raise ValueError(f"Invalid name length at entry {i}")

            # Format specifies N-1 bytes of filename
            name = f.read(name_len - 1).decode("utf-8", errors="replace")

            offset = read_u32(f)
            size = read_u32(f)

            entries.append({
                "name": name,
                "offset": offset,
                "size": size,
            })

        # Offsets are relative to the start of the data section
        data_base = f.tell()

        print(f"Data section starts at 0x{data_base:X}")

        # Extract files
        for entry in entries:
            file_offset = data_base + entry["offset"]

            f.seek(file_offset)
            data = f.read(entry["size"])

            # Undo bitwise NOT
            data = bytes((~b) & 0xFF for b in data)

            out_path = os.path.join(output_dir, entry["name"])

            parent = os.path.dirname(out_path)
            if parent:
                os.makedirs(parent, exist_ok=True)

            with open(out_path, "wb") as out:
                out.write(data)

            print(
                f"Extracted: {entry['name']} "
                f"(offset=0x{entry['offset']:X}, size={entry['size']})"
            )


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <archive.pak> <output_dir>")
        sys.exit(1)

    pak_path = sys.argv[1]
    output_dir = sys.argv[2]

    os.makedirs(output_dir, exist_ok=True)

    try:
        extract_pak(pak_path, output_dir)
        print("Done.")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()