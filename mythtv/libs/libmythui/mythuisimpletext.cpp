
#include "mythuisimpletext.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <QDomDocument>
#include <QFontMetrics>
#include <QString>
#include <QHash>

#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUISimpleText::MythUISimpleText(const QString &text,
                                   const MythFontProperties &font,
                                   const QRect rect, Qt::Alignment align,
                                   MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_justification(align),
      m_font(font),
      m_message(text.trimmed())
{
    SetArea(MythRect(rect));
    m_font = font;
}

void MythUISimpleText::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                                int alphaMod, QRect clipRect)
{
    QRect area = GetArea().toQRect();
    area.translate(xoffset, yoffset);

    int alpha = CalcAlpha(alphaMod);

    p->SetClipRect(clipRect);
    p->DrawText(area, m_message, m_justification, m_font, alpha, area);
}

void MythUISimpleText::CopyFrom(MythUIType *base)
{
    auto *text = dynamic_cast<MythUISimpleText *>(base);

    if (!text)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

    m_justification = text->m_justification;
    m_message = text->m_message;
    m_font = text->m_font;

    MythUIType::CopyFrom(base);
}

void MythUISimpleText::CreateCopy(MythUIType *parent)
{
    auto *text = new MythUISimpleText(parent, objectName());
    text->CopyFrom(this);
}
