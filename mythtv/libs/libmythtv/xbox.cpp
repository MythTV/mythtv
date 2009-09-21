// ANSI C
#include <cstdlib>

// POSIX
#include <unistd.h>

// Qt
#include <QTimer>

#include "xbox.h"
#include "mythcontext.h"
#include "remoteutil.h"
#include "mythsystem.h"

XBox::XBox(void) :
    timer(NULL),      RecCheck(0),
    RecordingLED(""), DefaultLED(""),
    PhaseCache(""),   BlinkBIN(""),
    LEDNonLiveTV(0)
{
}

void XBox::GetSettings(void)
{
    if (timer)
    {
        timer->disconnect();
        timer->stop();
        timer = NULL;
    }

    RecordingLED = gContext->GetSetting("XboxLEDRecording","rrrr");
    DefaultLED = gContext->GetSetting("XboxLEDDefault","gggg");
    BlinkBIN = gContext->GetSetting("XboxBlinkBIN");
    LEDNonLiveTV = gContext->GetNumSetting("XboxLEDNonLiveTV", 0);

    if (BlinkBIN.isEmpty())
        return;
    
    QString timelen = gContext->GetSetting("XboxCheckRec","5");
    int timeout = timelen.toInt() * 1000;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(CheckRec()));
    timer->start(timeout);
}

void XBox::CheckRec(void)
{
    QStringList recording = RemoteRecordings();

    int all = recording[0].toInt();
    int livetv = recording[1].toInt();
    int numrec = all;

    if (LEDNonLiveTV)
        numrec -= livetv;

    QString color = (numrec) ? RecordingLED : DefaultLED;

    if (color != PhaseCache)
    {
        QString tmp = BlinkBIN + " " + color;
        myth_system(tmp.toAscii().constData());
        PhaseCache = color;
    }
}
