#include "headers.h"

extern struct cli_args cli;
extern struct display_config display;

struct h264_config h264_encoder;


void send_h264_headers(struct h264_config *config);


void traverse_video_device_list() {
  if(cli.video_dev_path != NULL) {
    probe_capab(cli.video_dev_path);
  }
  else {
    DIR* dev_dir_fd = opendir("/dev");
    char temp_file_name[512];
    struct dirent *dir_entry = malloc(sizeof(struct dirent));

    while((dir_entry = readdir(dev_dir_fd))) {
      if(strstr(dir_entry->d_name, "video") == dir_entry->d_name) {
	strcpy(temp_file_name, "/dev/");
	strcat(temp_file_name, dir_entry->d_name);

	probe_capab(temp_file_name);
      }
    }

    free(dir_entry);
    closedir(dev_dir_fd);
  }
}

void probe_capab(const char *dev_path) {
  debug_s1("Using <%s> as the selected video input device...\n", dev_path);

  int video_devfd = open(dev_path, O_RDWR);
  if(video_devfd < 0) {
    debug_f0("Device file could not be opened.");
    return;
  }

  struct v4l2_capability caps;
  ioctl(video_devfd, VIDIOC_QUERYCAP, &caps);
  dump_capab_list(&caps);
  dump_video_inputs(video_devfd);
  
  close(video_devfd);
  debug_f0("\n");
}

void dump_capab_list(const struct v4l2_capability *caps) {
  debug_s1("Device <%s>\n", (const char *)caps->card);
  debug_s1("Driver <%s>\n", (const char *)caps->driver);
  debug_s1("HW path <%s>\n", (const char *)caps->bus_info);

  debug_f0("Physical device capabilities:\n");
  dump_capab_flags(caps->capabilities);
  if(caps->capabilities & 0x80000000) {
    debug_f0("Logical device capabilities:\n");
    dump_capab_flags(caps->device_caps);
  }
}

void dump_capab_flags(unsigned int cap_flags) {
  debug_f1("Capab. <%08Xh>:\n", cap_flags);

  /* Bitfields cf. https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/vidioc-querycap.html#vidioc-querycap */
  if(cap_flags & 0x80000000) debug_f0("\tbit 31 -- has DEVICE_CAPS\n");
  if(cap_flags & 0x40000000) /* unspecified */;
  if(cap_flags & 0x20000000) debug_f0("\tbit 29 -- use the media controller API");
  if(cap_flags & 0x10000000) debug_f0("\tbit 28 -- CAP_TOUCH");
  if(cap_flags & 0x08000000) debug_f0("\tbit 27 -- has output metadata\n");
  if(cap_flags & 0x04000000) debug_f0("\tbit 26 -- supports streaming I/O\n");
  if(cap_flags & 0x02000000) debug_f0("\tbit 25 -- stores EDID\n");
  if(cap_flags & 0x01000000) debug_f0("\tbit 24 -- supports read/write\n");
  if(cap_flags & 0x00800000) debug_f0("\tbit 23 -- has capture metadata\n");
  if(cap_flags & 0x00400000) debug_f0("\tbit 22 -- supports SDR output");
  if(cap_flags & 0x00200000) debug_f0("\tbit 21 -- supports EXT_PIX_FORMAT\n");
  if(cap_flags & 0x00100000) debug_f0("\tbit 20 -- supports SDR input\n");
  if(cap_flags & 0x00080000) debug_f0("\tbit 19 -- has RF output");
  if(cap_flags & 0x00040000) debug_f0("\tbit 18 -- is a radio receiver\n");
  if(cap_flags & 0x00020000) debug_f0("\tbit 17 -- has audio \n");
  if(cap_flags & 0x00010000) debug_f0("\tbit 16 -- has RF capture\n");
  if(cap_flags & 0x00008000) debug_f0("\tbit 15 -- supports single-plane M2M\n");
  if(cap_flags & 0x00004000) debug_f0("\tbit 14 -- multi-plane M2M\n");
  if(cap_flags & 0x00002000) debug_f0("\tbit 13 -- multiple output planes\n");
  if(cap_flags & 0x00001000) debug_f0("\tbit 12 -- multiple input planes\n");
  if(cap_flags & 0x00000800) debug_f0("\tbit 11 -- RDS output");
  if(cap_flags & 0x00000400) debug_f0("\tbit 10 -- HW frequency seeking\n");
  if(cap_flags & 0x00000200) debug_f0("\tbit  9 -- OSD");
  if(cap_flags & 0x00000100) debug_f0("\tbit  8 -- RDS capture");
  if(cap_flags & 0x00000080) debug_f0("\tbit  7 -- sliced VBI output");
  if(cap_flags & 0x00000040) debug_f0("\tbit  6 -- sliced VBI capture");
  if(cap_flags & 0x00000020) debug_f0("\tbit  5 -- VBI output");
  if(cap_flags & 0x00000010) debug_f0("\tbit  4 -- VBI capture");
  if(cap_flags & 0x00000008) debug_f0("\tbit  3 -- ???\n");
  if(cap_flags & 0x00000004) debug_f0("\tbit  2 -- overlays");
  if(cap_flags & 0x00000002) debug_f0("\tbit  1 -- output\n");
  if(cap_flags & 0x00000001) debug_f0("\tbit  0 -- capture\n");
}

