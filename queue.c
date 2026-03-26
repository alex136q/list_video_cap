#include "headers.h"


void init_queue(struct queue *stk) {
  stk->lock = 1;
  stk->head = NULL;
  stk->tail = NULL;
  stk->lock = 0;
  stk->max_length = 0x10;
}

int queue_empty(struct queue *stk) {
  return (stk->head == NULL || stk->tail == NULL);
}

void queue_push(struct queue *stk, struct video_msg *data) {
  while(queue_length(stk) >= stk->max_length);
  acquire_lock(&stk->lock);
  struct video_msg *last = malloc(sizeof(*data));
  *last = *data;
  if(stk->tail) {
    stk->tail->next = last;
    last->prev = stk->tail;
  }
  else {
    stk->head = last;
    last->prev = NULL;
  }
  last->next = NULL;
  stk->tail = last;
  release_lock(&stk->lock);
}

void queue_push_wait(struct queue *stk, struct video_msg *data,
		     unsigned int wait_ms) {
  while(queue_length(stk) >= stk->max_length) {
    sleep_ms(wait_ms);
  }
  queue_push(stk, data);
}

void queue_pop(struct queue *stk, struct video_msg *data) {
  acquire_lock(&stk->lock);
  struct video_msg *head;
  if(stk->head) {
    head = stk->head;
    stk->head = stk->head->next;
    if(stk->head) {
      stk->head->prev = NULL;
    }
    else {
      stk->tail = NULL;
    }
    head->next = NULL;
    *data = *head;
    free(head);
  }
  else {
    memset(data, 0, sizeof(*data));
  }
  release_lock(&stk->lock);
}

void queue_foreach(const struct queue *stk,
		   void (*func)(const struct video_msg *)) {
  for(struct video_msg *msg = stk->head; msg != NULL; msg = msg->next) {
    func(msg);
  }
}

struct queue *queue_map(struct queue *stk,
			struct video_msg *(*func)(const struct video_msg *)) {
  struct queue *msgs = malloc(sizeof(*stk));
  init_queue(msgs);
  for(struct video_msg *msg = stk->head; msg != NULL; msg = msg->next) {
    queue_push(msgs, func(msg));
  }
  return msgs;
}

void queue_filter(struct queue *stk,
		  int (*keep)(const struct video_msg *)) {
  for(struct video_msg *msg = stk->head; msg != NULL;) {
    struct video_msg *next = msg->next;
    if(!keep(msg)) {
      if(msg->prev) msg->prev->next = msg->next;
      if(msg->next) msg->next->prev = msg->prev;
      delete_video_msg(&msg);
    }
    msg = next;
  }
}

void queue_purge(struct queue *stk,
		 int (*purge)(const struct video_msg *)) {
  acquire_lock(&stk->lock);
  for(struct video_msg *msg = stk->head; msg != NULL;) {
    struct video_msg *next = msg->next;
    if(purge(msg)) {
      if(msg->prev) msg->prev->next = msg->next;
      if(msg->next) msg->next->prev = msg->prev;
      if(msg->prev == NULL) stk->head = next;
      if(msg->next == NULL) stk->tail = NULL;
      delete_video_msg(&msg);
    }
    msg = next;
  }
  release_lock(&stk->lock);
}

void queue_dump(struct queue *stk) {
  queue_foreach(stk, dump_msg);
}

void queue_reverse(struct queue *stk) {
  struct video_msg *next_left, *next_right;
  for(struct video_msg *left = stk->head, *right = stk->tail;
      left != NULL && right != NULL;
      left = left->next, right = right->prev) {
    next_left = left->next;
    next_right = right->prev;
    left->next = left->prev;
    left->prev = next_left;
    if(left != right) {
      right->prev = right->next;
      right->next = next_right;
    }
    else {
      break;
    }
  }
  next_right = stk->head;
  next_left = stk->tail;
  stk->head = next_left;
  stk->tail = next_right;
}

int queue_length(struct queue *stk) {
  acquire_lock(&stk->lock);
  int size = 0;
  for(struct video_msg *msg = stk->head; msg != NULL; msg = msg->next) ++size;
  release_lock(&stk->lock);
  return size;
}

