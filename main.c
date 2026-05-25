#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <X11/Xlib.h>


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
	
	bool quit = false;
	int keycode = 0;
	uint32_t *frame_buffer = calloc(1, w_width * w_height * sizeof(uint32_t));

	for (unsigned i = 0; i < w_width * w_height; ++i)
		frame_buffer[i] = 0xFF0000;
		
	
	while (!quit) {
		while (XPending(display) > 0) {
			XEvent event = {0};
			XNextEvent(display, &event);
			if (event.type == KeyPress) {
				printf("Key pressed: %d\n", event.xkey.keycode);
				keycode = event.xkey.keycode;
			} 
			if (event.type == ButtonPress) {
				printf("Mouse pressed\n");
				quit = true;
			} 
			if (event.type == ButtonRelease) {
				printf("Mouse Released\n");
			} 
		}

		for (unsigned i = 0; i < w_width * w_height; ++i)
		{
			unsigned x, y;
			x = i % w_width + keycode;
			y = i / w_width + keycode;
			uint32_t v = (x * 1973u + y * 9277u + keycode * 26699u) ^ (x * y);
			uint8_t r = (v >>  0) & 255;
			uint8_t g = (v >>  8) & 255;
			uint8_t b = (v >> 16) & 255;
			
			frame_buffer[i] = (r << 16) | (g << 8) | b;
		}

		XImage *img = XCreateImage(
  	  display,
  	  visual,
  	  depth,
  	  ZPixmap,
  	  0,
  	  (char*)frame_buffer,
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
