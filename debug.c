#include "headers.h"

extern struct display_config display;

struct debug_config debug_cfg;

const int bytes_per_mem_dump_line = 0x10;

const int debug_buffer_size = 8192;


void debug_f4(const char *template,
	      const long int value0, const long int value1,
	      const long int value2, const long int value3) {
  if(!debug_cfg.enable_debug_msgs) return;
  char buf[debug_buffer_size];
  memset(buf, 0, debug_buffer_size);
  snprintf(buf, debug_buffer_size - 1, template, value0, value1, value2, value3);
  debug_f0(buf);
}

void debug_f3(const char *template,
	      const long int value0, const long int value1, const long int value2) {
  if(!debug_cfg.enable_debug_msgs) return;
  char buf[debug_buffer_size];
  memset(buf, 0, debug_buffer_size);
  snprintf(buf, debug_buffer_size - 1, template, value0, value1, value2);
  debug_f0(buf);
}

void debug_f2(const char *template,
	      const long int value0, const long int value1) {
  if(!debug_cfg.enable_debug_msgs) return;
  char buf[debug_buffer_size];
  memset(buf, 0, debug_buffer_size);
  snprintf(buf, debug_buffer_size - 1, template, value0, value1);
  debug_f0(buf);
}

void debug_f1(const char *template,
	      const long int value) {
  if(!debug_cfg.enable_debug_msgs) return;
  char buf[debug_buffer_size];
  memset(buf, 0, debug_buffer_size);
  snprintf(buf, debug_buffer_size - 1, template, value);
  debug_f0(buf);
}

void debug_s1(const char *template,
	      const char *value) {
  if(!debug_cfg.enable_debug_msgs) return;
  char buf[debug_buffer_size];
  memset(buf, 0, debug_buffer_size);
  snprintf(buf, debug_buffer_size - 1, template, value);
  debug_f0(buf);
}

void debug_f0(const char *template) {
  if(!debug_cfg.enable_debug_msgs) return;
  struct video_msg debug_msg;
  debug_msg.oper = VIDEO_CMD_WRITE;
  debug_msg.dptr = malloc(debug_buffer_size);
  memset(debug_msg.dptr, 0, debug_buffer_size);
  snprintf((char *)debug_msg.dptr, debug_buffer_size - 1, template);
  debug_msg.size = strlen(debug_msg.dptr);
  acquire_lock(&debug_cfg.debug_lock);
  if(debug_cfg.show_memory_dump) {
    dump_msg(&debug_msg);
  }
  printf(debug_msg.dptr);
  free(debug_msg.dptr);
  release_lock(&debug_cfg.debug_lock);
}

void dump_msg(const struct video_msg *msg) {
  if(!debug_cfg.enable_debug_msgs) return;
  printf("[DEBUG] cmd(%08X %08X %016lX)\n",
	 msg->oper, msg->size, (const long int)msg->dptr);
  printf("    ");
  for(int b = 0; b < bytes_per_mem_dump_line; ++b) {
    printf(" %02X", b);
  }
  printf("\n");
  for(int ptr = 0; ptr < 0x1000 && ptr < msg->size;
      ptr += bytes_per_mem_dump_line) {
    printf("%04X", ptr);
    for(int b = 0;
	b < bytes_per_mem_dump_line
	  && ptr + b < msg->size;
	++b) {
	printf(" %02X", (const int)*(char *)(msg->dptr + ptr + b));
      }
    printf("\n");
  }
}

void dump_msg_header(const struct video_msg *msg) {
  if(!debug_cfg.enable_debug_msgs) return;
  debug_f3("cmd(%08X %08X %016lX)\n",
	   msg->oper, msg->size, (const long int)msg->dptr);
}

void dump_msg_headers(const struct queue *stk) {
  if(!debug_cfg.enable_debug_msgs) return;
  queue_foreach(stk, dump_msg_header);
}

void dump_queue_sizes() {
  if(!debug_cfg.enable_debug_msgs) return;
  debug_f2("[VIDEO] [video_ctl] queue sizes: "
	   "R %08X, W %08X\n",
	   queue_length(&display.queue_r),
	   queue_length(&display.queue_w));
}
