#ifndef INCLUDE_RECORD_H
#define INCLUDE_RECORD_H

#include <linux/videodev2.h>


void traverse_video_device_list();
void probe_capab(const char *dev_path);

void dump_capab_list(const struct v4l2_capability *caps);
void dump_capab_flags(unsigned int cap_flags);
void dump_image_format_flags(unsigned int flags);

void dump_video_inputs(int video_devfd);
void dump_video_input(const struct v4l2_input *input_s);
void dump_video_standards(int video_devfd);
void dump_image_formats(int video_devfd);
void dump_capture_image_format(const struct v4l2_format *fmt_s);
void dump_buffer_request_capabs(const struct v4l2_requestbuffers *req_bufs_s);
void dump_buffer_metadata(const struct v4l2_buffer *buf_s,
			  const void *buf_data_ptr);

void save_frame();

void wait_for_capture(int video_devfd,
		      struct v4l2_buffer *buf_s,
		      const void* buf_data_ptr,
		      int dump_info);

void stream_frames();

void stream_file();

void send_frame(unsigned char *ptr, const int length);
void send_video_packet(int type, void *ptr, long int size);

#endif
