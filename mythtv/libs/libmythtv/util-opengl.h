// -*- Mode: c++ -*-

#ifndef _UTIL_OPENGL_H_
#define _UTIL_OPENGL_H_

#ifdef USING_OPENGL

// Qt headers
#include <QSize>

void pack_yv12alpha(const unsigned char *source, const unsigned char *dest,
                    const int *offsets, const int *pitches,
                    const QSize size, const unsigned char *alpha = NULL);

void pack_yv12interlaced(const unsigned char *source, const unsigned char *dest,
                         const int *offsets, const int *pitches,
                         const QSize size);

#endif // USING_OPENGL
#endif // _UTIL_OPENGL_H_
