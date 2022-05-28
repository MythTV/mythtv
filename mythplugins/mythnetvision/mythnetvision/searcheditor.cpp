#include <libmythbase/netutils.h>

#include "searcheditor.h"

bool SearchEditor::InsertInDB(GrabberScript *script)
{
    return insertSearchInDB(script, VIDEO_FILE);
}

bool SearchEditor::RemoveFromDB(GrabberScript *script)
{
    return removeSearchFromDB(script);
}

bool SearchEditor::FindGrabberInDB(const QString &filename)
{
    return findSearchGrabberInDB(filename, VIDEO_FILE);
}

bool SearchEditor::Matches(bool search, bool /*tree*/)
{
    return search;
}
