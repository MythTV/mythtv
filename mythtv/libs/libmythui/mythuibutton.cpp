#include <iostream>
using namespace std;

#include "mythuibutton.h"
#include "mythmainwindow.h"

MythUIButton::MythUIButton(MythUIType *parent, const char *name, bool doInit)
            : MythUIType(parent, name)
{
    m_State = None;

    m_PaddingMargin = 0;

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
    m_CheckImage = new MythUIStateType(this, "buttoncheck");
    m_Text = new MythUIText(this, "buttontext");
    m_ButtonImage = new MythUIImage(this, "buttonimage");
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
        int paddingMargin = NormX(getFirstText(element).toInt());
        SetPaddingMargin(paddingMargin);
    }
    else if (element.tagName() == "multiline")
    {
        if (parseBool(element))
            m_textFlags |= Qt::WordBreak;
        else
            m_textFlags &= ~Qt::WordBreak;

        m_Text->SetJustification(m_textFlags);
    }
    else if (element.tagName() == "textflags")
    {
        QString align = getFirstText(element).lower();

        m_textFlags = m_textFlags & Qt::WordBreak;

        if (align == "center")
            m_textFlags |= Qt::AlignCenter;
        else if (align == "right")
            m_textFlags |= Qt::AlignRight;
        else if (align == "left")
            m_textFlags |= Qt::AlignLeft;
        else if (align == "allcenter")
            m_textFlags |= Qt::AlignHCenter | Qt::AlignVCenter;
        else if (align == "vcenter")
            m_textFlags |= Qt::AlignVCenter;
        else if (align == "hcenter")
            m_textFlags |= Qt::AlignHCenter;

        m_Text->SetJustification(m_textFlags);
    }
    else
        return MythUIType::ParseElement(element);

    return true;
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
    m_BackgroundImage->AddImage(num, image);

    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(image->size());
    m_Area.setSize(aSize);

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

void MythUIButton::SetPaddingMargin(int margin)
{
    m_PaddingMargin = margin;
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
    m_PaddingMargin = button->m_PaddingMargin;
    m_textFlags = button->m_textFlags;

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
    MythUIButton *button = new MythUIButton(parent, name(), false);
    button->CopyFrom(this);
}

bool MythUIButton::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
