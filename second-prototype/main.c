#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <math.h>

#include "linalg.h"

typedef struct {
  Window       window;
  Display     *display;
  Visual      *visual;
  long         event_mask;
  unsigned int win_w, win_h;
  int          screen, depth;
} AppConfig;

typedef struct {
  // TODO: double buffering
  uint32_t *data;
  float    *depth_buffer;
  uint32_t  width, height;
} FrameBuffer;

typedef uint32_t Color;

typedef struct {
  Vec3  pos;
  Color color;
} Vertex;

typedef enum {
  KEY_ESC = 9,
  KEY_W   = 25,
  KEY_A   = 38,
  KEY_S   = 39,
  KEY_D   = 40,
  KEY_R   = 27,
} KEY_ENUM;

// ============================================================================
// PLUMBING & MISC
// ============================================================================

void parse_args(AppConfig *cfg, int argc, char *argv[])
{
  if (argc < 3)
  {
    perror("Too few args\n");
    exit(1);
  }
  cfg->win_w = strtol(argv[1], NULL, 10);
  cfg->win_h = strtol(argv[2], NULL, 10);
}

void create_window(AppConfig *cfg, char const *title)
{
  cfg->display = XOpenDisplay(NULL);
  if (!cfg->display)
  {
    perror("Failed to open display\n");
    exit(1);
  }
  cfg->screen = DefaultScreen(cfg->display);
  cfg->visual = DefaultVisual(cfg->display, cfg->screen);
  cfg->depth  = DefaultDepth(cfg->display, cfg->screen);
  cfg->window = XCreateSimpleWindow(cfg->display,
                                    XDefaultRootWindow(cfg->display),  // parent
                                    0,                                 // x
                                    0,                                 // y
                                    cfg->win_w,
                                    cfg->win_h,
                                    0,           // border width
                                    0x00000000,  // border color
                                    0x00000000   // background color
  );
  XStoreName(cfg->display, cfg->window, title);

  XSelectInput(cfg->display, cfg->window, cfg->event_mask);
  XMapWindow(cfg->display, cfg->window);
}

void close_window(AppConfig *cfg)
{
  XDestroyWindow(cfg->display, cfg->window);
  XCloseDisplay(cfg->display);
}

FrameBuffer *init_framebuffer(uint32_t width, uint32_t height)
{
  FrameBuffer *frame_buffer = calloc(1, sizeof(FrameBuffer));
  uint32_t    *data         = calloc(1, width * height * sizeof(uint32_t));
  float       *depth_buffer = calloc(1, width * height * sizeof(float));

  frame_buffer->width        = width;
  frame_buffer->height       = height;
  frame_buffer->data         = data;
  frame_buffer->depth_buffer = depth_buffer;

  return frame_buffer;
}

void update_window(AppConfig const *cfg, XImage *x_img)
{
  XPutImage(cfg->display,
            cfg->window,
            DefaultGC(cfg->display, cfg->screen),
            x_img,
            0,  // src_x
            0,  // src_y
            0,  // dest_x
            0,  // dest_y
            cfg->win_w,
            cfg->win_h);
  XFlush(cfg->display);
}

// ============================================================================
// DRAWING
// ============================================================================

void draw_pixel(uint32_t const x,
                uint32_t const y,
                Color const    color,
                FrameBuffer   *fb)
{
  size_t const idx  = y * fb->width + x;
  size_t const size = fb->width * fb->height;
  if (idx >= size) return;
  fb->data[idx] = color;
}

static inline float edge(Vec3 a, Vec3 b, Vec3 p)
{
  return (b.y - a.y) * (p.x - a.x) - (b.x - a.x) * (p.y - a.y);
}

