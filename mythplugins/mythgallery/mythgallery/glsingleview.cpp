/* ============================================================
 * File  : glsingleview.cpp
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

#include <iostream>
#include <math.h>

#include <qtimer.h>
#include <qimage.h>
#include <qlayout.h>
#include <qsize.h>
#include <qfileinfo.h>
#include <qdir.h>

#include <mythtv/mythcontext.h>

#include "glsingleview.h"
#include "galleryutil.h"

GLSDialog::GLSDialog(const ThumbList& itemList, int pos, int slideShow, 
                     MythMainWindow *parent, const char *name)
    : MythDialog(parent, name)
{
    QBoxLayout *l = new QVBoxLayout(this);
    m_view = new GLSingleView(itemList, pos, slideShow, this);
    l->addWidget(m_view);

    setFocusProxy(m_view);
    m_view->setFocus();
}

// Have to clean up with this dirty hack because
// qglwidget segfaults with a destructive close

void GLSDialog::closeEvent(QCloseEvent *e)
{
    m_view->cleanUp();
    e->accept();
}

GLSingleView::GLSingleView(ThumbList itemList, int pos, int slideShow, 
                           QWidget *parent)
    : QGLWidget(parent)
{
    m_pos      = pos;
    m_itemList = itemList;
    m_movieState  = 0;
    m_slideShow = slideShow;

    // --------------------------------------------------------------------

    setFocusPolicy(QWidget::WheelFocus);

    int   xbase, ybase;
    gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                ybase, screenheight, hmult);
    
    m_w = QMIN( 1024, 1 << (int)ceil(log((float)screenwidth)/log((float)2)) );
    m_h = QMIN( 1024, 1 << (int)ceil(log((float)screenheight)/log((float)2)) );

    // --------------------------------------------------------------------

    // remove all dirs from m_itemList;
    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);

    m_itemList.setAutoDelete(false);
    ThumbItem* item = m_itemList.first();
    while (item) {
        ThumbItem* next = m_itemList.next();
        if (item->isDir) {
            if (recurse)
                GalleryUtil::loadDirectory(m_itemList, item->path, recurse, NULL, NULL);
            m_itemList.remove(item);
        }
        item = next;
    }
    
    // since we remove dirs item position might have changed
    item = itemList.at(m_pos);
    if (item) 
        m_pos = m_itemList.find(item);

    if (!item || (m_pos == -1))
        m_pos = 0;

    // --------------------------------------------------------------------

    m_curr           = 0;
    m_texItem[0].tex = 0;
    m_texItem[1].tex = 0;
    m_tex1First      = true;

    // --------------------------------------------------------------------

    m_sx = m_sy = 0;
    m_zoom = 1.0;       
    
    // ---------------------------------------------------------------

    registerEffects();

    m_effectMethod = 0;
    m_effectRandom = false;
    QString transType = gContext->GetSetting("SlideshowOpenGLTransition");
    if (!transType.isEmpty() && m_effectMap.contains(transType))
        m_effectMethod = m_effectMap[transType];
    
    if (!m_effectMethod || transType == QString("random (gl)")) {
        m_effectMethod = getRandomEffect();
        m_effectRandom = true;
    }
    
    // --------------------------------------------------------------------

    m_delay = gContext->GetNumSetting("SlideshowDelay", 0);
    if (!m_delay)
        m_delay = 2;

    // ---------------------------------------------------------------

    m_tmout         = m_delay * 1000;
    m_effectRunning = false;
    m_running       = false;
    m_texInfo       = 0;
    m_showInfo      = false;

    // ---------------------------------------------------------------

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()),
            SLOT(slotTimeOut()));

    if (slideShow > 1)
        randomFrame();

    if (slideShow) {
        m_running = true;
        m_timer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }
}

GLSingleView::~GLSingleView()
{
    
}

void GLSingleView::cleanUp()
{
    makeCurrent();

    m_timer->stop();
    delete m_timer;

    if (m_texItem[0].tex)
        glDeleteTextures(1, &m_texItem[0].tex);
    if (m_texItem[1].tex)
        glDeleteTextures(1, &m_texItem[1].tex);
}

void GLSingleView::initializeGL()
{
    // Enable Texture Mapping
    glEnable(GL_TEXTURE_2D);
    // Clear The Background Color
    glClearColor(0.0, 0.0, 0.0, 1.0f);

    // Turn Blending On
    glEnable(GL_BLEND);
    // Blending Function For Translucency Based On Source Alpha Value
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // Enable perspective vision
    glClearDepth(1.0f);

    loadImage();
}

void GLSingleView::resizeGL( int w, int h )
{
    // Reset The Current Viewport And Perspective Transformation
    glViewport(0, 0, (GLint)w, (GLint)h); 

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
}

void GLSingleView::paintGL()
{
    if (m_movieState > 0)
    {
        if (m_movieState == 1)
        {
            m_movieState = 2;
            ThumbItem* item = m_itemList.at(m_pos);
            QString path = QString("\"") + item->path + "\"";
            QString cmd = gContext->GetSetting("GalleryMoviePlayerCmd");
            cmd.replace("%s", path);
            myth_system(cmd);
            if (!m_running)
            {
                close();
            }
        }
        return;
    }
 
    glDisable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (m_effectRunning && m_effectMethod) {
        (this->*m_effectMethod)();
    }
    else 
        paintTexture();

    if (glGetError())  
        std::cout << "Oops! I screwed up my OpenGL calls somewhere"
                  << std::endl;
}

void GLSingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled    = false;

    bool wasRunning = m_running;
    m_timer->stop();
    m_running = false;
    gContext->RestoreScreensaver();
    m_effectRunning = false;
    m_tmout = m_delay * 1000;

    bool wasInfo = m_showInfo;
    m_showInfo = false;
    
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", e, actions);

    float scrollX = 0.02;
    float scrollY = 0.02;
    
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT" || action == "UP")
        {
            m_zoom = 1.0;
            m_sx   = 0;
            m_sy   = 0;
            retreatFrame();
            loadImage();
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            m_zoom = 1.0;
            m_sx   = 0;
            m_sy   = 0;
            advanceFrame();
            loadImage();
        }
        else if (action == "ZOOMOUT")
        {
            m_sx   = 0;
            m_sy   = 0;
            if (m_zoom > 0.5) {
                m_zoom = m_zoom /2;
            }
            else
                handled = false;
        }
        else if (action == "ZOOMIN")
        {
            m_sx   = 0;
            m_sy   = 0;
            if (m_zoom < 4.0) {
                m_zoom = m_zoom * 2;
            }
            else
                handled = false;
        }
        else if (action == "FULLSIZE")
        {
            m_sx = 0;
            m_sy = 0;
            if (m_zoom != 1) {
                m_zoom = 1.0;
            }
            else
                handled = false;
        }
        else if (action == "SCROLLLEFT")
        {
            if (m_zoom > 1.0 && m_sx < 1.0) {
                m_sx += scrollX;
                m_sx  = QMIN(m_sx, 1.0);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLRIGHT")
        {
            if (m_zoom > 1.0 && m_sx > -1.0) {
                m_sx -= scrollX;
                m_sx  = QMAX(m_sx, -1.0);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLUP")
        {
            if (m_zoom > 1.0 && m_sy < 1.0) {
                m_sy += scrollY;
                m_sy  = QMIN(m_sy, 1.0);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLDOWN")
        {
            if (m_zoom > 1.0 && m_sy > -1.0) {
                m_sy -= scrollY;
                m_sy  = QMAX(m_sy, -1.0);
            }
            else
                handled = false;
        }
        else if (action == "RECENTER")
        {
            if (m_zoom > 1.0) {
                m_sx = 0.0;
                m_sy = 0.0;
            }
            else
                handled = false;
        }
        else if (action == "UPLEFT")
        {
            if (m_zoom > 1.0) {
                m_sx  =  1.0;
                m_sy  = -1.0;
            }
            else
                handled = false;
        }
        else if (action == "LOWRIGHT")
        {
            if (m_zoom > 1.0) {
                m_sx = -1.0;
                m_sy =  1.0;
            }
            else
                handled = false;
        }
        else if (action == "ROTRIGHT")
        {
            m_sx = 0;
            m_sy = 0;
            rotate(90);
        }
        else if (action == "ROTLEFT")
        {
            m_sx = 0;
            m_sy = 0;
            rotate(-90);
        }
        else if (action == "PLAY")
        {
            m_sx   = 0;
            m_sy   = 0;
            m_zoom = 1.0;
            m_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_showInfo = !wasInfo;
        }
        else 
            handled = false;
    }

    if (m_running) {
        m_timer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }
    
    if (handled) {
        updateGL();
        e->accept();
    }
    else {
        e->ignore();
    }
}

void GLSingleView::paintTexture()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(m_sx, m_sy, 0.0);
    glScalef(m_zoom, m_zoom, 1.0);

    TexItem& t = m_texItem[m_curr];

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(t.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, t.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-t.cx, -t.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(t.cx, -t.cy, 0);
        
        glTexCoord2f(1, 1);
        glVertex3f(t.cx, t.cy, 0);
            
        glTexCoord2f(0, 1);
        glVertex3f(-t.cx, t.cy, 0);
    }
    glEnd();

    if (m_showInfo) {

        createTexInfo();
    
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        
        glBindTexture(GL_TEXTURE_2D, m_texInfo);
        glBegin(GL_QUADS);
        {
            glColor4f(1.0, 1.0, 1.0, 0.72);
            glTexCoord2f(0, 0);
            glVertex3f(-0.75, -0.75, 0);
            
            glTexCoord2f(1, 0);
            glVertex3f(0.75, -0.75, 0);
            
            glTexCoord2f(1, 1);
            glVertex3f(0.75, 0.75, 0);
            
            glTexCoord2f(0, 1);
            glVertex3f(-0.75, 0.75, 0);
        }
        glEnd();

    }

}

void GLSingleView::advanceFrame()
{
    m_pos++;
    if (m_pos >= (int)m_itemList.count())
        m_pos = 0;

    m_tex1First = !m_tex1First;
    m_curr      = (m_curr == 0) ? 1 : 0;
}

void GLSingleView::randomFrame()
{
    int newframe;
    if(m_itemList.count() > 1){
        while((newframe = (int)(rand()/(RAND_MAX+1.0) * m_itemList.count())) ==m_pos);
        m_pos = newframe;
    }

    m_tex1First = !m_tex1First;
    m_curr      = (m_curr == 0) ? 1 : 0;
}

void GLSingleView::retreatFrame()
{
    m_pos--;
    if (m_pos < 0)
        m_pos = m_itemList.count() - 1;

    m_tex1First = !m_tex1First;
    m_curr = (m_curr == 0) ? 1 : 0;
}

void GLSingleView::loadImage()
{
    m_movieState = 0;
    ThumbItem *item = m_itemList.at(m_pos);
    if (!item) {
        std::cerr << "GLSingleView: The impossible happened. No item at "
                  << m_pos << std::endl;
        return;
    }

    if (GalleryUtil::isMovie(item->path)) {
        m_movieState = 1;
        return;
    }

    QImage image(item->path);
    if (!image.isNull()) {

        int a  = m_tex1First ? 0 : 1;
        TexItem& t = m_texItem[a];

        t.item     = item;
        t.angle    = 0;

        t.angle = item->GetRotationAngle();

        t.width  = image.width();
        t.height = image.height();

        if (t.angle%180 != 0) {
            int tmp  = t.width;
            t.width  = t.height;
            t.height = tmp;
        }

        QSize sz(t.width,t.height);
        sz.scale(screenwidth, screenheight, QSize::ScaleMin);
    
        t.cx = (float)sz.width()/(float)screenwidth;
        t.cy = (float)sz.height()/(float)screenheight;

        image = image.smoothScale(m_w, m_h);
        QImage tex = convertToGLFormat(image);

        if (t.tex) {
            glDeleteTextures(1, &t.tex);
        }

        /* create the texture */
        glGenTextures(1, &t.tex);
        glBindTexture(GL_TEXTURE_2D, t.tex);

        /* actually generate the texture */
        glTexImage2D( GL_TEXTURE_2D, 0, 3, tex.width(), tex.height(), 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, tex.bits() );
        /* enable linear filtering  */
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    }
}

