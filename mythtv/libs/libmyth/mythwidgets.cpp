#include <iostream>
using namespace std;

#include <QStyle>
#include <QPainter>
#include <QCursor>
#include <QLayout>
#include <QKeyEvent>
#include <QHideEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QEvent>

#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythdate.h"
#include "mythdialogs.h"
#include "mythlogging.h"
#include "mythmainwindow.h"

MythComboBox::MythComboBox(bool rw, QWidget *parent, const char *name) :
    QComboBox(parent),
    helptext(QString::null), AcceptOnSelect(false), step(1)
{
    setObjectName(name);
    setEditable(rw);
}

MythComboBox::~MythComboBox()
{
    Teardown();
}

void MythComboBox::deleteLater(void)
{
    Teardown();
    QComboBox::deleteLater();
}

void MythComboBox::Teardown(void)
{
}

void MythComboBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythComboBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false, updated = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    if (!handled)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
            {
                focusNextPrevChild(false);
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
            }
            else if (action == "LEFT")
            {
                if (currentIndex() == 0)
                    setCurrentIndex(count()-1);
                else if (count() > 0)
                    setCurrentIndex((currentIndex() - 1) % count());
                updated = true;
            }
            else if (action == "RIGHT")
            {
                if (count() > 0)
                    setCurrentIndex((currentIndex() + 1) % count());
                updated = true;
            }
            else if (action == "PAGEDOWN")
            {
                if (currentIndex() == 0)
                    setCurrentIndex(count() - (step % count()));
                else if (count() > 0)
                    setCurrentIndex(
                        (currentIndex() + count() - (step % count())) % count());
                updated = true;
            }
            else if (action == "PAGEUP")
            {
                if (count() > 0)
                    setCurrentIndex(
                        (currentIndex() + (step % count())) % count());
                updated = true;
            }
            else if (action == "SELECT" && AcceptOnSelect)
                emit accepted(currentIndex());

            else
                handled = false;
        }
    }

    if (updated)
    {
        emit activated(currentIndex());
        emit activated(itemText(currentIndex()));
    }
    if (!handled)
    {
        if (isEditable())
            QComboBox::keyPressEvent(e);
        else
            e->ignore();
    }
}

void MythComboBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    if (lineEdit())
        lineEdit()->setPalette(palette);

    QComboBox::focusInEvent(e);
}

void MythComboBox::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    if (lineEdit())
    {
        lineEdit()->setPalette(QPalette());

        // commit change if the user was editing an entry
        QString curText = currentText();
        int i;
        bool foundItem = false;

        for(i = 0; i < count(); i++)
            if (curText == itemText(i))
                foundItem = true;

        if (!foundItem)
        {
            insertItem(curText);
            setCurrentIndex(count()-1);
        }
    }

    QComboBox::focusOutEvent(e);
}

void MythCheckBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;

    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for ( int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            focusNextPrevChild(false);
        else if (action == "DOWN")
            focusNextPrevChild(true);
        else if (action == "LEFT" || action == "RIGHT" || action == "SELECT")
            toggle();
        else
            handled = false;
    }

    if (!handled)
        e->ignore();
}

void MythCheckBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythCheckBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);
    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QCheckBox::focusInEvent(e);
}

void MythCheckBox::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QCheckBox::focusOutEvent(e);
}

void MythRadioButton::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            focusNextPrevChild(false);
        else if (action == "DOWN")
            focusNextPrevChild(true);
        else if (action == "LEFT" || action == "RIGHT")
            toggle();
        else
            handled = false;
    }

    if (!handled)
        e->ignore();
}

void MythRadioButton::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythRadioButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QRadioButton::focusInEvent(e);
}

void MythRadioButton::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QRadioButton::focusOutEvent(e);
}


void MythSpinBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythSpinBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            focusNextPrevChild(false);
        else if (action == "DOWN")
            focusNextPrevChild(true);
        else if (action == "LEFT")
            allowsinglestep ? setValue(value()-1) : stepDown();
        else if (action == "RIGHT")
            allowsinglestep ? setValue(value()+1) : stepUp();
        else if (action == "PAGEDOWN")
            stepDown();
        else if (action == "PAGEUP")
            stepUp();
        else if (action == "SELECT")
            handled = true;
        else
            handled = false;
    }

    if (!handled)
        QSpinBox::keyPressEvent(e);
}

void MythSpinBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QSpinBox::focusInEvent(e);
}

void MythSpinBox::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QSpinBox::focusOutEvent(e);
}

void MythSlider::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            focusNextPrevChild(false);
        else if (action == "DOWN")
            focusNextPrevChild(true);
        else if (action == "LEFT")
            setValue(value() - singleStep());
        else if (action == "RIGHT")
            setValue(value() + singleStep());
        else if (action == "SELECT")
            handled = true;
        else
            handled = false;
    }

    if (!handled)
        QSlider::keyPressEvent(e);
}

void MythSlider::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythSlider::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QSlider::focusInEvent(e);
}

void MythSlider::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QSlider::focusOutEvent(e);
}

MythLineEdit::MythLineEdit(QWidget *parent, const char *name) :
    QLineEdit(parent),
    helptext(QString::null), rw(true)
{
    setObjectName(name);
}

MythLineEdit::MythLineEdit(
    const QString &contents, QWidget *parent, const char *name) :
    QLineEdit(contents, parent),
    helptext(QString::null), rw(true)
{
    setObjectName(name);
}

MythLineEdit::~MythLineEdit()
{
    Teardown();
}

void MythLineEdit::deleteLater(void)
{
    Teardown();
    QLineEdit::deleteLater();
}

void MythLineEdit::Teardown(void)
{
}

void MythLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    if (!handled)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "SELECT" && e->text().isEmpty() )
                e->ignore();
            else
                handled = false;
        }
    }

    if (!handled)
        if (rw || e->key() == Qt::Key_Escape || e->key() == Qt::Key_Left
               || e->key() == Qt::Key_Return || e->key() == Qt::Key_Right)
            QLineEdit::keyPressEvent(e);
}

void MythLineEdit::setText(const QString &text)
{
    // Don't mess with the cursor position; it causes
    // counter-intuitive behaviour due to interactions with the
    // communication with the settings stuff

    int pos = cursorPosition();
    QLineEdit::setText(text);
    setCursorPosition(pos);
}

QString MythLineEdit::text(void)
{
    return QLineEdit::text();
}

void MythLineEdit::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QLineEdit::focusInEvent(e);
}

void MythLineEdit::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QLineEdit::focusOutEvent(e);
}

void MythLineEdit::hideEvent(QHideEvent *e)
{
    QLineEdit::hideEvent(e);
}

void MythLineEdit::mouseDoubleClickEvent(QMouseEvent *e)
{
    QLineEdit::mouseDoubleClickEvent(e);
}

