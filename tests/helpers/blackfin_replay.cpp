// Computer-only ISA replay; no stock adapter, cache model or firmware.
#include "engine.cpp"
using namespace nightdrive;
extern "C" void lab_counters(u32 *);
extern "C" void lab_write(const void *, unsigned);
[[gnu::noinline]] static void replay(Voice &voice, Shared &shared, i32 *out, const i32 *parameters,
                                     const u32 *pitch) {
    render(voice, shared, 1, out, parameters, pitch);
}
struct Record {
    u32 scenario, note, block, ageBefore, mask, varyingPitch;
    u32 instructions, reads, writes;
    Voice voice;
    i32 output[64];
};
extern "C" int lab_main() {
    const unsigned notes[] = {24, 60, 96, 108};
    const u32 increments[] = {3185015u, 25480119u, 203840952u, 407681904u};
    const i32 defaults[] = {536870912, 536870912, 751619276, 536870912};
    for (unsigned scenario = 0; scenario < 18; ++scenario)
        for (unsigned n = 0; n < 4; ++n) {
            Voice v{};
            Shared shared{};
            initialize(v, shared, 1);
            Record record{};
            u32 pitch[64];
            i32 p[8] = {};
            for (unsigned block = 0; block < 160; ++block) {
                if (scenario == 17 && block % 16 == 0)
                    start(v, shared, 1);
                for (unsigned k = 0; k < 4; ++k)
                    p[k] = scenario == 0 ? 1073741823 : defaults[k];
                if (scenario == 2)
                    p[0] = 0;
                if (scenario == 3)
                    p[3] = 0;
                if (scenario == 4)
                    p[3] = 2147483647;
                if (scenario == 5)
                    for (unsigned k = 0; k < 4; ++k)
                        p[k] = 2147483647;
                if (scenario == 6) {
                    p[0] = 1879048191;
                    p[1] = 2147483647;
                    p[2] = 1879048191;
                    p[3] = 1073741823;
                }
                if (scenario >= 7 && scenario <= 9 && (block & 8))
                    for (unsigned k = 0; k < 4; ++k)
                        if (scenario == 7 || (scenario == 8 && k == 0) || (scenario == 9 && k == 3))
                            p[k] = 2147483647 - p[k];
                if (scenario >= 14 && (block & 8))
                    for (unsigned k = 0; k < 4; ++k)
                        if ((scenario == 14 && k == 1) || (scenario == 15 && k == 2) ||
                            scenario >= 16)
                            p[k] = 2147483647 - p[k];
                if (scenario == 12)
                    for (unsigned k = 0; k < 4; ++k)
                        p[k] = i32((u32(block % 128) * 16777216u + (k & 1) * 1073741823u) &
                                   0x7fffffffu);
                if (scenario == 13)
                    p[3] = 861644443;
                record.mask = 0;
                for (unsigned k = 0; k < 4; ++k)
                    if (v.controls[k] != p[k])
                        record.mask |= 1u << k;
                record.varyingPitch = 0;
                for (unsigned i = 0; i < 64; ++i) {
                    pitch[i] = increments[n];
                    if (scenario == 10 && block == 0)
                        pitch[i] = (increments[n] >> 6) * (i + 1);
                    if (scenario == 11 || scenario == 16)
                        pitch[i] += i * 320u;
                    if (i && pitch[i] != pitch[0])
                        record.varyingPitch = 1;
                }
                record.scenario = scenario;
                record.note = notes[n];
                record.block = block;
                record.ageBefore = v.transientAge;
                u32 before[3], after[3];
                lab_counters(before);
                replay(v, shared, record.output, p, pitch);
                lab_counters(after);
                record.instructions = after[0] - before[0];
                record.reads = after[1] - before[1];
                record.writes = after[2] - before[2];
                record.voice = v;
                lab_write(&record, sizeof(record));
            }
        }
    return 0;
}
