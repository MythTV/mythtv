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
#include <netgrabbermanager.h>
#include <mythscreentype.h>
#include <mythprogressdialog.h>

class MythUIButtonList;

/** \class TreeEdit
 *  \brief Modify subscribed trees.
 */
class TreeEditor : public MythScreenType
{
    Q_OBJECT

  public:
    TreeEditor(MythScreenStack *parent,
               const QString &name = "TreeEditor");
   ~TreeEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    void loadData(void);
    void fillGrabberButtonList();
    mutable QMutex  m_lock;
    void parsedData();

    GrabberScript::scriptList m_grabberList;
    MythUIButtonList *m_grabbers;
    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;

    QNetworkAccessManager *m_manager;
    QNetworkReply         *m_reply;
    bool m_changed;

  protected:
    void createBusyDialog(QString title);

  signals:
    void itemsChanged(void);

  public slots:
    void slotLoadedData(void);
    void toggleItem(MythUIButtonListItem *item);
};

#endif /* TREEEDITOR_H */
