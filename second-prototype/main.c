#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
  Vec4  pos;
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

void draw_triangle(Vertex const *verts, FrameBuffer *fb, bool backface_culling)
{
  // NOTE: Assumes vertices are projected in screen space

  Vec4 v1, v2, v3;
  v1 = verts[0].pos;
  v2 = verts[1].pos;
  v3 = verts[2].pos;
  Color c1, c2, c3;
  c1 = verts[0].color;
  c2 = verts[1].color;
  c3 = verts[2].color;

  int32_t xmin = fmin(v1.x, fmin(v2.x, v3.x));
  int32_t ymin = fmin(v1.y, fmin(v2.y, v3.y));
  int32_t xmax = fmax(v1.x, fmax(v2.x, v3.x));
  int32_t ymax = fmax(v1.y, fmax(v2.y, v3.y));

  xmin = xmin < 0 ? 0 : xmin;
  ymin = ymin < 0 ? 0 : ymin;
  xmax = xmax >= fb->width ? fb->width - 1 : xmax;
  ymax = ymax >= fb->height ? fb->height - 1 : ymax;

  Vec4 const v12  = vec4_sub(v2, v1);
  Vec4 const v13  = vec4_sub(v3, v1);
  float      area = v12.x * v13.y - v12.y * v13.x;
  if (backface_culling && area >= 0) return;

  for (size_t x = xmin; x < xmax; ++x)
    for (size_t y = ymin; y < ymax; ++y)
    {
      Vec3  p  = new_vec3((float)x + 0.5f, (float)y + 0.5f, 0.0f);
      float w0 = (v3.y - v2.y) * (p.x - v2.x) - (v3.x - v2.x) * (p.y - v2.y);
      float w1 = (v1.y - v3.y) * (p.x - v3.x) - (v1.x - v3.x) * (p.y - v3.y);
      float w2 = (v2.y - v1.y) * (p.x - v1.x) - (v2.x - v1.x) * (p.y - v1.y);

      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
      {
        float b0 = w0 / area;
        float b1 = w1 / area;
        float b2 = w2 / area;

        float z = b0 * v1.z + b1 * v2.z + b2 * v3.z;

        size_t idx = y * fb->width + x;

        if (z >= fb->depth_buffer[idx]) continue;

        fb->depth_buffer[idx] = z;
        draw_pixel(x, y, c1, fb);
      }
    }
}

Mat4 look_at(Vec3 pos, Vec3 target, Vec3 up)
{
  Vec3 cam_dir   = vec3_norm(vec3_sub(pos, target));
  Vec3 cam_right = vec3_norm(cross(up, cam_dir));
  Vec3 cam_up    = cross(cam_dir, cam_right);
  Mat4 a         = (Mat4){
    cam_right.x,
    cam_up.x,
    cam_dir.x,
    0.0f,
    cam_right.y,
    cam_up.y,
    cam_dir.y,
    0.0f,
    cam_right.z,
    cam_up.z,
    cam_dir.z,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
  };
  Mat4 b = (Mat4){1.0f,
                  0.0f,
                  0.0f,
                  0.0f,
                  0.0f,
                  1.0f,
                  0.0f,
                  0.0f,
                  0.0f,
                  0.0f,
                  1.0f,
                  0.0f,
                  -pos.x,
                  -pos.y,
                  -pos.z,
                  1.0f};
  return mat4_mult(a, b);
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

void poll_input(AppConfig *cfg, bool *quit, Vec3 *camera_pos)
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
        case KEY_W: camera_pos->z -= 0.03; break;
        case KEY_A: camera_pos->x -= 0.03; break;
        case KEY_S: camera_pos->z += 0.03; break;
        case KEY_D: camera_pos->x += 0.03; break;
      }
    }
    if (event.type == ButtonPress) printf("Mouse pressed\n");

    if (event.type == ButtonRelease) printf("Mouse Released\n");
  }
}

