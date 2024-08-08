
// Own header
#include "mythuishape.h"

// C++
#include <algorithm>

// qt
#include <QColor>
#include <QDomDocument>
#include <QPainter>
#include <QSize>
#include <utility>

// myth
#include "libmythbase/mythlogging.h"

#include "mythpainter.h"
#include "mythimage.h"
#include "mythmainwindow.h"

MythUIShape::MythUIShape(MythUIType *parent, const QString &name)
    : MythUIType(parent, name)
{
    // Workaround a feature in Qt 4.8 where a QPen constructed with
    // style Qt::NoPen returns width = 1;
    // Qt 5.11 source still seems to do the same.
    m_linePen.setWidth(0);
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
    m_fillBrush = std::move(fill);
}

void MythUIShape::SetLinePen(QPen pen)
{
    m_linePen = std::move(pen);
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIShape::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                           int alphaMod, QRect clipRect)
{
    int alpha = CalcAlpha(alphaMod);
    QRect area = GetArea();
    m_cropRect.CalculateArea(area);

    if (!m_cropRect.isEmpty())
        area &= m_cropRect.toQRect();

    area.translate(xoffset, yoffset);

    p->SetClipRect(clipRect);
    if (m_type == "box")
        p->DrawRect(area, m_fillBrush, m_linePen, alpha);
    else if (m_type == "roundbox")
        p->DrawRoundRect(area, m_cornerRadius, m_fillBrush, m_linePen, alpha);
    else if (m_type == "ellipse")
        p->DrawEllipse(area, m_fillBrush, m_linePen, alpha);
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
            auto brushColor = QColor(color);
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
        {
            m_fillBrush.setStyle(Qt::NoBrush);
        }
    }
    else if (element.tagName() == "line")
    {
        QString style = element.attribute("style", "solid");
        QString color = element.attribute("color", "");

        if (style == "solid" && !color.isEmpty())
        {
            int orig_width = element.attribute("width", "1").toInt();
            int width = (orig_width) ? std::max(NormX(orig_width), 1) : 0;
            int alpha = element.attribute("alpha", "255").toInt();
            auto lineColor = QColor(color);
            lineColor.setAlpha(alpha);
            m_linePen.setColor(lineColor);
            m_linePen.setWidth(width);
            m_linePen.setStyle(Qt::SolidLine);
        }
        else
        {
            m_linePen.setStyle(Qt::NoPen);
        }
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
    auto *shape = dynamic_cast<MythUIShape *>(base);

    if (!shape)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

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
    auto *shape = new MythUIShape(parent, objectName());
    shape->CopyFrom(this);
}
