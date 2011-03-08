// Config header
#include "config.h"

// QT headers
#include <QCoreApplication>
#include <QPainter>
#include <QMutex>
#include <QX11Info>

// Mythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythrender_vdpau.h"

// Own header
#include "mythpainter_vdpau.h"

#define LOC QString("VDPAU Painter: ")

MythVDPAUPainter::MythVDPAUPainter(MythRenderVDPAU *render) :
    MythPainter(), m_render(render), m_created_render(true), m_target(0),
    m_swap_control(true)
{
    if (m_render)
        m_created_render = false;
}

MythVDPAUPainter::~MythVDPAUPainter()
{
    Teardown();
}

bool MythVDPAUPainter::InitVDPAU(QPaintDevice *parent)
{
    if (m_render)
        return  true;

    QWidget *real_parent = (QWidget*)parent;
    if (!real_parent)
        return false;

    m_render = new MythRenderVDPAU();
    if (!m_render)
        return false;

    m_created_render = true;
    if (m_render->Create(real_parent->size(), real_parent->winId()))
        return true;

    Teardown();
    return false;
}

void MythVDPAUPainter::Teardown(void)
{
    ExpireImages();
    FreeResources();

    m_ImageBitmapMap.clear();
    m_ImageExpireList.clear();
    m_bitmapDeleteList.clear();

    if (m_render)
    {
        if (m_created_render)
            delete m_render;
        m_created_render = true;
        m_render = NULL;
    }
}

void MythVDPAUPainter::FreeResources(void)
{
    ClearCache();
    DeleteBitmaps();
}

void MythVDPAUPainter::Begin(QPaintDevice *parent)
{
    if (!m_render)
    {
        if (!InitVDPAU(parent))
        {
            VERBOSE(VB_IMPORTANT, "Failed to create VDPAU render.");
            return;
        }
    }

    if (m_render->WasPreempted())
        ClearCache();
    DeleteBitmaps();

    if (m_target)
        m_render->DrawBitmap(0, m_target, NULL, NULL);
    else if (m_swap_control)
        m_render->WaitForFlip();

    MythPainter::Begin(parent);
}

void MythVDPAUPainter::End(void)
{
    if (m_render && m_swap_control)
        m_render->Flip();
    MythPainter::End();
}

void MythVDPAUPainter::ClearCache(void)
{
    VERBOSE(VB_GENERAL, LOC + "Clearing VDPAU painter cache.");

    QMutexLocker locker(&m_bitmapDeleteLock);
    QMapIterator<MythImage *, uint32_t> it(m_ImageBitmapMap);
    while (it.hasNext())
    {
        it.next();
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[it.key()]);
        m_ImageExpireList.remove(it.key());
    }
    m_ImageBitmapMap.clear();
}

void MythVDPAUPainter::DeleteBitmaps(void)
{
    QMutexLocker locker(&m_bitmapDeleteLock);
    while (!m_bitmapDeleteList.empty())
    {
        uint bitmap = m_bitmapDeleteList.front();
        m_bitmapDeleteList.pop_front();
        DecreaseCacheSize(m_render->GetBitmapSize(bitmap));
        m_render->DestroyBitmapSurface(bitmap);
    }
}

void MythVDPAUPainter::DrawImage(const QRect &r, MythImage *im,
                                 const QRect &src, int alpha)
{
    if (m_render)
        m_render->DrawBitmap(GetTextureFromCache(im), m_target,
                             &src, &r /*dst*/, alpha, 255, 255, 255);
}

void MythVDPAUPainter::DeleteFormatImagePriv(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        QMutexLocker locker(&m_bitmapDeleteLock);
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[im]);
        m_ImageBitmapMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

uint MythVDPAUPainter::GetTextureFromCache(MythImage *im)
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
    uint newbitmap = 0;
    if (m_render)
        newbitmap = m_render->CreateBitmapSurface(im->size());

    if (newbitmap)
    {
        CheckFormatImage(im);
        m_render->UploadMythImage(newbitmap, im);
        m_ImageBitmapMap[im] = newbitmap;
        m_ImageExpireList.push_back(im);
        IncreaseCacheSize(im->size());
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
       VERBOSE(VB_IMPORTANT, LOC + "Failed to create VDPAU UI bitmap.");
    }

    return newbitmap;
}
