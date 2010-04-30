#ifndef __BACKENDSELECT_H__
#define __BACKENDSELECT_H__

#include <QListWidget>

#include "mythdialogs.h"
#include "upnpdevice.h"

// define this to add a search button. In Nigel's testing, an extra ssdp
// doesn't get any extra responses, so it is disabled by default.
//#define SEARCH_BUTTON

struct DatabaseParams;

class ListBoxDevice : public QListWidgetItem
{
  public:

    ListBoxDevice(QListWidget *parent, const QString &name, DeviceLocation *dev)
        : QListWidgetItem(name, parent), m_dev(dev)
    {
        if (m_dev)
            m_dev->AddRef();
    }

    virtual ~ListBoxDevice()
    {
        if (m_dev)
            m_dev->Release();
    }

    DeviceLocation *m_dev;
};

typedef QMap <QString, ListBoxDevice *> ItemMap;


class BackendSelect : public MythDialog 
{
    Q_OBJECT

  public:
             BackendSelect(MythMainWindow *parent, DatabaseParams *params);
    virtual ~BackendSelect();

    void     customEvent(QEvent *e);

    QString  m_PIN;
    QString  m_USN;


  public slots:
    void Accept(QListWidgetItem *);  ///< Invoked by mouse click
    void Accept(void);   ///< Linked to the OK button
    void Manual(void);   ///< Linked to 'Configure Manually' button
#ifdef SEARCH_BUTTON
    void Search(void);
#endif


  protected:
    void AddItem    (DeviceLocation *dev);
    bool Connect    (DeviceLocation *dev);
    void CreateUI   (void);
    void FillListBox(void);
    void RemoveItem (QString URN);
    bool eventFilter(QObject *obj, QEvent *event);
    bool TryDBfromURL(const QString &error, QString URL);

    DatabaseParams *m_DBparams;
    ItemMap         m_devices;
    MythMainWindow *m_parent;
    QListWidget    *m_backends;
};

#endif
