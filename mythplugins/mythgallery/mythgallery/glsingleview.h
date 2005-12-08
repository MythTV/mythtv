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

#include "iconview.h"
#include "sequence.h"

class QImage;
class QTimer;

class GLSingleView;

class GLSDialog : public MythDialog
{
public:
    
    GLSDialog(const ThumbList& itemList, int pos, int slideShow, 
              MythMainWindow *parent, const char *name=0);

protected:

    void closeEvent(QCloseEvent *e);
    
private:

    GLSingleView *m_view;
};

class GLSingleView : public QGLWidget
{
    Q_OBJECT

public:

    GLSingleView(ThumbList itemList, int pos, int slideShow, QWidget *parent);
    ~GLSingleView();

    void cleanUp();

protected:

    void initializeGL();
    void resizeGL( int w, int h );
    void paintGL();

    void keyPressEvent(QKeyEvent *e);

private:

    typedef struct {
        GLuint        tex;
        float         cx, cy;
        int           width, height;
        int           angle;
        ThumbItem    *item;
    } TexItem;

    int           m_pos;
    ThumbList     m_itemList;

    int           m_movieState;
    int           screenwidth, screenheight;
    float         wmult, hmult;
    int           m_w, m_h;

    TexItem       m_texItem[2];
    int           m_curr;
    bool          m_tex1First;

    float         m_zoom;
    float         m_sx, m_sy;

    QTimer       *m_timer;
    int           m_tmout;
    int           m_delay;
    bool          m_effectRunning;
    bool          m_running;
    int           m_slideShow;

    GLuint        m_texInfo;
    bool          m_showInfo;
    
    int           m_i;
    int           m_dir;
    float         m_points[40][40][3];

    typedef void               (GLSingleView::*EffectMethod)();
    EffectMethod                m_effectMethod;
    QMap<QString,EffectMethod>  m_effectMap;
    bool                        m_effectRandom;
    SequenceBase               *m_sequence;

private:
    
    void  advanceFrame();
    void  retreatFrame();
    void  loadImage();
    void  paintTexture();
    void  rotate(int angle);
    void  createTexInfo();

    void  registerEffects();
    EffectMethod getRandomEffect();

    void effectNone();
    void effectBlend();
    void effectZoomBlend();
    void effectFade();
    void effectRotate();
    void effectBend();
    void effectInOut();
    void effectSlide();
    void effectFlutter();
    void effectCube();

private slots:

    void slotTimeOut();
    
};

#endif /* GLSINGLEVIEW_H */
