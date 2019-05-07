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
#include <mthread.h>
#include <mythdate.h>

// MythGallery headers
#include "mythdialogs.h"
#include "imageview.h"
#include "iconview.h"
#include "sequence.h"
#include "gltexture.h"

// QT headers
#include <QGLWidget>

class QImage;
class QTimer;

class GLSingleView;
class KenBurnsImageLoader;

class GLSDialog : public MythDialog
{
  public:
    GLSDialog(const ThumbList& itemList,
              int *pos, int slideShow, int sortOrder,
              MythMainWindow *parent, const char *name="GLSDialog");

  protected:
    void closeEvent(QCloseEvent *e) override; // QWidget

  private:
    GLSingleView *m_view {nullptr};
};

class GLSingleView : public QGLWidget, public ImageView
{
    Q_OBJECT

  public:
    GLSingleView(const ThumbList& itemList, int *pos, int slideShow, int sortorder,
                 QWidget *parent);
    ~GLSingleView();

    void CleanUp(void);
    void Ready(){m_effect_kenBurns_image_ready = true;}
    void LoadImage(QImage image, QSize origSize);


  protected:
    void initializeGL(void) override; // QGLWidget

    // Commands
    void Rotate(int angle) override; // ImageView
    void DisplayNext(bool reset, bool loadImage) override; // ImageView
    void DisplayPrev(bool reset, bool loadImage) override; // ImageView
    void Load(void) override; // ImageView
    void resizeGL(int w, int h) override; // QGLWidget

    void paintGL(void) override; // QGLWidget
    void paintTexture(void);
    void createTexInfo(void);
    void keyPressEvent(QKeyEvent *e) override; // QWidget
    void checkPosition(void);

    // Sets
    void SetZoom(float zoom) override; // ImageView
    void SetTransitionTimeout(int timeout);

    // Gets
    int GetNearestGLTextureSize(int) const;

    void RegisterEffects(void) override; // ImageView
    void RunEffect(const QString &effect) override; // ImageView

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
    void EffectKenBurns(void);

  private:
    float FindMaxScale(float x_loc, float y_loc);
    void FindRandXY(float &x_loc, float &y_loc);

  private slots:
    void SlideTimeout(void);

  private:
    // General
    float         m_source_x      {0.0F};
    float         m_source_y      {0.0F};
    ScaleMax      m_scaleMax      {kScaleToFit};

    // Texture variables (for display and effects)
    int           m_texMaxDim     {512};
    QSize         m_texSize       {512,512};
    GLTexture     m_texItem[2]    ;
    int           m_texCur        {0};
    bool          m_tex1First     {true};

    // Info variables
    GLuint        m_texInfo       {0};

    // Common effect state variables
    int           m_effect_rotate_direction                {0};
    MythTimer     m_effect_frame_time;
    int           m_effect_transition_timeout              {2000};
    float         m_effect_transition_timeout_inv          {1.0F / 2000};

    // Unshared effect state variables
    float         m_effect_flutter_points[40][40][3];
    float         m_effect_cube_xrot                       {0.0F};
    float         m_effect_cube_yrot                       {0.0F};
    float         m_effect_cube_zrot                       {0.0F};
    float         m_effect_kenBurns_location_x[2];
    float         m_effect_kenBurns_location_y[2];
    int           m_effect_kenBurns_projection[2];
    MythTimer     m_effect_kenBurns_image_time[2];
    float         m_effect_kenBurns_image_timeout          {0.0F};
    KenBurnsImageLoader *m_effect_kenBurns_imageLoadThread {nullptr};
    bool          m_effect_kenBurns_image_ready            {true};
    QImage        m_effect_kenBurns_image;
    QSize         m_effect_kenBurns_orig_image_size;
    ThumbItem     *m_effect_kenBurns_item                  {nullptr};
    bool          m_effect_kenBurns_initialized            {false};
    bool          m_effect_kenBurns_new_image_started      {true};

};

class KenBurnsImageLoader : public MThread
{
public:
    KenBurnsImageLoader(GLSingleView *singleView, QSize texSize, QSize screenSize)
        : MThread("KenBurnsImageLoader"),
          m_singleView(singleView),
          m_screenSize(screenSize),
          m_texSize(texSize) {}
    void run() override; // MThread
private:
    GLSingleView *m_singleView {nullptr};
    QSize         m_screenSize;
    QSize         m_texSize;

};

#endif // USING_OPENGL
#endif // GLSINGLEVIEW_H
