#include <qstyle.h>
#include <qpainter.h>
#include <qcursor.h>

#include <iostream>
using namespace std;

#include "mythwidgets.h"
#include "mythcontext.h"

void MyToolButton::drawButton( QPainter * p )
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

void MyPushButton::drawButton( QPainter * p )
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

MyDialog::MyDialog(MythContext *context, QWidget *parent, const char *name,
                   bool modal)
        : QDialog(parent, name, modal)
{
    m_context = context;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(context->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    context->ThemeWidget(this);
}

void MyDialog::Show(void)
{
    showFullScreen();
    setActiveWindow();
}

MyListView::MyListView(QWidget *parent)
          : QListView(parent)
{
    allowkeypress = true;

    viewport()->setPalette(palette());
    horizontalScrollBar()->setPalette(palette());
    verticalScrollBar()->setPalette(palette());
    header()->setPalette(palette());
    header()->setFont(font());

    setAllColumnsShowFocus(TRUE);
}

void MyListView::keyPressEvent(QKeyEvent *e)
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
            case Key_Space: emit spacePressed(currentItem()); return;
        }
    }

    QListView::keyPressEvent(e);
}

