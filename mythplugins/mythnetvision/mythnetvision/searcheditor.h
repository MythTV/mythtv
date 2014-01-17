#ifndef SEARCHEDITOR_H
#define SEARCHEDITOR_H

// Qt headers
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// MythTV headers
#include <mythscreentype.h>
#include <netgrabbermanager.h>
#include <mythscreentype.h>
#include <mythprogressdialog.h>

class MythUIButtonList;

/** \class SearchEdit
 *  \brief Modify subscribed search grabbers.
 */
class SearchEditor : public MythScreenType
{
    Q_OBJECT

  public:
    SearchEditor(MythScreenStack *parent,
               const QString &name = "SearchEditor");
   ~SearchEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    void loadData(void);
    void fillGrabberButtonList();
    void parsedData();

    GrabberScript::scriptList m_grabberList;
    MythUIButtonList *m_grabbers;
    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;

    QNetworkAccessManager *m_manager;
    QNetworkReply         *m_reply;
    bool                   m_changed;

  protected:
    void createBusyDialog(QString title);

  signals:
    void itemsChanged(void);

  public slots:
    void toggleItem(MythUIButtonListItem *item);
    void slotLoadedData(void);
};

#endif /* SEARCHEDITOR_H */
