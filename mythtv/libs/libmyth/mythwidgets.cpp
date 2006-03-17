#include <qstyle.h>
#include <qpainter.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qvbox.h>
#include <qlayout.h>

#include <iostream>

using namespace std;

#include "mythwidgets.h"
#include "mythcontext.h"
#include "util.h"
#include "mythdialogs.h"
#include "virtualkeyboard.h"
#include "libmythui/mythmainwindow.h"

void MythComboBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT")
            {
                if (currentItem() == 0)
                    setCurrentItem(count()-1);
                else if (count() > 0)
                    setCurrentItem((currentItem() - 1) % count());
            }
            else if (action == "RIGHT")
            {
                if (count() > 0)
                    setCurrentItem((currentItem() + 1) % count());
            }
            else if (action == "PAGEDOWN")
            {
                if (currentItem() == 0)
                    setCurrentItem(count() - (step % count()));
                else if (count() > 0)
                    setCurrentItem(
                        (currentItem() + count() - (step % count())) % count());
            }
            else if (action == "PAGEUP")
            {
                if (count() > 0)
                    setCurrentItem(
                        (currentItem() + (step % count())) % count());
            }
            else if (action == "SELECT" && AcceptOnSelect)
                emit accepted(currentItem());
            else
                handled = false;
        }
    }

    if (!handled)
    {
        if (editable())
            QComboBox::keyPressEvent(e);
        else
            e->ignore();
    }
}

void MythComboBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);

    if (lineEdit())
        lineEdit()->setPaletteBackgroundColor(highlight);

    QComboBox::focusInEvent(e);
}

void MythComboBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();

    if (lineEdit())
    {
        lineEdit()->unsetPalette();

        // commit change if the user was editting an entry
        QString curText = currentText();
        int i;
        bool foundItem = false;

        for(i = 0; i < count(); i++)
            if (curText == text(i))
                foundItem = true;

        if ( !foundItem )
        {
            insertItem(curText);
            setCurrentItem(count()-1);
        }
    }

    QComboBox::focusOutEvent(e);
}

void MythCheckBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
    }

    if (!handled)
        e->ignore();
}

void MythCheckBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QCheckBox::focusInEvent(e);
}

void MythCheckBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QCheckBox::focusOutEvent(e);
}

void MythRadioButton::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
    }

    if (!handled)
        e->ignore();
}

void MythRadioButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QRadioButton::focusInEvent(e);
}

void MythRadioButton::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QRadioButton::focusOutEvent(e);
}

bool MythSpinBox::eventFilter(QObject* o, QEvent* e)
{
    (void)o;

    if (e->type() == QEvent::FocusIn)
    {
        QColor highlight = colorGroup().highlight();
        editor()->setPaletteBackgroundColor(highlight);
        emit(changeHelpText(helptext));
    }
    else if (e->type() == QEvent::FocusOut)
    {
        editor()->unsetPalette();
    }

    if (e->type() != QEvent::KeyPress)
        return FALSE;

    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent *)e, 
                                                     actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT")
            {
                if (singlestep)
                    setValue(value() - 1);
                else
                    stepDown();
            }
            else if (action == "RIGHT")
            {
                if (singlestep)
                    setValue(value() + 1);
                else
                    stepUp();
            }
            else if (action == "PAGEUP")
                stepDown();
            else if (action == "PAGEDOWN")
                stepUp();
            else if (action == "SELECT" || action == "ESCAPE")
                return FALSE;
            else
                handled = false;
        }
    }

    return TRUE;
}

void MythSpinBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QSpinBox::focusInEvent(e);
}

void MythSpinBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QSpinBox::focusOutEvent(e);
}

void MythSlider::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT")
                setValue(value() - lineStep());
            else if (action == "RIGHT")
                setValue(value() + lineStep());
            else if (action == "SELECT")
                e->ignore();
            else
                handled = false;
        }
    }

    if (!handled)
        QSlider::keyPressEvent(e);
}

void MythSlider::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QSlider::focusInEvent(e);
}

void MythSlider::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QSlider::focusOutEvent(e);
}

MythLineEdit::~MythLineEdit()
{
    if (popup)
        delete popup;
}

