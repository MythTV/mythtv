// QT headers
#include <QApplication>
#include <QPainter>
#include <QMutex>

// Mythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythpainter_d3d9.h"

#define LOC QString("D3D9 Painter: ")

MythD3D9Painter::MythD3D9Painter(MythRenderD3D9 *render) :
    MythPainter(), m_render(render), m_created_render(true), m_target(NULL),
    m_swap_control(true)
{
    if (m_render)
        m_created_render = false;
}

MythD3D9Painter::~MythD3D9Painter()
{
    Teardown();
}

bool MythD3D9Painter::InitD3D9(QPaintDevice *parent)
{
    if (m_render)
        return  true;

    QWidget *real_parent = (QWidget*)parent;
    if (!real_parent)
        return false;

    m_render = new MythRenderD3D9();
    if (!m_render)
        return false;

    m_created_render = true;
    if (m_render->Create(real_parent->size(), real_parent->winId()))
        return true;

    Teardown();
    return false;
}

void MythD3D9Painter::Teardown(void)
{
    ExpireImages();
    ClearCache();
    DeleteBitmaps();

    m_ImageBitmapMap.clear();
    m_StringToImageMap.clear();
    m_ImageExpireList.clear();
    m_StringExpireList.clear();
    m_bitmapDeleteList.clear();

    if (m_render)
    {
        if (m_created_render)
            delete m_render;
        m_created_render = true;
        m_render = NULL;
    }
}

void MythD3D9Painter::FreeResources(void)
{
    ClearCache();
    DeleteBitmaps();
}

void MythD3D9Painter::Begin(QPaintDevice *parent)
{
    if (!m_render)
    {
        if (!InitD3D9(parent))
        {
            VERBOSE(VB_IMPORTANT, "Failed to create D3D9 render.");
            return;
        }
    }

    DeleteBitmaps();

    if (!m_target)
    {
        bool dummy;
        m_render->Test(dummy); // TODO recreate if necessary
    }
    else if (!m_target->SetAsRenderTarget())
    {
        VERBOSE(VB_IMPORTANT, "Failed to enable offscreen buffer.");
        return;
    }

    if (m_target)
        m_render->ClearBuffer();
    if (m_swap_control)
        m_render->Begin();
    MythPainter::Begin(parent);
}

void MythD3D9Painter::End(void)
{
    if (m_render)
    {
        if (m_swap_control)
        {
            m_render->End();
            m_render->Present(NULL);
        }
        if (m_target)
            m_render->SetRenderTarget(NULL);
    }
    MythPainter::End();
}

void MythD3D9Painter::ClearCache(void)
{
    VERBOSE(VB_GENERAL, LOC + "Clearing D3D9 painter cache.");

    QMutexLocker locker(&m_bitmapDeleteLock);
    QMapIterator<MythImage *, D3D9Image*> it(m_ImageBitmapMap);
    while (it.hasNext())
    {
        it.next();
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[it.key()]);
        m_ImageExpireList.remove(it.key());
    }
    m_ImageBitmapMap.clear();
}

void MythD3D9Painter::DeleteBitmaps(void)
{
    QMutexLocker locker(&m_bitmapDeleteLock);
    while (!m_bitmapDeleteList.empty())
    {
        D3D9Image *img = m_bitmapDeleteList.front();
        m_bitmapDeleteList.pop_front();
        DecreaseCacheSize(img->GetSize());
        delete img;
    }
}

void MythD3D9Painter::DrawImage(const QRect &r, MythImage *im,
                                const QRect &src, int alpha)
{
    D3D9Image *image = GetImageFromCache(im);
    if (image)
    {
        image->UpdateVertices(r, src, alpha);
        image->Draw();
    }
}

void MythD3D9Painter::DrawRect(const QRect &area, const QBrush &fillBrush,
                               const QPen &linePen, int alpha)
{
    int style = fillBrush.style();
    if (style == Qt::SolidPattern || style == Qt::NoBrush)
    {
        if (!m_render)
            return;

        if (style != Qt::NoBrush)
            m_render->DrawRect(area, fillBrush.color());

        if (linePen.style() != Qt::NoPen)
        {
            int lineWidth = linePen.width();
            QRect top(QPoint(area.x(), area.y()),
                      QSize(area.width(), lineWidth));
            QRect bot(QPoint(area.x(), area.y() + area.height() - lineWidth),
                      QSize(area.width(), lineWidth));
            QRect left(QPoint(area.x(), area.y()),
                       QSize(lineWidth, area.height()));
            QRect right(QPoint(area.x() + area.width() - lineWidth, area.y()),
                        QSize(lineWidth, area.height()));
            m_render->DrawRect(top,   linePen.color());
            m_render->DrawRect(bot,   linePen.color());
            m_render->DrawRect(left,  linePen.color());
            m_render->DrawRect(right, linePen.color());
        }
    }

    MythPainter::DrawRect(area, fillBrush, linePen, alpha);
}

void MythD3D9Painter::DeleteFormatImagePriv(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        QMutexLocker locker(&m_bitmapDeleteLock);
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[im]);
        m_ImageBitmapMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

D3D9Image* MythD3D9Painter::GetImageFromCache(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        if (!im->IsChanged())
        {
            m_ImageExpireList.remove(im);
            m_ImageExpireList.push_back(im);
            return m_ImageBitmapMap[im];
        }
        else
        {
            DeleteFormatImagePriv(im);
        }
    }

    im->SetChanged(false);
    D3D9Image *newimage = NULL;
    if (m_render)
        newimage = new D3D9Image(m_render,im->size());

    if (newimage && newimage->IsValid())
    {
        CheckFormatImage(im);
        IncreaseCacheSize(newimage->GetSize());
        newimage->UpdateImage(im);
        m_ImageBitmapMap[im] = newimage;
        m_ImageExpireList.push_back(im);

        while (m_CacheSize > m_MaxCacheSize)
        {
            MythImage *expiredIm = m_ImageExpireList.front();
            m_ImageExpireList.pop_front();
            DeleteFormatImagePriv(expiredIm);
            DeleteBitmaps();
        }
    }
    else
    {
       VERBOSE(VB_IMPORTANT, LOC + "Failed to create D3D9 UI bitmap.");
       if (newimage)
           delete newimage;
    }

    return newimage;
}
