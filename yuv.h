#ifndef LISTVIDEOCAP_YUV_H
#define LISTVIDEOCAP_YUV_H

struct yuv_matrix {
  float r_yy, r_cb, r_cr, r_ct, r_sc;
  float g_yy, g_cb, g_cr, g_ct, g_sc;
  float b_yy, b_cb, b_cr, b_ct, b_sc;
  float gamma;
};


void set_yuv_matrix_BT601(struct yuv_matrix *matrix);
void set_yuv_matrix_BT709(struct yuv_matrix *matrix);
void set_yuv_matrix_JPEG(struct yuv_matrix *matrix);
void set_yuv_matrix_experimental(struct yuv_matrix *matrix);

#endif