void GLSingleView::rotate(int angle)
{
    int& ang = m_texItem[m_curr].angle;

    ang += angle;

    if (ang >= 360)
        ang -= 360;
    if (ang < 0)
        ang += 360;

    ThumbItem *item = m_itemList.at(m_pos);
    if (item) {
        item->SetRotationAngle(ang);

        // Delete thumbnail for this
        if (item->pixmap)
            delete item->pixmap;
        item->pixmap = 0;
    }

    TexItem& t = m_texItem[m_curr];
    
    int tmp  = t.width;
    t.width  = t.height;
    t.height = tmp;

    QSize sz(t.width,t.height);
    sz.scale(screenwidth, screenheight, QSize::ScaleMin);
    
    t.cx = (float)sz.width()/(float)screenwidth;
    t.cy = (float)sz.height()/(float)screenheight;
}
void GLSingleView::registerEffects()
{
    m_effectMap.insert("none", &GLSingleView::effectNone);
    m_effectMap.insert("blend (gl)", &GLSingleView::effectBlend);
    m_effectMap.insert("zoom blend (gl)", &GLSingleView::effectZoomBlend);
    m_effectMap.insert("fade (gl)", &GLSingleView::effectFade);
    m_effectMap.insert("rotate (gl)", &GLSingleView::effectRotate);
    m_effectMap.insert("bend (gl)", &GLSingleView::effectBend);
    m_effectMap.insert("inout (gl)", &GLSingleView::effectInOut);
    m_effectMap.insert("slide (gl)", &GLSingleView::effectSlide);
    m_effectMap.insert("flutter (gl)", &GLSingleView::effectFlutter);
    m_effectMap.insert("cube (gl)", &GLSingleView::effectCube);
}

