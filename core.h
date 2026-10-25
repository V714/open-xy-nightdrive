#pragma once
#include <opxy/audio.hpp>
namespace nightdrive {
struct Voice {
    opxy::u32 carrier, modulator, ready, transientAge;
    opxy::i32 controls[8], feedback[2], prev, dc;
};
struct Shared {};
} // namespace nightdrive
