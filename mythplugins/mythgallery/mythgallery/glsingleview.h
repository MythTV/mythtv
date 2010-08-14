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
#ifdef USING_OPENGL

// MythTV plugin headers
#include <util.h>
#include <mythdialogs.h>

// MythGallery headers
#include "imageview.h"
#include "iconview.h"
#include "sequence.h"
#include "gltexture.h"

class QImage;
class QTimer;

class GLSingleView;

class GLSDialog : public MythDialog
{
  public:  
    GLSDialog(const ThumbList& itemList,
              int *pos, int slideShow, int sortOrder,
              MythMainWindow *parent, const char *name="GLSDialog");

  protected:
    void closeEvent(QCloseEvent *e);
    
  private:
    GLSingleView *m_view;
};

class GLSingleView : public QGLWidget, public ImageView
{
    Q_OBJECT

  public:
    GLSingleView(ThumbList itemList, int *pos, int slideShow, int sordorder,
                 QWidget *parent);
    ~GLSingleView();

    void CleanUp(void);

  protected:
    void initializeGL(void);

    // Commands
    virtual void Rotate(int angle);
    virtual void DisplayNext(bool reset, bool loadImage);
    virtual void DisplayPrev(bool reset, bool loadImage);
    virtual void Load(void);
    void resizeGL(int w, int h);

    void paintGL(void);
    void paintTexture(void);
    void createTexInfo(void);
    virtual void keyPressEvent(QKeyEvent *e);
    void checkPosition(void);

    // Sets
    virtual void SetZoom(float zoom);
    void SetTransitionTimeout(int timeout);

    // Gets
    int GetNearestGLTextureSize(int) const;

    virtual void RegisterEffects(void);
    virtual void RunEffect(const QString &effect);

    void EffectNone(void);
    void EffectBlend(void);
    void EffectZoomBlend(void);
    void EffectFade(void);
    void EffectRotate(void);
    void EffectBend(void);
    void EffectInOut(void);
    void EffectSlide(void);
    void EffectFlutter(void);
    void EffectCube(void);

  private slots:
    void SlideTimeout(void);

  private:
    // General
    float         m_source_x;
    float         m_source_y;
    ScaleMax      m_scaleMax;

    // Texture variables (for display and effects)
    int           m_texMaxDim;
    QSize         m_texSize;
    GLTexture     m_texItem[2];
    int           m_texCur;
    bool          m_tex1First;

    // Info variables
    GLuint        m_texInfo;
    
    // Common effect state variables
    int           m_effect_rotate_direction;
    MythTimer 	  m_effect_frame_time;
    int           m_effect_transition_timeout;
    float         m_effect_transition_timeout_inv;

    // Unshared effect state variables
    float         m_effect_flutter_points[40][40][3];
    float         m_effect_cube_xrot;
    float         m_effect_cube_yrot;
    float         m_effect_cube_zrot;
};

#endif // USING_OPENGL
#endif // GLSINGLEVIEW_H
