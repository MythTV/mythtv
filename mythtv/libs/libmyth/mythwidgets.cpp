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

void MythComboBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP")
            {
                focusNextPrevChild(false);
                handled = true;
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
                handled = true;
            }
            else if (action == "LEFT")
            {
                if (currentItem() == 0)
                    setCurrentItem(count()-1);
                else if (count() > 0)
                    setCurrentItem((currentItem() - 1) % count());
                handled = true;
            }
            else if (action == "RIGHT")
            {
                if (count() > 0)
                    setCurrentItem((currentItem() + 1) % count());
                handled = true;
            }
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
    QComboBox::focusInEvent(e);
}

void MythComboBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QComboBox::focusOutEvent(e);
}

void MythCheckBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP")
            {
                focusNextPrevChild(false);
                handled = true;
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
                handled = true;
            }
            else if (action == "LEFT" || action == "RIGHT")
            {
                toggle();
                handled = true;
            }
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
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP")
            {
                focusNextPrevChild(false);
                handled = true;
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
                handled = true;
            }
            else if (action == "LEFT")
            {
                stepDown();
                handled = true;
            }
            else if (action == "RIGHT")
            {
                stepUp();
                handled = true;
            }
            else if (action == "SELECT" || action == "ESCAPE")
                return FALSE;
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
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP")
            {
                focusNextPrevChild(false);
                handled = true;
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
                handled = true;
            }
            else if (action == "LEFT")
            {
                setValue(value() - lineStep());
                handled = true;
            }
            else if (action == "RIGHT")
            {
                setValue(value() + lineStep());
                handled = true;
            }
            else if (action == "SELECT")
            {
                e->ignore();
                handled = true;
            }
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

void MythLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP")
            {
                focusNextPrevChild(false);
                handled = true;
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
                handled = true;
            }
            else if (action == "SELECT" && e->text().isNull())
            {
                e->ignore();
                handled = true;
            }
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
    QLineEdit::focusOutEvent(e);
}

MythRemoteLineEdit::MythRemoteLineEdit(QFont *a_font, QWidget * parent, 
                                       const char * name)
                  : QTextEdit(parent, name)
{
    my_font = a_font;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(QWidget * parent, const char * name)
                  : QTextEdit(parent, name)
{
    my_font = NULL;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(const QString & contents, 
                                       QWidget * parent, const char * name)
                  : QTextEdit(parent, name)
{
    my_font = NULL;
    this->Init();
    setText(contents);
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
    setMaximumHeight(fontsize.height() * 5 / 4);

    connect(cycle_timer, SIGNAL(timeout()), this, SLOT(endCycle()));
}

void MythRemoteLineEdit::startCycle(QString current_choice, QString set)
{
    int end_paragraph;
    int end_position;
    int dummy;
    int dummy_two;
    
    if(active_cycle)
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
    
        
    aString = pre_cycle_text_upto;
    aString += "<FONT COLOR=\"#";
    aString += hex_unselected;
    aString += "\">[";
    aString += set;
    aString += "]</FONT>";
    aString += pre_cycle_text_from;
    setTextFormat(Qt::RichText);
    setText(aString);
    setCursorPosition(pre_cycle_para, pre_cycle_pos + set.length());
    update();
    setCursorPosition(pre_cycle_para, pre_cycle_pos);

    //  If current selection is delete, 
    //  select the character that may well
    //  get deleted
    
    if(current_choice == "X" && pre_cycle_pos > 0)
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
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP")
            {
                handled = true;
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
                handled = true;
                endCycle();
                QWidget::focusNextPrevChild(true);
                emit tryingToLooseFocus(true);
            }
        }
    }

    if (handled)
        return;

    switch(e->key())
    {
        case Key_Enter:
        case Key_Return:
            handled = true;
            endCycle();
            e->ignore();
            break;

            //  Only eat Key_Space if we are in a cycle
            
        case Key_Space:
            if(active_cycle)
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
    
    if(handled == false)
    {
        endCycle();
        QTextEdit::keyPressEvent(e);
        emit textChanged(this->text());
    }
}

void MythRemoteLineEdit::setCycleTime(float desired_interval)
{
    if(desired_interval < 0.5 || desired_interval > 10.0)
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
            if(index + 1 >= length)
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

    emit lostFocus();
    QTextEdit::focusOutEvent(e);
}

MythRemoteLineEdit::~MythRemoteLineEdit()
{
    if(cycle_timer)
    {
        delete cycle_timer;
    }
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
        for (unsigned int i = 0; i < actions.size(); i++)
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
            {
                handled = true;
            }
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

void MythPushButton::drawButton(QPainter * p)
{
    int diw = 0;
    if ( isDefault() || autoDefault() ) {
        diw = style().pixelMetric(QStyle::PM_ButtonDefaultIndicator, this);

        if ( diw > 0 ) {
            if (backgroundMode() == X11ParentRelative) {
                erase( 0, 0, width(), diw );
                erase( 0, 0, diw, height() );
                erase( 0, height() - diw, width(), diw );
                erase( width() - diw, 0, diw, height() );
            } else if ( parentWidget() && parentWidget()->backgroundPixmap() ){
                // pseudo tranparency
                p->drawTiledPixmap( 0, 0, width(), diw,
                                    *parentWidget()->backgroundPixmap(),
                                        x(), y() );
                p->drawTiledPixmap( 0, 0, diw, height(),
                                    *parentWidget()->backgroundPixmap(),
                                        x(), y() );
                p->drawTiledPixmap( 0, height()-diw, width(), diw,
                                    *parentWidget()->backgroundPixmap(),
                                        x(), y() );
                p->drawTiledPixmap( width()-diw, 0, diw, height(),
                                    *parentWidget()->backgroundPixmap(),
                                    x(), y() );
            } else {
                p->fillRect( 0, 0, width(), diw,
                             colorGroup().brush(QColorGroup::Background) );
                p->fillRect( 0, 0, diw, height(),
                             colorGroup().brush(QColorGroup::Background) );
                p->fillRect( 0, height()-diw, width(), diw,
                             colorGroup().brush(QColorGroup::Background) );
                p->fillRect( width()-diw, 0, diw, height(),
                             colorGroup().brush(QColorGroup::Background) );
            }
        }
    }


    QStyle::SFlags flags = QStyle::Style_Default;
    if (isEnabled())
        flags |= QStyle::Style_Enabled;
    if (hasFocus())
        flags |= QStyle::Style_MouseOver;
    if (isDown())
        flags |= QStyle::Style_Down;
    if (isOn())
        flags |= QStyle::Style_On;
    if (! isOn() && ! isDown())
        flags |= QStyle::Style_Raised;

    if (!origcolor.isValid())
        origcolor = palette().color(QPalette::Inactive, QColorGroup::Button);

    QColor set;

    if (flags & QStyle::Style_MouseOver)
        set = colorGroup().highlight();
    else
        set = origcolor;

    QPalette pal = palette();
    pal.setColor(QPalette::Active, QColorGroup::Button, set);
    setPalette(pal);

    style().drawControl(QStyle::CE_PushButton, p, this, rect(),
                        colorGroup(), flags);

    drawButtonLabel(p);
}

void MythPushButton::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent *)e,
                                                     actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "SELECT")
            {
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
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent *)e,
                                                     actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "SELECT")
            {
                QKeyEvent tempe(QEvent::KeyRelease, Key_Space, ' ', 0, " ");
                QPushButton::keyReleaseEvent(&tempe);
                handled = true;
            }
        }
    }

    if (!handled)
        QPushButton::keyReleaseEvent(e);
}

