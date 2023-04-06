#pragma once
#include "types.h"

typedef struct
{
    void *memory;
    u32 width;
    u32 height;
    u32 pitch;
} game_OffscreenBuffer;

void GameUpdateAndRender(game_OffscreenBuffer buffer);
