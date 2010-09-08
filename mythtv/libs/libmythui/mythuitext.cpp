
#include "mythuitext.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <QDomDocument>
#include <QFontMetrics>
#include <QString>
#include <QHash>

#include "mythverbose.h"

#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythcorecontext.h"

#include "compat.h"

MythUIText::MythUIText(MythUIType *parent, const QString &name)
           : MythUIType(parent, name),
            m_Justification(Qt::AlignLeft | Qt::AlignTop), m_OrigDisplayRect(),
            m_AltDisplayRect(),                            m_drawRect(),
            m_Message(""),                                 m_CutMessage(""),
            m_DefaultMessage(""),                          m_TemplateText(""),
            m_Cutdown(true),
            m_Font(new MythFontProperties()),              m_colorCycling(false),
            m_startColor(),                                m_endColor(),
            m_numSteps(0),                                 m_curStep(0),
            curR(0.0),              curG(0.0),             curB(0.0),
            incR(0.0),              incG(0.0),             incB(0.0)
{
    m_MultiLine = false;
    m_scrolling = false;
    m_scrollDirection = ScrollLeft;
    m_textCase = CaseNormal;

    m_FontStates.insert("default", MythFontProperties());
    *m_Font = m_FontStates["default"];
}

MythUIText::MythUIText(const QString &text, const MythFontProperties &font,
                       QRect displayRect, QRect altDisplayRect,
                       MythUIType *parent, const QString &name)
           : MythUIType(parent, name),
             m_Justification(Qt::AlignLeft | Qt::AlignTop),
             m_OrigDisplayRect(displayRect), m_AltDisplayRect(altDisplayRect),
             m_drawRect(displayRect),        m_Message(text.trimmed()),
             m_CutMessage(""),               m_DefaultMessage(text),
             m_Cutdown(true),                m_Font(new MythFontProperties()),
             m_colorCycling(false),          m_startColor(),
             m_endColor(),                   m_numSteps(0),
             m_curStep(0),
             curR(0.0),      curG(0.0),      curB(0.0),
             incR(0.0),      incG(0.0),      incB(0.0)
{
    m_MultiLine = false;
    m_scrolling = false;
    m_scrollDirection = ScrollLeft;
    m_textCase = CaseNormal;

    SetArea(displayRect);
    m_FontStates.insert("default", font);
    *m_Font = m_FontStates["default"];
}

MythUIText::~MythUIText()
{
    delete m_Font;
    m_Font = NULL;
}

void MythUIText::Reset()
{
    if (m_Message != m_DefaultMessage)
    {
        SetText(m_DefaultMessage);
        SetRedraw();
    }

    SetFontState("default");

    MythUIType::Reset();
}

void MythUIText::SetText(const QString &text)
{
    QString newtext = text;

    if (newtext == m_Message)
        return;

    if (newtext.isEmpty())
        m_Message = m_DefaultMessage;

    m_Message = newtext;
    m_CutMessage.clear();
    FillCutMessage();

    if (m_scrolling)
    {
        QFontMetrics fm(GetFontProperties()->face());
        QSize stringSize = fm.size(Qt::TextSingleLine, m_CutMessage);
        SetDrawRectSize(stringSize.width(), m_Area.height());
    }

    SetRedraw();
}

void MythUIText::SetTextFromMap(QHash<QString, QString> &map)
{
    if (!IsVisible())
        return;

    if (map.contains(objectName()))
    {
        QString newText = GetTemplateText();
        if (newText.isEmpty())
            newText = GetDefaultText();
        QRegExp regexp("%(\\|(.))?([^\\|]+)(\\|(.))?%");
        regexp.setMinimal(true);
        if (newText.contains(regexp))
        {
            int pos = 0;
            QString tempString = newText;
            while ((pos = regexp.indexIn(newText, pos)) != -1)
            {
                QString key = regexp.cap(3).toLower().trimmed();
                QString replacement;
                if (!map.value(key).isEmpty())
                {
                    replacement = QString("%1%2%3")
                                            .arg(regexp.cap(2))
                                            .arg(map.value(key))
                                            .arg(regexp.cap(5));
                }
                tempString.replace(regexp.cap(0), replacement);
                pos += regexp.matchedLength();
            }
            newText = tempString;
        }
        else
            newText = map.value(objectName());

        SetText(newText);
    }
}

