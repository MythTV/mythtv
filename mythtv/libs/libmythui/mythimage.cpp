#include <cassert>

#include "mythimage.h"
#include "mythmainwindow.h"

MythImage::MythImage(MythPainter *parent)
{
    assert(parent);

    m_Parent = parent;
    m_Changed = false;
}

MythImage::~MythImage()
{
    m_Parent->DeleteFormatImage(this);
}

void MythImage::Assign(const QImage &img)
{
    *(QImage *)this = img;
    SetChanged();
}

void MythImage::Assign(const QPixmap &pix)
{
    Assign(pix.convertToImage());
}

MythImage *MythImage::FromQImage(QImage *img)
{
    if (!img)
        return NULL;

    MythImage *ret = GetMythPainter()->GetFormatImage();
    ret->Assign(*img);
    delete img;
    return ret;
}

