/* ============================================================
 * File  : glslideshow.cpp
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

#include <iostream>
#include <math.h>

#include <qsqldatabase.h>
#include <qtimer.h>
#include <qimage.h>
#include <qlayout.h>

#include "glslideshow.h"

GLSSDialog::GLSSDialog(QSqlDatabase *db, const ThumbList& itemList,
                       int pos, MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    QBoxLayout *l = new QVBoxLayout(this);
    m_sshow = new GLSlideShow(db, itemList, pos, this);
    l->addWidget(m_sshow);
    setFocusProxy(m_sshow);
    m_sshow->setFocus();
}

// Have to clean up with this dirty hack because
// qglwidget segfaults with a destructive close

void GLSSDialog::closeEvent(QCloseEvent *e)
{
    m_sshow->cleanUp();
    e->accept();
}

GLSlideShow::GLSlideShow(QSqlDatabase *db, const ThumbList& itemList,
                         int pos, QWidget* parent)
    : QGLWidget(parent)
{
    m_db       = db;
    m_pos      = pos;
    m_itemList = itemList;

    // --------------------------------------------------------------------
    
    setFocusPolicy(QWidget::WheelFocus);
    
    int   xbase, ybase;
    float wmult, hmult;
    gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                ybase, screenheight, hmult);
    
    m_w = QMIN( 1024, 1 << (int)ceil(log((float)screenwidth)/log((float)2)) );
    m_h = QMIN( 1024, 1 << (int)ceil(log((float)screenheight)/log((float)2)) );

    // ---------------------------------------------------------------

    registerEffects();

    // ---------------------------------------------------------------

    m_itemList.setAutoDelete(false);
    // remove all dirs from m_itemList;
    ThumbItem* item = m_itemList.first();
    while (item) {
        ThumbItem* next = m_itemList.next();
        if (item->isDir)
            m_itemList.remove();
        item = next;
    }
    
    // ---------------------------------------------------------------

    m_effectMethod = 0;
    m_effectRandom = false;
    QString transType = gContext->GetSetting("SlideshowOpenGLTransition");
    if (!transType.isEmpty() && m_effectMap.contains(transType))
        m_effectMethod = m_effectMap[transType];
    
    if (!m_effectMethod || transType == "random") {
        m_effectMethod = getRandomEffect();
        m_effectRandom = true;
    }

    // ---------------------------------------------------------------

    m_delay = gContext->GetNumSetting("SlideshowDelay", 0);
    if (!m_delay)
        m_delay = 2;

    // ---------------------------------------------------------------

    m_curr          = 0;
    m_tmout         = m_delay * 1000;
    m_texture[0]    = m_texture[1] = 0;
    m_effectRunning = false;
    m_paused        = false;

    // ---------------------------------------------------------------

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()),
            SLOT(slotTimeOut()));
    m_timer->start(m_tmout, true);
}

GLSlideShow::~GLSlideShow()
{
}

void GLSlideShow::cleanUp()
{
    makeCurrent();

    m_timer->stop();
    delete m_timer;

    if (m_texture[0])
        glDeleteTextures(1, &m_texture[0]);
    if (m_texture[1])
        glDeleteTextures(1, &m_texture[1]);
}

void GLSlideShow::initializeGL()
{
    // Enable Texture Mapping
    glEnable(GL_TEXTURE_2D);
    // Clear The Background Color
    glClearColor(0.0, 0.0, 0.0, 0.0f);

    // Turn Blending On
    glEnable(GL_BLEND);
    // Blending Function For Translucency Based On Source Alpha Value ( NEW )
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    m_tex1First = true;
    loadNext();
}

void GLSlideShow::resizeGL( int w, int h )
{
    // Reset The Current Viewport And Perspective Transformation
    glViewport(0, 0, (GLint)w, (GLint)h); 

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
}

void GLSlideShow::paintGL()
{
    
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (m_effectRunning) {
        (this->*m_effectMethod)();
    }
    else 
        paintTexture(m_curr);

    // TODO: disable rendering text for now. nvidia drivers
    // are crashing with this
    /*
    if (m_paused) {
        glLoadIdentity();
        renderText(20, 20, "Paused"); 
    }
    */
}

void GLSlideShow::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", e, actions);

    bool handled = false;
    
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        handled = true;

        QString action = actions[i];
        if (action == "PLAY")
        {
            m_paused = !m_paused;
            if (m_paused) {
                m_timer->stop();
            }
            else {
                slotTimeOut();
            }

        }
        else 
            handled = false;
    }

    if (handled) {
        e->accept();
        updateGL();
    }
    else
        e->ignore();
}

