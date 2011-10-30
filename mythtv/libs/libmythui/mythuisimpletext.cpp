
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
      m_Message(text.trimmed()),
      m_Font(font)
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

#if 0 // Any reason to support this in a theme?
bool MythUISimpleText::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
    }
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);

        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);

        if (fp)
        {
            MythFontProperties font = *fp;
            int screenHeight = GetMythMainWindow()->GetUIScreenRect().height();
            font.Rescale(screenHeight);
            int fontStretch = GetMythUI()->GetFontStretch();
            font.AdjustStretch(fontStretch);
            QString state = element.attribute("state", "");

            if (!state.isEmpty())
            {
                m_FontStates.insert(state, font);
            }
            else
            {
                m_FontStates.insert("default", font);
                *m_Font = m_FontStates["default"];
            }
        }
    }
    else if (element.tagName() == "value")
    {
        if (element.attribute("lang", "").isEmpty())
        {
            m_Message = qApp->translate("ThemeUI",
                                        parseText(element).toUtf8(), NULL,
                                        QCoreApplication::UnicodeUTF8);
        }
        else if (element.attribute("lang", "").toLower() ==
                 gCoreContext->GetLanguageAndVariant())
        {
            m_Message = parseText(element);
        }
        else if (element.attribute("lang", "").toLower() ==
                 gCoreContext->GetLanguage())
        {
            m_Message = parseText(element);
        }

        m_DefaultMessage = m_Message;
        SetText(m_Message);
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();
        SetJustification(parseAlignment(align));
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}
#endif

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

#if 0 // Any reason to support this in a theme?
void MythUISimpleText::Finalize(void)
{
}
#endif