QString MythUIText::GetText(void) const
{
    return m_Message;
}

QString MythUIText::GetDefaultText(void) const
{
    return m_DefaultMessage;
}

void MythUIText::SetFontProperties(const MythFontProperties &fontProps)
{
    m_FontStates.insert("default", fontProps);
    *m_Font = m_FontStates["default"];
    FillCutMessage();
    SetRedraw();
}

void MythUIText::SetFontState(const QString &state)
{
    if (m_FontStates.contains(state))
        *m_Font = m_FontStates[state];
    else
        *m_Font = m_FontStates["default"];

    FillCutMessage();
    SetRedraw();
}

void MythUIText::UseAlternateArea(bool useAlt)
{
    if (useAlt && m_AltDisplayRect.width() > 1)
        MythUIType::SetArea(m_AltDisplayRect);
    else
        MythUIType::SetArea(m_OrigDisplayRect);
    FillCutMessage();
}

void MythUIText::SetJustification(int just)
{
    if (m_Justification != (just & ~Qt::TextWordWrap))
    {
        // preserve the wordbreak attribute, drop everything else
        m_Justification = m_Justification & Qt::TextWordWrap;
        m_Justification |= just;
        FillCutMessage();
        SetRedraw();
    }
}

int MythUIText::GetJustification(void)
{
    return m_Justification;
}

void MythUIText::SetCutDown(bool cut)
{
    m_Cutdown = cut;
    FillCutMessage();
    SetRedraw();
}

void MythUIText::SetMultiLine(bool multiline)
{
    m_MultiLine = multiline;
    if (m_MultiLine)
        m_Justification |= Qt::TextWordWrap;
    else
        m_Justification &= ~Qt::TextWordWrap;
    FillCutMessage();
    SetRedraw();
}

void MythUIText::SetArea(const MythRect &rect)
{
    MythUIType::SetArea(rect);
    m_CutMessage.clear();

    m_drawRect = m_Area;
    if (m_scrolling)
    {
        QFontMetrics fm(GetFontProperties()->face());
        QSize stringSize = fm.size(Qt::TextSingleLine, m_Message);
        SetDrawRectSize(stringSize.width(), m_Area.height());
    }
    FillCutMessage();
}

void MythUIText::SetPosition(const MythPoint &pos)
{
    MythUIType::SetPosition(pos);
    m_drawRect.moveTopLeft(m_Area.topLeft());
}

void MythUIText::SetDrawRectSize(const int width, const int height)
{
    QSize newsize(width,height);

    if (newsize == m_drawRect.size())
        return;

    m_drawRect.setSize(newsize);
    SetRedraw();
}

void MythUIText::SetDrawRectPosition(const int x, const int y)
{
    int startx = m_Area.x() + x;
    int starty = m_Area.y() + y;

    QPoint newpoint(startx,starty);
    if (newpoint == m_drawRect.topLeft())
        return;

    m_drawRect.moveTopLeft(QPoint(startx,starty));
    SetRedraw();
}

void MythUIText::MoveDrawRect(const int x, const int y)
{
    int newx = m_drawRect.x() + x;
    int newy = m_drawRect.y() + y;

    QPoint newpoint(newx,newy);
    if (newpoint == m_drawRect.topLeft())
        return;

    m_drawRect.moveTopLeft(QPoint(newx,newy));
    SetRedraw();
}

void MythUIText::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect)
{
    QRect area = GetArea().toQRect();
    area.translate(xoffset, yoffset);
    QRect drawrect = m_drawRect.toQRect();
    drawrect.translate(xoffset, yoffset);

    int alpha = CalcAlpha(alphaMod);

    p->DrawText(drawrect, m_CutMessage, m_Justification, *m_Font, alpha, area);
}

/*
 * If minsize and multiline are defined, minimize the width by using
 * as many lines as possible.
 */
