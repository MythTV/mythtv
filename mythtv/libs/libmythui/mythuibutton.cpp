#include <iostream>
using namespace std;

#include "mythuibutton.h"
#include "mythmainwindow.h"

MythUIButton::MythUIButton(MythUIType *parent, const QString &name, bool doInit)
            : MythUIType(parent, name)
{
    m_State = None;
    m_CheckState = MythUIStateType::None;

    m_PaddingMarginX = m_PaddingMarginY = 0;

    m_textFlags = Qt::AlignLeft | Qt::AlignVCenter;
    m_imageAlign = Qt::AlignLeft | Qt::AlignVCenter;

    if (doInit)
        Init();

    connect(this, SIGNAL(TakingFocus()), this, SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), this, SLOT(Deselect()));

    SetCanTakeFocus(true);
}

MythUIButton::~MythUIButton()
{
}

void MythUIButton::Init()
{
    m_BackgroundImage = new MythUIStateType(this, "buttonback");
    m_Text = new MythUIText(this, "buttontext");
    m_ButtonImage = new MythUIImage(this, "buttonimage");
    m_CheckImage = new MythUIStateType(this, "buttoncheck");
    m_ArrowImage = new MythUIImage(this, "arrowimage");

    m_CheckImage->SetVisible(false);
    m_ButtonImage->SetVisible(false);
    m_ArrowImage->SetVisible(false);

}

bool MythUIButton::ParseElement(QDomElement &element)
{
    if (element.tagName() == "inactivefont")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);
        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);
        if (fp)
            SetFont(Disabled, *fp);
    }
    else if (element.tagName() == "activefont")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);
        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);
        if (fp)
            SetFont(Normal, *fp);
    }
    else if (element.tagName() == "value")
    {
        QString msg = getFirstText(element);
        m_Text->SetText(msg);
    }
    else if (element.tagName() == "showarrow")
    {
        EnableRightArrow(parseBool(element));
    }
    else if (element.tagName() == "arrow")
    {
        MythImage *tmp = LoadImage(element);
        SetRightArrowImage(tmp);
    }
    else if (element.tagName() == "check-empty")
    {
        MythImage *tmp= LoadImage(element);
        SetCheckImage(MythUIStateType::Off, tmp);
    }
    else if (element.tagName() == "check-half")
    {
        MythImage *tmp= LoadImage(element);
        SetCheckImage(MythUIStateType::Half, tmp);
    }
    else if (element.tagName() == "check-full")
    {
        MythImage *tmp= LoadImage(element);
        SetCheckImage(MythUIStateType::Full, tmp);
    }
    else if (element.tagName() == "regular-background")
    {
        MythImage *tmp= LoadImage(element);
        SetBackgroundImage(Normal, tmp);
        SetBackgroundImage(Active, tmp);
        SelectState(Normal);
    }
    else if (element.tagName() == "selected-background")
    {
        MythImage *tmp= LoadImage(element);
        SetBackgroundImage(Selected, tmp);
    }
    else if (element.tagName() == "inactive-background")
    {
        MythImage *tmp= LoadImage(element);
        SetBackgroundImage(SelectedInactive,
                                tmp);
    }
    else if (element.tagName() == "margin")
    {
        QString paddingMargin = getFirstText(element);
        int paddingMarginY = 0;
        int paddingMarginX = 0;

        paddingMarginX = NormX(paddingMargin.section(',',0,0).toInt());
        if (!paddingMargin.section(',',1,1).isEmpty())
            paddingMarginY = NormY(paddingMargin.section(',',1,1).toInt());
        SetPaddingMargin(paddingMarginX,paddingMarginY);
    }
    else if (element.tagName() == "multiline")
    {
        if (parseBool(element))
            m_textFlags |= Qt::TextWordWrap;
        else
            m_textFlags &= ~Qt::TextWordWrap;

        m_Text->SetJustification(m_textFlags);
    }
    else if (element.tagName() == "textflags" || element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();

        m_textFlags = m_textFlags & Qt::TextWordWrap;

        m_textFlags |= parseAlignment(align);

        m_Text->SetJustification(m_textFlags);
    }
    else if (element.tagName() == "imagealign")
    {
        m_imageAlign = parseAlignment(element);
    }
    else
        return MythUIType::ParseElement(element);

    return true;
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUIButton::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        emit buttonPressed();
    }
}

