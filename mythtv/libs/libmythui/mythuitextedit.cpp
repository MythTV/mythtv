// Own header
#include "mythuitextedit.h"

// QT headers
#include <QApplication>
#include <QRegExp>
#include <QChar>
#include <QKeyEvent>
#include <QDomDocument>
#include <Qt>

// libmythbase headers
#include "mythlogging.h"
#include "mythdb.h"

// MythUI headers
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"

#define LOC      QString("MythUITextEdit: ")

MythUITextEdit::MythUITextEdit(MythUIType *parent, const QString &name)
    : MythUIType(parent, name)
{
    m_Message = "";
    m_Filter = FilterNone;

    m_isPassword = false;

    m_blinkInterval = 0;
    m_cursorBlinkRate = 40;

    m_Position = -1;

    m_maxLength = 255;

    m_backgroundState = NULL;
    m_cursorImage = NULL;
    m_Text = NULL;

    m_keyboardPosition = VK_POSBELOWEDIT;

    connect(this, SIGNAL(TakingFocus()), SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), SLOT(Deselect()));

    m_CanHaveFocus = true;

    m_initialized = false;

    m_lastKeyPress.start();

    m_composeKey = 0;
}

MythUITextEdit::~MythUITextEdit()
{
}

void MythUITextEdit::Select()
{
    if (m_backgroundState && !m_backgroundState->DisplayState("selected"))
        LOG(VB_GENERAL, LOG_ERR, LOC + "selected state doesn't exist");
}

void MythUITextEdit::Deselect()
{
    if (m_backgroundState && !m_backgroundState->DisplayState("active"))
        LOG(VB_GENERAL, LOG_ERR, LOC + "active state doesn't exist");
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
        if (m_lastKeyPress.elapsed() < 500)
        {
            m_cursorImage->SetVisible(true);
            m_blinkInterval = 0;
        }
        else if (m_blinkInterval > m_cursorBlinkRate)
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

bool MythUITextEdit::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    bool parsed = true;

    if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
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
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                        QString("Unknown popup position '%1'").arg(pos));
            m_keyboardPosition = VK_POSBELOWEDIT;
        }
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return parsed;
}

void MythUITextEdit::Finalize()
{
    SetInitialStates();

    // Give it something to chew on, so it can position the initial
    // cursor in the right place.  Toggle text, to force an area recalc.
    if (m_Text)
    {
        m_Text->SetText(".");
        m_Text->SetText("");
    }

    if (m_cursorImage && m_Text)
        m_cursorImage->SetPosition(m_Text->CursorPosition(0));
}

void MythUITextEdit::SetInitialStates()
{
    if (m_initialized)
        return;

    m_initialized = true;

    m_Text        = dynamic_cast<MythUIText *>(GetChild("text"));
    m_cursorImage = dynamic_cast<MythUIImage *>(GetChild("cursor"));
    m_backgroundState =
        dynamic_cast<MythUIStateType *>(GetChild("background"));

    if (!m_Text)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Missing text element.");

    if (!m_cursorImage)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Missing cursor element.");

    if (!m_backgroundState)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Missing background element.");

    if (!m_Text || !m_cursorImage)
    {
        m_Text = NULL;
        m_cursorImage = NULL;
        m_backgroundState = NULL;
        return;
    }

    if (m_backgroundState && !m_backgroundState->DisplayState("active"))
        LOG(VB_GENERAL, LOG_ERR, LOC + "active state doesn't exist");
    m_Text->SetCutDown(Qt::ElideNone);

    QFontMetrics fm(m_Text->GetFontProperties()->face());
    int height = fm.height();

    if (height > 0)
    {
        MythRect imageArea = m_cursorImage->GetFullArea();
        int width = int(((float)height / (float)imageArea.height())
                        * (float)imageArea.width());

        if (width <= 0)
            width = 1;

        m_cursorImage->ForceSize(QSize(width, height));
    }
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
    m_Message.detach();

    if (m_isPassword)
    {
        QString obscured;

        obscured.fill('*', m_Message.size());
        m_Text->SetText(obscured);
    }
    else
        m_Text->SetText(m_Message);

    if (moveCursor)
        MoveCursor(MoveEnd);

    emit valueChanged();
}

QString MythUITextEdit::GetText(void) const
{
    QString ret = m_Message;
    ret.detach();
    return ret;   
}

void MythUITextEdit::InsertText(const QString &text)
{
    if (!m_Text)
        return;

    int i = 0;

    for (; i < text.size(); ++i)
    {
        InsertCharacter(text.data()[i]);
    }

    emit valueChanged();
}