GLSingleView::EffectMethod GLSingleView::getRandomEffect()
{
    QMap<QString,EffectMethod>  tmpMap(m_effectMap);

    tmpMap.remove("none");
    QStringList t = tmpMap.keys();

    int count = t.count();

    int i = (int)((float)(count)*rand()/(RAND_MAX+1.0));
    QString key = t[i];

    return tmpMap[key];
}

void GLSingleView::effectNone()
{
    paintTexture();
    m_effectRunning = false;
    m_tmout = -1;
    return;
}

void GLSingleView::effectBlend()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx, -ta.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx, -ta.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx, ta.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx, ta.cy, 0);
    }
    glEnd();

    glBegin(GL_QUADS);
    {
        glColor4f(0.0, 0.0, 0.0, 1.0/(100.0)*(float)m_i);
        glVertex3f(-1, -1, 0);
        glVertex3f(1, -1, 0);
        glVertex3f(1, 1, 0);
        glVertex3f(-1, 1, 0);
    }
    glEnd();
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0/(100.0)*(float)m_i);
        glTexCoord2f(0, 0);
        glVertex3f(-tb.cx, -tb.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(tb.cx, -tb.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(tb.cx, tb.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-tb.cx, tb.cy, 0);
    }
    glEnd();

    m_i++;
}

