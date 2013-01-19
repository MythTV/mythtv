/* -*- Mode: c++ -*-
 *   Class ImportRecorder
 *
 *   Copyright (C) Daniel Kristjansson 2009
 *
 *   This program is free software; you can redistribute it and/or modify
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

// POSIX
#ifndef USING_MINGW
#include <sys/select.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Qt
#include <QDir>

// MythTV
#include "mythcommflagplayer.h"
#include "importrecorder.h"
#include "mythdirs.h"
#include "tv_rec.h"
#include "mythdate.h"

#define TVREC_CARDNUM \
        ((tvrec != NULL) ? QString::number(tvrec->GetCaptureCardNum()) : "NULL")

#define LOC QString("ImportRec[%1](%2): ") \
            .arg(TVREC_CARDNUM).arg(videodevice)

ImportRecorder::ImportRecorder(TVRec *rec) :
    DTVRecorder(rec), _import_fd(-1)
{
}

ImportRecorder::~ImportRecorder()
{
}

void ImportRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                           const QString &videodev,
                                           const QString &audiodev,
                                           const QString &vbidev)
{
    (void)audiodev;
    (void)vbidev;
    (void)profile;

    QString testVideoDev = videodev;

    if (videodev.toLower().startsWith("file:"))
        testVideoDev = videodev.mid(5);

    QFileInfo fi(testVideoDev);
    if (fi.exists() && fi.isReadable() && fi.isFile() && fi.size() > 1560)
        SetOption("videodevice", testVideoDev);
    else
        SetOption("videodevice", "unknown file");

    SetOption("tvformat",    gCoreContext->GetSetting("TVFormat"));
    SetOption("vbiformat",   gCoreContext->GetSetting("VbiFormat"));
}

void ImportRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");

    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- " +
        QString("attempting to open '%1'")
            .arg(curRecording->GetPathname()));

    // retry opening the file until StopRecording() is called.
    while (!Open() && IsRecordingRequested() && !IsErrored())
    {   // sleep 250 milliseconds unless StopRecording() or Unpause()
        // is called, just to avoid running this loop too often.
        QMutexLocker locker(&pauseLock);
        if (request_recording)
            unpauseWait.wait(&pauseLock, 250);
    }

    curRecording->SaveFilesize(ringBuffer->GetRealFileSize());

    // build seek table
    if (_import_fd && IsRecordingRequested() && !IsErrored())
    {
        MythCommFlagPlayer *cfp =
            new MythCommFlagPlayer((PlayerFlags)(kAudioMuted | kVideoIsNull));
        RingBuffer *rb = RingBuffer::Create(
            ringBuffer->GetFilename(), false, true, 6000);

        PlayerContext *ctx = new PlayerContext(kImportRecorderInUseID);
        ctx->SetPlayingInfo(curRecording);
        ctx->SetRingBuffer(rb);
        ctx->SetPlayer(cfp);
        cfp->SetPlayerInfo(NULL, NULL, ctx);

        cfp->RebuildSeekTable(false);

        delete ctx;
    }

    curRecording->SaveFilesize(ringBuffer->GetRealFileSize());

    // cleanup...
    Close();

    FinishRecording();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
}

bool ImportRecorder::Open(void)
{
    if (_import_fd >= 0)   // already open
        return true;

    if (!curRecording)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "no current recording!");
        return false;
    }

    ResetForNewFile();

    QString fn = curRecording->GetPathname();

    // Quick-and-dirty "copy" of sample prerecorded file.
    // Sadly, won't work on Windows.
    //
    QFile preRecorded(videodevice);
    QFile copy(fn);
    if (preRecorded.exists() && (!copy.exists() || copy.size() == 0))
    {
        if (copy.exists())   // always created by RecorderBase?
        {
            QDir targetDir(".");  // QDir::remove() needs an object
            targetDir.remove(fn);
        }

        LOG(VB_RECORD, LOG_INFO, LOC + QString("Trying to link %1 to %2")
                           .arg(videodevice).arg(fn));

        if (preRecorded.link(fn))
            LOG(VB_RECORD, LOG_DEBUG, LOC + "success!");
        else
            LOG(VB_RECORD, LOG_ERR, LOC + preRecorded.errorString());
    }

    if (fn.toLower().startsWith("myth://"))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Malformed recording ProgramInfo.");
        return false;
    }

    QFileInfo f(fn);
    if (!f.exists())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("'%1' does not exist yet").arg(fn));

        // Slow down run open loop when debugging -v record.
        // This is just to make the debugging output less spammy.
        if (VERBOSE_LEVEL_CHECK(VB_RECORD, LOG_ANY))
            usleep(250 * 1000);

        return false;
    }
    else if (!f.isReadable())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("'%1' is not readable").arg(fn));
        return false;
    }

    _import_fd = open(fn.toLocal8Bit().constData(), O_RDONLY);
    if (_import_fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Couldn't open '%1'").arg(fn) + ENO);
    }

    return _import_fd >= 0;
}

void ImportRecorder::Close(void)
{
    if (_import_fd >= 0)
    {
        close(_import_fd);
        _import_fd = -1;
    }
}
