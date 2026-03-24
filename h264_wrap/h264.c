#include "h264.h"

void h264_decode_frame_internal(struct h264_config *config);


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

  config->frame_config.size = (2 * (long)config->frame_config.width
			       * (long)config->frame_config.height);

  if(config->debug_info) {
    printf("[H264] Frame data at %016lX\n", (long int)data);
    printf("[H264] Encoding frame of size %dx%d (%ld bytes)...\n",
	   config->frame_config.width,
	   config->frame_config.height,
	   config->frame_config.size);
  }

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
  config->h264_data.size = nals_size;

  if(config->debug_info) {
    printf("[H264] total NAL size: %ld\n", nals_size);
  }

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

void h264_init_decoder(struct h264_config *config) {
  /* cf. tips in avcodec.h */
  config->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  config->parser = av_parser_init(config->codec->id);
  config->context = avcodec_alloc_context3(config->codec);
  avcodec_open2(config->context, config->codec, NULL);
  config->packet = av_packet_alloc();
}

void h264_free_decoder(struct h264_config *config) {
  av_parser_close(config->parser);
  avcodec_free_context(&config->context);
  av_packet_free(&config->packet);
}

void h264_decode_frame(struct h264_config *config,
		       unsigned char *data,
		       long int size) {

  const int buffer_size = 16384;
  char buffer[buffer_size + AV_INPUT_BUFFER_PADDING_SIZE];

  config->h264_data.output_frames = 0;
  config->h264_data.size = 0;

  int ptr = 0;
  int size_remaining = size;
  int count;

  while(size_remaining > 0) {
    if(config->debug_info) printf("[H264] Position %d/%d in chunk\n", ptr, size);

    count = av_parser_parse2(config->parser, config->context,
			     &config->packet->data, &config->packet->size,
			     data + ptr, size_remaining,
			     AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

    if(count > 0) {
      if(config->dump_bytes) {
	if(config->debug_info) printf("[H264] Packet: size %d\n", count);
	dump_array(data + ptr, count);
      }

      ptr += count;
      size_remaining -= count;

      if(config->packet->size > 0) {
	avcodec_send_packet(config->context, config->packet);
	h264_decode_frame_internal(config);
      }
    }
    else if(count < 0) {
      printf("[H264] Parser error\n");
      exit(0);
    }
  }
}

void h264_decode_frame_internal(struct h264_config *config) {
  if(config->debug_info) printf("[H264] Decoding frame\n");
  AVFrame *frame = av_frame_alloc();

  while(1) {
    int status = avcodec_receive_frame(config->context, frame);

    if(status == AVERROR_EOF) {
      if(config->debug_info) printf("[H264] End of stream\n");
      break;
    }
    else if(status == AVERROR(EAGAIN)) {
      if(config->debug_info) printf("[H264] Incomplete frame\n");
      break;
    }

    int yuyv422 = 0, yuv422p = 0;

    if(frame->format == AV_PIX_FMT_YUYV422) {
      if(config->debug_info) printf("[H264] YUYV 4:2:2 non-planar (packed)\n");
      yuyv422 = 1;
    }
    else if(frame->format == AV_PIX_FMT_YUV422P) {
      if(config->debug_info) printf("[H264] YUYV 4:2:2 planar\n");
      yuv422p = 1;
    }
    else {
      printf("[H264] ffmpeg returned pixel format"
	     " other than YUYV 4:2:2 or YUV422P: %d\n",
	     frame->format);
    }

    config->frame_config.width = frame->width;
    config->frame_config.height = frame->height;

    if(config->debug_info) {
      for(int k = 0; k < AV_NUM_DATA_POINTERS; ++k) {
	printf("[H264] Frame: plane %02X size %08X ptr %016lX\n",
	       k, frame->linesize[k], (long int)frame->data[k]);
      }
    }

    if(yuyv422) {
      /* packed YUV 4:2:2 */
      config->frame_planes.packed = malloc(frame->linesize[0]);
      memcpy(config->frame_planes.packed, frame->data[0], frame->linesize[0]);

      config->h264_data.size += frame->linesize[0];

      config->h264_data.output[config->h264_data.output_frames] =
	config->frame_planes.packed;

      config->h264_data.frame_sizes[config->h264_data.output_frames] =
	frame->linesize[0];

      ++config->h264_data.output_frames;

      printf("[H264] Decoded frame of size %dx%d\n",
	     frame->width, frame->height);
    }
    else if(yuv422p) {
      /* planar YUV 4:2:2 */
      config->frame_planes.packed = malloc(frame->linesize[0] * 2);

      for(int k = 0; k < frame->linesize[0]; k += 2) {
	config->frame_planes.packed[4 * k + 0] = frame->data[0][k +  0];
	config->frame_planes.packed[4 * k + 1] = frame->data[1][k >> 1];
	config->frame_planes.packed[4 * k + 2] = frame->data[0][k +  1];
	config->frame_planes.packed[4 * k + 3] = frame->data[1][k >> 1];
      }

      config->h264_data.size += frame->linesize[0] * 2;

      config->h264_data.output[config->h264_data.output_frames] =
	config->frame_planes.packed;

      config->h264_data.frame_sizes[config->h264_data.output_frames] =
	frame->linesize[0] * 2;

      ++config->h264_data.output_frames;

      printf("[H264] Decoded frame of size %dx%d\n",
	     frame->width, frame->height);
    }
    else {
      printf("[H264] Unreachable\n");
      exit(1);
    }
  }

  if(frame) av_frame_free(&frame);
}

void h264_decode_flush(struct h264_config *config) {
  avcodec_send_packet(config->context, NULL);
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

void h264_change_encoder_frame_size(struct h264_config *config,
				    int width, int height) {
  config->frame_config.width = width;
  config->frame_config.height = height;

  config->params.i_width = width;
  config->params.i_height = height;

  x264_encoder_reconfig(config->encoder, &config->params);
}
