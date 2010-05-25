/***
 *   This file was part of PulseAudio, the license has been upgraded to GPL v2
 *   or later as per the LGPL grant this was originally distributed under.
 *
 *   Copyright 2004-2006 Lennart Poettering
 *
 *   MythTV is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "audiopulseutil.h"
#include "util.h" // for IsPulseAudioRunning()
#include "exitcodes.h"
#include "mythverbose.h"
#include "mythcorecontext.h"

#ifdef USING_PULSE

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <locale.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <pulse/pulseaudio.h>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#define BUFSIZE 1024

#define LOC      QString("AudioPulseUtil: ")
#define LOC_WARN QString("AudioPulseUtil, Warning: ")
#define LOC_ERR  QString("AudioPulseUtil, Error: ")

enum pa_values {
    kPA_undefined                   = -1,
    kPA_suspended                   = +0,
    kPA_not_suspended_remote_server = +1,
    kPA_not_suspended_error         = +2,
    kPA_not_suspended_success       = +3,
    kPA_unsuspended_error           = +4,
    kPA_unsuspended_success         = +5,
};

static pa_context      *pau_context = NULL;
static pa_mainloop_api *pau_mainloop_api = NULL;
static QMutex           pau_lock;
static QWaitCondition   pau_wait;
static int              pau_value = kPA_undefined;

static void pau_set_value(int new_value)
{
    QMutexLocker ml(&pau_lock);
    pau_value = new_value;
    pau_wait.wakeAll();
}

static void pau_quit(int ret)
{
    if (pau_mainloop_api)
        pau_mainloop_api->quit(pau_mainloop_api, ret);
}

static void pau_context_drain_complete(pa_context *c, void *userdata)
{
    if (c)
        pa_context_disconnect(c);
}

static void pau_drain(void)
{
    if (!pau_context)
        return;

    pa_operation *operation = pa_context_drain(pau_context, pau_context_drain_complete, NULL);
    if (!operation)
        pa_context_disconnect(pau_context);
    else
        pa_operation_unref(operation);
}

static void pau_suspend_complete(pa_context *c, int success, void *userdata)
{
    if (!success)
    {
        if (!c)
            return;

        VERBOSE(VB_AUDIO, LOC_ERR + QString("Failure to suspend: %1")
                .arg(pa_strerror(pa_context_errno(c))));

        pau_set_value(kPA_not_suspended_error);

        return;
    }

    {
        QMutexLocker ml(&pau_lock);
        if (kPA_suspended == pau_value)
            return;
    }

    VERBOSE(VB_GENERAL, LOC + "Suspend Success");

    pau_set_value(kPA_suspended);
}

static void pau_resume_complete(pa_context *c, int success, void *userdata)
{
    static int n = 0;

    n++;

    if (!success)
    {
        if (!c)
            return;

        VERBOSE(VB_AUDIO, LOC_ERR + QString("Failure to resume: %1")
                .arg(pa_strerror(pa_context_errno(c))));

        pau_set_value(kPA_unsuspended_error);

        return;
    }

    if (n >= 2)
        pau_drain(); /* drain and quit */

    {
        QMutexLocker ml(&pau_lock);
        if (kPA_unsuspended_success == pau_value)
            return;
    }

    VERBOSE(VB_AUDIO, LOC + "Resume Success");

    pau_set_value(kPA_unsuspended_success);
}

static void pau_context_state_callback(pa_context *c, void *userdata)
{
    if (!c)
        return;

    switch (pa_context_get_state(c))
    {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            if (pa_context_is_local(c))
            {
                pa_operation *operation_sink =
                    pa_context_suspend_sink_by_index(
                        c, PA_INVALID_INDEX, 1, pau_suspend_complete, NULL);
                pa_operation_unref(operation_sink);

                pa_operation *operation_source =
                    pa_context_suspend_source_by_index(
                        c, PA_INVALID_INDEX, 1, pau_suspend_complete, NULL);
                pa_operation_unref(operation_source);
            }
            else
            {
                VERBOSE(VB_AUDIO, LOC_ERR +
                        "Sound server is not local, can not suspend.");

                pau_set_value(kPA_not_suspended_remote_server);
            }

            break;

        case PA_CONTEXT_TERMINATED:
            pau_quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            VERBOSE(VB_AUDIO, LOC_WARN +
                    "Can not connect to sound server, can not suspend." +
                    QString("\n\t\t\t%1")
                    .arg(pa_strerror(pa_context_errno(c))));

            pau_set_value(kPA_not_suspended_error);

            if (pau_context)
            {
                pa_context_unref(pau_context);
                pau_context = NULL;
            }

            break;
    }
}

