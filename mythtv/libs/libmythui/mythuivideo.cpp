// C/C++
#include <cstdlib>

// QT
#include <QRect>
#include <QDomDocument>

// libmythbase
#include "libmythbase/mythlogging.h"

// Mythui
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythscreentype.h"
#include "mythuivideo.h"
#include "mythimage.h"

#define LOC      QString("MythUIVideo(0x%1): ").arg((uint64_t)this, 0, 16)

MythUIVideo::MythUIVideo(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_image(GetMythPainter()->GetFormatImage())
{
}

MythUIVideo::~MythUIVideo()
{
    if (m_image)
    {
        m_image->DecrRef();
        m_image = nullptr;
    }
}

/**
 *  \brief Reset the video back to the default defined in the theme
 */
void MythUIVideo::Reset(void)
{
    if (m_image)
    {
        m_image->DecrRef();
        m_image = nullptr;
    }

    m_image = GetMythPainter()->GetFormatImage();

    MythUIType::Reset();
}

void MythUIVideo::UpdateFrame(MythImage *image)
{
    m_image->Assign(*image);

    SetRedraw();
}

void MythUIVideo::UpdateFrame(QPixmap *pixmap)
{
    m_image->Assign(*pixmap);

    SetRedraw();
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIVideo::Pulse(void)
{

    MythUIType::Pulse();
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIVideo::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                           int alphaMod, QRect clipRect)
{
    QRect area = GetArea();
    area.translate(xoffset, yoffset);

    if (!m_image || m_image->isNull())
        return;

    if (m_image) {
        p->SetClipRect(clipRect);
        p->DrawImage(area.x(), area.y(), m_image, alphaMod);
    }
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIVideo::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "backgroundcolor")
    {
        m_backgroundColor = QColor(getFirstText(element));
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIVideo::CopyFrom(MythUIType *base)
{
    MythUIType::CopyFrom(base);
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIVideo::CreateCopy(MythUIType *parent)
{
    auto *im = new MythUIVideo(parent, objectName());
    im->CopyFrom(this);
}
