// ANSI C headers
#include <cmath> // for ceilf

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QPainter>
#include <QLabel>

using namespace std;

#include "uitypes.h"
#include "mythdialogs.h"
#include "mythcontext.h"
#include "lcddevice.h"
#include "mythdate.h"

#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "x11colors.h"

#include "mythlogging.h"

#ifdef _WIN32
#undef LoadImage
#endif

LayerSet::LayerSet(const QString &name) :
    m_debug(false),
    m_context(-1),
    m_order(-1),
    m_name(name),
    numb_layers(-1),
    allTypes(new vector<UIType *>)
{
}

LayerSet::~LayerSet()
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); ++i)
    {
        UIType *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}

void LayerSet::AddType(UIType *type)
{
    type->SetDebug(m_debug);
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
    bumpUpLayers(type->getOrder());
}

UIType *LayerSet::GetType(const QString &name)
{
    UIType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
}

void LayerSet::bumpUpLayers(int a_number)
{
    if (a_number > numb_layers)
    {
        numb_layers = a_number;
    }
}

void LayerSet::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); ++i)
    {
        if (m_debug)
            LOG(VB_GENERAL, LOG_DEBUG, "-LayerSet::Draw");
        UIType *type = (*i);
        type->Draw(dr, drawlayer, context);
    }
  }
}

void LayerSet::DrawRegion(QPainter *dr, QRect &area, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); ++i)
    {
        if (m_debug)
            LOG(VB_GENERAL, LOG_DEBUG, "-LayerSet::DrawRegion");
        UIType *type = (*i);
        type->DrawRegion(dr, area, drawlayer, context);
    }
  }
}

void LayerSet::SetDrawFontShadow(bool state)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); ++i)
    {
        UIType *type = (*i);
        type->SetDrawFontShadow(state);
    }
}

// **************************************************************

UIType::UIType(const QString &name)
       : QObject(NULL)
{
    setObjectName(name);
    m_parent = NULL;
    m_name = name;
    m_debug = false;
    m_context = -1;
    m_order = -1;
    has_focus = false;
    takes_focus = false;
    screen_area = QRect(0,0,0,0);
    drawFontShadow = true;
    hidden = false;
    m_wmult = 0.0;
    m_hmult = 0.0;
}

void UIType::SetOrder(int order)
{
    m_order = order;
    if (m_parent)
    {
        m_parent->bumpUpLayers(order);
    }
}

UIType::~UIType()
{
}

void UIType::Draw(QPainter *dr, int drawlayer, int context)
{
    dr->end();
    (void)drawlayer;
    (void)context;
}

void UIType::DrawRegion(QPainter *dr, QRect &area, int drawlayer, int context)
{
    (void)dr;
    (void)area;
    (void)drawlayer;
    (void)context;
}

void UIType::SetParent(LayerSet *parent)
{
    m_parent = parent;
}

QString UIType::Name()
{
    return m_name;
}

bool UIType::takeFocus()
{
    if (takes_focus)
    {
        has_focus = true;
        refresh();
        emit takingFocus();
        return true;
    }
    has_focus = false;
    return false;
}

void UIType::looseFocus()
{
    emit loosingFocus();
    has_focus = false;
    refresh();
}

void UIType::calculateScreenArea()
{
    screen_area = QRect(0,0,0,0);
}

void UIType::refresh()
{
    emit requestUpdate(screen_area);
}

void UIType::show()
{
    //
    //  In this base class, this is pretty simple. The method is virtual
    //  though, and inherited fancy classes like UIListBttn can/will/should
    //  set a timer and fade things on a show()
    //

    hidden = false;
    refresh();
}

void UIType::hide()
{
    hidden = true;
    refresh();
}

bool UIType::toggleShow()
{
    if (hidden)
    {
        show();
    }
    else
    {
        hide();
    }

    return !hidden;
}

QString UIType::cutDown(const QString &data, QFont *testFont, bool multiline,
                        int overload_width, int overload_height)
{
    int length = data.length();
    if (length == 0)
        return data;

    int maxwidth = screen_area.width();
    if (overload_width != -1)
        maxwidth = overload_width;
    int maxheight = screen_area.height();
    if (overload_height != -1)
        maxheight = overload_height;

    int justification = Qt::AlignLeft | Qt::TextWordWrap;
    QFontMetrics fm(*testFont);

    int margin = length - 1;
    int index = 0;
    int diff = 0;

    while (margin > 0)
    {
        if (multiline)
            diff = maxheight - fm.boundingRect(0, 0, maxwidth, maxheight,
                                               justification,
                                               data.left(index + margin)
                                               ).height();
        else
            diff = maxwidth - fm.width(data, index + margin);
        if (diff >= 0)
            index += margin;

        margin /= 2;

        if (index + margin >= length - 1)
            margin = (length - 1) - index;
    }

    if (index < length - 1)
    {
        QString tmpStr(data);
        tmpStr.truncate(index);
        if (index >= 3)
            tmpStr.replace(index - 3, 3, "...");
        return tmpStr;
    }

    return data;
}


