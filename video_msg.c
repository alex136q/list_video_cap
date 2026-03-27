#include "headers.h"


void delete_video_msg(struct video_msg **msg) {
  if((*msg)->dptr) free((*msg)->dptr);
  free(*msg);
  *msg = NULL;
}

struct video_msg *alloc_video_msg() {
  struct video_msg *msg = malloc(sizeof(struct video_msg));
  init_video_msg(msg);
  return msg;
}

void init_video_msg(struct video_msg *msg) {
  static long int id = 0;
  msg->id = ++id;
}