bool MythUIText::MakeNarrow(QRect &min_rect)
{
    QFontMetrics fm(GetFontProperties()->face());

    if (m_scrolling || !m_MultiLine)
    {
        min_rect = fm.boundingRect(m_Area, m_Justification, m_CutMessage);
        return false;
    }

    /*
     * Shrinkage is desired, and multiline is allowed
     * If more than one line will fit, squeeze to the left
     */
    if ((m_Area.height() + fm.leading()) / fm.lineSpacing() < 2)
    {
        min_rect = fm.boundingRect(m_Area, m_Justification, m_CutMessage);
        return false;
    }

    // Give plenty of vertical space, to prevent clipping from coloring results
    min_rect = m_Area;
    min_rect.setHeight(m_Area.height() * 2);

    QRect rect;
    int   first = 1;
    int   last  = m_CutMessage.size() - 2;
    int   min_width = INT_MAX;

    /*
     * Test from each end to find the best width to use.
     * An interior line may actually represent the best width, but it
     * is not worth the extra complexity to test interior lines.
     */
    while (first < last && min_width > m_MinSize.x())
    {
        if ((first = m_CutMessage.indexOf(' ', first)) < 1)
            break;

        min_rect.setWidth(fm.width(m_CutMessage.left(first)));

        if (min_rect.width() < m_Area.width())
        {
            rect = fm.boundingRect(min_rect, m_Justification, m_CutMessage);
            if (rect.height() <= m_Area.height() && rect.width() < min_width)
                min_width = rect.width();
        }

        if ((last = m_CutMessage.lastIndexOf(' ', last)) < 0)
            break;

        min_rect.setWidth(fm.width(m_CutMessage.mid(last)));

        if (min_rect.width() < m_Area.width())
        {
            rect = fm.boundingRect(min_rect, m_Justification, m_CutMessage);
            if (rect.height() <= m_Area.height() && rect.width() < min_width)
                min_width = rect.width();
        }

        ++first;
        --last;
    }

    if (min_width == INT_MAX)
    {
        // won't fit, even when the ara is the maxium size
        min_rect = fm.boundingRect(m_Area, m_Justification, m_CutMessage);
        return false;
    }
    else
    {
        if (min_width < m_MinSize.x() - m_Area.x())
            min_width = m_MinSize.x() - m_Area.x();

        // Found the minimal width which will accommodate the whole message.
        min_rect.setHeight(m_Area.height());
        min_rect.setWidth(min_width);
        min_rect = fm.boundingRect(min_rect, m_Justification, m_CutMessage);
        return true;
    }
}

void MythUIText::FillCutMessage()
{
    m_CutMessage.clear();

    if (m_Message != m_DefaultMessage)
    {
        bool isNumber;
        int value = m_Message.toInt(&isNumber);
        if (isNumber && m_TemplateText.contains("%n"))
        {
            m_CutMessage = qApp->translate("ThemeUI",
                                           m_TemplateText.toUtf8(), NULL,
                                           QCoreApplication::UnicodeUTF8,
                                           qAbs(value));
        }
        else if (m_TemplateText.contains("%1"))
        {
            QString tmp = qApp->translate("ThemeUI", m_TemplateText.toUtf8(),
                                          NULL, QCoreApplication::UnicodeUTF8);
            m_CutMessage = tmp.arg(m_Message);
        }
    }

    if (m_CutMessage.isEmpty())
        m_CutMessage = m_Message;
    if (m_CutMessage.isEmpty())
        return;

    QStringList templist;
    QStringList::iterator it;
    switch (m_textCase)
    {
      case CaseUpper :
        m_CutMessage = m_CutMessage.toUpper();
        break;
      case CaseLower :
        m_CutMessage = m_CutMessage.toLower();
        break;
      case CaseCapitaliseFirst :
        //m_CutMessage = m_CutMessage.toLower();
        templist = m_CutMessage.split(". ");
        for (it = templist.begin(); it != templist.end(); ++it)
            (*it).replace(0,1,(*it).left(1).toUpper());
        m_CutMessage = templist.join(". ");
        break;
      case CaseCapitaliseAll :
        //m_CutMessage = m_CutMessage.toLower();
        templist = m_CutMessage.split(" ");
        for (it = templist.begin(); it != templist.end(); ++it)
            (*it).replace(0,1,(*it).left(1).toUpper());
        m_CutMessage = templist.join(" ");
        break;
    }

    if (m_MinSize.x() > 0)
    {
        QRect rect;
        MakeNarrow(rect);

        // Record the minimal area needed for the message.
        SetMinArea(rect.size());
        if (m_MinArea.width() > 0)
            SetDrawRectSize(m_MinArea.width(), m_MinArea.height());
    }

    if (m_Cutdown)
        m_CutMessage = cutDown(m_CutMessage, m_Font, m_MultiLine);
}

