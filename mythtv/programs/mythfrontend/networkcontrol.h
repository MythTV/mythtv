#ifndef NETWORKCONTROL_H_
#define NETWORKCONTROL_H_

#include <deque>
using namespace std;

#include <QWaitCondition>
#include <QStringList>
#include <QTcpSocket>
#include <QRunnable>
#include <QMutex>
#include <QEvent>

#include "mthread.h"
#include "serverpool.h"

class MainServer;
class QTextStream;

// Locking order
// clientLock -> ncLock
//            -> nrLock

class NetworkControlClient : public QObject
{
    Q_OBJECT
  public:
    NetworkControlClient(QTcpSocket *);
   ~NetworkControlClient();

    QTcpSocket  *getSocket()     { return m_socket; }
    QTextStream *getTextStream() { return m_textStream; }

  signals:
    void commandReceived(QString&);

  public slots:
    void readClient();

  private:
    QTcpSocket  *m_socket;
    QTextStream *m_textStream;
};

class NetworkCommand : public QObject
{
    Q_OBJECT
  public:
    NetworkCommand(NetworkControlClient *cli, QString c)
    {
        m_command = c.trimmed();
        m_client = cli;
        m_args = m_command.simplified().split(" ");
    }

    NetworkCommand &operator=(NetworkCommand const &nc)
    {
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
    NetworkControlClient *m_client;
    QStringList           m_args;
};

class NetworkControlCloseEvent : public QEvent
{
  public:
    NetworkControlCloseEvent(NetworkControlClient *ncc) :
        QEvent(kEventType), m_networkControlClient(ncc) {}

    NetworkControlClient *getClient() { return m_networkControlClient; }

    static Type kEventType;

  private:
    NetworkControlClient * m_networkControlClient;
};

class NetworkControl;

class NetworkControl : public ServerPool, public QRunnable
{
    Q_OBJECT

  public:
    NetworkControl();
    ~NetworkControl();

  private slots:
    void newConnection(QTcpSocket *socket);
    void receiveCommand(QString &command);
    void deleteClient(void);

  protected:
    void run(void); // QRunnable

  private:
    QString processJump(NetworkCommand *nc);
    QString processKey(NetworkCommand *nc);
    QString processLiveTV(NetworkCommand *nc);
    QString processPlay(NetworkCommand *nc, int clientID);
    QString processQuery(NetworkCommand *nc);
    QString processSet(NetworkCommand *nc);
    QString processMessage(NetworkCommand *nc);
    QString processNotification(NetworkCommand *nc);
    QString processHelp(NetworkCommand *nc);

    void notifyDataAvailable(void);
    void sendReplyToClient(NetworkControlClient *ncc, QString &reply);
    void customEvent(QEvent *e);

    QString listRecordings(QString chanid = "", QString starttime = "");
    QString listSchedule(const QString& chanID = "") const;
    QString listChannels(const uint start, const uint limit) const;
    QString saveScreenshot(NetworkCommand *nc);

    void processNetworkControlCommand(NetworkCommand *nc);

    void deleteClient(NetworkControlClient *ncc);

    QString prompt;
    bool gotAnswer;
    QString answer;
    QMap <QString, QString> jumpMap;
    QMap <QString, int> keyMap;
    QMap <int, QString> keyTextMap;

    mutable QMutex  clientLock;
    QList<NetworkControlClient*> clients;

    QList<NetworkCommand*> networkControlCommands; // protected by ncLock
    QMutex ncLock;
    QWaitCondition ncCond; // protected by ncLock

    QList<NetworkCommand*> networkControlReplies;
    QMutex nrLock;

    MThread *commandThread;
    bool stopCommandThread; // protected by ncLock
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

