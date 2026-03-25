#include "h264.h"

const int buffer_extra_padding = 64;


void h264_decode_frame_internal(struct h264_config *config);

void h264_resize_encoder_frame_internal(struct h264_config *config,
					int width, int height);


void extract_array(unsigned char *src,
		   long int length,
		   int offset,
		   int stride,
		   long int alloc_size,
		   unsigned char *dst){

  for(int ptr = offset, out_ptr = 0; ptr < length; ptr += stride, ++out_ptr) {
    dst[out_ptr] = src[ptr];
  }
}

void h264_init_encoder(struct h264_config *config,
		       int frame_width,
		       int frame_height,
		       int colorspace) {

  x264_param_default(&config->params);

  /* frame settings (libx264) */
  config->params.i_threads = 1;
  config->params.i_csp = config->frame_config.colorspace = colorspace;
  config->params.i_bitdepth = 8;
  config->params.i_level_idc = 9;
  // config->params.i_keyint_max = 20;

  h264_resize_encoder_frame_internal(config, frame_width, frame_height);

  /* video settings */
  config->params.vui.i_colorprim = 2; /* application-defined */
  config->params.vui.i_transfer = 2; /* application-defined */
  config->params.vui.i_colmatrix = 2; /* application-defined */
  config->params.vui.i_chroma_loc = 2; /* 4:2:2 */

  /* adaptive quantization setting */
  // config->params.rc.i_aq_mode = X264_AQ_NONE;

  /* enable libx264 logging */
  config->params.i_log_level = X264_LOG_INFO /* or X264_LOG_DEBUG */;

  /* don't show end-of-encoding metadata when the decoder gets deallocated */
  config->params.analyse.b_psnr = 0;
  config->params.analyse.b_ssim = 0;

  config->encoder = x264_encoder_open(&config->params);

  x264_picture_alloc(&config->input_frame,
		     config->frame_config.colorspace,
		     config->frame_config.width,
		     config->frame_config.height);

  /* required parameter for x264_encoder_encode */
  x264_picture_alloc(&config->dummy_frame,
		     config->frame_config.colorspace,
		     config->frame_config.width,
		     config->frame_config.height);
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

  /* input frames have no padding bytes at the end of scanlines */
  config->frame_config.size = (2
			       * (long)config->frame_config.width
			       * (long)config->frame_config.height);

  if(config->debug_info) {
    printf("[H264] [ENC] Frame data at %016lXh\n", (long int)data);
    printf("[H264] [ENC] Encoding frame of size %dx%d (%ld bytes)...\n",
	   config->frame_config.width,
	   config->frame_config.height,
	   config->frame_config.size);
  }

  if(config->debug_info)
  printf("[H264] [ENC] Input frame size %dx%d pitch %d size %ld\n",
	 config->frame_config.width, config->frame_config.height,
	 config->frame_config.scanline_length, config->frame_config.size);

  if(config->frame_config.colorspace == X264_CSP_YUYV) {
    /* YUV 4:2:2 packed is same as capture format */

    config->input_frame.img.i_plane = 1;
    config->input_frame.img.i_stride[0] = config->frame_config.scanline_length;

    memcpy(config->input_frame.img.plane[0], data, config->frame_config.size);

    if(config->debug_info)
    printf("[H264] [ENC] plane 0 stride %d ptr %016lXh\n",
	   config->frame_config.scanline_length, (long int)data);
  }
  else if(config->frame_config.colorspace == X264_CSP_I422) {
    /* YUV 4:2:2 planar needs deinterlacing */

    extract_array(data, config->frame_config.size, 0, 2,
		  config->frame_config.luma_length * config->frame_config.height,
		  config->input_frame.img.plane[0]);

    extract_array(data, config->frame_config.size, 1, 4,
		  config->frame_config.chroma_b_length * config->frame_config.height,
		  config->input_frame.img.plane[1]);

    extract_array(data, config->frame_config.size, 3, 4,
		  config->frame_config.chroma_r_length * config->frame_config.height,
		  config->input_frame.img.plane[2]);

    config->input_frame.img.i_plane = 3;

    config->input_frame.img.i_stride[0] = config->frame_config.luma_length;
    config->input_frame.img.i_stride[1] = config->frame_config.chroma_b_length;
    config->input_frame.img.i_stride[2] = config->frame_config.chroma_r_length;

    if(config->debug_info) {
      printf("[H264] [ENC] plane 0 stride %d ptr %016lXh\n",
	     config->frame_config.luma_length,
	     (long int)config->input_frame.img.plane[0]);
      printf("[H264] [ENC] plane 1 stride %d ptr %016lXh\n",
	     config->frame_config.chroma_b_length,
	     (long int)config->input_frame.img.plane[1]);
      printf("[H264] [ENC] plane 2 stride %d ptr %016lXh\n",
	     config->frame_config.chroma_r_length,
	     (long int)config->input_frame.img.plane[2]);
    }
  }
  else {
    printf("[H264] [ENC] Unsupported colorspace for libx264 wrapper: %d\n",
	   config->input_frame.img.i_csp);
    exit(1);
  }

  config->input_frame.i_pts = ++config->frame_config.frame_index;

  x264_encoder_encode(config->encoder,
		      &config->h264_data.nals,
		      &config->h264_data.nal_count,
		      &config->input_frame,
		      &config->dummy_frame);

  long nals_size = 0;
  for(int k = 0; k < config->h264_data.nal_count; ++k) {
    nals_size += config->h264_data.nals[k].i_payload;
  }

  config->h264_data.stream = malloc(nals_size);
  config->h264_data.size = nals_size;

  if(config->debug_info) {
    printf("[H264] [ENC] total NAL size: %ld\n", nals_size);
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
  config->frame = av_frame_alloc();
}

void h264_free_decoder(struct h264_config *config) {
  av_parser_close(config->parser);
  avcodec_free_context(&config->context);
  av_packet_free(&config->packet);
  av_frame_free(&config->frame);
}

void h264_decode_frame(struct h264_config *config,
		       unsigned char *data,
		       long int size) {

  const int buffer_size = 4096;
  char buffer[buffer_size + AV_INPUT_BUFFER_PADDING_SIZE];
  memset(buffer, 0, sizeof(buffer_size));

  config->h264_data.output_frames = 0;
  config->h264_data.size = 0;

  int ptr = 0;
  int size_remaining = size;
  int count;

  if(config->debug_info) {
    printf("[H264] [DEC] Frame decoding: got chunk of size %d\n", size);
  }

  while(size_remaining > 0) {
    if(config->debug_info) {
      printf("[H264] [DEC] Position %d/%d in chunk\n", ptr, size);
    }

    count = av_parser_parse2(config->parser, config->context,
			     &config->packet->data, &config->packet->size,
			     data + ptr, size_remaining,
			     AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

    if(count > 0) {
      ptr += count;
      size_remaining -= count;

      if(config->packet->size > 0) {
	if(config->debug_info) printf("[H264] [DEC] Decoding packet of size %d\n", count);
	avcodec_send_packet(config->context, config->packet);
	h264_decode_frame_internal(config);
      }
      else {
	if(config->debug_info) printf("[H264] [DEC] No packet in chunk of size %d\n", count);
      }
    }
    else if(count < 0) {
      if(config->debug_info) printf("[H264] [DEC] Parser error\n");
      exit(0);
    }
    else {
      if(config->debug_info) printf("[H264] [DEC] Parser requesting more input\n");
    }
  }
}

void h264_decode_frame_internal(struct h264_config *config) {
  if(config->debug_info) printf("[H264] [DEC] Decoding frame\n");

  while(1) {
    int status = avcodec_receive_frame(config->context, config->frame);

    if(status == AVERROR_EOF) {
      if(config->debug_info) printf("[H264] [DEC] End of stream\n");
      break;
    }
    else if(status == AVERROR(EAGAIN)) {
      if(config->debug_info) printf("[H264] [DEC] Incomplete frame\n");
      break;
    }

    int yuyv422 = 0, yuv422p = 0;

    config->frame_config.width  = config->frame->width;
    config->frame_config.height = config->frame->height;
    /* wrapper adds no padding bytes at the end of scanlines */
    config->frame_config.size   = config->frame->width * config->frame->height * 2;

    config->frame_config.scanline_length = config->frame->width << 1;
    config->frame_config.luma_length     = config->frame->width;
    config->frame_config.chroma_b_length = config->frame->width >> 1;
    config->frame_config.chroma_r_length = config->frame->width >> 1;

    if(config->frame->format == AV_PIX_FMT_YUYV422) {
      if(config->debug_info) printf("[H264] [DEC] YUYV 4:2:2 non-planar (packed)\n");
      yuyv422 = 1;
    }
    else if(config->frame->format == AV_PIX_FMT_YUV422P) {
      if(config->debug_info) printf("[H264] [DEC] YUYV 4:2:2 planar\n");
      yuv422p = 1;
    }
    else {
      printf("[H264] [DEC] ffmpeg returned pixel format"
	     " other than YUYV 4:2:2 or YUV422P: %d\n",
	     config->frame->format);
      exit(1);
    }

    if(config->debug_info) {
      for(int k = 0; k < AV_NUM_DATA_POINTERS; ++k) {
	if(config->debug_info)
	printf("[H264] [DEC] Frame: plane %02Xh size %08Xh ptr %016lXh\n",
	       k, config->frame->linesize[k], (long int)config->frame->data[k]);
      }
    }

    config->frame_config.size = config->frame->linesize[0] * config->frame->height;
    config->h264_data.size += config->frame_config.size;

    if(yuyv422) {
      if(config->debug_info) printf("[H264] [DEC] Decoding YUV 4:2:2 packed frame\n");

      config->frame_planes.packed = malloc(config->frame->linesize[0]
					   + buffer_extra_padding);
      memcpy(config->frame_planes.packed,
	     config->frame->data[0],
	     config->frame->linesize[0]);

      config->h264_data.output[config->h264_data.output_frames] =
	config->frame_planes.packed;

      config->h264_data.frame_sizes[config->h264_data.output_frames] =
	config->frame->linesize[0] * 2 * config->frame->height;

      ++config->h264_data.output_frames;

      if(config->debug_info)
      printf("[H264] [DEC] Decoded frame of size %dx%d\n",
	     config->frame->width, config->frame->height);
    }
    else if(yuv422p) {
      if(config->debug_info) printf("[H264] [DEC] Decoding YUV 4:2:2 planar frame\n");

      config->frame_planes.packed =
	malloc(config->frame_config.size + buffer_extra_padding);

      if(config->dump_bytes)
      printf("[H264] [DEC] Copying Y  data to offset %08Xh, size %08Xh\n",
	     0 * config->frame_config.chroma_b_length, config->frame_config.luma_length);

      memcpy(config->frame_planes.packed + 0 * config->frame_config.chroma_b_length,
	     config->frame->data[0],
	     config->frame_config.luma_length);

      if(config->dump_bytes)
      printf("[H264] [DEC] Copying Cb data to offset %08Xh, size %08Xh\n",
	     2 * config->frame_config.chroma_b_length, config->frame_config.chroma_b_length);

      memcpy(config->frame_planes.packed + 2 * config->frame_config.chroma_b_length,
	     config->frame->data[1],
	     config->frame_config.chroma_b_length);

      if(config->dump_bytes)
      printf("[H264] [DEC] Copying Cr data to offset %08Xh, size %08Xh\n",
	     3 * config->frame_config.chroma_b_length, config->frame_config.chroma_r_length);

      memcpy(config->frame_planes.packed + 3 * config->frame_config.chroma_b_length,
	     config->frame->data[2],
	     config->frame_config.chroma_r_length);

      config->h264_data.output[config->h264_data.output_frames] =
	config->frame_planes.packed;

      config->h264_data.frame_sizes[config->h264_data.output_frames] =
	config->frame->linesize[0] * 2 * config->frame->height;

      ++config->h264_data.output_frames;

      if(config->debug_info)
      printf("[H264] [DEC] Decoded frame of size %dx%d\n",
	     config->frame->width, config->frame->height);
    }
    else {
      printf("[H264] [DEC] Unreachable\n");
      exit(1);
    }
  }
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
  h264_resize_encoder_frame_internal(config, width, height);

  x264_encoder_reconfig(config->encoder, &config->params);
}

void h264_resize_encoder_frame_internal(struct h264_config *config,
					int width, int height) {
  if(config->debug_info) printf("[H264] [ENC] Resize to %dx%d\n", width, height);

  config->params.i_width = width;
  config->params.i_height = height;

  config->frame_config.width = width;
  config->frame_config.height = height;
  config->frame_config.scanline_length = width << 1;

  config->frame_config.size = config->frame_config.scanline_length * height;

  config->frame_config.luma_length = width;
  config->frame_config.chroma_b_length = width >> 1;
  config->frame_config.chroma_r_length = width >> 1;
}

