#include "h264.h"

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

void h264_init_encoder(struct h264_config *config,
		       int frame_width,
		       int frame_height) {

  x264_param_default(&config->params);

  /* frame settings (libx264) */
  config->params.i_threads = 0;
  config->params.i_csp = X264_CSP_YUYV;
  config->params.i_bitdepth = 8;
  config->params.i_level_idc = 9;

  config->params.i_width = frame_width;
  config->params.i_height = frame_height;

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

void h264_free_encoder(struct h264_config *config) {
  x264_encoder_close(config->encoder);
  x264_param_cleanup(&config->params);
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

void h264_init_decoder(struct h264_config *config,
		       int frame_width,
		       int frame_height) {

  // ... avcodec_receive_packet() ... (?)

}

void h264_free_decoder(struct h264_config *config) {
}

void h264_decode_frame(struct h264_config *config,
		       unsigned char *data) {
  return;
}

