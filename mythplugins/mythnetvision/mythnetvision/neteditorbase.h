#ifndef NETEDITORBASE_H
#define NETEDITORBASE_H

// Qt headers
#include <QDomDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

// MythTV headers
#include <libmythbase/netgrabbermanager.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreentype.h>

class MythUIButtonList;

/** \class TreeEdit
 *  \brief Modify subscribed trees.
 */
class NetEditorBase : public MythScreenType
{
    Q_OBJECT

  public:
    NetEditorBase(MythScreenStack *parent, const QString &name);
    ~NetEditorBase() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    void LoadData(void);
    void FillGrabberButtonList();
    void ParsedData();

    GrabberScript::scriptList m_grabberList;
    MythUIButtonList      *m_grabbers   {nullptr};
    MythUIBusyDialog      *m_busyPopup  {nullptr};
    MythScreenStack       *m_popupStack {nullptr};

    QNetworkAccessManager *m_manager    {nullptr};
    QNetworkReply         *m_reply      {nullptr};
    bool                   m_changed    {false};

  protected:
    void CreateBusyDialog(const QString& title);

    virtual bool InsertInDB(GrabberScript *script) = 0;
    virtual bool RemoveFromDB(GrabberScript *script) = 0;
    virtual bool FindGrabberInDB(const QString &filename) = 0;
    virtual bool Matches(bool search, bool tree) = 0;

  signals:
    void ItemsChanged(void);

  public slots:
    void SlotLoadedData(void);
    void ToggleItem(MythUIButtonListItem *item);
};

#endif /* NETEDITORBASE_H */
