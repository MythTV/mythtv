// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include <qobject.h>
#include <qapplication.h>
#include "audiooutput.h"
#include "constants.h"
#include "buffer.h"
#include "visual.h"

#include <stdio.h>
#include <string.h>

#include <iostream>

using namespace std;

extern Q_EXPORT QApplication* qApp;


void AudioOutput::stop()
{
    user_stop = TRUE;
}

void AudioOutput::status()
{
    long ct = (total_written - latency()) / bps;

    if (ct < 0)
	ct = 0;

    if (ct > current_seconds) {
	current_seconds = ct;
	OutputEvent e(current_seconds, total_written, lr, lf, lp, lc);
	dispatch(e);
    }
}

long AudioOutput::written()
{
    return total_written;
}

void AudioOutput::seek(long pos)
{
    recycler()->mutex()->lock();
    recycler()->clear();
    recycler()->mutex()->unlock();

    total_written = (pos * bps);
    current_seconds = -1;
}

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#if defined(__FreeBSD__)
#    include <machine/soundcard.h>
#elif defined(__linux__)
#  include <linux/soundcard.h>
#elif defined(__bsdi__)
#  include <sys/soundcard.h>
#endif


AudioOutput::AudioOutput(unsigned int sz, const QString &d)
    : Output(sz), audio_device(d), inited(FALSE), paus(FALSE), play(FALSE),
      user_stop(FALSE),
      total_written(0), current_seconds(-1),
      bps(1), lf(-1), lc(-1), lp(-1),
      do_select(TRUE),
      audio_fd(-1)
{
}

AudioOutput::~AudioOutput()
{
    if (audio_fd > 0) {
	close(audio_fd);
	audio_fd = -1;
    }
}

void AudioOutput::configure(long freq, int chan, int prec, int rate)
{
    // we need to configure
    if (freq != lf || chan != lc || prec != lp) {
	// we have already configured, but are changing settings...
	// reset the device
	resetDSP();

	lf = freq;
	lc = chan;
	lp = prec;

	bps = freq * chan * (prec / 8);

	int p;
	switch(prec) {
	default:
	case 16:
#if defined(AFMT_S16_NE)
	    p = AFMT_S16_NE;
#else
	    p = AFMT_S16_LE;
#endif
	    break;

	case 8:
	    p = AFMT_S8;
	    break;

	}

	ioctl(audio_fd, SNDCTL_DSP_SETFMT, &p);
	ioctl(audio_fd, SNDCTL_DSP_SAMPLESIZE, &prec);
	int stereo = (chan > 1) ? 1 : 0;
	ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo);
	ioctl(audio_fd, SNDCTL_DSP_SPEED, &freq);
    }

    lr = rate;

    prepareVisuals();
}


void AudioOutput::reset()
{
    if (audio_fd > 0) {
	close(audio_fd);
	audio_fd = -1;
    }

    audio_fd = open(audio_device, O_WRONLY, 0);

    if (audio_fd < 0) {
	error(QString("AudioOutput: failed to open output device '%1'").
	      arg(audio_device));
	return;
    }

    int flags;
    if ((flags = fcntl(audio_fd, F_GETFL, 0)) > 0) {
	flags &= O_NDELAY;
	fcntl(audio_fd, F_SETFL, flags);
    }

    fd_set afd;
    FD_ZERO(&afd);
    FD_SET(audio_fd, &afd);
    struct timeval tv;
    tv.tv_sec = 0l;
    tv.tv_usec = 50000l;
    do_select = (select(audio_fd + 1, 0, &afd, 0, &tv) > 0);
}


void AudioOutput::pause()
{
    paus = (paus) ? FALSE : TRUE;
}

void AudioOutput::post()
{
    if (audio_fd < 1)
	return;

    int unused;
    ioctl(audio_fd, SNDCTL_DSP_POST, &unused);
}

void AudioOutput::sync()
{
    if (audio_fd < 1)
	return;

    int unused;
    ioctl(audio_fd, SNDCTL_DSP_SYNC, &unused);
}


void AudioOutput::resetDSP()
{
    if (audio_fd < 1)
	return;

    int unused;
    ioctl(audio_fd, SNDCTL_DSP_RESET, &unused);
}


void AudioOutput::resetTime()
{
    mutex()->lock();
    current_seconds = -1;
    total_written = 0;
    mutex()->unlock();
}

bool AudioOutput::initialize()
{
    inited = paus = play = user_stop = FALSE;

    if (! audio_device) {
	error(QString("AudioOutput: cannot initialize, no device name"));

	return FALSE;
    }

    reset();
    if (audio_fd < 0)
	return FALSE;

    current_seconds = -1;
    total_written = 0;
    stat = OutputEvent::Stopped;

    inited = TRUE;
    return TRUE;
}


long AudioOutput::latency()
{
    ulong used = 0;

    if (! paus) {
	if (ioctl(audio_fd, SNDCTL_DSP_GETODELAY, &used) == -1)
	    used = 0;
    }

    return used;
}

void AudioOutput::run()
{
    mutex()->lock();

    if (! inited) {
	mutex()->unlock();

	return;
    }

    play = TRUE;

    mutex()->unlock();

    fd_set afd;
    struct timeval tv;
    Buffer *b = 0;
    bool done = FALSE;
    unsigned long n = 0, m = 0, l = 0;

    FD_ZERO(&afd);

    while (! done) {
	mutex()->lock();

	recycler()->mutex()->lock();

	done = user_stop;

	while (! done && (recycler()->empty() || paus)) {
	    post();

	    mutex()->unlock();

	    {
		stat = paus ? OutputEvent::Paused : OutputEvent::Buffering;
		OutputEvent e((OutputEvent::Type) stat);
		dispatch(e);
	    }

	    recycler()->cond()->wakeOne();
	    recycler()->cond()->wait(recycler()->mutex());

	    mutex()->lock();
	    done = user_stop;

	    {
		stat = OutputEvent::Playing;
		OutputEvent e((OutputEvent::Type) stat);
		dispatch(e);
	    }

	    status();
	}

       	if (! b) {
	    b = recycler()->next();
	    if (b->rate)
	        lr = b->rate;
	}

	recycler()->cond()->wakeOne();
	recycler()->mutex()->unlock();

	FD_ZERO(&afd);
	FD_SET(audio_fd, &afd);
	// nice long poll timeout
	tv.tv_sec = 5l;
	tv.tv_usec = 0l;

	if (b &&
	    (! do_select || (select(audio_fd + 1, 0, &afd, 0, &tv) > 0 &&
			     FD_ISSET(audio_fd, &afd)))) {
	    l = QMIN(2048, b->nbytes - n);
	    if (l > 0) {
		m = write(audio_fd, b->data + n, l);
		n += m;

		status();
                dispatchVisual(b, total_written, lc, lp);
	    } else {
		// force buffer change
		n = b->nbytes;
		m = 0;
	    }
	}

	total_written += m;

	if (n == b->nbytes) {
	    recycler()->mutex()->lock();
	    recycler()->done();
	    recycler()->mutex()->unlock();

	    b = 0;
	    n = 0;
	}

	mutex()->unlock();
    }

    mutex()->lock();

    if (! user_stop)
	sync();
    resetDSP();

    play = FALSE;

    {
	stat = OutputEvent::Stopped;
	OutputEvent e((OutputEvent::Type) stat);
	dispatch(e);
    }

    mutex()->unlock();
}
