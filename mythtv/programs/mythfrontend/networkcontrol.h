#ifndef NETWORKCONTROL_H_
#define NETWORKCONTROL_H_

#include <deque>

#include <QWaitCondition>
#include <QStringList>
#include <QTcpSocket>
#include <QRunnable>
#include <QMutex>
#include <QEvent>
#include <QRecursiveMutex>

#include "libmythbase/mthread.h"
#include "libmythbase/serverpool.h"

class MainServer;
class QTextStream;

// Locking order
// clientLock -> ncLock
//            -> nrLock

class NetworkControlClient : public QObject
{
    Q_OBJECT
  public:
    explicit NetworkControlClient(QTcpSocket *s);
   ~NetworkControlClient() override;

    QTcpSocket  *getSocket()     { return m_socket; }
    QTextStream *getTextStream() { return m_textStream; }

  signals:
    void commandReceived(QString&);

  public slots:
    void readClient();

  private:
    QTcpSocket  *m_socket     {nullptr};
    QTextStream *m_textStream {nullptr};
};

class NetworkCommand : public QObject
{
    Q_OBJECT
  public:
    NetworkCommand(NetworkControlClient *cli, const QString& c)
        : m_command(c.trimmed()),
          m_client(cli)
    {
        m_args = m_command.simplified().split(" ");
    }

    NetworkCommand &operator=(NetworkCommand const &nc)
    {
        if (this == &nc)
            return *this;
        m_command = nc.m_command;
        m_client = nc.m_client;
        m_args = m_command.simplified().split(" ");
        return *this;
    }

    QString               getCommand()    { return m_command; }
    NetworkControlClient *getClient()     { return m_client; }
    QString               getArg(int arg) { return m_args[arg]; }
    int                   getArgCount()   { return m_args.size(); }
    QString               getFrom(int arg);

  private:
    QString               m_command;
    NetworkControlClient *m_client {nullptr};
    QStringList           m_args;
};

class NetworkControlCloseEvent : public QEvent
{
  public:
    explicit NetworkControlCloseEvent(NetworkControlClient *ncc) :
        QEvent(kEventType), m_networkControlClient(ncc) {}

    NetworkControlClient *getClient() { return m_networkControlClient; }

    static const Type kEventType;

  private:
    NetworkControlClient *m_networkControlClient {nullptr};
};

class NetworkControl;
class MythUIType;

class NetworkControl : public ServerPool, public QRunnable
{
    Q_OBJECT

  public:
    NetworkControl();
    ~NetworkControl() override;

  private slots:
    void newControlConnection(QTcpSocket *client);
    void receiveCommand(QString &command);
    void deleteClient(void);

  protected:
    void run(void) override; // QRunnable

  private:
    QString processJump(NetworkCommand *nc);
    QString processKey(NetworkCommand *nc);
    QString processLiveTV(NetworkCommand *nc);
    QString processPlay(NetworkCommand *nc, int clientID);
    QString processQuery(NetworkCommand *nc);
    static QString processSet(NetworkCommand *nc);
    static QString processMessage(NetworkCommand *nc);
    static QString processNotification(NetworkCommand *nc);
    QString processTheme(NetworkCommand *nc);
    QString processHelp(NetworkCommand *nc);

    void notifyDataAvailable(void);
    void sendReplyToClient(NetworkControlClient *ncc, const QString &reply);
    void customEvent(QEvent *e) override; // QObject

    static QString listRecordings(const QString& chanid = "", const QString& starttime = "");
    static QString listSchedule(const QString& chanID = "") ;
    static QString listChannels(uint start, uint limit) ;
    static QString saveScreenshot(NetworkCommand *nc);

    void processNetworkControlCommand(NetworkCommand *nc);

    void deleteClient(NetworkControlClient *ncc);

    static QString getWidgetType(MythUIType *type);

    QString m_prompt             {"# "};
    bool    m_gotAnswer          {false};
    QString m_answer             {""};
    QMap <QString, QString> m_jumpMap;
    QMap <QString, int>     m_keyMap;
    QMap <int, QString>     m_keyTextMap;

    mutable QRecursiveMutex  m_clientLock;
    QList<NetworkControlClient*> m_clients;

    QList<NetworkCommand*> m_networkControlCommands; // protected by ncLock
    QMutex m_ncLock;
    QWaitCondition m_ncCond; // protected by ncLock

    QList<NetworkCommand*> m_networkControlReplies;
    QMutex m_nrLock;

    MThread *m_commandThread     {nullptr};
    bool m_stopCommandThread     {false}; // protected by ncLock
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

