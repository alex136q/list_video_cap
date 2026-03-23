#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <x264.h>


/* wrapper on top of libx264 */

struct h264_config {
  /* internal */
  x264_t *encoder;
  x264_param_t params;

  struct {
    /* input/output */
    unsigned char *stream;
    long size;
    /* internal */
    x264_nal_t *nals;
    int nal_count;
  } h264_data;

  struct {
    /* input/output */
    int frame_index;
    int width;
    int height;
    long size; /* computed (output) */
    /* internal */
    int luma_length;
    int chroma_b_length;
    int chroma_r_length;
  } frame_config;

  struct {
    unsigned char *packed; /* input/output */
    unsigned char *yvec;
    unsigned char *cbvec;
    unsigned char *crvec;
  } frame_planes;
};


void h264_init_codec(struct h264_config *config,
		       int frame_width,
		       int frame_height);

void h264_free_codec(struct h264_config *config);

void h264_get_headers(struct h264_config *config);

void h264_encode_frame(struct h264_config *config,
		       unsigned char *data);

void extract_array(unsigned char *src,
		   long length,
		   int offset,
		   int stride,
		   unsigned char **dst);


/* functions required for/by the h264_test executable */

#ifndef LISTVIDEOCAP_MAIN

unsigned char sinusoid(unsigned char bias,
		       unsigned char amplitude,
		       float time,
		       float period,
		       float phase);

unsigned char *solid_frame(int width, int height, long *size, float t);

void dump_array(unsigned char *ptr, long size);

void save_array(const unsigned char *raw_yuyv,
		long size,
		const char *filename_yuyv);

void export_luma_as_png(int width, int height,
			const char *filename_yuyv,
			const char *filename_png);

struct {
  int export_images;
  int frame_count;
} h264_cli_config;


void parse_h264_cli_args(int argc, char **argv,
			 struct h264_config *config);

void display_h264_cli_help();

void encode_test_frames(struct h264_config *config);
//void decode_test_frames(struct h264_config *config);

int main(int argc, char **argv) {
  struct h264_config codec;
  h264_init_codec(&codec, 1920, 1080);
  parse_h264_cli_args(argc, argv, &codec);
  encode_test_frames(&codec);
  h264_free_codec(&codec);

  return 0;
}

void encode_test_frames(struct h264_config *config) {
  if(h264_cli_config.export_images) {
    /* ensure the folder exists */
    system("mkdir h264_test_data");
  }

  FILE *nal_out = fopen("nals.bin", "w");

  config->frame_config.frame_index = 0;

  h264_get_headers(config);
  printf("H.264 headers: %d NALs\n", config->h264_data.nal_count);

  long packed_size_sum = 0;

  for(int k = 0; k < config->h264_data.nal_count; ++k) {
    printf("H.264 headers: NAL %d: size %d\n",
	   k, config->h264_data.nals[k].i_payload);

    dump_array(config->h264_data.nals[k].p_payload,
	       config->h264_data.nals[k].i_payload);

    packed_size_sum += config->h264_data.nals[k].i_payload;

    fwrite(config->h264_data.nals[k].p_payload,
	   config->h264_data.nals[k].i_payload,
	   1, nal_out);
  }

  for(int frame = 1; frame <= h264_cli_config.frame_count; ++frame) {

    unsigned char *raw_yuyv =
      solid_frame(config->frame_config.width,
		  config->frame_config.height,
		  &config->frame_config.size,
		  (60.0f * (float)frame / h264_cli_config.frame_count));

    h264_encode_frame(config, raw_yuyv);

    printf("Frame %d: %d NALs\n", frame, config->h264_data.nal_count);
    long nals_size = 0;
    for(int k = 0; k < config->h264_data.nal_count; ++k) {
      int size = config->h264_data.nals[k].i_payload;
      nals_size += size;
      fwrite(config->h264_data.nals[k].p_payload,
	     config->h264_data.nals[k].i_payload, 1, nal_out);
      printf("\tNAL %d size %d bytes\n", k, size);
    }

    printf("Frame %d: NAL total size %ld = %ld bytes\n",
	   frame, config->h264_data.size, nals_size);

    printf("Frame %d/%d: size %ld bytes, streamed size %ld bytes\n",
	   frame, h264_cli_config.frame_count, config->frame_config.size, nals_size);
    packed_size_sum += nals_size;

    if(h264_cli_config.export_images) {
      char filename_yuyv[128];
      char filename_png[128];

      sprintf(filename_yuyv, "h264_test_data/%04d.bin", frame);
      sprintf(filename_png,  "h264_test_data/%04d.png", frame);

      save_array(raw_yuyv, config->frame_config.size, filename_yuyv);
      export_luma_as_png(config->frame_config.width, config->frame_config.height,
			 filename_yuyv, filename_png);
    }

    free(raw_yuyv);
  }

  fclose(nal_out);

  printf("End of stream\n");

  long yuyv_size_sum = (long)h264_cli_config.frame_count * (long)config->frame_config.size;

  printf("Generated frames: %d totalling %ld bytes\n",
	 h264_cli_config.frame_count, yuyv_size_sum);
  printf("Encoded   frames: %d totalling %ld bytes\n",
	 h264_cli_config.frame_count, packed_size_sum);

  printf("Compression ratio: %.3f%%\n",
	 ((float)(100.0 * packed_size_sum) / yuyv_size_sum));
}

