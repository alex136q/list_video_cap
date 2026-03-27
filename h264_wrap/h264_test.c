#include "h264.h"

struct {
  int export_images;
  int frame_count;
  int test_encoding;
  int test_decoding;
  char *out_stream_path;
  char *in_stream_path;
} h264_cli_config;


unsigned char sinusoid(unsigned char bias,
		       unsigned char amplitude,
		       float time,
		       float period,
		       float phase);

unsigned char *solid_frame(int width, int height, long int *size, float t);

void save_array(const unsigned char *raw_yuyv,
		long size,
		const char *filename_yuyv);

void export_luma_as_png(int width, int height,
			const char *filename_yuyv,
			const char *filename_png);


void parse_h264_cli_args(int argc, char **argv,
			 struct h264_config *config);

void display_h264_cli_help();


void encode_test_frames(struct h264_config *config);
void decode_test_frames(struct h264_config *config);


void encode_test_frames(struct h264_config *config) {
  if(h264_cli_config.export_images) {
    /* ensure the folder exists */
    system("mkdir test_data");
  }

  FILE *nal_out = fopen(h264_cli_config.out_stream_path, "w");

  config->frame_config.frame_index = 0;

  h264_get_headers(config);
  if(config->debug_info) {
    printf("H.264 headers: %d NALs\n", config->h264_data.nal_count);
  }

  long packed_size_sum = 0;

  for(int k = 0; k < config->h264_data.nal_count; ++k) {
    if(config->debug_info) {
      printf("H.264 headers: NAL %d: size %d\n",
	     k, config->h264_data.nals[k].i_payload);
    }

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

    config->h264_data.stream = NULL;

    h264_encode_frame(config, raw_yuyv);

    free(raw_yuyv);

    if(config->debug_info) {
      printf("Frame %d: %d NALs\n", frame, config->h264_data.nal_count);
    }

    long nals_size = 0;

    for(int k = 0; k < config->h264_data.nal_count; ++k) {
      int size = config->h264_data.nals[k].i_payload;
      nals_size += size;
      fwrite(config->h264_data.nals[k].p_payload, 1,
	     config->h264_data.nals[k].i_payload, nal_out);
      if(config->debug_info) {
	printf("\tNAL %d size %d bytes\n", k, size);
      }
    }

    if(config->debug_info) {
      printf("Frame %d: NAL total size %ld = %ld bytes\n",
	     frame, config->h264_data.size, nals_size);

      printf("Frame %d/%d: size %ld bytes, streamed size %ld bytes\n",
	     frame, h264_cli_config.frame_count, config->frame_config.size, nals_size);
    }

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

unsigned char *solid_frame(int width, int height, long int *size, float t) {
  *size = width * height * 2;
  unsigned char *data = malloc(*size);

  const unsigned char Y_ = sinusoid(0x7F, 0x7F, t, 3.0f, 0.000f);
  const unsigned char Cb = sinusoid(0x7F, 0x7F, t, 2.0f, 0.369f);
  const unsigned char Cr = sinusoid(0x7F, 0x7F, t, 5.0f, 0.417f);

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

void parse_h264_cli_args(int argc, char **argv,
			 struct h264_config *config) {
  h264_cli_config.test_encoding = 0;
  h264_cli_config.test_decoding = 0;

  h264_cli_config.export_images = 0;
  h264_cli_config.frame_count = 1000;

  config->frame_config.width = 1920;
  config->frame_config.height = 1080;
  config->frame_config.colorspace = X264_CSP_YUYV;

  config->debug_info = 0;
  config->dump_bytes = 0;

  for(int arg = 1; arg < argc; ++arg) {
    if(strcmp(argv[arg], "-x") == 0) {
      h264_cli_config.export_images = 1;
    }
    else if(strcmp(argv[arg], "-n") == 0) {
      if(argc <= arg) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      h264_cli_config.frame_count = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-w") == 0) {
      if(argc == arg + 1) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config->frame_config.width = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-h") == 0) {
      if(argc == arg + 1) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      config->frame_config.height = atoi(argv[++arg]);
    }
    else if(strcmp(argv[arg], "-e") == 0) {
      if(argc == arg + 1) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      h264_cli_config.test_encoding = 1;
      h264_cli_config.out_stream_path = argv[++arg];
    }
    else if(strcmp(argv[arg], "-d") == 0) {
      if(argc == arg + 1) { printf("Missing argument for %s\n", argv[arg]); exit(1); }
      h264_cli_config.test_decoding = 1;
      h264_cli_config.in_stream_path = argv[++arg];
    }
    else if(strcmp(argv[arg], "-b") == 0) {
      config->debug_info = 1;
      config->dump_bytes = 1;
    }
    else if(strcmp(argv[arg], "-v") == 0) {
      config->debug_info = 1;
    }
    else if(strcmp(argv[arg], "-j") == 0) {
      config->frame_config.colorspace = X264_CSP_I422;
    }
    else {
      printf("Unknown flag or argument: %s\n", argv[arg]);
      display_h264_cli_help();
      exit(1);
    }
  }
}

void display_h264_cli_help() {
  printf("Flags:\n"
	 "--help\n\tDisplays this summary.\n\n"
	 "-e <PATH>\n\tOutput sample frames in H.264 format.\n\n"
	 "-d <PATH>\n\t(WIP) Decode sample H.264 stream outputted previously.\n\n"
	 "-x\n\tEnables export of images to ./h264_test_data/.\n\n"
	 "-n <count>\n\tSet frame count.\n\n"
	 "-w <width>\n\tSet frame width.\n\n"
	 "-h <height>\n\tSet frame height.\n\n"
	 "-j\n\tUse encoding YUV 4:2:2 planar. Default is YUV 4:2:2 packed.\n\n"
	 "-b\n\tDump decoded byte packets.\n\n"
	 "-v\n\tVerbose debugging messages.\n\n"
	 "");
}


void decode_test_frames(struct h264_config *config) {
  FILE *stream_fd = fopen(h264_cli_config.in_stream_path, "r");
  if(!stream_fd) {
    printf("File does not exist: %s\n", h264_cli_config.in_stream_path);
    return;
  }
  int buffer_size = 1024;
  char buffer[buffer_size];
  int length;

  config->h264_data.output_frames = 0;

  while(1) {
    length = fread(buffer, 1, buffer_size, stream_fd);
    if(length != 0) {
      if(config->debug_info) {
	printf("[H264] Chunk: offset %08lXh, size %08Xh\n", ftell(stream_fd), length);
      }

      h264_decode_frame(config, buffer, length);

      for(int k = 0; k < config->h264_data.output_frames; ++k) {
	if(config->h264_data.output[k]) {
	  if(config->debug_info) {
	    printf("[H264] Frame %d/%d size %d bytes ptr %016lXh\n",
		   (k + 1),
		   config->h264_data.output_frames,
		   config->h264_data.frame_sizes[k],
		   config->h264_data.output[k]);
	  }
	  free(config->h264_data.output[k]);
	}
      }
    }
    else {
      break;
    }
  }
  fclose(stream_fd);
}

int main(int argc, char **argv) {
  struct h264_config codec;
  parse_h264_cli_args(argc, argv, &codec);

  if(h264_cli_config.test_encoding) {
    h264_init_encoder(&codec,
		      codec.frame_config.width,
		      codec.frame_config.height,
		      codec.frame_config.colorspace,
		      5);
    parse_h264_cli_args(argc, argv, &codec);
    encode_test_frames(&codec);
    h264_free_encoder(&codec);
  }

  if(h264_cli_config.test_decoding) {
    h264_init_decoder(&codec);
    parse_h264_cli_args(argc, argv, &codec);
    decode_test_frames(&codec);
    h264_free_decoder(&codec);
  }

  if(!h264_cli_config.test_encoding && !h264_cli_config.test_decoding) {
    display_h264_cli_help();
  }

  return 0;
}

