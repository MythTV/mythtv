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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// POSIX
#ifndef _WIN32
#include <sys/select.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt
#include <QDir>

// MythTV
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"

#include "importrecorder.h"
#include "mythcommflagplayer.h"
#include "tv_rec.h"

#define TVREC_CARDNUM \
        ((m_tvrec != nullptr) ? QString::number(m_tvrec->GetInputId()) : "NULL")

#define LOC QString("ImportRec[%1](%2): ") \
            .arg(TVREC_CARDNUM, m_videodevice)

void ImportRecorder::SetOptionsFromProfile([[maybe_unused]] RecordingProfile *profile,
                                           const QString &videodev,
                                           [[maybe_unused]] const QString &audiodev,
                                           [[maybe_unused]] const QString &vbidev)
{
    QString testVideoDev = videodev;

    if (videodev.startsWith("file:", Qt::CaseInsensitive))
        testVideoDev = videodev.mid(5);

    QFileInfo fi(testVideoDev);
    if (fi.exists() && fi.isReadable() && fi.isFile() && fi.size() > 1560)
        SetOption("videodevice", testVideoDev);
    else
        SetOption("videodevice", "unknown file");

    SetOption("tvformat",    gCoreContext->GetSetting("TVFormat"));
    SetOption("vbiformat",   gCoreContext->GetSetting("VbiFormat"));
}

void UpdateFS(int pc, void* ir);
void UpdateFS(int /*pc*/, void* ir)
{
    if(ir)
        static_cast<ImportRecorder*>(ir)->UpdateRecSize();
}

void ImportRecorder::UpdateRecSize()
{
    m_curRecording->SaveFilesize(m_ringBuffer->GetRealFileSize());

    if(m_cfp)
        m_nfc=m_cfp->GetDecoder()->GetFramesRead();
}

long long ImportRecorder::GetFramesWritten(void)
{
    return m_nfc;
}

void ImportRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- " +
        QString("attempting to open '%1'")
            .arg(m_curRecording->GetPathname()));

    // retry opening the file until StopRecording() is called.
    while (!Open() && IsRecordingRequested() && !IsErrored())
    {   // sleep 250 milliseconds unless StopRecording() or Unpause()
        // is called, just to avoid running this loop too often.
        QMutexLocker locker(&m_pauseLock);
        if (m_requestRecording)
            m_unpauseWait.wait(&m_pauseLock, 15000);
    }

    m_curRecording->SaveFilesize(m_ringBuffer->GetRealFileSize());

    // build seek table
    if (m_importFd && IsRecordingRequested() && !IsErrored())
    {
        auto *ctx = new PlayerContext(kImportRecorderInUseID);
        auto *cfp = new MythCommFlagPlayer(ctx, (PlayerFlags)(kAudioMuted | kVideoIsNull | kNoITV));
        MythMediaBuffer *buffer = MythMediaBuffer::Create(m_ringBuffer->GetFilename(), false, true, 6s);
        //This does update the status but does not set the ultimate
        //recorded / failure status for the relevant recording
        SetRecordingStatus(RecStatus::Recording, __FILE__, __LINE__);
        ctx->SetPlayingInfo(m_curRecording);
        ctx->SetRingBuffer(buffer);
        ctx->SetPlayer(cfp);

        m_cfp=cfp;
        gCoreContext->RegisterFileForWrite(m_ringBuffer->GetFilename());
        cfp->RebuildSeekTable(false,UpdateFS,this);
        gCoreContext->UnregisterFileForWrite(m_ringBuffer->GetFilename());
        m_cfp=nullptr;

        delete ctx;
    }

    m_curRecording->SaveFilesize(m_ringBuffer->GetRealFileSize());

    // cleanup...
    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
}

bool ImportRecorder::Open(void)
{
    if (m_importFd >= 0)   // already open
        return true;

    if (!m_curRecording)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "no current recording!");
        return false;
    }

    ResetForNewFile();

    QString fn = m_curRecording->GetPathname();

    // Quick-and-dirty "copy" of sample prerecorded file.
    // Sadly, won't work on Windows.
    //
    QFile preRecorded(m_videodevice);
    QFile copy(fn);
    if (preRecorded.exists() && (!copy.exists() || copy.size() == 0))
    {
        if (copy.exists())   // always created by RecorderBase?
        {
            QDir targetDir(".");  // QDir::remove() needs an object
            targetDir.remove(fn);
        }

        LOG(VB_RECORD, LOG_INFO, LOC + QString("Trying to link %1 to %2")
                           .arg(m_videodevice, fn));

        if (preRecorded.link(fn))
            LOG(VB_RECORD, LOG_DEBUG, LOC + "success!");
        else
            LOG(VB_RECORD, LOG_ERR, LOC + preRecorded.errorString());
    }

    if (fn.startsWith("myth://", Qt::CaseInsensitive))
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
            std::this_thread::sleep_for(250ms);

        return false;
    }
    if (!f.isReadable())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("'%1' is not readable").arg(fn));
        return false;
    }
    if (!f.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("'%1' is empty").arg(fn));
        return false;
    }

    m_importFd = open(fn.toLocal8Bit().constData(), O_RDONLY);
    if (m_importFd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Couldn't open '%1'").arg(fn) + ENO);
    }

    return m_importFd >= 0;
}

void ImportRecorder::Close(void)
{
    if (m_importFd >= 0)
    {
        close(m_importFd);
        m_importFd = -1;
    }
}
