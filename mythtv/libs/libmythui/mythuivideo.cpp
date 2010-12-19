// C/C++
#include <cstdlib>

// QT
#include <QRect>
#include <QDomDocument>

// Libmythdb
#include "mythverbose.h"

// Mythui
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythscreentype.h"
#include "mythuivideo.h"
#include "mythimage.h"

#define LOC      QString("MythUIVideo(%1): ").arg((long long)this)
#define LOC_ERR  QString("MythUIVideo(%1) ERROR: ").arg((long long)this)
#define LOC_WARN QString("MythUIVideo(%1) WARNING: ").arg((long long)this)

MythUIVideo::MythUIVideo(MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_image = NULL;
    m_backgroundColor = QColor(Qt::black);
}

MythUIVideo::~MythUIVideo()
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }
}

/**
 *  \brief Reset the video back to the default defined in the theme
 */
void MythUIVideo::Reset(void)
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }
    MythUIType::Reset();
}

void MythUIVideo::UpdateFrame(MythImage *image)
{
    if (m_image)
        m_image->DownRef();

    m_image = image;
    m_image->UpRef();

    SetRedraw();
}

void MythUIVideo::UpdateFrame(QPixmap *pixmap)
{
    if (m_image)
        m_image->DownRef();

    m_image = GetMythPainter()->GetFormatImage();
    m_image->Assign(*pixmap);
    m_image->UpRef();

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

    if (m_image)
        p->DrawImage(area.x(), area.y(), m_image, alphaMod);
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
        return MythUIType::ParseElement(filename, element, showWarnings);

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
    MythUIVideo *im = new MythUIVideo(parent, objectName());
    im->CopyFrom(this);
}
