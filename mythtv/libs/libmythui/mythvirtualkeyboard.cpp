
// Own header
#include "mythvirtualkeyboard.h"

// c++
#include <iostream>

// qt
#include <QKeyEvent>
#include <QDomDocument>
#include <QFile>

// myth
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "mythuibutton.h"
#include "mythuitextedit.h"
#include "mythcorecontext.h"


#define LOC      QString("MythUIVirtualKeyboard: ")

static const int numcomps = 95;

static const QString comps[numcomps][3] = {
        {"!", "!", (QChar)0xa1},    {"c", "/", (QChar)0xa2},
        {"l", "-", (QChar)0xa3},    {"o", "x", (QChar)0xa4},
        {"y", "-", (QChar)0xa5},    {"|", "|", (QChar)0xa6},
        {"s", "o", (QChar)0xa7},    {"\"", "\"", (QChar)0xa8},
        {"c", "o", (QChar)0xa9},    {"-", "a", (QChar)0xaa},
        {"<", "<", (QChar)0xab},    {"-", "|", (QChar)0xac},
        {"-", "-", (QChar)0xad},    {"r", "o", (QChar)0xae},
        {"^", "-", (QChar)0xaf},    {"^", "0", (QChar)0xb0},
        {"+", "-", (QChar)0xb1},    {"^", "2", (QChar)0xb2},
        {"^", "3", (QChar)0xb3},    {"/", "/", (QChar)0xb4},
        {"/", "u", (QChar)0xb5},    {"P", "!", (QChar)0xb6},
        {"^", ".", (QChar)0xb7},    {",", ",", (QChar)0xb8},
        {"^", "1", (QChar)0xb9},    {"_", "o", (QChar)0xba},
        {">", ">", (QChar)0xbb},    {"1", "4", (QChar)0xbc},
        {"1", "2", (QChar)0xbd},    {"3", "4", (QChar)0xbe},
        {"?", "?", (QChar)0xbf},    {"A", "`", (QChar)0xc0},
        {"A", "'", (QChar)0xc1},    {"A", "^", (QChar)0xc2},
        {"A", "~", (QChar)0xc3},    {"A", "\"", (QChar)0xc4},
        {"A", "*", (QChar)0xc5},    {"A", "E", (QChar)0xc6},
        {"C", ",", (QChar)0xc7},    {"E", "`", (QChar)0xc8},
        {"E", "'", (QChar)0xc9},    {"E", "^", (QChar)0xca},
        {"E", "\"", (QChar)0xcb},   {"I", "`", (QChar)0xcc},
        {"I", "'", (QChar)0xcd},    {"I", "^", (QChar)0xce},
        {"I", "\"", (QChar)0xcf},   {"D", "-", (QChar)0xd0},
        {"N", "~", (QChar)0xd1},    {"O", "`", (QChar)0xd2},
        {"O", "'", (QChar)0xd3},    {"O", "^", (QChar)0xd4},
        {"O", "~", (QChar)0xd5},    {"O", "\"", (QChar)0xd6},
        {"x", "x", (QChar)0xd7},    {"O", "/", (QChar)0xd8},
        {"U", "`", (QChar)0xd9},    {"U", "'", (QChar)0xda},
        {"U", "^", (QChar)0xdb},    {"U", "\"", (QChar)0xdc},
        {"Y", "'", (QChar)0xdd},    {"T", "H", (QChar)0xde},
        {"s", "s", (QChar)0xdf},    {"a", "`", (QChar)0xe0},
        {"a", "'", (QChar)0xe1},    {"a", "^", (QChar)0xe2},
        {"a", "~", (QChar)0xe3},    {"a", "\"", (QChar)0xe4},
        {"a", "*", (QChar)0xe5},    {"a", "e", (QChar)0xe6},
        {"c", ",", (QChar)0xe7},    {"e", "`", (QChar)0xe8},
        {"e", "'", (QChar)0xe9},    {"e", "^", (QChar)0xea},
        {"e", "\"", (QChar)0xeb},   {"i", "`", (QChar)0xec},
        {"i", "'", (QChar)0xed},    {"i", "^", (QChar)0xee},
        {"i", "\"", (QChar)0xef},   {"d", "-", (QChar)0xf0},
        {"n", "~", (QChar)0xf1},    {"o", "`", (QChar)0xf2},
        {"o", "'", (QChar)0xf3},    {"o", "^", (QChar)0xf4},
        {"o", "~", (QChar)0xf5},    {"o", "\"", (QChar)0xf6},
        {"-", ":", (QChar)0xf7},    {"o", "/", (QChar)0xf8},
        {"u", "`", (QChar)0xf9},    {"u", "'", (QChar)0xfa},
        {"u", "^", (QChar)0xfb},    {"u", "\"", (QChar)0xfc},
        {"y", "'", (QChar)0xfd},    {"t", "h", (QChar)0xfe},
        {"y", "\"", (QChar)0xff}
};

