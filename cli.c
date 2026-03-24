#include "headers.h"
#include "cli.h"

extern struct debug_config debug_cfg;
extern struct display_config display;

extern struct h264_config h264_encoder;

struct cli_args cli;


int main(int argc, char **argv) {
  populate_cli_arguments(argc, argv);
  handle_cli_cmd();
  return 0;
}


void handle_cli_cmd() {
  if(cli.cmd == NULL || strcmp(cli.cmd, "help") == 0) {
    debug_cfg.enable_debug_msgs = 1;    
    show_help_text();
  }
  else if(strcmp(cli.cmd, "list") == 0) {
    debug_cfg.enable_debug_msgs = 1;    
    traverse_video_device_list();
  }
  else if(strcmp(cli.cmd, "save") == 0) {
    toggle_graphics(1);
    save_frame();
    toggle_graphics(0);
  }
  else if(strcmp(cli.cmd, "watch") == 0) {
    struct video_msg argv_msg;
    argv_msg.oper = VIDEO_CMD_SET_ARGC; argv_msg.size = 1;
    argv_msg.dptr = NULL; video_ctl(argv_msg);
    for(int arg = 0; arg < 1; ++arg) {
      debug_f1("Passing argv[%d]", arg);
      debug_s1(" \"%s\"...\n", cli.argv[arg]);
      argv_msg.oper = VIDEO_CMD_SET_ARGV; argv_msg.size = arg;
      argv_msg.dptr = cli.argv[arg]; video_ctl(argv_msg);
    }
    toggle_graphics(1);
    stream_frames();
    toggle_graphics(0);
  }
  else {
    show_cli_error_text(0);
  }
}

void show_help_text() {
  debug_f0("Usage: ./list_video_cap <COMMAND> <ARG>...\n\n"
	   "Commands:\n"
	   "\n./list_video_cap help\n\tShows this text.\n"
	   "\n./list_video_cap list\n\tLists the capabilities of available video devices.\n"
	   "\n./list_video_cap list -d <path>\n\tLists the capabilities of the specified device.\n"
	   "\n./list_video_cap list -i <integer>\n\tLists the capabilities of the given capture input for all devices.\n"
	   "\n./list_video_cap list -d <path> -i <integer>\n\tLists only the capabilities of the given input and device.\n"
	   "\n./list_video_cap save -d <path> -i <integer> -o <path>\n\tSave a single frame as grayscale (Y' component).\n"
	   "\n./list_video_cap watch -d <path> -i <integer>\n\tOpens a window displaying the camera feed.\n"
	   "\n"
	   "Flags:\n"
	   "\n-c\n\tFallback to raw frame data channel. The absence of this flag causes the application to stream H.264-encoded packets.\n"
	   "\n-k <SIZE>\n\tSize of the H.264 packets to be streamed to the graphics thread.\n"
	   "\n-f <FPS>\n\tLimit rendering frame rate to <FPS>.\n"
	   "\n-v\n\tEnable debug messages.\n"
	   "\n-b\n\tEnable H.264 packet byte dumps (implies -v).\n"
	   "\n-s <width> <height>\n\tVideo capture frame and window size hints for the V4L2 driver.\n"
	   "\n-R\n\tAllow window to be resized by the user.\n"
	   "\n-b\n\tShow a border around the video feed.\n"
	   "\n-m\n\tShow memory dumps in debug messages.\n"
	   "\n-t\n\tTest OpenGL by rendering dummy frames.\n"
	   "\n-F <type>\n\tOverlay solid color patterns over the captured frame (can be combined):\n"
	   "\t\t-F full\t\tFill whole frame.\n"
	   "\t\t-F half\t\tFill lower half of frame.\n"
	   "\t\t-F pixel\tColor top left pixel.\n"
	   "\t\t-F diag\t\tFill main diagonal.\n"
	   "\t\t-F horiz\tFill two lines.\n");
}

