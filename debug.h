#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

struct debug_config {
  int enable_debug_msgs;
  int show_memory_dump;
  int debug_lock;
};

void debug_f4(const char *template,
	      const long int value0,
	      const long int value1,
	      const long int value2,
	      const long int value3);

void debug_f3(const char *template,
	      const long int value0,
	      const long int value1,
	      const long int value2);

void debug_f2(const char *template,
	      const long int value0,
	      const long int value1);

void debug_f1(const char *template,
	      const long int value);

void debug_s1(const char *template,
	      const char *value);

void debug_f0(const char *template);

void dump_msg(const struct video_msg *msg);
void dump_msg_header(const struct video_msg *msg);
void dump_msg_headers(const struct queue *stk);

void dump_queue_sizes();

#endif
