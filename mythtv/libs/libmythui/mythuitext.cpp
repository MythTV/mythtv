#include "mythuitext.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

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

    setDebugColor(QColor(255,255,255));
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
    
    setDebugColor(QColor(255,255,255));
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

MythUIText::~MythUIText()
{
    if (m_Font)
    {
        delete m_Font;
        m_Font = NULL;
    }
}
