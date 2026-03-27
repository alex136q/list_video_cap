#include "headers.h"

extern struct cli_args cli;

struct display_config display;

struct h264_config h264_decoder;
extern struct h264_config h264_encoder;

extern struct yuv_matrix yuv_rgb;

#define stream_h264_buffer_size 4096


void apply_test_patterns(unsigned char *rgba_image);

void decode_h264_data(const struct video_msg *cmd);
void receive_partial_h264_packet(const struct video_msg *cmd);


void video_ctl(struct video_msg cmd) {
  debug_f0("[VIDEO] [video_ctl] Got packet: "); dump_msg_header(&cmd);

  dump_queue_sizes();

  struct video_msg msg;

  while(display.queue_debug.head != NULL
	&& display.queue_debug.head->oper == VIDEO_CMD_WRITE) {
    queue_pop(&display.queue_debug, &msg);
    if(msg.dptr) {
      debug_s1("[VIDEO] [video_ctl] %s", (char *)msg.dptr);
      free(msg.dptr);
    }
  }

  if(cmd.oper == VIDEO_CMD_OPEN) {
    debug_f0("[VIDEO] [video_ctl] opening window\n");
    init_state();
    init_display();
    h264_init_decoder(&h264_decoder);
    h264_decoder.debug_info = h264_encoder.debug_info;
    h264_decoder.dump_bytes = h264_encoder.dump_bytes;
  }
  else if(cmd.oper == VIDEO_CMD_SET_HEIGHT) {
    display.frame.height = cmd.size;
  }
  else if(cmd.oper == VIDEO_CMD_SET_WIDTH) {
    display.frame.width = cmd.size;
  }
  else if(cmd.oper == VIDEO_CMD_SET_PITCH) {
    display.frame.pitch = cmd.size;
  }
  else if(cmd.oper == VIDEO_CMD_FRAME_YUYV
	  || cmd.oper == VIDEO_CMD_FRAME_H264) {
    if(cli.frame_capture_path != NULL && cmd.oper == VIDEO_CMD_FRAME_H264) {
      debug_s1("[VIDEO] [video_ctl] Dumping packet into %s\n", cli.frame_capture_path);
      FILE *h264_dump_fd = fopen(cli.frame_capture_path, "a");
      fwrite(cmd.dptr, 1, cmd.size, h264_dump_fd);
      fclose(h264_dump_fd);
    }

    struct video_msg *msg_copy = alloc_video_msg();
    msg_copy->oper = cmd.oper;
    msg_copy->size = cmd.size;
    if(cmd.size) {
      msg_copy->dptr = cmd.dptr;
    }
    else {
      msg_copy->dptr = NULL;
    }

    debug_f0("[VIDEO] [video_ctl] Cloned as:\n"); dump_msg_header(msg_copy);

    if(cmd.oper == VIDEO_CMD_FRAME_YUYV) {
      queue_push(&display.queue_frames, msg_copy);
    }
    else {
      queue_push(&display.queue_packets, msg_copy);
    }

    debug_f3("[VIDEO] Message queued (%d/%d) at %016lX\n",
	     display.queue_packets.length,
	     display.queue_packets.max_length,
	     (long int)display.queue_packets.tail);

    print_messages(&display.queue_packets, "CTRL ");
    print_messages(&display.queue_debug, "DEBUG");
    print_messages(&display.queue_debug, "VIDEO");
  }
  else if(cmd.oper == VIDEO_CMD_CLOSE) {
    debug_f0("[VIDEO] [video_ctl] Quitting\n");
    display.window.open = 0;
    h264_free_decoder(&h264_decoder);
  }
  else {
    debug_f1("[VIDEO] [video_ctl] Unknown message type: %d; skipping\n", cmd.oper);
  }
}

void toggle_graphics(int init) {
  send_video_packet((init ? VIDEO_CMD_OPEN : VIDEO_CMD_CLOSE), NULL, 0);
}

void print_messages(struct queue *stk, const char *debug_label) {
  char buf[512];
  memset(buf, 0, sizeof(buf));

  sprintf(buf,
	  "[VIDEO] Queue sizes: R%d %04Xh, W%d %04Xh, F%d %04Xh\n",
	   display.queue_packets.lock, display.queue_packets.length,
	   display.queue_debug.lock, display.queue_debug.length,
	   display.queue_frames.lock, display.queue_frames.length);

  debug_f0(buf);
  debug_s1("[VIDEO] %s, stack:\n", debug_label);

  dump_msg_headers(stk);
}

void init_state() {
  display.window.title_string = "Framerate";
  display.window.open = 0;
  display.window.frame = NULL;

  init_queue(&display.queue_packets);
  init_queue(&display.queue_debug);
  init_queue(&display.queue_frames);

  clock_gettime(CLOCK_REALTIME, &display.stat.epoch);
  display.stat.frames_captured = 0;
  display.stat.frames_rendered = 0;
  display.stat.time_elapsed_ms = 0;
  display.stat.capture_frame_rate_fps = 0.0;
  display.stat.render_frame_rate_fps = 0.0;
}

