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
    TreeEditor(MythScreenStack *parent, const QString &name = "TreeEditor");

  protected:
    bool InsertInDB(GrabberScript *script);
    bool RemoveFromDB(GrabberScript *script);
    bool FindGrabberInDB(const QString &filename);
    bool Matches(bool search, bool tree);
};

#endif /* TREEEDITOR_H */
