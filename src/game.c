#include "types.h"
#include "game.h"

#include <math.h>
#define PI 3.14159265358979f

internal void
RenderWeirdGradient(game_OffscreenBuffer buffer, i32 xOffset, i32 yOffset)
{
    u32 width = buffer.width;
    u32 height = buffer.height;

    i32 pitch = width * buffer.pitch;
    u8 *row = (u8 *)(buffer.memory);
    for (u32 y = 0; y < height; y++)
    {
        u8 *pixel = (u8 *)row;
        for (u32 x = 0; x < width; x++)
        {
            *pixel++ = (u8)(x + xOffset);
            *pixel++ = (u8)(y + yOffset);
            *pixel++ = 0;
            *pixel++ = 0;
        }

        row += pitch;
    }
}

internal void
FillSoundBuffer(game_SoundBuffer buffer, i32 toneHz)
{
    int wavePeriod = buffer.sampleRate / toneHz;
    local_persist f32 tSine = 0.f;

    i16 *buff = buffer.data;
    for (int i = 0; i < buffer.bufferSize; i++) {
        tSine += 2.0f * PI * 1.0f / (f32)wavePeriod;
        i16 sample = sinf(tSine) * buffer.volume;
        *buff++ = sample;
        *buff++ = sample;
    }
}

void
GameUpdateAndRender(game_OffscreenBuffer screenBuffer,
        game_SoundBuffer soundBuffer)
{
    local_persist i32 xOffset = 0;
    local_persist i32 yOffset = 0;
    local_persist i32 toneHz = 440;
    RenderWeirdGradient(screenBuffer, xOffset, yOffset);
    FillSoundBuffer(soundBuffer, toneHz);
}
