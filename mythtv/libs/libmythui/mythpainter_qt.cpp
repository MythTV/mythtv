
// QT headers
#include <QPainter>
#include <QPixmap>

// MythUI headers
#include "mythpainter_qt.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"

// MythDB headers
#include "libmythbase/compat.h"
#include "libmythbase/mythlogging.h"

class MythQtImage : public MythImage
{
  public:
    explicit MythQtImage(MythQtPainter *parent) :
        MythImage(parent, "MythQtImage") { }

    void SetChanged(bool change = true) override; // MythImage
    QPixmap *GetPixmap(void) { return m_pixmap; }
    void SetPixmap(QPixmap *p) { m_pixmap = p; }

    bool NeedsRegen(void) const { return m_bRegenPixmap; }
    void RegeneratePixmap(void);

  protected:
    QPixmap *m_pixmap   {nullptr};
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
    if (!m_pixmap)
        m_pixmap = new QPixmap;

    if (m_pixmap)
    {
        *m_pixmap = QPixmap::fromImage(*((QImage *)this));
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

void MythQtPainter::SetClipRect(const QRect clipRect)
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
    {
        m_painter->setClipping(false);
    }
}

void MythQtPainter::DrawImage(const QRect r, MythImage *im,
                              const QRect src, int alpha)
{
    if (!m_painter)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "FATAL ERROR: DrawImage called with no painter");
        return;
    }

    auto *qim = reinterpret_cast<MythQtImage *>(im);

    if (qim->NeedsRegen())
        qim->RegeneratePixmap();

    m_painter->setOpacity(static_cast<qreal>(alpha) / 255.0);
    m_painter->drawPixmap(r.topLeft(), *(qim->GetPixmap()), src);
    m_painter->setOpacity(1.0);
}

MythImage *MythQtPainter::GetFormatImagePriv()
{
    return new MythQtImage(this);
}

void MythQtPainter::DeleteFormatImagePriv(MythImage *im)
{
    auto *qim = dynamic_cast<MythQtImage *>(im);

    QMutexLocker locker(&m_imageDeleteLock);
    if (qim && qim->GetPixmap())
    {
        m_imageDeleteList.push_back(qim->GetPixmap());
        qim->SetPixmap(nullptr);
    }
}
