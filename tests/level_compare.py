"""Compare raw synth levels and the SDK preview's declared-peak normalization."""

from pathlib import Path
import argparse
import json
import math
import subprocess
import sys

import numpy as np

RENDER = r'''
#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
namespace sound = @NS@;
int main(int argc, char **argv) {
    if (argc != 12) return 2;
    static sound::Voice voice;
    static sound::Shared shared;
    opxy::i32 parameters[8], output[64];
    opxy::u32 pitch[64];
    for (unsigned i = 0; i < 8; ++i) parameters[i] = opxy::i32(std::atof(argv[i + 1]) * 2147483647.);
    for (auto &p : pitch) p = opxy::u32(std::strtoul(argv[9], nullptr, 10));
    const unsigned blocks = unsigned(std::strtoul(argv[10], nullptr, 10));
    FILE *file = std::fopen(argv[11], "wb");
    if (!file) return 3;
    sound::initialize(voice, shared, 1);
    sound::start(voice, shared, 1);
    for (unsigned b = 0; b < blocks; ++b) {
        sound::render(voice, shared, 1, output, parameters, pitch);
        if (std::fwrite(output, sizeof(output), 1, file) != 1) return 4;
    }
    sound::destroy(voice, shared, 1);
    return std::fclose(file) ? 5 : 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--machine", type=Path, action="append", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/usr/bin/c++")
    args = parser.parse_args()
    sdk = args.sdk_root.resolve()
    sys.path.insert(0, str(sdk))
    from tooling.machines import machine_at

    machines = [machine_at(path.resolve()) for path in args.machine]
    out = args.out.resolve()
    if any(out.is_relative_to(m.root) for m in machines):
        parser.error("Keep outputs outside source folders")
    out.mkdir(parents=True, exist_ok=False)
    report = {"sample_rate": 44100, "hardware_tested": False, "machines": {}}
    for machine in machines:
        if machine.kind != "synth":
            parser.error("Only synths are supported")
        folder = out / machine.id
        folder.mkdir()
        unit, binary = folder / "render.cpp", folder / "render"
        unit.write_text(RENDER.replace("@NS@", machine.id))
        includes = [sdk / "sdk/include", machine.root,
                    *(machine.root / inc for inc in machine.includes)]
        subprocess.run([args.cxx, "-std=c++11", "-O2", *["-I" + str(p) for p in includes],
                        str(unit), *[str(machine.root / p) for p in machine.sources],
                        "-o", str(binary)], check=True)
        parameters = machine.definition["parameters"]
        defaults = [p["default"] for p in parameters]
        patches = {"default": defaults, "zero": [0] * len(defaults),
                   "maximum": [1] * len(defaults)}
        for i, p in enumerate(parameters):
            for value in (0, .25, .5, .75, 1):
                patch = defaults.copy()
                patch[i] = value
                patches[f"{p['id']}-{value}"] = patch
        for name, preset in machine.definition["presets"].items():
            patches["preset-" + name] = [preset[p["id"]] for p in parameters]
        declared = machine.definition["resources"]["outputPeakQ31"] / 2147483648
        rows = []
        max_peak = 0
        for patch_index, (name, values) in enumerate(patches.items()):
            for hz in (55, 110, 220, 440, 880, 1760):
                path = folder / "audio.q31"
                subprocess.run([str(binary), *map(str, values + [0] * (8 - len(values))),
                                str(round(hz * (1 << 32) / 44100)), "690", str(path)], check=True)
                samples = np.fromfile(path, dtype="<i4").astype(float) / 2147483648
                peak = float(np.max(np.abs(samples)))
                rms = float(np.sqrt(np.mean(samples ** 2)))
                early = float(np.sqrt(np.mean(samples[441:4410] ** 2)))
                late = float(np.sqrt(np.mean(samples[-4410:] ** 2)))
                max_peak = max(max_peak, peak)
                rows.append({"patch": name, "hz": hz, "peak": peak, "rms": rms,
                             "early_rms": early, "late_rms": late,
                             "rms_dbfs": 20 * math.log10(max(rms, 1e-15)),
                             "preview_rms_at_monitor_065": rms / declared * .1 * .65})
                if name == "default":
                    path.rename(folder / f"default-{hz}.q31")
        summary = {"source": str(machine.root), "declared_peak": declared,
                   "max_observed_peak": max_peak, "cases": rows}
        report["machines"][machine.id] = summary
        default_rows = [r for r in rows if r["patch"] == "default"]
        print(machine.id, "default RMS dBFS", [round(r["rms_dbfs"], 2) for r in default_rows],
              "max observed peak", round(max_peak, 6), "declared", declared, flush=True)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
