// Own header
#include "mythuitextedit.h"

// QT headers
#include <QApplication>
#include <QChar>
#include <QKeyEvent>
#include <QDomDocument>
#include <QInputMethodEvent>
#include <Qt>

// libmythbase headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

// MythUI headers
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "mythgesture.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythuiimage.h"

#define LOC      QString("MythUITextEdit: ")

MythUITextEdit::MythUITextEdit(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_message("")
{
    connect(this, &MythUIType::TakingFocus, this, &MythUITextEdit::Select);
    connect(this, &MythUIType::LosingFocus, this, &MythUITextEdit::Deselect);

    m_canHaveFocus = true;

    m_lastKeyPress.start();
    m_messageBak.clear();
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

    if (m_hasFocus)
    {
        if (m_lastKeyPress.elapsed() < 500ms)
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
    {
        m_cursorImage->SetVisible(false);
    }

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
    if (m_text)
    {
        m_text->SetText(".");
        m_text->SetText("");
    }

    if (m_cursorImage && m_text)
        m_cursorImage->SetPosition(m_text->CursorPosition(0));
}

void MythUITextEdit::SetInitialStates()
{
    if (m_initialized)
        return;

    m_initialized = true;

    m_text        = dynamic_cast<MythUIText *>(GetChild("text"));
    m_cursorImage = dynamic_cast<MythUIImage *>(GetChild("cursor"));
    m_backgroundState =
        dynamic_cast<MythUIStateType *>(GetChild("background"));

    if (!m_text)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Missing text element.");

    if (!m_cursorImage)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Missing cursor element.");

    if (!m_backgroundState)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Missing background element.");

    if (!m_text || !m_cursorImage)
    {
        m_text = nullptr;
        m_cursorImage = nullptr;
        m_backgroundState = nullptr;
        return;
    }

    if (m_backgroundState && !m_backgroundState->DisplayState("active"))
        LOG(VB_GENERAL, LOG_ERR, LOC + "active state doesn't exist");
    m_text->SetCutDown(Qt::ElideNone);

    QFontMetrics fm(m_text->GetFontProperties()->face());
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
    if (!m_text || (m_message == text))
        return;

    m_message = text;

    if (m_isPassword)
    {
        QString obscured;

        obscured.fill('*', m_message.size());
        m_text->SetText(obscured);
    }
    else
    {
        m_text->SetText(m_message);
    }

    if (moveCursor)
        MoveCursor(MoveEnd);

    emit valueChanged();
}

void MythUITextEdit::InsertText(const QString &text)
{
    if (!m_text)
        return;

    for (const auto& c : std::as_const(text))
        InsertCharacter(c);

    emit valueChanged();
}

bool MythUITextEdit::InsertCharacter(const QString &character)
{
    if (m_maxLength != 0 && m_message.length() == m_maxLength)
        return false;

    QString newmessage = m_message;

    const QChar *unichar = character.unicode();

    // Filter all non printable characters
    if (!unichar->isPrint())
        return false;

    if ((m_filter & FilterAlpha) && unichar->isLetter())
        return false;

    if ((m_filter & FilterNumeric) && unichar->isNumber())
        return false;

    if ((m_filter & FilterSymbols) && unichar->isSymbol())
        return false;

    if ((m_filter & FilterPunct) && unichar->isPunct())
        return false;

    newmessage.insert(m_position + 1, character);
    SetText(newmessage, false);
    MoveCursor(MoveRight);

    return true;
}

// This is used for updating IME.
bool MythUITextEdit::UpdateTmpString(const QString &str)
{
    if (!m_text)
        return false;

    if (str.isEmpty())
        return false;
    QString newmessage = m_message;
    newmessage.append(str);
    SetText(newmessage, false);
    return true;
}

void MythUITextEdit::RemoveCharacter(int position)
{
    if (m_message.isEmpty() || position < 0 || position >= m_message.size())
        return;

    QString newmessage = m_message;

    newmessage.remove(position, 1);
    SetText(newmessage, false);

    if (position == m_position)
        MoveCursor(MoveLeft);
}

bool MythUITextEdit::MoveCursor(MoveDirection moveDir)
{
    if (!m_text || !m_cursorImage)
        return false;

    switch (moveDir)
    {
        case MoveLeft:
            if (m_position < 0)
                return false;
            m_position--;
            break;
        case MoveRight:
            if (m_position == (m_message.size() - 1))
                return false;
            m_position++;
            break;
        case MoveUp:
        {
            int newPos = m_text->MoveCursor(-1);
            if (newPos == -1)
                return false;
            m_position = newPos - 1;
            break;
        }
        case MoveDown:
        {
            int newPos = m_text->MoveCursor(1);
            if (newPos == -1)
                return false;
            m_position = newPos - 1;
            break;
        }
        case MovePageUp:
        {
            int lines = m_text->m_area.height() / (m_text->m_lineHeight + m_text->m_leading);
            int newPos = m_text->MoveCursor(-lines);
            if (newPos == -1)
                return false;
            m_position = newPos - 1;
            break;
        }
        case MovePageDown:
        {
            int lines = m_text->m_area.height() / (m_text->m_lineHeight + m_text->m_leading);
            int newPos = m_text->MoveCursor(lines);
            if (newPos == -1)
                return false;
            m_position = newPos - 1;
            break;
        }
        case MoveEnd:
            m_position = m_message.size() - 1;
            break;
    }

    m_cursorImage->SetPosition(m_text->CursorPosition(m_position + 1));

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

    clipboard->setText(m_message);
}

void MythUITextEdit::PasteTextFromClipboard(QClipboard::Mode mode)
{
    QClipboard *clipboard = QApplication::clipboard();

    if (!clipboard->supportsSelection())
        mode = QClipboard::Clipboard;

    InsertText(clipboard->text(mode));
}

using keyCombo = QPair<int, int>;
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
}

bool MythUITextEdit::inputMethodEvent(QInputMethodEvent *event)
{
    // 1st test.
    if (m_isPassword)
        return false;

    bool _bak = m_isIMEinput;
    if (!m_isIMEinput && (event->commitString().isEmpty() || event->preeditString().isEmpty()))
    {
        m_isIMEinput = true;
        m_messageBak = m_message;
    }
#if 0
    printf("IME: %s->%s PREEDIT=\"%s\" COMMIT=\"%s\"\n"
           , (_bak) ? "ON" : "OFF"
           , (m_isIMEinput) ? "ON" : "OFF"
           , event->preeditString().toUtf8().constData()
           , event->commitString().toUtf8().constData());
#endif
    if (!event->commitString().isEmpty() && m_isIMEinput)
    {
        m_message = m_messageBak;
        m_messageBak.clear();
        InsertText(event->commitString());
        m_isIMEinput = false;
        return true; // commited
    }
    if (m_isIMEinput && !event->preeditString().isEmpty())
    {
        m_message = m_messageBak;
        UpdateTmpString(event->preeditString());
        return true; // preedited
    }
    if (m_isIMEinput && _bak)
    { // Abort?
        m_isIMEinput = false;
        QString newmessage= m_messageBak;
        m_messageBak.clear();
        SetText(newmessage, true);
        return true;
    }
    return true; // Not commited
}

bool MythUITextEdit::keyPressEvent(QKeyEvent *event)
{
    if (m_isIMEinput)    // Prefer IME then keyPress.
         return true;
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
    if (((modifiers & Qt::GroupSwitchModifier) != 0U) &&
        (keynum >= Qt::Key_Dead_Grave) && (keynum <= Qt::Key_Dead_Horn))
    {
        m_composeKey = keynum;
        handled = true;
    }
    else if (m_composeKey > 0) // 'Compose' the key
    {
        if (gDeadKeyMap.isEmpty())
            LoadDeadKeys(gDeadKeyMap);

        LOG(VB_GUI, LOG_DEBUG, QString("Compose key: %1 Key: %2")
            .arg(QString::number(m_composeKey, 16), QString::number(keynum, 16)));

        if (gDeadKeyMap.contains(keyCombo(m_composeKey, keynum)))
        {
            int keycode = gDeadKeyMap.value(keyCombo(m_composeKey, keynum));

            //QKeyEvent key(QEvent::KeyPress, keycode, modifiers);
            character = QChar(keycode);

            if ((modifiers & Qt::ShiftModifier) != 0U)
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

        const QString& action = actions[i];
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
        else if (action == "PAGEUP")
        {
            handled = MoveCursor(MovePageUp);
        }
        else if (action == "PAGEDOWN")
        {
            handled = MoveCursor(MovePageDown);
        }
        else if (action == "DELETE")
        {
            RemoveCharacter(m_position + 1);
        }
        else if (action == "BACKSPACE")
        {
            RemoveCharacter(m_position);
        }
        else if (action == "NEWLINE")
        {
            QString newmessage = m_message;
            newmessage.insert(m_position + 1, '\n');
            SetText(newmessage, false);
            MoveCursor(MoveRight);
        }
        else if (action == "SELECT" && keynum != Qt::Key_Space
                 && GetMythDB()->GetNumSetting("UseVirtualKeyboard", 1) == 1)
        {
            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
            auto *kb = new MythUIVirtualKeyboard(popupStack, this);

            if (kb->Create())
            {
                popupStack->AddScreen(kb);
            }
            else
            {
                delete kb;
            }
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
        {
            handled = false;
        }
    }

    return handled;
}

/** \brief Mouse click/movement handler, receives mouse gesture events from the
 *         QCoreApplication event loop. Should not be used directly.
 *
 *  \param event Mouse event
 */
bool MythUITextEdit::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;

    if (event->GetGesture() == MythGestureEvent::Click &&
        event->GetButton() == Qt::MiddleButton)
    {
        PasteTextFromClipboard(QClipboard::Selection);
    }

    return handled;
}

void MythUITextEdit::CopyFrom(MythUIType *base)
{
    auto *textedit = dynamic_cast<MythUITextEdit *>(base);

    if (!textedit)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ERROR, bad parsing");
        return;
    }

    m_message.clear();
    m_position = -1;

    m_blinkInterval = textedit->m_blinkInterval;
    m_cursorBlinkRate = textedit->m_cursorBlinkRate;
    m_maxLength = textedit->m_maxLength;
    m_filter = textedit->m_filter;
    m_keyboardPosition = textedit->m_keyboardPosition;

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

void MythUITextEdit::CreateCopy(MythUIType *parent)
{
    auto *textedit = new MythUITextEdit(parent, objectName());
    textedit->CopyFrom(this);
}
