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
  uint32_t *color_buffer[2];
  float    *depth_buffer[2];
  uint32_t  width, height;
  uint8_t   draw_idx;
} FrameBuffer;

typedef uint32_t Color;

typedef struct {
  Vec4  pos;
  Color color;
} Vertex;

typedef struct {
  Vertex *verts;
  size_t  vertex_count;
  size_t *indices;
  size_t  index_count;
} Mesh;

typedef struct {
  Mesh mesh;
  Mat4 mtw;
} Model;

typedef enum {
  KEY_ESC        = 9,
  KEY_LEFT_SHIFT = 50,
  KEY_LEFT_CTRL  = 37,
  KEY_W          = 25,
  KEY_A          = 38,
  KEY_S          = 39,
  KEY_D          = 40,
  KEY_R          = 27,
} KEY_ENUM;

// TODO: replace with bitfield later on
typedef struct {
  bool w;
  bool a;
  bool s;
  bool d;
  bool shift;
  bool ctrl;
} InputState;

// ============================================================================
// PLUMBING & MISC
// ============================================================================

static struct timespec last_frame;

double get_frame_delta()
{
  struct timespec current_frame;
  clock_gettime(CLOCK_MONOTONIC, &current_frame);
  double dt = (current_frame.tv_sec - last_frame.tv_sec) +
              (current_frame.tv_nsec - last_frame.tv_nsec) / 1e9;


  last_frame = current_frame;
  return dt;
}

void parse_args(AppConfig *cfg, int argc, char *argv[])
{
  if (argc < 3)
  {
    fprintf(stderr, "Too few args");
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
    fprintf(stderr, "Failed to open display");
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
  FrameBuffer *frame_buffer  = calloc(1, sizeof(FrameBuffer));
  uint32_t    *color_buffer0 = calloc(1, width * height * sizeof(uint32_t));
  uint32_t    *color_buffer1 = calloc(1, width * height * sizeof(uint32_t));
  float       *depth_buffer0 = calloc(1, width * height * sizeof(float));
  float       *depth_buffer1 = calloc(1, width * height * sizeof(float));

  frame_buffer->width           = width;
  frame_buffer->height          = height;
  frame_buffer->draw_idx        = 0;
  frame_buffer->color_buffer[0] = color_buffer0;
  frame_buffer->depth_buffer[0] = depth_buffer0;
  frame_buffer->color_buffer[1] = color_buffer1;
  frame_buffer->depth_buffer[1] = depth_buffer1;

  return frame_buffer;
}

void update_window(AppConfig const *cfg, XImage *x_img, FrameBuffer *fb)
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
  fb->draw_idx = !fb->draw_idx;
  x_img->data  = (char *)fb->color_buffer[fb->draw_idx];
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
  fb->color_buffer[fb->draw_idx][idx] = color;
}