// ********************************************************************

UIKeyType::UIKeyType(const QString &name)
         : UIType(name)
{
    m_normalImg = m_focusedImg = m_downImg = m_downFocusedImg = NULL;
    m_normalFont = m_focusedFont = m_downFont = m_downFocusedFont = NULL;

    m_pos = QPoint(0, 0);

    m_bDown = false;
    m_bShift = false;
    m_bAlt = false;
    m_bToggle = false;

    takes_focus = true;
    connect(&m_pushTimer, SIGNAL(timeout()), this, SLOT(unPush()));
}

UIKeyType::~UIKeyType()
{
}

void UIKeyType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            fontProp *tempFont;

            // draw the button image
            if (!m_bDown)
            {
                if (!has_focus)
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_normalImg);
                    tempFont = m_normalFont;
                }
                else
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_focusedImg);
                    tempFont = m_focusedFont;
                }
            }
            else
            {
                if (!has_focus)
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_downImg);
                    tempFont = m_downFont;
                }
                else
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_downFocusedImg);
                    tempFont = m_downFocusedFont;
                }
            }

            dr->setFont(tempFont->face);

            // draw the button text
            QString text;

            if (!m_bShift)
            {
                if (!m_bAlt)
                    text = m_normalChar;
                else
                    text = m_altChar;
            }
            else
            {
                if (!m_bAlt)
                    text = m_shiftChar;
                else
                    text = m_shiftAltChar;
            }

            if (drawFontShadow &&
                (tempFont->shadowOffset.x() != 0 ||
                 tempFont->shadowOffset.y() != 0))
            {
                dr->setBrush(tempFont->dropColor);
                dr->setPen(QPen(tempFont->dropColor, (int)(2 * m_wmult)));
                dr->drawText(m_pos.x() + tempFont->shadowOffset.x(),
                             m_pos.y() + tempFont->shadowOffset.y(),
                             m_area.width(), m_area.height(),
                             Qt::AlignCenter, text);
            }

            dr->setBrush(tempFont->color);
            dr->setPen(QPen(tempFont->color, (int)(2 * m_wmult)));
            dr->drawText(m_pos.x(), m_pos.y(), m_area.width(), m_area.height(),
                         Qt::AlignCenter, text);
        }
    }
}

void UIKeyType::SetImages(QPixmap *normal, QPixmap *focused,
                          QPixmap *down, QPixmap *downFocused)
{
    m_normalImg = normal;
    m_focusedImg = focused;
    m_downImg = down;
    m_downFocusedImg = downFocused;
}

void UIKeyType::SetDefaultImages(QPixmap *normal, QPixmap *focused,
                                 QPixmap *down, QPixmap *downFocused)
{
    if (!m_normalImg) m_normalImg = normal;
    if (!m_focusedImg) m_focusedImg = focused;
    if (!m_downImg) m_downImg = down;
    if (!m_downFocusedImg) m_downFocusedImg = downFocused;
}

void UIKeyType::SetFonts(fontProp *normal, fontProp *focused,
                         fontProp *down, fontProp *downFocused)
{
    m_normalFont = normal;
    m_focusedFont = focused;
    m_downFont = down;
    m_downFocusedFont = downFocused;
}

void UIKeyType::SetDefaultFonts(fontProp *normal, fontProp *focused,
                                fontProp *down, fontProp *downFocused)
{
    if (!m_normalFont) m_normalFont = normal;
    if (!m_focusedFont) m_focusedFont = focused;
    if (!m_downFont) m_downFont = down;
    if (!m_downFocusedFont) m_downFocusedFont = downFocused;
}

void UIKeyType::SetChars(QString normal, QString shift, QString alt,
                         QString shiftAlt)
{
    m_normalChar = decodeChar(normal);
    m_shiftChar = decodeChar(shift);
    m_altChar = decodeChar(alt);
    m_shiftAltChar = decodeChar(shiftAlt);
}

QString UIKeyType::GetChar()
{
    if (!m_bShift && !m_bAlt)
        return m_normalChar;
    else if (m_bShift && !m_bAlt)
        return m_shiftChar;
    else if (!m_bShift && m_bAlt)
        return m_altChar;
    else if (m_bShift && m_bAlt)
        return m_shiftAltChar;

    return m_normalChar;
}

