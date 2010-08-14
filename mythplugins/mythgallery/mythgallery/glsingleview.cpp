/* ============================================================
 * File  : glsingleview.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2004-01-13
 * Description : 
 * 
 * Copyright 2004 by Renchi Raju
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

// ANSI C headers
#include <cmath>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QTimer>
#include <QImage>
#include <QDir>
#include <QPainter>

// MythTV plugin headers
#include <mythcontext.h>
#include <util.h>
#include <mythuihelper.h>

// MythGallery headers
#include "glsingleview.h"
#include "galleryutil.h"

#define LOC QString("GLView: ")
#define LOC_ERR QString("GLView, Error: ")

GLSDialog::GLSDialog(const ThumbList& itemList,
                     int *pos, int slideShow, int sortOrder,
                     MythMainWindow *parent, const char *name)
    : MythDialog(parent, name)
{
    QBoxLayout *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    m_view = new GLSingleView(itemList, pos, slideShow, sortOrder, this);
    l->addWidget(m_view);

    setFocusProxy(m_view);
    m_view->setFocus();
}

// Have to clean up with this dirty hack because
// qglwidget segfaults with a destructive close

void GLSDialog::closeEvent(QCloseEvent *e)
{
    m_view->CleanUp();
    e->accept();

    accept();
}

GLSingleView::GLSingleView(ThumbList itemList, int *pos, int slideShow,
                           int sortorder, QWidget *parent)
    : QGLWidget(parent),
      ImageView(itemList, pos, slideShow, sortorder),
      // General
      m_source_x(0.0f),
      m_source_y(0.0f),
      m_scaleMax(kScaleToFit),

      // Texture variables (for display and effects)
      m_texMaxDim(512),
      m_texSize(512,512),
      m_texCur(0),
      m_tex1First(true),

      // Info variables
      m_texInfo(0),

      // Common effect state variables
      m_effect_rotate_direction(0),
      m_effect_transition_timeout(2000),
      m_effect_transition_timeout_inv(1.0f / m_effect_transition_timeout),

      // Unshared effect state variables
      m_effect_cube_xrot(0.0f),
      m_effect_cube_yrot(0.0f),
      m_effect_cube_zrot(0.0f)
{
    m_scaleMax = (ScaleMax) gCoreContext->GetNumSetting("GalleryScaleMax", 0);

    m_slideshow_timer = new QTimer(this);
    RegisterEffects();

    // --------------------------------------------------------------------

    setFocusPolicy(Qt::WheelFocus);

    // --------------------------------------------------------------------

    QString transType = gCoreContext->GetSetting("SlideshowOpenGLTransition");
    if (!transType.isEmpty() && m_effect_map.contains(transType))
        m_effect_method = m_effect_map[transType];

    if (m_effect_method.isEmpty() || transType == QString("random (gl)"))
    {
        m_effect_method = GetRandomEffect();
        m_effect_random = true;
    }

    SetTransitionTimeout(gCoreContext->GetNumSetting(
                             "SlideshowOpenGLTransitionLength", 2000));

    // --------------------------------------------------------------------

    connect(m_slideshow_timer, SIGNAL(timeout()), this, SLOT(SlideTimeout()));

    // --------------------------------------------------------------------

    if (slideShow)
    {
        m_slideshow_running = true;
        m_slideshow_timer->stop();
        m_slideshow_timer->setSingleShot(true);
        m_slideshow_timer->start(m_slideshow_frame_delay_state);
        GetMythUI()->DisableScreensaver();
    }
}

GLSingleView::~GLSingleView()
{
    // save the current m_scaleMax setting so we can restore it later
    gCoreContext->SaveSetting("GalleryScaleMax", m_scaleMax);
    CleanUp();
}

void GLSingleView::CleanUp(void)
{
    makeCurrent();

    if (m_slideshow_timer)
    {
        m_slideshow_timer->stop();
        m_slideshow_timer->deleteLater();
        m_slideshow_timer = NULL;
    }

    m_texItem[0].Deinit();
    m_texItem[1].Deinit();

    if (m_texInfo)
        glDeleteTextures(1, &m_texInfo);
}

void GLSingleView::initializeGL(void)
{
    // Enable Texture Mapping
    glEnable(GL_TEXTURE_2D);
    // Clear The Background Color
    glClearColor(0.0, 0.0, 0.0, 1.0f);

    // Turn Blending On
    glEnable(GL_BLEND);
    // Blending Function For Translucency Based On Source Alpha Value
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable perspective vision
    glClearDepth(1.0f);

    GLint param;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &param);
    m_texMaxDim = param;

    m_texSize = QSize(GetNearestGLTextureSize(m_screenSize.width()),
                      GetNearestGLTextureSize(m_screenSize.height()));

    Load();
}

void GLSingleView::resizeGL(int w, int h)
{
    // Reset The Current Viewport And Perspective Transformation
    glViewport(0, 0, (GLint)w, (GLint)h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
}

void GLSingleView::paintGL(void)
{
    if (m_movieState > 0)
    {
        if (m_movieState == 1)
        {
            m_movieState = 2;
            ThumbItem* item = m_itemList.at(m_pos);
            QString cmd = gCoreContext->GetSetting("GalleryMoviePlayerCmd");

            if ((cmd.indexOf("internal", 0, Qt::CaseInsensitive) > -1) ||
                (cmd.length() < 1))
            {
                cmd = "Internal";
                GetMythMainWindow()->HandleMedia(cmd, item->GetPath());
            }
            else
            {
                QString path = QString("\"%1\"").arg(item->GetPath());

                cmd.replace("%s", path);
                myth_system(cmd);
            }

            if (!m_slideshow_running)
            {
                if (item)
                {
                    QImage image;
                    GetScreenShot(image, item);
                    if (image.isNull())
                        return;

                    image = image.scaled(800, 600);

                    // overlay "Press SELECT to play again" text
                    QPainter p(&image);
                    QRect rect = QRect(20, image.height() - 100, image.width() - 40, 80);
                    p.fillRect(rect, QBrush(QColor(0,0,0,100)));
                    p.setFont(QFont("Arial", 25, QFont::Bold));
                    p.setPen(QColor(255,255,255));
                    p.drawText(rect, Qt::AlignCenter, tr("Press SELECT to play again"));
                    p.end();

                    int a = m_tex1First ? 0 : 1;
                    m_texItem[a].SetItem(item, image.size());
                    m_texItem[a].ScaleTo(m_screenSize, m_scaleMax);
                    m_texItem[a].Init(convertToGLFormat(
                        image.scaled(m_texSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                }
            }
        }
    }

    glDisable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (m_effect_running && !m_effect_method.isEmpty())
    {
        RunEffect(m_effect_method);
    }
    else
    {
        paintTexture();
    }

    if (glGetError())
        VERBOSE(VB_GENERAL, LOC_ERR + "OpenGL error detected");
}

void GLSingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled    = false;

    bool wasRunning = m_slideshow_running;
    m_slideshow_timer->stop();
    m_slideshow_running = false;
    GetMythUI()->RestoreScreensaver();
    m_effect_running = false;
    m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;

    bool wasInfo = m_info_show;
    m_info_show = false;
    bool wasInfoShort = m_info_show_short;
    m_info_show_short = false;

    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Gallery", e, actions);

    float scrollX = 0.2f;
    float scrollY = 0.2f;

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT" || action == "UP")
        {
            m_info_show = wasInfo;
            m_slideshow_running = wasRunning;
            DisplayPrev(true, true);
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            m_info_show = wasInfo;
            m_slideshow_running = wasRunning;
            DisplayNext(true, true);
        }
        else if (action == "ZOOMOUT")
        {
            if (m_zoom > 0.5f)
            {
                SetZoom(m_zoom - 0.5f);
                if (m_zoom > 1.0f)
                {
                    m_source_x   -= m_source_x / ((m_zoom + 0.5) * 2.0f);
                    m_source_y   -= m_source_y / ((m_zoom + 0.5) * 2.0f);

                    checkPosition();
                }
                else
                {
                    m_source_x = 0;
                    m_source_y = 0;
                }
            }
        }
        else if (action == "ZOOMIN")
        {
            if (m_zoom < 4.0f)
            {
                SetZoom(m_zoom + 0.5f);
                if (m_zoom > 1.0f)
                {
                    m_source_x   += m_source_x / (m_zoom * 2.0f);
                    m_source_y   += m_source_y / (m_zoom * 2.0f);

                    checkPosition();
                }
                else
                {
                    m_source_x = 0;
                    m_source_y = 0;
                }
            }
        }
        else if (action == "FULLSIZE")
        {
            m_source_x = 0;
            m_source_y = 0;
            if (m_zoom != 1)
                SetZoom(1.0f);
        }
        else if (action == "SCROLLLEFT")
        {
            if (m_zoom > 1.0f && m_source_x < m_zoom - 1.0f)
            {
                m_source_x += scrollX;
                m_source_x  = min(m_source_x, m_zoom - 1.0f);
            }
        }
        else if (action == "SCROLLRIGHT")
        {
            if (m_zoom > 1.0f && m_source_x > -m_zoom + 1.0f)
            {
                m_source_x -= scrollX;
                m_source_x  = max(m_source_x, -m_zoom + 1.0f);
            }
        }
        else if (action == "SCROLLUP")
        {
            if (m_zoom > 1.0f && m_source_y <  m_zoom - 1.0f)
            {
                m_source_y += scrollY;
                m_source_y  = min(m_source_y,  m_zoom - 1.0f);
            }
        }
        else if (action == "SCROLLDOWN")
        {
            if (m_zoom > 1.0f && m_source_y > -m_zoom + 1.0f)
            {
                m_source_y -= scrollY;
                m_source_y  = max(m_source_y, -m_zoom + 1.0f);
            }
        }
        else if (action == "RECENTER")
        {
            if (m_zoom > 1.0f)
            {
                m_source_x = 0.0f;
                m_source_y = 0.0f;
            }
        }
        else if (action == "UPLEFT")
        {
            if (m_zoom > 1.0f)
            {
                m_source_x  =  1.0f;
                m_source_y  = -1.0f;
            }
        }
        else if (action == "LOWRIGHT")
        {
            if (m_zoom > 1.0f)
            {
                m_source_x = -1.0f;
                m_source_y =  1.0f;
            }
        }
        else if (action == "ROTRIGHT")
        {
            m_source_x = 0;
            m_source_y = 0;
            Rotate(90);
        }
        else if (action == "ROTLEFT")
        {
            m_source_x = 0;
            m_source_y = 0;
            Rotate(-90);
        }
        else if (action == "DELETE")
        {
            ThumbItem *item = m_itemList.at(m_pos);
            if (item && GalleryUtil::Delete(item->GetPath()))
            {
                item->SetPixmap(NULL);
                DisplayNext(true, true);
            }
            m_info_show = wasInfo;
            m_slideshow_running = wasRunning;
        }
        else if ((action == "PLAY" || action == "SELECT") && m_movieState == 2)
        {
            m_movieState = 1;
        }
        else if (action == "PLAY" || action == "SLIDESHOW" ||
                 action == "RANDOMSHOW")
        {
            m_source_x   = 0;
            m_source_y   = 0;
            SetZoom(1.0f);
            m_info_show = wasInfo;
            m_info_show_short = true;
            m_slideshow_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_info_show = !wasInfo && !wasInfoShort;
            m_slideshow_running = wasRunning;
        }
        else if (action == "FULLSCREEN")
        {
            m_scaleMax = (ScaleMax) ((m_scaleMax + 1) % kScaleMaxCount);
            m_source_x = 0;
            m_source_y = 0;
            SetZoom(1.0f);
            Load();
        }
        else
        {
            handled = false;
        }
    }

    if (m_slideshow_running || m_info_show_short)
    {
        m_slideshow_timer->stop();
        m_slideshow_timer->setSingleShot(true);
        m_slideshow_timer->start(m_slideshow_frame_delay_state);
    }

    if (m_slideshow_running)
    {
        GetMythUI()->DisableScreensaver();
    }

    updateGL();

    if (handled)
    {
        e->accept();
    }
    else {
        e->ignore();
    }
}

void  GLSingleView::checkPosition(void)
{
    m_source_x = max(m_source_x, -m_zoom + 1.0f);
    m_source_y = max(m_source_y, -m_zoom + 1.0f);
    m_source_x = min(m_source_x, m_zoom - 1.0f);
    m_source_y = min(m_source_y, m_zoom - 1.0f);
}

void GLSingleView::paintTexture(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(m_source_x, m_source_y, 0.0f);
    glScalef(m_zoom, m_zoom, 1.0f);

    m_texItem[m_texCur].MakeQuad();

    if (m_info_show || m_info_show_short)
    {
        createTexInfo();

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();

        glBindTexture(GL_TEXTURE_2D, m_texInfo);
        glBegin(GL_QUADS);
        {
            glColor4f(1.0f, 1.0f, 1.0f, 0.72f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-0.75f, -0.75f, 0.0f);

            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(+0.75f, -0.75f, 0.0f);

            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(+0.75f, +0.75f, 0.0f);

            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-0.75f, +0.75f, 0.0f);
        }
        glEnd();
    }
}

void GLSingleView::DisplayNext(bool reset, bool loadImage)
{
    if (reset)
    {
        m_zoom     = 1.0f;
        m_source_x = 0.0f;
        m_source_y = 0.0f;
    }

    // Search for next item that hasn't been deleted.
    // Close viewer if none remain.
    ThumbItem *item;
    int oldpos = m_pos;

    while (true)
    {
        m_pos = m_slideshow_sequence->next();
        item = m_itemList.at(m_pos);
        if (item)
        {
            if (QFile::exists(item->GetPath()))
            {
                break;
            }
        }
        if (m_pos == oldpos)
        {
            // No valid items!!!
            close();
        }
    }

    m_tex1First = !m_tex1First;
    m_texCur      = (m_texCur) ? 0 : 1;

    if (loadImage)
        Load();
}

void GLSingleView::DisplayPrev(bool reset, bool loadImage)
{
    if (reset)
    {
        m_zoom     = 1.0f;
        m_source_x = 0.0f;
        m_source_y = 0.0f;
    }

    // Search for next item that hasn't been deleted.
    // Close viewer in none remain.
    int oldpos = m_pos;
    while (true)
    {
        m_pos = m_slideshow_sequence->prev();

        ThumbItem *item = m_itemList.at(m_pos);
        if (item && QFile::exists(item->GetPath()))
            break;

        if (m_pos == oldpos)
        {
            // No valid items!!!
            close();
        }
    };

    m_tex1First = !m_tex1First;
    m_texCur    = (m_texCur) ? 0 : 1;

    if (loadImage)
        Load();
}

void GLSingleView::Load(void)
{
    m_movieState = 0;
    ThumbItem *item = m_itemList.at(m_pos);
    if (!item)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No item at "<<m_pos);
        return;
    }

    if (GalleryUtil::IsMovie(item->GetPath()))
    {
        m_movieState = 1;
        return;
    }

    QImage image(item->GetPath());
    if (image.isNull())
        return;

    int a = m_tex1First ? 0 : 1;
    m_texItem[a].SetItem(item, image.size());
    m_texItem[a].ScaleTo(m_screenSize, m_scaleMax);
    m_texItem[a].Init(convertToGLFormat(
        image.scaled(m_texSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

    UpdateLCD(item);
}

void GLSingleView::Rotate(int angle)
{
    int ang = m_texItem[m_texCur].GetAngle() + angle;

    ang = (ang >= 360) ? ang - 360 : ang;
    ang = (ang < 0)    ? ang + 360 : ang;

    m_texItem[m_texCur].SetAngle(ang);

    ThumbItem *item = m_itemList.at(m_pos);
    if (item)
        item->SetRotationAngle(ang);

    m_texItem[m_texCur].SwapWidthHeight();
    m_texItem[m_texCur].ScaleTo(m_screenSize, m_scaleMax);
}

void GLSingleView::SetZoom(float zoom)
{
    m_zoom = zoom;
}

void GLSingleView::SetTransitionTimeout(int timeout)
{
    m_effect_transition_timeout = timeout;
    m_effect_transition_timeout_inv = 1.0f;
    if (timeout)
        m_effect_transition_timeout_inv = 1.0f / timeout;
}

int GLSingleView::GetNearestGLTextureSize(int v) const
{
    int n = 0, last = 0;
    int s;

    for (s = 0; s < 32; ++s)
    {
        if (((v >> s) & 1) == 1)
        {
            ++n;
            last = s;
        }
    }

    if (n > 1)
        s = 1 << (last + 1);
    else
        s = 1 << last;

    return min(s, m_texMaxDim);
}

void GLSingleView::RegisterEffects(void)
{
    m_effect_map.insert("none",            "EffectNone");
    m_effect_map.insert("blend (gl)",      "EffectBlend");
    m_effect_map.insert("zoom blend (gl)", "EffectZoomBlend");
    m_effect_map.insert("fade (gl)",       "EffectFade");
    m_effect_map.insert("rotate (gl)",     "EffectRotate");
    m_effect_map.insert("bend (gl)",       "EffectBend");
    m_effect_map.insert("inout (gl)",      "EffectInOut");
    m_effect_map.insert("slide (gl)",      "EffectSlide");
    m_effect_map.insert("flutter (gl)",    "EffectFlutter");
    m_effect_map.insert("cube (gl)",       "EffectCube");
}

void GLSingleView::RunEffect(const QString &effect)
{
    if (effect == "EffectBlend")
        EffectBlend();
    else if (effect == "EffectZoomBlend")
        EffectZoomBlend();
    else if (effect == "EffectFade")
        EffectFade();
    else if (effect == "EffectRotate")
        EffectRotate();
    else if (effect == "EffectBend")
        EffectBend();
    else if (effect == "EffectInOut")
        EffectInOut();
    else if (effect == "EffectSlide")
        EffectSlide();
    else if (effect == "EffectFlutter")
        EffectFlutter();
    else if (effect == "EffectCube")
        EffectCube();
    else //if (effect == "EffectNone")
        EffectNone();
}

void GLSingleView::EffectNone(void)
{
    paintTexture();
    m_effect_running = false;
    m_slideshow_frame_delay_state = -1;
    return;
}

void GLSingleView::EffectBlend(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    float t = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;

    m_texItem[(m_texCur) ? 0 : 1].MakeQuad();

    glBegin(GL_QUADS);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f * t);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glVertex3f(+1.0f, -1.0f, 0.0f);
        glVertex3f(+1.0f, +1.0f, 0.0f);
        glVertex3f(-1.0f, +1.0f, 0.0f);
    }
    glEnd();

    m_texItem[m_texCur].MakeQuad(t);

    m_effect_current_frame++;
}

void GLSingleView::EffectZoomBlend(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    float t = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;

    m_texItem[m_texCur ? 0 : 1].MakeQuad(1.0f - t, 1.0f + (0.75 * t));
    m_texItem[m_texCur].MakeQuad(t);

    m_effect_current_frame++;
}

void GLSingleView::EffectRotate(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    if (m_effect_current_frame == 0)
        m_effect_rotate_direction = (int)((2.0*rand()/(RAND_MAX+1.0)));

    float t = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;

    m_texItem[m_texCur].MakeQuad();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float rotate = 360.0f * t;
    glRotatef(((m_effect_rotate_direction == 0) ? -1 : 1) * rotate,
              0.0f, 0.0f, 1.0f);
    float scale = 1.0f * (1.0f - t);
    glScalef(scale, scale, 1.0f);

    m_texItem[(m_texCur) ? 0 : 1].MakeQuad();

    m_effect_current_frame++;
}

void GLSingleView::EffectBend(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    if (m_effect_current_frame == 0)
        m_effect_rotate_direction = (int)((2.0f*rand()/(RAND_MAX+1.0f)));

    float t = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;

    m_texItem[m_texCur].MakeQuad();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(90.0f * t,
              (m_effect_rotate_direction == 0) ? 1.0f : 0.0f,
              (m_effect_rotate_direction == 1) ? 1.0f : 0.0f,
              0.0f);

    m_texItem[(m_texCur) ? 0 : 1].MakeQuad();

    m_effect_current_frame++;
}

void GLSingleView::EffectFade(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    float t = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;

    if (m_effect_frame_time.elapsed() <= m_effect_transition_timeout / 2)
        m_texItem[(m_texCur) ? 0 : 1].MakeQuad(1.0f - (2.0f * t));
    else
        m_texItem[m_texCur].MakeQuad(2.0f * (t - 0.5f));

    m_effect_current_frame++;
}

void GLSingleView::EffectInOut(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    if (m_effect_current_frame == 0)
    {
        m_effect_rotate_direction = 1 + (int)((4.0f*rand()/(RAND_MAX+1.0f)));
    }

    int  texnum  = m_texCur;
    bool fadeout = false;
    if (m_effect_frame_time.elapsed() <= m_effect_transition_timeout / 2)
    {
        texnum  = (m_texCur) ? 0 : 1;
        fadeout = true;
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float tt = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;
    float t = 2.0f / ((fadeout) ? (0.5f - tt) : (tt - 0.5f));

    glScalef(t, t, 1.0f);
    t = 1.0f - t;
    glTranslatef((m_effect_rotate_direction % 2 == 0) ? ((m_effect_rotate_direction == 2)? 1 : -1) * t : 0.0f,
                 (m_effect_rotate_direction % 2 == 1) ? ((m_effect_rotate_direction == 1)? 1 : -1) * t : 0.0f,
                 0.0f);

    m_texItem[texnum].MakeQuad();

    m_effect_current_frame++;
}

void GLSingleView::EffectSlide(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    if (m_effect_current_frame == 0)
        m_effect_rotate_direction = 1 + (int)((4.0f * rand() / (RAND_MAX + 1.0f)));

    m_texItem[m_texCur].MakeQuad();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float t = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;
    float trans = 2.0f * t;
    glTranslatef((m_effect_rotate_direction % 2 == 0) ? ((m_effect_rotate_direction == 2)? 1 : -1) * trans : 0.0f,
                 (m_effect_rotate_direction % 2 == 1) ? ((m_effect_rotate_direction == 1)? 1 : -1) * trans : 0.0f,
                 0.0f);

    m_texItem[(m_texCur) ? 0 : 1].MakeQuad();

    m_effect_current_frame++;
}

void GLSingleView::EffectFlutter(void)
{
    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    GLTexture &ta = m_texItem[(m_texCur) ? 0 : 1];

    if (m_effect_current_frame == 0)
    {
        for (int x = 0; x < 40; x++)
        {
            for (int y = 0; y < 40; y++)
            {
                m_effect_flutter_points[x][y][0] =
                    (float) (x / 20.0f - 1.0f) * ta.GetTextureX();
                m_effect_flutter_points[x][y][1] =
                    (float) (y / 20.0f - 1.0f) * ta.GetTextureY();
                m_effect_flutter_points[x][y][2] =
                    (float) sin((x / 20.0f - 1.0f) * M_PI * 2.0f) / 5.0;
            }
        }
    }

    m_texItem[m_texCur].MakeQuad();

    float t      = m_effect_frame_time.elapsed() * m_effect_transition_timeout_inv;
    float rotate = 60.0f * t;
    float scale  = 1.0f  - t;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(rotate, 1.0f, 0.0f, 0.0f);
    glScalef(scale, scale, scale);
    glTranslatef(t, t, 0.0f);

    ta.Bind();

    glBegin(GL_QUADS);
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

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
                glVertex3f(m_effect_flutter_points[x][y][0],
                           m_effect_flutter_points[x][y][1],
                           m_effect_flutter_points[x][y][2]);
                glTexCoord2f(float_x, float_yb);
                glVertex3f(m_effect_flutter_points[x][y + 1][0],
                           m_effect_flutter_points[x][y + 1][1],
                           m_effect_flutter_points[x][y + 1][2]);
                glTexCoord2f(float_xb, float_yb);
                glVertex3f(m_effect_flutter_points[x + 1][y + 1][0],
                           m_effect_flutter_points[x + 1][y + 1][1],
                           m_effect_flutter_points[x + 1][y + 1][2]);
                glTexCoord2f(float_xb, float_y);
                glVertex3f(m_effect_flutter_points[x + 1][y][0],
                           m_effect_flutter_points[x + 1][y][1],
                           m_effect_flutter_points[x + 1][y][2]);
            }
        }
    }
    glEnd();

    // wave every two iterations
    if (m_effect_current_frame%2 == 0)
    {

        float hold;
        int x, y;
        for (y = 0; y < 40; y++)
        {
            hold = m_effect_flutter_points[0][y][2];
            for (x = 0; x < 39; x++)
            {
                m_effect_flutter_points[x][y][2] = m_effect_flutter_points[x + 1][y][2];
            }
            m_effect_flutter_points[39][y][2] = hold;
        }
    }
    m_effect_current_frame++;
}

void GLSingleView::EffectCube(void)
{
    float tot      = m_effect_transition_timeout ? m_effect_transition_timeout : 1.0f;
    float rotStart = 0.25f * m_effect_transition_timeout;

    if (m_effect_frame_time.elapsed() > m_effect_transition_timeout)
    {
        paintTexture();
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        return;
    }

    // Enable perspective vision
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    GLTexture &ta = m_texItem[(m_texCur) ? 0 : 1];
    GLTexture &tb = m_texItem[m_texCur];

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float PI = 4.0f * atan(1.0f);
    float znear = 3.0f;
    float theta = 2.0f * atan2(2.0f / 2.0f, znear);
    theta = theta * 180.0f/PI;

    glFrustum(-1.0f, 1.0f, -1.0f, 1.0f, znear - 0.01f, 10.0f);

    if (m_effect_current_frame == 0)
    {
        m_effect_cube_xrot = 0.0f;
        m_effect_cube_yrot = 0.0f;
        m_effect_cube_zrot = 0.0f;
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float elapsed = (float) m_effect_frame_time.elapsed();
    float tmp     = ((elapsed <= tot * 0.5) ? elapsed : tot - elapsed);
    float trans   = 5.0f * tmp / tot;

    glTranslatef(0.0f, 0.0f, -znear - 1.0f - trans);

    glRotatef(m_effect_cube_xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(m_effect_cube_yrot, 0.0f, 1.0f, 0.0f);

    glBindTexture(GL_TEXTURE_2D, 0);

    glBegin(GL_QUADS);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

        /* Front Face */
        glVertex3f(-1.00f, -1.00f,  0.99f);
        glVertex3f( 1.00f, -1.00f,  0.99f);
        glVertex3f( 1.00f,  1.00f,  0.99f);
        glVertex3f(-1.00f,  1.00f,  0.99f);

        /* Back Face */
        glVertex3f(-1.00f, -1.00f, -0.99f);
        glVertex3f(-1.00f,  1.00f, -0.99f);
        glVertex3f( 1.00f,  1.00f, -0.99f);
        glVertex3f( 1.00f, -1.00f, -0.99f);

        /* Top Face */
        glVertex3f(-1.00f,  0.99f, -1.00f);
        glVertex3f(-1.00f,  0.99f,  1.00f);
        glVertex3f( 1.00f,  0.99f,  1.00f);
        glVertex3f( 1.00f,  0.99f, -1.00f);

        /* Bottom Face */
        glVertex3f(-1.00f, -0.99f, -1.00f);
        glVertex3f( 1.00f, -0.99f, -1.00f);
        glVertex3f( 1.00f, -0.99f,  1.00f);
        glVertex3f(-1.00f, -0.99f,  1.00f);

        /* Right face */
        glVertex3f(0.99f, -1.00f, -1.00f);
        glVertex3f(0.99f,  1.00f, -1.00f);
        glVertex3f(0.99f,  1.00f,  1.00f);
        glVertex3f(0.99f, -1.00f,  1.00f);

        /* Left Face */
        glVertex3f(-0.99f, -1.00f, -1.00f);
        glVertex3f(-0.99f, -1.00f,  1.00f);
        glVertex3f(-0.99f,  1.00f,  1.00f);
        glVertex3f(-0.99f,  1.00f, -1.00f);

    }
    glEnd();

    ta.Bind();

    glBegin(GL_QUADS);
    {
        glColor4d(1.0f, 1.0f, 1.0f, 1.0f);

        // Front Face
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-ta.GetTextureX(), -ta.GetTextureY(),  1.00f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(+ta.GetTextureX(), -ta.GetTextureY(),  1.00f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(+ta.GetTextureX(), +ta.GetTextureY(),  1.00f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-ta.GetTextureX(), +ta.GetTextureY(),  1.00f);

        // Top Face
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-ta.GetTextureX(),  1.00f, -ta.GetTextureY());
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-ta.GetTextureX(),  1.00f, +ta.GetTextureY());
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(+ta.GetTextureX(),  1.00f, +ta.GetTextureY());
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(+ta.GetTextureX(),  1.00f, -ta.GetTextureY());

        // Bottom Face
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-ta.GetTextureX(), -1.00f, -ta.GetTextureY());
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(+ta.GetTextureX(), -1.00f, -ta.GetTextureY());
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(+ta.GetTextureX(), -1.00f, +ta.GetTextureY());
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-ta.GetTextureX(), -1.00f, +ta.GetTextureY());

        // Right face
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(1.00f, -ta.GetTextureX(), -ta.GetTextureY());
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(1.00f, -ta.GetTextureX(), +ta.GetTextureY());
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(1.00f, +ta.GetTextureX(), +ta.GetTextureY());
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(1.00f, +ta.GetTextureX(), -ta.GetTextureY());

        // Left Face
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-1.00f, -ta.GetTextureX(), -ta.GetTextureY());
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.00f, +ta.GetTextureX(), -ta.GetTextureY());
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.00f, +ta.GetTextureX(), +ta.GetTextureY());
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-1.00f, -ta.GetTextureX(), +ta.GetTextureY());
    }
    glEnd();

    tb.Bind();

    glBegin(GL_QUADS);
    {
        glColor4d(1.0f, 1.0f, 1.0f, 1.0f);

        // Back Face
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-tb.GetTextureX(), -tb.GetTextureY(), -1.00f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-tb.GetTextureX(), +tb.GetTextureY(), -1.00f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(+tb.GetTextureX(), +tb.GetTextureY(), -1.00f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(+tb.GetTextureX(), -tb.GetTextureY(), -1.00f);
    }
    glEnd();

    if ((elapsed >= rotStart) && (elapsed < (tot - rotStart)))
    {
        m_effect_cube_xrot = 360.0f * (elapsed - rotStart) / (tot - 2 * rotStart);
        m_effect_cube_yrot = 0.5f * m_effect_cube_xrot;
    }

    m_effect_current_frame++;
}