void draw_triangle(Vertex      *verts,
                   size_t       idx1,
                   size_t       idx2,
                   size_t       idx3,
                   FrameBuffer *fb,
                   bool         backface_culling)
{
  // NOTE: Assumes vertices are in clip space

  Vertex v[3] = {
    verts[idx1],
    verts[idx2],
    verts[idx3],
  };

  // reject triangles behind near plane
  // TODO: properly clip triangles
  if (v[0].pos.z < -v[0].pos.w || v[1].pos.z < -v[1].pos.w ||
      v[2].pos.z < -v[2].pos.w)
  {
    return;
  }

  // Clip space -> NDC -> screen space
  for (int i = 0; i < 3; i++)
  {
    v[i].pos.x /= v[i].pos.w;
    v[i].pos.y /= v[i].pos.w;
    v[i].pos.z /= v[i].pos.w;

    v[i].pos.x = (v[i].pos.x + 1.0f) * 0.5f * fb->width;
    v[i].pos.y = (1.0f - v[i].pos.y) * 0.5f * fb->height;
  }

  Vec4 v1 = v[0].pos;
  Vec4 v2 = v[1].pos;
  Vec4 v3 = v[2].pos;

  Color c1 = v[0].color;
  Color c2 = v[1].color;
  Color c3 = v[2].color;

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

  for (size_t x = xmin; x <= xmax; ++x)
    for (size_t y = ymin; y <= ymax; ++y)
    {
      Vec3  p  = new_vec3((float)x + 0.5f, (float)y + 0.5f, 0.0f);
      float w0 = (v3.x - v2.x) * (p.y - v2.y) - (v3.y - v2.y) * (p.x - v2.x);
      float w1 = (v1.x - v3.x) * (p.y - v3.y) - (v1.y - v3.y) * (p.x - v3.x);
      float w2 = (v2.x - v1.x) * (p.y - v1.y) - (v2.y - v1.y) * (p.x - v1.x);

      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
      {
        float bw0 = w0 / area;
        float bw1 = w1 / area;
        float bw2 = w2 / area;
        float z   = bw0 * v1.z + bw1 * v2.z + bw2 * v3.z;

        size_t idx = y * fb->width + x;
        if (z >= fb->depth_buffer[fb->draw_idx][idx]) continue;

        // Color interpolation
        uint8_t r1 = (c1 >> 24) & 0xFF;
        uint8_t g1 = (c1 >> 16) & 0xFF;
        uint8_t b1 = (c1 >> 8) & 0xFF;
        uint8_t a1 = (c1 >> 0) & 0xFF;

        uint8_t r2 = (c2 >> 24) & 0xFF;
        uint8_t g2 = (c2 >> 16) & 0xFF;
        uint8_t b2 = (c2 >> 8) & 0xFF;
        uint8_t a2 = (c2 >> 0) & 0xFF;

        uint8_t r3 = (c3 >> 24) & 0xFF;
        uint8_t g3 = (c3 >> 16) & 0xFF;
        uint8_t b3 = (c3 >> 8) & 0xFF;
        uint8_t a3 = (c3 >> 0) & 0xFF;

        uint8_t r = (uint8_t)(bw0 * r1 + bw1 * r2 + bw2 * r3);
        uint8_t g = (uint8_t)(bw0 * g1 + bw1 * g2 + bw2 * g3);
        uint8_t b = (uint8_t)(bw0 * b1 + bw1 * b2 + bw2 * b3);
        uint8_t a = (uint8_t)(bw0 * a1 + bw1 * a2 + bw2 * a3);

        Color c = ((uint32_t)r << 24) | ((uint32_t)g << 16) |
                  ((uint32_t)b << 8) | ((uint32_t)a << 0);

        fb->depth_buffer[fb->draw_idx][idx] = z;
        draw_pixel(x, y, c, fb);
      }
    }
}

