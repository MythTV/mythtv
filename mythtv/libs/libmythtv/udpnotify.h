#ifndef UDPNOTIFY_H
#define UDPNOTIFY_H

// C++ headers
#include <vector>
using namespace std;

// POSIC headers
#include <pthread.h>

// Qt headers
#include <QString>
#include <QObject>
#include <QMutex>
#include <QMap>

class TV;
class OSD;
class UDPNotifyOSDTypeText;
class QByteArray;
class QUdpSocket;
class QDomElement;

class UDPNotifyOSDSet
{
  public:
    UDPNotifyOSDSet(const QString &name, uint timeout);
    QString GetName(void) const;
    uint    GetTimeout(void) const;

    void SetTimeout(uint timeout_in_seconds);
    void SetType(const QString &name, const QString &value);
    void ResetTypes(void);

    void Lock(void)   const { m_lock.lock();   }
    void Unlock(void) const { m_lock.unlock(); }

    // NOTE: You must call Lock()/Unlock() around use of the iterator
    typedef QMap<QString,QString>::const_iterator const_iterator;
    const_iterator begin() const { return m_typesMap.begin(); }
    const_iterator end()   const { return m_typesMap.end(); }

  private:
    mutable QMutex        m_lock;
    QString               m_name;
    uint                  m_timeout;
    QMap<QString,QString> m_typesMap;
};

typedef QMap<QString, UDPNotifyOSDSet*> UDPNotifyOSDSetMap;

class UDPNotify : public QObject
{
    Q_OBJECT

  public:
    UDPNotify(uint udp_port);

  signals:
    void AddUDPNotifyEvent(const QString &name, const UDPNotifyOSDSet*);
    void ClearUDPNotifyEvents(void);

  public slots:
    virtual void deleteLater(void);

  private slots:
    void ReadPending(void);

  private:
    ~UDPNotify(void) { TeardownAll(); } // call deleteLater() instead

    void Process(const QByteArray &buf);
    void Set(UDPNotifyOSDSet *set, QString name);
    void ClearContainer(UDPNotifyOSDSet *container);

    UDPNotifyOSDSet *ParseContainer(QDomElement &element);
    void ParseTextArea(UDPNotifyOSDSet *container, QDomElement &element);
    QString GetFirstText(QDomElement &element);

    void TeardownAll(void);

  private:
    /// Socket Device for getting UDP datagrams
    QUdpSocket            *m_socket;
    uint                   m_db_osd_udpnotify_timeout;
    UDPNotifyOSDSetMap     m_sets;
};

#endif // UDPNOTIFY_H
