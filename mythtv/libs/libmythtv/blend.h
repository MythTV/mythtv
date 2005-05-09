#ifndef BLEND_H
#define BLEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
typedef void (*blendregion_ptr) (uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                                 int, uint8_t *, uint8_t *, uint8_t *,
                                 uint8_t *, int, int, int, int, int,
                                 int16_t[256], uint8_t[256][256]);

typedef void (*blendcolumn2_ptr) (uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                                 int, uint8_t *, uint8_t *, uint8_t *,
                                 uint8_t *, int, uint8_t *, uint8_t *,
                                 uint8_t *, uint8_t *, uint8_t *, int, int,
                                 int, int, int, int16_t[256],
                                 uint8_t[256][256]);

typedef void (*blendcolumn_ptr) (uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                                 int, uint8_t *, uint8_t *, uint8_t *,
                                 uint8_t *, int, int, int, int, int,
                                 int16_t[256], uint8_t[256][256]);

typedef void (*blendcolor_ptr) (uint8_t, uint8_t, uint8_t, uint8_t *, int,
                                uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                                int, int, int, int, int, int16_t[256],
                                uint8_t[256][256]);

typedef void (*blendconst_ptr) (uint8_t, uint8_t, uint8_t, uint8_t,
                                uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                                int, int, int, int, int16_t[256],
                                uint8_t[256][256]);

#ifdef MMX
void blendregion_mmx (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                      uint8_t * asrc, int srcstrd,
                      uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                      uint8_t * adst, int dststrd,
                      int width, int height, int alphamod, int dochroma,
                      int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendcolumn2_mmx (uint8_t * ysrc1, uint8_t * usrc1, uint8_t * vsrc1,
                       uint8_t * asrc1, int srcstrd1,
                       uint8_t * ysrc2, uint8_t * usrc2, uint8_t * vsrc2,
                       uint8_t * asrc2, int srcstrd2,
                       uint8_t * mask,
                       uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                       uint8_t * adst, int dststrd,
                       int width, int height, int alphamod, int dochroma,
                       int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendcolumn_mmx (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                      uint8_t * asrc, int srcstrd,
                      uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                      uint8_t * adst, int dststrd,
                      int width, int height, int alphamod, int dochroma,
                      int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendcolor_mmx (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                     uint8_t * asrc, int srcstrd,
                     uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                     uint8_t * adst, int dststrd,
                     int width, int height, int alphamod, int dochroma,
                     int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendconst_mmx (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                     uint8_t asrc,
                     uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                     uint8_t * adst, int dststrd,
                     int width, int height, int dochroma,
                     int16_t rec_lut[256], uint8_t pow_lut[256][256]);
#endif

void blendregion (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                  uint8_t * asrc, int srcstrd,
                  uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                  uint8_t * adst, int dststrd,
                  int width, int height, int alphamod, int dochroma,
                  int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendcolumn2 (uint8_t * ysrc1, uint8_t * usrc1, uint8_t * vsrc1,
                   uint8_t * asrc1, int srcstrd1,
                   uint8_t * ysrc2, uint8_t * usrc2, uint8_t * vsrc2,
                   uint8_t * asrc2, int srcstrd2,
                   uint8_t * mask,
                   uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                   uint8_t * adst, int dststrd,
                   int width, int height, int alphamod, int dochroma,
                   int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendcolumn (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                  uint8_t * asrc, int srcstrd,
                  uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                  uint8_t * adst, int dststrd,
                  int width, int height, int alphamod, int dochroma,
                  int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendcolor (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                 uint8_t * asrc, int srcstrd,
                 uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                 uint8_t * adst, int dststrd,
                 int width, int height, int alphamod, int dochroma,
                 int16_t rec_lut[256], uint8_t pow_lut[256][256]);

void blendconst (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                 uint8_t asrc,
                 uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                 uint8_t * adst, int dststrd,
                 int width, int height, int dochroma,
                 int16_t rec_lut[256], uint8_t pow_lut[256][256]);

#ifdef __cplusplus
}
#endif

#endif /* BLEND_H */
