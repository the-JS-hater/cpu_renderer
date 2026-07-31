#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>
#include <math.h>

#include "linalg.h"
#include "obj_loader.h"

typedef struct {
  Window       window;
  Display     *display;
  Visual      *visual;
  long         event_mask;
  unsigned int win_w, win_h;
  unsigned int res_w, res_h;
  int          screen, depth;
  bool         borderless;
} AppConfig;

typedef struct {
  uint32_t *color_buffer[2];
  float    *depth_buffer[2];
  uint32_t  width, height;
  uint8_t   draw_idx;
} FrameBuffer;

typedef struct {
  uint32_t *pixels;
  unsigned  width, height;
} DisplayBuffer;

typedef struct {
  Vec3 camera_up, camera_front, camera_pos;
} Camera;

typedef uint32_t Color;

typedef enum {
  COLOR_R,
  COLOR_G,
  COLOR_B,
  COLOR_A,
  NORMAL_X,
  NORMAL_Y,
  NORMAL_Z,
  MAX_VARYING_ATTRS,
} AttributeEnum;

typedef struct {
  Vec4  pos;
  float varying[MAX_VARYING_ATTRS];
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
  int  mouse_dx;
  int  mouse_dy;
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
  for (unsigned i = 1; i < argc; ++i)
  {
    if (!strcmp(argv[i], "-w"))
      sscanf(argv[++i], "%dx%d", &cfg->win_w, &cfg->win_h);
    else if (!strcmp(argv[i], "-r"))
      sscanf(argv[++i], "%dx%d", &cfg->res_w, &cfg->res_h);
    else if (!strcmp(argv[i], "-b"))
      cfg->borderless = true;
    else
    {
      fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      exit(1);
    }
  }
}

