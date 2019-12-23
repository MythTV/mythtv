#ifndef SEARCHEDITOR_H
#define SEARCHEDITOR_H

#include "neteditorbase.h"

/** \class SearchEdit
 *  \brief Modify subscribed search grabbers.
 */
class SearchEditor : public NetEditorBase
{
    Q_OBJECT

  public:
    explicit SearchEditor(MythScreenStack *parent,
                 const QString &name = "SearchEditor")
        : NetEditorBase(parent, name) {}

  protected:
    bool InsertInDB(GrabberScript *script) override; // NetEditorBase
    bool RemoveFromDB(GrabberScript *script) override; // NetEditorBase
    bool FindGrabberInDB(const QString &filename) override; // NetEditorBase
    bool Matches(bool search, bool tree) override; // NetEditorBase
};

#endif /* SEARCHEDITOR_H */
