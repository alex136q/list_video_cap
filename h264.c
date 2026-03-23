#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <x264.h>

struct {
  x264_t *encoder;
  x264_param_t params;
  x264_nal_t *nals;
  int nal_count;
} stream;

unsigned char sinusoid(unsigned char bias,
		       unsigned char amplitude,
		       float time,
		       float period,
		       float phase);

unsigned char *solid_frame(int width, int height, int *size, float t);


struct {
  long int time_ms;
  unsigned char *yvec;
  unsigned char *cbvec;
  unsigned char *crvec;
  int export_images;
  int frame_count;
  int frame_width;
  int frame_height;
} config;


long int now_ms();

void dump_array(unsigned char *ptr, int size);

void save_array(const unsigned char *raw_yuyv,
		int size,
		const char *filename_yuyv);

void export_luma_as_png(int width, int height,
			const char *filename_yuyv, const char *filename_png);

void extract_array(unsigned char *src,
		   int length,
		   int offset,
		   int stride,
		   unsigned char **dst);

void parse_h264_cli_args(int argc, char **argv);
void display_h264_cli_help();


int main(int argc, char **argv) {
  parse_h264_cli_args(argc, argv);

  config.time_ms = now_ms();

  /* initialize libx264 encoder and its parameters */

  x264_param_default(&stream.params);

  stream.params.i_threads = 0;
  stream.params.i_width = config.frame_width;
  stream.params.i_height = config.frame_height;
  stream.params.i_csp = X264_CSP_YUYV;
  stream.params.vui.i_colorprim = 2; /* application-defined */
  stream.params.vui.i_transfer = 2; /* application-defined */
  stream.params.vui.i_colmatrix = 2; /* application-defined */
  stream.params.vui.i_chroma_loc = 2; /* 4:2:2 */
  stream.params.i_log_level = X264_LOG_DEBUG;
  stream.params.i_bitdepth = 8;
  stream.params.i_level_idc = 9;
  // stream.params.rc.i_aq_mode = X264_AQ_NONE;
  stream.params.analyse.b_psnr = 1;
  stream.params.analyse.b_ssim = 1;

  stream.encoder = x264_encoder_open(&stream.params);

  /* encode test frames */

  int frame_size;
  int packed_size_sum = 0;

  char filename_yuyv[128];
  char filename_png[128];

  system("mkdir h264_test_data");

  FILE *nal_out = fopen("nals.bin", "w");

  x264_picture_t encoder_frame;
  x264_picture_t encoder_output;

  x264_encoder_headers(stream.encoder, &stream.nals, &stream.nal_count);
  printf("H.264 headers: %d NALs\n", stream.nal_count);
  for(int k = 0; k < stream.nal_count; ++k) {
    printf("H.264 headers: NAL %d: size %d\n", k, stream.nals[k].i_payload);
    dump_array(stream.nals[k].p_payload, stream.nals[k].i_payload);
    packed_size_sum += stream.nals[k].i_payload;
    fwrite(stream.nals[k].p_payload, stream.nals[k].i_payload, 1, nal_out);
  }

  for(int frame = 1; frame <= config.frame_count; ++frame) {
    x264_picture_alloc(&encoder_frame,  X264_CSP_YUYV, config.frame_width, config.frame_height);
    x264_picture_alloc(&encoder_output, X264_CSP_YUYV, config.frame_width, config.frame_height);

    unsigned char *raw_yuyv =
      solid_frame(config.frame_width, config.frame_height, &frame_size,
		  (60.0f * (float)frame / config.frame_count));

    extract_array(raw_yuyv, frame_size, 0, 2, &config.yvec);
    extract_array(raw_yuyv, frame_size, 1, 4, &config.cbvec);
    extract_array(raw_yuyv, frame_size, 3, 4, &config.crvec);

    encoder_frame.img.i_csp = X264_CSP_YUYV;
    encoder_frame.img.i_plane = 3;
    encoder_frame.img.i_stride[0] = config.frame_width;
    encoder_frame.img.i_stride[1] = config.frame_width >> 1;
    encoder_frame.img.i_stride[2] = config.frame_width >> 1;
    encoder_frame.img.plane[0] = config.yvec;
    encoder_frame.img.plane[1] = config.cbvec;
    encoder_frame.img.plane[2] = config.crvec;
    encoder_frame.i_pts = frame;

    stream.nals = NULL;
    stream.nal_count = 0;
    x264_encoder_encode(stream.encoder, &stream.nals, &stream.nal_count, &encoder_frame, &encoder_output);

    printf("Frame %d: %d NALs\n", frame, stream.nal_count);
    int nals_size = 0;
    for(int k = 0; k < stream.nal_count; ++k) {
      int size = stream.nals[k].i_payload;
      nals_size += size;
      fwrite(stream.nals[k].p_payload, stream.nals[k].i_payload, 1, nal_out);
      printf("\tNAL %d size %d bytes\n", k, size);
    }

    printf("Frame %d/%d: size %d bytes, streamed size %d bytes\n",
	   frame, config.frame_count, frame_size, nals_size);
    packed_size_sum += nals_size;

    if(config.export_images) {
      sprintf(filename_yuyv, "h264_test_data/%04d.bin", frame);
      sprintf(filename_png,  "h264_test_data/%04d.png", frame);
      save_array(raw_yuyv, frame_size, filename_yuyv);
      export_luma_as_png(config.frame_width, config.frame_height, filename_yuyv, filename_png);
    }

    free(raw_yuyv); /* freed by x264_picture_clean */
  }

  fclose(nal_out);

  printf("End of stream\n");
  printf("Generated frames: %d totalling %d bytes\n", config.frame_count, config.frame_count * frame_size);
  printf("Encoded   frames: %d totalling %d bytes\n", config.frame_count, packed_size_sum);
  printf("Compression ratio: %.3f%%\n", (float)(100.0 * packed_size_sum) / (config.frame_count * frame_size));

  x264_encoder_close(stream.encoder);

  return 0;
}

