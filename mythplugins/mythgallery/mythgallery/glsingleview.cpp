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

#include <cmath>

#include <algorithm>
using namespace std;

#include <qtimer.h>
#include <qimage.h>
#include <qlayout.h>
#include <qsize.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qpainter.h>

#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>
#include <mythtv/util.h>

#include "glsingleview.h"
#include "galleryutil.h"

#define LOC QString("GLXDialog: ")
#define LOC_ERR QString("GLXDialog, Error: ")

GLSDialog::GLSDialog(const ThumbList& itemList,
                     int pos, int slideShow, int sortOrder,
                     MythMainWindow *parent, const char *name)
    : MythDialog(parent, name)
{
    QBoxLayout *l = new QVBoxLayout(this);
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
}

/** \fn GLTexture::Init(const QImage&)
 *  \brief Create the texture initialized with QImage
 */
void GLTexture::Init(const QImage &image)
{
    Deinit();
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    /* actually generate the texture */
    glTexImage2D(GL_TEXTURE_2D, 0, 3, image.width(), image.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.bits());

    /* enable linear filtering  */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

/** \fn GLTexture::Init(void)
 *  \brief Delete the texture
 */
void GLTexture::Deinit(void)
{
    if (tex)
        glDeleteTextures(1, &tex);
}

void GLTexture::Bind(void)
{
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(GetAngle(), 0.0f, 0.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, tex);
}

void GLTexture::MakeQuad(float alpha, float scale)
{
    Bind();

    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-GetTextureX() * scale, -GetTextureY() * scale, 0.0f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(+GetTextureX() * scale, -GetTextureY() * scale, 0.0f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(+GetTextureX() * scale, +GetTextureY() * scale, 0.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-GetTextureX() * scale, +GetTextureY() * scale, 0.0f);
    glEnd();
}

void GLTexture::ScaleTo(const QSize &dest)
{
    QSize sz = GetSize();
    sz.scale(dest.width(), dest.height(), QSize::ScaleMin);
    SetScale((float)sz.width()  / (float)dest.width(),
             (float)sz.height() / (float)dest.height());
}

void GLTexture::SetItem(ThumbItem *thumbItem, const QSize &sz)
{
    item = thumbItem;
    if (item)
    {
        angle = item->GetRotationAngle();
        SetSize(sz);

        if (angle % 180 != 0)
            SwapWidthHeight();
    }
}

QString GLTexture::GetDescription(void) const
{
    if (!item)
        return QString::null;

    QFileInfo fi(item->path);

    QString info = item->name;
    info += "\n\n" + GLSingleView::tr("Folder: ") + fi.dir().dirName();
    info += "\n" + GLSingleView::tr("Created: ") + fi.created().toString();
    info += "\n" + GLSingleView::tr("Modified: ") +
        fi.lastModified().toString();
    info += "\n" + QString(GLSingleView::tr("Bytes") + ": %1").arg(fi.size());
    info += "\n" + QString(GLSingleView::tr("Width") + ": %1 " +
                           GLSingleView::tr("pixels")).arg(GetSize().width());
    info += "\n" + QString(GLSingleView::tr("Height") + ": %1 " +
                           GLSingleView::tr("pixels")).arg(GetSize().height());
    info += "\n" + QString(GLSingleView::tr("Pixel Count") + ": %1 " +
                           GLSingleView::tr("megapixels"))
        .arg((float) GetPixelCount() / 1000000.0f, 0, 'f', 2);
    info += "\n" + QString(GLSingleView::tr("Rotation Angle") + ": %1 " +
                           GLSingleView::tr("degrees")).arg(angle);

    return info;
}

GLSingleView::GLSingleView(ThumbList itemList, int pos, int slideShow,
                           int sortorder, QWidget *parent)
    : QGLWidget(parent),
      m_pos(pos),
      m_itemList(itemList),
      m_movieState(0),
      m_screenSize(640,480),
      m_wmult(1.0f),
      m_hmult(1.0f),
      m_textureSize(512,512),
      m_curr(0),
      m_tex1First(true),

      m_zoom(1.0f),
      m_sx(0.0f),
      m_sy(0.0f),

      m_timer(new QTimer(this)),
      m_delay(2),
      m_tmout(m_delay * 1000),
      m_transTimeout(2000),
      m_transTimeoutInv(1.0f / m_transTimeout),
      m_effectRunning(false),
      m_running(false),
      m_slideShow(slideShow),

      m_texInfo(0),
      m_showInfo(false),

      m_maxTexDim(512),
      m_i(0),
      m_dir(0),

      m_effectMethod(NULL),
      m_effectRandom(false),
      m_sequence(NULL),

      m_cube_xrot(0.0f),
      m_cube_yrot(0.0f),
      m_cube_zrot(0.0f)
{
    // --------------------------------------------------------------------

    setFocusPolicy(QWidget::WheelFocus);

    {
        int xbase, ybase, screenwidth, screenheight;
        gContext->GetScreenSettings(xbase, screenwidth,  m_wmult,
                                    ybase, screenheight, m_hmult);
        m_screenSize = QSize(screenwidth, screenheight);
    }

    // --------------------------------------------------------------------

    // remove all dirs from m_itemList;
    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);

    m_itemList.setAutoDelete(false);

    ThumbItem *item = m_itemList.first();
    while (item)
    {
        ThumbItem* next = m_itemList.next();
        if (item->isDir)
        {
            if (recurse)
            {
                GalleryUtil::loadDirectory(m_itemList, item->path, sortorder,
                                           recurse, NULL, NULL);
            }
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

    RegisterEffects();

    QString transType = gContext->GetSetting("SlideshowOpenGLTransition");
    if (!transType.isEmpty() && m_effectMap.contains(transType))
        m_effectMethod = m_effectMap[transType];

    if (!m_effectMethod || transType == QString("random (gl)"))
    {
        m_effectMethod = GetRandomEffect();
        m_effectRandom = true;
    }

    SetTransitionTimeout(gContext->GetNumSetting(
                             "SlideshowOpenGLTransitionLength", 2000));

    // --------------------------------------------------------------------

    m_delay = gContext->GetNumSetting("SlideshowDelay", 0);
    m_delay = (!m_delay) ? 2 : m_delay;
    m_tmout = m_delay * 1000;

    // --------------------------------------------------------------------

    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTimeOut()));

    // --------------------------------------------------------------------

    if (slideShow > 1)
    {
        m_sequence = new SequenceShuffle(m_itemList.count());
        m_pos = 0;
    }
    else
    {
        m_sequence = new SequenceInc(m_itemList.count());
    }

    m_pos = m_sequence->index(m_pos);

    if (slideShow)
    {
        m_running = true;
        m_timer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }
}

GLSingleView::~GLSingleView()
{
    if (m_sequence)
    {
        delete m_sequence;
        m_sequence = NULL;
    }
}

int GLSingleView::NearestGLTextureSize(int v)
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

    return min(s, m_maxTexDim);
}