void init_display() {
  pthread_create(&display.window.thread, NULL, main_loop, NULL);
}

void destroy_display() {
}

void rename_window() {
  if(!display.window.glfw_id) return;

  static long int last_rename = 0;
  long int now = time(NULL);
  if(now - last_rename >= 1) {
    last_rename = now;
  }
  else {
    return;
  }

  char window_title[256];

  sprintf(window_title,
	  "Stream: %6.2lf FPSr (%ld), %6.2lf FPSc (%ld), %.2lf s, "
	  "Win. %dx%d, R %d/%d W %d/%d F %d/%d",
	  display.stat.render_frame_rate_fps,
	  display.stat.frames_rendered,
	  display.stat.capture_frame_rate_fps,
	  display.stat.frames_captured,
	  display.stat.time_elapsed_ms / 1000.0,
	  display.window.width,
	  display.window.height,
	  display.queue_packets.length,
	  display.queue_packets.max_length,
	  display.queue_debug.length,
	  display.queue_debug.max_length,
	  display.queue_frames.length,
	  display.queue_frames.max_length);

  glfwSetWindowTitle(display.window.glfw_id, window_title);
}

void set_image(const struct video_msg *msg) {
  if(msg == NULL) {
    debug_f0("[FRAME] null message pointer\n");
    return;
  }

  if(msg->oper == VIDEO_CMD_FRAME_YUYV) {
    acquire_lock(&display.frame.lock);

    debug_f1("[FRAME] Input frame: raw YUYV, size %d:\n", msg->size);
    dump_msg_header(msg);

    display.frame.size = msg->size;

    if(display.frame.dptr) free(display.frame.dptr);
    display.frame.dptr = malloc(msg->size);

    memcpy(display.frame.dptr, msg->dptr, msg->size);

    ++display.stat.frames_captured; update_frame_stats();

    release_lock(&display.frame.lock);
  }
  else if(msg->oper == VIDEO_CMD_FRAME_H264) {
    debug_f0("[FRAME] frame: H.264\n");
    dump_msg_header(msg);

    receive_partial_h264_packet(msg);

    /* decoded frames will be enqueued */
  }
}

void update_frame_stats() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  display.stat.time_elapsed_ms =
    (1E+3 * (now.tv_sec - display.stat.epoch.tv_sec)
     + 1E-6 * (now.tv_nsec - display.stat.epoch.tv_nsec));

  if(display.stat.time_elapsed_ms >= 1.0) {
    display.stat.capture_frame_rate_fps =
      ((double)display.stat.frames_captured
       / (display.stat.time_elapsed_ms / 1000.0));
    display.stat.render_frame_rate_fps =
      ((double)display.stat.frames_rendered
       / (display.stat.time_elapsed_ms / 1000.0));
  }
  else {
    display.stat.capture_frame_rate_fps = -1.0;
    display.stat.render_frame_rate_fps = -1.0;
  }
}

void process_cmds() {
  acquire_lock(&display.frame.lock);

  resize_fb();
  rename_window();

  queue_purge(&display.queue_packets, process_packet_h264);
  queue_purge_all_but_last(&display.queue_frames, process_packet_yuyv);

  release_lock(&display.frame.lock);
}

int process_packet_yuyv(const struct video_msg *cmd) {
    debug_f3("[RENDER] [RECV] received cmd(%08lXh %08lXh %016lXh)\n",
	     cmd->oper, cmd->size, (long int)cmd->dptr);

    switch(cmd->oper) {

    case VIDEO_CMD_FRAME_YUYV:
      debug_f0("[RENDER] [RECV] New YUYV frame\n");
      set_image(cmd);
      return 1;

    case VIDEO_CMD_CLOSE:
    case VIDEO_CMD_FRAME_H264: /* partial H.264 packet */
    case VIDEO_CMD_SET_WIDTH:
    case VIDEO_CMD_SET_HEIGHT:
    case VIDEO_CMD_SET_PITCH:
    default:
      debug_f1("[RENDER] [RECV] Unknown cmd in queue: %d\n", cmd->oper);
      exit(1);
      return 0;
    }
}

int process_packet_h264(const struct video_msg *cmd) {
    debug_f3("[RENDER] [RECV] Received cmd(%08lXh %08lXh %016lXh):\n",
	     cmd->oper, cmd->size, (long int)cmd->dptr);

    switch(cmd->oper) {

    case VIDEO_CMD_CLOSE:
      debug_f0("[RENDER] [RECV] Closing\n");
      return 1;

    case VIDEO_CMD_FRAME_YUYV:
      debug_f0("[RENDER] [RECV] New raw YUYV frame\n");
      set_image(cmd);
      return 1;

    case VIDEO_CMD_FRAME_H264: /* partial H.264 packet */
      debug_f0("[RENDER] [RECV] New H.264 packet\n");
      set_image(cmd);
      return 1;

    case VIDEO_CMD_SET_WIDTH:
      debug_f1("[RENDER] [RECV] Frame width set to %d\n", cmd->size);
      resize_window(cmd->size, display.window.height);
      return 1;

    case VIDEO_CMD_SET_HEIGHT:
      debug_f1("[RENDER] [RECV] Frame height set to %d\n", cmd->size);
      resize_window(display.window.width, cmd->size);
      return 1;

    case VIDEO_CMD_SET_PITCH:
      debug_f1("[RENDER] [RECV] Frame pitch set to %d\n", cmd->size);
      display.window.pitch = cmd->size;
      return 1;

    default:
      debug_f1("[RENDER] [RECV] Unknown cmd in queue: %d\n", cmd->oper);
      exit(1);
      return 0;
    }
}