void GLSlideShow::slotTimeOut()
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

            m_tex1First = !m_tex1First;
            m_curr = m_tex1First ? 0 : 1;
            loadNext();

            m_tmout = 10;
            m_effectRunning = true;
            m_i = 0;
        }
    }

    updateGL();
    m_timer->start(m_tmout, true);
}

void GLSlideShow::loadNext()
{
    ThumbItem *item = m_itemList.at(m_pos);
    if (!item) {
        std::cerr << "The impossible happened"
                  << std::endl;
        return;
    }

    QImage image(item->path);
    if (!image.isNull()) {

        QImage black(screenwidth, screenheight, 32);
        black.fill(Qt::black.rgb());

        QString queryStr = "SELECT angle FROM gallerymetadata WHERE "
                           "image=\"" + item->path + "\";";
        QSqlQuery query = m_db->exec(queryStr);
        
        if (query.isActive()  && query.numRowsAffected() > 0) 
        {
            query.next();
            int rotateAngle = query.value(0).toInt();
            if (rotateAngle != 0) {
                    QWMatrix matrix;
                    matrix.rotate(rotateAngle);
                    image = image.xForm(matrix);
            }
        }

        image = image.smoothScale(screenwidth, screenheight,
                                  QImage::ScaleMin);
        montage(image, black);

        black = black.smoothScale(m_w,m_h);

        QImage tex = convertToGLFormat(black);

        int a = 0;
        if (!m_tex1First)
            a = 1;

        if (m_texture[a]) {
            glDeleteTextures(a, &m_texture[a]);
        }

        /* create the texture */
        glGenTextures(1, &m_texture[a]);
        glBindTexture(GL_TEXTURE_2D, m_texture[a]);
        /* actually generate the texture */
        glTexImage2D( GL_TEXTURE_2D, 0, 3, tex.width(), tex.height(), 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, tex.bits() );
        /* enable linear filtering  */
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    }
    
    m_pos++;
    if (m_pos >= (int)m_itemList.count())
        m_pos = 0;
}

void GLSlideShow::registerEffects()
{
    m_effectMap.insert("blend (gl)", &GLSlideShow::effectBlend);
    m_effectMap.insert("fade (gl)", &GLSlideShow::effectFade);
    m_effectMap.insert("rotate (gl)", &GLSlideShow::effectRotate);
    m_effectMap.insert("bend (gl)", &GLSlideShow::effectBend);
    m_effectMap.insert("inout (gl)", &GLSlideShow::effectInOut);
    m_effectMap.insert("slide (gl)", &GLSlideShow::effectSlide);
    m_effectMap.insert("flutter (gl)", &GLSlideShow::effectFlutter);
}

GLSlideShow::EffectMethod GLSlideShow::getRandomEffect()
{
    QStringList t = m_effectMap.keys();

    int count = t.count();

    int i = (int)((float)(count)*rand()/(RAND_MAX+1.0));
    QString key = t[i];

    return m_effectMap[key];
}

