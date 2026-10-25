#pragma once
#include <opxy/draw.hpp>
#include "artwork.h"
namespace nightdrive {
using Frame = opxy::draw::Frame<4, 64, art_nodes, 0, 16>;
struct DisplayState {
    opxy::u32 road, lamp, wheel, ready, track, key, previous[4], clock;
    opxy::u32 noteSerial, motion, pulse;
};
} // namespace nightdrive