void GLSingleView::effectZoomBlend()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;
    float zf = 0.75;

    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);
	float t=1.0/(100.00)*(float)m_i;
    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0-t);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx*(1.0+zf*t), -ta.cy*(1.0+zf*t), 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx*(1.0+zf*t), -ta.cy*(1.0+zf*t), 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx*(1.0+zf*t), ta.cy*(1.0+zf*t), 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx*(1.0+zf*t), ta.cy*(1.0+zf*t), 0);
    }
    glEnd();
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0/(100.0)*(float)m_i);
        glTexCoord2f(0, 0);
        glVertex3f(-tb.cx, -tb.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(tb.cx, -tb.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(tb.cx, tb.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-tb.cx, tb.cy, 0);
    }
    glEnd();

    m_i++;
}

void GLSingleView::effectRotate()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }
    
    if (m_i == 0) 
        m_dir = (int)((2.0*rand()/(RAND_MAX+1.0)));

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-tb.cx, -tb.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(tb.cx, -tb.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(tb.cx, tb.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-tb.cx, tb.cy, 0);
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float rotate = 360.0/100.0*(float)m_i;
    glRotatef( ((m_dir == 0) ? -1 : 1) * rotate,
               0.0, 0.0, 1.0);
    float scale = 1.0/100.0*(100.0-(float)(m_i));
    glScalef(scale,scale,1.0);
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx, -ta.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx, -ta.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx, ta.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx, ta.cy, 0);
    }
    glEnd();

    m_i++;

}

