#include <qwidget.h>
#include <qsocket.h>

#ifndef WEATHERSOCK_H_
#define WEATHERSOCK_H_

#include "weather.h"

class WeatherSock : public QObject
{
  Q_OBJECT
  public:
    WeatherSock(Weather *, bool, int);
    QString getData();
    void startConnect();
    int verifyData();
    int checkError();
    bool getStatus();

  private slots:
    void closeConnection();
    void socketReadyRead();
    void socketConnected();
    void socketConnectionClosed();
    void socketClosed();
    void delayedClosed();
    void socketError(int);

  private:
    Weather *parent;
    QSocket *httpSock;
    QString strStatus();
    QString httpData;
    void makeConnection();
    void resetConnection();
    int getIntStatus();
    int timeout_cnt;
    int invalid_cnt;
    int reset_cnt;
    int currentError;
    int aggressiveness;
    int error;
    bool myStatus;
    bool debug;
    bool gotDataHook;
    bool breakout;

};

#endif

