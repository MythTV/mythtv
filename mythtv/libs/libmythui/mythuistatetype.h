#ifndef MYTHUI_STATETYPE_H_
#define MYTHUI_STATETYPE_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qvaluevector.h>

#include "mythuitype.h"
#include "mythimage.h"

// Image class that displays one of an array of images (either predefined states
// or by typename).  Displays nothing if told to display a non-existant state
class MythUIStateType : public MythUIType
{
  public:
    enum StateType { None = 0, Off, Half, Full }; // Can be used for tri-state checks, two state toggles, etc.

    MythUIStateType(MythUIType *parent, const char *name);
   ~MythUIStateType();

    void SetShowEmpty(bool showempty) { m_ShowEmpty = showempty; }

    bool AddImage(const QString &name, MythImage *image);
    bool AddImage(StateType type, MythImage *image);

    bool AddObject(const QString &name, MythUIType *object);
    bool AddObject(StateType type, MythUIType *object);

    bool DisplayState(const QString &name);
    bool DisplayState(StateType type);

    void ClearImages();

  protected:
    void ClearMaps();

    QMap<QString, MythUIType *> m_ObjectsByName;
    QMap<int, MythUIType *> m_ObjectsByState;

    MythUIType *m_CurrentState;

    bool m_ShowEmpty;
};

#endif