void *main_loop(void *dummy) {
  debug_f0("[RENDER] [MAIN] Initializing GLFW.\n");

  glewExperimental = 1;

  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, (display.window.fixed_size ? GLFW_FALSE : GLFW_TRUE));
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

  display.window.glfw_id =
    glfwCreateWindow(display.window.width,
		     display.window.height,
		     display.window.title_string,
		     NULL, NULL);

  display.window.open = 1;

  glfwMakeContextCurrent(display.window.glfw_id);

  glewInit();

  debug_f1("[RENDER] [MAIN] Created window: %016Xh\n",
	   (long int)display.window.glfw_id);
  debug_f0("[RENDER] [MAIN] GLFW main loop.\n");

  while(display.window.open) {
    debug_f0("[RENDER] checking for messages to display...\n");
    process_cmds();

    if(!display.other.test) {
      debug_f0("[RENDER] live frame\n");
      acquire_lock(&display.frame.lock);
      render_frame();
      release_lock(&display.frame.lock);
    }
    else {
      debug_f0("[RENDER] solid color frame\n");
      render_test_frame();
    }

    glfwPollEvents();
    if(glfwWindowShouldClose(display.window.glfw_id)) {
      display.window.open = 0;
    }

    ++display.stat.frames_rendered; update_frame_stats();
    if(display.other.frame_delay_ms > 0) {
      sleep_ms(display.other.frame_delay_ms);
    }
    debug_f0("[RENDER] frame rendered\n");
  }

  glfwDestroyWindow(display.window.glfw_id);
  glfwTerminate();
  return NULL;
}

void resize_window(int msg_width, int msg_height) {
  int width, height;
  glfwGetWindowSize(display.window.glfw_id, &width, &height);

  if(width != msg_width || height != msg_height) {
    debug_f2("[RENDER] Resize window to %dx%d\n", msg_width, msg_height);

    if(display.window.fixed_size) {
      glfwSetWindowSizeLimits(display.window.glfw_id,
			      msg_width, msg_height,
			      msg_width, msg_height);
    }
    glfwSetWindowSize(display.window.glfw_id,
		      msg_width, msg_height);

    display.window.width = msg_width;
    display.window.height = msg_height;
    glViewport(0, 0, msg_width, msg_height);
  }
}

unsigned char *yuyv_to_gray(unsigned char *ptr,
			    int pitch,
			    int width,
			    int height) {
  int k;

  for(k = 1; k < width; k = k << 1);
  display.frame.tex_width = k;

  for(k = 1; k < height; k = k << 1);
  display.frame.tex_height = k;

  if(display.frame.tex_width > display.frame.tex_height) {
    display.frame.tex_height = display.frame.tex_width;
  }
  else if(display.frame.tex_height > display.frame.tex_width) {
    display.frame.tex_width = display.frame.tex_height;
  }

  display.frame.tex_pitch = display.frame.tex_width * 4;
  display.frame.tex_size = display.frame.tex_pitch * display.frame.tex_height;

  unsigned char *gray = malloc(display.frame.tex_size);

  int row;

  for(row = 0; row < height; ++row) {
    const int in_row_ptr = row * pitch;
    const int out_row_ptr = row * display.frame.tex_pitch;

    int col;

    for(col = 0; col < width; ++col) {
      gray[out_row_ptr + col * 4 + 0] = ptr[in_row_ptr + col * 2];
      gray[out_row_ptr + col * 4 + 1] = ptr[in_row_ptr + col * 2];
      gray[out_row_ptr + col * 4 + 2] = ptr[in_row_ptr + col * 2];
      gray[out_row_ptr + col * 4 + 3] = 0xFF;
    }

    for(; col < display.frame.tex_width; ++col) {
      gray[out_row_ptr + col * 4 + 0] = 0x00;
      gray[out_row_ptr + col * 4 + 1] = 0x00;
      gray[out_row_ptr + col * 4 + 2] = 0x00;
      gray[out_row_ptr + col * 4 + 3] = 0xFF;
    }
  }

  for(; row < display.frame.tex_height; ++row) {
    const int out_row_ptr = row * display.frame.tex_pitch;
    for(int col = 0; col < display.frame.tex_width; ++col) {
      gray[out_row_ptr + col * 4 + 0] = 0x00;
      gray[out_row_ptr + col * 4 + 1] = 0x00;
      gray[out_row_ptr + col * 4 + 2] = 0x00;
      gray[out_row_ptr + col * 4 + 3] = 0xFF;
    }
  }

  return gray;
}

