/*
	tone.h

	(c) 2004 Paul Volkaerts
	
  Header for a simple tone creation class
*/

#ifndef TONE_H_
#define TONE_H_

#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>
#include <mythtv/volumecontrol.h>

#include "directory.h"
#include "webcam.h"
#include "wavfile.h"
#include "rtp.h"




class Tone : public QObject
{
  Q_OBJECT

  public:
        Tone(int freqHz, int volume, int ms, QObject *parent = 0, const char * = 0);
        Tone(const Tone &t, QObject *parent = 0, const char * = 0);
        Tone(int ms, QObject *parent = 0, const char * = 0);
        Tone(wavfile &wav, QObject *parent = 0, const char * = 0);
        virtual ~Tone();
        Tone& operator+=(const Tone &rhs);
        void sum(int freqHz, int volume);
        short *getAudio() { return toneBuffer; };
        int getSamples() { return Samples; };
        bool Playing() { return (spkDev != -1); };

        void Play(QString deviceName, bool loop);
        void Stop();
        int  OpenSpeaker(QString devName);
        void CloseSpeaker();
   
  public slots:
        void audioTimerExpiry();

  private:
        int     Samples;
        short  *toneBuffer;
        int     spkDev;
        bool    Loop;
        QTimer *audioTimer;
        int     playPtr;
};


#endif
