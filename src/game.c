#include "types.h"
#include "game.h"

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

void
GameUpdateAndRender(game_OffscreenBuffer buffer)
{
    local_persist i32 xOffset = 0;
    local_persist i32 yOffset = 0;
    RenderWeirdGradient(buffer, xOffset, yOffset);
}