unsigned char *yuyv_to_rgba(unsigned char *ptr,
			    int pitch,
			    int width,
			    int height) {
  int k;

  for(k = 1; k < width; k = k << 1);
  display.frame.tex_width = k;

  for(k = 1; k < height; k = k << 1);
  display.frame.tex_height = k;

  if(display.frame.tex_width > display.frame.tex_height) {
    display.frame.tex_height = display.frame.tex_width;
  }
  else if(display.frame.tex_height > display.frame.tex_width) {
    display.frame.tex_width = display.frame.tex_height;
  }

  display.frame.tex_pitch = display.frame.tex_width * 4;
  display.frame.tex_size = display.frame.tex_pitch * display.frame.tex_height;

  debug_f4("[VIDEO] [RGBA] frame   size %dx%d pitch %d size %ld\n",
	   display.frame.width, display.frame.height,
	   display.frame.pitch, display.frame.size);

  debug_f4("[VIDEO] [RGBA] texture size %dx%d pitch %d size %ld\n",
	   display.frame.tex_width, display.frame.tex_height,
	   display.frame.tex_pitch, display.frame.tex_size);

  unsigned char *rgba = malloc(display.frame.tex_size);

  int row;
  for(row = 0; row < height; ++row) {
    const int in_row_ptr = row * pitch;
    const int out_row_ptr = row * display.frame.tex_pitch;

    int col;

    for(col = 0; col < width; ++col) {
      const int cb_diff_ptr = (col & 1) ? -2 : 0;

      float yy = (float)ptr[in_row_ptr + col * 2];
      float cb = (float)ptr[in_row_ptr + col * 2 + cb_diff_ptr + 1];
      float cr = (float)ptr[in_row_ptr + col * 2 + cb_diff_ptr + 3];

      yy /= 255.0f; cb /= 255.0f; cr /= 255.0f;

      float r = (yuv_rgb.r_yy * yy + yuv_rgb.r_cb * cb + yuv_rgb.r_cr * cr + yuv_rgb.r_ct) * 1.0f / yuv_rgb.r_sc;
      float g = (yuv_rgb.g_yy * yy + yuv_rgb.g_cb * cb + yuv_rgb.g_cr * cr + yuv_rgb.g_ct) * 1.0f / yuv_rgb.g_sc;
      float b = (yuv_rgb.b_yy * yy + yuv_rgb.b_cb * cb + yuv_rgb.b_cr * cr + yuv_rgb.b_ct) * 1.0f / yuv_rgb.b_sc;

      if(r < 0.001f) { r = 0.001f; } else if (r > 1.0f) { r = 1.0f; }
      if(g < 0.001f) { g = 0.001f; } else if (g > 1.0f) { g = 1.0f; }
      if(b < 0.001f) { b = 0.001f; } else if (b > 1.0f) { b = 1.0f; }

      r = pow(r, yuv_rgb.gamma);
      g = pow(g, yuv_rgb.gamma);
      b = pow(b, yuv_rgb.gamma);

      int ir = floor(r * 255.0f);
      int ig = floor(g * 255.0f);
      int ib = floor(b * 255.0f);

      if(ir < 0) { ir = 0; } if(ir > 255) { ir = 255; }
      if(ig < 0) { ig = 0; } if(ig > 255) { ig = 255; }
      if(ib < 0) { ib = 0; } if(ib > 255) { ib = 255; }

      rgba[out_row_ptr + col * 4 + 0] = ir;
      rgba[out_row_ptr + col * 4 + 1] = ig;
      rgba[out_row_ptr + col * 4 + 2] = ib;
      rgba[out_row_ptr + col * 4 + 3] = 0xFF;
    }

    for(; col < display.frame.tex_width; ++col) {
      rgba[out_row_ptr + col * 4 + 0] = 0x00;
      rgba[out_row_ptr + col * 4 + 1] = 0x00;
      rgba[out_row_ptr + col * 4 + 2] = 0x00;
      rgba[out_row_ptr + col * 4 + 3] = 0xFF;
    }
  }

  for(; row < display.frame.tex_height; ++row) {
    const int out_row_ptr = row * display.frame.tex_pitch;
    for(int col = 0; col < display.frame.tex_width; ++col) {
      rgba[out_row_ptr + col * 4 + 0] = 0x00;
      rgba[out_row_ptr + col * 4 + 1] = 0x00;
      rgba[out_row_ptr + col * 4 + 2] = 0x00;
      rgba[out_row_ptr + col * 4 + 3] = 0xFF;
    }
  }
  return rgba;
}

