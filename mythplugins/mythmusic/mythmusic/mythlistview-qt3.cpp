#include "mythcontext.h"
#include "mythmainwindow.h"

#include "mythlistview-qt3.h"
#include "Q3Header"

Q3MythListView::Q3MythListView(QWidget *parent) : Q3ListView(parent)
{
    viewport()->setPalette(palette());
    horizontalScrollBar()->setPalette(palette());
    verticalScrollBar()->setPalette(palette());
    header()->setPalette(palette());
    header()->setFont(font());

    setAllColumnsShowFocus(TRUE);
}

void Q3MythListView::ensureItemVCentered ( const Q3ListViewItem * i )
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

void Q3MythListView::keyPressEvent(QKeyEvent *e)
{
    if (currentItem() && !currentItem()->isEnabled())
    {
        Q3ListView::keyPressEvent(e);
        return;

    }

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP" && currentItem() == firstChild())
        {
            // Qt::Key_Up at top of list allows focus to move to other widgets
            clearSelection();
            if (!focusNextPrevChild(false))
            {
                // BUT (if we get here) there was no other widget to take focus
                setSelected(currentItem(), true);
            }
        }
        else if (action == "DOWN" && currentItem() == lastItem())
        {
            // Qt::Key_Down at bottom of list allows focus to move to other widgets
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

    Q3ListView::keyPressEvent(e);
}

void Q3MythListView::focusInEvent(QFocusEvent *e)
{
    //
    //  Let the base class do whatever it is
    //  it does
    //

    Q3ListView::focusInEvent(e);

    //
    //  Always highlight the current item
    //  as "Selected" in the Qt sense
    //  (not selected in the box ticked sense
    //

    setSelected(currentItem(), true);
}
