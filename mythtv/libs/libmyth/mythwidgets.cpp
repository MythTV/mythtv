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

void MythComboBox::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Key_Up:
        focusNextPrevChild(false);
        break;
    case Key_Down:
        focusNextPrevChild(true);
        break;
    case Key_Left:
        if (currentItem() == 0)
            setCurrentItem(count()-1);
        else if (count() > 0)
            setCurrentItem((currentItem() - 1) % count());
        break;
    case Key_Right:
        if (count() > 0)
            setCurrentItem((currentItem() + 1) % count());
        break;
    default:
        e->ignore();
    }
}

void MythCheckBox::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Key_Up:
        focusNextPrevChild(false);
        break;
    case Key_Down:
        focusNextPrevChild(true);
        break;
    case Key_Left:
    case Key_Right:
        toggle();
        break;
    default:
        e->ignore();
    }
}

bool MythSpinBox::eventFilter(QObject* o, QEvent* e)
{
    (void)o;

    if (e->type() == QEvent::FocusIn)
        emit(changeHelpText(helptext));

    if (e->type() != QEvent::KeyPress)
        return FALSE;

    switch (((QKeyEvent*)e)->key()) {
    case Key_Up:
        focusNextPrevChild(false);
        break;
    case Key_Down:
        focusNextPrevChild(true);
        break;
    case Key_Left:
        stepDown();
        break;
    case Key_Right:
        stepUp();
        break;
    default:
         return FALSE;
    }

    return TRUE;
}

void MythSlider::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Key_Up:
        focusNextPrevChild(false);
        break;
    case Key_Down:
        focusNextPrevChild(true);
        break;
    case Key_Left:
        setValue(value() - lineStep());
        break;
    case Key_Right:
        setValue(value() + lineStep());
        break;
    case Key_Enter:
    case Key_Return:
    case Key_Space:
        e->ignore();
    default:
        QSlider::keyPressEvent(e);
    }
}

void MythLineEdit::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Key_Up:
        focusNextPrevChild(FALSE);
        break;
    case Key_Down:
        focusNextPrevChild(TRUE);
        break;
    case Key_Enter:
    case Key_Return:
    case Key_Space:
        e->ignore();
    default:
        QLineEdit::keyPressEvent(e);
    }
}

void MythLineEdit::setText(const QString& text) {
    // Don't mess with the cursor position; it causes
    // counter-intuitive behaviour due to interactions with the
    // communication with the settings stuff

    int pos = cursorPosition();
    QLineEdit::setText(text);
    setCursorPosition(pos);
}

// thor feb 18 2003

