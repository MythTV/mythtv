
// Own header
#include "mythuitextedit.h"

// QT headers
#include <QApplication>
#include <QRegExp>
#include <QChar>
#include <QKeyEvent>
#include <QDomDocument>

// Libmythdb headers
#include "mythverbose.h"
#include "mythdb.h"

// MythUI headers
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"


MythUITextEdit::MythUITextEdit(MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_Message = "";
    m_Filter = FilterNone;

    m_isPassword = false;

    m_blinkInterval = 0;
    m_cursorBlinkRate = 40;

    m_Position = -1;

    m_maxLength = 0;

    m_backgroundState = NULL;
    m_cursorImage = NULL;
    m_Text = NULL;

    m_keyboardPosition = VK_POSBELOWEDIT;

    connect(this, SIGNAL(TakingFocus()), SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), SLOT(Deselect()));

    m_CanHaveFocus = true;

    m_initialized = false;
}

MythUITextEdit::~MythUITextEdit()
{
}

void MythUITextEdit::Select()
{
    if (m_backgroundState && !m_backgroundState->DisplayState("selected"))
        VERBOSE(VB_IMPORTANT, "MythUITextEdit: selected state doesn't exist");
}

void MythUITextEdit::Deselect()
{
    if (m_backgroundState && !m_backgroundState->DisplayState("active"))
        VERBOSE(VB_IMPORTANT, "MythUITextEdit: active state doesn't exist");
}

void MythUITextEdit::Reset()
{
    SetText("");
}

void MythUITextEdit::Pulse(void)
{
    if (!m_cursorImage)
        return;

    if (m_HasFocus)
    {
        if (m_blinkInterval > m_cursorBlinkRate)
        {
            m_blinkInterval = 0;
            if (m_cursorImage->IsVisible())
                m_cursorImage->SetVisible(false);
            else
                m_cursorImage->SetVisible(true);
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
        SetArea(parseRect(element));
    }
    else if (element.tagName() == "maxlength")
    {
        QString maxlength = getFirstText(element);
        SetMaxLength(maxlength.toInt());
    }
    else if (element.tagName() == "keyboardposition")
    {
        QString pos = getFirstText(element);
        if (pos == "aboveedit")
            m_keyboardPosition = VK_POSABOVEEDIT;
        else if (pos == "belowedit")
            m_keyboardPosition = VK_POSBELOWEDIT;
        else if (pos == "screentop")
            m_keyboardPosition = VK_POSTOPDIALOG;
        else if (pos == "screenbottom")
            m_keyboardPosition = VK_POSBOTTOMDIALOG;
        else if (pos == "screencenter")
            m_keyboardPosition = VK_POSCENTERDIALOG;
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("MythUITextEdit: Unknown popup position '%1'")
                            .arg(pos));
            m_keyboardPosition = VK_POSBELOWEDIT;
        }
    }
    else
        return MythUIType::ParseElement(element);

    return parsed;
}

void MythUITextEdit::Finalize()
{
    SetInitialStates();
}

void MythUITextEdit::SetInitialStates()
{
    if (m_initialized)
        return;

    m_initialized = true;

    m_backgroundState = dynamic_cast<MythUIStateType *>
                                                    (GetChild("background"));
    m_cursorImage = dynamic_cast<MythUIImage *> (GetChild("cursor"));
    m_Text = dynamic_cast<MythUIText *> (GetChild("text"));

    if (!m_Text || !m_cursorImage || !m_backgroundState)
    {
        VERBOSE(VB_IMPORTANT, "MythUITextEdit: Missing required element.");
        return;
    }

    if (m_backgroundState && !m_backgroundState->DisplayState("active"))
        VERBOSE(VB_IMPORTANT, "MythUITextEdit: active state doesn't exist");

    QFontMetrics fm(m_Text->GetFontProperties()->face());
    int height = fm.height();
    if (height > 0)
    {
        MythRect imageArea = m_cursorImage->GetArea();
        int width = int(((float)height / (float)imageArea.height())
                        * (float)imageArea.width());
        if (width <= 0)
            width = 1;
        m_cursorImage->ForceSize(QSize(width,height));
    }

    MythRect textrect = m_Text->GetArea();

    if (textrect.isNull())
        textrect = MythRect(5,5,m_Area.width()-10,m_Area.height()-10);

    textrect.setWidth(textrect.width() - m_cursorImage->GetArea().width());

    if (textrect.isValid())
        m_Text->SetArea(textrect);

    m_Text->SetCutDown(false);

    m_cursorImage->SetPosition(textrect.x(),textrect.y());
}

void MythUITextEdit::SetMaxLength(const int length)
{
    m_maxLength = length;
}

