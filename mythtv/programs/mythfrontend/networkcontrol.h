#ifndef NETWORKCONTROL_H_
#define NETWORKCONTROL_H_

#include <deque>
using namespace std;

#include <pthread.h>

#include <QEvent>
#include <QWaitCondition>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>

class MainServer;
class QTextStream;

// Locking order
// clientLock -> ncLock
//            -> nrLock

const int kNetworkControlCloseEvent = 35672;

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
        m_command = c;
        m_client = cli;
    }
    
    NetworkCommand &operator=(NetworkCommand const &nc)
    {
        m_command = nc.m_command;
        m_client = nc.m_client;
        return *this;
    }

    QString               getCommand() { return m_command; }
    NetworkControlClient *getClient()  { return m_client; }

  private:
    QString               m_command;
    NetworkControlClient *m_client;
};

class NetworkControlCloseEvent : public QEvent
{
  public:
    NetworkControlCloseEvent(NetworkControlClient * ncc)
        : QEvent((QEvent::Type)kNetworkControlCloseEvent),
          m_networkControlClient(ncc) {};

    NetworkControlClient *getClient() { return m_networkControlClient; }

  private:
    NetworkControlClient * m_networkControlClient;
};

class NetworkControl : public QTcpServer
{
    Q_OBJECT
  public:
    NetworkControl();
    ~NetworkControl();
    bool listen(const QHostAddress &address = QHostAddress::Any,
                quint16 port = 0);

  private slots:
    void newConnection();
    void receiveCommand(QString &command);
    void deleteClient(void);

  protected:
    static void *SocketThread(void *param);
    void RunSocketThread(void);
    static void *CommandThread(void *param);
    void RunCommandThread(void);

  private:
    QString processJump(QStringList tokens);
    QString processKey(QStringList tokens);
    QString processLiveTV(QStringList tokens);
    QString processPlay(QStringList tokens, int clientID);
    QString processQuery(QStringList tokens);
    QString processSet(QStringList tokens);
    QString processHelp(QStringList tokens);

    void notifyDataAvailable(void);
    void sendReplyToClient(NetworkControlClient *ncc, QString &reply);
    void customEvent(QEvent *e);

    QString listRecordings(QString chanid = "", QString starttime = "");
    QString listSchedule(const QString& chanID = "") const;
    QString saveScreenshot(QStringList tokens);

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

    QList<NetworkCommand*> networkControlCommands;
    QMutex ncLock;
    QWaitCondition ncCond;

    QList<NetworkCommand*> networkControlReplies;
    QMutex nrLock;

    pthread_t command_thread;
    bool stopCommandThread;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

