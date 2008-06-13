#include <iostream>
#include <cstdlib>
using namespace std;

#include "mythuiimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

#include "mythcontext.h"
#include "mythscreentype.h"

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
    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;

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
    m_cropRect = QRect(0,0,0,0);
    m_ForceSize = QSize(0,0);

    m_CurPos = 0;
    m_LastDisplay = QTime::currentTime();

    m_isReflected = false;
    m_reflectShear = 0;
    m_reflectScale = m_reflectLength = 100;
    m_reflectAxis = ReflectVertical;
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

void MythUIImage::SetImageCount(int low, int high)
{
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

    if (!m_ForceSize.isNull())
    {
        int w = (m_ForceSize.width() != -1) ? m_ForceSize.width() : img->width();
        int h = (m_ForceSize.height() != -1) ? m_ForceSize.height() : img->height();

        img->Assign(img->smoothScale(w, h));
    }

    m_Images.push_back(img);
    SetSize(img->size());
    m_CurPos = 0;
}

void MythUIImage::SetImages(QVector<MythImage *> &images)
{
    Clear();

    QVector<MythImage *>::iterator it;
    for (it = images.begin(); it != images.end(); ++it)
    {
        MythImage *im = (*it);
        im->UpRef();

        if (!m_ForceSize.isNull())
        {
            int w = (m_ForceSize.width() != -1) ? m_ForceSize.width() : im->width();
            int h = (m_ForceSize.height() != -1) ? m_ForceSize.height() : im->height();

            im->Assign(im->smoothScale(w, h));
        }

        m_Images.push_back(im);

        QSize aSize = m_Area.size();
        aSize = aSize.expandedTo(im->size());
        SetSize(aSize);
    }

    m_CurPos = 0;
}

void MythUIImage::SetSize(int width, int height)
{
    SetSize(QSize(width,height));
}

void MythUIImage::SetSize(const QSize &size)
{
    MythUIType::SetSize(size);
    m_NeedLoad = true;
}

void MythUIImage::SetCropRect(int x, int y, int width, int height)
{
    m_cropRect = QRect(x, y, width, height);
}

bool MythUIImage::Load(void)
{
    Clear();

    for (int i = m_LowNum; i <= m_HighNum; i++)
    {
        MythImage *image = GetMythPainter()->GetFormatImage();
        QString filename = m_Filename;
        if (m_HighNum >= 1)
            filename = QString(m_Filename).arg(i);

        if (!image->Load(filename))
            return false;

        if (m_isReflected)
            image->Reflect(m_reflectAxis, m_reflectShear, m_reflectScale,
                           m_reflectLength);

        if (!m_ForceSize.isNull())
        {
            int w = (m_ForceSize.width() != -1) ? m_ForceSize.width() : image->width();
            int h = (m_ForceSize.height() != -1) ? m_ForceSize.height() : image->height();

            image->Assign(image->smoothScale(w, h));
        }

        QSize aSize = m_Area.size();
        aSize = aSize.expandedTo(image->size());
        SetSize(aSize);

        image->SetChanged();

        if (image->isNull())
            image->DownRef();
        else
            m_Images.push_back(image);
    }

    m_LastDisplay = QTime::currentTime();
    SetRedraw();

    return true;
}

void MythUIImage::Reset(void)
{
    Clear();
}

void MythUIImage::Pulse(void)
{
    if (m_Delay > 0 &&
        abs(m_LastDisplay.msecsTo(QTime::currentTime())) > m_Delay)
    {
        m_CurPos++;
        if (m_CurPos >= (uint)m_Images.size())
            m_CurPos = 0;

        SetRedraw();
        m_LastDisplay = QTime::currentTime();
    }

    MythUIType::Pulse();
}

void MythUIImage::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                           int alphaMod, QRect clipRect)
{
    if (m_Images.size() > 0)
    {
        if (m_CurPos > (uint)m_Images.size())
            m_CurPos = 0;

        QRect area = m_Area;
        area.moveBy(xoffset, yoffset);

        int alpha = CalcAlpha(alphaMod); 

        QRect srcRect;
        if (!m_cropRect.isEmpty())
            srcRect = m_cropRect;
        else
            srcRect = m_Images[m_CurPos]->rect();

        p->DrawImage(area, m_Images[m_CurPos], srcRect, alpha);
    }
}

bool MythUIImage::ParseElement(QDomElement &element)
{
    if (element.tagName() == "filename")
        m_OrigFilename = m_Filename = getFirstText(element);
    else if (element.tagName() == "filepattern")
    {
        m_OrigFilename = m_Filename = getFirstText(element);
        QString tmp = element.attribute("low");
        if (!tmp.isEmpty())
            m_LowNum = tmp.toInt();
        tmp = element.attribute("high");
        if (!tmp.isEmpty())
            m_HighNum = tmp.toInt();
    }
    else if (element.tagName() == "area")
    {
        QRect area = parseRect(element);
        SetArea(area);
        m_ForceSize = area.size();
    }
    else if (element.tagName() == "staticsize")
    {
        QSize forceSize = parseSize(element);
        SetSize(forceSize);
    }
    else if (element.tagName() == "crop")
        m_cropRect = parseRect(element);
    else if (element.tagName() == "delay")
        m_Delay = getFirstText(element).toInt();
    else if (element.tagName() == "reflection")
    {
        m_isReflected = true;
        QString tmp = element.attribute("axis");
        if (!tmp.isEmpty())
        {
            if (tmp.toLower() == "horizontal")
                m_reflectAxis = ReflectHorizontal;
            else
                m_reflectAxis = ReflectVertical;
        }
        tmp = element.attribute("shear");
        if (!tmp.isEmpty())
            m_reflectShear = tmp.toInt();
        tmp = element.attribute("scale");
        if (!tmp.isEmpty())
            m_reflectScale = tmp.toInt();
        tmp = element.attribute("length");
        if (!tmp.isEmpty())
            m_reflectLength = tmp.toInt();
    }
    else
        return MythUIType::ParseElement(element);

    m_NeedLoad = true;
    return true;
}

void MythUIImage::CopyFrom(MythUIType *base)
{
    MythUIImage *im = dynamic_cast<MythUIImage *>(base);
    if (!im)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_Filename = im->m_Filename;
    m_OrigFilename = im->m_OrigFilename;

    m_cropRect = im->m_cropRect;
    m_ForceSize = im->m_ForceSize;

    m_Delay = im->m_Delay;
    m_LowNum = im->m_LowNum;
    m_HighNum = im->m_HighNum;

    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;

    m_isReflected = im->m_isReflected;
    m_reflectAxis = im->m_reflectAxis;
    m_reflectShear = im->m_reflectShear;
    m_reflectScale = im->m_reflectScale;
    m_reflectLength = im->m_reflectLength;

    SetImages(im->m_Images);

    MythUIType::CopyFrom(base);
}

void MythUIImage::CreateCopy(MythUIType *parent)
{
    MythUIImage *im = new MythUIImage(parent, objectName());
    im->CopyFrom(this);
}

void MythUIImage::Finalize(void)
{
    if (m_NeedLoad)
        Load();

    MythUIType::Finalize();
}
  
