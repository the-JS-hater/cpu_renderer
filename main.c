#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <math.h>
#include <X11/Xlib.h>


typedef enum
{	
	KEY_ESC = 9,
} KEY_ENUM;

typedef struct
{
	uint32_t *data;	
	int width, height;
} FrameBuffer;

typedef struct
{ float x, y; } Vec2;

typedef struct
{ float x, y, z; } Vec3;

typedef uint32_t Color;

void clear_background(FrameBuffer *frame_buffer, Color color)
{
	int const size = 
		frame_buffer->width * frame_buffer->height;

	for (unsigned i = 0; i < size; ++i)
		frame_buffer->data[i] = color;
}

void draw_line(FrameBuffer *frame_buffer, Vec2 s, Vec2 e, Color color)
{
	int width, height, xs, xe, ys, ye;
	
	width = frame_buffer->width;
	height = frame_buffer->height;

	xs = (int)roundf(s.x);
	xe = (int)roundf(e.x);
	ys = (int)roundf(s.y);
	ye = (int)roundf(e.y);
	
	int sx = (xe >= xs) ? 1 : -1;
	int sy = (ye >= ys) ? 1 : -1;

	int dx = abs(xe - xs);
	int dy = abs(ye - ys);

	int dk = 2 * dy - dx;
	int xk = xs;
	int yk = ys;
	
	while (true)
	{
		unsigned i = (yk * width) + xk;
		frame_buffer->data[i] = color;
		
		if (xk == xe && yk == ye)
			break;

		xk = xk + sx;
		if (dk > 0)
		{
			dk = dk + 2*dy - 2*dx;
			yk = yk + sy;
		}
		else 
		{
			dk = dk + 2*dy;
		}
	}
}

int main()
{
	Display *display = XOpenDisplay(NULL);
	if (!display) 
	{
		perror("Failed to open display\n");
		exit(1);
	}
	
	int screen = DefaultScreen(display);
	Visual *visual = DefaultVisual(display, screen);
	int depth = DefaultDepth(display, screen);
	
	uint32_t w_width = 800, w_height = 600;
	Window window = XCreateSimpleWindow(
		display,
		XDefaultRootWindow(display),	// parent
		0, 0,													// x, y
		w_width, w_height,						
		0,														// border width
		0x00000000,										// border color
		0x00000000										// background color
	);

	XStoreName(display, window, "Prototype");

	//NOTE: will prolly add mouse events later
	long event_mask = 
		ButtonPressMask | 
		ButtonReleaseMask | 
		KeyPressMask |
		KeyReleaseMask;

	XSelectInput(display, window, event_mask); 
	XMapWindow(display, window);
	
	FrameBuffer *frame_buffer = calloc(1, sizeof(FrameBuffer)); 
	uint32_t *data = calloc(1, w_width * w_height * sizeof(uint32_t));
	frame_buffer->data = data;
	frame_buffer->width = w_width;
	frame_buffer->height = w_height;

	bool quit = false;
	while (!quit) {
		while (XPending(display) > 0) {
			XEvent event = {0};
			XNextEvent(display, &event);
			if (event.type == KeyPress) {
				printf("Key pressed: %d\n", event.xkey.keycode);
				
				if (event.xkey.keycode == KEY_ESC) 
					quit = true;
			} 
			if (event.type == ButtonPress) {
				printf("Mouse pressed\n");
			} 
			if (event.type == ButtonRelease) {
				printf("Mouse Released\n");
			} 
		}

		clear_background(frame_buffer, 0xFFFFFFFF);
		
		draw_line(frame_buffer, (Vec2){7.3f, 14.9f}, (Vec2){765.0f, 476.7f}, 0xFFFF0000);
		draw_line(frame_buffer, (Vec2){457.3f, 254.0f}, (Vec2){30.0f, 345.6f}, 0xFF00FF00);
	
		XImage *img = XCreateImage(
  	  display,
  	  visual,
  	  depth,
  	  ZPixmap,
  	  0,
  	  (char*)frame_buffer->data,
  	  w_width,
  	  w_height,
  	  32,
  	  0
		);

		XPutImage(
		  display,
		  window,
		  DefaultGC(display, screen),
		  img,
		  0, 0,
		  0, 0,
		  w_width,
		  w_height
		);
		
		XFlush(display);
	}
	
	XDestroyWindow(display, window);
	XCloseDisplay(display);
	return 0;
}
