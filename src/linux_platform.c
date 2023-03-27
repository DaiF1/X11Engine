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

global_variable bool running = true;
global_variable void *bitmapBuffer = NULL;
global_variable XImage *bitmapImage = NULL;

global_variable u32 bitmapWidth;
global_variable u32 bitmapHeight;
global_variable i32 bytesPerPixel = 4;

internal void RenderWeirdGradient(i32 xOffset, i32 yOffset)
{
    u32 width = bitmapWidth;
    u32 height = bitmapHeight;

    i32 pitch = width * bytesPerPixel;
    u8 *row = (u8 *)bitmapBuffer;
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

internal void ResizeDIBSection(Display *display, i32 screen,
        u32 width, u32 height)
{
    if (bitmapImage)
        XDestroyImage(bitmapImage);

    bitmapBuffer = calloc(width * height, bytesPerPixel);
    bitmapImage = XCreateImage(display, CopyFromParent,
            DefaultDepth(display, screen), ZPixmap, 0,
            (i8 *)bitmapBuffer, width, height, 32, 0);

    bitmapWidth = width;
    bitmapHeight = height;
}

internal void UpdateWindow(Display *display, Window window, GC gc,
        i32 x, i32 y, u32 width, u32 height)
{
    XPutImage(display, window, gc, bitmapImage, x, y, 0, 0, width, height);
}

void EventProcess(Display *display, i32 screen, Window window, GC gc, XEvent event)
{
    switch (event.type)
    {
        case DestroyNotify:
        {
            printf("Destroy Notify\n");
            XDestroyImage(bitmapImage);
            running = false;
        } break;

        case ConfigureNotify:
        {
            int x, y;
            unsigned int width, height, border, depth;
            Window root;
            XGetGeometry(display, window, &root, &x, &y, &width, &height, &border, &depth);


            XConfigureEvent e = event.xconfigure;
            ResizeDIBSection(display, screen, e.width, e.height);
            UpdateWindow(display, window, gc, 0, 0, e.width, e.height);
        } break;

        default:
            break;
    }
}

i32 main()
{
    Display *display = XOpenDisplay((i8 *)0);
    i32 screen = DefaultScreen(display);

    u64 white = WhitePixel(display, screen);
    u64 black = BlackPixel(display, screen);

    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
           0, 0, 800, 600, 0, black, white);

    XSetStandardProperties(display, window, "Window", "Window",
            None, NULL, 0, NULL);

    GC gc = XCreateGC(display, window, 0, 0);

    XSetBackground(display, gc, white);
    XSetForeground(display, gc, black);

    XClearWindow(display, window);
    XMapRaised(display, window);

    XSelectInput(display, window, StructureNotifyMask);

    i32 xOffset = 0;
    while (running)
    {
        while (XPending(display))
        {
            XEvent event;
            XNextEvent(display, &event);
            EventProcess(display, screen, window, gc, event);
        }

        if (bitmapBuffer)
        {
            RenderWeirdGradient(xOffset, 0);
            UpdateWindow(display, window, gc, 0, 0, bitmapWidth, bitmapHeight);
        }

        xOffset++;
    }

    XFreeGC(display, gc);
    XCloseDisplay(display);
    return 0;
}
