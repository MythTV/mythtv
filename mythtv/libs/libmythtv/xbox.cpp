#include "xbox.h"

#include "mythcontext.h"
#include "remoteutil.h"

#include <unistd.h>
#include <stdlib.h>

XBox::XBox(void)
{
    timer = NULL;
    PhaseCache = "";
}

void XBox::GetSettings(void)
{
    if (timer)
    {
        timer->stop();
        delete timer;
    }

    RecordingLED = gContext->GetSetting("XboxLEDRecording","rrrr");
    DefaultLED = gContext->GetSetting("XboxLEDDefault","gggg");
    BlinkBIN = gContext->GetSetting("XboxBlinkBIN");
    LEDNonLiveTV = gContext->GetNumSetting("XboxLEDNonLiveTV", 0);

    if (!BlinkBIN)
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
        system(BlinkBIN + " " + color);
        PhaseCache = color;
    }
}

