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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mythsignalingtimer.h"
#include "mythverbose.h"

MythSignalingTimer::MythSignalingTimer(
    QObject *parent, const char *slot) :
    QThread(parent), dorun(false), running(false), microsec(0)
{
    connect(this, SIGNAL(timeout()), parent, slot,
            Qt::BlockingQueuedConnection);
}

MythSignalingTimer::~MythSignalingTimer()
{
    stop();
}

void MythSignalingTimer::start(int msec)
{
    VERBOSE(VB_IMPORTANT, QString("start(%1) -- begin").arg(msec));
    if (msec <= 0)
        return;

    microsec = 1000 * msec;

    QMutexLocker locker(&startStopLock);
    if (!running)
    {
        dorun = true;
        QThread::start();
        while (!running)
            usleep(10 * 1000);
    }
    VERBOSE(VB_IMPORTANT, QString("start(%1) -- end").arg(msec));
}

void MythSignalingTimer::stop(void)
{
    VERBOSE(VB_IMPORTANT, "stop() -- begin");
    QMutexLocker locker(&startStopLock);
    if (running)
    {
        dorun = false;
        QThread::wait();
    }
    VERBOSE(VB_IMPORTANT, "stop() -- end");
}

void MythSignalingTimer::run(void)
{
    VERBOSE(VB_IMPORTANT, "run() -- begin");
    running = true;
    while (true)
    {
        usleep(microsec);
        if (!dorun)
            break;
        emit timeout();
    }
    running = false;
    VERBOSE(VB_IMPORTANT, "run() -- end");
}