void render_test_frame() {
  static const float PI = 3.1415965;
  static double t = 0;
  if(t < 1e-3) t = glfwGetTime();
  double dt = glfwGetTime() - t;

  float r = 0.5f + 0.5f * sin(2.0f * PI * (float)(dt) / 20.0f);

  glClearColor(r, r, r, r);
  glClear(GL_COLOR_BUFFER_BIT);
  glFinish();
  glfwSwapBuffers(display.window.glfw_id);
}

void resize_fb() {
  if(display.window.glfw_id) {
    int width, height;
    glfwGetFramebufferSize(display.window.glfw_id,
			   &width,
			   &height);

    if(width != display.frame.width || height != display.frame.height) {
      debug_f2("[RENDER] Resize fb. to %dx%d\n", width, height);

      if(display.window.fixed_size) {
	width = display.frame.width;
	height = display.frame.height;

	resize_window(width, height); /* neglects display scaling */
	glViewport(0, 0, width, height);
      }
      else {
	glViewport(0, 0, width, height);

	glfwGetWindowSize(display.window.glfw_id, &width, &height);
	display.window.width = width;
	display.window.height = height;
      }
    }
  }
}


void render_frame() {
  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  if(!display.frame.size) {
    debug_f0("[FRAME] empty frame, skipping\n");
    debug_f2("[FRAME] window    size: %dx%d\n",
	     display.window.width, display.window.height);
    debug_f2("[FRAME] frame     size: %dx%d\n",
	     display.frame.width, display.frame.height);
    debug_f2("[FRAME] param. f. size: %dx%d\n",
	     h264_encoder.frame_config.width,
	     h264_encoder.frame_config.height);
    debug_f2("[FRAME] enc.   f. size: %dx%d\n",
	     h264_encoder.params.i_width,
	     h264_encoder.params.i_height);
    return;
  }


  debug_f2("[FRAME] using GLFW: OpenGL version %d.%d\n",
	   glfwGetWindowAttrib(display.window.glfw_id,
			       GLFW_CONTEXT_VERSION_MAJOR),
	   glfwGetWindowAttrib(display.window.glfw_id,
			       GLFW_CONTEXT_VERSION_MINOR));

  debug_f4("[FRAME] input: %d px by %d px, pitch %d B, size %d B\n",
	   display.frame.width, display.frame.height,
	   display.frame.pitch, display.frame.size);

  debug_f4("[FRAME] output: %d px by %d px, pitch %d B, size %d B\n",
	   display.frame.tex_width,
	   display.frame.tex_height,
	   display.frame.tex_pitch,
	   display.frame.tex_size);

  print_messages(&display.queue_packets, "CTRL ");
  print_messages(&display.queue_debug, "DEBUG");
  print_messages(&display.queue_frames, "VIDEO");

  unsigned char *rgba_image;

  if(display.transform.grayscale) {
    debug_f0("[FRAME] Converting YUYV to grayscale RGBA\n");
    rgba_image = yuyv_to_gray(display.frame.dptr,
			      display.frame.pitch,
			      display.frame.width,
			      display.frame.height);
  }
  else {
    debug_f0("[FRAME] Converting YUYV to color RGBA\n");
    rgba_image = yuyv_to_rgba(display.frame.dptr,
			      display.frame.pitch,
			      display.frame.width,
			      display.frame.height);
  }

  const int rgba_pitch = 4 * display.frame.tex_width;

  if(display.transform.flip_v) {
    debug_f0("[FRAME] Flipping frame vertically\n");
    char scanline[rgba_pitch];
    for(int row = 0; row < display.frame.height >> 1; ++row) {
      const int head_row_ptr = rgba_pitch * row;
      const int tail_row_ptr = rgba_pitch * (display.frame.height - 1 - row);
      memcpy(scanline, rgba_image + head_row_ptr, rgba_pitch);
      memcpy(rgba_image + head_row_ptr, rgba_image + tail_row_ptr, rgba_pitch);
      memcpy(rgba_image + tail_row_ptr, scanline, rgba_pitch);
    }
  }

  if(display.transform.flip_h) {
    debug_f0("[FRAME] Flipping frame horizontally\n");
    unsigned int pixel;
    for(int row = 0; row < display.frame.height; ++row) {
      for(int col = 0; col < (display.frame.width >> 1); ++col) {
	const int head_pixel_ptr = rgba_pitch * row + 4 * col;
	const int tail_pixel_ptr = rgba_pitch * row + 4 * (display.frame.width - 1 - col);

	pixel = *(unsigned int*)(rgba_image + head_pixel_ptr);
	*(unsigned int*)(rgba_image + head_pixel_ptr) = *(unsigned int *)(rgba_image + tail_pixel_ptr);
	*(unsigned int*)(rgba_image + tail_pixel_ptr) = pixel;
      }
    }
  }

  if(display.transform.complement) {
    debug_f0("[FRAME] Inverting colors\n");
    unsigned int *rgba_image_end = (unsigned int *)rgba_image + (rgba_pitch * display.frame.width >> 2);
    for(unsigned int *pixel = (unsigned int *)rgba_image; pixel < rgba_image_end; ++pixel) {
      /* memory order is R G B A/X; fourth field is unused */
      *pixel ^= 0xFFFFFFFF;
    }
  }

  blit_rgba_image(rgba_image);
  apply_test_patterns(rgba_image);

  glFlush();
  glFinish();
  glfwSwapBuffers(display.window.glfw_id);
}

