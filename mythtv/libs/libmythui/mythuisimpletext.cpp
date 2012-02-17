
#include "mythuisimpletext.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <QDomDocument>
#include <QFontMetrics>
#include <QString>
#include <QHash>

#include "mythlogging.h"

#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythcorecontext.h"

#include "compat.h"

MythUISimpleText::MythUISimpleText(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_Justification(Qt::AlignLeft | Qt::AlignTop)
{
}

MythUISimpleText::MythUISimpleText(const QString &text,
                                   const MythFontProperties &font,
                                   const QRect & rect, Qt::Alignment align,
                                   MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_Justification(align),
      m_Font(font),
      m_Message(text.trimmed())
{
    SetArea(rect);
    m_Font = font;
}

MythUISimpleText::~MythUISimpleText()
{
}

void MythUISimpleText::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                                int alphaMod, QRect clipRect)
{
    QRect area = GetArea().toQRect();
    area.translate(xoffset, yoffset);

    int alpha = CalcAlpha(alphaMod);

    p->DrawText(area, m_Message, m_Justification, m_Font, alpha, area);
}

void MythUISimpleText::CopyFrom(MythUIType *base)
{
    MythUISimpleText *text = dynamic_cast<MythUISimpleText *>(base);

    if (!text)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

    m_Justification = text->m_Justification;
    m_Message = text->m_Message;
    m_Font = text->m_Font;

    MythUIType::CopyFrom(base);
}

void MythUISimpleText::CreateCopy(MythUIType *parent)
{
    MythUISimpleText *text = new MythUISimpleText(parent, objectName());
    text->CopyFrom(this);
}
