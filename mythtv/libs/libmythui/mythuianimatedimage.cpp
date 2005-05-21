#include "mythuianimatedimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUIAnimatedImage::MythUIAnimatedImage(const QString &filepattern, 
                                         int low, int high, int delayms,
                                         MythUIType *parent, const char *name)
                   : MythUIType(parent, name)
{
    m_Filepattern = filepattern;
    m_LowNum = low;
    m_HighNum = high;

    m_Delay = delayms;

    Init();
}

MythUIAnimatedImage::MythUIAnimatedImage(MythUIType *parent, const char *name)
                   : MythUIType(parent, name)
{
    Init();
}

MythUIAnimatedImage::~MythUIAnimatedImage()
{
    while (!m_Images.isEmpty())
    {
        delete m_Images.back();
        m_Images.pop_back();
    }
}

void MythUIAnimatedImage::Init(void)
{
    m_ForceW = m_ForceH = -1;
    m_CurPos = 0;
    m_LastDisplay = QTime::currentTime();
}

void MythUIAnimatedImage::SetFilepattern(const QString &filepattern, int low,
                                         int high)
{
    m_Filepattern = filepattern;
    m_LowNum = low;
    m_HighNum = high;
}

void MythUIAnimatedImage::SetDelay(int delayms)
{
    m_Delay = delayms;
    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;
}

void MythUIAnimatedImage::SetSize(int width, int height)
{
    m_ForceW = width;
    m_ForceH = height;
}

void MythUIAnimatedImage::Load(void)
{
    for (int i = m_LowNum; i <= m_HighNum; i++)
    {
        MythImage *image = GetMythPainter()->GetFormatImage();
        QString filename = QString(m_Filepattern).arg(i);

        image->Load(filename);

        if (m_ForceW != -1 || m_ForceH != -1)
        {
            int w = (m_ForceW != -1) ? m_ForceW : image->width();
            int h = (m_ForceH != -1) ? m_ForceH : image->height();

            image->Assign(image->smoothScale(w, h));
        }

        m_Area.setSize(image->size());
        image->SetChanged();

        if (image->isNull())
            delete image;
        else
            m_Images.push_back(image);
    }

    m_LastDisplay = QTime::currentTime();
    SetRedraw();
}

void MythUIAnimatedImage::Pulse(void)
{
    if (m_LastDisplay.msecsTo(QTime::currentTime()) > m_Delay)
    {
        m_CurPos++;
        if (m_CurPos >= m_Images.size())
            m_CurPos = 0;

        SetRedraw();
        m_LastDisplay = QTime::currentTime();
    }
}

void MythUIAnimatedImage::Draw(MythPainter *p, int xoffset, int yoffset, 
                               int alphaMod)
{
    if (m_Images.size() > 0)
    {
        QRect area = m_Area;
        area.moveBy(xoffset, yoffset);

        int alpha = CalcAlpha(alphaMod); 

        QRect srcRect = m_Images[m_CurPos]->rect();
        p->DrawImage(area, m_Images[m_CurPos], srcRect, alpha);
    }

    MythUIType::Draw(p, xoffset, yoffset, alphaMod);
}

