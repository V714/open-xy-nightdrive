"""Generate experimental shaper data from an explicit exact Nightdrive source."""

from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile

from cpp_format import write_header

HERE = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--mode", choices=("nearest", "dense"), required=True)
    parser.add_argument("--nearest-bits", type=int, default=11)
    parser.add_argument("--drive-bits", type=int, default=5)
    parser.add_argument("--amplitude-bits", type=int, default=7)
    parser.add_argument("--packed", action="store_true")
    args = parser.parse_args()
    if not (2 <= args.nearest_bits <= 14 and 2 <= args.drive_bits <= 9 and 2 <= args.amplitude_bits <= 12):
        parser.error("Unsupported table dimensions")
    reference = args.reference.resolve()
    if args.mode == "nearest":
        body = '''for (unsigned i = 0; i <= @SEGMENTS@; ++i) {
            const unsigned pos = i == @SEGMENTS@ ? 0x1fffffffu : i << @SHIFT@;
            const unsigned j = pos >> 19;
            const auto value = nightdrive::dsp::mix(nightdrive::tanh_lut[j],
                nightdrive::tanh_lut[j+1], opxy::i32((pos & 0x7ffffu) << 12));
            std::printf("%d\\n", value);
        }'''
        segments = 1 << args.nearest_bits
        shift = 29 - args.nearest_bits
        body = body.replace("@SEGMENTS@", str(segments)).replace("@SHIFT@", str(shift))
        rows, columns, name = 1, segments + 1, "tanh_nearest_lut"
    else:
        body = '''for (unsigned row = 0; row <= @ROWS@; ++row) {
            const auto drive = opxy::i32(opxy::i64(2147483647) * row / @ROWS@);
            for (unsigned col = 0; col <= @COLS@; ++col) {
                const auto x = opxy::i32(opxy::i64(2147483647) * col / @COLS@);
                std::printf("%d\\n", nightdrive::dsp::saturate(x, drive));
            }
        }'''
        drive_segments, amplitude_segments = 1 << args.drive_bits, 1 << args.amplitude_bits
        body = body.replace("@ROWS@", str(drive_segments)).replace("@COLS@", str(amplitude_segments))
        rows, columns, name = drive_segments + 1, amplitude_segments + 1, "shaper_lut"
    packed = args.packed and args.mode == "dense"
    element_bytes = 2 if packed else 4
    if rows * columns * element_bytes > 70 * 1024:
        parser.error("Table alone exceeds the 70 KiB engine budget; use a smaller grid")
    source = '#include "dsp.h"\n#include <cstdio>\nint main() { ' + body + '\n}\n'
    with tempfile.TemporaryDirectory(prefix="nightdrive-shaper-") as temporary:
        folder = Path(temporary)
        unit, binary = folder / "generate.cpp", folder / "generate"
        unit.write_text(source)
        subprocess.run(["/usr/bin/c++", "-std=c++11", "-O2", "-I" + str(reference),
                        "-I" + str(args.sdk_root.resolve() / "sdk/include"),
                        str(unit), "-o", str(binary)], check=True)
        values = list(map(int, subprocess.check_output([str(binary)], text=True).splitlines()))
    if len(values) != rows * columns:
        raise RuntimeError("Wrong number of table values")
    if packed:
        values = [min(32767, max(0, (value + 32768) // 65536)) for value in values]
    header = '#pragma once\n#include <opxy/types.hpp>\nnamespace nightdrive {\n'
    if rows == 1:
        header += f'static const unsigned approx_nearest_shift = {shift};\n'
        header += f'static const opxy::i32 {name}[{columns}] = {{' + ','.join(map(str, values)) + '};\n'
    else:
        header += f'static const unsigned shaper_drive_bits = {args.drive_bits};\n'
        header += f'static const unsigned shaper_amplitude_bits = {args.amplitude_bits};\n'
        header += f'static const opxy::i32 shaper_scale = {65536 if packed else 1};\n'
        ctype = 'opxy::u16' if packed else 'opxy::i32'
        header += f'static const {ctype} {name}[{rows}][{columns}] = {{\n' 
        for row in range(rows):
            header += '{' + ','.join(map(str, values[row * columns:(row + 1) * columns])) + '},\n'
        header += '};\n'
    header += '}\n'
    path = HERE / 'approx_lut.h'
    write_header(path, header)
    metadata = {"mode": args.mode, "rows": rows, "columns": columns,
                "table_bytes": len(values) * element_bytes, "experimental": True,
                "packed_q15": packed, "full_engine_limit_bytes": 70 * 1024,
                "reference_sources": {name: hashlib.sha256((reference / name).read_bytes()).hexdigest()
                                      for name in ("dsp.h", "tables.h")},
                "header_sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    (HERE / 'generated/approximation_manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
