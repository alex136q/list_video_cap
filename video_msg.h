#ifndef INCLUDE_VIDEO_MSG_H
#define INCLUDE_VIDEO_MSG_H

struct video_msg {
  int oper;
  int size;
  void *dptr;
  struct video_msg *next;
  struct video_msg *prev;
};

void delete_video_msg(struct video_msg **msg);

#endif