void hide_cursor(AppConfig *cfg)
{
  char   data[1] = {0};
  Pixmap blank_pixmap =
    XCreateBitmapFromData(cfg->display, cfg->window, data, 1, 1);
  XColor dummy     = {0};
  Cursor invisible = XCreatePixmapCursor(cfg->display,
                                         blank_pixmap,
                                         blank_pixmap,
                                         &dummy,
                                         &dummy,
                                         0,
                                         0);
  XDefineCursor(cfg->display, cfg->window, invisible);
  XFreePixmap(cfg->display, blank_pixmap);
  XFreeCursor(cfg->display, invisible);
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
  if (cfg->borderless)
  {
    Atom motif_hints = XInternAtom(cfg->display, "_MOTIF_WM_HINTS", False);
    long hints[5]    = {(1L << 1), 0, 0, 0, 0};

    XChangeProperty(cfg->display,
                    cfg->window,
                    motif_hints,
                    motif_hints,
                    32,
                    PropModeReplace,
                    (unsigned char *)hints,
                    5);
  }
  XStoreName(cfg->display, cfg->window, title);
  XSelectInput(cfg->display, cfg->window, cfg->event_mask);
  XMapWindow(cfg->display, cfg->window);
  hide_cursor(cfg);
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

DisplayBuffer *init_display_buffer(unsigned width, unsigned height)
{
  DisplayBuffer *db = calloc(1, sizeof(*db));
  db->pixels        = calloc(1, width * height * sizeof(uint32_t));
  db->width         = width;
  db->height        = height;
  return db;
}

void resample_nearest(FrameBuffer const *fb, DisplayBuffer *db)
{
  uint32_t const *src = fb->color_buffer[fb->draw_idx];

  for (unsigned y = 0; y < db->height; ++y)
  {
    unsigned sy = (uint64_t)y * fb->height / db->height;
    for (unsigned x = 0; x < db->width; ++x)
    {
      unsigned sx                   = (uint64_t)x * fb->width / db->width;
      db->pixels[y * db->width + x] = src[sy * fb->width + sx];
    }
  }
}

void update_window(AppConfig const *cfg,
                   XImage          *render_img,
                   XImage          *disp_img,
                   DisplayBuffer   *db,
                   FrameBuffer     *fb)
{
  resample_nearest(fb, db);
  XPutImage(cfg->display,
            cfg->window,
            DefaultGC(cfg->display, cfg->screen),
            disp_img,
            0,  // src_x
            0,  // src_y
            0,  // dest_x
            0,  // dest_y
            cfg->win_w,
            cfg->win_h);
  XFlush(cfg->display);
  fb->draw_idx     = !fb->draw_idx;
  render_img->data = (char *)fb->color_buffer[fb->draw_idx];
}

Mesh mesh_from_obj(ObjObject const *obj)
{
  Mesh mesh         = {0};
  mesh.vertex_count = obj->vertex_count;
  mesh.index_count  = obj->face_count * 3;
  mesh.verts        = calloc(mesh.vertex_count, sizeof(Vertex));
  mesh.indices      = calloc(mesh.index_count, sizeof(size_t));

  if (!mesh.verts || !mesh.indices)
  {
    free(mesh.verts);
    free(mesh.indices);
    return (Mesh){0};
  }
  for (size_t i = 0; i < obj->vertex_count; i++)
  {
    obj_Vertex const *pos = &obj->verts[i];

    mesh.verts[i].pos = new_vec4(pos->x, pos->y, pos->z, pos->w);
  }
  size_t index = 0;
  for (size_t i = 0; i < obj->face_count; i++)
  {
    obj_Face const *face = &obj->faces[i];
    for (int j = 0; j < 3; j++)
    {
      obj_FaceElement const *e = &face->triangles[j];

      size_t vertex_index   = e->v_i - 1;
      mesh.indices[index++] = vertex_index;

      if (e->vn_i > 0)
      {
        obj_Normal const *n = &obj->normals[e->vn_i - 1];

        mesh.verts[vertex_index].varying[NORMAL_X] = n->x;
        mesh.verts[vertex_index].varying[NORMAL_Y] = n->y;
        mesh.verts[vertex_index].varying[NORMAL_Z] = n->z;
      }
    }
  }
  return mesh;
}

Model load_model(char const *filename)
{
  ObjObject obj = {0};
  if (!load_obj_file(filename, &obj))
  {
    fprintf(stderr, "Failed to load model: %s\n", filename);
    return (Model){0};
  }
  Model model = {.mesh = mesh_from_obj(&obj), .mtw = identity()};
  free_obj_object(&obj);
  return model;
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

uint32_t pack_color(float const *v)
{
  uint32_t r = (uint32_t)(v[COLOR_R] * 255.0f);
  uint32_t g = (uint32_t)(v[COLOR_G] * 255.0f);
  uint32_t b = (uint32_t)(v[COLOR_B] * 255.0f);
  uint32_t a = (uint32_t)(v[COLOR_A] * 255.0f);

  return (a << 24) | (r << 16) | (g << 8) | b;
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

        float varying[MAX_VARYING_ATTRS];
        for (int i = 0; i < MAX_VARYING_ATTRS; ++i)
        {
          varying[i] = bw0 * v[0].varying[i] + bw1 * v[1].varying[i] +
                       bw2 * v[2].varying[i];
        }
        Color c = pack_color(varying);

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
  int const center_x = cfg->win_w / 2;
  int const center_y = cfg->win_h / 2;

  input->mouse_dx = 0;
  input->mouse_dy = 0;
  while (XPending(cfg->display) > 0)
  {
    XEvent event = {0};
    XNextEvent(cfg->display, &event);
    if (event.type == MotionNotify)
    {
      int const x = event.xmotion.x;
      int const y = event.xmotion.y;

      // XWarpPointer generates BS event
      if (x == center_x && y == center_y) continue;

      input->mouse_dx += x - center_x;
      input->mouse_dy += y - center_y;
    }
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
  XWarpPointer(cfg->display, None, cfg->window, 0, 0, 0, 0, center_x, center_y);
  XFlush(cfg->display);
}

void update_camera(Camera *camera, InputState const *input, double dt)
{
  float const speed             = 2.5 * dt;
  float const mouse_sensitivity = 0.005f * dt;

  float const yaw      = input->mouse_dx * mouse_sensitivity;
  float const pitch    = input->mouse_dy * mouse_sensitivity;
  Vec3 const  world_up = {0.0f, 1.0f, 0.0f};
  Vec3 const  right    = vec3_norm(cross(camera->camera_front, world_up));

  camera->camera_front =
    vec3(transform_vec3(rotate_x(pitch), camera->camera_front));
  camera->camera_front =
    vec3(transform_vec3(rotate_y(yaw), camera->camera_front));
  camera->camera_front = vec3_norm(camera->camera_front);

  if (input->w)
    camera->camera_pos =
      vec3_add(camera->camera_pos, vec3_mult_val(camera->camera_front, speed));
  if (input->s)
    camera->camera_pos =
      vec3_sub(camera->camera_pos, vec3_mult_val(camera->camera_front, speed));
  if (input->d)
    camera->camera_pos =
      vec3_add(camera->camera_pos, vec3_mult_val(right, speed));
  if (input->a)
    camera->camera_pos =
      vec3_sub(camera->camera_pos, vec3_mult_val(right, speed));

  if (input->shift) camera->camera_pos.y += speed;
  if (input->ctrl) camera->camera_pos.y -= speed;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[])
{
  AppConfig *cfg = calloc(1, sizeof(*cfg));

  cfg->event_mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask |
                    KeyReleaseMask | PointerMotionMask;

  parse_args(cfg, argc, argv);
  create_window(cfg, "CPU RENDERING PROTOTYPE V2");

  FrameBuffer   *fb = init_framebuffer(cfg->res_w, cfg->res_h);
  DisplayBuffer *db = init_display_buffer(cfg->win_w, cfg->win_h);

  XImage *render_img = XCreateImage(cfg->display,
                                    cfg->visual,
                                    cfg->depth,
                                    ZPixmap,
                                    0,
                                    (char *)fb->color_buffer[fb->draw_idx],
                                    cfg->res_w,
                                    cfg->res_h,
                                    32,
                                    0);
  XImage *disp_img   = XCreateImage(cfg->display,
                                  cfg->visual,
                                  cfg->depth,
                                  ZPixmap,
                                  0,
                                  (char *)db->pixels,
                                  cfg->win_w,
                                  cfg->win_h,
                                  32,
                                  0);
  printf("X11 pixel format\n\tR: %08lx \n\tG: %08lx \n\tB: %08lx\n",
         render_img->red_mask,
         render_img->green_mask,
         render_img->blue_mask);

  Model teapot_model = load_model("teapot.obj");
  teapot_model.mtw   = identity();
  // TEMP: normals as colors
  for (size_t i = 0; i < teapot_model.mesh.vertex_count; i++)
  {
    Vertex *v = &teapot_model.mesh.verts[i];

    v->varying[COLOR_R] = v->varying[NORMAL_X] * 0.5f + 0.5f;
    v->varying[COLOR_G] = v->varying[NORMAL_Y] * 0.5f + 0.5f;
    v->varying[COLOR_B] = v->varying[NORMAL_Z] * 0.5f + 0.5f;
    v->varying[COLOR_A] = 1.0f;
  }

  Camera camera = {
    (Vec3){0.0f, 1.0f, 0.0f },
    (Vec3){0.0f, 0.0f, -1.0f},
    (Vec3){0.0f, 0.0f, 10.0f},
  };
  clock_gettime(CLOCK_MONOTONIC, &last_frame);

  InputState  input_state = {0};
  static bool quit        = false;
  while (!quit)
  {
    // WARN: only call once per frame
    double const dt = get_frame_delta();
    // printf("frame time: %f\n", df);

    poll_input(cfg, &quit, &input_state);

    update_camera(&camera, &input_state, dt);

    clear_background(fb, 0xFFFFFFFF);

    // model-to-world
    static double r = 0.0f;
    r += dt;
    teapot_model.mtw = rotate_y(r);

    // world-to-view
    Mat4 view = look_at(camera.camera_pos,
                        vec3_add(camera.camera_pos, camera.camera_front),
                        camera.camera_up);

    // projection
    float fov = 65.0, near = 0.05, far = 100.0;
    float aspect     = ((float)cfg->win_w / (float)cfg->win_h);
    Mat4  projection = perspective(fov * (M_PI / 180.0f), aspect, near, far);

    // Draw model
    draw_model(&teapot_model, &view, &projection, fb, true);

    update_window(cfg, render_img, disp_img, db, fb);
  };
  close_window(cfg);
  return 0;
}
