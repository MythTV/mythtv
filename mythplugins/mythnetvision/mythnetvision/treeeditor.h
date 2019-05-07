#ifndef TREEEDITOR_H
#define TREEEDITOR_H

#include "neteditorbase.h"

/** \class TreeEdit
 *  \brief Modify subscribed trees.
 */
class TreeEditor : public NetEditorBase
{
    Q_OBJECT

  public:
    TreeEditor(MythScreenStack *parent, const QString &name = "TreeEditor")
        : NetEditorBase(parent, name) {}

  protected:
    bool InsertInDB(GrabberScript *script) override; // NetEditorBase
    bool RemoveFromDB(GrabberScript *script) override; // NetEditorBase
    bool FindGrabberInDB(const QString &filename) override; // NetEditorBase
    bool Matches(bool search, bool tree) override; // NetEditorBase
};

#endif /* TREEEDITOR_H */
