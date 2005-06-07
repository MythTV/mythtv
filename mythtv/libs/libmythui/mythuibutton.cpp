#include <iostream>
using namespace std;

#include "mythuibutton.h"

MythUIButton::MythUIButton(MythUIType *parent, const char *name)
            : MythUIType(parent, name)
{
    m_State = None;
    m_BackgroundImage = new MythUIStateType(this, "buttonback");
    m_CheckImage = new MythUIStateType(this, "buttoncheck");
    m_Text = new MythUIText(this, "buttontext");
    m_ButtonImage = new MythUIImage(this, "buttonimage");
    m_ArrowImage = new MythUIImage(this, "arrowimage");

    m_CheckImage->SetVisible(false);
    m_ButtonImage->SetVisible(false);
    m_ArrowImage->SetVisible(false);

    m_PaddingMargin = 0;

    SetCanTakeFocus(true);
}

MythUIButton::~MythUIButton()
{
}

void MythUIButton::SetTextRect(const QRect &textRect)
{
    if (textRect == m_TextRect)
        return;

    m_TextRect = textRect;
    m_Text->SetArea(textRect);
}

void MythUIButton::SetBackgroundImage(StateType state, MythImage *image)
{
    QString num = QString::number((int)state);
    m_BackgroundImage->AddImage(num, image);

    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(image->size());
    m_Area.setSize(aSize);
}

void MythUIButton::SetCheckImage(MythUIStateType::StateType state, 
                                 MythImage *image)
{
    m_CheckImage->AddImage(state, image);
}

void MythUIButton::SetFont(StateType state, MythFontProperties &prop)
{
    m_FontProps[(int)state] = prop;
    m_Text->SetFontProperties(m_FontProps[(int)Normal]);
}

void MythUIButton::SetButtonImage(MythImage *image)
{
    if (image)
    {
        m_ButtonImage->SetImage(image);
        m_ButtonImage->SetVisible(true);
    }
    else
        m_ButtonImage->SetVisible(false);
    SetupPlacement();
}

void MythUIButton::SetRightArrowImage(MythImage *image)
{
    if (image)
    {
        m_ArrowImage->SetImage(image);
        m_ArrowImage->SetVisible(true);
    }
    else
        m_ArrowImage->SetVisible(false);
    SetupPlacement();
}

void MythUIButton::SetPaddingMargin(int margin)
{
    m_PaddingMargin = margin;
}

void MythUIButton::SetText(const QString &msg, int textFlags)
{
    m_Text->SetText(msg);
    if (textFlags > 0)
        m_Text->SetJustification(textFlags);
}

void MythUIButton::SelectState(StateType newState)
{
    if (m_State == newState)
        return;

    m_State = newState;

    if (!m_BackgroundImage->DisplayState(QString::number((int)m_State)))
        m_BackgroundImage->DisplayState(QString::number((int)Normal));

/*
    if (!m_FontProps.contains((int)m_State))
        m_Text->SetFontProperties(m_FontProps[(int)Normal]);
    else
        m_Text->SetFontProperties(m_FontProps[(int)m_State]);
*/
}

void MythUIButton::SetCheckState(MythUIStateType::StateType state)
{
    m_CheckImage->DisplayState(state);
}

void MythUIButton::EnableCheck(bool enable)
{
    m_CheckImage->SetVisible(enable);
    SetupPlacement();
}

void MythUIButton::EnableRightArrow(bool enable)
{
    m_ArrowImage->SetVisible(enable);
    SetupPlacement();
}

void MythUIButton::SetupPlacement(void)
{
    int width = m_Area.width();
    int height = m_Area.height();

    QRect arrowRect = QRect(), checkRect = QRect(), imageRect = QRect();
    arrowRect = m_ArrowImage->GetArea();
    if (m_CheckImage->IsVisible())
        checkRect = m_CheckImage->GetArea();
    if (m_ButtonImage->IsVisible())
        imageRect = m_ButtonImage->GetArea();

    int textx = m_PaddingMargin;
    int imagex = m_PaddingMargin;
    int textwidth = width - 2 * m_PaddingMargin;

    if (checkRect != QRect())
    {
        int tmp = checkRect.width() + m_PaddingMargin;
        textx += tmp;
        imagex += tmp;
        textwidth -= tmp;
        
        m_CheckImage->SetPosition(m_PaddingMargin, 
                                  (height - checkRect.height()) / 2);
    }

    if (arrowRect != QRect())
    {
        textwidth -= arrowRect.width() + m_PaddingMargin;
        m_ArrowImage->SetPosition(width - arrowRect.width() - m_PaddingMargin,
                                  (height - arrowRect.height()) / 2);
    }

    if (imageRect != QRect())
    {
        int tmp = imageRect.width() + m_PaddingMargin;
        textx += tmp;
        textwidth -= tmp;
        m_ButtonImage->SetPosition(imagex,
                                   (height - imageRect.height()) / 2);
    }

    SetTextRect(QRect(textx, 0, textwidth, height));
}

