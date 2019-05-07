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
    QObject(parent), MThread("SignalingTimer")
{
    connect(this, SIGNAL(timeout()), parent, slot,
            Qt::QueuedConnection);
}

MythSignalingTimer::~MythSignalingTimer()
{
    MythSignalingTimer::stop();
    wait();
}

void MythSignalingTimer::start(int msec)
{
    if (msec <= 0)
        return;

    m_millisec = msec;

    QMutexLocker locker(&m_startStopLock);
    if (!m_running)
    {
        m_dorun = true;
        MThread::start();
        while (m_dorun && !m_running)
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
        m_dorun = false;
        return;
    }

    QMutexLocker locker(&m_startStopLock);
    if (m_running)
    {
        m_dorun = false;
        m_timerWait.wakeAll();
        locker.unlock();
        wait();
    }
}

void MythSignalingTimer::run(void)
{
    m_running = true;
    RunProlog();
    while (m_dorun)
    {
        QMutexLocker locker(&m_startStopLock);
        if (m_dorun && !m_timerWait.wait(locker.mutex(), m_millisec))
        {
            locker.unlock();
            emit timeout();
            locker.relock();
        }
    }
    RunEpilog();
    m_running = false;
}