void GLSingleView::effectBend()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0) 
        m_dir = (int)((2.0*rand()/(RAND_MAX+1.0)));
    
    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-tb.cx, -tb.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(tb.cx, -tb.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(tb.cx, tb.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-tb.cx, tb.cy, 0);
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(90.0/100.0*(float)m_i,
              (m_dir == 0) ? 1.0 : 0.0,
              (m_dir == 1) ? 1.0 : 0.0, 
              0.0);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx, -ta.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx, -ta.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx, ta.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx, ta.cy, 0);
    }
    glEnd();

    m_i++;
}

void GLSingleView::effectFade()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    int a;
    float opacity;
    if (m_i <= 50) {
        a =  (m_curr == 0) ? 1 : 0;
        opacity = 1.0 - 1.0/50.0*(float)(m_i);
    }
    else {
        opacity = 1.0/50.0*(float)(m_i-50.0);
        a = m_curr;
    }

    TexItem& ta = m_texItem[a];

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, opacity);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx, -ta.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx, -ta.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx, ta.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx, ta.cy, 0);
    }
    glEnd();


    m_i++;
    
}

void GLSingleView::effectInOut()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0) {
        m_dir = 1 + (int)((4.0*rand()/(RAND_MAX+1.0)));
    }
    
    int a;
    bool out;
    if (m_i <= 50) {
        a   = (m_curr == 0) ? 1 : 0;
        out = 1;
    }
    else {
        a   = m_curr;
        out = 0;
    }

    TexItem& ta = m_texItem[a];

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float t = out ? 1.0/50.0*(50.0-m_i) : 1.0/50.0*(m_i-50.0);
    glScalef(t, t, 1.0);
    t = 1.0 - t;
    glTranslatef((m_dir % 2 == 0) ? ((m_dir == 2)? 1 : -1) * t : 0.0,
                 (m_dir % 2 == 1) ? ((m_dir == 1)? 1 : -1) * t : 0.0, 
                 0.0);
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx, -ta.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx, -ta.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx, ta.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx, ta.cy, 0);
    }
    glEnd();


    m_i++;
}

void GLSingleView::effectSlide()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0) 
        m_dir = 1 + (int)((4.0*rand()/(RAND_MAX+1.0)));

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-tb.cx, -tb.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(tb.cx, -tb.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(tb.cx, tb.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-tb.cx, tb.cy, 0);
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float trans = 2.0/100.0*(float)m_i;
    glTranslatef((m_dir % 2 == 0) ? ((m_dir == 2)? 1 : -1) * trans : 0.0,
                 (m_dir % 2 == 1) ? ((m_dir == 1)? 1 : -1) * trans : 0.0, 
                 0.0);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-ta.cx, -ta.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(ta.cx, -ta.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(ta.cx, ta.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-ta.cx, ta.cy, 0);
    }
    glEnd();

    m_i++;
}

