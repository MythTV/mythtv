#ifndef UDPNOTIFY_H
#define UDPNOTIFY_H

#include <qstring.h>
#include <qmap.h>
#include <pthread.h>
#include <qvaluevector.h>
#include <qvaluelist.h>
#include <qdom.h>

#include <qsocketdevice.h>
#include <qsocketnotifier.h>
#include <qhostaddress.h>

#include <qobject.h>
#include <vector>
using namespace std;


class TV;
class OSD;
class UDPNotifyOSDTypeText;

class UDPNotifyOSDSet
{
  public:
    UDPNotifyOSDSet(const QString &name);
   ~UDPNotifyOSDSet();

    UDPNotifyOSDTypeText *GetType(const QString &name);
    void AddType(UDPNotifyOSDTypeText *type, QString name);
    void ResetTypes(void);
    QString GetName(void);
    vector<UDPNotifyOSDTypeText *> *GetTypeList();

  private:
    QString m_name;
    QMap<QString, UDPNotifyOSDTypeText *> typesMap;
    vector<UDPNotifyOSDTypeText *> *allTypes;
};

class UDPNotifyOSDTypeText
{
  public:
    UDPNotifyOSDTypeText(const QString &name, const QString &text);
    ~UDPNotifyOSDTypeText();
    void SetText(const QString &text);
    QString GetName(void);
    QString GetText(void);

  private:
    QString m_name;
    QString m_text;
};

class UDPNotify : public QObject
{
  Q_OBJECT

  public:
    UDPNotify(TV *tv, int udp_port);
   ~UDPNotify(void);

  protected slots:
    virtual void incomingData(int socket);

  private:
    int m_udp_port;
    QHostAddress bcastaddr;
    TV *m_tv;
    OSD *osd;

    QMap<QString, UDPNotifyOSDSet *> setMap;
    vector<UDPNotifyOSDSet *> *setList;

    QDomDocument doc;

    // Socket Device for UDP communication.
    QSocketDevice *qsd;
    // Notifier, signals available data on socket.
    QSocketNotifier *qsn;

    void AddSet(UDPNotifyOSDSet *set, QString name);
    UDPNotifyOSDSet *GetSet(const QString &text);
    UDPNotifyOSDSet *parseContainer(QDomElement &element);
    void parseTextArea(UDPNotifyOSDSet *container, QDomElement &element);
    QString getFirstText(QDomElement &element);

    void ClearContainer(UDPNotifyOSDSet *container);
};

#endif