bool MythUITextEdit::InsertCharacter(const QString &character)
{
    if (m_maxLength != 0 && m_Message.length() == m_maxLength)
        return false;

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

    newmessage.insert(m_Position + 1, character);
    SetText(newmessage, false);
    MoveCursor(MoveRight);

    return true;
}

void MythUITextEdit::RemoveCharacter(int position)
{
    if (m_Message.isEmpty() || position < 0 || position >= m_Message.size())
        return;

    QString newmessage = m_Message;

    newmessage.remove(position, 1);
    SetText(newmessage, false);

    if (position == m_Position)
        MoveCursor(MoveLeft);
}

bool MythUITextEdit::MoveCursor(MoveDirection moveDir)
{
    if (!m_Text || !m_cursorImage)
        return false;

    switch (moveDir)
    {
        case MoveLeft:
            if (m_Position < 0)
                return false;
            m_Position--;
            break;
        case MoveRight:
            if (m_Position == (m_Message.size() - 1))
                return false;
            m_Position++;
            break;
        case MoveUp:
        {
            int newPos = m_Text->MoveCursor(true);
            if (newPos == -1)
                return false;
            m_Position = newPos - 1;
            break;
        }
        case MoveDown:
        {
            int newPos = m_Text->MoveCursor(false);
            if (newPos == -1)
                return false;
            m_Position = newPos - 1;
            break;
        }
        case MoveEnd:
            m_Position = m_Message.size() - 1;
            break;
    }

    m_cursorImage->SetPosition(m_Text->CursorPosition(m_Position + 1));

    SetRedraw();
    return true;
}

void MythUITextEdit::CutTextToClipboard()
{
    CopyTextToClipboard();
    Reset();
}

void MythUITextEdit::CopyTextToClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(m_Message);
}

void MythUITextEdit::PasteTextFromClipboard(QClipboard::Mode mode)
{
    QClipboard *clipboard = QApplication::clipboard();

    if (!clipboard->supportsSelection())
        mode = QClipboard::Clipboard;

    InsertText(clipboard->text(mode));
}

typedef QPair<int, int> keyCombo;
static QMap<keyCombo, int> gDeadKeyMap;

static void LoadDeadKeys(QMap<QPair<int, int>, int> &map)
{
                 // Dead key              // Key        // Result
    map[keyCombo(Qt::Key_Dead_Grave,      Qt::Key_A)] = Qt::Key_Agrave;
    map[keyCombo(Qt::Key_Dead_Acute,      Qt::Key_A)] = Qt::Key_Aacute;
    map[keyCombo(Qt::Key_Dead_Circumflex, Qt::Key_A)] = Qt::Key_Acircumflex;
    map[keyCombo(Qt::Key_Dead_Tilde,      Qt::Key_A)] = Qt::Key_Atilde;
    map[keyCombo(Qt::Key_Dead_Diaeresis,  Qt::Key_A)] = Qt::Key_Adiaeresis;
    map[keyCombo(Qt::Key_Dead_Abovering,  Qt::Key_A)] = Qt::Key_Aring;

    map[keyCombo(Qt::Key_Dead_Cedilla,    Qt::Key_C)] = Qt::Key_Ccedilla;

    map[keyCombo(Qt::Key_Dead_Grave,      Qt::Key_E)] = Qt::Key_Egrave;
    map[keyCombo(Qt::Key_Dead_Acute,      Qt::Key_E)] = Qt::Key_Eacute;
    map[keyCombo(Qt::Key_Dead_Circumflex, Qt::Key_E)] = Qt::Key_Ecircumflex;
    map[keyCombo(Qt::Key_Dead_Diaeresis,  Qt::Key_E)] = Qt::Key_Ediaeresis;

    map[keyCombo(Qt::Key_Dead_Grave,      Qt::Key_I)] = Qt::Key_Igrave;
    map[keyCombo(Qt::Key_Dead_Acute,      Qt::Key_I)] = Qt::Key_Iacute;
    map[keyCombo(Qt::Key_Dead_Circumflex, Qt::Key_I)] = Qt::Key_Icircumflex;
    map[keyCombo(Qt::Key_Dead_Diaeresis,  Qt::Key_I)] = Qt::Key_Idiaeresis;

    map[keyCombo(Qt::Key_Dead_Tilde,      Qt::Key_N)] = Qt::Key_Ntilde;

    map[keyCombo(Qt::Key_Dead_Grave,      Qt::Key_O)] = Qt::Key_Ograve;
    map[keyCombo(Qt::Key_Dead_Acute,      Qt::Key_O)] = Qt::Key_Oacute;
    map[keyCombo(Qt::Key_Dead_Circumflex, Qt::Key_O)] = Qt::Key_Ocircumflex;
    map[keyCombo(Qt::Key_Dead_Tilde,      Qt::Key_O)] = Qt::Key_Otilde;
    map[keyCombo(Qt::Key_Dead_Diaeresis,  Qt::Key_O)] = Qt::Key_Odiaeresis;

    map[keyCombo(Qt::Key_Dead_Grave,      Qt::Key_U)] = Qt::Key_Ugrave;
    map[keyCombo(Qt::Key_Dead_Acute,      Qt::Key_U)] = Qt::Key_Uacute;
    map[keyCombo(Qt::Key_Dead_Circumflex, Qt::Key_U)] = Qt::Key_Ucircumflex;
    map[keyCombo(Qt::Key_Dead_Diaeresis,  Qt::Key_U)] = Qt::Key_Udiaeresis;

    map[keyCombo(Qt::Key_Dead_Acute,      Qt::Key_Y)] = Qt::Key_Yacute;
    map[keyCombo(Qt::Key_Dead_Diaeresis,  Qt::Key_Y)] = Qt::Key_ydiaeresis;

    return;
}

