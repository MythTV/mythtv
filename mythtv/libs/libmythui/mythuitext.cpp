#include <iostream>
#include <QApplication>

#include "mythverbose.h"

#include "mythuihelper.h"
#include "mythuitext.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "compat.h"

MythUIText::MythUIText(MythUIType *parent, const QString &name)
          : MythUIType(parent, name)
{
    m_Message = m_DefaultMessage = "";

    m_Font = new MythFontProperties();

    m_OrigDisplayRect = m_AltDisplayRect = m_Area = m_drawRect = MythRect();

    m_Cutdown = true;
    m_CutMessage = "";

    m_Justification = (Qt::AlignLeft | Qt::AlignTop);

    m_colorCycling = false;
}

MythUIText::MythUIText(const QString &text, const MythFontProperties &font,
                       QRect displayRect, QRect altDisplayRect,
                       MythUIType *parent, const QString &name)
          : MythUIType(parent, name)
{
    m_Message = text;
    m_DefaultMessage = text;

    m_Font = new MythFontProperties();
    *m_Font = font;

    m_OrigDisplayRect = m_Area = m_drawRect = displayRect;
    m_AltDisplayRect = altDisplayRect;

    m_Cutdown = true;
    m_CutMessage = "";
    m_Justification = (Qt::AlignLeft | Qt::AlignTop);

    m_colorCycling = false;
}

MythUIText::~MythUIText()
{
    if (m_Font)
    {
        delete m_Font;
        m_Font = NULL;
    }
}

void MythUIText::Reset()
{
    m_Message = m_DefaultMessage;
    m_Area = m_drawRect = m_OrigDisplayRect;
    MythUIType::Reset();
}

void MythUIText::SetText(const QString &text)
{
    if (text == m_Message)
        return;

    m_Message = text;
    m_CutMessage = "";
    SetRedraw();
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
    if (m_Font)
        *m_Font = fontProps;
    SetRedraw();
}

void MythUIText::UseAlternateArea(bool useAlt)
{
    m_CutMessage = "";

    if (useAlt && m_AltDisplayRect.width() > 1)
        MythUIType::SetArea(m_AltDisplayRect);
    else
        MythUIType::SetArea(m_OrigDisplayRect);
}

void MythUIText::SetJustification(int just)
{
    if (m_Justification != just)
    {
        m_Justification = just;
        m_CutMessage = "";
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
    m_CutMessage = "";
    SetRedraw();
}

void MythUIText::SetArea(const MythRect &rect)
{
    MythUIType::SetArea(rect);
    m_CutMessage = "";
    m_drawRect = m_Area;
}

void MythUIText::SetPosition(const MythPoint &pos)
{
    MythUIType::SetPosition(pos);
    m_drawRect.moveTopLeft(m_Area.topLeft());
}

void MythUIText::SetStartPosition(const int x, const int y)
{
    int startx = m_Area.x() + x;
    int starty = m_Area.y() + y;
    m_drawRect.setTopLeft(QPoint(startx,starty));
    SetRedraw();
}

void MythUIText::MoveStartPosition(const int x, const int y)
{
    int newx = m_drawRect.x() + x;
    int newy = m_drawRect.y() + y;
    m_drawRect.setTopLeft(QPoint(newx,newy));
    SetRedraw();
}

void MythUIText::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect)
{
    QRect area = m_Area.toQRect();
    area.translate(xoffset, yoffset);
    QRect drawrect = m_drawRect.toQRect();
    drawrect.translate(xoffset, yoffset);

    int alpha = CalcAlpha(alphaMod);

    if (m_CutMessage == "")
    {
        bool multiline = (m_Justification & Qt::TextWordWrap);

        if (m_Cutdown)
        {
            QFont font = m_Font->face();
            m_CutMessage = cutDown(m_Message, &font, multiline);
        }
        else
            m_CutMessage = m_Message;
    }

    p->DrawText(drawrect, m_CutMessage, m_Justification, *m_Font, alpha, area);
}

void MythUIText::Pulse(void)
{
    //
    //  Call out base class pulse which will handle any alpha cycling and
    //  movement
    //

    MythUIType::Pulse();

    if (!m_colorCycling)
        return;

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

bool MythUIText::ParseElement(QDomElement &element)
{
    if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_OrigDisplayRect = m_Area;
    }
    else if (element.tagName() == "altarea")
        m_AltDisplayRect = parseRect(element);
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);
        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);
        if (fp)
            *m_Font = *fp;
    }
    else if (element.tagName() == "value")
    {
        if (element.attribute("lang","").isEmpty())
        {
            m_Message = qApp->translate("ThemeUI", qPrintable(getFirstText(element)));
        }
        else if (element.attribute("lang","").toLower() ==
                 GetMythUI()->GetLanguageAndVariant())
        {
            m_Message = getFirstText(element);
        }
        else if (element.attribute("lang","").toLower() ==
                 GetMythUI()->GetLanguage())
        {
            m_Message = getFirstText(element);
        }
    }
    else if (element.tagName() == "cutdown")
    {
        m_Cutdown = parseBool(element);
    }
    else if (element.tagName() == "multiline")
    {
        if (parseBool(element))
            m_Justification |= Qt::TextWordWrap;
        else
            m_Justification &= ~Qt::TextWordWrap;
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();

        // preserve the wordbreak attribute, drop everything else
        m_Justification = m_Justification & Qt::TextWordWrap;

        m_Justification |= parseAlignment(align);
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

        if (!element.attribute("disable").isEmpty())
            m_colorCycling = false;
    }
    else
        return MythUIType::ParseElement(element);

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

    m_Message = text->m_Message;
    m_CutMessage = text->m_CutMessage;
    m_DefaultMessage = text->m_DefaultMessage;

    m_Cutdown = text->m_Cutdown;

    *m_Font = *(text->m_Font);

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

    MythUIType::CopyFrom(base);
}

void MythUIText::CreateCopy(MythUIType *parent)
{
    MythUIText *text = new MythUIText(parent, objectName());
    text->CopyFrom(this);
}

void MythUIText::Finalize(void)
{
    m_CutMessage = "";
}