void apply_test_patterns(unsigned char *rgba_image) {
  /* fill */
  if(display.test.fill_solid)
  for(int k = 0; k < display.frame.tex_size; k += 4) {
    const int pixel = k;
    rgba_image[pixel + 0] = 0x00;
    rgba_image[pixel + 1] = 0x00;
    rgba_image[pixel + 2] = 0x80;
    rgba_image[pixel + 3] = 0xFF;
  }

  /* fill lower half */
  if(display.test.fill_half)
  for(int k = display.frame.height / 2; k < display.frame.height; ++k) {
    for(int col = 0; col < display.frame.width; ++col) {
      const int pixel = display.frame.tex_pitch * k + 4 * col;
      rgba_image[pixel + 0] = 0x70;
      rgba_image[pixel + 1] = 0x50;
      rgba_image[pixel + 2] = 0x30;
      rgba_image[pixel + 3] = 0xFF;
    }
  }

  /* diagonal line */
  if(display.test.fill_diag)
  for(int k = 0; k < display.frame.tex_width && k < display.frame.tex_height; ++k) {
    const int pixel = 4 * (display.frame.tex_pitch * k + k);
    rgba_image[pixel + 0] = 0x00;
    rgba_image[pixel + 1] = 0xFF;
    rgba_image[pixel + 2] = 0x00;
    rgba_image[pixel + 3] = 0xFF;
  }

  /* horizontal lines */
  if(display.test.fill_horiz)
  for(int k = 0; k < display.frame.tex_width; ++k) {
    const int row0 = 15;
    const int row1 = 63;
    int pixel;
    pixel = (display.frame.tex_pitch * row0) + 4 * k;
    rgba_image[pixel + 0] = 0xFF;
    rgba_image[pixel + 1] = 0xFF;
    rgba_image[pixel + 2] = 0xFF;
    rgba_image[pixel + 3] = 0xFF;
    pixel = (display.frame.tex_pitch * row1) + 4 * k;
    rgba_image[pixel + 0] = 0xFF;
    rgba_image[pixel + 1] = 0xFF;
    rgba_image[pixel + 2] = 0xFF;
    rgba_image[pixel + 3] = 0xFF;
  }

  /* pixels */
  static const unsigned int xycs[] = {
    0x00, 0x00, 0xFF0000FF, 0, /* red pixel */
  };
  if(display.test.fill_pixel)
  for(int k = 0; k < sizeof(xycs) / sizeof(xycs[0]); k += 4) {
    // printf("Pixel %d %d color %08Xh\n", xycs[k + 0], xycs[k + 1], xycs[k + 2]);
    const int px_ptr = xycs[k + 0] * display.frame.width + xycs[k + 1];
    ((unsigned int *)rgba_image)[px_ptr] = xycs[k + 2];
  }

  display.frame.size = 0;

  free(rgba_image);
  rgba_image = NULL;

  release_lock(&display.frame.lock);
}

