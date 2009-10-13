#ifndef NETWORKCONTROL_H_
#define NETWORKCONTROL_H_

#include <deque>
using namespace std;

#include <pthread.h>

#include <QWaitCondition>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>

class MainServer;
class QTextStream;

// Locking order
// clientLock -> ncLock
//            -> nrLock

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
    void readClient();
    void deleteClientLater();

  protected:
    static void *SocketThread(void *param);
    void RunSocketThread(void);
    static void *CommandThread(void *param);
    void RunCommandThread(void);

  private:
    QString processJump(QStringList tokens);
    QString processKey(QStringList tokens);
    QString processLiveTV(QStringList tokens);
    QString processPlay(QStringList tokens);
    QString processQuery(QStringList tokens);
    QString processSet(QStringList tokens);
    QString processHelp(QStringList tokens);

    void notifyDataAvailable(void);
    void customEvent(QEvent *e);

    QString listRecordings(QString chanid = "", QString starttime = "");
    QString listSchedule(const QString& chanID = "") const;
    QString saveScreenshot(QStringList tokens);

    void processNetworkControlCommand(QString command);


    QString prompt;
    bool gotAnswer;
    QString answer;
    QMap <QString, QString> jumpMap;
    QMap <QString, int> keyMap;
    QMap <int, QString> keyTextMap;

    mutable QMutex  clientLock;
    QTcpSocket     *client;
    QTextStream    *clientStream;

    deque<QString> networkControlCommands;
    QMutex ncLock;
    QWaitCondition ncCond;

    deque<QString> networkControlReplies;
    QMutex nrLock;

    pthread_t command_thread;
    bool stopCommandThread;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

