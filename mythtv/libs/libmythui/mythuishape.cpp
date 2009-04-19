// qt
#include <QDomDocument>

// myth
#include "mythverbose.h"
#include "mythpainter.h"

#include "mythuishape.h"

MythUIShape::MythUIShape(MythUIType *parent, const QString &name)
          : MythUIType(parent, name)
{
    m_type = "box";
    m_fillColor = QColor();
    m_lineColor = QColor();
    m_drawFill = false;
    m_drawLine = false;
    m_lineWidth = 1;
    m_cornerRadius = 10;
}

MythUIShape::~MythUIShape()
{
}

void MythUIShape::Reset()
{
    MythUIType::Reset();
}

void MythUIShape::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect)
{
    QRect area = m_Area.toQRect();
    area.translate(xoffset, yoffset);

    QColor fillColor(m_fillColor);
    fillColor.setAlpha((int)(m_fillColor.alpha() * (alphaMod / 255.0)));
    QColor lineColor(m_lineColor);
    lineColor.setAlpha((int)(m_lineColor.alpha() * (alphaMod / 255.0)));

    if (m_type == "box")
        p->DrawRect(area, m_drawFill, fillColor,
                    m_drawLine, m_lineWidth, lineColor);
    else if (m_type == "roundbox")
        p->DrawRoundRect(area, m_cornerRadius, m_drawFill, fillColor,
                         m_drawLine, m_lineWidth, lineColor);
}

bool MythUIShape::ParseElement(QDomElement &element)
{
    if (element.tagName() == "type")
    {
        m_type = getFirstText(element);
    }
    else if (element.tagName() == "fill")
    {
        QString color = element.attribute("color", "");
        int alpha = element.attribute("alpha", "255").toInt();

        if (!color.isEmpty())
        {
            m_fillColor = QColor(color);
            m_fillColor.setAlpha(alpha);
            m_drawFill = true;
        }
        else
        {
           m_fillColor = QColor();
           m_drawFill = false;
        }
    }
    else if (element.tagName() == "line")
    {
        QString color = element.attribute("color", "");
        int alpha = element.attribute("alpha", "255").toInt();

        if (!color.isEmpty())
        {
            m_lineColor = QColor(color);
            m_lineColor.setAlpha(alpha);
            m_drawLine = true;
        }
        else
        {
           m_lineColor = QColor();
           m_drawLine = false;
        }

        m_lineWidth = element.attribute("width", "1").toInt();
    }
    else if (element.tagName() == "cornerradius")
    {
        m_cornerRadius = getFirstText(element).toInt();
    }
    else
        return MythUIType::ParseElement(element);

    return true;
}

void MythUIShape::CopyFrom(MythUIType *base)
{
    MythUIShape *shape = dynamic_cast<MythUIShape *>(base);
    if (!shape)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_type = shape->m_type;
    m_fillColor = shape->m_fillColor;
    m_lineColor = shape->m_lineColor;
    m_lineWidth = shape->m_lineWidth;
    m_drawFill = shape->m_drawFill;
    m_drawLine = shape->m_drawLine;
    m_cornerRadius = shape->m_cornerRadius;

    MythUIType::CopyFrom(base);
}

void MythUIShape::CreateCopy(MythUIType *parent)
{
    MythUIShape *shape = new MythUIShape(parent, objectName());
    shape->CopyFrom(this);
}