void MythUITextEdit::SetText(const QString &text, bool moveCursor)
{
    if (!m_Text || (m_Message == text))
        return;

    m_Message = text;

    if (m_isPassword)
    {
        QString obscured;
        while (obscured.size() < m_Message.size())
            obscured.append("*");
        m_Text->SetText(obscured);
    }
    else
        m_Text->SetText(m_Message);

    if (moveCursor)
        MoveCursor(MoveEnd);
    emit valueChanged();
}

bool MythUITextEdit::InsertCharacter(const QString &character)
{
    if (m_maxLength != 0 && m_Message.length() == m_maxLength)
        return true;

    QString newmessage = m_Message;

    const QChar *unichar = character.unicode();
    // Filter all non printable characters
    if (!unichar->isPrint())
        return false;

    if ((m_Filter & FilterAlpha) && unichar->isLetter())
        return false;
    if ((m_Filter & FilterNumeric) && unichar->isNumber())
        return false;
    if ((m_Filter & FilterSymbols) && unichar->isSymbol())
        return false;
    if ((m_Filter & FilterPunct) && unichar->isPunct())
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
    if (!m_Text || !m_cursorImage)
        return false;

    QFontMetrics fm(m_Text->GetFontProperties()->face());

    int cursorPos = m_cursorImage->GetArea().x();
    int cursorWidth = m_cursorImage->GetArea().width();
    MythRect textRect = m_Text->GetArea();
    MythRect drawRect = m_Text->GetDrawRect();
    int newcursorPos = 0;

    QString string;

    if (m_isPassword)
    {
        while (string.size() < m_Message.size())
            string.append("*");
    }
    else
        string = m_Message;

    QSize stringSize = fm.size(Qt::TextSingleLine, string);
    m_Text->SetDrawRectSize(stringSize.width(), textRect.height());
    QSize charSize;

    switch (moveDir)
    {
        case MoveLeft:
            if (m_Position < 0)
                return false;

            charSize = fm.size(Qt::TextSingleLine, string.mid(m_Position,1));

            newcursorPos = cursorPos - charSize.width();

            if (newcursorPos < (textRect.x() + (textRect.width()/2)))
            {
                if (m_Position == 0 || (drawRect.x() + charSize.width() > textRect.x()))
                    m_Text->SetDrawRectPosition(0,0);
                else
                    m_Text->MoveDrawRect(charSize.width(), 0);

                if (drawRect.x() < textRect.x())
                    newcursorPos = cursorPos;
            }

            m_Position--;
            break;
        case MoveRight:
            if (m_Position == (string.size() - 1))
                return false;

            charSize = fm.size(Qt::TextSingleLine, string.mid(m_Position+1,1));

            newcursorPos = cursorPos + charSize.width();

            if (newcursorPos > textRect.width())
            {
                m_Text->MoveDrawRect(-(charSize.width()), 0);
                newcursorPos = cursorPos;
            }

            m_Position++;
            break;
        case MoveEnd:
            int messageWidth = stringSize.width();

            if ((messageWidth + cursorWidth)
                >= textRect.width())
            {
                int newx = drawRect.width() -
                           (messageWidth + cursorWidth);
                m_Text->MoveDrawRect(newx, 0);
                newcursorPos = messageWidth + newx + textRect.x();
            }
            else
            {
                m_Text->SetDrawRectPosition(0, 0);
                if (messageWidth <= 0)
                    newcursorPos = textRect.x();
                else
                    newcursorPos = messageWidth + textRect.x();
            }

            m_Position = string.size() - 1;

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

    handled = GetMythMainWindow()->TranslateKeyPress("Global", e, actions, false);

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
        else if (action == "BACKSPACE")
        {
            RemoveCharacter();
        }
        else if (action == "SELECT" && e->key() != Qt::Key_Space
                 && GetMythDB()->GetNumSetting("UseVirtualKeyboard", 1) == 1)
        {
            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
            MythUIVirtualKeyboard *kb =  new MythUIVirtualKeyboard(popupStack, this);

            if (kb->Create())
            {
                //connect(kb, SIGNAL(keyPress(QString)), SLOT(keyPress(QString)));
                popupStack->AddScreen(kb);
            }
            else
                delete kb;
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

    m_Message.clear();
    m_Position = -1;

    m_blinkInterval = textedit->m_blinkInterval;
    m_cursorBlinkRate = textedit->m_cursorBlinkRate;
    m_maxLength = textedit->m_maxLength;
    m_Filter = textedit->m_Filter;
    m_keyboardPosition = textedit->m_keyboardPosition;

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

void MythUITextEdit::CreateCopy(MythUIType *parent)
{
    MythUITextEdit *textedit = new MythUITextEdit(parent, objectName());
    textedit->CopyFrom(this);
}
