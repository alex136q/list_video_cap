#ifndef INCLUDE_VIDEO_MSG_H
#define INCLUDE_VIDEO_MSG_H

struct video_msg {
  int oper;
  int size;
  long int id;
  void *dptr;
  struct video_msg *next;
  struct video_msg *prev;
};

struct video_msg *alloc_video_msg();
void init_video_msg(struct video_msg *msg);
void delete_video_msg(struct video_msg **msg);

#endif
