#include <qapplication.h>

#include "mythuitext.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "mythcontext.h"

MythUIText::MythUIText(MythUIType *parent, const char *name)
          : MythUIType(parent, name)
{
    m_Message = m_DefaultMessage = " ";

    m_Font = new MythFontProperties();

    m_OrigDisplayRect = m_AltDisplayRect = m_Area = QRect();

    m_Cutdown = true;
    m_CutMessage = "";

    m_Justification = (Qt::AlignLeft | Qt::AlignTop);

    m_colorCycling = false;
}

MythUIText::MythUIText(const QString &text, const MythFontProperties &font,
                       QRect displayRect, QRect altDisplayRect,
                       MythUIType *parent, const char *name)
          : MythUIType(parent, name)
{
    m_Message = text;
    m_DefaultMessage = text;

    m_Font = new MythFontProperties();
    *m_Font = font;

    m_OrigDisplayRect = m_Area = displayRect;
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

void MythUIText::SetText(const QString &text)
{
    if (text == m_Message)
        return;

    m_Message = text;
    m_CutMessage = "";
    SetRedraw();
}

QString MythUIText::GetText(void)
{
    return m_Message;
}

QString MythUIText::GetDefaultText(void)
{
    return m_DefaultMessage;
}

void MythUIText::SetFontProperties(const MythFontProperties &fontProps)
{
    *m_Font = fontProps;
    SetRedraw();
}

void MythUIText::UseAlternateArea(bool useAlt)
{
    if (useAlt && m_AltDisplayRect.width() > 1)
        m_Area = m_AltDisplayRect;
    else
        m_Area = m_OrigDisplayRect;

    m_CutMessage = "";
    SetRedraw();
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

void MythUIText::SetArea(const QRect &rect)
{
    m_CutMessage = "";
    MythUIType::SetArea(rect);
}

void MythUIText::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                          int alphaMod, QRect clipRect)
{
    QRect area = m_Area;
    area.moveBy(xoffset, yoffset);

    int alpha = CalcAlpha(alphaMod);

    if (m_CutMessage == "")
    {
        bool multiline = (m_Justification & Qt::WordBreak);

        if (m_Cutdown)
        {
            QFont font = m_Font->face();
            m_CutMessage = cutDown(m_Message, &font, multiline);
        }
        else
            m_CutMessage = m_Message;
    }

    p->DrawText(area, m_CutMessage, m_Justification, *m_Font, alpha);
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
        SetArea(parseRect(element));
    else if (element.tagName() == "altarea")
        m_AltDisplayRect = parseRect(element);
    else if (element.tagName() == "font")
    {
        // add local fonts to each uitype
    }
    else if (element.tagName() == "value")
    {
        if ((m_Message.isNull() || m_Message.isEmpty()) &&
            element.attribute("lang","") == "")
        {
            m_Message = qApp->translate("ThemeUI", getFirstText(element));
        }
        else if (element.attribute("lang","").lower() ==
                 gContext->GetLanguageAndVariant())
        {
            m_Message = getFirstText(element);
        }
        else if (element.attribute("lang","").lower() ==
                 gContext->GetLanguage())
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
            m_Justification |= Qt::WordBreak;
        else
            m_Justification &= ~Qt::WordBreak;
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).lower();

        // preserve the wordbreak attribute, drop everything else
        m_Justification = m_Justification & Qt::WordBreak;

        if (align == "center")
            m_Justification |= Qt::AlignCenter;
        else if (align == "right")
            m_Justification |= Qt::AlignRight;
        else if (align == "left")
            m_Justification |= Qt::AlignLeft;
        else if (align == "allcenter")
            m_Justification |= Qt::AlignHCenter | Qt::AlignVCenter;
        else if (align == "vcenter")
            m_Justification |= Qt::AlignVCenter;
        else if (align == "hcenter")
            m_Justification |= Qt::AlignHCenter;
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
    m_AltDisplayRect = m_AltDisplayRect;

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
}

void MythUIText::CreateCopy(MythUIType *parent)
{
    MythUIText *text = new MythUIText(parent, name());
    text->CopyFrom(this);
}

void MythUIText::Finalize(void)
{
    m_CutMessage = "";
}