void MythUIText::Pulse(void)
{
    //
    //  Call out base class pulse which will handle any alpha cycling and
    //  movement
    //

    MythUIType::Pulse();

    if (m_colorCycling)
    {
        curR += incR;
        curG += incG;
        curB += incB;

        m_curStep++;
        if (m_curStep >= m_numSteps)
        {
            m_curStep = 0;
            incR *= -1;
            incG *= -1;
            incB *= -1;
        }

        QColor newColor = QColor((int)curR, (int)curG, (int)curB);
        if (newColor != m_Font->color())
        {
            m_Font->SetColor(newColor);
            SetRedraw();
        }
    }

    if (m_scrolling)
    {
        switch (m_scrollDirection)
        {
            case ScrollLeft :
                MoveDrawRect(-1, 0);
                if ((m_drawRect.x() + m_drawRect.width()) < 0)
                    SetDrawRectPosition(GetArea().width(),0);
                break;
            case ScrollRight :
                MoveDrawRect(1, 0);
                if (m_drawRect.x() > m_Area.width())
                    SetDrawRectPosition(-GetArea().width(),0);
                break;
            case ScrollUp :
                MoveDrawRect(0, -1);
                if (m_drawRect.y() > m_Area.height())
                    SetDrawRectPosition(0,-(GetArea().height()));
                break;
            case ScrollDown :
                MoveDrawRect(0, 1);
                if ((m_drawRect.y() + m_Area.height()) < 0)
                    SetDrawRectPosition(0,GetArea().height()-1);
                break;
        }
    }
}

void MythUIText::CycleColor(QColor startColor, QColor endColor, int numSteps)
{
    if (!GetMythPainter()->SupportsAnimation())
        return;

    m_startColor = startColor;
    m_endColor = endColor;
    m_numSteps = numSteps;
    m_curStep = 0;

    curR = startColor.red();
    curG = startColor.green();
    curB = startColor.blue();

    incR = (endColor.red() * 1.0 - curR) / m_numSteps;
    incG = (endColor.green() * 1.0 - curG) / m_numSteps;
    incB = (endColor.blue() * 1.0 - curB) / m_numSteps;

    m_colorCycling = true;
}

void MythUIText::StopCycling(void)
{
    if (!m_colorCycling)
        return;

    m_Font->SetColor(m_startColor);
    m_colorCycling = false;
    SetRedraw();
}

QString MythUIText::cutDown(const QString &data, MythFontProperties *font,
                            bool multiline)
{
    int length = data.length();
    if (length == 0)
        return data;

    int maxwidth = GetArea().width();
    int maxheight = GetArea().height();
    int justification = Qt::AlignLeft | Qt::TextWordWrap;
    QFontMetrics fm(font->face());

    int margin = length - 1;
    int index = 0;
    int diff = 0;

    while (margin > 0)
    {
        if (multiline)
            diff = maxheight - fm.boundingRect(0, 0, maxwidth, maxheight,
                                               justification,
                                               data.left(index + margin + 1)
                                               ).height();
        else
            diff = maxwidth - fm.width(data, index + margin + 1);
        if (diff >= 0)
            index += margin;

        margin /= 2;

        if (index + margin >= length - 1)
            margin = (length - 1) - index;
    }

    if (index < length - 1)
    {
        QString tmpStr(data);
        tmpStr.truncate(index);
        if (index >= 3)
            tmpStr.replace(index - 3, 3, "...");
        return tmpStr;
    }

    return data;

}

bool MythUIText::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_OrigDisplayRect = m_Area;
    }
