#include <iostream>
using namespace std;

#include "mythuiimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUIImage::MythUIImage(const QString &filename, MythUIType *parent, 
                         const char *name)
           : MythUIType(parent, name)
{
    m_Filename = filename;
    m_OrigFilename = filename;

    Init();
}

MythUIImage::MythUIImage(MythUIType *parent, const char *name)
           : MythUIType(parent, name)
{
    Init();
}

MythUIImage::~MythUIImage()
{
    delete m_Image;
}

void MythUIImage::Init(void)
{
    m_SkipX = m_SkipY = 0;
    m_ForceW = m_ForceH = -1;

    m_Image = GetMythPainter()->GetFormatImage();
}

void MythUIImage::SetFilename(const QString &filename)
{
    m_Filename = filename;
}

void MythUIImage::ResetFilename(void)
{
    m_Filename = m_OrigFilename;
    Load();
}

void MythUIImage::SetImage(const QImage &img)
{
    m_Image->Assign(img);
    m_Area.setSize(m_Image->size());
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
    m_Image->Load(m_Filename);

    if (m_ForceW != -1 || m_ForceH != -1)
    {
        int w = (m_ForceW != -1) ? m_ForceW : m_Image->width();
        int h = (m_ForceH != -1) ? m_ForceH : m_Image->height();

        m_Image->Assign(m_Image->smoothScale(w, h));
    }

    m_Area.setSize(m_Image->size());
    m_Image->SetChanged();

    SetRedraw();
}

void MythUIImage::Reset(void)
{
    m_Image->Assign(QImage());
}

void MythUIImage::Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod)
{

    QRect area = m_Area;
    area.moveBy(xoffset, yoffset);
    
    int alpha = CalcAlpha(alphaMod); 

    QRect srcRect = m_Image->rect();
    srcRect.setTopLeft(QPoint(m_SkipX, m_SkipY));

    p->DrawImage(area, m_Image, srcRect, alpha);

    MythUIType::Draw(p, xoffset, yoffset, alphaMod);
}

