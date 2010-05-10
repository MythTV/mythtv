#ifndef SEARCHEDITOR_H
#define SEARCHEDITOR_H

// Qt headers
#include <QString>

// MythTV headers
#include <mythscreentype.h>

#include "grabbermanager.h"

class MythUIButtonList;

/** \class SearchEdit
 *  \brief Modify subscribed search grabbers.
 */
class SearchEditor : public MythScreenType
{
    Q_OBJECT

  public:
    SearchEditor(MythScreenStack *parent,
               const QString name = "SearchEditor");
   ~SearchEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    void loadData(void);
    GrabberScript::scriptList fillGrabberList();
    void fillGrabberButtonList();

    GrabberScript::scriptList m_grabberList;
    MythUIButtonList *m_grabbers;

    bool m_changed;

  signals:
    void itemsChanged(void);

  public slots:
    void toggleItem(MythUIButtonListItem *item);
};

#endif /* SEARCHEDITOR_H */