void GLSingleView::CleanUp(void)
{
    if (class LCD* lcd = LCD::Get())
    {
        lcd->switchToTime();
    }

    makeCurrent();

    if (m_timer)
    {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = NULL;
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
    m_maxTexDim = param;

    m_textureSize = QSize(NearestGLTextureSize(m_screenSize.width()),
                          NearestGLTextureSize(m_screenSize.height()));

    loadImage();
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

    if (m_effectRunning && m_effectMethod)
    {
        (this->*m_effectMethod)();
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

    float scrollX = 0.02f;
    float scrollY = 0.02f;

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT" || action == "UP")
        {
            m_zoom = 1.0f;
            m_sx   = 0;
            m_sy   = 0;
            retreatFrame();
            loadImage();
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            m_zoom = 1.0f;
            m_sx   = 0;
            m_sy   = 0;
            advanceFrame();
            loadImage();
        }
        else if (action == "ZOOMOUT")
        {
            m_sx   = 0;
            m_sy   = 0;
            if (m_zoom > 0.5f)
            {
                m_zoom = m_zoom /2;
            }
            else
                handled = false;
        }
        else if (action == "ZOOMIN")
        {
            m_sx   = 0;
            m_sy   = 0;
            if (m_zoom < 4.0f)
            {
                m_zoom = m_zoom * 2;
            }
            else
                handled = false;
        }
        else if (action == "FULLSIZE")
        {
            m_sx = 0;
            m_sy = 0;
            if (m_zoom != 1)
            {
                m_zoom = 1.0f;
            }
            else
                handled = false;
        }
        else if (action == "SCROLLLEFT")
        {
            if (m_zoom > 1.0f && m_sx < 1.0f)
            {
                m_sx += scrollX;
                m_sx  = min(m_sx, 1.0f);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLRIGHT")
        {
            if (m_zoom > 1.0f && m_sx > -1.0f)
            {
                m_sx -= scrollX;
                m_sx  = max(m_sx, -1.0f);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLUP")
        {
            if (m_zoom > 1.0f && m_sy < 1.0f)
            {
                m_sy += scrollY;
                m_sy  = min(m_sy, 1.0f);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLDOWN")
        {
            if (m_zoom > 1.0f && m_sy > -1.0f)
            {
                m_sy -= scrollY;
                m_sy  = max(m_sy, -1.0f);
            }
            else
                handled = false;
        }
        else if (action == "RECENTER")
        {
            if (m_zoom > 1.0f)
            {
                m_sx = 0.0f;
                m_sy = 0.0f;
            }
            else
                handled = false;
        }
        else if (action == "UPLEFT")
        {
            if (m_zoom > 1.0f)
            {
                m_sx  =  1.0f;
                m_sy  = -1.0f;
            }
            else
                handled = false;
        }
        else if (action == "LOWRIGHT")
        {
            if (m_zoom > 1.0f)
            {
                m_sx = -1.0f;
                m_sy =  1.0f;
            }
            else
                handled = false;
        }
        else if (action == "ROTRIGHT")
        {
            m_sx = 0;
            m_sy = 0;
            Rotate(90);
        }
        else if (action == "ROTLEFT")
        {
            m_sx = 0;
            m_sy = 0;
            Rotate(-90);
        }
        else if (action == "DELETE")
        {
            ThumbItem *item = m_itemList.at(m_pos);
            if (item && GalleryUtil::Delete(item->path))
            {
                m_zoom = 1.0f;
                m_sx   = 0;
                m_sy   = 0;
                // Delete thumbnail for this
                if (item->pixmap)
                    delete item->pixmap;
                item->pixmap = 0;
                advanceFrame();
                loadImage();
            }
        }
        else if (action == "PLAY" || action == "SLIDESHOW" ||
                 action == "RANDOMSHOW")
        {
            m_sx   = 0;
            m_sy   = 0;
            m_zoom = 1.0f;
            m_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_showInfo = !wasInfo;
        }
        else
            handled = false;
    }

    if (m_running)
    {
        m_timer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }

    if (handled)
    {
        updateGL();
        e->accept();
    }
    else {
        e->ignore();
    }
}

void GLSingleView::paintTexture(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(m_sx, m_sy, 0.0f);
    glScalef(m_zoom, m_zoom, 1.0f);

    m_texItem[m_curr].MakeQuad();

    if (m_showInfo)
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

void GLSingleView::advanceFrame(void)
{
    // Search for next item that hasn't been deleted.
    // Close viewer if none remain.
    ThumbItem *item;
    int oldpos = m_pos;

    while (true)
    {
        m_pos = m_sequence->next();
        item = m_itemList.at(m_pos);
        if (item)
        {
            if (QFile::exists(item->path))
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
    m_curr      = (m_curr) ? 0 : 1;
}

void GLSingleView::retreatFrame(void)
{
    // Search for next item that hasn't been deleted.
    // Close viewer in none remain.
    ThumbItem *item;
    int oldpos = m_pos;

    while (true)
    {
        m_pos = m_sequence->prev();
        item = m_itemList.at(m_pos);
        if (item)
        {
            if (QFile::exists(item->path))
            {
                break;
            }
        }
        if (m_pos == oldpos)
        {
            // No valid items!!!
            close();
        }
    };

    m_tex1First = !m_tex1First;
    m_curr      = (m_curr) ? 0 : 1;
}

void GLSingleView::loadImage(void)
{
    m_movieState = 0;
    ThumbItem *item = m_itemList.at(m_pos);
    if (!item)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No item at "<<m_pos);
        return;
    }

    if (GalleryUtil::isMovie(item->path))
    {
        m_movieState = 1;
        return;
    }

    QImage image(item->path);
    if (image.isNull())
        return;

    LCD *lcd = LCD::Get();
    if (lcd)
    {
        QPtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);
        textItems.append(new LCDTextItem(1, ALIGN_CENTERED, item->name,
                                         "Generic", true));
        QString tmp = QString::number(m_pos + 1) + " / " +
            QString::number(m_itemList.count());
        textItems.append(new LCDTextItem(
                             2, ALIGN_CENTERED, tmp, "Generic", false));
        lcd->switchToGeneric(&textItems);
    }

    int a = m_tex1First ? 0 : 1;
    m_texItem[a].SetItem(item, image.size());
    m_texItem[a].ScaleTo(m_screenSize);
    m_texItem[a].Init(convertToGLFormat(image.smoothScale(m_textureSize)));
}

void GLSingleView::Rotate(int angle)
{
    int ang = m_texItem[m_curr].GetAngle() + angle;

    ang = (ang >= 360) ? ang - 360 : ang;
    ang = (ang < 0)    ? ang + 360 : ang;

    m_texItem[m_curr].SetAngle(ang);

    ThumbItem *item = m_itemList.at(m_pos);
    if (item)
    {
        item->SetRotationAngle(ang);

        // Delete thumbnail for this
        if (item->pixmap)
            delete item->pixmap;
        item->pixmap = 0;
    }

    m_texItem[m_curr].SwapWidthHeight();
    m_texItem[m_curr].ScaleTo(m_screenSize);
}

void GLSingleView::RegisterEffects(void)
{
    m_effectMap.insert("none",            &GLSingleView::effectNone);
    m_effectMap.insert("blend (gl)",      &GLSingleView::effectBlend);
    m_effectMap.insert("zoom blend (gl)", &GLSingleView::effectZoomBlend);
    m_effectMap.insert("fade (gl)",       &GLSingleView::effectFade);
    m_effectMap.insert("rotate (gl)",     &GLSingleView::effectRotate);
    m_effectMap.insert("bend (gl)",       &GLSingleView::effectBend);
    m_effectMap.insert("inout (gl)",      &GLSingleView::effectInOut);
    m_effectMap.insert("slide (gl)",      &GLSingleView::effectSlide);
    m_effectMap.insert("flutter (gl)",    &GLSingleView::effectFlutter);
    m_effectMap.insert("cube (gl)",       &GLSingleView::effectCube);
}

GLSingleView::EffectMethod GLSingleView::GetRandomEffect(void)
{
    QMap<QString, EffectMethod>  tmpMap(m_effectMap);

    tmpMap.remove("none");
    QStringList t = tmpMap.keys();

    int count = t.count();

    int i = (int)((float)(count) * rand()/(RAND_MAX+1.0));
    QString key = t[i];

    return tmpMap[key];
}

void GLSingleView::effectNone(void)
{
    paintTexture();
    m_effectRunning = false;
    m_tmout = -1;
    return;
}

void GLSingleView::effectBlend(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    float t = m_time.elapsed() * m_transTimeoutInv;

    m_texItem[(m_curr) ? 0 : 1].MakeQuad();

    glBegin(GL_QUADS);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f * t);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glVertex3f(+1.0f, -1.0f, 0.0f);
        glVertex3f(+1.0f, +1.0f, 0.0f);
        glVertex3f(-1.0f, +1.0f, 0.0f);
    }
    glEnd();

    m_texItem[m_curr].MakeQuad(t);

    m_i++;
}

void GLSingleView::effectZoomBlend(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    float t = m_time.elapsed() * m_transTimeoutInv;

    m_texItem[m_curr ? 0 : 1].MakeQuad(1.0f - t, 1.0f + (0.75 * t));
    m_texItem[m_curr].MakeQuad(t);

    m_i++;
}

void GLSingleView::effectRotate(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0)
        m_dir = (int)((2.0*rand()/(RAND_MAX+1.0)));

    float t = m_time.elapsed() * m_transTimeoutInv;

    m_texItem[m_curr].MakeQuad();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float rotate = 360.0f * t;
    glRotatef(((m_dir == 0) ? -1 : 1) * rotate,
              0.0f, 0.0f, 1.0f);
    float scale = 1.0f * (1.0f - t);
    glScalef(scale, scale, 1.0f);

    m_texItem[(m_curr) ? 0 : 1].MakeQuad();

    m_i++;
}

void GLSingleView::effectBend(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0)
        m_dir = (int)((2.0f*rand()/(RAND_MAX+1.0f)));

    float t = m_time.elapsed() * m_transTimeoutInv;

    m_texItem[m_curr].MakeQuad();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(90.0f * t,
              (m_dir == 0) ? 1.0f : 0.0f,
              (m_dir == 1) ? 1.0f : 0.0f,
              0.0f);

    m_texItem[(m_curr) ? 0 : 1].MakeQuad();

    m_i++;
}

void GLSingleView::effectFade(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    float t = m_time.elapsed() * m_transTimeoutInv;

    if (m_time.elapsed() <= m_transTimeout / 2)
        m_texItem[(m_curr) ? 0 : 1].MakeQuad(1.0f - (2.0f * t));
    else
        m_texItem[m_curr].MakeQuad(2.0f * (t - 0.5f));

    m_i++;
}

void GLSingleView::effectInOut(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0)
    {
        m_dir = 1 + (int)((4.0f*rand()/(RAND_MAX+1.0f)));
    }

    int  texnum  = m_curr;
    bool fadeout = false;
    if (m_time.elapsed() <= m_transTimeout / 2)
    {
        texnum  = (m_curr) ? 0 : 1;
        fadeout = true;
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float tt = m_time.elapsed() * m_transTimeoutInv;
    float t = 2.0f / ((fadeout) ? (0.5f - tt) : (tt - 0.5f));

    glScalef(t, t, 1.0f);
    t = 1.0f - t;
    glTranslatef((m_dir % 2 == 0) ? ((m_dir == 2)? 1 : -1) * t : 0.0f,
                 (m_dir % 2 == 1) ? ((m_dir == 1)? 1 : -1) * t : 0.0f,
                 0.0f);

    m_texItem[texnum].MakeQuad();

    m_i++;
}

void GLSingleView::effectSlide(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    if (m_i == 0)
        m_dir = 1 + (int)((4.0f * rand() / (RAND_MAX + 1.0f)));

    m_texItem[m_curr].MakeQuad();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float t = m_time.elapsed() * m_transTimeoutInv;
    float trans = 2.0f * t;
    glTranslatef((m_dir % 2 == 0) ? ((m_dir == 2)? 1 : -1) * trans : 0.0f,
                 (m_dir % 2 == 1) ? ((m_dir == 1)? 1 : -1) * trans : 0.0f,
                 0.0f);

    m_texItem[(m_curr) ? 0 : 1].MakeQuad();

    m_i++;
}

void GLSingleView::effectFlutter(void)
{
    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    GLTexture &ta = m_texItem[(m_curr) ? 0 : 1];

    if (m_i == 0)
    {
        for (int x = 0; x < 40; x++)
        {
            for (int y = 0; y < 40; y++)
            {
                m_points[x][y][0] =
                    (float) (x / 20.0f - 1.0f) * ta.GetTextureX();
                m_points[x][y][1] =
                    (float) (y / 20.0f - 1.0f) * ta.GetTextureY();
                m_points[x][y][2] =
                    (float) sin((x / 20.0f - 1.0f) * M_PI * 2.0f) / 5.0;
            }
        }
    }

    m_texItem[m_curr].MakeQuad();

    float t      = m_time.elapsed() * m_transTimeoutInv;
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
                glVertex3f(m_points[x][y][0],
                           m_points[x][y][1],
                           m_points[x][y][2]);
                glTexCoord2f(float_x, float_yb);
                glVertex3f(m_points[x][y + 1][0],
                           m_points[x][y + 1][1],
                           m_points[x][y + 1][2]);
                glTexCoord2f(float_xb, float_yb);
                glVertex3f(m_points[x + 1][y + 1][0],
                           m_points[x + 1][y + 1][1],
                           m_points[x + 1][y + 1][2]);
                glTexCoord2f(float_xb, float_y);
                glVertex3f(m_points[x + 1][y][0],
                           m_points[x + 1][y][1],
                           m_points[x + 1][y][2]);
            }
        }
    }
    glEnd();

    // wave every two iterations
    if (m_i%2 == 0)
    {

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

void GLSingleView::effectCube(void)
{
    float tot      = m_transTimeout ? m_transTimeout : 1.0f;
    float rotStart = 0.25f * m_transTimeout;

    if (m_time.elapsed() > m_transTimeout)
    {
        paintTexture();
        m_effectRunning = false;
        m_tmout = -1;
        return;
    }

    // Enable perspective vision
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    GLTexture &ta = m_texItem[(m_curr) ? 0 : 1];
    GLTexture &tb = m_texItem[m_curr];

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float PI = 4.0f * atan(1.0f);
    float znear = 3.0f;
    float theta = 2.0f * atan2(2.0f / 2.0f, znear);
    theta = theta * 180.0f/PI;

    glFrustum(-1.0f, 1.0f, -1.0f, 1.0f, znear - 0.01f, 10.0f);

    if (m_i == 0)
    {
        m_cube_xrot = 0.0f;
        m_cube_yrot = 0.0f;
        m_cube_zrot = 0.0f;
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float elapsed = (float) m_time.elapsed();
    float tmp     = ((elapsed <= tot * 0.5) ? elapsed : tot - elapsed);
    float trans   = 5.0f * tmp / tot;

    glTranslatef(0.0f, 0.0f, -znear - 1.0f - trans);

    glRotatef(m_cube_xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(m_cube_yrot, 0.0f, 1.0f, 0.0f);

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
        m_cube_xrot = 360.0f * (elapsed - rotStart) / (tot - 2 * rotStart);
        m_cube_yrot = 0.5f * m_cube_xrot;
    }

    m_i++;
}

void GLSingleView::slotTimeOut(void)
{
    bool wasMovie = false, isMovie = false;
    if (!m_effectMethod)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No transition method");
        return;
    }

    if (m_effectRunning)
    {
        m_tmout = 10;
    }
    else
    {
        if (m_tmout == -1)
        {
            // effect was running and is complete now
            // run timer while showing current image
            m_tmout = m_delay * 1000;
            m_i     = 0;
        }
        else
        {

            // timed out after showing current image
            // load next image and start effect

            if (m_effectRandom)
                m_effectMethod = GetRandomEffect();

            advanceFrame();

            wasMovie = m_movieState > 0;
            loadImage();
            isMovie = m_movieState > 0;
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
            m_time.restart();
        }
    }

    updateGL();
    m_timer->start(m_tmout, true);
    // If transitioning to/from a movie, no effect is running so
    // next timeout should trigger proper immage delay.
    if (wasMovie || isMovie)
    {
        m_tmout = -1;
    }
}

void GLSingleView::createTexInfo(void)
{
    if (m_texInfo)
        glDeleteTextures(1, &m_texInfo);

    QString info = m_texItem[m_curr].GetDescription();
    if (info.isEmpty())
        return;

    QPixmap pix(512, 512);

    QPainter p(&pix, this);
    p.fillRect(0, 0, pix.width(), pix.height(), Qt::black);
    p.setPen(Qt::white);

    p.drawText(10, 10, pix.width() - 20, pix.height() - 20,
               Qt::AlignLeft, info);
    p.end();

    QImage img(pix.convertToImage());
    img = img.convertDepth(32);

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
