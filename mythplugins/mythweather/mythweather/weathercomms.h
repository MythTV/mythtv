#include <qwidget.h>
#include <qfile.h>
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
    int connectType;

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
    QString weatherData;
    QString imageData;
    QString imageLoc;
    QString mapLoc;
    QString parseData(QString, QString);
    void makeConnection();
    void resetConnection();
    int getIntStatus();
    int timeout_cnt;
    int invalid_cnt;
    int reset_cnt;
    int currentError;
    int aggressiveness;
    int error;
    int imgSize;
    bool writeImage;
    bool myStatus;
    bool debug;
    bool gotDataHook;
    bool breakout;
    QFile image_out;
};

#endif

