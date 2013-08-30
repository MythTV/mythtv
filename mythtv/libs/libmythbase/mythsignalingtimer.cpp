/* -*- Mode: c++ -*-
 *
 *   Copyright (C) 2010 Jim Stichnoth, Daniel Kristjansson
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

#include "mythsignalingtimer.h"
#include "mythlogging.h"

MythSignalingTimer::MythSignalingTimer(
    QObject *parent, const char *slot) :
    QObject(parent), MThread("SignalingTimer"),
    dorun(false), running(false), millisec(0)
{
    connect(this, SIGNAL(timeout()), parent, slot,
            Qt::QueuedConnection);
}

MythSignalingTimer::~MythSignalingTimer()
{
    stop();
    wait();
}

void MythSignalingTimer::start(int msec)
{
    if (msec <= 0)
        return;

    millisec = msec;

    QMutexLocker locker(&startStopLock);
    if (!running)
    {
        dorun = true;
        MThread::start();
        while (dorun && !running)
        {
            locker.unlock();
            usleep(10 * 1000);
            locker.relock();
        }
    }
}

void MythSignalingTimer::stop(void)
{
    if (is_current_thread(this))
    {
        dorun = false;
        return;
    }

    QMutexLocker locker(&startStopLock);
    if (running)
    {
        dorun = false;
        timerWait.wakeAll();
        locker.unlock();
        wait();
    }
}

void MythSignalingTimer::run(void)
{
    running = true;
    RunProlog();
    while (dorun)
    {
        QMutexLocker locker(&startStopLock);
        if (dorun && !timerWait.wait(locker.mutex(), millisec))
        {
            locker.unlock();
            emit timeout();
            locker.relock();
        }
    }
    RunEpilog();
    running = false;
}
