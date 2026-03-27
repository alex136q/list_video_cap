#ifndef INCLUDE_CLI_H
#define INCLUDE_CLI_H

struct cli_args {
  int argc;
  char **argv;
  char *cmd;
  char *video_dev_path;
  int video_dev_input;
  char *frame_capture_path;
  char *frame_early_capture_path;
  int needs_graphics;
};


int main(int argc, char **argv);


void populate_cli_arguments(int argc, char **argv);
void show_cli_error_text(int arg);
void handle_cli_cmd();
void show_help_text();

#endif