void MythLineEdit::Init()
{
    popup = NULL;
    popupPosition = VK_POSBELOWEDIT;
}

void MythLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if ((!popup || !popup->isShown()) &&
        gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions, false))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "SELECT" && 
                    (e->text().isNull() ||
                    (e->key() == Qt::Key_Enter) ||
                    (e->key() == Qt::Key_Return)))
            {
                if ((allowVirtualKeyboard) &&
				    (gContext->GetNumSetting("UseVirtualKeyboard", 1) == 1))
                {
                    popup = new VirtualKeyboard(gContext->GetMainWindow(), this);
                    gContext->GetMainWindow()->detach(popup);
                    popup->exec();
                    delete popup;
                    popup = NULL;
                }
            }
            else if (action == "SELECT" && e->text().isNull() )
                e->ignore();
            else
                handled = false;
        }
    }

    if (!handled)
        if (rw || e->key() == Key_Escape)
            QLineEdit::keyPressEvent(e);
}

void MythLineEdit::setText(const QString& text) 
{
    // Don't mess with the cursor position; it causes
    // counter-intuitive behaviour due to interactions with the
    // communication with the settings stuff

    int pos = cursorPosition();
    QLineEdit::setText(text);
    setCursorPosition(pos);
}

void MythLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QLineEdit::focusInEvent(e);
}

void MythLineEdit::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    if (popup && popup->isShown() && !popup->hasFocus())
        popup->hide();
    QLineEdit::focusOutEvent(e);
}

void MythLineEdit::hideEvent(QHideEvent *e)
{
    if (popup && popup->isShown())
        popup->hide();
    QLineEdit::hideEvent(e);
}

void MythLineEdit::mouseDoubleClickEvent(QMouseEvent *e)
{
    QLineEdit::mouseDoubleClickEvent(e);
}

