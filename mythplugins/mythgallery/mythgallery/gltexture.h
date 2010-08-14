// -*- Mode: c++ -*-
/* ============================================================
 * File  : gltexture.h
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2006-07-24
 * Description : 
 * 
 * Copyright 2004-2006 Renchi Raju, Daniel Kristjansson
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */

#ifndef _GL_TEXTURE_H_
#define _GL_TEXTURE_H_
#ifdef USING_OPENGL

#include <QtOpenGL>
#include <QSize>

// MythGallery headers
#include "galleryutil.h"

class ThumbItem;

class GLTexture
{
  public:
    GLTexture() : tex(0), angle(0), item(NULL),
        width(512), height(512), cx(1.0f), cy(1.0f) {}
    ~GLTexture() { Deinit(); }

    void Init(const QImage &image);
    void Deinit(void);

    void Bind(void);
    void MakeQuad(float alpha = 1.0f, float scale = 1.0f);
    void SwapWidthHeight(void)
        { int tmp = width; width = height; height = tmp; }

    // Sets
    void SetItem(ThumbItem*, const QSize &sz);
    void SetSize(const QSize &sz)
        { width = sz.width(); height = sz.height(); }
    void SetScale(float x, float y)
        { cx = x; cy = y; }
    void ScaleTo(const QSize &dest, ScaleMax scaleMax);
    void SetAngle(int newangle) { angle = newangle; }

    // Gets
    QSize   GetSize(void)        const { return QSize(width, height); }
    uint    GetPixelCount(void)  const { return width * height; }
    float   GetTextureX(void)    const { return cx; }
    float   GetTextureY(void)    const { return cy; }
    int     GetAngle(void)       const { return angle; }
    QString GetDescription(const QString &status) const;

  private:
    GLuint     tex;
    int        angle;
    ThumbItem *item;
    int        width;
    int        height;
    float      cx;
    float      cy;
};

#endif // USING_OPENGL
#endif // _GL_TEXTURE_H_