void GLSingleView::effectFlutter()
{
    if (m_i > 100) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;
    
    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];
    

    if (m_i == 0) {
        for (int x = 0; x<40; x++) {
            for (int y = 0; y < 40; y++) {
                m_points[x][y][0] = (float) (x / 20.0f - 1.0f) * ta.cx;
                m_points[x][y][1] = (float) (y / 20.0f - 1.0f) * ta.cy;
                m_points[x][y][2] = (float) sin((x / 20.0f - 1.0f) * 3.141592654*2.0f)/5.0;
            }
        }
    }        

    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glTexCoord2f(0, 0);
        glVertex3f(-tb.cx, -tb.cy, 0);
        
        glTexCoord2f(1, 0);
        glVertex3f(tb.cx, -tb.cy, 0);
                 
        glTexCoord2f(1, 1);
        glVertex3f(tb.cx, tb.cy, 0);
        
        glTexCoord2f(0, 1);
        glVertex3f(-tb.cx, tb.cy, 0);
    }
    glEnd();
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float rotate = 60.0/100.0*(float)m_i;
    glRotatef(rotate, 1.0f, 0.0f, 0.0f);
    float scale = 1.0/100.0*(100.0-(float)m_i);
    glScalef(scale, scale, scale);
    glTranslatef(1.0/100.0*(float)m_i, 1.0/100.0*(float)m_i, 0.0);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4f(1.0, 1.0, 1.0, 1.0);

        float float_x, float_y, float_xb, float_yb;
        int x, y;
        
        for (x = 0; x < 39; x++)
        {
            for (y = 0; y < 39; y++)
            {
                float_x = (float) x / 40.0f;
                float_y = (float) y / 40.0f;
                float_xb = (float) (x + 1) / 40.0f;
                float_yb = (float) (y + 1) / 40.0f;
                glTexCoord2f(float_x, float_y);
                glVertex3f(m_points[x][y][0], m_points[x][y][1], m_points[x][y][2]);
                glTexCoord2f(float_x, float_yb);
                glVertex3f(m_points[x][y + 1][0], m_points[x][y + 1][1],
                           m_points[x][y + 1][2]);
                glTexCoord2f(float_xb, float_yb);
                glVertex3f(m_points[x + 1][y + 1][0], m_points[x + 1][y + 1][1],
                           m_points[x + 1][y + 1][2]);
                glTexCoord2f(float_xb, float_y);
                glVertex3f(m_points[x + 1][y][0], m_points[x + 1][y][1],
                           m_points[x + 1][y][2]);
            }
        }
    }
    glEnd();

    // wave every two iterations
    if (m_i%2 == 0) {

        float hold;
        int x, y;
        for (y = 0; y < 40; y++)
        {
            hold = m_points[0][y][2];
            for (x = 0; x < 39; x++)
            {
                m_points[x][y][2] = m_points[x + 1][y][2];
            }
            m_points[39][y][2] = hold;
        }
    }
    m_i++;
}

