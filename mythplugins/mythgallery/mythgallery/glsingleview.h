/* ============================================================
 * File  : glsingleview.h
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2004-01-13
 * Description : 
 * 
 * Copyright 2004 by Renchi Raju

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

#ifndef GLSINGLEVIEW_H
#define GLSINGLEVIEW_H

#include <qgl.h>
#include <qmap.h>
#include <qsize.h>

#include <mythtv/util.h>

#include "iconview.h"
#include "sequence.h"

class QImage;
class QTimer;

class GLSingleView;

class GLSDialog : public MythDialog
{
  public:  
    GLSDialog(const ThumbList& itemList,
              int pos, int slideShow, int sortOrder,
              MythMainWindow *parent, const char *name=0);

  protected:
    void closeEvent(QCloseEvent *e);
    
  private:
    GLSingleView *m_view;
};

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
    void ScaleTo(const QSize &dest);
    void SetAngle(int newangle) { angle = newangle; }

    // Gets
    QSize   GetSize(void)        const { return QSize(width, height); }
    uint    GetPixelCount(void)  const { return width * height; }
    float   GetTextureX(void)    const { return cx; }
    float   GetTextureY(void)    const { return cy; }
    int     GetAngle(void)       const { return angle; }
    QString GetDescription(void) const;

  private:
    GLuint     tex;
    int        angle;
    ThumbItem *item;
    int        width;
    int        height;
    float      cx;
    float      cy;
};

class GLSingleView : public QGLWidget
{
    Q_OBJECT

  public:
    GLSingleView(ThumbList itemList, int pos, int slideShow, int sordorder,
                 QWidget *parent);
    ~GLSingleView();

    void CleanUp(void);

    void SetTransitionTimeout(int timeout)
    {
        m_transTimeout = timeout;
        m_transTimeoutInv = 1.0f;
        if (timeout)
            m_transTimeoutInv = 1.0f / timeout;
    }

  protected:
    void initializeGL(void);
    void resizeGL(int w, int h);
    void paintGL(void);

    void keyPressEvent(QKeyEvent *e);

  private:
    int           m_pos;
    ThumbList     m_itemList;

    int           m_movieState;
    QSize         m_screenSize;
    float         m_wmult;
    float         m_hmult;

    QSize         m_textureSize;
    GLTexture     m_texItem[2];
    int           m_curr;
    bool          m_tex1First;

    float         m_zoom;
    float         m_sx;
    float         m_sy;

    QTimer       *m_timer;
    MythTimer 	  m_time;
    int           m_delay;
    int           m_tmout;
    int           m_transTimeout;
    float         m_transTimeoutInv;
    bool          m_effectRunning;
    bool          m_running;
    int           m_slideShow;

    GLuint        m_texInfo;
    bool          m_showInfo;
    int           m_maxTexDim;
    
    int           m_i;
    int           m_dir;
    float         m_points[40][40][3];

    typedef void               (GLSingleView::*EffectMethod)();
    EffectMethod                m_effectMethod;
    QMap<QString,EffectMethod>  m_effectMap;
    bool                        m_effectRandom;
    SequenceBase               *m_sequence;

    float         m_cube_xrot;
    float         m_cube_yrot;
    float         m_cube_zrot;

  private:
    int   NearestGLTextureSize(int); 
    void  advanceFrame(void);
    void  retreatFrame(void);
    void  loadImage(void);
    void  paintTexture(void);
    void  Rotate(int angle);
    void  createTexInfo(void);

    void  RegisterEffects(void);
    EffectMethod GetRandomEffect(void);

    void effectNone(void);
    void effectBlend(void);
    void effectZoomBlend(void);
    void effectFade(void);
    void effectRotate(void);
    void effectBend(void);
    void effectInOut(void);
    void effectSlide(void);
    void effectFlutter(void);
    void effectCube(void);

  private slots:
    void slotTimeOut(void);
};

#endif /* GLSINGLEVIEW_H */
