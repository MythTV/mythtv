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

MythDialog::MythDialog(MythContext *context, QWidget *parent, const char *name,
                       bool modal)
          : QDialog(parent, name, modal)
{
    m_context = context;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int x, y, w, h;
    GetMythTVGeometry(context, qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);

    setGeometry(x, y, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(context->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    context->ThemeWidget(this);
}

void MythDialog::Show(void)
{
    showFullScreen();
    setActiveWindow();
}

MythProgressDialog::MythProgressDialog(const QString& labelText, int totalSteps,
                                       QWidget* parent=NULL, const char* name=0,
                                       bool modal=FALSE):
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

MythImageSelector::MythImageSelector(QWidget* parent, const char* name):
        QWidget(parent, name) {

    fgcolor = paletteForegroundColor();
    highlightcolor = fgcolor;

    topleft = 0;
    current = 0;
    isSet = false;

    rows = 1;
    columns = 3;

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);

    setFocusPolicy(StrongFocus);
}

MythImageSelector::~MythImageSelector()
{
    clear();
}

void MythImageSelector::insertItem(const QString& label, QImage* image) {
    Selection sel;
    sel.label = label;
    sel.image = *image; // copy-on-write
    selections.push_back(sel);

    if (selections.size() == 1)
        setCurrentItem(0);
}

void MythImageSelector::setCurrentItem(int item) {
    if ( (!isSet || (unsigned)item != current)
         &&
         (unsigned int)item < selections.size() ) {
        isSet = true;
        current = item;
        emit selectionChanged(current);
        update();
    }
}

void MythImageSelector::clear(void) {
    selections.clear();
    current = 0;
}


void MythImageSelector::paintEvent(QPaintEvent* e)
{
    (void)e;

    if (current >= selections.size())
        return;

    QRect r = QRect(0, 0, width(), height());
    QPainter p(this);

    QPixmap pix(r.size());
    pix.fill(this, r.topLeft());

    QPainter tmp;
    tmp.begin(&pix, this);

    tmp.setFont(font());
    tmp.setPen(QPen(fgcolor, 2 * r.width() / 800));

//     cout << "I think I am " << r.width() << "x" << r.height() << endl;

    unsigned int thumbw = r.width() / (columns + 1);
    unsigned int thumbh = r.height() / (rows + 1);

//     cout << "I want " << thumbw << "x" << thumbh << " pixmaps\n";

    unsigned int spacew = thumbw / (columns + 1);
    unsigned int spaceh = (thumbh) / (rows + 1);
//     cout << "Spaced " << spacew << "x" << spaceh << " apart\n";

    unsigned int currow = current / columns;
    unsigned int curcol = current % columns;

    unsigned int curpos = topleft;
    unsigned int border = 4 * r.width() / 800;

    for (unsigned int y = 0; y < rows; y++)
    {
        int ypos = spaceh * (y + 1) + thumbh * y;

        for (unsigned int x = 0; x < columns; x++)
        {
             if (curpos >= selections.size())
                 continue;

             Selection* sel = &(selections[curpos]);

             int xpos = spacew * (x + 1) + thumbw * x;

             QImage tmpimage = sel->image;
             tmpimage = tmpimage.smoothScale(thumbw, thumbh, QImage::ScaleMin);

//              cout << "pixmap came out "
//                   << tmpimage.width() << "x" << tmpimage.height()
//                   << endl;
             QPixmap tmppixmap(tmpimage);

             int ximagepos = xpos + (thumbw - tmppixmap.width()) / 2;
             int yimagepos = ypos + (thumbh - tmppixmap.height()) / 2;
             tmp.drawPixmap(ximagepos, yimagepos, tmppixmap);


             tmp.drawText(xpos, ypos + thumbh, thumbw, spaceh,
                          AlignVCenter | AlignCenter, sel->label);

             if (currow == y && curcol == x)
             {
                 tmp.setPen(QPen(highlightcolor, border));
                 tmp.drawRect(xpos, ypos, // - 10 * r.height() / 600,
                              thumbw, thumbh + spaceh);
                 //tmp.setPen(QPen(fgcolor, 2)); // * r.width() / 800));
             }
             curpos++;
        }
    }
 
    tmp.flush();
    tmp.end();

    p.drawPixmap(r.topLeft(), pix);

}

void MythImageSelector::keyPressEvent(QKeyEvent* e) {
    int next = current;
    switch (e->key()) {
    case Key_Up:
        focusNextPrevChild(false);
        break;
    case Key_Down:
        focusNextPrevChild(true);
        break;

    case Key_Left:
        if (current > 0)
            next = current - 1;
        break;

    case Key_Right:
        if (current < selections.size()-1)
            next = current + 1;
        break;

    default:
            e->ignore();
            return;
    }

    setCurrentItem(next);
}