void populate_cli_arguments(int argc, char **argv) {
  cli.argc = argc;
  cli.argv = argv;
  cli.cmd = NULL;
  cli.video_dev_path = NULL;
  cli.video_dev_input = -1;
  cli.frame_capture_path = NULL;

  if(argc > 1) {
    cli.cmd = argv[1];
  }

  debug_cfg.show_memory_dump = 0;

  display.other.frame_delay_ms = 20;
  display.other.test = 0;

  display.test.fill_solid = 0;
  display.test.fill_half = 0;
  display.test.fill_pixel = 0;
  display.test.fill_diag = 0;
  display.test.fill_horiz = 0;

  display.window.fixed_size = 1;
  display.window.enable_border = 0;

  display.window.width = display.capture.req_width = 800;
  display.window.height = display.capture.req_height = 600;

  display.h264_param.use_h264 = 1;
  display.h264_param.chunk_size = 4096;

  for(int arg = 2; arg < argc; ++arg) {
    if(strcmp(argv[arg], "-d") == 0) {
      cli.video_dev_path = argv[++arg];
    }
    else if(strcmp(argv[arg], "-o") == 0) {
      cli.frame_capture_path = argv[++arg];
    }
    else if(strcmp(argv[arg], "-i") == 0) {
      cli.video_dev_input = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-m") == 0) {
      debug_cfg.show_memory_dump = 1;
    }
    else if(strcmp(argv[arg], "-v") == 0) {
      debug_cfg.enable_debug_msgs = 1;
      h264_encoder.debug_info = 1;
    }
    else if(strcmp(argv[arg], "-b") == 0) {
      debug_cfg.enable_debug_msgs = 1;
      h264_encoder.debug_info = 1;
      h264_encoder.dump_bytes = 1;
    }
    else if(strcmp(argv[arg], "-f") == 0 && argc > arg) {
      const int freq_hz = atoi(argv[++arg]); /* max. FPS */
      if(freq_hz == 0) {
	display.other.frame_delay_ms = 0;
      }
      else {
	display.other.frame_delay_ms = 1000 / freq_hz;
      }
    }
    else if(strcmp(argv[arg], "-t") == 0) {
      display.other.test = 1;
    }
    else if(strcmp(argv[arg], "-F") == 0 && argc > arg) {
      ++arg;
      if(strcmp(argv[arg], "full") == 0)
	display.test.fill_solid = 1;
      else if(strcmp(argv[arg], "half") == 0)
	display.test.fill_half = 1;
      else if(strcmp(argv[arg], "pixel") == 0)
	display.test.fill_pixel = 1;
      else if(strcmp(argv[arg], "diag") == 0)
	display.test.fill_diag = 1;
      else if(strcmp(argv[arg], "horiz") == 0)
	display.test.fill_horiz = 1;
      else {
	show_cli_error_text(arg);
	exit(1);
      }
    }
    else if(strcmp(argv[arg], "-R") == 0) {
      display.window.fixed_size = 0;
    }
    else if(strcmp(argv[arg], "-b") == 0) {
      display.window.enable_border = 1;
    }
    else if(strcmp(argv[arg], "-s") == 0 && argc > arg + 1) {
      display.window.width = display.capture.req_width = atoi(argv[++arg]);
      display.window.height = display.capture.req_height = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-b") == 0) {
      display.window.enable_border = 1;
    }
    else if(strcmp(argv[arg], "-c") == 0) {
      display.h264_param.use_h264 = 0;
    }
    else if(strcmp(argv[arg], "-k") == 0 && argc > arg) {
      display.h264_param.chunk_size = atoi(argv[++arg]);
    }
    else {
      show_cli_error_text(arg);
    }
  }

  h264_init_encoder(&h264_encoder, display.capture.req_width, display.capture.req_height);
}

void show_cli_error_text(int arg) {
  debug_cfg.enable_debug_msgs = 1;
  if(arg > 0) {
    debug_s1("Unknown flag or argument: %s\n", cli.argv[arg]);
  }
  debug_s1("Run \"%s help\" for a list of supported commands.\n", cli.argv[0]);
  exit(1);
}