MythUIVirtualKeyboard::MythUIVirtualKeyboard(MythScreenStack *parentStack, MythUITextEdit *parentEdit)
          : MythScreenType(parentStack, "MythUIVirtualKeyboard")
{
    m_parentEdit = parentEdit;

    m_shift = false;
    m_alt = false;
    m_lock = false;

    m_lockButton = NULL;
    m_altButton = NULL;
    m_compButton = NULL;
    m_shiftRButton = NULL;
    m_shiftLButton = NULL;

    m_composing = false;

    if (m_parentEdit)
        m_preferredPos = m_parentEdit->GetKeyboardPosition();
     else
        m_preferredPos = VK_POSBELOWEDIT;

     loadEventKeyDefinitions(&m_upKey, "UP");
     loadEventKeyDefinitions(&m_downKey, "DOWN");
     loadEventKeyDefinitions(&m_leftKey, "LEFT");
     loadEventKeyDefinitions(&m_rightKey, "RIGHT");
     loadEventKeyDefinitions(&m_newlineKey, "NEWLINE");
}

MythUIVirtualKeyboard::~MythUIVirtualKeyboard(void)
{
}

bool MythUIVirtualKeyboard::Create()
{
    if (!LoadWindowFromXML("keyboard/keyboard.xml", "keyboard", this))
        return false;

    BuildFocusList();

    loadKeyDefinitions(gCoreContext->GetLanguageAndVariant());
    updateKeys(true);

    int screenWidth, screenHeight;
    float xmult, ymult;
    GetMythUI()->GetScreenSettings(screenWidth, xmult, screenHeight, ymult);
    MythRect editArea = m_parentEdit->GetArea();
    MythRect area = GetArea();
    MythPoint newPos;

    //FIXME this assumes the edit is a direct child of the parent screen
    MythUIType *parentScreen = NULL;
    parentScreen = dynamic_cast<MythUIType *>(m_parentEdit->parent());
    if (parentScreen)
    {
        editArea.moveTopLeft(QPoint(editArea.x() + parentScreen->GetArea().x(),
                                    editArea.y() + parentScreen->GetArea().y()));
    }

    switch (m_preferredPos)
    {
        case VK_POSABOVEEDIT:
            if (editArea.y() - area.height() - 5 > 0)
            {
                newPos = QPoint(editArea.x() + editArea.width() / 2 - area.width() / 2,
                                editArea.y() - area.height() - 5);
            }
            else
            {
                newPos = QPoint(editArea.x() + editArea.width() / 2 - area.width() / 2,
                                editArea.y() + editArea.height() + 5);
            }
            break;

        case VK_POSTOPDIALOG:
            newPos = QPoint(screenWidth / 2 - area.width() / 2, 5);
            break;

        case VK_POSBOTTOMDIALOG:
            newPos = QPoint(screenWidth / 2 - area.width() / 2, screenHeight - 5 - area.height());
            break;

        case VK_POSCENTERDIALOG:
            newPos = QPoint(screenWidth / 2 - area.width() / 2, screenHeight / 2 - area.height() / 2);
            break;

        default:
            // VK_POSBELOWEDIT
            if (editArea.y() + editArea.height() + area.height() + 5 < screenHeight)
            {
                newPos = QPoint(editArea.x() + editArea.width() / 2 - area.width() / 2,
                                editArea.y() + editArea.height() + 5);
            }
            else
            {
                newPos = QPoint(editArea.x() + editArea.width() / 2 - area.width() / 2,
                                editArea.y() - area.height() - 5);
            }
            break;
    }

    // make sure the popup doesn't go off screen
    if (newPos.x() < 5)
        newPos.setX(5);
    if (newPos.x() + area.width() + 5 > screenWidth)
        newPos.setX(screenWidth - area.width() - 5);
    if (newPos.y() < 5)
        newPos.setY(5);
    if (newPos.y() + area.height() + 5 > screenHeight)
        newPos.setY(screenHeight - area.height() - 5);

    SetPosition(newPos);

    return true;
}