MythRemoteLineEdit::MythRemoteLineEdit(QWidget * parent, const char *name) :
    QTextEdit(parent)
{
    setObjectName(name);
    my_font = NULL;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(const QString & contents,
                                       QWidget * parent, const char *name) :
    QTextEdit(parent)
{
    setObjectName(name);
    my_font = NULL;
    m_lines = 1;
    this->Init();
    setText(contents);
}

MythRemoteLineEdit::MythRemoteLineEdit(QFont *a_font, QWidget * parent,
                                       const char *name) :
    QTextEdit(parent)
{
    setObjectName(name);
    my_font = a_font;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(int lines, QWidget * parent,
                                       const char *name) :
    QTextEdit(parent)
{
    setObjectName(name);
    my_font = NULL;
    m_lines = lines;
    this->Init();
}

void MythRemoteLineEdit::Init()
{
    //  Bunch of default values
    cycle_timer = new QTimer();
    shift = false;
    active_cycle = false;
    current_choice = "";
    current_set = "";

    cycle_time = 3000;

    pre_cycle_text_before_cursor = "";
    pre_cycle_text_after_cursor = "";

    setCharacterColors(
        QColor(100, 100, 100), QColor(0, 255, 255), QColor(255, 0, 0));

    //  Try and make sure it doesn't ever change
    setWordWrapMode(QTextOption::NoWrap);

    if (my_font)
        setFont(*my_font);

    QFontMetrics fontsize(font());

    setMinimumHeight(fontsize.height() * 5 / 4);
    setMaximumHeight(fontsize.height() * m_lines * 5 / 4);

    connect(cycle_timer, SIGNAL(timeout()), this, SLOT(endCycle()));
}


void MythRemoteLineEdit::setCharacterColors(
    QColor unselected, QColor selected, QColor special)
{
    col_unselected = unselected;
    hex_unselected = QString("%1%2%3")
        .arg(col_unselected.red(),   2, 16, QLatin1Char('0'))
        .arg(col_unselected.green(), 2, 16, QLatin1Char('0'))
        .arg(col_unselected.blue(),  2, 16, QLatin1Char('0'));

    col_selected = selected;
    hex_selected = QString("%1%2%3")
        .arg(col_selected.red(),   2, 16, QLatin1Char('0'))
        .arg(col_selected.green(), 2, 16, QLatin1Char('0'))
        .arg(col_selected.blue(),  2, 16, QLatin1Char('0'));

    col_special = special;
    hex_special = QString("%1%2%3")
        .arg(col_special.red(),   2, 16, QLatin1Char('0'))
        .arg(col_special.green(), 2, 16, QLatin1Char('0'))
        .arg(col_special.blue(),  2, 16, QLatin1Char('0'));
}

void MythRemoteLineEdit::startCycle(QString current_choice, QString set)
{
    if (active_cycle)
    {
        LOG(VB_GENERAL, LOG_ALERT, 
                 "startCycle() called, but edit is already in a cycle.");
        return;
    }

    cycle_timer->setSingleShot(true);
    cycle_timer->start(cycle_time);
    active_cycle = true;

    QTextCursor pre_cycle_cursor = textCursor();

    QTextCursor upto_cursor_sel = pre_cycle_cursor;
    upto_cursor_sel.movePosition(
        QTextCursor::NoMove, QTextCursor::MoveAnchor);
    upto_cursor_sel.movePosition(
        QTextCursor::Start,  QTextCursor::KeepAnchor);
    pre_cycle_text_before_cursor = upto_cursor_sel.selectedText();

    QTextCursor from_cursor_sel = pre_cycle_cursor;
    from_cursor_sel.movePosition(
        QTextCursor::NoMove, QTextCursor::MoveAnchor);
    from_cursor_sel.movePosition(
        QTextCursor::End,  QTextCursor::KeepAnchor);
    pre_cycle_text_after_cursor = from_cursor_sel.selectedText();

    pre_cycle_pos = pre_cycle_text_before_cursor.length();

    updateCycle(current_choice, set); // Show the user their options
}

void MythRemoteLineEdit::updateCycle(QString current_choice, QString set)
{
    int index;
    QString aString, bString;

    //  Show the characters in the current set being cycled
    //  through, with the current choice in a different color. If the current
    //  character is uppercase X (interpreted as destructive
    //  backspace) or an underscore (interpreted as a space)
    //  then show these special cases in yet another color.

    if (shift)
    {
        set = set.toUpper();
        current_choice = current_choice.toUpper();
    }

    bString  = "<B>";
    if (current_choice == "_" || current_choice == "X")
    {
        bString += "<FONT COLOR=\"#";
        bString += hex_special;
        bString += "\">";
        bString += current_choice;
        bString += "</FONT>";
    }
    else
    {
        bString += "<FONT COLOR=\"#";
        bString += hex_selected;
        bString += "\">";
        bString += current_choice;
        bString += "</FONT>";
    }
    bString += "</B>";

    index = set.indexOf(current_choice);
    int length = set.length();
    if (index < 0 || index > length)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("MythRemoteLineEdit passed a choice of \"%1"
                         "\" which is not in set \"%2\"")
                     .arg(current_choice).arg(set));
        setText("????");
        return;
    }
    else
    {
        set.replace(index, current_choice.length(), bString);
    }

    QString esc_upto =  pre_cycle_text_before_cursor;
    QString esc_from =  pre_cycle_text_after_cursor;

    esc_upto.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");
    esc_from.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");

    aString = esc_upto;
    aString += "<FONT COLOR=\"#";
    aString += hex_unselected;
    aString += "\">[";
    aString += set;
    aString += "]</FONT>";
    aString += esc_from;
    setHtml(aString);

    QTextCursor tmp = textCursor();
    tmp.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    tmp.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                     pre_cycle_pos + set.length());
    setTextCursor(tmp);
    update();

    if (current_choice == "X" && !pre_cycle_text_before_cursor.isEmpty())
    {
        //  If current selection is delete, select the character to be deleted
        QTextCursor tmp = textCursor();
        tmp.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        tmp.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                         pre_cycle_pos - 1);
        tmp.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        setTextCursor(tmp);
    }
    else
    {
        // Restore original cursor location
        QTextCursor tmp = textCursor();
        tmp.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        tmp.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                         pre_cycle_pos);
        setTextCursor(tmp);
    }
}