MythRemoteLineEdit::MythRemoteLineEdit(QWidget * parent, const char * name)
                  : QTextEdit(parent, name)
{
    my_font = NULL;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(const QString & contents, 
                                       QWidget * parent, const char * name)
                  : QTextEdit(parent, name)
{
    my_font = NULL;
    m_lines = 1;
    this->Init();
    setText(contents);
}

MythRemoteLineEdit::MythRemoteLineEdit(QFont *a_font, QWidget * parent, 
                                       const char * name)
                  : QTextEdit(parent, name)
{
    my_font = a_font;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(int lines, QWidget * parent, 
                                       const char * name)
                  : QTextEdit(parent, name)
{
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
    
    //  We need to start out in PlainText format
    //  and only toggle RichText on if we are in
    //  the middle of a character cycle. That's
    //  the only way to do all this in a way which
    //  works across most 3.x.x versions of Qt.
    setTextFormat(Qt::PlainText);    

    cycle_time = 3000;

    pre_cycle_text_upto = "";
    pre_cycle_text_from = "";
    pre_cycle_para = 0;
    pre_cycle_pos  = 0;

    col_unselected.setRgb(100, 100, 100);
    col_selected.setRgb(0, 255, 255);
    col_special.setRgb(255, 0, 0);

    assignHexColors();

    //  Try and make sure it doesn't ever change
    setWordWrap(QTextEdit::NoWrap);
    QScrollView::setVScrollBarMode(QScrollView::AlwaysOff);
    QScrollView::setHScrollBarMode(QScrollView::AlwaysOff);

    if (my_font)
        setFont(*my_font);

    QFontMetrics fontsize(font());
    
    setMinimumHeight(fontsize.height() * 5 / 4);
    setMaximumHeight(fontsize.height() * m_lines * 5 / 4);

    connect(cycle_timer, SIGNAL(timeout()), this, SLOT(endCycle()));
    
    popup = NULL;
    popupPosition = VK_POSBELOWEDIT;
}

void MythRemoteLineEdit::startCycle(QString current_choice, QString set)
{
    int end_paragraph;
    int end_position;
    int dummy;
    int dummy_two;
    
    if (active_cycle)
    {
        cerr << "libmyth: MythRemoteLineEdit was asked to start a cycle when a cycle was already active." << endl;
    }
    else
    {
        cycle_timer->start(cycle_time, true);
        active_cycle = true;
        //  Amazingly, Qt (version < 3.1.1) only lets us pull
        //  text out in segments by fiddling around
        //  with selecting it. Oh well.
        getCursorPosition(&pre_cycle_para, &pre_cycle_pos);
        selectAll(true);
        getSelection(&dummy, &dummy_two, &end_paragraph, &end_position);
        setSelection(pre_cycle_para, pre_cycle_pos, end_paragraph, end_position, 0);
        pre_cycle_text_from = selectedText();
        setSelection(0, 0, pre_cycle_para, pre_cycle_pos, 0);
        pre_cycle_text_upto = selectedText();
        selectAll(false);
        setCursorPosition(pre_cycle_para, pre_cycle_pos);        
        updateCycle(current_choice, set); // Show the user their options
    }
}

void MythRemoteLineEdit::setCharacterColors(QColor unselected, QColor selected,
                                            QColor special)
{
    col_unselected = unselected;
    col_selected = selected;
    col_special = special;
    assignHexColors();
}

void MythRemoteLineEdit::updateCycle(QString current_choice, QString set)
{
    int index;
    QString aString, bString;

    //  Show the characters in the current set being cycled
    //  through, with the current choice in a different color. If the current
    //  character is uppercase X (interpreted as desctructive
    //  backspace) or an underscore (interpreted as a space)
    //  then show these special cases in yet another color.
    
    if (shift)
    {
        set = set.upper();
        current_choice = current_choice.upper();
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

    index = set.find(current_choice);
    int length = set.length();
    if (index < 0 || index > length)
    {
        cerr << "libmyth: MythRemoteLineEdit passed a choice of \"" << current_choice << "\" which is not in set \"" << set << "\"" << endl;
        setText("????");
        return;
    }
    else
    {
        set.replace(index, current_choice.length(), bString);
    }    

    QString esc_upto =  pre_cycle_text_upto;
    QString esc_from =  pre_cycle_text_from;

    esc_upto.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");
    esc_from.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");

    aString = esc_upto;
    aString += "<FONT COLOR=\"#";
    aString += hex_unselected;
    aString += "\">[";
    aString += set;
    aString += "]</FONT>";
    aString += esc_from;
    setTextFormat(Qt::RichText);
    setText(aString);
    setCursorPosition(pre_cycle_para, pre_cycle_pos + set.length());
    update();
    setCursorPosition(pre_cycle_para, pre_cycle_pos);

    //  If current selection is delete, 
    //  select the character that may well
    //  get deleted
    
    if (current_choice == "X" && pre_cycle_pos > 0)
    {
        setSelection(pre_cycle_para, pre_cycle_pos - 1, pre_cycle_para, pre_cycle_pos, 0);
    }
}

void MythRemoteLineEdit::assignHexColors()
{
    char text[1024];
    
    sprintf(text, "%.2X%.2X%.2X", col_unselected.red(), col_unselected.green(), col_unselected.blue());
    hex_unselected = text;
    
    sprintf(text, "%.2X%.2X%.2X", col_selected.red(), col_selected.green(), col_selected.blue());
    hex_selected = text;
    
    sprintf(text, "%.2X%.2X%.2X", col_special.red(), col_special.green(), col_special.blue());
    hex_special = text;
}

void MythRemoteLineEdit::endCycle()
{
    QString aString;
    
    if (active_cycle)
    {
        //  The timer ran out or the user pressed a key
        //  outside of the current set of choices
        if (current_choice == "_")       //  Space
        {
            aString  = pre_cycle_text_upto;
            aString += " ";
            aString += pre_cycle_text_from;
        } 
        else if (current_choice == "X") // destructive backspace
        {
            //  Deal with special case in a way
            //  that all 3.x.x versions of Qt
            //  can handle
            
            if (pre_cycle_text_upto.length() > 0)
            {
                aString = pre_cycle_text_upto.left(pre_cycle_text_upto.length() - 1);
            }
            else
            {
                aString = "";
            }
            aString += pre_cycle_text_from;
            pre_cycle_pos--;
        }
        else if (shift)
        {
            aString  = pre_cycle_text_upto;
            aString += current_choice.upper();
            aString += pre_cycle_text_from;
        }
        else
        {
            aString  = pre_cycle_text_upto;
            aString += current_choice;
            aString += pre_cycle_text_from;
        }

        setTextFormat(Qt::PlainText);
        setText(aString);
        setCursorPosition(pre_cycle_para, pre_cycle_pos + 1);
        active_cycle = false;
        current_choice = "";
        current_set = "";
    }
    emit(textChanged(this->text()));
}

void MythRemoteLineEdit::setText(const QString& text) 
{
    int para, pos;
    
    //  Isaac had this in the original version
    //  of MythLineEdit, and I'm sure he had
    //  a reason ...
    getCursorPosition(&para, &pos);
    QTextEdit::setText(text);
    setCursorPosition(para, pos);
}

void MythRemoteLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;

    if ((!popup || !popup->isShown()) &&
          gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions, false))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
            else if ((action == "SELECT") &&
                     (!active_cycle) &&
                     ((e->text().isNull()) ||
                      (e->key() == Qt::Key_Enter) || 
                      (e->key() == Qt::Key_Return)))
            {
                if (gContext->GetNumSetting("UseVirtualKeyboard", 1) == 1)
                {
                    popup = new VirtualKeyboard(gContext->GetMainWindow(), this);
                    gContext->GetMainWindow()->detach(popup);
                    popup->exec();
                    delete popup;
                    popup = NULL;
                }
 
            }
            else
                handled = false;
        }
    }

    if (handled)
        return;

    if (popup && popup->isShown())
    {
        endCycle();
        QTextEdit::keyPressEvent(e);
        emit textChanged(this->text());
        return;
    }

    switch (e->key())
    {
        case Key_Enter:
        case Key_Return:
            handled = true;
            endCycle();
            e->ignore();
            break;

            //  Only eat Key_Space if we are in a cycle
            
        case Key_Space:
            if (active_cycle)
            {
                handled = true;
                endCycle();
                e->ignore();
            }
            break; 

            //  If you want to mess arround with other ways to allocate
            //  key presses you can just add entries between the quotes

        case Key_1:
            cycleKeys("_X%-/.?()1");
            handled = true;
            break;

        case Key_2:
            cycleKeys("abc2");
            handled = true;
            break;

        case Key_3:
            cycleKeys("def3"); 
            handled = true;
            break;

        case Key_4:
            cycleKeys("ghi4");
            handled = true;
            break;

        case Key_5:
            cycleKeys("jkl5"); 
            handled = true;
            break;

        case Key_6:
            cycleKeys("mno6"); 
            handled = true;
            break;

        case Key_7:
            cycleKeys("pqrs7");
            handled = true;
            break;

        case Key_8:
            cycleKeys("tuv8"); 
            handled = true;
            break;

        case Key_9:
            cycleKeys("wxyz90"); 
            handled = true;
            break;

        case Key_0:
            toggleShift();
            handled = true;
            break;
    }
    
    if (!handled)
    {
        endCycle();
        QTextEdit::keyPressEvent(e);
        emit textChanged(this->text());
    }
}