void MythUIVirtualKeyboard::loadKeyDefinitions(const QString &lang)
{
    QString language = lang.toLower();

    QString defFile = QString("keyboard/%1.xml").arg(language);

    if (!GetMythUI()->FindThemeFile(defFile))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "No keyboard definition file found for: " + language);

        // default to US keyboard layout
        defFile = "keyboard/en_us.xml";
        if (!GetMythUI()->FindThemeFile(defFile))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Cannot find definitions file: " + defFile);
            return;
        }
    }

    LOG(VB_GENERAL, LOG_NOTICE, "Loading definitions from: " + defFile);

    QDomDocument doc("keydefinitions");
    QFile file(defFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open definitions file: " + defFile);
        return;
    }
    if (!doc.setContent(&file))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Failed to parse definitions file: " + defFile);
        file.close();
        return;
    }
    file.close();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while(!n.isNull())
    {
        QDomElement e = n.toElement();
        if(!e.isNull())
        {
            if (e.tagName() == "key")
                parseKey(e);
        }
        n = n.nextSibling();
    }
}

void MythUIVirtualKeyboard::parseKey(const QDomElement &element)
{
    QString left, right, up, down;
    QString normal, shift, alt, altshift;

    QString name = element.attribute("name");
    QString type = element.attribute("type");

    QDomNode n = element.firstChild();
    while(!n.isNull())
    {
        QDomElement e = n.toElement();
        if(!e.isNull())
        {
            if (e.tagName() == "move")
            {
                left = e.attribute("left");
                right = e.attribute("right");
                up = e.attribute("up");
                down = e.attribute("down");
            }
            else if (e.tagName() == "char")
            {
                normal = e.attribute("normal");
                shift = e.attribute("shift");
                alt = e.attribute("alt");
                altshift = e.attribute("altshift");
            }
            else
                LOG(VB_GENERAL, LOG_ERR, "Unknown element in key definition");
        }
        n = n.nextSibling();
    }

    KeyDefinition key;
    key.name = name;
    key.type = type;
    key.left = left;
    key.right = right;
    key.up = up;
    key.down = down;
    key.normal = decodeChar(normal);
    key.alt = decodeChar(alt);
    key.shift = decodeChar(shift);
    key.altshift = decodeChar(altshift);

    m_keyMap[name] = key;
}

