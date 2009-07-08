#ifndef _COLOR_H_
#define _COLOR_H_

extern void (*yv12_to_yuy2)
  (const unsigned char *y_src, int y_src_pitch, 
   const unsigned char *u_src, int u_src_pitch, 
   const unsigned char *v_src, int v_src_pitch, 
   unsigned char *yuy2_map, int yuy2_pitch,
   int width, int height, int progressive);

extern void (*yuy2_to_yv12)
  (const unsigned char *yuy2_map, int yuy2_pitch,
   unsigned char *y_dst, int y_dst_pitch, 
   unsigned char *u_dst, int u_dst_pitch, 
   unsigned char *v_dst, int v_dst_pitch, 
   int width, int height);

void init_yuv_conversion(void);

void apply_chroma_filter( uint8_t *data, int stride, int width, int height );

#endif //_COLOR_H_