void MythRemoteLineEdit::setCycleTime(float desired_interval)
{
    if (desired_interval < 0.5 || desired_interval > 10.0)
    {
        cerr << "libmyth: Did not accept key cycle interval of " << desired_interval << " seconds" << endl; 
    }
    else
    {
        cycle_time = (int) (desired_interval * 1000.0);
    }
}

void MythRemoteLineEdit::cycleKeys(QString cycle_list)
{
    int index;
    
    if (active_cycle)
    {
        if (cycle_list == current_set)
        {
            //  Regular movement through existing set
            cycle_timer->changeInterval(cycle_time);
            index = current_set.find(current_choice);
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
            cycle_timer->changeInterval(cycle_time);
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
        temp_choice = current_choice.upper();
        temp_set = current_set.upper();
    }
    if (active_cycle)
    {
        updateCycle(temp_choice, temp_set);  
    }
}

void MythRemoteLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();    //perhaps we need to save a playlist?

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);

    QTextEdit::focusInEvent(e);
}

void MythRemoteLineEdit::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();

    if (popup && popup->isShown() && !popup->hasFocus())
        popup->hide();

    emit lostFocus();
    QTextEdit::focusOutEvent(e);
}


MythRemoteLineEdit::~MythRemoteLineEdit()
{
    if (cycle_timer)
    {
        delete cycle_timer;
    }

    if (popup)
        delete popup;
}

