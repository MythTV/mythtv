#include <libmythbase/netutils.h>

#include "treeeditor.h"

bool TreeEditor::InsertInDB(GrabberScript *script)
{
    return insertTreeInDB(script, VIDEO_FILE);
}

bool TreeEditor::RemoveFromDB(GrabberScript *script)
{
    bool removed = removeTreeFromDB(script);

    if (removed && !isTreeInUse(script->GetCommandline()))
        clearTreeItems(script->GetCommandline());

    return removed;
}

bool TreeEditor::FindGrabberInDB(const QString &filename)
{
    return findTreeGrabberInDB(filename, VIDEO_FILE);
}

bool TreeEditor::Matches(bool /*search*/, bool tree)
{
    return tree;
}
