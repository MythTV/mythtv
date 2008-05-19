#include <qapplication.h>
#include <qregexp.h>
#include <QChar>

#include "mythuitextedit.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "mythcontext.h"

MythUITextEdit::MythUITextEdit(MythUIType *parent, const char *name, bool doInit)
           : MythUIType(parent, name)
{
    m_Message = "";
    m_Filter = FilterNone;

    m_Area = QRect();

    m_Justification = (Qt::AlignLeft | Qt::AlignTop);

    m_showCursor = false;
    m_blinkInterval = 0;
    m_cursorBlinkRate = 40;

    m_Position = -1;

    m_PaddingMargin = 0;
    m_maxLength = 0;

    if (doInit)
        Init();

    m_CanHaveFocus = true;
}

MythUITextEdit::~MythUITextEdit()
{
    if (m_backgroundImage)
        delete m_backgroundImage;
    if (m_cursorImage)
        delete m_cursorImage;
    if (m_Text)
        delete m_Text;
}

void MythUITextEdit::Init(void)
{
    m_backgroundImage = new MythUIImage(this, "backgroundimage");
    m_Text = new MythUIText(this, "textarea");
    m_cursorImage = new MythUIImage(this, "cursorimage");
    m_Text->SetCutDown(false);
}

void MythUITextEdit::Pulse(void)
{
    if (m_showCursor && m_HasFocus)
    {
        if (m_blinkInterval > m_cursorBlinkRate)
        {
            m_blinkInterval = 0;
            if (m_cursorImage->IsVisible())
            {
                m_cursorImage->SetVisible(false);
            }
            else
            {
                m_cursorImage->SetVisible(true);
            }
        }

        m_blinkInterval++;
    }
    else
        m_cursorImage->SetVisible(false);

    MythUIType::Pulse();
}

bool MythUITextEdit::ParseElement(QDomElement &element)
{
    bool parsed = true;

    if (element.tagName() == "area")
    {
        m_Area = parseRect(element);
        SetTextRect(m_Area);
    }
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);
        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);
        if (fp)
            m_Text->SetFontProperties(*fp);
    }
    else if (element.tagName() == "value")
    {
        if (element.attribute("lang","").isEmpty())
        {
            m_Message = qApp->translate("ThemeUI", getFirstText(element));
        }
        else if (element.attribute("lang","").toLower() ==
                 gContext->GetLanguageAndVariant())
        {
            m_Message = getFirstText(element);
        }
        else if (element.attribute("lang","").toLower() ==
                 gContext->GetLanguage())
        {
            m_Message = getFirstText(element);
        }
    }
    else if (element.tagName() == "multiline")
    {
        if (parseBool(element))
            m_Justification |= Qt::WordBreak;
        else
            m_Justification &= ~Qt::WordBreak;

        m_Text->SetJustification(m_Justification);
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();

        // preserve the wordbreak attribute, drop everything else
        m_Justification = m_Justification & Qt::WordBreak;

        m_Justification |= parseAlignment(align);

        m_Text->SetJustification(m_Justification);
    }
    else if (element.tagName() == "maxlength")
    {
        QString maxlength = getFirstText(element);
        SetMaxLength(maxlength.toInt());
    }
    else if (element.tagName() == "margin")
    {
        int paddingMargin = NormX(getFirstText(element).toInt());
        SetPaddingMargin(paddingMargin);
    }
    else if (element.tagName() == "cursor")
    {
        MythImage *tmp;
        QString filename = element.attribute("filename");

        if (!filename.isEmpty())
        {
            tmp = GetMythPainter()->GetFormatImage();
            tmp->Load(filename);
            SetCursorImage(tmp);
        }
    }
    else if (element.tagName() == "background")
    {
        MythImage *tmp;
        QString filename = element.attribute("filename");

        tmp = GetMythPainter()->GetFormatImage();

        if (filename.isEmpty() || !tmp->Load(filename))
        {
            QColor startcol = QColor("#EEEEEE");
            QColor endcol = QColor("#AEAEAE");
            int alpha = 255;
            tmp = MythImage::Gradient(QSize(10,10), startcol,
                            endcol, alpha);
        }

        SetBackgroundImage(tmp);
    }
    else
        parsed = false;

    return parsed;
}

void MythUITextEdit::SetCursorImage(MythImage *image)
{
    if (image && !image->isNull())
    {
        QFontMetrics fm(m_Text->GetFontProperties()->face());
        int height = fm.height();
        if (height > 0)
        {
            int width = int(((float)height / (float)image->height())
                            * (float)image->width());
            image->Resize(QSize(width,height));
        }
        m_cursorImage->SetImage(image);

        m_showCursor = true;
        SetTextRect();
    }
}

void MythUITextEdit::SetBackgroundImage(MythImage *image)
{
    if (image)
    {
        image->Resize(QSize(m_Area.width(), m_Area.height()));
        m_backgroundImage->SetPosition(0,0);
        m_backgroundImage->SetImage(image);
    }
}

void MythUITextEdit::SetMaxLength(const int length)
{
    m_maxLength = length;
}

void MythUITextEdit::SetPaddingMargin(const int margin)
{
    m_PaddingMargin = margin;

    SetTextRect();
    m_cursorImage->SetPosition(margin,margin);
}