void GLSingleView::effectCube()
{
    int tot = 200;
    int rotStart = 50;
    
    if (m_i > tot) {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    // Enable perspective vision
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;
    
    TexItem& ta = m_texItem[a];
    TexItem& tb = m_texItem[b];


    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float PI = 4.0 * atan(1.0);
    float znear = 3.0;
    float theta = 2.0 * atan2((float)2.0/(float)2.0, (float)znear);
    theta = theta * 180.0/PI;

    glFrustum(-1.0,1.0,-1.0,1.0, znear-0.01,10.0);


    static float xrot;
    static float yrot;
    static float zrot;

    if (m_i == 0) {
        xrot = 0.0;
        yrot = 0.0;
        zrot = 0.0;
    }

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    float trans = 5.0 * (float)((m_i <= tot/2) ? m_i : tot-m_i)/(float)tot;
    glTranslatef(0.0,0.0, -znear - 1.0 - trans);

    glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(yrot, 0.0f, 1.0f, 0.0f);
    

    glBindTexture(GL_TEXTURE_2D, 0);
    glBegin(GL_QUADS);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

        /* Front Face */
        glVertex3f( -1.00f, -1.00f,  0.99f );
        glVertex3f(  1.00f, -1.00f,  0.99f );
        glVertex3f(  1.00f,  1.00f,  0.99f );
        glVertex3f( -1.00f,  1.00f,  0.99f );

        /* Back Face */
        glVertex3f( -1.00f, -1.00f, -0.99f );
        glVertex3f( -1.00f,  1.00f, -0.99f );
        glVertex3f(  1.00f,  1.00f, -0.99f );
        glVertex3f(  1.00f, -1.00f, -0.99f );
  
        /* Top Face */
        glVertex3f( -1.00f,  0.99f, -1.00f );
        glVertex3f( -1.00f,  0.99f,  1.00f );
        glVertex3f(  1.00f,  0.99f,  1.00f );
        glVertex3f(  1.00f,  0.99f, -1.00f );

        /* Bottom Face */
        glVertex3f( -1.00f, -0.99f, -1.00f );
        glVertex3f(  1.00f, -0.99f, -1.00f );
        glVertex3f(  1.00f, -0.99f,  1.00f );
        glVertex3f( -1.00f, -0.99f,  1.00f );

        /* Right face */
        glVertex3f( 0.99f, -1.00f, -1.00f );
        glVertex3f( 0.99f,  1.00f, -1.00f );
        glVertex3f( 0.99f,  1.00f,  1.00f );
        glVertex3f( 0.99f, -1.00f,  1.00f );

        /* Left Face */
        glVertex3f( -0.99f, -1.00f, -1.00f );
        glVertex3f( -0.99f, -1.00f,  1.00f );
        glVertex3f( -0.99f,  1.00f,  1.00f );
        glVertex3f( -0.99f,  1.00f, -1.00f );
        
    }
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(ta.angle, 0.0, 0.0, 1.0);


    glBindTexture(GL_TEXTURE_2D, ta.tex);
    glBegin(GL_QUADS);
    {
        glColor4d(1.0, 1.0, 1.0, 1.0);

        // Front Face 
        glTexCoord2f( 0.0f, 0.0f ); glVertex3f( -ta.cx, -ta.cy,  1.00f );
        glTexCoord2f( 1.0f, 0.0f ); glVertex3f(  ta.cx, -ta.cy,  1.00f );
        glTexCoord2f( 1.0f, 1.0f ); glVertex3f(  ta.cx,  ta.cy,  1.00f );
        glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -ta.cx,  ta.cy,  1.00f );

  
        // Top Face 
        glTexCoord2f( 1.0f, 1.0f ); glVertex3f( -ta.cx,  1.00f, -ta.cy );
        glTexCoord2f( 1.0f, 0.0f ); glVertex3f( -ta.cx,  1.00f,  ta.cy );
        glTexCoord2f( 0.0f, 0.0f ); glVertex3f(  ta.cx,  1.00f,  ta.cy );
        glTexCoord2f( 0.0f, 1.0f ); glVertex3f(  ta.cx,  1.00f, -ta.cy );

        // Bottom Face 
        glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -ta.cx, -1.00f, -ta.cy );
        glTexCoord2f( 1.0f, 1.0f ); glVertex3f(  ta.cx, -1.00f, -ta.cy );
        glTexCoord2f( 1.0f, 0.0f ); glVertex3f(  ta.cx, -1.00f,  ta.cy );
        glTexCoord2f( 0.0f, 0.0f ); glVertex3f( -ta.cx, -1.00f,  ta.cy );

        // Right face 
        glTexCoord2f( 0.0f, 0.0f ); glVertex3f( 1.00f, -ta.cx, -ta.cy );
        glTexCoord2f( 0.0f, 1.0f ); glVertex3f( 1.00f, -ta.cx,  ta.cy );
        glTexCoord2f( 1.0f, 1.0f ); glVertex3f( 1.00f,  ta.cx,  ta.cy );
        glTexCoord2f( 1.0f, 0.0f ); glVertex3f( 1.00f,  ta.cx, -ta.cy );

        // Left Face 
        glTexCoord2f( 1.0f, 0.0f ); glVertex3f( -1.00f, -ta.cx, -ta.cy );
        glTexCoord2f( 0.0f, 0.0f ); glVertex3f( -1.00f,  ta.cx, -ta.cy );
        glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -1.00f,  ta.cx,  ta.cy );
        glTexCoord2f( 1.0f, 1.0f ); glVertex3f( -1.00f, -ta.cx,  ta.cy );
        
    }
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(tb.angle, 0.0, 0.0, 1.0);

    glBindTexture(GL_TEXTURE_2D, tb.tex);
    glBegin(GL_QUADS);
    {
        glColor4d(1.0, 1.0, 1.0, 1.0);
        
        // Back Face 
        glTexCoord2f( 1.0f, 0.0f ); glVertex3f( -tb.cx, -tb.cy, -1.00f );
        glTexCoord2f( 1.0f, 1.0f ); glVertex3f( -tb.cx,  tb.cy, -1.00f );
        glTexCoord2f( 0.0f, 1.0f ); glVertex3f(  tb.cx,  tb.cy, -1.00f );
        glTexCoord2f( 0.0f, 0.0f ); glVertex3f(  tb.cx, -tb.cy, -1.00f );
    }
    glEnd();
    
    if (m_i >= rotStart && m_i < (tot-rotStart)) {
        xrot += 360.0f/(float)(tot-2*rotStart);
        yrot += 180.0f/(float)(tot-2*rotStart);
    }

    m_i++;

}