void MythRemoteLineEdit::endCycle(bool select)
{
    if (!active_cycle)
        return;

    QString tmpString = "";
    int pos = pre_cycle_pos;

    //  The timer ran out or the user pressed a key
    //  outside of the current set of choices
    if (!select)
    {
        tmpString = pre_cycle_text_before_cursor;
    }
    else if (current_choice == "X") // destructive backspace
    {
        if (!pre_cycle_text_before_cursor.isEmpty())
        {
            tmpString = pre_cycle_text_before_cursor.left(
                pre_cycle_text_before_cursor.length() - 1);
            pos--;
        }
    }
    else
    {
        current_choice = (current_choice == "_") ? " " : current_choice;
        current_choice = (shift) ? current_choice.toUpper() : current_choice;

        tmpString  = pre_cycle_text_before_cursor;
        tmpString += current_choice;
        pos++;
    }

    tmpString += pre_cycle_text_after_cursor;

    setPlainText(tmpString);

    QTextCursor tmpCursor = textCursor();
    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    tmpCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
    setTextCursor(tmpCursor);

    active_cycle                 = false;
    current_choice               = "";
    current_set                  = "";
    pre_cycle_text_before_cursor = "";
    pre_cycle_text_after_cursor  = "";

    if (select)
        emit textChanged(toPlainText());
}

void MythRemoteLineEdit::setText(const QString &text)
{
    QTextEdit::clear();
    QTextEdit::insertPlainText(text);
}

QString MythRemoteLineEdit::text(void)
{
    return QTextEdit::toPlainText();
}

void MythRemoteLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    if (!handled)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
            {
                endCycle();
                // Need to call very base one because
                // QTextEdit reimplements it to tab
                // through links (even if you're in
                // PlainText Mode !!)
                QWidget::focusNextPrevChild(false);
                emit tryingToLooseFocus(false);
            }
            else if (action == "DOWN")
            {
                endCycle();
                QWidget::focusNextPrevChild(true);
                emit tryingToLooseFocus(true);
            }
            else if ((action == "ESCAPE") && active_cycle)
            {
                endCycle(false);
            }
            else
                handled = false;
        }
    }

    if (handled)
        return;

    switch (e->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            handled = true;
            endCycle();
            e->ignore();
            break;

            //  Only eat Qt::Key_Space if we are in a cycle

        case Qt::Key_Space:
            if (active_cycle)
            {
                handled = true;
                endCycle();
                e->ignore();
            }
            break;

            //  If you want to mess around with other ways to allocate
            //  key presses you can just add entries between the quotes

        case Qt::Key_1:
            cycleKeys("_X%-/.?()1");
            handled = true;
            break;

        case Qt::Key_2:
            cycleKeys("abc2");
            handled = true;
            break;

        case Qt::Key_3:
            cycleKeys("def3");
            handled = true;
            break;

        case Qt::Key_4:
            cycleKeys("ghi4");
            handled = true;
            break;

        case Qt::Key_5:
            cycleKeys("jkl5");
            handled = true;
            break;

        case Qt::Key_6:
            cycleKeys("mno6");
            handled = true;
            break;

        case Qt::Key_7:
            cycleKeys("pqrs7");
            handled = true;
            break;

        case Qt::Key_8:
            cycleKeys("tuv8");
            handled = true;
            break;

        case Qt::Key_9:
            cycleKeys("wxyz90");
            handled = true;
            break;

        case Qt::Key_0:
            toggleShift();
            handled = true;
            break;
    }

    if (!handled)
    {
        endCycle();
        QTextEdit::keyPressEvent(e);
        emit textChanged(toPlainText());
    }
}