void MythRemoteLineEdit::insert(QString text)
{
    QTextEdit::insert(text);
    emit textChanged(this->text());
}

void MythRemoteLineEdit::del()
{
    doKeyboardAction(QTextEdit::ActionDelete);
    emit textChanged(this->text());
}

void MythRemoteLineEdit::backspace()
{
    doKeyboardAction(QTextEdit::ActionBackspace);
    emit textChanged(this->text());
}

void MythTable::keyPressEvent(QKeyEvent *e)
{
    if (isEditing() && item(currEditRow(), currEditCol()) &&
        item(currEditRow(), currEditCol())->editType() == QTableItem::OnTyping)
        return;

    int tmpRow = currentRow();
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent *)e,
                                                     actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];

            if (action == "UP")
            {
                if (tmpRow == 0)
                {
                    focusNextPrevChild(false);
                    handled = true;
                }
            }
            else if (action == "DOWN")
            {
                if (tmpRow == numRows() - 1)
                {
                    focusNextPrevChild(true);
                    handled = true;
                }
            }
            else if (action == "LEFT" || action == "RIGHT")
                handled = true;
        }
    }

    if (!handled)
        QTable::keyPressEvent(e);
}

void MythButtonGroup::moveFocus(int key)
{
    QButton *currentSel = selected();

    QButtonGroup::moveFocus(key);

    if (selected() == currentSel)
    {
        switch (key)
        {
            case Key_Up: focusNextPrevChild(FALSE); break;
            case Key_Down: focusNextPrevChild(TRUE); break;
            default: break;
        }
    }
}

void MythPushButton::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    keyPressActions.clear();

    if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent *)e,
                                                     actions))
    {
        keyPressActions = actions;

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "SELECT")
            {
                setDown(true);
                emit pressed();
                handled = true;
            }
            else if (arrowAccel)
            {
                if (action == "LEFT") 
                {
                    parent()->event(e);
                    handled = true;
                }
                else if (action == "RIGHT")
                {
                    setDown(true);
                    emit pressed();
                    handled = true;
                }
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
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "SELECT")
        {
            QKeyEvent tempe(QEvent::KeyRelease, Key_Space, ' ', 0, " ");
            QPushButton::keyReleaseEvent(&tempe);
            handled = true;
        }
    }

    if (!handled)
        QPushButton::keyReleaseEvent(e);
}

void MythPushButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QPushButton::focusInEvent(e);
}

void MythPushButton::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QPushButton::focusOutEvent(e);
}

MythListView::MythListView(QWidget *parent)
            : QListView(parent)
{
    viewport()->setPalette(palette());
    horizontalScrollBar()->setPalette(palette());
    verticalScrollBar()->setPalette(palette());
    header()->setPalette(palette());
    header()->setFont(font());

    setAllColumnsShowFocus(TRUE);
}

void MythListView::ensureItemVCentered ( const QListViewItem * i )
{
    if (!i)
        return;

    int y = itemPos(i);
    int h = i->height();
    
    if (y - h / 2 < visibleHeight() / 2 || 
        y - h / 2 > contentsHeight() - visibleHeight() / 2)
    {
        ensureItemVisible(i);
    }
    else
    {
        ensureVisible(contentsX(), y, 0, visibleHeight() / 2);
    }
}

