#ifndef INCLUDE_QUEUE_H
#define INCLUDE_QUEUE_H

#include "video_msg.h"

struct queue {
  struct video_msg *head;
  struct video_msg *tail;
  int lock;
  int max_length;
  int length;
};

void init_queue(struct queue *stk);

void queue_push(struct queue *stk, struct video_msg *data);
void queue_push_unsafe(struct queue *stk, struct video_msg *data);
void queue_push_wait(struct queue *stk, struct video_msg *data, unsigned int wait_ms);

void queue_pop(struct queue *stk, struct video_msg *data);

void queue_foreach(const struct queue *stk,
		   void (*func)(const struct video_msg *));
struct queue *queue_map(struct queue *stk,
			struct video_msg *(*func)(const struct video_msg *));

void queue_filter(struct queue *stk, int (*keep)(const struct video_msg *));

void queue_purge(struct queue *stk, int (*purge)(const struct video_msg *));

void queue_purge_all_but_last(struct queue *stk,
			      int (*purge)(const struct video_msg *));

void queue_dump(struct queue *stk);

void queue_reverse(struct queue *stk);

int queue_length(struct queue *stk);

#endif