void draw_triangle(Vertex const *verts, FrameBuffer *fb)
{
  // NOTE: Assumes vertices are projected in screen space

  Vec3 v1, v2, v3;
  v1 = verts[0].pos;
  v2 = verts[1].pos;
  v3 = verts[2].pos;
  Color c1, c2, c3;
  c1 = verts[0].color;
  c2 = verts[1].color;
  c3 = verts[2].color;

  // Bounding box
  size_t const xmin = fmin(v1.x, fmin(v2.x, v3.x));
  size_t const ymin = fmin(v1.y, fmin(v2.y, v3.y));
  size_t const xmax = fmax(v1.x, fmax(v2.x, v3.x));
  size_t const ymax = fmax(v1.y, fmax(v2.y, v3.y));

  Vec3 const v12 = vec3_sub(v2, v1);
  Vec3 const v13 = vec3_sub(v3, v1);
  for (size_t x = xmin; x < xmax; ++x)
    for (size_t y = ymin; y < ymax; ++y)
    {
      Vec3  p  = new_vec3((float)x + 0.5f, (float)y + 0.5f, 0.0f);
      float w0 = (v3.y - v2.y) * (p.x - v2.x) - (v3.x - v2.x) * (p.y - v2.y);
      float w1 = (v1.y - v3.y) * (p.x - v3.x) - (v1.x - v3.x) * (p.y - v3.y);
      float w2 = (v2.y - v1.y) * (p.x - v1.x) - (v2.x - v1.x) * (p.y - v1.y);

      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
      {
        draw_pixel(x, y, c1, fb);
      }
    }
}

void clear_background(FrameBuffer *frame_buffer, Color color)
{
  int const size = frame_buffer->width * frame_buffer->height;

  for (unsigned i = 0; i < size; ++i) frame_buffer->data[i] = color;
  for (unsigned i = 0; i < size; ++i) frame_buffer->depth_buffer[i] = INFINITY;
}

// ============================================================================
// INPUT
// ============================================================================

void poll_input(AppConfig *cfg, bool *quit)
{
  while (XPending(cfg->display) > 0)
  {
    XEvent event = {0};
    XNextEvent(cfg->display, &event);
    if (event.type == KeyPress)
    {
      printf("Key pressed: %d\n", event.xkey.keycode);
      switch (event.xkey.keycode)
      {
        case KEY_ESC: *quit = true; break;
      }
    }
    if (event.type == ButtonPress) printf("Mouse pressed\n");

    if (event.type == ButtonRelease) printf("Mouse Released\n");
  }
}

int main(int argc, char *argv[])
{
  AppConfig *cfg = calloc(1, sizeof(*cfg));

  // NOTE: will prolly add mouse events later
  cfg->event_mask =
    ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;

  parse_args(cfg, argc, argv);
  create_window(cfg, "CPU RENDERING PROTOTYPE V2");

  FrameBuffer *frame_buffer = init_framebuffer(cfg->win_w, cfg->win_h);

  XImage *x_img = XCreateImage(cfg->display,
                               cfg->visual,
                               cfg->depth,
                               ZPixmap,
                               0,
                               (char *)frame_buffer->data,
                               cfg->win_w,
                               cfg->win_h,
                               32,
                               0);

  printf("X11 pixel format\n\tR: %08lx \n\tG: %08lx \n\tB: %08lx\n",
         x_img->red_mask,
         x_img->green_mask,
         x_img->blue_mask);

  static bool quit = false;
  while (!quit)
  {
    poll_input(cfg, &quit);

    clear_background(frame_buffer, 0xFF000000);

    Vertex triangle[3] = {
      (Vertex){new_vec3(cfg->win_w * 0.5, cfg->win_h * 0.1, 0.0), 0xFFFF0000},
      (Vertex){new_vec3(cfg->win_w * 0.9, cfg->win_h * 0.9, 0.0), 0xFF00FF00},
      (Vertex){new_vec3(cfg->win_w * 0.1, cfg->win_h * 0.9, 0.0), 0xFF0000FF},
    };

    draw_triangle(&triangle, frame_buffer);

    update_window(cfg, x_img);
  };
  close_window(cfg);
  return 0;
}
