#ifndef NETWORKCONTROL_H_
#define NETWORKCONTROL_H_

#include <pthread.h>

#include <q3serversocket.h>
#include <q3textstream.h>
#include <q3socket.h>
#include <q3valuelist.h>
#include <qobject.h>
#include <qmutex.h>
#include <qwaitcondition.h>

class MainServer;

class NetworkControl : public Q3ServerSocket
{
    Q_OBJECT
  public:
    NetworkControl(int port);
    ~NetworkControl();

    void newConnection(int socket);

  private slots:
    void readClient();
    void discardClient();

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

    QMutex clientLock;
    Q3Socket *client;
    Q3TextStream *cs;

    Q3ValueList<QString> networkControlCommands;
    QMutex ncLock;
    QWaitCondition ncCond;

    Q3ValueList<QString> networkControlReplies;
    QMutex nrLock;

    pthread_t command_thread;
    bool stopCommandThread;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

