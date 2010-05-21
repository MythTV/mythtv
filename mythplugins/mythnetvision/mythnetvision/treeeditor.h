#ifndef TREEEDITOR_H
#define TREEEDITOR_H

// Qt headers
#include <QMutex>
#include <QString>
#include <QDomDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>

// MythTV headers
#include <mythscreentype.h>

#include "netgrabbermanager.h"

class MythUIButtonList;

/** \class TreeEdit
 *  \brief Modify subscribed trees.
 */
class TreeEditor : public MythScreenType
{
    Q_OBJECT

  public:
    TreeEditor(MythScreenStack *parent,
               const QString name = "TreeEditor");
   ~TreeEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    void loadData(void);
    GrabberScript::scriptList fillGrabberList();
    void fillGrabberButtonList();
    mutable QMutex  m_lock;

    GrabberScript::scriptList m_grabberList;
    MythUIButtonList *m_grabbers;

    bool m_changed;

  signals:
    void itemsChanged(void);

  public slots:
    void toggleItem(MythUIButtonListItem *item);
};

#endif /* TREEEDITOR_H */