unsigned char *solid_frame(int width, int height, long *size, float t) {
  *size = width * height * 2;
  unsigned char *data = malloc(*size);

  const unsigned char Y_ = sinusoid(0x7F, 0x7F, t, 16.0f, 0.000f);
  const unsigned char Cb = sinusoid(0x7F, 0x7F, t, 12.0f, 0.369f);
  const unsigned char Cr = sinusoid(0x7F, 0x7F, t,  7.0f, 0.417f);

  for(int ptr = 0; ptr < *size; ptr += 4) {
    data[ptr + 0] = Y_;
    data[ptr + 1] = Cb;
    data[ptr + 2] = Y_;
    data[ptr + 3] = Cr;
  }

  return data;
}

unsigned char sinusoid(unsigned char bias,
		       unsigned char amplitude,
		       float time,
		       float period,
		       float phase) {

  static const float TWO_PI = 3.1415965f * 2.0f;
  float sin_value = sin(TWO_PI * (time / period + phase / TWO_PI));
  return bias + (unsigned char)((float)amplitude * sin_value);
}

void export_luma_as_png(int width, int height,
		   const char *filename_yuyv, const char *filename_png) {
  char cmd[1024];

  sprintf(cmd, "python3 ./extract_yuyv_y.py %s", filename_yuyv);
  system(cmd);

  sprintf(cmd, "magick -depth 8 -size %dx%d gray:%s.luma png:%s",
	  width, height, filename_yuyv, filename_png);
  system(cmd);
}

void save_array(const unsigned char *raw_yuyv,
		long size,
		const char *filename_yuyv) {
  FILE *fd = fopen(filename_yuyv, "w");
  fwrite(raw_yuyv, 2, (size >> 1), fd);
  fclose(fd);
}

void dump_array(unsigned char *ptr, long size) {
  static const int bytes_per_line = 0x20;
  for(long ptr0 = 0; ptr0 < size; ptr0 += bytes_per_line) {
    long ptr1;
    printf("\t+%04lX ", ptr0);
    for(ptr1 = 0; ptr1 < bytes_per_line && (ptr0 + ptr1 < size); ++ptr1) {
      printf("%02X", ptr[ptr0 + ptr1]);
    }
    printf("\n");
  }
}

void parse_h264_cli_args(int argc, char **argv,
			 struct h264_config *config) {
  h264_cli_config.export_images = 0;
  h264_cli_config.frame_count = 1000;
  config->frame_config.width = 1920;
  config->frame_config.height = 1080;

  for(int arg = 0; arg < argc; ++arg) {
    if(strcmp(argv[arg], "-e") == 0) {
      h264_cli_config.export_images = 1;
    }
    else if(strcmp(argv[arg], "-n") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      h264_cli_config.frame_count = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-w") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config->frame_config.width = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-h") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config->frame_config.height = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "--help") == 0) {
      display_h264_cli_help();
      exit(0);
    }
  }
}

void display_h264_cli_help() {
  printf("Usage:\n"
	 "./h264_test --help\n"
	 "\tDisplays this summary.\n\n"
	 "./h264_test -e\n"
	 "\tEnables export of images to ./h264_test_data/.\n\n"
	 "./h264_test -n <count>\n"
	 "\tSet frame count.\n\n"
	 "./h264_test -w <width>\n"
	 "\tSet frame width.\n\n"
	 "./h264_test -h <height>\n"
	 "\tSet frame height.\n\n"
	 "");
}


#endif


