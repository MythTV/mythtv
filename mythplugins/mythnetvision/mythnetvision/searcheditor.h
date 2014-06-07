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
    SearchEditor(MythScreenStack *parent,
               const QString &name = "SearchEditor");

  protected:
    bool InsertInDB(GrabberScript *script);
    bool RemoveFromDB(GrabberScript *script);
    bool FindGrabberInDB(const QString &filename);
    bool Matches(bool search, bool tree);
};

#endif /* SEARCHEDITOR_H */
