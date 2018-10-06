/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#include "loglist.h"

LogList::LogList() : ListBoxSetting(this), idx(0)
{
    setSelectionMode(MythListBox::NoSelection);
}

void LogList::AppendLine(const QString &text)
{
    addSelection(text, QString::number(idx));
    setCurrentItem(idx);
    ++idx;
}
