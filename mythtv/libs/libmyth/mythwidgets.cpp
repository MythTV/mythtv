#include <qstyle.h>
#include <qpainter.h>
#include <qcursor.h>

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



LineEditHintBox::LineEditHintBox( QWidget * parent, const char * name)
    : QLabel(parent, name)
{
}

LineEditHintBox::~LineEditHintBox()
{
}

void LineEditHintBox::showShift(bool shift)
{
    //
    //  At some point an up/down arrow pixmap 
    //  might be a nice improvement
    //

    if(shift)
    {
        setText("[CAPS]");
    }
    else
    {
        setText("[nocaps]");
    }
}


void LineEditHintBox::showCycle(QString current_choice, QString set)
{
    int index;
    QString aString;

    //
    //  Show the characters in the current set being cycled
    //  through, with the current choice in bold. If the current
    //  character is uppercase X (interpreted as desctructive
    //  backspace) or an underscore (interpreted as a space)
    //  then show these special cases in red.
    //
    

    aString  = "<B>";
    if(current_choice == "_" || current_choice == "X")
    {
        aString += "<FONT COLOR=\"#ff0000\">";
        aString += current_choice;
        aString += "</FONT>";
    }
    else
    {
        aString += "<FONT COLOR=\"#00ffff\">";
        aString += current_choice;
        aString += "</FONT>";
    }
    aString += "</B>";
    
    index = set.find(current_choice);
    int length = set.length();
    if(index < 0 || index > length)
    {
        cerr << "libmyth: LineEditHintBox passed a choice of \"" << current_choice << "\" which is not in set \"" << set << "\"" << endl;
        setText("????");
        return;
    }
    else
    {
        set.replace(index, current_choice.length(), aString);
        setText(set);
    }    
}



KeypadLineEdit::KeypadLineEdit( QWidget * parent, const char * name)
    : QLineEdit(parent, name)
{
    this->Init();
}

KeypadLineEdit::KeypadLineEdit( const QString & contents, QWidget * parent, const char * name)
    : QLineEdit(contents, parent, name)
{
    this->Init();
}

void KeypadLineEdit::Init()
{
    cycle_timer = new QTimer();
    shift = false;
    active_cycle = false;
    current_choice = "";
    current_set = "";
    
    connect(cycle_timer, SIGNAL(timeout()), this, SLOT(endCycle()));
}

void KeypadLineEdit::endCycle()
{
    //
    //  The timer ran out or the user pressed a key
    //  outside of the current set of choices
    //
    if(current_choice == "_")       //  Space
    {
        insert(" ");
    } 
    else if (current_choice == "X") //  destructive backspace
    {
        backspace();
    }
    else if(shift)
    {
        insert(current_choice.upper());
    }
    else
    {
        insert(current_choice);
    }
    active_cycle = false;
    current_choice = "";
    current_set = "";
    emit cycleState("",""); 
}

KeypadLineEdit::~KeypadLineEdit()
{
    if(cycle_timer)
    {
        delete cycle_timer;
    }
}

void KeypadLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    //
    //  If you want to mess arround with other ways to allocate
    //  key presses you can just add entried between the quotes
    //

    switch(e->key())
    {
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

        case Key_1:
            cycleKeys("X_-/.?()");
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
        QLineEdit::keyPressEvent(e);
    }
}

void KeypadLineEdit::setCycleTime(float desired_interval)
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


void KeypadLineEdit::cycleKeys(QString cycle_list)
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
        }
        else
        {
            //
            //  Previous cycle was still active, but user moved
            //  to another keypad key
            //
            endCycle();
            active_cycle = true;
            current_choice = cycle_list.left(1);
            current_set = cycle_list;
            cycle_timer->changeInterval(cycle_time);            
        }
                
    }
    else
    {
        //
        //  First press with no cycle of any type active
        //
        active_cycle = true;
        cycle_timer->start(cycle_time, true);
        current_choice = cycle_list.left(1);
        current_set = cycle_list;
    }
    
    QString temp_choice = current_choice;
    QString temp_set = current_set;
    
    if(shift)
    {
        temp_choice = current_choice.upper();
        temp_set = current_set.upper();
    }    
    emit cycleState(temp_choice, temp_set);   // tell the hint box
}