void MythListView::keyPressEvent(QKeyEvent *e)
{
    if (currentItem() && !currentItem()->isEnabled())
    {
        QListView::keyPressEvent(e);
        return;

    }

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP" && currentItem() == firstChild())
        {
            // Key_Up at top of list allows focus to move to other widgets
            clearSelection();
            if (!focusNextPrevChild(false))
            {
                // BUT (if we get here) there was no other widget to take focus 
                setSelected(currentItem(), true);
            }
        }
        else if (action == "DOWN" && currentItem() == lastItem())
        {
            // Key_Down at bottom of list allows focus to move to other widgets
            clearSelection();
            if (!focusNextPrevChild(true))
                setSelected(currentItem(), true);
        }
        else if (action == "SELECT")
        {
            emit spacePressed(currentItem());
            return;
        }
        else
            handled = false;
    }

    QListView::keyPressEvent(e);
}

void MythListView::focusInEvent(QFocusEvent *e)
{
    //
    //  Let the base class do whatever it is
    //  it does
    //

    QListView::focusInEvent(e);    
    
    //
    //  Always highlight the current item
    //  as "Selected" in the Qt sense
    //  (not selected in the box ticked sense
    //
    
    setSelected(currentItem(), true);
}

MythListBox::MythListBox(QWidget* parent): QListBox(parent) 
{
}

void MythListBox::polish(void)
{
    QListBox::polish();
    
    QPalette pal = palette();
    QColorGroup::ColorRole role = QColorGroup::Highlight;
    pal.setColor(QPalette::Active, role, pal.active().button()); 
    pal.setColor(QPalette::Inactive, role, pal.active().button()); 
    pal.setColor(QPalette::Disabled, role, pal.active().button()); 
   
    setPalette(pal);
}

void MythListBox::setCurrentItem(const QString& matchText, bool caseSensitive, 
                                 bool partialMatch) 
{
    for (unsigned i = 0 ; i < count() ; ++i)
    {
        if (partialMatch)
        {
            if (caseSensitive)
            {
                if (text(i).startsWith(matchText))
                {
                    setCurrentItem(i);
                    break;
                }
            }
            else
            {
                if (text(i).lower().startsWith(matchText.lower()))
                {
                    setCurrentItem(i);
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
                    setCurrentItem(i);
                    break;
                }
            }
            else
            {    
                if (text(i).lower() == matchText.lower())
                {
                    setCurrentItem(i);
                    break;
                }
            }        
        }
    }            
}

void MythListBox::keyPressEvent(QKeyEvent* e) 
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "UP" || action == "DOWN" || action == "PAGEUP" ||
                action == "PAGEDOWN" || action == "LEFT" || action == "RIGHT")
            {
                int key;
                if (action == "UP")
                {
                    // Key_Up at top of list allows focus to move to other widgets
                    if (currentItem() == 0)
                    {
                        focusNextPrevChild(false);
                        handled = true;
                        continue;
                    }
                    
                    key = Key_Up;
                }
                else if (action == "DOWN")
                {
                    // Key_down at bottom of list allows focus to move to other widgets
                    if (currentItem() == (int) count() - 1)
                    {
                        focusNextPrevChild(true);
                        handled = true;
                        continue;
                    }
                    
                    key = Key_Down;
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
                    key = Key_Prior;
                else if (action == "PAGEDOWN")
                    key = Key_Next;
                else
                    key = Key_unknown;

                QKeyEvent ev(QEvent::KeyPress, key, 0, Qt::NoButton);
                QListBox::keyPressEvent(&ev);
                handled = true;
            }
            else if (action == "MENU")
                emit menuButtonPressed(currentItem());
            else if (action == "EDIT")
                emit editButtonPressed(currentItem());
            else if (action == "DELETE")
                emit deleteButtonPressed(currentItem());
            else if (action == "SELECT")
                emit accepted(currentItem());
        }
    }

    if (!handled)
        e->ignore();
}

void MythListBox::focusOutEvent(QFocusEvent *e)
{
    QPalette pal = palette();
    QColorGroup::ColorRole role = QColorGroup::Highlight;
    pal.setColor(QPalette::Active, role, pal.active().button()); 
    pal.setColor(QPalette::Inactive, role, pal.active().button()); 
    pal.setColor(QPalette::Disabled, role, pal.active().button()); 
   
    setPalette(pal);
    QListBox::focusOutEvent(e);
}

void MythListBox::focusInEvent(QFocusEvent *e)
{
    this->unsetPalette();
    
    emit changeHelpText(helptext);
    QListBox::focusInEvent(e);
}
