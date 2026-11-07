#pragma once
#include <opxy/audio.hpp>
namespace nightdrive {
struct Voice {
    opxy::u32 carrier, modulator, ready, transientAge;
    opxy::i32 controls[8], feedback[2];
    // Half-scale DC input/output history; transient state is reset on note-on.
    opxy::i32 prev, dc;
};
struct Shared {};
} // namespace nightdrive
