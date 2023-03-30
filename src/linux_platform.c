#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <alsa/asoundlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#define internal static
#define local_persist static
#define global_variable static

#define bool int
#define true 1
#define false 0

typedef char    i8;
typedef short   i16;
typedef int     i32;
typedef long    i64;

typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned int    u32;
typedef unsigned long   u64;

typedef float   f32;
typedef double  f64;

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

global_variable i32 xOffset = 0;
global_variable i32 yOffset = 0;

internal void
RenderWeirdGradient(x11_OffscreenBuffer buffer, i32 xOffset, i32 yOffset)
{
    u32 width = buffer.width;
    u32 height = buffer.height;

    i32 pitch = width * buffer.bytesPerPixel;
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
    if ((err = snd_pcm_hw_params_set_period_time(buffer->handle, buffer->params, 100000, 0)) < 0)
        errx(1, "[InitSound]: Failed to set period time (%s)\n", snd_strerror(err));

    if ((err = snd_pcm_hw_params(buffer->handle, buffer->params)) < 0)
        errx(1, "[InitSound]: Failed to set parameters (%s)\n", snd_strerror(err));
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
                if (buff == 'a')
                    xOffset++;
                if (buff == 'd')
                    xOffset--;
                if (buff == 'w')
                    yOffset++;
                if (buff == 's')
                    yOffset--;
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

    int toneHz = 440;
    int squareWavePeriod = globalSoundBuffer.sampleRate / toneHz;
    int halfSquareWavePeriod = squareWavePeriod / 2;
    int runningSampleIndex = 0;

    while (running)
    {
        while (XPending(display))
        {
            XEvent event;
            XNextEvent(display, &event);
            X11EventProcess(display, window, gc, event);
        }

        RenderWeirdGradient(globalBackBuffer, xOffset, yOffset);

        i16 *buff = globalSoundBuffer.data;
        for (int i = 0; i < globalSoundBuffer.bufferSize; i++) {
            i16 sample = ((runningSampleIndex / halfSquareWavePeriod) % 2) ? 10000 : -10000;
            *buff++ = sample;
            *buff++ = sample;
            runningSampleIndex++;
        }

        int err;
        if ((err = snd_pcm_writei(globalSoundBuffer.handle,
                        globalSoundBuffer.data,
                        globalSoundBuffer.bufferSize)) < 0)
            errx(1, "[MainLoop]: Failed to write to sound buffer (%s)\n",
                    snd_strerror(err));

        X11DisplayScreenBuffer(display, globalBackBuffer, window, gc,
                globalWindowDimensions);
    }
    snd_pcm_drain(globalSoundBuffer.handle);
    snd_pcm_close(globalSoundBuffer.handle);

    XFreeGC(display, gc);
    XCloseDisplay(display);
    return 0;
}
