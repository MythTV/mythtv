
// Own header
#include "mythuishape.h"

// C++
#include <algorithm>
using namespace std;

// qt
#include <QDomDocument>
#include <QPainter>
#include <QSize>
#include <QColor>

// myth
#include "mythverbose.h"
#include "mythpainter.h"
#include "mythimage.h"
#include "mythmainwindow.h"

MythUIShape::MythUIShape(MythUIType *parent, const QString &name)
          : MythUIType(parent, name)
{
    m_image = NULL;
    m_type = "box";
    m_fillBrush = QBrush(Qt::NoBrush);
    m_linePen = QPen(Qt::NoPen);
    m_cornerRadius = 10;
    m_cropRect = MythRect(0,0,0,0);
}

MythUIShape::~MythUIShape()
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }
}

/**
 *  \copydoc MythUIType::Reset()
 */
void MythUIShape::Reset()
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }
    
    MythUIType::Reset();
}

void MythUIShape::SetCropRect(int x, int y, int width, int height)
{
    SetCropRect(MythRect(x, y, width, height));
}

void MythUIShape::SetCropRect(const MythRect &rect)
{
    m_cropRect = rect;
    SetRedraw();
}

void MythUIShape::SetFillBrush(QBrush fill)
{
    m_fillBrush = fill;
}

void MythUIShape::SetLinePen(QPen pen)
{
    m_linePen = pen;
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIShape::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect)
{
    QRect area = GetArea();
    area.translate(xoffset, yoffset);

    if (!m_image || m_image->isNull())
    {
        if (m_type == "box")
            DrawRect(area, m_fillBrush, m_linePen);
        else if (m_type == "roundbox")
            DrawRoundRect(area, m_cornerRadius, m_fillBrush, m_linePen);
        else if (m_type == "ellipse")
            DrawEllipse(area, m_fillBrush, m_linePen);
    }

    if (m_image)
    {
        QRect dest = QRect(area.x(), area.y(), m_image->width(), m_image->height());
        QRect srcRect;
        m_cropRect.CalculateArea(m_Area);
        if (!m_cropRect.isEmpty())
            srcRect = m_cropRect.toQRect();
        else
            srcRect = m_image->rect();
        p->DrawImage(dest, m_image, srcRect, CalcAlpha(alphaMod));
    }
}

void MythUIShape::DrawRect(const QRect &area,const QBrush &fillBrush,
                           const QPen &linePen)
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }

    QImage image(QSize(area.width(), area.height()), QImage::Format_ARGB32);
    image.fill(0x00000000);
    QPainter painter(&image);

    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(linePen);
    painter.setBrush(fillBrush);
    
    int lineWidth = linePen.width();
    QRect r(lineWidth, lineWidth,
            area.width() - (lineWidth * 2), area.height() - (lineWidth * 2));
    painter.drawRect(r);

    painter.end();

    m_image = GetPainter()->GetFormatImage();
    m_image->UpRef();
    m_image->Assign(image);
}

void MythUIShape::DrawRoundRect(const QRect &area, int radius,
                                const QBrush &fillBrush, const QPen &linePen)
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }

    QImage image(QSize(area.width(), area.height()), QImage::Format_ARGB32);
    image.fill(0x00000000);
    QPainter painter(&image);

    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(linePen);
    painter.setBrush(fillBrush);

    if ((area.width() / 2) < radius)
        radius = area.width() / 2;

    if ((area.height() / 2) < radius)
        radius = area.height() / 2;

    int lineWidth = linePen.width();
    QRect r(lineWidth, lineWidth,
            area.width() - (lineWidth * 2), area.height() - (lineWidth * 2));
    painter.drawRoundedRect(r, (qreal)radius, qreal(radius));

    painter.end();

    m_image = GetPainter()->GetFormatImage();
    m_image->UpRef();
    m_image->Assign(image);
}

void MythUIShape::DrawEllipse(const QRect &area,const QBrush &fillBrush,
                              const QPen &linePen)
{
    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }

    QImage image(QSize(area.width(), area.height()), QImage::Format_ARGB32);
    image.fill(0x00000000);
    QPainter painter(&image);

    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(linePen);
    painter.setBrush(fillBrush);
    
    int lineWidth = linePen.width();
    QRect r(lineWidth, lineWidth,
            area.width() - (lineWidth * 2), area.height() - (lineWidth * 2));
    painter.drawEllipse(r);

    painter.end();

    m_image = GetPainter()->GetFormatImage();
    m_image->UpRef();
    m_image->Assign(image);
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIShape::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "type")
    {
        QString type = getFirstText(element);
        if (type == "box" || type == "roundbox" || type == "ellipse") // Validate input
            m_type = type;
    }
    else if (element.tagName() == "fill")
    {
        QString style = element.attribute("style", "solid");
        QString color = element.attribute("color", "");
        int alpha = element.attribute("alpha", "255").toInt();

        if (style == "solid" && !color.isEmpty())
        {
            m_fillBrush.setStyle(Qt::SolidPattern);
            QColor brushColor = QColor(color);
            brushColor.setAlpha(alpha);
            m_fillBrush.setColor(brushColor);
        }
        else if (style == "gradient")
        {
            for (QDomNode child = element.firstChild(); !child.isNull();
                child = child.nextSibling())
            {
                QDomElement childElem = child.toElement();
                if (childElem.tagName() == "gradient")
                    m_fillBrush = parseGradient(childElem);
            }
        }
        else
            m_fillBrush.setStyle(Qt::NoBrush);
    }
    else if (element.tagName() == "line")
    {
        QString style = element.attribute("style", "solid");
        QString color = element.attribute("color", "");

        if (style == "solid" && !color.isEmpty())
        {
            int orig_width = element.attribute("width", "1").toInt();
            int width = (orig_width) ? max(NormX(orig_width),1) : 0;
            int alpha = element.attribute("alpha", "255").toInt();
            QColor lineColor = QColor(color);
            lineColor.setAlpha(alpha);
            m_linePen.setColor(lineColor);
            m_linePen.setWidth(width);
            m_linePen.setStyle(Qt::SolidLine);
        }
        else
           m_linePen.setStyle(Qt::NoPen);
    }
    else if (element.tagName() == "cornerradius")
    {
        m_cornerRadius = NormX(getFirstText(element).toInt());
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
void MythUIShape::CopyFrom(MythUIType *base)
{
    MythUIShape *shape = dynamic_cast<MythUIShape *>(base);
    if (!shape)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_image = shape->m_image;
    if (m_image)
        m_image->UpRef();

    m_type = shape->m_type;
    m_fillBrush = shape->m_fillBrush;
    m_linePen = shape->m_linePen;
    m_cornerRadius = shape->m_cornerRadius;
    m_cropRect = shape->m_cropRect;

    MythUIType::CopyFrom(base);
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIShape::CreateCopy(MythUIType *parent)
{
    MythUIShape *shape = new MythUIShape(parent, objectName());
    shape->CopyFrom(this);
}

