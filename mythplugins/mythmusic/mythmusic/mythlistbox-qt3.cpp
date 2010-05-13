#include <QKeyEvent>

#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythlistbox-qt3.h"

Q3MythListBox::Q3MythListBox(QWidget* parent): Q3ListBox(parent)
{
}

void Q3MythListBox::polish(void)
{
    Q3ListBox::polish();

    QPalette pal = palette();
    QColorGroup::ColorRole role = QColorGroup::Highlight;
    pal.setColor(QPalette::Active, role, pal.active().button());
    pal.setColor(QPalette::Inactive, role, pal.active().button());
    pal.setColor(QPalette::Disabled, role, pal.active().button());

    setPalette(pal);
}

void Q3MythListBox::setCurrentItem(const QString& matchText, bool caseSensitive,
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
                if (text(i).toLower().startsWith(matchText.toLower()))
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
                if (text(i).toLower() == matchText.toLower())
                {
                    setCurrentItem(i);
                    break;
                }
            }
        }
    }
}

void Q3MythListBox::keyPressEvent(QKeyEvent* e)
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
                if (currentItem() == (int) count() - 1)
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

            QKeyEvent ev(QEvent::KeyPress, key, 0, Qt::NoButton);
            Q3ListBox::keyPressEvent(&ev);
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
                setTopItem(nextItem);
            setCurrentItem(nextItem);
            handled = true;
        }
        else if (action == "PREVVIEW")
        {
            int nextItem = currentItem();
            if (nextItem > 0)
                nextItem--;
            while (nextItem > 0 && text(nextItem)[0] == ' ')
                nextItem--;
            if (!itemVisible(nextItem))
                setTopItem(nextItem);
            setCurrentItem(nextItem);
            handled = true;
        }
        else if (action == "NEXTVIEW")
        {
            int nextItem = currentItem();
            if (nextItem < (int)count() - 1)
                nextItem++;
            while (nextItem < (int)count() - 1 && text(nextItem)[0] == ' ')
                nextItem++;
            if (!itemVisible(nextItem))
                setTopItem(nextItem);
            setCurrentItem(nextItem);
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

    if (!handled)
        e->ignore();
}

void Q3MythListBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void Q3MythListBox::focusOutEvent(QFocusEvent *e)
{
    QPalette pal = palette();
    QColorGroup::ColorRole role = QColorGroup::Highlight;
    pal.setColor(QPalette::Active, role, pal.active().button());
    pal.setColor(QPalette::Inactive, role, pal.active().button());
    pal.setColor(QPalette::Disabled, role, pal.active().button());

    setPalette(pal);
    Q3ListBox::focusOutEvent(e);
}

void Q3MythListBox::focusInEvent(QFocusEvent *e)
{
    this->unsetPalette();

    emit changeHelpText(helptext);
    Q3ListBox::focusInEvent(e);
}
