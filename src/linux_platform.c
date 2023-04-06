#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <alsa/asoundlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include "types.h"
#include "game.h"

typedef struct x11_OffBuff {
    void *memory;
    XImage *image;
    u32 width;
    u32 height;
    i32 bytesPerPixel;
    i32 screenDepth;

} x11_OffscreenBuffer;

typedef struct x11_WinDim {
    u32 width;
    u32 height;
} x11_WindowDimensions;

typedef struct x11_SoundBuff {
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    i32 sampleRate;
    i32 periodCount;
    i32 bufferSize;
    i16 *data;
} x11_SoundBuffer;

global_variable bool running = true;
global_variable x11_OffscreenBuffer globalBackBuffer;
global_variable x11_WindowDimensions globalWindowDimensions;
global_variable x11_SoundBuffer globalSoundBuffer;

internal void
X11InitALSA(x11_SoundBuffer *buffer)
{
    int err;
    if ((err = snd_pcm_open(&buffer->handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        errx(1, "[InitSound]: Failed to open device (%s)\n", snd_strerror(err));

    snd_pcm_hw_params_alloca(&buffer->params);
    if ((err = snd_pcm_hw_params_any(buffer->handle, buffer->params)) < 0)
        errx(1, "[InitSound]: Failed initialize parameters (%s)\n", snd_strerror(err));

    if ((err = snd_pcm_hw_params_set_access(buffer->handle, buffer->params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        errx(1, "[InitSound]: Failed to set access (%s)\n", snd_strerror(err));
    if ((err = snd_pcm_hw_params_set_format(buffer->handle, buffer->params, SND_PCM_FORMAT_S16_LE)) < 0)
        errx(1, "[InitSound]: Failed to set format (%s)\n", snd_strerror(err));
    if ((err = snd_pcm_hw_params_set_channels(buffer->handle, buffer->params, 2)) < 0)
        errx(1, "[InitSound]: Failed to set channels (%s)\n", snd_strerror(err));
    if ((err = snd_pcm_hw_params_set_rate(buffer->handle, buffer->params, buffer->sampleRate, 0)) < 0)
        errx(1, "[InitSound]: Failed to set rate (%s)\n", snd_strerror(err));
    if ((err = snd_pcm_hw_params_set_periods(buffer->handle, buffer->params, buffer->periodCount, 0)) < 0)
        errx(1, "[InitSound]: Failed to set periods (%s)\n", snd_strerror(err));
    if ((err = snd_pcm_hw_params_set_period_time(buffer->handle, buffer->params, 10000, 0)) < 0)
        errx(1, "[InitSound]: Failed to set period time (%s)\n", snd_strerror(err));

    if ((err = snd_pcm_hw_params(buffer->handle, buffer->params)) < 0)
        errx(1, "[InitSound]: Failed to set parameters (%s)\n", snd_strerror(err));
}

internal void
X11PlaySound(x11_SoundBuffer buffer)
{
    int err;
    if ((err = snd_pcm_writei(buffer.handle,
                    buffer.data,
                    buffer.bufferSize)) < 0)
        errx(1, "[MainLoop]: Failed to write to sound buffer (%s)\n",
                snd_strerror(err));
}

internal void
X11ResizeDIBSection(Display *display, x11_OffscreenBuffer *buffer,
        u32 width, u32 height)
{
    if (buffer->image)
        XDestroyImage(buffer->image);

    u32 *dest_buffer = calloc(width * height, buffer->bytesPerPixel);
    buffer->image = XCreateImage(display, CopyFromParent,
            buffer->screenDepth, ZPixmap, 0,
            (i8 *)dest_buffer, width, height, 32, 0);
}

internal void
X11DisplayScreenBuffer(Display *display, x11_OffscreenBuffer buffer,
        Window window, GC gc, x11_WindowDimensions dimensions)
{
    u32 *dest_buffer = (u32 *)buffer.image->data;
    u32 *src_buffer = (u32 *)buffer.memory;

    for (u32 y = 0; y < dimensions.height; y++)
    {
        for (u32 x = 0; x < dimensions.width; x++)
        {
            f32 dx = (f32)x / (f32)dimensions.width;
            f32 dy = (f32)y / (f32)dimensions.height;

            u32 u = (u32)(dx * (f32)buffer.width);
            u32 v = (u32)(dy * (f32)buffer.height);

            dest_buffer[y * dimensions.width + x] = src_buffer[v * buffer.width + u];
        }
    }

    XPutImage(display, window, gc, buffer.image, 0, 0,
            0, 0, dimensions.width, dimensions.height);
}

internal void
X11EventProcess(Display *display, Window window, GC gc, XEvent event)
{
    switch (event.type)
    {
        case DestroyNotify:
        {
            XDestroyImage(globalBackBuffer.image);
            running = false;
        } break;

        case ConfigureNotify:
        {
            XConfigureEvent e = event.xconfigure;
            globalWindowDimensions.width = e.width;
            globalWindowDimensions.height = e.height;
            X11ResizeDIBSection(display, &globalBackBuffer, e.width, e.height);
            X11DisplayScreenBuffer(display, globalBackBuffer, window, gc,
                    globalWindowDimensions);
        } break;

        case KeyPress:
        case KeyRelease:
        {
            char buff;
            KeySym key;
            if (XLookupString(&event.xkey, &buff, 1, &key, NULL))
            {
            }
        } break;

        default:
            break;
    }
}

i32
main()
{
    // Create display
    Display *display = XOpenDisplay((i8 *)0);
    i32 screen = DefaultScreen(display);

    u64 white = WhitePixel(display, screen);
    u64 black = BlackPixel(display, screen);

    // Create Window
    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
           0, 0, 800, 600, 0, black, white);

    XSetStandardProperties(display, window, "Window", "Window",
            None, NULL, 0, NULL);

    XSelectInput(display, window, StructureNotifyMask|KeyPressMask|KeyReleaseMask);
    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", false);

    // Set Dimensions Params
    globalWindowDimensions.width = 800;
    globalWindowDimensions.height = 600;

    // Create GC
    GC gc = XCreateGC(display, window, 0, 0);

    XSetBackground(display, gc, white);
    XSetForeground(display, gc, black);

    // Show Window
    XClearWindow(display, window);
    XMapRaised(display, window);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    // Init backbuffer
    globalBackBuffer.image = NULL;
    globalBackBuffer.width = 800;
    globalBackBuffer.height = 600;
    globalBackBuffer.bytesPerPixel = 4;
    globalBackBuffer.memory = malloc(globalBackBuffer.width *
            globalBackBuffer.height *
            globalBackBuffer.bytesPerPixel);
    globalBackBuffer.screenDepth = DefaultDepth(display, screen);

    X11ResizeDIBSection(display, &globalBackBuffer,
            globalWindowDimensions.width, globalWindowDimensions.height);

    // Init sound Buffer
    globalSoundBuffer.sampleRate = 48000;
    globalSoundBuffer.periodCount = 10;
    globalSoundBuffer.bufferSize =
        globalSoundBuffer.sampleRate / globalSoundBuffer.periodCount;
    globalSoundBuffer.data = malloc(sizeof(i16) * globalSoundBuffer.bufferSize * 2);
    X11InitALSA(&globalSoundBuffer);

    // Create game buffers
    game_OffscreenBuffer gameBackBuffer = {0};
    gameBackBuffer.memory = globalBackBuffer.memory;
    gameBackBuffer.width = globalBackBuffer.width;
    gameBackBuffer.height = globalBackBuffer.height;
    gameBackBuffer.pitch = globalBackBuffer.bytesPerPixel;

    game_SoundBuffer gameSoundBuffer = {0};
    gameSoundBuffer.data = globalSoundBuffer.data;
    gameSoundBuffer.bufferSize = globalSoundBuffer.bufferSize;
    gameSoundBuffer.sampleRate = globalSoundBuffer.sampleRate;
    gameSoundBuffer.volume = 5000;

    while (running)
    {
        XEvent event;
        while (XPending(display))
        {
            XNextEvent(display, &event);
            if ((u32)event.xclient.data.l[0] == wmDeleteMessage)
            {
                running = false;
                break;
            }

            X11EventProcess(display, window, gc, event);
        }

        GameUpdateAndRender(gameBackBuffer, gameSoundBuffer);

        X11DisplayScreenBuffer(display, globalBackBuffer, window, gc,
                globalWindowDimensions);
        X11PlaySound(globalSoundBuffer);
    }

    snd_pcm_close(globalSoundBuffer.handle);

    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
