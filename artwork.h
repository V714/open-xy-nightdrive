#pragma once
namespace nightdrive {
struct ArtPoint {
    int x, y;
};
static const unsigned art_nodes = 63;
static const unsigned lamp_node = 22;
static const unsigned wheel_sprite_nodes[2][7] = {{45, 46, 47, 48, 49, 50, 51},
                                                  {55, 56, 57, 58, 59, 60, 61}};
static const unsigned car_nodes[] = {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                                     37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
                                     51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62};
static const unsigned window_nodes[] = {29, 31, 34, 36};
static const unsigned dash_nodes[] = {9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
static const ArtPoint lamp_shape[] = {
    {0, 40960},      {0, -29696},     {48, -30672},    {192, -31552},   {432, -32336},
    {768, -33024},   {1200, -33616},  {1728, -34112},  {2352, -34512},  {3072, -34816},
    {3888, -35024},  {4800, -35136},  {5808, -35152},  {6912, -35072},  {8112, -34896},
    {9408, -34624},  {10800, -34256}, {12288, -33792}, {13872, -33232}, {15552, -32576},
    {17328, -31824}, {19200, -30976}, {21168, -30032}, {23232, -28992}, {25392, -27856},
    {27648, -26624}, {27648, -26624}};
static const int lane_shift_q10 = 29655;
} // namespace nightdrive
