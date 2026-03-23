#include "headers.h"

extern struct cli_args cli;

struct display_config display;


void video_ctl(struct video_msg cmd) {
  debug_f0("[VIDEO] [video_ctl] "); dump_msg_header(&cmd);

  struct video_msg msg;
  while(display.queue_w.head != NULL
	&& display.queue_w.head->oper == VIDEO_CMD_WRITE) {
    queue_pop(&display.queue_w, &msg);
    if(msg.dptr) {
      debug_s1("[VIDEO] %s", (char *)msg.dptr);
      free(msg.dptr);
    }
  }

  dump_queue_sizes();

  if(cmd.oper == VIDEO_CMD_OPEN) {
    debug_f0("[VIDEO] opening window\n");
    init_state();
    init_display();
  }
  else if(cmd.oper == VIDEO_CMD_SET_ARGC) {
    /* required by GLUT in the previous iteration */
    debug_f1("[VIDEO] set argc = %d\n", cmd.size);
    display.other.argc = cmd.size;
    display.other.argv = (char **)calloc(sizeof(char *), cmd.size);
  }
  else if(cmd.oper == VIDEO_CMD_SET_ARGV) {
    /* required by GLUT in the previous iteration */
    debug_f1("[VIDEO] set argv[%d]\n", cmd.size);
    display.other.argv[cmd.size] = malloc(strlen(cmd.dptr));
    strcpy(display.other.argv[cmd.size], cmd.dptr);
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
  else {
    debug_f0("[VIDEO] cloning "); dump_msg_header(&cmd);
    struct video_msg msg_copy;
    msg_copy.oper = cmd.oper;
    msg_copy.size = cmd.size;
    if(cmd.size != 0 && cmd.dptr != NULL) {
      msg_copy.dptr = malloc(cmd.size);
      memcpy(msg_copy.dptr, cmd.dptr, cmd.size);
    }
    else {
      msg_copy.dptr = NULL;
    }
    debug_f0("[VIDEO] cloned as "); dump_msg_header(&msg_copy);
    debug_f2("[VIDEO] message queued (%d/%d)\n",
	     queue_length(&display.queue_r),
	     display.queue_r.max_length);
    queue_push(&display.queue_r, &msg_copy);
    print_messages(&display.queue_r, "R");
    print_messages(&display.queue_w, "W");
  }
}

void toggle_graphics(int init) {
  struct video_msg video_openclose;
  memset(&video_openclose, 0, sizeof(struct video_msg));
  video_openclose.oper = init ? VIDEO_CMD_OPEN : VIDEO_CMD_CLOSE;
  video_ctl(video_openclose);
}

void print_messages(struct queue *stk, const char *debug_label) {
  long int now_t = time(NULL);
  char now[256];
  now[0] = 0;
  strcpy(now, ctime(&now_t));
  if(now[0] == 0) {
    sprintf(now, "%ld", now_t);
  }
  else {
    now[strlen(now) - 1] = 0;
  }

  debug_s1("[VIDEO] %s", debug_label);
  debug_s1(" TS %s ", now);
  debug_f4("[VIDEO] queue sizes: R%1d %08X, W%1d %08X\n",
	   display.queue_r.lock, queue_length(&display.queue_r),
	   display.queue_w.lock, queue_length(&display.queue_w));

  dump_msg_headers(stk);
}

void init_state() {
  display.window.title_string = "Framerate";
  display.window.open = 0;
  display.window.frame = NULL;

  init_queue(&display.queue_r);
  init_queue(&display.queue_w);

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

  char window_title[128];

  sprintf(window_title,
	  "Stream: %6.2lf FPSr (%ld), %6.2lf FPSc (%ld), %.2lf s, "
	  "Win. %dx%d, Queues: R %d/%d W %d/%d",
	  display.stat.render_frame_rate_fps,
	  display.stat.frames_rendered,
	  display.stat.capture_frame_rate_fps,
	  display.stat.frames_captured,
	  display.stat.time_elapsed_ms / 1000.0,
	  display.window.width,
	  display.window.height,
	  queue_length(&display.queue_r),
	  display.queue_r.max_length,
	  queue_length(&display.queue_w),
	  display.queue_w.max_length);

  glfwSetWindowTitle(display.window.glfw_id, window_title);
}

void set_image(const struct video_msg *msg) {
  if(msg == NULL) {
    debug_f0("[FRAME] null message pointer\n");
    return;
  }

  debug_f1("[FRAME] frame: size %d, ", msg->size);
  dump_msg_header(msg);

  acquire_lock(&display.frame.lock);
  display.frame.size = msg->size;
  if(display.frame.dptr) free(display.frame.dptr);
  display.frame.dptr = malloc(msg->size);
  memcpy(display.frame.dptr, msg->dptr, msg->size);
  release_lock(&display.frame.lock);

  ++display.stat.frames_captured; update_frame_stats();
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
  resize_fb();
  rename_window();
  queue_purge(&display.queue_r, process_cmd);
}

int process_cmd(const struct video_msg *cmd) {
    debug_f3("[RENDER] received cmd(%08lX %08lX %016lX)\n",
	     cmd->oper, cmd->size, (long int) cmd->dptr);

    switch(cmd->oper) {

    case VIDEO_CMD_CLOSE:
      debug_f0("[RENDER] closing\n");
      return 1;

    case VIDEO_CMD_FRAME:
      set_image(cmd);
      return 1;

    case VIDEO_CMD_SET_WIDTH:
      resize_window(cmd->size, display.window.height);
      return 1;

    case VIDEO_CMD_SET_HEIGHT:
      resize_window(display.window.width, cmd->size);
      return 1;

    case VIDEO_CMD_SET_PITCH:
      display.window.pitch = cmd->size;
      return 1;

    default:
      debug_f1("[RENDER] Unknown cmd %d\n", cmd->oper);
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

  debug_f1("[RENDER] [MAIN] Created window: %016X\n",
	   (long int)display.window.glfw_id);
  debug_f0("[RENDER] [MAIN] GLFW main loop.\n");

  while(display.window.open) {
    debug_f0("[RENDER] checking for messages to display...\n");
    process_cmds();

    if(!display.other.test) {
      debug_f0("[RENDER] live frame\n");
      render_frame();
    }
    else {
      render_test_frame();
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

void clear_msg(struct video_msg *msg) {
  if(msg->dptr != NULL && msg->size != 0) {
    debug_f1("[RENDER] Cmd. oper. %016X", msg->oper);
    debug_f1("[RENDER] Cmd. size  %016X", msg->size);
    debug_f1("[RENDER] Cmd. ptr.  %016X", (long int)msg->dptr);
    msg->oper = VIDEO_CMD_WRITE;
    if(msg->size != 0 && msg->dptr != NULL) {
      if(display.window.frame != NULL) free(display.window.frame);
      debug_f1("[RENDER] allocating space for frame at %016X\n",
	       (long int)msg->dptr);
      display.window.frame = malloc(msg->size);
      memcpy(display.window.frame, msg->dptr, msg->size);
      free(msg->dptr);
      msg->dptr = NULL;
      msg->size = 0;
    }
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
  glfwPollEvents();

  ++display.stat.frames_rendered; update_frame_stats();
  if(display.other.frame_delay_ms > 0) {
    sleep_ms(display.other.frame_delay_ms);
  }
}

void resize_fb() {
  if(display.window.glfw_id) {
    acquire_lock(&display.frame.lock);

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

    release_lock(&display.frame.lock);
  }
}


void render_frame() {
  if(!display.frame.size) {
    debug_f0("[FRAME] empty frame, skipping\n");
    return;
  }

  acquire_lock(&display.frame.lock);

  debug_f2("[FRAME] using GLFW: OpenGL version %d.%d\n",
	   glfwGetWindowAttrib(display.window.glfw_id,
			       GLFW_CONTEXT_VERSION_MAJOR),
	   glfwGetWindowAttrib(display.window.glfw_id,
			       GLFW_CONTEXT_VERSION_MINOR));

  debug_f4("[FRAME] input: %d px by %d px, pitch %d B, size %d B\n",
	   display.frame.width, display.frame.height,
	   display.frame.pitch, display.frame.size);

  unsigned char *grayscale_image = yuyv_to_gray(display.frame.dptr,
						display.frame.pitch,
						display.frame.width,
						display.frame.height);

  /* fill */
  if(display.test.fill_solid)
  for(int k = 0; k < display.frame.tex_size; k += 4) {
    const int pixel = k;
    grayscale_image[pixel + 0] = 0x00;
    grayscale_image[pixel + 1] = 0x00;
    grayscale_image[pixel + 2] = 0x80;
    grayscale_image[pixel + 3] = 0xFF;
  }

  /* fill lower half */
  if(display.test.fill_half)
  for(int k = display.frame.height / 2; k < display.frame.height; ++k) {
    for(int col = 0; col < display.frame.width; ++col) {
      const int pixel = display.frame.tex_pitch * k + 4 * col;
      grayscale_image[pixel + 0] = 0x70;
      grayscale_image[pixel + 1] = 0x50;
      grayscale_image[pixel + 2] = 0x30;
      grayscale_image[pixel + 3] = 0xFF;
    }
  }

  /* diagonal line */
  if(display.test.fill_diag)
  for(int k = 0; k < display.frame.tex_width && k < display.frame.tex_height; ++k) {
    const int pixel = 4 * (display.frame.tex_pitch * k + k);
    grayscale_image[pixel + 0] = 0x00;
    grayscale_image[pixel + 1] = 0xFF;
    grayscale_image[pixel + 2] = 0x00;
    grayscale_image[pixel + 3] = 0xFF;
  }

  /* horizontal lines */
  if(display.test.fill_horiz)
  for(int k = 0; k < display.frame.tex_width; ++k) {
    const int row0 = 15;
    const int row1 = 63;
    int pixel;
    pixel = (display.frame.tex_pitch * row0) + 4 * k;
    grayscale_image[pixel + 0] = 0xFF;
    grayscale_image[pixel + 1] = 0xFF;
    grayscale_image[pixel + 2] = 0xFF;
    grayscale_image[pixel + 3] = 0xFF;
    pixel = (display.frame.tex_pitch * row1) + 4 * k;
    grayscale_image[pixel + 0] = 0xFF;
    grayscale_image[pixel + 1] = 0xFF;
    grayscale_image[pixel + 2] = 0xFF;
    grayscale_image[pixel + 3] = 0xFF;
  }

  /* pixels */
  static const unsigned int xycs[] = {
    0x00, 0x00, 0xFF0000FF, 0, /* red pixel */
  };
  if(display.test.fill_pixel)
  for(int k = 0; k < sizeof(xycs) / sizeof(xycs[0]); k += 4) {
    // printf("Pixel %d %d color %08X\n", xycs[k + 0], xycs[k + 1], xycs[k + 2]);
    const int px_ptr = xycs[k + 0] * display.frame.width + xycs[k + 1];
    ((unsigned int *)grayscale_image)[px_ptr] = xycs[k + 2];
  }

  debug_f4("[FRAME] output: %d px by %d px, pitch %d B, size %d B\n",
	   display.frame.tex_width,
	   display.frame.tex_height,
	   display.frame.tex_pitch,
	   display.frame.tex_size);

  print_messages(&display.queue_r, "R");
  print_messages(&display.queue_w, "W");

  if(grayscale_image) {
    blit_rgba_image(grayscale_image);
    glFlush();
    glFinish();
    glfwSwapBuffers(display.window.glfw_id);

    free(grayscale_image);
    grayscale_image = NULL;
  }

  display.frame.size = 0;
  release_lock(&display.frame.lock);

  glfwPollEvents();
  if(glfwWindowShouldClose(display.window.glfw_id)) {
    display.window.open = 0;
  }
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

  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

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