void draw_model(Model const *model,
                Mat4 const  *view,
                Mat4 const  *projection,
                FrameBuffer *fb,
                bool         backface_culling)
{
  Vertex transformed[model->mesh.vertex_count];

  for (size_t i = 0; i < model->mesh.vertex_count; ++i)
  {
    transformed[i]     = model->mesh.verts[i];
    transformed[i].pos = transform(model->mtw, transformed[i].pos);
    transformed[i].pos = transform(*view, transformed[i].pos);
    transformed[i].pos = transform(*projection, transformed[i].pos);
  }
  for (size_t offset = 0; offset < model->mesh.index_count; offset += 3)
  {
    size_t idx1 = model->mesh.indices[offset];
    size_t idx2 = model->mesh.indices[offset + 1];
    size_t idx3 = model->mesh.indices[offset + 2];
    draw_triangle(transformed, idx1, idx2, idx3, fb, backface_culling);
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

void clear_background(FrameBuffer *fb, Color color)
{
  int const size = fb->width * fb->height;

  for (unsigned i = 0; i < size; ++i) fb->color_buffer[fb->draw_idx][i] = color;
  for (unsigned i = 0; i < size; ++i)
    fb->depth_buffer[fb->draw_idx][i] = INFINITY;
}

// ============================================================================
// INPUT
// ============================================================================

void poll_input(AppConfig *cfg, bool *quit, InputState *input)
{
  while (XPending(cfg->display) > 0)
  {
    XEvent event = {0};
    XNextEvent(cfg->display, &event);

    if (event.type == KeyPress)
    {
      switch (event.xkey.keycode)
      {
        case KEY_ESC: *quit = true; break;
        case KEY_W: input->w = true; break;
        case KEY_A: input->a = true; break;
        case KEY_S: input->s = true; break;
        case KEY_D: input->d = true; break;
        case KEY_LEFT_SHIFT: input->shift = true; break;
        case KEY_LEFT_CTRL: input->ctrl = true; break;
      }
    }

    if (event.type == KeyRelease)
    {
      switch (event.xkey.keycode)
      {
        case KEY_W: input->w = false; break;
        case KEY_A: input->a = false; break;
        case KEY_S: input->s = false; break;
        case KEY_D: input->d = false; break;
        case KEY_LEFT_SHIFT: input->shift = false; break;
        case KEY_LEFT_CTRL: input->ctrl = false; break;
      }
    }
  }
}

void update_camera(Vec3 *camera_pos, InputState const *input, double dt)
{
  float speed = 2.5 * dt;

  if (input->w) camera_pos->z -= speed;
  if (input->s) camera_pos->z += speed;
  if (input->a) camera_pos->x -= speed;
  if (input->d) camera_pos->x += speed;

  if (input->shift) camera_pos->y += speed;
  if (input->ctrl) camera_pos->y -= speed;
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

  FrameBuffer *fb = init_framebuffer(cfg->win_w, cfg->win_h);

  XImage *x_img = XCreateImage(cfg->display,
                               cfg->visual,
                               cfg->depth,
                               ZPixmap,
                               0,
                               (char *)fb->color_buffer[fb->draw_idx],
                               cfg->win_w,
                               cfg->win_h,
                               32,
                               0);

  printf("X11 pixel format\n\tR: %08lx \n\tG: %08lx \n\tB: %08lx\n",
         x_img->red_mask,
         x_img->green_mask,
         x_img->blue_mask);


  Vec3 camera_pos   = (Vec3){0.0f, 0.0f, 10.0f};
  Vec3 camera_front = (Vec3){0.0f, 0.0f, -1.0f};
  Vec3 camera_up    = (Vec3){0.0f, 1.0f, 0.0f};

  clock_gettime(CLOCK_MONOTONIC, &last_frame);

  InputState  input_state = {0};
  static bool quit        = false;
  while (!quit)
  {
    // WARN: only call once per frame
    double const dt = get_frame_delta();
    // printf("frame time: %f\n", df);

    poll_input(cfg, &quit, &input_state);
    update_camera(&camera_pos, &input_state, dt);

    clear_background(fb, 0xFF000000);

    Vertex pyramid_vertices[] = {
      // 0: top
      {new_vec4(0.0f,  1.0f,  0.0f,  1.0f), 0xFFFF0000},

      // 1-4: base corners
      {new_vec4(-1.0f, -1.0f, 1.0f,  1.0f), 0xFF00FF00},
      {new_vec4(1.0f,  -1.0f, 1.0f,  1.0f), 0xFF0000FF},
      {new_vec4(1.0f,  -1.0f, -1.0f, 1.0f), 0xFFFFFF00},
      {new_vec4(-1.0f, -1.0f, -1.0f, 1.0f), 0xFFFF00FF},
    };

    size_t pyramid_indices[] = {
      // front
      0,
      1,
      2,
      // right
      0,
      2,
      3,
      // back
      0,
      3,
      4,
      // left
      0,
      4,
      1,
      // bottom
      1,
      4,
      3,
      1,
      3,
      2,
    };

    Mesh pyramid_mesh = {
      pyramid_vertices,
      sizeof(pyramid_vertices) / sizeof(pyramid_vertices[0]),
      pyramid_indices,
      sizeof(pyramid_indices) / sizeof(pyramid_indices[0]),
    };

    // model-to-world
    static double r = 0.0f;
    r += dt;
    Mat4 model = rotate_y(r);

    Model pyramid_model;
    pyramid_model.mesh = pyramid_mesh;
    pyramid_model.mtw  = rotate_y(r);

    // world-to-view
    Mat4 view =
      look_at(camera_pos, vec3_add(camera_pos, camera_front), camera_up);

    // projection
    float fov = 65.0, near = 0.05, far = 100.0;
    float aspect     = ((float)cfg->win_w / (float)cfg->win_h);
    Mat4  projection = perspective(fov * (M_PI / 180.0f), aspect, near, far);

    // Draw model
    draw_model(&pyramid_model, &view, &projection, fb, true);

    update_window(cfg, x_img, fb);
  };
  close_window(cfg);
  return 0;
}
