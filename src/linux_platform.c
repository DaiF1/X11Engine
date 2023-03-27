#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <stdio.h>
#include <stdlib.h>

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

global_variable bool running = true;
global_variable x11_OffscreenBuffer globalBackBuffer;

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
X11ResizeDIBSection(Display *display, x11_OffscreenBuffer *buffer,
        u32 width, u32 height)
{
    if (buffer->image)
        XDestroyImage(buffer->image);

    buffer->memory = calloc(width * height, buffer->bytesPerPixel);
    buffer->image = XCreateImage(display, CopyFromParent,
            buffer->screenDepth, ZPixmap, 0,
            (i8 *)(buffer->memory), width, height, 32, 0);

    buffer->width = width;
    buffer->height = height;
}

internal void
X11DisplayScreenBuffer(Display *display, x11_OffscreenBuffer buffer,
        Window window, GC gc,
        i32 x, i32 y, u32 width, u32 height)
{
    XPutImage(display, window, gc, buffer.image, x, y, 0, 0, width, height);
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
            X11ResizeDIBSection(display, &globalBackBuffer, e.width, e.height);
            X11DisplayScreenBuffer(display, globalBackBuffer, window, gc,
                    0, 0, e.width, e.height);
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

    XSelectInput(display, window, StructureNotifyMask);

    // Create GC
    GC gc = XCreateGC(display, window, 0, 0);

    XSetBackground(display, gc, white);
    XSetForeground(display, gc, black);

    // Show Window
    XClearWindow(display, window);
    XMapRaised(display, window);

    // Init backbuffer
    globalBackBuffer.memory = NULL;
    globalBackBuffer.image = NULL;
    globalBackBuffer.width = 0;
    globalBackBuffer.height = 0;
    globalBackBuffer.bytesPerPixel = 4;
    globalBackBuffer.screenDepth = DefaultDepth(display, screen);

    i32 xOffset = 0;
    while (running)
    {
        while (XPending(display))
        {
            XEvent event;
            XNextEvent(display, &event);
            X11EventProcess(display, window, gc, event);
        }

        if (globalBackBuffer.memory)
        {
            RenderWeirdGradient(globalBackBuffer, xOffset, 0);
            X11DisplayScreenBuffer(display, globalBackBuffer, window, gc,
                    0, 0, globalBackBuffer.width, globalBackBuffer.height);
        }

        xOffset++;
    }

    XFreeGC(display, gc);
    XCloseDisplay(display);
    return 0;
}