void pau_pulseaudio_suspend_internal(void)
{
    pa_mainloop* m = NULL;
    int ret = 1;
    char *server = NULL;
    const char *bn = "mythtv";

    if (!(m = pa_mainloop_new()))
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "pa_mainloop_new() failed.");
        goto quit;
    }

    pau_mainloop_api = pa_mainloop_get_api(m);
    if (!pau_mainloop_api)
        goto quit;

    if (pa_signal_init(pau_mainloop_api) != 0)
        goto quit;

    if (!(pau_context = pa_context_new(pau_mainloop_api, bn)))
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "pa_context_new() failed.");
        goto quit;
    }

    pa_context_set_state_callback(pau_context, pau_context_state_callback, NULL);
    pa_context_connect(pau_context, server, PA_CONTEXT_NOAUTOSPAWN, NULL);

    if (pa_mainloop_run(m, &ret) < 0)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "pa_mainloop_run() failed.\n");
        goto quit;
    }

quit:
    if (pau_context)
        pa_context_unref(pau_context);

    if (m) {
        pa_signal_done();
        pa_mainloop_free(m);
    }

    pa_xfree(server);
}

class PAThread : public QThread
{
  public:
    void run(void)
    {
        pau_pulseaudio_suspend_internal();
    }
};

/// \returns true if successful
bool pulseaudio_suspend(void)
{
    setenv("PULSE_INTERNAL", "1", 0);

    QThread *t = new PAThread();
    t->start();

    QMutexLocker ml(&pau_lock);
    while (kPA_undefined == pau_value)
        pau_wait.wait(&pau_lock);

    return kPA_suspended == pau_value;
}

/// \returns true if successful
bool pulseaudio_unsuspend(void)
{
    if (!pau_context)
    {
        pau_quit(0);
        return false;
    }

    if (pa_context_is_local(pau_context))
    {
        pa_operation *operation_sink =
            pa_context_suspend_sink_by_index(
                pau_context, PA_INVALID_INDEX, 0, pau_resume_complete, NULL);
        pa_operation_unref(operation_sink);

        pa_operation *operation_source =
            pa_context_suspend_source_by_index(
                pau_context, PA_INVALID_INDEX, 0, pau_resume_complete, NULL);
        pa_operation_unref(operation_source);

        QMutexLocker ml(&pau_lock);
        while ((kPA_unsuspended_error   != pau_value) &&
               (kPA_unsuspended_success != pau_value))
        {
            pau_wait.wait(&pau_lock);
        }

        return kPA_unsuspended_success == pau_value;
    }
    else
    {
        pau_drain();
    }

    return false;
}

#endif // USING_PULSE

int pulseaudio_handle_startup(void)
{
#ifdef USING_PULSE
    if (getenv("DEBUG_PULSE_AUDIO_ALSA_EMULATION"))
    {
        VERBOSE(VB_AUDIO, "WARNING: ");
        VERBOSE(VB_AUDIO, "WARNING: *** PulseAudio is running ***");
        VERBOSE(VB_AUDIO, "WARNING: Ignoring.");
    }
    else if (IsPulseAudioRunning() && !pulseaudio_suspend())
    {
        VERBOSE(VB_AUDIO, "WARNING: *** Pulse Audio is running ***");
        VERBOSE(VB_AUDIO, "WARNING: Unable to suspend it.");

        return -1;
    }
#else
    if (IsPulseAudioRunning())
    {
        VERBOSE(VB_AUDIO, "WARNING: *** PulseAudio is running ***");
        VERBOSE(VB_AUDIO, "WARNING: Unable to suspend it "
                          "(PulseAudio support disabled)");
        return -1;
    }
#endif

    return 1;
}

int pulseaudio_handle_teardown(void)
{
#ifdef USING_PULSE
    if (getenv("DEBUG_PULSE_AUDIO_ALSA_EMULATION"))
        return 1;

    {
        QMutexLocker ml(&pau_lock);
        if (kPA_suspended != pau_value)
            return 1;
    }

    if (!pulseaudio_unsuspend())
    {
        VERBOSE(VB_AUDIO, "WARNING: Encountered error re-enabling pulse audio");
        return -1;
    }
#endif

    return 1;
}