/* definitions for the libx264 wrapper functions */

void extract_array(unsigned char *src,
		   long length,
		   int offset,
		   int stride,
		   unsigned char **dst){
  *dst = malloc(length / stride);
  for(int ptr = offset, out_ptr = 0; ptr < length; ptr += stride, ++out_ptr) {
    (*dst)[out_ptr] = src[ptr];
  }
}

void h264_init_codec(struct h264_config *config,
		       int frame_width,
		       int frame_height) {

  x264_param_default(&config->params);

  /* frame settings */
  config->params.i_threads = 0;
  config->params.i_width = frame_width;
  config->params.i_height = frame_height;
  config->params.i_csp = X264_CSP_YUYV;
  config->params.i_bitdepth = 8;
  config->params.i_level_idc = 9;

  config->frame_config.size = frame_height * frame_width * 2;
  config->frame_config.luma_length = frame_width >> 1;
  config->frame_config.chroma_b_length = frame_width >> 2;
  config->frame_config.chroma_r_length = frame_width >> 2;

  /* video settings */
  config->params.vui.i_colorprim = 2; /* application-defined */
  config->params.vui.i_transfer = 2; /* application-defined */
  config->params.vui.i_colmatrix = 2; /* application-defined */
  config->params.vui.i_chroma_loc = 2; /* 4:2:2 */

  /* adaptive quantization setting */
  // config->params.rc.i_aq_mode = X264_AQ_NONE;

  /* enable libx264 logging */
  config->params.i_log_level = X264_LOG_INFO /* or X264_LOG_DEBUG */;
  /* show end-of-encoding metadata when the decoder gets deallocated */
  config->params.analyse.b_psnr = 1;
  config->params.analyse.b_ssim = 1;

  config->encoder = x264_encoder_open(&config->params);
}

void h264_free_codec(struct h264_config *config) {
  x264_encoder_close(config->encoder);
}

void h264_get_headers(struct h264_config *config) {
  x264_encoder_headers(config->encoder,
		       &config->h264_data.nals,
		       &config->h264_data.nal_count);
}

void h264_encode_frame(struct h264_config *config,
		       unsigned char *data) {
  /* required parameter for x264_encoder_encode */
  x264_picture_t output_frame;
  x264_picture_alloc(&output_frame,
		     X264_CSP_YUYV,
		     config->frame_config.width,
		     config->frame_config.height);

  x264_picture_t input_frame;
  x264_picture_alloc(&input_frame,
		     X264_CSP_YUYV,
		     config->frame_config.width,
		     config->frame_config.height);

  extract_array(data, config->frame_config.size,
		0, 2, &config->frame_planes.yvec);
  extract_array(data, config->frame_config.size,
		1, 4, &config->frame_planes.cbvec);
  extract_array(data, config->frame_config.size,
		3, 4, &config->frame_planes.crvec);

  input_frame.img.i_csp = X264_CSP_YUYV;
  input_frame.img.i_plane = 3;

  input_frame.img.i_stride[0] = config->frame_config.luma_length;
  input_frame.img.i_stride[1] = config->frame_config.chroma_b_length;
  input_frame.img.i_stride[2] = config->frame_config.chroma_r_length;

  input_frame.img.plane[0] = config->frame_planes.yvec;
  input_frame.img.plane[1] = config->frame_planes.cbvec;
  input_frame.img.plane[2] = config->frame_planes.crvec;

  input_frame.i_pts = ++config->frame_config.frame_index;

  config->h264_data.nals = NULL;
  config->h264_data.nal_count = 0;

  x264_encoder_encode(config->encoder,
		      &config->h264_data.nals,
		      &config->h264_data.nal_count,
		      &input_frame,
		      &output_frame);

  long nals_size = 0;
  for(int k = 0; k < config->h264_data.nal_count; ++k) {
    nals_size += config->h264_data.nals[k].i_payload;
  }

  config->h264_data.stream = malloc(nals_size);

  /* libx264 "guarantees the NALs are contiguous in memory"... alas... SEGFAULT! */
  // memcpy(config->h264_data.stream, config->h264_data.nals[0].p_payload, nals_size);

  long ptr = 0;
  for(int k = 0; k < config->h264_data.nal_count; ++k) {
    memcpy(config->h264_data.stream + ptr,
	   config->h264_data.nals[k].p_payload,
	   config->h264_data.nals[k].i_payload);
    ptr += config->h264_data.nals[k].i_payload;
  }
}