void MythRemoteLineEdit::setCycleTime(float desired_interval)
{
    if (desired_interval < 0.5 || desired_interval > 10.0)
    {
        LOG(VB_GENERAL, LOG_ALERT, 
                 QString("cycle interval of %1 milliseconds ")
                     .arg((int) (desired_interval * 1000)) +
                 "\n\t\t\tis outside of the allowed range of "
                 "500 to 10,000 milliseconds");
        return;
    }

    cycle_time = (int) (desired_interval * 1000);
}

void MythRemoteLineEdit::cycleKeys(QString cycle_list)
{
    int index;

    if (active_cycle)
    {
        if (cycle_list == current_set)
        {
            //  Regular movement through existing set
            cycle_timer->start(cycle_time);
            index = current_set.indexOf(current_choice);
            int length = current_set.length();
            if (index + 1 >= length)
            {
                index = -1;
            }
            current_choice = current_set.mid(index + 1, 1);
            updateCycle(current_choice, current_set);
        }
        else
        {
            //  Previous cycle was still active, but user moved
            //  to another keypad key
            endCycle();
            current_choice = cycle_list.left(1);
            current_set = cycle_list;
            cycle_timer->start(cycle_time);
            startCycle(current_choice, current_set);
        }
    }
    else
    {
        //  First press with no cycle of any type active
        current_choice = cycle_list.left(1);
        current_set = cycle_list;
        startCycle(current_choice, current_set);
    }
}

void MythRemoteLineEdit::toggleShift()
{
    //  Toggle uppercase/lowercase and
    //  update the cycle display if it's
    //  active
    QString temp_choice = current_choice;
    QString temp_set = current_set;

    if (shift)
    {
        shift = false;
    }
    else
    {
        shift = true;
        temp_choice = current_choice.toUpper();
        temp_set = current_set.toUpper();
    }
    if (active_cycle)
    {
        updateCycle(temp_choice, temp_set);
    }
}

void MythRemoteLineEdit::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythRemoteLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();    //perhaps we need to save a playlist?

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QTextEdit::focusInEvent(e);
}

void MythRemoteLineEdit::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    emit lostFocus();
    QTextEdit::focusOutEvent(e);
}

MythRemoteLineEdit::~MythRemoteLineEdit()
{
    Teardown();
}

void MythRemoteLineEdit::deleteLater(void)
{
    Teardown();
    QTextEdit::deleteLater();
}

void MythRemoteLineEdit::Teardown(void)
{
    if (cycle_timer)
    {
        cycle_timer->disconnect();
        cycle_timer->deleteLater();
        cycle_timer = NULL;
    }
}

void MythRemoteLineEdit::insert(QString text)
{
    QTextEdit::insertPlainText(text);
    emit textChanged(toPlainText());
}

void MythRemoteLineEdit::del()
{
    textCursor().deleteChar();
    emit textChanged(toPlainText());
}

void MythRemoteLineEdit::backspace()
{
    textCursor().deletePreviousChar();
    emit textChanged(toPlainText());
}

