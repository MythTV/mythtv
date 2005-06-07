#include <iostream>
using namespace std;

#include "mythuiimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUIImage::MythUIImage(const QString &filepattern,
                         int low, int high, int delayms,
                         MythUIType *parent, const char *name)
           : MythUIType(parent, name)
{
    m_Filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;

    m_Delay = delayms;

    Init();
}

MythUIImage::MythUIImage(const QString &filename, MythUIType *parent, 
                         const char *name)
           : MythUIType(parent, name)
{
    m_Filename = filename;
    m_OrigFilename = filename;

    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;

    Init();
}

MythUIImage::MythUIImage(MythUIType *parent, const char *name)
           : MythUIType(parent, name)
{
    Init();
}

MythUIImage::~MythUIImage()
{
    Clear();
}

void MythUIImage::Clear(void)
{
    while (!m_Images.isEmpty())
    {
        m_Images.back()->DownRef();
        m_Images.pop_back();
    }
}

void MythUIImage::Init(void)
{
    m_SkipX = m_SkipY = 0;
    m_ForceW = m_ForceH = -1;

    m_CurPos = 0;
    m_LastDisplay = QTime::currentTime();
}

void MythUIImage::SetFilename(const QString &filename)
{
    m_Filename = filename;
}

void MythUIImage::SetFilepattern(const QString &filepattern, int low,
                                         int high)
{
    m_Filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;
}

void MythUIImage::SetDelay(int delayms)
{
    m_Delay = delayms;
    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;
}

void MythUIImage::ResetFilename(void)
{
    m_Filename = m_OrigFilename;
    Load();
}

void MythUIImage::SetImage(MythImage *img)
{
    Clear();
    m_Delay = -1;

    img->UpRef();
    m_Images.push_back(img);
    m_Area.setSize(img->size());
    m_CurPos = 0;
}

void MythUIImage::SetImages(QValueVector<MythImage *> &m_Images)
{
    Clear();

    QValueVector<MythImage *>::iterator it;
    for (it = m_Images.begin(); it != m_Images.end(); ++it)
    {
        MythImage *im = (*it);
        im->UpRef();
        m_Images.push_back(im);

        QSize aSize = m_Area.size();
        aSize = aSize.expandedTo(im->size());
        m_Area.setSize(aSize);
    }

    m_CurPos = 0;
}

void MythUIImage::SetSize(int width, int height)
{
    m_ForceW = width;
    m_ForceH = height;
}

void MythUIImage::SetSkip(int x, int y)
{
    m_SkipX = x;
    m_SkipY = y;
}

void MythUIImage::Load(void)
{
    for (int i = m_LowNum; i <= m_HighNum; i++)
    {
        MythImage *image = GetMythPainter()->GetFormatImage();
        QString filename = m_Filename;
        if (m_HighNum >= 1)
            filename = QString(m_Filename).arg(i);

        image->Load(filename);

        if (m_ForceW != -1 || m_ForceH != -1)
        {
            int w = (m_ForceW != -1) ? m_ForceW : image->width();
            int h = (m_ForceH != -1) ? m_ForceH : image->height();

            image->Assign(image->smoothScale(w, h));
        }

        QSize aSize = m_Area.size();
        aSize = aSize.expandedTo(image->size());
        m_Area.setSize(aSize);

        image->SetChanged();

        if (image->isNull())
            image->DownRef();
        else
            m_Images.push_back(image);
    }

    m_LastDisplay = QTime::currentTime();
    SetRedraw();
}

void MythUIImage::Reset(void)
{
    Clear();
}

void MythUIImage::Pulse(void)
{
    if (m_Delay > 0 && m_LastDisplay.msecsTo(QTime::currentTime()) > m_Delay)
    {
        m_CurPos++;
        if (m_CurPos >= m_Images.size())
            m_CurPos = 0;

        SetRedraw();
        m_LastDisplay = QTime::currentTime();
    }
}

void MythUIImage::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                           int alphaMod, QRect clipRect)
{
    if (m_Images.size() > 0)
    {
        if (m_CurPos > m_Images.size())
            m_CurPos = 0;

        QRect area = m_Area;
        area.moveBy(xoffset, yoffset);
    
        int alpha = CalcAlpha(alphaMod); 

        QRect srcRect = m_Images[m_CurPos]->rect();
        srcRect.setTopLeft(QPoint(m_SkipX, m_SkipY));

        p->DrawImage(area, m_Images[m_CurPos], srcRect, alpha);
    }
}

