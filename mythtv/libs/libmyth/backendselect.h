#ifndef __BACKENDSELECT_H__
#define __BACKENDSELECT_H__

#include "mythdialogs.h"
#include "mythwidgets.h"

#include "libmythupnp/upnpdevice.h"

class ListBoxDevice : public QListBoxText
{
    public:

        ListBoxDevice(QListBox *list, const QString &name, DeviceLocation *dev)
            : QListBoxText(list, name)
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

typedef QMap< QString, ListBoxDevice *> ItemMap;


class BackendSelect : public MythDialog 
{
    Q_OBJECT

    public:
                 BackendSelect(MythMainWindow *parent, DatabaseParams *params);
        virtual ~BackendSelect();

        void     customEvent(QCustomEvent *e);

        QString  m_PIN;
        QString  m_USN;


    public slots:

        void Accept    (void);   ///< Linked to the OK button
        void Manual    (void);   ///< Linked to 'Configure Manually' button
        //void Search    (void);


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