void MythUIVirtualKeyboard::updateKeys(bool connectSignals)
{
    QList<MythUIType *> *children = GetAllChildren();
    for (int i = 0; i < children->size(); ++i)
    {
        MythUIButton *button = dynamic_cast<MythUIButton *>(children->at(i));
        if (button)
        {
            if (m_keyMap.contains(button->objectName()))
            {
                KeyDefinition key = m_keyMap.value(button->objectName());
                button->SetText(getKeyText(key));

                if (connectSignals)
                {
                    if (key.type == "shift")
                    {
                        if (!m_shiftLButton)
                            m_shiftLButton = button;
                        else if (!m_shiftRButton)
                            m_shiftRButton = button;

                        button->SetLockable(true);
                        connect(button, SIGNAL(Clicked()), SLOT(shiftClicked()));
                    }
                    else if (key.type == "char")
                        connect(button, SIGNAL(Clicked()), SLOT(charClicked()));
                    else if (key.type == "done")
                        connect(button, SIGNAL(Clicked()), SLOT(returnClicked()));
                    else if (key.type == "del")
                        connect(button, SIGNAL(Clicked()), SLOT(delClicked()));
                    else if (key.type == "lock")
                    {
                        m_lockButton = button;
                        m_lockButton->SetLockable(true);
                        connect(m_lockButton, SIGNAL(Clicked()), SLOT(lockClicked()));
                    }
                    else if (key.type == "alt")
                    {
                        m_altButton = button;
                        m_altButton->SetLockable(true);
                        connect(m_altButton, SIGNAL(Clicked()), SLOT(altClicked()));
                    }
                    else if (key.type == "comp")
                    {
                        m_compButton = button;
                        m_compButton->SetLockable(true);
                        connect(m_compButton, SIGNAL(Clicked()), SLOT(compClicked()));
                    }
                    else if (key.type == "moveleft")
                        connect(button, SIGNAL(Clicked()), SLOT(moveleftClicked()));
                    else if (key.type == "moveright")
                        connect(button, SIGNAL(Clicked()), SLOT(moverightClicked()));
                    else if (key.type == "back")
                        connect(button, SIGNAL(Clicked()), SLOT(backClicked()));
                }
            }
            else
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("WARNING - Key '%1' not found in map")
                        .arg(button->objectName()));
        }
    }
}

bool MythUIVirtualKeyboard::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    if (handled)
        return true;

    bool keyFound = false;
    KeyDefinition key;
    if (GetFocusWidget())
    {
        if (m_keyMap.contains(GetFocusWidget()->objectName()))
        {
            key = m_keyMap.value(GetFocusWidget()->objectName());
            keyFound = true;;
        }
    }

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            if (keyFound)
                SetFocusWidget(GetChild(key.up));
        }
        else if (action == "DOWN")
        {
            if (keyFound)
                SetFocusWidget(GetChild(key.down));
        }
        else if (action == "LEFT")
        {
            if (keyFound)
                SetFocusWidget(GetChild(key.left));
        }
        else if (action == "RIGHT")
        {
            if (keyFound)
                SetFocusWidget(GetChild(key.right));
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(e))
        handled = true;

    return handled;
}

void MythUIVirtualKeyboard::charClicked(void)
{
    if (!GetFocusWidget())
        return;

    KeyDefinition key = m_keyMap.value(GetFocusWidget()->objectName());
    QString c = getKeyText(key);

    if (m_composing)
    {
        if (m_composeStr.isEmpty())
            m_composeStr = c;
        else
        {
            // Produce the composed key.
            for (int i = 0; i < numcomps; i++)
            {
                if ((m_composeStr == comps[i][0]) && (c == comps[i][1]))
                {
                    c = comps[i][2];

                    emit keyPressed(c);

                    if (m_parentEdit)
                    {
                        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, c);
                        m_parentEdit->keyPressEvent(event);
                    }

                    break;
                }
            }

            m_composeStr.clear();
            m_composing = false;
            if (m_compButton)
                m_compButton->SetLocked(false);
        }
    }
    else
    {
        emit keyPressed(c);

        if (m_parentEdit)
        {
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, c);
            m_parentEdit->keyPressEvent(event);
        }

        if (m_shift && !m_lock)
        {
            m_shift = false;
            if (m_shiftLButton)
                m_shiftLButton->SetLocked(false);
            if (m_shiftRButton)
                m_shiftRButton->SetLocked(false);

            updateKeys();
        }
    }
}

void MythUIVirtualKeyboard::shiftClicked(void)
{
    m_shift = !m_shift;

    if (m_shiftLButton)
        m_shiftLButton->SetLocked(m_shift);
    if (m_shiftRButton)
        m_shiftRButton->SetLocked(m_shift);
    if (m_lockButton && m_lock)
    {
        m_lockButton->SetLocked(false);
        m_lock = false;
    }

    updateKeys();
}

void MythUIVirtualKeyboard::delClicked(void)
{
    emit keyPressed("{DELETE}");

    if (m_parentEdit)
    {
        //QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier, "");
        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier, "");
        m_parentEdit->keyPressEvent(event);
    }
}

