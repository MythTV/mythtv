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

MythSignalingTimer::MythSignalingTimer(
    QObject *parent, const char *slot) :
    QThread(parent), dorun(false), running(false), microsec(0)
{
    connect(this, SIGNAL(timeout()), parent, slot,
            Qt::QueuedConnection);
}

MythSignalingTimer::~MythSignalingTimer()
{
    stop();
}

void MythSignalingTimer::start(int msec)
{
    if (msec <= 0)
        return;

    microsec = 1000 * msec;

    QMutexLocker locker(&startStopLock);
    if (!running)
    {
        dorun = true;
        QThread::start();
        while (dorun && !running)
            usleep(10 * 1000);
    }
}

void MythSignalingTimer::stop(void)
{
    if (thread() == this)
    {
        dorun = false;
        return;
    }

    QMutexLocker locker(&startStopLock);
    if (running)
    {
        dorun = false;
        wait();
    }
}

void MythSignalingTimer::run(void)
{
    running = true;
    while (dorun)
    {
        usleep(microsec);
        if (dorun)
            emit timeout();
    }
    running = false;
}