void GLSingleView::slotTimeOut()
{
    if (!m_effectMethod) {
        std::cerr << "GLSlideShow: No transition method"
                  << std::endl;
        return;
    }

    if (m_effectRunning) {
        m_tmout = 10;
    }
    else {
        if (m_tmout == -1) {
            // effect was running and is complete now
            // run timer while showing current image
            m_tmout = m_delay * 1000;
            m_i     = 0;
        }
        else {

            // timed out after showing current image
            // load next image and start effect

            if (m_effectRandom)
                m_effectMethod = getRandomEffect();

            if (m_slideShow > 1)
                randomFrame();
            else
                advanceFrame();

            bool wasMovie = m_movieState > 0;
            loadImage();
            bool isMovie = m_movieState > 0;
            // If transitioning to/from a movie, don't do an effect,
            // and shorten timeout
            if (wasMovie || isMovie)
            {
                m_tmout = 1;
            }
            else
            {
                m_tmout = 10;
                m_effectRunning = true;
                m_i = 0;
            }
        }
    }

    updateGL();
    m_timer->start(m_tmout, true);
}

void GLSingleView::createTexInfo()
{
    if (m_texInfo)
        glDeleteTextures(1, &m_texInfo);

    TexItem& t = m_texItem[m_curr];
    if (!t.tex || !t.item)
        return;
    
    QPixmap pix(512, 512);

    QPainter p(&pix, this);
    p.fillRect(0,0,pix.width(),pix.height(),Qt::black);
    p.setPen(Qt::white);

    QFileInfo fi(t.item->path);
    QString info(t.item->name);

    info += "\n\n" + tr("Folder: ") + fi.dir().dirName();
    info += "\n" + tr("Created: ") + fi.created().toString();
    info += "\n" + tr("Modified: ") + fi.lastModified().toString();
    info += "\n" + QString(tr("Bytes") + ": %1").arg(fi.size());
    info += "\n" + QString(tr("Width") + ": %1 " + tr("pixels"))
             .arg(t.width);
    info += "\n" + QString(tr("Height") + ": %1 " + tr("pixels"))
            .arg(t.height);
    info += "\n" + QString(tr("Pixel Count") + ": %1 " + 
                           tr("megapixels"))
            .arg((float) t.width * t.height / 1000000,
                 0, 'f', 2);
    info += "\n" + QString(tr("Rotation Angle") + ": %1 " +
                           tr("degrees")).arg(t.angle);
    p.drawText(10, 10, pix.width()-20, pix.height()-20,
               Qt::AlignLeft, info);
    p.end();

    QImage img(pix.convertToImage());
    img = img.convertDepth(32);

    QImage tex = convertToGLFormat(img);

    /* create the texture */
    glGenTextures(1, &m_texInfo);
    glBindTexture(GL_TEXTURE_2D, m_texInfo);
    /* actually generate the texture */
    glTexImage2D( GL_TEXTURE_2D, 0, 3, tex.width(), tex.height(), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, tex.bits() );
    /* enable linear filtering  */
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
}