bool MythUITextEdit::keyPressEvent(QKeyEvent *event)
{
    m_lastKeyPress.restart();

    QStringList actions;
    bool handled = false;

    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions,
                                                     false);

    Qt::KeyboardModifiers modifiers = event->modifiers();
    int keynum = event->key();

    if (keynum >= Qt::Key_Shift && keynum <= Qt::Key_CapsLock)
        return false;

    QString character;
    // Compose key handling
    // Enter composition mode
    if ((modifiers & Qt::GroupSwitchModifier) &&
        (keynum >= Qt::Key_Dead_Grave) && (keynum <= Qt::Key_Dead_Horn))
    {
        m_composeKey = keynum;
        handled = true;
    }
    else if (m_composeKey > 0) // 'Compose' the key
    {
        if (gDeadKeyMap.isEmpty())
            LoadDeadKeys(gDeadKeyMap);

        LOG(VB_GUI, LOG_DEBUG, QString("Compose key: %1 Key: %2").arg(QString::number(m_composeKey, 16)).arg(QString::number(keynum, 16)));

        if (gDeadKeyMap.contains(keyCombo(m_composeKey, keynum)))
        {
            int keycode = gDeadKeyMap.value(keyCombo(m_composeKey, keynum));

            //QKeyEvent key(QEvent::KeyPress, keycode, modifiers);
            character = QChar(keycode);

            if (modifiers & Qt::ShiftModifier)
                character = character.toUpper();
            else
                character = character.toLower();
            LOG(VB_GUI, LOG_DEBUG, QString("Found match for dead-key combo - %1").arg(character));
        }
        m_composeKey = 0;
    }

    if (character.isEmpty())
        character = event->text();

    if (!handled && InsertCharacter(character))
        handled = true;

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            MoveCursor(MoveLeft);
        }
        else if (action == "RIGHT")
        {
            MoveCursor(MoveRight);
        }
        else if (action == "UP")
        {
            handled = MoveCursor(MoveUp);
        }
        else if (action == "DOWN")
        {
            handled = MoveCursor(MoveDown);
        }
        else if (action == "DELETE")
        {
            RemoveCharacter(m_Position + 1);
        }
        else if (action == "BACKSPACE")
        {
            RemoveCharacter(m_Position);
        }
        else if (action == "SELECT" && keynum != Qt::Key_Space
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
        else if (action == "CUT")
        {
            CutTextToClipboard();
        }
        else if (action == "COPY")
        {
            CopyTextToClipboard();
        }
        else if (action == "PASTE")
        {
            PasteTextFromClipboard();
        }
        else
            handled = false;
    }

    return handled;
}

/** \brief Mouse click/movement handler, receives mouse gesture events from the
 *         QCoreApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
bool MythUITextEdit::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;

    if (event->gesture() == MythGestureEvent::Click &&
        event->GetButton() == MythGestureEvent::MiddleButton)
    {
        PasteTextFromClipboard(QClipboard::Selection);
    }

    return handled;
}

void MythUITextEdit::CopyFrom(MythUIType *base)
{
    MythUITextEdit *textedit = dynamic_cast<MythUITextEdit *>(base);

    if (!textedit)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ERROR, bad parsing");
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