// ============================================================================
// MAIN
// ============================================================================

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

  struct timespec tw1;  // both C11 and POSIX
  clock_t         t1 = clock();

  static bool quit         = false;
  Vec3        camera_pos   = (Vec3){0.0f, 0.0f, 10.0f};
  Vec3        camera_front = (Vec3){0.0f, 0.0f, -1.0f};
  Vec3        camera_up    = (Vec3){0.0f, 1.0f, 0.0f};
  while (!quit)
  {

    poll_input(cfg, &quit, &camera_pos);

    clear_background(frame_buffer, 0xFF000000);

    Vertex triangle[3] = {
      {new_vec4(-1.0f, -1.0f, 0.0f, 1.0f), 0xFFFF0000},
      {new_vec4(1.0f,  -1.0f, 0.0f, 1.0f), 0xFF00FF00},
      {new_vec4(0.0f,  1.0f,  0.0f, 1.0f), 0xFF0000FF},
    };

    Vertex pyramid[18] = {
      // front
      {new_vec4(0.0f,  1.0f,  0.0f,  1.0f), 0xFFFF0000},
      {new_vec4(-1.0f, -1.0f, 1.0f,  1.0f), 0xFFFF0000},
      {new_vec4(1.0f,  -1.0f, 1.0f,  1.0f), 0xFFFF0000},

      // right
      {new_vec4(0.0f,  1.0f,  0.0f,  1.0f), 0xFF00FF00},
      {new_vec4(1.0f,  -1.0f, 1.0f,  1.0f), 0xFF00FF00},
      {new_vec4(1.0f,  -1.0f, -1.0f, 1.0f), 0xFF00FF00},

      // back
      {new_vec4(0.0f,  1.0f,  0.0f,  1.0f), 0xFF0000FF},
      {new_vec4(1.0f,  -1.0f, -1.0f, 1.0f), 0xFF0000FF},
      {new_vec4(-1.0f, -1.0f, -1.0f, 1.0f), 0xFF0000FF},

      // left
      {new_vec4(0.0f,  1.0f,  0.0f,  1.0f), 0xFFFFFF00},
      {new_vec4(-1.0f, -1.0f, -1.0f, 1.0f), 0xFFFFFF00},
      {new_vec4(-1.0f, -1.0f, 1.0f,  1.0f), 0xFFFFFF00},

      // base triangle 1
      {new_vec4(-1.0f, -1.0f, 1.0f,  1.0f), 0xFF00FFFF},
      {new_vec4(-1.0f, -1.0f, -1.0f, 1.0f), 0xFF00FFFF},
      {new_vec4(1.0f,  -1.0f, -1.0f, 1.0f), 0xFF00FFFF},

      // base triangle 2
      {new_vec4(-1.0f, -1.0f, 1.0f,  1.0f), 0xFFFF00FF},
      {new_vec4(1.0f,  -1.0f, -1.0f, 1.0f), 0xFFFF00FF},
      {new_vec4(1.0f,  -1.0f, 1.0f,  1.0f), 0xFFFF00FF},
    };

    // model-to-world
    clock_t t2    = clock();
    double  dur   = 1.0 * (t2 - t1) / CLOCKS_PER_SEC;
    Mat4    model = rotate_y(dur);
    // Mat4 model = identity();

    // world-to-view
    Mat4 view =
      look_at(camera_pos, vec3_add(camera_pos, camera_front), camera_up);

    // projection
    float fov = 65.0, near = 0.05, far = 100.0;
    float aspect     = ((float)cfg->win_w / (float)cfg->win_h);
    Mat4  projection = perspective(fov * (M_PI / 180.0f), aspect, near, far);

    // for (unsigned i = 0; i < 3; ++i)
    // {
    //   // model -> world -> view -> projection
    //   triangle[i].pos = transform(model, triangle[i].pos);
    //   triangle[i].pos = transform(view, triangle[i].pos);
    //   triangle[i].pos = transform(projection, triangle[i].pos);
    //   // NDC coordinates
    //   triangle[i].pos.x /= triangle[i].pos.w;
    //   triangle[i].pos.y /= triangle[i].pos.w;
    //   triangle[i].pos.z /= triangle[i].pos.w;
    //   // Device/display coordinates
    //   triangle[i].pos.x = (triangle[i].pos.x + 1.0f) * 0.5f * cfg->win_w;
    //   triangle[i].pos.y = (1.0f - triangle[i].pos.y) * 0.5f * cfg->win_h;
    // }
    // draw_triangle(triangle, frame_buffer, false);
    for (unsigned i = 0; i < 18; ++i)
    {
      // model -> world -> view -> projection
      pyramid[i].pos = transform(model, pyramid[i].pos);
      pyramid[i].pos = transform(view, pyramid[i].pos);
      pyramid[i].pos = transform(projection, pyramid[i].pos);
      // NDC coordinates
      pyramid[i].pos.x /= pyramid[i].pos.w;
      pyramid[i].pos.y /= pyramid[i].pos.w;
      pyramid[i].pos.z /= pyramid[i].pos.w;
      // Device/display coordinates
      pyramid[i].pos.x = (pyramid[i].pos.x + 1.0f) * 0.5f * cfg->win_w;
      pyramid[i].pos.y = (1.0f - pyramid[i].pos.y) * 0.5f * cfg->win_h;
    }

    for (unsigned i = 0; i < 6; ++i)
      draw_triangle(pyramid + (i * 3), frame_buffer, true);

    update_window(cfg, x_img);
  };
  close_window(cfg);
  return 0;
}