MythListView::MythListView(QWidget *parent)
            : QListView(parent)
{
    allowkeypress = true;
    fixspace = true;

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

/* XXX FIXME */
void MythListView::keyPressEvent(QKeyEvent *e)
{
    if (!allowkeypress)
        return;

    if (currentItem() && !currentItem()->isEnabled())
    {
    }
    else
    {
        switch(e->key())
        {
            case 'd': 
            case 'D': 
                emit deletePressed(currentItem()); 
                return;
                
            case 'p': 
            case 'P': 
                emit playPressed(currentItem()); 
                return;
                
            case 'i':
            case 'I':
                emit infoPressed(currentItem());
                return;
                
            case '0':
                emit numberPressed(currentItem(), 0);
                return;
                
            case '1':
                emit numberPressed(currentItem(), 1);
                return;
                
            case '2':
                emit numberPressed(currentItem(), 2);
                return;
                
            case '3':
                emit numberPressed(currentItem(), 3);
                return;
                
            case '4':
                emit numberPressed(currentItem(), 4);
                return;
                
            case '5':
                emit numberPressed(currentItem(), 5);
                return;
                
            case '6':
                emit numberPressed(currentItem(), 6);
                return;
                
            case '7':
                emit numberPressed(currentItem(), 7);
                return;
                
            case '8':
                emit numberPressed(currentItem(), 8);
                return;
                
            case '9':
                emit numberPressed(currentItem(), 9);
                return;
                
        }
        if(e->key() == Key_Up && currentItem() == firstChild())
        {
            //
            //  Key_Up at top of list allows
            //  focus to move to other widgets
            //
            clearSelection();
            if(!focusNextPrevChild(false))
            {
                // BUT (if we get here) there
                // was no other widget to take
                // the focus 
                setSelected(currentItem(), true);
            }
        }
        if(e->key() == Key_Down && currentItem() == lastItem())
        {
            //
            //  Key_Down at bottom of list allows
            //  focus to move to other widgets
            //
            clearSelection();
            if(!focusNextPrevChild(true))
            {
                setSelected(currentItem(), true);
            }
        }
        if (e->key() == Key_Space ||
            e->key() == Key_Return ||
            e->key() == Key_Enter)
        {
            emit spacePressed(currentItem());
            return;
        }
    }

    QListView::keyPressEvent(e);
    emit unhandledKeyPress(e);
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

void MythListBox::setCurrentItem(const QString& matchText) 
{
    for (unsigned i = 0 ; i < count() ; ++i)
        if (text(i) == matchText)
            setCurrentItem(i);
}

void MythListBox::keyPressEvent(QKeyEvent* e) 
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            QString action = actions[i];
            if (action == "UP" || action == "DOWN" || action == "PAGEUP" ||
                action == "PAGEDOWN")
            {
                QListBox::keyPressEvent(e);
                handled = true;
            }
        }
    }

    if (!handled)
        e->ignore();
}

