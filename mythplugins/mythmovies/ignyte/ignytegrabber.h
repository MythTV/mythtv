#include <qapplication.h>
#include <qtimer.h>
#include <qstring.h>
#include <iostream>
#include "mythsoap.h"

using namespace std;

class IgnyteGrabber : public QObject
{
    Q_OBJECT

    public:
        IgnyteGrabber(QString, QString, QApplication*);

    private:
        QTimer *waitForSoap;
        MythSoap ms;
        QApplication *app;
        void outputData(QString);

    public slots:
        void checkHttp();
};

