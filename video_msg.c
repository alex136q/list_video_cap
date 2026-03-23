#include "headers.h"


void delete_video_msg(struct video_msg **msg) {
  if((*msg)->dptr) free((*msg)->dptr);
  free(*msg);
  *msg = NULL;
}


