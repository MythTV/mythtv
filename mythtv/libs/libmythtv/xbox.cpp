#include "xbox.h"

#include "mythcontext.h"
#include "remoteutil.h"

#include <unistd.h>

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
    bool recording = RemoteIsRecording();

    QString color = (recording) ? RecordingLED : DefaultLED;

    if (color != PhaseCache)
    {
        system(BlinkBIN + " " + color);
        PhaseCache = color;
    }
}