void GLSingleView::SlideTimeout(void)
{
    bool wasMovie = false, isMovie = false;
    if (m_effect_method.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No transition method");
        return;
    }

    if (m_effect_running)
    {
        m_slideshow_frame_delay_state = 10;
    }
    else
    {
        if (m_slideshow_frame_delay_state == -1)
        {
            // effect was running and is complete now
            // run timer while showing current image
            m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;
            m_effect_current_frame     = 0;
        }
        else
        {
            // timed out after showing current image
            // load next image and start effect

            if (m_slideshow_running)
            {
                if (m_effect_random)
                    m_effect_method = GetRandomEffect();

                DisplayNext(false, false);

                wasMovie = m_movieState > 0;
                Load();
                isMovie = m_movieState > 0;
                // If transitioning to/from a movie, don't do an effect,
                // and shorten timeout
                if (wasMovie || isMovie)
                {
                    m_slideshow_frame_delay_state = 1;
                }
                else
                {
                    m_slideshow_frame_delay_state = 10;
                    m_effect_running = true;
                    m_effect_current_frame = 0;
                }
                m_effect_frame_time.restart();
            }
            m_info_show_short = false;
        }
    }

    updateGL();
    if (m_slideshow_running)
    {
        m_slideshow_timer->stop();
        m_slideshow_timer->setSingleShot(true);
        m_slideshow_timer->start(max(0, m_slideshow_frame_delay_state));

        // If transitioning to/from a movie, no effect is running so
        // next timeout should trigger proper immage delay.
        if (wasMovie || isMovie)
        {
            m_slideshow_frame_delay_state = -1;
        }
    }
}

void GLSingleView::createTexInfo(void)
{
    if (m_texInfo)
        glDeleteTextures(1, &m_texInfo);

    QString info = m_texItem[m_texCur].GetDescription(GetDescriptionStatus());
    if (info.isEmpty())
        return;

    QPixmap pix(512, 512);

    QPainter p(&pix);
    p.initFrom(this);
    p.fillRect(0, 0, pix.width(), pix.height(), Qt::black);
    p.setPen(Qt::white);

    p.drawText(10, 10, pix.width() - 20, pix.height() - 20,
               Qt::AlignLeft, info);
    p.end();

    QImage img = pix.toImage();
    img = img.convertToFormat(QImage::Format_ARGB32);

    QImage tex = convertToGLFormat(img);

    /* create the texture */
    glGenTextures(1, &m_texInfo);
    glBindTexture(GL_TEXTURE_2D, m_texInfo);
    /* actually generate the texture */
    glTexImage2D(GL_TEXTURE_2D, 0, 3, tex.width(), tex.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, tex.bits());
    /* enable linear filtering  */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}
