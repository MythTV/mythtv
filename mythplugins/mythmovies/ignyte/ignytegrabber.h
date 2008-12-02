#include <iostream>
using namespace std;

#include <QApplication>
#include <QTimer>
#include <QString>

#include "mythsoap.h"

class IgnyteGrabber : public QObject
{
  Q_OBJECT

  public:
    IgnyteGrabber(QString, QString, QApplication*);

  private:
    ~IgnyteGrabber() {}
    void outputData(QString);

  private:
    QTimer *waitForSoap;
    MythSoap *ms;
    QApplication *app;

  public slots:
    void checkHttp(void);
};

