// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef   __audiooutput_h
#define   __audiooutput_h

class AudioOutput;

#include "output.h"

#if defined( Q_OS_WIN32 )
#include <dsound.h>
#include "constants.h"
#endif


class AudioOutput : public Output
{
public:
    AudioOutput(unsigned int, const QString &);
    virtual ~AudioOutput();

    bool initialized() const { return inited; }

    bool initialize();
    void configure(long, int, int, int);
    void stop();
    void pause();
    long written();
    long latency();
    void seek(long);


private:
    // thread run function
    void run();

    // helper functions
    void reset();
    void resetDSP();
    void status();
    void post();
    void sync();

    QString audio_device;

    bool inited,paus,play,user_stop;
    long total_written, current_seconds,bps;
    int stat;
    int lr, lf, lc, lp;
#if defined(_OS_UNIX_) || defined(Q_OS_UNIX)
    bool do_select;
    int audio_fd;

#elif defined( _OS_WIN32_ ) || defined( Q_OS_WIN32 )
    LPDIRECTSOUND lpds;
    LPDIRECTSOUNDBUFFER lpdsb;
    LPDIRECTSOUNDNOTIFY lpdsn;
    PCMWAVEFORMAT pcmwf;
    HANDLE hNotifyEvent;
    DSBPOSITIONNOTIFY notifies[ BUFFERBLOCKS ];
    DWORD currentLatency;
/* Not sure we need this
    DSCAPS dsCaps;
*/
#endif

};


#endif // __audiooutout_h