void dump_video_inputs(int video_devfd) {
  struct v4l2_input input_s;

  if(cli.video_dev_input >= 0) {
    debug_f1("Using only input %d.\n", cli.video_dev_input);
    if(ioctl(video_devfd, VIDIOC_S_INPUT, &cli.video_dev_input) < 0) return;
    if(ioctl(video_devfd, VIDIOC_ENUMINPUT, &input_s) < 0) return;
    dump_video_input(&input_s);
    dump_video_standards(video_devfd);
  }
  else {
    for(unsigned int input = 0;; ++input) {
      if(ioctl(video_devfd, VIDIOC_S_INPUT, &input) < 0) break;
      if(ioctl(video_devfd, VIDIOC_ENUMINPUT, &input_s) < 0) break;
      dump_video_input(&input_s);
      dump_video_standards(video_devfd);
      dump_image_formats(video_devfd);
    }
  }
}

const char input_type_descs[][8] = {"???", "tuner", "camera", "touch"};

void dump_video_input(const struct v4l2_input *input_s) {
  debug_f1("Input %d", input_s->index);
  debug_s1(" <%s>\n", (const char *)input_s->name);
  debug_f1("Type %d", input_s->type);
  debug_s1(" -- %s\n", input_type_descs[input_s->type]);
  debug_f1("Audio bitfields <%08Xh>\n", input_s->audioset);
}

void dump_video_standards(int video_devfd) {
  debug_f0("TV video standards supported:");
  struct v4l2_standard std_s;
  unsigned int index;
  for(index = 0;; ++index) {
    std_s.index = index;
    if(ioctl(video_devfd, VIDIOC_ENUMSTD, &std_s) < 0) break;
    debug_f2(" (%04Xh %016lXh", std_s.index, std_s.id);
    debug_s1(" \"%s\")", (const char *)std_s.name);
  }
  if(index == 0) debug_f0(" (none)");
  debug_f0("\n");
}