//    else if (element.tagName() == "altarea") // Unused, but maybe in future?
//        m_AltDisplayRect = parseRect(element);
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);
        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);
        if (fp)
        {
            QString state = element.attribute("state","");
            if (!state.isEmpty())
            {
                m_FontStates.insert(state, *fp);
            }
            else
            {
                m_FontStates.insert("default", *fp);
                *m_Font = m_FontStates["default"];
            }
        }
    }
    else if (element.tagName() == "value")
    {
        if (element.attribute("lang","").isEmpty())
        {
            m_Message = qApp->translate("ThemeUI",
                                        parseText(element).toUtf8(), NULL,
                                        QCoreApplication::UnicodeUTF8);
        }
        else if (element.attribute("lang","").toLower() ==
                 gCoreContext->GetLanguageAndVariant())
        {
            m_Message = parseText(element);
        }
        else if (element.attribute("lang","").toLower() ==
                 gCoreContext->GetLanguage())
        {
            m_Message = parseText(element);
        }

        m_DefaultMessage = m_Message;
        SetText(m_Message);
    }
    else if (element.tagName() == "template")
    {
        m_TemplateText = parseText(element);
    }
    else if (element.tagName() == "cutdown")
    {
        SetCutDown(parseBool(element));
    }
    else if (element.tagName() == "multiline")
    {
        SetMultiLine(parseBool(element));
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();
        SetJustification(parseAlignment(align));
    }
    else if (element.tagName() == "colorcycle")
    {
        if (GetMythPainter()->SupportsAnimation())
        {
            QString tmp = element.attribute("start");
            if (!tmp.isEmpty())
                m_startColor = QColor(tmp);
            tmp = element.attribute("end");
            if (!tmp.isEmpty())
                m_endColor = QColor(tmp);
            tmp = element.attribute("steps");
            if (!tmp.isEmpty())
                m_numSteps = tmp.toInt();

            // initialize the rest of the stuff
            CycleColor(m_startColor, m_endColor, m_numSteps);
        }
        else
            m_colorCycling = false;

        m_colorCycling = parseBool(element.attribute("disable"));
    }
    else if (element.tagName() == "scroll")
    {
        if (GetMythPainter()->SupportsAnimation())
        {
            QString tmp = element.attribute("direction");
            if (!tmp.isEmpty())
            {
                tmp = tmp.toLower();
                if (tmp == "left")
                    m_scrollDirection = ScrollLeft;
                else if (tmp == "right")
                    m_scrollDirection = ScrollRight;
                else if (tmp == "up")
                    m_scrollDirection = ScrollUp;
                else if (tmp == "down")
                    m_scrollDirection = ScrollDown;
            }

            m_scrolling = true;
        }
        else
            m_scrolling = false;
    }
    else if (element.tagName() == "case")
    {
        QString stringCase = getFirstText(element).toLower();
        if (stringCase == "lower")
            m_textCase = CaseLower;
        else if (stringCase == "upper")
            m_textCase = CaseUpper;
        else if (stringCase == "capitalisefirst")
            m_textCase = CaseCapitaliseFirst;
        else  if (stringCase == "capitaliseall")
            m_textCase = CaseCapitaliseAll;
        else
            m_textCase = CaseNormal;
        FillCutMessage();
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIText::CopyFrom(MythUIType *base)
{
    MythUIText *text = dynamic_cast<MythUIText *>(base);
    if (!text)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_Justification = text->m_Justification;
    m_OrigDisplayRect = text->m_OrigDisplayRect;
    m_AltDisplayRect = text->m_AltDisplayRect;
    m_drawRect = text->m_drawRect;

    m_DefaultMessage = text->m_DefaultMessage;
    SetText(text->m_Message);
    m_CutMessage = text->m_CutMessage;
    m_TemplateText = text->m_TemplateText;

    m_Cutdown = text->m_Cutdown;
    m_MultiLine = text->m_MultiLine;

    QMutableMapIterator<QString, MythFontProperties> it(text->m_FontStates);
    while (it.hasNext())
    {
        it.next();
        m_FontStates.insert(it.key(), it.value());
    }

    *m_Font = m_FontStates["default"];

    m_colorCycling = text->m_colorCycling;
    m_startColor = text->m_startColor;
    m_endColor = text->m_endColor;
    m_numSteps = text->m_numSteps;
    m_curStep = text->m_curStep;
    curR = text->curR;
    curG = text->curG;
    curB = text->curB;
    incR = text->incR;
    incG = text->incG;
    incB = text->incB;

    m_scrolling = text->m_scrolling;
    m_scrollDirection = text->m_scrollDirection;

    m_textCase = text->m_textCase;

    MythUIType::CopyFrom(base);
}

void MythUIText::CreateCopy(MythUIType *parent)
{
    MythUIText *text = new MythUIText(parent, objectName());
    text->CopyFrom(this);
}

void MythUIText::Finalize(void)
{
    FillCutMessage();
}