QString UIKeyType::decodeChar(QString c)
{
    QString res = "";

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
                LOG(VB_GENERAL, LOG_ERR,
                         QString("UIKeyType::decodeChar - bad char code (%1)")
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

void UIKeyType::SetMoves(QString moveLeft, QString moveRight, QString moveUp,
                         QString moveDown)
{
    m_moveLeft = moveLeft;
    m_moveRight = moveRight;
    m_moveUp = moveUp;
    m_moveDown = moveDown;
}

QString UIKeyType::GetMove(QString direction)
{
    QString res = m_moveLeft;

    if (direction == "Up")
        res = m_moveUp;
    else if (direction == "Down")
        res = m_moveDown;
    else if (direction == "Right")
        res = m_moveRight;

    return res;
}

void UIKeyType::calculateScreenArea()
{
    if (!m_normalImg)
        return;

    int width = m_normalImg->width();
    int height = m_normalImg->height();

    QRect r = QRect(m_pos.x(), m_pos.y(), width, height);
    r.translate(m_parent->GetAreaRect().left(), m_parent->GetAreaRect().top());
    screen_area = r;
    m_area = r;
}

void UIKeyType::SetShiftState(bool sh, bool ag)
{
    m_bShift = sh;
    m_bAlt = ag;
    refresh();
}

void UIKeyType::push()
{
    if (m_bToggle)
    {
        m_bDown = !m_bDown;
        refresh();
        emit pushed();

        return;
    }

    if (m_bDown)
        return;

    m_bDown = true;
    m_pushTimer.setSingleShot(true);
    m_pushTimer.start(300);
    refresh();
    emit pushed();
}

void UIKeyType::unPush()
{
    if (!m_bToggle)
    {
        m_bDown = false;
        refresh();
    }
}

// ********************************************************************

const int numcomps = 95;

const QString comps[numcomps][3] = {
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

UIKeyboardType::UIKeyboardType(const QString &name, int order)
                    : UIType(name)
{
    m_order = order;
    m_container = NULL;
    m_parentEdit = NULL;
    m_parentDialog = NULL;
    m_bInitalized = false;
    m_focusedKey = m_doneKey = m_altKey = m_lockKey = NULL;
    m_shiftRKey = m_shiftLKey = NULL;

    m_bCompTrap = false;
    m_comp1 = "";
}


UIKeyboardType::~UIKeyboardType()
{
    if (m_container)
        delete m_container;
}

void UIKeyboardType::init()
{
    m_bInitalized = true;

    KeyList::iterator it = m_keyList.begin();
    for (; it != m_keyList.end(); ++it)
    {
        UIKeyType *key = *it;
        if (key->GetType() == "char")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( charKey() ) );
        }
        else if (key->GetType() == "shift")
        {
            if (!m_shiftLKey)
            {
                connect(key, SIGNAL( pushed() ), this, SLOT( shiftLOnOff() ) );
                m_shiftLKey = key;
                m_shiftLKey->SetToggleKey(true);
            }
            else
            {
                connect(key, SIGNAL( pushed() ), this, SLOT( shiftROnOff() ) );
                m_shiftRKey = key;
                m_shiftRKey->SetToggleKey(true);
            }
        }
        else if (key->GetType() == "del")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( delKey() ) );
        }
        else if (key->GetType() == "back")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( backspaceKey() ) );
        }
        else if (key->GetType() == "lock")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( lockOnOff() ) );
            m_lockKey = key;
            m_lockKey->SetToggleKey(true);
        }
        else if (key->GetType() == "done")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( close() ) );
            m_doneKey = key;
        }
        else if (key->GetType() == "moveleft")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( leftCursor() ) );
        }
        else if (key->GetType() == "moveright")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( rightCursor() ) );
        }
        else if (key->GetType() == "comp")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( compOnOff() ) );
        }
        else if (key->GetType() == "alt")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( altGrOnOff() ) );
            m_altKey = key;
            m_altKey->SetToggleKey(true);
        }
    }
}

void UIKeyboardType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (!m_bInitalized)
        init();

    if (hidden)
        return;

    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            dr = dr;
        }
    }
}