void dump_image_formats(int video_devfd) {
  debug_f0("Image formats supported:\n");

  struct v4l2_fmtdesc format_s;

  for(format_s.index = 0;; ++format_s.index) {
    format_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format_s.mbus_code = 0;
    if(ioctl(video_devfd, VIDIOC_ENUM_FMT, &format_s) < 0) break;
    char px_fmt[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    strncpy(px_fmt, (char*)&format_s.pixelformat, 4);
    debug_f2("\t%04Xh %08Xh", format_s.index, format_s.flags);
    debug_s1(" <%s>", (const char *)px_fmt);
    debug_s1(" <%s>\n", (const char *)format_s.description);
    dump_image_format_flags(format_s.flags);
  }
}

const char image_format_flag_bit_desc[][16] = {
  "compressed"       /* bit  0 */,
  "emulated"         /* bit  1 */,
  "bytestream"       /* bit  2 */,
  "dyn.-resolution"  /* bit  3 */,
  "...(0x00000010)"  /* bit  4 */,
  "set-colorspace"   /* bit  5 */,
  "set-transfer-fn." /* bit  6 */,
  "set-Y'CbCr/HSV"   /* bit  7 */,
  "set-quant."       /* bit  8 */,
  "...(0x00000200)"  /* bit  9 */,
  "???" /* bit 10 */,
  "???" /* bit 11 */,
  "???" /* bit 12 */, "???" /* bit 13 */, "???" /* bit 14 */, "???" /* bit 15 */,
  "???" /* bit 16 */, "???" /* bit 17 */, "???" /* bit 18 */, "???" /* bit 19 */,
  "???" /* bit 20 */, "???" /* bit 21 */, "???" /* bit 22 */, "???" /* bit 23 */,
  "???" /* bit 24 */, "???" /* bit 25 */, "???" /* bit 26 */, "???" /* bit 27 */,
  "???" /* bit 28 */,
  "???" /* bit 29 */,
  "???" /* bit 30 */,
  "enum-all"         /* bit 31 */,
};

void dump_image_format_flags(unsigned int flags) {
  for(int bit = 31; bit >= 0; --bit) {
    if(flags & (1u << bit)) {
      debug_s1("\t\t%s", image_format_flag_bit_desc[bit]);
    }
  }
}

void save_frame() {
  if(cli.video_dev_path == NULL || cli.video_dev_input < 0) {
    debug_f0("Please set the video device path and the index of the input.\n");
    exit(1);
  }

  struct v4l2_format fmt_s;
  fmt_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  /* Choosing this format since it is common to most webcams I own. */
  fmt_s.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  /* Structure (one byte each in memory order):
     (YUYV 4:2:2)
     {Y'0 Cb0 Y'1 Cr0} {Y'2 Cb2 Y'3 Cr2} ...

     Pixel assocs.:
     (Y') (Cb) (Cr)
     0123 0022 0022
   */

  /* These will be adjusted by the video subsystem. */
  fmt_s.fmt.pix.width = display.capture.req_width;
  fmt_s.fmt.pix.height = display.capture.req_width;

  struct v4l2_requestbuffers req_bufs_s;

  req_bufs_s.count = 1;
  req_bufs_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req_bufs_s.memory = V4L2_MEMORY_MMAP;
  req_bufs_s.flags = 0;

  struct v4l2_buffer buf_s;

  int video_devfd = open(cli.video_dev_path, O_RDWR);

  if(ioctl(video_devfd, VIDIOC_S_INPUT, &cli.video_dev_input) < 0) {
    debug_f1("Input does not exist: %d\n", cli.video_dev_input);
    close(video_devfd);
    exit(1);
  }
  else if(ioctl(video_devfd, VIDIOC_S_FMT, &fmt_s) < 0) {
    debug_f0("Could not set video format.\n");
    close(video_devfd);
    exit(1);
  }
  else if(ioctl(video_devfd, VIDIOC_REQBUFS, &req_bufs_s) < 0) {
    debug_f0("Could not request buffers.\n");
    close(video_devfd);
    exit(1);
  }

  if(ioctl(video_devfd, VIDIOC_G_FMT, &fmt_s) < 0) {
    debug_f0("Could not read image format.\n");
    close(video_devfd);
    exit(1);
  }
  dump_capture_image_format(&fmt_s);

  dump_buffer_request_capabs(&req_bufs_s);

  buf_s.index = 0;
  buf_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf_s.memory = V4L2_MEMORY_MMAP;
  buf_s.reserved2 = 0;
  buf_s.request_fd = video_devfd;

  while(ioctl(video_devfd, VIDIOC_QUERYBUF, &buf_s) < 0) {
    debug_f0("Could not query the buffer.\n");
    // close(video_devfd);
    // exit(1);
  }

  const unsigned int buf_base = buf_s.m.offset;
  const unsigned int buf_size = buf_s.length;

  debug_f2("MMAP with:\n"
	 "\t%08Xh size\n"
	 "\t%08Xh offset\n",
	 buf_size,
	 buf_base);

  unsigned char *buf_data_ptr = NULL;
  dump_buffer_metadata(&buf_s, buf_data_ptr);

  buf_data_ptr = (unsigned char *) mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, video_devfd, buf_base);

  debug_f1("MMAP address: %016lXh\n", (unsigned long int)buf_data_ptr);
  if((long int)buf_data_ptr <= 0) {
    debug_f0("Null address.\n");
    close(video_devfd);
    exit(1);
  }

  dump_buffer_metadata(&buf_s, buf_data_ptr);

  if(ioctl(video_devfd, VIDIOC_QBUF, &buf_s) < 0) {
    debug_f0("Could not enque buffer to the driver.\n");
    munmap(buf_data_ptr, buf_size);
    close(video_devfd);
    exit(1);
  }
  else {
    debug_f0("Enqueued buffer...\n");
  }

  int buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  ioctl(video_devfd, VIDIOC_STREAMON, &buf_type);

  wait_for_capture(video_devfd, &buf_s, buf_data_ptr, 0);
  debug_f0("Stopped:\n");
  dump_buffer_metadata(&buf_s, buf_data_ptr);

  ioctl(video_devfd, VIDIOC_DQBUF, &buf_s);
  debug_f0("Unqueued:\n");
  dump_buffer_metadata(&buf_s, buf_data_ptr);

  ioctl(video_devfd, VIDIOC_STREAMOFF, &buf_type);
  debug_f0("Capture ended:\n");
  dump_buffer_metadata(&buf_s, buf_data_ptr);

  /* deinterlace */
  unsigned char y[buf_s.bytesused / 2];
  unsigned char u[buf_s.bytesused / 2];
  unsigned char v[buf_s.bytesused / 2];

  for(int k = 0, t = 0; k < buf_s.bytesused; k += 2, ++t) { y[t] = buf_data_ptr[k]; }
  for(int k = 1, t = 0; k < buf_s.bytesused; k += 4, ++t) { u[t + 1] = u[t] = buf_data_ptr[k]; }
  for(int k = 3, t = 0; k < buf_s.bytesused; k += 4, ++t) { v[t + 1] = v[t] = buf_data_ptr[k]; }

  /* save the grayscale bitmap */
  if(cli.frame_capture_path != NULL) {
    debug_f1("Writing %d bytes", buf_s.bytesused);
    debug_s1(" to file \"%s\"...\n", cli.frame_capture_path);
    FILE *frame_fd = fopen(cli.frame_capture_path, "w");
    fwrite(y, buf_s.bytesused / 2, 1, frame_fd);
    fclose(frame_fd);
  }

  munmap(buf_data_ptr, buf_size);
  close(video_devfd);
}

void dump_capture_image_format(const struct v4l2_format *fmt_s) {
  debug_f4("Image capture:\n"
	 "\t%9d px width\n"
	 "\t%9d px height\n"
	 "\t%9d B  pitch\n"
	 "\t%9d B  buffer size\n",
	 fmt_s->fmt.pix.width,
	 fmt_s->fmt.pix.height,
	 fmt_s->fmt.pix.bytesperline,
	 fmt_s->fmt.pix.sizeimage);
  char fmt_cc[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  strncpy(fmt_cc, (char*)&fmt_s->fmt.pix.pixelformat, 4);
  debug_s1("\t%9s    format\n", fmt_cc);
}

void dump_buffer_request_capabs(const struct v4l2_requestbuffers *req_bufs_s) {
  unsigned int flags = req_bufs_s->capabilities;
  debug_f1("Buffer request capabilities: %08Xh\n", flags);
  if(flags & (1 << 2)) debug_f0("\tDMABUF\n");
  if(flags & (1 << 1)) debug_f0("\tUSRPTR\n");
  if(flags & (1 << 0)) debug_f0("\tMMAP\n");
  if(flags == 0) debug_f0("\t(none)\n");
}

void dump_buffer_metadata(const struct v4l2_buffer *buf_s,
			  const void *buf_data_ptr) {
  debug_f1("Buffer %08Xh:\n", buf_s->index);
  debug_f1("\t%016Xh B  length\n", buf_s->bytesused);
  debug_f1("\t%016Xh B  size allocated\n", buf_s->length);
  debug_f1("\t%016lXh    address\n", (long int)buf_data_ptr);
  debug_f1("\t%16d    frames\n", buf_s->sequence);
  debug_f1("\t%16ld    timestamp (s)\n", buf_s->timestamp.tv_sec);
  debug_f1("\t%16ld    timestamp (us)\n", buf_s->timestamp.tv_usec);

  debug_f1("\tBuffer flags: %08Xh\n", buf_s->flags);
  debug_f1("\t\tprepared: %d\n", ((buf_s->flags & (1 << 10)) != 0));
  debug_f1("\t\tin-req.:  %d\n", ((buf_s->flags & (1 << 7)) != 0));
  debug_f1("\t\terror:    %d\n", ((buf_s->flags & (1 << 6)) != 0));
  debug_f1("\t\tdone:     %d\n", ((buf_s->flags & (1 << 2)) != 0));
  debug_f1("\t\tqueued:   %d\n", ((buf_s->flags & (1 << 1)) != 0));
  debug_f1("\t\tmapped:   %d\n", ((buf_s->flags & (1 << 0)) != 0));
}

void wait_for_capture(int video_devfd,
		      struct v4l2_buffer *buf_s,
		      const void *buf_data_ptr,
		      int dump_info) {
  int retry_count = 0;
  while(1) {
    ++retry_count;
    if(ioctl(video_devfd, VIDIOC_QUERYBUF, buf_s) < 0) {
      debug_f0("[CAPTURE] Could not query the buffer.\n");
      close(video_devfd);
      exit(1);
    }
    debug_f1("[CAPTURE] Check #%d\n", retry_count);
    if(dump_info) dump_buffer_metadata(buf_s, buf_data_ptr);
    if((buf_s->flags & (1 << 1)) == 0) {
      debug_f0("[CAPTURE] Buffer ready.\n");
      break;
    }
    sleep_ms(display.other.frame_delay_ms);
  }
}

void stream_frames() {
  if(cli.video_dev_path == NULL || cli.video_dev_input < 0) {
    debug_f0("Please set the video device path and the index of the input.\n");
    exit(1);
  }

  int prev_width = 0;
  int prev_pitch = 0;
  int prev_height = 0;

  const int buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  int video_devfd = open(cli.video_dev_path, O_RDWR);

  ioctl(video_devfd, VIDIOC_STREAMOFF,  &buf_type);

  struct v4l2_format fmt_s;
  fmt_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt_s.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt_s.fmt.pix.width = display.capture.req_width;
  fmt_s.fmt.pix.height = display.capture.req_width;

  struct v4l2_requestbuffers req_bufs_s;
  req_bufs_s.count = 1;
  req_bufs_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req_bufs_s.memory = V4L2_MEMORY_MMAP;
  req_bufs_s.flags = 0;

  struct v4l2_buffer buf_s;

  if(ioctl(video_devfd, VIDIOC_S_INPUT, &cli.video_dev_input) < 0) {
    debug_f1("Input does not exist: %d\n", cli.video_dev_input);
    close(video_devfd);
    exit(1);
  }
  else if(ioctl(video_devfd, VIDIOC_S_FMT, &fmt_s) < 0) {
    debug_f0("Could not set video format.\n");
    close(video_devfd);
    exit(1);
  }
  else if(ioctl(video_devfd, VIDIOC_REQBUFS, &req_bufs_s) < 0) {
    debug_f0("Could not request buffers.\n");
    close(video_devfd);
    exit(1);
  }

  ioctl(video_devfd, VIDIOC_STREAMON,  &buf_type);

  while(display.window.open || display.window.glfw_id == 0) {
    if(ioctl(video_devfd, VIDIOC_G_FMT, &fmt_s) < 0) {
      debug_f0("Could not read image format.\n");
      close(video_devfd);
      exit(1);
    }

    int frame_width  = fmt_s.fmt.pix.width;
    int frame_height = fmt_s.fmt.pix.height;
    int frame_pitch  = fmt_s.fmt.pix.bytesperline;

    if(frame_width != prev_width) {
      prev_width = frame_width;
      struct video_msg size_msg;
      size_msg.oper = VIDEO_CMD_SET_WIDTH;
      size_msg.size = frame_width;
      size_msg.dptr = NULL;
      video_ctl(size_msg);
    }
    if(frame_pitch != prev_pitch) {
      prev_pitch = frame_pitch;
      struct video_msg size_msg;
      size_msg.oper = VIDEO_CMD_SET_PITCH;
      size_msg.size = frame_pitch;
      size_msg.dptr = NULL;
      video_ctl(size_msg);
    }
    if(frame_height != prev_height) {
      prev_height = frame_height;
      struct video_msg size_msg;
      size_msg.oper = VIDEO_CMD_SET_HEIGHT;
      size_msg.size = frame_height;
      size_msg.dptr = NULL;
      video_ctl(size_msg);
    }

    if(frame_width > 0 && frame_height > 0
       && display.h264_param.use_h264
       && h264_encoder.frame
       && (h264_encoder.frame->width != frame_width
	   || h264_encoder.frame->height != frame_height)) {
      h264_change_encoder_frame_size(&h264_encoder,
				     frame_width, frame_height);
    }

    buf_s.index = 0;
    buf_s.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_s.memory = V4L2_MEMORY_MMAP;
    buf_s.reserved2 = 0;
    buf_s.request_fd = video_devfd;

    if(ioctl(video_devfd, VIDIOC_QUERYBUF, &buf_s) < 0) {
      debug_f0("Could not query the buffer.\n");
      close(video_devfd);
      exit(1);
    }

    const unsigned int buf_base = buf_s.m.offset;
    const unsigned int buf_size = buf_s.length;

    unsigned char *buf_data_ptr = NULL;

    buf_data_ptr = (unsigned char *) mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
					  MAP_SHARED, video_devfd, buf_base);

    if((long int)buf_data_ptr <= 0) {
      debug_f0("[CAPTURE] Unusable address from mmap.\n");
      close(video_devfd);
      exit(1);
    }
    else if(ioctl(video_devfd, VIDIOC_QBUF, &buf_s) < 0) {
      debug_f0("[CAPTURE] Could not enqueue buffer to the driver.\n");
      munmap(buf_data_ptr, buf_size);
      close(video_devfd);
      exit(1);
    }

    wait_for_capture(video_devfd, &buf_s, buf_data_ptr, 0);

    if(buf_s.length) {
      debug_f0("[CAPTURE] Sending frame...\n");
      unsigned char *data_ptr = malloc(buf_s.length);
      memcpy(data_ptr, buf_data_ptr, buf_s.length);
      send_frame(data_ptr, buf_s.length);
    }

    if(ioctl(video_devfd, VIDIOC_DQBUF, &buf_s) < 0) {
      debug_f0("[CAPTURE] Could not deque buffer.\n");
      munmap(buf_data_ptr, buf_size);
      close(video_devfd);
      exit(1);
    }

    munmap(buf_data_ptr, buf_size);
  }

  ioctl(video_devfd, VIDIOC_STREAMOFF, &buf_type);
  close(video_devfd);
}

void send_frame(unsigned char *ptr, const int length)
{
  static int sent_headers = 0;

  if(display.h264_param.use_h264) {
    if(!sent_headers) {
      send_h264_headers(&h264_encoder);
      sent_headers = 1;
    }

    /* send H.264-encoded stream of packets */
    h264_encode_frame(&h264_encoder, ptr);

    if(h264_encoder.h264_data.size) {
      debug_f2("[CAPTURE] NALs of size %08Xh at ptr %016lX\n",
	       h264_encoder.h264_data.size,
	       (long int)h264_encoder.h264_data.stream);

      send_stream(h264_encoder.h264_data.stream,
		  h264_encoder.h264_data.size);

      free(ptr);

      if(h264_encoder.h264_data.stream) {
	free(h264_encoder.h264_data.stream);
      }
    }
    else {
      debug_f0("[CAPTURE] No data to send\n");
    }
  }
  else {
    /* send raw YUYV frame data */
    send_video_packet(VIDEO_CMD_FRAME_YUYV, ptr, length);
  }

  ++display.stat.frames_captured;
}

void send_video_packet(int type, void *ptr, long int length) {
  struct video_msg msg;

  msg.oper = type;
  msg.size = length;
  msg.dptr = ptr;

  video_ctl(msg);
}

void send_stream(unsigned char *ptr, const int length) {
  static unsigned char buffer[4096];
  static int buf_ptr = 0;

  debug_f1("[CAPTURE] Stream length: %d bytes\n", length);

  long int size;

  for(long int start = 0; start < length;) {
    if(length - start > display.h264_param.chunk_size) {
      size = display.h264_param.chunk_size;
    }
    else {
      size = length - start;
    }

    if(h264_encoder.dump_bytes) {
      debug_f1("[CAPTURE] Video packet: %d bytes\n", size);
      dump_array(ptr + start, size);
    }

    if(buf_ptr + size < sizeof(buffer)) {
      debug_f2("[CAPTURE] Storing chunk in buffer (used %d/%d bytes)\n",
	       buf_ptr + size, sizeof(buffer));
      memcpy(buffer + buf_ptr, ptr, size);
      buf_ptr += size;
    }
    else if(buf_ptr > 0) {
      debug_f2("[CAPTURE] Sending buffer (used %d/%d bytes)\n",
	       buf_ptr, sizeof(buffer));
      unsigned char *temp = malloc(buf_ptr);
      memcpy(temp, buffer, buf_ptr);
      send_video_packet(VIDEO_CMD_FRAME_H264, temp, buf_ptr);
      debug_f2("[CAPTURE] Storing chunk in buffer (used %d/%d bytes)\n",
	       size, sizeof(buffer));
      memcpy(buffer, ptr, size);
      buf_ptr = size;
    }
    else {
      debug_f1("[CAPTURE] Sending large chunk (%d bytes)\n", size);
      send_video_packet(VIDEO_CMD_FRAME_H264, ptr, size);
    }

    start += size;
  }
}

void send_h264_headers(struct h264_config *config) {
  h264_get_headers(config);

  long int headers_size = 0;
  for(int nal = 0; nal < h264_encoder.h264_data.nal_count; ++nal) {
    headers_size += h264_encoder.h264_data.nals[nal].i_payload;
  }

  unsigned char *headers_data = malloc(headers_size);

  long int ptr = 0;

  for(int nal = 0; nal < h264_encoder.h264_data.nal_count; ++nal) {
    int header_size = h264_encoder.h264_data.nals[nal].i_payload;

    memcpy(headers_data + ptr,
	   h264_encoder.h264_data.nals[nal].p_payload,
	   header_size);

    ptr += header_size;
  }

  debug_f1("[CAPTURE] Sending H.264 headers: size %ld bytes\n", headers_size);

  send_video_packet(VIDEO_CMD_FRAME_H264, headers_data, headers_size);
}

