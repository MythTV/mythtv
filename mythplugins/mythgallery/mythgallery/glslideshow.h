/* ============================================================
 * File  : glslideshow.h
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2004-01-10
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

#ifndef GLSLIDESHOW_H
#define GLSLIDESHOW_H

#include <qgl.h>
#include <qmap.h>

#include "iconview.h"

class QImage;
class GLSlideShow;

class GLSSDialog : public MythDialog
{
public:
    
    GLSSDialog(QSqlDatabase *db, const ThumbList& itemList,
               int pos, MythMainWindow *parent, const char *name=0);

protected:

    void closeEvent(QCloseEvent *e);
    
private:

    GLSlideShow *m_sshow;
};

class GLSlideShow : public QGLWidget
{
    Q_OBJECT

public:

    GLSlideShow(QSqlDatabase *db, const ThumbList& itemList,
                int pos, QWidget *parent);
    ~GLSlideShow();

    void cleanUp();
    
protected:

    void initializeGL();
    void resizeGL( int w, int h );
    void paintGL();

    void keyPressEvent(QKeyEvent *e);

private:

    void loadNext();
    void paintTexture(int index);
    void montage(QImage& top, QImage& bot);

    void effectBlend();
    void effectFade();
    void effectRotate();
    void effectBend();
    void effectInOut();
    void effectSlide();
    void effectFlutter();

    typedef      void (GLSlideShow::*EffectMethod)();

    void         registerEffects();
    EffectMethod getRandomEffect();

    EffectMethod               m_effectMethod;
    QMap<QString,EffectMethod> m_effectMap;
    bool                       m_effectRandom;

    QSqlDatabase *m_db;

    GLuint     m_texture[2];
    int        m_w;
    int        m_h;

    int        m_pos;
    int        m_curr;
    int        m_tex1First;
    int        m_tmout;
    int        m_delay;
    bool       m_effectRunning;
    bool       m_paused;

    QTimer    *m_timer;
    ThumbList  m_itemList;

    int        m_i;
    int        m_dir;
    float      m_points[40][40][3];

    int        screenwidth, screenheight;

private slots:

    void slotTimeOut();

};

#endif /* GLSLIDESHOW_H */



