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

typedef struct 
{ Vec2 s, e; } Line;

typedef struct
{ Vec2 v1, v2, v3; } Triangle2D;

typedef uint32_t Color;

typedef struct
{
	Vec3 pos;
	Color color;
} Vertex;

void clear_background(FrameBuffer *frame_buffer, Color color)
{
	int const size = 
		frame_buffer->width * frame_buffer->height;

	for (unsigned i = 0; i < size; ++i)
		frame_buffer->data[i] = color;
}

void draw_line(FrameBuffer *frame_buffer, Line l, Color color)
{
	int width, xs, xe, ys, ye;
	Vec2 s = l.s; 
	Vec2 e = l.e;
	
	width = frame_buffer->width;

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

float cross_product2D(Vec2 v, Vec2 u)
{
	return v.x*u.y - v.y*u.x; 
}

void draw_triangle2D(FrameBuffer *frame_buffer, Triangle2D triangle, Color color)
{
	int width = frame_buffer->width;

	Vec2 v1, v2, v3;
	v1 = triangle.v1;
	v2 = triangle.v2;
	v3 = triangle.v3;
	
	int max_x = roundf(fmax(v1.x, fmax(v2.x, v3.x)));
	int min_x = roundf(fmin(v1.x, fmin(v2.x, v3.x)));
	int max_y = roundf(fmax(v1.y, fmax(v2.y, v3.y)));
	int min_y = roundf(fmin(v1.y, fmin(v2.y, v3.y)));
	
	Vec2 vs1 = {v2.x - v1.x, v2.y - v1.y};
	Vec2 vs2 = {v3.x - v1.x, v3.y - v1.y};

	for (int x = min_x; x <= max_x; x++)
	{
	  for (int y = min_y; y <= max_y; y++)
	  {
			Vec2 q = {x - v1.x, y - v1.y};
	
	    float s = cross_product2D(q, vs2) / cross_product2D(vs1, vs2);
	    float t = cross_product2D(vs1, q) / cross_product2D(vs1, vs2);
	
	    if ( (s >= 0) && (t >= 0) && (s + t <= 1))
	    {
				unsigned i = (y * width) + x;
	      frame_buffer->data[i] = color;
	    }
	  }
	}
}

float cross_product(Vec3 v, Vec3 u)
{
	return 0.0f;
}

void draw_triangle(FrameBuffer *frame_buffer, Vertex *triangle)
{
	int width = frame_buffer->width;

	Vec3 v1, v2, v3;
	v1 = triangle[0].pos;
	v2 = triangle[1].pos;
	v3 = triangle[2].pos;
	Color c1, c2, c3;
	c1 = triangle[0].color;
	c2 = triangle[1].color;
	c3 = triangle[2].color;
	
	int max_x = roundf(fmax(v1.x, fmax(v2.x, v3.x)));
	int min_x = roundf(fmin(v1.x, fmin(v2.x, v3.x)));
	int max_y = roundf(fmax(v1.y, fmax(v2.y, v3.y)));
	int min_y = roundf(fmin(v1.y, fmin(v2.y, v3.y)));
	
	Vec2 vs1 = {v2.x - v1.x, v2.y - v1.y};
	Vec2 vs2 = {v3.x - v1.x, v3.y - v1.y};

	for (int x = min_x; x <= max_x; x++)
	{
	  for (int y = min_y; y <= max_y; y++)
	  {
			Vec2 q = {x - v1.x, y - v1.y};
	
	    float s = cross_product2D(q, vs2) / cross_product2D(vs1, vs2);
	    float t = cross_product2D(vs1, q) / cross_product2D(vs1, vs2);
	
	    if ( (s >= 0) && (t >= 0) && (s + t <= 1))
	    {
				unsigned i = (y * width) + x;
				uint8_t r1 = (c1 >> 24) & 0xFF;
				uint8_t g1 = (c1 >> 16) & 0xFF;
				uint8_t b1 = (c1 >> 8)  & 0xFF;
				uint8_t a1 = (c1)       & 0xFF;
				uint8_t r2 = (c2 >> 24) & 0xFF;
				uint8_t g2 = (c2 >> 16) & 0xFF;
				uint8_t b2 = (c2 >> 8)  & 0xFF;
				uint8_t a2 = (c2)       & 0xFF;
				uint8_t r3 = (c3 >> 24) & 0xFF;
				uint8_t g3 = (c3 >> 16) & 0xFF;
				uint8_t b3 = (c3 >> 8)  & 0xFF;
				uint8_t a3 = (c3)       & 0xFF;
				uint8_t r = (1-s-t)*r1 + s*r2 + t*r3;
				uint8_t g = (1-s-t)*g1 + s*g2 + t*g3;
				uint8_t b = (1-s-t)*b1 + s*b2 + t*b3;
				uint8_t a = (1-s-t)*a1 + s*a2 + t*a3;
				Color color =
    			(r << 24) |
    			(g << 16) |
    			(b << 8)  |
    			(a);
	      frame_buffer->data[i] = color;
	    }
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
		float cx = (float)w_width / 2.0f; 
		float cy = (float)w_height / 2.0f;
		float offset = (float)w_height / 4.0f;
		// Triangle2D t = {
		// 	{cx, cy - offset},
		// 	{cx + offset, cy + offset},
		// 	{cx - offset, cy + offset},
		// };
		// draw_triangle2D(frame_buffer, t, 0x00FF0000);
		
		Vertex triangle[3] = {
			{
		 		{cx, cy - offset, 1.0f},
				0xFFFF0000
			},
			{
		 		{cx + offset, cy + offset, 1.0f},
				0xFF00FF00
			},
			{
		 		{cx - offset, cy + offset, 1.0f},
				0xFF0000FF
			},
		};
		draw_triangle(frame_buffer, triangle);

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
