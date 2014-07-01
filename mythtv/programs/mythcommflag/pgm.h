/*
 * pgm.h
 *
 * Routines for querying and manipulating PGM (greyscale) images.
 */

#ifndef __PGM_H__
#define __PGM_H__

struct VideoFrame_;
struct AVPicture;

int pgm_read(unsigned char *buf, int width, int height, const char *filename);
int pgm_write(const unsigned char *buf, int width, int height,
        const char *filename);
int pgm_crop(struct AVPicture *dst, const struct AVPicture *src, int srcheight,
        int srcrow, int srccol, int cropwidth, int cropheight);
int pgm_overlay(struct AVPicture *dst,
        const struct AVPicture *s1, int s1height, int s1row, int s1col,
        const struct AVPicture *s2, int s2height);
int pgm_convolve_radial(struct AVPicture *dst, struct AVPicture *s1,
        struct AVPicture *s2, const struct AVPicture *src, int srcheight,
        const double *mask, int mask_radius);

#endif  /* !__PGM_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