MythPushButton::MythPushButton(const QString &ontext, const QString &offtext,
                               QWidget *parent, bool isOn)
                               : QPushButton(ontext, parent)
{
    onText = ontext;
    offText = offtext;

    setCheckable(true);

    if (isOn)
        setText(onText);
    else
        setText(offText);

    setChecked(isOn);
}

void MythPushButton::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythPushButton::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    keyPressActions.clear();

    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled && !actions.isEmpty())
    {
        keyPressActions = actions;

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "SELECT")
            {
                if (isCheckable())
                    toggleText();
                setDown(true);
                emit pressed();
                handled = true;
            }
        }
    }

    if (!handled)
        QPushButton::keyPressEvent(e);
}

void MythPushButton::keyReleaseEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions = keyPressActions;
    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "SELECT")
        {
            QKeyEvent tempe(QEvent::KeyRelease, Qt::Key_Space,
                            Qt::NoModifier, " ");
            QPushButton::keyReleaseEvent(&tempe);
            handled = true;
        }
    }

    if (!handled)
        QPushButton::keyReleaseEvent(e);
}

void MythPushButton::toggleText(void)
{
    if (!isCheckable())
        return;

    if (isChecked())
        setText(offText);
    else
        setText(onText);
}

void MythPushButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);
    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QPushButton::focusInEvent(e);
}

void MythPushButton::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QPushButton::focusOutEvent(e);
}

MythListBox::MythListBox(QWidget *parent, const QString &name) :
    QListWidget(parent)
{
    setObjectName(name);
    connect(this, SIGNAL(itemSelectionChanged()),
            this, SLOT(HandleItemSelectionChanged()));
}

void MythListBox::ensurePolished(void) const
{
    QListWidget::ensurePolished();

    QPalette pal = palette();
    QPalette::ColorRole  nR = QPalette::Highlight;
    QPalette::ColorGroup oA = QPalette::Active;
    QPalette::ColorRole  oR = QPalette::Button;
    pal.setColor(QPalette::Active,   nR, pal.color(oA,oR));
    pal.setColor(QPalette::Inactive, nR, pal.color(oA,oR));
    pal.setColor(QPalette::Disabled, nR, pal.color(oA,oR));

    const_cast<MythListBox*>(this)->setPalette(pal);
}

void MythListBox::setCurrentItem(const QString& matchText, bool caseSensitive,
                                 bool partialMatch)
{
    for (unsigned i = 0 ; i < (unsigned)count() ; ++i)
    {
        if (partialMatch)
        {
            if (caseSensitive)
            {
                if (text(i).startsWith(matchText))
                {
                    setCurrentRow(i);
                    break;
                }
            }
            else
            {
                if (text(i).toLower().startsWith(matchText.toLower()))
                {
                    setCurrentRow(i);
                    break;
                }
            }
        }
        else
        {
            if (caseSensitive)
            {
                if (text(i) == matchText)
                {
                    setCurrentRow(i);
                    break;
                }
            }
            else
            {
                if (text(i).toLower() == matchText.toLower())
                {
                    setCurrentRow(i);
                    break;
                }
            }
        }
    }
}

void MythListBox::HandleItemSelectionChanged(void)
{
    QList<QListWidgetItem*> items = QListWidget::selectedItems();
    int row = getIndex(items);
    if (row >= 0)
        emit highlighted(row);
}

void MythListBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "UP" || action == "DOWN" || action == "PAGEUP" ||
            action == "PAGEDOWN" || action == "LEFT" || action == "RIGHT")
        {
            int key;
            if (action == "UP")
            {
                // Qt::Key_Up at top of list allows focus to move to other widgets
                if (currentItem() == 0)
                {
                    focusNextPrevChild(false);
                    handled = true;
                    continue;
                }

                key = Qt::Key_Up;
            }
            else if (action == "DOWN")
            {
                // Qt::Key_down at bottom of list allows focus to move to other widgets
                if (currentRow() == (int) count() - 1)
                {
                    focusNextPrevChild(true);
                    handled = true;
                    continue;
                }

                key = Qt::Key_Down;
            }
            else if (action == "LEFT")
            {
                focusNextPrevChild(false);
                handled = true;
                continue;
            }
            else if (action == "RIGHT")
            {
                focusNextPrevChild(true);
                handled = true;
                continue;
            }
            else if (action == "PAGEUP")
                key = Qt::Key_PageUp;
            else if (action == "PAGEDOWN")
                key = Qt::Key_PageDown;
            else
                key = Qt::Key_unknown;

            QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier);
            QListWidget::keyPressEvent(&ev);
            handled = true;
        }
        else if (action == "0" || action == "1" || action == "2" ||
                    action == "3" || action == "4" || action == "5" ||
                    action == "6" || action == "7" || action == "8" ||
                    action == "9")
        {
            int percent = action.toInt() * 10;
            int nextItem = percent * count() / 100;
            if (!itemVisible(nextItem))
                setTopRow(nextItem);
            setCurrentRow(nextItem);
            handled = true;
        }
        else if (action == "PREVVIEW")
        {
            int nextItem = currentRow();
            if (nextItem > 0)
                nextItem--;
            while (nextItem > 0 && text(nextItem)[0] == ' ')
                nextItem--;
            if (!itemVisible(nextItem))
                setTopRow(nextItem);
            setCurrentRow(nextItem);
            handled = true;
        }
        else if (action == "NEXTVIEW")
        {
            int nextItem = currentRow();
            if (nextItem < (int)count() - 1)
                nextItem++;
            while (nextItem < (int)count() - 1 && text(nextItem)[0] == ' ')
                nextItem++;
            if (!itemVisible(nextItem))
                setTopRow(nextItem);
            setCurrentRow(nextItem);
            handled = true;
        }
        else if (action == "MENU")
            emit menuButtonPressed(currentRow());
        else if (action == "EDIT")
            emit editButtonPressed(currentRow());
        else if (action == "DELETE")
            emit deleteButtonPressed(currentRow());
        else if (action == "SELECT")
            emit accepted(currentRow());
    }

    if (!handled)
        e->ignore();
}

void MythListBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythListBox::focusOutEvent(QFocusEvent *e)
{
    QPalette pal = palette();
    QPalette::ColorRole  nR = QPalette::Highlight;
    QPalette::ColorGroup oA = QPalette::Active;
    QPalette::ColorRole  oR = QPalette::Button;
    pal.setColor(QPalette::Active,   nR, pal.color(oA,oR));
    pal.setColor(QPalette::Inactive, nR, pal.color(oA,oR));
    pal.setColor(QPalette::Disabled, nR, pal.color(oA,oR));
    setPalette(pal);
    QListWidget::focusOutEvent(e);
}

void MythListBox::focusInEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    emit changeHelpText(helptext);
    QListWidget::focusInEvent(e);
}


void MythListBox::setTopRow(uint row)
{
    QListWidgetItem *widget = item(row);
    if (widget)
        scrollToItem(widget, QAbstractItemView::PositionAtTop);
}

void MythListBox::insertItem(const QString &label)
{
    addItem(label);
}

void MythListBox::insertStringList(const QStringList &label_list)
{
    addItems(label_list);
}

void MythListBox::removeRow(uint row)
{
    QListWidgetItem *widget = takeItem(row);
    if (widget)
        delete widget;
}

void MythListBox::changeItem(const QString &new_text, uint row)
{
    QListWidgetItem *widget = item(row);
    if (widget)
        widget->setText(new_text);
}

int MythListBox::getIndex(const QList<QListWidgetItem*> &list)
{
    return (list.empty()) ? -1 : row(list[0]);
}

QString MythListBox::text(uint row) const
{
    QListWidgetItem *widget = item(row);
    return (widget) ? widget->text() : QString::null;
}

bool MythListBox::itemVisible(uint row) const
{
    QListWidgetItem *widget = item(row);
    return (widget) ? !isItemHidden(widget) : false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
