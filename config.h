#ifndef INCLUDE_CONFIG_H
#define INCLUDE_CONFIG_H

#include "queue.h"

struct display_config {
  struct {
    int req_width;
    int req_height;
  } capture;

  struct {
    int use_h264;
    int chunk_size;
    int colorspace;
  } h264_param;

  struct {
    int width;
    int pitch;
    int height;
    const char *title_string;
    int fixed_size;
    int enable_border;
    int open;
    GLFWwindow *glfw_id;
    unsigned char *frame;
    pthread_t thread;
  } window;

  struct {
    struct timespec epoch;
    long int frames_captured;
    long int frames_rendered;
    long int time_elapsed_ms;
    double capture_frame_rate_fps;
    double render_frame_rate_fps;
  } stat;

  struct queue queue_r;
  struct queue queue_w;

  struct {
    int argc;
    char **argv;
    unsigned int frame_delay_ms;
    int test;
  } other;

  struct {
    int size;
    int pitch;
    int width;
    int height;
    int tex_size;
    int tex_pitch;
    int tex_width;
    int tex_height;
    unsigned char *dptr;
    int lock;
  } frame;

  struct {
    int fill_solid;
    int fill_half;
    int fill_pixel;
    int fill_diag;
    int fill_horiz;
  } test;
};

#endif
