#ifndef __BACKENDSELECT_H__
#define __BACKENDSELECT_H__

#include "mythdialogs.h"
#include "mythwidgets.h"

#include "libmythupnp/upnpdevice.h"

// define this to add a search button. In Nigel's testing, an extra ssdp
// doesn't get any extra responses, so it is disabled by default.
//#define SEARCH_BUTTON

class DatabaseParams;

class ListBoxDevice : public Q3ListBoxText
{
  public:

    ListBoxDevice(Q3ListBox *list, const QString &name, DeviceLocation *dev)
        : Q3ListBoxText(list, name)
    {
        if ((m_dev = dev) != NULL)
            m_dev->AddRef();
    }

    virtual ~ListBoxDevice()
    {
        if (m_dev != NULL)
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
    void Accept    (void);   ///< Linked to the OK button
    void Manual    (void);   ///< Linked to 'Configure Manually' button
#ifdef SEARCH_BUTTON
    void Search    (void);
#endif


  protected:
    void AddItem    (DeviceLocation *dev);
    bool Connect    (DeviceLocation *dev);
    void CreateUI   (void);
    void FillListBox(void);
    void RemoveItem (QString URN);

    DatabaseParams *m_DBparams;
    ItemMap         m_devices;
    MythMainWindow *m_parent;
    MythListBox    *m_backends;
};

#endif
