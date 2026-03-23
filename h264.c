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
  x264_nal_t **nals;
} stream;

unsigned char sinusoid(unsigned char bias,
		       unsigned char amplitude,
		       float time,
		       float period,
		       float phase);

unsigned char *solid_frame(int width, int height, int *size, float t);


#ifndef LISTVIDEOCAP_MAIN

struct {
  long int time_ms;
} config;

const int frame_count = 10;

const int frame_width = 800;
const int frame_height = 600;


long int now_ms();

void save_array(const unsigned char *raw_yuyv,
		int size,
		const char *filename_yuyv);

void export_luma_as_png(int frame_width, int frame_height,
			const char *filename_yuyv, const char *filename_png);

int main(int argc, char **argv) {
  config.time_ms = now_ms();

  /* initialize libx264 encoder and its parameters */

  x264_param_default(&stream.params);

  stream.params.i_width = frame_width;
  stream.params.i_height = frame_height;
  stream.params.i_csp = X264_CSP_YUYV;

  stream.encoder = x264_encoder_open(&stream.params);

  /* encode test frames */

  int frame_size;
  char filename_yuyv[128];
  char filename_png[128];

  for(int frame = 1; frame <= frame_count; ++frame) {
    unsigned char *raw_yuyv =
      solid_frame(frame_width, frame_height, &frame_size,
		  (60.0f * (float)frame / frame_count));

    sprintf(filename_yuyv, "h264_test_data/%04d.bin", frame);
    sprintf(filename_png,  "h264_test_data/%04d.png", frame);
    save_array(raw_yuyv, frame_size, filename_yuyv);
    export_luma_as_png(frame_width, frame_height, filename_yuyv, filename_png);
    free(raw_yuyv);
  }

  x264_encoder_close(stream.encoder);

  /* decode test frames */
  return 0;
}

long int now_ms() {
  /* measures process time, which can skew harshly */
  return (1000.0d / CLOCKS_PER_SEC) * clock();
}

#endif


unsigned char *solid_frame(int width, int height, int *size, float t) {
  *size = width * height * 2;
  unsigned char *data = malloc(*size);

  const unsigned char Y_ = sinusoid(0x7F, 0x7F, t, 5.0f, 0.000f);
  const unsigned char Cb = sinusoid(0x7F, 0x7F, t, 2.0f, 0.500f);
  const unsigned char Cr = sinusoid(0x7F, 0x7F, t, 7.0f, 0.125f);

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

void export_luma_as_png(int frame_width, int frame_height,
		   const char *filename_yuyv, const char *filename_png) {
  char cmd[1024];

  sprintf(cmd, "python3 ./extract_yuyv_y.py %s", filename_yuyv);
  system(cmd);

  sprintf(cmd, "magick -depth 8 -size %dx%d gray:%s.luma png:%s",
	  frame_width, frame_height, filename_yuyv, filename_png);
  system(cmd);
}

void save_array(const unsigned char *raw_yuyv,
		int size,
		const char *filename_yuyv) {
  FILE *fd = fopen(filename_yuyv, "w");
  fwrite(raw_yuyv, 2, (size >> 1), fd);
  fclose(fd);
}

