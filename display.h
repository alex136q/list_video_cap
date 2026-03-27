#ifndef INCLUDE_DISPLAY_H
#define INCLUDE_DISPLAY_H

#include "queue.h"


enum CMD {
  VIDEO_CMD_NOP = -1,
  VIDEO_CMD_CLOSE = 0,
  VIDEO_CMD_OPEN = 1,
  VIDEO_CMD_SET_WIDTH = 2,
  VIDEO_CMD_SET_PITCH = 3,
  VIDEO_CMD_FRAME_YUYV = 4,
  VIDEO_CMD_SET_HEIGHT = 5,
  VIDEO_CMD_SET_ARGC = 6,
  VIDEO_CMD_SET_ARGV = 7,
  VIDEO_CMD_WRITE = 8,
  VIDEO_CMD_FRAME_H264 = 9,
};

void video_ctl(struct video_msg cmd);
void toggle_graphics(int init);

void shrink_queue(struct queue *stk);

void init_state();
void init_display();
void destroy_display();

void rename_window();

void set_image(const struct video_msg *cmd);

int process_packet_yuyv(const struct video_msg *cmd);
int process_packet_h264(const struct video_msg *cmd);

void render_frame();
void render_test_frame();
void *main_loop(void *dummy);

void resize_window(int msg_width, int msg_height);
void resize_fb();

void update_frame_stats();

unsigned char *yuyv_to_gray(unsigned char *ptr,
			    int pitch, int width, int height);

void blit_rgba_image(unsigned char *ptr);

void print_messages(struct queue *stk, const char *debug_label);


#endif
