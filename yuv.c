#include "yuv.h"

struct yuv_matrix yuv_rgb;


void set_yuv_matrix_BT601(struct yuv_matrix *matrix) {
  /* mangled, cf. BT.601 matrix */

  matrix->r_yy = +298.082f / 256.0f;
  matrix->r_cb =     +0.0f / 256.0f;
  matrix->r_cr = +408.583f / 256.0f;
  matrix->r_ct = -222.921f / 256.0f;
  matrix->r_sc = +1.0000f;

  matrix->g_yy = +298.082f / 256.0f;
  matrix->g_cb = -100.291f / 256.0f;
  matrix->g_cr = -208.120f / 256.0f;
  matrix->g_ct = +135.576f / 256.0f;
  matrix->g_sc = +1.0000f;

  matrix->b_yy = +298.082f / 256.0f;
  matrix->b_cb = +516.412f / 256.0f;
  matrix->b_cr =     +0.0f / 256.0f;
  matrix->b_ct = -276.736f / 256.0f;
  matrix->b_sc = +1.0000f;

  matrix->gamma = +1.000f;
}

void set_yuv_matrix_BT709(struct yuv_matrix *matrix) {
  /* mangled, cf. ITU BT.709 at https://en.wikipedia.org/wiki/YCbCr */

  matrix->r_yy = +1.0000f;
  matrix->r_cb = +0.0000f;
  matrix->r_cr = +1.5748f;
  matrix->r_ct = +0.0000f;
  matrix->r_sc = +1.0000f;

  matrix->g_yy = +1.0000f;
  matrix->g_cb = -0.1873f;
  matrix->g_cr = -0.4618f;
  matrix->g_ct = +0.0000f;
  matrix->g_sc = +1.0000f;

  matrix->b_yy = +1.0000f;
  matrix->b_cb = +1.8556f;
  matrix->b_cr = +0.0000f;
  matrix->b_ct = +0.0000f;
  matrix->b_sc = +1.0000f;

  matrix->gamma = +1.000f;
}

void set_yuv_matrix_JPEG(struct yuv_matrix *matrix) {
  /* cf. JFIF/JPEG YUV at https://en.wikipedia.org/wiki/YCbCr */

  matrix->r_yy = +1.0000f;
  matrix->r_cb = +0.0000f;
  matrix->r_cr = +1.4020f;
  matrix->r_ct = -0.5000f * +1.4020f;
  matrix->r_sc = +1.0000f;

  matrix->g_yy = +1.0000f;
  matrix->g_cb = -0.3441f;
  matrix->g_cr = -0.7141f;
  matrix->g_ct = -0.5000f * (+0.3411f +0.7141f);
  matrix->g_sc = +1.0000f;

  matrix->b_yy = +1.0000f;
  matrix->b_cb = +1.7720f;
  matrix->b_cr = +0.0000f;
  matrix->b_ct = -0.5000f * +1.7720f;
  matrix->b_sc = +1.0000f;

  matrix->gamma = +1.000f;
}

void set_yuv_matrix_experimental(struct yuv_matrix *matrix) {
  /* experimental colorspace transform */

  matrix->r_yy = +1.0000f;
  matrix->r_cb = +0.0000f;
  matrix->r_cr = +0.5000f;
  matrix->r_ct = +0.0000f;
  matrix->r_sc = +1.0000f;

  matrix->g_yy = +1.0000f;
  matrix->g_cb = -0.2000f;
  matrix->g_cr = -0.4000f;
  matrix->g_ct = +0.5000f;
  matrix->g_sc = +1.0000f;

  matrix->b_yy = +1.0000f;
  matrix->b_cb = +0.5000f;
  matrix->b_cr = +0.0000f;
  matrix->b_ct = +0.0000f;
  matrix->b_sc = +1.0000f;

  matrix->gamma = +2.000f;
}