MythImage* MythUIButton::LoadImage(QDomElement element)
{
    MythImage *tmp;
    QString filename = element.attribute("filename");

    if (!filename.isEmpty())
    {
        tmp = GetMythPainter()->GetFormatImage();
        tmp->Load(filename);
    }
    else
    {
        QColor startcol = QColor(element.attribute("gradientstart", "#505050"));
        QColor endcol = QColor(element.attribute("gradientend", "#000000"));
        int alpha = element.attribute("gradientalpha", "100").toInt();

        tmp = MythImage::Gradient(QSize(10, 10), startcol, endcol, alpha);
    }

    return tmp;
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

    if (m_Area.isValid())
    {
        QSize aSize = m_Area.size();
        image->Resize(aSize);
    }

    m_BackgroundImage->AddImage(num, image);

    SetupPlacement();
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

void MythUIButton::SetPaddingMargin(int marginx, int marginy)
{
    m_PaddingMarginX = marginx;
    m_PaddingMarginY = marginy;
}

void MythUIButton::SetImageAlignment(int imagealign)
{
    m_imageAlign = imagealign;
}

void MythUIButton::SetText(const QString &msg, int textFlags)
{
    m_Text->SetText(msg);
    if (textFlags > 0)
        m_Text->SetJustification(textFlags);
    else if (m_textFlags > 0)
        m_Text->SetJustification(m_textFlags);
    SetRedraw();
}

void MythUIButton::SelectState(StateType newState)
{
    if (m_State == newState)
        return;

    m_State = newState;

    if (!m_BackgroundImage->DisplayState(QString::number((int)m_State)))
        m_BackgroundImage->DisplayState(QString::number((int)Normal));

    if (!m_FontProps.contains((int)m_State))
        m_Text->SetFontProperties(m_FontProps[(int)Normal]);
    else
        m_Text->SetFontProperties(m_FontProps[(int)m_State]);

    SetRedraw();
}

void MythUIButton::SetCheckState(MythUIStateType::StateType state)
{
    m_CheckState = state;
    m_CheckImage->DisplayState(state);
}

MythUIStateType::StateType MythUIButton::GetCheckState()
{
    return m_CheckState;
}

void MythUIButton::EnableCheck(bool enable)
{
    if (m_CheckState == MythUIStateType::None)
        m_CheckState = MythUIStateType::Off;
    m_CheckImage->DisplayState(m_CheckState);
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

    int textx = m_PaddingMarginX;
    int imagex = m_PaddingMarginX;
    int imagey = m_PaddingMarginY;
    int textwidth = width - 2 * m_PaddingMarginX;

    if (checkRect != QRect())
    {
        int tmp = checkRect.width() + m_PaddingMarginX;
        textx += tmp;
        imagex += tmp;
        textwidth -= tmp;

        m_CheckImage->SetPosition(m_PaddingMarginX,
                                  (height - checkRect.height()) / 2);
    }

    if (arrowRect != QRect())
    {
        textwidth -= arrowRect.width() + m_PaddingMarginX;
        m_ArrowImage->SetPosition(width - arrowRect.width() - m_PaddingMarginX,
                                  (height - arrowRect.height()) / 2);
    }

    if (imageRect != QRect())
    {
        int tmp = imageRect.width() + m_PaddingMarginX;
        textx += tmp;
        textwidth -= tmp;

        // Horizontal component
        if (m_imageAlign & Qt::AlignLeft)
        {
            // Default
        }
        else if (m_imageAlign & Qt::AlignCenter)
        {
            imagex = (width - imageRect.width()) / 2;
        }
        else if (m_imageAlign & Qt::AlignRight)
        {
            imagex = width - m_PaddingMarginX - imageRect.width();
        }

        // Vertical component
        if (m_imageAlign & Qt::AlignTop)
        {
            imagey = m_PaddingMarginY;
        }
        else if (m_imageAlign & Qt::AlignCenter)
        {
            imagey = (height - imageRect.height()) / 2;
        }
        else if (m_imageAlign & Qt::AlignBottom)
        {
            imagey = height - m_PaddingMarginY - imageRect.height();
        }

        m_ButtonImage->SetPosition(imagex, imagey);
    }

    SetTextRect(QRect(textx, 0, textwidth, height));
}

void MythUIButton::CopyFrom(MythUIType *base)
{
    MythUIButton *button = dynamic_cast<MythUIButton *>(base);
    if (!button)
    {
        cerr << "ERROR, bad parsing" << endl;
        return;
    }

    m_FontProps = button->m_FontProps;

    m_TextRect = button->m_TextRect;
    m_PaddingMarginX = button->m_PaddingMarginX;
    m_PaddingMarginY = button->m_PaddingMarginY;
    m_textFlags = button->m_textFlags;
    m_imageAlign = button->m_imageAlign;

    MythUIType::CopyFrom(base);

    m_BackgroundImage = dynamic_cast<MythUIStateType *>
                    (GetChild("buttonback"));
    m_CheckImage = dynamic_cast<MythUIStateType *>
                    (GetChild("buttoncheck"));
    m_Text = dynamic_cast<MythUIText *>
                    (GetChild("buttontext"));
    m_ButtonImage = dynamic_cast<MythUIImage *>
                    (GetChild("buttonimage"));
    m_ArrowImage = dynamic_cast<MythUIImage *>
                    (GetChild("arrowimage"));

    m_CheckImage->SetVisible(false);
    m_ButtonImage->SetVisible(false);
    m_ArrowImage->SetVisible(false);

    SetupPlacement();
    SelectState(button->m_State);
}

void MythUIButton::CreateCopy(MythUIType *parent)
{
    MythUIButton *button = new MythUIButton(parent, objectName(), false);
    button->CopyFrom(this);
}

bool MythUIButton::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            emit buttonPressed();
        }
        else
            handled = false;
    }

    return handled;
}
