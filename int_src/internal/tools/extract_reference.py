#!/usr/bin/env python3
"""Extract verified UI resources from the supplied packed Android ELF.

Reads the file as data; it never executes the reference program. Supports its
UPX LZMA blocks, stb-compressed TrueType fonts and embedded PNG images.
"""
import argparse
import hashlib
import json
import lzma
from pathlib import Path
import struct
import zlib


def lzma_block(data, size):
    if len(data) < 3:
        raise ValueError("Truncated LZMA block")
    pb, lp, lc = data[0] & 7, data[1] >> 4, data[1] & 15
    if data[0] >> 3 != lp + lc:
        raise ValueError("Invalid UPX LZMA properties")
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=[{
        "id": lzma.FILTER_LZMA1, "dict_size": max(4096, size),
        "lc": lc, "lp": lp, "pb": pb,
    }])
    result = decoder.decompress(data[2:], max_length=size)
    if len(result) != size:
        raise ValueError("LZMA output size differs from its block header")
    return result


def unpack(data):
    if data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("Expected a little-endian 64-bit ELF")
    start = None
    for offset in range(0x100, min(4096, len(data) - 12), 4):
        size, packed, method, fid, cto, extra = struct.unpack_from("<IIBBBB", data, offset)
        if method != 14 or fid or not (64 <= size <= 4096 and 3 <= packed <= size):
            continue
        try:
            header = lzma_block(data[offset + 12:offset + 12 + packed], size)
        except (ValueError, lzma.LZMAError):
            continue
        if header.startswith(b"\x7fELF"):
            start = offset
            break
    if start is None:
        raise ValueError("Cannot locate the packed ELF headers")
    blocks = []
    offset = start
    while offset + 12 <= len(data):
        size, packed, method, fid, cto, extra = struct.unpack_from("<IIBBBB", data, offset)
        if not size:
            break
        if not (0 < packed <= size <= 64 * 1024 * 1024) or offset + 12 + packed > len(data):
            raise ValueError("Invalid packed block bounds")
        payload = data[offset + 12:offset + 12 + packed]
        if packed == size:
            raw = payload
        elif method == 14:
            raw = lzma_block(payload, size)
        else:
            raise ValueError(f"Unsupported compression method {method}")
        if fid == 0x52:
            decoded = bytearray(raw)
            for instruction in range(0, len(decoded) - 4, 4):
                value = struct.unpack_from("<I", decoded, instruction)[0]
                if value >> 24 & 0x7c == 0x14:
                    value = value & 0xfc000000 | (value - instruction // 4) & 0x3ffffff
                    struct.pack_into("<I", decoded, instruction, value)
            raw = bytes(decoded)
        elif fid:
            raise ValueError(f"Unsupported UPX filter {fid:#x}")
        blocks.append((offset, fid, raw))
        offset += 12 + packed
    if len(blocks) != 4:
        raise ValueError(f"Expected four reference blocks, found {len(blocks)}")
    header = blocks[0][2]
    phoff = struct.unpack_from("<Q", header, 32)[0]
    phsize, phcount = struct.unpack_from("<HH", header, 54)
    loads = []
    for i in range(phcount):
        kind, flags, foff, vaddr, paddr, filesz, memsz, align = struct.unpack_from(
            "<IIQQQQQQ", header, phoff + i * phsize)
        if kind == 1:
            loads.append((foff, filesz))
    if len(loads) != 3 or len(header) + len(blocks[1][2]) != loads[0][1]:
        raise ValueError("Packed blocks do not match the ELF load segments")
    image = bytearray(max(offset + size for offset, size in loads))
    for number, target in ((0, 0), (1, len(header)), (2, loads[1][0]), (3, loads[2][0])):
        image[target:target + len(blocks[number][2])] = blocks[number][2]
    return bytes(image)


def stb_font(data):
    if data[:8] != b"\x57\xbc\0\0\0\0\0\0":
        raise ValueError("Invalid stb font signature")
    size = int.from_bytes(data[8:12], "big")
    if not 0 < size <= 16 * 1024 * 1024:
        raise ValueError("Invalid font size")
    out = bytearray()
    pos = 16

    def integer(offset, length):
        return int.from_bytes(data[offset:offset + length], "big")

    while len(out) <= size:
        value = data[pos]
        distance = length = literal = 0
        if value >= 0x80:
            distance, length = data[pos + 1] + 1, value - 0x80 + 1
            pos += 2
        elif value >= 0x40:
            distance, length = integer(pos, 2) - 0x4000 + 1, data[pos + 2] + 1
            pos += 3
        elif value >= 0x20:
            literal = value - 0x20 + 1
            pos += 1
        elif value >= 0x18:
            distance, length = integer(pos, 3) - 0x180000 + 1, data[pos + 3] + 1
            pos += 4
        elif value >= 0x10:
            distance, length = integer(pos, 3) - 0x100000 + 1, integer(pos + 3, 2) + 1
            pos += 5
        elif value >= 8:
            literal = integer(pos, 2) - 0x800 + 1
            pos += 2
        elif value == 7:
            literal = integer(pos + 1, 2) + 1
            pos += 3
        elif value == 6:
            distance, length = integer(pos + 1, 3) + 1, data[pos + 4] + 1
            pos += 5
        elif value == 4:
            distance, length = integer(pos + 1, 3) + 1, integer(pos + 4, 2) + 1
            pos += 6
        elif data[pos:pos + 2] == b"\x05\xfa":
            if len(out) != size or zlib.adler32(out) != integer(pos + 2, 4):
                raise ValueError("Font size/checksum mismatch")
            return bytes(out)
        else:
            raise ValueError("Invalid stb token")
        if literal:
            out += data[pos:pos + literal]
            pos += literal
        else:
            if not 0 < distance <= len(out):
                raise ValueError("Invalid stb backreference")
            for _ in range(length):
                out.append(out[-distance])
    raise ValueError("Font output exceeds declared size")


def png(data):
    pos = 8
    while pos + 12 <= len(data):
        size = int.from_bytes(data[pos:pos + 4], "big")
        end = pos + 12 + size
        if end > len(data):
            raise ValueError("Truncated PNG")
        if zlib.crc32(data[pos + 4:pos + 8 + size]) != int.from_bytes(data[pos + 8 + size:end], "big"):
            raise ValueError("PNG chunk CRC mismatch")
        kind = data[pos + 4:pos + 8]
        pos = end
        if kind == b"IEND":
            return data[:pos]
    raise ValueError("Missing PNG IEND")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    original = args.reference.read_bytes()
    image = unpack(original)
    args.output.mkdir(parents=True, exist_ok=True)
    resources = []
    for signature, kind, extension, decode in (
        (b"\x57\xbc\0\0\0\0\0\0", "font", "ttf", stb_font),
        (b"\x89PNG\r\n\x1a\n", "image", "png", png),
    ):
        offset = image.find(signature)
        while offset >= 0:
            data = decode(image[offset:])
            name = f"{kind}-{offset:x}.{extension}"
            (args.output / name).write_bytes(data)
            item = {"file": name, "elf_offset": hex(offset), "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest()}
            if kind == "image":
                item["width"], item["height"] = struct.unpack_from(">II", data, 16)
            resources.append(item)
            offset = image.find(signature, offset + 1)
    manifest = {"reference": args.reference.name,
                "reference_sha256": hashlib.sha256(original).hexdigest(),
                "resources": resources}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    # The two tables contain 126 English/Russian labels. These addresses refer
    # to the supplied reference build, not to offsets inside the game.
    relocations = {}
    for offset in range(0x2910, 0x11748, 24):
        address, info, addend = struct.unpack_from("<QQq", image, offset)
        if info & 0xffffffff == 1027:
            relocations[address] = addend
    labels = []
    for index in range(126):
        pair = []
        for base in (0x167210, 0x167600):
            address = relocations[base + index * 8]
            end = image.index(0, address)
            pair.append(image[address:end].decode("utf-8"))
        labels.append(pair)
    (args.output / "labels.json").write_text(json.dumps(labels, ensure_ascii=False, indent=2) + "\n")
    palette=[struct.unpack_from('<4f',image,0x1aa20+index*16) for index in range(28)]
    (args.output / "palette.json").write_text(json.dumps(palette,indent=2)+"\n")
    print(f"Extracted {len(resources)} resources with verified checksums")


if __name__ == "__main__":
    main()