MythRemoteLineEdit::MythRemoteLineEdit( QWidget * parent, const char * name)
    : QTextEdit(parent, name)
{
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit( const QString & contents, QWidget * parent, const char * name)
    : QTextEdit(parent, name)
{
    this->Init();
    setText(contents);
}

void MythRemoteLineEdit::Init()
{
    //
    //  Bunch of default values
    //

    cycle_timer = new QTimer();
    shift = false;
    active_cycle = false;
    current_choice = "";
    current_set = "";
    
    //
    //  We need to start out in PlainText format
    //  and only toggle RichText on if we are in
    //  the middle of a character cycle. That's
    //  the only way to do all this in a way which
    //  works across most 3.x.x versions of Qt.
    //

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

    //
    //  Try and make sure it doesn't ever change
    //    
    setWordWrap(QTextEdit::NoWrap);
    QScrollView::setVScrollBarMode(QScrollView::AlwaysOff);
    QScrollView::setHScrollBarMode(QScrollView::AlwaysOff);

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
        //
        //  Amazingly, Qt (version < 3.1.1) only lets us pull
        //  text out in segments by fiddling around
        //  with selecting it. Oh well.
        //
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

void MythRemoteLineEdit::setCharacterColors(QColor unselected, QColor selected, QColor special)
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

    //
    //  Show the characters in the current set being cycled
    //  through, with the current choice in a different color. If the current
    //  character is uppercase X (interpreted as desctructive
    //  backspace) or an underscore (interpreted as a space)
    //  then show these special cases in yet another color.
    //
    
    if(shift)
    {
        set = set.upper();
        current_choice = current_choice.upper();
    }

    bString  = "<B>";
    if(current_choice == "_" || current_choice == "X")
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
    if(index < 0 || index > length)
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
    setCursorPosition(pre_cycle_para, pre_cycle_pos);

    //
    //  If current selection is delete, 
    //  select the character that may well
    //  get deleted
    //
    
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
    
    if(active_cycle)
    {
        //
        //  The timer ran out or the user pressed a key
        //  outside of the current set of choices
        //
        if(current_choice == "_")       //  Space
        {
            aString  = pre_cycle_text_upto;
            aString += " ";
            aString += pre_cycle_text_from;
        } 
        else if (current_choice == "X") // destructive backspace
        {
            //
            //  Deal with special case in a way
            //  that all 3.x.x versions of Qt
            //  can handle
            //
            
            if(pre_cycle_text_upto.length() > 0)
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
        else if(shift)
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
}

void MythRemoteLineEdit::setText(const QString& text) 
{
    int para, pos;
    
    //
    //  Isaac had this in the original version
    //  of MythLineEdit, and I'm sure he had
    //  a reason ...
    //    
    getCursorPosition(&para, &pos);
    QTextEdit::setText(text);
    setCursorPosition(para, pos);

}



void MythRemoteLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    switch(e->key())
    {
        case Key_Up:
            handled = true;
            endCycle();
            // Need to call very base one because
            // QTextEdit reimplements it to tab
            // through links (even if you're in
            // PlainText Mode !!)
            QWidget::focusNextPrevChild(false);
            break;
   
        case Key_Down:
            handled = true;
            endCycle();
            QWidget::focusNextPrevChild(true);
            break;
    
        case Key_Enter:
        case Key_Return:
        case Key_Space:
            handled = true;
            endCycle();
            e->ignore();
            break;

            //
            //  If you want to mess arround with other ways to allocate
            //  key presses you can just add entried between the quotes
            //

        case Key_1:
            cycleKeys("_X-/.?()1");
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
    
    
    if(active_cycle)
    {
        if(cycle_list == current_set)
        {
            //
            //  Regular movement through existing set
            //
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
            //
            //  Previous cycle was still active, but user moved
            //  to another keypad key
            //
            endCycle();
            current_choice = cycle_list.left(1);
            current_set = cycle_list;
            cycle_timer->changeInterval(cycle_time);
            startCycle(current_choice, current_set);
        }
                
    }
    else
    {
        //
        //  First press with no cycle of any type active
        //
        current_choice = cycle_list.left(1);
        current_set = cycle_list;
        startCycle(current_choice, current_set);
    }
}

void MythRemoteLineEdit::toggleShift()
{
    //
    //  Toggle uppercase/lowercase and
    //  update the cycle display if it's
    //  active
    //

    QString temp_choice = current_choice;
    QString temp_set = current_set;

    if(shift)
    {
        shift = false;
    }
    else
    {
        shift = true;
        temp_choice = current_choice.upper();
        temp_set = current_set.upper();
    }
    if(active_cycle)
    {
        updateCycle(temp_choice, temp_set);  
    }
}

void MythRemoteLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();    //perhaps we need to save a playlist?
    QTextEdit::focusInEvent(e);
}


void MythRemoteLineEdit::focusOutEvent(QFocusEvent *e)
{
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
    bool handled = false;

    if (isEditing() && item(currEditRow(), currEditCol()) &&
        item(currEditRow(), currEditCol())->editType() == QTableItem::OnTyping)
        return;

    int tmpRow = currentRow();

    switch (e->key())
    {
        case Key_Left:
        case Key_Right:
        {
            handled = true;
            break;
        }
        case Key_Up:
        {
            if (tmpRow == 0)
            {
                focusNextPrevChild(FALSE);
                handled = true;
            }
            break;
        }
        case Key_Down:
        {
            if (tmpRow == numRows() - 1)
            {
                focusNextPrevChild(TRUE);
                handled = true;
            }
            break;
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

void MythToolButton::drawButton(QPainter *p)
{
    QStyle::SCFlags controls = QStyle::SC_ToolButton;
    QStyle::SCFlags active = QStyle::SC_None;

    if (isDown())
        active |= QStyle::SC_ToolButton;

    QStyle::SFlags flags = QStyle::Style_Default;
    if (isEnabled())
        flags |= QStyle::Style_Enabled;
    if (hasFocus())
        flags |= QStyle::Style_MouseOver;
    if (isDown())
        flags |= QStyle::Style_Down;
    if (isOn())
        flags |= QStyle::Style_On;
    if (!autoRaise()) {
        flags |= QStyle::Style_AutoRaise;
        if (uses3D()) {
            flags |= QStyle::Style_MouseOver;
            if (! isOn() && ! isDown())
                flags |= QStyle::Style_Raised;
        }
    } else if (! isOn() && ! isDown())
        flags |= QStyle::Style_Raised;

    if (!origcolor.isValid())
        origcolor = palette().color(QPalette::Inactive, QColorGroup::Button);;

    QColor set;

    if (flags & QStyle::Style_MouseOver)
        set = colorGroup().highlight();
    else
        set = origcolor;

    QPalette pal = palette();
    pal.setColor(QPalette::Active, QColorGroup::Button, set);
    setPalette(pal);

    style().drawComplexControl(QStyle::CC_ToolButton, p, this, rect(),
                               colorGroup(), flags, controls, active,
                               QStyleOption());

    drawButtonLabel(p);
}

void MythPushButton::drawButton( QPainter * p )
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

MythDialog::MythDialog(QWidget *parent, const char *name, bool modal)
          : QDialog(parent, name, modal)
{
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int x, y, w, h;
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);

    setGeometry(x, y, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    gContext->ThemeWidget(this);
}

void MythDialog::Show(void)
{
    showFullScreen();
    setActiveWindow();
}

MythProgressDialog::MythProgressDialog(const QString &message, int totalSteps)
                  : MythDialog(NULL, 0, true)
{
    int yoff = screenheight / 3; int xoff = screenwidth / 10;
    setGeometry(xoff, yoff, screenwidth - xoff * 2, yoff);
    setFixedSize(QSize(screenwidth - xoff * 2, yoff));

    qApp->processEvents();

    QVBoxLayout *lay = new QVBoxLayout(this, 0);

    QVBox *vbox = new QVBox(this);
    lay->addWidget(vbox);

    vbox->setLineWidth(3);
    vbox->setMidLineWidth(3);
    vbox->setFrameShape(QFrame::Panel);
    vbox->setFrameShadow(QFrame::Raised);
    vbox->setMargin((int)(15 * wmult));

    QLabel *msglabel = new QLabel(vbox);
    msglabel->setBackgroundOrigin(WindowOrigin);
    msglabel->setText(message);

    progress = new QProgressBar(totalSteps, vbox);
    progress->setBackgroundOrigin(WindowOrigin);
    progress->setProgress(0);

    steps = totalSteps / 1000;
    if (steps == 0)
        steps = 1;

    Show();

    qApp->processEvents();

    setGeometry(xoff, yoff, screenwidth - xoff * 2, yoff);
}

void MythProgressDialog::Close(void)
{
    accept();
}

void MythProgressDialog::setProgress(int curprogress)
{
    progress->setProgress(curprogress);
    if (curprogress % steps == 0)
        qApp->processEvents();
}

void MythProgressDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Key_Escape)
        ;
    else
        MythDialog::keyPressEvent(e);
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
        if (e->key() == Key_Space && fixspace)
        {
            emit spacePressed(currentItem());
            return;
        }
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

void MythListBox::setCurrentItem(const QString& matchText) {
    for(unsigned i = 0 ; i < count() ; ++i)
        if (text(i) == matchText)
            setCurrentItem(i);
}

void MythListBox::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Key_Up:
    case Key_Down:
    case Key_Next:
    case Key_Prior:
    case Key_Home:
    case Key_End:
        QListBox::keyPressEvent(e);
        break;
    default:
        e->ignore();
    }
}

MythPopupBox::MythPopupBox(QWidget *parent)
            : QFrame(parent, 0, WType_Popup)
{
    int w, h;
    float wmult, hmult;

    gContext->GetScreenSettings(w, wmult, h, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setPalette(parent->palette());
    setFont(parent->font());

    vbox = new QVBoxLayout(this, (int)(10 * hmult));
}

void MythPopupBox::addWidget(QWidget *widget, bool setAppearance)
{
    if (setAppearance == true)
    {
         widget->setPalette(palette());
         widget->setFont(font());
    }
    vbox->addWidget(widget);
}