void KeypadLineEdit::toggleShift()
{

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
    emit shiftState(shift);
    emit cycleState(temp_choice, temp_set);   // tell the hint box
    
}

void KeypadLineEdit::focusInEvent(QFocusEvent *e)
{
    emit askParentToChangeHelpText();
    QLineEdit::focusInEvent(e);
}



MythRemoteLineEdit::MythRemoteLineEdit( QWidget * parent, const char * name)
    : QWidget(parent, name)
{
    editor = new KeypadLineEdit(this);
    character_hint = new LineEditHintBox(this);
    shift_hint = new LineEditHintBox(this);
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(const QString &contents, QWidget * parent, const char * name)
    : QWidget(parent, name)
{
    editor = new KeypadLineEdit(contents, this);
    character_hint = new LineEditHintBox(this);
    shift_hint = new LineEditHintBox(this);
    this->Init();
}

void MythRemoteLineEdit::Init()
{
    character_hint->setFrameStyle(QFrame::Panel | QFrame::Raised);
    character_hint->show();
    character_hint->setTextFormat(Qt::RichText);
    character_hint->setAlignment(Qt::AlignCenter);
    character_hint->setFocusPolicy(QWidget::NoFocus);
        
    shift_hint->setFrameStyle(QFrame::Panel | QFrame::Raised);
    shift_hint->show();
    shift_hint->setTextFormat(Qt::RichText);
    shift_hint->setAlignment(Qt::AlignCenter);
    shift_hint->setFocusPolicy(QWidget::NoFocus);

    editor->show();
    editor->setCycleTime(2.0);
    
    connect(editor, SIGNAL(shiftState(bool)), shift_hint, SLOT(showShift(bool)));
    connect(editor, SIGNAL(cycleState(QString, QString)), character_hint, SLOT(showCycle(QString, QString)));
    connect(editor, SIGNAL(textChanged(const QString &)), this, SLOT(textHasChanged(const QString &)));
    connect(editor, SIGNAL(askParentToChangeHelpText()), this, SLOT(honorRequestToChangeHelpText()));
    
    shift_hint->showShift(false);   //  Set intial shifted state
    
}

void MythRemoteLineEdit::textHasChanged(const QString &text)
{
    emit textChanged(text);
}


void MythRemoteLineEdit::setGeometry(int x, int y, int w, int h)
{
    //
    //  Should be a less kludgy way to do this
    //  but I can't for the life of me figure out
    //  how themed menu sets this stuff ???
    //
    QWidget::setGeometry(x, y, w, h);
    

    //  This sucks:
    QFontMetrics fm = fontMetrics();
    int widget_height = fm.height() + 8;    
    int space_above = (h - widget_height) / 2 ;
    editor->setGeometry(0, space_above, w - 130, widget_height);
    character_hint->setGeometry(w - 130 + 5, space_above, 60, widget_height);
    shift_hint->setGeometry(w - 130 + 70, space_above, 60, widget_height);


}

void MythRemoteLineEdit::setCycleTime(float desired_interval)
{
    editor->setCycleTime(desired_interval);
}


void MythRemoteLineEdit::setText(const QString& text) 
{
    int pos = editor->cursorPosition();
    editor->setText(text);
    editor->setCursorPosition(pos);
}

void MythRemoteLineEdit::honorRequestToChangeHelpText(void)
{
    emit changeHelpText(helptext);
}

MythRemoteLineEdit::~MythRemoteLineEdit()
{
    if(editor)
    {
        delete editor;
    }  
    if(character_hint)
    {
        delete character_hint;
    } 
    if(shift_hint)
    {
        delete shift_hint;
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
        origcolor = colorGroup().button();

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
        origcolor = colorGroup().button();

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

MythProgressDialog::MythProgressDialog(const QString& labelText, int totalSteps,
                                       QWidget* parent, const char* name,
                                       bool modal):
     QProgressDialog(labelText, "Cancel", totalSteps, parent, name, modal) {
     QPushButton* mcancelButton = new MythPushButton("Cancel", this);
     this->setCancelButton(mcancelButton);
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
            case 'd': case 'D': emit deletePressed(currentItem()); return;
            case 'p': case 'P': emit playPressed(currentItem()); return;
        }
        if (e->key() == Key_Space && fixspace)
        {
            emit spacePressed(currentItem());
            return;
        }
    }

    QListView::keyPressEvent(e);
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