void blit_rgba_image(unsigned char *ptr) {
  if(cli.frame_capture_path) {
    FILE *img = fopen(cli.frame_capture_path, "w");
    fwrite(ptr, display.frame.tex_size, 1, img);
    fclose(img);
    debug_f4("[GL] dumped image into texture.bin\n"
	     "[GL] W=%d H=%d P=%d L=%d\n",
	     display.frame.tex_width,
	     display.frame.tex_height,
	     display.frame.tex_pitch,
	     display.frame.tex_size);
  }

  static const char *vs_src =
    "#version 460 core\n"
    "precision highp float;\n"
    "layout (location = 0) in vec4 pos;\n"
    "layout (location = 1) in vec2 tex_v;\n"
    "layout (location = 2) in vec4 color_v;\n"
    "out vec2 tex_f;\n"
    "out vec4 color_f;\n"
    "void main() {\n"
    "  gl_Position = pos;\n"
    "  tex_f = tex_v;\n"
    "  color_f = color_v;\n"
    "}\n\0";

  static const char *fs_src =
    "#version 460 core\n"
    "precision highp float;\n"
    "out vec4 out_color;\n"
    "in  vec2 tex_f;\n"
    "in  vec4 color_f;\n"
    "uniform sampler2D txs;\n"
    "void main() {\n"
    "  // out_color = vec4(tex_f, 0.0f, 1.0f);\n"
    "  out_color = texture(txs, tex_f);\n"
    "}\n\0";

  debug_f0("[GL] creating shaders\n");
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vs, 1, &vs_src, NULL);
  glShaderSource(fs, 1, &fs_src, NULL);

  debug_f0("[GL] compiling shaders\n");
  glCompileShader(vs);
  glCompileShader(fs);

  GLint compile_result;
  int compile_log_size;
  char *compile_log_str = NULL;

  compile_result = GL_FALSE;
  glGetShaderiv(vs, GL_COMPILE_STATUS, &compile_result);
  glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &compile_log_size);
  debug_f2("[GL] [SHADER] (V), %d, compilation result %d\n", vs, compile_result);
  if(compile_result != GL_TRUE) {
    compile_log_str = malloc(compile_log_size);
    glGetShaderInfoLog(vs, compile_log_size, NULL, compile_log_str);
    debug_s1("[GL] [SHADER] (V) %s\n", compile_log_str);
    free(compile_log_str);
    glDeleteShader(vs);
    vs = 0;
  }

  compile_result = GL_FALSE;
  glGetShaderiv(fs, GL_COMPILE_STATUS, &compile_result);
  glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &compile_log_size);
  debug_f2("[GL] [SHADER] (F), %d, compilation result %d\n", fs, compile_result);
  if(compile_result != GL_TRUE) {
    compile_log_str = malloc(compile_log_size);
    glGetShaderInfoLog(fs, compile_log_size, NULL, compile_log_str);
    debug_s1("[GL] [SHADER] (F) %s\n", compile_log_str);
    free(compile_log_str);
    glDeleteShader(fs);
    fs = 0;
  }

  if(vs == 0 || fs == 0) {
    return;
  }

  debug_f0("[GL] linking graphics program\n");
  GLuint vf = glCreateProgram();
  glAttachShader(vf, fs);
  glAttachShader(vf, vs);
  glLinkProgram(vf);

  compile_result = GL_FALSE;
  glGetProgramiv(vf, GL_LINK_STATUS, &compile_result);
  glGetProgramiv(vf, GL_INFO_LOG_LENGTH, &compile_log_size);
  debug_f1("[GL] [SHADER] (P) linking result %d\n", compile_result);

  if(compile_result != GL_TRUE) {
    compile_log_str = malloc(compile_log_size);
    glGetProgramInfoLog(vf, compile_log_size, NULL, compile_log_str);
    debug_s1("[GL] [SHADER] (P) %s\n", compile_log_str);

    free(compile_log_str);

    glDetachShader(vf, fs);
    glDetachShader(vf, vs);

    glDeleteShader(fs);
    glDeleteShader(vs);

    glDeleteProgram(vf);
    return;
  }

  /* two triangles */
  const float tex_w = (float)display.frame.width  / display.frame.tex_width;
  const float tex_h = (float)display.frame.height / display.frame.tex_height;

  const float edge = (display.window.enable_border ? 0.8f : 1.0f);
  
  const float square[] = {
    -edge, +edge, 0.0f, +1.0f,  0.0f,  0.0f, 0.0f, 0.0f, +1.0f, +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    +edge, +edge, 0.0f, +1.0f, tex_w,  0.0f, 0.0f, 0.0f, +1.0f, +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    +edge, -edge, 0.0f, +1.0f, tex_w, tex_h, 0.0f, 0.0f, +1.0f, +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    +edge, -edge, 0.0f, +1.0f, tex_w, tex_h, 0.0f, 0.0f, +1.0f, +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    -edge, -edge, 0.0f, +1.0f,  0.0f, tex_h, 0.0f, 0.0f, +1.0f, +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    -edge, +edge, 0.0f, +1.0f,  0.0f,  0.0f, 0.0f, 0.0f, +1.0f, +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
  };
  static const int point_count = 6;

  // debug_f0("[GL] creating framebuffer\n");
  // GLuint fb;
  // glGenFramebuffers(1, &fb);
  // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);
  // debug_f1("[GL] created framebuffer %u\n", fb);

  debug_f0("[GL] creating VAO\n");
  GLuint vao;
  glGenVertexArrays(1, &vao);
  debug_f1("[GL] VAO %d\n", vao);

  debug_f0("[GL] selecting VAO\n");
  glBindVertexArray(vao);

  debug_f0("[GL] creating VBO\n");
  GLuint buf;
  glGenBuffers(1, &buf);

  debug_f0("[GL] binding VBO\n");
  glBindBuffer(GL_ARRAY_BUFFER, buf);

  debug_f0("[GL] filling VBO\n");
  glBufferData(GL_ARRAY_BUFFER, sizeof(square), square, GL_STATIC_DRAW);

  debug_f0("[GL] creating texture\n");
  glEnable(GL_TEXTURE_2D);
  GLuint tex;
  glGenTextures(1, &tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  static float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
  	       display.frame.tex_width, display.frame.tex_height,
  	       0, GL_RGBA, GL_UNSIGNED_BYTE, ptr);
  glGenerateMipmap(GL_TEXTURE_2D);

  debug_f0("[GL] selecting texture\n");
  glActiveTexture(GL_TEXTURE0);

  debug_f0("[GL] setting vertex attrs.\n");
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_TRUE,
			sizeof(square) / point_count,
			(void *)(0 * sizeof(float)));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_TRUE,
			sizeof(square) / point_count,
			(void *)(4 * sizeof(float)));
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_TRUE,
			sizeof(square) / point_count,
			(void *)(8 * sizeof(float)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  debug_f0("[GL] setting uniform\n");
  glUniform1i(glGetUniformLocation(vf, "txs"), 0);

  // debug_f0("[GL] setting drawing mode\n");
  // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  debug_f0("[GL] enabling shader program\n");
  glUseProgram(vf);

  debug_f0("[GL] drawing\n");
  glShadeModel(GL_FLAT);
  glDrawArrays(GL_TRIANGLES, 0, point_count);

  debug_f0("[GL] deselecting VAO\n");
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);

  glBindVertexArray(0);

  debug_f0("[GL] freeing up space\n");
  glUseProgram(0);

  glDetachShader(vf, fs);
  glDetachShader(vf, vs);

  glDeleteShader(fs);
  glDeleteShader(vs);

  glDeleteProgram(vf);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &buf);
  glDeleteVertexArrays(1, &vao);
  // glDeleteFramebuffers(1, &fb);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &tex);

  debug_f0("[GL] end of frame\n");
}

