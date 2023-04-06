#pragma once
#include "types.h"

typedef struct
{
    void *memory;
    u32 width;
    u32 height;
    u32 pitch;
} game_OffscreenBuffer;

typedef struct
{
    i16 *data;
    i32 bufferSize;
    i32 sampleRate;
    i32 volume;
} game_SoundBuffer;

void GameUpdateAndRender(game_OffscreenBuffer screenBuffer,
        game_SoundBuffer soundBuffer);
