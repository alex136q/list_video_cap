#ifndef INCLUDE_H264_H
#define INCLUDE_H264_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <x264.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>


/* wrapper on top of libx264 (encoding) and libavcodec (decoding) */

struct h264_config {
  /* internal, libx264 */
  x264_t *encoder;
  x264_param_t params;
  x264_picture_t dummy_frame;
  x264_picture_t input_frame;

  /* internal, libavcodec */
  AVCodec *codec;
  AVCodecParserContext *parser;
  AVCodecContext *context;
  AVPacket *packet;
  AVFrame *frame;

  struct {
    /* input */
    unsigned char *stream;
    long size;
    /* output */
    unsigned char *output[256]; /* individual frames */
    long int frame_sizes[256];
    long output_frames;
    /* internal */
    x264_nal_t *nals;
    int nal_count;
  } h264_data;

  struct {
    /* input/output */
    int frame_index;
    int width;
    int height;
    int colorspace;
    /* output */
    long int size;
    int scanline_length;
    /* internal */
    int luma_length;
    int chroma_b_length;
    int chroma_r_length;
  } frame_config;

  struct {
    unsigned char *packed; /* input/output */
    /* internal */
    unsigned char *yvec;
    unsigned char *cbvec;
    unsigned char *crvec;
  } frame_planes;

  /* debug */
  int debug_info;
  int dump_bytes;
};


void h264_init_encoder(struct h264_config *config,
		       int frame_width,
		       int frame_height,
		       int colorspace,
		       int interval);

void h264_init_decoder(struct h264_config *config);

void h264_free_encoder(struct h264_config *config);
void h264_free_decoder(struct h264_config *config);

void h264_get_headers(struct h264_config *config);

void h264_encode_frame(struct h264_config *config,
		       unsigned char *data);

void h264_decode_frame(struct h264_config *config,
		       unsigned char *data,
		       long int size);

void h264_decode_flush(struct h264_config *config);

void h264_change_encoder_frame_size(struct h264_config *config,
				    int _width, int height);

void extract_array(unsigned char *src,
		   long int length,
		   int offset,
		   int stride,
		   unsigned char *dst);

void pack_array(unsigned char *src,
		long int length,
		int offset,
		int stride,
		unsigned char *dst);

void dump_array(unsigned char *ptr, long size);

#endif