void decode_h264_data(const struct video_msg *cmd) {
  debug_f0("[VIDEO] Got video data "); dump_msg_header(cmd);

  if(display.h264_param.use_h264) {
    h264_decoder.h264_data.output_frames = 0;

    h264_decode_frame(&h264_decoder, cmd->dptr, cmd->size);

    long int total_size = 0;

    const int frame_count = h264_decoder.h264_data.output_frames;

    for(int frame = 0; frame < frame_count; ++frame) {
      debug_f2("[VIDEO] frame %d/%d\n", frame, frame_count);

      struct video_msg msg_copy;
      init_video_msg(&msg_copy);

      msg_copy.oper = VIDEO_CMD_FRAME_YUYV;

      const int frame_size = h264_decoder.h264_data.frame_sizes[frame];
      msg_copy.size = frame_size;
      total_size += frame_size;

      msg_copy.dptr = malloc(frame_size);
      memcpy(msg_copy.dptr, h264_decoder.h264_data.output[frame], frame_size);
      free(h264_decoder.h264_data.output[frame]);
      h264_decoder.h264_data.output[frame] = NULL;

      debug_f2("[VIDEO] Decoded frame %d/%d cloned as:\n", (frame + 1), frame_count);
      dump_msg_header(&msg_copy);
      debug_f3("[VIDEO] Message queued (%d/%d) at %016lX\n",
	       queue_length(&display.queue_packets),
	       display.queue_packets.max_length,
	       (long int)display.queue_packets.tail);

      queue_push(&display.queue_packets, &msg_copy);
    }

    debug_f3("[VIDEO] Decoded packet:"
	     " packed size %d,"
	     " frame count %d,"
	     " frame sizes %ld bytes"
	     "\n",
	     cmd->size,
	     frame_count,
	     total_size);
  }
}

void receive_partial_h264_packet(const struct video_msg *cmd) {
  static unsigned char buffer[stream_h264_buffer_size];
  static int buf_ptr = 0;

  debug_f1("[CAPTURE] Stream length: %d bytes\n", cmd->size);

  long int size;

  struct video_msg temp_msg;

  for(long int start = 0; start < cmd->size;) {
    if(cmd->size - start > display.h264_param.chunk_size) {
      size = display.h264_param.chunk_size;
    }
    else {
      size = cmd->size - start;
    }

    if(h264_encoder.dump_bytes) {
      debug_f1("[CAPTURE] Video packet: %d bytes\n", size);
      dump_array(cmd->dptr, size);
    }

    if(buf_ptr + size < sizeof(buffer)) {
      debug_f2("[CAPTURE] Storing H.264 chunk in buffer (used %d/%d bytes)\n",
	       buf_ptr + size, sizeof(buffer));

      memcpy(buffer + buf_ptr, cmd->dptr, size);
      buf_ptr += size;
    }
    else if(buf_ptr > 0) {
      debug_f2("[CAPTURE] Sending buffer (used %d/%d bytes)\n",
	       buf_ptr, sizeof(buffer));

      unsigned char *temp = malloc(buf_ptr);
      memcpy(temp, buffer, buf_ptr);

      temp_msg.oper = VIDEO_CMD_FRAME_H264;
      temp_msg.dptr = temp;
      temp_msg.size = buf_ptr;

      buf_ptr = size;

      decode_h264_data(&temp_msg);
      free(temp);

      debug_f2("[CAPTURE] Storing chunk in buffer (used %d/%d bytes)\n",
	       size, sizeof(buffer));
      memcpy(buffer, cmd->dptr, size);
    }
    else {
      debug_f1("[CAPTURE] Sending large chunk (%d bytes)\n", size);
      start = cmd->size;
      decode_h264_data(cmd);
      free(cmd->dptr);
    }

    start += size;
  }
}