void MythUIVirtualKeyboard::backClicked(void)
{
    emit keyPressed("{BACK}");

    if (m_parentEdit)
    {
        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier, "");
        m_parentEdit->keyPressEvent(event);
    }
}

void MythUIVirtualKeyboard::lockClicked(void)
{
    m_lock = !m_lock;
    m_shift = m_lock;

    if (m_shiftLButton)
        m_shiftLButton->SetLocked(m_shift);
    if (m_shiftRButton)
        m_shiftRButton->SetLocked(m_shift);

    updateKeys();
}

void MythUIVirtualKeyboard::altClicked(void)
{
    m_alt = !m_alt;

    updateKeys();
}

void MythUIVirtualKeyboard::compClicked(void)
{
    m_composing = !m_composing;
    m_composeStr.clear();
}

void MythUIVirtualKeyboard::returnClicked(void)
{
    if (m_shift)
    {
        emit keyPressed("{NEWLINE}");
        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, m_newlineKey.keyCode, m_newlineKey.modifiers, "");
        m_parentEdit->keyPressEvent(event);
    }
    else
        Close();
}

void MythUIVirtualKeyboard::moveleftClicked(void)
{
    if (m_parentEdit)
    {
        if (m_shift)
        {
            emit keyPressed("{MOVEUP}");
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, m_upKey.keyCode, m_upKey.modifiers, "");
            m_parentEdit->keyPressEvent(event);
        }
        else
        {
            emit keyPressed("{MOVELEFT}");
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, m_leftKey.keyCode, m_leftKey.modifiers,"");
            m_parentEdit->keyPressEvent(event);
        }
    }
}

void MythUIVirtualKeyboard::moverightClicked(void)
{
    if (m_parentEdit)
    {
        if (m_shift)
        {
            emit keyPressed("{MOVEDOWN}");
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, m_downKey.keyCode, m_downKey.modifiers, "");
            m_parentEdit->keyPressEvent(event);
        }
        else
        {
            emit keyPressed("{MOVERIGHT}");
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, m_rightKey.keyCode, m_rightKey.modifiers,"");
            m_parentEdit->keyPressEvent(event);
        }
    }
}

QString MythUIVirtualKeyboard::decodeChar(QString c)
{
    QString res;

    while (c.length() > 0)
    {
        if (c.startsWith("0x"))
        {
            QString sCode = c.left(6);
            bool bOK;
            short nCode = sCode.toShort(&bOK, 16);

            c = c.mid(6);

            if (bOK)
            {
                QChar uc(nCode);
                res += QString(uc);
            }
            else
                LOG(VB_GENERAL, LOG_ERR, QString("bad char code (%1)")
                                .arg(sCode));
        }
        else
        {
            res += c.left(1);
            c = c.mid(1);
        }
    }

    return res;
}

QString MythUIVirtualKeyboard::getKeyText(KeyDefinition key)
{

    if (m_shift)
    {
        if (m_alt)
            return key.altshift;
        else
            return key.shift;
    }

    if (m_alt)
        return key.alt;

    return key.normal;
}

void MythUIVirtualKeyboard::loadEventKeyDefinitions(KeyEventDefinition *keyDef, const QString &action)
{
    QString keylist = GetMythMainWindow()->GetKey("Global", action);
    QStringList keys = keylist.split(',', QString::SkipEmptyParts);
    if (keys.empty())
        return;

    QKeySequence a(keys[0]);
    if (a.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("loadEventKeyDefinitions bad key (%1)").arg(keys[0]));
        return;
    }

    keyDef->keyCode = a[0];

    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    QStringList parts = keys[0].split('+');
    for (int j = 0; j < parts.count(); j++)
    {
        if (parts[j].toUpper() == "CTRL")
            modifiers |= Qt::ControlModifier;
        if (parts[j].toUpper() == "SHIFT")
            modifiers |= Qt::ShiftModifier;
        if (parts[j].toUpper() == "ALT")
            modifiers |= Qt::AltModifier;
        if (parts[j].toUpper() == "META")
            modifiers |= Qt::MetaModifier;
    }

    keyDef->modifiers = modifiers;
}
