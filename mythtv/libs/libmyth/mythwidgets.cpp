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
    case Key_Space:
        popup();
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
        e->ignore();
    default:
        QLineEdit::keyPressEvent(e);
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

MythDialog::MythDialog(MythContext *context, QWidget *parent, const char *name,
                       bool modal)
          : QDialog(parent, name, modal)
{
    m_context = context;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int x, y, w, h;
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);

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

void MythWizard::showPage(QWidget* page) {
    QWizard::showPage(page);

    if (indexOf(page) == pageCount()-1) {
        // last page
        finishButton()->setEnabled(TRUE);
        finishButton()->setFocus();
    }
}