void UIKeyboardType::calculateScreenArea()
{
    QRect r = m_area;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

void UIKeyboardType::leftCursor()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->cursorBackward(m_shiftLKey->IsOn());
    }
    else if (m_parentEdit->inherits("QTextEdit"))
    {
        QTextEdit *par = (QTextEdit *)m_parentEdit;
        par->textCursor().movePosition(
            QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor);
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::rightCursor()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->cursorForward(m_shiftLKey->IsOn());
    }
    else if (m_parentEdit->inherits("QTextEdit"))
    {
        QTextEdit *par = (QTextEdit *)m_parentEdit;
        par->textCursor().movePosition(
            QTextCursor::NextCharacter, QTextCursor::MoveAnchor);
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::backspaceKey()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->backspace();
    }
    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
    {
        MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
        par->backspace();
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::delKey()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->del();
    }
    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
    {
        MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
        par->del();
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::altGrOnOff()
{
    if (m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
        m_lockKey->SetOn(false);
    }
    updateButtons();
}

void UIKeyboardType::compOnOff()
{
    m_bCompTrap = !m_bCompTrap;
    m_comp1 = "";
}

void UIKeyboardType::charKey()
{
    if (m_focusedKey && m_focusedKey->GetType() == "char")
    {
        insertChar(m_focusedKey->GetChar());
        shiftOff();
    }
}

void UIKeyboardType::insertChar(QString c)
{
    if (!m_bCompTrap)
    {
        if (m_parentEdit->inherits("QLineEdit"))
        {
            QLineEdit *par = (QLineEdit *)m_parentEdit;
            par->insert(c);
        }
        else if (m_parentEdit->inherits("MythRemoteLineEdit"))
        {
            MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
            par->insert(c);
        }
        else
        {
            QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier,
                                           c, false, c.length());
            QCoreApplication::postEvent(m_parentEdit, key);
        }
    }
    else
    {
        if (m_comp1.isEmpty()) m_comp1 = c;
        else
        {
            // Produce the composed key.
            for (int i=0; i<numcomps; i++)
            {
                if ((m_comp1 == comps[i][0]) && (c == comps[i][1]))
                {
                    if (m_parentEdit->inherits("QLineEdit"))
                    {
                        QLineEdit *par = (QLineEdit *)m_parentEdit;
                        par->insert(comps[i][2]);
                    }
                    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
                    {
                        MythRemoteLineEdit *par =
                            (MythRemoteLineEdit *)m_parentEdit;
                        par->insert(comps[i][2]);
                    }
                    else
                    {
                        QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, 0,
                                                       Qt::NoModifier,
                                                       comps[i][2], false,
                                                       comps[i][2].length());
                        QCoreApplication::postEvent(m_parentEdit, key);
                    }

                    break;
                }
            }
            // Reset compTrap.
            m_comp1 = "";
            m_bCompTrap = false;
        }
    }
}

void UIKeyboardType::lockOnOff()
{
    if (m_lockKey->IsOn())
    {
        if (!(m_altKey && m_altKey->IsOn()))
        {
            m_shiftLKey->SetOn(true);
            if (m_shiftRKey) m_shiftRKey->SetOn(true);
        }
    }
    else
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
    }
    updateButtons();
}

void UIKeyboardType::shiftOff()
{
    if (!m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
    }
    updateButtons();
}

void UIKeyboardType::close(void)
{
    if (!m_parentDialog)
        return;

    m_parentDialog->done(kDialogCodeAccepted);
}

void UIKeyboardType::updateButtons()
{
    bool bShift = m_shiftLKey->IsOn();
    bool bAlt = (m_altKey ? m_altKey->IsOn() : false);

    KeyList::iterator it = m_keyList.begin();
    for (; it != m_keyList.end(); ++it)
        (*it)->SetShiftState(bShift, bAlt);
}

void UIKeyboardType::shiftLOnOff()
{
    if (m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
        m_lockKey->SetOn(false);
    }
    else if (m_shiftRKey) m_shiftRKey->SetOn(m_shiftLKey->IsOn());

    updateButtons();
}

void UIKeyboardType::shiftROnOff()
{
    if (!m_shiftRKey) return;

    if (m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
        m_lockKey->SetOn(false);
    }
    else m_shiftLKey->SetOn(m_shiftRKey->IsOn());

    updateButtons();
}

void UIKeyboardType::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            moveUp();
        }
        else if (action == "DOWN")
        {
            moveDown();
        }
        else if (action == "LEFT")
        {
            moveLeft();
        }
        else if (action == "RIGHT")
        {
            moveRight();
        }
        else if (action == "SELECT")
            m_focusedKey->activate();
        else
            handled = false;
    }

    if (!handled)
    {
        QKeyEvent *key = new QKeyEvent(e->type(), e->key(), e->modifiers(),
                                       e->text(), e->isAutoRepeat(),
                                       e->count());
        QCoreApplication::postEvent(m_parentEdit, key);
        m_parentEdit->setFocus();
    }
}

void UIKeyboardType::moveUp()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Up"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

void UIKeyboardType::moveDown()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Down"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

void UIKeyboardType::moveLeft()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Left"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

void UIKeyboardType::moveRight()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Right"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

UIKeyType *UIKeyboardType::findKey(QString keyName)
{
    KeyList::const_iterator it = m_keyList.begin();
    for (; it != m_keyList.end(); ++it)
    {
        if ((*it)->getName() == keyName)
            return (*it);
    }
    return NULL;
}

void UIKeyboardType::AddKey(UIKeyType *key)
{
    m_keyList.append(key);

    if (key->GetType().toLower() == "done")
    {
        key->takeFocus();
        m_focusedKey = key;
    }
}

// vim:set sw=4 expandtab:
