#ifndef INCLUDE_CLI_H
#define INCLUDE_CLI_H

struct cli_args {
  char *cmd;
  char *video_dev_path;
  int video_dev_input;
  char *frame_capture_path;
  char *frame_early_capture_path;
  char *frame_replay_path;
};

int main(int argc, char **argv);

void populate_cli_arguments(int argc, char **argv);
void show_cli_error_text(int arg, int argc, char **argv);
void handle_cli_cmd();
void show_help_text();

#endif