long int now_ms() {
  /* measures process time, which can skew harshly */
  return (1000.0d / CLOCKS_PER_SEC) * clock();
}

unsigned char *solid_frame(int width, int height, int *size, float t) {
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
		int size,
		const char *filename_yuyv) {
  FILE *fd = fopen(filename_yuyv, "w");
  fwrite(raw_yuyv, 2, (size >> 1), fd);
  fclose(fd);
}

void dump_array(unsigned char *ptr, int size) {
  static const int bytes_per_line = 0x20;
  for(int ptr0 = 0; ptr0 < size; ptr0 += bytes_per_line) {
    int ptr1;
    printf("\t+%04X ", ptr0);
    for(ptr1 = 0; ptr1 < bytes_per_line && (ptr0 + ptr1 < size); ++ptr1) {
      printf("%02X", ptr[ptr0 + ptr1]);
    }
    printf("\n");
  }
}

void extract_array(unsigned char *src,
		   int length,
		   int offset,
		   int stride,
		   unsigned char **dst){
  *dst = malloc(length / stride);
  for(int ptr = offset, out_ptr = 0; ptr < length; ptr += stride, ++out_ptr) {
    (*dst)[out_ptr] = src[ptr];
  }
}

void parse_h264_cli_args(int argc, char **argv) {
  config.export_images = 0;
  config.frame_count = 1000;
  config.frame_width = 1920;
  config.frame_height = 1080;

  for(int arg = 0; arg < argc; ++arg) {
    if(strcmp(argv[arg], "-e") == 0) {
      config.export_images = 1;
    }
    else if(strcmp(argv[arg], "-n") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config.frame_count = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-w") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config.frame_width = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-h") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config.frame_height = atoi(argv[++arg]);
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