void GLSlideShow::paintTexture(int index)
{
    glBindTexture(GL_TEXTURE_2D, m_texture[index]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
        
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
            
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();
}

void GLSlideShow::effectBlend()
{
    if (m_i > 100) {
        paintTexture(m_curr);
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;
    
    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, m_texture[b]);
    glColor4d(1.0, 1.0, 1.0, 1.0/(100.0)*(float)m_i);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    m_i++;
}

void GLSlideShow::effectRotate()
{
    if (m_i > 100) {
        paintTexture(m_curr);
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }
    
    if (m_i == 0) 
        m_dir = (int)((2.0*rand()/(RAND_MAX+1.0)));

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    glBindTexture(GL_TEXTURE_2D, m_texture[b]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    float rotate = 360.0/100.0*(float)m_i;
    glRotatef( ((m_dir == 0) ? -1 : 1) * rotate,
               0.0, 0.0, 1.0);
    float scale = 1.0/100.0*(100.0-(float)(m_i));
    glScalef(scale,scale,scale);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3f(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3f(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3f(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3f(-1, 1, 0);
    }
    glEnd();


    m_i++;

}

void GLSlideShow::effectBend()
{
    if (m_i > 100) {
        paintTexture(m_curr);
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0) 
        m_dir = (int)((2.0*rand()/(RAND_MAX+1.0)));
    
    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    glBindTexture(GL_TEXTURE_2D, m_texture[b]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {

        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    float rotate = 90.0/100.0*(float)m_i;
    glRotatef(rotate,
              (m_dir == 0) ? 1.0 : 0.0,
              (m_dir == 1) ? 1.0 : 0.0, 
              0.0);

    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {

        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    m_i++;
}

void GLSlideShow::effectFade()
{
    if (m_i > 100) {
        paintTexture(m_curr);
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

    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    glColor4d(1.0, 1.0, 1.0, opacity);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();


    m_i++;
    
}

void GLSlideShow::effectInOut()
{
    if (m_i > 100) {
        paintTexture(m_curr);
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    int a;
    bool out;
    if (m_i <= 50) {
        a     = (m_curr == 0) ? 1 : 0;
        out = 1;
    }
    else {
        a     = m_curr;
        out = 0;
    }

    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    float t = out ? 1.0/100.0*m_i : 1.0/100.0*(100.0-m_i);
    t *= 1.5;
    glTranslatef(-t, -t, 0.0);
    glBegin(GL_QUADS);
    {
        
        float c = out ? 1.0 - 1.0/50.0 * (float)m_i :
                  1.0 - (1.0/50.0) * (float)(100.0-m_i);
        glTexCoord2d(0, 0);
        glVertex3d(-c, -c, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(c, -c, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(c, c, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-c, c, 0);
    }
    glEnd();


    m_i++;
}

void GLSlideShow::effectSlide()
{
    if (m_i > 100) {
        paintTexture(m_curr);
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0) 
        m_dir = 1 + (int)((4.0*rand()/(RAND_MAX+1.0)));

    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;

    glBindTexture(GL_TEXTURE_2D, m_texture[b]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {

        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    float trans = 2.0/100.0*(float)m_i;
    glTranslatef((m_dir % 2 == 0) ? ((m_dir == 2)? 1 : -1) * trans : 0.0,
                 (m_dir % 2 == 1) ? ((m_dir == 1)? 1 : -1) * trans : 0.0, 
                 0.0);

    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {

        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    m_i++;
}

void GLSlideShow::effectFlutter()
{
    if (m_i > 100) {
        paintTexture(m_curr);
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0) {
        for (int x = 0; x<40; x++) {
            for (int y = 0; y < 40; y++) {
                m_points[x][y][0] = (float) (x / 20.0f - 1.0f);
                m_points[x][y][1] = (float) (y / 20.0f - 1.0f);
                m_points[x][y][2] = (float) sin((x / 20.0f - 1.0f) * 3.141592654*2.0f)/5.0;
            }
        }
    }        

    
    int a = (m_curr == 0) ? 1 : 0;
    int b =  m_curr;
    
    glBindTexture(GL_TEXTURE_2D, m_texture[b]);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {
        glTexCoord2d(0, 0);
        glVertex3d(-1, -1, 0);
        
        glTexCoord2d(1, 0);
        glVertex3d(1, -1, 0);
                 
        glTexCoord2d(1, 1);
        glVertex3d(1, 1, 0);
        
        glTexCoord2d(0, 1);
        glVertex3d(-1, 1, 0);
    }
    glEnd();

    
    glBindTexture(GL_TEXTURE_2D, m_texture[a]);
    float rotate = 60.0/100.0*(float)m_i;
    glRotatef(rotate, 1.0f, 0.0f, 0.0f);
    float scale = 1.0/100.0*(100.0-(float)m_i);
    glScalef(scale, scale, scale);
    glTranslatef(1.0/100.0*(float)m_i, 1.0/100.0*(float)m_i, 0.0);
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    {
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

void GLSlideShow::montage(QImage& top, QImage& bot)
{
    int tw = top.width(); int th = top.height();
    int bw = bot.width(); int bh = bot.height();

    if (tw > bw || th > bh)
        qFatal("Top Image should be smaller or same size as Bottom Image");
    
    if (top.depth() != 32) top = top.convertDepth(32);
    if (bot.depth() != 32) bot = bot.convertDepth(32);

    int sw = bw/2 - tw/2; //int ew = bw/2 + tw/2;
    int sh = bh/2 - th/2; int eh = bh/2 + th/2;
    

    unsigned int *tdata = (unsigned int*) top.scanLine(0);
    unsigned int *bdata = 0;

    for (int y = sh; y < eh; y++) {

        bdata = (unsigned int*) bot.scanLine(y) + sw;
        for (int x = 0; x < tw; x++) {
            *(bdata++) = *(tdata++);
         }

    }
}