void MythUITextEdit::SetTextRect(const QRect area)
{
    QRect textrect;

    if (area.isNull() || area.isEmpty())
        textrect = m_Area;
    else
        textrect = area;

    textrect = QRect( m_PaddingMargin,
                      m_PaddingMargin,
                      textrect.width() - m_PaddingMargin * 2,
                      textrect.height() - m_PaddingMargin * 2);

    if (m_cursorImage)
        textrect.setWidth(textrect.width() - m_cursorImage->GetArea().width());

    if (textrect.isValid())
        m_Text->SetArea(textrect);
}

void MythUITextEdit::SetText(const QString text, bool moveCursor)
{
    m_Message = text;
    m_Text->SetText(m_Message);
    if (moveCursor)
        MoveCursor(MoveEnd);
}

bool MythUITextEdit::InsertCharacter(const QString character)
{
    if (m_maxLength != 0 && m_Message.length() == m_maxLength)
        return true;

    QString newmessage = m_Message;

    const QChar *unichar = character.unicode();
    if (!(unichar->isLetterOrNumber() || unichar->isPunct()
        || unichar->isSpace() || unichar->isSymbol()))
        return false;

    if ((m_Filter & FilterAlpha) & unichar->isLetter())
        return false;
    if ((m_Filter & FilterNumeric) & unichar->isNumber())
        return false;
    if ((m_Filter & FilterSymbols) & unichar->isSymbol())
        return false;
    if ((m_Filter & FilterPunct) & unichar->isPunct())
        return false;

    newmessage.insert(m_Position+1, character);
    SetText(newmessage, false);
    MoveCursor(MoveRight);

    return true;
}

void MythUITextEdit::RemoveCharacter()
{
    if (m_Message.isEmpty())
        return;

    QString newmessage = m_Message;

    newmessage.remove(m_Position, 1);
    MoveCursor(MoveLeft);
    SetText(newmessage, false);
}

bool MythUITextEdit::MoveCursor(MoveDirection moveDir)
{
    QFontMetrics fm(m_Text->GetFontProperties()->face());

    int cursorPos = m_cursorImage->GetArea().x();
    int cursorWidth = m_cursorImage->GetArea().width();
    QRect textRect = m_Text->GetArea();
    QRect drawRect = m_Text->GetDrawRect();
    int newcursorPos = 0;
    QSize size;

    switch (moveDir)
    {
        case MoveLeft:
            if (m_Position < 0)
                return false;

            size = fm.size(Qt::TextSingleLine, m_Message.mid(m_Position,1));

            newcursorPos = cursorPos - size.width();

            if (newcursorPos < m_PaddingMargin)
            {
                newcursorPos = m_PaddingMargin;
                if (m_Position == 0)
                    m_Text->SetStartPosition(0, 0);
                else
                    m_Text->MoveStartPosition(size.width(), 0);
            }

            m_Position--;
            break;
        case MoveRight:
            if (m_Position == (m_Message.size() - 1))
                return false;

            size = fm.size(Qt::TextSingleLine, m_Message.mid(m_Position+1,1));

            newcursorPos = cursorPos + size.width();

            if (newcursorPos > textRect.width())
            {
                m_Text->MoveStartPosition(-(size.width()), 0);
                newcursorPos = cursorPos;
            }

            m_Position++;
            break;
        case MoveEnd:
            size = fm.size(Qt::TextSingleLine, m_Message);

            int messageWidth = size.width();

            if ((messageWidth + cursorWidth)
                >= textRect.width())
            {
                int newx = drawRect.width() -
                           (messageWidth + cursorWidth);
                m_Text->MoveStartPosition(newx, 0);
                newcursorPos = messageWidth + newx + m_PaddingMargin;
            }
            else
            {
                m_Text->SetStartPosition(0,0);
                if (messageWidth <= 0)
                    newcursorPos = m_PaddingMargin;
                else
                    newcursorPos = messageWidth + m_PaddingMargin;
            }

            m_Position = m_Message.size() - 1;

            break;
    }

    m_cursorImage->SetPosition(newcursorPos, textRect.y());

    SetRedraw();
    return true;
}

bool MythUITextEdit::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;

    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            if (!MoveCursor(MoveLeft))
                handled = false;
        }
        else if (action == "RIGHT")
        {
            if (!MoveCursor(MoveRight))
                handled = false;
        }
        else if (action == "BACKSPACE" || action == "DELETE")
        {
            RemoveCharacter();
        }
        else
            handled = false;
    }

    if (!handled && InsertCharacter(e->text()))
        handled = true;

    return handled;
}

void MythUITextEdit::CopyFrom(MythUIType *base)
{
    MythUITextEdit *textedit = dynamic_cast<MythUITextEdit *>(base);
    if (!textedit)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_blinkInterval = textedit->m_blinkInterval;
    m_cursorBlinkRate = textedit->m_cursorBlinkRate;
    m_showCursor = textedit->m_showCursor;
    m_maxLength = textedit->m_maxLength;
    m_Justification = textedit->m_Justification;
    m_PaddingMargin = textedit->m_PaddingMargin;
    m_Message = textedit->m_Message;
    m_Position = textedit->m_Position;
    m_Filter = textedit->m_Filter;

    MythUIType::CopyFrom(base);

    m_backgroundImage = dynamic_cast<MythUIImage *>
                    (GetChild("backgroundimage"));
    m_Text = dynamic_cast<MythUIText *>
                (GetChild("textarea"));
    m_cursorImage = dynamic_cast<MythUIImage *>
                    (GetChild("cursorimage"));
}

void MythUITextEdit::CreateCopy(MythUIType *parent)
{
    MythUITextEdit *textedit = new MythUITextEdit(parent, objectName());
    textedit->CopyFrom(this);
}
