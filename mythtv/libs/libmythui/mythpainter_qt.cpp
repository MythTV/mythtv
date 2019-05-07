
// QT headers
#include <QPainter>
#include <QPixmap>

// MythUI headers
#include "mythpainter_qt.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"

// MythDB headers
#include "compat.h"
#include "mythlogging.h"

class MythQtImage : public MythImage
{
  public:
    explicit MythQtImage(MythQtPainter *parent) :
        MythImage(parent, "MythQtImage") { }

    void SetChanged(bool change = true) override; // MythImage
    QPixmap *GetPixmap(void) { return m_Pixmap; }
    void SetPixmap(QPixmap *p) { m_Pixmap = p; }

    bool NeedsRegen(void) { return m_bRegenPixmap; }
    void RegeneratePixmap(void);

  protected:
    QPixmap *m_Pixmap   {nullptr};
    bool m_bRegenPixmap {false};
};

void MythQtImage::SetChanged(bool change)
{
    if (change)
        m_bRegenPixmap = true;

    MythImage::SetChanged(change);
}

void MythQtImage::RegeneratePixmap(void)
{
    // We allocate the pixmap here so it is done in the UI
    // thread since QPixmap uses non-reentrant X calls.
    if (!m_Pixmap)
        m_Pixmap = new QPixmap;

    if (m_Pixmap)
    {
        *m_Pixmap = QPixmap::fromImage(*((QImage *)this));
        m_bRegenPixmap = false;
    }
}

MythQtPainter::~MythQtPainter()
{
    Teardown();
    DeletePixmaps();
}

void MythQtPainter::DeletePixmaps(void)
{
    QMutexLocker locker(&m_imageDeleteLock);
    while (!m_imageDeleteList.empty())
    {
        QPixmap *pm = m_imageDeleteList.front();
        m_imageDeleteList.pop_front();
        delete pm;
    }
}

void MythQtPainter::Begin(QPaintDevice *parent)
{
    if (!parent)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "FATAL ERROR: No parent widget defined for QT Painter, bailing");
        return;
    }

    MythPainter::Begin(parent);

    m_painter = new QPainter(parent);
    m_clipRegion = QRegion(QRect(0, 0, 0, 0));

    DeletePixmaps();
}

void MythQtPainter::End(void)
{
    m_painter->end();
    delete m_painter;

    MythPainter::End();
}

void MythQtPainter::SetClipRect(const QRect &clipRect)
{
    m_painter->setClipRect(clipRect);
    if (!clipRect.isEmpty())
    {
        m_painter->setClipping(true);
        if (m_clipRegion.isEmpty())
            m_clipRegion = QRegion(clipRect);
        else
            m_clipRegion = m_clipRegion.united(clipRect);
    }
    else
        m_painter->setClipping(false);
}

void MythQtPainter::DrawImage(const QRect &r, MythImage *im,
                              const QRect &src, int alpha)
{
    if (!m_painter)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "FATAL ERROR: DrawImage called with no painter");
        return;
    }

    MythQtImage *qim = reinterpret_cast<MythQtImage *>(im);

    if (qim->NeedsRegen())
        qim->RegeneratePixmap();

    m_painter->setOpacity(static_cast<float>(alpha) / 255.0F);
    m_painter->drawPixmap(r.topLeft(), *(qim->GetPixmap()), src);
    m_painter->setOpacity(1.0);
}

MythImage *MythQtPainter::GetFormatImagePriv()
{
    return new MythQtImage(this);
}

void MythQtPainter::DeleteFormatImagePriv(MythImage *im)
{
    MythQtImage *qim = static_cast<MythQtImage *>(im);

    QMutexLocker locker(&m_imageDeleteLock);
    if (qim->GetPixmap())
    {
        m_imageDeleteList.push_back(qim->GetPixmap());
        qim->SetPixmap(nullptr);
    }
}
