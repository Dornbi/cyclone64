#ifndef ISLANDS_DATA_H
#define ISLANDS_DATA_H

#include <c64/types.h>

#define NUM_ISLANDS 14
#define NUM_DENSE_TILES 130

typedef struct IslandDesc {
    int world_x;
    int world_y;
    byte atlas_x;
    byte atlas_y;
    byte width;
    byte height;
    byte has_helipad;
    const char *name;
} IslandDesc;

static const IslandDesc ISLANDS[NUM_ISLANDS] = {
    { 626, 412,   0,   0, 52, 24, 0, "BANANA ISLAND" },
    { 414, 540,   0,  20, 48, 28, 0, "ORTE ROCKS" },
    { 365, 176,   0,  44, 48, 28, 1, "KOKOLA ISLAND" },
    { 138, 212,   0,  70, 64, 28, 0, "LAGOON ISLAND" },
    { 574, 116,   0,  96, 40, 28, 1, "PEAK ISLAND" },
    { 306, 364,   0, 121, 80, 28, 1, "BASE ISLAND" },
    { 306, 116,  84,   0, 40, 28, 1, "GILLIGANS ISLAND" },
    { 398, 604,  88,  25, 36, 28, 1, "RED ISLAND" },
    { 134, 540,  88,  53, 36, 28, 1, "SKEG ISLAND" },
    { 450, 152,  67,  74, 60, 28, 0, "BONE ISLAND" },
    { 430, 296,  55,   0, 28, 40, 0, "GIANTS GATEWAY" },
    { 474, 476,  53,  40, 32, 32, 0, "CLAW ISLAND" },
    {  70, 588,  40, 102, 36, 32, 0, "LUKELAND ISLES" },
    {  90, 460,  79, 102, 44, 48, 1, "ENTERPRISE ISLAND" },
};

#endif
